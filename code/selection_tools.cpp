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
        doc.recordSelectionChange("Rectangle Select");
        bool antiAlias = getAppState().selectionAntiAlias;
        doc.selection.setRectangle(Recti(x, y, w, h), addMode, subtractMode, antiAlias);
        doc.notifySelectionChanged();
    } else if (!addMode && !subtractMode) {
        // Single click without drag - deselect if clicking outside selection
        i32 clickX = static_cast<i32>(e.position.x);
        i32 clickY = static_cast<i32>(e.position.y);
        if (doc.selection.hasSelection && !doc.selection.isSelected(clickX, clickY)) {
            doc.recordSelectionChange("Deselect");
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
        doc.recordSelectionChange("Ellipse Select");
        bool antiAlias = getAppState().selectionAntiAlias;
        doc.selection.setEllipse(Recti(x, y, w, h), addMode, subtractMode, antiAlias);
        doc.notifySelectionChanged();
    } else if (!addMode && !subtractMode) {
        // Single click without drag - deselect if clicking outside selection
        i32 clickX = static_cast<i32>(e.position.x);
        i32 clickY = static_cast<i32>(e.position.y);
        if (doc.selection.hasSelection && !doc.selection.isSelected(clickX, clickY)) {
            doc.recordSelectionChange("Deselect");
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
        doc.recordSelectionChange("Free Select");
        bool antiAlias = getAppState().selectionAntiAlias;
        doc.selection.setPolygon(points, addMode, subtractMode, antiAlias);
        doc.notifySelectionChanged();
    } else if (!addMode && !subtractMode) {
        // Click without valid selection - deselect if clicking outside selection
        i32 clickX = static_cast<i32>(e.position.x);
        i32 clickY = static_cast<i32>(e.position.y);
        if (doc.selection.hasSelection && !doc.selection.isSelected(clickX, clickY)) {
            doc.recordSelectionChange("Deselect");
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
                doc.recordSelectionChange("Deselect");
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
        doc.recordSelectionChange("Polygon Select");
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

    // Document coordinates from click
    i32 docX = static_cast<i32>(e.position.x);
    i32 docY = static_cast<i32>(e.position.y);

    // Check document bounds
    if (docX < 0 || docY < 0 || docX >= static_cast<i32>(doc.width) ||
        docY >= static_cast<i32>(doc.height)) {
        return;
    }

    // Compute layer transforms
    Matrix3x2 layerToDoc = layer->transform.toMatrix(layer->canvas.width, layer->canvas.height);
    Matrix3x2 docToLayer = layerToDoc.inverted();

    // Transform click position to layer space
    Vec2 layerPos = docToLayer.transform(Vec2(static_cast<f32>(docX), static_cast<f32>(docY)));
    i32 layerX = static_cast<i32>(std::floor(layerPos.x));
    i32 layerY = static_cast<i32>(std::floor(layerPos.y));

    u32 targetColor = layer->canvas.getPixel(layerX, layerY);

    bool addMode = e.shiftHeld;
    bool subtractMode = e.altHeld;

    // Record for undo
    doc.recordSelectionChange("Magic Wand");

    if (!addMode && !subtractMode) {
        doc.selection.clear();
    }

    if (contiguous) {
        floodSelectTransformed(doc.selection, layer->canvas, layerX, layerY, targetColor,
                               tolerance, addMode, subtractMode, layerToDoc);
    } else {
        globalSelectTransformed(doc.selection, layer->canvas, targetColor,
                                tolerance, addMode, subtractMode, layerToDoc);
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

void MagicWandTool::floodSelectTransformed(Selection& sel, const TiledCanvas& canvas,
                                            i32 startX, i32 startY, u32 targetColor,
                                            f32 tolerance, bool add, bool subtract,
                                            const Matrix3x2& layerToDoc) {
    // Calculate bounds in layer space by inverse-transforming document corners
    // This prevents infinite flood fill on transparent areas
    Matrix3x2 docToLayer = layerToDoc.inverted();

    // Transform all four document corners to layer space to find bounds
    Vec2 corners[4] = {
        docToLayer.transform(Vec2(0, 0)),
        docToLayer.transform(Vec2(static_cast<f32>(sel.width), 0)),
        docToLayer.transform(Vec2(0, static_cast<f32>(sel.height))),
        docToLayer.transform(Vec2(static_cast<f32>(sel.width), static_cast<f32>(sel.height)))
    };

    i32 minX = static_cast<i32>(std::floor(std::min({corners[0].x, corners[1].x, corners[2].x, corners[3].x}))) - 1;
    i32 maxX = static_cast<i32>(std::ceil(std::max({corners[0].x, corners[1].x, corners[2].x, corners[3].x}))) + 1;
    i32 minY = static_cast<i32>(std::floor(std::min({corners[0].y, corners[1].y, corners[2].y, corners[3].y}))) - 1;
    i32 maxY = static_cast<i32>(std::ceil(std::max({corners[0].y, corners[1].y, corners[2].y, corners[3].y}))) + 1;

    // Use a set to track visited pixels (supports any coordinates including negative)
    std::unordered_set<u64> visited;

    auto packCoord = [](i32 x, i32 y) -> u64 {
        return (static_cast<u64>(static_cast<u32>(y)) << 32) |
               static_cast<u64>(static_cast<u32>(x));
    };

    std::queue<std::pair<i32, i32>> queue;
    queue.push({startX, startY});
    visited.insert(packCoord(startX, startY));

    while (!queue.empty()) {
        auto [x, y] = queue.front();
        queue.pop();

        u32 currentColor = canvas.getPixel(x, y);
        if (colorDifference(currentColor, targetColor) > tolerance) continue;

        // Transform layer coords to document coords for selection
        Vec2 docPos = layerToDoc.transform(Vec2(static_cast<f32>(x), static_cast<f32>(y)));
        i32 docX = static_cast<i32>(std::floor(docPos.x));
        i32 docY = static_cast<i32>(std::floor(docPos.y));

        // Only set selection if within document bounds
        if (docX >= 0 && docY >= 0 &&
            docX < static_cast<i32>(sel.width) && docY < static_cast<i32>(sel.height)) {
            if (subtract) {
                sel.setValue(docX, docY, 0);
            } else {
                sel.setValue(docX, docY, 255);
            }
        }

        const i32 dx[] = {-1, 1, 0, 0};
        const i32 dy[] = {0, 0, -1, 1};

        for (i32 i = 0; i < 4; ++i) {
            i32 nx = x + dx[i];
            i32 ny = y + dy[i];

            // Bounds check in layer space
            if (nx < minX || nx > maxX || ny < minY || ny > maxY) continue;

            u64 key = packCoord(nx, ny);
            if (visited.find(key) != visited.end()) continue;

            u32 neighborColor = canvas.getPixel(nx, ny);
            if (colorDifference(neighborColor, targetColor) <= tolerance) {
                visited.insert(key);
                queue.push({nx, ny});
            }
        }
    }
}

void MagicWandTool::globalSelectTransformed(Selection& sel, const TiledCanvas& canvas,
                                             u32 targetColor, f32 tolerance, bool add, bool subtract,
                                             const Matrix3x2& layerToDoc) {
    // Iterate over existing tiles and select matching pixels
    canvas.forEachPixel([&](u32 x, u32 y, u32 pixel) {
        if (colorDifference(pixel, targetColor) <= tolerance) {
            // Transform layer coords to document coords for selection
            Vec2 docPos = layerToDoc.transform(Vec2(static_cast<f32>(x), static_cast<f32>(y)));
            i32 docX = static_cast<i32>(std::floor(docPos.x));
            i32 docY = static_cast<i32>(std::floor(docPos.y));

            if (subtract) {
                sel.setValue(docX, docY, 0);
            } else {
                sel.setValue(docX, docY, 255);
            }
        }
    });
}
