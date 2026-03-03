# Local OTA test server

Run `python serve.py [port]` to serve the local test server
Set firmware `CONFIG_VERSION_BASE_URL` and `CONFIG_CHECKIN_URL` to point here


## Check-in (device handshake)

The firmware calls this on every boot after WiFi connect. The server must implement the following.

**Request**

- **Method:** `GET`
- **Path:** `/device/checkin`
- **Query:** `id=<device_id>` (hex device identifier)

**Response**

- **Status:** `200 OK`
- **Body:** JSON object with at least:
  - `api_base` (string): Base URL for subsequent API calls (e.g. `https://api.example.com` or `http://10.0.0.1:8080`). No trailing slash.
  - `token` (string): Bearer token the device will send in `Authorization: Bearer <token>` when calling endpoints under `api_base`.
  - `heartbeat` (int): Optional number of seconds a device should configure is heartbeat pulses
- **Headers (recommended):** `Content-Type: application/json`. Optionally send `Authorization: Bearer <token>` so the client can use the same token from the body.

**Example response body**

```json
{
  "api_base": "https://api.aldervon.com",
  "token": "eyJhbG..."
}
```

**Firmware block (OTA, optional)**

To advertise an OTA update, include a `firmware` object in the check-in response. Omit `firmware` or send `"firmware": {}` when no update is available; the device skips OTA gracefully.

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `latest_version` | string | yes* | Version string the device compares to its own (e.g. `v1.0-19-gabc1234`). Format: `vMAJOR.MINOR-COMMIT[-suffix]`. |
| `url` | string | yes* | Full HTTPS URL of the firmware binary. |
| `checksum` | string | no | SHA-256 hex of the binary (informational; device uses image validation). |

\* If `firmware` is present, both `latest_version` and `url` must be set for the device to attempt OTA; missing or empty values are treated as “no update”.

**Example with firmware**

```json
{
  "api_base": "https://api.aldervon.com",
  "token": "eyJhbG...",
  "firmware": {
    "latest_version": "v1.0-19-gabc1234",
    "url": "https://api.aldervon.com/firmware/v1.0-19.bin",
    "checksum": "a1b2c3..."
  }
}
```

The device keeps `api_base` and `token` in RAM for the current session only (not persisted). It check-ins every boot and uses the returned values for authenticated requests until the next reboot.


## Bulk (MDB data upload)

For bulk data (mostly just to faccilitate a smarter firmware sorting system) the adapter should push to /bulk like this

**Request**

- **Method:** `POST`
- **Path:** `/bulk` (full URL: `api_base` + `/bulk`, e.g. `https://api.example.com/bulk`)
- **Headers:**
  - `Content-Type: application/json`
  - `Authorization: Bearer <token>` (session token from check-in)
- **Body:** JSON object with at least:
  - `device_id` (string): Device identifier (hex).
  - `data` (string): Payload from the device (e.g. MDB/telemetry data; format is application-specific).

**Example request body**

```json
{
  "device_id": "c6de87aa1787e36010649810",
  "data": "Test Data..."
}
```

**Response**

- **Status:** `200 OK` (or any 2xx). The device treats 2xx as success and continues; non-2xx or connection failure is logged and the task retries after the next interval.
- **Body:** Optional (e.g. `{}` or a JSON object). I dont do anything with the replied body but replying `{'OK'}` could be nice.


# Notes for Joe
Copy the latest .bin to www

```shell
cp .pio/build/esp32-c6/firmware.bin www/$(git describe --long --always).bin
```
