#ifndef _H_X11_WINDOW_
#define _H_X11_WINDOW_

#include "platform_window.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/Xresource.h>

// Save X11's None value before undefining the macro
// X11 defines None as 0L, which conflicts with our enum values
constexpr long X11_NONE = None;

// Undefine X11's None macro to avoid conflicts with our enum values
#ifdef None
#undef None
#endif

// X11 window wrapper for native Linux windowing
struct X11Window : public PlatformWindow {
    // X11 core resources
    Display* display = nullptr;
    Window window = 0;
    i32 screen = 0;
    GC gc = nullptr;
    Visual* visual = nullptr;
    i32 depth = 0;

    // Image buffer for software rendering
    XImage* image = nullptr;
    u32* imageBuffer = nullptr;
    u32 imageWidth = 0;
    u32 imageHeight = 0;

    // Window close handling
    Atom wmDeleteMessage;
    Atom wmProtocols;

    // XDND (drag and drop) atoms
    Atom xdndAware;
    Atom xdndEnter;
    Atom xdndPosition;
    Atom xdndStatus;
    Atom xdndLeave;
    Atom xdndDrop;
    Atom xdndFinished;
    Atom xdndActionCopy;
    Atom xdndSelection;
    Atom xdndTypeList;
    Atom textUriList;
    Atom textPlain;

    // XDND state
    Window xdndSourceWindow = 0;
    bool xdndWaitingForData = false;

    // Input method for text input
    XIM xim = nullptr;
    XIC xic = nullptr;

    // Window state
    u32 width = 0;
    u32 height = 0;
    f32 dpiScale = 1.0f;
    bool maximized = false;
    bool decorated = true;

    // Stored geometry for restore from maximize
    i32 restoreX = 0;
    i32 restoreY = 0;
    u32 restoreWidth = 0;
    u32 restoreHeight = 0;

    // Non-copyable
    X11Window() = default;
    ~X11Window() override { destroy(); }
    X11Window(const X11Window&) = delete;
    X11Window& operator=(const X11Window&) = delete;

    // PlatformWindow interface implementation
    bool create(u32 w, u32 h, const char* title) override;
    void destroy() override;
    void setTitle(const char* title) override;
    void resize(u32 w, u32 h) override;
    u32 getWidth() const override { return width; }
    u32 getHeight() const override { return height; }
    f32 getDpiScale() const override { return dpiScale; }

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

    // DPI handling
    void updateDpiScale();

    // Drag and drop
    void handleXdndEvent(XEvent& event);

private:
    bool createImageBuffer(u32 w, u32 h);
    void destroyImageBuffer();
    void initXdnd();
    void sendXdndStatus(Window source, bool accept);
    void sendXdndFinished(Window source, bool accepted);
    void requestXdndData();
    void handleSelectionNotify(XEvent& event);
    std::string parseUriList(const char* data, unsigned long length);
};

#endif
