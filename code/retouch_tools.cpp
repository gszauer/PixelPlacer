#include "retouch_tools.h"

// Helper to check if a pixel is within selection (in document space)
static inline bool isInSelection(const Selection* sel, i32 layerX, i32 layerY,
                                  const Matrix3x2& layerToDoc) {
    if (!sel || !sel->hasSelection) return true;

    // Transform layer coords to document coords
    Vec2 docPos = layerToDoc.transform(Vec2(static_cast<f32>(layerX), static_cast<f32>(layerY)));
    i32 docX = static_cast<i32>(std::floor(docPos.x));
    i32 docY = static_cast<i32>(std::floor(docPos.y));

    // Check bounds
    if (docX < 0 || docY < 0 ||
        docX >= static_cast<i32>(sel->width) ||
        docY >= static_cast<i32>(sel->height)) {
        return false;
    }

    return sel->getValue(docX, docY) > 0;
}

// CloneTool implementations
void CloneTool::onMouseDown(Document& doc, const ToolEvent& e) {
    AppState& state = getAppState();

    // Alt+click also works as alternative sampling method
    if (e.altHeld) {
        state.cloneSourcePos = e.position;
        state.cloneSourceSet = true;
        state.cloneSampleMode = false;
        state.needsRedraw = true;
        return;
    }

    // Sample mode: first click samples and turns off sample mode
    if (state.cloneSampleMode) {
        state.cloneSourcePos = e.position;
        state.cloneSourceSet = true;
        state.cloneSampleMode = false;
        state.needsRedraw = true;
        return;
    }

    // No source set yet - can't clone
    if (!state.cloneSourceSet) return;

    PixelLayer* layer = doc.getActivePixelLayer();
    if (!layer || layer->locked) return;

    // Begin undo
    doc.beginPixelUndo("Clone", doc.activeLayerIndex);

    updateStamp();
    stroking = true;
    lastPos = e.position;
    firstStrokePos = e.position;
    strokeLayer = layer;

    // Compute transforms
    layerToDocTransform = layer->transform.toMatrix(layer->canvas.width, layer->canvas.height);
    docToLayerTransform = layerToDocTransform.inverted();

    // Store selection reference
    strokeSelection = doc.selection.hasSelection ? &doc.selection : nullptr;

    // Create snapshot of the layer so we sample from original pixels, not newly cloned ones
    sourceSnapshot = std::make_unique<TiledCanvas>(layer->canvas.width, layer->canvas.height);
    layer->canvas.forEachPixel([this](u32 x, u32 y, u32 pixel) {
        if (pixel & 0xFF) {  // Has alpha
            sourceSnapshot->setPixel(x, y, pixel);
        }
    });

    // Capture tiles before modifying
    f32 size = state.brushSize;
    Recti affectedRect(
        static_cast<i32>(e.position.x - size),
        static_cast<i32>(e.position.y - size),
        static_cast<i32>(size * 2) + 1,
        static_cast<i32>(size * 2) + 1
    );
    doc.captureOriginalTilesInRect(doc.activeLayerIndex, affectedRect);

    cloneAt(layer->canvas, e.position, e.pressure, docToLayerTransform);
    doc.notifyChanged(Rect(e.position.x - size, e.position.y - size, size * 2, size * 2));
}

void CloneTool::onMouseDrag(Document& doc, const ToolEvent& e) {
    AppState& state = getAppState();
    if (!stroking || !state.cloneSourceSet || !sourceSnapshot || !strokeLayer) return;

    PixelLayer* layer = strokeLayer;
    if (layer->locked) return;

    f32 size = state.brushSize;

    // Capture tiles along the stroke path for undo
    Recti strokeRect(
        static_cast<i32>(std::min(lastPos.x, e.position.x) - size),
        static_cast<i32>(std::min(lastPos.y, e.position.y) - size),
        static_cast<i32>(std::abs(e.position.x - lastPos.x) + size * 2) + 1,
        static_cast<i32>(std::abs(e.position.y - lastPos.y) + size * 2) + 1
    );
    doc.captureOriginalTilesInRect(doc.activeLayerIndex, strokeRect);

    // Clone along the stroke
    Vec2 delta = e.position - lastPos;
    f32 distance = delta.length();
    f32 stepSize = std::max(1.0f, stamp.size * 0.25f);
    i32 steps = std::max(1, static_cast<i32>(distance / stepSize));

    for (i32 i = 1; i <= steps; ++i) {
        Vec2 pos = lastPos + delta * (static_cast<f32>(i) / steps);
        cloneAt(layer->canvas, pos, e.pressure, docToLayerTransform);
    }

    Rect dirty(
        std::min(lastPos.x, e.position.x) - size,
        std::min(lastPos.y, e.position.y) - size,
        std::abs(e.position.x - lastPos.x) + size * 2,
        std::abs(e.position.y - lastPos.y) + size * 2
    );

    lastPos = e.position;
    doc.notifyChanged(dirty);
}

