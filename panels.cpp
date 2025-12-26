#include "panels.h"

// PanelHeader implementations
void PanelHeader::render(Framebuffer& fb) {
    if (!visible) return;

    Rect global = globalBounds();

    // Draw header background
    Recti rect(
        static_cast<i32>(global.x),
        static_cast<i32>(global.y),
        static_cast<i32>(global.w),
        static_cast<i32>(global.h)
    );
    fb.fillRect(rect, bgColor);

    // Draw bottom border line
    fb.drawLine(
        static_cast<i32>(global.x),
        static_cast<i32>(global.y + global.h - 1),
        static_cast<i32>(global.x + global.w),
        static_cast<i32>(global.y + global.h - 1),
        Config::COLOR_BORDER
    );

    // Draw title text (centered vertically, left-padded)
    Vec2 textSize = FontRenderer::instance().measureText(title, Config::defaultFontSize());
    f32 textX = global.x + 8 * Config::uiScale;
    f32 textY = global.y + (global.h - textSize.y) / 2;

    FontRenderer::instance().renderText(fb, title,
        static_cast<i32>(textX), static_cast<i32>(textY),
        textColor, Config::defaultFontSize());
}

// NavigatorThumbnail implementations
void NavigatorThumbnail::renderSelf(Framebuffer& fb) {
    if (!view || !view->document) {
        // No document - just fill with background
        Rect gb = globalBounds();
        Recti r(static_cast<i32>(gb.x), static_cast<i32>(gb.y),
                static_cast<i32>(gb.w), static_cast<i32>(gb.h));
        fb.fillRect(r, Config::COLOR_BACKGROUND);
        return;
    }

    Document* doc = view->document;
    Rect gb = globalBounds();

    // Calculate scale to fit document in thumbnail area (maintain aspect ratio)
    f32 scaleX = gb.w / static_cast<f32>(doc->width);
    f32 scaleY = gb.h / static_cast<f32>(doc->height);
    thumbScale = std::min(scaleX, scaleY);

    // Thumbnail dimensions and offset (centered)
    thumbW = static_cast<i32>(doc->width * thumbScale);
    thumbH = static_cast<i32>(doc->height * thumbScale);
    thumbX = static_cast<i32>(gb.x + (gb.w - thumbW) / 2);
    thumbY = static_cast<i32>(gb.y + (gb.h - thumbH) / 2);

    // Fill background around thumbnail
    Recti fullRect(static_cast<i32>(gb.x), static_cast<i32>(gb.y),
                   static_cast<i32>(gb.w), static_cast<i32>(gb.h));
    fb.fillRect(fullRect, Config::COLOR_BACKGROUND);

    // Draw checkerboard for thumbnail area (transparency indicator)
    i32 checkSize = std::max(1, static_cast<i32>(4 * Config::uiScale));
    for (i32 ty = 0; ty < thumbH; ++ty) {
        for (i32 tx = 0; tx < thumbW; ++tx) {
            bool light = ((tx / checkSize) + (ty / checkSize)) % 2 == 0;
            u32 checkColor = light ? Config::CHECKER_COLOR1 : Config::CHECKER_COLOR2;
            fb.setPixel(thumbX + tx, thumbY + ty, checkColor);
        }
    }

    // Sample document pixels at thumbnail resolution
    for (i32 ty = 0; ty < thumbH; ++ty) {
        for (i32 tx = 0; tx < thumbW; ++tx) {
            // Convert thumbnail position to document coordinates
            f32 docX = static_cast<f32>(tx) / thumbScale;
            f32 docY = static_cast<f32>(ty) / thumbScale;

            // Sample all visible layers (simplified compositing)
            u32 pixel = sampleDocumentAt(doc, docX, docY);

            if ((pixel & 0xFF) > 0) {  // Has alpha
                // Blend onto checkerboard
                u32 bgPixel = fb.getPixel(thumbX + tx, thumbY + ty);
                u32 blended = Blend::alphaBlend(bgPixel, pixel);
                fb.setPixel(thumbX + tx, thumbY + ty, blended);
            }
        }
    }

    // Draw border around thumbnail
    Recti thumbRect(thumbX, thumbY, thumbW, thumbH);
    fb.drawRect(thumbRect, Config::COLOR_BORDER, 1);

    // Draw viewport rectangle showing visible area
    drawViewportRect(fb);
}

bool NavigatorThumbnail::onMouseDown(const MouseEvent& e) {
    if (!view || !view->document) return false;

    // Check if click is within thumbnail area
    Rect gb = globalBounds();
    if (e.position.x >= 0 && e.position.x < gb.w &&
        e.position.y >= 0 && e.position.y < gb.h) {
        dragging = true;
        panToThumbnailPos(e.position);
        getAppState().capturedWidget = this;
        return true;
    }
    return false;
}

bool NavigatorThumbnail::onMouseDrag(const MouseEvent& e) {
    if (dragging) {
        panToThumbnailPos(e.position);
        return true;
    }
    return false;
}

bool NavigatorThumbnail::onMouseUp(const MouseEvent& e) {
    if (dragging) {
        dragging = false;
        getAppState().capturedWidget = nullptr;
        return true;
    }
    return false;
}

u32 NavigatorThumbnail::sampleDocumentAt(Document* doc, f32 docX, f32 docY) {
    u32 result = 0;  // Start transparent

    // Composite visible layers from bottom to top
    for (const auto& layer : doc->layers) {
        if (!layer->visible) continue;

        u32 layerPixel = 0;

        if (layer->isPixelLayer()) {
            const PixelLayer* pixelLayer = static_cast<const PixelLayer*>(layer.get());

            // Apply layer transform
            f32 layerX = docX - pixelLayer->transform.position.x;
            f32 layerY = docY - pixelLayer->transform.position.y;

            // Check bounds
            if (layerX >= 0 && layerX < pixelLayer->canvas.width &&
                layerY >= 0 && layerY < pixelLayer->canvas.height) {
                i32 ix = static_cast<i32>(layerX);
                i32 iy = static_cast<i32>(layerY);
                layerPixel = pixelLayer->canvas.getPixel(ix, iy);
            }
        }
        else if (layer->isTextLayer()) {
            TextLayer* textLayer = const_cast<TextLayer*>(static_cast<const TextLayer*>(layer.get()));
            textLayer->ensureCacheValid();

            // Apply layer transform
            f32 layerX = docX - layer->transform.position.x;
            f32 layerY = docY - layer->transform.position.y;

            // Check bounds against rasterized cache
            if (layerX >= 0 && layerX < textLayer->rasterizedCache.width &&
                layerY >= 0 && layerY < textLayer->rasterizedCache.height) {
                i32 ix = static_cast<i32>(layerX);
                i32 iy = static_cast<i32>(layerY);
                layerPixel = textLayer->rasterizedCache.getPixel(ix, iy);
            }
        }
        else if (layer->isAdjustmentLayer()) {
            // Apply adjustment to composited result so far
            const AdjustmentLayer* adj = static_cast<const AdjustmentLayer*>(layer.get());
            if ((result & 0xFF) > 0) {
                result = Compositor::applyAdjustment(result, *adj);
            }
            continue;  // Don't blend - adjustment modifies existing pixels
        }

        if ((layerPixel & 0xFF) > 0) {
            result = Blend::blend(result, layerPixel, layer->blend, layer->opacity);
        }
    }

    return result;
}

