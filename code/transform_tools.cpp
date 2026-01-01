#include "transform_tools.h"

// CropTool implementations
void CropTool::initializeCropRect(Document& doc) {
    if (!initialized) {
        cropRect = Recti(0, 0, doc.width, doc.height);
        initialized = true;
    }
}

void CropTool::reset(Document& doc) {
    cropRect = Recti(0, 0, doc.width, doc.height);
    getAppState().needsRedraw = true;
}

void CropTool::apply(Document& doc) {
    if (cropRect.w <= 0 || cropRect.h <= 0) return;

    // Clear any active selection before cropping to avoid issues
    doc.selection.clear();

    u32 newWidth = static_cast<u32>(cropRect.w);
    u32 newHeight = static_cast<u32>(cropRect.h);

    // For each layer, handle cropping appropriately based on transform
    for (auto& layer : doc.layers) {
        if (layer->isPixelLayer()) {
            PixelLayer* pixelLayer = static_cast<PixelLayer*>(layer.get());

            if (pixelLayer->transform.isIdentity()) {
                // Identity transform: layer coords == document coords
                // Recreate canvas with cropped region
                TiledCanvas newCanvas(newWidth, newHeight);

                pixelLayer->canvas.forEachPixel([&](u32 x, u32 y, u32 pixel) {
                    i32 newX = static_cast<i32>(x) - cropRect.x;
                    i32 newY = static_cast<i32>(y) - cropRect.y;
                    if (newX >= 0 && newX < static_cast<i32>(newWidth) &&
                        newY >= 0 && newY < static_cast<i32>(newHeight)) {
                        newCanvas.setPixel(newX, newY, pixel);
                    }
                });

                pixelLayer->canvas = std::move(newCanvas);
            } else {
                // Transformed layer: just adjust position in document space
                // The canvas stays the same (it's in layer space)
                // This preserves the layer content even if partially outside crop
                pixelLayer->transform.position.x -= cropRect.x;
                pixelLayer->transform.position.y -= cropRect.y;
            }
        }
        else if (layer->isTextLayer()) {
            // Adjust text layer position
            layer->transform.position.x -= cropRect.x;
            layer->transform.position.y -= cropRect.y;
        }
    }

    doc.width = newWidth;
    doc.height = newHeight;
    doc.selection.resize(newWidth, newHeight);
    doc.notifyChanged(Rect(0, 0, newWidth, newHeight));

    // Reset crop rect to new canvas size
    cropRect = Recti(0, 0, doc.width, doc.height);
    getAppState().needsRedraw = true;
}

CropHandle CropTool::hitTest(const Vec2& pos, f32 zoom) {
    // Visual handle is 4 * UI_SCALE pixels in screen space
    // Hit detection is 1.5x larger for easier interaction
    f32 handleSize = (4.0f * Config::uiScale * 1.5f) / zoom;

    i32 right = cropRect.x + cropRect.w;
    i32 bottom = cropRect.y + cropRect.h;

    // Check corners first (they have priority)
    if (Vec2::distance(pos, Vec2(cropRect.x, cropRect.y)) < handleSize)
        return CropHandle::TopLeft;
    if (Vec2::distance(pos, Vec2(right, cropRect.y)) < handleSize)
        return CropHandle::TopRight;
    if (Vec2::distance(pos, Vec2(cropRect.x, bottom)) < handleSize)
        return CropHandle::BottomLeft;
    if (Vec2::distance(pos, Vec2(right, bottom)) < handleSize)
        return CropHandle::BottomRight;

    // Check center handle (uniform scale)
    Vec2 center(cropRect.x + cropRect.w / 2.0f, cropRect.y + cropRect.h / 2.0f);
    if (Vec2::distance(pos, center) < handleSize)
        return CropHandle::Center;

    // Check edge midpoints
    if (Vec2::distance(pos, Vec2(cropRect.x + cropRect.w / 2.0f, cropRect.y)) < handleSize)
        return CropHandle::Top;
    if (Vec2::distance(pos, Vec2(cropRect.x + cropRect.w / 2.0f, bottom)) < handleSize)
        return CropHandle::Bottom;
    if (Vec2::distance(pos, Vec2(cropRect.x, cropRect.y + cropRect.h / 2.0f)) < handleSize)
        return CropHandle::Left;
    if (Vec2::distance(pos, Vec2(right, cropRect.y + cropRect.h / 2.0f)) < handleSize)
        return CropHandle::Right;

    return CropHandle::None;
}

