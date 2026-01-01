#include "main_window.h"

#ifdef __EMSCRIPTEN__
// Access global pressure from WASM touch events
extern f32 g_wasmPressure;
#endif

// ============================================================================
// DocumentViewWidget implementation
// ============================================================================

DocumentViewWidget::DocumentViewWidget() {
    horizontalPolicy = SizePolicy::Expanding;
    verticalPolicy = SizePolicy::Expanding;
}

DocumentViewWidget::~DocumentViewWidget() {
    if (view.document) {
        view.document->removeObserver(this);
    }
}

void DocumentViewWidget::setDocument(Document* doc) {
    if (view.document) {
        view.document->removeObserver(this);
    }
    view.setDocument(doc);
    if (doc) {
        doc->addObserver(this);
        needsCentering = true;
    }
}

void DocumentViewWidget::layout() {
    Rect oldViewport = view.viewport;
    Vec2 centerDocPoint(0, 0);
    bool hasValidViewport = oldViewport.w > 0 && oldViewport.h > 0 && view.document;

    if (hasValidViewport) {
        Vec2 oldCenter(oldViewport.x + oldViewport.w / 2, oldViewport.y + oldViewport.h / 2);
        centerDocPoint = view.screenToDocument(oldCenter);
    }

    Widget::layout();

    Rect newViewport = globalBounds();
    view.viewport = newViewport;

    if (hasValidViewport && newViewport.w > 0 && newViewport.h > 0) {
        Vec2 newCenter(newViewport.x + newViewport.w / 2, newViewport.y + newViewport.h / 2);
        view.pan.x = newCenter.x - centerDocPoint.x * view.zoom - newViewport.x;
        view.pan.y = newCenter.y - centerDocPoint.y * view.zoom - newViewport.y;
    }
}

void DocumentViewWidget::renderSelf(Framebuffer& fb) {
    Rect global = globalBounds();
    view.viewport = global;

    if (needsCentering && view.document && global.w > 0 && global.h > 0) {
        view.zoomToFit();
        needsCentering = false;
        getAppState().needsRedraw = true;
    }

    // Note: Background fill removed - application.cpp already clears framebuffer
    // Checkerboard will be drawn by compositor for document area

    if (view.document) {
        Compositor::compositeDocument(fb, *view.document, view.viewport,
                                      view.zoom, view.pan);

        Tool* tool = view.document->getTool();
        bool showOverlay = tool && tool->hasOverlay() &&
                          (mouseOverCanvas || tool->type == ToolType::Crop || tool->type == ToolType::Move);
        if (showOverlay) {
            Vec2 cursorScreen = view.documentToScreen(lastMousePos);
            Recti clipRect(static_cast<i32>(global.x), static_cast<i32>(global.y),
                          static_cast<i32>(global.w), static_cast<i32>(global.h));
            Vec2 fullPan(view.pan.x + global.x, view.pan.y + global.y);
            tool->renderOverlay(fb, cursorScreen, view.zoom, fullPan, clipRect);
        }

        if (tool) {
            bool shouldShowPreview = toolActive;
            if (tool->type == ToolType::PolygonSelect) {
                auto* polyTool = static_cast<PolygonSelectTool*>(tool);
                shouldShowPreview = polyTool->active;
            }
            if (shouldShowPreview) {
                renderSelectionPreview(fb, tool);
            }
        }
    }
}

void DocumentViewWidget::drawEllipseOutline(Framebuffer& fb, i32 cx, i32 cy, i32 rx, i32 ry, u32 color) {
    if (rx <= 0 || ry <= 0) return;

    i32 rx2 = rx * rx;
    i32 ry2 = ry * ry;
    i32 twoRx2 = 2 * rx2;
    i32 twoRy2 = 2 * ry2;

    i32 x = 0;
    i32 y = ry;
    i32 px = 0;
    i32 py = twoRx2 * y;

    i32 p = static_cast<i32>(ry2 - (rx2 * ry) + (0.25f * rx2));
    while (px < py) {
        fb.setPixel(cx + x, cy + y, color);
        fb.setPixel(cx - x, cy + y, color);
        fb.setPixel(cx + x, cy - y, color);
        fb.setPixel(cx - x, cy - y, color);
        x++;
        px += twoRy2;
        if (p < 0) {
            p += ry2 + px;
        } else {
            y--;
            py -= twoRx2;
            p += ry2 + px - py;
        }
    }

    p = static_cast<i32>(ry2 * (x + 0.5f) * (x + 0.5f) + rx2 * (y - 1) * (y - 1) - rx2 * ry2);
    while (y >= 0) {
        fb.setPixel(cx + x, cy + y, color);
        fb.setPixel(cx - x, cy + y, color);
        fb.setPixel(cx + x, cy - y, color);
        fb.setPixel(cx - x, cy - y, color);
        y--;
        py -= twoRx2;
        if (p > 0) {
            p += rx2 - py;
        } else {
            x++;
            px += twoRy2;
            p += rx2 - py + px;
        }
    }
}

void DocumentViewWidget::renderSelectionPreview(Framebuffer& fb, Tool* tool) {
    if (!tool) return;

    i32 thickness = std::max(1, static_cast<i32>(Config::uiScale));

    auto drawVisibleLine = [&](i32 x0, i32 y0, i32 x1, i32 y1) {
        fb.drawLine(x0, y0, x1, y1, 0x000000FF);
        i32 dx = x1 - x0;
        i32 dy = y1 - y0;
        if (std::abs(dx) >= std::abs(dy)) {
            fb.drawLine(x0, y0 + 1, x1, y1 + 1, 0xFFFFFFFF);
        } else {
            fb.drawLine(x0 + 1, y0, x1 + 1, y1, 0xFFFFFFFF);
        }
    };

    if (tool->type == ToolType::PolygonSelect) {
        auto* polyTool = static_cast<PolygonSelectTool*>(tool);
        if (!polyTool->active || polyTool->points.empty()) return;

        for (size_t i = 0; i + 1 < polyTool->points.size(); ++i) {
            Vec2 p1 = view.documentToScreen(polyTool->points[i]);
            Vec2 p2 = view.documentToScreen(polyTool->points[i + 1]);
            drawVisibleLine(static_cast<i32>(p1.x), static_cast<i32>(p1.y),
                           static_cast<i32>(p2.x), static_cast<i32>(p2.y));
        }

        Vec2 lastPt = view.documentToScreen(polyTool->points.back());
        Vec2 curPt = view.documentToScreen(lastMousePos);
        drawVisibleLine(static_cast<i32>(lastPt.x), static_cast<i32>(lastPt.y),
                       static_cast<i32>(curPt.x), static_cast<i32>(curPt.y));

        if (polyTool->points.size() >= 2) {
            Vec2 startPt = view.documentToScreen(polyTool->points.front());
            fb.drawLine(static_cast<i32>(curPt.x), static_cast<i32>(curPt.y),
                       static_cast<i32>(startPt.x), static_cast<i32>(startPt.y), 0x888888FF);
        }

        for (const auto& pt : polyTool->points) {
            Vec2 screenPt = view.documentToScreen(pt);
            i32 sx = static_cast<i32>(screenPt.x);
            i32 sy = static_cast<i32>(screenPt.y);
            fb.fillRect(Recti(sx - 2, sy - 2, 5, 5), 0xFFFFFFFF);
            fb.fillRect(Recti(sx - 1, sy - 1, 3, 3), 0x000000FF);
        }
        return;
    }

    if (tool->type == ToolType::FreeSelect) {
        auto* freeTool = static_cast<FreeSelectTool*>(tool);
        if (!freeTool->selecting || freeTool->points.empty()) return;

        for (size_t i = 0; i + 1 < freeTool->points.size(); ++i) {
            Vec2 p1 = view.documentToScreen(freeTool->points[i]);
            Vec2 p2 = view.documentToScreen(freeTool->points[i + 1]);
            drawVisibleLine(static_cast<i32>(p1.x), static_cast<i32>(p1.y),
                           static_cast<i32>(p2.x), static_cast<i32>(p2.y));
        }

        Vec2 lastPt = view.documentToScreen(freeTool->points.back());
        Vec2 curPt = view.documentToScreen(lastMousePos);
        drawVisibleLine(static_cast<i32>(lastPt.x), static_cast<i32>(lastPt.y),
                       static_cast<i32>(curPt.x), static_cast<i32>(curPt.y));

        Vec2 startPt = view.documentToScreen(freeTool->points.front());
        fb.drawLine(static_cast<i32>(curPt.x), static_cast<i32>(curPt.y),
                   static_cast<i32>(startPt.x), static_cast<i32>(startPt.y), 0x888888FF);
        return;
    }

    Vec2 startDoc, endDoc;
    bool isEllipse = false;

    if (tool->type == ToolType::RectangleSelect) {
        auto* rectTool = static_cast<RectangleSelectTool*>(tool);
        if (!rectTool->selecting) return;
        startDoc = rectTool->startPos;
        endDoc = lastMousePos;
    } else if (tool->type == ToolType::EllipseSelect) {
        auto* ellipseTool = static_cast<EllipseSelectTool*>(tool);
        if (!ellipseTool->selecting) return;
        startDoc = ellipseTool->startPos;
        endDoc = lastMousePos;
        isEllipse = true;
    } else {
        return;
    }

    Vec2 startScreen = view.documentToScreen(startDoc);
    Vec2 endScreen = view.documentToScreen(endDoc);

    i32 x1 = static_cast<i32>(std::min(startScreen.x, endScreen.x));
    i32 y1 = static_cast<i32>(std::min(startScreen.y, endScreen.y));
    i32 x2 = static_cast<i32>(std::max(startScreen.x, endScreen.x));
    i32 y2 = static_cast<i32>(std::max(startScreen.y, endScreen.y));
    i32 w = x2 - x1;
    i32 h = y2 - y1;

    if (isEllipse) {
        i32 cx = (x1 + x2) / 2;
        i32 cy = (y1 + y2) / 2;
        i32 rx = w / 2;
        i32 ry = h / 2;
        for (i32 t = 0; t < thickness; ++t) {
            drawEllipseOutline(fb, cx, cy, rx - t, ry - t, (t == 0) ? 0x000000FF : 0xFFFFFFFF);
        }
    } else {
        fb.drawRect(Recti(x1, y1, w, h), 0x000000FF, thickness);
        if (w > thickness * 2 && h > thickness * 2) {
            fb.drawRect(Recti(x1 + thickness, y1 + thickness, w - thickness * 2, h - thickness * 2), 0xFFFFFFFF, thickness);
        }
    }
}

bool DocumentViewWidget::onMouseDown(const MouseEvent& e) {
    AppState& state = getAppState();

    if (state.spaceHeld || e.button == MouseButton::Middle) {
        panning = true;
        panStartPos = e.globalPosition;
        state.capturedWidget = this;
        return true;
    }

    if (view.document && (e.button == MouseButton::Left || e.button == MouseButton::Right)) {
        Tool* currentTool = view.document->getTool();

        if (currentTool && currentTool->type == ToolType::Zoom) {
            zooming = true;
            zoomDragged = false;
            zoomButton = e.button;
            zoomStartPos = e.globalPosition;
            zoomStartLevel = view.zoom;
            zoomCenter = e.globalPosition;
            state.capturedWidget = this;
            return true;
        }
    }

    if (view.document && e.button == MouseButton::Left) {
        Tool* currentTool = view.document->getTool();

        if (currentTool && currentTool->type == ToolType::Pan) {
            panning = true;
            panStartPos = e.globalPosition;
            state.capturedWidget = this;
            return true;
        }

        Vec2 docPos = view.screenToDocument(e.globalPosition);
        lastMousePos = docPos;

        ToolEvent te;
        te.position = docPos;
#ifdef __EMSCRIPTEN__
        te.pressure = g_wasmPressure;  // Use touch pressure from JavaScript
#else
        te.pressure = 1.0f;
#endif
        te.zoom = view.zoom;
        te.shiftHeld = e.mods.shift;
        te.ctrlHeld = e.mods.ctrl;
        te.altHeld = e.mods.alt;

        view.document->handleMouseDown(te);
        toolActive = true;
        state.capturedWidget = this;
        state.needsRedraw = true;
        return true;
    }

    return false;
}

bool DocumentViewWidget::onMouseUp(const MouseEvent& e) {
    AppState& state = getAppState();

    if (panning) {
        panning = false;
        state.capturedWidget = nullptr;
        return true;
    }

    if (zooming) {
        if (!zoomDragged) {
            bool zoomIn = (state.zoomClickMode == 0);
            if (zoomButton == MouseButton::Right) {
                zoomIn = !zoomIn;
            }

            f32 factor = zoomIn ? 1.5f : (1.0f / 1.5f);
            f32 newZoom = view.zoom * factor;
            view.zoomAtPoint(zoomCenter, newZoom);
            state.needsRedraw = true;
        }

        zooming = false;
        zoomButton = MouseButton::None;
        state.capturedWidget = nullptr;
        return true;
    }

    if (view.document && (e.button == MouseButton::Left || toolActive)) {
        Vec2 docPos = view.screenToDocument(e.globalPosition);
        lastMousePos = docPos;

        ToolEvent te;
        te.position = docPos;
        te.zoom = view.zoom;
        te.shiftHeld = e.mods.shift;
        te.ctrlHeld = e.mods.ctrl;
        te.altHeld = e.mods.alt;

        view.document->handleMouseUp(te);
        toolActive = false;
        state.capturedWidget = nullptr;
        state.needsRedraw = true;
        return true;
    }

    return false;
}

bool DocumentViewWidget::onMouseDrag(const MouseEvent& e) {
    AppState& state = getAppState();

    if (panning) {
        Vec2 delta = e.globalPosition - panStartPos;
        view.panBy(delta);
        panStartPos = e.globalPosition;
        state.needsRedraw = true;
        return true;
    }

    if (zooming) {
        Vec2 delta = e.globalPosition - zoomStartPos;
        f32 dragDistance = std::sqrt(delta.x * delta.x + delta.y * delta.y);
        if (dragDistance > 5.0f) {
            zoomDragged = true;
        }

        if (zoomDragged) {
            f32 deltaY = zoomStartPos.y - e.globalPosition.y;
            f32 zoomFactor = 1.0f + deltaY * 0.005f;
            zoomFactor = clamp(zoomFactor, 0.1f, 10.0f);
            f32 newZoom = zoomStartLevel * zoomFactor;
            view.zoomAtPoint(zoomCenter, newZoom);
            state.needsRedraw = true;
        }
        return true;
    }

    if (view.document) {
        Vec2 docPos = view.screenToDocument(e.globalPosition);
        lastMousePos = docPos;

        ToolEvent te;
        te.position = docPos;
#ifdef __EMSCRIPTEN__
        te.pressure = g_wasmPressure;  // Use touch pressure from JavaScript
#else
        te.pressure = 1.0f;
#endif
        te.zoom = view.zoom;
        te.shiftHeld = e.mods.shift;
        te.ctrlHeld = e.mods.ctrl;
        te.altHeld = e.mods.alt;

        view.document->handleMouseDrag(te);
        state.needsRedraw = true;
        return true;
    }

    return false;
}

