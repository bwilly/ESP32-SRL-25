#include "WebRoutes.h"

#include <Arduino.h>
#include <WiFi.h>
#include <SPIFFS.h>

// Your project headers / globals
#include "shared_vars.h"
#include "HtmlVarProcessor.h"
#include "ConfigDump.h"
#include "ConfigLoad.h"
#include "TemperatureReading.h"
#include "TemperatureSensor.h"
// #include "BootstrapConfig.h"

// ---- Externs from elsewhere in your codebase ----

extern String version;

// UI / sensors
extern String printDS18b20(void);
extern String SendHTML(TemperatureReading *readings, int maxReadings);
extern String buildPrometheusMultiTemptExport(TemperatureReading *readings);
extern const char *readAndGeneratePrometheusExport(const char *deviceName);
extern float readDHTTemperature();

// CHT sensor global you referenced (envSensor.read)
extern decltype(envSensor) envSensor;

// W1 / temp sensor globals referenced by routes
extern TemperatureSensor temptSensor;

// Legacy POST handler you’re retiring later (kept for now)
extern void handlePostParameters(AsyncWebServerRequest *request);

// Bootstrapping JSON save function (MUST have external linkage; see notes below)
extern bool saveBootstrapConfigJson(const String &jsonBody, String &err);

// Cache helper
extern bool clearConfigJsonCache(fs::FS &fs);

// OTA scheduling globals
extern String g_otaUrl;
extern bool g_otaRequested;

// Legacy map still used by /ota/run currently
extern std::map<String, String *> paramToVariableMap;

// Your “processor” callback
extern String processor(const String &var);

// Bootstrap “defer work out of async_tcp” flags/buffers
extern volatile bool g_bootstrapPending;
extern String g_bootstrapBody;
extern String g_bootstrapErr;

// -------------------------
// /config/* endpoints
// -------------------------

static void registerConfigRoutesCommon(AsyncWebServer &server)
{
    // POST raw JSON bootstrap config (curl-friendly)
    server.on(
        "/config/post/bootstrap",
        HTTP_POST,
        [](AsyncWebServerRequest *request) {
            // body handler responds; empty finalizer OK
        },
        NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {

            // Hard guard to avoid RAM blowups
            if (total > 4096) {
                request->send(413, "text/plain", "Too large");
                return;
            }

            if (index == 0) {
                g_bootstrapBody = "";
                g_bootstrapBody.reserve(total);
                g_bootstrapErr = "";
            }

            g_bootstrapBody += String((char *)data, len);

            if (index + len == total) {
                // Defer JSON parse + SPIFFS write out of the async callback
                g_bootstrapPending = true;
                request->send(200, "text/plain", "Bootstrap received; applying shortly...");
            }
        });

    // View the locally stored working config: /config.json
    server.on("/config/show/FNAME_CONFIG", HTTP_GET, [](AsyncWebServerRequest *request) {
        const char *path = FNAME_CONFIG;
        if (!SPIFFS.exists(path)) {
            request->send(404, "text/plain", "No /config.json stored");
            return;
        }
        request->send(SPIFFS, path, "application/json");
    });

    // View the effective cache file (NOTE: EFFECTIVE_CACHE_PATH comes from ConfigLoad.h)
    // legacy
    server.on("/config/show/EFFECTIVE_CACHE_PATH", HTTP_GET, [](AsyncWebServerRequest *request) {
        const char *path = EFFECTIVE_CACHE_PATH;

        if (!SPIFFS.exists(path)) {
            request->send(404, "text/plain", "No effective cache file stored");
            return;
        }

        request->send(SPIFFS, path, "application/json");
    });

    // View the last downloaded remote snapshot: /config-remote.json
    server.on("/config/show/FNAME_CONFIGREMOTE", HTTP_GET, [](AsyncWebServerRequest *request) {
        const char *path = FNAME_CONFIGREMOTE;

        if (!SPIFFS.exists(path)) {
            request->send(404, "text/plain", "No /config-remote.json stored");
            return;
        }

        request->send(SPIFFS, path, "application/json");
    });

    // Clear cache helper
    // legacy
    server.on("/config/delete/file/EFFECTIVE_CACHE_PATH", HTTP_GET, [](AsyncWebServerRequest *request) {
        bool ok = deleteJsonFile(SPIFFS, EFFECTIVE_CACHE_PATH);
        if (ok) {
            request->send(200, "text/plain",
                          "legacy Config JSON cache cleared. It will not be used until remote config repopulates it.");
        } else {
            request->send(500, "text/plain", "Failed to clear config JSON cache.");
        }
    });

    server.on("/config/delete/file/FNAME_CONFIG", HTTP_GET, [](AsyncWebServerRequest *request) {
        bool ok = deleteJsonFile(SPIFFS, FNAME_CONFIG);
        if (ok) {
            request->send(200, "text/plain",
                          "Config.JSON deleted.");
        } else {
            request->send(500, "text/plain", "Failed to clear config JSON.");
        }
    });

    server.on("/config/delete/file/FNAME_BOOTSTRAP", HTTP_GET, [](AsyncWebServerRequest *request) {
        bool ok = deleteJsonFile(SPIFFS, FNAME_BOOTSTRAP);
        if (ok) {
            request->send(200, "text/plain",
                          "Config bootstrap deleted.");
        } else {
            request->send(500, "text/plain", "Failed to clear config JSON.");
        }
    });

    server.on("/config/delete/file/FNAME_CONFIGREMOTE", HTTP_GET, [](AsyncWebServerRequest *request) {
        bool ok = deleteJsonFile(SPIFFS, FNAME_CONFIGREMOTE);
        if (ok) {
            request->send(200, "text/plain",
                          "remote config stored locally has been deleted.");
        } else {
            request->send(500, "text/plain", "Failed to clear config JSON.");
        }
    });
}

