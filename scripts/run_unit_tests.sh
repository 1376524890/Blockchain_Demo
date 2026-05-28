#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
if [[ ! -x build/rbft_unit_tests ]]; then
  ./scripts/build.sh
fi
./build/rbft_unit_tests
