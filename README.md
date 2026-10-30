# Out of Seconds
30-second endless runner (raylib 5.5, single file `src/main.c`, sprite sheet in `assets/runner.png` (46 frames, 48x44 cells; packed into the APK assets root on Android)). Menu: RUN / UPGRADES / EXIT.

Controls (keyboard / touch buttons):
- Jump: Space / W / Up
- Dash: Shift / D / Right
- Slide: S / Down / Ctrl
- Menu: Up/Down + Enter; Esc back

Build PC (needs ..\toolchain\w64devkit):
    .\build.ps1 -Run

Android arm64 APK, landscape (needs ..\toolchain\android; package com.fleetinglabs.jamsandbox):
    .\build_apk.ps1          # or .\android\build_apk.ps1 [-Version 0.1.0 -VersionCode 1]
Output: build\android\JamSandbox-v<version>.apk
