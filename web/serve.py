#!/usr/bin/env python3
"""Local test server for web/dist. Serves HTTP/1.1 (not the http.server
module's default HTTP/1.0), which service worker registration requires --
Chromium refuses to install a service worker fetched over HTTP/1.0 with an
opaque "unknown error occurred when fetching the script" failure.

Usage: python serve.py [port]
"""
import http.server
import functools
import os
import sys

port = int(sys.argv[1]) if len(sys.argv) > 1 else 8080
directory = os.path.join(os.path.dirname(os.path.abspath(__file__)), "dist")


class Handler(http.server.SimpleHTTPRequestHandler):
    protocol_version = "HTTP/1.1"


handler = functools.partial(Handler, directory=directory)
httpd = http.server.ThreadingHTTPServer(("0.0.0.0", port), handler)
print(f"Serving {directory} at http://0.0.0.0:{port} (HTTP/1.1)")
httpd.serve_forever()
