#pragma once

#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "ConfigModel.h"

class MessagePublisher {
public:
    static void publishTemperature(PubSubClient &client, float temperature, const SensorMetadata &metadata, const String &defaultBaseName);
    static void publishHumidity(PubSubClient &client, float humidity, const SensorMetadata &metadata, const String &defaultBaseName);
    // static void publishPumpState(PubSubClient &client, bool isOn, const String &location);
    static void publishPumpState(PubSubClient &client, bool isOn, float amps, const SensorMetadata &metadata, const String &defaultBaseName);

private:
    static String buildTopic(const SensorMetadata &metadata, const char *topicSuffix);
};
