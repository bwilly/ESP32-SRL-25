// ConfigMerge.cpp

#include "SPIFFS.h"
#include "ConfigFile.h"
#include "ConfigFetch.h"
#include <ArduinoJson.h>
#include "Logger.h"
#include "JsonMerge.h"


static Logger* logger = nullptr;
static const size_t CONFIG_JSON_CAPACITY = 4096;

static MergeOptions opt;  // nullDeletes=false, arraysById=true, idKey="id"

static bool writeStringToFile(const char *path, const std::string &data)
{
  File f = SPIFFS.open(path, FILE_WRITE); // truncates or creates
  if (!f)
  {
    Serial.print(F("s:writeStringToFile: failed to open "));
    Serial.print(path);
    Serial.println(F(" for writing"));
    return false;
  }

  size_t written = f.print(data.c_str());
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

static bool readFileToString(const char *path, std::string &out)
{
  File f = SPIFFS.open(path, FILE_READ);
  if (!f)
  {
    return false;
  }
  
  size_t size = f.size();
  out.resize(size);
  
  if (size > 0)
  {
    f.readBytes(&out[0], size);
  }
  
  f.close();
  return true;
}

DynamicJsonDocument loadConfigJson(const char *path, std::string &errOut){
    Serial.println(F("s:loadConfigJson: loading config json... "));

    std::string jsonBody;
    if (!readFileToString(path, jsonBody)) {
        errOut = "Failed to read config file.";
        return DynamicJsonDocument(0); // empty
    }

    Serial.println(F("s:loadConfigJson: read file... "));

    // 1) Parse JSON (sanity check) - use global reusable buffer
    DynamicJsonDocument doc(APP_CONFIG_JSON_CAPACITY);
    DeserializationError err = deserializeJson(doc, String(jsonBody.c_str()));
    if (err) {
        errOut = std::string("JSON parse error: ") + err.c_str();
        return DynamicJsonDocument(0); // empty
    }

    Serial.println(F("s:loadConfigJson: deserialized... "));

    return doc;
}

std::string globalUrl(const std::string& configUrl) {
  
  if (configUrl.length() == 0)
  {
    logger->log("ConfigMerge: configUrl is empty; cannot fetch remote config\n");
    return "";  // Return empty string instead of bare return
  }

  // Build GLOBAL URL: <base>/global.json
  std::string url = configUrl;  
  if (!url.empty() && url.back() != '/')
  {
    url += "/";
  }
  url += "global.json";
  return url;  
}

std::string instanceUrl(const std::string& baseUrl, const std::string& locationName) {
  
  if (locationName.length() == 0)
  {
    logger->log("ConfigMerge: locationName is empty\n");
    return "";
  }

  // Build INSTANCE URL: <base>/<locationName>.json
  std::string url = baseUrl;
  if (!url.empty() && url.back() != '/')
  {
    url += "/";
  }
  url += locationName + ".json";
  return url;
}



static StaticJsonDocument<4096> mergeRemotes(
    Logger&        inLogger,
    const std::string&  configUrl,
    const std::string&  locationName,
    const char*    mergedRemoteLocalFilename
) {

  static StaticJsonDocument<CONFIG_JSON_CAPACITY> instanceDoc;
  static StaticJsonDocument<CONFIG_JSON_CAPACITY> globalDoc;

  std::string globalJsonString;
  std::string gUrl = globalUrl(configUrl);
  if (!downloadConfigJson(gUrl, globalJsonString))
  {
    char buf[256];
    snprintf(buf, sizeof(buf),
             "ConfigMerge: %s config fetch FAILED or not found at %s\n",
             "global", gUrl.c_str());
    logger->log(buf);
  }

  std::string instanceJsonString;
  std::string iUrl = globalUrl(configUrl);
  if (!downloadConfigJson(iUrl, instanceJsonString))
  {
    char buf[256];
    snprintf(buf, sizeof(buf),
             "ConfigMerge: %s config fetch FAILED or not found at %s\n",
             "instance", iUrl.c_str());
    logger->log(buf);
  }

  DeserializationError gError = deserializeJson(globalDoc, globalJsonString);
  if (gError) {
    logger->log("Failed to parse JSON: ");
    logger->log(gError.c_str());
  }

  DeserializationError iError = deserializeJson(instanceDoc, instanceJsonString);
  if (iError) {
    logger->log("Failed to parse JSON: ");
    logger->log(iError.c_str());
  }
  
  // Merge order: bootstrap < global < instance
  // this method only concerns itself with global < instance
  deepMerge(instanceDoc.as<JsonVariant>(), globalDoc.as<JsonVariant>(), opt);
  auto& remoteDoc = instanceDoc; // instanceDoc is now merged with global
  std::string newRemoteJson;
  serializeJson(remoteDoc, newRemoteJson); 

  return remoteDoc; 
}

StaticJsonDocument<4096> buildAppConfig(
    Logger&        inLogger,
    const std::string&  configUrl,
    const std::string&  locationName,
    const char*    mergedRemoteLocalFilename,
    const char*    bootstrapFilename,
    const char*    configFilename
) {

  // Load previous remote snapshot
  std::string previousRemoteJson;
  StaticJsonDocument<CONFIG_JSON_CAPACITY> previousRemoteDoc;
  bool hasPreviousRemote = readFileToString(mergedRemoteLocalFilename, previousRemoteJson);
  if(hasPreviousRemote) {
    deserializeJson(previousRemoteDoc, previousRemoteJson);
  }
  
  char buf[128];
  snprintf(
    buf,
    sizeof(buf),
    "ConfigMerge: %s previous remote snapshot %s\n",
    hasPreviousRemote ? "found" : "no",
    mergedRemoteLocalFilename
  ); 
  logger->log(buf);


  auto remoteDoc = mergeRemotes(inLogger, configUrl, locationName, mergedRemoteLocalFilename);

  // Compare snapshots *only on remote config*
  if (hasPreviousRemote && (remoteDoc == previousRemoteDoc)) 
  {
    logger->log("ConfigMerge: remote config unchanged local copy; no persist.\n");
  }
  else
  {
    logger->log("ConfigMerge: remote config changed; persisting\n");
    // Serialize new remote snapshot
    std::string newRemoteJson;
    serializeJson(remoteDoc, newRemoteJson);
    writeStringToFile(mergedRemoteLocalFilename, newRemoteJson);
  } 
  
  
  std::string bootJson;
  if (!readFileToString(bootstrapFilename, bootJson))
  {
    logger->log("ConfigMerge: failed to read bootstrap file\n");
    return remoteDoc;
  }

  static StaticJsonDocument<CONFIG_JSON_CAPACITY> bootDoc;
  DeserializationError error = deserializeJson(bootDoc, bootJson);
  if (error) {
    logger->log("Failed to parse bootstrap JSON: ");
    logger->log(error.c_str());
  }

  deepMerge(remoteDoc.as<JsonVariant>(), bootDoc.as<JsonVariant>(), opt);
  auto& fullDoc = remoteDoc; // now fully merged boot/global/instance

  std::string err1;
  auto prevConfigDoc = loadConfigJson(configFilename, err1);
  if(prevConfigDoc == fullDoc) {
    logger->log("ConfigMerge: full config unchanged vs existing config; no persist, no reboot\n");
    return prevConfigDoc;
  }


  std::string err;  
  if (!saveConfigJson(fullDoc.as<JsonVariant>(), err))
  {
    logger->log("ConfigMerge: failed to save merged config to general json config\n");

    if (!err.empty()) {
        logger->log("buildAppConfig: error=");
        logger->log(err.c_str());
        logger->log("\n");
    }
    else {
        logger->log("buildAppConfig: no error detail provided\n");
    }

    return fullDoc;
  }
  logger->log("buildAppConfig: fully merged boot/global/instance config saved successfully as general config\n");


  logger->handle();
  logger->flush(16);


  // Allow logger + UART/WiFi buffers to flush
  delay(500);

  // Yield once more for good measure
  yield();

  ESP.restart();

}

