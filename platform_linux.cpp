#include "platform.h"
#include "x11_window.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <chrono>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

namespace Platform {

std::string openFileDialog(const char* title, const char* filters) {
    char buffer[4096] = {0};
    std::string command;

    // Try kdialog first, then zenity
    if (system("which kdialog > /dev/null 2>&1") == 0) {
        command = "kdialog --title \"";
        command += title;
        command += "\" --getopenfilename ~ \"";
        command += filters;
        command += "\" 2>/dev/null";
    } else {
        command = "zenity --title=\"";
        command += title;
        command += "\" --file-selection 2>/dev/null";
    }

    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) return "";

    if (fgets(buffer, sizeof(buffer), pipe)) {
        // Remove trailing newline
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len-1] == '\n') {
            buffer[len-1] = '\0';
        }
    }
    pclose(pipe);

    return std::string(buffer);
}

std::string saveFileDialog(const char* title, const char* defaultName, const char* filters) {
    char buffer[4096] = {0};
    std::string command;

    if (system("which kdialog > /dev/null 2>&1") == 0) {
        command = "kdialog --title \"";
        command += title;
        command += "\" --getsavefilename \"~/";
        command += defaultName;
        command += "\" \"";
        command += filters;
        command += "\" 2>/dev/null";
    } else {
        command = "zenity --title=\"";
        command += title;
        command += "\" --file-selection --save --filename=\"";
        command += defaultName;
        command += "\" 2>/dev/null";
    }

    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) return "";

    if (fgets(buffer, sizeof(buffer), pipe)) {
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len-1] == '\n') {
            buffer[len-1] = '\0';
        }
    }
    pclose(pipe);

    return std::string(buffer);
}

bool confirmDialog(const char* title, const char* message) {
    std::string command;

    if (system("which kdialog > /dev/null 2>&1") == 0) {
        command = "kdialog --title \"";
        command += title;
        command += "\" --yesno \"";
        command += message;
        command += "\" 2>/dev/null";
    } else {
        command = "zenity --title=\"";
        command += title;
        command += "\" --question --text=\"";
        command += message;
        command += "\" 2>/dev/null";
    }

    bool result = system(command.c_str()) == 0;

    return result;
}

void messageBox(const char* title, const char* message) {
    std::string command;

    if (system("which kdialog > /dev/null 2>&1") == 0) {
        command = "kdialog --title \"";
        command += title;
        command += "\" --msgbox \"";
        command += message;
        command += "\" 2>/dev/null";
    } else {
        command = "zenity --title=\"";
        command += title;
        command += "\" --info --text=\"";
        command += message;
        command += "\" 2>/dev/null";
    }

    (void)system(command.c_str());
}

void launchBrowser(const char* url) {
    std::string command = "xdg-open \"";
    command += url;
    command += "\" 2>/dev/null &";
    (void)system(command.c_str());
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
    char buffer[65536] = {0};
    FILE* pipe = popen("xclip -selection clipboard -o 2>/dev/null", "r");
    if (!pipe) return "";

    std::string result;
    while (fgets(buffer, sizeof(buffer), pipe)) {
        result += buffer;
    }
    pclose(pipe);
    return result;
}

void setClipboardText(const std::string& text) {
    std::string command = "echo -n \"";
    // Escape special characters
    for (char c : text) {
        if (c == '"' || c == '\\' || c == '$' || c == '`') {
            command += '\\';
        }
        command += c;
    }
    command += "\" | xclip -selection clipboard 2>/dev/null";
    (void)system(command.c_str());
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
    struct stat buffer;
    return stat(path.c_str(), &buffer) == 0;
}

std::string getFileExtension(const std::string& path) {
    size_t dot = path.rfind('.');
    if (dot == std::string::npos) return "";
    return path.substr(dot);
}

std::string getFileName(const std::string& path) {
    size_t slash = path.rfind('/');
    if (slash == std::string::npos) return path;
    return path.substr(slash + 1);
}

std::string getDirectory(const std::string& path) {
    size_t slash = path.rfind('/');
    if (slash == std::string::npos) return ".";
    return path.substr(0, slash);
}

void sleepMs(u32 ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000;
    nanosleep(&ts, nullptr);
}

std::unique_ptr<PlatformWindow> createWindow() {
    return std::make_unique<X11Window>();
}

}
