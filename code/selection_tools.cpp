#include "selection_tools.h"
#include <queue>
#include <cmath>

// RectangleSelectTool implementations
void RectangleSelectTool::onMouseDown(Document& doc, const ToolEvent& e) {
    startPos = e.position;
    selecting = true;
    addMode = e.shiftHeld;
    subtractMode = e.altHeld;
}

void RectangleSelectTool::onMouseDrag(Document& doc, const ToolEvent& e) {
    if (!selecting) return;
    // Preview rectangle (handled by overlay)
}

void RectangleSelectTool::onMouseUp(Document& doc, const ToolEvent& e) {
    if (!selecting) return;
    selecting = false;

    Vec2 endPos = e.position;

    // Clamp both positions to document bounds
    f32 clampedStartX = std::max(0.0f, std::min(startPos.x, static_cast<f32>(doc.width)));
    f32 clampedStartY = std::max(0.0f, std::min(startPos.y, static_cast<f32>(doc.height)));
    f32 clampedEndX = std::max(0.0f, std::min(endPos.x, static_cast<f32>(doc.width)));
    f32 clampedEndY = std::max(0.0f, std::min(endPos.y, static_cast<f32>(doc.height)));

    i32 x = static_cast<i32>(std::min(clampedStartX, clampedEndX));
    i32 y = static_cast<i32>(std::min(clampedStartY, clampedEndY));
    i32 w = static_cast<i32>(std::abs(clampedEndX - clampedStartX));
    i32 h = static_cast<i32>(std::abs(clampedEndY - clampedStartY));

    if (w > 0 && h > 0) {
        bool antiAlias = getAppState().selectionAntiAlias;
        doc.selection.setRectangle(Recti(x, y, w, h), addMode, subtractMode, antiAlias);
        doc.notifySelectionChanged();
    } else if (!addMode && !subtractMode) {
        // Single click without drag - deselect if clicking outside selection
        i32 clickX = static_cast<i32>(e.position.x);
        i32 clickY = static_cast<i32>(e.position.y);
        if (doc.selection.hasSelection && !doc.selection.isSelected(clickX, clickY)) {
            doc.selection.clear();
            doc.notifySelectionChanged();
        }
    }
}

void RectangleSelectTool::renderOverlay(Framebuffer& fb, const Vec2& cursorPos, f32 zoom, const Vec2& pan, const Recti& clipRect) {
    // Draw selection rectangle preview (marching ants)
    // This would need to transform coordinates appropriately
}

// EllipseSelectTool implementations
void EllipseSelectTool::onMouseDown(Document& doc, const ToolEvent& e) {
    startPos = e.position;
    selecting = true;
    addMode = e.shiftHeld;
    subtractMode = e.altHeld;
}

void EllipseSelectTool::onMouseUp(Document& doc, const ToolEvent& e) {
    if (!selecting) return;
    selecting = false;

    Vec2 endPos = e.position;

    // Clamp both positions to document bounds
    f32 clampedStartX = std::max(0.0f, std::min(startPos.x, static_cast<f32>(doc.width)));
    f32 clampedStartY = std::max(0.0f, std::min(startPos.y, static_cast<f32>(doc.height)));
    f32 clampedEndX = std::max(0.0f, std::min(endPos.x, static_cast<f32>(doc.width)));
    f32 clampedEndY = std::max(0.0f, std::min(endPos.y, static_cast<f32>(doc.height)));

    i32 x = static_cast<i32>(std::min(clampedStartX, clampedEndX));
    i32 y = static_cast<i32>(std::min(clampedStartY, clampedEndY));
    i32 w = static_cast<i32>(std::abs(clampedEndX - clampedStartX));
    i32 h = static_cast<i32>(std::abs(clampedEndY - clampedStartY));

    if (w > 0 && h > 0) {
        bool antiAlias = getAppState().selectionAntiAlias;
        doc.selection.setEllipse(Recti(x, y, w, h), addMode, subtractMode, antiAlias);
        doc.notifySelectionChanged();
    } else if (!addMode && !subtractMode) {
        // Single click without drag - deselect if clicking outside selection
        i32 clickX = static_cast<i32>(e.position.x);
        i32 clickY = static_cast<i32>(e.position.y);
        if (doc.selection.hasSelection && !doc.selection.isSelected(clickX, clickY)) {
            doc.selection.clear();
            doc.notifySelectionChanged();
        }
    }
}

