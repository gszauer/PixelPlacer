// PixelPlacer - A photo editor between Paint and Photoshop
// Implemented in minimal, readable C++ with automatic memory management
// Pure software rendering, no GPU required

#include "application.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <cstdio>

// Global application instance for Emscripten main loop
static Application* g_app = nullptr;

// Emscripten main loop callback
static void emscripten_main_loop() {
    if (g_app) {
        g_app->frame();
    }
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    fprintf(stderr, "PixelPlacer WASM main() starting...\n");

    g_app = new Application();
    fprintf(stderr, "Application created, initializing...\n");

    if (!g_app->initialize(0, 0, "PixelPlacer")) {
        fprintf(stderr, "ERROR: Application initialization failed!\n");
        delete g_app;
        g_app = nullptr;
        return 1;
    }

    fprintf(stderr, "Application initialized, starting main loop...\n");

    // Set up Emscripten main loop
    // 0 = use requestAnimationFrame (typically 60fps)
    // 1 = simulate infinite loop
    emscripten_set_main_loop(emscripten_main_loop, 0, 1);

    // Note: In Emscripten, control flow doesn't return from set_main_loop
    // Cleanup happens when the page is closed
    return 0;
}

#else

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    // Create and run application
    Application app;
    if (!app.initialize(0, 0, "PixelPlacer")) {  // 0,0 = auto-size to half screen
        return 1;
    }

    app.run();
    app.shutdown();

    return 0;
}

#endif

// Unity build: compile everything by building just main.cpp
#ifdef UNITY_BUILD

#if defined(__EMSCRIPTEN__)
#include "platform_wasm.cpp"
#include "wasm_window.cpp"
#elif defined(_WIN32)
#include "platform_windows.cpp"
#include "win32_window.cpp"
#elif defined(__linux__)
#include "platform_linux.cpp"
#include "x11_window.cpp"
#endif

#include "material_font.cpp"
#include "inter_font.cpp"
#include "primitives.cpp"
#include "tile.cpp"
#include "tiled_canvas.cpp"
#include "layer.cpp"
#include "document.cpp"
#include "document_view.cpp"
#include "selection.cpp"
#include "tool.cpp"
#include "brush_tool.cpp"
#include "eraser_tool.cpp"
#include "fill_tool.cpp"
#include "selection_tools.cpp"
#include "transform_tools.cpp"
#include "retouch_tools.cpp"
#include "widget.cpp"
#include "layouts.cpp"
#include "basic_widgets.cpp"
#include "panels.cpp"
#include "overlay_manager.cpp"
#include "dialogs.cpp"
#include "brush_dialogs.cpp"
#include "main_window.cpp"
#include "framebuffer.cpp"
#include "sampler.cpp"
#include "blend.cpp"
#include "compositor.cpp"
#include "brush_renderer.cpp"
#include "image_io.cpp"
#include "project_file.cpp"
#include "application.cpp"
#include "app_state.cpp"

#endif
