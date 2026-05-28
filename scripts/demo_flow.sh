#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
base="${BASE_URL:-http://127.0.0.1:8001}"
echo "Demo target: ${base}"
curl -s "${base}/api/node/status" || true
echo
