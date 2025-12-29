#include "eraser_tool.h"

void EraserTool::updateFromAppState() {
    AppState& state = getAppState();
    if (size != state.brushSize || hardness != state.brushHardness) {
        size = state.brushSize;
        hardness = state.brushHardness;
        stampDirty = true;
    }
    opacity = state.brushOpacity;
    flow = state.brushFlow;
    spacing = state.brushSpacing;
    pressureMode = state.eraserPressureMode;  // Uses eraser-specific setting
}

f32 EraserTool::applyPressureCurve(f32 rawPressure) {
    AppState& state = getAppState();
    return evaluatePressureCurve(rawPressure, state.pressureCurveCP1, state.pressureCurveCP2);
}

void EraserTool::ensureStamp() {
    if (stampDirty && !isPencilMode()) {
        currentStamp = BrushRenderer::generateStamp(size, hardness);
        stampDirty = false;
    }
}

void EraserTool::onMouseDown(Document& doc, const ToolEvent& e) {
    PixelLayer* layer = doc.getActivePixelLayer();
    if (!layer || layer->locked) return;

    updateFromAppState();
    ensureStamp();

    stroking = true;
    lastPos = e.position;
    strokeLayer = layer;

    // Compute layer-to-document and document-to-layer transforms
    Matrix3x2 layerToDoc = layer->transform.toMatrix(layer->canvas.width, layer->canvas.height);
    Matrix3x2 invMat = layerToDoc.inverted();
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

    // Get selection pointer (nullptr if no selection)
    const Selection* sel = doc.selection.hasSelection ? &doc.selection : nullptr;

    // Pass layer-to-doc transform for selection checking if layer is transformed
    const Matrix3x2* selTransform = layer->transform.isIdentity() ? nullptr : &layerToDoc;

    if (isPencilMode()) {
        // Pencil mode: erase directly (no beading issue with single pixels)
        i32 px = static_cast<i32>(std::floor(layerPos.x));
        i32 py = static_cast<i32>(std::floor(layerPos.y));
        lastPixelX = px;
        lastPixelY = py;
        BrushRenderer::pencilErase(layer->canvas, px, py, effectiveFlow, sel, selTransform);
        doc.notifyChanged(Rect(e.position.x - 1, e.position.y - 1, 3, 3));
    } else {
        // Create stroke buffer for this stroke
        strokeBuffer = std::make_unique<TiledCanvas>(layer->canvas.width, layer->canvas.height);

        // Regenerate stamp if size changed due to pressure
        if (pressureMode == 1 && effectiveSize != size) {
            currentStamp = BrushRenderer::generateStamp(effectiveSize, hardness);
        }

        // Stamp erase amount to buffer (not directly to canvas!)
        BrushRenderer::eraseStampToBuffer(*strokeBuffer, currentStamp, layerPos, effectiveFlow, sel, selTransform);

        // Track stroke bounds
        f32 r = effectiveSize / 2.0f + 1;
        strokeBounds = Rect(layerPos.x - r, layerPos.y - r, effectiveSize + 2, effectiveSize + 2);

        doc.notifyChanged(Rect(e.position.x - r, e.position.y - r, effectiveSize + 2, effectiveSize + 2));
    }
}

