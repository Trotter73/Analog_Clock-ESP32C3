Not tested.

Check Device Manager under Ports (COM & LPT) with the ESP plugged in to identify what com port is it, substitute the actual port for COM 5 below.

Open PowerShell and change directory to the the sketch folder...Then.......

.\compile.ps1 -Port COM5      # build and flash
.\compile.ps1 -NoWrite        # build only

If Windows blocks the script, run powershell -ExecutionPolicy Bypass -File .\compile.ps1 -Port COM5. The port defaults to COM3. Look up the real one in Device Manager under Ports (COM & LPT).

Same flow: it keeps the original steps. It builds the LittleFS data image, flashes it to 0x230000, compiles with core 2.0.17 and the same build flags, then uploads. -NoWrite replaces --noWrite.

Prerequisites: Git and Python 3 need to be on your PATH. winget install Git.Git Python.Python.3.12 covers both.
Tool storage: core, tools and libraries go in C:\arduino_clock_cli instead of /tmp. The path is short on purpose, because the ESP32 core's deep folder tree can hit Windows' 260-character path limit.

Caches: the cache wipe now targets %TEMP%\arduino\sketches and %TEMP%\arduino\cores, which is where Windows keeps them, plus build_cache.

Tool lookup: it finds mklittlefs.exe and esptool by searching the tools folder, so it doesn't depend on the exact version folder names. It uses esptool.exe if present and falls back to esptool.py through Python.
Failures: the script stops at the first failing step instead of carrying on.