// FreeSelectTool implementations
Vec2 FreeSelectTool::clampToDoc(const Vec2& p, const Document& doc) {
    return Vec2(
        std::max(0.0f, std::min(p.x, static_cast<f32>(doc.width))),
        std::max(0.0f, std::min(p.y, static_cast<f32>(doc.height)))
    );
}

void FreeSelectTool::onMouseDown(Document& doc, const ToolEvent& e) {
    points.clear();
    points.push_back(clampToDoc(e.position, doc));
    selecting = true;
    addMode = e.shiftHeld;
    subtractMode = e.altHeld;
}

void FreeSelectTool::onMouseDrag(Document& doc, const ToolEvent& e) {
    if (!selecting) return;

    // Add point if far enough from last point
    if (!points.empty()) {
        Vec2 clamped = clampToDoc(e.position, doc);
        Vec2 last = points.back();
        if (Vec2::distance(last, clamped) > 2.0f) {
            points.push_back(clamped);
        }
    }
}

void FreeSelectTool::onMouseUp(Document& doc, const ToolEvent& e) {
    if (!selecting) return;
    selecting = false;

    if (points.size() >= 3) {
        bool antiAlias = getAppState().selectionAntiAlias;
        doc.selection.setPolygon(points, addMode, subtractMode, antiAlias);
        doc.notifySelectionChanged();
    } else if (!addMode && !subtractMode) {
        // Click without valid selection - deselect if clicking outside selection
        i32 clickX = static_cast<i32>(e.position.x);
        i32 clickY = static_cast<i32>(e.position.y);
        if (doc.selection.hasSelection && !doc.selection.isSelected(clickX, clickY)) {
            doc.selection.clear();
            doc.notifySelectionChanged();
        }
    }

    points.clear();
}

// PolygonSelectTool implementations
Vec2 PolygonSelectTool::clampToDoc(const Vec2& p, const Document& doc) {
    return Vec2(
        std::max(0.0f, std::min(p.x, static_cast<f32>(doc.width))),
        std::max(0.0f, std::min(p.y, static_cast<f32>(doc.height)))
    );
}

void PolygonSelectTool::onMouseDown(Document& doc, const ToolEvent& e) {
    Vec2 clamped = clampToDoc(e.position, doc);
    u64 currentTime = Platform::getMilliseconds();
    bool isDoubleClick = (currentTime - lastClickTime) < DOUBLE_CLICK_TIME;
    lastClickTime = currentTime;

    if (!active) {
        // Starting new polygon - check if we should clear existing selection
        if (!e.shiftHeld && !e.altHeld) {
            i32 clickX = static_cast<i32>(e.position.x);
            i32 clickY = static_cast<i32>(e.position.y);
            if (doc.selection.hasSelection && !doc.selection.isSelected(clickX, clickY)) {
                doc.selection.clear();
                doc.notifySelectionChanged();
            }
        }
        // Start new polygon
        points.clear();
        points.push_back(clamped);
        active = true;
        addMode = e.shiftHeld;
        subtractMode = e.altHeld;
    } else {
        // Check if closing polygon via double-click
        if (isDoubleClick && points.size() >= 3) {
            finishPolygon(doc);
            return;
        }

        // Add point
        points.push_back(clamped);

        // Check if closing polygon (near start)
        if (points.size() >= 3 && Vec2::distance(points.front(), clamped) < 10.0f) {
            finishPolygon(doc);
        }
    }
}

void PolygonSelectTool::onKeyDown(Document& doc, i32 keyCode) {
    // Enter or Escape to finish
    if (keyCode == 13) { // Enter
        finishPolygon(doc);
    } else if (keyCode == 27) { // Escape
        points.clear();
        active = false;
    }
}

