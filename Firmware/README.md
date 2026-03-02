# Aldervend Adapter Firmware

ESP32-C6 Mini, **ESP-IDF** framework.

## Requirements

- [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c6/get-started/) (v5.0+ recommended) with target `esp32c6`.

## Build and flash

```shell
pio run
pio run -t upload
pio run -t monitor
```

## Configuration (menuconfig)

To get to the menuconfig

```shell
pio run -t menuconfig
```

- **Aldervend Adapter → WiFi SSID / WiFi Password**: Set wifi login
- **Aldervend Adapter → OTA version check URL**: Default `https://api.aldervon.com/device/version`.

## To clear nv config

```
pio run -t erase
pio run -t upload && pio run -t monitor
```