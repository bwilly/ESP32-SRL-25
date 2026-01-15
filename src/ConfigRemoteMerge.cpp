// ConfigRemoteMerge.cpp
#include <Arduino.h>
#include "SPIFFS.h"
#include "ConfigFile.h"
#include "ConfigFetch.h"
#include <ArduinoJson.h>
#include "Logger.h"
#include "ConfigLoad.h" // legacy 

// Remote config JSON merge docs (kept off stack)
static const size_t REMOTE_JSON_CAPACITY = 4096;
static StaticJsonDocument<REMOTE_JSON_CAPACITY> g_remoteMergedDoc;
static StaticJsonDocument<REMOTE_JSON_CAPACITY> g_remoteTmpDoc;
static Logger* logger = nullptr;


static bool writeStringToFile(const char *path, const String &data)
{
  File f = SPIFFS.open(path, FILE_WRITE); // truncates or creates
  if (!f)
  {
    Serial.print(F("s:writeStringToFile: failed to open "));
    Serial.print(path);
    Serial.println(F(" for writing"));
    return false;
  }

  size_t written = f.print(data);
  f.close();

  if (written != data.length())
  {
    Serial.print(F("s:writeStringToFile: short write to "));
    Serial.print(path);
    Serial.print(F(" (expected "));
    Serial.print(data.length());
    Serial.print(F(" bytes, wrote "));
    Serial.print(written);
    Serial.println(F(")"));
    return false;
  }

  return true;
}

static bool readFileToString(const char *path, String &out)
{
  File f = SPIFFS.open(path, FILE_READ);
  if (!f)
  {
    return false;
  }
  out = f.readString();
  f.close();
  return true;
}

static void fetchApplyAndMergeRemoteConfig(
    Logger&       inLogger,
    const String& url,
    const char*   label,
    String&       json,             // reused buffer to avoid extra allocs
    JsonObject    mergedRoot,
    bool&         anyRemoteApplied  // out flag
)
{
  logger = &inLogger;

  if (url.length() == 0)
  {
    return;
  }

  if (!downloadConfigJson(url, json))
  {
    logger->log(String("ConfigFetch: ") + label +
               " config fetch FAILED or not found at " + url + "\n");
    return;
  }

  logger->log(String("ConfigFetch: downloaded ") + label + " config from " + url +
             " (" + String(json.length()) + " bytes)\n");

  // Apply to in-memory params
  if (!legacyLoadConfigFromJsonString(json))
  {
    logger->log(String("ConfigFetch: ") + label + " JSON parse/apply FAILED\n");
    return;
  }

  logger->log(String("ConfigFetch: ") + label + " JSON applied OK\n");

  // Merge into remote snapshot doc
  g_remoteTmpDoc.clear();
  DeserializationError err = deserializeJson(g_remoteTmpDoc, json);
  if (err)
  {
    logger->log(String("ConfigFetch: ") + label +
               " JSON re-parse FAILED for snapshot merge\n");
    return;
  }

  JsonObject src = g_remoteTmpDoc.as<JsonObject>();
  for (JsonPair kv : src)
  {
    mergedRoot[kv.key()] = kv.value(); // instance overrides global on same key
  }

  anyRemoteApplied = true;
}


// todo:workingHere: refactor this to use ConfigCodec 
// also, moved globals in here g_remoteMergedDoc, g_remoteTmpDoc
void tryFetchAndApplyRemoteConfig(
    Logger&        logger,
    const String&  configUrl,
    const String&  locationName,
    const char*    FNAME_CONFIGREMOTE
)
{
  const String baseUrl = configUrl;

  if (baseUrl.length() == 0)
  {
    logger.log("ConfigFetch: base configUrl is empty; cannot fetch remote config\n");
    return;
  }

  // Build GLOBAL URL: <base>/global.json
  String globalUrl = baseUrl;
  if (!globalUrl.endsWith("/"))
  {
    globalUrl += "/";
  }
  globalUrl += "global.json"; // <-- make sure this says "global.json", not "global."

  // Build INSTANCE URL: <base>/<locationName>.json
  String instanceUrl;
  if (locationName.length() > 0)
  {
    instanceUrl = baseUrl;
    if (!instanceUrl.endsWith("/"))
    {
      instanceUrl += "/";
    }
    instanceUrl += locationName + ".json";
  }
  else
  {
    logger.log("ConfigFetch: locationName is empty; will only attempt GLOBAL config\n");
  }

  // Load previous remote snapshot
  String previousRemoteJson;
  bool hasPreviousRemote = readFileToString(FNAME_CONFIGREMOTE, previousRemoteJson);
  
  char buf[128];

  snprintf(
    buf,
    sizeof(buf),
    "ConfigFetch: %s previous remote snapshot %s\n",
    hasPreviousRemote ? "found" : "no",
    FNAME_CONFIGREMOTE
  );
  logger.log(buf);


  // Clear and prepare merged remote doc
  g_remoteMergedDoc.clear();
  JsonObject mergedRoot = g_remoteMergedDoc.to<JsonObject>();

  bool anyRemoteApplied = false;
  String json;

  // GLOBAL then INSTANCE
  fetchApplyAndMergeRemoteConfig(logger, globalUrl, "GLOBAL", json, mergedRoot, anyRemoteApplied);
  if (instanceUrl.length() > 0)
  {
    fetchApplyAndMergeRemoteConfig(logger, instanceUrl, "INSTANCE", json, mergedRoot, anyRemoteApplied);
  }


  if (!anyRemoteApplied)
  {
    logger.log("ConfigFetch: no remote configs applied; leaving local config as-is; no reboot\n");
    return;
  }

  // Serialize new remote snapshot
  String newRemoteJson;
  serializeJson(g_remoteMergedDoc, newRemoteJson);

  // Compare snapshots *only on remote config*
  if (hasPreviousRemote && (newRemoteJson == previousRemoteJson))
  {
    logger.log("ConfigFetch: remote config unchanged vs /config-remote.json; no persist, no reboot\n");
    return;
  }

  logger.log("ConfigFetch: remote config changed; persisting and rebooting\n");

  // Persist remote snapshot
  if (writeStringToFile(FNAME_CONFIGREMOTE, newRemoteJson))
  {
    logger.log("ConfigFetch: wrote new remote snapshot to /config-remote.json\n");
  }
  else
  {
    logger.log("ConfigFetch: FAILED to write /config-remote.json\n");
  }

  // // Persist full effective config
  // if (saveLegacyEffectiveCacheToFile(EFFECTIVE_CACHE_PATH)) 
  // {
  //   logger.log("ConfigFetch: persisted merged effective config config.json; rebooting\n");
  // }
  // else
  // {
  //   logger.log("ConfigFetch: FAILED to persist merged config; rebooting anyway\n");
  // }


  String err;

  if (!saveConfigJson(newRemoteJson, err))
  {
    logger.log("applyRemoteConfig: failed to save remote config to general json config\n");

    if (err.length()) {
        logger.log("applyRemoteConfig: error=");
        logger.log(err);
        logger.log("\n");
    }
    else {
        logger.log("applyRemoteConfig: no error detail provided\n");
    }

    return;
  }

  logger.log("applyRemoteConfig: remote config saved successfully as general config\n");


  // Allow logger + UART/WiFi buffers to flush
  delay(500);

  // Yield once more for good measure
  yield();

  ESP.restart();
}
