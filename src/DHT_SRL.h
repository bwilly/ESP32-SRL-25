#pragma once
#include <Arduino.h>

// Function declarations
float readDHTTemperature();
float readDHTHumidity();
void initSensorTask(int dhtPin);
