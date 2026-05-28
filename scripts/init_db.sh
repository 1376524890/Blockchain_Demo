#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p data/node1 data/node2 data/node3 data/node4
if [[ ! -x build/rbft_node ]]; then
  ./scripts/build.sh
fi
for i in 1 2 3 4; do
  ./build/rbft_node --config "config/node${i}.json" --init-db
done
