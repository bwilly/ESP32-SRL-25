// ConfigLoad.cpp
#include "ConfigLoad.h"

// #include <BufferedLogger.h>

#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <FS.h>


#include "shared_vars.h"   // paramToVariableMap, paramToBoolMap, w1Address, w1Name, W1_NUM_BYTES

// Keep in sync with ConfigDump
static const size_t CONFIG_JSON_CAPACITY = 4096;

// Helper: parse an even-length hex string into a byte buffer
// Returns true on success, false if invalid.
static bool hexStringToBytes(const String &hex, uint8_t *out, size_t outLen)
{
    size_t n = hex.length();
    if (n == 0 || (n % 2) != 0) {
        return false;
    }

    size_t bytesNeeded = n / 2;
    if (bytesNeeded > outLen) {
        bytesNeeded = outLen;  // truncate rather than overflow
    }

    for (size_t i = 0; i < bytesNeeded; ++i) {
        char c1 = hex[2 * i];
        char c2 = hex[2 * i + 1];

        auto hexVal = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
            if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
            return -1;
        };

        int v1 = hexVal(c1);
        int v2 = hexVal(c2);
        if (v1 < 0 || v2 < 0) {
            return false;
        }

        out[i] = (uint8_t)((v1 << 4) | v2);
    }

    // zero any remaining bytes if buffer is larger than string
    for (size_t i = bytesNeeded; i < outLen; ++i) {
        out[i] = 0;
    }

    return true;
}

// Internal: apply a parsed JsonDocument to the current in-memory config
static bool applyConfigJsonDoc(JsonDocument &doc)
{
    // 1) String params (ssid, pass, location, pins, mqtt-server, etc.)
    for (auto &entry : paramToVariableMap) {
        const String &key   = entry.first;
        String       *value = entry.second;

        if (!value) {
            logger.log("ConfigLoad: string param '" + key + "' has null target pointer\n");
            continue;
        }

        // todo: put me back in the logger Jan13'26 (removing due to noise while refactoring config load)
        // if (!doc.containsKey(key)) {
        //     logger.log("ConfigLoad: string param '" + key + "' not present in JSON\n");
        //     continue;
        // }

        // Accept JSON string, number, or bool and stringify it
        JsonVariant v = doc[key];
        if (v.isNull()) {
            logger.log("ConfigLoad: string param '" + key + "' present but null\n");
            continue;
        }

        if (v.is<const char*>()) {
            *value = String(v.as<const char*>());
        } else if (v.is<long>() || v.is<double>()) {
            *value = String(v.as<double>(), 6);  // simple numeric → string
        } else if (v.is<bool>()) {
            *value = v.as<bool>() ? "1" : "0";
        } else {
            // Fallback: JSON-encode the value into a temporary string
            String tmp;
            serializeJson(v, tmp);
            *value = tmp;
        }

        logger.log("ConfigLoad: applied string param '" + key + "' = '" + *value + "'\n");
    }

    // 2) Bool params (enableW1, enableDHT, enableAcs712, enableMQTT, etc.)
    // for (auto &entry : paramToBoolMap) {
    //     const String &key   = entry.first;
    //     bool         *value = entry.second;

    //     if (!value) {
    //         logger.log("ConfigLoad: bool param '" + key + "' has null target pointer\n");
    //         continue;
    //     }

    //     if (!doc.containsKey(key)) {
    //         logger.log("ConfigLoad: bool param '" + key + "' not present in JSON\n");
    //         continue;
    //     }

    //     JsonVariant v = doc[key];
    //     if (v.isNull()) {
    //         logger.log("ConfigLoad: bool param '" + key + "' present but null\n");
    //         continue;
    //     }

    //     bool result = *value;  // default to existing

    //     if (v.is<bool>()) {
    //         result = v.as<bool>();
    //     } else if (v.is<long>() || v.is<double>()) {
    //         result = (v.as<long>() != 0);
    //     } else if (v.is<const char*>()) {
    //         String s = v.as<const char*>();
    //         s.toLowerCase();
    //         result = (s == "1" || s == "true" || s == "yes" || s == "on");
    //     } else {
    //         logger.log("ConfigLoad: bool param '" + key + "' has unsupported JSON type; leaving existing value\n");
    //     }

    //     *value = result;
    //     logger.log("ConfigLoad: applied bool param '" + key + "' = " + String(*value ? "true\n" : "false\n"));
    // }

    // 3) W1 sensors from w1-1 / w1-1-name ... w1-6
    for (int i = 0; i < 6; ++i) {
        String hexKey  = "w1-" + String(i + 1);
        String nameKey = hexKey + "-name";

        // Address
        if (doc.containsKey(hexKey)) {
            const char *hexCStr = doc[hexKey].as<const char*>();
            String hexStr = hexCStr ? String(hexCStr) : String();

            if (!hexStr.isEmpty()) {
                bool ok = hexStringToBytes(hexStr, w1Address[i], W1_NUM_BYTES);
                if (!ok) {
                    logger.log("ConfigLoad: invalid hex for '" + hexKey + "' = '" + hexStr + "'\n");
                } else {
                    logger.log("ConfigLoad: applied W1 address '" + hexKey + "' = '" + hexStr + "'\n");
                }
            } else {
                logger.log("ConfigLoad: W1 address key '" + hexKey + "' present but empty\n");
            }
        } else {
            // todo: put me back in the logger Jan13'26 (removing due to noise while refactoring config load)
            // logger.log("ConfigLoad: W1 address key '" + hexKey + "' not present in JSON\n");
        }

        // Name
        if (doc.containsKey(nameKey)) {
            const char *nm = doc[nameKey].as<const char*>();
            if (nm) {
                w1Name[i] = String(nm);
                logger.log("ConfigLoad: applied W1 name '" + nameKey + "' = '" + w1Name[i] + "'\n");
            } else {
                logger.log("ConfigLoad: W1 name key '" + nameKey + "' present but null\n");
            }
        } else {
            // todo: put me back in the logger Jan13'26 (removing due to noise while refactoring config load)
            // logger.log("ConfigLoad: W1 name key '" + nameKey + "' not present in JSON\n");
        }
    }

    return true;
}