bool DocumentViewWidget::onMouseMove(const MouseEvent& e) {
    if (view.document) {
        Vec2 docPos = view.screenToDocument(e.globalPosition);
        lastMousePos = docPos;

        ToolEvent te;
        te.position = docPos;
        te.zoom = view.zoom;
        te.shiftHeld = e.mods.shift;
        te.ctrlHeld = e.mods.ctrl;
        te.altHeld = e.mods.alt;

        view.document->handleMouseMove(te);
        getAppState().needsRedraw = true;
    }
    return Widget::onMouseMove(e);
}

bool DocumentViewWidget::onMouseWheel(const MouseEvent& e) {
    if (e.wheelDelta != 0) {
        f32 zoomFactor = e.wheelDelta > 0 ? Config::ZOOM_STEP : 1.0f / Config::ZOOM_STEP;
        view.zoomAtPoint(e.globalPosition, view.zoom * zoomFactor);
        getAppState().needsRedraw = true;
        return true;
    }
    return false;
}

void DocumentViewWidget::onMouseEnter(const MouseEvent& e) {
    mouseOverCanvas = true;
    getAppState().needsRedraw = true;
}

void DocumentViewWidget::onMouseLeave(const MouseEvent& e) {
    mouseOverCanvas = false;
    getAppState().needsRedraw = true;
}

void DocumentViewWidget::onDocumentChanged(const Rect& dirtyRect) {
    // If we have a specific dirty rect, convert to screen coordinates and mark dirty
    if (dirtyRect.w > 0 && dirtyRect.h > 0) {
        // Convert document rect to screen coordinates
        Rect screenRect = view.documentToScreen(dirtyRect);

        // Add padding for brush cursor and anti-aliasing
        const f32 padding = 4.0f;
        screenRect.x -= padding;
        screenRect.y -= padding;
        screenRect.w += padding * 2;
        screenRect.h += padding * 2;

        // Clip to our widget bounds
        Rect gb = globalBounds();
        i32 x0 = std::max(static_cast<i32>(screenRect.x), static_cast<i32>(gb.x));
        i32 y0 = std::max(static_cast<i32>(screenRect.y), static_cast<i32>(gb.y));
        i32 x1 = std::min(static_cast<i32>(screenRect.x + screenRect.w), static_cast<i32>(gb.x + gb.w));
        i32 y1 = std::min(static_cast<i32>(screenRect.y + screenRect.h), static_cast<i32>(gb.y + gb.h));

        if (x1 > x0 && y1 > y0) {
            markDirty(Recti(x0, y0, x1 - x0, y1 - y0));
            return;
        }
    }

    // Fallback: mark entire canvas area dirty
    Rect gb = globalBounds();
    markDirty(Recti(static_cast<i32>(gb.x), static_cast<i32>(gb.y),
                    static_cast<i32>(gb.w), static_cast<i32>(gb.h)));
}

// ============================================================================
// ToolPalette implementation
// ============================================================================

ToolType ToolPalette::getButtonTypeForTool(ToolType type) {
    switch (type) {
        case ToolType::EllipseSelect:
        case ToolType::FreeSelect:
        case ToolType::PolygonSelect:
            return ToolType::RectangleSelect;
        case ToolType::Gradient:
            return ToolType::Fill;
        default:
            return type;
    }
}

ToolPalette::ToolPalette() {
    bgColor = Config::COLOR_PANEL;
    preferredSize = Vec2(Config::toolPaletteWidth(), 0);
    horizontalPolicy = SizePolicy::Fixed;
    verticalPolicy = SizePolicy::Expanding;
    setPadding(4 * Config::uiScale);

    auto vbox = createChild<VBoxLayout>(0);
    vbox->horizontalPolicy = SizePolicy::Expanding;
    vbox->verticalPolicy = SizePolicy::Expanding;

    gridLayout = vbox->createChild<GridLayout>(2, 4 * Config::uiScale, 4 * Config::uiScale);
    gridLayout->verticalPolicy = SizePolicy::Fixed;

    addToolButton(ToolType::Move, "V");
    addToolButton(ToolType::RectangleSelect, "M");
    addToolButton(ToolType::Brush, "B");
    addToolButton(ToolType::Eraser, "E");
    addToolButton(ToolType::Fill, "G");
    addToolButton(ToolType::MagicWand, "W");
    addToolButton(ToolType::Clone, "S");
    addToolButton(ToolType::Smudge, "R");
    addToolButton(ToolType::Dodge, "O");
    addToolButton(ToolType::Burn, "O");
    addToolButton(ToolType::Pan, "H");
    addToolButton(ToolType::Zoom, "Z");
    addToolButton(ToolType::Crop, "C");
    addToolButton(ToolType::ColorPicker, "I");

    u32 rows = (toolButtons.size() + 1) / 2;
    f32 buttonSize = 32 * Config::uiScale;
    f32 spacing = 4 * Config::uiScale;
    gridLayout->preferredSize = Vec2(0, rows * buttonSize + (rows - 1) * spacing);

    auto colorSection = vbox->createChild<VBoxLayout>(4 * Config::uiScale);
    colorSection->verticalPolicy = SizePolicy::Fixed;
    colorSection->preferredSize = Vec2(0, 78 * Config::uiScale);
    colorSection->setPadding(4 * Config::uiScale);

    swatchContainer = colorSection->createChild<Widget>();
    swatchContainer->preferredSize = Vec2(0, 44 * Config::uiScale);
    swatchContainer->verticalPolicy = SizePolicy::Fixed;

    bgSwatch = swatchContainer->createChild<ColorSwatch>(Color::white());
    bgSwatch->preferredSize = Vec2(28 * Config::uiScale, 28 * Config::uiScale);
    bgSwatch->onClick = [this]() {
        if (onColorSwatchClicked) onColorSwatchClicked(false);
    };

    fgSwatch = swatchContainer->createChild<ColorSwatch>(Color::black());
    fgSwatch->preferredSize = Vec2(28 * Config::uiScale, 28 * Config::uiScale);
    fgSwatch->onClick = [this]() {
        if (onColorSwatchClicked) onColorSwatchClicked(true);
    };

    auto btnRow = colorSection->createChild<HBoxLayout>(2 * Config::uiScale);
    btnRow->preferredSize = Vec2(0, 22 * Config::uiScale);
    btnRow->verticalPolicy = SizePolicy::Fixed;

    swapBtn = btnRow->createChild<IconButton>();
    swapBtn->preferredSize = Vec2(24 * Config::uiScale, 20 * Config::uiScale);
    swapBtn->renderIcon = [](Framebuffer& fb, const Rect& r, u32 color) {
        FontRenderer::instance().renderIconCentered(fb, "\xF3\xB0\x93\xA1", r, color, Config::defaultFontSize() * 1.3f, "Material Icons");
    };
    swapBtn->onClick = [this]() {
        getAppState().swapColors();
        updateColors();
        getAppState().needsRedraw = true;
    };

    resetBtn = btnRow->createChild<IconButton>();
    resetBtn->preferredSize = Vec2(24 * Config::uiScale, 20 * Config::uiScale);
    resetBtn->renderIcon = [](Framebuffer& fb, const Rect& r, u32 color) {
        FontRenderer::instance().renderIconCentered(fb, "\xF3\xB0\x80\xBD", r, color, Config::defaultFontSize(), "Material Icons");
    };
    resetBtn->onClick = [this]() {
        getAppState().resetColors();
        updateColors();
        getAppState().needsRedraw = true;
    };

    vbox->createChild<Spacer>();
}

void ToolPalette::updateColors() {
    AppState& state = getAppState();
    if (fgSwatch) fgSwatch->color = state.foregroundColor;
    if (bgSwatch) bgSwatch->color = state.backgroundColor;
}

void ToolPalette::layout() {
    Panel::layout();

    if (fgSwatch && bgSwatch && swatchContainer) {
        f32 swatchSize = 28 * Config::uiScale;
        f32 offset = 14 * Config::uiScale;

        fgSwatch->bounds = Rect(
            swatchContainer->bounds.x + 4 * Config::uiScale,
            swatchContainer->bounds.y,
            swatchSize, swatchSize);

        bgSwatch->bounds = Rect(
            swatchContainer->bounds.x + 4 * Config::uiScale + offset,
            swatchContainer->bounds.y + offset,
            swatchSize, swatchSize);
    }
}

void ToolPalette::addToolButton(ToolType type, const char* label) {
    auto btn = gridLayout->createChild<IconButton>();
    btn->preferredSize = Vec2(32 * Config::uiScale, 32 * Config::uiScale);
    btn->minSize = Vec2(32 * Config::uiScale, 32 * Config::uiScale);
    btn->maxSize = Vec2(32 * Config::uiScale, 32 * Config::uiScale);
    btn->horizontalPolicy = SizePolicy::Fixed;
    btn->verticalPolicy = SizePolicy::Fixed;
    btn->toggleMode = true;
    btn->iconColor = Config::GRAY_700;

    const char* materialIcon = nullptr;
    if (type == ToolType::Brush) {
        materialIcon = "\xF3\xB0\x83\xA3";
    } else if (type == ToolType::Eraser) {
        materialIcon = "\xF3\xB0\x87\xBE";
    } else if (type == ToolType::Move) {
        materialIcon = "\xF3\xB0\x81\x81";
    } else if (type == ToolType::RectangleSelect) {
        materialIcon = "\xF3\xB0\x92\x85";
    } else if (type == ToolType::Fill) {
        materialIcon = "\xF3\xB0\x89\xA6";
    } else if (type == ToolType::MagicWand) {
        materialIcon = "\xF3\xB1\xA1\x84";
    } else if (type == ToolType::Clone) {
        materialIcon = "\xF3\xB0\xB4\xB9";
    } else if (type == ToolType::Smudge) {
        materialIcon = "\xF3\xB1\x92\x84";
    } else if (type == ToolType::Burn) {
        materialIcon = "\xF3\xB0\x88\xB8";
    } else if (type == ToolType::Pan) {
        materialIcon = "\xF3\xB1\xA0\xAC";
    } else if (type == ToolType::Zoom) {
        materialIcon = "\xF3\xB0\x8D\x89";
    } else if (type == ToolType::Dodge) {
        materialIcon = "\xF3\xB0\x96\x99";
    } else if (type == ToolType::Crop) {
        materialIcon = "\xF3\xB0\x86\x9E";
    } else if (type == ToolType::ColorPicker) {
        materialIcon = "\xF3\xB0\x88\x8A";
    }

    if (materialIcon) {
        std::string iconStr = materialIcon;
        btn->renderIcon = [iconStr](Framebuffer& fb, const Rect& r, u32 color) {
            f32 iconSize = Config::defaultFontSize() * 1.5f;
            FontRenderer::instance().renderIconCentered(fb, iconStr, r, color, iconSize, "Material Icons");
        };
    } else {
        btn->renderIcon = [label](Framebuffer& fb, const Rect& r, u32 color) {
            f32 fontSize = Config::defaultFontSize() * 1.5f;
            Vec2 textSize = FontRenderer::instance().measureText(label, fontSize);
            FontRenderer::instance().renderText(fb, label,
                static_cast<i32>(r.x + (r.w - textSize.x) / 2),
                static_cast<i32>(r.y + (r.h - textSize.y) / 2),
                color, fontSize);
        };
    }

    btn->onClick = [this, type]() {
        selectTool(type);
    };

    if (type == ToolType::Zoom) {
        btn->onDoubleClick = [this]() {
            if (onZoomReset) onZoomReset();
        };
    } else if (type == ToolType::Pan) {
        btn->onDoubleClick = [this]() {
            if (onViewReset) onViewReset();
        };
    }

    toolButtons.push_back(btn);
    buttonToolTypes.push_back(type);
}

void ToolPalette::selectTool(ToolType type) {
    AppState& state = getAppState();
    Document* doc = state.activeDocument;
    if (!doc) return;

    PixelLayer* layer = doc->getActivePixelLayer();
    if (layer && (layer->transform.rotation != 0.0f ||
                  layer->transform.scale.x != 1.0f ||
                  layer->transform.scale.y != 1.0f)) {
        doc->rasterizePixelLayerTransform(doc->activeLayerIndex);
    }

    std::unique_ptr<Tool> tool;

    switch (type) {
        case ToolType::Move: tool = std::make_unique<MoveTool>(); break;
        case ToolType::RectangleSelect: tool = std::make_unique<RectangleSelectTool>(); break;
        case ToolType::EllipseSelect: tool = std::make_unique<EllipseSelectTool>(); break;
        case ToolType::FreeSelect: tool = std::make_unique<FreeSelectTool>(); break;
        case ToolType::PolygonSelect: tool = std::make_unique<PolygonSelectTool>(); break;
        case ToolType::MagicWand: tool = std::make_unique<MagicWandTool>(); break;
        case ToolType::Brush: tool = std::make_unique<BrushTool>(); break;
        case ToolType::Eraser: tool = std::make_unique<EraserTool>(); break;
        case ToolType::Fill:
        case ToolType::Gradient:
            if (state.fillMode == 0) {
                tool = std::make_unique<FillTool>();
                type = ToolType::Fill;
            } else {
                tool = std::make_unique<GradientTool>();
                type = ToolType::Gradient;
            }
            break;
        case ToolType::Clone: tool = std::make_unique<CloneTool>(); break;
        case ToolType::Smudge: tool = std::make_unique<SmudgeTool>(); break;
        case ToolType::Dodge: tool = std::make_unique<DodgeTool>(); break;
        case ToolType::Burn: tool = std::make_unique<BurnTool>(); break;
        case ToolType::ColorPicker: tool = std::make_unique<ColorPickerTool>(); break;
        case ToolType::Pan: tool = std::make_unique<PanTool>(); break;
        case ToolType::Zoom: tool = std::make_unique<ZoomTool>(); break;
        case ToolType::Crop: {
            auto cropTool = std::make_unique<CropTool>();
            cropTool->initializeCropRect(*doc);
            tool = std::move(cropTool);
            break;
        }
        default: tool = std::make_unique<BrushTool>(); break;
    }

    doc->setTool(std::move(tool));
    state.currentToolType = static_cast<i32>(type);

    ToolType buttonType = getButtonTypeForTool(type);
    for (size_t i = 0; i < toolButtons.size(); ++i) {
        toolButtons[i]->selected = (buttonToolTypes[i] == buttonType);
    }

    if (onToolChanged) {
        onToolChanged(type);
    }

    state.needsRedraw = true;
}

