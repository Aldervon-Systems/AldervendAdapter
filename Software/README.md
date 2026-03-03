# Cat Herder OTA Server (Docker)

**Endpoints**

### 1. Device Check-in
- **GET /device/checkin?id=<device_id>**
- Reply:  
  ```json
  {
    "api_base": "https://aldervon.com",
    "token": "...",
    "firmware": {
      "latest_version": "v1.1-23-gabcdef",
      "url": "https://aldervon.com/firmware/v1.1-23-gabcdef.bin",
      "checksum": "sha256hex"
    }
  }
  ```
  - `firmware` omitted or empty if not relevant.

### 2. Bulk Data Upload
- **POST /bulk**  
  Headers:  
    - `Authorization: Bearer <token>`  
    - `Content-Type: application/json`
  Body:
  ```json
  {
    "device_id": "...",
    "data": "Anything you want"
  }
  ```
  - Reply: `{}`

### 3. Firmware Download
- **GET /firmware/<filename>.bin**  
  Download latest .bin served from `/firmware/`.


## Quick start

1. Build:  
   `docker build -t cat-herder .`
2. Run:  
   `docker run -p 8000:8000 cat-herder`

Device firmware should set base/check-in URLs to the container’s host:port (e.g. `http://localhost:8000`).  
Upload new firmware bins to the volume or data dir as needed.


## Example Compose

```yml
services:
  app:
    build: .
    ports:
      - "8000:8000"
      - "8001:8001"
    volumes:
      - cat-herder-data:/app/.local
    environment:
      - API_BASE=https://api.aldervon.com

volumes:
  cat-herder-data:
```