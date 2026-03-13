
/*********
// Brian Willy
// Climate Sensor
// Prometheus Exporter
// Copywrite 2022-2023
// Copyright 2022-2023, 2024, 2025

// Unstable Wifi post network/wifi failure
//     - plan for fix here is to switch base wifi platform code away from Rui Santos to

// Only supports DHT22 Sensor [obsolete]]

// Immediate need for DSB18b20 Sensor support and multi names for multi sensor. [done]
// I guess that will be accomplished via the hard coded promexporter names.
// Must be configurable.
// With ability to map DSB ID to a name, such as raw water in, post air cooler, post heat exchanger, etc

// Configurable:
// Location as mDNS Name Suffix
// Wifi
// ESP Pin
*/

/** some params can be set via UI. Anything params loaded later as the json web call will supersede UI params [Nov28'25] */

/*********
// *
// * Thanks to
//  Rui Santos
//  Complete instructions at https://RandomNerdTutorials.com/esp32-wi-fi-manager-asyncwebserver/
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
//  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
// *********/

// todo Nov25'25
// why is volt-test trying to send dht?
// readDHTTemperature...
// ⚠️ sensor mutex not initialized in readDHTTemperature!

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include "SPIFFS.h"

#include "WebRoutes.h"

#include "ConfigLoad.h"
#include "ConfigFetch.h"

#include "esp_log.h"
#include <WiFiMulti.h>
#include "ESPmDNS.h"

// #include <DHT_U.h>
// #include <dht.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// #include <AsyncElegantOTA.h>
#include <ElegantOTA.h>

#include <string>

#include <iostream>
#include <sstream>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#include "shared_vars.h"
#include "ParamHandler.h"
#include "SpiffsHandler.h"
#include "DHT_SRL.h"
#include "ACS712_SRL.h"
#include "prometheus_srl.h"
#include "HtmlVarProcessor.h"
#include "TemperatureSensor.h"
#include "Config.h"
#include "MessagePublisher.h"
// #include "ConfigDump.h"
#include "OtaUpdate.h"
#include "CHT832xSensor.h"
#include "SCT_SRL.h"

#include "version.h"
// #include <TelnetStream.h>

// #include <AsyncTelnetSerial.h>
#include <AsyncTCP.h> // dependency for the async streams

// #include "TelnetBridge.h" // stopped using this due to comppile conflcot in nov'25. would liek its feature returned for remote log monitor.

#include "ConfigModel.h"
#include "ConfigStorage.h"
#include "ConfigMerge.h"

#include "SpiffsFileStore.h"

#include "DeviceIdentity.h"

#include "Logger.h"
#include "EspLoggerAdapter.h"
#include <ConfigBootstrapper.h>

// #include "ConfigRemoteMerge.h"

Logger logger;
EspLoggerAdapter coreLog(logger);

// extern AsyncTelnetSerial telnetSerial;
// static AsyncTelnetSerial telnetSerial(&Serial);

// ---- Remote debug configuration ----
constexpr uint16_t SRL_TELNET_PORT = 23;
constexpr const char *SRL_TELNET_PASSWORD = "saltmeadow";

constexpr const char *AP_SSID_PREFIX = "srl-sesp-";
constexpr const char *AP_PASSWORD = "saltmeadow"; // >= 8 chars

// no good reason for these to be directives
#define MDNS_DEVICE_NAME "sesp-"
#define SERVICE_NAME "climate-http"
#define SERVICE_PROTOCOL "tcp"
#define SERVICE_PORT 80
// #define LOCATION "SandBBedroom"


// Timer variables
#define AP_REBOOT_TIMEOUT 300000 // 5 minutes in milliseconds
unsigned long reconnect_delay = 900000; // 15-minute delay before rebooting after STA disconnect
const long WIFI_CONNECT_INTERVAL = 90000; // interval to wait for Wi-Fi connection (milliseconds)
unsigned long apStartTime = 0;   // Variable to track the start time in fallback AP mode
unsigned long previousMillis = 0;
unsigned long staDisconnectStartTime = 0;


static bool lastPumpState = false; // Assume OFF at startup
static bool firstRun = true;       // New flag to force first publish

volatile bool g_otaRequested = false;
String g_otaUrl;

// Config save buffers (reusable, not allocated per-call)
StaticJsonDocument<APP_CONFIG_JSON_CAPACITY> g_configSaveDoc;

// BEFORE (something like this)
// const char* version = APP_VERSION "::" APP_COMMIT_HASH ":: TelnetBridge";

// AFTER
// String version = String(APP_VERSION) + "::" + APP_COMMIT_HASH + ":: TelnetBridge-removed";
String version = String(APP_VERSION) + "::" +
                 APP_COMMIT_HASH + "::" +
                 APP_BUILD_DATE + ":: v4: wifi stability, sensor subsystem readiness, topic; wifi timers, json-module, OTA fixed, w1, threshold. requires dups of modern shape on remote/ and remote/module for legacy upgrades.";

// trying to identify cause of unreliable dht22 readings

// Serial.println("Application Version: " APP_VERSION);
// Serial.println("Commit Hash: " APP_COMMIT_HASH);

// MQTT Server details
// const char *mqtt_server = "192.168.68.120"; // todo: change to config param
// const int mqtt_port = 1883;                 // todo: change to config param

WiFiClient espClient;
PubSubClient mqClient(espClient);
WiFiMulti wifiMulti;
static bool wifiMultiConfigured = false;

constexpr unsigned long MQTT_RECONNECT_INITIAL_DELAY_MS = 5000;
constexpr unsigned long MQTT_RECONNECT_MAX_DELAY_MS = 60000;

unsigned long mqttReconnectDelayMs = MQTT_RECONNECT_INITIAL_DELAY_MS;
unsigned long nextMqttReconnectAtMs = 0;

