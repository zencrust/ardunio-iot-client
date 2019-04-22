#include <WiFi.h>
#include <PubSubClient.h>
#include <map>

#include "main.hpp"
#include "frequency_count.h"
#include "config_parser.hpp"

#define DEBUG(...) ESP_LOGD(__func__, ...)

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;

// LED Pin
const int ledPin = 2;
Configuration config;

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

void public_data(String topic, String channel, int32_t value)
{
    String topic_buf = config.device_id + '/' + topic + '/' + channel;
    DEBUG("publish:");
    DEBUG(topic_buf);
    DEBUG("value:");
    DEBUG(value);
    client.publish(topic_buf.c_str(), String(value).c_str());
    client.loop();
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
        literpermin = (pulse_frequency / 7.5);
        pulse_frequency = 0;
        public_data("freq", config.counter.mqtt_id, literpermin);
        Serial.print(literpermin, DEC);
        Serial.println(" Liter/min");
    }
}

void task(void *parameter)
{
    while (1)
    {
        if (!client.connected())
        {
            return;
        }

        analog_input();
        digital_input();
        pulse_freq_measurement();

        vTaskDelay(40); // one tick delay (60ms) in between reads for stability
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

        if (config.mqtt_config.enable)
        {
            result = client.connect(config.device_id.c_str(), config.mqtt_config.user_name.c_str(), config.mqtt_config.password.c_str());
        }
        else
        {
            result = client.connect(config.device_id.c_str());
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

    attachInterrupt(sensorInterrupt, getFlow, FALLING);
}

void loop()
{
    if (!client.connected())
    {
        reconnect();
    }

    client.loop();
}