// -------------------------
// /device/* endpoints
// -------------------------

static void registerDeviceRoutes(AsyncWebServer &server)
{
    server.on("/device/restart", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", "Restarting...");
        delay(300);
        ESP.restart();
    });
}

// -------------------------
// /ota/* endpoints
// -------------------------

static void registerOtaRoutes(AsyncWebServer &server)
{
    server.on("/ota/run", HTTP_GET, [](AsyncWebServerRequest *request) {

        auto it = paramToVariableMap.find("ota-url");
        if (it == paramToVariableMap.end() || it->second == nullptr) {
            request->send(400, "text/plain", "Missing or null 'ota-url' param in config");
            return;
        }

        String fwUrl = *(it->second);
        fwUrl.trim();

        if (fwUrl.length() == 0) {
            request->send(400, "text/plain", "Empty 'ota-url' value in config");
            return;
        }

        logger.log("OTA: requested via /ota/run, URL = " + fwUrl + "\n");

        // schedule OTA (run in loop() to avoid async_tcp WDT)
        g_otaUrl = fwUrl;
        g_otaRequested = true;

        request->send(200, "text/plain",
                      "OTA scheduled from " + fwUrl +
                          "\nDevice will reboot if update succeeds.");
    });
}

// -------------------------
// Station-mode UI endpoints
// -------------------------

