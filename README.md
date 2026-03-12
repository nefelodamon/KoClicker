# KoClicker

KoClicker is a wireless, single-button page-turner for [KOReader](https://koreader.rocks/) running on a Kindle. It runs on an **ESP32-C3** (or ESP32-S3) microcontroller and communicates with KOReader's built-in HTTP server to send page-turn commands over Wi-Fi.

---

## How It Works

KoClicker hosts a small web server and sends HTTP GET requests to KOReader's HTTP API on port 8080. A single physical button controls page navigation:

| Press | Action |
|---|---|
| Short press (`< 500 ms`) | Next page |
| Double-click (two short presses within `300 ms`) | Previous page |
| Long press (`500 ms – 5 s`) | Show average time per page on OLED |
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
| Button / Wake pin | 2 | BOOT button; `INPUT_PULLUP`; `LOW` = pressed; also used as deep-sleep wake pin |
| Red LED | 3 | Active-low |
| Green LED | 4 | Active-low |
| Blue LED | 7 | Active-low |
| OLED SCL | 6 | SSD1306 72×40, I2C |
| OLED SDA | 5 | SSD1306 72×40, I2C |

> **Note:** GPIO 5 (OLED SDA) must not be shared with any LED or other output.

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

The ESP32-C3 variant supports a 72×40 SSD1306 OLED display. It shows:

- **Line 1:** Three items always visible — mode label (`AP` / `HS` / `HM`) on the left, signal info centred, page counter on the right
- **Line 2:** Current action or state (e.g. `Connecting`, `Page-->`, `Kindle`, `Scanning`, `Sleeping in`)
- **Line 3:** Detail text (e.g. IP address, `Found`, `Error`, `MM:SS` sleep countdown)

**Signal info (line 1 centre):**

| Mode | Display | Example |
|---|---|---|
| AccessPoint | Number of connected stations | `1ST` |
| HotSpot / Home (connected) | RSSI in dBm | `-67dB` |
| HotSpot / Home (disconnected) | `--` | `--` |

On boot, an animated splash screen plays: the "KoClicker" logo flips horizontally, the firmware version types out character by character, then the screen fills with random pixels and fades to black.

Lines 2 and 3 automatically clear after 3 seconds of inactivity.

---

## Web Interface

Once connected, KoClicker's IP address is shown on the OLED and in the serial log. Open it in a browser to access:

| URL | Description |
|---|---|
| `http://<device-ip>/` | Status dashboard — shows current mode, Kindle IP, firmware version |
| `http://<device-ip>/settings` | Settings form — shows current mode, Kindle IP, and all configurable parameters |
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
| `doubleClickMs` | 300 ms | Time window after a short press to detect a second press as a double-click; set to `0` to disable |
| `kindlhmipaddr` | — | Kindle's static IP for Home mode |
| `ap_ssid` | `KoClicker` | SSID for AccessPoint mode |
| `ap_password` | `12345678` | Password for AccessPoint mode |
| `hs_ssid` / `hs_password` | — | Hotspot SSID and password |
| `hm_ssid` / `hm_password` | — | Home Wi-Fi SSID and password |
| `ota_password` | `1234` | Password for ArduinoOTA updates |
| `mac_filter` | — | Comma-separated MAC allowlist for AP mode (empty = allow all) |

**POST** to `/api/settings` with a JSON body to update settings programmatically. **POST** to `/api/reset` to wipe all settings and restart.

---

## Page Counter

KoClicker keeps a running count of page turns since the last boot. It increments by 1 on a successful next-page call and decrements by 1 on a successful previous-page call. The counter is displayed on the right side of OLED line 1 and resets to 0 on every reboot or wake from sleep.

A **double-click** (two short presses within the `doubleClickMs` window, default 300 ms) turns the page back (previous page). A **long press** displays the average time per page (`Avg/page:` on line 2, formatted time on line 3) without turning a page. If no pages have been turned yet, it shows `---`.

---

## Sleep Behaviour

KoClicker enters deep sleep (`esp_deep_sleep_start()`) when:

- The device has been idle (no button press) longer than `sleepCutoff`, **or**
- The Wi-Fi connection is lost (Kindle disconnected in AP mode, or Wi-Fi dropped in other modes)

Sleep is disabled when `sleepCutoff` is set to `0`.

**Countdown (ESP32-C3 only):** During the final 2 minutes before sleep, the OLED shows `Sleeping in / MM:SS`. During the final 60 seconds the mode LED (red / green / blue) blinks at 500 ms on/off as an additional warning.

The device wakes from sleep on a button press. On the ESP32-C3, the button (GPIO 2) doubles as the wake pin — both share the same GPIO. On wake, the device performs a full reboot and `setup()` runs from the start. The ESP32-S3 variant does not define a wake pin by default.

---

## Flashing

### First Flash (USB required)

The first flash must be done over USB — OTA is not available on a blank device.

Download the latest pre-built binary for your board from the [Releases](https://github.com/nefelodamon/KoClicker/releases) page, then flash it using one of the tools below.

**[ESP Web Flasher](https://espressif.github.io/esptool-js/) (browser, no install required):**
1. Open the link in Chrome or Edge (requires WebSerial support).
2. Connect the ESP32 via USB, click **Connect**, and select the port.
3. Set the flash address to `0x0` and select the downloaded `.bin` file.
4. Click **Program**.

**[esptool](https://github.com/espressif/esptool) (cross-platform CLI):**
```bash
# ESP32-C3
esptool.py --chip esp32c3 --port <COMx> write_flash 0x0 KoClicker-esp32c3.bin

# ESP32-S3
esptool.py --chip esp32s3 --port <COMx> write_flash 0x0 KoClicker-esp32s3.bin
```

**[ESP32 Flash Download Tool](https://www.espressif.com/en/support/download/other-tools) (Windows GUI):**
1. Select the chip type (`ESP32-C3` or `ESP32-S3`).
2. Add the `.bin` file at address `0x0`.
3. Select the correct COM port and click **Start**.

After flashing, open a serial monitor at **115200 baud** to confirm the device boots correctly.

### Subsequent Updates (OTA)

Once the device is running and connected to Wi-Fi, future firmware updates can be done wirelessly:

- **Browser:** Navigate to `http://<device-ip>/update`, select the new `.bin` file, and upload. The device restarts automatically on success.
- **Arduino IDE:** The device appears as a network port (`KoClicker`). Upload as normal — you will be prompted for the OTA password (default `1234`, configurable in settings).

> OTA updates require the device to be powered on and connected to Home mode.

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
