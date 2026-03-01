# KoClicker
KoClicker is an ESP32-powered remote control for the KoReader e-book application, allowing you to turn pages wirelessly.

## Board Support
This project is designed for the **LilyGo T7 S3** (ESP32-S3).

### Features
- Remote page turning for KoReader.
- WiFi configuration for HotSpot, Home, and Access Point modes.
- Battery management and deep sleep support.
- OTA (Over-The-Air) updates.
- Interactive Web Console for logs and commands.

## Build & Compile
The project is set up to automatically compile on every push to the `main` branch using GitHub Actions.

### Manual Compilation Requirements
- **Board:** ESP32S3 Dev Module
- **Flash Size:** 16MB
- **PSRAM:** OPI
- **USB Mode:** Hardware CDC
- **Libraries:**
  - `AsyncTCP`
  - `ESP Async WebServer`
  - `ElegantOTA`
  - `ArduinoJson`

## GitHub Actions
A workflow is included in `.github/workflows/compile.yml` that uses the `arduino-cli` to compile the binary and ensure code correctness on every commit.
