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
// deprecated: instead use ConfigCodec::configFromJson()
// this is legacy
static bool legacyApplyConfigJsonDoc(JsonDocument &doc)
{
    // 1) String params (ssid, pass, location, pins, mqtt-server, etc.)
    for (auto &entry : paramToVariableMap) {
        const std::string &key   = entry.first;
        std::string       *value = entry.second;

        if (!value) {
            char buf[256];
            snprintf(buf, sizeof(buf), "ConfigLoad: string param '%s' has null target pointer\n", key.c_str());
            logger.log(buf);
            continue;
        }

        // Accept JSON string, number, or bool and stringify it
        JsonVariant v = doc[key.c_str()];
        if (v.isNull()) {
            char buf[256];
            snprintf(buf, sizeof(buf), "ConfigLoad: string param '%s' present but null\n", key.c_str());
            logger.log(buf);
            continue;
        }

        if (v.is<const char*>()) {
            *value = v.as<const char*>();
        } else if (v.is<long>() || v.is<double>()) {
            char numBuf[32];
            snprintf(numBuf, sizeof(numBuf), "%.6f", v.as<double>());
            *value = numBuf;
        } else if (v.is<bool>()) {
            *value = v.as<bool>() ? "1" : "0";
        } else {
            // Fallback: JSON-encode the value into a temporary string
            String tmp;
            serializeJson(v, tmp);
            *value = tmp.c_str();
        }

        char buf[512];
        snprintf(buf, sizeof(buf), "ConfigLoad: applied string param '%s' = '%s'\n", 
                 key.c_str(), value->c_str());
        logger.log(buf);
    }

    // 3) W1 sensors from w1-1 / w1-1-name ... w1-6
    for (int i = 0; i < 6; ++i) {
        char hexKey[16];
        char nameKey[32];
        snprintf(hexKey, sizeof(hexKey), "w1-%d", i + 1);
        snprintf(nameKey, sizeof(nameKey), "%s-name", hexKey);

        // Address
        if (doc.containsKey(hexKey)) {
            const char *hexCStr = doc[hexKey].as<const char*>();
            std::string hexStr = hexCStr ? hexCStr : "";

            if (!hexStr.empty()) {
                bool ok = hexStringToBytes(hexStr.c_str(), w1Address[i], W1_NUM_BYTES);
                if (!ok) {
                    char buf[256];
                    snprintf(buf, sizeof(buf), "ConfigLoad: invalid hex for '%s' = '%s'\n", 
                             hexKey, hexStr.c_str());
                    logger.log(buf);
                } else {
                    char buf[256];
                    snprintf(buf, sizeof(buf), "ConfigLoad: applied W1 address '%s' = '%s'\n", 
                             hexKey, hexStr.c_str());
                    logger.log(buf);
                }
            } else {
                char buf[256];
                snprintf(buf, sizeof(buf), "ConfigLoad: W1 address key '%s' present but empty\n", hexKey);
                logger.log(buf);
            }
        }

        // Name
        if (doc.containsKey(nameKey)) {
            const char *nm = doc[nameKey].as<const char*>();
            if (nm) {
                w1Name[i] = nm;
                char buf[256];
                snprintf(buf, sizeof(buf), "ConfigLoad: applied W1 name '%s' = '%s'\n", 
                         nameKey, w1Name[i].c_str());
                logger.log(buf);
            } else {
                char buf[256];
                snprintf(buf, sizeof(buf), "ConfigLoad: W1 name key '%s' present but null\n", nameKey);
                logger.log(buf);
            }
        }
    }

    return true;
}

bool legacyLoadConfigFromJsonString(const String &json)
{
    StaticJsonDocument<CONFIG_JSON_CAPACITY> doc;

    DeserializationError err = deserializeJson(doc, json);
    if (err) {
        Serial.print(F("loadConfigFromJsonString: parse error: "));
        Serial.println(err.c_str());
        return false;
    }

    return legacyApplyConfigJsonDoc(doc);
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

    return legacyApplyConfigJsonDoc(doc);
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

