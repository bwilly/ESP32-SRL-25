#pragma once

#include <ESPAsyncWebServer.h>
#include "ConfigModel.h"

void registerWebRoutesStation(AsyncWebServer &server, AppConfig &cfg);
void registerWebRoutesAp(AsyncWebServer &server, AppConfig &cfg);