void NavigatorThumbnail::drawViewportRect(Framebuffer& fb) {
    if (!view || !view->document || thumbScale <= 0) return;

    // Get viewport bounds from the document view
    // The view's viewport rectangle contains the actual size
    f32 viewportW = view->viewport.w;
    f32 viewportH = view->viewport.h;

    // Fall back to reasonable defaults if viewport not set
    if (viewportW <= 0) viewportW = 800.0f;
    if (viewportH <= 0) viewportH = 600.0f;

    // Calculate visible document area
    // pan is the screen offset where document (0,0) appears
    // So visible doc area starts at -pan/zoom
    f32 visibleDocX = -view->pan.x / view->zoom;
    f32 visibleDocY = -view->pan.y / view->zoom;
    f32 visibleDocW = viewportW / view->zoom;
    f32 visibleDocH = viewportH / view->zoom;

    // Scale to thumbnail coordinates
    i32 vpX = thumbX + static_cast<i32>(visibleDocX * thumbScale);
    i32 vpY = thumbY + static_cast<i32>(visibleDocY * thumbScale);
    i32 vpW = static_cast<i32>(visibleDocW * thumbScale);
    i32 vpH = static_cast<i32>(visibleDocH * thumbScale);

    // Clip rectangle to thumbnail bounds (symmetric for all edges)
    i32 x1 = vpX;
    i32 y1 = vpY;
    i32 x2 = vpX + vpW;
    i32 y2 = vpY + vpH;

    // Clip to thumbnail area
    i32 clipX1 = std::max(x1, thumbX);
    i32 clipY1 = std::max(y1, thumbY);
    i32 clipX2 = std::min(x2, thumbX + thumbW);
    i32 clipY2 = std::min(y2, thumbY + thumbH);

    i32 clippedW = clipX2 - clipX1;
    i32 clippedH = clipY2 - clipY1;

    if (clippedW > 0 && clippedH > 0) {
        // Draw red viewport rectangle (clipped portion)
        Recti vpRect(clipX1, clipY1, clippedW, clippedH);
        i32 thickness = static_cast<i32>(Config::uiScale);
        fb.drawRect(vpRect, 0xFF0000FF, thickness);
    }
}

void NavigatorThumbnail::panToThumbnailPos(Vec2 localPos) {
    if (!view || !view->document || thumbScale <= 0) return;

    Rect gb = globalBounds();

    // Convert local position to thumbnail-relative position
    f32 relX = localPos.x - (thumbX - gb.x);
    f32 relY = localPos.y - (thumbY - gb.y);

    // Convert to document coordinates
    f32 docX = relX / thumbScale;
    f32 docY = relY / thumbScale;

    // Get actual viewport size
    f32 viewportW = view->viewport.w;
    f32 viewportH = view->viewport.h;
    if (viewportW <= 0) viewportW = 800.0f;
    if (viewportH <= 0) viewportH = 600.0f;

    // Center the view on this document position
    // pan = -docPos * zoom + viewportCenter
    view->pan.x = -docX * view->zoom + viewportW / 2;
    view->pan.y = -docY * view->zoom + viewportH / 2;

    getAppState().needsRedraw = true;
}

// LayerThumbnail implementations
void LayerThumbnail::renderSelf(Framebuffer& fb) {
    Document* doc = getAppState().activeDocument;
    if (!doc || layerIndex < 0 || layerIndex >= static_cast<i32>(doc->layers.size())) {
        // No layer - just fill with background
        Rect gb = globalBounds();
        Recti r(static_cast<i32>(gb.x), static_cast<i32>(gb.y),
                static_cast<i32>(gb.w), static_cast<i32>(gb.h));
        fb.fillRect(r, Config::COLOR_BACKGROUND);
        return;
    }

    LayerBase* layer = doc->getLayer(layerIndex);
    if (!layer) return;

    Rect gb = globalBounds();
    i32 thumbX = static_cast<i32>(gb.x);
    i32 thumbY = static_cast<i32>(gb.y);
    i32 thumbW = static_cast<i32>(gb.w);
    i32 thumbH = static_cast<i32>(gb.h);

    // Render based on layer type
    if (layer->isPixelLayer()) {
        // Draw checkerboard background for pixel layers (transparency indicator)
        i32 checkSize = std::max(1, static_cast<i32>(4 * Config::uiScale));
        for (i32 ty = 0; ty < thumbH; ++ty) {
            for (i32 tx = 0; tx < thumbW; ++tx) {
                bool light = ((tx / checkSize) + (ty / checkSize)) % 2 == 0;
                u32 checkColor = light ? Config::CHECKER_COLOR1 : Config::CHECKER_COLOR2;
                fb.setPixel(thumbX + tx, thumbY + ty, checkColor);
            }
        }
        renderPixelLayer(fb, static_cast<PixelLayer*>(layer), thumbX, thumbY, thumbW, thumbH);
    } else if (layer->isTextLayer()) {
        // Solid background for text layers
        Recti bgRect(thumbX, thumbY, thumbW, thumbH);
        fb.fillRect(bgRect, Config::COLOR_BACKGROUND);
        renderTextLayer(fb, static_cast<TextLayer*>(layer), thumbX, thumbY, thumbW, thumbH);
    } else if (layer->isAdjustmentLayer()) {
        // Solid background for adjustment layers
        Recti bgRect(thumbX, thumbY, thumbW, thumbH);
        fb.fillRect(bgRect, Config::COLOR_BACKGROUND);
        renderAdjustmentLayer(fb, static_cast<AdjustmentLayer*>(layer), thumbX, thumbY, thumbW, thumbH);
    }

    // Draw border
    Recti thumbRect(thumbX, thumbY, thumbW, thumbH);
    fb.drawRect(thumbRect, Config::COLOR_BORDER, 1);
}

