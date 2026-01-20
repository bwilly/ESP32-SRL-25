// In ParamHandler.cpp
#include "ParamHandler.h"
#include "SpiffsHandler.h"
#include "SPIFFS.h"
#include "shared_vars.h"
#include <iostream>
#include <array>
#include <string>

// debug
void printHexArrayContents(int sensorIndex)
{
    if (sensorIndex < 0 || sensorIndex >= w1Sensors.sensors.size())
    {
        Serial.println("Error: Sensor index out of bounds.");
        return;
    }

    Serial.print("Sensor contents via printHexArrayContents(int sensorIndex");
    Serial.print(sensorIndex);
    Serial.print(": ");

    for (int i = 0; i < w1Sensors.sensors[sensorIndex].HEX_ARRAY.size(); ++i)
    {
        Serial.print("0x");
        Serial.print(w1Sensors.sensors[sensorIndex].HEX_ARRAY[i], HEX);
        Serial.print(" ");
    }

    Serial.println();
}

void printHexArrayContentsViaArgs(int sensorIndex, SensorGroupW1 sensorGroup)
{
    if (sensorIndex < 0 || sensorIndex >= sensorGroup.sensors.size())
    {
        Serial.println("Error: Sensor index out of bounds.");
        return;
    }

    Serial.print("Sensor contents via printHexArrayContentsViaArgs(int sensorIndex, SensorGroupW1 sensorGroup");
    Serial.print(sensorIndex);
    Serial.print(": ");

    for (int i = 0; i < sensorGroup.sensors[sensorIndex].HEX_ARRAY.size(); ++i)
    {
        Serial.print("0x");
        Serial.print(sensorGroup.sensors[sensorIndex].HEX_ARRAY[i], HEX);
        Serial.print(" ");
    }

    Serial.println();
}

// todo: refactor this is copy/paste from SpiffsHandler.cpp
// void parseHexToArray_dup(const String &value, uint8_t array[8])
// example input: {0x28, 0x95, 0x93, 0x49, 0xf6, 0xcc, 0x3c, 0x76}
std::array<uint8_t, 8> parseHexToArray_dup(const String &value)
{
    std::array<uint8_t, 8> intoArray{}; // Create and initialize the array
    int startIndex = value.indexOf('{');
    int endIndex = value.indexOf('}');

    if (startIndex != -1 && endIndex != -1)
    {
        String subValue = value.substring(startIndex + 1, endIndex);
        int byteCount = 0;
        int commaIndex;

        // Parse each comma-separated hex byte
        while ((commaIndex = subValue.indexOf(',')) != -1 && byteCount < intoArray.size())
        {
            // below line was returned from Chat 4 classic and woudln't compile.
            // String byteStr = subValue.substring(0, commaIndex).trim(); // Trim any spaces around the byte
            String byteStr = subValue.substring(0, commaIndex);
            byteStr.trim(); // Correct usage of trim

            intoArray[byteCount] = (uint8_t)strtol(byteStr.c_str(), NULL, 16);

            // Print each parsed byte for verification
            Serial.print("Parsed hex: ");
            Serial.println("0x" + String(intoArray[byteCount], HEX));

            // Move to the next part of the string
            subValue = subValue.substring(commaIndex + 1);
            byteCount++;
        }

        // Process the last byte (after the final comma)
        if (byteCount < intoArray.size())
        {
            subValue.trim(); // Trim any spaces around the last byte
            intoArray[byteCount] = (uint8_t)strtol(subValue.c_str(), NULL, 16);

            // Print the last parsed byte
            Serial.print("Parsed hex: ");
            Serial.println("0x" + String(intoArray[byteCount], HEX));
        }
        else
        {
            Serial.println("Error. Hex array size exceeded.");
        }
    }
    else
    {
        Serial.println("Error. Braces not found in input string.");
    }

    // Verify that the entire array is populated
    Serial.print("Final array contents: ");
    for (auto hex : intoArray)
    {
        Serial.print("0x");
        Serial.print(hex, HEX);
        Serial.print(" ");
    }
    Serial.println();

    return intoArray; // Return the populated array
}

