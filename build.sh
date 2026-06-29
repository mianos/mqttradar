#!/usr/bin/env bash
# Build wrapper. Runs from the repo root regardless of where it's invoked, and
# uses the ESP-IDF pointed to by $IDF_PATH (set it, or source export.sh first).
set -e
cd "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

: "${IDF_PATH:?IDF_PATH is not set — point it at your esp-idf checkout or run '. \$IDF_PATH/export.sh' first}"
# Bring idf.py and the toolchain onto PATH if they aren't already.
command -v idf.py >/dev/null 2>&1 || . "$IDF_PATH/export.sh" >/dev/null

idf.py build
