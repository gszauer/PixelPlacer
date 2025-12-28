#include "retouch_tools.h"

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

    updateStamp();
    stroking = true;
    lastPos = e.position;
    firstStrokePos = e.position;

    // Create snapshot of the layer so we sample from original pixels, not newly cloned ones
    sourceSnapshot = std::make_unique<TiledCanvas>(layer->canvas.width, layer->canvas.height);
    layer->canvas.forEachPixel([this](u32 x, u32 y, u32 pixel) {
        if (pixel & 0xFF) {  // Has alpha
            sourceSnapshot->setPixel(x, y, pixel);
        }
    });

    cloneAt(layer->canvas, e.position, e.pressure);
    f32 size = state.brushSize;
    doc.notifyChanged(Rect(e.position.x - size, e.position.y - size, size * 2, size * 2));
}

void CloneTool::onMouseDrag(Document& doc, const ToolEvent& e) {
    AppState& state = getAppState();
    if (!stroking || !state.cloneSourceSet || !sourceSnapshot) return;

    PixelLayer* layer = doc.getActivePixelLayer();
    if (!layer || layer->locked) return;

    // Clone along the stroke
    Vec2 delta = e.position - lastPos;
    f32 distance = delta.length();
    f32 stepSize = std::max(1.0f, stamp.size * 0.25f);
    i32 steps = std::max(1, static_cast<i32>(distance / stepSize));

    for (i32 i = 1; i <= steps; ++i) {
        Vec2 pos = lastPos + delta * (static_cast<f32>(i) / steps);
        cloneAt(layer->canvas, pos, e.pressure);
    }

    f32 size = state.brushSize;
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
    stroking = false;
    sourceSnapshot.reset();  // Free the snapshot
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

void CloneTool::cloneAt(TiledCanvas& canvas, const Vec2& destPos, f32 pressure) {
    if (!sourceSnapshot) return;

    AppState& state = getAppState();

    // Calculate source position: source point + offset from first stroke position
    Vec2 offset = destPos - firstStrokePos;
    Vec2 srcPos = state.cloneSourcePos + offset;

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

    i32 startX = static_cast<i32>(destPos.x - stamp.size / 2.0f);
    i32 startY = static_cast<i32>(destPos.y - stamp.size / 2.0f);

    for (u32 by = 0; by < stamp.size; ++by) {
        for (u32 bx = 0; bx < stamp.size; ++bx) {
            f32 brushAlpha = stamp.getAlpha(bx, by);
            if (brushAlpha <= 0.0f) continue;

            i32 dx = startX + bx;
            i32 dy = startY + by;
            i32 sx = static_cast<i32>(srcPos.x - stamp.size / 2.0f) + bx;
            i32 sy = static_cast<i32>(srcPos.y - stamp.size / 2.0f) + by;

            if (dx < 0 || dy < 0 || dx >= static_cast<i32>(canvas.width) ||
                dy >= static_cast<i32>(canvas.height)) continue;
            if (sx < 0 || sy < 0 || sx >= static_cast<i32>(sourceSnapshot->width) ||
                sy >= static_cast<i32>(sourceSnapshot->height)) continue;

            // Read from snapshot (original pixels), not the live canvas
            u32 srcPixel = sourceSnapshot->getPixel(sx, sy);
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

    updateStamp();
    stroking = true;
    lastPos = e.position;

    // Initialize carried color buffer by sampling from canvas
    sampleCarriedColors(layer->canvas, e.position);

    // Apply first smudge dab
    smudgeAt(layer->canvas, e.position, e.pressure);

    AppState& state = getAppState();
    f32 size = state.brushSize;
    doc.notifyChanged(Rect(e.position.x - size, e.position.y - size, size * 2, size * 2));
}

void SmudgeTool::onMouseDrag(Document& doc, const ToolEvent& e) {
    if (!stroking) return;

    PixelLayer* layer = doc.getActivePixelLayer();
    if (!layer || layer->locked) return;

    // Interpolate dabs along stroke
    Vec2 delta = e.position - lastPos;
    f32 distance = delta.length();
    f32 stepSize = std::max(1.0f, stamp.size * 0.25f);
    i32 steps = std::max(1, static_cast<i32>(distance / stepSize));

    for (i32 i = 1; i <= steps; ++i) {
        Vec2 pos = lastPos + delta * (static_cast<f32>(i) / steps);
        smudgeAt(layer->canvas, pos, e.pressure);
    }

    AppState& state = getAppState();
    f32 size = state.brushSize;
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
    stroking = false;
    carriedColors.clear();
    carriedSize = 0;
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

void SmudgeTool::sampleCarriedColors(TiledCanvas& canvas, const Vec2& pos) {
    carriedSize = stamp.size;
    carriedColors.resize(carriedSize * carriedSize, 0);

    i32 startX = static_cast<i32>(pos.x - stamp.size / 2.0f);
    i32 startY = static_cast<i32>(pos.y - stamp.size / 2.0f);

    for (u32 by = 0; by < stamp.size; ++by) {
        for (u32 bx = 0; bx < stamp.size; ++bx) {
            i32 x = startX + bx;
            i32 y = startY + by;

            if (x >= 0 && y >= 0 && x < static_cast<i32>(canvas.width) &&
                y < static_cast<i32>(canvas.height)) {
                carriedColors[by * carriedSize + bx] = canvas.getPixel(x, y);
            }
        }
    }
}

void SmudgeTool::smudgeAt(TiledCanvas& canvas, const Vec2& pos, f32 pressure) {
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

    i32 startX = static_cast<i32>(pos.x - stamp.size / 2.0f);
    i32 startY = static_cast<i32>(pos.y - stamp.size / 2.0f);

    for (u32 by = 0; by < stamp.size; ++by) {
        for (u32 bx = 0; bx < stamp.size; ++bx) {
            f32 brushAlpha = stamp.getAlpha(bx, by);
            if (brushAlpha <= 0.0f) continue;

            i32 x = startX + bx;
            i32 y = startY + by;

            if (x < 0 || y < 0 || x >= static_cast<i32>(canvas.width) ||
                y >= static_cast<i32>(canvas.height)) continue;

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

    updateStamp();
    stroking = true;
    lastPos = e.position;

    dodgeAt(layer->canvas, e.position, e.pressure);
    AppState& state = getAppState();
    f32 size = state.brushSize;
    doc.notifyChanged(Rect(e.position.x - size, e.position.y - size, size * 2, size * 2));
}

void DodgeTool::onMouseDrag(Document& doc, const ToolEvent& e) {
    if (!stroking) return;

    PixelLayer* layer = doc.getActivePixelLayer();
    if (!layer || layer->locked) return;

    // Interpolate dabs along stroke
    Vec2 delta = e.position - lastPos;
    f32 distance = delta.length();
    f32 stepSize = std::max(1.0f, stamp.size * 0.25f);
    i32 steps = std::max(1, static_cast<i32>(distance / stepSize));

    for (i32 i = 1; i <= steps; ++i) {
        Vec2 pos = lastPos + delta * (static_cast<f32>(i) / steps);
        dodgeAt(layer->canvas, pos, e.pressure);
    }

    AppState& state = getAppState();
    f32 size = state.brushSize;
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
    stroking = false;
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

void DodgeTool::dodgeAt(TiledCanvas& canvas, const Vec2& pos, f32 pressure) {
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

    i32 startX = static_cast<i32>(pos.x - stamp.size / 2.0f);
    i32 startY = static_cast<i32>(pos.y - stamp.size / 2.0f);

    for (u32 by = 0; by < stamp.size; ++by) {
        for (u32 bx = 0; bx < stamp.size; ++bx) {
            f32 brushAlpha = stamp.getAlpha(bx, by);
            if (brushAlpha <= 0.0f) continue;

            i32 x = startX + bx;
            i32 y = startY + by;

            if (x < 0 || y < 0 || x >= static_cast<i32>(canvas.width) ||
                y >= static_cast<i32>(canvas.height)) continue;

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

    updateStamp();
    stroking = true;
    lastPos = e.position;

    burnAt(layer->canvas, e.position, e.pressure);
    AppState& state = getAppState();
    f32 size = state.brushSize;
    doc.notifyChanged(Rect(e.position.x - size, e.position.y - size, size * 2, size * 2));
}

void BurnTool::onMouseDrag(Document& doc, const ToolEvent& e) {
    if (!stroking) return;

    PixelLayer* layer = doc.getActivePixelLayer();
    if (!layer || layer->locked) return;

    // Interpolate dabs along stroke
    Vec2 delta = e.position - lastPos;
    f32 distance = delta.length();
    f32 stepSize = std::max(1.0f, stamp.size * 0.25f);
    i32 steps = std::max(1, static_cast<i32>(distance / stepSize));

    for (i32 i = 1; i <= steps; ++i) {
        Vec2 pos = lastPos + delta * (static_cast<f32>(i) / steps);
        burnAt(layer->canvas, pos, e.pressure);
    }

    AppState& state = getAppState();
    f32 size = state.brushSize;
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
    stroking = false;
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

void BurnTool::burnAt(TiledCanvas& canvas, const Vec2& pos, f32 pressure) {
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

    i32 startX = static_cast<i32>(pos.x - stamp.size / 2.0f);
    i32 startY = static_cast<i32>(pos.y - stamp.size / 2.0f);

    for (u32 by = 0; by < stamp.size; ++by) {
        for (u32 bx = 0; bx < stamp.size; ++bx) {
            f32 brushAlpha = stamp.getAlpha(bx, by);
            if (brushAlpha <= 0.0f) continue;

            i32 x = startX + bx;
            i32 y = startY + by;

            if (x < 0 || y < 0 || x >= static_cast<i32>(canvas.width) ||
                y >= static_cast<i32>(canvas.height)) continue;

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
