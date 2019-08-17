

#include "config_parser.hpp"

// Loads the configuration from a file
Configuration::Configuration(void)
{
    Serial.println("loading configuration....");
    device_id = "partalarm\\station1";
    mqtt_server = "zencrust.cf";
    mqtt_port = 1883;
    mqtt_config = {
        false, "", ""
    };

    wifi = {
        "",
        ""
    };

    boot = {
        12
    };

    switch_inp = {1, "Swicth Pressed"};
    lamp_do = {2, "Tower Light ON"};
}