// ConfigStorage.h
#pragma once

#include <Arduino.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>

#include "ConfigModel.h"
#include "ConfigCodec.h"

namespace ConfigStorage {

    // Load AppConfig from a JSON file (e.g. "/config.json").
    // Returns true on success.
    bool loadAppConfigFromFile(const char *path, AppConfig &cfg);

    // Save AppConfig to a JSON file (e.g. "/config.json").
    // Uses provided reusable JsonDocument to avoid stack allocation.
    // Returns true on success.
    bool saveAppConfigToFile(const char *path, const AppConfig &cfg, JsonDocument &doc);

    // Legacy version for backward compatibility (uses dynamic allocation)
    bool saveAppConfigToFile(const char *path, const AppConfig &cfg);

} // namespace ConfigStorage
