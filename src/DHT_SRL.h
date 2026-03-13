#pragma once
#include <Arduino.h>

// Function declarations
float readDHTTemperature();
float readDHTHumidity();
bool initSensorTask(int dhtPin);
