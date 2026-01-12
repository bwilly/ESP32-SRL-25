// In shared_vars.h
#ifndef SHARED_VARS_H
#define SHARED_VARS_H

#pragma once
#include <Arduino.h>
#include <vector> // Include the vector header
#include <map>
#include "ParamMetadata.h"
#include "CHT832xSensor.h"
using namespace std;

#include "Logger.h"
extern Logger logger;

#include "DeviceIdentity.h"
extern DeviceIdentity gIdentity;

// Search for parameter in HTTP POST request
extern const char *PARAM_WIFI_SSID; // = "ssid";
extern const char *PARAM_WIFI_PASS; // = "pass";
extern const char *PARAM_LOCATION;  // = "location";
extern const char *PARAM_CONFIG_URL;  
extern const char *PARAM_OTA_URL;
extern const char *PARAM_PIN_DHT;   // = "pinDht";
extern const char *PARAM_PIN_ACS;   // = "pinAcs";
extern const char *PARAM_MQTT_SERVER;
extern const char *PARAM_MQTT_PORT;
extern const char *PARAM_MAIN_DELAY;
extern const int W1_NUM_BYTES;
extern const char *PARAM_W1_1;
extern const char *PARAM_W1_2;
extern const char *PARAM_W1_3;
extern const char *PARAM_W1_4;
extern const char *PARAM_W1_5;
extern const char *PARAM_W1_6;
extern const char *PARAM_W1_1_NAME;
extern const char *PARAM_W1_2_NAME;
extern const char *PARAM_W1_3_NAME;
extern const char *PARAM_W1_4_NAME;
extern const char *PARAM_W1_5_NAME;
extern const char *PARAM_W1_6_NAME;
extern const char *PARAM_ENABLE_W1;   
extern const char *PARAM_ENABLE_DHT;  
extern const char *PARAM_ENABLE_ACS;  
extern const char *PARAM_ENABLE_CHT832x;  
extern const char *PARAM_ENABLE_SCT;  
extern const char *PARAM_ENABLE_MQTT; 

extern volatile bool g_bootstrapPending;
extern String g_bootstrapBody;
extern String g_bootstrapErr;

// Path for the on-disk effective config cache
extern const char* FNAME_CONFIGREMOTE;
extern const char* FNAME_CONFIG;
extern const char* FNAME_BOOTSTRAP;


// Variables to save values from HTML form
extern String ssid;
extern String pass;
extern String locationName; // used during regular operation, not only setup
extern String configUrl; // http://salt-r420:9080/esp-config/salt will have instance name .json and global.json appended. /salt is the site name
extern String otaUrl;
extern String pinDht;
extern String pinAcs;
extern String mqttServer;
extern String mqttPort;
extern bool w1Enabled;


// CHT832x I2C temperature/humidity sensor (full OO)
extern CHT832xSensor envSensor;

extern String mainDelay;
extern uint8_t w1Address[6][8]; // todo:remove post refactor
extern String w1Name[6];        // todo:remove post refactor

struct W1Sensor
{
    std::string name;
    std::array<uint8_t, 8> HEX_ARRAY = {};
};

struct SensorGroupW1
{
    std::array<W1Sensor, 6> sensors;
};

extern SensorGroupW1 w1Sensors;


extern std::vector<ParamMetadata> paramList;
extern std::map<String, String *> paramToVariableMap;
extern std::map<String, bool *> paramToBoolMap;
#endif // SHARED_VARS_H