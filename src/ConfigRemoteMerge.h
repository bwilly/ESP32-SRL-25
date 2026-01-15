#pragma once

#include <Arduino.h>

class Logger;   // forward declaration. cpp will include full header

void tryFetchAndApplyRemoteConfig(
    Logger&        logger,
    const String&  configUrl,
    const String&  locationName,
    const char*    FNAME_CONFIGREMOTE
);