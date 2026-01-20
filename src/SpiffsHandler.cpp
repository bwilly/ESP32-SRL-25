#include "SpiffsHandler.h"
#include "shared_vars.h"
#include "SPIFFS.h"
#include "ParamMetadata.h"
#include <ArduinoJson.h>
#include <sstream>

std::string makePath(const char *param)
{
    return "/" + std::string(param) + ".txt";
}



void loadLegacyPersistedValues()
{
    Serial.println("AppConfig: loading persisted values from SPIFFS");

    for (const auto &param : paramList)
    {
        String value = readFile(SPIFFS, param.spiffsPath.c_str());

        Serial.print("  ");
        Serial.print(param.name.c_str());
        Serial.print(" <- ");
        Serial.print(param.spiffsPath.c_str());
        Serial.print(" = ");

        if (param.type == ParamMetadata::STRING)
        {
            auto it = paramToVariableMap.find(param.name.c_str());
            if (it != paramToVariableMap.end())
            {
                *(it->second) = value.c_str();

                // Print assigned value
                Serial.println(value.length() ? value : "<empty>");
            }
            else
            {
                Serial.println("<no target variable>");
            }
        }
        else
        {
            Serial.println("<unsupported type>");
        }
    }

    Serial.println("AppConfig: finished loading persisted values\n");
}



void parseHexToArray(const String &value, std::array<uint8_t, 8> intoArray)
{
    int startIndex = value.indexOf('{');
    int endIndex = value.indexOf('}');

    if (startIndex != -1 && endIndex != -1)
    {
        String subValue = value.substring(startIndex + 1, endIndex);

        int byteCount = 0;
        int commaIndex;
        while ((commaIndex = subValue.indexOf(',')) != -1 && byteCount < W1_NUM_BYTES)
        {
            // String byteStr = subValue.substring(0, commaIndex).trim();
            String byteStr = subValue.substring(0, commaIndex);
            byteStr.trim();
            intoArray[byteCount] = (uint8_t)strtol(byteStr.c_str(), NULL, 16);

            subValue = subValue.substring(commaIndex + 1);
            byteCount++;
        }
        if (byteCount < W1_NUM_BYTES)
        { // Process the last byte
            intoArray[byteCount] = (uint8_t)strtol(subValue.c_str(), NULL, 16);
        }
    }
}

// Read File from SPIFFS
String readFile(fs::FS &fs, const char *path)
{
    Serial.printf("Reading file: %s\r\n", path);

    File file = fs.open(path);
    if (!file || file.isDirectory())
    {
        Serial.println("- failed to open file for reading");
        return String();
    }

    String fileContent;
    while (file.available())
    {
        fileContent = file.readStringUntil('\n');
        break;
    }
    return fileContent;
}

// void loadW1AddressFromFile(fs::FS &fs, const char *path, uint8_t entryIndex)
// {
//     String content = readFile(fs, path);
//     if (content.length() > 0)
//     {
//         parseHexToArray(content, w1Address[entryIndex]); // todo:refactor so w1Address is passed as an arg rather than be global
//     }
// }

// Write file to SPIFFS
void writeFile(fs::FS &fs, const char *path, const char *message)
{
    Serial.printf("Writing file: %s\r\n", path);

    File file = fs.open(path, FILE_WRITE);
    if (!file)
    {
        Serial.println("- failed to open file for writing");
        return;
    }
    if (file.print(message))
    {
        Serial.println("- file written");
    }
    else
    {
        Serial.println("- frite failed");
    }
}

void saveW1SensorConfigToFile(fs::FS &fs, const char *path, SensorGroupW1 &w1Sensors)
{
    Serial.println("Verifying HEX_ARRAY values before JSON serialization:");
    for (const auto &sensor : w1Sensors.sensors)
    {
        Serial.print((sensor.name + ": ").c_str());

        for (auto hex : sensor.HEX_ARRAY)
        {
            Serial.printf("0x%02X ", hex);
        }
        Serial.println();
    }

    // Create a JSON document
    DynamicJsonDocument doc(2048);

    // Create an array in the JSON document
    JsonArray sensorsArray = doc.createNestedArray("sensors");

    // Iterate over each sensor in w1Sensors and add it to the JSON array
    for (const auto &sensor : w1Sensors.sensors) // todo: abstract out w1Sensors Nov10'24
    {
        JsonObject sensorObj = sensorsArray.createNestedObject();
        sensorObj["name"] = sensor.name.c_str();

        // Debug: Print sensor name
        Serial.print("Processing sensor: ");
        Serial.println(sensor.name.c_str());

        // Create an array for the address
        JsonArray addrArray = sensorObj.createNestedArray("addr");

        // Iterate over each hex value in HEX_ARRAY and add it to JSON
        for (const auto &hex : sensor.HEX_ARRAY)
        {
            // Format each byte as hex and add to JSON array
            char hexStr[5];                                  // Buffer to hold "0xXX"
            snprintf(hexStr, sizeof(hexStr), "0x%02X", hex); // Format each byte as hex
            addrArray.add(hexStr);

            // Debug: Print each hex value in source array
            Serial.print("Adding hex value to JSON: ");
            Serial.println(hexStr);
        }
    }

    // Serialize JSON to a string
    char jsonOutput[2048]; // todo:i bet i am running out of space (buffer overflow?)
    serializeJson(doc, jsonOutput, sizeof(jsonOutput));

    // Use writeFile to save the JSON string to the specified path
    writeFile(fs, path, jsonOutput);

    // Debug: Print the entire JSON output to verify final structure
    Serial.println("Saved sensors to JSON file:");
    Serial.println(jsonOutput);
}


uint8_t hexStringToByte(const char *hexString)
{
    unsigned int byte;
    std::stringstream ss;
    ss << std::hex << (hexString + 2); // Skip "0x" prefix
    ss >> byte;
    return static_cast<uint8_t>(byte);
}

void loadW1SensorConfigFromFile(fs::FS &fs, const char *path, SensorGroupW1 &w1Sensors)
{
    File file = fs.open(path, "r");
    if (!file)
    {
        Serial.println("Failed to open file for reading");
        return;
    }

    DynamicJsonDocument doc(2048);

    DeserializationError error = deserializeJson(doc, file);
    if (error)
    {
        Serial.print("Failed to parse JSON for loadW1SensorConfigFromFile. (this might be ok since we are phasing out legacy spiff in favor of remotely fetched json): ");
        Serial.println(error.c_str());
        file.close();
        return;
    }

    file.close();

    JsonArray sensorsArray = doc["sensors"];
    int i = 0;
    for (JsonObject sensorObj : sensorsArray)
    {
        if (i >= w1Sensors.sensors.size())
            break;

        w1Sensors.sensors[i].name = sensorObj["name"].as<const char *>();

        Serial.println("Hydrated name with: " + String(w1Sensors.sensors[i].name.c_str()));

        int j = 0;
        for (JsonVariant hexVal : sensorObj["addr"].as<JsonArray>())
        {
            if (j >= w1Sensors.sensors[i].HEX_ARRAY.size())
                break;
            w1Sensors.sensors[i].HEX_ARRAY[j] = hexStringToByte(hexVal.as<const char *>());
            j++;
        }
        i++;
    }

    Serial.println("Loaded sensors from JSON:");
    for (const auto &sensor : w1Sensors.sensors)
    {
        Serial.print("Name: ");
        Serial.println(sensor.name.c_str());
        Serial.print("Address: ");
        for (const auto &hex : sensor.HEX_ARRAY)
        {
            Serial.print(hex);
            Serial.print(" ");
        }
        Serial.println();
    }
}
