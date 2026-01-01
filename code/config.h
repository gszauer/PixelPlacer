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
    constexpr f32 BASE_MENU_BAR_HEIGHT = 36.0f;  // Taller to fit larger menu font
    constexpr f32 BASE_TOOL_OPTIONS_HEIGHT = 32.0f;
    constexpr f32 BASE_TOOL_PALETTE_WIDTH = 80.0f;
    constexpr f32 BASE_RIGHT_SIDEBAR_WIDTH = 240.0f;
    constexpr f32 BASE_STATUS_BAR_HEIGHT = 24.0f;
    constexpr f32 BASE_TAB_BAR_HEIGHT = 28.0f;
    constexpr f32 BASE_PANEL_HEADER_HEIGHT = 24.0f;
    constexpr f32 BASE_LAYER_ITEM_HEIGHT = 48.0f;
    constexpr f32 BASE_DEFAULT_FONT_SIZE = 16.0f;
    constexpr f32 BASE_SMALL_FONT_SIZE = 14.0f;
    constexpr f32 BASE_MENU_FONT_SIZE = 21.0f;  // 1.3x default font for menu bar

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

    // ==========================================
    // Adobe Spectrum Dark Theme Colors
    // ==========================================
    // Colors in RGBA format: 0xRRGGBBAA
    // Source: spectrum-css tokens (dark theme)

    // Spectrum Gray Scale
    constexpr u32 GRAY_50  = 0x1B1B1BFF;  // Darkest - title bar, borders
    constexpr u32 GRAY_75  = 0x222222FF;  // Dark surfaces, disabled bg
    constexpr u32 GRAY_100 = 0x2C2C2CFF;  // Default background
    constexpr u32 GRAY_200 = 0x323232FF;  // Slightly elevated
    constexpr u32 GRAY_300 = 0x393939FF;  // Panels
    constexpr u32 GRAY_400 = 0x444444FF;  // Panel headers, hover
    constexpr u32 GRAY_500 = 0x6D6D6DFF;  // Borders, active state
    constexpr u32 GRAY_600 = 0x8A8A8AFF;  // Dim/disabled text
    constexpr u32 GRAY_700 = 0xAFAFAFFF;  // Secondary text
    constexpr u32 GRAY_800 = 0xDBDBDBFF;  // Primary text
    constexpr u32 GRAY_900 = 0xF2F2F2FF;  // High contrast text

    // Spectrum Blue Accent Scale
    constexpr u32 BLUE_700 = 0x5D89FFFF;  // Focus rings
    constexpr u32 BLUE_800 = 0x4B75FFFF;  // Primary accent, selected
    constexpr u32 BLUE_900 = 0x3B63FBFF;  // Pressed state

    // Semantic Color Aliases (using Spectrum tokens)
    // Surface hierarchy (dark to light for elevation)
    constexpr u32 COLOR_BACKGROUND          = GRAY_100;  // Main canvas/work area
    constexpr u32 COLOR_BACKGROUND_DISABLED = GRAY_75;
    constexpr u32 COLOR_TITLEBAR            = GRAY_50;   // Darkest - top bar
    constexpr u32 COLOR_PANEL               = GRAY_200;  // Sidebars, panels
    constexpr u32 COLOR_PANEL_HEADER        = GRAY_300;  // Panel headers
    constexpr u32 COLOR_BORDER              = GRAY_500;  // Visible borders
    constexpr u32 COLOR_RESIZER             = 0x1E1E1EFF;  // Panel resizers
    constexpr u32 COLOR_RESIZER_HOVER       = 0x262626FF;  // Resizer hover/drag

    // Interactive elements (buttons stand out from panels)
    constexpr u32 COLOR_BUTTON              = GRAY_400;  // Default button
    constexpr u32 COLOR_BUTTON_HOVER        = GRAY_500;  // Button hover
    constexpr u32 COLOR_BUTTON_PRESSED      = GRAY_600;  // Button pressed
    constexpr u32 COLOR_INPUT               = GRAY_75;   // Text fields, dark inset

    // Text
    constexpr u32 COLOR_TEXT                = GRAY_800;
    constexpr u32 COLOR_TEXT_DIM            = GRAY_600;
    constexpr u32 COLOR_TEXT_DISABLED       = GRAY_500;

    // Accent colors
    constexpr u32 COLOR_ACCENT              = BLUE_800;
    constexpr u32 COLOR_ACCENT_HOVER        = BLUE_700;
    constexpr u32 COLOR_ACCENT_PRESSED      = BLUE_900;
    constexpr u32 COLOR_SELECTION           = 0x4B75FF40;  // BLUE_800 @ 25% alpha
    constexpr u32 COLOR_FOCUS               = BLUE_700;

    // Legacy aliases (for gradual migration)
    constexpr u32 COLOR_HOVER               = GRAY_500;
    constexpr u32 COLOR_ACTIVE              = GRAY_600;
    constexpr u32 COLOR_PRESSED             = BLUE_900;

    // Checkerboard (transparency indicator) - muted for dark theme
    constexpr u32 CHECKER_SIZE = 8;
    constexpr u32 CHECKER_COLOR1 = 0x505050FF;  // Dark square
    constexpr u32 CHECKER_COLOR2 = 0x787878FF;  // Light square

    // Misc
    constexpr u32 MAX_LAYERS = 256;
    constexpr f32 SCROLL_SPEED = 20.0f;
    constexpr u32 DOUBLE_CLICK_MS = 400;

    // Undo/Redo
    constexpr u32 MAX_UNDO_STEPS = 20;
}

#endif
