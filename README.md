# mstnDevController

Win32 + **WebView2** (offline HTML glass UI) for Densha de Go / train desk controllers.

- Live monitor: HEX/DEC/bits, P/B notches, buttons
- Key map + per-game presets (`profiles/<game>-<device>.json`)
- Keyboard output, disable feedback, door/vib test via `denconvMMF`
- Driver probe + Zadig prompt when WinUSB missing
- i18n TH/JA/EN (Thai → bundled **Noto Sans Thai Looped**), light/dark theme, center toasts
- Device picker: Type2 / Shinkansen / Ryojou / MTC / Keikyu / ZUIKI / DInput / XInput / DGOC-44 / DRC-184  
  (WinUSB read path live for PS2USB VID/PID family; others selectable for mapping/presets)

## Requirements

- Windows 10/11 x64
- Type2 on **Zadig WinUSB** (same as denconv)
- [WebView2 Evergreen Runtime](https://developer.microsoft.com/microsoft-edge/webview2/) (usually already installed with Edge)
- CMake ≥ 3.21, MSVC (VS 2022), Ninja optional
- Close **denconv** before Start USB (exclusive WinUSB handle)

## Build

```powershell
cd C:\Users\Naito\Desktop\BveTs\mstn-dev-controller
powershell -ExecutionPolicy Bypass -File .\scripts\fetch-webview2.ps1
cmake -S . -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release
cmake --build build
.\build\check_decode.exe
.\build\mstnDevController.exe
```

Visual Studio generator:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
.\build\Release\mstnDevController.exe
```

Dynamic CRT + `WebView2Loader.dll` beside the exe — **no static link**.

## Profiles

`profiles/bve6-type2.json` seeds BVE6 keys from the local Type2 map (Z/A/Q, ./,/, SPACE pedal, …). Use Load / Save / Export in the UI.

Calibration arrays `power_raw` / `brake_raw` are nearest-match tables — retune from the live BR/MS hex strip if notches feel off.

## MMF

Compatible with denconv Ver4.01 `denconvMMF` (64 bytes). Existing `DenconvFeedback.dll` can keep writing door/vib/ATC/speed; this app mirrors them in the Feedback panel and publishes notches on bytes 0–2.
