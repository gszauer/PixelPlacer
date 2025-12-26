#ifndef _H_SELECTION_TOOLS_
#define _H_SELECTION_TOOLS_

#include "tool.h"
#include "selection.h"
#include "platform.h"
#include "app_state.h"
#include <vector>

// Rectangle selection tool
class RectangleSelectTool : public Tool {
public:
    Vec2 startPos;
    bool selecting = false;
    bool addMode = false;
    bool subtractMode = false;

    RectangleSelectTool() : Tool(ToolType::RectangleSelect, "Rectangle Select") {}

    void onMouseDown(Document& doc, const ToolEvent& e) override;
    void onMouseDrag(Document& doc, const ToolEvent& e) override;
    void onMouseUp(Document& doc, const ToolEvent& e) override;

    bool hasOverlay() const override { return selecting; }
    void renderOverlay(Framebuffer& fb, const Vec2& cursorPos, f32 zoom, const Vec2& pan, const Recti& clipRect = Recti()) override;
};

// Ellipse selection tool
class EllipseSelectTool : public Tool {
public:
    Vec2 startPos;
    bool selecting = false;
    bool addMode = false;
    bool subtractMode = false;

    EllipseSelectTool() : Tool(ToolType::EllipseSelect, "Ellipse Select") {}

    void onMouseDown(Document& doc, const ToolEvent& e) override;
    void onMouseUp(Document& doc, const ToolEvent& e) override;
};

// Free selection (lasso) tool
class FreeSelectTool : public Tool {
public:
    std::vector<Vec2> points;
    bool selecting = false;
    bool addMode = false;
    bool subtractMode = false;

    FreeSelectTool() : Tool(ToolType::FreeSelect, "Free Select") {}

    void onMouseDown(Document& doc, const ToolEvent& e) override;
    void onMouseDrag(Document& doc, const ToolEvent& e) override;
    void onMouseUp(Document& doc, const ToolEvent& e) override;

private:
    static Vec2 clampToDoc(const Vec2& p, const Document& doc);
};

// Polygon selection tool
class PolygonSelectTool : public Tool {
public:
    std::vector<Vec2> points;
    bool active = false;
    bool addMode = false;
    bool subtractMode = false;
    u64 lastClickTime = 0;
    static constexpr u64 DOUBLE_CLICK_TIME = 400;  // milliseconds

    PolygonSelectTool() : Tool(ToolType::PolygonSelect, "Polygon Select") {}

    void onMouseDown(Document& doc, const ToolEvent& e) override;
    void onKeyDown(Document& doc, i32 keyCode) override;

private:
    static Vec2 clampToDoc(const Vec2& p, const Document& doc);
    void finishPolygon(Document& doc);
};

// Magic wand selection tool
class MagicWandTool : public Tool {
public:
    MagicWandTool() : Tool(ToolType::MagicWand, "Magic Wand") {}

    void onMouseDown(Document& doc, const ToolEvent& e) override;

private:
    static f32 colorDifference(u32 a, u32 b);
    static void floodSelect(Selection& sel, const TiledCanvas& canvas,
                           i32 startX, i32 startY, u32 targetColor,
                           f32 tolerance, bool add, bool subtract);
    static void globalSelect(Selection& sel, const TiledCanvas& canvas,
                            u32 targetColor, f32 tolerance, bool add, bool subtract);
};

#endif
