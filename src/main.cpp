#include <main.hpp>
#include <WiFi.h>
#include <PubSubClient.h>
#include <driver/adc.h>

#include "frequency_count.h"
#include "config_parser.hpp"

#include <OneWire.h>
#include <DallasTemperature.h>
#define TEMPERATURE_PRECISION 12


//#define DEBUG(...) ESP_LOGD(__func__, ...)

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;

// LED Pin
const int ledPin = 2;
Configuration config;
const int a = ADC1_CHANNEL_6 ;

uint8_t buzzer_previous_value = -1;
uint8_t buzzar_last_milliseconds = 0;
volatile uint8_t buzzar_value = BUZZAR_STARTUP;
//SemaphoreHandle_t mutex;
//portMUX_TYPE mmux = portMUX_INITIALIZER_UNLOCKED;

void setBuzzar(uint8_t val){
    //xSemaphoreTake(mutex, portMAX_DELAY);
    buzzar_value = val;
    //xSemaphoreGive(mutex);
}

void setup_wifi()
{
    delay(10);
    // We start by connecting to a WiFi network
    do
    {
        Serial.println();
        Serial.print("Connecting to ");
        Serial.println(config.wifi.ssid);
        
        WiFi.disconnect();
        WiFi.begin(config.wifi.ssid.c_str(), config.wifi.password.c_str());
        WiFi.setHostname(config.device_id.c_str());
        setBuzzar(BUZZAR_WIFI_DOWN);
        int i = 0;
        while (!WiFi.isConnected())
        {
            delay(500);
            Serial.print(".");
            if(i > 20){
                break;
            }

            i++;
        }

    }while(!WiFi.isConnected());

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
}

template <typename T>
void public_data(String topic, String channel, T value)
{
    String topic_buf = config.device_id + '/' + topic + '/' + channel;
    // DEBUG("publish:");
    // DEBUG(topic_buf);
    // DEBUG("value:");
    // DEBUG(value);
    client.publish(topic_buf.c_str(), String(value).c_str());
    client.loop();
}

std::vector<std::tuple<uint8_t, String, DallasTemperature, OneWire> > temperatures;
std::vector<int> disconnected_times;
void setupTemperature()
{   
    Serial.println("setting setupTemperature");
    uint8_t index = 0;    
    for (auto &&channel : config.temperature_onewire){
        temperatures.emplace_back(index, channel.mqtt_id, DallasTemperature(), OneWire(channel.pin));
        disconnected_times.push_back(0);
        index++;
    }

    for (auto &&channel : temperatures){
        std::get<2>(channel).setOneWire(&std::get<3>(channel));
        std::get<2>(channel).setResolution(12);
    }

    Serial.println("setting setupTemperature done");
}

void readTemperatures()
{
    for (auto &&temp_channel : temperatures)
    {
        Serial.println(std::get<1>(temp_channel));
        std::get<2>(temp_channel).requestTemperatures();
        auto Cel = std::get<2>(temp_channel).getTempCByIndex(0);
        Serial.println(Cel);

        if(DEVICE_DISCONNECTED_C != Cel){
            public_data("temp", std::get<1>(temp_channel), Cel);
            auto index = std::get<0>(temp_channel);
            disconnected_times[index] = 0;

        }
        else{
            auto index = std::get<0>(temp_channel);
            disconnected_times[index]++;
            Serial.println(disconnected_times[index]);
            if(disconnected_times[index] >= 10)
            {
                Serial.println("write disconnected");
                disconnected_times[index] = 0;
                public_data("temp", std::get<1>(temp_channel), "Disconnected");
            }
        }        
    }
}

void analog_input()
{
    for (auto &&channel : config.adc)
    {
        int32_t val = analogRead(channel.pin);
        public_data("aio", channel.mqtt_id, val);
    }
}

void digital_input()
{
    for (auto &&channel : config.di)
    {
        int32_t val = digitalRead(channel.pin);
        if(channel.activelow){
            val = val ? 0 : 1;
        }
        
        public_data("dio", channel.mqtt_id, val);
    }
}

int literpermin;
unsigned long currentTime, loopTime;
volatile unsigned int pulse_frequency;

void pulse_freq_measurement()
{
    currentTime = millis();

    //cannot measure less than 2hz
    if (currentTime >= (loopTime + 1000))
    {
        loopTime = currentTime;
        public_data("freq", config.counter.mqtt_id, pulse_frequency);
        pulse_frequency = 0;
    }
}



