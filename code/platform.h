#ifndef _H_PLATFORM_
#define _H_PLATFORM_

#include "types.h"
#include <string>
#include <vector>
#include <functional>
#include <memory>

struct PlatformWindow;  // Forward declaration

namespace Platform {
    // File dialog callbacks
    using OpenFileCallback = std::function<void(const std::string& filePath, std::vector<u8> fileData)>;
    using SaveFileCallback = std::function<void(const std::string& filePath)>;
    using YesNoCallback = std::function<void(bool result)>;

    // File dialogs (blocking - returns immediately with result)
    std::string openFileDialog(const char* title, const char* filters);
    std::string saveFileDialog(const char* title, const char* defaultName, const char* filters);
    bool confirmDialog(const char* title, const char* message);
    void messageBox(const char* title, const char* message);

    // URL opening
    void launchBrowser(const char* url);

    // Time
    u64 getMilliseconds();
    u64 getMicroseconds();

    // Clipboard
    std::string getClipboardText();
    void setClipboardText(const std::string& text);

    // File I/O utilities
    std::vector<u8> readFile(const std::string& path);
    bool writeFile(const std::string& path, const u8* data, size_t size);
    bool fileExists(const std::string& path);
    std::string getFileExtension(const std::string& path);
    std::string getFileName(const std::string& path);
    std::string getDirectory(const std::string& path);

    // Window creation (platform-specific implementation)
    std::unique_ptr<PlatformWindow> createWindow();

    // Time utilities
    void sleepMs(u32 ms);
}

#endif
