#include "x11_window.h"
#include <X11/cursorfont.h>
#include <X11/Xatom.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

bool X11Window::create(u32 w, u32 h, const char* title) {
    // Open display connection
    display = XOpenDisplay(nullptr);
    if (!display) {
        fprintf(stderr, "Failed to open X display\n");
        return false;
    }

    screen = DefaultScreen(display);
    visual = DefaultVisual(display, screen);
    depth = DefaultDepth(display, screen);

    // Get DPI scale before creating window
    updateDpiScale();

    // Auto-calculate window size if w or h is 0
    // Use half of screen size, minimum 1280x800
    if (w == 0 || h == 0) {
        u32 screenW = static_cast<u32>(XDisplayWidth(display, screen));
        u32 screenH = static_cast<u32>(XDisplayHeight(display, screen));
        constexpr u32 MIN_WIDTH = 1280;
        constexpr u32 MIN_HEIGHT = 800;
        w = std::max(MIN_WIDTH, screenW / 2);
        h = std::max(MIN_HEIGHT, screenH / 2);
        fprintf(stderr, "Screen: %dx%d, Window: %dx%d, DPI Scale: %.2f\n",
                screenW, screenH, w, h, dpiScale);
    }

    // Create window
    Window root = RootWindow(display, screen);
    XSetWindowAttributes attrs;
    attrs.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask |
                       ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
                       StructureNotifyMask | FocusChangeMask;
    attrs.background_pixel = BlackPixel(display, screen);

    window = XCreateWindow(
        display, root,
        0, 0, w, h,
        0,                      // border width
        depth,                  // depth
        InputOutput,            // class
        visual,
        CWEventMask | CWBackPixel,
        &attrs
    );

    if (!window) {
        fprintf(stderr, "Failed to create X window\n");
        XCloseDisplay(display);
        display = nullptr;
        return false;
    }

    // Set window title
    XStoreName(display, window, title);

    // Handle window close events
    wmDeleteMessage = XInternAtom(display, "WM_DELETE_WINDOW", False);
    wmProtocols = XInternAtom(display, "WM_PROTOCOLS", False);
    XSetWMProtocols(display, window, &wmDeleteMessage, 1);

    // Create graphics context
    gc = XCreateGC(display, window, 0, nullptr);

    // Setup input method for text input
    xim = XOpenIM(display, nullptr, nullptr, nullptr);
    if (xim) {
        xic = XCreateIC(xim,
            XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
            XNClientWindow, window,
            XNFocusWindow, window,
            nullptr);
    }

    // Initialize drag and drop support
    initXdnd();

    // Store dimensions
    width = w;
    height = h;

    // Set minimum size (1280x800)
    XSizeHints* hints = XAllocSizeHints();
    if (hints) {
        hints->flags = PMinSize;
        hints->min_width = 1280;
        hints->min_height = 800;
        XSetWMNormalHints(display, window, hints);
        XFree(hints);
    }

    // Center window on screen before mapping
    u32 screenW = static_cast<u32>(XDisplayWidth(display, screen));
    u32 screenH = static_cast<u32>(XDisplayHeight(display, screen));
    i32 x = static_cast<i32>((screenW - w) / 2);
    i32 y = static_cast<i32>((screenH - h) / 2);
    XMoveWindow(display, window, x, y);

    // Map (show) window
    XMapWindow(display, window);
    XFlush(display);

    // Wait for the window to be mapped
    XEvent event;
    while (true) {
        XNextEvent(display, &event);
        if (event.type == MapNotify) break;
    }

    // Create initial image buffer
    if (!createImageBuffer(w, h)) {
        destroy();
        return false;
    }

    return true;
}

void X11Window::destroy() {
    destroyImageBuffer();

    if (xic) {
        XDestroyIC(xic);
        xic = nullptr;
    }

    if (xim) {
        XCloseIM(xim);
        xim = nullptr;
    }

    if (gc) {
        XFreeGC(display, gc);
        gc = nullptr;
    }

    if (window) {
        XDestroyWindow(display, window);
        window = 0;
    }

    if (display) {
        XCloseDisplay(display);
        display = nullptr;
    }
}

void X11Window::setTitle(const char* title) {
    if (display && window) {
        XStoreName(display, window, title);
        XFlush(display);
    }
}

void X11Window::resize(u32 w, u32 h) {
    if (display && window) {
        XResizeWindow(display, window, w, h);
        XFlush(display);
        width = w;
        height = h;
    }
}

