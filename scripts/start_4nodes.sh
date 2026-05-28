#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
if [[ ! -x build/rbft_node ]]; then
  ./scripts/build.sh
fi
mkdir -p data/node1 data/node2 data/node3 data/node4
for i in 1 2 3 4; do
  node="node${i}"
  if [[ -f "data/${node}/node.pid" ]] && kill -0 "$(cat "data/${node}/node.pid")" 2>/dev/null; then
    echo "${node} already running"
    continue
  fi
  nohup ./build/rbft_node --config "config/${node}.json" > "data/${node}/node.log" 2>&1 &
  echo $! > "data/${node}/node.pid"
  echo "started ${node} pid $(cat "data/${node}/node.pid")"
done