constexpr unsigned long WIFI_MULTI_RUN_INTERVAL_MS = 4000;
constexpr unsigned long WIFI_MULTI_CONNECT_TIMEOUT_MS = 12000;
constexpr unsigned long WIFI_DIAG_INTERVAL_MS = 2000;

// DNS settings
IPAddress primaryDNS(10, 27, 1, 30); // Your Raspberry Pi's IP (DNS server) mar'25: why is this here? is it doing anything
IPAddress secondaryDNS(8, 8, 8, 8);  // Optional: Google DNS

float previousTemperature = NAN;
float previousHumidity = NAN;
unsigned long lastPublishTime_tempt = 0;
unsigned long lastPublishTime_humidity = 0;

// For CHT832x
float previousCHTTemperature = NAN;
float previousCHTHumidity = NAN;
unsigned long lastPublishTime_temptCHT = 0;
unsigned long lastPublishTime_humidityCHT = 0;

// Globals to store the last published values
// float lastPublishedTemperature = NAN;
// float lastPublishedHumidity = NAN;

// #define DHTPIN 23 // Digital pin connected to the DHT sensor
// const int DHTPIN;
// TODO: allow pin and sensor type to be configurable

// Uncomment the type of sensor in use:
// #define DHTTYPE    DHT11     // DHT 11
// #define DHTTYPE DHT22 // DHT 22 (AM2302)
// #define DHTTYPE    DHT21     // DHT 21 (AM2301)

// DHT dht(DHTPIN, DHTTYPE);
// DHT *dht = nullptr; // global pointer declaration

// // CHT832x I2C temperature/humidity sensor (full OO)
// CHT832xSensor envSensor(0x44); // default address; todo: externalize Dec3'25

// SCT pump state tracking
static bool sctFirstRun = true;
static bool sctLastPumpState = false;
// Defaults align with ConfigModel::Sct013Config defaults and will be overwritten from gConfig in setup().
SctSensor sctSensor(32, 15.0f);

// DS18b20
// Data wire is plugged into port 15 on the ESP32
#define ONE_WIRE_BUS 23 // todo: externalize Nov20-24

// // Setup a oneWire instance to communicate with any OneWire devices
// OneWire oneWire(ONE_WIRE_BUS);

// // Pass our oneWire reference to Dallas Temperature.
// DallasTemperature sensors(&oneWire);

OneWire oneWire(ONE_WIRE_BUS); // todo:externalize I/O port nov'24
// Create a TemperatureSensor instance
TemperatureSensor temptSensor(&oneWire); // Dallas

// uint8_t w1[3][8] = {
//     {0x28, 0xa0, 0x7b, 0x49, 0xf6, 0xde, 0x3c, 0xe9},
//     {0x28, 0x08, 0xd3, 0x49, 0xf6, 0x3c, 0x3c, 0xfd},
//     {0x28, 0xc5, 0xe1, 0x49, 0xf6, 0x50, 0x3c, 0x38}};

// #define MAX_READINGS 4

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// New Zabbix agent server on port 10050
AsyncWebServer zabbixServer(10050);

// IPAddress localIP;
// IPAddress localIP(192, 168, 1, 200); // hardcoded

// Set your Gateway IP address
// IPAddress localGateway;
// IPAddress localGateway(192, 168, 1, 1); //hardcoded
// IPAddress subnet(255, 255, 0, 0);



// Set LED GPIO
const int ledPin = 2;
// Stores LED state

String ledState;

// Forward declarations for setup refactor
void loadBootstrapConfig();
void setupStationMode();
void setupAccessPointMode();
static void resetSensorRuntimeState();
static const char *wifiStatusToString(wl_status_t status);
static int logVisibleWifiCandidates(const char *targetSsid);

void onOTAEnd(bool success)
{
  // Log when OTA has finished
  if (success)
  {
    Serial.println("s:OTA update finished successfully! Restarting...");
    ESP.restart();
  }
  else
  {
    Serial.println("s:There was an error during OTA update!");
  }
  // <Add your own code here>
}
void onOTAStart()
{
  // Log when OTA has started
  Serial.println("s:OTA update started!");
}

/* Append a semi-unique id to the name template */
// char *MakeMine(const char *NameTemplate)
// {
//   // uint16_t uChipId = GetDeviceId();
//   // String Result = String(NameTemplate) + String(uChipId, HEX);
//   String Result = String(NameTemplate) + String(locationName);

//   char *tab2 = new char[Result.length() + 1];
//   strcpy(tab2, Result.c_str());

//   return tab2;
// }

// Function to handle Zabbix agent.ping
void handleZabbixPing(AsyncWebServerRequest *request)
{
  request->send(200, "text/plain", "1");
}

// Function to handle Zabbix agent.version
void handleZabbixVersion(AsyncWebServerRequest *request)
{
  request->send(200, "text/plain", version);
}

// Function to handle system.uptime
void handleSystemUptime(AsyncWebServerRequest *request)
{
  unsigned long uptimeMillis = millis();
  unsigned long uptimeSeconds = uptimeMillis / 1000;
  request->send(200, "text/plain", String(uptimeSeconds));
}

// // Function to handle vm.memory.size[available]
// void handleAvailableMemory(AsyncWebServerRequest *request)
// {
//   // Placeholder value for available memory
//   // Implement actual memory reading logic if possible
//   int availableMemory = 1024; // example value in bytes
//   request->send(200, "text/plain", String(availableMemory));
// }

// Function to handle vm.memory.size[available]
void handleAvailableMemory(AsyncWebServerRequest *request)
{
  // Get available memory
  int availableMemory = ESP.getFreeHeap(); // Get free heap memory in bytes
  request->send(200, "text/plain", String(availableMemory));
}

// Function to handle agent.hostname
void handleHostName(AsyncWebServerRequest *request)
{
  request->send(200, "text/plain", gIdentity.name());
}

// Function to handle zabbix.agent.availability
void handleAvailability(AsyncWebServerRequest *request)
{
  request->send(200, "text/plain", "1");
}

