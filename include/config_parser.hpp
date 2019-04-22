#ifndef __CONFIG_FILE_H__
#define __CONFIG_FILE_H__

#include <Arduino.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <SPIFFS.h>
#include <vector>

#define CONFIG_FILE "/config.json"

struct adc
{
    uint8_t pin;
    uint8_t attunation;
    String mqtt_id;
};

struct di
{
    uint8_t pin;
    String mqtt_id;
};

struct counter
{
    uint8_t pin;

    //in milliseconds
    uint16_t apature_time;
};

struct mqtt_config
{
    bool enable;
    String user_name;
    String password;
};

struct wifi
{
    String ssid;
    String password;
};

class Configuration
{
  public:
    String device_id;
    String mqtt_server;
    uint16_t mqtt_port;
    struct mqtt_config mqtt_config;
    struct wifi wifi;

    std::vector<struct adc> adc;
    std::vector<struct di> di;
    struct counter counter;

    void load();
};

#endif