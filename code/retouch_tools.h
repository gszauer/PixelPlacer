#ifndef _H_RETOUCH_TOOLS_
#define _H_RETOUCH_TOOLS_

#include "tool.h"
#include "brush_renderer.h"
#include "app_state.h"
#include <memory>
#include <vector>

// Clone tool
class CloneTool : public Tool {
public:
    bool stroking = false;
    Vec2 lastPos;
    Vec2 firstStrokePos;  // First position of current stroke (for offset calculation)
    BrushRenderer::BrushStamp stamp;
    bool stampDirty = true;
    f32 cachedSize = 0.0f;
    f32 cachedHardness = 0.0f;

    // Snapshot of the layer at stroke start - we read from this to avoid sampling newly cloned pixels
    std::unique_ptr<TiledCanvas> sourceSnapshot;

    // Layer transform support
    PixelLayer* strokeLayer = nullptr;
    Matrix3x2 docToLayerTransform;
    Matrix3x2 layerToDocTransform;

    // Selection support
    const Selection* strokeSelection = nullptr;

    CloneTool() : Tool(ToolType::Clone, "Clone") {}

    void onMouseDown(Document& doc, const ToolEvent& e) override;
    void onMouseDrag(Document& doc, const ToolEvent& e) override;
    void onMouseUp(Document& doc, const ToolEvent& e) override;

    bool hasOverlay() const override { return true; }
    void renderOverlay(Framebuffer& fb, const Vec2& cursorPos, f32 zoom, const Vec2& pan, const Recti& clipRect = Recti()) override;

private:
    void updateStamp();
    void cloneAt(TiledCanvas& canvas, const Vec2& destPos, f32 pressure, const Matrix3x2& docToLayer);
};

// Smudge tool - finger painting that picks up and pushes color
class SmudgeTool : public Tool {
public:
    bool stroking = false;
    Vec2 lastPos;
    BrushRenderer::BrushStamp stamp;
    f32 cachedSize = 0.0f;
    f32 cachedHardness = 0.0f;

    // Carried color buffer - colors picked up and pushed along
    std::vector<u32> carriedColors;
    u32 carriedSize = 0;

    // Layer transform support
    PixelLayer* strokeLayer = nullptr;
    Matrix3x2 docToLayerTransform;
    Matrix3x2 layerToDocTransform;

    // Selection support
    const Selection* strokeSelection = nullptr;

    SmudgeTool() : Tool(ToolType::Smudge, "Smudge") {}

    void onMouseDown(Document& doc, const ToolEvent& e) override;
    void onMouseDrag(Document& doc, const ToolEvent& e) override;
    void onMouseUp(Document& doc, const ToolEvent& e) override;

    bool hasOverlay() const override { return true; }
    void renderOverlay(Framebuffer& fb, const Vec2& cursorPos, f32 zoom, const Vec2& pan, const Recti& clipRect = Recti()) override;

private:
    void updateStamp();
    void sampleCarriedColors(TiledCanvas& canvas, const Vec2& layerPos);
    void smudgeAt(TiledCanvas& canvas, const Vec2& layerPos, f32 pressure);
};

// Dodge tool (lighten)
class DodgeTool : public Tool {
public:
    bool stroking = false;
    Vec2 lastPos;
    BrushRenderer::BrushStamp stamp;
    f32 cachedSize = 0.0f;
    f32 cachedHardness = 0.0f;

    // Layer transform support
    PixelLayer* strokeLayer = nullptr;
    Matrix3x2 docToLayerTransform;
    Matrix3x2 layerToDocTransform;

    // Selection support
    const Selection* strokeSelection = nullptr;

    DodgeTool() : Tool(ToolType::Dodge, "Dodge") {}

    void onMouseDown(Document& doc, const ToolEvent& e) override;
    void onMouseDrag(Document& doc, const ToolEvent& e) override;
    void onMouseUp(Document& doc, const ToolEvent& e) override;

    bool hasOverlay() const override { return true; }
    void renderOverlay(Framebuffer& fb, const Vec2& cursorPos, f32 zoom, const Vec2& pan, const Recti& clipRect = Recti()) override;

private:
    void updateStamp();
    void dodgeAt(TiledCanvas& canvas, const Vec2& layerPos, f32 pressure);
};

// Burn tool (darken)
class BurnTool : public Tool {
public:
    bool stroking = false;
    Vec2 lastPos;
    BrushRenderer::BrushStamp stamp;
    f32 cachedSize = 0.0f;
    f32 cachedHardness = 0.0f;

    // Layer transform support
    PixelLayer* strokeLayer = nullptr;
    Matrix3x2 docToLayerTransform;
    Matrix3x2 layerToDocTransform;

    // Selection support
    const Selection* strokeSelection = nullptr;

    BurnTool() : Tool(ToolType::Burn, "Burn") {}

    void onMouseDown(Document& doc, const ToolEvent& e) override;
    void onMouseDrag(Document& doc, const ToolEvent& e) override;
    void onMouseUp(Document& doc, const ToolEvent& e) override;

    bool hasOverlay() const override { return true; }
    void renderOverlay(Framebuffer& fb, const Vec2& cursorPos, f32 zoom, const Vec2& pan, const Recti& clipRect = Recti()) override;

private:
    void updateStamp();
    void burnAt(TiledCanvas& canvas, const Vec2& layerPos, f32 pressure);
};

#endif