void handleWiFiBSSID(AsyncWebServerRequest *request)
{
  request->send(200, "text/plain", WiFi.BSSIDstr());
}

void handleWiFiSignalStrength(AsyncWebServerRequest *request)
{
  String rssiString = String(WiFi.RSSI());
  const char *rssiCharArray = rssiString.c_str();
  request->send(200, "text/plain", rssiCharArray);
}

// Function to initialize the Zabbix agent server
void initZabbixServer()
{
  zabbixServer.on("/", HTTP_GET, handleZabbixPing);
  zabbixServer.on("/agent.version", HTTP_GET, handleZabbixVersion);
  zabbixServer.on("/system.uptime", HTTP_GET, handleSystemUptime);
  zabbixServer.on("/vm.memory.size[available]", HTTP_GET, handleAvailableMemory);
  zabbixServer.on("/hostname", HTTP_GET, handleHostName);
  zabbixServer.on("/bssid", HTTP_GET, handleWiFiBSSID);
  zabbixServer.on("/signal", HTTP_GET, handleWiFiSignalStrength);
  zabbixServer.on("/availability", HTTP_GET, handleAvailability);
  zabbixServer.begin();
  logger.log("Zabbix agent server started on port 10050\n");
}

String printAddressAsString(DeviceAddress deviceAddress)
{
  String addressString = "";
  for (uint8_t i = 0; i < 8; i++)
  {
    addressString += "0x";
    if (deviceAddress[i] < 0x10)
      addressString += "0";
    addressString += String(deviceAddress[i], HEX);
    if (i < 7)
      addressString += ", ";
  }
  addressString += "\n";
  return addressString;
}

// Initialize SPIFFS
void initSPIFFS()
{
  if (!SPIFFS.begin(true))
  {
    Serial.println("s:An error has occurred while mounting SPIFFS");
  }
  Serial.println("s:SPIFFS mounted successfully");

  // loadLegacyPersistedValues();
}

bool initWiFi()
{
  if (ssid == "")
  {
    Serial.println("s:Undefined SSID.");
    return false;
  }

  Serial.println("s:Setting WiFi to WIFI_STA...");

  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.disconnect(false, true);
  delay(250);

  // Set custom hostname
  if (!WiFi.setHostname(gIdentity.name()))
  {
    Serial.println("s:Error setting hostname");
  }
  else
  {
    Serial.print("s:Setting DNS hostname to: ");
    Serial.println(gIdentity.name());
  }

  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
  WiFi.scanDelete();

  const int matches = logVisibleWifiCandidates(ssid.c_str());
  if (matches == 0)
  {
    Serial.println("s:Target SSID not visible in scan.");
  }

  // Add Wi-Fi networks to WiFiMulti
  if (!wifiMultiConfigured)
  {
    wifiMulti.addAP(ssid.c_str(), pass.c_str());
    wifiMultiConfigured = true;
  }

  Serial.println("s:Connecting to Wi-Fi; looking for the strongest mesh node...");

  // Start the connection attempt
  unsigned long startAttemptTime = millis();
  unsigned long lastRunAt = 0;
  unsigned long lastDiagAt = 0;
  wl_status_t status = WiFi.status();

  while (status != WL_CONNECTED)
  {
    unsigned long currentMillis = millis();
    if (currentMillis - startAttemptTime >= WIFI_CONNECT_INTERVAL)
    {
      Serial.println("s:Failed to connect after interval timeout.");
      return false;
    }

    if (lastRunAt == 0 || currentMillis - lastRunAt >= WIFI_MULTI_RUN_INTERVAL_MS)
    {
      status = static_cast<wl_status_t>(wifiMulti.run(WIFI_MULTI_CONNECT_TIMEOUT_MS));
      lastRunAt = millis();
    }
    else
    {
      status = WiFi.status();
    }

    if (status == WL_CONNECTED)
    {
      break;
    }

    if (lastDiagAt == 0 || currentMillis - lastDiagAt >= WIFI_DIAG_INTERVAL_MS)
    {
      Serial.print("s:WiFi status=");
      Serial.print(wifiStatusToString(status));
      Serial.print(" (");
      Serial.print(status);
      Serial.println(")");
      Serial.print("Last SSID attempted: ");
      Serial.println(WiFi.SSID());
      Serial.print("Last BSSID attempted: ");
      Serial.println(WiFi.BSSIDstr());
      Serial.print("Signal strength (RSSI): ");
      Serial.println(WiFi.RSSI());
      lastDiagAt = currentMillis;
    }

    delay(250);
  }

  // Successful connection
  gRuntime.isFallbackApMode = false;
  staDisconnectStartTime = 0;

  Serial.println("\ns:WiFi connected");
  Serial.print("s:IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("s:Connected to BSSID: ");
  Serial.println(WiFi.BSSIDstr());

  return true;
}

static const char *wifiStatusToString(wl_status_t status)
{
  switch (status)
  {
  case WL_IDLE_STATUS:
    return "idle";
  case WL_NO_SSID_AVAIL:
    return "no_ssid";
  case WL_SCAN_COMPLETED:
    return "scan_completed";
  case WL_CONNECTED:
    return "connected";
  case WL_CONNECT_FAILED:
    return "connect_failed";
  case WL_CONNECTION_LOST:
    return "connection_lost";
  case WL_DISCONNECTED:
    return "disconnected";
  default:
    return "unknown";
  }
}

static int logVisibleWifiCandidates(const char *targetSsid)
{
  const int networkCount = WiFi.scanNetworks();
  int matchingCount = 0;

  Serial.println("s:Scan complete");
  for (int i = 0; i < networkCount; ++i)
  {
    Serial.print("SSID: ");
    Serial.print(WiFi.SSID(i));
    Serial.print(", RSSI: ");
    Serial.println(WiFi.RSSI(i));

    if (WiFi.SSID(i) == targetSsid)
    {
      ++matchingCount;
    }
  }

  Serial.print("s:Visible target BSSIDs: ");
  Serial.println(matchingCount);
  return matchingCount;
}

void AdvertiseServices()
{
  Serial.println("s:AdvertiseServices on mDNS...");
  const char *myName = gIdentity.name();

  if (MDNS.begin(myName))
  {
    Serial.println(F("s:mDNS responder started"));
    Serial.print(F("s:I am: "));
    Serial.println(myName);

    MDNS.addService(SERVICE_NAME, SERVICE_PROTOCOL, SERVICE_PORT);
  }
  else
  {
    while (1)
    {
      Serial.println(F("s:Error setting up MDNS responder"));
      delay(1000);
    }
  }
}

bool initDNS()
{
  // if (!MDNS.begin(MDNS_DEVICE_NAME))
  // {
  //   Serial.println("Error starting mDNS");
  //   return false;
  // }

  AdvertiseServices();
  return true;
}

// Replaces placeholder with DHT values
// String processor(const String& var){
//   //Serial.println(var);
//   if(var == "TEMPERATURE"){
//     return readDHTTemperature();
//   }
//   else if(var == "HUMIDITY"){
//     return readDHTHumidity();
//   }
//   return String();
// }

// variable to hold device addresses
DeviceAddress Thermometer;

int deviceCount = 0;

String printDS18b20(void)
{
  String output = "";

  // sensors.begin();
  temptSensor.sensors.begin();

  output += "Locating devices...\n";
  output += "Found ";
  deviceCount = temptSensor.sensors.getDeviceCount();
  output += String(deviceCount, DEC);
  output += " devices.\n\n";
  output += "Printing addresses...\n";

  for (int i = 0; i < deviceCount; i++)
  {
    output += "Sensor ";
    output += String(i + 1);
    output += " : ";
    temptSensor.sensors.getAddress(Thermometer, i);
    // Assuming printAddress() returns a formatted string
    output += printAddressAsString(Thermometer);
  }

  return output;
}

void printAddress(DeviceAddress deviceAddress)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    Serial.print("0x");
    if (deviceAddress[i] < 0x10)
      Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
    if (i < 7)
      Serial.print(", ");
  }
  Serial.println("");
}

