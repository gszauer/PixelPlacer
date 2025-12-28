// PixelPlacer - A photo editor between Paint and Photoshop
// Implemented in minimal, readable C++ with automatic memory management
// Pure software rendering, no GPU required

#include "application.h"

int main(int argc, char* argv[]) {
    // Create and run application
    Application app;
    if (!app.initialize(0, 0, "PixelPlacer")) {  // 0,0 = auto-size to half screen
        return 1;
    }

    app.run();
    app.shutdown();

    return 0;
}

// Unity build: compile everything by building just main.cpp
#ifdef UNITY_BUILD

#ifdef __linux__
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