void CropTool::onMouseDown(Document& doc, const ToolEvent& e) {
    initializeCropRect(doc);

    activeHandle = hitTest(e.position, e.zoom);
    if (activeHandle != CropHandle::None) {
        dragStart = e.position;
        dragStartRect = cropRect;
    }
}

void CropTool::onMouseDrag(Document& doc, const ToolEvent& e) {
    if (activeHandle == CropHandle::None) return;

    Vec2 delta = e.position - dragStart;
    i32 dx = static_cast<i32>(delta.x);
    i32 dy = static_cast<i32>(delta.y);

    switch (activeHandle) {
        case CropHandle::TopLeft:
            cropRect.x = dragStartRect.x + dx;
            cropRect.y = dragStartRect.y + dy;
            cropRect.w = dragStartRect.w - dx;
            cropRect.h = dragStartRect.h - dy;
            break;
        case CropHandle::Top:
            cropRect.y = dragStartRect.y + dy;
            cropRect.h = dragStartRect.h - dy;
            break;
        case CropHandle::TopRight:
            cropRect.y = dragStartRect.y + dy;
            cropRect.w = dragStartRect.w + dx;
            cropRect.h = dragStartRect.h - dy;
            break;
        case CropHandle::Left:
            cropRect.x = dragStartRect.x + dx;
            cropRect.w = dragStartRect.w - dx;
            break;
        case CropHandle::Right:
            cropRect.w = dragStartRect.w + dx;
            break;
        case CropHandle::BottomLeft:
            cropRect.x = dragStartRect.x + dx;
            cropRect.w = dragStartRect.w - dx;
            cropRect.h = dragStartRect.h + dy;
            break;
        case CropHandle::Bottom:
            cropRect.h = dragStartRect.h + dy;
            break;
        case CropHandle::BottomRight:
            cropRect.w = dragStartRect.w + dx;
            cropRect.h = dragStartRect.h + dy;
            break;
        case CropHandle::Center: {
            // Uniform scale from center
            f32 scale = 1.0f + (dx - dy) * 0.005f;  // Drag right/up to grow
            i32 newW = static_cast<i32>(dragStartRect.w * scale);
            i32 newH = static_cast<i32>(dragStartRect.h * scale);
            if (newW > 0 && newH > 0) {
                i32 centerX = dragStartRect.x + dragStartRect.w / 2;
                i32 centerY = dragStartRect.y + dragStartRect.h / 2;
                cropRect.x = centerX - newW / 2;
                cropRect.y = centerY - newH / 2;
                cropRect.w = newW;
                cropRect.h = newH;
            }
            break;
        }
        default:
            break;
    }

    // Ensure minimum size
    if (cropRect.w < 1) cropRect.w = 1;
    if (cropRect.h < 1) cropRect.h = 1;

    getAppState().needsRedraw = true;
}

void CropTool::onMouseUp(Document& doc, const ToolEvent& e) {
    activeHandle = CropHandle::None;
}

void CropTool::onMouseMove(Document& doc, const ToolEvent& e) {
    initializeCropRect(doc);
    getAppState().needsRedraw = true;
}

void CropTool::onKeyDown(Document& doc, i32 keyCode) {
    if (keyCode == 27) { // Escape - reset
        reset(doc);
    }
}

