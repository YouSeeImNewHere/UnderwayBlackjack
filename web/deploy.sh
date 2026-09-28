#!/bin/sh
# Rebuilds the web version and pushes it to the homelab deployment.
# Usage: sh web/deploy.sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

sh "$SCRIPT_DIR/build.sh"

echo "Pushing to homelab..."
scp -rq "$SCRIPT_DIR/dist" homelab:~/underway-blackjack/

echo "Deployed: https://homelab.taileb5ffb.ts.net:8443"
