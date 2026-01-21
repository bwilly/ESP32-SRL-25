// ConfigMerge.h
#pragma once

#include <string>
#include <ArduinoJson.h>
#include "ConfigCodec.h"

class Logger;   // forward declaration. cpp will include full header

StaticJsonDocument<APP_CONFIG_JSON_CAPACITY> buildAppConfig(
    Logger&        inLogger,
    const std::string&  configUrl,
    const std::string&  locationName,
    const char*    mergedRemoteLocalFilename,
    const char*    bootstrapFilename,
    const char*    configFilename
);