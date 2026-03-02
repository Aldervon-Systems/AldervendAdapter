#!/usr/bin/env python3

import requests
import json
import os
import time
from datetime import datetime

# ===== CONFIG =====
BASE_URL = "https://api.aldervon.com"
DEVICE_ID = "esp32c6-test-01"
FIRMWARE_VERSION = "1.0.0"
HARDWARE_VERSION = "revA"
TOKEN_FILE = ".device_token.json"

# ===== UTIL =====

def pretty_print(title, data):
    print("\n" + "=" * 60)
    print(title)
    print("=" * 60)
    if isinstance(data, (dict, list)):
        print(json.dumps(data, indent=4))
    else:
        print(data)
    print("=" * 60)


def save_token(token):
    with open(TOKEN_FILE, "w") as f:
        json.dump({"token": token}, f)


def load_token():
    if not os.path.exists(TOKEN_FILE):
        return None
    with open(TOKEN_FILE, "r") as f:
        return json.load(f).get("token")


def response_body(r):
    """Parse response as JSON if possible, otherwise return raw text and status info."""
    try:
        if not r.text or not r.text.strip():
            return {"_raw": "(empty response)", "_status": r.status_code}
        return r.json()
    except (json.JSONDecodeError, requests.exceptions.JSONDecodeError):
        return {"_raw": r.text[:500], "_status": r.status_code, "_content_type": r.headers.get("Content-Type", "")}


# ===== HANDSHAKE =====

def register():
    url = f"{BASE_URL}/device/register"

    payload = {
        "device_id": DEVICE_ID,
        "firmware_version": FIRMWARE_VERSION,
        "hardware_version": HARDWARE_VERSION,
        "capabilities": ["serial_push"]
    }

    pretty_print("REGISTER → REQUEST", payload)

    r = requests.post(url, json=payload)
    body = response_body(r)

    pretty_print("REGISTER ← RESPONSE", body)

    if r.status_code != 200:
        raise Exception(f"Registration failed: HTTP {r.status_code} — {body.get('_raw', body)}")

    if isinstance(body.get("auth"), dict) and body["auth"].get("token"):
        token = body["auth"]["token"]
        save_token(token)
        return token
    raise Exception(f"Registration response missing auth token: {body}")


def authenticated_get(endpoint, token):
    url = f"{BASE_URL}{endpoint}"
    headers = {
        "Authorization": f"Bearer {token}"
    }

    pretty_print(f"GET {endpoint} → HEADERS", headers)

    r = requests.get(url, headers=headers)
    pretty_print(f"GET {endpoint} ← RESPONSE", response_body(r))

    return r


def authenticated_post(endpoint, token, payload):
    url = f"{BASE_URL}{endpoint}"
    headers = {
        "Authorization": f"Bearer {token}"
    }

    pretty_print(f"POST {endpoint} → HEADERS", headers)
    pretty_print(f"POST {endpoint} → PAYLOAD", payload)

    r = requests.post(url, headers=headers, json=payload)
    pretty_print(f"POST {endpoint} ← RESPONSE", response_body(r))

    return r


# ===== MAIN FLOW =====

def main():
    token = load_token()

    if not token:
        print("No token found, registering device...")
        token = register()
    else:
        print("Loaded existing token.")

    # Test config endpoint
    authenticated_get("/device/config", token)

    # Send heartbeat
    heartbeat_payload = {
        "firmware_version": FIRMWARE_VERSION,
        "free_heap": 123456,
        "timestamp": int(time.time())
    }

    authenticated_post("/device/heartbeat", token, heartbeat_payload)

    # Simulate serial push
    push_payload = {
        "timestamp": int(time.time()),
        "data": "SERIAL: temperature=23.4C"
    }

    authenticated_post("/device/push", token, push_payload)


if __name__ == "__main__":
    main()
