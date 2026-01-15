

Jan14'26

todo: potential bugs 
- https://chatgpt.com/c/6927dcf0-ac18-8328-bb4c-5567ad3f3c0a
- https://chatgpt.com/g/g-p-690e1322bfb48191a2d52b69284ec5cd-srl-sensors/c/69685205-8868-832c-86c7-12571d09b5fe




=============================================================================================================
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


Git submodule update [Jan13'26]: todo: switch to SSH per ChatGPT
git -c protocol.file.allow=always submodule update --remote --merge