u32 LayerThumbnail::sampleBilinear(const TiledCanvas& canvas, f32 x, f32 y) {
    i32 x0 = static_cast<i32>(x);
    i32 y0 = static_cast<i32>(y);
    i32 x1 = x0 + 1;
    i32 y1 = y0 + 1;

    f32 fx = x - x0;
    f32 fy = y - y0;

    // Clamp to canvas bounds
    x0 = std::max(0, std::min(x0, static_cast<i32>(canvas.width) - 1));
    y0 = std::max(0, std::min(y0, static_cast<i32>(canvas.height) - 1));
    x1 = std::max(0, std::min(x1, static_cast<i32>(canvas.width) - 1));
    y1 = std::max(0, std::min(y1, static_cast<i32>(canvas.height) - 1));

    // Get four corner pixels
    u32 p00 = canvas.getPixel(x0, y0);
    u32 p10 = canvas.getPixel(x1, y0);
    u32 p01 = canvas.getPixel(x0, y1);
    u32 p11 = canvas.getPixel(x1, y1);

    // Bilinear interpolate each channel
    auto lerp = [](u8 a, u8 b, f32 t) -> u8 {
        return static_cast<u8>(a + (b - a) * t);
    };

    auto lerpColor = [&](u32 c00, u32 c10, u32 c01, u32 c11) -> u32 {
        u8 r = lerp(lerp((c00 >> 24) & 0xFF, (c10 >> 24) & 0xFF, fx),
                    lerp((c01 >> 24) & 0xFF, (c11 >> 24) & 0xFF, fx), fy);
        u8 g = lerp(lerp((c00 >> 16) & 0xFF, (c10 >> 16) & 0xFF, fx),
                    lerp((c01 >> 16) & 0xFF, (c11 >> 16) & 0xFF, fx), fy);
        u8 b = lerp(lerp((c00 >> 8) & 0xFF, (c10 >> 8) & 0xFF, fx),
                    lerp((c01 >> 8) & 0xFF, (c11 >> 8) & 0xFF, fx), fy);
        u8 a = lerp(lerp(c00 & 0xFF, c10 & 0xFF, fx),
                    lerp(c01 & 0xFF, c11 & 0xFF, fx), fy);
        return (static_cast<u32>(r) << 24) | (static_cast<u32>(g) << 16) |
               (static_cast<u32>(b) << 8) | static_cast<u32>(a);
    };

    return lerpColor(p00, p10, p01, p11);
}

void LayerThumbnail::renderPixelLayer(Framebuffer& fb, PixelLayer* layer, i32 thumbX, i32 thumbY, i32 thumbW, i32 thumbH) {
    if (layer->canvas.width == 0 || layer->canvas.height == 0) return;

    // Calculate scale to fit layer in thumbnail (maintain aspect ratio)
    f32 scaleX = static_cast<f32>(thumbW) / layer->canvas.width;
    f32 scaleY = static_cast<f32>(thumbH) / layer->canvas.height;
    f32 scale = std::min(scaleX, scaleY);

    // Actual rendered size (centered)
    i32 renderW = static_cast<i32>(layer->canvas.width * scale);
    i32 renderH = static_cast<i32>(layer->canvas.height * scale);
    i32 offsetX = (thumbW - renderW) / 2;
    i32 offsetY = (thumbH - renderH) / 2;

    // Sample layer pixels with bilinear filtering
    for (i32 ty = 0; ty < renderH; ++ty) {
        for (i32 tx = 0; tx < renderW; ++tx) {
            f32 srcX = static_cast<f32>(tx) / scale;
            f32 srcY = static_cast<f32>(ty) / scale;

            u32 pixel = sampleBilinear(layer->canvas, srcX, srcY);
            if ((pixel & 0xFF) > 0) {  // Has alpha
                u32 bgPixel = fb.getPixel(thumbX + offsetX + tx, thumbY + offsetY + ty);
                u32 blended = Blend::alphaBlend(bgPixel, pixel);
                fb.setPixel(thumbX + offsetX + tx, thumbY + offsetY + ty, blended);
            }
        }
    }
}

void LayerThumbnail::renderTextLayer(Framebuffer& fb, TextLayer* layer, i32 thumbX, i32 thumbY, i32 thumbW, i32 thumbH) {
    // Draw a "T" icon to indicate text layer
    u32 iconColor = layer->textColor.toRGBA();
    i32 cx = thumbX + thumbW / 2;
    i32 cy = thumbY + thumbH / 2;
    i32 size = std::min(thumbW, thumbH) * 2 / 3;

    // Draw T shape (thicker)
    i32 barH = std::max(2, size / 6);
    i32 stemW = std::max(2, size / 5);
    fb.fillRect(cx - size/2, cy - size/2, size, barH, iconColor);  // Top bar
    fb.fillRect(cx - stemW/2, cy - size/2, stemW, size, iconColor);  // Vertical stem
}

void LayerThumbnail::renderAdjustmentLayer(Framebuffer& fb, AdjustmentLayer* layer, i32 thumbX, i32 thumbY, i32 thumbW, i32 thumbH) {
    // Draw adjustment sliders icon (three horizontal bars with dots)
    u32 iconColor = Config::COLOR_ACCENT;
    i32 cx = thumbX + thumbW / 2;
    i32 cy = thumbY + thumbH / 2;
    i32 size = std::min(thumbW, thumbH) * 2 / 3;
    i32 barW = size;
    i32 barH = std::max(2, size / 8);
    i32 spacing = size / 3;

    // Draw three horizontal bars
    for (i32 i = -1; i <= 1; ++i) {
        i32 barY = cy + i * spacing - barH / 2;
        fb.fillRect(cx - barW/2, barY, barW, barH, iconColor);

        // Draw a "knob" dot at different positions on each bar
        i32 knobX = cx + (i * barW / 4);  // Stagger knob positions
        i32 knobR = barH;
        fb.fillRect(knobX - knobR, barY - knobR/2, knobR * 2, barH + knobR, 0xFFFFFFFF);  // White knob
    }
}

// LayerListItem implementations
void LayerListItem::startEditing() {
    if (editing) return;

    if (!document || layerIndex < 0 || layerIndex >= static_cast<i32>(document->layers.size())) return;
    LayerBase* layer = document->getLayer(layerIndex);
    if (!layer) return;

    editing = true;
    viewLayout->visible = false;
    editLayout->visible = true;

    // Initialize text field with current name
    nameField->text = layer->name;
    nameField->cursorPos = static_cast<i32>(layer->name.length());
    nameField->selectionStart = -1;  // No selection, cursor at end

    // Focus the text field properly
    Widget* oldFocus = getAppState().focusedWidget;
    if (oldFocus) oldFocus->onBlur();
    getAppState().focusedWidget = nameField;
    nameField->onFocus();

    // Notify parent to gray out other items
    if (onEditStart) onEditStart(this);

    layout();
    getAppState().needsRedraw = true;
}

void LayerListItem::endEditing() {
    if (!editing) return;

    editing = false;
    viewLayout->visible = true;
    editLayout->visible = false;

    // Clear focus
    if (getAppState().focusedWidget == nameField) {
        getAppState().focusedWidget = nullptr;
    }

    // Notify parent
    if (onEditEnd) onEditEnd();

    updateFromLayer();
    layout();
    getAppState().needsRedraw = true;
}

