#include "wasm_window.h"
#include "keycodes.h"
#include <emscripten.h>
#include <emscripten/html5.h>
#include <cstdio>
#include <cstring>

// Global window instance for JavaScript callbacks
WasmWindow* g_wasmWindow = nullptr;

WasmWindow::WasmWindow() = default;

WasmWindow::~WasmWindow() {
    destroy();
}

bool WasmWindow::create(u32 w, u32 h, const char* title) {
    g_wasmWindow = this;

    fprintf(stderr, "WasmWindow::create() starting...\n");

    // Get physical pixel dimensions (CSS size × devicePixelRatio)
    // We render at full physical resolution for crisp HiDPI display
    f32 browserDpr = static_cast<f32>(EM_ASM_DOUBLE({
        return window.devicePixelRatio || 1.0;
    }));
    i32 cssW = EM_ASM_INT({ return window.innerWidth; });
    i32 cssH = EM_ASM_INT({ return window.innerHeight; });
    width = static_cast<u32>(cssW * browserDpr);
    height = static_cast<u32>(cssH * browserDpr);

    fprintf(stderr, "Physical size: %dx%d (CSS: %dx%d, DPR: %.2f)\n",
            width, height, cssW, cssH, browserDpr);

    // DPI scale is 1.0 since we're already at physical pixel resolution
    // The browser's devicePixelRatio is handled by JavaScript canvas scaling
    dpiScale = 1.0f;

    // Set window title
    setTitle(title);

    fprintf(stderr, "WasmWindow created: %ux%u\n", width, height);
    return true;
}

void WasmWindow::destroy() {
    if (g_wasmWindow == this) {
        g_wasmWindow = nullptr;
    }
}

void WasmWindow::setTitle(const char* title) {
    EM_ASM({
        document.title = UTF8ToString($0);
    }, title);
}

void WasmWindow::resize(u32 w, u32 h) {
    width = w;
    height = h;
    // Canvas resize is handled by JavaScript
}

u32 WasmWindow::getWidth() const {
    return width;
}

u32 WasmWindow::getHeight() const {
    return height;
}

f32 WasmWindow::getDpiScale() const {
    return dpiScale;
}

void WasmWindow::getScreenSize(u32& outWidth, u32& outHeight) const {
    outWidth = static_cast<u32>(EM_ASM_INT({ return window.innerWidth; }));
    outHeight = static_cast<u32>(EM_ASM_INT({ return window.innerHeight; }));
}

void WasmWindow::setMinSize(u32 minW, u32 minH) {
    // Not applicable for fullscreen browser canvas
    (void)minW;
    (void)minH;
}

void WasmWindow::centerOnScreen() {
    // Canvas is always centered via CSS
}

void WasmWindow::setDecorated(bool decorated) {
    // Browser handles decorations, we render our own title bar
    (void)decorated;
}

void WasmWindow::startDrag(i32 rootX, i32 rootY) {
    // Not applicable in browser - window is fullscreen
    (void)rootX;
    (void)rootY;
}

void WasmWindow::startResize(i32 direction) {
    // Not applicable in browser - window is fullscreen
    (void)direction;
}

void WasmWindow::minimize() {
    // Not applicable in browser
}

void WasmWindow::maximize() {
    // Request fullscreen
    EM_ASM({
        const canvas = document.getElementById('canvas');
        if (canvas.requestFullscreen) {
            canvas.requestFullscreen();
        }
    });
    maximized = true;
}

void WasmWindow::restore() {
    EM_ASM({
        if (document.exitFullscreen) {
            document.exitFullscreen();
        }
    });
    maximized = false;
}

void WasmWindow::toggleMaximize() {
    if (maximized) {
        restore();
    } else {
        maximize();
    }
}

bool WasmWindow::isMaximized() const {
    // In browser, we're always "maximized" (fullscreen canvas)
    return true;
}

void WasmWindow::setCursor(i32 resizeDirection) {
    const char* cursor = "default";

    switch (resizeDirection) {
        case RESIZE_TOP:
        case RESIZE_BOTTOM:
            cursor = "ns-resize";
            break;
        case RESIZE_LEFT:
        case RESIZE_RIGHT:
            cursor = "ew-resize";
            break;
        case RESIZE_TOPLEFT:
        case RESIZE_BOTTOMRIGHT:
            cursor = "nwse-resize";
            break;
        case RESIZE_TOPRIGHT:
        case RESIZE_BOTTOMLEFT:
            cursor = "nesw-resize";
            break;
        default:
            cursor = "default";
            break;
    }

    EM_ASM({
        document.getElementById('canvas').style.cursor = UTF8ToString($0);
    }, cursor);
}