void CloneTool::onMouseUp(Document& doc, const ToolEvent& e) {
    // Commit undo
    doc.commitUndo();

    stroking = false;
    sourceSnapshot.reset();  // Free the snapshot
    strokeLayer = nullptr;
}

void CloneTool::renderOverlay(Framebuffer& fb, const Vec2& cursorPos, f32 zoom, const Vec2& pan, const Recti& clipRect) {
    AppState& state = getAppState();
    f32 size = state.brushSize;

    i32 cx = static_cast<i32>(cursorPos.x);
    i32 cy = static_cast<i32>(cursorPos.y);
    i32 thickness = static_cast<i32>(Config::uiScale);

    // Draw brush circle cursor
    i32 radius = static_cast<i32>((size / 2.0f) * zoom);
    if (radius < 1) radius = 1;

    fb.drawCircle(cx, cy, radius, 0x000000FF, thickness);
    if (radius > thickness) {
        fb.drawCircle(cx, cy, radius - thickness, 0xFFFFFFFF, thickness);
    }

    // If source is set and we're not in sample mode, show source indicator
    if (state.cloneSourceSet && !state.cloneSampleMode && stroking) {
        // Calculate source position on screen
        Vec2 offset = Vec2(cx / zoom - pan.x, cy / zoom - pan.y) - firstStrokePos;
        Vec2 srcDocPos = state.cloneSourcePos + offset;
        i32 srcX = static_cast<i32>((srcDocPos.x + pan.x) * zoom);
        i32 srcY = static_cast<i32>((srcDocPos.y + pan.y) * zoom);

        // Draw crosshair at source position
        i32 crossSize = static_cast<i32>(8 * Config::uiScale);

        // Draw black outline
        for (i32 t = -1; t <= 1; ++t) {
            fb.drawHorizontalLine(srcX - crossSize, srcX + crossSize, srcY + t, 0x000000FF);
            fb.drawVerticalLine(srcX + t, srcY - crossSize, srcY + crossSize, 0x000000FF);
        }
        // Draw white center
        fb.drawHorizontalLine(srcX - crossSize + 1, srcX + crossSize - 1, srcY, 0xFFFFFFFF);
        fb.drawVerticalLine(srcX, srcY - crossSize + 1, srcY + crossSize - 1, 0xFFFFFFFF);

        // Draw source circle
        fb.drawCircle(srcX, srcY, radius, 0x00FF00AA, thickness);
    }
}

void CloneTool::updateStamp() {
    AppState& state = getAppState();
    if (cachedSize != state.brushSize || cachedHardness != state.brushHardness || stampDirty) {
        cachedSize = state.brushSize;
        cachedHardness = state.brushHardness;
        stamp = BrushRenderer::generateStamp(cachedSize, cachedHardness);
        stampDirty = false;
    }
}

