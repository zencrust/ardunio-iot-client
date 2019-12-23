#include <main.hpp>
#include <PubSubClient.h>
#include <time.h>
#include "InputDebounce.h"
#include "config_parser.hpp"
#include <ArduinoOTA.h>
#include <IotWebConf.h>
#define FIRMWARE_VERSION "v3.0"
// -- Initial name of the Thing. Used e.g. as SSID of the own Access Point.

// -- Initial password to connect to the Thing, when it creates an own Access Point.
const char wifiInitialApPassword[] = "12345678";
DNSServer dnsServer;
WebServer server(80);

const char *getName()
{
    String ssid = "KitMonitor" + String(ESP.getChipId());
    return ssid.c_str();
}
#define STRING_LEN 128
char mqttServerValue[STRING_LEN];
char mqttServerportValue[10] = "1883";
char ntpServerValue[STRING_LEN] = {};

IotWebConfParameter configParameters[] = {
    IotWebConfParameter("MQTT server", "mqttServer", mqttServerValue, STRING_LEN, "text", NULL, "broker.hivemq.com"),
    IotWebConfParameter("MQTT Port", "mqttserverport", mqttServerportValue, sizeof(mqttServerportValue), "number", NULL, "1883"),
    IotWebConfParameter("NTP server", "ntpServerValue", ntpServerValue, sizeof(ntpServerValue), "text", NULL, "smartdashboard.local")};

IotWebConf iotWebConf(getName(), &dnsServer, &server, wifiInitialApPassword, FIRMWARE_VERSION);

void IotWebConfSetup()
{
    for (auto &par : configParameters)
    {
        iotWebConf.addParameter(&par);
    }
}

#define MEASUREMENTSAMPLETIME 1000
#define WDTIMEOUT 90000
#define DEBOUNCE_DELAY 1000

#define POWER_ON_RESET_VAL 85
#define TAG_TELEMETRY "telemetry"
#define TAG_UPDATE_TIME "last update time"
#define TAG_WIFI_SIGNAL "wifi Signal Strength"
#define TAG_DIO "dio"

//#define DEBUG(...) ESP_LOGD(__func__, ...)
const long gmtOffset_sec = 19800;
const int daylightOffset_sec = 0;

long lastMsg = 0;
char msg[50];
int value = 0;

// LED Pin
const int ledPin = 2;
Configuration config;

const int ledChannel = 0;

const uint64_t wdtTimeout = 90; //time in sec to trigger the watchdog
static InputDebounce switch_pressed;
static InputDebounce switch_pressed2;

//WiFiClientSecure espClient;

WiFiClient espClient;
PubSubClient client(espClient);

void handleRoot()
{
    // -- Let IotWebConf test and handle captive portal requests.
    if (iotWebConf.handleCaptivePortal())
    {
        // -- Captive portal request were already served.
        return;
    }

    time_t now = time(NULL);
    char current_time[20], temp[20];
    strftime(current_time, 20, "%b %d %Y %H:%M:%S", localtime(&now));

    String s = "<!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/>";
    s += "<title>Kit Monitor - Device</title>";
    s += "<style> table, th, td { \
        border: 1px solid black; \
        border-collapse: collapse; \
        } \
        th, td { \
            padding: 10px ; \
        } \
        header { \
            background-color: #666; \
            padding: 10px; \
            text-align: center; \
            color: white; \
        } \
        footer { \
        background-color: #777; \
        padding: 10px; \
        position: absolute; \
        bottom: 0; \
        width: 100%; \
        text-align: center; \
        color: white; } \
        section { \
            padding: 20px; \
        } \
        </style>";
    s += "</head><body><header><h1>Kit Monitor Device Configuration</h1></header>";
    s += "<h3> Go to <a href='config'>configure page</a> to change values.</h3>";
    s += "<section><table style=\"width:100%\">";
    s += "<tr>";
    s += "<th>Parameter</th>";
    s += "<th>Value</th>";
    s += "</tr>";
    s += "<tr>";
    s += "<td>Device Name</td>";
    s += "<td>";
    s += iotWebConf.getThingName();
    s += "</td>";
    s += "<tr>";
    s += "<td>Serial Number</td>";
    s += "<td>";
    s += ESP.getChipId();
    s += "</td>";
    s += "</tr>";

    s += "<tr>";
    s += "<td>Firmware build</td>";
    s += "<td>";
    s += FIRMWARE_VERSION ", " __DATE__  " " __TIME__;
    s += "</td>";
    s += "</tr>";
    s += "<tr>";

    s += "<tr>";
    s += "<td>MQTT server</td>";
    s += "<td>";
    s += mqttServerValue;
    s += "</td>";
    s += "</tr>";
    s += "<tr>";
    s += "<td>MQTT Port</td>";
    s += "<td>";
    s += mqttServerportValue;
    s += "</td>";
    s += "</tr>";
    s += "<tr>";
    s += "<td>NTP Server</td>";
    s += "<td>";
    s += ntpServerValue;
    s += "</td>";
    s += "</tr>";
    s += "<tr>";
    s += "<td>Current Time</td>";
    s += "<td>";
    s += current_time;
    s += "</td>";
    s += "</tr>";
    s += "<td>Memory Available</td>";
    s += "<td>";
    s += itoa(system_get_free_heap_size(), temp, 10);
    s += "</td>";
    s += "</tr>";
    s += "</table></section>";
    // s += "<footer><p>2019 Smart Dashboard</p></footer>";
    s += "</body></html>\n";

    server.send(200, "text/html", s);
}