bool LayerListItem::onMouseDown(const MouseEvent& e) {
    // Block all interactions when disabled (another layer is being edited)
    if (disabled) return true;

    if (e.button == MouseButton::Left) {
        // Check for double-click
        u64 now = Platform::getMilliseconds();
        if (now - lastClickTime < Config::DOUBLE_CLICK_MS && selected) {
            // Double-click - start editing
            startEditing();
            lastClickTime = 0;  // Reset to prevent triple-click issues
            return true;
        }
        lastClickTime = now;

        // Start potential drag
        dragPending = true;
        dragStartPos = localToGlobal(e.position);
        getAppState().capturedWidget = this;

        if (onSelect) onSelect(layerIndex);
        return true;
    }
    return Panel::onMouseDown(e);
}

bool LayerListItem::onMouseDrag(const MouseEvent& e) {
    if (!dragPending && !onDragMove) return false;

    Vec2 globalPos = localToGlobal(e.position);

    if (dragPending) {
        // Check if we've moved past threshold
        f32 dx = globalPos.x - dragStartPos.x;
        f32 dy = globalPos.y - dragStartPos.y;
        f32 dist = std::sqrt(dx * dx + dy * dy);

        if (dist >= 5.0f) {  // DRAG_THRESHOLD
            dragPending = false;
            if (onDragStart) onDragStart(layerIndex, globalPos);
        }
    } else {
        // Already dragging - update position
        if (onDragMove) onDragMove(globalPos);
    }

    return true;
}

bool LayerListItem::onMouseUp(const MouseEvent& e) {
    if (e.button == MouseButton::Left) {
        getAppState().capturedWidget = nullptr;

        if (dragPending) {
            // Was just a click, not a drag
            dragPending = false;
        } else if (onDragEnd) {
            // Commit the drag
            onDragEnd();
        }
        return true;
    }
    return Panel::onMouseUp(e);
}

bool LayerListItem::onKeyDown(const KeyEvent& e) {
    if (editing) {
        // Enter key confirms
        if (e.keyCode == 13 || e.keyCode == 271) {  // Return or Keypad Enter
            confirmEdit();
            return true;
        }
        // Escape key cancels
        else if (e.keyCode == 27) {  // Escape
            cancelEdit();
            return true;
        }
    }
    return Panel::onKeyDown(e);
}

// LayerPropsPanel implementations
void LayerPropsPanel::buildCommonControls(VBoxLayout* layout) {
    // Opacity
    auto opacityRow = layout->createChild<HBoxLayout>(4 * Config::uiScale);
    opacityRow->preferredSize = Vec2(0, 24 * Config::uiScale);
    opacityRow->verticalPolicy = SizePolicy::Fixed;

    opacityRow->createChild<Label>("Opacity")->preferredSize = Vec2(60 * Config::uiScale, 24 * Config::uiScale);
    opacitySlider = opacityRow->createChild<Slider>(0.0f, 1.0f, 1.0f);
    opacitySlider->horizontalPolicy = SizePolicy::Expanding;
    opacitySlider->onChanged = [](f32 value) {
        // Use AppState's activeDocument to avoid stale pointer
        Document* doc = getAppState().activeDocument;
        if (!doc) return;
        LayerBase* layer = doc->getActiveLayer();
        if (layer) {
            layer->opacity = value;
            doc->notifyLayerChanged(doc->activeLayerIndex);
            getAppState().needsRedraw = true;
        }
    };

    // Blend mode - all 12 modes
    auto blendRow = layout->createChild<HBoxLayout>(4 * Config::uiScale);
    blendRow->preferredSize = Vec2(0, 24 * Config::uiScale);
    blendRow->verticalPolicy = SizePolicy::Fixed;

    blendRow->createChild<Label>("Blend")->preferredSize = Vec2(60 * Config::uiScale, 24 * Config::uiScale);
    blendModeCombo = blendRow->createChild<ComboBox>();
    blendModeCombo->horizontalPolicy = SizePolicy::Expanding;
    blendModeCombo->addItem("Normal");
    blendModeCombo->addItem("Multiply");
    blendModeCombo->addItem("Screen");
    blendModeCombo->addItem("Overlay");
    blendModeCombo->addItem("Darken");
    blendModeCombo->addItem("Lighten");
    blendModeCombo->addItem("ColorDodge");
    blendModeCombo->addItem("ColorBurn");
    blendModeCombo->addItem("HardLight");
    blendModeCombo->addItem("SoftLight");
    blendModeCombo->addItem("Difference");
    blendModeCombo->addItem("Exclusion");
    blendModeCombo->onSelectionChanged = [](i32 index) {
        // Use AppState's activeDocument to avoid stale pointer
        Document* doc = getAppState().activeDocument;
        if (!doc) return;
        LayerBase* layer = doc->getActiveLayer();
        if (layer) {
            layer->blend = static_cast<BlendMode>(index);
            doc->notifyLayerChanged(doc->activeLayerIndex);
            getAppState().needsRedraw = true;
        }
    };
}

void LayerPropsPanel::rebuildForActiveLayer() {
    // Update common control values (but not locked state yet)
    if (document) {
        LayerBase* layer = document->getActiveLayer();
        if (layer) {
            if (opacitySlider) opacitySlider->setValue(layer->opacity);
            if (blendModeCombo) blendModeCombo->selectedIndex = static_cast<i32>(layer->blend);
        }
    }

    // Clear type-specific controls
    if (typeSpecificContainer) {
        typeSpecificContainer->clearChildren();
    }

    if (!document) return;
    LayerBase* layer = document->getActiveLayer();
    if (!layer) return;

    // Build controls based on layer type
    if (layer->isPixelLayer()) {
        buildPixelLayerControls(static_cast<PixelLayer*>(layer));
    } else if (layer->isTextLayer()) {
        buildTextLayerControls(static_cast<TextLayer*>(layer));
    } else if (layer->isAdjustmentLayer()) {
        buildAdjustmentControls(static_cast<AdjustmentLayer*>(layer));
    }

    // Update typeSpecificContainer's size based on its children
    updateTypeSpecificContainerSize();

    // Re-layout scrollView to recalculate content height
    if (scrollView) {
        scrollView->layout();
    }

    // Update locked state AFTER controls are built
    updateLockedState();

    getAppState().needsRedraw = true;
}

void LayerPropsPanel::buildPixelLayerControls(PixelLayer* layer) {
    auto label = typeSpecificContainer->createChild<Label>("Pixel Layer");
    label->preferredSize = Vec2(0, 20 * Config::uiScale);

    // Show canvas info
    auto infoRow = typeSpecificContainer->createChild<HBoxLayout>(4 * Config::uiScale);
    infoRow->preferredSize = Vec2(0, 20 * Config::uiScale);
    infoRow->verticalPolicy = SizePolicy::Fixed;

    std::string sizeStr = "Size: " + std::to_string(layer->canvas.width) + " x " +
                          std::to_string(layer->canvas.height);
    infoRow->createChild<Label>(sizeStr);
}

