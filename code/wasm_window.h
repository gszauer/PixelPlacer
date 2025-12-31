#ifndef _H_WASM_WINDOW_
#define _H_WASM_WINDOW_

#include "platform_window.h"
#include <vector>

// Event types for the event queue
enum class WasmEventType {
    MouseDown,
    MouseUp,
    MouseMove,
    MouseWheel,
    KeyDown,
    KeyUp,
    TextInput,
    Resize,
    FileDrop
};

// Unified event structure
struct WasmEvent {
    WasmEventType type;
    i32 x, y;
    i32 button;
    i32 keyCode;
    i32 scanCode;
    i32 mods;
    i32 wheelDelta;
    bool repeat;
    std::string text;  // For text input and file drop
};

// WebAssembly implementation of PlatformWindow using HTML5 Canvas
class WasmWindow : public PlatformWindow {
public:
    WasmWindow();
    ~WasmWindow() override;

    // PlatformWindow interface implementation
    bool create(u32 w, u32 h, const char* title) override;
    void destroy() override;

    void setTitle(const char* title) override;
    void resize(u32 w, u32 h) override;
    u32 getWidth() const override;
    u32 getHeight() const override;
    f32 getDpiScale() const override;
    void getScreenSize(u32& outWidth, u32& outHeight) const override;
    void setMinSize(u32 minW, u32 minH) override;
    void centerOnScreen() override;

    void setDecorated(bool decorated) override;
    void startDrag(i32 rootX, i32 rootY) override;
    void startResize(i32 direction) override;
    void minimize() override;
    void maximize() override;
    void restore() override;
    void toggleMaximize() override;
    bool isMaximized() const override;

    void setCursor(i32 resizeDirection) override;

    void present(const u32* pixels, u32 w, u32 h) override;
    void presentPartial(const u32* pixels, u32 w, u32 h, i32 dx, i32 dy, i32 dw, i32 dh) override;
    bool processEvents() override;

    // Called from JavaScript to queue events
    void pushEvent(const WasmEvent& event);

private:
    u32 width = 0;
    u32 height = 0;
    f32 dpiScale = 1.0f;
    bool maximized = false;

    // Event queue - JavaScript pushes events, processEvents() handles them
    std::vector<WasmEvent> eventQueue;

    // Convert JS key code to our key code
    i32 mapKeyCode(i32 jsKeyCode) const;

    // Convert JS button to MouseButton
    MouseButton mapMouseButton(i32 jsButton) const;

    // Convert JS mods to KeyMods
    KeyMods mapModifiers(i32 jsMods) const;
};

// Global window instance for JavaScript callbacks
extern WasmWindow* g_wasmWindow;

// C functions exported to JavaScript for event handling
extern "C" {
    void wasm_push_mouse_event(i32 type, i32 x, i32 y, i32 button, i32 mods);
    void wasm_push_key_event(i32 type, i32 keyCode, i32 scanCode, i32 mods, i32 repeat);
    void wasm_push_resize_event(i32 width, i32 height);
    void wasm_push_text_input(const char* text);
    void wasm_push_file_drop(const char* path);
    void wasm_push_wheel_event(i32 x, i32 y, i32 delta, i32 mods);
}

#endif
