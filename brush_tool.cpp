#include "brush_tool.h"

void BrushTool::updateFromAppState() {
    AppState& state = getAppState();

    // Check if size, hardness, tip, or angle changed
    bool tipChanged = currentTipIndex != state.currentBrushTipIndex;
    bool angleChanged = currentAngle != state.brushAngle;

    if (size != state.brushSize || hardness != state.brushHardness || tipChanged || angleChanged) {
        size = state.brushSize;
        hardness = state.brushHardness;
        currentTipIndex = state.currentBrushTipIndex;
        currentAngle = state.brushAngle;
        stampDirty = true;
    }

    opacity = state.brushOpacity;
    flow = state.brushFlow;
    spacing = state.brushSpacing;
    pressureMode = state.brushPressureMode;
    dynamics = state.brushDynamics;

    // Update current tip pointer
    if (currentTipIndex >= 0) {
        currentTip = state.brushLibrary.getTip(currentTipIndex);
    } else {
        currentTip = nullptr;
    }
}

f32 BrushTool::applyPressureCurve(f32 rawPressure) {
    AppState& state = getAppState();
    return evaluatePressureCurve(rawPressure, state.pressureCurveCP1, state.pressureCurveCP2);
}

void BrushTool::ensureStamp() {
    if (stampDirty && !isPencilMode()) {
        if (currentTip) {
            // Custom tip - generate from image
            currentStamp = BrushRenderer::generateStampFromTip(*currentTip, size, currentAngle);
        } else {
            // Round brush
            currentStamp = BrushRenderer::generateStamp(size, hardness);
        }
        stampDirty = false;
    }
}

void BrushTool::onMouseDown(Document& doc, const ToolEvent& e) {
    PixelLayer* layer = doc.getActivePixelLayer();
    if (!layer || layer->locked) return;

    updateFromAppState();
    ensureStamp();

    stroking = true;
    lastPos = e.position;
    strokeLayer = layer;

    // Convert document position to layer position using inverse transform
    Matrix3x2 invMat = layer->transform.toMatrix(layer->canvas.width, layer->canvas.height).inverted();
    Vec2 layerPos = invMat.transform(e.position);
    lastLayerPos = layerPos;

    // Apply pressure curve
    f32 pressure = (pressureMode != 0) ? applyPressureCurve(e.pressure) : 1.0f;

    // Calculate effective values based on pressure mode
    f32 effectiveSize = size;
    f32 effectiveOpacity = opacity;
    f32 effectiveFlow = flow;

    switch (pressureMode) {
        case 1: effectiveSize *= pressure; break;      // Size
        case 2: effectiveOpacity *= pressure; break;   // Opacity
        case 3: effectiveFlow *= pressure; break;      // Flow
    }

    // Get color
    strokeColor = getAppState().foregroundColor.toRGBA();

    // Get selection pointer (nullptr if no selection)
    const Selection* sel = doc.selection.hasSelection ? &doc.selection : nullptr;

    if (isPencilMode()) {
        // Pencil mode: render directly to canvas (no beading issue with single pixels)
        i32 px = static_cast<i32>(std::floor(layerPos.x));
        i32 py = static_cast<i32>(std::floor(layerPos.y));
        lastPixelX = px;
        lastPixelY = py;
        BrushRenderer::pencilPixel(layer->canvas, px, py, strokeColor, effectiveFlow, sel);
        doc.notifyChanged(Rect(e.position.x - 1, e.position.y - 1, 3, 3));
    } else {
        // Create stroke buffer for this stroke
        strokeBuffer = std::make_unique<TiledCanvas>(layer->canvas.width, layer->canvas.height);

        // Regenerate stamp if size changed due to pressure (for non-custom tips)
        if (pressureMode == 1 && effectiveSize != size && !currentTip) {
            currentStamp = BrushRenderer::generateStamp(effectiveSize, hardness);
        } else if (pressureMode == 1 && effectiveSize != size && currentTip) {
            currentStamp = BrushRenderer::generateStampFromTip(*currentTip, effectiveSize, currentAngle);
        }

        // Stamp to buffer - use dynamics-aware version if any dynamics enabled
        if (dynamics.hasAnyDynamics()) {
            BrushRenderer::stampToBufferWithDynamics(*strokeBuffer, currentStamp, currentTip,
                layerPos, strokeColor, effectiveFlow, effectiveSize, currentAngle, hardness,
                dynamics, BlendMode::Normal, sel);
        } else {
            BrushRenderer::stampToBuffer(*strokeBuffer, currentStamp, layerPos, strokeColor,
                effectiveFlow, BlendMode::Normal, sel);
        }

        // Track stroke bounds (add extra margin for scattering)
        f32 scatterMargin = dynamics.scatterAmount > 0 ? dynamics.scatterAmount * effectiveSize : 0;
        f32 r = effectiveSize / 2.0f + 1 + scatterMargin;
        strokeBounds = Rect(layerPos.x - r, layerPos.y - r, effectiveSize + 2 + scatterMargin * 2, effectiveSize + 2 + scatterMargin * 2);

        doc.notifyChanged(Rect(e.position.x - r, e.position.y - r, effectiveSize + 2 + scatterMargin * 2, effectiveSize + 2 + scatterMargin * 2));
    }
}

