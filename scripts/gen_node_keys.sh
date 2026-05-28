#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p data/node1 data/node2 data/node3 data/node4
for i in 1 2 3 4; do
  python3 - <<PY
import os, secrets
node="data/node${i}"
os.makedirs(node, exist_ok=True)
open(f"{node}/node.key","w").write(secrets.token_hex(64)+"\n")
open(f"{node}/node.pub","w").write(secrets.token_hex(32)+"\n")
PY
done
echo "Generated demo node key files. The C++ crypto module can replace these with libsodium keys."
