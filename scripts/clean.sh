#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
rm -rf build
find data -name '*.db' -o -name '*.db-*' -o -name '*.log' -o -name '*.pid' | xargs -r rm -f
