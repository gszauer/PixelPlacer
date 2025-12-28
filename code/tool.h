#ifndef _H_TOOL_
#define _H_TOOL_

#include "types.h"
#include "primitives.h"
#include "document.h"
#include "app_state.h"
#include "config.h"
#include <string>
#include <cmath>
#include <cstdio>  // Debug logging

// Tool types
enum class ToolType {
    // Selection
    RectangleSelect,
    EllipseSelect,
    FreeSelect,
    PolygonSelect,
    MagicWand,

    // Transform
    Move,
    Crop,

    // Painting
    Brush,
    Eraser,
    Fill,
    Gradient,

    // Retouching
    Clone,
    Heal,
    Smudge,
    Dodge,
    Burn,

    // Other
    ColorPicker,
    Pan,
    Zoom
};

// Base tool class
class Tool {
public:
    ToolType type;
    std::string name;
    std::string tooltip;

    Tool(ToolType t, const std::string& n) : type(t), name(n) {}
    virtual ~Tool() = default;

    // Event handlers
    virtual void onMouseDown(Document& doc, const ToolEvent& e) {}
    virtual void onMouseDrag(Document& doc, const ToolEvent& e) {}
    virtual void onMouseUp(Document& doc, const ToolEvent& e) {}
    virtual void onMouseMove(Document& doc, const ToolEvent& e) {}
    virtual void onKeyDown(Document& doc, i32 keyCode) {}
    virtual void onKeyUp(Document& doc, i32 keyCode) {}

    // Cursor rendering (tool-specific cursor overlay)
    // cursorPos is the cursor position in screen coordinates
    // pan is the screen offset for converting document to screen coords: screenPos = docPos * zoom + pan
    virtual bool hasOverlay() const { return false; }
    virtual void renderOverlay(Framebuffer& fb, const Vec2& cursorPos, f32 zoom, const Vec2& pan, const Recti& clipRect = Recti()) {}

    // Tool options (shown in tool options bar)
    virtual void renderOptions(Widget& container) {}
};

// Pan tool
class PanTool : public Tool {
public:
    Vec2 lastPos;
    bool dragging = false;

    PanTool() : Tool(ToolType::Pan, "Pan") {}

    void onMouseDown(Document& doc, const ToolEvent& e) override {
        lastPos = e.position;
        dragging = true;
    }

    void onMouseDrag(Document& doc, const ToolEvent& e) override {
        if (dragging) {
            // Pan is handled by the view, not the document
            lastPos = e.position;
        }
    }

    void onMouseUp(Document& doc, const ToolEvent& e) override {
        dragging = false;
    }
};

// Zoom tool
class ZoomTool : public Tool {
public:
    ZoomTool() : Tool(ToolType::Zoom, "Zoom") {}

    void onMouseDown(Document& doc, const ToolEvent& e) override {
        // Zoom is handled by the view
        // Left click = zoom in, Alt+click = zoom out
    }
};

// Transform handle types
enum class TransformHandle {
    None,
    Move,           // Clicking inside bounds
    TopLeft,        // Corner handles (rotate or scale based on mode)
    TopRight,
    BottomLeft,
    BottomRight,
    Top,            // Scale handles (edges)
    Bottom,
    Left,
    Right,
    Pivot           // Center pivot point
};

// Corner behavior mode
enum class CornerBehavior {
    Rotate,
    Scale
};

// Move/Transform tool
class MoveTool : public Tool {
public:
    Vec2 startPos;
    Vec2 lastPos;
    Transform originalTransform;
    bool dragging = false;
    bool movingSelection = false;  // True if moving selection, false if moving layer
    bool movingContent = false;    // True if also moving pixels with selection

    // Floating content when moving selection + content
    std::unique_ptr<TiledCanvas> floatingPixels;
    Recti floatingOrigin;  // Original bounds of the floating content

    // Corner behavior (configurable via tool options)
    CornerBehavior cornerBehavior = CornerBehavior::Rotate;

    // Transform tool state
    TransformHandle activeHandle = TransformHandle::None;
    Vec2 corners[4];      // TL, TR, BR, BL in document space after transform
    Vec2 pivotPos;        // Pivot in document space
    Vec2 center;          // Center of bounding box
    f32 startAngle = 0.0f;
    f32 originalRotation = 0.0f;
    Vec2 originalScale;
    Vec2 scaleAnchor;     // Anchor point for scaling (the pivot)

    // Visual sizes (in screen pixels, will be scaled by UI_SCALE)
    static constexpr f32 LINE_THICKNESS = 2.0f;
    static constexpr f32 CORNER_NOTCH_SIZE = 6.0f;
    static constexpr f32 EDGE_HANDLE_SIZE = 4.0f;
    static constexpr f32 EDGE_INTERACT_RADIUS = 4.0f;
    static constexpr f32 CORNER_INTERACT_RADIUS = 10.0f;
    static constexpr f32 PIVOT_INTERACT_RADIUS = 12.0f;

    MoveTool() : Tool(ToolType::Move, "Move") {}

    // Cached content bounds for the current layer
    Recti contentBounds;
    bool hasContent = false;

    // Cached canvas dimensions for the current layer
    f32 canvasWidth = 0, canvasHeight = 0;

    // Track which layer we last initialized pivot for
    const LayerBase* lastInitializedLayer = nullptr;

    // Method declarations
    void initializePivotToContentCenter(PixelLayer* layer);
    void initializePivotToContentCenter(TextLayer* layer);
    void updateCorners(const PixelLayer* layer);
    void updateCorners(TextLayer* layer);

    Vec2 getEdgeMidpoint(i32 edge) const {
        return (corners[edge] + corners[(edge + 1) % 4]) * 0.5f;
    }

    bool pointInQuad(const Vec2& p) const;
    f32 distanceToEdge(const Vec2& p, const Vec2& a, const Vec2& b) const;
    TransformHandle hitTest(const Vec2& pos, f32 zoom);

    void onMouseDown(Document& doc, const ToolEvent& e) override;
    void onMouseDrag(Document& doc, const ToolEvent& e) override;
    void onMouseUp(Document& doc, const ToolEvent& e) override;
    void onKeyDown(Document& doc, i32 keyCode) override;

    bool hasOverlay() const override { return true; }
    void renderOverlay(Framebuffer& fb, const Vec2& cursorPos, f32 zoom, const Vec2& pan, const Recti& clipRect = Recti()) override;
};

// Color picker tool
class ColorPickerTool : public Tool {
public:
    ColorPickerTool() : Tool(ToolType::ColorPicker, "Color Picker") {}

    void onMouseDown(Document& doc, const ToolEvent& e) override {
        pickColor(doc, e, false);
    }

    void onMouseDrag(Document& doc, const ToolEvent& e) override {
        pickColor(doc, e, false);
    }

    void onMouseUp(Document& doc, const ToolEvent& e) override {
        pickColor(doc, e, true);
    }

private:
    void pickColor(Document& doc, const ToolEvent& e, bool log);
};

#endif
