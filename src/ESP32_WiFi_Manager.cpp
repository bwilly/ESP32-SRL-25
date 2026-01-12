
/*********
// Brian Willy
// Climate Sensor
// Prometheus Exporter
// Copywrite 2022-2023
// Copyright 2022-2023, 2024, 2025

// Unstable Wifi post network/wifi failure
      - plan for fix here is to switch base wifi platform code away from Rui Santos to

// Only supports DHT22 Sensor [obsolete]]

Immediate need for DSB18b20 Sensor support and multi names for multi sensor. [done]
I guess that will be accomplished via the hard coded promexporter names.
Must be configurable.
With ability to map DSB ID to a name, such as raw water in, post air cooler, post heat exchanger, etc

// Configurable:
// Location as mDNS Name Suffix
// Wifi
// ESP Pin
*/

/** some params can be set via UI. Anything params loaded later as the json web call will supersede UI params [Nov28'25] */

/*********
 *
 * Thanks to
  Rui Santos
  Complete instructions at https://RandomNerdTutorials.com/esp32-wi-fi-manager-asyncwebserver/

  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*********/

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
#include "ConfigDump.h"
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
#include "ConfigFile.h"

#include "DeviceIdentity.h"

#include "Logger.h"

Logger logger;

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

#define AP_REBOOT_TIMEOUT 600000 // 10 minutes in milliseconds
unsigned long apStartTime = 0;   // Variable to track the start time in AP mode

volatile bool g_otaRequested = false;
String g_otaUrl;

// Remote config JSON merge docs (kept off stack)
static const size_t REMOTE_JSON_CAPACITY = 4096;
static StaticJsonDocument<REMOTE_JSON_CAPACITY> g_remoteMergedDoc;
static StaticJsonDocument<REMOTE_JSON_CAPACITY> g_remoteTmpDoc;

// BEFORE (something like this)
// const char* version = APP_VERSION "::" APP_COMMIT_HASH ":: TelnetBridge";

// AFTER
// String version = String(APP_VERSION) + "::" + APP_COMMIT_HASH + ":: TelnetBridge-removed";
String version = String(APP_VERSION) + "::" +
                 APP_COMMIT_HASH + "::" +
                 APP_BUILD_DATE + ":: TelnetBridge-backAgain";

// trying to identify cause of unreliable dht22 readings

// Serial.println("Application Version: " APP_VERSION);
// Serial.println("Commit Hash: " APP_COMMIT_HASH);

// MQTT Server details
// const char *mqtt_server = "192.168.68.120"; // todo: change to config param
// const int mqtt_port = 1883;                 // todo: change to config param

WiFiClient espClient;
PubSubClient mqClient(espClient);
WiFiMulti wifiMulti;

// DNS settings
IPAddress primaryDNS(10, 27, 1, 30); // Your Raspberry Pi's IP (DNS server) mar'25: why is this here? is it doing anything
IPAddress secondaryDNS(8, 8, 8, 8);  // Optional: Google DNS

const float THRESHOLD_TEMPERATURE_PERCENTAGE = 3.0;
const float THRESHOLD_HUMIDITY_PERCENTAGE = 4.0;
const unsigned long PUBLISH_INTERVAL = 5 * 60 * 1000; // Five minutes in milliseconds

float previousTemperature = NAN;
float previousHumidity = NAN;
unsigned long lastPublishTime_tempt = 0;
unsigned long lastPublishTime_humidity = 0;

// For CHT832x
float previousCHTTemperature = NAN;
float previousCHTHumidity = NAN;
unsigned long lastPublishTime_temptCHT = 0;
unsigned long lastPublishTime_humidityCHT = 0;

// Local pump config (Nov 25, 2025)
namespace
{
  constexpr float PUMP_ON_THRESHOLD_AMPS = 1.75f; // was hardcoded in loop
}

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