void setup_wifi()
{
    delay(10);
    // We start by connecting to a WiFi network
    ESP.wdtFeed();

    //espClient.setCACert(root_ca);
    Serial.println();
    Serial.print("Connecting to ");
    // -- Initializing the configuration.
    iotWebConf.init();

    // -- Set up required URL handlers on the web server.
    server.on("/", handleRoot);
    server.on("/config", [] { iotWebConf.handleConfig(); });
    server.onNotFound([]() { iotWebConf.handleNotFound(); });

    setBuzzar(BUZZAR_WIFI_DOWN);

    do
    {
        delay(10);
        iotWebConf.doLoop();
    } while (!WiFi.isConnected());

    espClient.setNoDelay(true);
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
}

void MqttErr(bool success)
{
    if (!success)
    {
        reconnect();
    }
}

template <typename T>
void public_data(String function, String channel, T value, bool retained = false)
{
    if (!client.connected())
    {
        reconnect();
    }

    String topic_buf = "partalarm/" + String(iotWebConf.getThingName()) + '/' + function + '/' + channel;
    client.publish(topic_buf.c_str(), String(value).c_str(), retained);
}
#define GET_MQTT_ID(pinIn) pinIn == config.switch_inp.pin ? config.switch_inp.mqtt_id : config.switch2_inp.mqtt_id

String get_switch_index(uint8_t pinIn)
{
    return pinIn == config.switch_inp.pin ? "0" : "1";
}

void switch_pressed_callback(uint8_t pinIn)
{
    digitalWrite(config.lamp_do.pin, HIGH);
    Serial.println("Switch" + get_switch_index(pinIn) + "Pressed");
    public_data(TAG_DIO, GET_MQTT_ID(pinIn), 1, false);
}

void switch_untill_pressed_callback(uint8_t pinIn, unsigned long duration)
{
    public_data(TAG_DIO, GET_MQTT_ID(pinIn), duration / 1000, false);
    delay(1000);
}

void switch_untill_released_callback(uint8_t pinIn, unsigned long duration)
{
    public_data(TAG_DIO, GET_MQTT_ID(pinIn), 0, false);
    delay(3000);
}

void switch_released_callback(uint8_t pinIn)
{
    if (switch_pressed.isReleased() && switch_pressed2.isReleased())
    {
        digitalWrite(config.lamp_do.pin, LOW);
    }

    Serial.println("Switch" + get_switch_index(pinIn) + "Released");
    public_data(TAG_DIO, GET_MQTT_ID(pinIn), 0, false);
}

#define CALCULATE_DUTYCUCLE(x) (x * 255) / 2000
void setBuzzar(uint8_t buzzar_value)
{
    switch (buzzar_value)
    {
    case BUZZAR_NOERROR:
        digitalWrite(config.boot.pin, HIGH);
        Serial.println("BUZZAR_NOERROR");
        break;
    case BUZZAR_WIFI_DOWN:
        digitalWrite(config.boot.pin, LOW);
        Serial.println("BUZZAR_WIFI_DOWN");
        break;
    case BUZZAR_MQTT_DOWN:
        digitalWrite(config.boot.pin, LOW);
        Serial.println("BUZZAR_MQTT_DOWN");
        break;
    case BUZZAR_STARTUP:
        digitalWrite(config.boot.pin, LOW);
        Serial.println("BUZZAR_STARTUP");
        break;
    default:
        digitalWrite(config.boot.pin, LOW);
    }
}

void callback(char *topic, byte *message, unsigned int length)
{
}

