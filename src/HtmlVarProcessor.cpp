// In HtmlVarProcessor.cpp
#include "shared_vars.h"
#include "SpiffsHandler.h"
#include "SPIFFS.h"
#include "TemperatureReading.h" // Make sure to include the header for TemperatureReading

// PoC method.
String SendHTML(TemperatureReading *readings, int numReadings)
{
    String ptr = "<!DOCTYPE html> <html>\n";
    ptr += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
    ptr += "<title>ESP32 Temperature Monitor</title>\n";
    ptr += "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
    ptr += "body{margin-top: 50px;} h1 {color: #444444;margin: 50px auto 30px;}\n";
    ptr += "p {font-size: 24px;color: #444444;margin-bottom: 10px;}\n";
    ptr += "</style>\n";
    ptr += "</head>\n";
    ptr += "<body>\n";
    ptr += "<div id=\"webpage\">\n";
    ptr += "<h1>ESP32 Temperature Monitor</h1>\n";

    for (int i = 0; i < numReadings; ++i)
    {
        ptr += "<p>";
        ptr += readings[i].name;
        ptr += ": ";
        ptr += readings[i].value;
        ptr += "&deg;C</p>";
    }

    ptr += "</div>\n";
    ptr += "</body>\n";
    ptr += "</html>\n";
    return ptr;
}

String processor(const String &var)
{
    // Check for variables in the form of "w1-<index>" or "w1-<index>-name"
    if (var.startsWith("w1-"))
    {
        int indexStart = 3; // Position after "w1-"
        int hyphenPos = var.indexOf('-', indexStart);

        // Extract the sensor index (one-based) and convert to zero-based
        int sensorIndex = var.substring(indexStart, hyphenPos).toInt() - 1;
        if (sensorIndex < 0 || sensorIndex >= w1Sensors.sensors.size())
        {
            return ""; // Invalid index, return empty
        }

        // Check if var ends with "-name"
        if (var.endsWith("-name"))
        {
            return String(w1Sensors.sensors[sensorIndex].name.c_str()); // Convert std::string to String
        }
        else
        {
            // Return sensor address as a comma-separated hex string
            String address;
            for (int i = 0; i < w1Sensors.sensors[sensorIndex].HEX_ARRAY.size(); ++i)
            {
                if (i > 0)
                {
                    address += ",";
                }
                address += "0x";
                if (w1Sensors.sensors[sensorIndex].HEX_ARRAY[i] < 0x10)
                {
                    address += "0"; // Pad single digit hex values
                }
                address += String(w1Sensors.sensors[sensorIndex].HEX_ARRAY[i], HEX);
            }
            return address;
        }
    }

    // Fallback: existing paramList processing for other variables
    for (const auto &paramMetadata : paramList)
    {
        if (var == paramMetadata.name && paramMetadata.type == ParamMetadata::BOOLEAN)
        {
            String fileVal = readFile(SPIFFS, paramMetadata.spiffsPath.c_str());

            Serial.print("Reading ");
            Serial.println(paramMetadata.name);
            Serial.print("Value: ");
            Serial.println(fileVal);

            if (fileVal == "true")
            {
                return "checked";
            }
            else
            {
                return "";
            }
        }
        if (var == paramMetadata.name)
        {
            return readFile(SPIFFS, paramMetadata.spiffsPath.c_str());
        }
    }

    return ""; // Return empty if var does not match any known parameters
}

// String processor(const String &var)
// {
//     // Iterate over paramList to find the matching parameter
//     for (const auto &paramMetadata : paramList)
//     {
//         if (var == paramMetadata.name && paramMetadata.type == ParamMetadata::BOOLEAN)
//         {

//             String fileVal = readFile(SPIFFS, paramMetadata.spiffsPath.c_str());

//             Serial.print("Reading ");
//             Serial.println(paramMetadata.name);
//             Serial.print("Value: ");
//             Serial.println(fileVal);

//             if (fileVal == "true")
//             {
//                 return "checked";
//             }
//             else
//             {
//                 return "";
//             }
//         }
//         if (var == paramMetadata.name)
//         {
//             return readFile(SPIFFS, paramMetadata.spiffsPath.c_str());
//         }
//     }

//     if (var == "STATE") // this is from the example sample
//     {
//         // if (digitalRead(ledPin))
//         // {
//         //     ledState = "ON";
//         // }
//         // else
//         // {
//         //     ledState = "OFF";
//         // }
//         // return ledState;
//     }
//     // Handle special cases if any
//     // ...

