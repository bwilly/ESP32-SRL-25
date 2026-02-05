// ConfigStorage.cpp
#include "ConfigStorage.h"

namespace ConfigStorage {

    AppConfigLoadResult loadAppConfigFromFile(const char *path, AppConfig &cfg)
    {
        File f = SPIFFS.open(path, FILE_READ);
        if (!f) {
            // No file yet
            return AppConfigLoadResult::NotFoundOrInvalid;
        }

        StaticJsonDocument<APP_CONFIG_JSON_CAPACITY> doc;
        DeserializationError err = deserializeJson(doc, f);
        f.close();

        if (err) {
            // Parse error
            return AppConfigLoadResult::NotFoundOrInvalid;
        }

        JsonObject root = doc.as<JsonObject>();
        const bool isLegacyShape = !root.containsKey("boot");
        if (!configFromJson(root, cfg)) {
            return AppConfigLoadResult::NotFoundOrInvalid;
        }

        return isLegacyShape
            ? AppConfigLoadResult::LoadedLegacy
            : AppConfigLoadResult::LoadedCurrent;
    }


    // todo: this is a good helper method such that call doens't have to put togther the plbuming like esp32-wifi-manager does right now feb4'26
    bool saveAppConfigToFile(const char *path, const AppConfig &cfg, JsonDocument &doc)
    {
        doc.clear();
        JsonObject root = doc.to<JsonObject>();

        configToJson(cfg, root);

        File f = SPIFFS.open(path, FILE_WRITE);
        if (!f) {
            return false;
        }

        if (serializeJson(doc, f) == 0) {
            f.close();
            return false;
        }

        f.close();
        return true;
    }

    // Legacy version for backward compatibility (uses dynamic allocation)
    bool saveAppConfigToFile(const char *path, const AppConfig &cfg)
    {
        DynamicJsonDocument doc(APP_CONFIG_JSON_CAPACITY);
        return saveAppConfigToFile(path, cfg, doc);
    }

} // namespace ConfigStorage