void CloneTool::cloneAt(TiledCanvas& canvas, const Vec2& destPos, f32 pressure, const Matrix3x2& docToLayer) {
    if (!sourceSnapshot) return;

    AppState& state = getAppState();

    // Calculate source position in document space: source point + offset from first stroke position
    Vec2 offset = destPos - firstStrokePos;
    Vec2 srcDocPos = state.cloneSourcePos + offset;

    // Transform to layer space
    Vec2 destLayerPos = docToLayer.transform(destPos);
    Vec2 srcLayerPos = docToLayer.transform(srcDocPos);

    // Apply pressure curve
    f32 adjustedPressure = evaluatePressureCurve(pressure, state.pressureCurveCP1, state.pressureCurveCP2);

    // Get opacity and flow
    f32 opacity = state.brushOpacity;
    f32 flow = state.brushFlow;
    f32 size = cachedSize;

    // Apply pressure based on mode
    switch (state.clonePressureMode) {
        case 1: // Size
            size = cachedSize * adjustedPressure;
            if (size < 1.0f) return;
            // Regenerate stamp for different size
            stamp = BrushRenderer::generateStamp(size, cachedHardness);
            break;
        case 2: // Opacity
            opacity *= adjustedPressure;
            break;
        case 3: // Flow
            flow *= adjustedPressure;
            break;
        default:
            break;
    }

    i32 startX = static_cast<i32>(destLayerPos.x - stamp.size / 2.0f);
    i32 startY = static_cast<i32>(destLayerPos.y - stamp.size / 2.0f);
    i32 srcStartX = static_cast<i32>(srcLayerPos.x - stamp.size / 2.0f);
    i32 srcStartY = static_cast<i32>(srcLayerPos.y - stamp.size / 2.0f);

    for (u32 by = 0; by < stamp.size; ++by) {
        for (u32 bx = 0; bx < stamp.size; ++bx) {
            f32 brushAlpha = stamp.getAlpha(bx, by);
            if (brushAlpha <= 0.0f) continue;

            i32 dx = startX + bx;
            i32 dy = startY + by;
            i32 sx = srcStartX + bx;
            i32 sy = srcStartY + by;

            // Check selection mask (destination must be in selection)
            if (!isInSelection(strokeSelection, dx, dy, layerToDocTransform)) continue;

            // Read from snapshot (original pixels), not the live canvas
            // TiledCanvas handles any coordinates - returns 0 for non-existent tiles
            u32 srcPixel = sourceSnapshot->getPixel(sx, sy);
            if ((srcPixel & 0xFF) == 0) continue;  // Skip transparent source pixels

            f32 finalAlpha = brushAlpha * opacity * flow;

            u8 r, g, b, a;
            Blend::unpack(srcPixel, r, g, b, a);
            u32 stampColor = Blend::pack(r, g, b, static_cast<u8>(a * finalAlpha));

            canvas.blendPixel(dx, dy, stampColor);
        }
    }
}

// SmudgeTool implementations
void SmudgeTool::onMouseDown(Document& doc, const ToolEvent& e) {
    PixelLayer* layer = doc.getActivePixelLayer();
    if (!layer || layer->locked) return;

    // Begin undo
    doc.beginPixelUndo("Smudge", doc.activeLayerIndex);

    updateStamp();
    stroking = true;
    lastPos = e.position;
    strokeLayer = layer;

    // Compute transforms
    layerToDocTransform = layer->transform.toMatrix(layer->canvas.width, layer->canvas.height);
    docToLayerTransform = layerToDocTransform.inverted();

    // Store selection reference
    strokeSelection = doc.selection.hasSelection ? &doc.selection : nullptr;

    // Transform to layer space
    Vec2 layerPos = docToLayerTransform.transform(e.position);

    // Initialize carried color buffer by sampling from canvas
    sampleCarriedColors(layer->canvas, layerPos);

    // Capture tiles before modifying
    AppState& state = getAppState();
    f32 size = state.brushSize;
    Recti affectedRect(
        static_cast<i32>(e.position.x - size),
        static_cast<i32>(e.position.y - size),
        static_cast<i32>(size * 2) + 1,
        static_cast<i32>(size * 2) + 1
    );
    doc.captureOriginalTilesInRect(doc.activeLayerIndex, affectedRect);

    // Apply first smudge dab
    smudgeAt(layer->canvas, layerPos, e.pressure);

    doc.notifyChanged(Rect(e.position.x - size, e.position.y - size, size * 2, size * 2));
}

