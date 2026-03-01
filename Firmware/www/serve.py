#!/usr/bin/env python3
"""
Serve version.json and firmware .bin for local OTA testing.
Usage:
  cd www && python serve.py [port]
  Set CONFIG_VERSION_BASE_URL to http://<your-ip>:8080/version (no .json; we serve JSON at that path).
  Or use https://api.aldervon.com/device/version in production (append ?id=... by firmware).
"""
import argparse
import hashlib
import http.server
import json
import os
import socket
import sys

PORT = 8080
WWW_DIR = os.path.dirname(os.path.abspath(__file__))


def get_local_ip():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(("8.8.8.8", 80))
        return s.getsockname()[0]
    except Exception:
        return "localhost"
    finally:
        s.close()


def make_version_json(base_url, bin_path):
    with open(os.path.join(WWW_DIR, "version.json")) as f:
        j = json.load(f)
    j["url"] = base_url.rstrip("/") + "/firmware.bin"
    if os.path.isfile(bin_path):
        with open(bin_path, "rb") as f:
            j["sha256"] = hashlib.sha256(f.read()).hexdigest()
    return json.dumps(j)


class OTAHandler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, base_url=None, bin_path=None, **kwargs):
        self._base_url = base_url
        self._bin_path = bin_path
        super().__init__(*args, **kwargs)

    def do_GET(self):
        if self.path.rstrip("/") in ("/version", "/version.json"):
            body = make_version_json(self._base_url, self._bin_path).encode()
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return
        super().do_GET()

    def log_message(self, format, *args):
        sys.stderr.write("%s - - [%s] %s\n" % (self.address_string(), self.log_date_time_string(), format % args))


def main():
    global PORT
    ap = argparse.ArgumentParser(description="Serve OTA version.json and firmware.bin")
    ap.add_argument("port", nargs="?", type=int, default=PORT, help="Port (default %d)" % PORT)
    ap.add_argument("--url-base", action="store_true",
                    help="Print version.json with url pointing at this host (for copying to server)")
    args = ap.parse_args()
    PORT = args.port

    os.chdir(WWW_DIR)
    bin_path = os.path.join(WWW_DIR, "firmware.bin")
    ip = get_local_ip()
    base_url = "http://%s:%d" % (ip, PORT)

    if args.url_base:
        j = json.loads(make_version_json(base_url, bin_path))
        print(json.dumps(j, indent=2))
        return 0

    if not os.path.isfile(bin_path):
        print("Warning: www/firmware.bin not found. Copy build .bin here for OTA test.", file=sys.stderr)
        print("  cp build/aldervend_adapter.bin www/firmware.bin", file=sys.stderr)

    def handler(*args, **kwargs):
        OTAHandler(*args, base_url=base_url, bin_path=bin_path, **kwargs)

    with http.server.HTTPServer(("", PORT), handler) as httpd:
        print("Serving %s at %s" % (WWW_DIR, base_url))
        print("  Version (JSON): %s/version  (append ?id=xxx in firmware)" % base_url)
        print("  Firmware:       %s/firmware.bin" % base_url)
        print("Set CONFIG_VERSION_BASE_URL to %s/version" % base_url)
        print("Ctrl+C to stop")
        httpd.serve_forever()
    return 0


if __name__ == "__main__":
    sys.exit(main())