void BrushTool::onMouseDrag(Document& doc, const ToolEvent& e) {
    if (!stroking) return;

    PixelLayer* layer = doc.getActivePixelLayer();
    if (!layer || layer->locked) return;

    updateFromAppState();
    ensureStamp();

    // Convert document positions to layer positions using inverse transform
    Matrix3x2 invMat = layer->transform.toMatrix(layer->canvas.width, layer->canvas.height).inverted();
    Vec2 layerPosTo = invMat.transform(e.position);

    // Apply pressure curve
    f32 pressure = (pressureMode != 0) ? applyPressureCurve(e.pressure) : 1.0f;

    // Calculate effective values based on pressure mode
    f32 effectiveSize = size;
    f32 effectiveOpacity = opacity;
    f32 effectiveFlow = flow;

    switch (pressureMode) {
        case 1: effectiveSize *= pressure; break;      // Size
        case 2: effectiveOpacity *= pressure; break;   // Opacity
        case 3: effectiveFlow *= pressure; break;      // Flow
    }

    // Get selection pointer (nullptr if no selection)
    const Selection* sel = doc.selection.hasSelection ? &doc.selection : nullptr;

    Rect dirty;
    if (isPencilMode()) {
        // Pencil mode: pixel-perfect line directly to canvas
        i32 px = static_cast<i32>(std::floor(layerPosTo.x));
        i32 py = static_cast<i32>(std::floor(layerPosTo.y));
        BrushRenderer::pencilLine(layer->canvas, lastPixelX, lastPixelY, px, py,
            strokeColor, effectiveFlow, sel);
        dirty = Rect(
            std::min(lastPos.x, e.position.x) - 1,
            std::min(lastPos.y, e.position.y) - 1,
            std::abs(e.position.x - lastPos.x) + 3,
            std::abs(e.position.y - lastPos.y) + 3
        );
        lastPixelX = px;
        lastPixelY = py;
    } else {
        if (!strokeBuffer) return;

        // Regenerate stamp if size changed due to pressure
        if (pressureMode == 1 && effectiveSize != size && !currentTip) {
            currentStamp = BrushRenderer::generateStamp(effectiveSize, hardness);
        } else if (pressureMode == 1 && effectiveSize != size && currentTip) {
            currentStamp = BrushRenderer::generateStampFromTip(*currentTip, effectiveSize, currentAngle);
        }

        // Stroke line to buffer - use dynamics-aware version if any dynamics enabled
        if (dynamics.hasAnyDynamics()) {
            BrushRenderer::strokeLineToBufferWithDynamics(*strokeBuffer, currentStamp, currentTip,
                lastLayerPos, layerPosTo, strokeColor, effectiveFlow, spacing,
                effectiveSize, currentAngle, hardness, dynamics, BlendMode::Normal, sel);
        } else {
            BrushRenderer::strokeLineToBuffer(*strokeBuffer, currentStamp, lastLayerPos, layerPosTo,
                strokeColor, effectiveFlow, spacing, BlendMode::Normal, sel);
        }

        // Expand stroke bounds (add extra margin for scattering)
        f32 scatterMargin = dynamics.scatterAmount > 0 ? dynamics.scatterAmount * effectiveSize : 0;
        f32 r = effectiveSize / 2.0f + 1 + scatterMargin;
        Rect newBounds(
            std::min(lastLayerPos.x, layerPosTo.x) - r,
            std::min(lastLayerPos.y, layerPosTo.y) - r,
            std::abs(layerPosTo.x - lastLayerPos.x) + effectiveSize + 2 + scatterMargin * 2,
            std::abs(layerPosTo.y - lastLayerPos.y) + effectiveSize + 2 + scatterMargin * 2
        );
        strokeBounds = strokeBounds.united(newBounds);

        dirty = Rect(
            std::min(lastPos.x, e.position.x) - r,
            std::min(lastPos.y, e.position.y) - r,
            std::abs(e.position.x - lastPos.x) + effectiveSize + 2 + scatterMargin * 2,
            std::abs(e.position.y - lastPos.y) + effectiveSize + 2 + scatterMargin * 2
        );

        lastLayerPos = layerPosTo;
    }

    lastPos = e.position;
    doc.notifyChanged(dirty);
}

