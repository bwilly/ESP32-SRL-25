// ConfigDump.cpp
#include "ConfigDump.h"

#include <ArduinoJson.h>
#include <SPIFFS.h>

#include "shared_vars.h"

// Keep in sync with ConfigLoad.cpp
static const size_t CONFIG_JSON_CAPACITY = 4096;

// Helper: convert raw bytes to uppercase hex string, e.g. "28FF3C1234567890"
static String bytesToHexString(const uint8_t *data, size_t len)
{
    // W1_NUM_BYTES comes from shared_vars.h; this buffer is big enough for it.
    char buf[2 * W1_NUM_BYTES + 1];
    size_t outLen = len * 2;

    if (outLen > sizeof(buf) - 1) {
        outLen = sizeof(buf) - 1;
    }

    for (size_t i = 0; i < len && (i * 2 + 1) < sizeof(buf); ++i) {
        sprintf(&buf[i * 2], "%02X", data[i]);
    }
    buf[outLen] = '\0';

    return String(buf);
}

// Build a flat JSON doc from the current in-memory values:
// - String params via paramToVariableMap
// - Bool params via paramToBoolMap
// - W1 sensor addresses/names via w1Address[] and w1Name[]
String buildLegacyEffectiveConfigJson()
{
    StaticJsonDocument<CONFIG_JSON_CAPACITY> doc;

    // 1) All String params (ssid, pass, locationName, pins, mqtt-server, etc.)
    for (auto &entry : paramToVariableMap) {
        const String &key   = entry.first;
        String       *value = entry.second;

        if (!value) {
            continue;
        }

        doc[key] = *value;
    }

    // 2) All bool params (enableW1, enableDHT, enableAcs712, enableMQTT, etc.)
    // for (auto &entry : paramToBoolMap) {
    //     const String &key   = entry.first;
    //     bool         *value = entry.second;

    //     if (!value) {
    //         continue;
    //     }

    //     doc[key] = *value;
    // }

    // 3) W1 sensor hex + names → flat keys: w1-1, w1-1-name, ... w1-6, w1-6-name
    for (int i = 0; i < 6; ++i) {
        String hexKey  = "w1-" + String(i + 1);
        String nameKey = hexKey + "-name";

        bool hasName = (w1Name[i].length() > 0);
        bool hasAddr = false;

        for (int b = 0; b < W1_NUM_BYTES; ++b) {
            if (w1Address[i][b] != 0x00) {
                hasAddr = true;
                break;
            }
        }

        if (hasAddr) {
            doc[hexKey] = bytesToHexString(w1Address[i], W1_NUM_BYTES);
        }

        if (hasName) {
            doc[nameKey] = w1Name[i];
        }
    }

    String out;
    serializeJsonPretty(doc, out);   // pretty for human-readable export
    return out;
}

// Save the current in-memory config JSON to a file in SPIFFS.
// Default path is "/config-backup.json".
// bool saveConfigBackupToFile(const char *path)
bool saveLegacyEffectiveCacheToFile(const char* path)
{
    String json = buildLegacyEffectiveConfigJson();

    File f = SPIFFS.open(path, FILE_WRITE);  // truncates or creates
    if (!f) {
        Serial.print(F("ConfigDump: failed to open "));
        Serial.print(path);
        Serial.println(F(" for writing"));
        return false;
    }

    size_t written = f.print(json);
    f.close();

    if (written != json.length()) {
        Serial.print(F("ConfigDump: short write to "));
        Serial.print(path);
        Serial.print(F(" (expected "));
        Serial.print(json.length());
        Serial.print(F(" bytes, wrote "));
        Serial.print(written);
        Serial.println(F(")"));
        return false;
    }

    return true;
}