void ToolPalette::setEnabled(bool isEnabled) {
    for (auto* btn : toolButtons) {
        btn->enabled = isEnabled;
        if (!isEnabled) {
            btn->selected = false;
        }
    }
    if (fgSwatch) fgSwatch->enabled = isEnabled;
    if (bgSwatch) bgSwatch->enabled = isEnabled;
    if (swapBtn) swapBtn->enabled = isEnabled;
    if (resetBtn) resetBtn->enabled = isEnabled;
}

void ToolPalette::clearSelection() {
    for (auto* btn : toolButtons) {
        btn->selected = false;
    }
}

// ============================================================================
// TitleBarDragArea implementation
// ============================================================================

TitleBarDragArea::TitleBarDragArea() {
    horizontalPolicy = SizePolicy::Expanding;
    verticalPolicy = SizePolicy::Expanding;
    minSize = Vec2(100 * Config::uiScale, 0);
}

bool TitleBarDragArea::onMouseDown(const MouseEvent& e) {
    if (e.button == MouseButton::Left) {
        u64 now = Platform::getMilliseconds();
        if (now - lastClickTime < 300) {
            if (onDoubleClick) onDoubleClick();
            lastClickTime = 0;
        } else {
            lastClickTime = now;
            if (onStartDrag) {
                i32 rootX = static_cast<i32>(e.globalPosition.x);
                i32 rootY = static_cast<i32>(e.globalPosition.y);
                onStartDrag(rootX, rootY);
            }
        }
        return true;
    }
    return false;
}

void TitleBarDragArea::renderSelf(Framebuffer& fb) {
    // Drag area is transparent - just uses panel background
}

// ============================================================================
// WindowControlButton implementation
// ============================================================================

WindowControlButton::WindowControlButton(Type t) : type(t) {
    f32 size = Config::menuBarHeight();
    preferredSize = Vec2(size * 1.5f, size);
    horizontalPolicy = SizePolicy::Fixed;
    verticalPolicy = SizePolicy::Fixed;
}

void WindowControlButton::renderSelf(Framebuffer& fb) {
    Rect gb = globalBounds();
    Recti rect(static_cast<i32>(gb.x), static_cast<i32>(gb.y),
               static_cast<i32>(gb.w), static_cast<i32>(gb.h));

    u32 bgColor = Config::COLOR_TITLEBAR;
    if (type == Type::Close) {
        if (pressed) bgColor = 0xC42B1CFF;
        else if (hovered) bgColor = 0xE81123FF;
    } else {
        if (pressed) bgColor = Config::COLOR_ACTIVE;
        else if (hovered) bgColor = Config::COLOR_HOVER;
    }
    fb.fillRect(rect, bgColor);

    i32 cx = rect.x + rect.w / 2;
    i32 cy = rect.y + rect.h / 2;
    u32 iconColor = (type == Type::Close && (hovered || pressed)) ? 0xFFFFFFFF : Config::COLOR_TEXT;
    i32 iconSize = static_cast<i32>(5 * Config::uiScale);

    switch (type) {
        case Type::Minimize:
            fb.fillRect(cx - iconSize, cy, iconSize * 2, static_cast<i32>(Config::uiScale), iconColor);
            break;
        case Type::Maximize:
            fb.drawRect(Recti(cx - iconSize, cy - iconSize, iconSize * 2, iconSize * 2), iconColor, 1);
            break;
        case Type::Restore:
            fb.drawRect(Recti(cx - iconSize + 2, cy - iconSize - 2, iconSize * 2 - 2, iconSize * 2 - 2), iconColor, 1);
            fb.fillRect(cx - iconSize, cy - iconSize + 2, iconSize * 2 - 2, iconSize * 2 - 2, bgColor);
            fb.drawRect(Recti(cx - iconSize, cy - iconSize + 2, iconSize * 2 - 2, iconSize * 2 - 2), iconColor, 1);
            break;
        case Type::Close:
            for (i32 i = -iconSize; i <= iconSize; ++i) {
                fb.setPixel(cx + i, cy + i, iconColor);
                fb.setPixel(cx + i, cy - i, iconColor);
                fb.setPixel(cx + i + 1, cy + i, iconColor);
                fb.setPixel(cx + i + 1, cy - i, iconColor);
            }
            break;
    }
}

void WindowControlButton::onMouseEnter(const MouseEvent& e) {
    hovered = true;
    getAppState().needsRedraw = true;
}

void WindowControlButton::onMouseLeave(const MouseEvent& e) {
    hovered = false;
    pressed = false;
    getAppState().needsRedraw = true;
}

bool WindowControlButton::onMouseDown(const MouseEvent& e) {
    if (e.button == MouseButton::Left) {
        pressed = true;
        getAppState().needsRedraw = true;
        return true;
    }
    return false;
}

bool WindowControlButton::onMouseUp(const MouseEvent& e) {
    if (e.button == MouseButton::Left && pressed) {
        pressed = false;
        if (hovered && onClick) onClick();
        getAppState().needsRedraw = true;
        return true;
    }
    return false;
}

// ============================================================================
// ResizeDivider implementation
// ============================================================================

ResizeDivider::ResizeDivider() {
    preferredSize = Vec2(5 * Config::uiScale, 0);
    horizontalPolicy = SizePolicy::Fixed;
    verticalPolicy = SizePolicy::Expanding;
}

void ResizeDivider::renderSelf(Framebuffer& fb) {
    Rect gb = globalBounds();
    u32 color = (dragging || hovered) ? Config::COLOR_RESIZER_HOVER : Config::COLOR_RESIZER;
    Recti rect(static_cast<i32>(gb.x), static_cast<i32>(gb.y),
               static_cast<i32>(gb.w), static_cast<i32>(gb.h));
    fb.fillRect(rect, color);
}

bool ResizeDivider::onMouseDown(const MouseEvent& e) {
    if (e.button == MouseButton::Left && targetWidget) {
        dragging = true;
        dragStartX = e.globalPosition.x;
        dragStartWidth = targetWidget->preferredSize.x;
        getAppState().capturedWidget = this;
        getAppState().needsRedraw = true;
        return true;
    }
    return false;
}

bool ResizeDivider::onMouseDrag(const MouseEvent& e) {
    if (dragging && targetWidget) {
        f32 deltaX = dragStartX - e.globalPosition.x;
        f32 newWidth = dragStartWidth + deltaX;
        newWidth = std::max(minWidth, std::min(maxWidth, newWidth));
        targetWidget->preferredSize.x = newWidth;
        if (onResized) onResized();
        getAppState().needsRedraw = true;
        return true;
    }
    return false;
}

bool ResizeDivider::onMouseUp(const MouseEvent& e) {
    if (dragging) {
        dragging = false;
        getAppState().capturedWidget = nullptr;
        getAppState().needsRedraw = true;
        return true;
    }
    return false;
}

void ResizeDivider::onMouseEnter(const MouseEvent& e) {
    hovered = true;
    getAppState().needsRedraw = true;
}

void ResizeDivider::onMouseLeave(const MouseEvent& e) {
    hovered = false;
    getAppState().needsRedraw = true;
}

// ============================================================================
// VPanelResizer implementation
// ============================================================================

VPanelResizer::VPanelResizer() {
    preferredSize = Vec2(0, 5 * Config::uiScale);
    horizontalPolicy = SizePolicy::Expanding;
    verticalPolicy = SizePolicy::Fixed;
}

void VPanelResizer::renderSelf(Framebuffer& fb) {
    Rect gb = globalBounds();
    u32 color = (dragging || hovered) ? Config::COLOR_RESIZER_HOVER : Config::COLOR_RESIZER;
    Recti rect(static_cast<i32>(gb.x), static_cast<i32>(gb.y),
               static_cast<i32>(gb.w), static_cast<i32>(gb.h));
    fb.fillRect(rect, color);
}

bool VPanelResizer::onMouseDown(const MouseEvent& e) {
    if (e.button == MouseButton::Left && aboveWidget && belowWidget) {
        dragging = true;
        dragStartY = e.globalPosition.y;
        dragStartHeightAbove = aboveWidget->bounds.h;
        dragStartHeightBelow = belowWidget->bounds.h;
        aboveWidget->verticalPolicy = SizePolicy::Fixed;
        belowWidget->verticalPolicy = SizePolicy::Fixed;
        aboveWidget->preferredSize.y = dragStartHeightAbove;
        belowWidget->preferredSize.y = dragStartHeightBelow;
        getAppState().capturedWidget = this;
        getAppState().needsRedraw = true;
        return true;
    }
    return false;
}

bool VPanelResizer::onMouseDrag(const MouseEvent& e) {
    if (dragging && aboveWidget && belowWidget) {
        f32 deltaY = e.globalPosition.y - dragStartY;
        f32 newHeightAbove = dragStartHeightAbove + deltaY;
        f32 newHeightBelow = dragStartHeightBelow - deltaY;
        if (newHeightAbove < minHeight) {
            f32 diff = minHeight - newHeightAbove;
            newHeightAbove = minHeight;
            newHeightBelow -= diff;
        }
        if (newHeightBelow < minHeight) {
            f32 diff = minHeight - newHeightBelow;
            newHeightBelow = minHeight;
            newHeightAbove -= diff;
        }
        aboveWidget->preferredSize.y = newHeightAbove;
        belowWidget->preferredSize.y = newHeightBelow;
        if (onResized) onResized();
        getAppState().needsRedraw = true;
        return true;
    }
    return false;
}

bool VPanelResizer::onMouseUp(const MouseEvent& e) {
    if (dragging) {
        dragging = false;
        getAppState().capturedWidget = nullptr;
        getAppState().needsRedraw = true;
        return true;
    }
    return false;
}

void VPanelResizer::onMouseEnter(const MouseEvent& e) {
    hovered = true;
    getAppState().needsRedraw = true;
}

void VPanelResizer::onMouseLeave(const MouseEvent& e) {
    hovered = false;
    getAppState().needsRedraw = true;
}

// ============================================================================
// StatusBar implementation
// ============================================================================

StatusBar::StatusBar() {
    bgColor = Config::COLOR_PANEL;
    preferredSize = Vec2(0, Config::statusBarHeight());
    verticalPolicy = SizePolicy::Fixed;
    setPadding(4 * Config::uiScale);

    auto layout = createChild<HBoxLayout>(8 * Config::uiScale);
    layout->stretch = true;
    constexpr f32 LABEL_PADDING = 4.0f;
    constexpr f32 BTN_PADDING = 8.0f;
    f32 itemHeight = 20 * Config::uiScale;

    // Left side container (zoom, size, position)
    leftLayout = layout->createChild<HBoxLayout>(8 * Config::uiScale);
    leftLayout->horizontalPolicy = SizePolicy::Fixed;

    zoomButton = leftLayout->createChild<Button>("100%");
    {
        Vec2 textSize = FontRenderer::instance().measureText("6400%", Config::defaultFontSize());
        zoomButton->preferredSize = Vec2(textSize.x + BTN_PADDING * 2, itemHeight);
    }
    zoomButton->normalColor = Config::COLOR_PANEL;
    zoomButton->hoverColor = Config::COLOR_HOVER;
    zoomButton->borderColor = 0;
    zoomButton->onClick = [this]() {
        if (onFitToScreen) onFitToScreen();
    };

    zoomSeparator = leftLayout->createChild<Separator>(false);
    sizeLabel = leftLayout->createChild<Label>("1920 x 1080");
    {
        Vec2 textSize = FontRenderer::instance().measureText("99999 x 99999", Config::defaultFontSize());
        sizeLabel->preferredSize = Vec2(textSize.x + LABEL_PADDING * 2, itemHeight);
        sizeLabel->minSize = sizeLabel->preferredSize;
    }

    sizeSeparator = leftLayout->createChild<Separator>(false);
    positionLabel = leftLayout->createChild<Label>("X: 0, Y: 0");
    {
        Vec2 minTextSize = FontRenderer::instance().measureText("X: -9999, Y: -9999", Config::defaultFontSize());
        positionLabel->minSize = Vec2(minTextSize.x + LABEL_PADDING * 2, itemHeight);
        positionLabel->preferredSize = positionLabel->minSize;
    }

    // Spacer pushes right side to edge
    layout->createChild<Spacer>();

    // Right side container (scale controls) - always aligned to right
    rightLayout = layout->createChild<HBoxLayout>(8 * Config::uiScale);
    rightLayout->horizontalPolicy = SizePolicy::Fixed;

    scaleSeparator = rightLayout->createChild<Separator>(false);

    scaleLabel = rightLayout->createChild<Label>("UI Scale");
    {
        Vec2 textSize = FontRenderer::instance().measureText("UI Scale", Config::defaultFontSize());
        scaleLabel->preferredSize = Vec2(textSize.x + LABEL_PADDING * 4, itemHeight);
        scaleLabel->minSize = scaleLabel->preferredSize;
    }

    scaleSlider = rightLayout->createChild<Slider>(0.5f, 4.0f, Config::uiScale);
    scaleSlider->preferredSize = Vec2(80 * Config::uiScale, itemHeight);
    scaleSlider->onDragEnd = [this]() {
        if (onScaleChanged) onScaleChanged(scaleSlider->value);
    };

    scale1xBtn = rightLayout->createChild<Button>("1x");
    {
        Vec2 textSize = FontRenderer::instance().measureText("1x", Config::defaultFontSize());
        scale1xBtn->preferredSize = Vec2(textSize.x + BTN_PADDING * 2, itemHeight);
    }
    scale1xBtn->normalColor = Config::COLOR_PANEL;
    scale1xBtn->hoverColor = Config::COLOR_HOVER;
    scale1xBtn->onClick = [this]() {
        scaleSlider->setValue(1.0f);
        if (onScaleChanged) onScaleChanged(1.0f);
    };

    scale2xBtn = rightLayout->createChild<Button>("2x");
    {
        Vec2 textSize = FontRenderer::instance().measureText("2x", Config::defaultFontSize());
        scale2xBtn->preferredSize = Vec2(textSize.x + BTN_PADDING * 2, itemHeight);
    }
    scale2xBtn->normalColor = Config::COLOR_PANEL;
    scale2xBtn->hoverColor = Config::COLOR_HOVER;
    scale2xBtn->onClick = [this]() {
        scaleSlider->setValue(2.0f);
        if (onScaleChanged) onScaleChanged(2.0f);
    };

    scale4xBtn = rightLayout->createChild<Button>("4x");
    {
        Vec2 textSize = FontRenderer::instance().measureText("4x", Config::defaultFontSize());
        scale4xBtn->preferredSize = Vec2(textSize.x + BTN_PADDING * 2, itemHeight);
    }
    scale4xBtn->normalColor = Config::COLOR_PANEL;
    scale4xBtn->hoverColor = Config::COLOR_HOVER;
    scale4xBtn->onClick = [this]() {
        scaleSlider->setValue(4.0f);
        if (onScaleChanged) onScaleChanged(4.0f);
    };
}

