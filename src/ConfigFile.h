#pragma once
#include <ArduinoJson.h>
#include "ConfigCodec.h"  // For APP_CONFIG_JSON_CAPACITY

// Reusable global buffer for config saves (declared in ESP32_WiFi_Manager.cpp)
extern StaticJsonDocument<APP_CONFIG_JSON_CAPACITY> g_configSaveDoc;

// Saves validated bootstrap config JSON into /config.json (modular)
// errOut is set on failure.
bool saveConfigJson(const std::string &jsonBody, std::string &errOut);