//     // else if (var == "enableW1_checked")
//         //     {
//         //         String fileValue = readFile(SPIFFS, enableW1Path.c_str());
//         //         if (fileValue == "true")
//         //         {
//         //             return "checked";
//         //         }
//         //         else if (fileValue == "false" || fileValue == "")
//         //         {
//         //             return "";
//         //         }
//         //     }

//         return String(); // Return an empty string if no match is found
// }

// Replaces placeholder with LED state value
// Only used for printing info
// String processor(const String &var)
// {
//     Serial.println("process template token: ");
//     Serial.println(var);

//     if (var == "STATE") // this is from the example sample
//     {
//         // if (digitalRead(ledPin))
//         // {
//         //     ledState = "ON";
//         // }
//         // else
//         // {
//         //     ledState = "OFF";
//         // }
//         // return ledState;
//     }
//     else if (var == "ssid")
//     {
//         return readFile(SPIFFS, ssidPath.c_str());
//     }
//     else if (var == "pass")
//     {
//         return readFile(SPIFFS, passPath.c_str());
//     }
//     else if (var == "location")
//     {
//         return readFile(SPIFFS, locationNamePath.c_str());
//     }
//     else if (var == "pinDht")
//     {
//         return readFile(SPIFFS, pinDhtPath.c_str());
//     }
//     else if (var == "mqtt-server")
//     {
//         return readFile(SPIFFS, mqttServerPath.c_str());
//     }
//     else if (var == "mqtt-port")
//     {
//         return readFile(SPIFFS, mqttPortPath.c_str());
//     }
//     else if (var == "main-delay")
//     {
//         return readFile(SPIFFS, mainDelayPath.c_str());
//     }
//     else if (var == "w1-1")
//     {
//         return readFile(SPIFFS, w1_1Path.c_str());
//     }
//     else if (var == "w1-2")
//     {
//         return readFile(SPIFFS, w1_2Path.c_str());
//     }
//     else if (var == "w1-3")
//     {
//         return readFile(SPIFFS, w1_3Path.c_str());
//     }
//     else if (var == "w1-1-name")
//     {
//         return readFile(SPIFFS, w1_1_name_Path.c_str());
//     }
//     else if (var == "w1-2-name")
//     {
//         return readFile(SPIFFS, w1_2_name_Path.c_str());
//     }
//     else if (var == "w1-3-name")
//     {
//         return readFile(SPIFFS, w1_3_name_Path.c_str());
//     }

//     else if (var == "enableW1")
//     {
//         return readFile(SPIFFS, enableW1Path.c_str());
//     }
//     else if (var == "enableDHT")
//     {
//         return readFile(SPIFFS, enableDHTPath.c_str());
//     }
//     else if (var == "enableMQTT")
//     {
//         return readFile(SPIFFS, enableMQTTPath.c_str());
//     }
//     else if (var == "enableW1_checked")
//     {
//         String fileValue = readFile(SPIFFS, enableW1Path.c_str());
//         if (fileValue == "true")
//         {
//             return "checked";
//         }
//         else if (fileValue == "false" || fileValue == "")
//         {
//             return "";
//         }
//     }
//     else if (var == "enableW1_checked")
//     {
//         String fileValue = readFile(SPIFFS, enableW1Path.c_str());
//         if (fileValue == "true")
//         {
//             return "checked";
//         }
//         else if (fileValue == "false" || fileValue == "")
//         {
//             return "";
//         }
//     }
//     else if (var == "enableDHT_checked")
//     {
//         String fileValue = readFile(SPIFFS, enableDHTPath.c_str());
//         if (fileValue == "true")
//         {
//             return "checked";
//         }
//         else if (fileValue == "false" || fileValue == "")
//         {
//             return "";
//         }
//     }
//     else if (var == "enableMQTT_checked")
//     {
//         String fileValue = readFile(SPIFFS, enableMQTTPath.c_str());
//         if (fileValue == "true")
//         {
//             return "checked";
//         }
//         else if (fileValue == "false" || fileValue == "")
//         {
//             return "";
//         }
//     }
//     else
//         return String();

//     // ssid = readFile(SPIFFS, ssidPath);
//     // pass = readFile(SPIFFS, passPath);
//     // locationName = readFile(SPIFFS, locationNamePath);
//     // pinDht = readFile(SPIFFS, pinDhtPath);
// }