// void parseHexToArray_dup(const String &value, std::array<uint8_t, 8> &intoArray)
// {
//     int startIndex = value.indexOf('{');
//     int endIndex = value.indexOf('}');

//     if (startIndex != -1 && endIndex != -1)
//     {
//         String subValue = value.substring(startIndex + 1, endIndex);

//         int byteCount = 0;
//         int commaIndex;

//         // Parse each comma-separated hex byte
//         while ((commaIndex = subValue.indexOf(',')) != -1 && byteCount < intoArray.size())
//         {
//             String byteStr = subValue.substring(0, commaIndex);
//             byteStr.trim(); // Trim any spaces around the byte

//             // Convert hex string to uint8_t and store in array
//             intoArray[byteCount] = (uint8_t)strtol(byteStr.c_str(), NULL, 16);

//             // Print each parsed byte for verification
//             Serial.print("Parsed hex: ");
//             Serial.println("0x" + String(intoArray[byteCount], HEX));

//             // Move to the next part of the string
//             subValue = subValue.substring(commaIndex + 1);
//             byteCount++;
//         }

//         // Process the last byte (after the final comma)
//         if (byteCount < intoArray.size())
//         {
//             subValue.trim(); // Trim any spaces around the last byte
//             intoArray[byteCount] = (uint8_t)strtol(subValue.c_str(), NULL, 16);

//             // Print the last parsed byte
//             Serial.print("Parsed hex: ");
//             Serial.println("0x" + String(intoArray[byteCount], HEX));
//         }
//         else
//         {
//             Serial.println("Error. Hex array size exceeded.");
//         }

//         // Verify that the entire array is populated
//         Serial.print("Final array contents: ");
//         for (int i = 0; i < intoArray.size(); ++i)
//         {
//             Serial.print("0x");
//             Serial.print(intoArray[i], HEX);
//             Serial.print(" ");
//         }
//         Serial.println();
//     }
//     else
//     {
//         Serial.println("Error. Braces not found in input string.");
//     }
// }

bool startsWithW1(const std::string &name)
{
    // Check if `name` starts with "w1"
    return name.rfind("w1", 0) == 0;
}

bool endsWithName(const std::string &name)
{
    const std::string suffix = "-name";
    // Check if `name` ends with "-name"
    if (name.size() >= suffix.size())
    {
        return name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0;
    }
    return false;
}

int getSensorArrayIndex(const std::string &name)
{
    if (name.size() < 4 || name.substr(0, 3) != "w1-")
    {
        return -1; // Check if the format starts with "w1-" and is long enough
    }

    // The fourth character is the sensor index, directly convert it to integer
    char indexChar = name[3]; // Assuming the index is the fourth character

    // Check if the character is a digit
    if (!isdigit(indexChar))
    {
        return -1;
    }

    int sensorPosition = indexChar - '0'; // Convert char to int

    return sensorPosition - 1; // Convert to zero-based index
}