// SctSensor sctSensor(32, 15.0f);  // default 15A;
// choose the ADC1 pin you're using (example)
static const int PIN_SCT = 32;
// choose the rating (10, 15, or 20A version)
static const float SCT_RATED_AMPS = 15.0f;
static const float FEEDPUMP_ON_THRESHOLD_AMPS = 0.5f;

// SCT pump state tracking
static bool sctFirstRun = true;
static bool sctLastPumpState = false;
// your OO instance (just like envSensor)
SctSensor sctSensor(PIN_SCT, SCT_RATED_AMPS);

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

// Timer variables
// todo: can combine interval and delay. each is from copy/paste. one solves initial connect, the other is for lost connection retry
unsigned long previousMillis = 0;
const long interval = 40000; // interval to wait for Wi-Fi connection (milliseconds)
unsigned long previous_time = 0;
unsigned long reconnect_delay = 180000; // 3-min delay

static bool lastPumpState = false; // Assume OFF at startup
static bool firstRun = true;       // New flag to force first publish

// Set LED GPIO
const int ledPin = 2;
// Stores LED state

String ledState;

// Forward declarations for setup refactor
void setupSpiffsAndConfig();
void setupStationMode();
void setupAccessPointMode();

void onOTAEnd(bool success)
{
  // Log when OTA has finished
  if (success)
  {
    Serial.println("OTA update finished successfully! Restarting...");
    ESP.restart();
  }
  else
  {
    Serial.println("There was an error during OTA update!");
  }
  // <Add your own code here>
}
void onOTAStart()
{
  // Log when OTA has started
  Serial.println("OTA update started!");
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

static bool readFileToString(const char *path, String &out)
{
  File f = SPIFFS.open(path, FILE_READ);
  if (!f)
  {
    return false;
  }
  out = f.readString();
  f.close();
  return true;
}

static bool writeStringToFile(const char *path, const String &data)
{
  File f = SPIFFS.open(path, FILE_WRITE); // truncates or creates
  if (!f)
  {
    Serial.print(F("writeStringToFile: failed to open "));
    Serial.print(path);
    Serial.println(F(" for writing"));
    return false;
  }

  size_t written = f.print(data);
  f.close();

  if (written != data.length())
  {
    Serial.print(F("writeStringToFile: short write to "));
    Serial.print(path);
    Serial.print(F(" (expected "));
    Serial.print(data.length());
    Serial.print(F(" bytes, wrote "));
    Serial.print(written);
    Serial.println(F(")"));
    return false;
  }

  return true;
}

void tryFetchAndApplyRemoteConfig()
{
  const String baseUrl = configUrl;

  if (baseUrl.length() == 0)
  {
    logger.log("ConfigFetch: base configUrl is empty; cannot fetch remote config\n");
    return;
  }

  // Build GLOBAL URL: <base>/global.json
  String globalUrl = baseUrl;
  if (!globalUrl.endsWith("/"))
  {
    globalUrl += "/";
  }
  globalUrl += "global.json"; // <-- make sure this says "global.json", not "global."

  // Build INSTANCE URL: <base>/<locationName>.json
  String instanceUrl;
  if (locationName.length() > 0)
  {
    instanceUrl = baseUrl;
    if (!instanceUrl.endsWith("/"))
    {
      instanceUrl += "/";
    }
    instanceUrl += locationName + ".json";
  }
  else
  {
    logger.log("ConfigFetch: locationName is empty; will only attempt GLOBAL config\n");
  }

  // Load previous remote snapshot
  String previousRemoteJson;
  bool hasPreviousRemote = readFileToString(FNAME_CONFIGREMOTE, previousRemoteJson);
  if (hasPreviousRemote)
  {
    logger.log("ConfigFetch: found previous remote snapshot /config-remote.json\n");
  }
  else
  {
    logger.log("ConfigFetch: no previous /config-remote.json (first run or missing)\n");
  }

  // Clear and prepare merged remote doc
  g_remoteMergedDoc.clear();
  JsonObject mergedRoot = g_remoteMergedDoc.to<JsonObject>();

  bool anyRemoteApplied = false;
  String json;

  auto fetchApplyAndMerge = [&](const String &url, const char *label)
  {
    if (url.length() == 0)
    {
      return;
    }

    if (!downloadConfigJson(url, json))
    {
      logger.log(String("ConfigFetch: ") + label +
                 " config fetch FAILED or not found at " + url + "\n");
      return;
    }

    logger.log(String("ConfigFetch: downloaded ") + label + " config from " + url +
               " (" + String(json.length()) + " bytes)\n");

    // Apply to in-memory params
    if (!loadConfigFromJsonString(json))
    {
      logger.log(String("ConfigFetch: ") + label + " JSON parse/apply FAILED\n");
      return;
    }

    logger.log(String("ConfigFetch: ") + label + " JSON applied OK\n");

    // Merge into remote snapshot doc
    g_remoteTmpDoc.clear();
    DeserializationError err = deserializeJson(g_remoteTmpDoc, json);
    if (err)
    {
      logger.log(String("ConfigFetch: ") + label +
                 " JSON re-parse FAILED for snapshot merge\n");
      return;
    }

    JsonObject src = g_remoteTmpDoc.as<JsonObject>();
    for (JsonPair kv : src)
    {
      mergedRoot[kv.key()] = kv.value(); // instance overrides global on same key
    }

    anyRemoteApplied = true;
  };

  // GLOBAL then INSTANCE
  fetchApplyAndMerge(globalUrl, "GLOBAL");
  if (instanceUrl.length() > 0)
  {
    fetchApplyAndMerge(instanceUrl, "INSTANCE");
  }

  if (!anyRemoteApplied)
  {
    logger.log("ConfigFetch: no remote configs applied; leaving local config as-is; no reboot\n");
    return;
  }

  // Serialize new remote snapshot
  String newRemoteJson;
  serializeJson(g_remoteMergedDoc, newRemoteJson);

  // Compare snapshots *only on remote config*
  if (hasPreviousRemote && (newRemoteJson == previousRemoteJson))
  {
    logger.log("ConfigFetch: remote config unchanged vs /config-remote.json; no persist, no reboot\n");
    return;
  }

  logger.log("ConfigFetch: remote config changed; persisting and rebooting\n");

  // Persist remote snapshot
  if (writeStringToFile(FNAME_CONFIGREMOTE, newRemoteJson))
  {
    logger.log("ConfigFetch: wrote new remote snapshot to /config-remote.json\n");
  }
  else
  {
    logger.log("ConfigFetch: FAILED to write /config-remote.json\n");
  }

  // // Persist full effective config
  // if (saveLegacyEffectiveCacheToFile(EFFECTIVE_CACHE_PATH)) 
  // {
  //   logger.log("ConfigFetch: persisted merged effective config config.json; rebooting\n");
  // }
  // else
  // {
  //   logger.log("ConfigFetch: FAILED to persist merged config; rebooting anyway\n");
  // }


  String err;

  if (!saveConfigJson(newRemoteJson, err))
  {
    logger.log("applyRemoteConfig: failed to save remote config to general json config\n");

    if (err.length()) {
        logger.log("applyRemoteConfig: error=");
        logger.log(err);
        logger.log("\n");
    }
    else {
        logger.log("applyRemoteConfig: no error detail provided\n");
    }

    return;
  }

  logger.log("applyRemoteConfig: remote config saved successfully as general config\n");


  // Allow logger + UART/WiFi buffers to flush
  delay(500);

  // Yield once more for good measure
  yield();

  ESP.restart();
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
    Serial.println("An error has occurred while mounting SPIFFS");
  }
  Serial.println("SPIFFS mounted successfully");

  loadLegacyPersistedValues();
}