void LayerPropsPanel::buildTextLayerControls(TextLayer* layer) {
    auto label = typeSpecificContainer->createChild<Label>("Text Layer");
    label->preferredSize = Vec2(0, 20 * Config::uiScale);

    // Text content
    auto textRow = typeSpecificContainer->createChild<HBoxLayout>(4 * Config::uiScale);
    textRow->preferredSize = Vec2(0, 24 * Config::uiScale);
    textRow->verticalPolicy = SizePolicy::Fixed;

    textRow->createChild<Label>("Text")->preferredSize = Vec2(50 * Config::uiScale, 24 * Config::uiScale);
    auto textField = textRow->createChild<TextField>();
    textField->text = layer->text;
    textField->horizontalPolicy = SizePolicy::Expanding;
    textField->onChanged = [this, layer](const std::string& text) {
        if (layer->locked) return;
        layer->text = text;
        layer->invalidateCache();
        if (document) {
            document->notifyLayerChanged(document->activeLayerIndex);
            getAppState().needsRedraw = true;
        }
    };

    // Font size
    auto sizeRow = typeSpecificContainer->createChild<HBoxLayout>(4 * Config::uiScale);
    sizeRow->preferredSize = Vec2(0, 24 * Config::uiScale);
    sizeRow->verticalPolicy = SizePolicy::Fixed;

    sizeRow->createChild<Label>("Size")->preferredSize = Vec2(50 * Config::uiScale, 24 * Config::uiScale);
    auto sizeSlider = sizeRow->createChild<Slider>(1.0f, 200.0f, static_cast<f32>(layer->fontSize));
    sizeSlider->horizontalPolicy = SizePolicy::Expanding;
    sizeSlider->onChanged = [this, layer](f32 value) {
        if (layer->locked) return;
        layer->fontSize = static_cast<u32>(value);
        layer->invalidateCache();
        if (document) {
            document->notifyLayerChanged(document->activeLayerIndex);
            getAppState().needsRedraw = true;
        }
    };

    // Font selection
    auto fontRow = typeSpecificContainer->createChild<HBoxLayout>(4 * Config::uiScale);
    fontRow->preferredSize = Vec2(0, 24 * Config::uiScale);
    fontRow->verticalPolicy = SizePolicy::Fixed;

    fontRow->createChild<Label>("Font")->preferredSize = Vec2(50 * Config::uiScale, 24 * Config::uiScale);
    fontCombo = fontRow->createChild<ComboBox>();
    fontCombo->horizontalPolicy = SizePolicy::Expanding;

    // Populate font list
    fontCombo->items.clear();
    fontCombo->addItem("Internal Font");
    fontCombo->addItem("Load Font...");

    // Add custom fonts from document
    if (document) {
        for (const auto& fontName : document->getFontNames()) {
            fontCombo->addItem(fontName);
        }
    }

    // Set current selection based on layer's fontFamily
    if (layer->fontFamily.empty() || layer->fontFamily == "Internal Font") {
        fontCombo->selectedIndex = 0;
    } else {
        // Find the font in the list
        fontCombo->selectedIndex = 0;  // Default to internal
        for (size_t i = 2; i < fontCombo->items.size(); ++i) {
            if (fontCombo->items[i] == layer->fontFamily) {
                fontCombo->selectedIndex = static_cast<i32>(i);
                break;
            }
        }
    }

    fontCombo->onSelectionChanged = [this, layer](i32 index) {
        if (layer->locked) return;
        if (index == 0) {
            // Internal Font
            layer->fontFamily = "";
            layer->invalidateCache();
            if (document) {
                document->notifyLayerChanged(document->activeLayerIndex);
                getAppState().needsRedraw = true;
            }
        } else if (index == 1) {
            // Load Font...
            // Hide dropdown before opening blocking file dialog
            if (fontCombo) {
                fontCombo->hideDropdown();
            }
            if (onRequestLoadFont) {
                onRequestLoadFont([this, layer](const std::string& fontName, std::vector<u8>& fontData) {
                    if (fontName.empty() || fontData.empty()) return;

                    // Add font to document
                    if (document) {
                        document->addFont(fontName, std::move(fontData));

                        // Register with FontRenderer
                        const auto* data = document->getFontData(fontName);
                        if (data) {
                            FontRenderer::instance().loadCustomFont(fontName, data->data(), static_cast<i32>(data->size()));
                        }

                        // Update layer
                        layer->fontFamily = fontName;
                        layer->invalidateCache();

                        // Refresh the font list and rebuild controls
                        rebuildForActiveLayer();
                        document->notifyLayerChanged(document->activeLayerIndex);
                        getAppState().needsRedraw = true;
                    }
                });
            }
            // Reset selection to current font (don't stay on "Load Font...")
            if (layer->fontFamily.empty()) {
                fontCombo->selectedIndex = 0;
            }
        } else {
            // Custom font selected
            layer->fontFamily = fontCombo->items[index];
            layer->invalidateCache();
            if (document) {
                document->notifyLayerChanged(document->activeLayerIndex);
                getAppState().needsRedraw = true;
            }
        }
    };

    // Color
    auto colorRow = typeSpecificContainer->createChild<HBoxLayout>(4 * Config::uiScale);
    colorRow->preferredSize = Vec2(0, 28 * Config::uiScale);
    colorRow->verticalPolicy = SizePolicy::Fixed;

    colorRow->createChild<Label>("Color")->preferredSize = Vec2(50 * Config::uiScale, 24 * Config::uiScale);
    textColorSwatch = colorRow->createChild<ColorSwatch>(layer->textColor);
    textColorSwatch->preferredSize = Vec2(32 * Config::uiScale, 24 * Config::uiScale);
    textColorSwatch->onClick = [this, layer]() {
        if (layer->locked) return;
        if (onRequestColorPicker) {
            onRequestColorPicker(layer->textColor, [this, layer](const Color& c) {
                if (layer->locked) return;  // Check again in case locked while picker open
                layer->textColor = c;
                layer->invalidateCache();
                if (textColorSwatch) textColorSwatch->color = c;
                if (document) {
                    document->notifyLayerChanged(document->activeLayerIndex);
                    getAppState().needsRedraw = true;
                }
            });
        }
    };

    auto setFgBtn = colorRow->createChild<Button>("Set to FG");
    setFgBtn->preferredSize = Vec2(70 * Config::uiScale, 24 * Config::uiScale);
    setFgBtn->onClick = [this, layer]() {
        if (layer->locked) return;
        layer->textColor = getAppState().foregroundColor;
        layer->invalidateCache();
        if (textColorSwatch) textColorSwatch->color = layer->textColor;
        if (document) {
            document->notifyLayerChanged(document->activeLayerIndex);
            getAppState().needsRedraw = true;
        }
    };

    // Rasterize button
    auto rasterizeBtn = typeSpecificContainer->createChild<Button>("Rasterize");
    rasterizeBtn->preferredSize = Vec2(0, 28 * Config::uiScale);
    rasterizeBtn->horizontalPolicy = SizePolicy::Expanding;
    rasterizeBtn->onClick = [this]() {
        if (document && document->activeLayerIndex >= 0) {
            document->rasterizeLayer(document->activeLayerIndex);
            getAppState().needsRedraw = true;
        }
    };
}

