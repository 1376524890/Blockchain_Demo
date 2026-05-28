#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
for i in 1 2 3 4; do
  pid_file="data/node${i}/node.pid"
  if [[ -f "$pid_file" ]]; then
    pid="$(cat "$pid_file")"
    if kill -0 "$pid" 2>/dev/null; then
      kill "$pid" || true
    fi
    rm -f "$pid_file"
    echo "stopped node${i}"
  fi
done