void setupDS18b20(void)
{
  // start serial port
  // Serial.begin(115200);

  // Start up the library
  // sensors.begin();
  temptSensor.sensors.begin();

  // locate devices on the bus
  Serial.println("s:Locating devices...");
  Serial.print("Found ");
  deviceCount = temptSensor.sensors.getDeviceCount();
  Serial.print(deviceCount, DEC);
  Serial.println(" devices.");
  Serial.println("");

  Serial.println("s:Printing addresses...");
  for (int i = 0; i < deviceCount; i++)
  {
    Serial.print("Sensor ");
    Serial.print(i + 1);
    Serial.print(" : ");
    temptSensor.sensors.getAddress(Thermometer, i);
    printAddress(Thermometer);
  }

  // Output
  // Printing addresses...
  // Sensor 1 : 0x28, 0xA0, 0x7B, 0x49, 0xF6, 0xDE, 0x3C, 0xE9
  // Sensor 2 : 0x28, 0x08, 0xD3, 0x49, 0xF6, 0x3C, 0x3C, 0xFD
  // Sensor 3 : 0x28, 0xC5, 0xE1, 0x49, 0xF6, 0x50, 0x3C, 0x38
}

// todo:extract to another file
// Define function to populate w1Address and w1Name from w1Sensors
void populateW1Addresses(uint8_t w1Address[6][8], String w1Name[6], const SensorGroupW1 &w1Sensors)
{
  for (size_t i = 0; i < w1Sensors.sensors.size(); ++i)
  {
    // Populate w1Name
    w1Name[i] = String(w1Sensors.sensors[i].name.c_str()); // Convert std::string to Arduino String

    // Populate w1Address
    for (size_t j = 0; j < w1Sensors.sensors[i].HEX_ARRAY.size(); ++j)
    {
      w1Address[i][j] = w1Sensors.sensors[i].HEX_ARRAY[j];
    }
  }
}

static bool saveUploadedBootstrapConfigJson(const String &jsonBody, String &errOut, const char *&outWrittenFilename)
{
  // Default it early so caller always gets something predictable.
  outWrittenFilename = FNAME_BOOTSTRAP;

  std::string jsonBodyStdString = std::string(jsonBody.c_str());

  SpiffsFileStore fs;
  if (!ConfigStorage::saveBootstrapConfigToFile(FNAME_BOOTSTRAP, jsonBodyStdString, fs, coreLog))
  {
    errOut = "Failed to write /bootstrap.json";
    return false;
  }

  return true;
}

// Entry point
void setup()
{
  // Serial port for debugging purposes
  delay(500);
  Serial.begin(115200);
  delay(500);

  // Enable verbose logging for the WiFi component
  esp_log_level_set("wifi", ESP_LOG_VERBOSE);

  // Experimenting w/ different sizes. want larger to see json not truncated. but something is causing esp to hang. or at least the logger. not sure which yet. Feb18'26
  logger.begin(locationName.c_str(), SRL_TELNET_PASSWORD, SRL_TELNET_PORT, 32, 192);
  // logger.begin(locationName.c_str(), SRL_TELNET_PASSWORD, SRL_TELNET_PORT, 64, 512);

  Serial.println("s:initSpiffs...");
  logger.log("setup: initSpiffs...\n");
  initSPIFFS();
  // SPIFFS, legacy params, /config.json, W1 sensors, etc.
  Serial.println("s:loadBootstrapConfig...");
  logger.log("setup: loadBootstrapConfig...\n");
  loadBootstrapConfig();

  Serial.println("s:initWiFi...");
  logger.log("setup: initWiFi...\n");
  // Decide which path: Station vs AP
  if (initWiFi()) // Station Mode
  {
    // Start Telnet logger
    logger.startTelnet();
    logger.log("Wifi and Telnet logger shoudl now be functioning.\n");
    logger.log("setup: Wifi and Telnet logger shoudl now be functioning...\n");
    setupStationMode();
  }
  else
  {
    // IMPORTANT:
    // Do NOT start logger.begin() here. It starts a WiFiServer (RemoteDebug) which
    // requires lwIP/tcpip to be ready. In AP path, that isn't true yet because
    // softAP hasn't been started.
    //
    // Start logger.begin() inside setupAccessPointMode() AFTER WiFi.softAP(...).

    // logger.begin(locationName.c_str(), SRL_TELNET_PASSWORD, SRL_TELNET_PORT, 64, 192);
    // logger.log("boot\n");
    Serial.println("s:wifi failed. setupAccessPointMode...");
    logger.log("setup: wifi failed. setupAccessPointMode...\n");
    setupAccessPointMode();
  }
}

