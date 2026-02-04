#pragma once
#include <Arduino.h>

class DeviceIdentity {
public:
  void init(const char* nameTemplate, const String& locationName) {
    // template + location (add "-" if you want: "%s-%s")
    snprintf(_name, sizeof(_name), "%s%s", nameTemplate, locationName.c_str());
    _name[sizeof(_name) - 1] = '\0'; // defensive
  }

  const char* name() const {
    return _name[0] ? _name : "unnamed-device";
  }

private:
  char _name[64] = {0};
};
