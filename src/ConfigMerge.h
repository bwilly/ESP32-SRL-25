// ConfigMerge.h
#pragma once

#include <string>
#include <ArduinoJson.h>

class Logger;   // forward declaration. cpp will include full header

StaticJsonDocument<4096> buildAppConfig(
    Logger&        inLogger,
    const std::string&  configUrl,
    const std::string&  locationName,
    const char*    mergedRemoteLocalFilename,
    const char*    bootstrapFilename,
    const char*    configFilename
);