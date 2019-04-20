#include <FS.h>          // this needs to be first, or it all crashes and burns...

/* this code uses development version of WiFiManager.
download https://github.com/tzapu/WiFiManager/archive/development.zip and extract
it to %HOMEPATH%\\Documents\\Arduino\\libraries\\WiFiManager.

This is true untill WiFiManager released version supports esp32 by default
*/
#include <WiFiManager.h> 
#include <ArduinoJson.h> // https://github.com/bblanchon/ArduinoJson

#include <SD.h>
#include <string>
#include <PubSubClient.h>



#define CONFIG_FILE "/config.json"
void callback(const char* topic, byte* payload, uint32_t length);

struct Config {
    char mqtt_server[40];
    uint16_t mqtt_port;
    char mqtt_user[20];
    char mqtt_pass[20];
    char device_id[20];
};

WiFiClient espClient;
PubSubClient client(espClient);
Config config = { "ps.mqtt", 1883, "mqtt_user", "mqtt_pass", "dev1" };                         // <- global configuration object

//flag for saving data
bool shouldSaveConfig = false;
//callback notifying us of the need to save config
void saveConfigCallback() {
    Serial.println("Should save config");
    shouldSaveConfig = true;
}

// Loads the configuration from a file
void loadConfiguration(const char* filename, Config& config) {
    // Open file for reading
    File file = SD.open(filename);

    // Allocate a temporary JsonDocument
    // Don't forget to change the capacity to match your requirements.
    // Use arduinojson.org/v6/assistant to compute the capacity.
    StaticJsonDocument<512> doc;

    // Deserialize the JSON document
    DeserializationError error = deserializeJson(doc, file);
    if(error)
    {
        Serial.println(F("Failed to read file, using default configuration"));
        return;
    }


    strlcpy(config.mqtt_server, doc["mqtt_server"], sizeof(config.mqtt_server));
    config.mqtt_port =  doc["mqtt_port"];
    strlcpy(config.mqtt_user, doc["mqtt_user"], sizeof(config.mqtt_user));
    strlcpy(config.mqtt_pass, doc["mqtt_pass"], sizeof(config.mqtt_pass));
    strlcpy(config.device_id, doc["device_id"], sizeof(config.device_id));

// Close the file (Curiously, File's destructor doesn't close the file)
    file.close();
}


void saveConfigutation(const char* filename, Config& config){
    Serial.println("saving config");
    // Delete existing file, otherwise the configuration is appended to the file
    SD.remove(filename);

    // Open file for writing
    File file = SD.open(filename, FILE_WRITE);
    if(!file) {
        Serial.println(F("Failed to create file"));
        return;
    }

    StaticJsonDocument<256> json;
    json["mqtt_server"] = config.mqtt_server;
    json["mqtt_port"] = config.mqtt_port;
    json["mqtt_user"] = config.mqtt_user;
    json["mqtt_pass"] = config.mqtt_pass;
    json["device_id"] = config.device_id;

    // Serialize JSON to file
    if(serializeJson(json, file) == 0) {
        Serial.println(F("Failed to write to file"));
    }

    // Close the file
    file.close();
}


