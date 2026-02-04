#pragma once

#include "IFileStore.h"

// Forward declare to avoid leaking SPIFFS into headers if you want later.
// For now it's fine if this header is ESP32-only anyway.
class SpiffsFileStore final : public IFileStore {
public:
  bool readText(const char* path, std::string& out) override;
  bool writeText(const char* path, const std::string& data) override;
};