bool initWiFi()
{
  if (ssid == "")
  {
    Serial.println("Undefined SSID.");
    return false;
  }

  Serial.println("Setting WiFi to WIFI_STA...");

  WiFi.disconnect(true);
  // delay(180);
  // Set custom hostname
  if (!WiFi.setHostname(gIdentity.name()))
  {
    Serial.println("Error setting hostname");
  }
  else
  {
    Serial.print("Setting DNS hostname to: ");
    Serial.println(gIdentity.name());
  }
  WiFi.mode(WIFI_STA);

  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);

  // WiFi.begin(ssid.c_str(), pass.c_str());

  int n = WiFi.scanNetworks();
  Serial.println("Scan complete");
  for (int i = 0; i < n; ++i)
  {
    Serial.print("SSID: ");
    Serial.print(WiFi.SSID(i));
    Serial.print(", RSSI: ");
    Serial.println(WiFi.RSSI(i));
  }

  // Add Wi-Fi networks to WiFiMulti
  wifiMulti.addAP(ssid.c_str(), pass.c_str());

  Serial.println("Connecting to Wi-Fi; looking for the strongest mesh node...");

  // Start the connection attempt
  unsigned long startAttemptTime = millis();
  while (wifiMulti.run() != WL_CONNECTED)
  {
    // Additional diagnostics
    Serial.print("Last SSID attempted: ");
    Serial.println(WiFi.SSID());
    Serial.print("Last BSSID attempted: ");
    Serial.println(WiFi.BSSIDstr());
    Serial.print("Signal strength (RSSI): ");
    Serial.println(WiFi.RSSI());
    Serial.print("Connection status: ");
    Serial.println(WiFi.status());

    unsigned long currentMillis = millis();
    if (currentMillis - startAttemptTime >= interval)
    {
      Serial.println("Failed to connect after interval timeout.");

      // Note: WiFi.reasonCode() can provide additional reason if available
      // if (WiFi.status() != WL_CONNECTED)
      // {
      //   Serial.println("WiFi did not connect.");
      // }

      return false;
    }
    Serial.print(".");
    // WiFi.disconnect(true);
    delay(500); // Shorter delay to show progress

    // WiFi.reconnect();
  }

  // Successful connection
  Serial.println("\nWiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Connected to BSSID: ");
  Serial.println(WiFi.BSSIDstr());

  return true;
}

