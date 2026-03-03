#!/usr/bin/env python3
"""
Serve version.json and firmware .bin for local OTA testing.
Usage:
  cd www && python serve.py [port]
  Caddy forward to https://api.aldervon.com/device/version in production (append ?id=... by firmware).
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

def get_newest_v_bin():
    """
    Return the filename (e.g. v16.bin) of the newest v*.bin in WWW_DIR by mtime,
    or None if none found.
    """
    import glob
    pattern = os.path.join(WWW_DIR, "v*.bin")
    bins = glob.glob(pattern)
    if not bins:
        return None
    newest = max(bins, key=os.path.getmtime)
    return os.path.basename(newest)


def get_git_describe():
    """
    Returns the output of `git describe --long`, or None if unavailable.
    """
    import subprocess
    try:
        result = subprocess.run(
            ["git", "describe", "--long"], 
            cwd=WWW_DIR,
            stdout=subprocess.PIPE, 
            stderr=subprocess.PIPE, 
            text=True,
            check=True
        )
        return result.stdout.strip()
    except Exception:
        return None


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
        path = self.path.split("?")[0].rstrip("/")
        if path in ("/version", "/version.json"):
            body = make_version_json(self._base_url, self._bin_path).encode()
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return
        if path == "/device/checkin":
            # Firmware expects JSON body with api_base, token, and optional firmware info.
            api_base = "https://api.aldervon.com"
            token = "dev-%s" % (self.path.split("id=")[-1].split("&")[0][:12] if "id=" in self.path else "local")
            v_bin = get_newest_v_bin()
            firmware_url = api_base.rstrip("/") + "/" + v_bin
            payload = {
                "api_base": api_base,
                "token": token,
                "firmware": {
                    "latest_version": get_git_describe(),
                    "url": firmware_url,
                    "checksum": "0" * 64,
                },
            }
            body = json.dumps(payload).encode()
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Authorization", "Bearer %s" % token)
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return
        super().do_GET()

    def do_POST(self):
        path = self.path.split("?")[0].rstrip("/")
        if path == "/bulk":
            content_len = int(self.headers.get("Content-Length", 0))
            body = self.rfile.read(min(content_len, 4096)) if content_len else b""
            try:
                j = json.loads(body.decode()) if body else {}
                device_id = j.get("device_id", "?")
                data = j.get("data", "")
                sys.stderr.write("bulk: device_id=%s data=%r\n" % (device_id, data))
            except Exception:
                pass
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", "2")
            self.end_headers()
            self.wfile.write(b"{}")
            return
        self.send_response(404)
        self.end_headers()

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
        print("  Check-in:       %s/device/checkin?id=xxx  (JSON: api_base, token; + Authorization header)" % base_url)
        print("Set CONFIG_VERSION_BASE_URL to %s/version" % base_url)
        print("Ctrl+C to stop")
        httpd.serve_forever()
    return 0


if __name__ == "__main__":
    sys.exit(main())
