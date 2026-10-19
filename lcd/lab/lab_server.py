"""Freenove Kit Lab server: the static lab pages plus a tiny serial relay.

POST /serial          body = raw text, delivered to the lab's simulated Serial input as-is
GET  /serial?since=N  -> {"next": M, "items": [...]}  (the lab polls this)

Run: python3 lab_server.py [port]   (default 8602)
"""
import json
import sys
import threading
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlparse

ROOT = Path(__file__).resolve().parent
KEEP = 2000                      # items kept for late pollers
items, base, lock = [], 0, threading.Lock()


class Handler(SimpleHTTPRequestHandler):
    def __init__(self, *a, **kw):
        super().__init__(*a, directory=str(ROOT), **kw)

    def log_message(self, *a):   # quiet: the lab polls several times a second
        pass

    def do_GET(self):
        u = urlparse(self.path)
        if u.path != "/serial":
            return super().do_GET()
        since = int(parse_qs(u.query).get("since", ["0"])[0] or 0)
        with lock:
            start = max(since, base) - base
            body = {"next": base + len(items), "items": items[start:]}
        self._json(body)

    def do_POST(self):
        global base
        if urlparse(self.path).path != "/serial":
            return self.send_error(404)
        n = int(self.headers.get("Content-Length", 0))
        if n > 4096:
            return self.send_error(413)
        text = self.rfile.read(n).decode("utf-8", "replace")
        with lock:
            items.append(text)
            if len(items) > KEEP:
                drop = len(items) - KEEP
                del items[:drop]
                base += drop
        self._json({"ok": True})

    def do_OPTIONS(self):           # the face builder on GitHub Pages may POST here from another origin
        self.send_response(204)
        self._cors()
        self.end_headers()

    def _cors(self):
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")

    def _json(self, obj):
        data = json.dumps(obj).encode()
        self.send_response(200)
        self._cors()
        self.send_header("Content-Type", "application/json")
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)


if __name__ == "__main__":
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8602
    ThreadingHTTPServer(("0.0.0.0", port), Handler).serve_forever()
