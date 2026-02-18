#include "WebRoutes.h"

#include <Arduino.h>
#include <WiFi.h>
#include <SPIFFS.h>

// Your project headers / globals
#include "shared_vars.h"
#include "HtmlVarProcessor.h"
// #include "ConfigDump.h"
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
extern std::map<std::string, std::string *> paramToVariableMap;

// Your “processor” callback
extern String processor(const String &var);

// Bootstrap “defer work out of async_tcp” flags/buffers
extern volatile bool g_bootstrapPending;
extern String g_bootstrapBody;
extern String g_bootstrapErr;

static void registerRoutesIndex(AsyncWebServer &server)
{
    server.on("/routes", HTTP_GET, [](AsyncWebServerRequest *request) {

        struct LinkRow {
            const char *name;
            const char *path;
            const char *method;   // "GET" or "POST"
            const char *notes;    // optional
        };

        static const LinkRow rows[] = {
            // ---- Config/common ----
            {"POST bootstrap JSON",                  "/config/post/bootstrap",                  "POST", "raw JSON (curl/Postman). Path: /config/post/bootstrap"},
            {"Show FNAME_BOOTSTRAP",                 "/config/show/FNAME_BOOTSTRAP",            "GET",  ""},
            {"Show FNAME_CONFIG",                    "/config/show/FNAME_CONFIG",               "GET",  ""},
            {"Show EFFECTIVE_CACHE_PATH (legacy)",   "/config/show/EFFECTIVE_CACHE_PATH",       "GET",  ""},
            {"Show FNAME_CONFIGREMOTE",              "/config/show/FNAME_CONFIGREMOTE",         "GET",  ""},

            {"Delete EFFECTIVE_CACHE_PATH (legacy)", "/config/delete/file/EFFECTIVE_CACHE_PATH","GET",  ""},
            {"Delete FNAME_CONFIG",                  "/config/delete/file/FNAME_CONFIG",        "GET",  ""},
            {"Delete FNAME_BOOTSTRAP",               "/config/delete/file/FNAME_BOOTSTRAP",     "GET",  ""},
            {"Delete FNAME_CONFIGREMOTE",            "/config/delete/file/FNAME_CONFIGREMOTE",  "GET",  ""},

            // ---- Device ----
            {"Device restart",                       "/device/restart",                         "GET",  ""},

            // ---- OTA ----
            {"Run OTA",                              "/ota/run",                                "GET",  ""},

            // ---- Station UI ----
            {"Root page (index)",                    "/",                                       "GET",  "SPIFFS /index.html"},
            {"Root POST (legacy form submit)",       "/",                                       "POST", "calls handlePostParameters + restart"},
            {"Prometheus metrics",                   "/metrics",                                "GET",  ""},
            {"Device name",                          "/devicename",                             "GET",  ""},
            {"BSSID",                                "/bssid",                                  "GET",  ""},
            {"DHT temperature",                      "/temperature",                            "GET",  ""},
            {"CHT temperature",                      "/cht/temperature",                        "GET",  ""},
            {"CHT humidity",                         "/cht/humidity",                           "GET",  ""},
            {"Manage (WiFi manager page)",           "/manage",                                 "GET",  "SPIFFS /wifimanager.html"},
            {"Version",                              "/version",                                "GET",  ""},
            {"OneWire (raw)",                        "/onewire",                                "GET",  ""},
            {"OneWire HTML",                         "/onewiretempt",                           "GET",  ""},
            {"OneWire metrics",                      "/onewiremetrics",                         "GET",  ""},
        };

        String html;
        html.reserve(8192);

        html += "<!doctype html><html><head>"
                "<meta charset='utf-8'/>"
                "<meta name='viewport' content='width=device-width, initial-scale=1'/>"
                "<title>Endpoints</title>"
                "<style>"
                "body{font-family:system-ui,-apple-system,Segoe UI,Roboto,Arial;margin:16px;}"
                "table{border-collapse:collapse;width:100%;}"
                "th,td{border:1px solid #ddd;padding:10px;vertical-align:top;}"
                "th{background:#f5f5f5;text-align:left;}"
                "a{word-break:break-all;}"
                ".method{font-family:ui-monospace,SFMono-Regular,Menlo,Monaco,Consolas,monospace;white-space:nowrap;}"
                ".notes{color:#666;font-size:0.9em;}"
                "textarea{width:100%;min-height:120px;}"
                "button{padding:8px 12px;}"
                "</style>"
                "</head><body>"
                "<h2>Device Endpoints</h2>"
                "<table>"
                "<tr><th>Name</th><th>Method</th><th>Action</th><th>Notes</th></tr>";

        for (auto &r : rows) {
            html += "<tr>";
            html += "<td>"; html += r.name; html += "</td>";
            html += "<td class='method'>"; html += r.method; html += "</td>";

            html += "<td>";
            if (strcmp(r.method, "GET") == 0) {
                html += "<a href='";
                html += r.path;
                html += "'>";
                html += r.path;
                html += "</a>";
            } else {
                // Render a mini form for POST endpoints.
                // NOTE: this posts form-encoded. For raw JSON endpoints, keep using curl/Postman.
                html += "<form method='POST' action='";
                html += r.path;
                html += "'>";
                html += "<textarea name='body' placeholder='Paste JSON or form content here'></textarea><br/>";
                html += "<button type='submit'>POST</button>";
                html += "</form>";
            }
            html += "</td>";

            html += "<td class='notes'>";
            html += (r.notes ? r.notes : "");
            html += "</td>";

            html += "</tr>";
        }

        html += "</table></body></html>";

        request->send(200, "text/html", html);
    });
}


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
    server.on("/config/show/FNAME_BOOTSTRAP", HTTP_GET, [](AsyncWebServerRequest *request) {
        const char *path = FNAME_BOOTSTRAP;
        if (!SPIFFS.exists(path)) {
            request->send(404, "text/plain", "No /bootstrap.json stored");
            return;
        }
        request->send(SPIFFS, path, "application/json");
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

        std::string fwUrl = *(it->second);
        
        // Trim whitespace from both ends
        fwUrl.erase(0, fwUrl.find_first_not_of(" \t\n\r"));
        fwUrl.erase(fwUrl.find_last_not_of(" \t\n\r") + 1);

        if (fwUrl.empty()) {
            request->send(400, "text/plain", "Empty 'ota-url' value in config");
            return;
        }

        char buf[256];
        snprintf(buf, sizeof(buf), "OTA: requested via /ota/run, URL = %s\n", fwUrl.c_str());
        logger.log(buf);

        // schedule OTA (run in loop() to avoid async_tcp WDT)
        g_otaUrl = String(fwUrl.c_str());
        g_otaRequested = true;

        std::string response = "OTA scheduled from " + fwUrl + "\nDevice will reboot if update succeeds.";
        request->send(200, "text/plain", response.c_str());
    });
}
// -------------------------
// Station-mode UI endpoints
// -------------------------

static void registerStationUiRoutes(AsyncWebServer &server)
{
    logger.log("set web root /index.html...\n");

    registerRoutesIndex(server);

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
        request->send(200, "text/html", String(locationName.c_str()));
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
    // 0
    registerStationUiRoutes(server);

    // 1) /config/*
    registerConfigRoutesCommon(server);

    // 2) /device/*
    registerDeviceRoutes(server);

    // 3) AP UI routes
    registerApUiRoutes(server);
}