void CropTool::renderOverlay(Framebuffer& fb, const Vec2& cursorPos, f32 zoom, const Vec2& pan, const Recti& clipRectParam) {
    if (!initialized) return;

    // Convert crop rect from document to screen coordinates
    // pan already includes viewport offset, so: screenX = docX * zoom + pan.x
    i32 x1 = static_cast<i32>(cropRect.x * zoom + pan.x);
    i32 y1 = static_cast<i32>(cropRect.y * zoom + pan.y);
    i32 x2 = static_cast<i32>((cropRect.x + cropRect.w) * zoom + pan.x);
    i32 y2 = static_cast<i32>((cropRect.y + cropRect.h) * zoom + pan.y);

    i32 thickness = static_cast<i32>(Config::uiScale);
    i32 handleRadius = static_cast<i32>(4 * Config::uiScale);

    // Draw crop rectangle (dashed pattern via alternating colors)
    // Black outline
    fb.drawRect(Recti(x1, y1, x2 - x1, y2 - y1), 0x000000FF, thickness);
    // White inner outline
    if (x2 - x1 > thickness * 2 && y2 - y1 > thickness * 2) {
        fb.drawRect(Recti(x1 + thickness, y1 + thickness, x2 - x1 - thickness * 2, y2 - y1 - thickness * 2), 0xFFFFFFFF, thickness);
    }

    // Draw rule of thirds grid lines (like Photoshop)
    // Two vertical lines at 1/3 and 2/3, two horizontal at 1/3 and 2/3
    i32 thirdW = (x2 - x1) / 3;
    i32 thirdH = (y2 - y1) / 3;
    if (thirdW > 0 && thirdH > 0) {
        // Draw black outline first, then white on top for visibility on any background
        u32 darkColor = 0x00000080;  // Black with 50% alpha
        u32 lightColor = 0xFFFFFFCC; // White with 80% alpha

        i32 vLine1 = x1 + thirdW;
        i32 vLine2 = x1 + thirdW * 2;
        i32 hLine1 = y1 + thirdH;
        i32 hLine2 = y1 + thirdH * 2;

        // Vertical lines - black outline
        fb.drawVerticalLine(vLine1 - 1, y1, y2, darkColor);
        fb.drawVerticalLine(vLine1 + 1, y1, y2, darkColor);
        fb.drawVerticalLine(vLine2 - 1, y1, y2, darkColor);
        fb.drawVerticalLine(vLine2 + 1, y1, y2, darkColor);
        // Vertical lines - white center
        fb.drawVerticalLine(vLine1, y1, y2, lightColor);
        fb.drawVerticalLine(vLine2, y1, y2, lightColor);

        // Horizontal lines - black outline
        fb.drawHorizontalLine(x1, x2, hLine1 - 1, darkColor);
        fb.drawHorizontalLine(x1, x2, hLine1 + 1, darkColor);
        fb.drawHorizontalLine(x1, x2, hLine2 - 1, darkColor);
        fb.drawHorizontalLine(x1, x2, hLine2 + 1, darkColor);
        // Horizontal lines - white center
        fb.drawHorizontalLine(x1, x2, hLine1, lightColor);
        fb.drawHorizontalLine(x1, x2, hLine2, lightColor);
    }

    // Draw dimmed area outside crop region
    // This is complex with the framebuffer API, skip for now

    // Helper to draw a handle
    auto drawHandle = [&](i32 cx, i32 cy, bool isCenter = false) {
        i32 size = isCenter ? handleRadius : handleRadius;
        // Fill
        fb.fillRect(Recti(cx - size, cy - size, size * 2, size * 2), 0xFFFFFFFF);
        // Border
        fb.drawRect(Recti(cx - size, cy - size, size * 2, size * 2), 0x000000FF, 1);
    };

    // Draw corner handles
    drawHandle(x1, y1);
    drawHandle(x2, y1);
    drawHandle(x1, y2);
    drawHandle(x2, y2);

    // Draw edge handles
    drawHandle((x1 + x2) / 2, y1);
    drawHandle((x1 + x2) / 2, y2);
    drawHandle(x1, (y1 + y2) / 2);
    drawHandle(x2, (y1 + y2) / 2);

    // Draw center handle (slightly different - a circle or diamond)
    i32 cx = (x1 + x2) / 2;
    i32 cy = (y1 + y2) / 2;
    i32 centerSize = handleRadius - 1;
    fb.fillRect(Recti(cx - centerSize, cy - centerSize, centerSize * 2, centerSize * 2), 0xCCCCCCFF);
    fb.drawRect(Recti(cx - centerSize, cy - centerSize, centerSize * 2, centerSize * 2), 0x000000FF, 1);
}

// GradientTool implementations
void GradientTool::onMouseDown(Document& doc, const ToolEvent& e) {
    startPos = e.position;
    endPos = e.position;
    dragging = true;
}

void GradientTool::onMouseDrag(Document& doc, const ToolEvent& e) {
    if (dragging) {
        endPos = e.position;
    }
}