void loadBootstrapConfig()
{
  std::string pathSpiffTxt = "/";

  SpiffsFileStore fs;

  ConfigBootstrapper cb(gConfig, FNAME_BOOTSTRAP, FNAME_CONFIG, pathSpiffTxt, fs, coreLog);
  cb.load();

  // 2) Apply bootstrap values into your legacy globals that initWiFi() expects
  // (You can delete these legacy globals later, but for now keep it explicit)
  ssid = gConfig.boot.wifi.ssid;
  pass = gConfig.boot.wifi.pass;
  locationName = gConfig.boot.identity.locationName.c_str();
  // otaUrl legacy string (if still used anywhere)
  otaUrl = gConfig.boot.remote.otaUrl.c_str();
  configUrl = gConfig.boot.remote.configBaseUrl;
  mainDelay = gConfig.timing.mainDelayMs;

  logger.log("gConfig.boot.wifi.ssid=");
  logger.log(gConfig.boot.wifi.ssid.c_str());
  logger.log("\n");
  logger.log("gConfig.boot.wifi.pass=");
  logger.log(gConfig.boot.wifi.pass.c_str());
  logger.log("\n");
  logger.log("gConfig.boot.identity.locationName=");
  logger.log(gConfig.boot.identity.locationName.c_str());
  logger.log("\n");
  logger.log("gConfig.boot.remote.configBaseUrl=");
  logger.log(gConfig.boot.remote.configBaseUrl.c_str());
  logger.log("\n");

  gIdentity.init(MDNS_DEVICE_NAME, gConfig.boot.identity.locationName.c_str());

  Serial.println("s:serial output. assuming logger not yet started.");
  logger.log("gConfig: output. assuming logger not yet started.\n");
}

void setupStationMode()
{
  gRuntime.isFallbackApMode = false;
  staDisconnectStartTime = 0;
  resetSensorRuntimeState();
  gRuntime.sensors.subsystemStarted = true;

  // setup: path1 (Station Mode)
  // todo: configUrl and locationName should come from gConfig boot values
  // tryFetchAndApplyRemoteConfig(logger, configUrl, locationName, FNAME_CONFIGREMOTE);

  SpiffsFileStore fs;
  ConfigFetch fetch(fs, coreLog);
  ConfigMerge cm(coreLog, fs, fetch);

  logger.handle();
  logger.flush(16);

  const std::string mergedRemoteStr = FNAME_CONFIGREMOTE;
  const std::string bootstrapStr = FNAME_BOOTSTRAP;
  const std::string configFileStr = FNAME_CONFIG;

  std::string err;
  std::string jsonString = cm.buildAppConfigJson(
      configUrl,
      locationName,
      mergedRemoteStr.c_str(),
      bootstrapStr.c_str(),
      configFileStr.c_str(),
      err);

  logger.handle();
  logger.flush(16);

  if (!configFromJson(jsonString, gConfig))
  {
    if (!err.empty())
    {
      logger.log("Error: configFromJsonFile failed - ");
      logger.log(err.c_str());
      logger.log("\n");
      if (!jsonString.empty())
      {
        logger.log("The JSON string that failed to parse was:\n");
        logger.log(jsonString.c_str());
        logger.log("\n");
      }
    }
    else
    {
      logger.log("Error: configFromJsonFile failed to pull, compare and apply config JSON to gConfig\n");
    }
  }
  else
  {
    logger.log("configFromJsonFile succeeded to pull, compare and apply config JSON to gConfig\n");
  }

  logger.handle();
  logger.flush(16);

  logger.log("initDNS...\n");
  initDNS();

  // Initialize Zabbix agent server
  initZabbixServer();

  // @pattern
  if (gConfig.sensors.dht.enabled)
  {
    logger.log("DHT: enabled via gConfig, starting sensor task\n");
    gRuntime.sensors.dhtReady = initSensorTask(gConfig.sensors.dht.pin);
  }

  if (gConfig.sensors.acs.enabled)
  {
    setupACS712();
    gRuntime.sensors.acsReady = true;
  }

  if (gConfig.sensors.w1.enabled)
  {
    setupDS18b20();
    gRuntime.sensors.w1Ready = true;
  }

  // I2C pins for CHT832x
  Wire.begin(gConfig.sensors.cht.pinSda, gConfig.sensors.cht.pinScl);
  envSensor.setAddress(static_cast<uint8_t>(gConfig.sensors.cht.addr));
  if (gConfig.sensors.cht.enabled)
  {
    envSensor.begin();
    gRuntime.sensors.chtReady = true;
  }

  if (gConfig.sensors.sct.enabled)
  {
    sctSensor = SctSensor(gConfig.sensors.sct.pin, gConfig.sensors.sct.ratedAmps);
    sctSensor.begin();
    gRuntime.sensors.sctReady = true;
  }

  registerWebRoutesStation(server, gConfig);

  // uses path like server.on("/update")
  // AsyncElegantOTA.begin(&server);
  ElegantOTA.begin(&server);
  ElegantOTA.onStart(onOTAStart);
  // ElegantOTA.onProgress(onOTAProgress);
  ElegantOTA.onEnd(onOTAEnd);

  configTime(0, 0, "pool.ntp.org"); // Set timezone offset and daylight offset to 0 for simplicity
  time_t now;
  while (!(time(&now)))
  { // Wait for time to be set
    logger.log("Waiting for time...\n");
    delay(1000);
  }

  server.begin();

  // Set MQTT server using new AppConfig model (JSON-first, no legacy bridge)
  logger.log("Setting MQTT server and port from gConfig.mqtt...\n");
  logger.log(gConfig.mqtt.server.c_str());
  logger.log(" : ");
  logger.log(String(gConfig.mqtt.port).c_str());
  logger.log("\n");

  if (gConfig.mqtt.server.length() > 0 && gConfig.mqtt.port > 0)
  {
    mqClient.setServer(gConfig.mqtt.server.c_str(), gConfig.mqtt.port);
  }
  else
  {
    logger.log("Error: MQTT config missing server or port in gConfig.mqtt; MQTT will be disabled\n");
  }

  logger.log("\nEntry setup loop complete.");

  logger.handle();
  logger.flush(16);
}

