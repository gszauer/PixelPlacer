#ifndef _H_LAYOUTS_
#define _H_LAYOUTS_

#include "widget.h"
#include <algorithm>

// Horizontal box layout
class HBoxLayout : public Widget {
public:
    f32 spacing = 4.0f;
    bool stretch = true;  // Stretch children to fill height

    HBoxLayout() = default;
    explicit HBoxLayout(f32 sp) : spacing(sp) {}

    void layout() override;
};

// Vertical box layout
class VBoxLayout : public Widget {
public:
    f32 spacing = 4.0f;
    bool stretch = true;  // Stretch children to fill width

    VBoxLayout() = default;
    explicit VBoxLayout(f32 sp) : spacing(sp) {}

    void layout() override;
};

// Grid layout
class GridLayout : public Widget {
public:
    u32 columns = 2;
    f32 hSpacing = 4.0f;
    f32 vSpacing = 4.0f;
    bool uniformCells = false;

    GridLayout() = default;
    GridLayout(u32 cols, f32 hsp = 4.0f, f32 vsp = 4.0f)
        : columns(cols), hSpacing(hsp), vSpacing(vsp) {}

    void layout() override;
};

// Stack layout (overlapping children)
class StackLayout : public Widget {
public:
    void layout() override;
};

// Forward declaration for getAppState
struct AppState;
AppState& getAppState();

// ScrollView - scrollable container with vertical scrollbar
class ScrollView : public Widget {
public:
    f32 scrollOffset = 0.0f;      // Current scroll position (pixels from top)
    f32 contentHeight = 0.0f;     // Total height of content
    f32 scrollSpeed = 20.0f;      // Pixels per wheel notch
    bool showScrollbar = true;    // Whether to show scrollbar
    f32 scrollbarWidth = 8.0f * Config::uiScale;
    f32 scrollbarMargin = 4.0f * Config::uiScale;  // Gap between content and scrollbar

private:
    bool draggingScrollbar = false;
    f32 dragStartY = 0.0f;
    f32 dragStartOffset = 0.0f;

    f32 getMaxScroll() const;
    f32 getViewportHeight() const;
    void clampScroll();
    Rect getScrollbarThumbRect() const;

public:
    ScrollView() = default;

    void layout() override;
    f32 calculateContentHeight(Widget* widget);
    void renderSelf(Framebuffer& fb) override;
    void render(Framebuffer& fb) override;
    bool onMouseWheel(const MouseEvent& e) override;
    bool onMouseDown(const MouseEvent& e) override;
    bool onMouseDrag(const MouseEvent& e) override;
    bool onMouseUp(const MouseEvent& e) override;
    bool onMouseMove(const MouseEvent& e) override;
    Widget* findWidgetAt(const Vec2& point) override;
    void ensureVisible(Widget* widget);
    void scrollToTop();
    void scrollToBottom();
};

#endif
