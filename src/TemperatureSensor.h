// TemperatureSensor.h
#pragma once

#include <DallasTemperature.h>
#include "TemperatureReading.h"
#include "Config.h"
#include "ConfigModel.h"

class TemperatureSensor
{
public:
    TemperatureSensor();
    bool begin(int oneWirePin);

    void requestTemperatures();
    TemperatureReading *getTemperatureReadings(const W1Config &w1Config);

    DallasTemperature sensors; // Now public
private:
    OneWire oneWire;
    TemperatureReading readings[MAX_READINGS + 1]; // +1 for the ending marker
};
