// ConfigFetch.cpp
#include "ConfigFetch.h"

#include <WiFi.h>
#include <HTTPClient.h>

bool downloadConfigJson(const std::string &url, std::string &outJson)
{
    outJson.clear();  // clear any previous content

    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println(F("s:downloadConfigJson: WiFi not connected"));
        return false;
    }

    HTTPClient http;
    Serial.print(F("s:downloadConfigJson: GET "));
    Serial.println(url.c_str());

    if (!http.begin(url.c_str()))
    {
        Serial.println(F("downloadConfigJson: http.begin() failed"));
        return false;
    }

    int httpCode = http.GET();

    if (httpCode != HTTP_CODE_OK)
    {
        char buf[128];
        snprintf(buf, sizeof(buf), "s:downloadConfigJson: HTTP code %d\n", httpCode);
        Serial.print(buf);
        http.end();
        return false;
    }

    String payload = http.getString();  // HTTPClient returns Arduino String
    http.end();

    if (payload.length() == 0)
    {
        Serial.println(F("s:downloadConfigJson: empty response body"));
        return false;
    }

    outJson = payload.c_str();  // Convert Arduino String to std::string
    
    char buf[128];
    snprintf(buf, sizeof(buf), "s:downloadConfigJson: received %zu bytes\n", outJson.length());
    Serial.print(buf);

    return true;
}