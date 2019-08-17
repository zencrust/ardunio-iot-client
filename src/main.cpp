#include <main.hpp>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <time.h>
#include "InputDebounce.h"
#include "config_parser.hpp"

#define MEASUREMENTSAMPLETIME 1000
#define WDTIMEOUT 90000
#define DEBOUNCE_DELAY 750

#define POWER_ON_RESET_VAL 85
#define WIFI_CONNECT_TIMEOUT 10000 // in ms
#define TAG_TELEMETRY "telemetry"
#define TAG_UPDATE_TIME "last update time"
#define TAG_WIFI_SIGNAL "wifi Signal Strength"
#define TAG_DIO "dio"

//#define DEBUG(...) ESP_LOGD(__func__, ...)
const char *ntpServer = "in.pool.ntp.org";
const long gmtOffset_sec = 19800;
const int daylightOffset_sec = 0;

long lastMsg = 0;
char msg[50];
int value = 0;

// LED Pin
const int ledPin = 2;
Configuration config;

const int ledChannel = 0;

const uint64_t wdtTimeout = 90;  //time in sec to trigger the watchdog

// WiFiClientSecure espClient;

WiFiClient espClient;
PubSubClient client(espClient);

void setup_wifi()
{
    delay(10);
    // We start by connecting to a WiFi network
    ESP.wdtFeed();
    do
    {
        //espClient.setCACert(root_ca);
        Serial.println();
        Serial.print("Connecting to ");
        Serial.println(config.wifi.ssid);

        WiFi.disconnect();
        WiFi.begin(config.wifi.ssid.c_str(), config.wifi.password.c_str());
        setBuzzar(BUZZAR_WIFI_DOWN);
        
        auto initial_time = millis();
        //wait for 10 seconds
        do
        {
            delay(500);
            Serial.print(".");
        }
        while ((millis() - initial_time) < WIFI_CONNECT_TIMEOUT && (!WiFi.isConnected()));

    } while (!WiFi.isConnected());
    espClient.setNoDelay(true);
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
}

void MqttErr(bool success)
{
    if (!success)
    {
        reconnect();
    }
}

template <typename T>
void public_data(String function, String channel, T value, bool retained = false)
{   
    if(!client.connected()){
        reconnect();
    }

    String topic_buf = config.device_id + '/' + function + '/' + channel;
    client.publish(topic_buf.c_str(), String(value).c_str(), retained);
}

void switch_pressed_callback(uint8_t pinIn)
{
    public_data(TAG_DIO, config.switch_inp.mqtt_id, 0, true);
}

void switch_released_callback(uint8_t pinIn)
{
    time_t now;
    time(&now);
    public_data(TAG_DIO, config.switch_inp.mqtt_id, now, true);
}

#define CALCULATE_DUTYCUCLE(x) (x * 255) / 2000
void setBuzzar(uint8_t buzzar_value)
{
    switch (buzzar_value)
    {
    case BUZZAR_NOERROR:
        digitalWrite(config.boot.pin, LOW);
        Serial.println("BUZZAR_NOERROR");
        break;
    case BUZZAR_WIFI_DOWN:
        digitalWrite(config.boot.pin, HIGH);
        Serial.println("BUZZAR_WIFI_DOWN");
        break;
    case BUZZAR_MQTT_DOWN:
        digitalWrite(config.boot.pin, HIGH);
        Serial.println("BUZZAR_MQTT_DOWN");
        break;
    case BUZZAR_STARTUP:
        digitalWrite(config.boot.pin, HIGH);
        Serial.println("BUZZAR_STARTUP");
        break;
    default:
        digitalWrite(config.boot.pin, HIGH);
    }
}

void callback(char *topic, byte *message, unsigned int length)
{
}

void reconnect()
{
    // Loop until we're reconnected
    Serial.print("reconnecting...");
    ESP.wdtFeed();
    while (!client.connected())
    {
        setBuzzar(BUZZAR_MQTT_DOWN);
        if (!WiFi.isConnected())
        {
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
                                    MQTTQOS0, false, "Disconnected");
        }
        else
        {
            result = client.connect(config.device_id.c_str(), willMessage.c_str(), MQTTQOS0, false, "Disconnected");
            Serial.println("mqtt connect without auth");
            Serial.println(config.device_id);
            Serial.println(config.mqtt_server);
            Serial.println(config.mqtt_port);
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

    setBuzzar(BUZZAR_NOERROR);
}

void send_last_update_time()
{
    time_t now;
    time(&now);
    public_data(TAG_TELEMETRY, TAG_UPDATE_TIME, now, true);
}

static InputDebounce switch_pressed;
void setup()
{
    Serial.begin(115200);
    pinMode(config.boot.pin, OUTPUT);

    setBuzzar(BUZZAR_STARTUP);
    setup_wifi();
    client.setServer(config.mqtt_server.c_str(), config.mqtt_port);
    client.setCallback(callback);
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    reconnect();

    switch_pressed.registerCallbacks(switch_pressed_callback, switch_released_callback, NULL, NULL);
    switch_pressed.setup(config.switch_inp.pin, DEBOUNCE_DELAY, InputDebounce::PIM_EXT_PULL_DOWN_RES, 0, 
    InputDebounce::ST_NORMALLY_OPEN);
    pinMode(config.switch_inp.pin, INPUT);
    pinMode(config.lamp_do.pin, OUTPUT);
    ESP.wdtEnable(WDTIMEOUT);
}

uint8_t RssiToPercentage(int dBm)
{
    if (dBm <= -100)
        return 0;
    if (dBm >= -50)
        return 100;
    return 2 * (dBm + 100);
}

unsigned long last_update_time = 0;
unsigned long last_update_time_telemetry = 0;
void loop()
{
    switch_pressed.process(millis());
    auto curr_time_tele = millis() - last_update_time_telemetry;

    if(curr_time_tele > 5000)
    {
        uint8_t rssi = RssiToPercentage(WiFi.RSSI());
        public_data(TAG_TELEMETRY, TAG_WIFI_SIGNAL, rssi);
        send_last_update_time();
        last_update_time_telemetry = millis();
    }

    ESP.wdtFeed();
    delay(100); // [ms]
    client.loop();
}