void LayerPropsPanel::buildAdjustmentControls(AdjustmentLayer* layer) {
    // Label showing adjustment type
    const char* typeName = getAdjustmentTypeName(layer->type);
    auto label = typeSpecificContainer->createChild<Label>(typeName);
    label->preferredSize = Vec2(0, 20 * Config::uiScale);

    // Build controls based on adjustment type
    switch (layer->type) {
        case AdjustmentType::BrightnessContrast:
            buildBrightnessContrastControls(layer);
            break;
        case AdjustmentType::TemperatureTint:
            buildTemperatureTintControls(layer);
            break;
        case AdjustmentType::HueSaturation:
            buildHueSaturationControls(layer);
            break;
        case AdjustmentType::Vibrance:
            buildVibranceControls(layer);
            break;
        case AdjustmentType::ColorBalance:
            buildColorBalanceControls(layer);
            break;
        case AdjustmentType::HighlightsShadows:
            buildHighlightsShadowsControls(layer);
            break;
        case AdjustmentType::Exposure:
            buildExposureControls(layer);
            break;
        case AdjustmentType::Levels:
            buildLevelsControls(layer);
            break;
        case AdjustmentType::Invert:
            buildInvertControls(layer);
            break;
        case AdjustmentType::BlackAndWhite:
            buildBlackAndWhiteControls(layer);
            break;
    }
}

const char* LayerPropsPanel::getAdjustmentTypeName(AdjustmentType type) {
    switch (type) {
        case AdjustmentType::BrightnessContrast: return "Brightness/Contrast";
        case AdjustmentType::TemperatureTint: return "Temperature/Tint";
        case AdjustmentType::HueSaturation: return "Hue/Saturation";
        case AdjustmentType::Vibrance: return "Vibrance";
        case AdjustmentType::ColorBalance: return "Color Balance";
        case AdjustmentType::HighlightsShadows: return "Highlights/Shadows";
        case AdjustmentType::Exposure: return "Exposure";
        case AdjustmentType::Levels: return "Levels";
        case AdjustmentType::Invert: return "Invert";
        case AdjustmentType::BlackAndWhite: return "Black & White";
        default: return "Adjustment";
    }
}

Slider* LayerPropsPanel::addSliderRow(const char* labelText, f32 min, f32 max, f32 value,
                                       std::function<void(f32)> onChange) {
    auto row = typeSpecificContainer->createChild<HBoxLayout>(4 * Config::uiScale);
    row->preferredSize = Vec2(0, 24 * Config::uiScale);
    row->verticalPolicy = SizePolicy::Fixed;

    row->createChild<Label>(labelText)->preferredSize = Vec2(80 * Config::uiScale, 24 * Config::uiScale);
    auto slider = row->createChild<Slider>(min, max, value);
    slider->horizontalPolicy = SizePolicy::Expanding;
    slider->onChanged = onChange;
    return slider;
}

void LayerPropsPanel::updateTypeSpecificContainerSize() {
    if (!typeSpecificContainer) return;

    f32 totalHeight = 0;
    f32 spacing = 4 * Config::uiScale;  // VBoxLayout spacing

    for (size_t i = 0; i < typeSpecificContainer->children.size(); ++i) {
        auto& child = typeSpecificContainer->children[i];
        f32 childHeight = child->preferredSize.y > 0 ? child->preferredSize.y : child->minSize.y;
        totalHeight += childHeight;
        if (i > 0) totalHeight += spacing;
    }

    // Add padding
    totalHeight += typeSpecificContainer->paddingTop + typeSpecificContainer->paddingBottom;

    typeSpecificContainer->preferredSize.y = totalHeight;
    typeSpecificContainer->minSize.y = totalHeight;
}

void LayerPropsPanel::buildBrightnessContrastControls(AdjustmentLayer* layer) {
    auto* params = getAdjustmentParams<BrightnessContrastParams>(layer);
    if (!params) return;

    addSliderRow("Brightness", -100.0f, 100.0f, params->brightness,
        [this, params](f32 v) { params->brightness = v; notifyAdjustmentChanged(); });

    addSliderRow("Contrast", -100.0f, 100.0f, params->contrast,
        [this, params](f32 v) { params->contrast = v; notifyAdjustmentChanged(); });
}

void LayerPropsPanel::buildTemperatureTintControls(AdjustmentLayer* layer) {
    auto* params = getAdjustmentParams<TemperatureTintParams>(layer);
    if (!params) return;

    addSliderRow("Temperature", -100.0f, 100.0f, params->temperature,
        [this, params](f32 v) { params->temperature = v; notifyAdjustmentChanged(); });

    addSliderRow("Tint", -100.0f, 100.0f, params->tint,
        [this, params](f32 v) { params->tint = v; notifyAdjustmentChanged(); });
}

void LayerPropsPanel::buildHueSaturationControls(AdjustmentLayer* layer) {
    auto* params = getAdjustmentParams<HueSaturationParams>(layer);
    if (!params) return;

    addSliderRow("Hue", -180.0f, 180.0f, params->hue,
        [this, params](f32 v) { params->hue = v; notifyAdjustmentChanged(); });

    addSliderRow("Saturation", -100.0f, 100.0f, params->saturation,
        [this, params](f32 v) { params->saturation = v; notifyAdjustmentChanged(); });

    addSliderRow("Lightness", -100.0f, 100.0f, params->lightness,
        [this, params](f32 v) { params->lightness = v; notifyAdjustmentChanged(); });
}

void LayerPropsPanel::buildVibranceControls(AdjustmentLayer* layer) {
    auto* params = getAdjustmentParams<VibranceParams>(layer);
    if (!params) return;

    addSliderRow("Vibrance", -100.0f, 100.0f, params->vibrance,
        [this, params](f32 v) { params->vibrance = v; notifyAdjustmentChanged(); });
}

