#include "Logger.h"
#include <stdarg.h>
#include <string.h>

#if defined(ARDUINO)
  #include <WiFi.h>
#endif

void Logger::cleanup_() {
  if (_q) {
    vQueueDelete(_q);
    _q = nullptr;
  }
  if (_drainBuf) {
    free(_drainBuf);
    _drainBuf = nullptr;
  }
  _msgSize = 0;
  _serialReady = false;
  _telnetConfigured = false;
  _telnetStarted = false;
}

void Logger::end() {
  cleanup_();
}

bool Logger::wifiReady_() const {
#if defined(ARDUINO)
  return WiFi.status() == WL_CONNECTED;
#else
  return false;
#endif
}

bool Logger::begin(const char* hostname,
                   const char* password,
                   uint16_t port,
                   size_t queueLen,
                   size_t msgSize)
{
  // If begin called twice, cleanly reset.
  cleanup_();

  if (!hostname) hostname = "esp";
  if (!password) password = "";

  // Hard cap msg size to avoid stack blowups + keep behavior consistent
  if (msgSize == 0) msgSize = kMaxMsg;
  if (msgSize > kMaxMsg) msgSize = kMaxMsg;
  _msgSize = msgSize;

  _q = xQueueCreate(queueLen, _msgSize);
  if (!_q) { cleanup_(); return false; }

  _drainBuf = (char*)malloc(_msgSize);
  if (!_drainBuf) { cleanup_(); return false; }

  _serialReady = true;

  // Store telnet config; do NOT require WiFi here.
  _hostname = hostname;
  _password = password;
  _port = port;
  _telnetConfigured = true;
  _telnetStarted = false;

  enqueueCstr("Serial logger ready (telnet pending)\n");
  return true; // important: Serial fallback is now “ready”
}

bool Logger::tryStartTelnet_() {
  if (!_telnetConfigured || _telnetStarted) return _telnetStarted;
  if (!wifiReady_()) return false;

  // Now it’s safe/useful to start RemoteDebug
  _dbg.begin(_hostname.c_str());
  _dbg.setPassword(_password);
  _dbg.setSerialEnabled(true); // mirror to USB serial too (optional)

  // Port: API differs between forks; enable if your fork supports it
  // _dbg.setPort(_port);

  _telnetStarted = true;
  enqueueCstr("Telnet logger started\n");
  return true;
}

bool Logger::startTelnet() {
  return tryStartTelnet_();
}

void Logger::handle() {
  // auto-start telnet when WiFi becomes ready
  tryStartTelnet_();

  if (_telnetStarted) {
    _dbg.handle();
  }
}

void Logger::flush(size_t maxDrain) {
  if (!_q || !_drainBuf) return;

  // auto-start telnet when WiFi becomes ready
  tryStartTelnet_();

  for (size_t i = 0; i < maxDrain; i++) {
    if (xQueueReceive(_q, _drainBuf, 0) != pdTRUE) break;

    // Paranoia: ensure termination even if caller sent a full slot
    _drainBuf[_msgSize - 1] = '\0';

    if (_telnetStarted) {
      _dbg.print(_drainBuf);
    } else if (_serialReady) {
      Serial.print(_drainBuf);
    }
  }
}

bool Logger::enqueueCstr(const char* s) {
  if (!_q || !s) return false;

  char tmp[kMaxMsg];

  // Copy at most _msgSize-1, always terminate
  const size_t cap = (_msgSize <= kMaxMsg) ? _msgSize : kMaxMsg;
  strncpy(tmp, s, cap - 1);
  tmp[cap - 1] = '\0';

  if (xQueueSend(_q, tmp, 0) != pdTRUE) {
    _dropped++;
    return false;
  }
  return true;
}

void Logger::log(const char* msg) { enqueueCstr(msg); }
void Logger::log(const String& msg) { enqueueCstr(msg.c_str()); }

void Logger::log(int val) {
  char b[32];
  snprintf(b, sizeof(b), "%d", val);
  enqueueCstr(b);
}

void Logger::log(unsigned long val) {
  char b[32];
  snprintf(b, sizeof(b), "%lu", val);
  enqueueCstr(b);
}

void Logger::log(float val, uint8_t decimals) {
  char b[48];
  char f[32];
  dtostrf(val, 0, decimals, f);
  snprintf(b, sizeof(b), "%s", f);
  enqueueCstr(b);
}

void Logger::logf(const char* fmt, ...) {
  char b[kMaxMsg];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(b, sizeof(b), fmt, ap);
  va_end(ap);
  enqueueCstr(b);
}
