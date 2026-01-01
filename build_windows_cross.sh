#!/bin/bash

# PixelPlacer Windows cross-compile script (build on Linux for Windows)
# Usage: ./build_windows_cross.sh [debug]
#
# Prerequisites:
#   Ubuntu/Debian: sudo apt install mingw-w64
#   Fedora:        sudo dnf install mingw64-gcc-c++
#   Arch:          sudo pacman -S mingw-w64-gcc

set -e

cd "$(dirname "$0")"

# Check for MinGW cross-compiler
if ! command -v x86_64-w64-mingw32-g++ &> /dev/null; then
    echo "ERROR: MinGW-w64 cross-compiler not found."
    echo ""
    echo "Install it with:"
    echo "  Ubuntu/Debian: sudo apt install mingw-w64"
    echo "  Fedora:        sudo dnf install mingw64-gcc-c++"
    echo "  Arch:          sudo pacman -S mingw-w64-gcc"
    exit 1
fi

# Clean previous build
rm -f pixelplacer.exe pixelplacer_debug.exe

if [ "$1" = "debug" ]; then
    echo "Cross-compiling debug version for Windows..."
    x86_64-w64-mingw32-g++ -std=c++17 -g -O0 -DUNITY_BUILD -D_WIN32 -Wall -Wextra \
        -Wno-unused-parameter \
        code/main.cpp \
        -lgdi32 -luser32 -lshell32 -lcomdlg32 -lole32 -lwinmm \
        -mwindows -static -static-libgcc -static-libstdc++ \
        -o pixelplacer_debug.exe
    echo "Built: pixelplacer_debug.exe"
else
    echo "Cross-compiling release version for Windows..."
    x86_64-w64-mingw32-g++ -std=c++17 -O3 -DNDEBUG -DUNITY_BUILD -D_WIN32 \
        -Wno-unused-parameter \
        code/main.cpp \
        -lgdi32 -luser32 -lshell32 -lcomdlg32 -lole32 -lwinmm \
        -mwindows -static -static-libgcc -static-libstdc++ \
        -o pixelplacer.exe
    echo "Built: pixelplacer.exe"
fi

echo "Done!"
