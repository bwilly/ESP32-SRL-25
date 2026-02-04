#include <Arduino.h>
#include "shared_vars.h"
// In shared_vars.cpp
#include "ParamMetadata.h"
#include <map>


#include "shared_vars.h"

DeviceIdentity gIdentity;


volatile bool g_bootstrapPending = false;
String g_bootstrapBody;
String g_bootstrapErr;


// Search for parameter in HTTP POST request
// This value must match the name or ID of the HTML input control and the process() value for displaying the value in the manage page
const char *PARAM_WIFI_SSID = "ssid";
const char *PARAM_WIFI_PASS = "pass";
const char *PARAM_LOCATION = "location";
const char *PARAM_CONFIG_URL = "config-url";
const char *PARAM_OTA_URL = "ota-url";
// const char *PARAM_PIN_DHT = "pinDht"; // wish i could change name convention. see above comment
// const char *PARAM_PIN_ACS = "pinAcs"; 
// const char *PARAM_MQTT_SERVER = "mqtt-server";
// const char *PARAM_MQTT_PORT = "mqtt-port";
// const char *PARAM_MAIN_DELAY = "main-delay";
// const char *PARAM_W1_1 = "w1-1";
// const char *PARAM_W1_2 = "w1-2";
// const char *PARAM_W1_3 = "w1-3";
// const char *PARAM_W1_4 = "w1-4";
// const char *PARAM_W1_5 = "w1-5";
// const char *PARAM_W1_6 = "w1-6";
const int W1_NUM_BYTES = 8; // The expected number of bytes
// const char *PARAM_W1_1_NAME = "w1-1-name";
// const char *PARAM_W1_2_NAME = "w1-2-name";
// const char *PARAM_W1_3_NAME = "w1-3-name";
// const char *PARAM_W1_4_NAME = "w1-4-name";
// const char *PARAM_W1_5_NAME = "w1-5-name";
// const char *PARAM_W1_6_NAME = "w1-6-name";
// const char *PARAM_ENABLE_W1 = "enableW1";
// const char *PARAM_ENABLE_DHT = "enableDHT";
// const char *PARAM_ENABLE_ACS = "enableCHT832x";
// const char *PARAM_ENABLE_CHT832x = "enableAcs712";
// const char *PARAM_ENABLE_SCT = "enableSct";

// const char *PARAM_ENABLE_MQTT = "enableMQTT";

// Variables to save values from HTML form
std::string ssid;
std::string pass;
std::string locationName; // used during regular operation, not only setup (todo: need to switch to gConifg Jan14'26)
std::string configUrl;
std::string otaUrl;
// String pinDht;
// String pinAcs;
// String mqttServer;
// String mqttPort;
// bool w1Enabled;


const char* FNAME_CONFIGREMOTE = "/config-remote.json";
const char* FNAME_CONFIG = "/config.json";
const char* FNAME_BOOTSTRAP = "/bootstrap.json";

String mainDelay;
uint8_t w1Address[6][8]; // accounted for in ParamHandler.cpp todo:workingHere: it was until my refactor. i bet now it doesn't work yet anymore Dec2, 2023. Yes me, i'd be correct. Nov11'24
String w1Name[6];        // todo:remove this and above after refactor Nov8'24
// std::array<W1Sensor, 3> w1Sensor;

// uint8_t w1Sensors[];


CHT832xSensor envSensor(0x44); // default address; todo: externalize Dec3'25


std::map<std::string, std::string*> paramToVariableMap;

// void initParamMap() {
//     paramToVariableMap["ssid"] = &ssid;
//     paramToVariableMap["pass"] = &pass;
//     paramToVariableMap["config-url"] = &configUrl;
//     paramToVariableMap["ota-url"] = &otaUrl;
//     paramToVariableMap["main-delay"] = &mainDelay;
// }

// always must update me when adding new config params
// std::map<std::string, std::string *> paramToVariableMap = {
    // {"ssid", &ssid},
    // {"pass", &pass},
    // {"location", &locationName},
    // {"config-url", &configUrl},
    // {"ota-url", &otaUrl},
    // {"pinDht", &pinDht},
    // {"pinAcs", &pinAcs},
    // {"mqtt-server", &mqttServer},
    // {"mqtt-port", &mqttPort},
    // {"main-delay", &mainDelay}
    // Note: For Boolean and other non-String types, you might need a different approach

// };

// std::map<String, bool *> paramToBoolMap = {
//     {"enableW1", &w1Enabled},
//     {"enableDHT", &dhtEnabled},
//     {"enableAcs712", &acs712Enabled},
//     {"enableMQTT", &mqttEnabled},
//     {"enableCHT832x", &cht832xEnabled},
//     {"enableSct", &sctEnabled}
// };

SensorGroupW1 w1Sensors;