void GradientTool::onMouseUp(Document& doc, const ToolEvent& e) {
    if (!dragging) return;
    dragging = false;

    endPos = e.position;

    PixelLayer* layer = doc.getActivePixelLayer();
    if (!layer || layer->locked) return;

    AppState& state = getAppState();
    Color fgColor = state.foregroundColor;
    Color bgColor = state.backgroundColor;

    // Expand layer canvas to cover document bounds if needed
    expandLayerToDocument(layer, doc.width, doc.height);

    // Begin undo - capture all tiles since gradient affects entire canvas
    doc.beginPixelUndo("Gradient", doc.activeLayerIndex);
    Recti fullBounds(0, 0, layer->canvas.width, layer->canvas.height);
    doc.captureOriginalTilesInRect(doc.activeLayerIndex, fullBounds);

    // Read gradient type from fillMode: 1=Linear, 2=Radial
    bool isLinear = (state.fillMode == 1);

    // Now layer covers full document, so we use document coordinates directly
    if (isLinear) {
        applyLinearGradient(layer->canvas, doc.selection, startPos, endPos,
                           fgColor, bgColor, 0, 0, doc.width, doc.height);
    } else {
        applyRadialGradient(layer->canvas, doc.selection, startPos, endPos,
                           fgColor, bgColor, 0, 0, doc.width, doc.height);
    }

    // Commit undo
    doc.commitUndo();

    doc.notifyChanged(Rect(0, 0, doc.width, doc.height));
}

void GradientTool::expandLayerToDocument(PixelLayer* layer, u32 docWidth, u32 docHeight) {
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

void GradientTool::applyLinearGradient(TiledCanvas& canvas, const Selection& sel,
                                       const Vec2& start, const Vec2& end,
                                       const Color& color1, const Color& color2,
                                       i32 layerOffsetX, i32 layerOffsetY,
                                       i32 docWidth, i32 docHeight) {
    Vec2 dir = end - start;
    f32 length = dir.length();
    if (length < 1.0f) return;

    Vec2 norm = dir.normalized();

    for (u32 y = 0; y < canvas.height; ++y) {
        for (u32 x = 0; x < canvas.width; ++x) {
            // Convert to document coords for bounds/selection check
            i32 docX = static_cast<i32>(x) + layerOffsetX;
            i32 docY = static_cast<i32>(y) + layerOffsetY;

            // Skip if outside selection
            if (sel.hasSelection && !sel.isSelected(docX, docY)) continue;

            // Skip if outside document bounds (when no selection)
            if (!sel.hasSelection && docWidth > 0 && docHeight > 0) {
                if (docX < 0 || docY < 0 || docX >= docWidth || docY >= docHeight) continue;
            }

            Vec2 p(x + 0.5f, y + 0.5f);
            Vec2 toP = p - start;
            f32 t = toP.dot(norm) / length;
            t = clamp(t, 0.0f, 1.0f);

            Color c = Color::lerp(color1, color2, t);
            u32 pixel = c.toRGBA();

            if (sel.hasSelection) {
                u8 selAlpha = sel.getValue(docX, docY);
                if (selAlpha < 255) {
                    u8 alpha = (c.a * selAlpha) / 255;
                    pixel = Blend::pack(c.r, c.g, c.b, alpha);
                }
            }

            canvas.blendPixel(x, y, pixel);
        }
    }
}

void GradientTool::applyRadialGradient(TiledCanvas& canvas, const Selection& sel,
                                       const Vec2& center, const Vec2& edge,
                                       const Color& color1, const Color& color2,
                                       i32 layerOffsetX, i32 layerOffsetY,
                                       i32 docWidth, i32 docHeight) {
    f32 radius = Vec2::distance(center, edge);
    if (radius < 1.0f) return;

    for (u32 y = 0; y < canvas.height; ++y) {
        for (u32 x = 0; x < canvas.width; ++x) {
            // Convert to document coords for bounds/selection check
            i32 docX = static_cast<i32>(x) + layerOffsetX;
            i32 docY = static_cast<i32>(y) + layerOffsetY;

            // Skip if outside selection
            if (sel.hasSelection && !sel.isSelected(docX, docY)) continue;

            // Skip if outside document bounds (when no selection)
            if (!sel.hasSelection && docWidth > 0 && docHeight > 0) {
                if (docX < 0 || docY < 0 || docX >= docWidth || docY >= docHeight) continue;
            }

            Vec2 p(x + 0.5f, y + 0.5f);
            f32 dist = Vec2::distance(p, center);
            f32 t = dist / radius;
            t = clamp(t, 0.0f, 1.0f);

            Color c = Color::lerp(color1, color2, t);
            u32 pixel = c.toRGBA();

            if (sel.hasSelection) {
                u8 selAlpha = sel.getValue(docX, docY);
                if (selAlpha < 255) {
                    u8 alpha = (c.a * selAlpha) / 255;
                    pixel = Blend::pack(c.r, c.g, c.b, alpha);
                }
            }

            canvas.blendPixel(x, y, pixel);
        }
    }
}
