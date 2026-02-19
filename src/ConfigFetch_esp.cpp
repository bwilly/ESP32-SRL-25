// ConfigFetch_esp.cpp
#include "ConfigFetch.h"
#include "IFileStore.h"
#include "ILogger.h"

#include <WiFi.h>
#include <HTTPClient.h>

// Constructor implementation
ConfigFetch::ConfigFetch(IFileStore& fs, ILogger& log) : fs_(fs), log_(log) {}

// Method implementation (note the ConfigFetch:: scope)
bool ConfigFetch::downloadConfigJson(const std::string& url, std::string& outJson)
{
    outJson.clear();  // clear any previous content

    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println(F("s:downloadConfigJson: WiFi not connected"));
        log_.log("downloadConfigJson: WiFi not connected\n");
        return false;
    }

    HTTPClient http;
    Serial.print(F("s:downloadConfigJson: GET "));
    Serial.println(url.c_str());
    log_.log("downloadConfigJson: GET ");    
    log_.log(url.c_str());
    log_.log("\n");

    if (!http.begin(url.c_str()))
    {
        Serial.println(F("downloadConfigJson: http.begin() failed"));
        log_.log("downloadConfigJson: http.begin() failed\n");
        return false;
    }

    int httpCode = http.GET();

    if (httpCode != HTTP_CODE_OK)
    {
        char buf[128];
        snprintf(buf, sizeof(buf), "s:downloadConfigJson: HTTP code %d\n", httpCode);
        Serial.print(buf);
        log_.log(buf);
        http.end();
        return false;
    }

    String payload = http.getString();  // HTTPClient returns Arduino String
    http.end();

    if (payload.length() == 0)
    {
        Serial.println(F("s:downloadConfigJson: empty response body"));
        log_.log("downloadConfigJson: empty response body");
        log_.log("\n");
        return false;
    }

    outJson = payload.c_str();  // Convert Arduino String to std::string
    
    char buf[128];
    snprintf(buf, sizeof(buf), "s:downloadConfigJson: received %zu bytes\n", outJson.length());
    Serial.print(buf);
    log_.log(buf);

    return true;
}