void setupAccessPointMode()
{
  // SETUP : Path2
  // This path is meant to run only upon initial one-time setup

  gRuntime.isFallbackApMode = true;
  apStartTime = millis(); // Record the start time in AP mode
  staDisconnectStartTime = 0;
  resetSensorRuntimeState();

  Serial.println("s:Setting AP (Access Point)");
  WiFi.mode(WIFI_AP);

  // Build base SSID (without prefix)
  std::string base;

  if (locationName.length() > 0)
  {
    // Use configured location name
    base = locationName;
  }
  else
  {
    // Build fallback with unique suffix from MAC
    uint64_t mac = ESP.getEfuseMac();
    uint32_t suffix = (uint32_t)(mac & 0xFFFFFF); // last 3 bytes

    char buf[32];
    snprintf(buf, sizeof(buf), "%06X", suffix);
    base = buf;
  }

  // Final SSID: ALWAYS prefixed with "sesp-"
  // Guarantee SSID ≤ 31 chars (Wi-Fi limit)
  char apSsid[32];
  snprintf(apSsid, sizeof(apSsid), "%s%s", AP_SSID_PREFIX, base.c_str());

  WiFi.softAP(apSsid, AP_PASSWORD);

  Serial.print("s:AP SSID: ");
  Serial.println(apSsid);

  IPAddress IP = WiFi.softAPIP();
  Serial.print("s:AP IP address: ");
  Serial.println(IP);

  logger.log("boot\n");

  if (!SPIFFS.begin(true))
  {
    Serial.println("s:SPIFFS is out of scope per bwilly!");
  }
  // Web Server Root URL
  registerWebRoutesAp(server, gConfig);

  logger.log("Starting web server in AP-mode...\n");
  server.begin();
}

void serviceMqttConnection()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    return;
  }

  if (mqClient.connected())
  {
    mqClient.loop();
    return;
  }

  const unsigned long now = millis();
  if (now < nextMqttReconnectAtMs)
  {
    return;
  }

  const char *clientId = gIdentity.name();

  logger.log("Attempting MQTT connection...");
  if (mqClient.connect(clientId))
  {
    logger.log("connected\n");
    mqttReconnectDelayMs = MQTT_RECONNECT_INITIAL_DELAY_MS;
    nextMqttReconnectAtMs = 0;
    mqClient.loop();
    return;
  }

  logger.log("failed, rc=");
  logger.log(mqClient.state());
  logger.log(" retry in ");
  logger.log(String(mqttReconnectDelayMs / 1000).c_str());
  logger.log(" seconds\n");

  nextMqttReconnectAtMs = now + mqttReconnectDelayMs;
  mqttReconnectDelayMs = min(mqttReconnectDelayMs * 2, MQTT_RECONNECT_MAX_DELAY_MS);
}

static void resetSensorRuntimeState()
{
  gRuntime.sensors = SensorRuntimeState{};
}

void publishSimpleMessage()
{
  // Define the message and topic
  const char *topic = "test/topic";
  const char *message = "Hello World!";

  // Publish the message
  if (mqClient.connected())
  {
    if (mqClient.publish(topic, message))
    {
      logger.log("MQTT: Message published successfully: ");
      logger.log(message);
      logger.log("\n");
    }
    else
    {
      logger.log("MQTT: Message publishing failed.\n");
    }
  }
  else
  {
    logger.log("MQTT: Not connected to MQTT broker.");
    logger.log(mqClient.state());
    logger.log(" --MQTT: service pending\n");
  }
}

// Helper function to calculate percentage change
float calculatePercentageChange(float oldValue, float newValue)
{
  if (!isfinite(newValue))
  {
    return NAN;
  }

  if (!isfinite(oldValue) || oldValue == 0.0f)
  {
    return 100.0f; // Return 100% if old value is NaN or 0 to force an update
  }
  return abs((newValue - oldValue) / oldValue) * 100.0f;
}

