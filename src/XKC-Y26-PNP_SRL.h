#ifndef XKC_Y26_PNP_SRL_H
#define XKC_Y26_PNP_SRL_H

#include <Arduino.h>
#include <stddef.h>

constexpr size_t MAX_XKC_Y26_PNP_INSTANCES = 5;

struct XkcY26PnpReading {
    bool detected = false;
    uint8_t activeCount = 0;
    uint8_t rawMask = 0;
};

// @pattern class
// Class definition for XKC-Y26-PNP non-contact liquid level sensor.
class XkcY26PnpSensor {
private:
    int  _pinXkc;
    bool _activeHigh;

    static constexpr int NUM_SAMPLES = 5;

public:
    // Constructor with pin parameter. PNP outputs are normally active-high.
    explicit XkcY26PnpSensor(int pin, bool activeHigh = true);

    void begin();          // Initializes the sensor
    XkcY26PnpReading readReading();
    bool readDetected();   // Reads whether liquid/object is detected
    int readRaw();         // Reads the raw digital pin state
    void serialOutInfo();  // Prints sensor readings to Serial
};

// Function declarations for external usage
bool setupXkcY26Pnp(size_t index, int pin, bool activeHigh = true);
void loopXkcY26Pnp(size_t index);
XkcY26PnpReading readXkcY26PnpReading(size_t index);
bool readXkcY26PnpDetected(size_t index);
int readXkcY26PnpRaw(size_t index);

#endif // XKC_Y26_PNP_SRL_H