void X11Window::getScreenSize(u32& outWidth, u32& outHeight) const {
    if (display) {
        outWidth = static_cast<u32>(XDisplayWidth(display, screen));
        outHeight = static_cast<u32>(XDisplayHeight(display, screen));
    } else {
        outWidth = 1920;
        outHeight = 1080;
    }
}

void X11Window::setMinSize(u32 minW, u32 minH) {
    if (!display || !window) return;

    XSizeHints* hints = XAllocSizeHints();
    if (hints) {
        hints->flags = PMinSize;
        hints->min_width = static_cast<int>(minW);
        hints->min_height = static_cast<int>(minH);
        XSetWMNormalHints(display, window, hints);
        XFree(hints);
    }
}

void X11Window::centerOnScreen() {
    if (!display || !window) return;

    u32 screenW = static_cast<u32>(XDisplayWidth(display, screen));
    u32 screenH = static_cast<u32>(XDisplayHeight(display, screen));

    i32 x = static_cast<i32>((screenW - width) / 2);
    i32 y = static_cast<i32>((screenH - height) / 2);

    XMoveWindow(display, window, x, y);
    XFlush(display);
}

void X11Window::setDecorated(bool decor) {
    if (!display || !window) return;

    // Use Motif WM hints to control decorations
    struct {
        unsigned long flags;
        unsigned long functions;
        unsigned long decorations;
        long inputMode;
        unsigned long status;
    } hints = {0};

    hints.flags = 2;  // MWM_HINTS_DECORATIONS
    hints.decorations = decor ? 1 : 0;

    Atom motifHints = XInternAtom(display, "_MOTIF_WM_HINTS", False);
    XChangeProperty(display, window, motifHints, motifHints, 32,
                    PropModeReplace, (unsigned char*)&hints, 5);
    XFlush(display);
    decorated = decor;
}

void X11Window::startDrag(i32 /*hintX*/, i32 /*hintY*/) {
    if (!display || !window) return;

    // Query actual mouse position in root coordinates
    Window rootReturn, childReturn;
    int rootX, rootY, winX, winY;
    unsigned int maskReturn;
    XQueryPointer(display, DefaultRootWindow(display),
                  &rootReturn, &childReturn,
                  &rootX, &rootY, &winX, &winY, &maskReturn);

    // Use _NET_WM_MOVERESIZE to let the window manager handle dragging
    XUngrabPointer(display, CurrentTime);

    XEvent ev = {0};
    ev.xclient.type = ClientMessage;
    ev.xclient.window = window;
    ev.xclient.message_type = XInternAtom(display, "_NET_WM_MOVERESIZE", False);
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = rootX;
    ev.xclient.data.l[1] = rootY;
    ev.xclient.data.l[2] = 8;  // _NET_WM_MOVERESIZE_MOVE
    ev.xclient.data.l[3] = Button1;
    ev.xclient.data.l[4] = 1;  // source indication (normal application)

    XSendEvent(display, DefaultRootWindow(display), False,
               SubstructureRedirectMask | SubstructureNotifyMask, &ev);
    XFlush(display);
}

void X11Window::startResize(i32 direction) {
    if (!display || !window) return;
    if (direction < 0 || direction > 7) return;

    // Query actual mouse position in root coordinates
    Window rootReturn, childReturn;
    int rootX, rootY, winX, winY;
    unsigned int maskReturn;
    XQueryPointer(display, DefaultRootWindow(display),
                  &rootReturn, &childReturn,
                  &rootX, &rootY, &winX, &winY, &maskReturn);

    // Use _NET_WM_MOVERESIZE to let the window manager handle resizing
    XUngrabPointer(display, CurrentTime);

    XEvent ev = {0};
    ev.xclient.type = ClientMessage;
    ev.xclient.window = window;
    ev.xclient.message_type = XInternAtom(display, "_NET_WM_MOVERESIZE", False);
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = rootX;
    ev.xclient.data.l[1] = rootY;
    ev.xclient.data.l[2] = direction;  // 0-7 for resize directions
    ev.xclient.data.l[3] = Button1;
    ev.xclient.data.l[4] = 1;  // source indication (normal application)

    XSendEvent(display, DefaultRootWindow(display), False,
               SubstructureRedirectMask | SubstructureNotifyMask, &ev);
    XFlush(display);
}

void X11Window::minimize() {
    if (!display || !window) return;
    XIconifyWindow(display, window, screen);
    XFlush(display);
}

