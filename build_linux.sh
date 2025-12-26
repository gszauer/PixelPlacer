#!/bin/bash

# SimplePaint Linux build script
# Usage: ./build_linux.sh [debug]

set -e

cd "$(dirname "$0")"

# Clean previous build
rm -f simplepaint simplepaint_debug

if [ "$1" = "debug" ]; then
    echo "Building debug version..."
    g++ -std=c++17 -g -O0 -DUNITY_BUILD -Wall -Wextra \
        main.cpp \
        -lX11 \
        -o simplepaint_debug
    echo "Built: simplepaint_debug"
else
    echo "Building release version..."
    g++ -std=c++17 -O3 -DNDEBUG -DUNITY_BUILD \
        main.cpp \
        -lX11 \
        -o simplepaint
    echo "Built: simplepaint"
fi

echo "Done!"
