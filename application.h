#ifndef _H_APPLICATION_
#define _H_APPLICATION_

#include "types.h"
#include "primitives.h"
#include "framebuffer.h"
#include "widget.h"
#include "app_state.h"
#include "platform_window.h"
#include <memory>
#include <string>

class Application {
public:
    Application();
    ~Application();

    // Non-copyable
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    bool initialize(u32 width, u32 height, const char* title);
    void run();
    void shutdown();

    // Window management
    void setTitle(const std::string& title);
    Vec2 getWindowSize() const;
    void resize(u32 width, u32 height);

    // Event handling
    void handleKeyDown(i32 keyCode, i32 scanCode, bool repeat);
    void handleKeyUp(i32 keyCode, i32 scanCode);
    void handleMouseDown(i32 x, i32 y, MouseButton button);
    void handleMouseUp(i32 x, i32 y, MouseButton button);
    void handleMouseMove(i32 x, i32 y);
    void handleMouseWheel(i32 x, i32 y, i32 deltaY);
    void handleTextInput(const char* text);
    void handleWindowResize(i32 width, i32 height);

    // Rendering
    void render();
    void present();

    // Root widget management
    void setRootWidget(std::unique_ptr<Widget> root);
    Widget* getRootWidget() { return rootWidget.get(); }

    // Focus management
    void setFocus(Widget* widget);
    void captureMouse(Widget* widget);
    void releaseMouse();

private:
    std::unique_ptr<PlatformWindow> window;
    Framebuffer framebuffer;
    std::unique_ptr<Widget> rootWidget;

    u32 windowWidth = 0;
    u32 windowHeight = 0;
    u32 drawableWidth = 0;
    u32 drawableHeight = 0;
    f32 dpiScale = 1.0f;  // Ratio of drawable to window size
    bool initialized = false;

    // Input state
    KeyMods currentMods;

    // Create main window layout
    void createMainWindow();
    void loadDefaultFont();
    void updateDPIScale();
    void rebuildUIWithScale(f32 newScale);

    // Scale mouse coordinates from window to drawable space
    // With X11 + XPutImage, window size equals drawable size, so no scaling needed
    Vec2 scaleMouseCoords(i32 x, i32 y) const {
        return Vec2(static_cast<f32>(x), static_cast<f32>(y));
    }

    // Check if mouse is on a resize edge, returns direction (0-7) or -1
    i32 getResizeDirection(i32 x, i32 y) const;
};

#endif
