# Aldervend Adapter Firmware

ESP32-C6 Mini, **ESP-IDF** framework.

- **OTA**: On boot (when WiFi is configured), device requests `CONFIG_VERSION_BASE_URL?id=<device_id>`, expects JSON `{ "version", "url", "sha256" }`, compares version; if newer, downloads firmware from `url`, verifies SHA256, installs and reboots.

## Requirements

- [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c6/get-started/) (v5.0+ recommended) with target `esp32c6`.

## Build and flash

```shell
pio run
pio run -t upload
pio run -t monitor
```

## Configuration (menuconfig)

- **Aldervend Adapter → WiFi SSID / WiFi Password**: Set wifi login
- **Aldervend Adapter → OTA version check URL**: Default `https://api.aldervon.com/device/version`.

## Version

Bump `FIRMWARE_VERSION` in `main/ota.h` when releasing; the device only updates when the remote `version` is **greater** (semver-style).

## Local OTA test server

See **www/README.md**. In short: copy `build/aldervend_adapter.bin` to `www/firmware.bin`, bump version in `www/version.json`, run `python3 www/serve.py`, proxy the update url thru to `https://api.aldervon.com/device/version`