void buzzar_task(void *parameter){
    while(1){

        //if( xSemaphoreTake(mutex, 10/portTICK_RATE_MS) == pdTRUE)
        {
            uint8_t tmp_buzzar_val = buzzar_value;
            if(tmp_buzzar_val != buzzer_previous_value){
                buzzar_last_milliseconds = millis();
                buzzer_previous_value = tmp_buzzar_val;
                //Serial.print("new buzzar value");
                //Serial.println(buzzar_value);
            }
            unsigned long tmp;
            switch(tmp_buzzar_val){
                case BUZZAR_NOERROR:
                    digitalWrite(config.boot.pin, LOW);
                    digitalWrite(config.mqtt_fault.pin, LOW);
                    //Serial.println("buzzar No Error");
                    break;
                case BUZZAR_WIFI_DOWN:
                    tmp = millis() - buzzar_last_milliseconds;
                    tmp = tmp %1000;
                    if(( tmp/250) == 0)
                    {
                        digitalWrite(config.boot.pin, HIGH);
                    }
                    else
                    {
                        digitalWrite(config.boot.pin, LOW);
                    }
                    digitalWrite(config.mqtt_fault.pin, HIGH);
                    //Serial.println("buzzar WIFI_DOWN");
                    break;
                case BUZZAR_MQTT_DOWN:
                    tmp = millis() - buzzar_last_milliseconds;
                    tmp = tmp %2000;
                    if(( tmp/500) == 0)
                    {
                        digitalWrite(config.boot.pin, HIGH);
                    }
                    else
                    {
                        digitalWrite(config.boot.pin, LOW);
                    }
                    digitalWrite(config.mqtt_fault.pin, HIGH);
                    //Serial.println("buzzar MQTT_DOWN");
                    break;
                case BUZZAR_STARTUP:
                    tmp = millis() - buzzar_last_milliseconds;
                    tmp = tmp %2000;
                    if(( tmp/250) == 0)
                    {
                        digitalWrite(config.boot.pin, HIGH);
                    }
                    else
                    {
                        digitalWrite(config.boot.pin, LOW);
                    }
                    digitalWrite(config.mqtt_fault.pin, HIGH);
                    //Serial.println("buzzar STARTUP");
                    break;
                default:
                    digitalWrite(config.boot.pin, HIGH);   
                
            }

            //xSemaphoreAltGive(mutex);
        }
        delay(250);
    }
}

void task(void *parameter)
{
    pulse_frequency = 0;
    while (1)
    {
        if (!client.connected())
        {
            Serial.println("task client not connected");
            delay(4000);
            continue;
        }

        Serial.println("task measuring...");

        analog_input();
        digital_input();
        pulse_freq_measurement();
        readTemperatures();

        uint8_t rssi = RssiToPercentage(WiFi.RSSI());
        public_data("telemetry", "wifi Signal Strength", rssi);
        delay(1000);
    }
}

void callback(char *topic, byte *message, unsigned int length)
{
}

void reconnect()
{
    // Loop until we're reconnected
    while (!client.connected())
    {
        if(!WiFi.isConnected()){
            setup_wifi();
        }

        Serial.print("Attempting MQTT connection...");
        // Attempt to connect
        boolean result;
        String willMessage = config.device_id + "/" + "heartbeat";
        if (config.mqtt_config.enable)
        {
            result = client.connect(config.device_id.c_str(), config.mqtt_config.user_name.c_str(), 
            config.mqtt_config.password.c_str(), willMessage.c_str(),
            MQTTQOS0, true, "Disconnected");
        }
        else
        {          
            result = client.connect(config.device_id.c_str(), willMessage.c_str(), MQTTQOS0,true, "Disconnected");
            Serial.println("mqtt connect without auth");
            Serial.println(config.device_id);
            Serial.println(config.mqtt_server);
        }
        if (result)
        {
            Serial.println("connected");
            // Subscribe to events here
        }
        else
        {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            // Wait 5 seconds before retrying
            delay(5000);
        }
    }
}

void task_create()
{
    Serial.println("creating task");
    xTaskCreate(
        task,                        /* Task function. */
        "ana_dig",                   /* String with name of task. */
        2048,                        /* Stack size in bytes. */
        NULL,                        /* Parameter passed as input of the task */
        1,                           /* Priority of the task. */
        NULL); /* Task handle. */
}

void task_buzzar_create()
{
    Serial.println("creating buzzar ask");
    xTaskCreate(
        buzzar_task,                        /* Task function. */
        "buzzar_task",                   /* String with name of task. */
        1024,                        /* Stack size in bytes. */
        NULL,                        /* Parameter passed as input of the task */
        1,                           /* Priority of the task. */
        NULL); /* Task handle. */
}

byte sensorInterrupt = 0;
void getFlow()
{
    pulse_frequency++;
}

void setup()
{
    Serial.begin(115200);
    config.load();
    //mutex = xSemaphoreCreateMutex();
    pinMode(config.boot.pin, OUTPUT);
    pinMode(config.mqtt_fault.pin, OUTPUT);
    task_buzzar_create();
    buzzar_value = BUZZAR_STARTUP;

    //digitalWrite(config.boot.pin, HIGH);

    setupTemperature();
    setup_wifi();
    client.setServer(config.mqtt_server.c_str(), config.mqtt_port);
    client.setCallback(callback);

    
    reconnect();
    task_create();

    pinMode(config.counter.pin, INPUT);
    for (auto &&channel : config.di)
    {
        pinMode(channel.pin, INPUT_PULLDOWN);
    }
   
    attachInterrupt(config.counter.pin, getFlow, FALLING);
}

uint8_t RssiToPercentage(int dBm)
{
    if(dBm <= -100)
        return 0;
    if(dBm >= -50)
        return 100;
    return 2 * (dBm + 100);
}

void loop()
{
    if (!client.connected())
    {
        //digitalWrite(config.boot.pin, HIGH);
        //digitalWrite(config.mqtt_fault.pin, HIGH);
        setBuzzar(BUZZAR_MQTT_DOWN);
        reconnect();
    }

    //digitalWrite(config.boot.pin, LOW);
    //digitalWrite(config.mqtt_fault.pin, HIGH);
    setBuzzar(BUZZAR_NOERROR);
    client.loop();
}