void PolygonSelectTool::finishPolygon(Document& doc) {
    if (points.size() >= 3) {
        bool antiAlias = getAppState().selectionAntiAlias;
        doc.selection.setPolygon(points, addMode, subtractMode, antiAlias);
        doc.notifySelectionChanged();
    }
    points.clear();
    active = false;
}

// MagicWandTool implementations
void MagicWandTool::onMouseDown(Document& doc, const ToolEvent& e) {
    PixelLayer* layer = doc.getActivePixelLayer();
    if (!layer) return;

    AppState& state = getAppState();
    f32 tolerance = state.wandTolerance;
    bool contiguous = state.wandContiguous;

    i32 x = static_cast<i32>(e.position.x);
    i32 y = static_cast<i32>(e.position.y);

    if (x < 0 || y < 0 || x >= static_cast<i32>(layer->canvas.width) ||
        y >= static_cast<i32>(layer->canvas.height)) {
        return;
    }

    u32 targetColor = layer->canvas.getPixel(x, y);

    bool addMode = e.shiftHeld;
    bool subtractMode = e.altHeld;

    if (!addMode && !subtractMode) {
        doc.selection.clear();
    }

    if (contiguous) {
        floodSelect(doc.selection, layer->canvas, x, y, targetColor, tolerance, addMode, subtractMode);
    } else {
        globalSelect(doc.selection, layer->canvas, targetColor, tolerance, addMode, subtractMode);
    }

    doc.selection.updateBounds();
    doc.notifySelectionChanged();
}

f32 MagicWandTool::colorDifference(u32 a, u32 b) {
    u8 ar, ag, ab, aa;
    u8 br, bg, bb, ba;
    Blend::unpack(a, ar, ag, ab, aa);
    Blend::unpack(b, br, bg, bb, ba);

    f32 dr = static_cast<f32>(ar) - br;
    f32 dg = static_cast<f32>(ag) - bg;
    f32 db = static_cast<f32>(ab) - bb;
    f32 da = static_cast<f32>(aa) - ba;

    return std::sqrt(dr * dr + dg * dg + db * db + da * da);
}

void MagicWandTool::floodSelect(Selection& sel, const TiledCanvas& canvas,
                                i32 startX, i32 startY, u32 targetColor,
                                f32 tolerance, bool add, bool subtract) {
    u32 w = canvas.width;
    u32 h = canvas.height;

    std::vector<bool> visited(w * h, false);
    std::queue<std::pair<i32, i32>> queue;

    queue.push({startX, startY});
    visited[startY * w + startX] = true;

    while (!queue.empty()) {
        auto [x, y] = queue.front();
        queue.pop();

        u32 currentColor = canvas.getPixel(x, y);
        if (colorDifference(currentColor, targetColor) > tolerance) continue;

        if (subtract) {
            sel.setValue(x, y, 0);
        } else {
            sel.setValue(x, y, 255);
        }

        const i32 dx[] = {-1, 1, 0, 0};
        const i32 dy[] = {0, 0, -1, 1};

        for (i32 i = 0; i < 4; ++i) {
            i32 nx = x + dx[i];
            i32 ny = y + dy[i];

            if (nx >= 0 && ny >= 0 && nx < static_cast<i32>(w) && ny < static_cast<i32>(h)) {
                i32 idx = ny * w + nx;
                if (!visited[idx]) {
                    u32 neighborColor = canvas.getPixel(nx, ny);
                    if (colorDifference(neighborColor, targetColor) <= tolerance) {
                        visited[idx] = true;
                        queue.push({nx, ny});
                    }
                }
            }
        }
    }
}

void MagicWandTool::globalSelect(Selection& sel, const TiledCanvas& canvas,
                                 u32 targetColor, f32 tolerance, bool add, bool subtract) {
    for (u32 y = 0; y < canvas.height; ++y) {
        for (u32 x = 0; x < canvas.width; ++x) {
            u32 currentColor = canvas.getPixel(x, y);
            if (colorDifference(currentColor, targetColor) <= tolerance) {
                if (subtract) {
                    sel.setValue(x, y, 0);
                } else {
                    sel.setValue(x, y, 255);
                }
            }
        }
    }
}
