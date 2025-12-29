#include "platform.h"
#include "wasm_window.h"
#include <emscripten.h>
#include <emscripten/html5.h>
#include <cstdio>
#include <cstring>
#include <chrono>

// Global storage for pending file data from JavaScript
static std::vector<u8> g_pendingFileData;
static std::string g_pendingFilePath;
static bool g_fileDataReady = false;

// C function called by JavaScript to deliver file data
extern "C" {
    EMSCRIPTEN_KEEPALIVE
    void wasm_receive_file_data(const u8* data, i32 size, const char* filename) {
        g_pendingFileData.assign(data, data + size);
        g_pendingFilePath = filename ? filename : "";
        g_fileDataReady = true;
    }

    EMSCRIPTEN_KEEPALIVE
    void wasm_cancel_file_dialog() {
        g_pendingFileData.clear();
        g_pendingFilePath.clear();
        g_fileDataReady = true;
    }
}

namespace Platform {

std::string openFileDialog(const char* title, const char* filters) {
    (void)title;

    // Reset state
    g_pendingFileData.clear();
    g_pendingFilePath.clear();
    g_fileDataReady = false;

    // Trigger file input click directly
    EM_ASM({
        const input = document.getElementById("file-input");
        if (!input) {
            console.error("file-input element not found");
            Module._wasm_cancel_file_dialog();
            return;
        }

        // Set accept filter
        const filterStr = UTF8ToString($0);
        if (filterStr) {
            const accept = filterStr.split(" ").map(f => f.replace("*", "")).join(",");
            input.accept = accept;
        }

        let resolved = false;

        const handleChange = async (e) => {
            if (resolved) return;
            resolved = true;
            input.removeEventListener("change", handleChange);

            const file = e.target.files[0];
            if (file) {
                try {
                    const arrayBuffer = await file.arrayBuffer();
                    const data = new Uint8Array(arrayBuffer);

                    // Allocate WASM memory and copy data
                    const ptr = Module._malloc(data.length);
                    HEAPU8.set(data, ptr);

                    // Call C++ to receive the data
                    const namePtr = stringToNewUTF8(file.name);
                    Module._wasm_receive_file_data(ptr, data.length, namePtr);
                    Module._free(namePtr);
                    Module._free(ptr);
                } catch (err) {
                    console.error("File read error:", err);
                    Module._wasm_cancel_file_dialog();
                }
            } else {
                Module._wasm_cancel_file_dialog();
            }
            input.value = "";
        };

        input.addEventListener("change", handleChange);

        // Handle cancel via focus return
        const handleCancel = () => {
            setTimeout(() => {
                if (!resolved && input.files.length === 0) {
                    resolved = true;
                    input.removeEventListener("change", handleChange);
                    Module._wasm_cancel_file_dialog();
                }
            }, 300);
        };

        window.addEventListener("focus", handleCancel, { once: true });
        input.click();
    }, filters);

    // Wait for file data (ASYNCIFY handles the async wait)
    while (!g_fileDataReady) {
        emscripten_sleep(10);
    }

    // Return the filename (caller should use getLastFileData() to get actual data)
    return g_pendingFilePath;
}

// Get the data from the last opened file
std::vector<u8> getLastOpenedFileData() {
    return std::move(g_pendingFileData);
}

std::string saveFileDialog(const char* title, const char* defaultName, const char* filters) {
    (void)title;
    (void)filters;
    // In browser, we can't do a save dialog - we just return a marker path
    // The actual download happens in writeFile when the path starts with special prefix
    return std::string("__download__:") + defaultName;
}

bool confirmDialog(const char* title, const char* message) {
    return EM_ASM_INT({
        return confirm(UTF8ToString($0) + "\n\n" + UTF8ToString($1)) ? 1 : 0;
    }, title, message) != 0;
}

void messageBox(const char* title, const char* message) {
    EM_ASM({
        alert(UTF8ToString($0) + "\n\n" + UTF8ToString($1));
    }, title, message);
}

void launchBrowser(const char* url) {
    EM_ASM({
        window.open(UTF8ToString($0), "_blank");
    }, url);
}

u64 getMilliseconds() {
    return static_cast<u64>(EM_ASM_DOUBLE({ return performance.now(); }));
}

u64 getMicroseconds() {
    return static_cast<u64>(EM_ASM_DOUBLE({ return performance.now() * 1000.0; }));
}

std::string getClipboardText() {
    char* textPtr = (char*)EM_ASM_INT({
        return Asyncify.handleAsync(async () => {
            try {
                const text = await navigator.clipboard.readText();
                return stringToNewUTF8(text);
            } catch (e) {
                console.warn("Clipboard read failed:", e);
                return 0;
            }
        });
    });

    std::string result;
    if (textPtr) {
        result = textPtr;
        free(textPtr);
    }
    return result;
}

void setClipboardText(const std::string& text) {
    EM_ASM({
        navigator.clipboard.writeText(UTF8ToString($0)).catch(e => {
            console.warn("Clipboard write failed:", e);
        });
    }, text.c_str());
}

std::vector<u8> readFile(const std::string& path) {
    // Check if this is data from the file dialog
    if (!g_pendingFilePath.empty() && g_pendingFilePath == path) {
        return std::move(g_pendingFileData);
    }
    // For other reads, return empty (we don't have a filesystem)
    return {};
}

bool writeFile(const std::string& path, const u8* data, size_t size) {
    // Check if this is a download request
    if (path.rfind("__download__:", 0) == 0) {
        std::string filename = path.substr(13);  // Remove "__download__:" prefix
        EM_ASM({
            const data = HEAPU8.slice($0, $0 + $1);
            const blob = new Blob([data], { type: "application/octet-stream" });
            const url = URL.createObjectURL(blob);

            const anchor = document.getElementById("download-anchor");
            anchor.href = url;
            anchor.download = UTF8ToString($2);
            anchor.click();

            setTimeout(() => URL.revokeObjectURL(url), 1000);
        }, data, size, filename.c_str());
        return true;
    }

    fprintf(stderr, "writeFile: Cannot write '%s' - no filesystem in WASM\n", path.c_str());
    return false;
}

bool fileExists(const std::string& path) {
    (void)path;
    return false;  // No filesystem in WASM
}

std::string getFileExtension(const std::string& path) {
    size_t dot = path.rfind('.');
    if (dot == std::string::npos) return "";
    return path.substr(dot);
}

std::string getFileName(const std::string& path) {
    size_t slash = path.rfind('/');
    size_t colon = path.rfind(':');  // Handle __download__:filename
    size_t pos = std::max(slash, colon);
    if (pos == std::string::npos) return path;
    return path.substr(pos + 1);
}

std::string getDirectory(const std::string& path) {
    size_t slash = path.rfind('/');
    if (slash == std::string::npos) return ".";
    return path.substr(0, slash);
}

void sleepMs(u32 ms) {
    if (ms > 0) {
        emscripten_sleep(ms);
    }
}

std::unique_ptr<PlatformWindow> createWindow() {
    return std::make_unique<WasmWindow>();
}

}
