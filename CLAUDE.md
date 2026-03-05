# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

KoClicker is a single-file Arduino sketch for an **ESP32-C3** that acts as a wireless page-turner for KOReader running on a Kindle. It sends HTTP GET requests to KOReader's built-in HTTP server on port 8080.

## Build & Flash

This is an Arduino sketch (single `.ino` file). There is no makefile or test runner.

**Arduino IDE 2.x:**
- Compile: `Ctrl+R`
- Upload via USB: `Ctrl+U`
- Board: `ESP32C3 Dev Module` (or your specific ESP32-C3 variant)
- Optimize: Tools → Optimize → **MinSizeRel (-Os)**

**Arduino CLI:**
```bash
arduino-cli compile --fqbn esp32:esp32:esp32c3 .
arduino-cli upload  --fqbn esp32:esp32:esp32c3 --port <COMx> .
```

**OTA (after first flash):**
- Via Arduino IDE: ArduinoOTA on hostname `KoClicker`, password stored in NVS (default `1234`)
- Via browser: `http://<device-ip>/update` (ElegantOTA)

**Required libraries** (install via Arduino Library Manager):
- `AsyncTCP`, `ESPAsyncWebServer`, `ElegantOTA`, `ArduinoJson`

## Hardware (ESP32-C3 pin assignments)

| Symbol | GPIO | Notes |
|---|---|---|
| `BUTTON` | 9 | BOOT button; `INPUT_PULLUP`; `LOW` = pressed |
| `RED_LED` | 3 | Active-low (`ON=LOW`, `OFF=HIGH`) |
| `GREEN_LED` | 4 | Active-low |
| `BLUE_LED` | 5 | Active-low |

## Architecture

### WiFi Modes
Three modes are cycled by a super-long button press (restart required). Mode is persisted in NVS under key `"mode"`.

| Mode | LED | Behaviour |
|---|---|---|
| `AccessPoint` | Red | ESP32-C3 creates its own AP; Kindle connects to it. Kindle IP learned via `ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED` event. |
| `HotSpot` | Green | Both devices join a phone hotspot. Kindle IP found by subnet scan (255→1); last good IP cached in NVS as `last_Good_Ip`. |
| `Home` | Blue | Both devices on home WiFi. Kindle IP set manually in settings (`kindlhmipaddr`). |

### Button Actions (in `loop()`)
- **Short press** (`< shortClick` ms): next page → `event/GotoViewRel/1` then `broadcast/InputEvent`
- **Long press** (`shortClick` to `longClick` ms): previous page → `event/GotoViewRel/-1` then `broadcast/InputEvent`
- **Super-long press** (`> longClick` ms): call `switchMode()` → saves new mode to NVS, restarts

### KOReader HTTP API
All calls go to `http://<kindleIpAddress>:8080/koreader/<endpoint>` via `webCall()`. A successful response (HTTP > 0) flashes green LED; failure flashes red.

### Web Server Endpoints
| Path | Method | Handler |
|---|---|---|
| `/` | GET | `buildIndexHtml()` — dynamic index |
| `/console` | GET | `PAGE_CONSOLE` (PROGMEM) — WebSocket log console |
| `/settings` | GET | `buildSettingsHtml()` — dynamic settings form |
| `/api/settings` | GET/POST | `handleGetSettings` / `handlePostSettings` — JSON, uses ArduinoJson |
| `/api/reset` | POST | `handleReset` — clears NVS namespace, restarts AP |
| `/update` | GET | ElegantOTA firmware upload |

### Settings / NVS
All persistent settings are in NVS namespace `"settings"` (see `K_*` constants). Loaded into globals by `loadPreferences()`, which is called at boot and after every POST to `/api/settings`.

### Sleep
`esp_deep_sleep_start()` is called from `sleep()` when either:
- Inactivity timer exceeds `sleepCutoff` ms, **or**
- WiFi is disconnected (station count = 0 in AP mode; `WL_CONNECTED` lost otherwise)

Sleep is disabled when `sleepCutoff == 0`.

### Logging
`logLine(fmt, ...)` and `logLinenl(fmt, ...)` write to both `Serial` and all connected WebSocket clients simultaneously. Use these instead of `Serial.print` for anything that should appear in the `/console` page.