void SmudgeTool::onMouseDrag(Document& doc, const ToolEvent& e) {
    if (!stroking || !strokeLayer) return;

    PixelLayer* layer = strokeLayer;
    if (layer->locked) return;

    AppState& state = getAppState();
    f32 size = state.brushSize;

    // Capture tiles along the stroke path for undo
    Recti strokeRect(
        static_cast<i32>(std::min(lastPos.x, e.position.x) - size),
        static_cast<i32>(std::min(lastPos.y, e.position.y) - size),
        static_cast<i32>(std::abs(e.position.x - lastPos.x) + size * 2) + 1,
        static_cast<i32>(std::abs(e.position.y - lastPos.y) + size * 2) + 1
    );
    doc.captureOriginalTilesInRect(doc.activeLayerIndex, strokeRect);

    // Transform positions to layer space
    Vec2 lastLayerPos = docToLayerTransform.transform(lastPos);
    Vec2 currLayerPos = docToLayerTransform.transform(e.position);

    // Interpolate dabs along stroke in layer space
    Vec2 delta = currLayerPos - lastLayerPos;
    f32 distance = delta.length();
    f32 stepSize = std::max(1.0f, stamp.size * 0.25f);
    i32 steps = std::max(1, static_cast<i32>(distance / stepSize));

    for (i32 i = 1; i <= steps; ++i) {
        Vec2 layerPos = lastLayerPos + delta * (static_cast<f32>(i) / steps);
        smudgeAt(layer->canvas, layerPos, e.pressure);
    }

    Rect dirty(
        std::min(lastPos.x, e.position.x) - size,
        std::min(lastPos.y, e.position.y) - size,
        std::abs(e.position.x - lastPos.x) + size * 2,
        std::abs(e.position.y - lastPos.y) + size * 2
    );

    lastPos = e.position;
    doc.notifyChanged(dirty);
}

void SmudgeTool::onMouseUp(Document& doc, const ToolEvent& e) {
    // Commit undo
    doc.commitUndo();

    stroking = false;
    carriedColors.clear();
    carriedSize = 0;
    strokeLayer = nullptr;
}

void SmudgeTool::renderOverlay(Framebuffer& fb, const Vec2& cursorPos, f32 zoom, const Vec2& pan, const Recti& clipRect) {
    AppState& state = getAppState();
    f32 size = state.brushSize;

    i32 cx = static_cast<i32>(cursorPos.x);
    i32 cy = static_cast<i32>(cursorPos.y);
    i32 thickness = static_cast<i32>(Config::uiScale);

    // Draw brush circle cursor
    i32 radius = static_cast<i32>((size / 2.0f) * zoom);
    if (radius < 1) radius = 1;

    fb.drawCircle(cx, cy, radius, 0x000000FF, thickness);
    if (radius > thickness) {
        fb.drawCircle(cx, cy, radius - thickness, 0xFFFFFFFF, thickness);
    }
}

void SmudgeTool::updateStamp() {
    AppState& state = getAppState();
    if (cachedSize != state.brushSize || cachedHardness != state.brushHardness) {
        cachedSize = state.brushSize;
        cachedHardness = state.brushHardness;
        stamp = BrushRenderer::generateStamp(cachedSize, cachedHardness);
    }
}

void SmudgeTool::sampleCarriedColors(TiledCanvas& canvas, const Vec2& layerPos) {
    carriedSize = stamp.size;
    carriedColors.resize(carriedSize * carriedSize, 0);

    i32 startX = static_cast<i32>(layerPos.x - stamp.size / 2.0f);
    i32 startY = static_cast<i32>(layerPos.y - stamp.size / 2.0f);

    for (u32 by = 0; by < stamp.size; ++by) {
        for (u32 bx = 0; bx < stamp.size; ++bx) {
            i32 x = startX + bx;
            i32 y = startY + by;
            // TiledCanvas handles any coordinates - returns 0 for non-existent tiles
            carriedColors[by * carriedSize + bx] = canvas.getPixel(x, y);
        }
    }
}

