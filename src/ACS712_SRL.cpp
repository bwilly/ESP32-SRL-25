#include "ACS712_SRL.h"
#include "version.h" // Version macros
#include "shared_vars.h"

// @pattern class
// Constructor with pin parameter
ACS712Sensor::ACS712Sensor(int pin) : _pinAcs(pin) {}

void ACS712Sensor::begin() {
    Serial.begin(115200);

    // Print version info
    Serial.println("Application Version: " APP_VERSION);
    Serial.println("Commit Hash: " APP_COMMIT_HASH);

    // IMPORTANT:
    // _pinAcs MUST be provided by configuration.
    // There is intentionally NO default here.
    // The recommended default (GPIO32) should be defined in config JSON,
    // not hardcoded in firmware.

    if (_pinAcs <= 0)
    {
        logger.log("ACS712: invalid pin provided; sensor not initialized\n");
        return;
    }

    pinMode(_pinAcs, INPUT);
    Serial.println("ACS712 Current Sensor Initialized on pin " + String(_pinAcs));
}


// Helper function to read DC current
// float ACS712Sensor::readCurrentDC() {
//     // Read raw ADC value
//     int rawValue = analogRead(_pinAcs);    

//     float Vdiv = rawValue * ADC_MAX_VOLTAGE / ADC_RESOLUTION;
//     float amps = (Vdiv - ADC_ZERO_VOLT) / ADC_SENSITIVITY;

//     return amps;
// }

float ACS712Sensor::readCurrentDC() {
    float sumAmps = 0.0f;

    for (int i = 0; i < NUM_SAMPLES; ++i) {
        int rawValue = analogRead(_pinAcs);
        float Vdiv = rawValue * ADC_MAX_VOLTAGE / ADC_RESOLUTION;
        float amps = (Vdiv - ADC_ZERO_VOLT) / ADC_SENSITIVITY;
        sumAmps += amps;

        // optional: smooth out ADC sampling timing
        // delayMicroseconds(200);
    }

    float avgAmps = sumAmps / NUM_SAMPLES;
    return avgAmps;
}

void ACS712Sensor::serialOutInfo() {
    // Read and print current
    float current = readCurrentDC();
    Serial.print("DC Current: ");
    Serial.print(current);
    Serial.println(" A");
}

// Retrieve pin number from shared variable map
// int __pinAcs = paramToVariableMap["pinAcs"]->toInt(); // @pattern:toInt #2 todo: study these to patterns. which do i want? Mar8'25
//
// Instantiate the sensor using the mapped pin
// ACS712Sensor sensor(__pinAcs);
//
// ^^^ The above was causing a crash before setup() because it runs during global initialization.
//     We must not touch paramToVariableMap or call String::toInt() at file scope.

// Create the sensor lazily after config is loaded (setup-time), not at global init time.
static ACS712Sensor *sensor = nullptr;

// Helper: decide which pin to use, safely, at runtime.
static int resolveAcsPin()
{
    // Default / fallback pin (your current temporary fix)
    int pin = 32; // todo: externalize

    // Legacy lookup: only do this at runtime, and only if present + non-null
    auto it = paramToVariableMap.find("pinAcs");
    if (it != paramToVariableMap.end() && it->second != nullptr)
    {
        // int parsed = it->second->toInt();
        int parsed = std::stoi(*it->second);
        if (parsed > 0)
        {
            pin = parsed;
        }
        else
        {
            logger.log("ACS712: pinAcs present but invalid/0; using fallback pin 32\n");
        }
    }
    else
    {
        logger.log("ACS712: pinAcs not found in paramToVariableMap; using fallback pin 32\n");
    }

    return pin;
}

// External functions for calling from another file
void setupACS712() {
    if (sensor == nullptr)
    {
        int pin = resolveAcsPin();
        sensor = new ACS712Sensor(pin);

        // NOTE: begin() currently overwrites _pinAcs with 32 (temporary fix).
        // If you want begin() to honor the constructor pin, we can adjust begin()
        // in a small follow-up.
    }

    sensor->begin();
}

void loopACS712() {
    if (sensor == nullptr) return;
    sensor->serialOutInfo();
}

float readACS712Current() {
    if (sensor == nullptr) return NAN;
    return sensor->readCurrentDC();
}