void StatusBar::layout() {
    f32 availableWidth = bounds.w;
    f32 padding = 4 * Config::uiScale;
    f32 spacing = 8 * Config::uiScale;
    f32 sep = 1 + spacing;  // separator + one spacing

    // Calculate widths of each element
    f32 zoomW = zoomButton->preferredSize.x;
    f32 sizeW = sizeLabel->preferredSize.x;
    f32 posW = positionLabel->preferredSize.x;
    f32 scaleLabelW = scaleLabel->preferredSize.x;
    f32 sliderW = scaleSlider->preferredSize.x;
    f32 btn1W = scale1xBtn->preferredSize.x;
    f32 btn2W = scale2xBtn->preferredSize.x;
    f32 btn4W = scale4xBtn->preferredSize.x;

    // Right side always needs: buttons
    f32 buttonsW = btn1W + spacing + btn2W + spacing + btn4W;
    f32 minRightWidth = buttonsW;

    // Determine what fits
    f32 usedWidth = padding * 2 + minRightWidth;

    bool showSlider = (availableWidth >= usedWidth + spacing + sliderW);
    if (showSlider) usedWidth += spacing + sliderW;

    bool showScaleLabel = showSlider && (availableWidth >= usedWidth + spacing + scaleLabelW);
    if (showScaleLabel) usedWidth += spacing + scaleLabelW;

    // Left side elements (need some minimum space for spacer too)
    f32 minSpacerWidth = 20 * Config::uiScale;

    bool showZoom = (availableWidth >= usedWidth + minSpacerWidth + sep + zoomW);
    bool showSize = showZoom && (availableWidth >= usedWidth + minSpacerWidth + sep + zoomW + sep + sizeW);
    bool showPos = showSize && (availableWidth >= usedWidth + minSpacerWidth + sep + zoomW + sep + sizeW + sep + posW);

    // Apply visibility
    if (scaleLabel) scaleLabel->visible = showScaleLabel;
    if (scaleSeparator) scaleSeparator->visible = showSlider;
    if (scaleSlider) scaleSlider->visible = showSlider;
    if (zoomButton) zoomButton->visible = showZoom;
    if (zoomSeparator) zoomSeparator->visible = showZoom && showSize;
    if (sizeLabel) sizeLabel->visible = showSize;
    if (sizeSeparator) sizeSeparator->visible = showSize && showPos;
    if (positionLabel) positionLabel->visible = showPos;

    // Update left layout width based on visible elements
    f32 leftWidth = 0;
    if (showZoom) leftWidth += zoomW;
    if (showSize) leftWidth += sep + sizeW;
    if (showPos) leftWidth += sep + posW;
    if (leftLayout) leftLayout->preferredSize.x = leftWidth;

    // Update right layout width based on visible elements
    f32 rightWidth = buttonsW;
    if (showSlider) rightWidth += spacing + sliderW;
    if (showScaleLabel) rightWidth += spacing + scaleLabelW;
    if (showSlider) rightWidth += sep;  // separator before scale controls
    if (rightLayout) rightLayout->preferredSize.x = rightWidth;

    Panel::layout();
}

void StatusBar::update(const Vec2& mousePos, f32 zoom, u32 width, u32 height) {
    if (positionLabel) {
        positionLabel->setText("X: " + std::to_string(static_cast<i32>(mousePos.x)) +
                              ", Y: " + std::to_string(static_cast<i32>(mousePos.y)));
    }
    if (zoomButton && zoomButton->enabled) {
        zoomButton->text = std::to_string(static_cast<i32>(zoom * 100 + 0.5f)) + "%";
    }
    if (sizeLabel) {
        sizeLabel->setText(std::to_string(width) + " x " + std::to_string(height));
    }
}

void StatusBar::setEnabled(bool isEnabled) {
    if (zoomButton) {
        zoomButton->enabled = isEnabled;
        if (!isEnabled) {
            zoomButton->text = "0%";
            zoomButton->hovered = false;
        }
    }
}

// ============================================================================
// MenuBar implementation
// ============================================================================

MenuBar::MenuBar() {
    bgColor = Config::COLOR_TITLEBAR;
    preferredSize = Vec2(0, Config::menuBarHeight());
    verticalPolicy = SizePolicy::Fixed;
    horizontalPolicy = SizePolicy::Expanding;

    auto layout = createChild<HBoxLayout>(0);
    layout->stretch = true;

    // Menu items on the left
    menuLayout = layout->createChild<HBoxLayout>(0);
    menuLayout->horizontalPolicy = SizePolicy::Fixed;

    addMenu(menuLayout, "File", createFileMenu());
    addMenu(menuLayout, "Edit", createEditMenu());
    addMenu(menuLayout, "Canvas", createCanvasMenu());
    addMenu(menuLayout, "Layer", createLayerMenu());
    addMenu(menuLayout, "Select", createSelectMenu());
    addMenu(menuLayout, "View", createViewMenu());
    addMenu(menuLayout, "Help", createHelpMenu());

#ifndef __EMSCRIPTEN__
    // Draggable title bar area in the middle (not needed in browser)
    dragArea = layout->createChild<TitleBarDragArea>();
    dragArea->onStartDrag = [this](i32 x, i32 y) {
        if (onWindowDrag) onWindowDrag(x, y);
    };
    dragArea->onDoubleClick = [this]() {
        if (onWindowMaximize) onWindowMaximize();
    };

    // Window control buttons on the right (browser provides these)
    controlLayout = layout->createChild<HBoxLayout>(0);
    controlLayout->horizontalPolicy = SizePolicy::Fixed;

    minimizeBtn = controlLayout->createChild<WindowControlButton>(WindowControlButton::Type::Minimize);
    minimizeBtn->onClick = [this]() {
        if (onWindowMinimize) onWindowMinimize();
    };

    maximizeBtn = controlLayout->createChild<WindowControlButton>(WindowControlButton::Type::Maximize);
    maximizeBtn->onClick = [this]() {
        if (onWindowMaximize) onWindowMaximize();
    };

    closeBtn = controlLayout->createChild<WindowControlButton>(WindowControlButton::Type::Close);
    closeBtn->onClick = [this]() {
        if (onWindowClose) onWindowClose();
    };
#endif
}

void MenuBar::updateMaximizeButton() {
    if (maximizeBtn && isWindowMaximized) {
        maximizeBtn->setType(isWindowMaximized() ?
            WindowControlButton::Type::Restore : WindowControlButton::Type::Maximize);
    }
}

void MenuBar::doLayout() {
    // Priority (highest to lowest):
    // 1. Window buttons (close > maximize > minimize) - never hide
    // 2. Drag area (min 100px, but can shrink if needed)
    // 3. Menu items (hide right-to-left when space is tight)

    f32 totalWidth = bounds.w;
    f32 minDragWidth = 100.0f * Config::uiScale;

    // Calculate control buttons width (always visible)
    f32 controlWidth = 0;
    if (minimizeBtn) controlWidth += minimizeBtn->preferredSize.x;
    if (maximizeBtn) controlWidth += maximizeBtn->preferredSize.x;
    if (closeBtn) controlWidth += closeBtn->preferredSize.x;

    if (controlLayout) {
        controlLayout->preferredSize.x = controlWidth;
    }

    // Calculate how much space is available for menus + drag area
    f32 availableForMenusAndDrag = totalWidth - controlWidth;

    // First, calculate total width if all menus are shown
    f32 totalMenuWidth = 0;
    for (auto& [btn, popup] : menus) {
        totalMenuWidth += btn->preferredSize.x;
    }

    // Determine how many menus we can show (hide from right to left)
    f32 menuWidth = 0;
    i32 visibleCount = 0;

    // Try to fit as many menus as possible from left to right
    for (size_t i = 0; i < menus.size(); ++i) {
        auto& [btn, popup] = menus[i];
        f32 btnWidth = btn->preferredSize.x;

        if (menuWidth + btnWidth + minDragWidth <= availableForMenusAndDrag) {
            menuWidth += btnWidth;
            visibleCount = i + 1;
        } else {
            break;
        }
    }

    // Now set visibility: show first visibleCount menus, hide the rest
    for (size_t i = 0; i < menus.size(); ++i) {
        menus[i].first->visible = (i < static_cast<size_t>(visibleCount));
    }

    // Recalculate actual menu width based on visible menus
    menuWidth = 0;
    for (size_t i = 0; i < static_cast<size_t>(visibleCount); ++i) {
        menuWidth += menus[i].first->preferredSize.x;
    }

    if (menuLayout) {
        menuLayout->preferredSize.x = menuWidth;
    }
}

void MenuBar::layout() {
    doLayout();
    Panel::layout();
}

bool MenuBar::onMouseMove(const MouseEvent& e) {
    // If a menu is open, hovering over other menu buttons should switch menus
    if (menuModeActive && activeMenu) {
        for (auto& [btn, popup] : menus) {
            if (btn != nullptr && popup != activeMenu) {
                Rect btnBounds = btn->globalBounds();
                if (btnBounds.contains(e.globalPosition)) {
                    // Switch to this menu
                    switchingMenus = true;
                    closeActiveMenu();
                    activeMenu = popup;
                    popup->show(btnBounds.x, btnBounds.bottom());
                    OverlayManager::instance().registerOverlay(popup, ZOrder::POPUP_MENU, [this]() {
                        closeActiveMenu();
                    });
                    switchingMenus = false;
                    getAppState().needsRedraw = true;
                    return true;
                }
            }
        }
    }
    return Panel::onMouseMove(e);
}

void MenuBar::closeActiveMenu() {
    if (activeMenu) {
        activeMenu->hide();
        OverlayManager::instance().unregisterOverlay(activeMenu);
        if (!switchingMenus) {
            lastMenuCloseTime = Platform::getMilliseconds();
            menuModeActive = false;
        }
        activeMenu = nullptr;
    }
}

void MenuBar::setDocumentMenusEnabled(bool enabled) {
    // Enable/disable menu items that require an open document
    for (auto& [btn, popup] : menus) {
        // Skip File and Help menus - they work without documents
        if (btn->text == "File" || btn->text == "Help") continue;
        btn->enabled = enabled;
        for (auto& item : popup->items) {
            if (!item.separator) {
                item.enabled = enabled;
            }
        }
    }
}

void MenuBar::addMenu(HBoxLayout* layout, const char* name, PopupMenu* popup) {
    auto* btn = layout->createChild<Button>(name);
    btn->fontSize = Config::menuFontSize();
    Vec2 textSize = FontRenderer::instance().measureText(name, btn->fontSize);
    btn->preferredSize = Vec2(textSize.x + 16 * Config::uiScale, Config::menuBarHeight());
    btn->normalColor = Config::COLOR_TITLEBAR;
    btn->hoverColor = Config::COLOR_HOVER;
    btn->borderColor = 0;

    btn->onClick = [this, btn, popup]() {
        u64 now = Platform::getMilliseconds();
        // Prevent reopening if just closed
        if (now - lastMenuCloseTime < 100) {
            return;
        }

        if (activeMenu == popup) {
            closeActiveMenu();
        } else {
            closeActiveMenu();
            activeMenu = popup;
            menuModeActive = true;
            Rect btnBounds = btn->globalBounds();
            popup->show(btnBounds.x, btnBounds.bottom());
            OverlayManager::instance().registerOverlay(popup, ZOrder::POPUP_MENU, [this]() {
                closeActiveMenu();
            });
        }
    };

    menus.push_back({btn, popup});
    createChild<Widget>()->addChild(std::unique_ptr<PopupMenu>(popup));
}

PopupMenu* MenuBar::createFileMenu() {
    auto* menu = new PopupMenu();

    menu->addItem("New...", "", [this]() {
        closeActiveMenu();
        if (onNewDocument) onNewDocument();
    });

    menu->addItem("Open...", "", [this]() {
        closeActiveMenu();
        getAppState().requestOpenFileDialog("Open File", "*.png *.jpg *.jpeg *.bmp *.gif *.pp",
            [](const std::string& path) {
                if (path.empty()) return;

                std::unique_ptr<Document> doc;
                if (Platform::getFileExtension(path) == ".pp") {
                    doc = ProjectFile::load(path);
                } else {
                    doc = ImageIO::loadAsDocument(path);
                }

                if (doc) {
                    AppState& state = getAppState();
                    doc->filePath = path;
                    doc->name = Platform::getFileName(path);

                    // Register embedded fonts with FontRenderer
                    for (const auto& [fontName, fontData] : doc->embeddedFonts) {
                        FontRenderer::instance().loadCustomFont(fontName, fontData.data(), static_cast<i32>(fontData.size()));
                    }

                    Document* docPtr = doc.get();
                    state.documents.push_back(std::move(doc));
                    state.setActiveDocument(docPtr);
                }
            });
    });

    menu->addSeparator();

    menu->addItem("Close", "", [this]() {
        closeActiveMenu();
        AppState& state = getAppState();
        if (state.activeDocument) {
            state.closeDocument(state.activeDocument);
        }
    });

    menu->addItem("Close All", "", [this]() {
        closeActiveMenu();
        AppState& state = getAppState();
        while (!state.documents.empty()) {
            state.closeDocument(0);
        }
    });

    menu->addSeparator();

    menu->addItem("Save...", "", [this]() {
        closeActiveMenu();
        AppState& state = getAppState();
        Document* doc = state.activeDocument;
        if (!doc) return;

        std::string defaultName = doc->name;
        if (Platform::getFileExtension(defaultName) != ".pp") {
            defaultName += ".pp";
        }
        state.requestSaveFileDialog("Save Project", defaultName, "*.pp",
            [doc](const std::string& path) {
                if (path.empty()) return;
                ProjectFile::save(path, *doc);
            });
    });

    menu->addItem("Export...", "", [this]() {
        closeActiveMenu();
        AppState& state = getAppState();
        Document* doc = state.activeDocument;
        if (!doc) return;

        std::string defaultName = doc->name + ".png";
        state.requestSaveFileDialog("Export PNG", defaultName, "*.png",
            [doc](const std::string& path) {
                if (path.empty()) return;
                ImageIO::exportPNG(path, *doc);
            });
    });

    menu->addSeparator();

    menu->addItem("Quit", "", [this]() {
        closeActiveMenu();
        getAppState().running = false;
    });

    return menu;
}

PopupMenu* MenuBar::createEditMenu() {
    auto* menu = new PopupMenu();

    menu->addItem("Cut", "", [this]() {
        closeActiveMenu();
        Document* doc = getAppState().activeDocument;
        if (doc) doc->cut();
    });

    menu->addItem("Copy", "", [this]() {
        closeActiveMenu();
        Document* doc = getAppState().activeDocument;
        if (doc) doc->copy();
    });

    menu->addItem("Paste", "", [this]() {
        closeActiveMenu();
        Document* doc = getAppState().activeDocument;
        if (doc) doc->paste();
    });

    menu->addItem("Paste in Place", "", [this]() {
        closeActiveMenu();
        Document* doc = getAppState().activeDocument;
        if (doc) doc->pasteInPlace();
    });

    menu->addSeparator();

    menu->addItem("Rename Document", "", [this]() {
        closeActiveMenu();
        if (onRenameDocument) onRenameDocument();
    });

    return menu;
}

