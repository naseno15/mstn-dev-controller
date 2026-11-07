# mstnDevController

Win32 + **WebView2** (offline HTML glass UI) for Densha de Go / train desk controllers. Also builds as a native macOS app (see [macOS](#macos-native)).

- Live monitor: HEX/DEC/bits, P/B notches, buttons
- Key map + per-game presets (`profiles/<game>-<device>.json`)
- Keyboard output, disable feedback, door/vib via USB vendor control + `denconvMMF`
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

## Build (static by default)

Static **MSVC `/MT`** + **`WebView2LoaderStatic.lib`** — no `vcruntime*.dll` / `WebView2Loader.dll` next to the exe. Target PC still needs [WebView2 Evergreen Runtime](https://developer.microsoft.com/microsoft-edge/webview2/) (usually already present with Edge).

```powershell
cd mstn-dev-controller
powershell -ExecutionPolicy Bypass -File .\scripts\fetch-webview2.ps1
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DMSTN_STATIC=ON
cmake --build build --config Release
.\build\Release\mstnDevController.exe
```

Package a portable zip:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\package-release.ps1 -Version 1.0.0
# → out\mstnDevController-1.0.0-win-x64.zip
```

Dynamic CRT + Loader DLL (optional): `-DMSTN_STATIC=OFF`.

## macOS (native)

Same UI and profiles in an AppKit + `WKWebView` app (`mstnCon.app`). macOS 12+, Xcode command line tools, CMake ≥ 3.21.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/check_decode && ./build/check_mac_keys && ./build/check_mmf_telemetry
open build/mstnCon.app        # or copy it to /Applications
```

- **No driver step**: Type2 is a vendor-class USB device, so `IOUSBHost` opens it directly (no Zadig/WinUSB). Door lamp / vibration work the same.
- **Accessibility**: key output uses `CGEventPost`; allow mstnCon in System Settings → Privacy & Security → Accessibility (the app asks on first launch). Ad-hoc builds get a new signature each rebuild, so re-tick it after rebuilding — or sign with your identity: `-DMSTN_CODESIGN_ID="Apple Development: …"`.
- Profiles live in `~/Library/Application Support/mstnCon/profiles` (seeded from the bundle once, never overwritten).
- Universal binary (arm64 + x86_64), links only macOS system frameworks — copy `mstnCon.app` anywhere.
- Default profile on macOS is `openbve-type2.json` (openBVE's default keys; no power-off key because openBVE's `Q` is `SINGLE_BRAKE`).
- Keys are sent by physical position (US ANSI layout). Keep openBVE focused while driving.
- Game feedback: with the `BveLive.Telemetry.OpenBve` input-device plugin (bve-live-dashboard `openbve-telemetry/`) enabled in openBVE, the app reads `~/Documents/BveTs/Settings/live_telemetry.json` (in place of Windows `denconvMMF`) for the train's notch counts (controller P5/B8 stretched onto the real train), speed and the door pilot lamp. Without it, notches map 1:1.

## Profiles

`profiles/bve6-type2.json` seeds BVE6 keys from the local Type2 map (Z/A/Q, ./,/, SPACE pedal, …). Use Load / Save / Export in the UI.

Calibration arrays `power_raw` / `brake_raw` are nearest-match tables — retune from the live BR/MS hex strip if notches feel off.

## MMF

Compatible with denconv Ver4.01 `denconvMMF` (64 bytes). Existing `DenconvFeedback.dll` can keep writing door/vib/ATC/speed; this app mirrors them in the Feedback panel and publishes notches on bytes 0–2.
