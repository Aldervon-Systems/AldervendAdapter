"""API server (port 8000)"""
import hashlib
import logging
import os
import re
import secrets
import sqlite3
from datetime import datetime, timezone
from pathlib import Path

from flask import Flask, jsonify, request

api_app = Flask(__name__)
logger = logging.getLogger(__name__)

# .local dir (next to this file): SQLite DB + firmware/
LOCAL_DIR = Path(__file__).resolve().parent / ".local"
FIRMWARE_DIR = LOCAL_DIR / "firmware"
DB_PATH = LOCAL_DIR / "devices.db"

# Response config (override with env)
API_BASE = os.environ.get("API_BASE", "https://api.aldervon.com")
TOKEN_BYTES = 16  # 32 hex chars


def init_local():
    """Ensure .local and .local/firmware exist; init SQLite schema."""
    LOCAL_DIR.mkdir(exist_ok=True)
    FIRMWARE_DIR.mkdir(exist_ok=True)
    with sqlite3.connect(DB_PATH) as conn:
        conn.execute(
            """
            CREATE TABLE IF NOT EXISTS devices (
                device_id TEXT PRIMARY KEY,
                last_seen TEXT NOT NULL,
                token TEXT NOT NULL,
                version TEXT
            )
            """
        )
        # Add token column if table existed from before we added it
        try:
            conn.execute("ALTER TABLE devices ADD COLUMN token TEXT")
        except sqlite3.OperationalError:
            pass
        # Add version column if missing
        try:
            conn.execute("ALTER TABLE devices ADD COLUMN version TEXT")
        except sqlite3.OperationalError:
            pass
        # Backfill a unique token per device for any existing rows
        for (device_id,) in conn.execute(
            "SELECT device_id FROM devices WHERE token = '' OR token IS NULL"
        ).fetchall():
            conn.execute(
                "UPDATE devices SET token = ? WHERE device_id = ?",
                (secrets.token_hex(TOKEN_BYTES), device_id),
            )


def record_device(device_id: str, version: str | None = None) -> str:
    """Record or update device; return the device's token (existing or newly generated)."""
    now = datetime.now(timezone.utc).isoformat()
    with sqlite3.connect(DB_PATH) as conn:
        cur = conn.execute(
            "SELECT token, version FROM devices WHERE device_id = ?", (device_id,)
        )
        row = cur.fetchone()
        if row and row[0]:
            token = row[0]
            new_version = version if version is not None else row[1]
            conn.execute(
                "UPDATE devices SET last_seen = ?, version = ? WHERE device_id = ?",
                (now, new_version, device_id),
            )
        else:
            token = secrets.token_hex(TOKEN_BYTES)
            conn.execute(
                "INSERT INTO devices (device_id, last_seen, token, version) VALUES (?, ?, ?, ?)",
                (device_id, now, token, version),
            )
    return token


def get_all_devices() -> list[dict]:
    """Return all devices with device_id, last_seen (ISO), version for the UI."""
    with sqlite3.connect(DB_PATH) as conn:
        conn.row_factory = sqlite3.Row
        rows = conn.execute(
            "SELECT device_id, last_seen, version FROM devices ORDER BY last_seen DESC"
        ).fetchall()
    return [dict(r) for r in rows]


def _version_key(filename: str) -> tuple:
    """Parse v1.0-20-gabc1234.bin -> (1, 0, 20) for sorting."""
    m = re.match(r"v(\d+)\.(\d+)-(\d+)-", filename)
    if m:
        return (int(m.group(1)), int(m.group(2)), int(m.group(3)))
    return (0, 0, 0)


def get_latest_firmware() -> dict | None:
    """Return latest firmware info from .local/firmware/*.bin, or None if empty."""
    bins = list(FIRMWARE_DIR.glob("*.bin"))
    if not bins:
        return None
    latest_path = max(bins, key=lambda p: _version_key(p.name))
    version = latest_path.stem  # e.g. v1.0-20-g4249e05
    digest = hashlib.sha256(latest_path.read_bytes()).hexdigest()
    url = f"{API_BASE.rstrip('/')}/firmware/{latest_path.name}"
    return {
        "latest_version": version,
        "url": url,
        "checksum": digest,
    }


@api_app.route("/device/checkin")
def device_checkin():
    """Device check-in: ?id=<device_id>&version=<version>. Records device, returns api_base, token, firmware."""
    device_id = request.args.get("id", "").strip()
    if not device_id:
        return jsonify({"error": "missing id"}), 400
    version = request.args.get("version", "").strip() or None
    token = record_device(device_id, version=version)
    logger.info("Device check-in: %s (version=%s)", device_id, version or "unknown")
    payload = {
        "api_base": API_BASE,
        "token": token,
        "firmware": get_latest_firmware(),
    }
    return jsonify(payload)


@api_app.route("/bulk", methods=["POST"])
def bulk():
    """Accept bulk data from devices; update device record (last_seen, version), log payload, return {}."""
    try:
        payload = request.get_json(silent=True)
        if payload is None:
            payload = request.get_data(as_text=True) or "(empty)"
        logger.info("Bulk data: %s", payload)
        if isinstance(payload, dict) and payload.get("device_id"):
            device_id = str(payload["device_id"]).strip()
            version = payload.get("version")
            if version is not None:
                version = str(version).strip() or None
            record_device(device_id, version=version)
    except Exception as e:
        logger.warning("Bulk read/update failed: %s", e)
    return jsonify({})


@api_app.route("/", defaults={"path": ""})
@api_app.route("/<path:path>")
def catch_all(path):
    logger.info("API request: %s", path or "/")
    return jsonify({})


# Run once when module loads
init_local()
