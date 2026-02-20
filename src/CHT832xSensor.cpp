// Brian Willy
// Copyright 2025


#include "CHT832xSensor.h"
// #include "BufferedLogger.h"   // your existing logger type
#include <shared_vars.h>

// extern BufferedLogger logger;

CHT832xSensor::CHT832xSensor(uint8_t i2cAddress)
    : _addr(i2cAddress), _wire(&Wire)
{
}

void CHT832xSensor::begin(TwoWire *wire)
{
    if (wire) {
        _wire = wire;
    }

    // Ensure I2C is initialized (ok if it's already done elsewhere)
    _wire->begin();

    logger.logf("CHT832xSensor: begin() on addr 0x%02X\n", _addr);
}

void CHT832xSensor::setAddress(uint8_t i2cAddress)
{
    _addr = i2cAddress;
}

// Polynomial 0x31, init 0xFF (Sensirion CRC-8)
uint8_t CHT832xSensor::computeCrc(const uint8_t *data, size_t len) const
{
    uint8_t crc = 0xFF;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; ++bit) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x31;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

bool CHT832xSensor::read(float &temperatureC, float &humidityRH)
{
    // 1) Kick off a single-shot measurement: 0x24 0x00
    _wire->beginTransmission(_addr);
    _wire->write(0x24);
    _wire->write(0x00);
    int err = _wire->endTransmission();
    if (err != 0) {
        logger.logf("CHT832xSensor: command failed, err=%d\n", err);
        return false;
    }

    // 2) Wait for conversion
    delay(60);  // ms – per datasheet / sample code

    // 3) Read 6-byte frame: T_MSB, T_LSB, T_CRC, RH_MSB, RH_LSB, RH_CRC
    int requested = _wire->requestFrom((int)_addr, 6);
    if (requested != 6) {
        logger.logf("CHT832xSensor: requestFrom=%d (expected 6)\n", requested);
        return false;
    }

    if (_wire->available() < 6) {
        logger.log("CHT832xSensor: fewer than 6 bytes available\n");
        return false;
    }

    uint8_t t_msb = _wire->read();
    uint8_t t_lsb = _wire->read();
    uint8_t t_crc = _wire->read();

    uint8_t h_msb = _wire->read();
    uint8_t h_lsb = _wire->read();
    uint8_t h_crc = _wire->read();

    uint8_t t_raw_bytes[2] = { t_msb, t_lsb };
    uint8_t h_raw_bytes[2] = { h_msb, h_lsb };

    // Optional but nice: CRC validation
    uint8_t t_crc_calc = computeCrc(t_raw_bytes, 2);
    uint8_t h_crc_calc = computeCrc(h_raw_bytes, 2);

    if (t_crc != t_crc_calc) {
        logger.logf("CHT832xSensor: temp CRC mismatch (got 0x%02X, expected 0x%02X)\n",
                    t_crc, t_crc_calc);
        return false;
    }

    if (h_crc != h_crc_calc) {
        logger.logf("CHT832xSensor: humidity CRC mismatch (got 0x%02X, expected 0x%02X)\n",
                    h_crc, h_crc_calc);
        return false;
    }

    uint16_t t_raw = ((uint16_t)t_msb << 8) | t_lsb;
    uint16_t h_raw = ((uint16_t)h_msb << 8) | h_lsb;

    // SHT3x/CHT832x transfer function
    temperatureC = -45.0f + 175.0f * (t_raw / 65535.0f);
    humidityRH   = 100.0f * (h_raw / 65535.0f);

    return true;
}
