// ConfigFetch.cpp
#include "ConfigFetch.h"

#include <WiFi.h>
#include <HTTPClient.h>

bool downloadConfigJson(const String &url, String &outJson)
{
    outJson = "";  // clear any previous content

    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println(F("s:downloadConfigJson: WiFi not connected"));
        return false;
    }

    HTTPClient http;
    Serial.print(F("s:downloadConfigJson: GET "));
    Serial.println(url);

    if (!http.begin(url))
    {
        Serial.println(F("downloadConfigJson: http.begin() failed"));
        return false;
    }

    int httpCode = http.GET();

    if (httpCode != HTTP_CODE_OK)
    {
        Serial.print(F("s:downloadConfigJson: HTTP code "));
        Serial.println(httpCode);
        http.end();
        return false;
    }

    String payload = http.getString();
    http.end();

    if (payload.length() == 0)
    {
        Serial.println(F("s:downloadConfigJson: empty response body"));
        return false;
    }

    outJson = payload;
    Serial.print(F("s:downloadConfigJson: received "));
    Serial.print(outJson.length());
    Serial.println(F(" bytes"));

    return true;
}
