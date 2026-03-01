# Local OTA test server

Self-serve `version.json` and firmware `.bin` for testing OTA without deploying to api.aldervon.com.

## Quick start

1. Build the firmware (from `Firmware/`):
   ```bash
   idf.py build
   ```
2. Copy the built binary into `www/`:
   ```bash
   cp build/aldervend_adapter.bin www/firmware.bin
   ```
3. Edit `www/version.json` and set `version` to something **newer** than the device (e.g. `1.0.1`) so the device will take the update.
4. Start the server (from repo root or `Firmware/`):
   ```bash
   python3 www/serve.py
   ```
   Or from `www/`: `python3 serve.py`
5. Set the device to use this server:
   - Run `idf.py menuconfig`
   - **Aldervend Adapter → OTA version check URL** = `http://<your-pc-ip>:8080/version`
   - Set **WiFi SSID** and **WiFi Password** so the device can reach your LAN.
6. Flash and run the device. On boot it will connect to WiFi, request `http://<ip>:8080/version?id=<device_id>`, get the JSON (with correct `url` and `sha256`), download `firmware.bin`, verify, and reboot into the new image.

## Notes

- The server rewrites `version.json` on each request so that `url` points at this host and `sha256` matches the current `firmware.bin`.
- For production, use `https://api.aldervon.com/device/version` and deploy `version.json` and the `.bin` there; the device appends `?id=<device_id>`.
