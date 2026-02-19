# ESP32-SRL-25
[Feb8'26]

ESP32-based IoT firmware project with bootstrap configuration, remote config fetch, and OTA firmware deployment.

Renamed to ESP32-SRL-25 from ESP32_Wifi_OTA_PromExporter on Jan11'26.

---

## 1. Project History & Status

Successfully uploaded and used this POC. Feb24'23

Built and deployed using VS Code and PlatformIO plugin.

---

## 2. Bootstrap Configuration

### Bootstrap Upload

Bootstrap upload to 192.168.4.1 [Feb4'26]

pwd = /Users/bwilly/PlatformIO/Projects/srlESP32-iot/ESP32-SRL-25/temp/config-module
```
curl -i -X POST \
  -H "Content-Type: application/json" \
  --data-binary @silviya-bedroom.bootstrap.json \
  http://192.168.4.1/config/post/bootstrap
```
---

## 3. Firmware Build & OTA Deployment

### Build Output Location (Mac)

/Users/bwilly/PlatformIO/Projects/srlESP32-iot/ESP32-SRL-25/.pio/build/nodemcu-32s

### Upload Firmware to Host

scp firmware.bin bwilly@salt-r420:~

### Move Firmware into Web-Served Repo (salt-r420)

cp ~/firmware.bin /home/bwilly/config-repo/esp-config/salt

---

## 4. Remote Config Fetch Behavior

Jan19'26

ConfigFetch: remote config unchanged vs /config-remote.json; no persist, no reboot

todo: above log isn't true as I changed the instance values.
but the new val did make it to config on device.

---

## 5. Known Issues / Investigation Links

Jan14'26

todo: potential bugs
- https://chatgpt.com/c/6927dcf0-ac18-8328-bb4c-5567ad3f3c0a
- https://chatgpt.com/g/g-p-690e1322bfb48191a2d52b69284ec5cd-srl-sensors/c/69685205-8868-832c-86c7-12571d09b5fe

---

## 6. RemoteDebug Build Patch

Dec14'25

Patch RemoteDebug to fix build error.

The build is failing because this header was removed/moved in newer ESP32 cores; the widely used workaround is to replace:

#include <hwcrypto/sha.h>

with:

#include <esp32/sha.h>

This is exactly what people report as the fix for this same error.
GitHub
+2
GitHub
+2

### Patch Location

.pio/libdeps/nodemcu-32s/RemoteDebug/src/utility/WebSockets.cpp

Open that file and change the include line near the top.

### Minimal Diff

-#include <hwcrypto/sha.h>
+#include <esp32/sha.h>

I have a copy of the corrected source at lib/RemoteDebug/WebSockets.cpp

Then rebuild.

---

## 7. Git Submodule Management

Git submodule update [Jan13'26]

todo: switch to SSH per ChatGPT

git -c protocol.file.allow=always submodule update --remote --merge

ABOVE still seems to require an explicit pull in the submodule dir.

From /Users/bwilly/local/esp32_local/config-core
```
   83cb33c..c628e03  main       -> origin/main
```

