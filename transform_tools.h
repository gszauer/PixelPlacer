#ifndef _H_TRANSFORM_TOOLS_
#define _H_TRANSFORM_TOOLS_

#include "tool.h"

// Crop handle types
enum class CropHandle {
    None,
    TopLeft, Top, TopRight,
    Left, Center, Right,
    BottomLeft, Bottom, BottomRight
};

// Crop tool
class CropTool : public Tool {
public:
    // Crop rectangle in document coordinates
    Recti cropRect;
    bool initialized = false;

    // Dragging state
    CropHandle activeHandle = CropHandle::None;
    Vec2 dragStart;
    Recti dragStartRect;

    CropTool() : Tool(ToolType::Crop, "Crop") {}

    void initializeCropRect(Document& doc);
    void reset(Document& doc);
    void apply(Document& doc);
    CropHandle hitTest(const Vec2& pos, f32 zoom);

    void onMouseDown(Document& doc, const ToolEvent& e) override;
    void onMouseDrag(Document& doc, const ToolEvent& e) override;
    void onMouseUp(Document& doc, const ToolEvent& e) override;
    void onMouseMove(Document& doc, const ToolEvent& e) override;
    void onKeyDown(Document& doc, i32 keyCode) override;

    bool hasOverlay() const override { return true; }
    void renderOverlay(Framebuffer& fb, const Vec2& cursorPos, f32 zoom, const Vec2& pan, const Recti& clipRectParam = Recti()) override;
};

// Gradient tool
class GradientTool : public Tool {
public:
    Vec2 startPos;
    Vec2 endPos;
    bool dragging = false;

    GradientTool() : Tool(ToolType::Gradient, "Gradient") {}

    void onMouseDown(Document& doc, const ToolEvent& e) override;
    void onMouseDrag(Document& doc, const ToolEvent& e) override;
    void onMouseUp(Document& doc, const ToolEvent& e) override;

    bool hasOverlay() const override { return dragging; }

private:
    static void expandLayerToDocument(PixelLayer* layer, u32 docWidth, u32 docHeight);
    void applyLinearGradient(TiledCanvas& canvas, const Selection& sel,
                            const Vec2& start, const Vec2& end,
                            const Color& color1, const Color& color2,
                            i32 layerOffsetX, i32 layerOffsetY,
                            i32 docWidth, i32 docHeight);
    void applyRadialGradient(TiledCanvas& canvas, const Selection& sel,
                            const Vec2& center, const Vec2& edge,
                            const Color& color1, const Color& color2,
                            i32 layerOffsetX, i32 layerOffsetY,
                            i32 docWidth, i32 docHeight);
};

#endif
