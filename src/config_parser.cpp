

#include "config_parser.hpp"

// Loads the configuration from a file
void Configuration::load(void)
{
    SPIFFS.begin(true);
    Serial.println("loading configuration....");

    // Open file for reading
    File file = SPIFFS.open(CONFIG_FILE);
    StaticJsonDocument<2048> config_json;
    
    // Allocate a temporary JsonDocument
    // Don't forget to change the capacity to match your requirements.
    // Use arduinojson.org/v6/assistant to compute the capacity.

    // Deserialize the JSON document
    DeserializationError error = deserializeJson(config_json, file);
    if (error)
    {
        Serial.println("Failed to read file, using default configuration");
        return;
    }
    else
    {
        Serial.println("config file parsing successful!");
    }

    device_id = String((const char *)config_json["device_id"]);
    mqtt_server = String((const char *)config_json["mqtt_server"]);
    mqtt_port = config_json["mqtt_port"];

    mqtt_config.enable = config_json["mqtt_config"]["enable"];

    mqtt_config.user_name = String((const char *)config_json["mqtt_config"]["user_name"]);
    mqtt_config.password = String((const char *)config_json["mqtt_config"]["password"]);
    wifi.password = String((const char *)config_json["wifi"]["password"]);
    wifi.ssid = String((const char *)config_json["wifi"]["ssid"]);

    for(int i = 0; i<5; i++){
        struct adc ax = {
            (uint8_t)config_json["adc"][i]["pin"],
            (uint8_t)config_json["adc"][i]["attunation"],
            String((const char *)config_json["adc"][i]["mqtt_id"])};
        adc.push_back(ax);

        struct di d = {
            (uint8_t)config_json["di"][i]["pin"],
            String((const char *)config_json["di"][i]["mqtt_id"])};

        di.push_back(d);
    }


    Serial.println("config loaded...");
    // Close the file (Curiously, File's destructor doesn't close the file)
    file.close();
}