PopupMenu* MenuBar::createCanvasMenu() {
    auto* menu = new PopupMenu();

    menu->addItem("Rotate Left", "", [this]() {
        closeActiveMenu();
        Document* doc = getAppState().activeDocument;
        if (doc) doc->rotateLeft();
    });

    menu->addItem("Rotate Right", "", [this]() {
        closeActiveMenu();
        Document* doc = getAppState().activeDocument;
        if (doc) doc->rotateRight();
    });

    menu->addSeparator();

    menu->addItem("Flip Horizontal", "", [this]() {
        closeActiveMenu();
        Document* doc = getAppState().activeDocument;
        if (doc) doc->flipHorizontal();
    });

    menu->addItem("Flip Vertical", "", [this]() {
        closeActiveMenu();
        Document* doc = getAppState().activeDocument;
        if (doc) doc->flipVertical();
    });

    menu->addSeparator();

    menu->addItem("Canvas Size...", "", [this]() {
        closeActiveMenu();
        if (onCanvasSize) onCanvasSize();
    });

    return menu;
}

PopupMenu* MenuBar::createLayerMenu() {
    auto* menu = new PopupMenu();

    menu->addItem("Rotate Left", "", [this]() {
        closeActiveMenu();
        Document* doc = getAppState().activeDocument;
        if (doc) doc->rotateLeft();
    });

    menu->addItem("Rotate Right", "", [this]() {
        closeActiveMenu();
        Document* doc = getAppState().activeDocument;
        if (doc) doc->rotateRight();
    });

    menu->addSeparator();

    menu->addItem("Flip Horizontal", "", [this]() {
        closeActiveMenu();
        Document* doc = getAppState().activeDocument;
        if (doc) doc->flipHorizontal();
    });

    menu->addItem("Flip Vertical", "", [this]() {
        closeActiveMenu();
        Document* doc = getAppState().activeDocument;
        if (doc) doc->flipVertical();
    });

    menu->addSeparator();

    menu->addItem("Merge Down", "", [this]() {
        closeActiveMenu();
        Document* doc = getAppState().activeDocument;
        if (doc && doc->activeLayerIndex >= 0) {
            doc->mergeDown(doc->activeLayerIndex);
        }
    });

    menu->addItem("Merge Visible", "", [this]() {
        closeActiveMenu();
        Document* doc = getAppState().activeDocument;
        if (doc) doc->mergeVisible();
    });

    menu->addSeparator();

    menu->addItem("Move Up", "", [this]() {
        closeActiveMenu();
        Document* doc = getAppState().activeDocument;
        if (doc && doc->activeLayerIndex > 0) {
            doc->moveLayer(doc->activeLayerIndex, doc->activeLayerIndex - 1);
        }
    });

    menu->addItem("Move Down", "", [this]() {
        closeActiveMenu();
        Document* doc = getAppState().activeDocument;
        if (doc && doc->activeLayerIndex < doc->getLayerCount() - 1) {
            doc->moveLayer(doc->activeLayerIndex, doc->activeLayerIndex + 1);
        }
    });

    return menu;
}

PopupMenu* MenuBar::createSelectMenu() {
    auto* menu = new PopupMenu();

    menu->addItem("Select All", "", [this]() {
        closeActiveMenu();
        Document* doc = getAppState().activeDocument;
        if (doc) doc->selectAll();
    });

    menu->addItem("Deselect", "", [this]() {
        closeActiveMenu();
        Document* doc = getAppState().activeDocument;
        if (doc) doc->deselect();
    });

    menu->addItem("Invert Selection", "", [this]() {
        closeActiveMenu();
        Document* doc = getAppState().activeDocument;
        if (doc) doc->invertSelection();
    });

    return menu;
}

PopupMenu* MenuBar::createViewMenu() {
    auto* menu = new PopupMenu();

    menu->addItem("Navigator Panel", "", [this]() {
        closeActiveMenu();
        AppState& state = getAppState();
        state.showNavigator = !state.showNavigator;
        state.needsRedraw = true;
    });

    menu->addItem("Properties Panel", "", [this]() {
        closeActiveMenu();
        AppState& state = getAppState();
        state.showProperties = !state.showProperties;
        state.needsRedraw = true;
    });

    menu->addItem("Layers Panel", "", [this]() {
        closeActiveMenu();
        AppState& state = getAppState();
        state.showLayers = !state.showLayers;
        state.needsRedraw = true;
    });

    menu->addSeparator();

    menu->addItem("Fit Screen", "", [this]() {
        closeActiveMenu();
        if (onFitToScreen) onFitToScreen();
    });

    menu->addItem("Zoom In", "", [this]() {
        closeActiveMenu();
    });

    menu->addItem("Zoom Out", "", [this]() {
        closeActiveMenu();
    });

    return menu;
}

PopupMenu* MenuBar::createHelpMenu() {
    auto* menu = new PopupMenu();

    menu->addItem("About", "", [this]() {
        closeActiveMenu();
        if (onAbout) onAbout();
    });

    menu->addItem("GitHub", "", [this]() {
        closeActiveMenu();
        Platform::launchBrowser("https://github.com");
    });

    return menu;
}

// ============================================================================
// ToolOptionsBar implementation
// ============================================================================

ToolOptionsBar::ToolOptionsBar() {
    bgColor = Config::COLOR_PANEL;
    preferredSize = Vec2(0, Config::toolOptionsHeight());
    verticalPolicy = SizePolicy::Fixed;
    setPadding(4 * Config::uiScale);

    layout = createChild<HBoxLayout>(8 * Config::uiScale);

    // Build initial options (will be empty until tool is set)
    rebuildOptions();
}

void ToolOptionsBar::update() {
    AppState& state = getAppState();

    // If no active document, keep the bar empty
    if (!state.activeDocument) {
        if (currentToolType != -1) {
            clear();
        }
        return;
    }

    bool needsRebuild = false;

    if (state.currentToolType != currentToolType) {
        ToolType oldTool = static_cast<ToolType>(currentToolType);
        ToolType newTool = static_cast<ToolType>(state.currentToolType);
        currentToolType = state.currentToolType;

        // If switching between selection tools, just update the combo box
        if (isSelectionTool(oldTool) && isSelectionTool(newTool) && shapeCombo) {
            switch (newTool) {
                case ToolType::RectangleSelect: shapeCombo->selectedIndex = 0; break;
                case ToolType::EllipseSelect: shapeCombo->selectedIndex = 1; break;
                case ToolType::FreeSelect: shapeCombo->selectedIndex = 2; break;
                case ToolType::PolygonSelect: shapeCombo->selectedIndex = 3; break;
                default: break;
            }
            getAppState().needsRedraw = true;
        }
        // If switching between fill tools, check if we need rebuild for mode change
        else if (isFillTool(oldTool) && isFillTool(newTool)) {
            // Check if mode changed between solid (0) and gradient (1,2)
            bool wasGradient = lastFillMode > 0;
            bool isGradient = state.fillMode > 0;
            if (wasGradient != isGradient) {
                // Need rebuild to show/hide tolerance controls
                needsRebuild = true;
            } else if (fillModeCombo) {
                // Just update combo selection
                fillModeCombo->selectedIndex = state.fillMode;
                getAppState().needsRedraw = true;
            }
            lastFillMode = state.fillMode;
        } else {
            needsRebuild = true;
        }
    }

    // For Move tool, check if selection state changed
    if (static_cast<ToolType>(currentToolType) == ToolType::Move) {
        bool hasSelection = state.activeDocument && state.activeDocument->selection.hasSelection;
        if (hasSelection != lastHadSelection) {
            lastHadSelection = hasSelection;
            needsRebuild = true;
        }
    }

    // For Clone tool, sync sample mode checkbox with state
    if (static_cast<ToolType>(currentToolType) == ToolType::Clone) {
        if (sampleModeCheck && sampleModeCheck->checked != state.cloneSampleMode) {
            sampleModeCheck->checked = state.cloneSampleMode;
            state.needsRedraw = true;
        }
    }

    if (needsRebuild) {
        // Defer rebuild to avoid destroying widgets during their callbacks
        pendingRebuild = true;
    }
}

void ToolOptionsBar::applyPendingChanges() {
    if (pendingRebuild) {
        pendingRebuild = false;
        rebuildOptions();
    }
}

void ToolOptionsBar::clearOptions() {
    // Clear all widget references
    sizeSlider = nullptr;
    hardnessLabel = nullptr;
    hardnessSlider = nullptr;
    opacitySlider = nullptr;
    toleranceSlider = nullptr;
    contiguousCheck = nullptr;
    antiAliasCheck = nullptr;
    shapeCombo = nullptr;
    fillModeCombo = nullptr;
    curveBtn = nullptr;
    pressureCombo = nullptr;
    sampleModeCheck = nullptr;

    // Remove all children from layout
    layout->children.clear();
}

void ToolOptionsBar::clear() {
    currentToolType = -1;
    lastHadSelection = false;
    clearOptions();
}

void ToolOptionsBar::updateHardnessVisibility() {
    bool showHardness = getAppState().currentBrushTipIndex < 0;
    if (hardnessLabel) hardnessLabel->visible = showHardness;
    if (hardnessSlider) hardnessSlider->visible = showHardness;
    // Trigger layout recalculation
    if (layout) {
        layout->layout();
        getAppState().needsRedraw = true;
    }
}

void ToolOptionsBar::updateCurveVisibility() {
    bool showCurve = getAppState().brushPressureMode != 0;  // 0 = None
    if (curveBtn) curveBtn->visible = showCurve;
    // Trigger layout recalculation
    if (layout) {
        layout->layout();
        getAppState().needsRedraw = true;
    }
}

const char* ToolOptionsBar::getToolName(ToolType tool) {
    switch (tool) {
        case ToolType::Move: return "Move";
        case ToolType::RectangleSelect:
        case ToolType::EllipseSelect:
        case ToolType::FreeSelect:
        case ToolType::PolygonSelect: return "Select";
        case ToolType::MagicWand: return "Magic Wand";
        case ToolType::Brush: return "Brush";
        case ToolType::Eraser: return "Eraser";
        case ToolType::Fill:
        case ToolType::Gradient: return "Fill";
        case ToolType::Clone: return "Clone Stamp";
        case ToolType::Heal: return "Heal";
        case ToolType::Smudge: return "Smudge";
        case ToolType::Dodge: return "Dodge";
        case ToolType::Burn: return "Burn";
        case ToolType::ColorPicker: return "Color Picker";
        case ToolType::Pan: return "Pan";
        case ToolType::Zoom: return "Zoom";
        case ToolType::Crop: return "Crop";
        default: return "Tool";
    }
}

void ToolOptionsBar::rebuildOptions() {
    clearOptions();

    ToolType tool = static_cast<ToolType>(currentToolType);

    // Add tool name label (dynamically sized)
    auto* toolLabel = addLabel(getToolName(tool));
    toolLabel->textColor = Config::COLOR_TEXT;

    // Add separator after tool name
    layout->createChild<Separator>(false);
    addItemSpacing();

    switch (tool) {
        case ToolType::Brush:
            buildBrushOptions();
            break;
        case ToolType::Eraser:
            buildEraserOptions();
            break;

        case ToolType::Clone:
            buildCloneOptions();
            break;

        case ToolType::Smudge:
            buildSmudgeOptions();
            break;

        case ToolType::Dodge:
        case ToolType::Burn:
            buildDodgeBurnOptions();
            break;

        case ToolType::Fill:
        case ToolType::Gradient:
            buildFillOptions();
            break;

        case ToolType::RectangleSelect:
        case ToolType::EllipseSelect:
        case ToolType::FreeSelect:
        case ToolType::PolygonSelect:
            buildSelectionOptions();
            break;

        case ToolType::MagicWand:
            buildMagicWandOptions();
            break;

        case ToolType::ColorPicker:
            buildColorPickerOptions();
            break;

        case ToolType::Move:
            buildMoveOptions();
            break;

        case ToolType::Zoom:
            buildZoomOptions();
            break;

        case ToolType::Pan:
            buildPanOptions();
            break;

        case ToolType::Crop:
            buildCropOptions();
            break;

        default:
            // No additional options for these tools
            break;
    }

    // Always add trailing spacer
    layout->createChild<Spacer>();

    // Force relayout
    layout->layout();
    getAppState().needsRedraw = true;
}

void ToolOptionsBar::buildBrushOptions() {
    AppState& state = getAppState();

    // Tip button
    auto* tipBtn = addButton("Tip");
    tipBtn->onClick = [this, tipBtn]() {
        if (onOpenBrushTipPopup) {
            Rect btnBounds = tipBtn->globalBounds();
            onOpenBrushTipPopup(btnBounds.x, btnBounds.bottom());
        }
    };

    addItemSpacing();

    // Manage brushes button
    auto* manageBtn = addButton("Manage");
    manageBtn->onClick = [this, manageBtn]() {
        if (onOpenManageBrushesPopup) {
            Rect btnBounds = manageBtn->globalBounds();
            onOpenManageBrushesPopup(btnBounds.x, btnBounds.bottom());
        }
    };

    addItemSpacing();

    // Size
    addLabel("Size");
    sizeSlider = addNumberSlider(Config::MIN_BRUSH_SIZE, Config::MAX_BRUSH_SIZE, state.brushSize, 0, 50);
    sizeSlider->suffix = "px";
    sizeSlider->onChanged = [](f32 value) { getAppState().brushSize = value; };

    addGroupSpacing();

    // Opacity
    addLabel("Opacity");
    opacitySlider = addNumberSlider(0.0f, 100.0f, state.brushOpacity * 100.0f, 0, 45);
    opacitySlider->suffix = "%";
    opacitySlider->onChanged = [](f32 value) { getAppState().brushOpacity = value / 100.0f; };

    addGroupSpacing();

    // Flow
    addLabel("Flow");
    auto* flowSlider = addNumberSlider(0.0f, 100.0f, state.brushFlow * 100.0f, 0, 45);
    flowSlider->suffix = "%";
    flowSlider->onChanged = [](f32 value) { getAppState().brushFlow = value / 100.0f; };

    addGroupSpacing();

    // Hardness (only visible for round brush)
    hardnessLabel = addLabel("Hardness");
    hardnessSlider = addNumberSlider(0.0f, 100.0f, state.brushHardness * 100.0f, 0, 45);
    hardnessSlider->suffix = "%";
    hardnessSlider->onChanged = [](f32 value) { getAppState().brushHardness = value / 100.0f; };

    // Hide hardness controls if custom tip is selected
    bool showHardness = state.currentBrushTipIndex < 0;
    hardnessLabel->visible = showHardness;
    hardnessSlider->visible = showHardness;

    addGroupSpacing();

    // Pressure sensitivity
    addLabel("Pressure");
    pressureCombo = addComboBox({"None", "Size", "Opacity", "Flow"}, state.brushPressureMode);
    pressureCombo->onSelectionChanged = [this](i32 index) {
        getAppState().brushPressureMode = index;
        updateCurveVisibility();
    };

    addItemSpacing();

    // Pressure curve button
    curveBtn = addButton("Curve");
    curveBtn->onClick = [this]() {
        if (onOpenPressureCurvePopup) {
            Rect btnBounds = curveBtn->globalBounds();
            onOpenPressureCurvePopup(btnBounds.right(), btnBounds.bottom());
        }
    };
    curveBtn->visible = state.brushPressureMode != 0;
}

