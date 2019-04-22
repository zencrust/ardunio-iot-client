#include <WiFi.h>
#include <PubSubClient.h>

#include "main.hpp"
#include "frequency_count.h"
#include "config_parser.hpp"


WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;

// LED Pin
const int ledPin = 2;
Configuration config;

void setup()
{
    Serial.begin(115200);
    config.load();

    setup_wifi();
    client.setServer(config.mqtt_server.c_str(), config.mqtt_port);
    client.setCallback(callback);

    pinMode(ledPin, OUTPUT);
    
}

void setup_wifi()
{
    delay(10);
    // We start by connecting to a WiFi network
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(config.wifi.ssid);

    WiFi.begin(config.wifi.ssid.c_str(), config.wifi.password.c_str());

    while (WiFi.status() != WL_CONNECTED){
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
    Serial.print("publish:");
    Serial.println(topic_buf);
    Serial.print("value:");
    Serial.println(value);
    client.publish(topic_buf.c_str(), String(value).c_str());
    client.loop();
}

void analog_input(struct adc channel)
{
    Serial.println("reading analog input ...");
    int32_t val = analogRead(channel.pin);
    public_data("aio", channel.mqtt_id, val);
}

void digital_input(struct di channel)
{
    int32_t val = digitalRead(channel.pin);
    public_data("dio", channel.mqtt_id, val);
}

void pulse_freq_measurement()
{
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

        if(config.mqtt_config.enable){
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

void loop()
{
    if (!client.connected())
    {
        reconnect();
    }

    for (auto &&n : config.adc)
    {
        analog_input(n);
    }

    for (auto &&n : config.di)
    {
        digital_input(n);
    }

    client.loop();
}