// Loop the list of expected, pre-defined params
void handlePostParameters(AsyncWebServerRequest *request)
{
    for (const auto &paramMetadata : paramList)
    {
        String value;
        bool paramFound = request->hasParam(paramMetadata.name.c_str(), true);

        if (paramMetadata.type == ParamMetadata::STRING)
        {
            if (paramFound)
            {
                Serial.print("Saving param: ");
                Serial.println(paramMetadata.name.c_str());

                const AsyncWebParameter *p = request->getParam(paramMetadata.name.c_str(), true);
                value = p->value();

                Serial.println(" -> " + value);

                if (startsWithW1(paramMetadata.name.c_str())) // W1 Name
                {
                    int index = getSensorArrayIndex(paramMetadata.name.c_str());

                    Serial.println("Sensor Index: " + String(index) + " for " + paramMetadata.name.c_str());

                    if (endsWithName(paramMetadata.name.c_str()))
                    {

                        w1Sensors.sensors[index].name = value.c_str();
                    }
                    else
                    { // the string of hex vals
                        w1Sensors.sensors[index].HEX_ARRAY = parseHexToArray_dup(value);

                        // Verify that the entire array is populated
                        Serial.print("Final array contents after return to ParamHandler: ");
                        for (int i = 0; i < w1Sensors.sensors[index].HEX_ARRAY.size(); ++i)
                        {
                            Serial.print("0x");
                            Serial.print(w1Sensors.sensors[index].HEX_ARRAY[i], HEX);
                            Serial.print(" ");
                        }
                        Serial.println();

                        printHexArrayContents(0);
                        printHexArrayContentsViaArgs(index, w1Sensors);
                    }
                }
                else
                { // any other type of string

                    if (!value.isEmpty())
                    {
                        *(paramToVariableMap[paramMetadata.name]) = value.c_str(); // i don't think this is needed. we are saving to file and then system restarts
                        writeFile(SPIFFS, paramMetadata.spiffsPath.c_str(), value.c_str());
                    }
                }
            }
        }
        // else if (paramMetadata.type == ParamMetadata::BOOLEAN)
        // {
        //     Serial.print("Saving param: ");
        //     Serial.println(paramMetadata.name);

        //     // Checkboxes are a special case. If the parameter is not found, it means the checkbox was not checked.
        //     bool isChecked = paramFound && request->getParam(paramMetadata.name.c_str(), true)->value() == "on";
        //     // bool isChecked = request->getParam(paramMetadata.name.c_str(), true)->value() == "on";
        //     *(paramToBoolMap[paramMetadata.name]) = isChecked;
        //     writeFile(SPIFFS, paramMetadata.spiffsPath.c_str(), isChecked ? "true" : "false");
        // }

        else
        {
            Serial.print("Unexpected parameter: ");
            Serial.println(paramMetadata.name.c_str());
        }
        // ... additional handling for other types
        // Now, save these to SPIFFS regardless of whether they were received in the POST or not.
        // writeFile(SPIFFS, enableW1Path.c_str(), enableW1 ? "true" : "false");
        // writeFile(SPIFFS, enableDHTPath.c_str(), enableDHT ? "true" : "false");
        // writeFile(SPIFFS, enableMQTTPath.c_str(), enableMQTT ? "true" : "false");

        // Special handling for complex types like w1Address and w1Name
        // ...
    }

    // debug
    printHexArrayContents(0);
    printHexArrayContentsViaArgs(0, w1Sensors);

    // save the w1 sensors as json to its w1 file
    saveW1SensorConfigToFile(SPIFFS, "/w1Json", w1Sensors);
}

// void handlePostParameters(AsyncWebServerRequest *request)
// {
//     for (const auto &paramMetadata : paramList)
//     {
//         if (request->hasParam(paramMetadata.name.c_str(), true))
//         {
//             AsyncWebParameter *p = request->getParam(paramMetadata.name.c_str(), true);
//             String value = p->value();

//             if (paramMetadata.type == ParamMetadata::STRING)
//             {
//                 // Handle String type
//                 if (paramToVariableMap.find(paramMetadata.name) != paramToVariableMap.end())
//                 {
//                     *(paramToVariableMap[paramMetadata.name]) = value;
//                 }
//             }
//             else if (paramMetadata.type == ParamMetadata::BOOLEAN)
//             {
//                 // Handle Boolean type
//                 if (paramToBoolMap.find(paramMetadata.name) != paramToBoolMap.end())
//                 {
//                     *(paramToBoolMap[paramMetadata.name]) = (value == "true" || value == "on");
//                 }
//             }
//             // ... additional handling for other types

//             // Special handling for w1Address array
//             if (paramMetadata.name == "w1-1" || paramMetadata.name == "w1-2" || paramMetadata.name == "w1-3")
//             {
//                 int index = (paramMetadata.name == "w1-1") ? 0 : (paramMetadata.name == "w1-2") ? 1
//                                                                                                 : 2;
//                 parseHexToArray_dup(value, w1Address[index]);
//             }

//             // Special handling for w1Name array
//             if (paramMetadata.name == PARAM_W1_1_NAME)
//             {
//                 w1Name[0] = value;
//             }
//             else if (paramMetadata.name == PARAM_W1_2_NAME)
//             {
//                 w1Name[1] = value;
//             }
//             else if (paramMetadata.name == PARAM_W1_3_NAME)
//             {
//                 w1Name[2] = value;
//             }