void WasmWindow::present(const u32* pixels, u32 w, u32 h) {
    // Call JavaScript to render pixels to canvas
    EM_ASM({
        js_render_frame($0, $1, $2);
    }, pixels, w, h);
}

void WasmWindow::presentPartial(const u32* pixels, u32 w, u32 h, i32 dx, i32 dy, i32 dw, i32 dh) {
    // Call JavaScript to render only the dirty region
    EM_ASM({
        js_render_frame_partial($0, $1, $2, $3, $4, $5, $6);
    }, pixels, w, h, dx, dy, dw, dh);
}

bool WasmWindow::processEvents() {
    // Process all queued events
    for (const auto& event : eventQueue) {
        switch (event.type) {
            case WasmEventType::MouseDown:
                if (onMouseDown) {
                    onMouseDown(event.x, event.y, mapMouseButton(event.button));
                }
                break;

            case WasmEventType::MouseUp:
                if (onMouseUp) {
                    onMouseUp(event.x, event.y, mapMouseButton(event.button));
                }
                break;

            case WasmEventType::MouseMove:
                if (onMouseMove) {
                    onMouseMove(event.x, event.y);
                }
                break;

            case WasmEventType::MouseWheel:
                if (onMouseWheel) {
                    onMouseWheel(event.x, event.y, event.wheelDelta);
                }
                break;

            case WasmEventType::KeyDown:
                if (onKeyDown) {
                    onKeyDown(mapKeyCode(event.keyCode), event.scanCode,
                              mapModifiers(event.mods), event.repeat);
                }
                break;

            case WasmEventType::KeyUp:
                if (onKeyUp) {
                    onKeyUp(mapKeyCode(event.keyCode), event.scanCode,
                            mapModifiers(event.mods));
                }
                break;

            case WasmEventType::TextInput:
                if (onTextInput && !event.text.empty()) {
                    onTextInput(event.text.c_str());
                }
                break;

            case WasmEventType::Resize:
                width = static_cast<u32>(event.x);
                height = static_cast<u32>(event.y);
                if (onResize) {
                    onResize(width, height);
                }
                break;

            case WasmEventType::FileDrop:
                if (onFileDrop && !event.text.empty()) {
                    onFileDrop(event.text);
                }
                break;
        }
    }

    eventQueue.clear();
    return true;
}

void WasmWindow::pushEvent(const WasmEvent& event) {
    eventQueue.push_back(event);
}

// Map JavaScript key codes to our X11-like key codes
i32 WasmWindow::mapKeyCode(i32 jsKeyCode) const {
    // JavaScript uses DOM key codes which are different from X11
    // Map common keys
    switch (jsKeyCode) {
        // Special keys
        case 8: return Key::BACKSPACE;
        case 9: return Key::TAB;
        case 13: return Key::RETURN;
        case 27: return Key::ESCAPE;
        case 46: return Key::DELETE;
        case 32: return Key::SPACE;

        // Navigation keys
        case 36: return Key::HOME;
        case 35: return Key::END;
        case 37: return Key::LEFT;
        case 38: return Key::UP;
        case 39: return Key::RIGHT;
        case 40: return Key::DOWN;
        case 33: return Key::PAGE_UP;
        case 34: return Key::PAGE_DOWN;

        // Function keys
        case 112: return Key::F1;
        case 113: return Key::F2;
        case 114: return Key::F3;
        case 115: return Key::F4;
        case 116: return Key::F5;
        case 117: return Key::F6;
        case 118: return Key::F7;
        case 119: return Key::F8;
        case 120: return Key::F9;
        case 121: return Key::F10;
        case 122: return Key::F11;
        case 123: return Key::F12;

        // Modifier keys
        case 16: return Key::SHIFT_L;
        case 17: return Key::CONTROL_L;
        case 18: return Key::ALT_L;

        // Number keys (top row)
        case 48: return Key::KEY_0;
        case 49: return Key::KEY_1;
        case 50: return Key::KEY_2;
        case 51: return Key::KEY_3;
        case 52: return Key::KEY_4;
        case 53: return Key::KEY_5;
        case 54: return Key::KEY_6;
        case 55: return Key::KEY_7;
        case 56: return Key::KEY_8;
        case 57: return Key::KEY_9;

        // Letter keys (A-Z) - JS uses uppercase, we use lowercase
        case 65: return Key::A;
        case 66: return Key::B;
        case 67: return Key::C;
        case 68: return Key::D;
        case 69: return Key::E;
        case 70: return Key::F;
        case 71: return Key::G;
        case 72: return Key::H;
        case 73: return Key::I;
        case 74: return Key::J;
        case 75: return Key::K;
        case 76: return Key::L;
        case 77: return Key::M;
        case 78: return Key::N;
        case 79: return Key::O;
        case 80: return Key::P;
        case 81: return Key::Q;
        case 82: return Key::R;
        case 83: return Key::S;
        case 84: return Key::T;
        case 85: return Key::U;
        case 86: return Key::V;
        case 87: return Key::W;
        case 88: return Key::X;
        case 89: return Key::Y;
        case 90: return Key::Z;

        // Punctuation
        case 186: return Key::SEMICOLON;    // ;:
        case 187: return Key::EQUALS;       // =+
        case 188: return Key::COMMA;        // ,<
        case 189: return Key::MINUS;        // -_
        case 190: return Key::PERIOD;       // .>
        case 191: return Key::SLASH;        // /?
        case 192: return Key::BACKQUOTE;    // `~
        case 219: return Key::LEFTBRACKET;  // [{
        case 220: return Key::BACKSLASH;    // \|
        case 221: return Key::RIGHTBRACKET; // ]}
        case 222: return Key::QUOTE;        // '"

        default:
            return jsKeyCode;
    }
}

