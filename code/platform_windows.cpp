#include "platform.h"
#include "win32_window.h"

// windows.h already included via win32_window.h
#include <commdlg.h>
#include <shlobj.h>
#include <shellapi.h>
#include <chrono>
#include <fstream>
#include <cstdio>

namespace Platform {

// Helper to build filter string for Windows file dialogs
// Converts "*.png *.jpg" format to "Supported Files\0*.png;*.jpg\0All Files\0*.*\0\0"
static std::wstring buildFilterString(const char* filters) {
    std::wstring result;

    // "Supported Files" label
    result += L"Supported Files";
    result += L'\0';

    // Convert filter patterns
    std::string filterStr(filters);
    std::wstring patterns;
    size_t pos = 0;
    while (pos < filterStr.size()) {
        // Skip whitespace
        while (pos < filterStr.size() && filterStr[pos] == ' ') pos++;
        if (pos >= filterStr.size()) break;

        // Find end of pattern
        size_t end = filterStr.find(' ', pos);
        if (end == std::string::npos) end = filterStr.size();

        std::string pattern = filterStr.substr(pos, end - pos);
        if (!pattern.empty()) {
            if (!patterns.empty()) patterns += L';';
            // Convert to wide
            i32 len = MultiByteToWideChar(CP_UTF8, 0, pattern.c_str(), -1, nullptr, 0);
            std::wstring wpattern(len - 1, 0);
            MultiByteToWideChar(CP_UTF8, 0, pattern.c_str(), -1, &wpattern[0], len);
            patterns += wpattern;
        }
        pos = end;
    }

    result += patterns;
    result += L'\0';

    // "All Files" option
    result += L"All Files";
    result += L'\0';
    result += L"*.*";
    result += L'\0';

    // Double null terminator
    result += L'\0';

    return result;
}

std::string openFileDialog(const char* title, const char* filters) {
    wchar_t filename[MAX_PATH] = {0};

    // Convert title to wide
    i32 titleLen = MultiByteToWideChar(CP_UTF8, 0, title, -1, nullptr, 0);
    std::wstring wideTitle(titleLen, 0);
    MultiByteToWideChar(CP_UTF8, 0, title, -1, &wideTitle[0], titleLen);

    // Build filter string
    std::wstring filterStr = buildFilterString(filters);

    OPENFILENAMEW ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = wideTitle.c_str();
    ofn.lpstrFilter = filterStr.c_str();
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameW(&ofn)) {
        // Convert to UTF-8
        i32 len = WideCharToMultiByte(CP_UTF8, 0, filename, -1, nullptr, 0, nullptr, nullptr);
        std::string result(len - 1, 0);
        WideCharToMultiByte(CP_UTF8, 0, filename, -1, &result[0], len, nullptr, nullptr);
        return result;
    }
    return "";
}

std::string saveFileDialog(const char* title, const char* defaultName, const char* filters) {
    wchar_t filename[MAX_PATH] = {0};

    // Convert default filename to wide and copy to buffer
    i32 defLen = MultiByteToWideChar(CP_UTF8, 0, defaultName, -1, nullptr, 0);
    MultiByteToWideChar(CP_UTF8, 0, defaultName, -1, filename, (std::min)(defLen, static_cast<i32>(MAX_PATH)));

    // Convert title to wide
    i32 titleLen = MultiByteToWideChar(CP_UTF8, 0, title, -1, nullptr, 0);
    std::wstring wideTitle(titleLen, 0);
    MultiByteToWideChar(CP_UTF8, 0, title, -1, &wideTitle[0], titleLen);

    // Build filter string
    std::wstring filterStr = buildFilterString(filters);

    OPENFILENAMEW ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = wideTitle.c_str();
    ofn.lpstrFilter = filterStr.c_str();
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetSaveFileNameW(&ofn)) {
        // Convert to UTF-8
        i32 len = WideCharToMultiByte(CP_UTF8, 0, filename, -1, nullptr, 0, nullptr, nullptr);
        std::string result(len - 1, 0);
        WideCharToMultiByte(CP_UTF8, 0, filename, -1, &result[0], len, nullptr, nullptr);
        return result;
    }
    return "";
}