void setup() {
    // put your setup code here, to run once:
    Serial.begin(115200);
    Serial.println();
    char port_val[6];
    //clean FS for testing 
  //  SPIFFS.format();

    //read configuration from FS json
    Serial.println("mounting FS...");
    loadConfiguration(CONFIG_FILE, config);

    //end read



    // The extra parameters to be configured (can be either global or just in the setup)
    // After connecting, parameter.getValue() will get you the configured value
    // id/name placeholder/prompt default length
    sprintf(port_val, "%d", config.mqtt_port);
    WiFiManagerParameter custom_mqtt_server("server", "mqtt server", config.mqtt_server, 40);
    WiFiManagerParameter custom_mqtt_port("port", "mqtt port", port_val, 6);
    WiFiManagerParameter custom_mqtt_user("user", "mqtt user", config.mqtt_user, 20);
    WiFiManagerParameter custom_mqtt_pass("pass", "mqtt pass", config.mqtt_pass, 20);
    WiFiManagerParameter custom_device_id("device_id", "device id", config.device_id, 20);

    //WiFiManager
    //Local intialization. Once its business is done, there is no need to keep it around
    WiFiManager wifiManager;

    // Reset Wifi settings for testing  
    //  wifiManager.resetSettings();

      //set config save notify callback
    wifiManager.setSaveConfigCallback(saveConfigCallback);

    //set static ip
  //  wifiManager.setSTAStaticIPConfig(IPAddress(10,0,1,99), IPAddress(10,0,1,1), IPAddress(255,255,255,0));

    //add all your parameters here
    wifiManager.addParameter(&custom_mqtt_server);
    wifiManager.addParameter(&custom_mqtt_port);
    wifiManager.addParameter(&custom_mqtt_user);
    wifiManager.addParameter(&custom_mqtt_pass);
    wifiManager.addParameter(&custom_device_id);
    //reset settings - for testing
    //wifiManager.resetSettings();

    //set minimum quality of signal so it ignores AP's under that quality
    //defaults to 8%
    //wifiManager.setMinimumSignalQuality();

    //sets timeout until configuration portal gets turned off
    //useful to make it all retry or go to sleep
    //in seconds
    //wifiManager.setTimeout(120);

    //fetches ssid and pass and tries to connect
    //if it does not connect it starts an access point with the specified name
    //here  "AutoConnectAP"
    //and goes into a blocking loop awaiting configuration
    if(!wifiManager.autoConnect("slave_dev", "1234")) {
        Serial.println("failed to connect and hit timeout");
        delay(3000);
        //reset and try again, or maybe put it to deep sleep
        ESP.restart();
        delay(5000);
    }

    //if you get here you have connected to the WiFi
    Serial.println("connected...yeey :)");

    //read updated parameters
    strcpy(config.mqtt_server, custom_mqtt_server.getValue());
    strcpy(port_val, custom_mqtt_port.getValue());
    strcpy(config.mqtt_user, custom_mqtt_user.getValue());
    strcpy(config.mqtt_pass, custom_mqtt_pass.getValue());
    strcpy(config.device_id, custom_device_id.getValue());
    config.mqtt_port = atoi(port_val);
    

     //save the custom parameters to FS
    if(shouldSaveConfig) {
        shouldSaveConfig = false;
        saveConfigutation(CONFIG_FILE, config);
    }

    Serial.println("local ip");
    Serial.println(WiFi.localIP());

    client.setServer(config.mqtt_server, config.mqtt_port);
    client.setCallback(callback);
}


void reconnect() {
    // Loop until we're reconnected
     do{
        Serial.print("Attempting MQTT connection...");
        // Attempt to connect
        // If you do not want to use a username and password, change next line to
        // if (client.connect("ESP8266Client")) {
        if(client.connect(config.device_id, config.mqtt_user, config.mqtt_pass)) {
            Serial.println("connected");
        }
        else {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            // Wait 5 seconds before retrying
            delay(5000);
        }
    }while(!client.connected());
}

void public_data(const char* topic, byte channel, int32_t value){
    char topic_buf[40];
    char snum[10];
    sprintf_P(topic_buf, "%s/%s/%d", config.device_id, topic, channel);
    itoa(value, snum, 10);
    client.publish(topic_buf, snum);
    client.loop();
}

int analog_input(byte channel){
    //todo: get analog value
    int32_t val = analogRead(channel);
    public_data("aio", channel, val);
}

bool digital_io(byte channel, bool read, bool value)
{
    if(read) {
        int32_t val = digitalRead(read);
        public_data("aio", channel, val);
    }
    else {
        digitalWrite(channel, value);
    }
}

void pwm_output(byte channel, uint32_t wait_time, uint16_t pulse_count)
{
    for(uint16_t i = 0; i < pulse_count; i++){
        digitalWrite(channel, 1);
        delayMicroseconds(wait_time);
        digitalWrite(channel, 0);
        delayMicroseconds(wait_time);
    }
}

const char* topic_header[] = { "aio", "dio", "pwm" };

void callback(const char* topic, byte* payload, uint32_t length){
    
    for(byte i = 0; i < 3; i++) {
        char topic_devid[40];
        sprintf_P(topic_devid, "%s\%s", config.device_id, topic_header[i]);
        if(strcmp(topic, topic_devid)) {
            switch(i){
            case 0:
                if(length >= 1) {
                    analog_input(payload[0]);
                }
                break;
            case 1:
                if(length >= 3) {
                    digital_io(payload[0], payload[1], payload[2]);
                }
                break;
            case 2:
                if(length >= 7) {
                    uint32_t wait_time_us = 0;
                    uint16_t pulse_count = 0;

                    //big endian format
                    wait_time_us = payload[1] << 24 | payload[2] << 16 | payload[3] << 8 | payload[4];
                    pulse_count = payload[5] << 8 | payload[6];

                    pwm_output(payload[0], wait_time_us, pulse_count);
                }
                break;
            }
        }
    }
}

void loop() {
    // put your main code here, to run repeatedly:
    if(!client.connected()) {
        reconnect();
        for(byte i = 0; i < 3; i++) {
            char topic_devid[40];
            sprintf_P(topic_devid, "%s\%s", config.device_id, topic_header[i]);
            client.subscribe(topic_devid);
        }
    }

    client.loop();

}