static void registerStationUiRoutes(AsyncWebServer &server)
{
    logger.log("set web root /index.html...\n");

    // Route for root / web page
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/index.html", "text/html", false, processor);
    });
    server.serveStatic("/", SPIFFS, "/");

    // Route to Prometheus Metrics Exporter
    server.on("/metrics", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html", readAndGeneratePrometheusExport(locationName.c_str()));
    });

    server.on("/devicename", HTTP_GET, [](AsyncWebServerRequest *request) {
        // keep endpoint name stable; returning locationName matches your newer pattern
        request->send(200, "text/html", locationName);
    });

    server.on("/bssid", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html", WiFi.BSSIDstr());
    });

    server.on("/temperature", HTTP_GET, [](AsyncWebServerRequest *request) {
        char buffer[32];
        float temperature = readDHTTemperature();
        snprintf(buffer, sizeof(buffer), "%.2f", temperature);
        request->send(200, "text/html", buffer);
    });

    server.on("/cht/temperature", HTTP_GET, [](AsyncWebServerRequest *request) {
        char buffer[32];
        float chtTemp = NAN;
        float chtHum = NAN;

        if (envSensor.read(chtTemp, chtHum)) {
            snprintf(buffer, sizeof(buffer), "%.2f", chtTemp);
            request->send(200, "text/html", buffer);
        } else {
            request->send(500, "text/plain", "CHT read failed");
        }
    });

    server.on("/cht/humidity", HTTP_GET, [](AsyncWebServerRequest *request) {
        char buffer[32];
        float chtTemp = NAN;
        float chtHum = NAN;

        if (envSensor.read(chtTemp, chtHum)) {
            snprintf(buffer, sizeof(buffer), "%.2f", chtHum);
            request->send(200, "text/html", buffer);
        } else {
            request->send(500, "text/plain", "CHT read failed");
        }
    });

    // copy/paste from setup section for AP -- changing URL path
    // todo: consolidate this copied code
    server.on("/manage", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/wifimanager.html", "text/html", false, processor);
    });

    server.on("/version", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html", version);
    });

    server.on("/onewire", HTTP_GET, [](AsyncWebServerRequest *request) {
        String result = printDS18b20();
        request->send(200, "text/html", result);
    });

    // todo: find out why some readings provide 129 now, and on prev commit, they returned -127 for same bad reading. Now, the method below return -127, but this one is now 129. Odd. Aug19 '23
    server.on("/onewiretempt", HTTP_GET, [](AsyncWebServerRequest *request) {
        temptSensor.requestTemperatures();
        TemperatureReading *readings = temptSensor.getTemperatureReadings();
        request->send(200, "text/html", SendHTML(readings, MAX_READINGS));
    });

    // todo: find out why some readings provide -127
    server.on("/onewiremetrics", HTTP_GET, [](AsyncWebServerRequest *request) {
        temptSensor.requestTemperatures();
        TemperatureReading *readings = temptSensor.getTemperatureReadings();
        request->send(200, "text/html", buildPrometheusMultiTemptExport(readings));
    });

    // Legacy HTML POST handler remains for now.
    // You said you want to retire it; we’ll remove after bootstrap JSON migration is solid.
    server.on("/", HTTP_POST, [](AsyncWebServerRequest *request) {
        handlePostParameters(request);
        request->send(200, "text/plain", "Done. ESP will restart, connect to your AP");
        delay(mainDelay.toInt()); // delay(3000);
        ESP.restart();
    });
}

// -------------------------
// AP-mode UI endpoints
// -------------------------

static void registerApUiRoutes(AsyncWebServer &server)
{
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/wifimanager.html", "text/html", false, processor);
    });

    server.serveStatic("/", SPIFFS, "/"); // for things such as CSS

    // Legacy HTML POST handler remains for now.
    // You said you want JSON-only; we’ll remove this after bootstrap migration is in.
    server.on("/", HTTP_POST, [](AsyncWebServerRequest *request) {
        handlePostParameters(request);
        request->send(200, "text/plain", "Done. ESP will restart, connect to your AP");
        delay(3000);
        logger.log("Updated. Now restarting...\n");
        ESP.restart();
    });
}

// ---- Public API ----

void registerWebRoutesStation(AsyncWebServer &server)
{
    // Keep groupings stable and obvious:
    // 1) UI / metrics routes
    registerStationUiRoutes(server);

    // 2) /config/*
    registerConfigRoutesCommon(server);

    // 3) /device/*
    registerDeviceRoutes(server);

    // 4) /ota/*
    registerOtaRoutes(server);
}

void registerWebRoutesAp(AsyncWebServer &server)
{
    // 1) /config/*
    registerConfigRoutesCommon(server);

    // 2) /device/*
    registerDeviceRoutes(server);

    // 3) AP UI routes
    registerApUiRoutes(server);
}