bool confirmDialog(const char* title, const char* message) {
    // Convert to wide strings
    i32 titleLen = MultiByteToWideChar(CP_UTF8, 0, title, -1, nullptr, 0);
    i32 msgLen = MultiByteToWideChar(CP_UTF8, 0, message, -1, nullptr, 0);
    std::wstring wideTitle(titleLen, 0);
    std::wstring wideMsg(msgLen, 0);
    MultiByteToWideChar(CP_UTF8, 0, title, -1, &wideTitle[0], titleLen);
    MultiByteToWideChar(CP_UTF8, 0, message, -1, &wideMsg[0], msgLen);

    return MessageBoxW(nullptr, wideMsg.c_str(), wideTitle.c_str(),
                       MB_YESNO | MB_ICONQUESTION) == IDYES;
}

void messageBox(const char* title, const char* message) {
    // Convert to wide strings
    i32 titleLen = MultiByteToWideChar(CP_UTF8, 0, title, -1, nullptr, 0);
    i32 msgLen = MultiByteToWideChar(CP_UTF8, 0, message, -1, nullptr, 0);
    std::wstring wideTitle(titleLen, 0);
    std::wstring wideMsg(msgLen, 0);
    MultiByteToWideChar(CP_UTF8, 0, title, -1, &wideTitle[0], titleLen);
    MultiByteToWideChar(CP_UTF8, 0, message, -1, &wideMsg[0], msgLen);

    MessageBoxW(nullptr, wideMsg.c_str(), wideTitle.c_str(), MB_OK | MB_ICONINFORMATION);
}

void launchBrowser(const char* url) {
    ShellExecuteA(nullptr, "open", url, nullptr, nullptr, SW_SHOWNORMAL);
}

u64 getMilliseconds() {
    auto now = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
    return static_cast<u64>(ms.count());
}

u64 getMicroseconds() {
    auto now = std::chrono::steady_clock::now();
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch());
    return static_cast<u64>(us.count());
}

std::string getClipboardText() {
    if (!OpenClipboard(nullptr)) return "";

    std::string result;
    HANDLE hData = GetClipboardData(CF_UNICODETEXT);
    if (hData) {
        wchar_t* pData = static_cast<wchar_t*>(GlobalLock(hData));
        if (pData) {
            // Convert UTF-16 to UTF-8
            i32 len = WideCharToMultiByte(CP_UTF8, 0, pData, -1, nullptr, 0, nullptr, nullptr);
            if (len > 1) {
                result.resize(len - 1);
                WideCharToMultiByte(CP_UTF8, 0, pData, -1, &result[0], len, nullptr, nullptr);
            }
            GlobalUnlock(hData);
        }
    }
    CloseClipboard();
    return result;
}

void setClipboardText(const std::string& text) {
    if (!OpenClipboard(nullptr)) return;
    EmptyClipboard();

    // Convert UTF-8 to UTF-16
    i32 len = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len * sizeof(wchar_t));
    if (hMem) {
        wchar_t* pData = static_cast<wchar_t*>(GlobalLock(hMem));
        if (pData) {
            MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, pData, len);
            GlobalUnlock(hMem);
            SetClipboardData(CF_UNICODETEXT, hMem);
        } else {
            GlobalFree(hMem);
        }
    }
    CloseClipboard();
}

std::vector<u8> readFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return {};

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<u8> buffer(size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        return {};
    }

    return buffer;
}

bool writeFile(const std::string& path, const u8* data, size_t size) {
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    file.write(reinterpret_cast<const char*>(data), size);
    return file.good();
}

bool fileExists(const std::string& path) {
    DWORD attrs = GetFileAttributesA(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
}

std::string getFileExtension(const std::string& path) {
    size_t dot = path.rfind('.');
    if (dot == std::string::npos) return "";
    return path.substr(dot);
}

std::string getFileName(const std::string& path) {
    // Handle both forward and back slashes
    size_t slash = path.find_last_of("/\\");
    if (slash == std::string::npos) return path;
    return path.substr(slash + 1);
}

std::string getDirectory(const std::string& path) {
    // Handle both forward and back slashes
    size_t slash = path.find_last_of("/\\");
    if (slash == std::string::npos) return ".";
    return path.substr(0, slash);
}

void sleepMs(u32 ms) {
    Sleep(ms);
}

std::unique_ptr<PlatformWindow> createWindow() {
    // Enable drag and drop support
    OleInitialize(nullptr);

    return std::make_unique<Win32Window>();
}

}
