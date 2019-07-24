#ifndef __MAIN_H__
#define __MAIN_H__
#if CONFIG_FREERTOS_UNICORE
#define ARDUINO_RUNNING_CORE 0
#else
#define ARDUINO_RUNNING_CORE 1
#endif
#define MQTT_KEEPALIVE 3

#define BUZZAR_NOERROR 0
#define BUZZAR_WIFI_DOWN 1
#define BUZZAR_MQTT_DOWN 2
#define BUZZAR_STARTUP 3


#include <Arduino.h>

void setup_wifi();
void callback(char* topic, byte* message, unsigned int length);
uint8_t RssiToPercentage(int dBm);

#endif