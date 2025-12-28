#ifndef _H_BRUSH_TOOL_
#define _H_BRUSH_TOOL_

#include "tool.h"
#include "brush_renderer.h"
#include "app_state.h"
#include <memory>

class BrushTool : public Tool {
public:
    // Brush settings (from app state)
    f32 size = 10.0f;
    f32 hardness = 0.8f;
    f32 opacity = 1.0f;      // Stroke ceiling (applied when compositing buffer to layer)
    f32 flow = 1.0f;         // Per-dab opacity (applied when rendering to buffer)
    f32 spacing = 0.25f;
    i32 pressureMode = 0;    // 0=None, 1=Size, 2=Opacity, 3=Flow

    // Custom brush tip support
    CustomBrushTip* currentTip = nullptr;  // nullptr = round brush
    f32 currentAngle = 0.0f;
    i32 currentTipIndex = -1;  // Cached to detect changes
    BrushDynamics dynamics;    // Cached dynamics settings

    // Current stroke state
    bool stroking = false;
    Vec2 lastPos;
    Vec2 lastLayerPos;       // For stroke buffer rendering
    i32 lastPixelX = 0;      // For pencil mode
    i32 lastPixelY = 0;
    BrushRenderer::BrushStamp currentStamp;
    bool stampDirty = true;

    // Stroke buffer for Photoshop-style rendering
    // Dabs blend freely into buffer, then buffer is composited with opacity on mouseUp
    std::unique_ptr<TiledCanvas> strokeBuffer;
    Rect strokeBounds;

    // Cached values for current stroke
    u32 strokeColor = 0;
    PixelLayer* strokeLayer = nullptr;

    BrushTool() : Tool(ToolType::Brush, "Brush") {}

    // Check if we're in pencil mode (size == 1)
    bool isPencilMode() const { return size <= 1.0f; }

    // Accessors for compositor to render stroke preview
    TiledCanvas* getStrokeBuffer() const { return strokeBuffer.get(); }
    bool isStroking() const { return stroking; }
    f32 getStrokeOpacity() const { return opacity; }
    Rect getStrokeBounds() const { return strokeBounds; }
    PixelLayer* getStrokeLayer() const { return strokeLayer; }

    void updateFromAppState();
    f32 applyPressureCurve(f32 rawPressure);
    void ensureStamp();

    void onMouseDown(Document& doc, const ToolEvent& e) override;
    void onMouseDrag(Document& doc, const ToolEvent& e) override;
    void onMouseUp(Document& doc, const ToolEvent& e) override;

    bool hasOverlay() const override { return true; }
    void renderOverlay(Framebuffer& fb, const Vec2& cursorPos, f32 zoom, const Vec2& pan, const Recti& clipRect = Recti()) override;
};

#endif