void ToolOptionsBar::buildCropOptions() {
    auto* applyBtn = addButton("Apply");
    applyBtn->onClick = [this]() {
        if (onCropApply) onCropApply();
    };

    addGroupSpacing();

    auto* resetBtn = addButton("Reset");
    resetBtn->onClick = [this]() {
        if (onCropReset) onCropReset();
    };
}

void ToolOptionsBar::buildPanOptions() {
    auto* fitBtn = addButton("Fit");
    fitBtn->onClick = [this]() {
        if (onFitToScreen) onFitToScreen();
    };
}

void ToolOptionsBar::buildEraserOptions() {
    AppState& state = getAppState();

    // Size
    addLabel("Size");
    sizeSlider = addNumberSlider(Config::MIN_BRUSH_SIZE, Config::MAX_BRUSH_SIZE, state.brushSize, 0, 50);
    sizeSlider->suffix = "px";
    sizeSlider->onChanged = [](f32 value) { getAppState().brushSize = value; };

    addGroupSpacing();

    // Hardness
    addLabel("Hard");
    hardnessSlider = addNumberSlider(0.0f, 100.0f, state.brushHardness * 100.0f, 0, 45);
    hardnessSlider->suffix = "%";
    hardnessSlider->onChanged = [](f32 value) { getAppState().brushHardness = value / 100.0f; };

    addGroupSpacing();

    // Opacity
    addLabel("Opacity");
    opacitySlider = addNumberSlider(0.0f, 100.0f, state.brushOpacity * 100.0f, 0, 45);
    opacitySlider->suffix = "%";
    opacitySlider->onChanged = [](f32 value) { getAppState().brushOpacity = value / 100.0f; };

    addGroupSpacing();

    // Flow
    addLabel("Flow");
    auto* flowSlider = addNumberSlider(0.0f, 100.0f, state.brushFlow * 100.0f, 0, 45);
    flowSlider->suffix = "%";
    flowSlider->onChanged = [](f32 value) { getAppState().brushFlow = value / 100.0f; };

    addGroupSpacing();

    // Pressure sensitivity
    addLabel("Pressure");
    pressureCombo = addComboBox({"None", "Size", "Opacity", "Flow"}, state.eraserPressureMode);
    pressureCombo->onSelectionChanged = [this](i32 index) {
        getAppState().eraserPressureMode = index;
        updateEraserCurveVisibility();
    };

    addItemSpacing();

    // Pressure curve button
    curveBtn = addButton("Curve");
    curveBtn->onClick = [this]() {
        if (onOpenPressureCurvePopup) {
            Rect btnBounds = curveBtn->globalBounds();
            onOpenPressureCurvePopup(btnBounds.right(), btnBounds.bottom());
        }
    };
    curveBtn->visible = state.eraserPressureMode != 0;
}

void ToolOptionsBar::updateEraserCurveVisibility() {
    bool showCurve = getAppState().eraserPressureMode != 0;  // 0 = None
    if (curveBtn) curveBtn->visible = showCurve;
    // Trigger layout recalculation
    if (layout) {
        layout->layout();
        getAppState().needsRedraw = true;
    }
}

void ToolOptionsBar::buildDodgeBurnOptions() {
    AppState& state = getAppState();

    // Size
    addLabel("Size");
    sizeSlider = addNumberSlider(Config::MIN_BRUSH_SIZE, Config::MAX_BRUSH_SIZE, state.brushSize, 0, 50);
    sizeSlider->suffix = "px";
    sizeSlider->onChanged = [](f32 value) { getAppState().brushSize = value; };

    addGroupSpacing();

    // Hardness
    addLabel("Hard");
    hardnessSlider = addNumberSlider(0.0f, 100.0f, state.brushHardness * 100.0f, 0, 45);
    hardnessSlider->suffix = "%";
    hardnessSlider->onChanged = [](f32 value) { getAppState().brushHardness = value / 100.0f; };

    addGroupSpacing();

    // Exposure
    addLabel("Exposure");
    opacitySlider = addNumberSlider(0.0f, 100.0f, state.brushOpacity * 100.0f, 0, 45);
    opacitySlider->suffix = "%";
    opacitySlider->onChanged = [](f32 value) { getAppState().brushOpacity = value / 100.0f; };

    addGroupSpacing();

    // Flow
    addLabel("Flow");
    auto* flowSlider = addNumberSlider(0.0f, 100.0f, state.brushFlow * 100.0f, 0, 45);
    flowSlider->suffix = "%";
    flowSlider->onChanged = [](f32 value) { getAppState().brushFlow = value / 100.0f; };

    addGroupSpacing();

    // Pressure sensitivity
    addLabel("Pressure");
    pressureCombo = addComboBox({"None", "Size", "Exposure", "Flow"}, state.dodgeBurnPressureMode);
    pressureCombo->onSelectionChanged = [this](i32 index) {
        getAppState().dodgeBurnPressureMode = index;
        updateDodgeBurnCurveVisibility();
    };

    addGroupSpacing();

    // Pressure curve button
    curveBtn = addButton("Curve");
    curveBtn->onClick = [this]() {
        if (onOpenPressureCurvePopup) {
            Rect btnBounds = curveBtn->globalBounds();
            onOpenPressureCurvePopup(btnBounds.right(), btnBounds.bottom());
        }
    };

    updateDodgeBurnCurveVisibility();
}

void ToolOptionsBar::updateDodgeBurnCurveVisibility() {
    bool showCurve = getAppState().dodgeBurnPressureMode != 0;  // 0 = None
    if (curveBtn) curveBtn->visible = showCurve;
}

void ToolOptionsBar::buildZoomOptions() {
    AppState& state = getAppState();

    // Click mode dropdown
    addLabel("Click");
    auto* clickModeCombo = addComboBox({"Zoom In", "Zoom Out"}, state.zoomClickMode);
    clickModeCombo->onSelectionChanged = [](i32 index) {
        getAppState().zoomClickMode = index;
    };

    addGroupSpacing();

    // Fit to screen button
    auto* fitBtn = addButton("Fit");
    fitBtn->onClick = [this]() {
        if (onFitToScreen) onFitToScreen();
    };
}

void ToolOptionsBar::buildCloneOptions() {
    AppState& state = getAppState();

    // Sample mode checkbox
    sampleModeCheck = addCheckbox("Sample");
    sampleModeCheck->checked = state.cloneSampleMode;
    sampleModeCheck->onChanged = [](bool checked) {
        getAppState().cloneSampleMode = checked;
    };

    addGroupSpacing();

    // Size
    addLabel("Size");
    sizeSlider = addNumberSlider(Config::MIN_BRUSH_SIZE, Config::MAX_BRUSH_SIZE, state.brushSize, 0, 50);
    sizeSlider->suffix = "px";
    sizeSlider->onChanged = [](f32 value) { getAppState().brushSize = value; };

    addGroupSpacing();

    // Hardness
    addLabel("Hard");
    hardnessSlider = addNumberSlider(0.0f, 100.0f, state.brushHardness * 100.0f, 0, 45);
    hardnessSlider->suffix = "%";
    hardnessSlider->onChanged = [](f32 value) { getAppState().brushHardness = value / 100.0f; };

    addGroupSpacing();

    // Opacity
    addLabel("Opacity");
    opacitySlider = addNumberSlider(0.0f, 100.0f, state.brushOpacity * 100.0f, 0, 45);
    opacitySlider->suffix = "%";
    opacitySlider->onChanged = [](f32 value) { getAppState().brushOpacity = value / 100.0f; };

    addGroupSpacing();

    // Flow
    addLabel("Flow");
    auto* flowSlider = addNumberSlider(0.0f, 100.0f, state.brushFlow * 100.0f, 0, 45);
    flowSlider->suffix = "%";
    flowSlider->onChanged = [](f32 value) { getAppState().brushFlow = value / 100.0f; };

    addGroupSpacing();

    // Pressure sensitivity
    addLabel("Pressure");
    pressureCombo = addComboBox({"None", "Size", "Opacity", "Flow"}, state.clonePressureMode);
    pressureCombo->onSelectionChanged = [this](i32 index) {
        getAppState().clonePressureMode = index;
        updateCloneCurveVisibility();
    };

    addItemSpacing();

    // Pressure curve button
    curveBtn = addButton("Curve");
    curveBtn->onClick = [this]() {
        if (onOpenPressureCurvePopup) {
            Rect btnBounds = curveBtn->globalBounds();
            onOpenPressureCurvePopup(btnBounds.right(), btnBounds.bottom());
        }
    };
    curveBtn->visible = state.clonePressureMode != 0;
}

void ToolOptionsBar::updateCloneCurveVisibility() {
    bool showCurve = getAppState().clonePressureMode != 0;  // 0 = None
    if (curveBtn) curveBtn->visible = showCurve;
    // Trigger layout recalculation
    if (layout) {
        layout->layout();
        getAppState().needsRedraw = true;
    }
}

void ToolOptionsBar::buildSmudgeOptions() {
    AppState& state = getAppState();

    // Size
    addLabel("Size");
    sizeSlider = addNumberSlider(Config::MIN_BRUSH_SIZE, Config::MAX_BRUSH_SIZE, state.brushSize, 0, 50);
    sizeSlider->suffix = "px";
    sizeSlider->onChanged = [](f32 value) { getAppState().brushSize = value; };

    addGroupSpacing();

    // Hardness
    addLabel("Hard");
    hardnessSlider = addNumberSlider(0.0f, 100.0f, state.brushHardness * 100.0f, 0, 45);
    hardnessSlider->suffix = "%";
    hardnessSlider->onChanged = [](f32 value) { getAppState().brushHardness = value / 100.0f; };

    addGroupSpacing();

    // Strength
    addLabel("Strength");
    opacitySlider = addNumberSlider(0.0f, 100.0f, state.brushOpacity * 100.0f, 0, 45);
    opacitySlider->suffix = "%";
    opacitySlider->onChanged = [](f32 value) { getAppState().brushOpacity = value / 100.0f; };

    addGroupSpacing();

    // Flow
    addLabel("Flow");
    auto* flowSlider = addNumberSlider(0.0f, 100.0f, state.brushFlow * 100.0f, 0, 45);
    flowSlider->suffix = "%";
    flowSlider->onChanged = [](f32 value) { getAppState().brushFlow = value / 100.0f; };

    addGroupSpacing();

    // Pressure sensitivity
    addLabel("Pressure");
    pressureCombo = addComboBox({"None", "Size", "Strength", "Flow"}, state.smudgePressureMode);
    pressureCombo->onSelectionChanged = [this](i32 index) {
        getAppState().smudgePressureMode = index;
        updateSmudgeCurveVisibility();
    };

    addItemSpacing();

    // Pressure curve button
    curveBtn = addButton("Curve");
    curveBtn->onClick = [this]() {
        if (onOpenPressureCurvePopup) {
            Rect btnBounds = curveBtn->globalBounds();
            onOpenPressureCurvePopup(btnBounds.right(), btnBounds.bottom());
        }
    };
    curveBtn->visible = state.smudgePressureMode != 0;
}

void ToolOptionsBar::updateSmudgeCurveVisibility() {
    bool showCurve = getAppState().smudgePressureMode != 0;  // 0 = None
    if (curveBtn) curveBtn->visible = showCurve;
    // Trigger layout recalculation
    if (layout) {
        layout->layout();
        getAppState().needsRedraw = true;
    }
}

void ToolOptionsBar::buildFillOptions() {
    AppState& state = getAppState();
    lastFillMode = state.fillMode;

    // Mode dropdown
    fillModeCombo = addComboBox({"Solid Fill", "Linear Gradient", "Radial Gradient"}, state.fillMode);
    fillModeCombo->onSelectionChanged = [this](i32 index) {
        AppState& s = getAppState();
        s.fillMode = index;
        if (onSelectTool) {
            onSelectTool(index == 0 ? ToolType::Fill : ToolType::Gradient);
        }
    };

    addGroupSpacing();

    // Tolerance and Contiguous only shown for Solid Fill mode
    if (state.fillMode == 0) {
        addLabel("Tolerance");
        toleranceSlider = addNumberSlider(0.0f, 510.0f, state.fillTolerance, 0, 50);
        toleranceSlider->onChanged = [](f32 value) {
            getAppState().fillTolerance = value;
        };

        addGroupSpacing();

        contiguousCheck = addCheckbox("Contiguous");
        contiguousCheck->checked = state.fillContiguous;
        contiguousCheck->onChanged = [](bool checked) {
            getAppState().fillContiguous = checked;
        };
    }
}

void ToolOptionsBar::buildGradientOptions() {
    addLabel("Gradient");

    addGroupSpacing();

    // Opacity
    AppState& state = getAppState();
    addLabel("Opacity");
    opacitySlider = addNumberSlider(0.0f, 100.0f, state.brushOpacity * 100.0f, 0, 45);
    opacitySlider->suffix = "%";
    opacitySlider->onChanged = [](f32 value) { getAppState().brushOpacity = value / 100.0f; };
}

void ToolOptionsBar::buildMoveOptions() {
    AppState& state = getAppState();
    bool hasSelection = state.activeDocument && state.activeDocument->selection.hasSelection;
    lastHadSelection = hasSelection;

    if (hasSelection) {
        // Selection mode: show "Move content" checkbox
        auto* moveContentCheck = addCheckbox("Move content");
        moveContentCheck->checked = state.moveSelectionContent;
        moveContentCheck->onChanged = [](bool checked) {
            getAppState().moveSelectionContent = checked;
        };
    } else {
        // Layer transform mode: show corner behavior dropdown
        addLabel("Corners");
        auto* cornerCombo = addComboBox({"Rotate", "Scale"}, 0);
        cornerCombo->onSelectionChanged = [](i32 index) {
            Document* doc = getAppState().activeDocument;
            if (!doc) return;

            Tool* tool = doc->getTool();
            if (!tool || tool->type != ToolType::Move) return;

            MoveTool* moveTool = static_cast<MoveTool*>(tool);
            moveTool->cornerBehavior = (index == 0) ? CornerBehavior::Rotate : CornerBehavior::Scale;
        };
    }
}

