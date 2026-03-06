# KoClicker

KoClicker is a wireless, single-button page-turner for [KOReader](https://koreader.rocks/) running on a Kindle. It runs on an **ESP32-C3** (or ESP32-S3) microcontroller and communicates with KOReader's built-in HTTP server to send page-turn commands over Wi-Fi.

---

## How It Works

KoClicker hosts a small web server and sends HTTP GET requests to KOReader's HTTP API on port 8080. A single physical button controls page navigation:

| Press duration | Action |
|---|---|
| Short press (`< 500 ms`) | Next page |
| Long press (`500 ms – 5 s`) | Previous page |
| Super-long press (`> 5 s`) | Switch Wi-Fi mode (restarts) |

Visual feedback is provided via three LEDs (red, green, blue) and an optional OLED display.

---

## Wi-Fi Modes

Three connectivity modes can be cycled through via a super-long button press. The active mode is persisted in NVS (non-volatile storage) and survives reboots.

| Mode | Indicator LED | Description |
|---|---|---|
| **AccessPoint** | Red | KoClicker creates its own Wi-Fi AP. The Kindle connects directly to it. The Kindle's IP is learned automatically when it connects. |
| **HotSpot** | Green | Both KoClicker and the Kindle join a shared mobile hotspot. KoClicker scans the subnet (255→1) to find the Kindle's IP, and caches the last successful IP for faster reconnection. |
| **Home** | Blue | Both devices are on a home Wi-Fi network. The Kindle's IP is set manually in the settings page. |

---

## Hardware

### ESP32-C3

| Component | GPIO | Notes |
|---|---|---|
| Button | 9 | BOOT button; `INPUT_PULLUP`; `LOW` = pressed |
| Red LED | 3 | Active-low |
| Green LED | 4 | Active-low |
| Blue LED | 5 | Active-low |
| OLED SCL | 6 | SSD1306 72x40, I2C |
| OLED SDA | 5 | SSD1306 72x40, I2C |
| Wake pin | 2 | GPIO used to wake from deep sleep (must be GPIO 0–5) |

### ESP32-S3

| Component | GPIO | Notes |
|---|---|---|
| Button | 48 | `INPUT_PULLUP`; `LOW` = pressed |
| Red LED | 47 | Active-low |
| Green LED | 46 | Active-low |
| Blue LED | 45 | Active-low |

> **Note:** The ESP32-S3 variant does not include OLED display support or a defined deep-sleep wake pin. The startup sequence uses a simple LED animation instead of the OLED boot animation.

---

## OLED Display (ESP32-C3 only)

The ESP32-C3 variant supports a 72x40 SSD1306 OLED display. It shows:

- **Line 1:** Current Wi-Fi mode abbreviation (`AP` / `HS` / `HM`) and connection status (`OK` / `NOK` / `SCAN`)
- **Line 2:** Current action or state (e.g. `Connecting`, `Page-->`, `Kindle`, `Scanning`)
- **Line 3:** Detail text (e.g. IP address, `Found`, `Error`, sleep countdown)

On boot, an animated splash screen plays: the "KoClicker" logo flips horizontally, the firmware version types out character by character, then the screen fills with random pixels and fades to black.

Lines 2 and 3 automatically clear after 3 seconds of inactivity.

---

## Web Interface

Once connected, KoClicker's IP address is shown on the OLED and in the serial log. Open it in a browser to access:

| URL | Description |
|---|---|
| `http://<device-ip>/` | Status dashboard — shows mode, IP addresses, firmware version |
| `http://<device-ip>/settings` | Settings form — configure all parameters |
| `http://<device-ip>/console` | Live WebSocket log console with interactive commands |
| `http://<device-ip>/update` | OTA firmware update via browser (ElegantOTA) |

### Console Commands

Connect to `/console` in a browser to view live logs and run commands:

| Command | Description |
|---|---|
| `help` | List available commands |
| `version` | Firmware version |
| `uptime` | Time since last boot |
| `heap` | Free / minimum free heap memory |
| `ip` | KoClicker's IP address |
| `kindle` | Current Kindle IP address |
| `mode` | Active Wi-Fi mode |
| `wifi` | SSID and RSSI (or AP station count) |
| `rssi` | Wi-Fi signal strength in dBm |
| `settings` | Dump key runtime settings |
| `sleep` | Sleep cutoff and current idle time |
| `restart` | Restart the device |

---

## Settings

All settings are stored in NVS and configurable via `/settings` or the JSON API at `/api/settings`.

| Setting | Default | Description |
|---|---|---|
| `shortClick` | 500 ms | Maximum duration for a short (next page) press |
| `longClick` | 5000 ms | Maximum duration for a long (previous page) press; beyond this triggers mode switch |
| `sleepCutoff` | 300000 ms (5 min) | Idle time before deep sleep; set to `0` to disable sleep |
| `httpCallTimeout` | 750 ms | HTTP response timeout for KOReader calls |
| `httpConnectTimeout` | 750 ms | HTTP connection timeout |
| `delayBetweenCalls` | 400 ms | Delay between the `GotoViewRel` and `InputEvent` HTTP calls |
| `kindlhmipaddr` | — | Kindle's static IP for Home mode |
| `ap_ssid` | `KoClicker` | SSID for AccessPoint mode |
| `ap_password` | `12345678` | Password for AccessPoint mode |
| `hs_ssid` / `hs_password` | — | Hotspot SSID and password |
| `hm_ssid` / `hm_password` | — | Home Wi-Fi SSID and password |
| `ota_password` | `1234` | Password for ArduinoOTA updates |
| `mac_filter` | — | Comma-separated MAC allowlist for AP mode (empty = allow all) |

**POST** to `/api/settings` with a JSON body to update settings programmatically. **POST** to `/api/reset` to wipe all settings and restart.

---

## Sleep Behaviour

KoClicker enters deep sleep (`esp_deep_sleep_start()`) when:

- The device has been idle (no button press) longer than `sleepCutoff`, **or**
- The Wi-Fi connection is lost (Kindle disconnected in AP mode, or Wi-Fi dropped in other modes)

Sleep is disabled when `sleepCutoff` is set to `0`. A countdown timer is shown on the OLED during the final 2 minutes before sleep (ESP32-C3 only).

The device wakes from sleep on a button press via GPIO wake. On the ESP32-C3, `WAKE_PIN` defaults to GPIO 2 (must be in the RTC domain, GPIO 0–5). The ESP32-S3 variant does not define a wake pin by default — add one in the sketch to enable wake-from-sleep.

---

## Build & Flash

**Requirements (install via Arduino Library Manager):**
- `AsyncTCP`
- `ESPAsyncWebServer`
- `ElegantOTA`
- `ArduinoJson`
- `U8g2` (for OLED support on ESP32-C3)

**Arduino IDE 2.x:**

| Target | Board selection | Notes |
|---|---|---|
| ESP32-C3 | `ESP32C3 Dev Module` | OLED and deep-sleep wake supported |
| ESP32-S3 | `ESP32S3 Dev Module` | No OLED; adjust `PSRAM` and flash settings as needed |

- Tools → Optimize → **MinSizeRel (-Os)** (recommended for both)
- Compile: `Ctrl+R` | Upload: `Ctrl+U`

**Arduino CLI:**
```bash
# ESP32-C3
arduino-cli compile --fqbn esp32:esp32:esp32c3 .
arduino-cli upload  --fqbn esp32:esp32:esp32c3 --port <COMx> .

# ESP32-S3
arduino-cli compile --fqbn esp32:esp32:esp32s3 .
arduino-cli upload  --fqbn esp32:esp32:esp32s3 --port <COMx> .
```

**Flashing — First Time (USB required)**

The first flash must be done over USB, as the OTA functionality is not available on a blank device:

1. Connect the ESP32 to your PC via USB.
2. Select the correct board and port in Arduino IDE (or specify `--port` in the CLI).
3. Compile and upload (`Ctrl+U` / `arduino-cli upload`).
4. Open the Serial Monitor at 115200 baud to confirm the device boots and connects.

**Subsequent Updates (OTA)**

After the first USB flash, future firmware updates can be done wirelessly in two ways:

- **Browser:** Navigate to `http://<device-ip>/update`, select a compiled `.bin` file, and upload. The device restarts automatically on success.
- **Arduino IDE:** Select the `KoClicker` device from Tools → Port (it will appear as a network port). Upload as normal — you will be prompted for the OTA password (default `1234`, configurable in settings).

> OTA updates require the device to be powered on and connected to Wi-Fi.

---

## KOReader API

KoClicker calls KOReader's HTTP server on port **8080**. The endpoints used are:

| Endpoint | Trigger |
|---|---|
| `GET /koreader/event/GotoViewRel/1` | Next page |
| `GET /koreader/event/GotoViewRel/-1` | Previous page |
| `GET /koreader/broadcast/InputEvent` | Confirm page turn |
| `GET /koreader/event/` | Connectivity check |
| `GET /koreader/event/ToggleNightMode` | Flashed twice on startup to confirm connection |

A successful HTTP response (code > 0) flashes the green LED; a failure flashes red.
