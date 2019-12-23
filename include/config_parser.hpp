#ifndef __CONFIG_FILE_H__
#define __CONFIG_FILE_H__

#include <Arduino.h>

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
    bool enabled;
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
    struct mqtt_config mqtt_config;

    struct di switch_inp;
    struct di switch2_inp;
    struct di lamp_do;
    struct boot boot;
    Configuration();
};

#endif