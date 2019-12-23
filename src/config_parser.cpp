

#include "config_parser.hpp"

static const char* station_names[] = 
{
    "XFA ASSEMBLY", //0
    "XFDPS ASSEMBLY", //1
    "XGA ASSEMBLY", //2
    "XPS ASSEMBLY", //3
    "RGA ASSEMBLY", //4
    "RPS ASSEMBLY", //5
    "SSSA ASSEMBLY", //6
    "SSSPS ASSEMBLY", //7
    "DEMO1" //8
};

// Loads the configuration from a file
Configuration::Configuration(void)
{
    Serial.println("loading configuration....");

    boot = {
        12
    };

    switch_inp = {4, "Switch Pressed"};
    switch2_inp = {5, "Switch2 Pressed"};
    lamp_do = {13, "Tower Light ON"};
}