void maybePublishEnvToMqtt(
    const char *sourceName,
    const SensorMetadata &metadata,
    float currentTemperature,
    float currentHumidity,
    float &previousTemperatureRef,
    float &previousHumidityRef,
    unsigned long &lastPublishTimeTempRef,
    unsigned long &lastPublishTimeHumRef)
{
  unsigned long now = millis();

  const bool validTemperature = isfinite(currentTemperature);
  const bool validHumidity = isfinite(currentHumidity);

  if (!validTemperature)
  {
    logger.log(sourceName);
    logger.log(": current temperature invalid; skipping publish.\n");
  }

  if (!validHumidity)
  {
    logger.log(sourceName);
    logger.log(": current humidity invalid; skipping publish.\n");
  }

  float tempChange = calculatePercentageChange(previousTemperatureRef, currentTemperature);
  float humidityChange = calculatePercentageChange(previousHumidityRef, currentHumidity);

  unsigned long timeSinceLastPublishTemp = now - lastPublishTimeTempRef;
  unsigned long timeSinceLastPublishHum = now - lastPublishTimeHumRef;

  bool shouldPublishTemp = false;
  bool shouldPublishHum = false;

  if (validTemperature && tempChange >= gConfig.sensors.publishThreshold.temperatureChangePct)
  {
    shouldPublishTemp = true;
  }
  if (validHumidity && humidityChange >= gConfig.sensors.publishThreshold.humidityChangePct)
  {
    shouldPublishHum = true;
  }

  if (validTemperature)
  {
    logger.log(sourceName);
    logger.log(" Temperature changed from ");
    logger.log(previousTemperatureRef);
    logger.log(" to ");
    logger.log(currentTemperature);
    logger.log(". Percentage change: ");
    logger.log(tempChange);
    logger.log("\n");
  }
  else
  {
    logger.log(sourceName);
    logger.log(" Temperature change unavailable because the current reading is invalid.\n");
  }

  if (validHumidity)
  {
    logger.log(sourceName);
    logger.log(" Humidity changed from ");
    logger.log(previousHumidityRef);
    logger.log(" to ");
    logger.log(currentHumidity);
    logger.log(". Percentage change: ");
    logger.log(humidityChange);
    logger.log("\n");
  }
  else
  {
    logger.log(sourceName);
    logger.log(" Humidity change unavailable because the current reading is invalid.\n");
  }

  if (validTemperature && timeSinceLastPublishTemp >= gConfig.timing.publishIntervalMs)
  {
    shouldPublishTemp = true;
    logger.log(sourceName);
    logger.log(": time interval since last temp publish exceeded; publishing.\n");
  }
  else
  {
    logger.log(sourceName);
    logger.log(": time to next temp publish: ");
    logger.log((gConfig.timing.publishIntervalMs - timeSinceLastPublishTemp) / 1000);
    logger.log(" seconds.\n");
  }

  if (validHumidity && timeSinceLastPublishHum >= gConfig.timing.publishIntervalMs)
  {
    shouldPublishHum = true;
    logger.log(sourceName);
    logger.log(": time interval since last humidity publish exceeded; publishing.\n");
  }
  else
  {
    logger.log(sourceName);
    logger.log(": time to next humidity publish: ");
    logger.log((gConfig.timing.publishIntervalMs - timeSinceLastPublishHum) / 1000);
    logger.log(" seconds.\n");
  }

  if (shouldPublishTemp)
  {
    logger.log(sourceName);
    logger.log(": publishTemperature...\n");
    MessagePublisher::publishTemperature(mqClient, currentTemperature, metadata);
    previousTemperatureRef = currentTemperature;
    lastPublishTimeTempRef = now;
  }
  else
  {
    logger.log(sourceName);
    logger.log(": skipping publish temperature.\n");
  }

  if (shouldPublishHum)
  {
    logger.log(sourceName);
    logger.log(": publishHumidity...\n");
    MessagePublisher::publishHumidity(mqClient, currentHumidity, metadata);
    previousHumidityRef = currentHumidity;
    lastPublishTimeHumRef = now;
  }
  else
  {
    logger.log(sourceName);
    logger.log(": skipping publish humidity.\n");
  }

  logger.log("\n");
}

unsigned long g_lastMainRunMs = 0;

