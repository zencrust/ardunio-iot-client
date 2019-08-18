

#include "config_parser.hpp"

// Loads the configuration from a file
Configuration::Configuration(void)
{
    Serial.println("loading configuration....");
    device_id = "station1";
    mqtt_server = "SmartDashBoard.local";
    mqtt_port = 1883;
    mqtt_config = {
        false, "", ""
    };

    wifi = {
        "Harsha_2p4",
        "passw0rD"
    };

    boot = {
        2
    };

    switch_inp = {5, "Swicth Pressed"};
    lamp_do = {13, "Tower Light ON"};
}