bool loadConfigFromJsonString(const String &json)
{
    StaticJsonDocument<CONFIG_JSON_CAPACITY> doc;

    DeserializationError err = deserializeJson(doc, json);
    if (err) {
        Serial.print(F("loadConfigFromJsonString: parse error: "));
        Serial.println(err.c_str());
        return false;
    }

    return applyConfigJsonDoc(doc);
}

// bool loadConfigFromJsonFile(const char *path)
bool loadEffectiveCacheFromFile(const char* path)
{
    if (!SPIFFS.begin(true)) {
        Serial.println(F("loadConfigFromJsonFile: SPIFFS.begin() failed\n"));
        return false;
    }

    File f = SPIFFS.open(path, "r");
    if (!f) {
        Serial.print(F("loadConfigFromJsonFile: cannot open "));
        Serial.println(path);
        return false;
    }

    StaticJsonDocument<CONFIG_JSON_CAPACITY> doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();

    if (err) {
        Serial.print(F("loadConfigFromJsonFile: parse error: "));
        Serial.println(err.c_str());
        return false;
    }

    return applyConfigJsonDoc(doc);
}

bool deleteJsonFile(fs::FS &fs, const char* filePath)
{
    if (!filePath || filePath[0] == '\0') {
        logger.log("clearConfigJsonCache: invalid cachePath\n");
        return false;
    }

    logger.log("clearConfigJsonCache: path=");
    logger.log(filePath);
    logger.log("\n");

    if (!fs.exists(filePath)) {
        logger.log("clearConfigJsonCache: no cache file to remove: ");
        logger.log(filePath);
        logger.log("\n");
        return true;  // nothing to do, but state is as desired
    }

    if (fs.remove(filePath)) {
        logger.log("clearConfigJsonCache: cache file removed: ");
        logger.log(filePath);
        logger.log("\n");
        return true;
    } else {
        logger.log("clearConfigJsonCache: failed to remove cache file: ");
        logger.log(filePath);
        logger.log("\n");
        return false;
    }
}

