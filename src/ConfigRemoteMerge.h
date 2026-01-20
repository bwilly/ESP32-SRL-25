// ConfigRemoteMerge.h
#pragma once

// #include <Arduino.h>
#include <string>

class Logger;   // forward declaration. cpp will include full header

void tryFetchAndApplyRemoteConfig(
    Logger&        inLogger,
    const std::string&  configUrl,
    const std::string&  locationName,
    const char*    mergedRemoteLocalFilename
);