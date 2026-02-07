#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <RemoteDebug.h>  // or whatever your RemoteDebug2 include is

class Logger {
public:
  Logger() = default;
  ~Logger() { end(); }

  // Keep your existing signature (so callers don't break)
  // New behavior: ALWAYS makes Serial fallback available immediately.
  // Telnet will auto-start later when WiFi is connected.
  bool begin(const char* hostname,
             const char* password,
             uint16_t port,
             size_t queueLen,
             size_t msgSize);

  // Optional explicit start (normally you don't need to call this)
  bool startTelnet();

  void handle();                 // call from loop()
  void flush(size_t maxDrain=16);
  void end();

  void log(const char* msg);
  void log(const String& msg);
  void log(int val);
  void log(unsigned long val);
  void log(float val, uint8_t decimals=2);
  void logf(const char* fmt, ...);

  uint32_t dropped() const { return _dropped; }

private:
  static constexpr size_t kMaxMsg = 256;

  bool enqueueCstr(const char* s);
  void cleanup_();
  bool wifiReady_() const;
  bool tryStartTelnet_();

private:
  QueueHandle_t _q = nullptr;
  char* _drainBuf = nullptr;
  size_t _msgSize = 0;
  volatile uint32_t _dropped = 0;

  // Telnet config + state
  bool _serialReady = false;
  bool _telnetConfigured = false;
  bool _telnetStarted = false;

  String _hostname;
  String _password;
  uint16_t _port = 0;

  RemoteDebug _dbg; // your RemoteDebug2 instance
};
