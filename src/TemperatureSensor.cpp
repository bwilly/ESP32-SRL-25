// TemperatureSensor.cpp
#include "TemperatureSensor.h"
#include <cstring>

namespace
{
int hexNibble(char c)
{
    if (c >= '0' && c <= '9')
    {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f')
    {
        return 10 + (c - 'a');
    }
    if (c >= 'A' && c <= 'F')
    {
        return 10 + (c - 'A');
    }
    return -1;
}

bool parseW1Address(const std::string &src, DeviceAddress out)
{
    uint8_t bytes[8] = {0};
    size_t byteIndex = 0;
    int highNibble = -1;

    for (char c : src)
    {
        const int nibble = hexNibble(c);
        if (nibble < 0)
        {
            continue;
        }

        if (highNibble < 0)
        {
            highNibble = nibble;
            continue;
        }

        if (byteIndex >= 8)
        {
            return false;
        }

        bytes[byteIndex++] = static_cast<uint8_t>((highNibble << 4) | nibble);
        highNibble = -1;
    }

    if (highNibble >= 0 || byteIndex != 8)
    {
        return false;
    }

    std::memcpy(out, bytes, sizeof(bytes));
    return true;
}
} // namespace

TemperatureSensor::TemperatureSensor(OneWire *oneWire) : sensors(oneWire) {}

void TemperatureSensor::requestTemperatures()
{
    sensors.requestTemperatures();
}

TemperatureReading *TemperatureSensor::getTemperatureReadings(const W1Config &w1Config)
{
    for (int i = 0; i < MAX_READINGS; ++i)
    {
        readings[i] = {"", 0.0f};
    }

    const size_t count = w1Config.devices.size() < static_cast<size_t>(MAX_READINGS)
                             ? w1Config.devices.size()
                             : static_cast<size_t>(MAX_READINGS);

    for (size_t i = 0; i < count; ++i)
    {
        const W1Device &device = w1Config.devices[i];
        DeviceAddress address = {0};
        float value = DEVICE_DISCONNECTED_C;

        if (parseW1Address(device.addr, address))
        {
            value = sensors.getTempC(address);
        }

        readings[i] = {String(device.name.c_str()), value};
    }

    readings[MAX_READINGS] = {"", 0.0f}; // Ending marker with default values
    return readings;
}
