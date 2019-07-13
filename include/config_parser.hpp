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

struct temperature
{
    uint8_t pin;
    String mqtt_id;
};

typedef struct temperature Temperature;

struct counter
{
    uint8_t pin;
    String mqtt_id;
    
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

struct boot{
    uint8_t pin;
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
    std::vector<Temperature> temperature_onewire;
    struct counter counter;
    struct boot boot;
    void load();
};

#endif