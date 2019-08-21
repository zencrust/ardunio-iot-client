

#include "config_parser.hpp"

static const char* station_names[] = 
{
    "XFA ASSEMBLY",
    "XFDPS ASSEMBLY",
    "XGA ASSEMBLY",
    "XPS ASSEMBLY",
    "RGA ASSEMBLY",
    "RPS ASSEMBLY",
    "SSSA ASSEMBLY",
    "SSSPS ASSEMBLY",
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

    switch_inp = {4, "Swicth Pressed"};
    lamp_do = {13, "Tower Light ON"};
}