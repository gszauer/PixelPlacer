#include "fill_tool.h"

void FillTool::onMouseDown(Document& doc, const ToolEvent& e) {
    PixelLayer* layer = doc.getActivePixelLayer();
    if (!layer || layer->locked) return;

    AppState& state = getAppState();
    f32 tolerance = state.fillTolerance;
    bool contiguous = state.fillContiguous;

    i32 docX = static_cast<i32>(e.position.x);
    i32 docY = static_cast<i32>(e.position.y);

    // Check if click is within document bounds
    if (docX < 0 || docY < 0 || docX >= static_cast<i32>(doc.width) ||
        docY >= static_cast<i32>(doc.height)) {
        return;
    }

    // If there's a selection, only fill if clicking inside it
    const Selection* sel = doc.selection.hasSelection ? &doc.selection : nullptr;
    if (sel && !sel->isSelected(docX, docY)) {
        return;
    }

    // Expand layer canvas to cover document bounds if needed
    expandLayerToDocument(layer, doc.width, doc.height);

    // Now layer covers full document, so layer coords = document coords
    i32 x = docX;
    i32 y = docY;

    u32 targetColor = layer->canvas.getPixel(x, y);
    u32 fillColor = state.foregroundColor.toRGBA();

    if (targetColor == fillColor) return;

    if (contiguous) {
        floodFill(layer->canvas, x, y, targetColor, fillColor, tolerance, sel,
                  0, 0, doc.width, doc.height);
    } else {
        globalFill(layer->canvas, targetColor, fillColor, tolerance, sel,
                   0, 0, doc.width, doc.height);
    }

    doc.notifyChanged(Rect(0, 0, doc.width, doc.height));
}

void FillTool::expandLayerToDocument(PixelLayer* layer, u32 docWidth, u32 docHeight) {
    i32 layerX = static_cast<i32>(layer->transform.position.x);
    i32 layerY = static_cast<i32>(layer->transform.position.y);
    i32 layerW = static_cast<i32>(layer->canvas.width);
    i32 layerH = static_cast<i32>(layer->canvas.height);

    // Calculate the bounds needed to cover both current layer area and document
    i32 minX = std::min(0, layerX);
    i32 minY = std::min(0, layerY);
    i32 maxX = std::max(static_cast<i32>(docWidth), layerX + layerW);
    i32 maxY = std::max(static_cast<i32>(docHeight), layerY + layerH);

    i32 newW = maxX - minX;
    i32 newH = maxY - minY;

    // Validate dimensions before creating canvas
    if (newW <= 0 || newH <= 0) return;
    if (newW > static_cast<i32>(Config::MAX_CANVAS_SIZE) ||
        newH > static_cast<i32>(Config::MAX_CANVAS_SIZE)) return;

    // If layer already covers document, no expansion needed
    if (layerX <= 0 && layerY <= 0 &&
        layerX + layerW >= static_cast<i32>(docWidth) &&
        layerY + layerH >= static_cast<i32>(docHeight)) {
        return;
    }

    // Create new canvas
    TiledCanvas newCanvas(static_cast<u32>(newW), static_cast<u32>(newH));

    // Copy existing pixels to new position
    i32 offsetX = layerX - minX;
    i32 offsetY = layerY - minY;
    layer->canvas.forEachPixel([&](u32 x, u32 y, u32 pixel) {
        if (pixel & 0xFF) {
            newCanvas.setPixel(x + offsetX, y + offsetY, pixel);
        }
    });

    // Replace canvas and update position
    layer->canvas = std::move(newCanvas);
    layer->transform.position.x = static_cast<f32>(minX);
    layer->transform.position.y = static_cast<f32>(minY);
}

f32 FillTool::colorDifference(u32 a, u32 b) {
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

void FillTool::floodFill(TiledCanvas& canvas, i32 startX, i32 startY,
                         u32 targetColor, u32 fillColor, f32 tolerance,
                         const Selection* sel,
                         i32 layerOffsetX, i32 layerOffsetY,
                         i32 docWidth, i32 docHeight) {
    u32 w = canvas.width;
    u32 h = canvas.height;

    // Overflow check: w * h must not overflow
    if (w == 0 || h == 0) return;
    if (h > std::numeric_limits<u32>::max() / w) {
        return; // Canvas too large for flood fill visited array
    }

    std::vector<bool> visited(static_cast<size_t>(w) * h, false);
    std::queue<std::pair<i32, i32>> queue;

    queue.push({startX, startY});
    visited[startY * w + startX] = true;

    while (!queue.empty()) {
        auto [x, y] = queue.front();
        queue.pop();

        // Convert to document coords for bounds/selection check
        i32 docX = x + layerOffsetX;
        i32 docY = y + layerOffsetY;

        // Skip if outside selection
        if (sel && !sel->isSelected(docX, docY)) continue;

        // Skip if outside document bounds (when no selection)
        if (!sel && docWidth > 0 && docHeight > 0) {
            if (docX < 0 || docY < 0 || docX >= docWidth || docY >= docHeight) continue;
        }

        u32 currentColor = canvas.getPixel(x, y);
        if (colorDifference(currentColor, targetColor) > tolerance) continue;

        canvas.setPixel(x, y, fillColor);

        // Check 4-connected neighbors
        const i32 dx[] = {-1, 1, 0, 0};
        const i32 dy[] = {0, 0, -1, 1};

        for (i32 i = 0; i < 4; ++i) {
            i32 nx = x + dx[i];
            i32 ny = y + dy[i];

            if (nx >= 0 && ny >= 0 && nx < static_cast<i32>(w) && ny < static_cast<i32>(h)) {
                i32 idx = ny * w + nx;
                if (!visited[idx]) {
                    i32 ndocX = nx + layerOffsetX;
                    i32 ndocY = ny + layerOffsetY;

                    // Check selection for neighbors
                    if (sel && !sel->isSelected(ndocX, ndocY)) continue;

                    // Check document bounds for neighbors (when no selection)
                    if (!sel && docWidth > 0 && docHeight > 0) {
                        if (ndocX < 0 || ndocY < 0 || ndocX >= docWidth || ndocY >= docHeight) continue;
                    }

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

void FillTool::globalFill(TiledCanvas& canvas, u32 targetColor, u32 fillColor, f32 tolerance,
                          const Selection* sel,
                          i32 layerOffsetX, i32 layerOffsetY,
                          i32 docWidth, i32 docHeight) {
    for (u32 y = 0; y < canvas.height; ++y) {
        for (u32 x = 0; x < canvas.width; ++x) {
            // Convert to document coords
            i32 docX = static_cast<i32>(x) + layerOffsetX;
            i32 docY = static_cast<i32>(y) + layerOffsetY;

            // Skip if outside selection
            if (sel && !sel->isSelected(docX, docY)) continue;

            // Skip if outside document bounds (when no selection)
            if (!sel && docWidth > 0 && docHeight > 0) {
                if (docX < 0 || docY < 0 || docX >= docWidth || docY >= docHeight) continue;
            }

            u32 currentColor = canvas.getPixel(x, y);
            if (colorDifference(currentColor, targetColor) <= tolerance) {
                canvas.setPixel(x, y, fillColor);
            }
        }
    }
}