//             // Write to file only if the value is non-empty
//             if (!value.isEmpty())
//             {
//                 writeFile(SPIFFS, paramMetadata.spiffsPath.c_str(), value.c_str());
//             }
//         }
//     }
// }

// void handlePostParameters(AsyncWebServerRequest *request)
// {
//     for (const auto &paramMetadata : paramList)
//     {
//         if (request->hasParam(paramMetadata.name.c_str(), true))
//         {
//             AsyncWebParameter *p = request->getParam(paramMetadata.name.c_str(), true);

//             if (paramMetadata.type == ParamMetadata::STRING)
//             {
//                 // Handle String type
//                 if (paramToVariableMap.find(paramMetadata.name) != paramToVariableMap.end())
//                 {
//                     *(paramToVariableMap[paramMetadata.name]) = p->value().c_str();
//                 }
//             }
//             else if (paramMetadata.type == ParamMetadata::BOOLEAN)
//             {
//                 // Handle Boolean type
//                 if (paramToBoolMap.find(paramMetadata.name) != paramToBoolMap.end())
//                 {
//                     *(paramToBoolMap[paramMetadata.name]) = (p->value() == "true" || p->value() == "on");
//                 }
//             }
//             // else if (paramMetadata.type == ParamMetadata::NUMBER)
//             // {
//             //     // Handle Number type
//             //     if (paramToIntMap.find(paramMetadata.name) != paramToIntMap.end())
//             //     {
//             //         *(paramToIntMap[paramMetadata.name]) = p->value().toInt();
//             //     }
//             // }

//             if (paramMetadata.name == "w1-1" || paramMetadata.name == "w1-2" || paramMetadata.name == "w1-3")
//             {
//                 // Special handling for w1Address array
//                 int index = (paramMetadata.name == "w1-1") ? 0 : (paramMetadata.name == "w1-2") ? 1
//                                                                                                 : 2;
//                 parseHexToArray_dup(p->value(), w1Address[index]);
//             }

//             // Special handling for w1Name array
//             if (paramMetadata.name == PARAM_W1_1_NAME)
//             {
//                 w1Name[0] = p->value().c_str();
//             }
//             else if (paramMetadata.name == PARAM_W1_2_NAME)
//             {
//                 w1Name[1] = p->value().c_str();
//             }
//             else if (paramMetadata.name == PARAM_W1_3_NAME)
//             {
//                 w1Name[2] = p->value().c_str();
//             }

//             writeFile(SPIFFS, paramMetadata.spiffsPath.c_str(), p->value().c_str());
//         }
//     }
// }

// void handlePostParameters(AsyncWebServerRequest *request)
// {
//     // Default values
//     bool enableW1 = false;
//     bool enableDHT = false;
//     bool enableMQTT = false;

//     bool enableW1_checked = false;
//     bool enableDHT_checked = false;
//     bool enableMQTT_checked = false;

