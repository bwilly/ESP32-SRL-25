#include "XKC-Y26-PNP_SRL.h"
#include "version.h" // Version macros
#include "shared_vars.h"

static bool pinSupportsInternalPullDown(int pin) {
    return pin < 34 || pin > 39;
}

// @pattern class
// Constructor with pin parameter
XkcY26PnpSensor::XkcY26PnpSensor(int pin, bool activeHigh)
    : _pinXkc(pin), _activeHigh(activeHigh)
{
}

// Non-contact liquid level sensor
void XkcY26PnpSensor::begin() {
    

    // IMPORTANT:
    // _pinXkc MUST be provided by configuration.
    // There is intentionally NO default here.
    // The fallback pin is resolved at setup time, not hardcoded in begin().

    if (_pinXkc <= 0)
    {
        logger.log("XKC-Y26-PNP: invalid pin provided; sensor not initialized\n");
        return;
    }

    // XKC-Y26-PNP is a digital output sensor. Use pulldown so an inactive or
    // disconnected input does not float. Ensure sensor output voltage is ESP32-safe.
    if (pinSupportsInternalPullDown(_pinXkc)) {
        pinMode(_pinXkc, INPUT_PULLDOWN);
    } else {
        pinMode(_pinXkc, INPUT);
        logger.logf("XKC-Y26-PNP: pin %d has no ESP32 internal pulldown; add external pulldown or use another GPIO\n", _pinXkc);
    }
    logger.logf("XKC-Y26-PNP Liquid Level Sensor Initialized on pin %d\n", _pinXkc);
}

int XkcY26PnpSensor::readRaw() {
    if (_pinXkc <= 0) return LOW;
    return digitalRead(_pinXkc);
}

XkcY26PnpReading XkcY26PnpSensor::readReading() {
    XkcY26PnpReading reading;
    if (_pinXkc <= 0) return reading;

    for (int i = 0; i < NUM_SAMPLES; ++i) {
        int rawValue = readRaw();
        bool active = _activeHigh ? (rawValue == HIGH) : (rawValue == LOW);
        if (rawValue == HIGH) {
            reading.rawMask |= (1U << i);
        }
        if (active) {
            ++reading.activeCount;
        }

        delayMicroseconds(500);
    }

    reading.detected = reading.activeCount > (NUM_SAMPLES / 2);
    return reading;
}

bool XkcY26PnpSensor::readDetected() {
    return readReading().detected;
}

void XkcY26PnpSensor::serialOutInfo() {
    XkcY26PnpReading reading = readReading();
    logger.logf("XKC-Y26-PNP detected=%s activeCount=%u rawMask=0x%02X pin=%d\n",
                reading.detected ? "true" : "false",
                reading.activeCount,
                reading.rawMask,
                _pinXkc);
}

static XkcY26PnpSensor *sensors[MAX_XKC_Y26_PNP_INSTANCES] = {};

bool setupXkcY26Pnp(size_t index, int pin, bool activeHigh) {
    if (index >= MAX_XKC_Y26_PNP_INSTANCES || pin <= 0)
        return false;

    delete sensors[index];
    sensors[index] = new XkcY26PnpSensor(pin, activeHigh);
    sensors[index]->begin();
    return true;
}

void loopXkcY26Pnp(size_t index) {
    if (index >= MAX_XKC_Y26_PNP_INSTANCES || sensors[index] == nullptr) return;
    sensors[index]->serialOutInfo();
}

XkcY26PnpReading readXkcY26PnpReading(size_t index) {
    if (index >= MAX_XKC_Y26_PNP_INSTANCES || sensors[index] == nullptr) return XkcY26PnpReading{};
    return sensors[index]->readReading();
}

bool readXkcY26PnpDetected(size_t index) {
    if (index >= MAX_XKC_Y26_PNP_INSTANCES || sensors[index] == nullptr) return false;
    return sensors[index]->readDetected();
}

int readXkcY26PnpRaw(size_t index) {
    if (index >= MAX_XKC_Y26_PNP_INSTANCES || sensors[index] == nullptr) return LOW;
    return sensors[index]->readRaw();
}
