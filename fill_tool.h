#ifndef _H_FILL_TOOL_
#define _H_FILL_TOOL_

#include "tool.h"
#include "app_state.h"
#include <queue>
#include <cmath>
#include <limits>

class FillTool : public Tool {
public:
    FillTool() : Tool(ToolType::Fill, "Fill") {}

    void onMouseDown(Document& doc, const ToolEvent& e) override;

private:
    static void expandLayerToDocument(PixelLayer* layer, u32 docWidth, u32 docHeight);
    static f32 colorDifference(u32 a, u32 b);

    static void floodFill(TiledCanvas& canvas, i32 startX, i32 startY,
                          u32 targetColor, u32 fillColor, f32 tolerance,
                          const Selection* sel = nullptr,
                          i32 layerOffsetX = 0, i32 layerOffsetY = 0,
                          i32 docWidth = 0, i32 docHeight = 0);

    static void globalFill(TiledCanvas& canvas, u32 targetColor, u32 fillColor, f32 tolerance,
                           const Selection* sel = nullptr,
                           i32 layerOffsetX = 0, i32 layerOffsetY = 0,
                           i32 docWidth = 0, i32 docHeight = 0);
};

#endif