void ToolOptionsBar::buildSelectionOptions() {
    ToolType tool = static_cast<ToolType>(currentToolType);

    // Shape dropdown
    i32 shapeIndex = 0;
    switch (tool) {
        case ToolType::RectangleSelect: shapeIndex = 0; break;
        case ToolType::EllipseSelect: shapeIndex = 1; break;
        case ToolType::FreeSelect: shapeIndex = 2; break;
        case ToolType::PolygonSelect: shapeIndex = 3; break;
        default: shapeIndex = 0; break;
    }
    shapeCombo = addComboBox({"Rectangle", "Ellipse", "Free", "Polygon"}, shapeIndex);
    shapeCombo->onSelectionChanged = [this](i32 index) {
        if (!onSelectTool) return;
        switch (index) {
            case 0: onSelectTool(ToolType::RectangleSelect); break;
            case 1: onSelectTool(ToolType::EllipseSelect); break;
            case 2: onSelectTool(ToolType::FreeSelect); break;
            case 3: onSelectTool(ToolType::PolygonSelect); break;
        }
    };

    addGroupSpacing();

    // Anti-alias
    antiAliasCheck = addCheckbox("Anti-alias");
    antiAliasCheck->checked = getAppState().selectionAntiAlias;
    antiAliasCheck->onChanged = [](bool checked) {
        getAppState().selectionAntiAlias = checked;
    };
}

void ToolOptionsBar::buildMagicWandOptions() {
    AppState& state = getAppState();

    // Tolerance
    addLabel("Tolerance");
    toleranceSlider = addNumberSlider(0.0f, 510.0f, state.wandTolerance, 0, 50);
    toleranceSlider->onChanged = [](f32 value) {
        getAppState().wandTolerance = value;
    };

    addGroupSpacing();

    // Contiguous
    contiguousCheck = addCheckbox("Contiguous");
    contiguousCheck->checked = state.wandContiguous;
    contiguousCheck->onChanged = [](bool checked) {
        getAppState().wandContiguous = checked;
    };

    addGroupSpacing();

    // Anti-alias
    antiAliasCheck = addCheckbox("Anti-alias");
    antiAliasCheck->checked = state.selectionAntiAlias;
    antiAliasCheck->onChanged = [](bool checked) {
        getAppState().selectionAntiAlias = checked;
    };
}

void ToolOptionsBar::buildColorPickerOptions() {
    AppState& state = getAppState();

    addLabel("Sample");
    auto* sampleCombo = addComboBox({"Current Layer", "Current & Below", "All Layers"}, state.colorPickerSampleMode);
    sampleCombo->onSelectionChanged = [](i32 index) {
        getAppState().colorPickerSampleMode = index;
    };
}

// ============================================================================
// MainWindow implementation
// ============================================================================

MainWindow::MainWindow() {
    buildUI();
    createDialogs();

    // Set up brush tip popup callback to update hardness visibility
    // (must be after both buildUI and createDialogs)
    brushTipPopup->onTipChanged = [this]() {
        toolOptions->updateHardnessVisibility();
    };

    // Register for active document changes (for Ctrl+O and other shortcuts)
    getAppState().onActiveDocumentChanged = [this]() {
        connectToDocument();
    };
}

void MainWindow::createDialogs() {
    // New document dialog
    newDocDialog = createChild<NewDocumentDialog>();
    newDocDialog->onConfirm = [this](const std::string& name, u32 width, u32 height) {
        AppState& state = getAppState();
        Document* doc = state.createDocument(width, height, name);
        if (doc) {
            connectToDocument();
            // Fit new document to screen
            if (docView) {
                docView->view.zoomToFit();
            }
        }
    };

    // Canvas size dialog
    canvasSizeDialog = createChild<CanvasSizeDialog>();
    canvasSizeDialog->onConfirm = [this](u32 width, u32 height, i32 anchorX, i32 anchorY, CanvasResizeMode mode) {
        Document* doc = getAppState().activeDocument;
        if (doc) {
            doc->resizeCanvas(width, height, anchorX, anchorY, mode);
            // Fit to screen after resize
            if (docView) {
                docView->view.zoomToFit();
            }
            getAppState().needsRedraw = true;
        }
    };

    // Rename document dialog
    renameDocDialog = createChild<RenameDocumentDialog>();
    renameDocDialog->onConfirm = [this](const std::string& newName) {
        Document* doc = getAppState().activeDocument;
        if (doc) {
            doc->name = newName;
            syncTabs();
            getAppState().needsRedraw = true;
        }
    };

    // Color picker dialog
    colorPickerDialog = createChild<ColorPickerDialog>();
    colorPickerDialog->onColorSelected = [this](const Color& c) {
        if (editingForegroundColor) {
            getAppState().foregroundColor = c;
        } else {
            getAppState().backgroundColor = c;
        }
        toolPalette->updateColors();
        getAppState().needsRedraw = true;
    };

    // Pressure curve popup
    pressureCurvePopup = createChild<PressureCurvePopup>();

    // New brush dialog
    newBrushDialog = createChild<NewBrushDialog>();
    newBrushDialog->onBrushCreated = [this](std::unique_ptr<CustomBrushTip> tip) {
        if (tip) {
            AppState& state = getAppState();
            state.brushLibrary.addTip(std::move(tip));
            // Select the new tip
            state.currentBrushTipIndex = static_cast<i32>(state.brushLibrary.count()) - 1;
            state.needsRedraw = true;
            // Update tool options UI (hide hardness for custom brush)
            toolOptions->updateHardnessVisibility();
        }
    };

    // Manage brushes popup
    manageBrushesPopup = createChild<ManageBrushesPopup>();
    manageBrushesPopup->onNewFromFile = [this]() { showNewBrushDialog(false); };
    manageBrushesPopup->onNewFromCanvas = [this]() { showNewBrushDialog(true); };
    manageBrushesPopup->onBrushDeleted = [this]() {
        // Update tool options UI (show hardness for round brush)
        toolOptions->updateHardnessVisibility();
    };

    // Brush tip selector popup
    brushTipPopup = createChild<BrushTipSelectorPopup>();

    // About dialog
    aboutDialog = createChild<AboutDialog>();
}

void MainWindow::buildUI() {
    auto mainLayout = createChild<VBoxLayout>(0);
    mainLayout->stretch = true;

    // Menu bar
    menuBar = mainLayout->createChild<MenuBar>();

    // Tool options
    toolOptions = mainLayout->createChild<ToolOptionsBar>();

    // Main content area
    auto contentLayout = mainLayout->createChild<HBoxLayout>(0);
    contentLayout->verticalPolicy = SizePolicy::Expanding;

    // Left: Tool palette
    toolPalette = contentLayout->createChild<ToolPalette>();

    // Center: Tab bar and document view
    auto centerArea = contentLayout->createChild<VBoxLayout>(0);
    centerArea->horizontalPolicy = SizePolicy::Expanding;
    centerArea->verticalPolicy = SizePolicy::Expanding;

    tabBar = centerArea->createChild<TabBar>();
    docView = centerArea->createChild<DocumentViewWidget>();

    // Resize divider between center and right sidebar
    sidebarDivider = contentLayout->createChild<ResizeDivider>();

    // Right: Sidebar
    rightSidebar = contentLayout->createChild<VBoxLayout>(0);
    rightSidebar->preferredSize = Vec2(Config::rightSidebarWidth(), 0);
    rightSidebar->horizontalPolicy = SizePolicy::Fixed;

    // Connect divider to sidebar
    sidebarDivider->targetWidget = rightSidebar;
    sidebarDivider->onResized = [this]() { layout(); };

    navigatorPanel = rightSidebar->createChild<NavigatorPanel>();

    // Resizer between navigator and layer props
    navPropsResizer = rightSidebar->createChild<VPanelResizer>();
    navPropsResizer->aboveWidget = navigatorPanel;
    navPropsResizer->onResized = [this]() { layout(); };

    layerPropsPanel = rightSidebar->createChild<LayerPropsPanel>();
    navPropsResizer->belowWidget = layerPropsPanel;
    layerPropsPanel->onRequestColorPicker = [this](const Color& initialColor, std::function<void(const Color&)> callback) {
        colorPickerDialog->setColor(initialColor);
        colorPickerDialog->onColorSelected = callback;
        centerDialog(colorPickerDialog);
        colorPickerDialog->show();
        OverlayManager::instance().registerOverlay(colorPickerDialog, ZOrder::MODAL_DIALOG, true);
    };
    layerPropsPanel->onRequestLoadFont = [this](std::function<void(const std::string&, std::vector<u8>&)> callback) {
        // Defer file dialog to next frame to avoid X11 mouse grab issues on Linux
        getAppState().requestFileDialog("Load Font", "*.ttf *.otf", [callback](const std::string& path) {
            if (path.empty()) return;

            // Read font file using platform abstraction
            std::vector<u8> fontData = Platform::readFile(path);
            if (fontData.empty()) return;

            // Extract filename from path
            std::string fontName = Platform::getFileName(path);

            // Call the callback with font name and data
            callback(fontName, fontData);
        });
    };

    // Resizer between layer props and layer list
    propsLayerResizer = rightSidebar->createChild<VPanelResizer>();
    propsLayerResizer->aboveWidget = layerPropsPanel;
    propsLayerResizer->onResized = [this]() { layout(); };

    layerPanel = rightSidebar->createChild<LayerPanel>();
    propsLayerResizer->belowWidget = layerPanel;

    // Status bar
    statusBar = mainLayout->createChild<StatusBar>();
    statusBar->onFitToScreen = [this]() {
        if (docView) {
            docView->view.zoomToFit();
            getAppState().needsRedraw = true;
        }
    };

    // Set up menu callbacks
    menuBar->onNewDocument = [this]() { showNewDocumentDialog(); };
    menuBar->onCanvasSize = [this]() { showCanvasSizeDialog(); };
    menuBar->onFitToScreen = [this]() {
        if (docView) {
            docView->view.zoomToFit();
            getAppState().needsRedraw = true;
        }
    };
    menuBar->onRenameDocument = [this]() { showRenameDocumentDialog(); };
    menuBar->onAbout = [this]() { showAboutDialog(); };

    // Set up tool change callback
    toolPalette->onToolChanged = [this](ToolType) {
        toolOptions->update();
    };

    // Set up tool options bar callback to switch tools (for selection shape dropdown)
    toolOptions->onSelectTool = [this](ToolType type) {
        // Switch the document's tool
        toolPalette->selectTool(type);
        // Keep the parent button selected (M for selection types, G for fill/gradient)
        // Don't rebuild options since we're just switching sub-type
    };

    // Double-click on Zoom tool resets zoom to 100%
    toolPalette->onZoomReset = [this]() {
        if (docView) {
            docView->view.zoomTo100();
            getAppState().needsRedraw = true;
        }
    };

    // Double-click on Hand tool resets view (zoom and center)
    toolPalette->onViewReset = [this]() {
        if (docView) {
            docView->view.zoomToFit();
            getAppState().needsRedraw = true;
        }
    };

    // Set up color swatch callback (on tool palette)
    toolPalette->onColorSwatchClicked = [this](bool foreground) {
        editingForegroundColor = foreground;
        Color initialColor = foreground ? getAppState().foregroundColor : getAppState().backgroundColor;
        showColorPickerDialog(initialColor);
    };

    // Set up pressure curve popup callback
    toolOptions->onOpenPressureCurvePopup = [this](f32 x, f32 y) {
        showPressureCurvePopup(x, y);
    };

    // Set up brush tip popup callback
    toolOptions->onOpenBrushTipPopup = [this](f32 x, f32 y) {
        showBrushTipPopup(x, y);
    };

    // Set up manage brushes popup callback
    toolOptions->onOpenManageBrushesPopup = [this](f32 x, f32 y) {
        showManageBrushesPopup(x, y);
    };

    // Set up fit to screen callback
    toolOptions->onFitToScreen = [this]() {
        if (docView) {
            docView->view.zoomToFit();
            getAppState().needsRedraw = true;
        }
    };

    // Set up crop tool callbacks
    toolOptions->onCropApply = [this]() {
        AppState& state = getAppState();
        if (state.activeDocument) {
            Tool* tool = state.activeDocument->getTool();
            if (tool && tool->type == ToolType::Crop) {
                CropTool* cropTool = static_cast<CropTool*>(tool);
                cropTool->apply(*state.activeDocument);
                // Fit to screen so the new canvas is visible
                if (docView) {
                    docView->view.zoomToFit();
                }
            }
        }
    };
    toolOptions->onCropReset = [this]() {
        AppState& state = getAppState();
        if (state.activeDocument) {
            Tool* tool = state.activeDocument->getTool();
            if (tool && tool->type == ToolType::Crop) {
                CropTool* cropTool = static_cast<CropTool*>(tool);
                cropTool->reset(*state.activeDocument);
            }
        }
    };

    // Set up tab bar callbacks
    tabBar->onTabSelected = [this](i32 index) {
        switchToDocument(index);
    };

    tabBar->onTabClosed = [this](i32 index) {
        closeDocumentTab(index);
    };

    // Connect to app state
    connectToDocument();
}

void MainWindow::applyDeferredChanges() {
    if (toolOptions) {
        toolOptions->update();  // Sync checkbox states, etc.
        toolOptions->applyPendingChanges();
    }
}

void MainWindow::connectToDocument() {
    AppState& state = getAppState();
    Document* doc = state.activeDocument;

    // Sync tab bar with documents
    syncTabs();

    // Update UI enabled state based on whether we have a document
    bool hasDocument = (doc != nullptr);
    toolPalette->setEnabled(hasDocument);
    navigatorPanel->setEnabled(hasDocument);
    layerPropsPanel->setEnabled(hasDocument);
    layerPanel->setEnabled(hasDocument);
    menuBar->setDocumentMenusEnabled(hasDocument);
    statusBar->setEnabled(hasDocument);

    if (doc) {
        docView->setDocument(doc);
        navigatorPanel->setView(&docView->view);
        layerPropsPanel->setDocument(doc);
        layerPanel->setDocument(doc);

        // Set default tool if not already set
        if (!doc->getTool()) {
            toolPalette->selectTool(ToolType::Brush);
            if (!toolPalette->toolButtons.empty()) {
                // Select brush button (index 2)
                for (size_t i = 0; i < toolPalette->toolButtons.size(); ++i) {
                    toolPalette->toolButtons[i]->selected = (i == 2);
                }
            }
        }
    } else {
        docView->setDocument(nullptr);
        navigatorPanel->setView(nullptr);
        layerPropsPanel->setDocument(nullptr);
        layerPanel->setDocument(nullptr);
        // Clear tool selection and options bar when no document
        toolPalette->clearSelection();
        toolOptions->clear();
    }

    toolPalette->updateColors();
}

void MainWindow::syncTabs() {
    AppState& state = getAppState();

    // Rebuild tabs from documents
    tabBar->tabs.clear();
    for (size_t i = 0; i < state.documents.size(); ++i) {
        Document* doc = state.documents[i].get();
        tabBar->addTab(doc->name, doc, true);
    }

    // Set active tab
    tabBar->setActiveTab(state.activeDocumentIndex);
}