void LayerPropsPanel::buildColorBalanceControls(AdjustmentLayer* layer) {
    auto* params = getAdjustmentParams<ColorBalanceParams>(layer);
    if (!params) return;

    // Shadows section
    typeSpecificContainer->createChild<Label>("Shadows")->preferredSize = Vec2(0, 18 * Config::uiScale);
    addSliderRow("Cyan-Red", -100.0f, 100.0f, params->shadowsCyanRed,
        [this, params](f32 v) { params->shadowsCyanRed = v; notifyAdjustmentChanged(); });
    addSliderRow("Mag-Green", -100.0f, 100.0f, params->shadowsMagentaGreen,
        [this, params](f32 v) { params->shadowsMagentaGreen = v; notifyAdjustmentChanged(); });
    addSliderRow("Yel-Blue", -100.0f, 100.0f, params->shadowsYellowBlue,
        [this, params](f32 v) { params->shadowsYellowBlue = v; notifyAdjustmentChanged(); });

    // Midtones section
    typeSpecificContainer->createChild<Label>("Midtones")->preferredSize = Vec2(0, 18 * Config::uiScale);
    addSliderRow("Cyan-Red", -100.0f, 100.0f, params->midtonesCyanRed,
        [this, params](f32 v) { params->midtonesCyanRed = v; notifyAdjustmentChanged(); });
    addSliderRow("Mag-Green", -100.0f, 100.0f, params->midtonesMagentaGreen,
        [this, params](f32 v) { params->midtonesMagentaGreen = v; notifyAdjustmentChanged(); });
    addSliderRow("Yel-Blue", -100.0f, 100.0f, params->midtonesYellowBlue,
        [this, params](f32 v) { params->midtonesYellowBlue = v; notifyAdjustmentChanged(); });

    // Highlights section
    typeSpecificContainer->createChild<Label>("Highlights")->preferredSize = Vec2(0, 18 * Config::uiScale);
    addSliderRow("Cyan-Red", -100.0f, 100.0f, params->highlightsCyanRed,
        [this, params](f32 v) { params->highlightsCyanRed = v; notifyAdjustmentChanged(); });
    addSliderRow("Mag-Green", -100.0f, 100.0f, params->highlightsMagentaGreen,
        [this, params](f32 v) { params->highlightsMagentaGreen = v; notifyAdjustmentChanged(); });
    addSliderRow("Yel-Blue", -100.0f, 100.0f, params->highlightsYellowBlue,
        [this, params](f32 v) { params->highlightsYellowBlue = v; notifyAdjustmentChanged(); });
}

void LayerPropsPanel::buildHighlightsShadowsControls(AdjustmentLayer* layer) {
    auto* params = getAdjustmentParams<HighlightsShadowsParams>(layer);
    if (!params) return;

    addSliderRow("Highlights", -100.0f, 100.0f, params->highlights,
        [this, params](f32 v) { params->highlights = v; notifyAdjustmentChanged(); });

    addSliderRow("Shadows", -100.0f, 100.0f, params->shadows,
        [this, params](f32 v) { params->shadows = v; notifyAdjustmentChanged(); });
}

void LayerPropsPanel::buildExposureControls(AdjustmentLayer* layer) {
    auto* params = getAdjustmentParams<ExposureParams>(layer);
    if (!params) return;

    addSliderRow("Exposure", -5.0f, 5.0f, params->exposure,
        [this, params](f32 v) { params->exposure = v; notifyAdjustmentChanged(); });

    addSliderRow("Offset", -0.5f, 0.5f, params->offset,
        [this, params](f32 v) { params->offset = v; notifyAdjustmentChanged(); });

    addSliderRow("Gamma", 0.01f, 3.0f, params->gamma,
        [this, params](f32 v) { params->gamma = v; notifyAdjustmentChanged(); });
}

void LayerPropsPanel::buildLevelsControls(AdjustmentLayer* layer) {
    auto* params = getAdjustmentParams<LevelsParams>(layer);
    if (!params) return;

    typeSpecificContainer->createChild<Label>("Input")->preferredSize = Vec2(0, 18 * Config::uiScale);

    addSliderRow("Black", 0.0f, 255.0f, params->inputBlack,
        [this, params](f32 v) { params->inputBlack = v; notifyAdjustmentChanged(); });

    addSliderRow("Gamma", 0.1f, 3.0f, params->inputGamma,
        [this, params](f32 v) { params->inputGamma = v; notifyAdjustmentChanged(); });

    addSliderRow("White", 0.0f, 255.0f, params->inputWhite,
        [this, params](f32 v) { params->inputWhite = v; notifyAdjustmentChanged(); });

    typeSpecificContainer->createChild<Label>("Output")->preferredSize = Vec2(0, 18 * Config::uiScale);

    addSliderRow("Black", 0.0f, 255.0f, params->outputBlack,
        [this, params](f32 v) { params->outputBlack = v; notifyAdjustmentChanged(); });

    addSliderRow("White", 0.0f, 255.0f, params->outputWhite,
        [this, params](f32 v) { params->outputWhite = v; notifyAdjustmentChanged(); });
}

void LayerPropsPanel::buildInvertControls(AdjustmentLayer* layer) {
    // Invert has no adjustable parameters
    auto info = typeSpecificContainer->createChild<Label>("No adjustable parameters");
    info->preferredSize = Vec2(0, 20 * Config::uiScale);
}

void LayerPropsPanel::buildBlackAndWhiteControls(AdjustmentLayer* layer) {
    auto* params = getAdjustmentParams<BlackAndWhiteParams>(layer);
    if (!params) return;

    addSliderRow("Reds", -200.0f, 300.0f, params->reds,
        [this, params](f32 v) { params->reds = v; notifyAdjustmentChanged(); });

    addSliderRow("Yellows", -200.0f, 300.0f, params->yellows,
        [this, params](f32 v) { params->yellows = v; notifyAdjustmentChanged(); });

    addSliderRow("Greens", -200.0f, 300.0f, params->greens,
        [this, params](f32 v) { params->greens = v; notifyAdjustmentChanged(); });

    addSliderRow("Cyans", -200.0f, 300.0f, params->cyans,
        [this, params](f32 v) { params->cyans = v; notifyAdjustmentChanged(); });

    addSliderRow("Blues", -200.0f, 300.0f, params->blues,
        [this, params](f32 v) { params->blues = v; notifyAdjustmentChanged(); });

    addSliderRow("Magentas", -200.0f, 300.0f, params->magentas,
        [this, params](f32 v) { params->magentas = v; notifyAdjustmentChanged(); });

    typeSpecificContainer->createChild<Separator>(true);

    addSliderRow("Tint Hue", 0.0f, 360.0f, params->tintHue,
        [this, params](f32 v) { params->tintHue = v; notifyAdjustmentChanged(); });

    addSliderRow("Tint Amount", 0.0f, 100.0f, params->tintAmount,
        [this, params](f32 v) { params->tintAmount = v; notifyAdjustmentChanged(); });
}

// LayerPanel implementations
void LayerPanel::showAdjustmentMenu() {
    if (!adjustmentMenu || !adjustmentBtn) return;

    // Get button's global bounds
    Rect btnBounds = adjustmentBtn->globalBounds();

    // Menu width is 180 * UI_SCALE (from PopupMenu::show)
    f32 menuWidth = 180 * Config::uiScale;

    // Calculate menu height to position it above the button
    f32 itemHeight = 24 * Config::uiScale;
    f32 separatorHeight = 8 * Config::uiScale;
    f32 menuHeight = 4 * Config::uiScale;  // Padding
    for (const auto& item : adjustmentMenu->items) {
        menuHeight += item.separator ? separatorHeight : itemHeight;
    }

    // Right-align menu to button's right edge, position above button
    f32 globalMenuX = btnBounds.x + btnBounds.w - menuWidth;
    f32 globalMenuY = btnBounds.y - menuHeight;

    // Convert global coordinates to local (relative to this panel)
    Vec2 localPos = globalToLocal(Vec2(globalMenuX, globalMenuY));

    adjustmentMenu->show(localPos.x, localPos.y);
    OverlayManager::instance().registerOverlay(adjustmentMenu, ZOrder::POPUP_MENU,
        [this]() { closeAdjustmentMenu(); });
}

