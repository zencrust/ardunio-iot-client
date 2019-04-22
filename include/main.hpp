#ifndef __MAIN_H__
#define __MAIN_H__

#include <Arduino.h>
#include "pins_arduino.h"


void setup_wifi();
void callback(char* topic, byte* message, unsigned int length);

#endif