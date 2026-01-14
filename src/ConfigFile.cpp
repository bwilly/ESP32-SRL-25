#include "ConfigFile.h"

#include <ArduinoJson.h>

#include "ConfigModel.h"        // wherever AppConfig / gConfig are declared
#include "ConfigStorage.h"    // ConfigStorage::saveAppConfigToFile/load...
// #include "ConfigJson.h"       // configFromJson(...)
// #include "ConfigCaps.h"       // APP_CONFIG_JSON_CAPACITY (or where it lives)
#include "shared_vars.h"   


bool saveConfigJson(const String &jsonBody, String &errOut)
{
    Serial.println(F("s:saveConfigJson: saving config.json... "));


    // 1) Parse JSON (sanity check) - use global reusable buffer
    g_configSaveDoc.clear();
    DeserializationError err = deserializeJson(g_configSaveDoc, jsonBody);
    if (err) {
        errOut = String("JSON parse error: ") + err.c_str();
        return false;
    }

    Serial.println(F("s:saveConfigJson: deserialized... "));

    // 2) Apply into a temp AppConfig so we can validate without clobbering gConfig
    AppConfig tmp = gConfig; // start from current defaults
    JsonObject root = g_configSaveDoc.as<JsonObject>();
    if (!configFromJson(root, tmp)) {
        errOut = "configFromJson failed";
        return false;
    }

    Serial.println(F("s:saveConfigJson: about to persist... "));

    // 4) Persist to /config.json in modular format
    if (!ConfigStorage::saveAppConfigToFile(FNAME_CONFIG, tmp)) {
        errOut = "Failed to write /config.json";
        return false;
    }

    // 5) Also update live gConfig
    gConfig = tmp;

    return true;
}
