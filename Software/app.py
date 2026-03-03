"""Cat Herder"""
import logging
import threading
from collections import deque
from pathlib import Path

from flask import Flask, jsonify, request, send_from_directory

# Ring buffer of last 20 log lines for the UI
LOG_BUFFER_MAX = 20
log_buffer = deque(maxlen=LOG_BUFFER_MAX)


class BufferHandler(logging.Handler):
    """Capture log records into the last N lines for the UI."""

    def emit(self, record):
        try:
            msg = self.format(record)
            log_buffer.append(msg)
        except Exception:
            self.handleError(record)


def setup_logging():
    handler = BufferHandler()
    handler.setFormatter(logging.Formatter("%(asctime)s [%(levelname)s] %(message)s"))
    root = logging.getLogger()
    root.addHandler(handler)
    root.setLevel(logging.INFO)
    # Reduce Flask/Werkzeug noise
    logging.getLogger("werkzeug").setLevel(logging.WARNING)


setup_logging()
logger = logging.getLogger(__name__)

from api import FIRMWARE_DIR, api_app, get_all_devices

# App for port 8001: static index.html
web_app = Flask(__name__, static_folder=Path(__file__).resolve().parent)


@web_app.route("/")
def index():
    logger.info("Served index.html")
    return send_from_directory(web_app.static_folder, "index.html")


@web_app.route("/favicon.ico")
def favicon():
    return "", 204


@web_app.route("/logs")
def logs():
    """Last 20 log lines for the UI."""
    r = jsonify({"lines": list(log_buffer)})
    r.headers["Access-Control-Allow-Origin"] = "*"
    return r


@web_app.route("/devices")
def devices():
    """List all devices (device_id, last_seen, version) for the UI."""
    r = jsonify(get_all_devices())
    r.headers["Access-Control-Allow-Origin"] = "*"
    return r


@web_app.route("/firmware/upload", methods=["POST"])
def firmware_upload():
    """Upload a .bin file to .local/firmware/. Filename must end with .bin and contain no path."""
    if "file" not in request.files:
        return jsonify({"error": "no file"}), 400
    f = request.files["file"]
    if not f.filename or f.filename.strip() == "":
        return jsonify({"error": "no filename"}), 400
    name = Path(f.filename).name
    if not name.endswith(".bin"):
        return jsonify({"error": "filename must end with .bin"}), 400
    if ".." in name or "/" in name or "\\" in name:
        return jsonify({"error": "invalid filename"}), 400
    dest = FIRMWARE_DIR / name
    try:
        f.save(str(dest))
        logger.info("Firmware uploaded: %s", name)
        return jsonify({"ok": True, "filename": name})
    except Exception as e:
        logger.exception("Firmware upload failed")
        return jsonify({"error": str(e)}), 500


def run_api():
    api_app.run(host="0.0.0.0", port=8000, use_reloader=False)


def run_web():
    web_app.run(host="0.0.0.0", port=8001, use_reloader=False)


if __name__ == "__main__":
    logger.info("Cat Herder starting (API=8000, Web=8001)")
    t1 = threading.Thread(target=run_api)
    t2 = threading.Thread(target=run_web)
    t1.start()
    t2.start()
    t1.join()
    t2.join()