//     int params = request->params();
//     for (int i = 0; i < params; i++)
//     {
//         AsyncWebParameter *p = request->getParam(i);
//         // if (p->isPost())
//         if (p->isPost() && p->value().length() > 0) // This checks if the value is not empty
//         {
//             if (p->name() == PARAM_WIFI_SSID)
//             {
//                 ssid = p->value().c_str();
//                 Serial.print("SSID set to: ");
//                 Serial.println(ssid);
//                 // Write file to save value
//                 writeFile(SPIFFS, ssidPath.c_str(), ssid.c_str());
//             }
//             // HTTP POST pass value
//             if (p->name() == PARAM_WIFI_PASS)
//             {
//                 pass = p->value().c_str();
//                 Serial.print("Password set to: ");
//                 Serial.println(pass);
//                 // Write file to save value
//                 writeFile(SPIFFS, passPath.c_str(), pass.c_str());
//             }
//             // HTTP POST location value
//             if (p->name() == PARAM_LOCATION)
//             {
//                 locationName = p->value().c_str();
//                 Serial.print("Location (for mDNS Hostname and Prometheus): ");
//                 Serial.println(locationName);
//                 // Write file to save value
//                 writeFile(SPIFFS, locationNamePath.c_str(), locationName.c_str());
//             }
//             // HTTP POST location value
//             if (p->name() == PARAM_PIN_DHT)
//             {
//                 pinDht = p->value().c_str();
//                 Serial.print("Pin (for DHT-22): ");
//                 Serial.println(pinDht);
//                 // Write file to save value
//                 writeFile(SPIFFS, pinDhtPath.c_str(), pinDht.c_str());
//             }
//             // HTTP POST location value
//             if (p->name() == PARAM_MQTT_SERVER)
//             {
//                 mqttServer = p->value().c_str();
//                 Serial.print("PARAM_MQTT_SERVER: ");
//                 Serial.println(mqttServer);
//                 // Write file to save value
//                 writeFile(SPIFFS, mqttServerPath.c_str(), mqttServer.c_str());
//             }
//             // HTTP POST location value
//             if (p->name() == PARAM_MQTT_PORT)
//             {
//                 mqttPort = p->value().c_str();
//                 Serial.print("PARAM_MQTT_PORT: ");
//                 Serial.println(mqttPort);
//                 // Write file to save value
//                 writeFile(SPIFFS, mqttPortPath.c_str(), mqttPort.c_str());
//             }
//             if (p->name() == PARAM_MAIN_DELAY)
//             {
//                 mainDelay = p->value().c_str();
//                 Serial.print("PARAM_MAIN_DELAY: ");
//                 Serial.println(mainDelay);
//                 // Write file to save value
//                 writeFile(SPIFFS, mainDelayPath.c_str(), mainDelay.c_str());
//             }
//             if (p->name() == PARAM_W1_1_NAME)
//             {
//                 w1Name[0] = p->value().c_str();
//                 Serial.print("PARAM_W1_1_NAME: ");
//                 Serial.println(w1Name[0]);
//                 // Write file to save value
//                 writeFile(SPIFFS, w1_1_name_Path.c_str(), w1Name[0].c_str());
//             }
//             if (p->name() == PARAM_W1_2_NAME)
//             {
//                 w1Name[1] = p->value().c_str();
//                 Serial.print("PARAM_W1_2_NAME: ");
//                 Serial.println(w1Name[1]);
//                 // Write file to save value
//                 writeFile(SPIFFS, w1_2_name_Path.c_str(), w1Name[1].c_str());
//             }
//             if (p->name() == PARAM_W1_3_NAME)
//             {
//                 w1Name[2] = p->value().c_str();
//                 Serial.print("PARAM_W1_3_NAME: ");
//                 Serial.println(w1Name[2]);
//                 // Write file to save value
//                 writeFile(SPIFFS, w1_3_name_Path.c_str(), w1Name[2].c_str());
//             }
//             if (p->name() == PARAM_W1_1)
//             {
//                 parseAndStoreHex(p->value(), 0);
//                 writeFile(SPIFFS, w1_1Path.c_str(), p->value().c_str());
//             }
//             else if (p->name() == PARAM_W1_2)
//             {
//                 parseAndStoreHex(p->value(), 1);
//                 writeFile(SPIFFS, w1_2Path.c_str(), p->value().c_str());
//             }
//             else if (p->name() == PARAM_W1_3)
//             {
//                 parseAndStoreHex(p->value(), 2);
//                 writeFile(SPIFFS, w1_3Path.c_str(), p->value().c_str());
//             }

//             else if (p->name() == PARAM_ENABLE_W1 && p->value() == "on")
//             {
//                 enableW1 = true;
//             }
//             else if (p->name() == PARAM_ENABLE_DHT && p->value() == "on")
//             {
//                 enableDHT = true;
//             }
//             else if (p->name() == PARAM_ENABLE_MQTT && p->value() == "on")
//             {
//                 enableMQTT = true;
//             }
//         }
//     }
//     // Now, save these to SPIFFS regardless of whether they were received in the POST or not.
//     writeFile(SPIFFS, enableW1Path.c_str(), enableW1 ? "true" : "false");
//     writeFile(SPIFFS, enableDHTPath.c_str(), enableDHT ? "true" : "false");
//     writeFile(SPIFFS, enableMQTTPath.c_str(), enableMQTT ? "true" : "false");
// }