void LayerPanel::updateDisabledState() {
    if (!layerList) return;

    // Disable all items except the one being edited
    for (auto& child : layerList->children) {
        LayerListItem* item = dynamic_cast<LayerListItem*>(child.get());
        if (item) {
            // Disable if another item is being edited (not this one)
            bool shouldDisable = (editingItem != nullptr && item != editingItem);
            item->setDisabled(shouldDisable);
        }
    }

    getAppState().needsRedraw = true;
}

void LayerPanel::rebuildLayerList() {
    if (!layerList || !document) return;

    editingItem = nullptr;  // Clear edit state when rebuilding
    clearDragState();  // Clear any pending drag state
    layerList->clearChildren();

    // Add layers in reverse order (top layer first in UI)
    for (i32 i = document->layers.size() - 1; i >= 0; --i) {
        auto item = layerList->createChild<LayerListItem>(i, document);
        item->selected = (i == document->activeLayerIndex);
        item->onSelect = [this](i32 index) {
            if (editingItem) return;  // Block selection during edit
            Document* doc = getAppState().activeDocument;
            if (!doc || index < 0 || index >= static_cast<i32>(doc->layers.size())) return;
            doc->setActiveLayer(index);
            updateSelection();
            getAppState().needsRedraw = true;
        };
        item->onEditStart = [this](LayerListItem* editItem) {
            setEditMode(editItem);
        };
        item->onEditEnd = [this]() {
            clearEditMode();
        };

        // Drag and drop callbacks
        item->onDragStart = [this](i32 layerIndex, Vec2 globalPos) {
            if (editingItem) return;  // Block drag during edit
            startDrag(layerIndex, globalPos);
            updateDropTarget(globalPos);
        };
        item->onDragMove = [this](Vec2 globalPos) {
            updateDropTarget(globalPos);
        };
        item->onDragEnd = [this]() {
            commitDrag();
        };
        item->onDragCancel = [this]() {
            clearDragState();
        };
    }

    // Layout scrollView to recalculate content height for scrollbar
    if (scrollView) {
        scrollView->layout();
    }
}

void LayerPanel::startDrag(i32 layerIndex, Vec2 globalPos) {
    dragging = true;
    dragSourceIndex = layerIndex;
    dropTargetIndex = -1;
    getAppState().needsRedraw = true;
}

void LayerPanel::updateDropTarget(Vec2 globalPos) {
    if (!dragging || !document || !scrollView || !layerList) {
        dropTargetIndex = -1;
        return;
    }

    // Convert global position to scroll view's local coordinates
    Vec2 scrollViewLocal = scrollView->globalToLocal(globalPos);

    // Account for scroll offset to get position in content
    f32 contentY = scrollViewLocal.y + scrollView->scrollOffset;

    // Calculate which gap the mouse is closest to
    f32 itemHeight = Config::layerItemHeight();
    f32 spacing = 2 * Config::uiScale;  // VBoxLayout spacing
    f32 totalItemHeight = itemHeight + spacing;

    // Determine gap index (0 = before first item, N = after last item)
    i32 numLayers = static_cast<i32>(document->layers.size());
    i32 gapIndex = static_cast<i32>((contentY + totalItemHeight / 2) / totalItemHeight);
    gapIndex = std::max(0, std::min(numLayers, gapIndex));

    // UI shows layers in reverse order (top layer = index numLayers-1 is shown first)
    // gapIndex 0 means insert at the top of UI list = becomes top layer (doc index numLayers-1)
    // gapIndex N means insert at bottom of UI list = becomes bottom layer (doc index 0)
    // Formula: targetIndex = numLayers - 1 - gapIndex, clamped to valid range
    i32 targetIndex = numLayers - 1 - gapIndex;
    if (targetIndex < 0) targetIndex = 0;

    // Don't allow dropping at the same position (no-op)
    if (targetIndex == dragSourceIndex) {
        dropTargetIndex = -1;
        dropGapIndex = -1;
    } else {
        dropTargetIndex = targetIndex;
        dropGapIndex = gapIndex;  // Store for rendering
    }

    getAppState().needsRedraw = true;
}

void LayerPanel::commitDrag() {
    if (!dragging || !document || dropTargetIndex < 0) {
        clearDragState();
        return;
    }

    // moveLayer adjusts for the "remove then insert" semantics
    document->moveLayer(dragSourceIndex, dropTargetIndex);
    clearDragState();
}

void LayerPanel::clearDragState() {
    dragging = false;
    dragSourceIndex = -1;
    dropTargetIndex = -1;
    dropGapIndex = -1;
    dragPending = false;
    getAppState().needsRedraw = true;
}

void LayerPanel::render(Framebuffer& fb) {
    // Render panel and children normally
    Panel::render(fb);

    // Draw insert line AFTER children so it appears on top
    if (dragging && dropGapIndex >= 0 && document && scrollView) {
        i32 numLayers = static_cast<i32>(document->layers.size());

        // Use stored gap index for line position
        i32 uiGapIndex = dropGapIndex;

        // Calculate Y position of the insert line in content space
        f32 itemHeight = Config::layerItemHeight();
        f32 spacing = 2 * Config::uiScale;
        f32 totalItemHeight = itemHeight + spacing;

        // Position line at the gap between items
        // For gap 0 (top): at the top edge of first item
        // For gap N (bottom): at the bottom edge of last item
        f32 contentLineY;
        if (uiGapIndex == 0) {
            contentLineY = 0;  // Top of first item
        } else if (uiGapIndex >= numLayers) {
            contentLineY = numLayers * totalItemHeight - spacing;  // Bottom of last item
        } else {
            contentLineY = uiGapIndex * totalItemHeight - spacing / 2;  // Between items
        }

        // Convert to screen coordinates (subtract scroll offset)
        Rect scrollBounds = scrollView->globalBounds();
        f32 lineY = scrollBounds.y + contentLineY - scrollView->scrollOffset;

        // Clamp to scroll view bounds and check visibility
        f32 clampedLineY = std::max(scrollBounds.y, std::min(scrollBounds.y + scrollBounds.h - 1, lineY));
        if (lineY >= scrollBounds.y - 10 && lineY <= scrollBounds.y + scrollBounds.h + 10) {
            // Draw a colored line across the layer list
            i32 lineThickness = static_cast<i32>(3 * Config::uiScale);
            Recti lineRect(
                static_cast<i32>(scrollBounds.x + 4),
                static_cast<i32>(clampedLineY - lineThickness / 2),
                static_cast<i32>(scrollBounds.w - 8),
                lineThickness
            );
            fb.fillRect(lineRect, Config::COLOR_ACCENT);
        }
    }
}