void reconnect()
{
    // Loop until we're reconnected
    Serial.print("reconnecting...");
    ESP.wdtFeed();
    while (!client.connected())
    {
        setBuzzar(BUZZAR_MQTT_DOWN);
        if (!WiFi.isConnected())
        {
            setup_wifi();
        }
        iotWebConf.doLoop();
        Serial.print("Attempting MQTT connection...");
        // Attempt to connect
        boolean result;
        String willMessage = "partalarm/" + String(iotWebConf.getThingName()) + "/" + "heartbeat";
        if (config.mqtt_config.enable)
        {
            result = client.connect(iotWebConf.getThingName(), config.mqtt_config.user_name.c_str(),
                                    config.mqtt_config.password.c_str(), willMessage.c_str(),
                                    MQTTQOS0, false, "Disconnected");
        }
        else
        {
            result = client.connect(iotWebConf.getThingName(), willMessage.c_str(), MQTTQOS0, false, "Disconnected");
            Serial.println("mqtt connect without auth");
            Serial.println(iotWebConf.getThingName());
            Serial.println(mqttServerValue);
            Serial.println(atoi(mqttServerportValue));
        }
        if (result)
        {
            Serial.println("connected");
            // Subscribe to events here
        }
        else
        {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            // Wait 5 seconds before retrying
            for (int i = 0; i < 500; i++)
            {
                iotWebConf.doLoop();
                delay(10);
            }
        }
    }

    setBuzzar(BUZZAR_NOERROR);
}

void send_last_update_time()
{
    time_t now;
    time(&now);
    public_data(TAG_TELEMETRY, TAG_UPDATE_TIME, now, true);
}

void print_current_time()
{
    time_t now;
    time(&now);
    Serial.println(now);
}

void wait_for_ntp()
{
    time_t now;
    time(&now);
    Serial.println("Waiting for NTP");
    while (now < 1566121974)
    {
        delay(50);
        Serial.print(".");
        time(&now);
    }
}

void setOTA(const char *hostname)
{
    ArduinoOTA
        .onStart([]() {
            String type;
            if (ArduinoOTA.getCommand() == U_FLASH)
                type = "sketch";
            else // U_SPIFFS
                type = "filesystem";

            // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
            Serial.println("Start updating " + type);
        });
    ArduinoOTA.onEnd([]() {
        Serial.println("\nEnd");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR)
            Serial.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR)
            Serial.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR)
            Serial.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR)
            Serial.println("Receive Failed");
        else if (error == OTA_END_ERROR)
            Serial.println("End Failed");
    });
    ArduinoOTA.setHostname(hostname);
    ArduinoOTA.begin();
}

void setup()
{
    Serial.begin(115200);
    pinMode(config.boot.pin, OUTPUT);
    pinMode(config.lamp_do.pin, OUTPUT);
    digitalWrite(config.lamp_do.pin, LOW);
    IotWebConfSetup();

    setBuzzar(BUZZAR_STARTUP);
    setup_wifi();
    client.setServer(mqttServerValue, atoi(mqttServerportValue));
    client.setCallback(callback);
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServerValue);
    reconnect();
    // wait_for_ntp();
    switch_pressed.registerCallbacks(switch_pressed_callback, switch_released_callback,
                                     switch_untill_pressed_callback,
                                     switch_untill_released_callback);
    switch_pressed.setup(config.switch_inp.pin, DEBOUNCE_DELAY, InputDebounce::PIM_INT_PULL_UP_RES, 0,
                         InputDebounce::ST_NORMALLY_CLOSED);

    switch_pressed2.registerCallbacks(switch_pressed_callback, switch_released_callback,
                                      switch_untill_pressed_callback,
                                      switch_untill_released_callback);
    switch_pressed2.setup(config.switch2_inp.pin, DEBOUNCE_DELAY, InputDebounce::PIM_INT_PULL_UP_RES, 0,
                          InputDebounce::ST_NORMALLY_CLOSED);

    switch_released_callback(config.switch_inp.pin);
    switch_released_callback(config.switch2_inp.pin);

    ESP.wdtEnable(WDTIMEOUT);
    print_current_time();
    setOTA(iotWebConf.getThingName());
}

uint8_t RssiToPercentage(int dBm)
{
    if (dBm <= -100)
        return 0;
    if (dBm >= -50)
        return 100;
    return 2 * (dBm + 100);
}

unsigned long last_update_time = 0;
unsigned long last_update_time_telemetry = 0;
void loop()
{
    switch_pressed.process(millis());
    switch_pressed2.process(millis());
    auto curr_time_tele = millis() - last_update_time_telemetry;

    if (curr_time_tele > 5000)
    {
        uint8_t rssi = RssiToPercentage(WiFi.RSSI());
        public_data(TAG_TELEMETRY, TAG_WIFI_SIGNAL, rssi);
        //send_last_update_time();
        last_update_time_telemetry = millis();
    }

    ESP.wdtFeed();
    delay(100); // [ms]
    client.loop();
    iotWebConf.doLoop();
    ArduinoOTA.handle();
}