void X11Window::maximize() {
    if (!display || !window || maximized) return;

    // Store current geometry for restore
    Window root;
    i32 x, y;
    u32 w, h, border, depthRet;
    XGetGeometry(display, window, &root, &x, &y, &w, &h, &border, &depthRet);

    // Get window position relative to root
    Window child;
    XTranslateCoordinates(display, window, DefaultRootWindow(display),
                          0, 0, &restoreX, &restoreY, &child);
    restoreWidth = w;
    restoreHeight = h;

    // Send _NET_WM_STATE maximize request
    XEvent ev = {0};
    ev.xclient.type = ClientMessage;
    ev.xclient.window = window;
    ev.xclient.message_type = XInternAtom(display, "_NET_WM_STATE", False);
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = 1;  // _NET_WM_STATE_ADD
    ev.xclient.data.l[1] = XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
    ev.xclient.data.l[2] = XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_VERT", False);
    ev.xclient.data.l[3] = 1;  // source indication

    XSendEvent(display, DefaultRootWindow(display), False,
               SubstructureRedirectMask | SubstructureNotifyMask, &ev);
    XFlush(display);
    maximized = true;
}

void X11Window::restore() {
    if (!display || !window || !maximized) return;

    // Send _NET_WM_STATE remove maximize request
    XEvent ev = {0};
    ev.xclient.type = ClientMessage;
    ev.xclient.window = window;
    ev.xclient.message_type = XInternAtom(display, "_NET_WM_STATE", False);
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = 0;  // _NET_WM_STATE_REMOVE
    ev.xclient.data.l[1] = XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
    ev.xclient.data.l[2] = XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_VERT", False);
    ev.xclient.data.l[3] = 1;  // source indication

    XSendEvent(display, DefaultRootWindow(display), False,
               SubstructureRedirectMask | SubstructureNotifyMask, &ev);
    XFlush(display);

    // Restore previous geometry
    if (restoreWidth > 0 && restoreHeight > 0) {
        XMoveResizeWindow(display, window, restoreX, restoreY, restoreWidth, restoreHeight);
        XFlush(display);
    }

    maximized = false;
}

void X11Window::toggleMaximize() {
    if (maximized) {
        restore();
    } else {
        maximize();
    }
}

void X11Window::setCursor(i32 resizeDirection) {
    if (!display || !window) return;

    static Cursor cursors[9] = {0};  // 0-7 for resize, 8 for default
    static bool initialized = false;

    if (!initialized) {
        cursors[RESIZE_TOPLEFT] = XCreateFontCursor(display, XC_top_left_corner);
        cursors[RESIZE_TOP] = XCreateFontCursor(display, XC_top_side);
        cursors[RESIZE_TOPRIGHT] = XCreateFontCursor(display, XC_top_right_corner);
        cursors[RESIZE_RIGHT] = XCreateFontCursor(display, XC_right_side);
        cursors[RESIZE_BOTTOMRIGHT] = XCreateFontCursor(display, XC_bottom_right_corner);
        cursors[RESIZE_BOTTOM] = XCreateFontCursor(display, XC_bottom_side);
        cursors[RESIZE_BOTTOMLEFT] = XCreateFontCursor(display, XC_bottom_left_corner);
        cursors[RESIZE_LEFT] = XCreateFontCursor(display, XC_left_side);
        cursors[8] = XCreateFontCursor(display, XC_left_ptr);  // Default arrow
        initialized = true;
    }

    Cursor cursor;
    if (resizeDirection >= 0 && resizeDirection <= 7) {
        cursor = cursors[resizeDirection];
    } else {
        cursor = cursors[8];  // Default
    }

    XDefineCursor(display, window, cursor);
    XFlush(display);
}

bool X11Window::createImageBuffer(u32 w, u32 h) {
    destroyImageBuffer();

    // Allocate pixel buffer (BGRA format for X11)
    imageBuffer = new (std::nothrow) u32[w * h];
    if (!imageBuffer) {
        fprintf(stderr, "Failed to allocate image buffer\n");
        return false;
    }

    // Create XImage structure
    image = XCreateImage(
        display,
        visual,
        depth,
        ZPixmap,
        0,                          // offset
        reinterpret_cast<char*>(imageBuffer),
        w, h,
        32,                         // bitmap_pad
        0                           // bytes_per_line (0 = auto)
    );

    if (!image) {
        delete[] imageBuffer;
        imageBuffer = nullptr;
        fprintf(stderr, "Failed to create XImage\n");
        return false;
    }

    imageWidth = w;
    imageHeight = h;
    return true;
}

