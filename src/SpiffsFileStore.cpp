#include "SpiffsFileStore.h"

#include "SPIFFS.h"
#include <Arduino.h>

bool SpiffsFileStore::writeText(const char* path, const std::string& data) {
  File f = SPIFFS.open(path, FILE_WRITE); // truncates or creates
  if (!f) return false;

  size_t written = f.print(data.c_str());
  f.close();

  return written == data.length();
}

bool SpiffsFileStore::readText(const char* path, std::string& out) {
  File f = SPIFFS.open(path, FILE_READ);
  if (!f) return false;

  size_t size = f.size();
  out.resize(size);

  if (size > 0) {
    f.readBytes(&out[0], size);
  }

  f.close();
  return true;
}
