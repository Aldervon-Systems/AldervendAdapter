# Local OTA test server

Run `python serve.py [port]` to serve the local test server
Set firmware `CONFIG_VERSION_BASE_URL` and `CONFIG_CHECKIN_URL` to point here

---

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

The device keeps `api_base` and `token` in RAM for the current session only (not persisted). It check-ins every boot and uses the returned values for authenticated requests until the next reboot.