void X11Window::destroyImageBuffer() {
    if (image) {
        // Don't let XDestroyImage free our buffer, we manage it
        image->data = nullptr;
        XDestroyImage(image);
        image = nullptr;
    }

    if (imageBuffer) {
        delete[] imageBuffer;
        imageBuffer = nullptr;
    }

    imageWidth = 0;
    imageHeight = 0;
}

void X11Window::present(const u32* pixels, u32 w, u32 h) {
    if (!display || !window || !pixels) return;

    // Resize image buffer if needed
    if (w != imageWidth || h != imageHeight) {
        if (!createImageBuffer(w, h)) {
            return;
        }
    }

    // Copy pixels with format conversion (RGBA -> BGRA for X11)
    // X11 typically expects BGRA (or BGRX) in 32-bit depth
    const u32* src = pixels;
    u32* dst = imageBuffer;
    u32 count = w * h;

    for (u32 i = 0; i < count; ++i) {
        u32 rgba = src[i];
        // RGBA to BGRA: swap R and B
        u8 r = (rgba >> 24) & 0xFF;
        u8 g = (rgba >> 16) & 0xFF;
        u8 b = (rgba >> 8) & 0xFF;
        u8 a = rgba & 0xFF;
        dst[i] = (a << 24) | (r << 16) | (g << 8) | b;
    }

    // Blit to window
    XPutImage(display, window, gc, image, 0, 0, 0, 0, w, h);
    XFlush(display);
}

void X11Window::updateDpiScale() {
    dpiScale = 1.0f;

    if (!display) return;

    // Try to get DPI from Xft.dpi resource
    char* rms = XResourceManagerString(display);
    if (rms) {
        XrmDatabase db = XrmGetStringDatabase(rms);
        if (db) {
            XrmValue value;
            char* type = nullptr;
            if (XrmGetResource(db, "Xft.dpi", "Xft.Dpi", &type, &value)) {
                if (value.addr) {
                    f32 dpi = static_cast<f32>(atof(value.addr));
                    if (dpi > 0) {
                        // Standard DPI is 96
                        dpiScale = dpi / 96.0f;
                    }
                }
            }
            XrmDestroyDatabase(db);
        }
    }

    // Clamp to reasonable range
    if (dpiScale < 0.5f) dpiScale = 0.5f;
    if (dpiScale > 4.0f) dpiScale = 4.0f;
}

void X11Window::initXdnd() {
    if (!display || !window) return;

    // Initialize XDND atoms
    xdndAware = XInternAtom(display, "XdndAware", False);
    xdndEnter = XInternAtom(display, "XdndEnter", False);
    xdndPosition = XInternAtom(display, "XdndPosition", False);
    xdndStatus = XInternAtom(display, "XdndStatus", False);
    xdndLeave = XInternAtom(display, "XdndLeave", False);
    xdndDrop = XInternAtom(display, "XdndDrop", False);
    xdndFinished = XInternAtom(display, "XdndFinished", False);
    xdndActionCopy = XInternAtom(display, "XdndActionCopy", False);
    xdndSelection = XInternAtom(display, "XdndSelection", False);
    xdndTypeList = XInternAtom(display, "XdndTypeList", False);
    textUriList = XInternAtom(display, "text/uri-list", False);
    textPlain = XInternAtom(display, "text/plain", False);

    // Advertise XDND support (version 5)
    Atom xdndVersion = 5;
    XChangeProperty(display, window, xdndAware, XA_ATOM, 32,
                    PropModeReplace, (unsigned char*)&xdndVersion, 1);
}

void X11Window::handleXdndEvent(XEvent& event) {
    if (event.type == ClientMessage) {
        if (event.xclient.message_type == xdndEnter) {
            // Drag entered our window
            xdndSourceWindow = event.xclient.data.l[0];
            // We accept the drag - we'll check the file type on drop
        }
        else if (event.xclient.message_type == xdndPosition) {
            // Drag is moving over our window - always accept
            sendXdndStatus(xdndSourceWindow, true);
        }
        else if (event.xclient.message_type == xdndLeave) {
            // Drag left our window
            xdndSourceWindow = 0;
        }
        else if (event.xclient.message_type == xdndDrop) {
            // File was dropped - request the data
            if (xdndSourceWindow) {
                requestXdndData();
            }
        }
    }
    else if (event.type == SelectionNotify) {
        handleSelectionNotify(event);
    }
}

