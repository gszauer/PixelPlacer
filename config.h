#ifndef _H_CONFIG_
#define _H_CONFIG_

#include "types.h"

namespace Config {
    // Canvas
    constexpr u32 TILE_SIZE = 64;
    constexpr u32 MAX_CANVAS_SIZE = 16384;
    constexpr u32 DEFAULT_CANVAS_WIDTH = 1920;
    constexpr u32 DEFAULT_CANVAS_HEIGHT = 1080;

    // Tools
    constexpr f32 MIN_BRUSH_SIZE = 1.0f;
    constexpr f32 MAX_BRUSH_SIZE = 500.0f;
    constexpr f32 DEFAULT_BRUSH_SIZE = 10.0f;
    constexpr f32 DEFAULT_BRUSH_SPACING = 0.25f;
    constexpr f32 DEFAULT_BRUSH_HARDNESS = 0.8f;
    constexpr f32 DEFAULT_BRUSH_OPACITY = 1.0f;

    // View
    constexpr f32 MIN_ZOOM = 0.01f;   // 1%
    constexpr f32 MAX_ZOOM = 30.0f;   // 3000%
    constexpr f32 DEFAULT_ZOOM = 1.0f;
    constexpr f32 ZOOM_STEP = 1.2f;

    // Runtime UI scale (adjustable, default for HiDPI)
    extern f32 uiScale;

    // Base UI layout values (unscaled)
    constexpr f32 BASE_MENU_BAR_HEIGHT = 31.0f;  // ~30% taller than original 24
    constexpr f32 BASE_TOOL_OPTIONS_HEIGHT = 32.0f;
    constexpr f32 BASE_TOOL_PALETTE_WIDTH = 80.0f;
    constexpr f32 BASE_RIGHT_SIDEBAR_WIDTH = 240.0f;
    constexpr f32 BASE_STATUS_BAR_HEIGHT = 24.0f;
    constexpr f32 BASE_TAB_BAR_HEIGHT = 28.0f;
    constexpr f32 BASE_PANEL_HEADER_HEIGHT = 24.0f;
    constexpr f32 BASE_LAYER_ITEM_HEIGHT = 48.0f;
    constexpr f32 BASE_DEFAULT_FONT_SIZE = 16.0f;
    constexpr f32 BASE_SMALL_FONT_SIZE = 14.0f;
    constexpr f32 BASE_MENU_FONT_SIZE = 18.0f;  // Larger font for menu bar

    // Scaled UI accessors (use these instead of constants)
    inline f32 menuBarHeight() { return BASE_MENU_BAR_HEIGHT * uiScale; }
    inline f32 toolOptionsHeight() { return BASE_TOOL_OPTIONS_HEIGHT * uiScale; }
    inline f32 toolPaletteWidth() { return BASE_TOOL_PALETTE_WIDTH * uiScale; }
    inline f32 rightSidebarWidth() { return BASE_RIGHT_SIDEBAR_WIDTH * uiScale; }
    inline f32 statusBarHeight() { return BASE_STATUS_BAR_HEIGHT * uiScale; }
    inline f32 tabBarHeight() { return BASE_TAB_BAR_HEIGHT * uiScale; }
    inline f32 panelHeaderHeight() { return BASE_PANEL_HEADER_HEIGHT * uiScale; }
    inline f32 layerItemHeight() { return BASE_LAYER_ITEM_HEIGHT * uiScale; }
    inline f32 defaultFontSize() { return BASE_DEFAULT_FONT_SIZE * uiScale; }
    inline f32 smallFontSize() { return BASE_SMALL_FONT_SIZE * uiScale; }
    inline f32 menuFontSize() { return BASE_MENU_FONT_SIZE * uiScale; }

    // Colors (RGBA format: 0xRRGGBBAA)
    constexpr u32 COLOR_BACKGROUND = 0x2D2D2DFF;
    constexpr u32 COLOR_BACKGROUND_DISABLED = 0x252525FF;  // Darker for disabled/readonly
    constexpr u32 COLOR_TITLEBAR = 0x1E1E1EFF;  // Dark title bar / menu bar
    constexpr u32 COLOR_PANEL = 0x3C3C3CFF;
    constexpr u32 COLOR_PANEL_HEADER = 0x4A4A4AFF;
    constexpr u32 COLOR_BORDER = 0x1A1A1AFF;
    constexpr u32 COLOR_TEXT = 0xE0E0E0FF;
    constexpr u32 COLOR_TEXT_DIM = 0x808080FF;
    constexpr u32 COLOR_ACCENT = 0x4A90D9FF;
    constexpr u32 COLOR_HOVER = 0x505050FF;
    constexpr u32 COLOR_ACTIVE = 0x606060FF;
    constexpr u32 COLOR_SELECTION = 0x4A90D940;

    // Checkerboard (transparency indicator)
    constexpr u32 CHECKER_SIZE = 8;
    constexpr u32 CHECKER_COLOR1 = 0xCCCCCCFF;
    constexpr u32 CHECKER_COLOR2 = 0xFFFFFFFF;

    // Misc
    constexpr u32 MAX_LAYERS = 256;
    constexpr f32 SCROLL_SPEED = 20.0f;
    constexpr u32 DOUBLE_CLICK_MS = 400;
}

#endif