void SmudgeTool::smudgeAt(TiledCanvas& canvas, const Vec2& layerPos, f32 pressure) {
    if (carriedColors.empty()) return;

    AppState& state = getAppState();

    // Apply pressure curve
    f32 adjustedPressure = evaluatePressureCurve(pressure, state.pressureCurveCP1, state.pressureCurveCP2);

    // Get settings
    f32 strength = state.brushOpacity;  // Opacity controls smudge strength
    f32 flow = state.brushFlow;
    f32 size = cachedSize;

    // Apply pressure based on mode
    switch (state.smudgePressureMode) {
        case 1: // Size
            size = cachedSize * adjustedPressure;
            if (size < 1.0f) return;
            stamp = BrushRenderer::generateStamp(size, cachedHardness);
            break;
        case 2: // Opacity (strength)
            strength *= adjustedPressure;
            break;
        case 3: // Flow
            flow *= adjustedPressure;
            break;
        default:
            break;
    }

    f32 effectiveStrength = strength * flow;
    f32 pickupRate = 0.5f;  // How much color to pick up from destination

    i32 startX = static_cast<i32>(layerPos.x - stamp.size / 2.0f);
    i32 startY = static_cast<i32>(layerPos.y - stamp.size / 2.0f);

    for (u32 by = 0; by < stamp.size; ++by) {
        for (u32 bx = 0; bx < stamp.size; ++bx) {
            f32 brushAlpha = stamp.getAlpha(bx, by);
            if (brushAlpha <= 0.0f) continue;

            i32 x = startX + bx;
            i32 y = startY + by;

            // Check selection mask
            if (!isInSelection(strokeSelection, x, y, layerToDocTransform)) continue;

            // Get carried color (scale index if stamp size changed)
            u32 carriedIdx;
            if (stamp.size == carriedSize) {
                carriedIdx = by * carriedSize + bx;
            } else {
                // Sample from carried buffer with scaling
                u32 cx = bx * carriedSize / stamp.size;
                u32 cy = by * carriedSize / stamp.size;
                carriedIdx = cy * carriedSize + cx;
            }

            if (carriedIdx >= carriedColors.size()) continue;

            u32 carriedPixel = carriedColors[carriedIdx];
            u32 destPixel = canvas.getPixel(x, y);

            // Unpack colors
            u8 cr, cg, cb, ca;
            u8 dr, dg, db, da;
            Blend::unpack(carriedPixel, cr, cg, cb, ca);
            Blend::unpack(destPixel, dr, dg, db, da);

            // Blend carried color onto destination
            f32 t = effectiveStrength * brushAlpha;
            u8 nr = static_cast<u8>(dr + (cr - dr) * t);
            u8 ng = static_cast<u8>(dg + (cg - dg) * t);
            u8 nb = static_cast<u8>(db + (cb - db) * t);
            u8 na = static_cast<u8>(da + (ca - da) * t);

            canvas.setPixel(x, y, Blend::pack(nr, ng, nb, na));

            // Pick up some destination color into carried buffer
            f32 p = pickupRate * brushAlpha;
            u8 ncr = static_cast<u8>(cr + (dr - cr) * p);
            u8 ncg = static_cast<u8>(cg + (dg - cg) * p);
            u8 ncb = static_cast<u8>(cb + (db - cb) * p);
            u8 nca = static_cast<u8>(ca + (da - ca) * p);
            carriedColors[carriedIdx] = Blend::pack(ncr, ncg, ncb, nca);
        }
    }
}

// DodgeTool implementations
void DodgeTool::onMouseDown(Document& doc, const ToolEvent& e) {
    PixelLayer* layer = doc.getActivePixelLayer();
    if (!layer || layer->locked) return;

    // Begin undo
    doc.beginPixelUndo("Dodge", doc.activeLayerIndex);

    updateStamp();
    stroking = true;
    lastPos = e.position;
    strokeLayer = layer;

    // Compute transforms
    layerToDocTransform = layer->transform.toMatrix(layer->canvas.width, layer->canvas.height);
    docToLayerTransform = layerToDocTransform.inverted();

    // Store selection reference
    strokeSelection = doc.selection.hasSelection ? &doc.selection : nullptr;

    // Transform to layer space
    Vec2 layerPos = docToLayerTransform.transform(e.position);

    // Capture tiles before modifying
    AppState& state = getAppState();
    f32 size = state.brushSize;
    Recti affectedRect(
        static_cast<i32>(e.position.x - size),
        static_cast<i32>(e.position.y - size),
        static_cast<i32>(size * 2) + 1,
        static_cast<i32>(size * 2) + 1
    );
    doc.captureOriginalTilesInRect(doc.activeLayerIndex, affectedRect);

    dodgeAt(layer->canvas, layerPos, e.pressure);
    doc.notifyChanged(Rect(e.position.x - size, e.position.y - size, size * 2, size * 2));
}