void X11Window::sendXdndStatus(Window source, bool accept) {
    if (!display || !window) return;

    XEvent ev = {0};
    ev.xclient.type = ClientMessage;
    ev.xclient.window = source;
    ev.xclient.message_type = xdndStatus;
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = window;           // Target window
    ev.xclient.data.l[1] = accept ? 1 : 0;   // Accept flag
    ev.xclient.data.l[2] = 0;                // Empty rectangle
    ev.xclient.data.l[3] = 0;
    ev.xclient.data.l[4] = accept ? xdndActionCopy : 0;

    XSendEvent(display, source, False, NoEventMask, &ev);
    XFlush(display);
}

void X11Window::sendXdndFinished(Window source, bool accepted) {
    if (!display || !window) return;

    XEvent ev = {0};
    ev.xclient.type = ClientMessage;
    ev.xclient.window = source;
    ev.xclient.message_type = xdndFinished;
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = window;           // Target window
    ev.xclient.data.l[1] = accepted ? 1 : 0; // Accepted flag
    ev.xclient.data.l[2] = accepted ? xdndActionCopy : 0;

    XSendEvent(display, source, False, NoEventMask, &ev);
    XFlush(display);
}

void X11Window::requestXdndData() {
    if (!display || !window || !xdndSourceWindow) return;

    // Request the data in text/uri-list format
    XConvertSelection(display, xdndSelection, textUriList,
                      xdndSelection, window, CurrentTime);
    xdndWaitingForData = true;
    XFlush(display);
}

void X11Window::handleSelectionNotify(XEvent& event) {
    if (!xdndWaitingForData) return;
    xdndWaitingForData = false;

    if (event.xselection.property == 0) {
        // Selection failed
        sendXdndFinished(xdndSourceWindow, false);
        xdndSourceWindow = 0;
        return;
    }

    // Get the selection data
    Atom actualType;
    int actualFormat;
    unsigned long numItems, bytesAfter;
    unsigned char* data = nullptr;

    int result = XGetWindowProperty(
        display, window, event.xselection.property,
        0, 65536, True, AnyPropertyType,
        &actualType, &actualFormat, &numItems, &bytesAfter, &data
    );

    if (result == Success && data && numItems > 0) {
        std::string filePath = parseUriList(reinterpret_cast<char*>(data), numItems);
        if (!filePath.empty() && onFileDrop) {
            onFileDrop(filePath);
        }
        sendXdndFinished(xdndSourceWindow, !filePath.empty());
    } else {
        sendXdndFinished(xdndSourceWindow, false);
    }

    if (data) {
        XFree(data);
    }
    xdndSourceWindow = 0;
}

std::string X11Window::parseUriList(const char* data, unsigned long length) {
    // Parse text/uri-list format (each URI on a line, may start with file://)
    std::string uriList(data, length);

    // Find the first non-comment line
    size_t pos = 0;
    while (pos < uriList.size()) {
        // Skip comments (lines starting with #)
        if (uriList[pos] == '#') {
            size_t eol = uriList.find('\n', pos);
            if (eol == std::string::npos) break;
            pos = eol + 1;
            continue;
        }

        // Find end of line
        size_t eol = uriList.find_first_of("\r\n", pos);
        if (eol == std::string::npos) eol = uriList.size();

        std::string uri = uriList.substr(pos, eol - pos);
        if (!uri.empty()) {
            // Remove file:// prefix if present
            const char* filePrefix = "file://";
            if (uri.compare(0, 7, filePrefix) == 0) {
                uri = uri.substr(7);
            }

            // URL decode the path (handle %20 for spaces, etc.)
            std::string decoded;
            for (size_t i = 0; i < uri.size(); ++i) {
                if (uri[i] == '%' && i + 2 < uri.size()) {
                    char hex[3] = { uri[i+1], uri[i+2], 0 };
                    char* end;
                    long val = strtol(hex, &end, 16);
                    if (end == hex + 2) {
                        decoded += static_cast<char>(val);
                        i += 2;
                        continue;
                    }
                }
                decoded += uri[i];
            }

            // Check if it's a supported file type
            size_t dotPos = decoded.rfind('.');
            if (dotPos != std::string::npos) {
                std::string ext = decoded.substr(dotPos);
                // Convert to lowercase for comparison
                for (char& c : ext) {
                    if (c >= 'A' && c <= 'Z') c += 32;
                }
                if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
                    ext == ".bmp" || ext == ".gif" || ext == ".pp") {
                    return decoded;
                }
            }
        }

        pos = eol + 1;
    }

    return "";
}

