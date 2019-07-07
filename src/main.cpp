#include <main.hpp>

#include <WiFi.h>
#include <PubSubClient.h>
#include <map>
#include <driver/adc.h>

#include "frequency_count.h"
#include "config_parser.hpp"

#include <OneWire.h>
#include <DallasTemperature.h>
#define TEMPERATURE_PRECISION 12


#define DEBUG(...) ESP_LOGD(__func__, ...)

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;

// LED Pin
const int ledPin = 2;
Configuration config;
const int a = ADC1_CHANNEL_6 ;

void setup_wifi()
{
    delay(10);
    // We start by connecting to a WiFi network
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(config.wifi.ssid);

    WiFi.begin(config.wifi.ssid.c_str(), config.wifi.password.c_str());

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
}

template <typename T>
void public_data(String topic, String channel, T value)
{
    String topic_buf = config.device_id + '/' + topic + '/' + channel;
    DEBUG("publish:");
    DEBUG(topic_buf);
    DEBUG("value:");
    DEBUG(value);
    client.publish(topic_buf.c_str(), String(value).c_str());
    client.loop();
}

std::vector<OneWire> onewires;
std::vector<std::tuple<String, DallasTemperature> > temperatures;

void setupTemperature()
{   
    for (auto &&channel : config.temperature_onewire){
        OneWire onewire(channel.pin);
        
        DallasTemperature temp(&onewire);
        temp.setResolution(TEMPERATURE_PRECISION);

        onewires.push_back(onewire);
        temperatures.push_back(std::make_tuple(channel.mqtt_id, temp));       
    }
}

void readTemperatures()
{
    for (auto &&temp_channel : temperatures)
    {
        std::get<1>(temp_channel).requestTemperatures();
        public_data("temp", std::get<0>(temp_channel), std::get<1>(temp_channel).getTempCByIndex(0));
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

void task(void *parameter)
{
    const TickType_t foursecdelay = 4000/portTICK_PERIOD_MS;
    const TickType_t secdelay = 1000/portTICK_PERIOD_MS;
    pulse_frequency = 0;
    while (1)
    {
        if (!client.connected())
        {
            Serial.println("task client not connected");
            vTaskDelay(foursecdelay);
            continue;
        }

        Serial.println("task measuring...");

        analog_input();
        digital_input();
        pulse_freq_measurement();
        readTemperatures();

        vTaskDelay(secdelay); // one tick delay (60ms) in between reads for stability
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
    xTaskCreatePinnedToCore(
        task,                        /* Task function. */
        "ana_dig",                   /* String with name of task. */
        2048,                        /* Stack size in bytes. */
        NULL,                        /* Parameter passed as input of the task */
        1,                           /* Priority of the task. */
        NULL, ARDUINO_RUNNING_CORE); /* Task handle. */
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

    setup_wifi();
    client.setServer(config.mqtt_server.c_str(), config.mqtt_port);
    client.setCallback(callback);
    pinMode(ledPin, OUTPUT);
    reconnect();
    task_create();

    pinMode(config.counter.pin, INPUT);
    for (auto &&channel : config.di)
    {
        pinMode(channel.pin, INPUT);
    }

    setupTemperature();
    attachInterrupt(config.counter.pin, getFlow, FALLING);
}

void loop()
{
    if (!client.connected())
    {
        reconnect();
    }

    client.loop();
}
