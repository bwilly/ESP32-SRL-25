#define MQTT_MAX_PACKET_SIZE 512
#define MQTT_DEBUG

#include "MessagePublisher.h"
#include <Arduino.h>
#include <math.h>
#include <time.h>
// #include <BufferedLogger.h>
#include <shared_vars.h>

// v4 topic example: srl/site/natasha/metric/propulsion/gear/temperature
// v4 topic example: srl/site/salt/metric/climate/room/temperature
// srl/site/natasha will be set for now in telegraf config

namespace {
    constexpr char METRIC_TOPIC[] = "metric";
    constexpr char TEMPERATURE_TOPIC[] = "temperature";
    constexpr char HUMIDITY_TOPIC[] = "humidity";
    constexpr char PUMP_STATE_TOPIC[] = "pump";

    const char *baseNameFor(const SensorMetadata &metadata, const String &defaultBaseName) {
        return metadata.asset.empty() ? defaultBaseName.c_str() : metadata.asset.c_str();
    }

    float roundToHundredth(float value) {
        return roundf(value * 100.0f) / 100.0f;
    }

    void formatHundredthJson(float value, char *buffer, size_t bufferSize) {
        snprintf(buffer, bufferSize, "%.2f", roundToHundredth(value));
    }

    void addMetadataFields(JsonDocument &doc, const SensorMetadata &metadata) {
        if (!metadata.asset.empty()) {
            doc["a"] = metadata.asset.c_str();
        }
        if (!metadata.assetType.empty()) {
            doc["at"] = metadata.assetType.c_str();
        }
        if (!metadata.group.empty()) {
            doc["g"] = metadata.group.c_str();
        }
        if (!metadata.system.empty()) {
            doc["s"] = metadata.system.c_str();
        }
        if (!metadata.canonicalSystem.empty()) {
            doc["cs"] = metadata.canonicalSystem.c_str();
        }
        if (!metadata.parenset.empty()) {
            doc["p"] = metadata.parenset.c_str();
        }
    }
}

String MessagePublisher::buildTopic(const SensorMetadata &metadata, const char *topicSuffix) {
    const std::string &systemName =
        metadata.canonicalSystem.empty() ? metadata.system : metadata.canonicalSystem;

    String topic(METRIC_TOPIC);
    topic += "/";
    topic += systemName.c_str();
    topic += "/";
    topic += metadata.assetType.c_str();
    topic += "/";
    topic += topicSuffix;

    return topic;
}

// baseName (bn) is the esp device name, independent of sensors it may manage. also referred to as "locationName" 
void MessagePublisher::publishTemperature(PubSubClient &client, float temperature, const SensorMetadata &metadata, const String &defaultBaseName) {
    const size_t capacity = JSON_OBJECT_SIZE(16);
    DynamicJsonDocument doc(capacity);
    const String topic = buildTopic(metadata, TEMPERATURE_TOPIC);
    char valueBuffer[16];

    doc["bn"] = baseNameFor(metadata, defaultBaseName);
    doc["n"] = "temperature";
    doc["u"] = "C";
    formatHundredthJson(temperature, valueBuffer, sizeof(valueBuffer));
    doc["v"] = serialized(valueBuffer);
    doc["ut"] = (int)time(nullptr);
    addMetadataFields(doc, metadata);

    char buffer[512];
    serializeJson(doc, buffer);

    Serial.print("Publishing the following to msg broker: ");
    Serial.println(buffer);

    if (!client.connected()) {
        logger.log("MQTT client disconnected before publish!");
  
    }

    bool ok = client.publish(topic.c_str(), buffer);
    if (ok) {
        logger.log("Msg pub ok \n");
    } else {
        logger.log("Msg pub FAIL \n");
    }
}

void MessagePublisher::publishHumidity(PubSubClient &client, float humidity, const SensorMetadata &metadata, const String &defaultBaseName) {
    const size_t capacity = JSON_OBJECT_SIZE(16);
    DynamicJsonDocument doc(capacity);
    const String topic = buildTopic(metadata, HUMIDITY_TOPIC);
    char valueBuffer[16];

    doc["bn"] = baseNameFor(metadata, defaultBaseName);
    doc["n"]  = "humidity";
    doc["u"]  = "%";
    formatHundredthJson(humidity, valueBuffer, sizeof(valueBuffer));
    doc["v"]  = serialized(valueBuffer);
    doc["ut"] = (int)time(nullptr);
    addMetadataFields(doc, metadata);

    char buffer[512];
    serializeJson(doc, buffer);

    Serial.print("Publishing the following to msg broker: ");
    Serial.println(buffer);

    if (!client.connected()) {
        logger.log("MQTT client disconnected before publish!");
        // you might want to return here, but I’ll leave behavior same as temp()
    }

    bool ok = client.publish(topic.c_str(), buffer);
    if (ok) {
        logger.log("Humidity msg pub ok \n");
    } else {
        logger.log("Humidity msg pub FAIL \n");
    }
}


// void MessagePublisher::publishPumpState(PubSubClient &client, bool isOn, const String &location) {
//     const size_t capacity = JSON_OBJECT_SIZE(10);
//     DynamicJsonDocument doc(capacity);

//     doc["bn"] = location;
//     doc["n"] = "pumpState";
//     doc["u"] = "bool";
//     doc["v"] = isOn ? 1 : 0;
//     doc["ut"] = (int)time(nullptr);

//     char buffer[256];
//     serializeJson(doc, buffer);

//     Serial.print("Publishing the following to msg broker: ");
//     Serial.println(buffer);

//     client.publish(PUMP_STATE_TOPIC, buffer);
// }

void MessagePublisher::publishPumpState(PubSubClient &client, bool isOn, float amps, const SensorMetadata &metadata, const String &defaultBaseName) {
    const size_t capacity = JSON_OBJECT_SIZE(18);
    DynamicJsonDocument doc(capacity);
    const String topic = buildTopic(metadata, PUMP_STATE_TOPIC);

    doc["bn"] = baseNameFor(metadata, defaultBaseName);
    doc["n"] = "pumpState";
    doc["u"] = "bool";
    doc["v"] = isOn ? 1 : 0;
    doc["ut"] = (int)time(nullptr);
    doc["amps"] = amps;
    addMetadataFields(doc, metadata);

    char buffer[256];
    serializeJson(doc, buffer);

    Serial.print("Publishing the following to msg broker: ");
    Serial.println(buffer);

    client.publish(topic.c_str(), buffer);
}
