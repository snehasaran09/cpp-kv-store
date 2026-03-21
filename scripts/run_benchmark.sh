#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/../build"

if [ ! -f "$BUILD_DIR/load_test" ]; then
    echo "[!] Build first: cd build && cmake .. && make -j$(nproc)"
    exit 1
fi

echo "Running load benchmark..."
cd "$BUILD_DIR"
./load_test