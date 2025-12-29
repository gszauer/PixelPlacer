#!/bin/bash

# PixelPlacer WebAssembly Build Script
# Requires Emscripten SDK (emcc) to be installed and activated

set -e

BUILD_TYPE="${1:-release}"
OUTPUT_DIR="www"

# Create output directory
mkdir -p "$OUTPUT_DIR"

# Common flags
COMMON_FLAGS=(
    -std=c++17
    -DUNITY_BUILD
    -D__EMSCRIPTEN__
    -s WASM=1
    -s ALLOW_MEMORY_GROWTH=1
    -s INITIAL_MEMORY=536870912       # 512MB initial
    -s MAXIMUM_MEMORY=2147483648      # 2GB max
    -s STACK_SIZE=8388608             # 8MB stack (default is 64KB)
    -s ASYNCIFY=1
    -s ASYNCIFY_STACK_SIZE=65536      # 64KB asyncify stack
    -s "ASYNCIFY_IMPORTS=['emscripten_sleep','js_open_file_dialog','js_save_file_dialog','js_get_clipboard_text','js_set_clipboard_text']"
    -s "EXPORTED_RUNTIME_METHODS=['ccall','cwrap','UTF8ToString','stringToUTF8','stringToNewUTF8']"
    -s "EXPORTED_FUNCTIONS=['_main','_malloc','_free','_wasm_push_mouse_event','_wasm_push_key_event','_wasm_push_resize_event','_wasm_push_text_input','_wasm_push_file_drop','_wasm_push_wheel_event','_wasm_receive_file_data','_wasm_cancel_file_dialog']"
    -s NO_EXIT_RUNTIME=1
    -s MODULARIZE=0
    --shell-file "code/shell.html"
)

if [ "$BUILD_TYPE" = "debug" ]; then
    echo "Building WebAssembly (debug)..."
    emcc code/main.cpp -o "${OUTPUT_DIR}/pixelplacer.html" \
        "${COMMON_FLAGS[@]}" \
        -O0 \
        -g3 \
        -s ASSERTIONS=2 \
        -s SAFE_HEAP=1 \
        -s STACK_OVERFLOW_CHECK=2
    mv "${OUTPUT_DIR}/pixelplacer.html" "${OUTPUT_DIR}/index.html"
    echo "Debug build complete: ${OUTPUT_DIR}/index.html"
else
    echo "Building WebAssembly (release)..."
    emcc code/main.cpp -o "${OUTPUT_DIR}/pixelplacer.html" \
        "${COMMON_FLAGS[@]}" \
        -O2 \
        -s ASSERTIONS=0
    mv "${OUTPUT_DIR}/pixelplacer.html" "${OUTPUT_DIR}/index.html"
    echo "Release build complete: ${OUTPUT_DIR}/index.html"
fi

echo ""
echo "To test locally:"
echo "  cd ${OUTPUT_DIR} && python3 -m http.server 8080"
echo "  Then open http://localhost:8080"
