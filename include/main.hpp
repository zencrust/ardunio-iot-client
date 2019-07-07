#ifndef __MAIN_H__
#define __MAIN_H__
#if CONFIG_FREERTOS_UNICORE
#define ARDUINO_RUNNING_CORE 0
#else
#define ARDUINO_RUNNING_CORE 1
#endif
#define MQTT_KEEPALIVE 2

#include <Arduino.h>

void setup_wifi();
void callback(char* topic, byte* message, unsigned int length);

#endif