void MainWindow::switchToDocument(i32 index) {
    AppState& state = getAppState();
    if (index >= 0 && index < static_cast<i32>(state.documents.size())) {
        state.setActiveDocument(index);
        connectToDocument();
        state.needsRedraw = true;
    }
}

void MainWindow::closeDocumentTab(i32 index) {
    AppState& state = getAppState();
    if (index >= 0 && index < static_cast<i32>(state.documents.size())) {
        state.closeDocument(index);
        connectToDocument();
        state.needsRedraw = true;
    }
}

void MainWindow::addDocumentTab(Document* doc) {
    if (!doc) return;
    tabBar->addTab(doc->name, doc, true);
    tabBar->setActiveTab(static_cast<i32>(tabBar->tabs.size()) - 1);
}

void MainWindow::layout() {
    // Clamp sidebar width to fit in available space
    clampSidebarWidth();

    // Set children bounds based on main layout
    if (!children.empty()) {
        children[0]->setBounds(0, 0, bounds.w, bounds.h);
    }
    Widget::layout();

    // Reposition visible dialogs to keep them centered and on screen
    repositionDialogs();
}

void MainWindow::clampSidebarWidth() {
    if (!rightSidebar || !rightSidebar->visible) return;

    // Calculate available width for the sidebar
    // Total width minus: tool palette, divider, minimum center area
    f32 toolPaletteWidth = Config::toolPaletteWidth();
    f32 dividerWidth = sidebarDivider ? sidebarDivider->preferredSize.x : 0;
    f32 minCenterWidth = 200.0f;  // Minimum width for document view area

    f32 maxSidebarWidth = bounds.w - toolPaletteWidth - dividerWidth - minCenterWidth;

    // Ensure at least minimum sidebar width, but don't exceed available space
    f32 minSidebarWidth = sidebarDivider ? sidebarDivider->minWidth : 100.0f;
    maxSidebarWidth = std::max(minSidebarWidth, maxSidebarWidth);

    // Clamp current sidebar width
    if (rightSidebar->preferredSize.x > maxSidebarWidth) {
        rightSidebar->preferredSize.x = maxSidebarWidth;
    }

    // Also update the divider's maxWidth so dragging respects current window size
    if (sidebarDivider) {
        sidebarDivider->maxWidth = maxSidebarWidth;
    }
}

void MainWindow::repositionDialogs() {
    // Helper to ensure dialog stays within window bounds
    auto repositionDialog = [this](Dialog* dialog) {
        if (!dialog || !dialog->visible) return;

        // Clamp dialog size to window if needed
        f32 dialogW = std::min(dialog->preferredSize.x, bounds.w - 20);
        f32 dialogH = std::min(dialog->preferredSize.y, bounds.h - 20);

        // Center in window
        f32 x = (bounds.w - dialogW) / 2;
        f32 y = (bounds.h - dialogH) / 2;

        // Ensure not off-screen
        x = std::max(10.0f, x);
        y = std::max(10.0f, y);

        dialog->setBounds(x, y, dialogW, dialogH);
        dialog->layout();
    };

    repositionDialog(newDocDialog);
    repositionDialog(canvasSizeDialog);
    repositionDialog(colorPickerDialog);
    repositionDialog(newBrushDialog);
    repositionDialog(renameDocDialog);
    repositionDialog(aboutDialog);
    // Note: pressureCurvePopup and manageBrushesPopup are non-modal, positioned relative to button
}

Dialog* MainWindow::getActiveDialog() {
    if (newDocDialog && newDocDialog->visible) return newDocDialog;
    if (canvasSizeDialog && canvasSizeDialog->visible) return canvasSizeDialog;
    if (colorPickerDialog && colorPickerDialog->visible) return colorPickerDialog;
    if (newBrushDialog && newBrushDialog->visible) return newBrushDialog;
    if (renameDocDialog && renameDocDialog->visible) return renameDocDialog;
    if (aboutDialog && aboutDialog->visible) return aboutDialog;
    // Note: popups are non-modal, not included here
    return nullptr;
}

Recti MainWindow::getSelectionScreenBounds() const {
    if (!docView || !docView->view.document) {
        return Recti(0, 0, 0, 0);
    }

    const Document& doc = *docView->view.document;
    if (!doc.selection.hasSelection) {
        return Recti(0, 0, 0, 0);
    }

    // Convert selection bounds from document to screen coordinates
    const Recti& selBounds = doc.selection.bounds;
    Rect docRect(static_cast<f32>(selBounds.x), static_cast<f32>(selBounds.y),
                 static_cast<f32>(selBounds.w), static_cast<f32>(selBounds.h));

    Rect screenRect = docView->view.documentToScreen(docRect);

    // Add padding for marching ants line thickness
    i32 padding = std::max(4, static_cast<i32>(Config::uiScale + 0.5f));

    return Recti(
        static_cast<i32>(screenRect.x) - padding,
        static_cast<i32>(screenRect.y) - padding,
        static_cast<i32>(std::ceil(screenRect.w)) + padding * 2,
        static_cast<i32>(std::ceil(screenRect.h)) + padding * 2
    );
}

bool MainWindow::onMouseDown(const MouseEvent& e) {
    // Modal dialogs block all input below them
    // The OverlayManager handles the actual event routing
    Dialog* dialog = getActiveDialog();
    if (dialog) {
        // If click is outside dialog, we still block input (modal behavior)
        Rect dialogBounds = dialog->globalBounds();
        if (!dialogBounds.contains(e.globalPosition)) {
            return true;  // Block click outside modal
        }
    }
    return Widget::onMouseDown(e);
}

bool MainWindow::onMouseMove(const MouseEvent& e) {
    return Widget::onMouseMove(e);
}

void MainWindow::centerDialog(Dialog* dialog) {
    if (!dialog) return;
    f32 x = (bounds.w - dialog->preferredSize.x) / 2;
    f32 y = (bounds.h - dialog->preferredSize.y) / 2;
    dialog->setBounds(x, y, dialog->preferredSize.x, dialog->preferredSize.y);
    dialog->layout();
}

void MainWindow::showNewDocumentDialog() {
    centerDialog(newDocDialog);
    newDocDialog->show();
    OverlayManager::instance().registerOverlay(newDocDialog, ZOrder::MODAL_DIALOG, true);
}

void MainWindow::showCanvasSizeDialog() {
    centerDialog(canvasSizeDialog);
    canvasSizeDialog->show();
    OverlayManager::instance().registerOverlay(canvasSizeDialog, ZOrder::MODAL_DIALOG, true);
}

void MainWindow::showColorPickerDialog(const Color& initialColor) {
    colorPickerDialog->setColor(initialColor);
    centerDialog(colorPickerDialog);
    colorPickerDialog->show();
    OverlayManager::instance().registerOverlay(colorPickerDialog, ZOrder::MODAL_DIALOG, true);
}

void MainWindow::showAboutDialog() {
    centerDialog(aboutDialog);
    aboutDialog->show();
    OverlayManager::instance().registerOverlay(aboutDialog, ZOrder::MODAL_DIALOG, true);
}

void MainWindow::showPressureCurvePopup(f32 x, f32 y) {
    // Check if popup was just closed (within 100ms) - if so, don't reopen
    u64 now = Platform::getMilliseconds();
    if (now - pressureCurvePopupCloseTime < 100) {
        return;  // Popup was just closed by click-outside, don't reopen
    }

    // If already visible, close it
    if (pressureCurvePopup->visible) {
        pressureCurvePopup->hide();
        OverlayManager::instance().unregisterOverlay(pressureCurvePopup);
        pressureCurvePopupCloseTime = now;
        return;
    }

    pressureCurvePopup->show(x, y);
    OverlayManager::instance().registerOverlay(pressureCurvePopup, ZOrder::POPUP_MENU,
        [this]() {
            pressureCurvePopup->hide();
            pressureCurvePopupCloseTime = Platform::getMilliseconds();
        });
}

void MainWindow::showNewBrushDialog(bool fromCurrentCanvas) {
    newBrushDialog->fromCurrentCanvas = fromCurrentCanvas;
    if (fromCurrentCanvas) {
        // Load from active layer canvas
        Document* doc = getAppState().activeDocument;
        if (doc) {
            PixelLayer* layer = doc->getActivePixelLayer();
            if (layer) {
                newBrushDialog->loadFromCanvas(&layer->canvas, layer->canvas.width, layer->canvas.height);
            }
        }
    }
    centerDialog(newBrushDialog);
    newBrushDialog->show();
    OverlayManager::instance().registerOverlay(newBrushDialog, ZOrder::MODAL_DIALOG, true);
}

void MainWindow::showRenameDocumentDialog() {
    if (!getAppState().activeDocument) return;
    centerDialog(renameDocDialog);
    renameDocDialog->show();
    OverlayManager::instance().registerOverlay(renameDocDialog, ZOrder::MODAL_DIALOG, true);
}

void MainWindow::showManageBrushesPopup(f32 x, f32 y) {
    // Check if popup was just closed (within 100ms) - if so, don't reopen
    u64 now = Platform::getMilliseconds();
    if (now - manageBrushesPopupCloseTime < 100) {
        return;
    }

    // If already visible, close it
    if (manageBrushesPopup->visible) {
        manageBrushesPopup->hide();
        OverlayManager::instance().unregisterOverlay(manageBrushesPopup);
        manageBrushesPopupCloseTime = now;
        return;
    }

    // Left-align popup with button
    manageBrushesPopup->show(x, y);
    OverlayManager::instance().registerOverlay(manageBrushesPopup, ZOrder::POPUP_MENU,
        [this]() {
            manageBrushesPopup->hide();
            manageBrushesPopupCloseTime = Platform::getMilliseconds();
        });
}

void MainWindow::showBrushTipPopup(f32 x, f32 y) {
    // Check if popup was just closed (within 100ms) - if so, don't reopen
    u64 now = Platform::getMilliseconds();
    if (now - brushTipPopupCloseTime < 100) {
        return;  // Popup was just closed by click-outside, don't reopen
    }

    // If already visible, close it
    if (brushTipPopup->visible) {
        brushTipPopup->hide();
        OverlayManager::instance().unregisterOverlay(brushTipPopup);
        brushTipPopupCloseTime = now;
        return;
    }

    brushTipPopup->rebuild();
    brushTipPopup->show(x, y);
    OverlayManager::instance().registerOverlay(brushTipPopup, ZOrder::POPUP_MENU,
        [this]() {
            brushTipPopup->hide();
            brushTipPopupCloseTime = Platform::getMilliseconds();
        });
}

void MainWindow::render(Framebuffer& fb) {
    // Update color swatches from state (in case color picker changed them)
    if (toolPalette) {
        toolPalette->updateColors();
    }

    // Check for panel visibility changes (from View menu)
    AppState& state = getAppState();
    bool visibilityChanged = false;
    f32 panelMinHeight = 50.0f * Config::uiScale;  // Same as VPanelResizer::minHeight

    // Helper to make room for a panel by taking space from another panel
    auto makeRoomForPanel = [&](Widget* panelToShow, Widget* panelToShrink) {
        if (!panelToShow || !panelToShrink) return;
        // If the panel to shrink has Fixed policy and enough height, take some for the new panel
        if (panelToShrink->verticalPolicy == SizePolicy::Fixed &&
            panelToShrink->preferredSize.y > panelMinHeight * 2) {
            f32 spaceToTake = panelMinHeight;
            panelToShrink->preferredSize.y -= spaceToTake;
            panelToShow->preferredSize.y = spaceToTake;
            panelToShow->verticalPolicy = SizePolicy::Fixed;
        } else {
            // Just set the showing panel to expanding so layout gives it space
            panelToShow->verticalPolicy = SizePolicy::Expanding;
        }
    };

    if (state.showNavigator != prevShowNavigator) {
        if (navigatorPanel) {
            if (state.showNavigator) {
                // Take space from the next visible panel below
                if (state.showProperties && layerPropsPanel) {
                    makeRoomForPanel(navigatorPanel, layerPropsPanel);
                } else if (state.showLayers && layerPanel) {
                    makeRoomForPanel(navigatorPanel, layerPanel);
                }
            }
            navigatorPanel->visible = state.showNavigator;
        }
        prevShowNavigator = state.showNavigator;
        visibilityChanged = true;
    }
    if (state.showProperties != prevShowProperties) {
        if (layerPropsPanel) {
            if (state.showProperties) {
                // Take space from an adjacent visible panel
                if (state.showLayers && layerPanel) {
                    makeRoomForPanel(layerPropsPanel, layerPanel);
                } else if (state.showNavigator && navigatorPanel) {
                    makeRoomForPanel(layerPropsPanel, navigatorPanel);
                }
            }
            layerPropsPanel->visible = state.showProperties;
        }
        prevShowProperties = state.showProperties;
        visibilityChanged = true;
    }
    if (state.showLayers != prevShowLayers) {
        if (layerPanel) {
            if (state.showLayers) {
                // Take space from the panel above
                if (state.showProperties && layerPropsPanel) {
                    makeRoomForPanel(layerPanel, layerPropsPanel);
                } else if (state.showNavigator && navigatorPanel) {
                    makeRoomForPanel(layerPanel, navigatorPanel);
                }
            }
            layerPanel->visible = state.showLayers;
        }
        prevShowLayers = state.showLayers;
        visibilityChanged = true;
    }

    // Panel resizers - dynamically connect to visible panels
    // navPropsResizer: connects navigator to the next visible panel below it
    // propsLayerResizer: connects layer props to layers (only when props visible)
    if (navPropsResizer) {
        if (state.showNavigator && state.showProperties) {
            // Normal case: navigator -> props
            navPropsResizer->visible = true;
            navPropsResizer->belowWidget = layerPropsPanel;
        } else if (state.showNavigator && !state.showProperties && state.showLayers) {
            // Props hidden: navigator -> layers directly
            navPropsResizer->visible = true;
            navPropsResizer->belowWidget = layerPanel;
        } else {
            navPropsResizer->visible = false;
        }
    }
    if (propsLayerResizer) {
        // Only visible when properties is visible (otherwise navPropsResizer handles it)
        propsLayerResizer->visible = state.showProperties && state.showLayers;
    }

    // Show/hide sidebar and divider based on whether any panel is visible
    bool anySidebarVisible = state.showNavigator || state.showProperties || state.showLayers;
    if (rightSidebar) rightSidebar->visible = anySidebarVisible;
    if (sidebarDivider) sidebarDivider->visible = anySidebarVisible;

    if (visibilityChanged) {
        layout();  // Redistribute space among visible panels
    }

    // Update status bar with current state
    if (statusBar && docView && state.activeDocument) {
        Document* doc = state.activeDocument;
        statusBar->update(
            docView->lastMousePos,
            docView->view.zoom,
            doc->width,
            doc->height
        );
    }

    Widget::render(fb);
    // Note: Overlays (popups, dialogs) are rendered by OverlayManager in Application::render()
}