bool X11Window::processEvents() {
    if (!display) return false;

    while (XPending(display)) {
        XEvent event;
        XNextEvent(display, &event);

        // Filter events for XIM
        if (xic && XFilterEvent(&event, X11_NONE)) {
            continue;
        }

        switch (event.type) {
            case ClientMessage:
                if (event.xclient.message_type == wmProtocols &&
                    static_cast<Atom>(event.xclient.data.l[0]) == wmDeleteMessage) {
                    if (onCloseRequested) onCloseRequested();
                } else {
                    // Handle XDND (drag and drop) events
                    handleXdndEvent(event);
                }
                break;

            case SelectionNotify:
                // Handle drag and drop selection data
                handleXdndEvent(event);
                break;

            case KeyPress: {
                // Build modifiers
                KeyMods mods;
                mods.shift = (event.xkey.state & ShiftMask) != 0;
                mods.ctrl = (event.xkey.state & ControlMask) != 0;
                mods.alt = (event.xkey.state & Mod1Mask) != 0;

                // Get keysym
                KeySym keysym = XLookupKeysym(&event.xkey, 0);

                // Notify key down
                if (onKeyDown) {
                    onKeyDown(static_cast<i32>(keysym), 0, mods, false);
                }

                // Handle text input via XIM
                if (xic && onTextInput) {
                    char buffer[32];
                    KeySym ks;
                    Status status;
                    i32 len = Xutf8LookupString(xic, &event.xkey, buffer, sizeof(buffer) - 1, &ks, &status);
                    if (len > 0 && (status == XLookupChars || status == XLookupBoth)) {
                        buffer[len] = '\0';
                        // Filter out control characters
                        if (static_cast<u8>(buffer[0]) >= 32) {
                            onTextInput(buffer);
                        }
                    }
                }
                break;
            }

            case KeyRelease: {
                // Check for auto-repeat (key release immediately followed by key press)
                if (XPending(display)) {
                    XEvent next;
                    XPeekEvent(display, &next);
                    if (next.type == KeyPress &&
                        next.xkey.time == event.xkey.time &&
                        next.xkey.keycode == event.xkey.keycode) {
                        // Skip auto-repeat key release
                        break;
                    }
                }

                KeyMods mods;
                mods.shift = (event.xkey.state & ShiftMask) != 0;
                mods.ctrl = (event.xkey.state & ControlMask) != 0;
                mods.alt = (event.xkey.state & Mod1Mask) != 0;

                KeySym keysym = XLookupKeysym(&event.xkey, 0);
                if (onKeyUp) {
                    onKeyUp(static_cast<i32>(keysym), 0, mods);
                }
                break;
            }

            case ButtonPress: {
                i32 x = event.xbutton.x;
                i32 y = event.xbutton.y;
                u32 button = event.xbutton.button;

                // Mouse wheel (button 4 = up, button 5 = down)
                if (button == 4) {
                    if (onMouseWheel) onMouseWheel(x, y, 1);
                } else if (button == 5) {
                    if (onMouseWheel) onMouseWheel(x, y, -1);
                } else if (button == 6 || button == 7) {
                    // Horizontal scroll - ignore for now
                } else {
                    // Regular mouse button (1=left, 2=middle, 3=right)
                    if (onMouseDown) {
                        onMouseDown(x, y, static_cast<MouseButton>(button));
                    }
                }
                break;
            }

            case ButtonRelease: {
                u32 button = event.xbutton.button;
                // Ignore scroll wheel releases
                if (button >= 4 && button <= 7) break;
                if (onMouseUp) {
                    onMouseUp(event.xbutton.x, event.xbutton.y, static_cast<MouseButton>(button));
                }
                break;
            }

            case MotionNotify:
                if (onMouseMove) {
                    onMouseMove(event.xmotion.x, event.xmotion.y);
                }
                break;

            case ConfigureNotify:
                if (static_cast<u32>(event.xconfigure.width) != width ||
                    static_cast<u32>(event.xconfigure.height) != height) {
                    width = event.xconfigure.width;
                    height = event.xconfigure.height;
                    if (onResize) {
                        onResize(width, height);
                    }
                }
                break;

            case Expose:
                // Only redraw on the last expose event in sequence
                if (event.xexpose.count == 0) {
                    if (onExpose) onExpose();
                }
                break;

            case FocusIn:
                if (xic) {
                    XSetICFocus(xic);
                }
                break;

            case FocusOut:
                if (xic) {
                    XUnsetICFocus(xic);
                }
                break;
        }
    }

    return true;
}
