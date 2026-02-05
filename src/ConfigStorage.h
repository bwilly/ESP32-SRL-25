// ConfigStorage.h
#pragma once

#include <Arduino.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>

#include "ConfigModel.h"
#include "ConfigCodec.h"

namespace ConfigStorage {

    enum class AppConfigLoadResult {
        NotFoundOrInvalid = 0,
        LoadedCurrent,
        LoadedLegacy
    };

    // Load AppConfig from a JSON file (e.g. "/config.json").
    // Returns a tri-state result: missing/invalid, loaded current format, loaded legacy format.
    AppConfigLoadResult loadAppConfigFromFile(const char *path, AppConfig &cfg);

    // Save AppConfig to a JSON file (e.g. "/config.json").
    // Uses provided reusable JsonDocument to avoid stack allocation.
    // Returns true on success.
    bool saveAppConfigToFile(const char *path, const AppConfig &cfg, JsonDocument &doc);

    // Legacy version for backward compatibility (uses dynamic allocation)
    bool saveAppConfigToFile(const char *path, const AppConfig &cfg);

} // namespace ConfigStorage