void EraserTool::onMouseDrag(Document& doc, const ToolEvent& e) {
    if (!stroking) return;

    PixelLayer* layer = doc.getActivePixelLayer();
    if (!layer || layer->locked) return;

    updateFromAppState();
    ensureStamp();

    // Compute layer-to-document and document-to-layer transforms
    Matrix3x2 layerToDoc = layer->transform.toMatrix(layer->canvas.width, layer->canvas.height);
    Matrix3x2 invMat = layerToDoc.inverted();
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

    // Pass layer-to-doc transform for selection checking if layer is transformed
    const Matrix3x2* selTransform = layer->transform.isIdentity() ? nullptr : &layerToDoc;

    Rect dirty;
    if (isPencilMode()) {
        // Pencil mode: pixel-perfect line directly to canvas
        i32 px = static_cast<i32>(std::floor(layerPosTo.x));
        i32 py = static_cast<i32>(std::floor(layerPosTo.y));
        BrushRenderer::pencilEraseLine(layer->canvas, lastPixelX, lastPixelY, px, py, effectiveFlow, sel, selTransform);
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
        if (pressureMode == 1 && effectiveSize != size) {
            currentStamp = BrushRenderer::generateStamp(effectiveSize, hardness);
        }

        // Erase line to buffer (not directly to canvas!)
        BrushRenderer::eraseLineToBuffer(*strokeBuffer, currentStamp, lastLayerPos, layerPosTo,
            effectiveFlow, spacing, sel, selTransform);

        // Expand stroke bounds
        f32 r = effectiveSize / 2.0f + 1;
        Rect newBounds(
            std::min(lastLayerPos.x, layerPosTo.x) - r,
            std::min(lastLayerPos.y, layerPosTo.y) - r,
            std::abs(layerPosTo.x - lastLayerPos.x) + effectiveSize + 2,
            std::abs(layerPosTo.y - lastLayerPos.y) + effectiveSize + 2
        );
        strokeBounds = strokeBounds.united(newBounds);

        dirty = Rect(
            std::min(lastPos.x, e.position.x) - r,
            std::min(lastPos.y, e.position.y) - r,
            std::abs(e.position.x - lastPos.x) + effectiveSize + 2,
            std::abs(e.position.y - lastPos.y) + effectiveSize + 2
        );

        lastLayerPos = layerPosTo;
    }

    lastPos = e.position;
    doc.notifyChanged(dirty);
}

void EraserTool::onMouseUp(Document& doc, const ToolEvent& e) {
    if (!stroking) return;

    // For eraser mode (not pencil), apply erase buffer to layer
    if (!isPencilMode() && strokeBuffer && strokeLayer) {
        // Apply the erase buffer to the layer with stroke opacity
        BrushRenderer::compositeEraseBufferToLayer(strokeLayer->canvas, *strokeBuffer, opacity);

        // Prune empty tiles after erasing
        strokeLayer->canvas.pruneEmptyTiles();

        // Transform stroke bounds from layer space to document space for dirty rect
        Matrix3x2 layerToDoc = strokeLayer->transform.toMatrix(
            strokeLayer->canvas.width, strokeLayer->canvas.height);

        // Transform all four corners of the bounds and compute bounding box
        Vec2 corners[4] = {
            layerToDoc.transform(Vec2(strokeBounds.x, strokeBounds.y)),
            layerToDoc.transform(Vec2(strokeBounds.x + strokeBounds.w, strokeBounds.y)),
            layerToDoc.transform(Vec2(strokeBounds.x, strokeBounds.y + strokeBounds.h)),
            layerToDoc.transform(Vec2(strokeBounds.x + strokeBounds.w, strokeBounds.y + strokeBounds.h))
        };

        f32 minX = corners[0].x, maxX = corners[0].x;
        f32 minY = corners[0].y, maxY = corners[0].y;
        for (int i = 1; i < 4; ++i) {
            minX = std::min(minX, corners[i].x);
            maxX = std::max(maxX, corners[i].x);
            minY = std::min(minY, corners[i].y);
            maxY = std::max(maxY, corners[i].y);
        }

        Rect docBounds(minX, minY, maxX - minX, maxY - minY);
        doc.notifyChanged(docBounds);
    } else if (isPencilMode() && strokeLayer) {
        // Prune empty tiles for pencil mode too
        strokeLayer->canvas.pruneEmptyTiles();
    }

    // Cleanup
    stroking = false;
    strokeBuffer.reset();
    strokeLayer = nullptr;
    strokeBounds = Rect();
}

void EraserTool::renderOverlay(Framebuffer& fb, const Vec2& cursorPos, f32 zoom, const Vec2& pan, const Recti& clipRect) {
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
    } else {
        // Normal eraser mode: draw circle cursor
        i32 radius = static_cast<i32>((size / 2.0f) * zoom);
        if (radius < 1) radius = 1;

        fb.drawCircle(cx, cy, radius, 0x000000FF, thickness);
        if (radius > thickness) {
            fb.drawCircle(cx, cy, radius - thickness, 0xFFFFFFFF, thickness);
        }
    }
}
