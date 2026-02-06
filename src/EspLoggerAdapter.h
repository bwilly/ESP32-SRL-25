#pragma once

#include <cstdarg>
#include <cstdio>

#include "ILogger.h"   // from config-core include path
#include "Logger.h"    // your existing ESP Logger (RemoteDebug + queue)

class EspLoggerAdapter final : public ILogger {
public:
  explicit EspLoggerAdapter(Logger& impl) : impl_(impl) {}

  void log(const char* msg) override { impl_.log(msg); }
  void log(int v) override { impl_.log(v); }
  void log(unsigned long v) override { impl_.log(v); }
  void log(float v, uint8_t decimals = 2) override { impl_.log(v, decimals); }

  void vlogf(const char* fmt, va_list args) override {
    // Format here and forward to impl_.log().
    // (If you later add Logger::vlogf, we can call that instead.)
    char buf[256];
    vsnprintf(buf, sizeof(buf), fmt, args);
    impl_.log(buf);
  }

private:
  Logger& impl_;
};