void DodgeTool::onMouseDrag(Document& doc, const ToolEvent& e) {
    if (!stroking || !strokeLayer) return;

    PixelLayer* layer = strokeLayer;
    if (layer->locked) return;

    AppState& state = getAppState();
    f32 size = state.brushSize;

    // Capture tiles along the stroke path for undo
    Recti strokeRect(
        static_cast<i32>(std::min(lastPos.x, e.position.x) - size),
        static_cast<i32>(std::min(lastPos.y, e.position.y) - size),
        static_cast<i32>(std::abs(e.position.x - lastPos.x) + size * 2) + 1,
        static_cast<i32>(std::abs(e.position.y - lastPos.y) + size * 2) + 1
    );
    doc.captureOriginalTilesInRect(doc.activeLayerIndex, strokeRect);

    // Transform positions to layer space
    Vec2 lastLayerPos = docToLayerTransform.transform(lastPos);
    Vec2 currLayerPos = docToLayerTransform.transform(e.position);

    // Interpolate dabs along stroke in layer space
    Vec2 delta = currLayerPos - lastLayerPos;
    f32 distance = delta.length();
    f32 stepSize = std::max(1.0f, stamp.size * 0.25f);
    i32 steps = std::max(1, static_cast<i32>(distance / stepSize));

    for (i32 i = 1; i <= steps; ++i) {
        Vec2 layerPos = lastLayerPos + delta * (static_cast<f32>(i) / steps);
        dodgeAt(layer->canvas, layerPos, e.pressure);
    }

    Rect dirty(
        std::min(lastPos.x, e.position.x) - size,
        std::min(lastPos.y, e.position.y) - size,
        std::abs(e.position.x - lastPos.x) + size * 2,
        std::abs(e.position.y - lastPos.y) + size * 2
    );

    lastPos = e.position;
    doc.notifyChanged(dirty);
}

void DodgeTool::onMouseUp(Document& doc, const ToolEvent& e) {
    // Commit undo
    doc.commitUndo();

    stroking = false;
    strokeLayer = nullptr;
}

void DodgeTool::renderOverlay(Framebuffer& fb, const Vec2& cursorPos, f32 zoom, const Vec2& pan, const Recti& clipRect) {
    AppState& state = getAppState();
    f32 size = state.brushSize;

    i32 cx = static_cast<i32>(cursorPos.x);
    i32 cy = static_cast<i32>(cursorPos.y);
    i32 thickness = static_cast<i32>(Config::uiScale);

    i32 radius = static_cast<i32>((size / 2.0f) * zoom);
    if (radius < 1) radius = 1;

    fb.drawCircle(cx, cy, radius, 0x000000FF, thickness);
    if (radius > thickness) {
        fb.drawCircle(cx, cy, radius - thickness, 0xFFFFFFFF, thickness);
    }
}

void DodgeTool::updateStamp() {
    AppState& state = getAppState();
    if (cachedSize != state.brushSize || cachedHardness != state.brushHardness) {
        cachedSize = state.brushSize;
        cachedHardness = state.brushHardness;
        stamp = BrushRenderer::generateStamp(cachedSize, cachedHardness);
    }
}

