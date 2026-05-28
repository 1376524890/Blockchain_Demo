#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
cd build
cmake ..
make -j"$(nproc)"
