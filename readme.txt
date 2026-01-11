Renamed to ESP32-SRL-25 from ESP32_Wifi_OTA_PromExporter on Jan11'26


Successfully uploaded and used this POC. Feb24'23

Built and deployed using VS Code and PlatformIO plugin.


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

Where to patch (your case)

PlatformIO is compiling:

.pio/libdeps/nodemcu-32s/RemoteDebug/src/utility/WebSockets.cpp


So open that file and change the include line near the top.

Minimal diff
-#include <hwcrypto/sha.h>
+#include <esp32/sha.h>


Then rebuild.