void DodgeTool::dodgeAt(TiledCanvas& canvas, const Vec2& layerPos, f32 pressure) {
    AppState& state = getAppState();

    // Apply pressure curve
    f32 adjustedPressure = evaluatePressureCurve(pressure, state.pressureCurveCP1, state.pressureCurveCP2);

    // Get settings
    f32 exposure = state.brushOpacity;
    f32 flow = state.brushFlow;
    f32 size = cachedSize;

    // Apply pressure based on mode
    switch (state.dodgeBurnPressureMode) {
        case 1: // Size
            size = cachedSize * adjustedPressure;
            if (size < 1.0f) return;
            stamp = BrushRenderer::generateStamp(size, cachedHardness);
            break;
        case 2: // Exposure
            exposure *= adjustedPressure;
            break;
        case 3: // Flow
            flow *= adjustedPressure;
            break;
        default:
            break;
    }

    f32 effectiveExposure = exposure * flow * 0.1f;

    i32 startX = static_cast<i32>(layerPos.x - stamp.size / 2.0f);
    i32 startY = static_cast<i32>(layerPos.y - stamp.size / 2.0f);

    for (u32 by = 0; by < stamp.size; ++by) {
        for (u32 bx = 0; bx < stamp.size; ++bx) {
            f32 brushAlpha = stamp.getAlpha(bx, by);
            if (brushAlpha <= 0.0f) continue;

            i32 x = startX + bx;
            i32 y = startY + by;

            // Check selection mask
            if (!isInSelection(strokeSelection, x, y, layerToDocTransform)) continue;

            u32 pixel = canvas.getPixel(x, y);
            u8 r, g, b, a;
            Blend::unpack(pixel, r, g, b, a);

            if (a == 0) continue;

            f32 amount = brushAlpha * effectiveExposure;

            // Lighten
            r = static_cast<u8>(std::min(255.0f, r + (255 - r) * amount));
            g = static_cast<u8>(std::min(255.0f, g + (255 - g) * amount));
            b = static_cast<u8>(std::min(255.0f, b + (255 - b) * amount));

            canvas.setPixel(x, y, Blend::pack(r, g, b, a));
        }
    }
}

// BurnTool implementations
void BurnTool::onMouseDown(Document& doc, const ToolEvent& e) {
    PixelLayer* layer = doc.getActivePixelLayer();
    if (!layer || layer->locked) return;

    // Begin undo
    doc.beginPixelUndo("Burn", doc.activeLayerIndex);

    updateStamp();
    stroking = true;
    lastPos = e.position;
    strokeLayer = layer;

    // Compute transforms
    layerToDocTransform = layer->transform.toMatrix(layer->canvas.width, layer->canvas.height);
    docToLayerTransform = layerToDocTransform.inverted();

    // Store selection reference
    strokeSelection = doc.selection.hasSelection ? &doc.selection : nullptr;

    // Transform to layer space
    Vec2 layerPos = docToLayerTransform.transform(e.position);

    // Capture tiles before modifying
    AppState& state = getAppState();
    f32 size = state.brushSize;
    Recti affectedRect(
        static_cast<i32>(e.position.x - size),
        static_cast<i32>(e.position.y - size),
        static_cast<i32>(size * 2) + 1,
        static_cast<i32>(size * 2) + 1
    );
    doc.captureOriginalTilesInRect(doc.activeLayerIndex, affectedRect);

    burnAt(layer->canvas, layerPos, e.pressure);
    doc.notifyChanged(Rect(e.position.x - size, e.position.y - size, size * 2, size * 2));
}

void BurnTool::onMouseDrag(Document& doc, const ToolEvent& e) {
    if (!stroking || !strokeLayer) return;

    PixelLayer* layer = strokeLayer;
    if (layer->locked) return;

    AppState& state = getAppState();
    f32 size = state.brushSize;

    // Capture tiles along the stroke path for undo
    Recti strokeRect(
        static_cast<i32>(std::min(lastPos.x, e.position.x) - size),
        static_cast<i32>(std::min(lastPos.y, e.position.y) - size),
        static_cast<i32>(std::abs(e.position.x - lastPos.x) + size * 2) + 1,
        static_cast<i32>(std::abs(e.position.y - lastPos.y) + size * 2) + 1
    );
    doc.captureOriginalTilesInRect(doc.activeLayerIndex, strokeRect);

    // Transform positions to layer space
    Vec2 lastLayerPos = docToLayerTransform.transform(lastPos);
    Vec2 currLayerPos = docToLayerTransform.transform(e.position);

    // Interpolate dabs along stroke in layer space
    Vec2 delta = currLayerPos - lastLayerPos;
    f32 distance = delta.length();
    f32 stepSize = std::max(1.0f, stamp.size * 0.25f);
    i32 steps = std::max(1, static_cast<i32>(distance / stepSize));

    for (i32 i = 1; i <= steps; ++i) {
        Vec2 layerPos = lastLayerPos + delta * (static_cast<f32>(i) / steps);
        burnAt(layer->canvas, layerPos, e.pressure);
    }

    Rect dirty(
        std::min(lastPos.x, e.position.x) - size,
        std::min(lastPos.y, e.position.y) - size,
        std::abs(e.position.x - lastPos.x) + size * 2,
        std::abs(e.position.y - lastPos.y) + size * 2
    );

    lastPos = e.position;
    doc.notifyChanged(dirty);
}

