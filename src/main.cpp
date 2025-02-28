#include <main.hpp>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <driver/adc.h>
#include <time.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#include "config_parser.hpp"
#include "cert.h"
#include "esp_system.h"

#define POWER_ON_RESET_VAL 85
#define WIFI_CONNECT_TIMEOUT 10000 // in ms
#define TAG_TELEMETRY "telemetry"
#define TAG_UPDATE_TIME "last update time"
#define TAG_WIFI_SIGNAL "wifi Signal Strength"
#define TAG_DIO "dio"
#define TAG_TEMPERATURE "temp"
#define TAG_FREQUENCY "freq"

const int MEASUREMENTSAMPLETIME  = 20000;
const int TEMPERATURE_PRECISION = 10;
const int TEMPERATURE_RETRIES = 8;

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

const uint64_t wdtTimeout = 200;  //time in sec to trigger the watchdog
hw_timer_t *timer = NULL;
std::vector<std::tuple<uint8_t, String, DallasTemperature, OneWire>> temperatures;

int literpermin;
unsigned long currentTime, loopTime;
volatile unsigned int pulse_frequency = 0;


WiFiClientSecure espClient;

//WiFiClient espClient;
PubSubClient client(espClient);

void setup_wifi()
{
    delay(10);
    timerWrite(timer, 0);
    // We start by connecting to a WiFi network
    do
    {
        //espClient.setCACert(root_ca);
        Serial.println();
        Serial.print("Connecting to ");
        Serial.println(config.wifi.ssid);

        WiFi.disconnect();
        WiFi.begin(config.wifi.ssid.c_str(), config.wifi.password.c_str());
        WiFi.setHostname(config.device_id.c_str());
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

void setupTemperature()
{
    Serial.println("setting setupTemperature");
    uint8_t index = 0;
    for (auto &&channel : config.temperature_onewire)
    {
        temperatures.emplace_back(index, channel.mqtt_id, DallasTemperature(), OneWire(channel.pin));
        index++;
    }

    for (auto &&channel : temperatures)
    {
        std::get<2>(channel).setOneWire(&std::get<3>(channel));
        std::get<2>(channel).setResolution(TEMPERATURE_PRECISION);
    }

    Serial.println("setting setupTemperature done");
}

void readTemperatures()
{
    for (auto &&temp_channel : temperatures)
    {
        std::get<2>(temp_channel).setWaitForConversion(false);
        std::get<2>(temp_channel).requestTemperatures();
    }
    for (auto &&temp_channel : temperatures)
    {
        int retry = 0;
        bool isWritten = false;
        do
        {
            Serial.println(std::get<1>(temp_channel));
            
            int timeToWaitms = (std::get<2>(temp_channel).millisToWaitForConversion(TEMPERATURE_PRECISION)) + 30; //give 30ms more time to get temp
            auto initialTime = millis();
            do
            {
                delay(10);
                if ((millis() - initialTime) > timeToWaitms)
                {
                    break;
                }
            } while (!std::get<2>(temp_channel).isConversionComplete());

            auto Cel = std::get<2>(temp_channel).getTempCByIndex(0);
            Serial.println(Cel);
            auto cmp_val = int(Cel);
            if (DEVICE_DISCONNECTED_C != cmp_val && cmp_val != POWER_ON_RESET_VAL)
            {                
                public_data(TAG_TEMPERATURE, std::get<1>(temp_channel), Cel);
                isWritten = true;
                break;
            }
            retry++;
        }
        while (retry < TEMPERATURE_RETRIES);
        if (!isWritten)
        {
            Serial.println("write disconnected");
            public_data("temp", std::get<1>(temp_channel), "Disconnected");
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
        if (channel.activelow)
        {
            val = val ? 0 : 1;
        }

        public_data(TAG_DIO, channel.mqtt_id, val);
    }
}


void pulse_freq_measurement()
{
    currentTime = millis();

    //cannot measure less than 2hz
    if (currentTime >= (loopTime + 1000))
    {
        loopTime = currentTime;
        public_data(TAG_FREQUENCY, config.counter.mqtt_id, pulse_frequency);
        pulse_frequency = 0;
    }
}

#define CALCULATE_DUTYCUCLE(x) (x * 255) / 2000
void setBuzzar(uint8_t buzzar_value)
{
    switch (buzzar_value)
    {
    case BUZZAR_NOERROR:
        ledcWrite(ledChannel, 0);
        digitalWrite(config.mqtt_fault.pin, LOW);
        Serial.println("BUZZAR_NOERROR");
        break;
    case BUZZAR_WIFI_DOWN:
        ledcWrite(ledChannel, CALCULATE_DUTYCUCLE(500));
        digitalWrite(config.mqtt_fault.pin, HIGH);
        Serial.println("BUZZAR_WIFI_DOWN");
        break;
    case BUZZAR_MQTT_DOWN:
        ledcWrite(ledChannel, CALCULATE_DUTYCUCLE(250));
        digitalWrite(config.mqtt_fault.pin, HIGH);
        Serial.println("BUZZAR_MQTT_DOWN");
        break;
    case BUZZAR_STARTUP:
        ledcWrite(ledChannel, CALCULATE_DUTYCUCLE(1000));
        digitalWrite(config.mqtt_fault.pin, HIGH);
        Serial.println("BUZZAR_STARTUP");
        break;
    default:
        ledcWrite(ledChannel, 255);
    }
}

void callback(char *topic, byte *message, unsigned int length)
{
}

void reconnect()
{
    // Loop until we're reconnected
    Serial.print("reconnecting...");
    timerWrite(timer, 0);
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

void getFlow()
{
    pulse_frequency++;
}

void send_last_update_time()
{
    time_t now;
    time(&now);
    public_data(TAG_TELEMETRY, TAG_UPDATE_TIME, now, true);
}


void IRAM_ATTR resetModule() {
  ets_printf("reboot\n");
  esp_restart();
}

void set_client_cert()
{
    SPIFFS.begin();
    
    auto pub_key = SPIFFS.open(CLIENT_CERT_PUBLIC_KEY_PATH);
    auto priv_key = SPIFFS.open(CLIENT_CERT_PRIVATE_KEY_PATH);

    espClient.loadCertificate(pub_key, pub_key.size());
    espClient.loadPrivateKey(priv_key, pub_key.size());

}
void setup()
{
    timer = timerBegin(0, 80, true);                  //timer 0, div 80
    //espClient.setCACert(zencrust_cert);

    Serial.begin(115200);
    config.load();
    pinMode(config.boot.pin, OUTPUT);
    pinMode(config.mqtt_fault.pin, OUTPUT);

    ledcSetup(ledChannel, 0.5, 8);
    ledcAttachPin(config.boot.pin, ledChannel);

    setBuzzar(BUZZAR_STARTUP);
    setupTemperature();
    setup_wifi();
    client.setServer(config.mqtt_server.c_str(), config.mqtt_port);
    client.setCallback(callback);
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    reconnect();

    
    for (auto &&channel : config.di)
    {
        pinMode(channel.pin, INPUT);
    }

    if(config.counter.enabled){
        pinMode(config.counter.pin, INPUT);
        attachInterrupt(config.counter.pin, getFlow, FALLING);
    }

    timerWrite(timer, 0);
    timerAttachInterrupt(timer, &resetModule, true);  //attach callback
    timerAlarmWrite(timer, wdtTimeout * 1000 * 1000, false); //set time in us
    timerAlarmEnable(timer);                          //enable interrupt
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

void loop()
{
    auto curr_time = millis() - last_update_time;
    if (curr_time > MEASUREMENTSAMPLETIME)
    {
        Serial.println("task measuring...");
        Serial.println(curr_time);
        last_update_time = millis();
        analog_input();
        digital_input();
        if(config.counter.enabled){
            pulse_freq_measurement();
        }

        readTemperatures();

        uint8_t rssi = RssiToPercentage(WiFi.RSSI());
        public_data(TAG_TELEMETRY, TAG_WIFI_SIGNAL, rssi);
        send_last_update_time();
    }

    timerWrite(timer, 0);
    client.loop();
}
