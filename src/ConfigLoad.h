// ConfigLoad.h
#pragma once

#include <Arduino.h>
#include <FS.h>


// Path for the on-disk effective config cache
static constexpr const char* EFFECTIVE_CACHE_PATH = "/config.effective.cache.json";

// Load/apply config from a JSON string (already in memory)
bool loadConfigFromJsonString(const String &json);

// Load/apply config from a JSON file on SPIFFS (e.g. "/config.json")
// bool loadConfigFromJsonFile(const char *path = "/config.json");
bool loadEffectiveCacheFromFile(const char* path);

bool deleteJsonFile(fs::FS &fs, const char* filePath);
