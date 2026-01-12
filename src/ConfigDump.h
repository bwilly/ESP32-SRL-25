// ConfigDump.h
#pragma once

#include <Arduino.h>

// Emit flat JSON from current in-memory params:
// - Uses paramToVariableMap for String params
// - Uses paramToBoolMap for bools
// - Emits W1 sensor hex + names as w1-1, w1-1-name ... w1-6, w1-6-name
String buildLegacyEffectiveConfigJson();

// Save the current in-memory config to the given path in SPIFFS.
// Default is a backup file, but you can pass "/config.json" as well.
// Returns true on success.
bool saveLegacyEffectiveCacheToFile(const char* path);

