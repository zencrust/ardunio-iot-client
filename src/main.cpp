#include <main.hpp>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <driver/adc.h>

#include "frequency_count.h"
#include "config_parser.hpp"

#include <OneWire.h>
#include <DallasTemperature.h>
const int TEMPERATURE_PRECISION = 10;
const int TEMPERATURE_RETRIES = 5;

//#define DEBUG(...) ESP_LOGD(__func__, ...)

WiFiClientSecure espClient;
//WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;

// LED Pin
const int ledPin = 2;
Configuration config;
const int a = ADC1_CHANNEL_6;

const int ledChannel = 0;
void setup_wifi()
{
    delay(10);
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
        int i = 0;
        while (!WiFi.isConnected())
        {
            delay(500);
            Serial.print(".");
            if (i > 20)
            {
                break;
            }

            i++;
        }

    } while (!WiFi.isConnected());

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
void public_data(String topic, String channel, T value)
{
    String topic_buf = config.device_id + '/' + topic + '/' + channel;
    // DEBUG("publish:");
    // DEBUG(topic_buf);
    // DEBUG("value:");
    // DEBUG(value);
    MqttErr(client.publish(topic_buf.c_str(), String(value).c_str()));
    client.loop();
}

std::vector<std::tuple<uint8_t, String, DallasTemperature, OneWire>> temperatures;
std::vector<int> disconnected_times;
void setupTemperature()
{
    Serial.println("setting setupTemperature");
    uint8_t index = 0;
    for (auto &&channel : config.temperature_onewire)
    {
        temperatures.emplace_back(index, channel.mqtt_id, DallasTemperature(), OneWire(channel.pin));
        disconnected_times.push_back(0);
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

            if (DEVICE_DISCONNECTED_C != Cel)
            {
                public_data("temp", std::get<1>(temp_channel), Cel);
                auto index = std::get<0>(temp_channel);
                disconnected_times[index] = 0;
                isWritten = true;
                break;
            }
            retry++;
        } while (retry < TEMPERATURE_RETRIES);
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

        public_data("dio", channel.mqtt_id, val);
    }
}

int literpermin;
unsigned long currentTime, loopTime;
volatile unsigned int pulse_frequency = 0;

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
                                    MQTTQOS0, true, "Disconnected");
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

byte sensorInterrupt = 0;
void getFlow()
{
    pulse_frequency++;
}


void setup()
{
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

    reconnect();

    pinMode(config.counter.pin, INPUT);
    for (auto &&channel : config.di)
    {
        pinMode(channel.pin, INPUT);
    }

    attachInterrupt(config.counter.pin, getFlow, FALLING);
}

uint8_t RssiToPercentage(int dBm)
{
    if (dBm <= -100)
        return 0;
    if (dBm >= -50)
        return 100;
    return 2 * (dBm + 100);
}
#define MEASUREMENTSAMPLETIME 5000

void loop()
{
    Serial.println("task measuring...");
    auto initial_time = millis();
    analog_input();
    digital_input();
    pulse_freq_measurement();
    readTemperatures();

    uint8_t rssi = RssiToPercentage(WiFi.RSSI());
    public_data("telemetry", "wifi Signal Strength", rssi);
    auto final_time = millis();
    long delay_time = MEASUREMENTSAMPLETIME - (final_time - initial_time);
    if (delay_time > 0)
    {
        delay(delay_time);
    }
}
