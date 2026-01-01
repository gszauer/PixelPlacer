#ifndef _H_WIN32_WINDOW_
#define _H_WIN32_WINDOW_

#include "platform_window.h"

#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#include <mmsystem.h>  // For timeBeginPeriod/timeEndPeriod

// Undefine Windows macros that conflict with our code
#ifdef DELETE
#undef DELETE
#endif
#ifdef COLOR_BACKGROUND
#undef COLOR_BACKGROUND
#endif

#include <string>

// Windows implementation of PlatformWindow using double-buffered GDI
struct Win32Window : public PlatformWindow {
    // Win32 handles
    HWND hwnd = nullptr;
    HDC hdcWindow = nullptr;
    HDC hdcBackBuffer = nullptr;
    HBITMAP hBitmap = nullptr;
    HBITMAP hOldBitmap = nullptr;

    // Back buffer pixel data (direct access via CreateDIBSection)
    u32* backBufferPixels = nullptr;
    u32 backBufferWidth = 0;
    u32 backBufferHeight = 0;

    // Window state
    u32 width = 0;
    u32 height = 0;
    f32 dpiScale = 1.0f;
    bool maximized = false;
    bool decorated = true;

    // Minimum size
    u32 minWidth = 1280;
    u32 minHeight = 800;

    // Stored geometry for restore from maximize
    RECT restoreRect = {0, 0, 0, 0};

    // Track modifier state
    bool shiftDown = false;
    bool ctrlDown = false;
    bool altDown = false;

    // Track last mouse position for history retrieval
    DWORD lastMouseTime = 0;
    i32 lastMouseX = 0;
    i32 lastMouseY = 0;

    // Window class name
    static constexpr const wchar_t* WINDOW_CLASS = L"PixelPlacerWindow";
    static bool classRegistered;

    // Non-copyable
    Win32Window() = default;
    ~Win32Window() override { destroy(); }
    Win32Window(const Win32Window&) = delete;
    Win32Window& operator=(const Win32Window&) = delete;

    // PlatformWindow interface implementation
    bool create(u32 w, u32 h, const char* title) override;
    void destroy() override;
    void setTitle(const char* title) override;
    void resize(u32 w, u32 h) override;
    u32 getWidth() const override { return width; }
    u32 getHeight() const override { return height; }
    f32 getDpiScale() const override { return dpiScale; }
    void getScreenSize(u32& outWidth, u32& outHeight) const override;
    void setMinSize(u32 minW, u32 minH) override;
    void centerOnScreen() override;

    // Window decorations and controls
    void setDecorated(bool decorated) override;
    void startDrag(i32 rootX, i32 rootY) override;
    void startResize(i32 direction) override;
    void minimize() override;
    void maximize() override;
    void restore() override;
    void toggleMaximize() override;
    bool isMaximized() const override { return maximized; }

    // Cursor management
    void setCursor(i32 resizeDirection) override;

    // Rendering
    void present(const u32* pixels, u32 w, u32 h) override;

    // Event processing
    bool processEvents() override;

    // Window procedure callback
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

private:
    bool createBackBuffer(u32 w, u32 h);
    void destroyBackBuffer();
    void updateDpiScale();
    i32 mapVirtualKey(WPARAM vk) const;
    KeyMods getCurrentMods() const;
    void registerWindowClass();
};

#endif