void BurnTool::onMouseUp(Document& doc, const ToolEvent& e) {
    // Commit undo
    doc.commitUndo();

    stroking = false;
    strokeLayer = nullptr;
}

void BurnTool::renderOverlay(Framebuffer& fb, const Vec2& cursorPos, f32 zoom, const Vec2& pan, const Recti& clipRect) {
    AppState& state = getAppState();
    f32 size = state.brushSize;

    i32 cx = static_cast<i32>(cursorPos.x);
    i32 cy = static_cast<i32>(cursorPos.y);
    i32 thickness = static_cast<i32>(Config::uiScale);

    i32 radius = static_cast<i32>((size / 2.0f) * zoom);
    if (radius < 1) radius = 1;

    fb.drawCircle(cx, cy, radius, 0x000000FF, thickness);
    if (radius > thickness) {
        fb.drawCircle(cx, cy, radius - thickness, 0xFFFFFFFF, thickness);
    }
}

void BurnTool::updateStamp() {
    AppState& state = getAppState();
    if (cachedSize != state.brushSize || cachedHardness != state.brushHardness) {
        cachedSize = state.brushSize;
        cachedHardness = state.brushHardness;
        stamp = BrushRenderer::generateStamp(cachedSize, cachedHardness);
    }
}

void BurnTool::burnAt(TiledCanvas& canvas, const Vec2& layerPos, f32 pressure) {
    AppState& state = getAppState();

    // Apply pressure curve
    f32 adjustedPressure = evaluatePressureCurve(pressure, state.pressureCurveCP1, state.pressureCurveCP2);

    // Get settings
    f32 exposure = state.brushOpacity;
    f32 flow = state.brushFlow;
    f32 size = cachedSize;

    // Apply pressure based on mode
    switch (state.dodgeBurnPressureMode) {
        case 1: // Size
            size = cachedSize * adjustedPressure;
            if (size < 1.0f) return;
            stamp = BrushRenderer::generateStamp(size, cachedHardness);
            break;
        case 2: // Exposure
            exposure *= adjustedPressure;
            break;
        case 3: // Flow
            flow *= adjustedPressure;
            break;
        default:
            break;
    }

    f32 effectiveExposure = exposure * flow * 0.1f;

    i32 startX = static_cast<i32>(layerPos.x - stamp.size / 2.0f);
    i32 startY = static_cast<i32>(layerPos.y - stamp.size / 2.0f);

    for (u32 by = 0; by < stamp.size; ++by) {
        for (u32 bx = 0; bx < stamp.size; ++bx) {
            f32 brushAlpha = stamp.getAlpha(bx, by);
            if (brushAlpha <= 0.0f) continue;

            i32 x = startX + bx;
            i32 y = startY + by;

            // Check selection mask
            if (!isInSelection(strokeSelection, x, y, layerToDocTransform)) continue;

            u32 pixel = canvas.getPixel(x, y);
            u8 r, g, b, a;
            Blend::unpack(pixel, r, g, b, a);

            if (a == 0) continue;

            f32 amount = brushAlpha * effectiveExposure;

            // Darken
            r = static_cast<u8>(std::max(0.0f, r - r * amount));
            g = static_cast<u8>(std::max(0.0f, g - g * amount));
            b = static_cast<u8>(std::max(0.0f, b - b * amount));

            canvas.setPixel(x, y, Blend::pack(r, g, b, a));
        }
    }
}