void AdvertiseServices()
{
  Serial.println("AdvertiseServices on mDNS...");
  const char *myName = gIdentity.name();

  if (MDNS.begin(myName))
  {
    Serial.println(F("mDNS responder started"));
    Serial.print(F("I am: "));
    Serial.println(myName);

    MDNS.addService(SERVICE_NAME, SERVICE_PROTOCOL, SERVICE_PORT);
  }
  else
  {
    while (1)
    {
      Serial.println(F("Error setting up MDNS responder"));
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
  Serial.println("Locating devices...");
  Serial.print("Found ");
  deviceCount = temptSensor.sensors.getDeviceCount();
  Serial.print(deviceCount, DEC);
  Serial.println(" devices.");
  Serial.println("");

  Serial.println("Printing addresses...");
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

static bool saveBootstrapConfigJson(const String &jsonBody, String &errOut)
{
  // 1) Parse JSON (sanity check)
  StaticJsonDocument<APP_CONFIG_JSON_CAPACITY> doc;
  DeserializationError err = deserializeJson(doc, jsonBody);
  if (err)
  {
    errOut = String("JSON parse error: ") + err.c_str();
    return false;
  }

  // 2) Apply into a temp AppConfig so we can validate without clobbering gConfig
  AppConfig tmp = gConfig; // start from current defaults
  JsonObject root = doc.as<JsonObject>();
  if (!configFromJson(root, tmp))
  {
    errOut = "configFromJson failed";
    return false;
  }

  // 3) Minimal bootstrap validation (tweak rules as you like)
  //    If you want AP bootstrap to ONLY set wifi + identity, keep it strict here.
  if (tmp.wifi.ssid.empty())
  {
    errOut = "Missing required field: wifi.ssid";
    return false;
  }
  if (tmp.wifi.pass.empty())
  {
    errOut = "Missing required field: wifi.pass";
    return false;
  }
  if (tmp.identity.locationName.empty())
  {
    errOut = "Missing required field: identity.locationName";
    return false;
  }

  // Default instance to locationName if omitted
  if (tmp.identity.instance.empty())
  {
    tmp.identity.instance = tmp.identity.locationName;
  }

  // 4) Persist to /bootstrap.json in modular format
  if (!ConfigStorage::saveAppConfigToFile(FNAME_BOOTSTRAP, tmp))
  {
    errOut = "Failed to write /bootstrap.json";
    return false;
  }

  // 5) Also update live gConfig (optional but useful for immediate debug endpoints)
  gConfig = tmp;

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

  // SPIFFS, legacy params, /config.json, W1 sensors, etc.
  setupSpiffsAndConfig();

  // Decide which path: Station vs AP
  if (initWiFi()) // Station Mode
  {
    // Start Telnet logger
    logger.begin(locationName.c_str(), SRL_TELNET_PASSWORD, SRL_TELNET_PORT, 64, 192);
    logger.log("Wifi and Telnet logger shoudl now be functioning.\n");
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

    setupAccessPointMode();
  }
}

void setupSpiffsAndConfig()
{
  Serial.println("initSpiffs...");
  initSPIFFS();

  // 1) Load BOOTSTRAP config first (new source of truth for wifi + identity + base URLs)
  if (ConfigStorage::loadAppConfigFromFile("/bootstrap.json", gConfig))
  {
    // no logger during bootstrap 
    Serial.println("AppConfig: loaded BOOTSTRAP from /bootstrap.json into gConfig. Since this succeeded, shoudl skip legacy indie spiff files.\n");
  }
  else
  {
    // no logger during bootstrap
    Serial.println("AppConfig: no /bootstrap.json (or parse error). Will try legacy bootstrap migration next.\n");

    // 1a) Legacy bootstrap migration path (one-time):
    // loadPersistedValues() reads /ssid.txt /pass.txt /location.txt /config-url.txt /ota-url.txt
    // We only migrate *bootstrap keys*.
    loadLegacyPersistedValues();

    // If legacy has at least SSID + PASS + LOCATION, write bootstrap.json
    // NOTE: This assumes ssid/pass/locationName/configUrl/otaUrl are the legacy globals you already have.
    if (ssid.length() > 0 && pass.length() > 0 && locationName.length() > 0)
    {
      AppConfig tmp = gConfig; // keep defaults
      tmp.wifi.ssid = std::string(ssid.c_str());
      tmp.wifi.pass = std::string(pass.c_str());

      tmp.identity.locationName = std::string(locationName.c_str());

      // Default instance to locationName (your rule)
      if (tmp.identity.instance.empty())
      {
        tmp.identity.instance = tmp.identity.locationName;
      }

      // If you have these in legacy:
      // configUrl = "http://salt-r420:9080/esp-config/salt"
      // otaUrl    = "http://.../firmware.bin"
      if (configUrl.length() > 0)
      {
        tmp.remote.configBaseUrl = std::string(configUrl.c_str());
      }
      if (otaUrl.length() > 0)
      {
        tmp.remote.otaUrl = std::string(otaUrl.c_str());
      }

      if (ConfigStorage::saveAppConfigToFile("/bootstrap.json", tmp))
      {
        Serial.println("AppConfig: migrated legacy bootstrap params -> /bootstrap.json\n");
        gConfig = tmp;
      }
      else
      {
        Serial.println("AppConfig: FAILED to write /bootstrap.json during legacy migration\n");
      }
    }
    else
    {
      Serial.println("AppConfig: legacy bootstrap values incomplete; staying unconfigured (AP mode expected)\n");
    }
  }

  // 2) Apply bootstrap values into your legacy globals that initWiFi() expects
  // (You can delete these legacy globals later, but for now keep it explicit)
  ssid = String(gConfig.wifi.ssid.c_str());
  pass = String(gConfig.wifi.pass.c_str());
  locationName = String(gConfig.identity.locationName.c_str());

  gIdentity.init(MDNS_DEVICE_NAME, gConfig.identity.locationName.c_str());

  // configUrl in your code is now baseUrl like http://salt-r420:9080/esp-config/salt
  configUrl = String(gConfig.remote.configBaseUrl.c_str());

  // otaUrl legacy string (if still used anywhere)
  otaUrl = String(gConfig.remote.otaUrl.c_str());
  mainDelay = gConfig.timing.mainDelayMs;

  // 3) Now load the effective runtime cache (mqtt/sensors/pins/etc)
  // if (loadEffectiveCacheFromFile(EFFECTIVE_CACHE_PATH))
  // {
  //   Serial.println("ConfigLoad: EFFECTIVE_CACHE_PATH = '/config.effective.cache.json' loaded and applied (overrides per-param SPIFFS files).");
  // }
  // else
  // {
  //   Serial.println("ConfigLoad: no EFFECTIVE_CACHE_PATH = '/config.effective.cache.json' (or parse error); continuing with bootstrap-only config.");
  // }

  Serial.println(ssid);
  Serial.println(pass);
  Serial.println(locationName);
  Serial.println(mainDelay);
}

void setupStationMode()
{
  // setup: path1 (Station Mode)

  tryFetchAndApplyRemoteConfig();

  logger.log("initDNS...\n");
  initDNS();

  // Initialize Zabbix agent server
  initZabbixServer();

  // @pattern
  if (gConfig.sensors.dht.enabled)
  {
    logger.log("DHT: enabled via gConfig, starting sensor task\n");
    initSensorTask(gConfig.sensors.dht.pin);
  }

  if (gConfig.sensors.acs.enabled)
  {
    setupACS712();
  }

  // I2C pins for CHT832x
  Wire.begin(32, 33); // todo:externalize: why is this here instead of inside the instantiation of the CHT sensor? Dec6'25
  if (gConfig.sensors.cht.enabled)
  {
    envSensor.begin();
  }

  if (gConfig.sensors.sct.enabled)
  {
    sctSensor.begin();
  }

  registerWebRoutesStation(server);

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
}

void setupAccessPointMode()
{
  // SETUP : Path2
  // This path is meant to run only upon initial one-time setup

  apStartTime = millis(); // Record the start time in AP mode

  Serial.println("Setting AP (Access Point)");

  // Build base SSID (without prefix)
  String base;

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

  Serial.print("AP SSID: ");
  Serial.println(apSsid);

  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  logger.begin(locationName.c_str(), SRL_TELNET_PASSWORD, SRL_TELNET_PORT, 64, 192);
  logger.log("boot\n");

  if (!SPIFFS.begin(true))
  {
    Serial.println("SPIFFS is out of scope per bwilly!");
  }
  // Web Server Root URL
  registerWebRoutesAp(server);

  logger.log("Starting web server...\n");
  server.begin();
}

void reconnectMQ()
{
  const char *clientId = gIdentity.name();

  while (!mqClient.connected())
  {
    logger.log("Attempting MQTT connection...");
    if (mqClient.connect(clientId))
    {
      logger.log("connected\n");
    }
    else
    {
      logger.log("failed, rc=");
      logger.log(mqClient.state());
      logger.log(" try again in 5 seconds\n");
      delay(5000);
    }
  }
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
      Serial.print("Message published successfully: ");
      Serial.println(message);
    }
    else
    {
      Serial.println("Message publishing failed.");
    }
  }
  else
  {
    Serial.println("Not connected to MQTT broker.");
    Serial.println(mqClient.state());

    reconnectMQ();
  }
}

// Helper function to calculate percentage change
float calculatePercentageChange(float oldValue, float newValue)
{
  if (isnan(oldValue) || oldValue == 0.0f)
  {
    return 100.0f; // Return 100% if old value is NaN or 0 to force an update
  }
  return abs((newValue - oldValue) / oldValue) * 100.0f;
}

void maybePublishEnvToMqtt(
    const char *sourceName,
    float currentTemperature,
    float currentHumidity,
    float &previousTemperatureRef,
    float &previousHumidityRef,
    unsigned long &lastPublishTimeTempRef,
    unsigned long &lastPublishTimeHumRef)
{
  unsigned long now = millis();

  float tempChange = calculatePercentageChange(previousTemperatureRef, currentTemperature);
  float humidityChange = calculatePercentageChange(previousHumidityRef, currentHumidity);

  unsigned long timeSinceLastPublishTemp = now - lastPublishTimeTempRef;
  unsigned long timeSinceLastPublishHum = now - lastPublishTimeHumRef;

  bool shouldPublishTemp = false;
  bool shouldPublishHum = false;

  if (tempChange >= THRESHOLD_TEMPERATURE_PERCENTAGE)
  {
    shouldPublishTemp = true;
  }
  if (humidityChange >= THRESHOLD_HUMIDITY_PERCENTAGE)
  {
    shouldPublishHum = true;
  }

  logger.log(sourceName);
  logger.log(" Temperature changed from ");
  logger.log(previousTemperatureRef);
  logger.log(" to ");
  logger.log(currentTemperature);
  logger.log(". Percentage change: ");
  logger.log(tempChange);
  logger.log("\n");

  logger.log(sourceName);
  logger.log(" Humidity changed from ");
  logger.log(previousHumidityRef);
  logger.log(" to ");
  logger.log(currentHumidity);
  logger.log(". Percentage change: ");
  logger.log(humidityChange);
  logger.log("\n");

  if (timeSinceLastPublishTemp >= PUBLISH_INTERVAL)
  {
    shouldPublishTemp = true;
    logger.log(sourceName);
    logger.log(": time interval since last temp publish exceeded; publishing.\n");
  }
  else
  {
    logger.log(sourceName);
    logger.log(": time to next temp publish: ");
    logger.log((PUBLISH_INTERVAL - timeSinceLastPublishTemp) / 1000);
    logger.log(" seconds.\n");
  }

  if (timeSinceLastPublishHum >= PUBLISH_INTERVAL)
  {
    shouldPublishHum = true;
    logger.log(sourceName);
    logger.log(": time interval since last humidity publish exceeded; publishing.\n");
  }
  else
  {
    logger.log(sourceName);
    logger.log(": time to next humidity publish: ");
    logger.log((PUBLISH_INTERVAL - timeSinceLastPublishHum) / 1000);
    logger.log(" seconds.\n");
  }

  if (shouldPublishTemp)
  {
    logger.log(sourceName);
    logger.log(": publishTemperature...\n");
    MessagePublisher::publishTemperature(mqClient, currentTemperature, locationName);
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
    MessagePublisher::publishHumidity(mqClient, currentHumidity, locationName);
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

void loop()
{
  unsigned long currentMillis = millis();

  logger.handle();
  logger.flush(16);

  if (g_bootstrapPending)
  {
    g_bootstrapPending = false;

    String err;
    bool ok = saveBootstrapConfigJson(g_bootstrapBody, err);

    if (ok)
    {
      logger.log("Bootstrap: saved /config.json; rebooting\n");
      logger.handle();
      logger.flush(16);
      delay(200);
      ESP.restart();
    }
    else
    {
      logger.log("Bootstrap: rejected: " + err + "\n");
      // optionally keep running AP mode so you can retry
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

  // Check if we are in AP mode and if so, if it's time to reboot
  if (WiFi.getMode() == WIFI_AP) // && WiFi.softAPIP().toString() == "192.168.4.1")
  {
    if (currentMillis - apStartTime >= AP_REBOOT_TIMEOUT)
    {
      Serial.println("Rebooting due to extended time in AP mode...");
      ESP.restart();
    }
  }

  // does this need to be here? I didn't use it, here, on the new master project. maybe it was only needed for the non-async web server? bwilly Feb26'23
  // server.handleClient();

  unsigned long current_time = millis(); // number of milliseconds since the upload

  // checking for WIFI connection
  if ((WiFi.status() != WL_CONNECTED) && (current_time - previous_time >= reconnect_delay))
  {
    Serial.print("millis: ");
    Serial.println(millis());
    Serial.print("previous_time: ");
    Serial.println(previous_time);
    Serial.print("current_time: ");
    Serial.println(current_time);
    Serial.print("reconnect_delay: ");
    Serial.println(reconnect_delay);
    Serial.println("Reconnecting to WIFI network by restarting to leverage best AP algorithm");
    // WiFi.disconnect();
    // WiFi.reconnect();
    ESP.restart();
    previous_time = current_time;
  }

  if (gConfig.mqtt.enabled)
  {

    reconnectMQ();
    // publishSimpleMessage(); // manual test

    if (!mqClient.connected())
    {
      reconnectMQ();
    }
    mqClient.loop();

    // @anti-pattern as compared to dhtEnabledValue? Maybe this is the bettr way and the dhtEnabledValue was mean for checkbox population? Mar4'25
    if (gConfig.sensors.dht.enabled)
    {
      float currentTemperature = readDHTTemperature();
      float currentHumidity = readDHTHumidity();

      maybePublishEnvToMqtt(
          "DHT",
          currentTemperature,
          currentHumidity,
          previousTemperature,
          previousHumidity,
          lastPublishTime_tempt,
          lastPublishTime_humidity);
    }

    // Dec3'25
    if (gConfig.sensors.cht.enabled)
    {
      float chtTemp = NAN;
      float chtHum = NAN;

      if (envSensor.read(chtTemp, chtHum))
      {
        maybePublishEnvToMqtt(
            "CHT832x",
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

    if (gConfig.sensors.sct.enabled)
    {
      float amps = sctSensor.readCurrentACRms();
      logger.logf("iot.sct.current %.3fA pin=%d rated=%.0f\n",
                  amps, PIN_SCT, SCT_RATED_AMPS);
      sctSensor.serialOutAdcDebug(); // for debug only
    }
  }

  if (gConfig.sensors.sct.enabled)
  {
    float amps = fabsf(sctSensor.readCurrentACRms());

    bool pumpState = (amps > FEEDPUMP_ON_THRESHOLD_AMPS);

    if (sctFirstRun || pumpState != sctLastPumpState || pumpState)
    {
      MessagePublisher::publishPumpState(
          mqClient,
          pumpState,
          amps,
          "feed-pump");

      sctLastPumpState = pumpState;
      sctFirstRun = false;
    }
  }

  if (gConfig.sensors.w1.enabled)
  {
    temptSensor.requestTemperatures();
    TemperatureReading *readings = temptSensor.getTemperatureReadings(); // todo:performance: move declaration outside of the esp loop

    for (int i = 0; i < MAX_READINGS; i++)
    {
      // Check if the reading is valid, e.g., by checking if the name is not empty
      if (!readings[i].name.isEmpty())
      {
        MessagePublisher::publishTemperature(mqClient, readings[i].value, readings[i].name);
      }
    }
  }

  if (gConfig.sensors.acs.enabled)
  {
    float amps = fabs(readACS712Current());
    logger.log(amps);
    logger.log(" amps\n");

    bool pumpState = (amps > PUMP_ON_THRESHOLD_AMPS);

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
      MessagePublisher::publishPumpState(mqClient, pumpState, amps, "engineRoomPump");
      lastPumpState = pumpState; // Update the last known state
      firstRun = false;
    }
  }

  logger.log("loop finished. sleep/delay...\n");
  // logger.flushTelnet();
  //  TelnetStream.println("loop finished. sleep/delay...");

  // (1) Read-and-parse the configured delay (in ms)
  int requestedDelay = mainDelay.toInt();

  // (2) Enforce a minimum of 500 ms—if below, log + bump up to 3000 ms
  int actualDelay = requestedDelay < 500
                        ? 3000
                        : requestedDelay;
  if (requestedDelay < 500)
  {
    logger.logf(
        "Requested delay (%d ms) < 500 ms; overriding to %d ms\n",
        requestedDelay, actualDelay);
  }

  delay(actualDelay); // Wait for 5 seconds before next loop
}