MouseButton WasmWindow::mapMouseButton(i32 jsButton) const {
    switch (jsButton) {
        case 1: return MouseButton::Left;
        case 2: return MouseButton::Middle;
        case 3: return MouseButton::Right;
        default: return MouseButton::None;
    }
}

KeyMods WasmWindow::mapModifiers(i32 jsMods) const {
    KeyMods mods;
    mods.shift = (jsMods & 1) != 0;
    mods.ctrl = (jsMods & 2) != 0;
    mods.alt = (jsMods & 4) != 0;
    // Note: Meta key (jsMods & 8) is treated as Ctrl for Mac compatibility
    if (jsMods & 8) mods.ctrl = true;
    return mods;
}

// C functions called from JavaScript to push events
extern "C" {

EMSCRIPTEN_KEEPALIVE
void wasm_push_mouse_event(i32 type, i32 x, i32 y, i32 button, i32 mods) {
    if (!g_wasmWindow) return;

    WasmEvent event;
    event.x = x;
    event.y = y;
    event.button = button;
    event.mods = mods;

    switch (type) {
        case 0: event.type = WasmEventType::MouseDown; break;
        case 1: event.type = WasmEventType::MouseUp; break;
        case 2: event.type = WasmEventType::MouseMove; break;
        default: return;
    }

    g_wasmWindow->pushEvent(event);
}

EMSCRIPTEN_KEEPALIVE
void wasm_push_key_event(i32 type, i32 keyCode, i32 scanCode, i32 mods, i32 repeat) {
    if (!g_wasmWindow) return;

    WasmEvent event;
    event.type = (type == 0) ? WasmEventType::KeyDown : WasmEventType::KeyUp;
    event.keyCode = keyCode;
    event.scanCode = scanCode;
    event.mods = mods;
    event.repeat = (repeat != 0);

    g_wasmWindow->pushEvent(event);
}

EMSCRIPTEN_KEEPALIVE
void wasm_push_resize_event(i32 width, i32 height) {
    if (!g_wasmWindow) return;

    WasmEvent event;
    event.type = WasmEventType::Resize;
    event.x = width;
    event.y = height;

    g_wasmWindow->pushEvent(event);
}

EMSCRIPTEN_KEEPALIVE
void wasm_push_text_input(const char* text) {
    if (!g_wasmWindow || !text) return;

    WasmEvent event;
    event.type = WasmEventType::TextInput;
    event.text = text;

    g_wasmWindow->pushEvent(event);
}

EMSCRIPTEN_KEEPALIVE
void wasm_push_file_drop(const char* path) {
    if (!g_wasmWindow || !path) return;

    WasmEvent event;
    event.type = WasmEventType::FileDrop;
    event.text = path;

    g_wasmWindow->pushEvent(event);
}

EMSCRIPTEN_KEEPALIVE
void wasm_push_wheel_event(i32 x, i32 y, i32 delta, i32 mods) {
    if (!g_wasmWindow) return;

    WasmEvent event;
    event.type = WasmEventType::MouseWheel;
    event.x = x;
    event.y = y;
    event.wheelDelta = delta;
    event.mods = mods;

    g_wasmWindow->pushEvent(event);
}

} // extern "C"