void BrushTool::onMouseUp(Document& doc, const ToolEvent& e) {
    if (!stroking) return;

    // For brush mode (not pencil), composite stroke buffer to layer
    if (!isPencilMode() && strokeBuffer && strokeLayer) {
        // Composite the stroke buffer onto the layer with stroke opacity
        BrushRenderer::compositeStrokeToLayer(strokeLayer->canvas, *strokeBuffer, opacity, BlendMode::Normal);

        // Notify full stroke bounds changed
        doc.notifyChanged(strokeBounds);
    }

    // Cleanup
    stroking = false;
    strokeBuffer.reset();
    strokeLayer = nullptr;
    strokeBounds = Rect();
}

void BrushTool::renderOverlay(Framebuffer& fb, const Vec2& cursorPos, f32 zoom, const Vec2& pan, const Recti& clipRect) {
    updateFromAppState();

    i32 cx = static_cast<i32>(cursorPos.x);
    i32 cy = static_cast<i32>(cursorPos.y);
    i32 thickness = static_cast<i32>(Config::uiScale);

    if (isPencilMode()) {
        // Pencil mode: draw crosshair cursor
        i32 crossSize = static_cast<i32>(6 * Config::uiScale);

        // Draw black outline lines
        for (i32 t = -1; t <= 1; ++t) {
            fb.drawHorizontalLine(cx - crossSize, cx + crossSize, cy + t, 0x000000FF);
            fb.drawVerticalLine(cx + t, cy - crossSize, cy + crossSize, 0x000000FF);
        }
        // Draw white center lines
        fb.drawHorizontalLine(cx - crossSize + 1, cx + crossSize - 1, cy, 0xFFFFFFFF);
        fb.drawVerticalLine(cx, cy - crossSize + 1, cy + crossSize - 1, 0xFFFFFFFF);
    } else if (currentTip && getAppState().brushShowBoundingBox) {
        // Custom brush with bounding box: draw rotated rectangle
        f32 scale = size / static_cast<f32>(std::max(currentTip->width, currentTip->height));
        f32 halfW = (currentTip->width * scale * zoom) / 2.0f;
        f32 halfH = (currentTip->height * scale * zoom) / 2.0f;

        // Rotation
        constexpr f32 PI = 3.14159265358979323846f;
        f32 rad = currentAngle * (PI / 180.0f);
        f32 cosA = std::cos(rad);
        f32 sinA = std::sin(rad);

        // Calculate 4 corners relative to center
        auto rotatePoint = [cosA, sinA](f32 x, f32 y) -> Vec2 {
            return Vec2(x * cosA - y * sinA, x * sinA + y * cosA);
        };

        Vec2 corners[4] = {
            rotatePoint(-halfW, -halfH),
            rotatePoint( halfW, -halfH),
            rotatePoint( halfW,  halfH),
            rotatePoint(-halfW,  halfH)
        };

        // Draw the rotated rectangle
        for (i32 i = 0; i < 4; ++i) {
            i32 x0 = cx + static_cast<i32>(corners[i].x);
            i32 y0 = cy + static_cast<i32>(corners[i].y);
            i32 x1 = cx + static_cast<i32>(corners[(i+1)%4].x);
            i32 y1 = cy + static_cast<i32>(corners[(i+1)%4].y);
            fb.drawLine(x0, y0, x1, y1, 0x000000FF);
        }
        // Draw white inner lines
        for (i32 i = 0; i < 4; ++i) {
            f32 inset = 1.0f;
            Vec2 insetCorners[4] = {
                rotatePoint(-halfW + inset, -halfH + inset),
                rotatePoint( halfW - inset, -halfH + inset),
                rotatePoint( halfW - inset,  halfH - inset),
                rotatePoint(-halfW + inset,  halfH - inset)
            };
            i32 x0 = cx + static_cast<i32>(insetCorners[i].x);
            i32 y0 = cy + static_cast<i32>(insetCorners[i].y);
            i32 x1 = cx + static_cast<i32>(insetCorners[(i+1)%4].x);
            i32 y1 = cy + static_cast<i32>(insetCorners[(i+1)%4].y);
            fb.drawLine(x0, y0, x1, y1, 0xFFFFFFFF);
        }
    } else {
        // Normal brush mode: draw circle cursor
        i32 radius = static_cast<i32>((size / 2.0f) * zoom);
        if (radius < 1) radius = 1;

        fb.drawCircle(cx, cy, radius, 0x000000FF, thickness);  // Black outline
        if (radius > thickness) {
            fb.drawCircle(cx, cy, radius - thickness, 0xFFFFFFFF, thickness);  // White inner
        }
    }
}