void loop()
{
  unsigned long currentMillis = millis();
  const unsigned long mainIntervalMs =
      gConfig.timing.mainDelayMs < 500 ? 3000 : static_cast<unsigned long>(gConfig.timing.mainDelayMs);

  if(currentMillis - g_lastMainRunMs < mainIntervalMs)
  {
    // Not time to run main loop logic yet
    return;
  }
  g_lastMainRunMs = currentMillis;


  logger.handle();
  logger.flush(16);

  // Apply pending bootstrap config if any, delete main config to force fresh start on next boot, and reboot to apply new config
  if (g_bootstrapPending)
  {
    g_bootstrapPending = false;

    String err;
    const char *writtenFile = nullptr;
    bool ok = saveUploadedBootstrapConfigJson(g_bootstrapBody, err, writtenFile);

    // also delete the shared/general config file since bootstrap was just updated. eg, start fresh next boot.
    bool dOk = deleteJsonFile(SPIFFS, FNAME_CONFIG);
    if (!dOk)
    {
      logger.log("Failed to delete existing config after bootstrap update.\n");
    }
    else
    {
      logger.log("Deleted existing config to force fresh start on next boot after bootstrap update.\n");
    }

    if (ok)
    {
      char buf[96];
      snprintf(buf, sizeof(buf),
               "Bootstrap: saved %s; rebooting\n",
               writtenFile);

      logger.log(buf);
      logger.handle();
      logger.flush();
      delay(800);
      ESP.restart(); // note: logs above do not display before reboot. Mar12'26
    }
    else
    {
      logger.log("Bootstrap: rejected: " + err + "\n");
    }
  }

  // Defered OTA execution from main loop (not async_tcp task)
  if (g_otaRequested)
  {
    g_otaRequested = false; // clear first to avoid reentry if OTA fails

    logger.log("OTA: executing deferred OTA from loop using URL = " + g_otaUrl + "\n");

    bool ok = performHttpOta(g_otaUrl);
    if (!ok)
    {
      logger.log("OTA: performHttpOta() failed; no reboot will occur\n");
    }

    // If performHttpOta succeeds, it calls ESP.restart() and we never reach here.
  }

  // Reboot if the device has been left in fallback AP mode too long.
  if (gRuntime.isFallbackApMode)
  {
    if (currentMillis - apStartTime >= AP_REBOOT_TIMEOUT)
    {
      logger.log("Rebooting due to extended time in AP mode...");
      Serial.println("s:Rebooting due to extended time in AP mode...");
      logger.handle();
      logger.flush(16);
      delay(200);
      ESP.restart();
    }
  }

  // does this need to be here? I didn't use it, here, on the new master project. maybe it was only needed for the non-async web server? bwilly Feb26'23
  // server.handleClient();

  if (!gRuntime.isFallbackApMode)
  {
    if (WiFi.status() == WL_CONNECTED)
    {
      staDisconnectStartTime = 0;
    }
    else
    {
      if (staDisconnectStartTime == 0)
      {
        staDisconnectStartTime = currentMillis;
        Serial.println("s:WiFi disconnected; starting station reconnect watchdog.");
        logger.log("WiFi: disconnected; starting station reconnect watchdog\n");
      }
      else if (currentMillis - staDisconnectStartTime >= reconnect_delay)
      {
        Serial.print("s:millis: ");
        logger.log("WiFi:millis: ");
        Serial.println(millis());
        logger.log(String(millis()).c_str());
        logger.log("\n");
        Serial.print("s:staDisconnectStartTime: ");
        logger.log("WiFi:staDisconnectStartTime: ");
        Serial.println(staDisconnectStartTime);
        logger.log(String(staDisconnectStartTime).c_str());
        logger.log("\n");
        Serial.print("s:reconnect_delay: ");
        logger.log("WiFi:reconnect_delay: ");
        Serial.println(reconnect_delay);
        logger.log(String(reconnect_delay).c_str());
        logger.log("\n");
        Serial.println("s:WiFi remained disconnected in station mode; restarting ESP.");
        logger.log("WiFi: remained disconnected in station mode; restarting ESP\n");
        logger.handle();
        logger.flush(16);
        delay(200);
        ESP.restart();
      }
    }
  }

  if (gConfig.mqtt.enabled)
  {
    // publishSimpleMessage(); // manual test

    serviceMqttConnection();

    // @anti-pattern as compared to dhtEnabledValue? Maybe this is the bettr way and the dhtEnabledValue was mean for checkbox population? Mar4'25
    if (gConfig.sensors.dht.enabled && gRuntime.sensors.dhtReady)
    {
      float currentTemperature = readDHTTemperature();
      float currentHumidity = readDHTHumidity();

      maybePublishEnvToMqtt(
          "DHT",
          gConfig.sensors.dht,
          currentTemperature,
          currentHumidity,
          previousTemperature,
          previousHumidity,
          lastPublishTime_tempt,
          lastPublishTime_humidity);
    }

    // Dec3'25
    if (gConfig.sensors.cht.enabled && gRuntime.sensors.chtReady)
    {
      float chtTemp = NAN;
      float chtHum = NAN;

      if (envSensor.read(chtTemp, chtHum))
      {
        maybePublishEnvToMqtt(
            "CHT832x",
            gConfig.sensors.cht,
            chtTemp,
            chtHum,
            previousCHTTemperature,
            previousCHTHumidity,
            lastPublishTime_temptCHT,
            lastPublishTime_humidityCHT);
      }
      else
      {
        logger.log("CHT832xSensor: read failed; skipping publish this cycle\n");
      }
    }

    if (gConfig.sensors.sct.enabled && gRuntime.sensors.sctReady)
    {
      float amps = sctSensor.readCurrentACRms();
      logger.logf("iot.sct.current %.3fA pin=%d rated=%.0f\n",
                  amps, gConfig.sensors.sct.pin, gConfig.sensors.sct.ratedAmps);
      sctSensor.serialOutAdcDebug(); // for debug only
    }
  }

  if (gConfig.sensors.sct.enabled && gRuntime.sensors.sctReady)
  {
    float amps = fabsf(sctSensor.readCurrentACRms());

    bool pumpState = (amps > gConfig.sensors.sct.onThresholdAmps);

    if (sctFirstRun || pumpState != sctLastPumpState || pumpState)
    {
      MessagePublisher::publishPumpState(
          mqClient,
          pumpState,
          amps,
          gConfig.sensors.sct);

      sctLastPumpState = pumpState;
      sctFirstRun = false;
    }
  }

  if (gConfig.sensors.w1.enabled && gRuntime.sensors.w1Ready)
  {
    temptSensor.requestTemperatures();
    TemperatureReading *readings = temptSensor.getTemperatureReadings(gConfig.sensors.w1); // todo:performance: move declaration outside of the esp loop
    const size_t w1DeviceCount =
        gConfig.sensors.w1.devices.size() < static_cast<size_t>(MAX_READINGS)
            ? gConfig.sensors.w1.devices.size()
            : static_cast<size_t>(MAX_READINGS);

    for (size_t i = 0; i < w1DeviceCount; i++)
    {
      // Check if the reading is valid, e.g., by checking if the name is not empty
      if (!readings[i].name.isEmpty())
      {
        MessagePublisher::publishTemperature(
            mqClient,
            readings[i].value,
            gConfig.sensors.w1.devices[i]);
      }
    }
  }

  if (gConfig.sensors.acs.enabled && gRuntime.sensors.acsReady)
  {
    float amps = fabs(readACS712Current());
    logger.log(amps);
    logger.log(" amps\n");

    bool pumpState = (amps > gConfig.sensors.acs.onThresholdAmps);

    if (pumpState)
    {
      logger.log("Pump ON\n");
    }
    else
    {
      logger.log("Pump OFF\n");
    }

    if (firstRun || pumpState != lastPumpState || pumpState)
    {
      // Only publish if the pump state changed ...i don't like hiding the visibility of being OFF, but too much data
      MessagePublisher::publishPumpState(mqClient, pumpState, amps, gConfig.sensors.acs);
      lastPumpState = pumpState; // Update the last known state
      firstRun = false;
    }
  }
}
