

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
};

// Loads the configuration from a file
Configuration::Configuration(void)
{
    Serial.println("loading configuration....");
    device_id = station_names[0];
    mqtt_server = "SmartDashboard.local";
    mqtt_port = 1883;
    mqtt_config = {
        false, "", ""
    };

    wifi = {
        "SmartDashboard",
        "SmartDashboard@1"
    };

    boot = {
        12
    };

    switch_inp = {4, "Switch Pressed"};
    lamp_do = {13, "Tower Light ON"};
}