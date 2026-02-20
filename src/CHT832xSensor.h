#pragma once


// Brian Willy
// Copyright 2025


#include <Arduino.h>
#include <Wire.h>

// Forward-declare your logger so the driver can log like ACS
// class BufferedLogger;
// extern BufferedLogger logger;

/**
 * OO driver for CHT832x / SHT3x-style I2C temp/RH sensor.
 *
 * Usage:
 *   CHT832xSensor envSensor(0x44);
 *   envSensor.begin();  // optional: envSensor.begin(&Wire);
 *   float t, h;
 *   if (envSensor.read(t, h)) { ... }
 */
class CHT832xSensor {
public:
    explicit CHT832xSensor(uint8_t i2cAddress = 0x44);

    // Optionally supply an alternate I2C bus; defaults to &Wire
    void begin(TwoWire *wire = &Wire);
    void setAddress(uint8_t i2cAddress);

    // Read temperature (°C) and humidity (%RH).
    // Returns true on success.
    bool read(float &temperatureC, float &humidityRH);

    // For debugging / health check
    uint8_t getAddress() const { return _addr; }

private:
    uint8_t  _addr;
    TwoWire *_wire;

    // Optional CRC check (SHT3x/CHT832x frame CRC)
    // Sensirion-style CRC-8 (poly 0x31, init 0xFF)
    uint8_t computeCrc(const uint8_t *data, size_t len) const;
};
