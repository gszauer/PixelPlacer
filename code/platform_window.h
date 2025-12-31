#ifndef _H_PLATFORM_WINDOW_
#define _H_PLATFORM_WINDOW_

#include "types.h"
#include "widget.h"  // For MouseButton, KeyMods
#include <functional>
#include <string>

// Abstract window interface for cross-platform windowing
// Platform-specific implementations: X11Window (Linux), Win32Window (Windows), etc.
struct PlatformWindow {
    virtual ~PlatformWindow() = default;

    // Window lifecycle
    virtual bool create(u32 w, u32 h, const char* title) = 0;
    virtual void destroy() = 0;

    // Window properties
    virtual void setTitle(const char* title) = 0;
    virtual void resize(u32 w, u32 h) = 0;
    virtual u32 getWidth() const = 0;
    virtual u32 getHeight() const = 0;
    virtual f32 getDpiScale() const = 0;
    virtual void getScreenSize(u32& outWidth, u32& outHeight) const = 0;
    virtual void setMinSize(u32 minW, u32 minH) = 0;
    virtual void centerOnScreen() = 0;

    // Window decorations and controls
    virtual void setDecorated(bool decorated) = 0;
    virtual void startDrag(i32 rootX, i32 rootY) = 0;
    virtual void startResize(i32 direction) = 0;
    virtual void minimize() = 0;
    virtual void maximize() = 0;
    virtual void restore() = 0;
    virtual void toggleMaximize() = 0;
    virtual bool isMaximized() const = 0;

    // Cursor management
    virtual void setCursor(i32 resizeDirection) = 0;

    // Rendering
    virtual void present(const u32* pixels, u32 w, u32 h) = 0;
    virtual void presentPartial(const u32* pixels, u32 w, u32 h, i32 dx, i32 dy, i32 dw, i32 dh) {
        // Default: full present (platforms can override for optimization)
        present(pixels, w, h);
    }

    // Event processing - processes all pending events and calls callbacks
    // Returns false if window should close
    virtual bool processEvents() = 0;

    // Event callbacks (set by Application)
    std::function<void()> onCloseRequested;
    std::function<void(i32 keyCode, i32 scanCode, KeyMods mods, bool repeat)> onKeyDown;
    std::function<void(i32 keyCode, i32 scanCode, KeyMods mods)> onKeyUp;
    std::function<void(const char* text)> onTextInput;
    std::function<void(i32 x, i32 y, MouseButton button)> onMouseDown;
    std::function<void(i32 x, i32 y, MouseButton button)> onMouseUp;
    std::function<void(i32 x, i32 y)> onMouseMove;
    std::function<void(i32 x, i32 y, i32 deltaY)> onMouseWheel;
    std::function<void(u32 width, u32 height)> onResize;
    std::function<void()> onExpose;
    std::function<void(const std::string& path)> onFileDrop;

    // Resize direction constants (matching _NET_WM_MOVERESIZE)
    static constexpr i32 RESIZE_TOPLEFT = 0;
    static constexpr i32 RESIZE_TOP = 1;
    static constexpr i32 RESIZE_TOPRIGHT = 2;
    static constexpr i32 RESIZE_RIGHT = 3;
    static constexpr i32 RESIZE_BOTTOMRIGHT = 4;
    static constexpr i32 RESIZE_BOTTOM = 5;
    static constexpr i32 RESIZE_BOTTOMLEFT = 6;
    static constexpr i32 RESIZE_LEFT = 7;
    static constexpr i32 CURSOR_DEFAULT = -1;
};

#endif
