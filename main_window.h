#ifndef _H_MAIN_WINDOW_
#define _H_MAIN_WINDOW_

#include "widget.h"
#include "layouts.h"
#include "basic_widgets.h"
#include "panels.h"
#include "document.h"
#include "document_view.h"
#include "compositor.h"
#include "app_state.h"
#include "tool.h"
#include "brush_tool.h"
#include "eraser_tool.h"
#include "fill_tool.h"
#include "selection_tools.h"
#include "transform_tools.h"
#include "retouch_tools.h"
#include "platform.h"
#include "image_io.h"
#include "project_file.h"
#include "dialogs.h"
#include "color_picker.h"
#include "brush_dialogs.h"
#include "overlay_manager.h"

// Document view widget - displays the canvas
class DocumentViewWidget : public Widget, public DocumentObserver {
public:
    DocumentView view;
    bool panning = false;
    bool zooming = false;
    bool zoomDragged = false;  // True if zoom drag moved significantly (not a click)
    MouseButton zoomButton = MouseButton::None;  // Which button started the zoom
    bool toolActive = false;  // True when a tool operation is in progress
    bool needsCentering = false;  // Flag to center document on first valid render
    bool mouseOverCanvas = false;  // True when mouse is over this widget
    Vec2 lastMousePos;      // Document coordinates for tool/status bar
    Vec2 panStartPos;       // Screen coordinates for panning
    Vec2 zoomStartPos;      // Screen coordinates for zoom drag start
    Vec2 zoomCenter;        // Screen point to zoom around
    f32 zoomStartLevel = 1.0f;

    DocumentViewWidget() {
        horizontalPolicy = SizePolicy::Expanding;
        verticalPolicy = SizePolicy::Expanding;
    }

    ~DocumentViewWidget() override {
        if (view.document) {
            view.document->removeObserver(this);
        }
    }

    void setDocument(Document* doc) {
        if (view.document) {
            view.document->removeObserver(this);
        }
        view.setDocument(doc);
        if (doc) {
            doc->addObserver(this);
            needsCentering = true;  // Center on first render with valid viewport
        }
    }

    void layout() override {
        // Get the document point at the current viewport center before layout changes
        Rect oldViewport = view.viewport;
        Vec2 centerDocPoint(0, 0);
        bool hasValidViewport = oldViewport.w > 0 && oldViewport.h > 0 && view.document;

        if (hasValidViewport) {
            Vec2 oldCenter(oldViewport.x + oldViewport.w / 2, oldViewport.y + oldViewport.h / 2);
            centerDocPoint = view.screenToDocument(oldCenter);
        }

        // Let parent handle the layout (updates bounds)
        Widget::layout();

        // Update viewport to new bounds
        Rect newViewport = globalBounds();
        view.viewport = newViewport;

        // Adjust pan to keep the same document point at the new center
        if (hasValidViewport && newViewport.w > 0 && newViewport.h > 0) {
            Vec2 newCenter(newViewport.x + newViewport.w / 2, newViewport.y + newViewport.h / 2);
            // Calculate where the document point would appear with current pan
            // docPos * zoom + pan + viewport.x = screenX
            // So: pan = screenX - docPos * zoom - viewport.x
            view.pan.x = newCenter.x - centerDocPoint.x * view.zoom - newViewport.x;
            view.pan.y = newCenter.y - centerDocPoint.y * view.zoom - newViewport.y;
        }
    }

    void renderSelf(Framebuffer& fb) override {
        Rect global = globalBounds();
        view.viewport = global;

        // Fit document to screen on first valid render
        if (needsCentering && view.document && global.w > 0 && global.h > 0) {
            view.zoomToFit();
            needsCentering = false;
            getAppState().needsRedraw = true;
        }

        // Draw background
        fb.fillRect(Recti(global), Config::COLOR_BACKGROUND);

        if (view.document) {
            Compositor::compositeDocument(fb, *view.document, view.viewport,
                                          view.zoom, view.pan);

            // Draw tool overlay (clipped to canvas viewport)
            // Most tools only show overlay when mouse is over canvas, but crop tool is special
            Tool* tool = view.document->getTool();
            bool showOverlay = tool && tool->hasOverlay() &&
                              (mouseOverCanvas || tool->type == ToolType::Crop);
            if (showOverlay) {
                Vec2 cursorScreen = view.documentToScreen(lastMousePos);
                Recti clipRect(static_cast<i32>(global.x), static_cast<i32>(global.y),
                              static_cast<i32>(global.w), static_cast<i32>(global.h));
                // Full offset = pan + viewport position
                Vec2 fullPan(view.pan.x + global.x, view.pan.y + global.y);
                tool->renderOverlay(fb, cursorScreen, view.zoom, fullPan, clipRect);
            }

            // Draw selection preview while dragging or for polygon tool (which stays active between clicks)
            if (tool) {
                bool shouldShowPreview = toolActive;
                // Polygon tool needs preview even when not dragging
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

    // Draw an ellipse outline using the midpoint algorithm
    void drawEllipseOutline(Framebuffer& fb, i32 cx, i32 cy, i32 rx, i32 ry, u32 color) {
        if (rx <= 0 || ry <= 0) return;

        i32 rx2 = rx * rx;
        i32 ry2 = ry * ry;
        i32 twoRx2 = 2 * rx2;
        i32 twoRy2 = 2 * ry2;

        i32 x = 0;
        i32 y = ry;
        i32 px = 0;
        i32 py = twoRx2 * y;

        // Region 1
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

        // Region 2
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

    // Render selection tool preview (rectangle/ellipse/free outline during drag)
    void renderSelectionPreview(Framebuffer& fb, Tool* tool) {
        if (!tool) return;

        i32 thickness = std::max(1, static_cast<i32>(Config::uiScale));

        // Helper to draw a visible line (black with white offset for contrast)
        auto drawVisibleLine = [&](i32 x0, i32 y0, i32 x1, i32 y1) {
            // Draw black line
            fb.drawLine(x0, y0, x1, y1, 0x000000FF);
            // Draw white line offset perpendicular to line direction for visibility
            i32 dx = x1 - x0;
            i32 dy = y1 - y0;
            // Offset perpendicular: if more horizontal, offset in Y; if more vertical, offset in X
            if (std::abs(dx) >= std::abs(dy)) {
                fb.drawLine(x0, y0 + 1, x1, y1 + 1, 0xFFFFFFFF);
            } else {
                fb.drawLine(x0 + 1, y0, x1 + 1, y1, 0xFFFFFFFF);
            }
        };

        // Handle polygon select tool
        if (tool->type == ToolType::PolygonSelect) {
            auto* polyTool = static_cast<PolygonSelectTool*>(tool);
            if (!polyTool->active || polyTool->points.empty()) return;

            // Draw lines connecting all points
            for (size_t i = 0; i + 1 < polyTool->points.size(); ++i) {
                Vec2 p1 = view.documentToScreen(polyTool->points[i]);
                Vec2 p2 = view.documentToScreen(polyTool->points[i + 1]);
                drawVisibleLine(static_cast<i32>(p1.x), static_cast<i32>(p1.y),
                               static_cast<i32>(p2.x), static_cast<i32>(p2.y));
            }

            // Draw line from last point to current mouse position
            Vec2 lastPt = view.documentToScreen(polyTool->points.back());
            Vec2 curPt = view.documentToScreen(lastMousePos);
            drawVisibleLine(static_cast<i32>(lastPt.x), static_cast<i32>(lastPt.y),
                           static_cast<i32>(curPt.x), static_cast<i32>(curPt.y));

            // Draw closing line (from current to start) in gray if we have enough points
            if (polyTool->points.size() >= 2) {
                Vec2 startPt = view.documentToScreen(polyTool->points.front());
                fb.drawLine(static_cast<i32>(curPt.x), static_cast<i32>(curPt.y),
                           static_cast<i32>(startPt.x), static_cast<i32>(startPt.y), 0x888888FF);
            }

            // Draw vertices as small squares
            for (const auto& pt : polyTool->points) {
                Vec2 screenPt = view.documentToScreen(pt);
                i32 sx = static_cast<i32>(screenPt.x);
                i32 sy = static_cast<i32>(screenPt.y);
                fb.fillRect(Recti(sx - 2, sy - 2, 5, 5), 0xFFFFFFFF);
                fb.fillRect(Recti(sx - 1, sy - 1, 3, 3), 0x000000FF);
            }
            return;
        }

        // Handle free select (lasso) tool
        if (tool->type == ToolType::FreeSelect) {
            auto* freeTool = static_cast<FreeSelectTool*>(tool);
            if (!freeTool->selecting || freeTool->points.empty()) return;

            // Draw lines connecting all points
            for (size_t i = 0; i + 1 < freeTool->points.size(); ++i) {
                Vec2 p1 = view.documentToScreen(freeTool->points[i]);
                Vec2 p2 = view.documentToScreen(freeTool->points[i + 1]);
                drawVisibleLine(static_cast<i32>(p1.x), static_cast<i32>(p1.y),
                               static_cast<i32>(p2.x), static_cast<i32>(p2.y));
            }

            // Draw line from last point to current mouse position
            Vec2 lastPt = view.documentToScreen(freeTool->points.back());
            Vec2 curPt = view.documentToScreen(lastMousePos);
            drawVisibleLine(static_cast<i32>(lastPt.x), static_cast<i32>(lastPt.y),
                           static_cast<i32>(curPt.x), static_cast<i32>(curPt.y));

            // Draw closing line (from current to start) in gray
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

        // Convert to screen coordinates
        Vec2 startScreen = view.documentToScreen(startDoc);
        Vec2 endScreen = view.documentToScreen(endDoc);

        i32 x1 = static_cast<i32>(std::min(startScreen.x, endScreen.x));
        i32 y1 = static_cast<i32>(std::min(startScreen.y, endScreen.y));
        i32 x2 = static_cast<i32>(std::max(startScreen.x, endScreen.x));
        i32 y2 = static_cast<i32>(std::max(startScreen.y, endScreen.y));
        i32 w = x2 - x1;
        i32 h = y2 - y1;

        if (isEllipse) {
            // Draw ellipse preview
            i32 cx = (x1 + x2) / 2;
            i32 cy = (y1 + y2) / 2;
            i32 rx = w / 2;
            i32 ry = h / 2;
            // Draw black and white ellipses for visibility
            for (i32 t = 0; t < thickness; ++t) {
                drawEllipseOutline(fb, cx, cy, rx - t, ry - t, (t == 0) ? 0x000000FF : 0xFFFFFFFF);
            }
        } else {
            // Draw rectangle preview with black/white outline for visibility
            fb.drawRect(Recti(x1, y1, w, h), 0x000000FF, thickness);
            if (w > thickness * 2 && h > thickness * 2) {
                fb.drawRect(Recti(x1 + thickness, y1 + thickness, w - thickness * 2, h - thickness * 2), 0xFFFFFFFF, thickness);
            }
        }
    }

    bool onMouseDown(const MouseEvent& e) override {
        AppState& state = getAppState();

        // Middle click or space always pans
        if (state.spaceHeld || e.button == MouseButton::Middle) {
            panning = true;
            panStartPos = e.globalPosition;
            state.capturedWidget = this;  // Capture mouse for panning
            return true;
        }

        // Zoom tool responds to both left and right click
        if (view.document && (e.button == MouseButton::Left || e.button == MouseButton::Right)) {
            Tool* currentTool = view.document->getTool();

            // Zoom tool zooms on drag, clicks zoom in/out
            if (currentTool && currentTool->type == ToolType::Zoom) {
                zooming = true;
                zoomDragged = false;
                zoomButton = e.button;
                zoomStartPos = e.globalPosition;
                zoomStartLevel = view.zoom;
                zoomCenter = e.globalPosition;
                state.capturedWidget = this;  // Capture mouse for zooming
                return true;
            }
        }

        if (view.document && e.button == MouseButton::Left) {
            Tool* currentTool = view.document->getTool();

            // Pan tool pans like middle click
            if (currentTool && currentTool->type == ToolType::Pan) {
                panning = true;
                panStartPos = e.globalPosition;
                state.capturedWidget = this;  // Capture mouse for panning
                return true;
            }

            Vec2 docPos = view.screenToDocument(e.globalPosition);
            lastMousePos = docPos;  // Update for preview

            ToolEvent te;
            te.position = docPos;
            te.pressure = 1.0f;  // Tablet pressure requires SDL 2.0.22+ pen API
            te.zoom = view.zoom;
            te.shiftHeld = e.mods.shift;
            te.ctrlHeld = e.mods.ctrl;
            te.altHeld = e.mods.alt;

            view.document->handleMouseDown(te);
            toolActive = true;
            state.capturedWidget = this;  // Capture mouse for tool operations
            state.needsRedraw = true;
            return true;
        }

        return false;
    }

    bool onMouseUp(const MouseEvent& e) override {
        AppState& state = getAppState();

        if (panning) {
            panning = false;
            state.capturedWidget = nullptr;  // Release mouse capture
            return true;
        }

        if (zooming) {
            // If it was a click (not a drag), zoom in or out at the click point
            if (!zoomDragged) {
                // Determine zoom direction based on click mode and button
                // zoomClickMode: 0 = Zoom In, 1 = Zoom Out
                // Left button does what's selected, right button does opposite
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
            state.capturedWidget = nullptr;  // Release mouse capture
            return true;
        }

        if (view.document && (e.button == MouseButton::Left || toolActive)) {
            Vec2 docPos = view.screenToDocument(e.globalPosition);
            lastMousePos = docPos;  // Update for final position

            ToolEvent te;
            te.position = docPos;
            te.zoom = view.zoom;
            te.shiftHeld = e.mods.shift;
            te.ctrlHeld = e.mods.ctrl;
            te.altHeld = e.mods.alt;

            view.document->handleMouseUp(te);
            toolActive = false;
            state.capturedWidget = nullptr;  // Release mouse capture
            state.needsRedraw = true;
            return true;
        }

        return false;
    }

    bool onMouseDrag(const MouseEvent& e) override {
        AppState& state = getAppState();

        if (panning) {
            Vec2 delta = e.globalPosition - panStartPos;
            view.panBy(delta);
            panStartPos = e.globalPosition;
            state.needsRedraw = true;
            return true;
        }

        if (zooming) {
            // Check if we've dragged far enough to count as a drag (not a click)
            Vec2 delta = e.globalPosition - zoomStartPos;
            f32 dragDistance = std::sqrt(delta.x * delta.x + delta.y * delta.y);
            if (dragDistance > 5.0f) {
                zoomDragged = true;
            }

            // Only apply zoom on drag if actually dragging
            if (zoomDragged) {
                // Drag up/right to zoom in, down/left to zoom out
                f32 deltaY = zoomStartPos.y - e.globalPosition.y;  // Inverted: drag up = positive
                f32 zoomFactor = 1.0f + deltaY * 0.005f;  // Sensitivity factor
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
            te.pressure = 1.0f;  // Tablet pressure requires SDL 2.0.22+ pen API
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

    bool onMouseMove(const MouseEvent& e) override {
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

    bool onMouseWheel(const MouseEvent& e) override {
        if (e.wheelDelta != 0) {
            f32 zoomFactor = e.wheelDelta > 0 ? Config::ZOOM_STEP : 1.0f / Config::ZOOM_STEP;
            view.zoomAtPoint(e.globalPosition, view.zoom * zoomFactor);
            getAppState().needsRedraw = true;
            return true;
        }
        return false;
    }

    void onMouseEnter(const MouseEvent& e) override {
        mouseOverCanvas = true;
        getAppState().needsRedraw = true;
    }

    void onMouseLeave(const MouseEvent& e) override {
        mouseOverCanvas = false;
        getAppState().needsRedraw = true;
    }

    // DocumentObserver
    void onDocumentChanged(const Rect& dirtyRect) override {
        getAppState().needsRedraw = true;
    }
};

// Tool palette
class ToolPalette : public Panel {
public:
    std::vector<IconButton*> toolButtons;
    std::vector<ToolType> buttonToolTypes;  // Track which tool type each button represents
    GridLayout* gridLayout = nullptr;
    std::function<void(ToolType)> onToolChanged;  // Callback when tool selection changes
    std::function<void()> onZoomReset;            // Double-click on Zoom tool
    std::function<void()> onViewReset;            // Double-click on Hand tool

    // Color swatches (Photoshop-style at bottom)
    ColorSwatch* fgSwatch = nullptr;
    ColorSwatch* bgSwatch = nullptr;
    Widget* swatchContainer = nullptr;
    IconButton* swapBtn = nullptr;
    IconButton* resetBtn = nullptr;
    std::function<void(bool)> onColorSwatchClicked;  // true = foreground, false = background

    // Map sub-types to their parent button type
    static ToolType getButtonTypeForTool(ToolType type) {
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

    ToolPalette() {
        bgColor = Config::COLOR_PANEL;
        preferredSize = Vec2(Config::toolPaletteWidth(), 0);
        horizontalPolicy = SizePolicy::Fixed;
        verticalPolicy = SizePolicy::Expanding;
        setPadding(4 * Config::uiScale);

        // Use a VBoxLayout to contain the grid, spacer, and color swatches
        auto vbox = createChild<VBoxLayout>(0);
        vbox->horizontalPolicy = SizePolicy::Expanding;
        vbox->verticalPolicy = SizePolicy::Expanding;

        gridLayout = vbox->createChild<GridLayout>(2, 4 * Config::uiScale, 4 * Config::uiScale);
        gridLayout->verticalPolicy = SizePolicy::Fixed;

        // Add tool buttons
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

        // Calculate grid preferred height
        u32 rows = (toolButtons.size() + 1) / 2;  // 2 columns
        f32 buttonSize = 32 * Config::uiScale;
        f32 spacing = 4 * Config::uiScale;
        gridLayout->preferredSize = Vec2(0, rows * buttonSize + (rows - 1) * spacing);

        // Color swatches section directly under tools
        auto colorSection = vbox->createChild<VBoxLayout>(4 * Config::uiScale);
        colorSection->verticalPolicy = SizePolicy::Fixed;
        colorSection->preferredSize = Vec2(0, 78 * Config::uiScale);  // swatches + buttons + padding
        colorSection->setPadding(4 * Config::uiScale);

        // Container for overlapping swatches
        swatchContainer = colorSection->createChild<Widget>();
        swatchContainer->preferredSize = Vec2(0, 44 * Config::uiScale);
        swatchContainer->verticalPolicy = SizePolicy::Fixed;

        // BG swatch (rendered first, so appears behind)
        bgSwatch = swatchContainer->createChild<ColorSwatch>(Color::white());
        bgSwatch->preferredSize = Vec2(28 * Config::uiScale, 28 * Config::uiScale);
        bgSwatch->onClick = [this]() {
            if (onColorSwatchClicked) onColorSwatchClicked(false);
        };

        // FG swatch (rendered second, so appears in front)
        fgSwatch = swatchContainer->createChild<ColorSwatch>(Color::black());
        fgSwatch->preferredSize = Vec2(28 * Config::uiScale, 28 * Config::uiScale);
        fgSwatch->onClick = [this]() {
            if (onColorSwatchClicked) onColorSwatchClicked(true);
        };

        // Swap/Reset buttons row
        auto btnRow = colorSection->createChild<HBoxLayout>(2 * Config::uiScale);
        btnRow->preferredSize = Vec2(0, 22 * Config::uiScale);
        btnRow->verticalPolicy = SizePolicy::Fixed;

        swapBtn = btnRow->createChild<IconButton>();
        swapBtn->preferredSize = Vec2(24 * Config::uiScale, 20 * Config::uiScale);
        swapBtn->renderIcon = [](Framebuffer& fb, const Rect& r, u32 color) {
            FontRenderer::instance().renderIconCentered(fb, "\xF3\xB0\x93\xA1", r, color, Config::defaultFontSize(), "Material Icons");  // U+F04E1 swap
        };
        swapBtn->onClick = [this]() {
            getAppState().swapColors();
            updateColors();
            getAppState().needsRedraw = true;
        };

        resetBtn = btnRow->createChild<IconButton>();
        resetBtn->preferredSize = Vec2(24 * Config::uiScale, 20 * Config::uiScale);
        resetBtn->renderIcon = [](Framebuffer& fb, const Rect& r, u32 color) {
            FontRenderer::instance().renderIconCentered(fb, "\xF3\xB0\x80\xBD", r, color, Config::defaultFontSize(), "Material Icons");  // U+F003D reset
        };
        resetBtn->onClick = [this]() {
            getAppState().resetColors();
            updateColors();
            getAppState().needsRedraw = true;
        };

        // Spacer to push tools and colors to top
        vbox->createChild<Spacer>();
    }

    void updateColors() {
        AppState& state = getAppState();
        if (fgSwatch) fgSwatch->color = state.foregroundColor;
        if (bgSwatch) bgSwatch->color = state.backgroundColor;
    }

    void layout() override {
        Panel::layout();

        // Position swatches for overlapping Photoshop-style effect
        if (fgSwatch && bgSwatch && swatchContainer) {
            f32 swatchSize = 28 * Config::uiScale;
            f32 offset = 14 * Config::uiScale;

            // FG at top-left
            fgSwatch->bounds = Rect(
                swatchContainer->bounds.x + 4 * Config::uiScale,
                swatchContainer->bounds.y,
                swatchSize, swatchSize);

            // BG offset to bottom-right
            bgSwatch->bounds = Rect(
                swatchContainer->bounds.x + 4 * Config::uiScale + offset,
                swatchContainer->bounds.y + offset,
                swatchSize, swatchSize);
        }
    }

    void addToolButton(ToolType type, const char* label) {
        auto btn = gridLayout->createChild<IconButton>();
        btn->preferredSize = Vec2(32 * Config::uiScale, 32 * Config::uiScale);
        btn->minSize = Vec2(32 * Config::uiScale, 32 * Config::uiScale);
        btn->maxSize = Vec2(32 * Config::uiScale, 32 * Config::uiScale);
        btn->horizontalPolicy = SizePolicy::Fixed;
        btn->verticalPolicy = SizePolicy::Fixed;
        btn->toggleMode = true;

        // Check if this tool has a Material Icon
        const char* materialIcon = nullptr;
        if (type == ToolType::Brush) {
            materialIcon = "\xF3\xB0\x83\xA3";  // U+F00E3 brush icon
        } else if (type == ToolType::Eraser) {
            materialIcon = "\xF3\xB0\x87\xBE";  // U+F01FE eraser icon
        } else if (type == ToolType::Move) {
            materialIcon = "\xF3\xB0\x81\x81";  // U+F0041 move icon
        } else if (type == ToolType::RectangleSelect) {
            materialIcon = "\xF3\xB0\x92\x85";  // U+F0485 select icon
        } else if (type == ToolType::Fill) {
            materialIcon = "\xF3\xB0\x89\xA6";  // U+F0266 fill icon
        } else if (type == ToolType::MagicWand) {
            materialIcon = "\xF3\xB1\xA1\x84";  // U+F1844 magic wand icon
        } else if (type == ToolType::Clone) {
            materialIcon = "\xF3\xB0\xB4\xB9";  // U+F0D39 clone stamp icon
        } else if (type == ToolType::Smudge) {
            materialIcon = "\xF3\xB1\x92\x84";  // U+F1484 smudge icon
        } else if (type == ToolType::Burn) {
            materialIcon = "\xF3\xB0\x88\xB8";  // U+F0238 burn icon
        } else if (type == ToolType::Pan) {
            materialIcon = "\xF3\xB1\xA0\xAC";  // U+F182C pan icon
        } else if (type == ToolType::Zoom) {
            materialIcon = "\xF3\xB0\x8D\x89";  // U+F0349 zoom icon
        } else if (type == ToolType::Dodge) {
            materialIcon = "\xF3\xB0\x96\x99";  // U+F0599 dodge icon
        } else if (type == ToolType::Crop) {
            materialIcon = "\xF3\xB0\x86\x9E";  // U+F019E crop icon
        } else if (type == ToolType::ColorPicker) {
            materialIcon = "\xF3\xB0\x88\x8A";  // U+F020A color picker icon
        }

        if (materialIcon) {
            // Use Material Icons font with proper visual centering
            std::string iconStr = materialIcon;
            btn->renderIcon = [iconStr](Framebuffer& fb, const Rect& r, u32 color) {
                f32 iconSize = Config::defaultFontSize();
                FontRenderer::instance().renderIconCentered(fb, iconStr, r, color, iconSize, "Material Icons");
            };
        } else {
            // Simple text icon - center the single character
            btn->renderIcon = [label](Framebuffer& fb, const Rect& r, u32 color) {
                Vec2 textSize = FontRenderer::instance().measureText(label, Config::defaultFontSize());
                FontRenderer::instance().renderText(fb, label,
                    static_cast<i32>(r.x + (r.w - textSize.x) / 2),
                    static_cast<i32>(r.y + (r.h - textSize.y) / 2),
                    color);
            };
        }

        btn->onClick = [this, type]() {
            selectTool(type);
            // Button selection is handled in selectTool()
        };

        // Add double-click handlers for special tools
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

    void selectTool(ToolType type) {
        AppState& state = getAppState();
        Document* doc = state.activeDocument;
        if (!doc) return;

        // Rasterize any pending transform before switching tools
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
                // Create appropriate tool based on fillMode
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

        // Update button selection - map sub-types to parent button type
        ToolType buttonType = getButtonTypeForTool(type);
        for (size_t i = 0; i < toolButtons.size(); ++i) {
            toolButtons[i]->selected = (buttonToolTypes[i] == buttonType);
        }

        // Notify that tool changed
        if (onToolChanged) {
            onToolChanged(type);
        }

        // Ensure redraw for tool overlay
        state.needsRedraw = true;
    }

    void setEnabled(bool isEnabled) {
        for (auto* btn : toolButtons) {
            btn->enabled = isEnabled;
            if (!isEnabled) {
                btn->selected = false;
            }
        }
        // Also disable color swatches and swap/reset buttons
        if (fgSwatch) fgSwatch->enabled = isEnabled;
        if (bgSwatch) bgSwatch->enabled = isEnabled;
        if (swapBtn) swapBtn->enabled = isEnabled;
        if (resetBtn) resetBtn->enabled = isEnabled;
    }

    void clearSelection() {
        for (auto* btn : toolButtons) {
            btn->selected = false;
        }
    }
};

// Tool options bar - context-sensitive to current tool
class ToolOptionsBar : public Panel {
public:
    HBoxLayout* layout = nullptr;
    i32 currentToolType = -1;  // Track current tool to detect changes
    bool lastHadSelection = false;  // Track selection state for Move tool
    bool pendingRebuild = false;  // Defer rebuild to avoid destroying widgets during callbacks

    // Common widget references (may be null depending on tool)
    Slider* sizeSlider = nullptr;
    Label* hardnessLabel = nullptr;
    Slider* hardnessSlider = nullptr;
    Slider* opacitySlider = nullptr;
    Slider* toleranceSlider = nullptr;
    Checkbox* contiguousCheck = nullptr;
    Checkbox* antiAliasCheck = nullptr;
    ComboBox* shapeCombo = nullptr;
    ComboBox* fillModeCombo = nullptr;
    Button* curveBtn = nullptr;
    ComboBox* pressureCombo = nullptr;
    i32 lastFillMode = -1;  // Track fill mode changes

    // Callback for switching tools (set by MainWindow)
    std::function<void(ToolType)> onSelectTool;

    // Dynamic sizing constants and helpers
    static constexpr f32 TOOLBAR_LABEL_PADDING = 4.0f;
    static constexpr f32 TOOLBAR_BTN_PADDING = 14.0f;
    static constexpr f32 TOOLBAR_ITEM_SPACING = 4.0f;
    static constexpr f32 TOOLBAR_GROUP_SPACING = 4.0f;

    f32 itemHeight() const { return 24 * Config::uiScale; }
    f32 sliderHeight() const { return 20 * Config::uiScale; }

    // Create a dynamically-sized label
    Label* addLabel(const char* text) {
        auto* label = layout->createChild<Label>(text);
        Vec2 textSize = FontRenderer::instance().measureText(text, Config::defaultFontSize());
        f32 padding = TOOLBAR_LABEL_PADDING * Config::uiScale;
        label->preferredSize = Vec2(textSize.x + padding * 2, itemHeight());
        return label;
    }

    // Create a dynamically-sized button
    Button* addButton(const char* text) {
        auto* btn = layout->createChild<Button>(text);
        Vec2 textSize = FontRenderer::instance().measureText(text, Config::defaultFontSize());
        f32 padding = TOOLBAR_BTN_PADDING * Config::uiScale;
        btn->preferredSize = Vec2(textSize.x + padding * 2, itemHeight());
        return btn;
    }

    // Add spacing between items
    void addItemSpacing() {
        layout->createChild<Spacer>(TOOLBAR_ITEM_SPACING * Config::uiScale, true);
    }

    // Add spacing between groups
    void addGroupSpacing() {
        layout->createChild<Spacer>(TOOLBAR_GROUP_SPACING * Config::uiScale, true);
    }

    // Create a slider with appropriate sizing
    Slider* addSlider(f32 min, f32 max, f32 value, f32 width = 80.0f) {
        auto* slider = layout->createChild<Slider>(min, max, value);
        slider->preferredSize = Vec2(width * Config::uiScale, sliderHeight());
        return slider;
    }

    // Create a combo box with width based on widest item
    ComboBox* addComboBox(const std::vector<const char*>& items, i32 selectedIndex = 0) {
        auto* combo = layout->createChild<ComboBox>();
        f32 maxWidth = 0;
        for (const char* item : items) {
            combo->addItem(item);
            Vec2 textSize = FontRenderer::instance().measureText(item, Config::defaultFontSize());
            maxWidth = std::max(maxWidth, textSize.x);
        }
        // Add padding for dropdown arrow and margins
        combo->preferredSize = Vec2(maxWidth + 30 * Config::uiScale, itemHeight());
        combo->selectedIndex = selectedIndex;
        return combo;
    }

    // Create a checkbox with dynamic sizing
    Checkbox* addCheckbox(const char* text) {
        auto* check = layout->createChild<Checkbox>(text);
        Vec2 textSize = FontRenderer::instance().measureText(text, Config::defaultFontSize());
        f32 padding = TOOLBAR_LABEL_PADDING * Config::uiScale;
        // Checkbox has a box (20px) + spacing (4px) + text + padding
        check->preferredSize = Vec2(24 * Config::uiScale + textSize.x + padding, itemHeight());
        return check;
    }

    // Helper to check if a tool type is a selection tool
    static bool isSelectionTool(ToolType t) {
        return t == ToolType::RectangleSelect || t == ToolType::EllipseSelect ||
               t == ToolType::FreeSelect || t == ToolType::PolygonSelect;
    }

    // Helper to check if a tool type is a fill tool (solid fill or gradient)
    static bool isFillTool(ToolType t) {
        return t == ToolType::Fill || t == ToolType::Gradient;
    }

    ToolOptionsBar() {
        bgColor = Config::COLOR_PANEL;
        preferredSize = Vec2(0, Config::toolOptionsHeight());
        verticalPolicy = SizePolicy::Fixed;
        setPadding(4 * Config::uiScale);

        layout = createChild<HBoxLayout>(8 * Config::uiScale);

        // Build initial options (will be empty until tool is set)
        rebuildOptions();
    }

    void update() {
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

    // Call this from a safe location (e.g., end of frame) to apply deferred rebuilds
    void applyPendingChanges() {
        if (pendingRebuild) {
            pendingRebuild = false;
            rebuildOptions();
        }
    }

    void clearOptions() {
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

    // Clear the tool options bar completely (when no document is open)
    void clear() {
        currentToolType = -1;
        lastHadSelection = false;
        clearOptions();
    }

    // Update hardness visibility based on current brush tip selection
    void updateHardnessVisibility() {
        bool showHardness = getAppState().currentBrushTipIndex < 0;
        if (hardnessLabel) hardnessLabel->visible = showHardness;
        if (hardnessSlider) hardnessSlider->visible = showHardness;
        // Trigger layout recalculation
        if (layout) {
            layout->layout();
            getAppState().needsRedraw = true;
        }
    }

    // Update curve button visibility based on pressure mode
    void updateCurveVisibility() {
        bool showCurve = getAppState().brushPressureMode != 0;  // 0 = None
        if (curveBtn) curveBtn->visible = showCurve;
        // Trigger layout recalculation
        if (layout) {
            layout->layout();
            getAppState().needsRedraw = true;
        }
    }

    const char* getToolName(ToolType tool) {
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

    void rebuildOptions() {
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

    void buildBrushOptions() {
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

        // Size
        addLabel("Size");
        sizeSlider = addSlider(Config::MIN_BRUSH_SIZE, Config::MAX_BRUSH_SIZE, state.brushSize, 80);
        sizeSlider->onChanged = [](f32 value) { getAppState().brushSize = value; };

        addGroupSpacing();

        // Opacity
        addLabel("Opacity");
        opacitySlider = addSlider(0.0f, 1.0f, state.brushOpacity, 60);
        opacitySlider->onChanged = [](f32 value) { getAppState().brushOpacity = value; };

        addGroupSpacing();

        // Flow
        addLabel("Flow");
        auto* flowSlider = addSlider(0.0f, 1.0f, state.brushFlow, 60);
        flowSlider->onChanged = [](f32 value) { getAppState().brushFlow = value; };

        addGroupSpacing();

        // Hardness (only visible for round brush)
        hardnessLabel = addLabel("Hardness");
        hardnessSlider = addSlider(0.0f, 1.0f, state.brushHardness, 60);
        hardnessSlider->onChanged = [](f32 value) { getAppState().brushHardness = value; };

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
                onOpenPressureCurvePopup(btnBounds.x, btnBounds.bottom());
            }
        };
        curveBtn->visible = state.brushPressureMode != 0;

        addItemSpacing();

        // Manage brushes button
        auto* manageBtn = addButton("Manage");
        manageBtn->onClick = [this, manageBtn]() {
            if (onOpenManageBrushesPopup) {
                Rect btnBounds = manageBtn->globalBounds();
                onOpenManageBrushesPopup(btnBounds.right(), btnBounds.bottom());
            }
        };
    }

    // Callback for opening pressure curve popup (set by MainWindow)
    std::function<void(f32 x, f32 y)> onOpenPressureCurvePopup;

    // Callback for opening brush tip popup (set by MainWindow)
    std::function<void(f32 x, f32 y)> onOpenBrushTipPopup;

    // Callback for opening manage brushes popup (set by MainWindow)
    std::function<void(f32 x, f32 y)> onOpenManageBrushesPopup;

    // Callback for fit to screen (set by MainWindow)
    std::function<void()> onFitToScreen;

    // Callbacks for crop tool (set by MainWindow)
    std::function<void()> onCropApply;
    std::function<void()> onCropReset;

    void buildCropOptions() {
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

    void buildPanOptions() {
        auto* fitBtn = addButton("Fit");
        fitBtn->onClick = [this]() {
            if (onFitToScreen) onFitToScreen();
        };
    }

    void buildEraserOptions() {
        AppState& state = getAppState();

        // Size
        addLabel("Size");
        sizeSlider = addSlider(Config::MIN_BRUSH_SIZE, Config::MAX_BRUSH_SIZE, state.brushSize, 80);
        sizeSlider->onChanged = [](f32 value) { getAppState().brushSize = value; };

        addGroupSpacing();

        // Hardness
        addLabel("Hard");
        hardnessSlider = addSlider(0.0f, 1.0f, state.brushHardness, 60);
        hardnessSlider->onChanged = [](f32 value) { getAppState().brushHardness = value; };

        addGroupSpacing();

        // Opacity
        addLabel("Opacity");
        opacitySlider = addSlider(0.0f, 1.0f, state.brushOpacity, 60);
        opacitySlider->onChanged = [](f32 value) { getAppState().brushOpacity = value; };

        addGroupSpacing();

        // Flow
        addLabel("Flow");
        auto* flowSlider = addSlider(0.0f, 1.0f, state.brushFlow, 60);
        flowSlider->onChanged = [](f32 value) { getAppState().brushFlow = value; };

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
                onOpenPressureCurvePopup(btnBounds.x, btnBounds.bottom());
            }
        };
        curveBtn->visible = state.eraserPressureMode != 0;
    }

    // Update eraser curve button visibility based on pressure mode
    void updateEraserCurveVisibility() {
        bool showCurve = getAppState().eraserPressureMode != 0;  // 0 = None
        if (curveBtn) curveBtn->visible = showCurve;
        // Trigger layout recalculation
        if (layout) {
            layout->layout();
            getAppState().needsRedraw = true;
        }
    }

    void buildDodgeBurnOptions() {
        AppState& state = getAppState();

        // Size
        addLabel("Size");
        sizeSlider = addSlider(Config::MIN_BRUSH_SIZE, Config::MAX_BRUSH_SIZE, state.brushSize, 80);
        sizeSlider->onChanged = [](f32 value) { getAppState().brushSize = value; };

        addGroupSpacing();

        // Hardness
        addLabel("Hard");
        hardnessSlider = addSlider(0.0f, 1.0f, state.brushHardness, 60);
        hardnessSlider->onChanged = [](f32 value) { getAppState().brushHardness = value; };

        addGroupSpacing();

        // Exposure
        addLabel("Exposure");
        opacitySlider = addSlider(0.0f, 1.0f, state.brushOpacity, 60);
        opacitySlider->onChanged = [](f32 value) { getAppState().brushOpacity = value; };

        addGroupSpacing();

        // Flow
        addLabel("Flow");
        auto* flowSlider = addSlider(0.0f, 1.0f, state.brushFlow, 60);
        flowSlider->onChanged = [](f32 value) { getAppState().brushFlow = value; };

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
                onOpenPressureCurvePopup(btnBounds.x, btnBounds.bottom());
            }
        };

        updateDodgeBurnCurveVisibility();
    }

    void updateDodgeBurnCurveVisibility() {
        bool showCurve = getAppState().dodgeBurnPressureMode != 0;  // 0 = None
        if (curveBtn) curveBtn->visible = showCurve;
    }

    void buildZoomOptions() {
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

    // Clone-specific checkbox for sample mode
    Checkbox* sampleModeCheck = nullptr;

    void buildCloneOptions() {
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
        sizeSlider = addSlider(Config::MIN_BRUSH_SIZE, Config::MAX_BRUSH_SIZE, state.brushSize, 80);
        sizeSlider->onChanged = [](f32 value) { getAppState().brushSize = value; };

        addGroupSpacing();

        // Hardness
        addLabel("Hard");
        hardnessSlider = addSlider(0.0f, 1.0f, state.brushHardness, 60);
        hardnessSlider->onChanged = [](f32 value) { getAppState().brushHardness = value; };

        addGroupSpacing();

        // Opacity
        addLabel("Opacity");
        opacitySlider = addSlider(0.0f, 1.0f, state.brushOpacity, 60);
        opacitySlider->onChanged = [](f32 value) { getAppState().brushOpacity = value; };

        addGroupSpacing();

        // Flow
        addLabel("Flow");
        auto* flowSlider = addSlider(0.0f, 1.0f, state.brushFlow, 60);
        flowSlider->onChanged = [](f32 value) { getAppState().brushFlow = value; };

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
                onOpenPressureCurvePopup(btnBounds.x, btnBounds.bottom());
            }
        };
        curveBtn->visible = state.clonePressureMode != 0;
    }

    // Update clone curve button visibility based on pressure mode
    void updateCloneCurveVisibility() {
        bool showCurve = getAppState().clonePressureMode != 0;  // 0 = None
        if (curveBtn) curveBtn->visible = showCurve;
        // Trigger layout recalculation
        if (layout) {
            layout->layout();
            getAppState().needsRedraw = true;
        }
    }

    void buildSmudgeOptions() {
        AppState& state = getAppState();

        // Size
        addLabel("Size");
        sizeSlider = addSlider(Config::MIN_BRUSH_SIZE, Config::MAX_BRUSH_SIZE, state.brushSize, 80);
        sizeSlider->onChanged = [](f32 value) { getAppState().brushSize = value; };

        addGroupSpacing();

        // Hardness
        addLabel("Hard");
        hardnessSlider = addSlider(0.0f, 1.0f, state.brushHardness, 60);
        hardnessSlider->onChanged = [](f32 value) { getAppState().brushHardness = value; };

        addGroupSpacing();

        // Strength
        addLabel("Strength");
        opacitySlider = addSlider(0.0f, 1.0f, state.brushOpacity, 60);
        opacitySlider->onChanged = [](f32 value) { getAppState().brushOpacity = value; };

        addGroupSpacing();

        // Flow
        addLabel("Flow");
        auto* flowSlider = addSlider(0.0f, 1.0f, state.brushFlow, 60);
        flowSlider->onChanged = [](f32 value) { getAppState().brushFlow = value; };

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
                onOpenPressureCurvePopup(btnBounds.x, btnBounds.bottom());
            }
        };
        curveBtn->visible = state.smudgePressureMode != 0;
    }

    // Update smudge curve button visibility based on pressure mode
    void updateSmudgeCurveVisibility() {
        bool showCurve = getAppState().smudgePressureMode != 0;  // 0 = None
        if (curveBtn) curveBtn->visible = showCurve;
        // Trigger layout recalculation
        if (layout) {
            layout->layout();
            getAppState().needsRedraw = true;
        }
    }

    void buildFillOptions() {
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
            toleranceSlider = addSlider(0.0f, 510.0f, state.fillTolerance, 100);
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

    void buildGradientOptions() {
        addLabel("Gradient");

        addGroupSpacing();

        // Opacity
        AppState& state = getAppState();
        addLabel("Opacity");
        opacitySlider = addSlider(0.0f, 1.0f, state.brushOpacity, 80);
        opacitySlider->onChanged = [](f32 value) { getAppState().brushOpacity = value; };
    }

    void buildMoveOptions() {
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

    void buildSelectionOptions() {
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

    void buildMagicWandOptions() {
        AppState& state = getAppState();

        // Tolerance
        addLabel("Tolerance");
        toleranceSlider = addSlider(0.0f, 510.0f, state.wandTolerance, 100);
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

    void buildColorPickerOptions() {
        AppState& state = getAppState();

        addLabel("Sample");
        auto* sampleCombo = addComboBox({"Current Layer", "Current & Below", "All Layers"}, state.colorPickerSampleMode);
        sampleCombo->onSelectionChanged = [](i32 index) {
            getAppState().colorPickerSampleMode = index;
        };
    }
};

// Drag area for custom title bar
class TitleBarDragArea : public Widget {
public:
    std::function<void(i32, i32)> onStartDrag;  // Called with root coordinates
    std::function<void()> onDoubleClick;
    u64 lastClickTime = 0;

    TitleBarDragArea() {
        horizontalPolicy = SizePolicy::Expanding;
        verticalPolicy = SizePolicy::Expanding;
        minSize = Vec2(100 * Config::uiScale, 0);  // Minimum drag area width
    }

    bool onMouseDown(const MouseEvent& e) override {
        if (e.button == MouseButton::Left) {
            u64 now = Platform::getMilliseconds();
            if (now - lastClickTime < 300) {
                // Double click
                if (onDoubleClick) onDoubleClick();
                lastClickTime = 0;
            } else {
                lastClickTime = now;
                // Start drag - use global position as root coordinates
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

    void renderSelf(Framebuffer& fb) override {
        // Drag area is transparent - just uses panel background
    }
};

// Window control button (minimize, maximize, close)
class WindowControlButton : public Widget {
public:
    enum class Type { Minimize, Maximize, Restore, Close };
    Type type;
    bool hovered = false;
    bool pressed = false;
    std::function<void()> onClick;

    WindowControlButton(Type t) : type(t) {
        f32 size = Config::menuBarHeight();
        preferredSize = Vec2(size * 1.5f, size);
        horizontalPolicy = SizePolicy::Fixed;
        verticalPolicy = SizePolicy::Fixed;
    }

    void setType(Type t) { type = t; }

    void renderSelf(Framebuffer& fb) override {
        Rect gb = globalBounds();
        Recti rect(static_cast<i32>(gb.x), static_cast<i32>(gb.y),
                   static_cast<i32>(gb.w), static_cast<i32>(gb.h));

        // Background color based on state and type
        u32 bgColor = Config::COLOR_TITLEBAR;
        if (type == Type::Close) {
            if (pressed) bgColor = 0xC42B1CFF;
            else if (hovered) bgColor = 0xE81123FF;
        } else {
            if (pressed) bgColor = Config::COLOR_ACTIVE;
            else if (hovered) bgColor = Config::COLOR_HOVER;
        }
        fb.fillRect(rect, bgColor);

        // Draw icon
        i32 cx = rect.x + rect.w / 2;
        i32 cy = rect.y + rect.h / 2;
        u32 iconColor = (type == Type::Close && (hovered || pressed)) ? 0xFFFFFFFF : Config::COLOR_TEXT;
        i32 iconSize = static_cast<i32>(5 * Config::uiScale);

        switch (type) {
            case Type::Minimize:
                // Horizontal line
                fb.fillRect(cx - iconSize, cy, iconSize * 2, static_cast<i32>(Config::uiScale), iconColor);
                break;
            case Type::Maximize:
                // Square outline
                fb.drawRect(Recti(cx - iconSize, cy - iconSize, iconSize * 2, iconSize * 2), iconColor, 1);
                break;
            case Type::Restore:
                // Two overlapping squares
                fb.drawRect(Recti(cx - iconSize + 2, cy - iconSize - 2, iconSize * 2 - 2, iconSize * 2 - 2), iconColor, 1);
                fb.fillRect(cx - iconSize, cy - iconSize + 2, iconSize * 2 - 2, iconSize * 2 - 2, bgColor);
                fb.drawRect(Recti(cx - iconSize, cy - iconSize + 2, iconSize * 2 - 2, iconSize * 2 - 2), iconColor, 1);
                break;
            case Type::Close:
                // X shape
                for (i32 i = -iconSize; i <= iconSize; ++i) {
                    fb.setPixel(cx + i, cy + i, iconColor);
                    fb.setPixel(cx + i, cy - i, iconColor);
                    fb.setPixel(cx + i + 1, cy + i, iconColor);
                    fb.setPixel(cx + i + 1, cy - i, iconColor);
                }
                break;
        }
    }

    void onMouseEnter(const MouseEvent& e) override {
        hovered = true;
        getAppState().needsRedraw = true;
    }

    void onMouseLeave(const MouseEvent& e) override {
        hovered = false;
        pressed = false;
        getAppState().needsRedraw = true;
    }

    bool onMouseDown(const MouseEvent& e) override {
        if (e.button == MouseButton::Left) {
            pressed = true;
            getAppState().needsRedraw = true;
            return true;
        }
        return false;
    }

    bool onMouseUp(const MouseEvent& e) override {
        if (e.button == MouseButton::Left && pressed) {
            pressed = false;
            if (hovered && onClick) onClick();
            getAppState().needsRedraw = true;
            return true;
        }
        return false;
    }
};

// Menu bar with integrated title bar
class MenuBar : public Panel {
public:
    PopupMenu* activeMenu = nullptr;
    std::vector<std::pair<Button*, PopupMenu*>> menus;
    bool menuModeActive = false;  // True when any menu is open
    u64 lastMenuCloseTime = 0;    // Timestamp when menu was closed (for toggle detection)

    // Window control widgets
    TitleBarDragArea* dragArea = nullptr;
    WindowControlButton* minimizeBtn = nullptr;
    WindowControlButton* maximizeBtn = nullptr;
    WindowControlButton* closeBtn = nullptr;
    HBoxLayout* menuLayout = nullptr;

    // Callbacks for actions that need MainWindow access
    std::function<void()> onNewDocument;
    std::function<void()> onCanvasSize;
    std::function<void()> onFitToScreen;
    std::function<void()> onRenameDocument;

    // Window control callbacks
    std::function<void(i32, i32)> onWindowDrag;
    std::function<void()> onWindowMinimize;
    std::function<void()> onWindowMaximize;
    std::function<void()> onWindowClose;
    std::function<bool()> isWindowMaximized;

    MenuBar() {
        bgColor = Config::COLOR_TITLEBAR;
        preferredSize = Vec2(0, Config::menuBarHeight());
        verticalPolicy = SizePolicy::Fixed;
        setPadding(0);

        menuLayout = createChild<HBoxLayout>(0);

        addMenu(menuLayout, "File", createFileMenu());
        addMenu(menuLayout, "Edit", createEditMenu());
        addMenu(menuLayout, "Canvas", createCanvasMenu());
        addMenu(menuLayout, "Layer", createLayerMenu());
        addMenu(menuLayout, "Select", createSelectMenu());
        addMenu(menuLayout, "View", createViewMenu());
        addMenu(menuLayout, "Help", createHelpMenu());

        // Drag area (expands to fill available space)
        dragArea = menuLayout->createChild<TitleBarDragArea>();
        dragArea->onStartDrag = [this](i32 x, i32 y) {
            if (onWindowDrag) onWindowDrag(x, y);
        };
        dragArea->onDoubleClick = [this]() {
            if (onWindowMaximize) onWindowMaximize();
        };

        // Window control buttons
        minimizeBtn = menuLayout->createChild<WindowControlButton>(WindowControlButton::Type::Minimize);
        minimizeBtn->onClick = [this]() {
            if (onWindowMinimize) onWindowMinimize();
        };

        maximizeBtn = menuLayout->createChild<WindowControlButton>(WindowControlButton::Type::Maximize);
        maximizeBtn->onClick = [this]() {
            if (onWindowMaximize) onWindowMaximize();
        };

        closeBtn = menuLayout->createChild<WindowControlButton>(WindowControlButton::Type::Close);
        closeBtn->onClick = [this]() {
            if (onWindowClose) onWindowClose();
        };
    }

    void updateMaximizeButton() {
        if (maximizeBtn && isWindowMaximized) {
            maximizeBtn->setType(isWindowMaximized() ?
                WindowControlButton::Type::Restore :
                WindowControlButton::Type::Maximize);
        }
    }

    // Custom layout to hide menu items when space is tight
    void doLayout() {
        // Calculate total width needed
        f32 availableWidth = bounds.w;
        f32 controlsWidth = 0;
        f32 dragMinWidth = dragArea ? dragArea->minSize.x : 100 * Config::uiScale;

        // Window controls are always visible
        if (minimizeBtn) controlsWidth += minimizeBtn->preferredSize.x;
        if (maximizeBtn) controlsWidth += maximizeBtn->preferredSize.x;
        if (closeBtn) controlsWidth += closeBtn->preferredSize.x;

        // Calculate menu width (only visible menus)
        f32 menusWidth = 0;
        for (auto& [btn, popup] : menus) {
            menusWidth += btn->preferredSize.x;
        }

        f32 requiredWidth = menusWidth + dragMinWidth + controlsWidth;

        // Hide menu items from right to left if not enough space
        if (requiredWidth > availableWidth) {
            f32 excess = requiredWidth - availableWidth;
            // Hide menus from right to left (Help, View, Select, Layer, Canvas, Edit - keep File)
            for (i32 i = static_cast<i32>(menus.size()) - 1; i >= 1 && excess > 0; --i) {
                if (menus[i].first->visible) {
                    menus[i].first->visible = false;
                    excess -= menus[i].first->preferredSize.x;
                }
            }
        } else {
            // Show all menus
            for (auto& [btn, popup] : menus) {
                btn->visible = true;
            }
        }
    }

    void layout() override {
        doLayout();
        // Only layout the menuLayout child, not popup menus
        // (Panel::layout() would set all visible children to fill the content area,
        // which breaks popup menus when they're open during resize)
        if (menuLayout) {
            Rect content = contentRect();
            menuLayout->setBounds(content.x, content.y, content.w, content.h);
            menuLayout->layout();
        }
    }

    bool onMouseMove(const MouseEvent& e) override {
        // When menu is active, hovering another menu button should switch to it
        if (menuModeActive && activeMenu && activeMenu->visible) {
            for (auto& [btn, popup] : menus) {
                Rect btnBounds = btn->globalBounds();
                // Only switch to enabled menus
                if (btnBounds.contains(e.globalPosition) && popup != activeMenu && btn->enabled) {
                    // Switch to this menu - use switchingMenus flag to prevent onClose from resetting state
                    switchingMenus = true;
                    activeMenu->hide();
                    OverlayManager::instance().unregisterOverlay(activeMenu);
                    switchingMenus = false;

                    popup->show(btnBounds.x, btnBounds.bottom());
                    OverlayManager::instance().registerOverlay(popup, ZOrder::POPUP_MENU,
                        [this]() { closeActiveMenu(); });
                    activeMenu = popup;
                    getAppState().needsRedraw = true;
                    break;
                }
            }
        }
        return Panel::onMouseMove(e);
    }

    void closeActiveMenu() {
        if (activeMenu) {
            switchingMenus = true;
            activeMenu->hide();
            switchingMenus = false;
            OverlayManager::instance().unregisterOverlay(activeMenu);
            activeMenu = nullptr;
            menuModeActive = false;
            lastMenuCloseTime = Platform::getMilliseconds();  // Record when menu was closed
            getAppState().needsRedraw = true;
        }
    }

    // Enable/disable menus by name (Edit, Canvas, Layer, Select, View)
    void setDocumentMenusEnabled(bool enabled) {
        // Menus are in order: File(0), Edit(1), Canvas(2), Layer(3), Select(4), View(5), Help(6)
        // Disable Edit, Canvas, Layer, Select, View when no document
        const std::vector<size_t> documentMenuIndices = {1, 2, 3, 4, 5};
        for (size_t idx : documentMenuIndices) {
            if (idx < menus.size()) {
                menus[idx].first->enabled = enabled;
            }
        }
    }

private:
    bool switchingMenus = false;  // Flag to prevent onClose from resetting state during menu switch

public:

    PopupMenu* createFileMenu() {
        auto menu = createChild<PopupMenu>();
        menu->addItem("New", "", [this]() {
            if (onNewDocument) onNewDocument();
        });
        menu->addSeparator();
        menu->addItem("Open", "", [this]() {
            getAppState().requestOpenFileDialog("Open Image", "*.png *.jpg *.jpeg *.bmp *.gif *.pp",
                [this](const std::string& path) {
                    if (path.empty()) return;
                    AppState& state = getAppState();
                    std::unique_ptr<Document> doc;
                    if (Platform::getFileExtension(path) == ".pp") {
                        doc = ProjectFile::load(path);
                    } else {
                        doc = ImageIO::loadAsDocument(path);
                    }
                    if (doc) {
                        doc->filePath = path;
                        doc->name = Platform::getFileName(path);

                        // Register embedded fonts with FontRenderer
                        for (const auto& [fontName, fontData] : doc->embeddedFonts) {
                            FontRenderer::instance().loadCustomFont(fontName, fontData.data(), static_cast<i32>(fontData.size()));
                        }

                        Document* docPtr = doc.get();
                        state.documents.push_back(std::move(doc));
                        state.setActiveDocument(docPtr);  // Triggers connectToDocument via callback
                        state.needsRedraw = true;
                    }
                });
        });
        menu->addSeparator();
        menu->addItem("Close", "", []() {
            AppState& state = getAppState();
            if (state.activeDocument) {
                state.closeDocument(state.activeDocument);
                state.needsRedraw = true;
            }
        });
        menu->addItem("Close All", "", []() {
            AppState& state = getAppState();
            while (!state.documents.empty()) {
                state.closeDocument(0);
            }
            state.needsRedraw = true;
        });
        menu->addSeparator();
        menu->addItem("Save", "", []() {
            AppState& state = getAppState();
            Document* doc = state.activeDocument;
            if (!doc) return;

            std::string path = doc->filePath;
            if (!path.empty() && Platform::getFileExtension(path) == ".pp") {
                // Already have a valid path, save immediately
                ProjectFile::save(path, *doc);
            } else {
                // Need to prompt for save location
                std::string defaultName = doc->name + ".pp";
                state.requestSaveFileDialog("Save Project", defaultName, "*.pp",
                    [](const std::string& path) {
                        if (path.empty()) return;
                        AppState& state = getAppState();
                        Document* doc = state.activeDocument;
                        if (!doc) return;
                        if (ProjectFile::save(path, *doc)) {
                            doc->filePath = path;
                        }
                    });
            }
        });
        menu->addItem("Export", "", []() {
            AppState& state = getAppState();
            Document* doc = state.activeDocument;
            if (!doc) return;

            std::string defaultName = doc->name;
            if (defaultName.find('.') != std::string::npos) {
                defaultName = defaultName.substr(0, defaultName.rfind('.'));
            }
            defaultName += ".png";

            state.requestSaveFileDialog("Export PNG", defaultName, "*.png",
                [](const std::string& path) {
                    if (path.empty()) return;
                    Document* doc = getAppState().activeDocument;
                    if (doc) {
                        ImageIO::exportPNG(path, *doc);
                    }
                });
        });
        menu->addSeparator();
        menu->addItem("Quit", "", []() {
            getAppState().running = false;
        });
        return menu;
    }

    PopupMenu* createEditMenu() {
        auto menu = createChild<PopupMenu>();
        menu->addItem("Cut", "", []() {
            Document* doc = getAppState().activeDocument;
            if (doc) {
                doc->cut();
                getAppState().needsRedraw = true;
            }
        });
        menu->addItem("Copy", "", []() {
            Document* doc = getAppState().activeDocument;
            if (doc) {
                doc->copy();
            }
        });
        menu->addItem("Paste", "", []() {
            Document* doc = getAppState().activeDocument;
            if (doc) {
                doc->paste();
                getAppState().needsRedraw = true;
            }
        });
        menu->addItem("Paste in Place", "", []() {
            Document* doc = getAppState().activeDocument;
            if (doc) {
                doc->pasteInPlace();
                getAppState().needsRedraw = true;
            }
        });
        menu->addSeparator();
        menu->addItem("Rename Document...", "", [this]() {
            if (onRenameDocument) onRenameDocument();
        });
        return menu;
    }

    PopupMenu* createCanvasMenu() {
        auto menu = createChild<PopupMenu>();
        menu->addItem("Rotate Left", "", []() {
            Document* doc = getAppState().activeDocument;
            if (doc) {
                doc->rotateLeft();
                getAppState().needsRedraw = true;
            }
        });
        menu->addItem("Rotate Right", "", []() {
            Document* doc = getAppState().activeDocument;
            if (doc) {
                doc->rotateRight();
                getAppState().needsRedraw = true;
            }
        });
        menu->addSeparator();
        menu->addItem("Flip Horizontal", "", []() {
            Document* doc = getAppState().activeDocument;
            if (doc) {
                doc->flipHorizontal();
                getAppState().needsRedraw = true;
            }
        });
        menu->addItem("Flip Vertical", "", []() {
            Document* doc = getAppState().activeDocument;
            if (doc) {
                doc->flipVertical();
                getAppState().needsRedraw = true;
            }
        });
        menu->addSeparator();
        menu->addItem("Canvas Size...", "", [this]() {
            if (onCanvasSize) onCanvasSize();
        });
        return menu;
    }

    PopupMenu* createLayerMenu() {
        auto menu = createChild<PopupMenu>();
        menu->addItem("Rotate Left", "", []() {
            Document* doc = getAppState().activeDocument;
            if (doc) {
                doc->rotateLayerLeft();
                getAppState().needsRedraw = true;
            }
        });
        menu->addItem("Rotate Right", "", []() {
            Document* doc = getAppState().activeDocument;
            if (doc) {
                doc->rotateLayerRight();
                getAppState().needsRedraw = true;
            }
        });
        menu->addItem("Flip Horizontal", "", []() {
            Document* doc = getAppState().activeDocument;
            if (doc) {
                doc->flipLayerHorizontal();
                getAppState().needsRedraw = true;
            }
        });
        menu->addItem("Flip Vertical", "", []() {
            Document* doc = getAppState().activeDocument;
            if (doc) {
                doc->flipLayerVertical();
                getAppState().needsRedraw = true;
            }
        });
        menu->addSeparator();
        menu->addItem("Merge Down", "", []() {
            Document* doc = getAppState().activeDocument;
            if (doc && doc->activeLayerIndex >= 0) {
                doc->mergeDown(doc->activeLayerIndex);
            }
        });
        menu->addItem("Merge Visible", "", []() {
            Document* doc = getAppState().activeDocument;
            if (doc) {
                doc->mergeVisible();
            }
        });
        menu->addSeparator();
        menu->addItem("Move Up", "", []() {
            Document* doc = getAppState().activeDocument;
            if (doc && doc->activeLayerIndex >= 0) {
                doc->moveLayer(doc->activeLayerIndex, doc->activeLayerIndex + 1);
            }
        });
        menu->addItem("Move Down", "", []() {
            Document* doc = getAppState().activeDocument;
            if (doc && doc->activeLayerIndex >= 0) {
                doc->moveLayer(doc->activeLayerIndex, doc->activeLayerIndex - 1);
            }
        });
        return menu;
    }

    PopupMenu* createSelectMenu() {
        auto menu = createChild<PopupMenu>();
        menu->addItem("Select All", "", []() {
            Document* doc = getAppState().activeDocument;
            if (doc) {
                doc->selectAll();
            }
        });
        menu->addItem("Deselect", "", []() {
            Document* doc = getAppState().activeDocument;
            if (doc) {
                doc->deselect();
            }
        });
        menu->addItem("Invert", "", []() {
            Document* doc = getAppState().activeDocument;
            if (doc) {
                doc->invertSelection();
            }
        });
        return menu;
    }

    PopupMenu* createAdjustmentMenu() {
        auto menu = createChild<PopupMenu>();

        auto addAdjustment = [](AdjustmentType type) {
            Document* doc = getAppState().activeDocument;
            if (doc) {
                doc->addAdjustmentLayer(type);
                getAppState().needsRedraw = true;
            }
        };

        menu->addItem("Brightness & Contrast", "", [=]() { addAdjustment(AdjustmentType::BrightnessContrast); });
        menu->addItem("Temperature & Tint", "", [=]() { addAdjustment(AdjustmentType::TemperatureTint); });
        menu->addItem("Hue & Saturation", "", [=]() { addAdjustment(AdjustmentType::HueSaturation); });
        menu->addItem("Vibrance", "", [=]() { addAdjustment(AdjustmentType::Vibrance); });
        menu->addItem("Color Balance", "", [=]() { addAdjustment(AdjustmentType::ColorBalance); });
        menu->addItem("Highlights & Shadows", "", [=]() { addAdjustment(AdjustmentType::HighlightsShadows); });
        menu->addItem("Exposure", "", [=]() { addAdjustment(AdjustmentType::Exposure); });
        menu->addSeparator();
        menu->addItem("Levels", "", [=]() { addAdjustment(AdjustmentType::Levels); });
        menu->addSeparator();
        menu->addItem("Invert", "", [=]() { addAdjustment(AdjustmentType::Invert); });
        menu->addItem("Black & White", "", [=]() { addAdjustment(AdjustmentType::BlackAndWhite); });

        return menu;
    }

    PopupMenu* createViewMenu() {
        auto menu = createChild<PopupMenu>();
        menu->addItem("Navigation Panel", "", []() {
            getAppState().showNavigator = !getAppState().showNavigator;
            getAppState().needsRedraw = true;
        });
        menu->addItem("Properties Panel", "", []() {
            getAppState().showProperties = !getAppState().showProperties;
            getAppState().needsRedraw = true;
        });
        menu->addItem("Layers Panel", "", []() {
            getAppState().showLayers = !getAppState().showLayers;
            getAppState().needsRedraw = true;
        });
        menu->addSeparator();
        menu->addItem("Fit Screen", "", [this]() {
            if (onFitToScreen) onFitToScreen();
        });
        menu->addItem("Zoom In", "");
        menu->addItem("Zoom Out", "");
        return menu;
    }

    PopupMenu* createHelpMenu() {
        auto menu = createChild<PopupMenu>();
        menu->addItem("GitHub", "", []() {
            Platform::launchBrowser("https://github.com/gszauer/PixelPlacer");
        });
        return menu;
    }

    void addMenu(HBoxLayout* layout, const char* name, PopupMenu* popup) {
        auto btn = layout->createChild<Button>(name);

        // Use larger font for menu bar items
        btn->fontSize = Config::menuFontSize();

        // Calculate width dynamically: padding + text width + padding
        constexpr f32 MENU_ITEM_SIDE_PADDING = 12.0f;
        Vec2 textSize = FontRenderer::instance().measureText(name, Config::menuFontSize());
        f32 buttonWidth = (MENU_ITEM_SIDE_PADDING * 2 + textSize.x);
        btn->preferredSize = Vec2(buttonWidth, Config::menuBarHeight());

        btn->normalColor = Config::COLOR_TITLEBAR;
        btn->borderColor = 0;

        menus.push_back({btn, popup});

        popup->onClose = [this]() {
            // Only reset state if we're not in the middle of switching menus
            if (!switchingMenus) {
                if (activeMenu) {
                    OverlayManager::instance().unregisterOverlay(activeMenu);
                }
                activeMenu = nullptr;
                menuModeActive = false;
            }
        };

        btn->onClick = [this, btn, popup]() {
            // Check if this menu was just closed (within 100ms) - if so, don't reopen
            u64 now = Platform::getMilliseconds();
            if (now - lastMenuCloseTime < 100) {
                return;  // Menu was just closed by click-outside, don't reopen
            }

            if (activeMenu == popup && popup->visible) {
                closeActiveMenu();
            } else {
                if (activeMenu) {
                    activeMenu->hide();
                    OverlayManager::instance().unregisterOverlay(activeMenu);
                }
                Rect btnBounds = btn->globalBounds();
                popup->show(btnBounds.x, btnBounds.bottom());

                // Register with overlay manager (handles click-outside and rendering)
                OverlayManager::instance().registerOverlay(popup, ZOrder::POPUP_MENU,
                    [this]() { closeActiveMenu(); });

                activeMenu = popup;
                menuModeActive = true;
            }
        };
    }

};

// Status bar
class StatusBar : public Panel {
public:
    Label* positionLabel = nullptr;
    Button* zoomButton = nullptr;
    Label* sizeLabel = nullptr;
    Label* memoryLabel = nullptr;

    // Separators before collapsible elements (to hide with them)
    Widget* sizeSeparator = nullptr;
    Widget* memorySeparator = nullptr;

    // UI Scale controls
    Label* scaleLabel = nullptr;
    Slider* scaleSlider = nullptr;
    Button* scale1xBtn = nullptr;
    Button* scale2xBtn = nullptr;
    Button* scale4xBtn = nullptr;

    // Callbacks
    std::function<void()> onFitToScreen;
    std::function<void(f32)> onScaleChanged;  // Called when UI scale changes

    StatusBar() {
        bgColor = Config::COLOR_PANEL;
        preferredSize = Vec2(0, Config::statusBarHeight());
        verticalPolicy = SizePolicy::Fixed;
        setPadding(4 * Config::uiScale);

        auto layout = createChild<HBoxLayout>(8 * Config::uiScale);
        constexpr f32 LABEL_PADDING = 4.0f;
        constexpr f32 BTN_PADDING = 8.0f;
        f32 itemHeight = 20 * Config::uiScale;

        // Position label - min size for "X: 0, Y: 0", grows for larger coords
        positionLabel = layout->createChild<Label>("X: 0, Y: 0");
        {
            Vec2 minTextSize = FontRenderer::instance().measureText("X: -999, Y: -999", Config::defaultFontSize());
            positionLabel->minSize = Vec2(minTextSize.x + LABEL_PADDING * 2, itemHeight);
            positionLabel->preferredSize = positionLabel->minSize;
        }

        layout->createChild<Separator>(false);

        // Zoom button - sized for widest expected "6400%"
        zoomButton = layout->createChild<Button>("100%");
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

        // Collapsible: size label - sized for "99999 x 99999"
        sizeSeparator = layout->createChild<Separator>(false);
        sizeLabel = layout->createChild<Label>("1920 x 1080");
        {
            Vec2 textSize = FontRenderer::instance().measureText("99999 x 99999", Config::defaultFontSize());
            sizeLabel->preferredSize = Vec2(textSize.x + LABEL_PADDING * 2, itemHeight);
        }

        // Collapsible: memory label - sized for "9999 MB"
        memorySeparator = layout->createChild<Separator>(false);
        memoryLabel = layout->createChild<Label>("0 MB");
        {
            Vec2 textSize = FontRenderer::instance().measureText("9999 MB", Config::defaultFontSize());
            memoryLabel->preferredSize = Vec2(textSize.x + LABEL_PADDING * 2, itemHeight);
        }

        layout->createChild<Spacer>();

        // UI Scale controls (right side)
        layout->createChild<Separator>(false);

        scaleLabel = layout->createChild<Label>("UI Scale");
        {
            Vec2 textSize = FontRenderer::instance().measureText("UI Scale", Config::defaultFontSize());
            scaleLabel->preferredSize = Vec2(textSize.x + LABEL_PADDING * 2, itemHeight);
        }

        scaleSlider = layout->createChild<Slider>(0.5f, 4.0f, Config::uiScale);  // Min 0.5 to prevent crashes
        scaleSlider->preferredSize = Vec2(80 * Config::uiScale, itemHeight);
        scaleSlider->onDragEnd = [this]() {
            if (onScaleChanged) onScaleChanged(scaleSlider->value);
        };

        scale1xBtn = layout->createChild<Button>("1x");
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

        scale2xBtn = layout->createChild<Button>("2x");
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

        scale4xBtn = layout->createChild<Button>("4x");
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

    void layout() override {
        // Calculate widths for responsive layout
        f32 availableWidth = bounds.w;
        f32 padding = 4 * Config::uiScale;
        f32 spacing = 8 * Config::uiScale;  // Match HBoxLayout spacing
        f32 separatorWidth = 1;  // Separator is 1px wide

        // Essential elements (always visible): position, zoom separator, zoom, spacer, scale separator, scale controls
        f32 essentialWidth = padding * 2 +
            positionLabel->preferredSize.x + spacing + separatorWidth + spacing +
            zoomButton->preferredSize.x + spacing +
            // spacer takes remaining space
            separatorWidth + spacing +
            scaleLabel->preferredSize.x + spacing +
            scaleSlider->preferredSize.x + spacing +
            scale1xBtn->preferredSize.x + spacing +
            scale2xBtn->preferredSize.x + spacing +
            scale4xBtn->preferredSize.x;

        // Width needed for size label
        f32 sizeWidth = separatorWidth + spacing + sizeLabel->preferredSize.x + spacing;

        // Width needed for memory label
        f32 memoryWidth = separatorWidth + spacing + memoryLabel->preferredSize.x + spacing;

        // Determine what to show based on available width
        bool showMemory = (availableWidth >= essentialWidth + sizeWidth + memoryWidth);
        bool showSize = (availableWidth >= essentialWidth + sizeWidth);

        // Hide/show collapsible elements
        if (memoryLabel) memoryLabel->visible = showMemory;
        if (memorySeparator) memorySeparator->visible = showMemory;
        if (sizeLabel) sizeLabel->visible = showSize;
        if (sizeSeparator) sizeSeparator->visible = showSize;

        // Call parent layout
        Panel::layout();
    }

    void update(const Vec2& mousePos, f32 zoom, u32 width, u32 height, size_t memBytes) {
        if (positionLabel) {
            positionLabel->setText("X: " + std::to_string(static_cast<i32>(mousePos.x)) +
                                  ", Y: " + std::to_string(static_cast<i32>(mousePos.y)));
        }
        if (zoomButton && zoomButton->enabled) {
            // Round to match getZoomString() for consistency
            zoomButton->text = std::to_string(static_cast<i32>(zoom * 100 + 0.5f)) + "%";
        }
        if (sizeLabel) {
            sizeLabel->setText(std::to_string(width) + " x " + std::to_string(height));
        }
        if (memoryLabel) {
            memoryLabel->setText(std::to_string(memBytes / (1024 * 1024)) + " MB");
        }
    }

    void setEnabled(bool isEnabled) {
        if (zoomButton) {
            zoomButton->enabled = isEnabled;
            if (!isEnabled) {
                zoomButton->text = "0%";
                zoomButton->hovered = false;  // Clear hover state
            }
        }
    }
};

// Vertical resize divider for resizing panels
class ResizeDivider : public Widget {
public:
    bool dragging = false;
    f32 dragStartX = 0;
    f32 dragStartWidth = 0;
    Widget* targetWidget = nullptr;  // Widget to resize (right sidebar)
    f32 minWidth = 150.0f * Config::uiScale;
    f32 maxWidth = 600.0f * Config::uiScale;

    std::function<void()> onResized;

    ResizeDivider() {
        preferredSize = Vec2(6 * Config::uiScale, 0);
        horizontalPolicy = SizePolicy::Fixed;
        verticalPolicy = SizePolicy::Expanding;
    }

    void renderSelf(Framebuffer& fb) override {
        Rect gb = globalBounds();

        // Draw divider background
        u32 color = (dragging || hovered) ? Config::COLOR_HOVER : Config::COLOR_BORDER;
        Recti rect(
            static_cast<i32>(gb.x),
            static_cast<i32>(gb.y),
            static_cast<i32>(gb.w),
            static_cast<i32>(gb.h)
        );
        fb.fillRect(rect, color);

        // Draw grip dots in the middle
        i32 cx = static_cast<i32>(gb.x + gb.w / 2);
        i32 cy = static_cast<i32>(gb.y + gb.h / 2);
        u32 dotColor = Config::COLOR_TEXT_DIM;
        i32 dotSpacing = static_cast<i32>(8 * Config::uiScale);

        for (i32 i = -2; i <= 2; ++i) {
            i32 dy = i * dotSpacing;
            fb.fillRect(cx - 1, cy + dy - 1, 2, 2, dotColor);
        }
    }

    bool onMouseDown(const MouseEvent& e) override {
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

    bool onMouseDrag(const MouseEvent& e) override {
        if (dragging && targetWidget) {
            // Dragging left makes panel wider, right makes it narrower
            f32 deltaX = dragStartX - e.globalPosition.x;
            f32 newWidth = dragStartWidth + deltaX;

            // Clamp to min/max
            newWidth = std::max(minWidth, std::min(maxWidth, newWidth));

            targetWidget->preferredSize.x = newWidth;

            // Trigger relayout
            if (onResized) onResized();
            getAppState().needsRedraw = true;
            return true;
        }
        return false;
    }

    bool onMouseUp(const MouseEvent& e) override {
        if (dragging) {
            dragging = false;
            getAppState().capturedWidget = nullptr;
            getAppState().needsRedraw = true;
            return true;
        }
        return false;
    }

    void onMouseEnter(const MouseEvent& e) override {
        hovered = true;
        getAppState().needsRedraw = true;
    }

    void onMouseLeave(const MouseEvent& e) override {
        hovered = false;
        getAppState().needsRedraw = true;
    }
};

// Horizontal resize divider for resizing panels vertically (drag up/down)
class VPanelResizer : public Widget {
public:
    bool dragging = false;
    f32 dragStartY = 0;
    f32 dragStartHeightAbove = 0;
    f32 dragStartHeightBelow = 0;
    Widget* aboveWidget = nullptr;  // Widget above (gets smaller when dragging down)
    Widget* belowWidget = nullptr;  // Widget below (gets larger when dragging down)
    f32 minHeight = 50.0f * Config::uiScale;

    std::function<void()> onResized;

    VPanelResizer() {
        preferredSize = Vec2(0, 6 * Config::uiScale);
        horizontalPolicy = SizePolicy::Expanding;
        verticalPolicy = SizePolicy::Fixed;
    }

    void renderSelf(Framebuffer& fb) override {
        Rect gb = globalBounds();

        // Draw divider background
        u32 color = (dragging || hovered) ? Config::COLOR_HOVER : Config::COLOR_BORDER;
        Recti rect(
            static_cast<i32>(gb.x),
            static_cast<i32>(gb.y),
            static_cast<i32>(gb.w),
            static_cast<i32>(gb.h)
        );
        fb.fillRect(rect, color);

        // Draw grip dots in the middle (horizontal)
        i32 cx = static_cast<i32>(gb.x + gb.w / 2);
        i32 cy = static_cast<i32>(gb.y + gb.h / 2);
        u32 dotColor = Config::COLOR_TEXT_DIM;
        i32 dotSpacing = static_cast<i32>(8 * Config::uiScale);

        for (i32 i = -2; i <= 2; ++i) {
            i32 dx = i * dotSpacing;
            fb.fillRect(cx + dx - 1, cy - 1, 2, 2, dotColor);
        }
    }

    bool onMouseDown(const MouseEvent& e) override {
        if (e.button == MouseButton::Left && aboveWidget && belowWidget) {
            dragging = true;
            dragStartY = e.globalPosition.y;

            // Use actual bounds heights (not preferredSize which may not match)
            dragStartHeightAbove = aboveWidget->bounds.h;
            dragStartHeightBelow = belowWidget->bounds.h;

            // Switch to Fixed policy so layout respects our height changes
            aboveWidget->verticalPolicy = SizePolicy::Fixed;
            belowWidget->verticalPolicy = SizePolicy::Fixed;

            // Set preferredSize to current actual heights
            aboveWidget->preferredSize.y = dragStartHeightAbove;
            belowWidget->preferredSize.y = dragStartHeightBelow;

            getAppState().capturedWidget = this;
            getAppState().needsRedraw = true;
            return true;
        }
        return false;
    }

    bool onMouseDrag(const MouseEvent& e) override {
        if (dragging && aboveWidget && belowWidget) {
            f32 deltaY = e.globalPosition.y - dragStartY;

            // Calculate new heights
            f32 newHeightAbove = dragStartHeightAbove + deltaY;
            f32 newHeightBelow = dragStartHeightBelow - deltaY;

            // Enforce minimum heights
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

            // Apply new heights
            aboveWidget->preferredSize.y = newHeightAbove;
            belowWidget->preferredSize.y = newHeightBelow;

            // Trigger relayout
            if (onResized) onResized();
            getAppState().needsRedraw = true;
            return true;
        }
        return false;
    }

    bool onMouseUp(const MouseEvent& e) override {
        if (dragging) {
            dragging = false;
            getAppState().capturedWidget = nullptr;
            getAppState().needsRedraw = true;
            return true;
        }
        return false;
    }

    void onMouseEnter(const MouseEvent& e) override {
        hovered = true;
        getAppState().needsRedraw = true;
    }

    void onMouseLeave(const MouseEvent& e) override {
        hovered = false;
        getAppState().needsRedraw = true;
    }
};

// Main window widget
class MainWindow : public Widget {
public:
    MenuBar* menuBar = nullptr;
    ToolOptionsBar* toolOptions = nullptr;
    ToolPalette* toolPalette = nullptr;
    TabBar* tabBar = nullptr;
    DocumentViewWidget* docView = nullptr;
    ResizeDivider* sidebarDivider = nullptr;
    VBoxLayout* rightSidebar = nullptr;
    NavigatorPanel* navigatorPanel = nullptr;
    VPanelResizer* navPropsResizer = nullptr;  // Between navigator and layer props
    LayerPropsPanel* layerPropsPanel = nullptr;
    VPanelResizer* propsLayerResizer = nullptr;  // Between layer props and layer panel
    LayerPanel* layerPanel = nullptr;
    StatusBar* statusBar = nullptr;

    // Dialogs
    NewDocumentDialog* newDocDialog = nullptr;
    CanvasSizeDialog* canvasSizeDialog = nullptr;
    RenameDocumentDialog* renameDocDialog = nullptr;
    ColorPickerDialog* colorPickerDialog = nullptr;
    PressureCurvePopup* pressureCurvePopup = nullptr;
    u64 pressureCurvePopupCloseTime = 0;  // For toggle detection
    NewBrushDialog* newBrushDialog = nullptr;
    ManageBrushesPopup* manageBrushesPopup = nullptr;
    u64 manageBrushesPopupCloseTime = 0;  // For toggle detection
    BrushTipSelectorPopup* brushTipPopup = nullptr;
    u64 brushTipPopupCloseTime = 0;  // For toggle detection

    // Track whether editing foreground or background color
    bool editingForegroundColor = true;

    // Track panel visibility state for change detection
    bool prevShowNavigator = true;
    bool prevShowProperties = true;
    bool prevShowLayers = true;

    MainWindow() {
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

    void createDialogs() {
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
        canvasSizeDialog->onConfirm = [this](u32 width, u32 height, i32 anchorX, i32 anchorY) {
            Document* doc = getAppState().activeDocument;
            if (doc) {
                doc->resizeCanvas(width, height, anchorX, anchorY);
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
    }

    void buildUI() {
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

                // Read font file
                std::ifstream file(path, std::ios::binary | std::ios::ate);
                if (!file.is_open()) return;

                std::streamsize size = file.tellg();
                file.seekg(0, std::ios::beg);

                std::vector<u8> fontData(size);
                if (!file.read(reinterpret_cast<char*>(fontData.data()), size)) return;

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

    // Called after event processing to apply deferred UI changes safely
    void applyDeferredChanges() {
        if (toolOptions) {
            toolOptions->update();  // Sync checkbox states, etc.
            toolOptions->applyPendingChanges();
        }
    }

    void connectToDocument() {
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

    void syncTabs() {
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

    void switchToDocument(i32 index) {
        AppState& state = getAppState();
        if (index >= 0 && index < static_cast<i32>(state.documents.size())) {
            state.setActiveDocument(index);
            connectToDocument();
            state.needsRedraw = true;
        }
    }

    void closeDocumentTab(i32 index) {
        AppState& state = getAppState();
        if (index >= 0 && index < static_cast<i32>(state.documents.size())) {
            state.closeDocument(index);
            connectToDocument();
            state.needsRedraw = true;
        }
    }

    void addDocumentTab(Document* doc) {
        if (!doc) return;
        tabBar->addTab(doc->name, doc, true);
        tabBar->setActiveTab(static_cast<i32>(tabBar->tabs.size()) - 1);
    }

    void layout() override {
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

    void clampSidebarWidth() {
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

    void repositionDialogs() {
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
        // Note: pressureCurvePopup and manageBrushesPopup are non-modal, positioned relative to button
    }

    Dialog* getActiveDialog() {
        if (newDocDialog && newDocDialog->visible) return newDocDialog;
        if (canvasSizeDialog && canvasSizeDialog->visible) return canvasSizeDialog;
        if (colorPickerDialog && colorPickerDialog->visible) return colorPickerDialog;
        if (newBrushDialog && newBrushDialog->visible) return newBrushDialog;
        if (renameDocDialog && renameDocDialog->visible) return renameDocDialog;
        // Note: popups are non-modal, not included here
        return nullptr;
    }

    bool onMouseDown(const MouseEvent& e) override {
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

    bool onMouseMove(const MouseEvent& e) override {
        return Widget::onMouseMove(e);
    }

    void centerDialog(Dialog* dialog) {
        if (!dialog) return;
        f32 x = (bounds.w - dialog->preferredSize.x) / 2;
        f32 y = (bounds.h - dialog->preferredSize.y) / 2;
        dialog->setBounds(x, y, dialog->preferredSize.x, dialog->preferredSize.y);
        dialog->layout();
    }

    void showNewDocumentDialog() {
        centerDialog(newDocDialog);
        newDocDialog->show();
        OverlayManager::instance().registerOverlay(newDocDialog, ZOrder::MODAL_DIALOG, true);
    }

    void showCanvasSizeDialog() {
        centerDialog(canvasSizeDialog);
        canvasSizeDialog->show();
        OverlayManager::instance().registerOverlay(canvasSizeDialog, ZOrder::MODAL_DIALOG, true);
    }

    void showColorPickerDialog(const Color& initialColor) {
        colorPickerDialog->setColor(initialColor);
        centerDialog(colorPickerDialog);
        colorPickerDialog->show();
        OverlayManager::instance().registerOverlay(colorPickerDialog, ZOrder::MODAL_DIALOG, true);
    }

    void showPressureCurvePopup(f32 x, f32 y) {
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

    void showNewBrushDialog(bool fromCurrentCanvas) {
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

    void showRenameDocumentDialog() {
        if (!getAppState().activeDocument) return;
        centerDialog(renameDocDialog);
        renameDocDialog->show();
        OverlayManager::instance().registerOverlay(renameDocDialog, ZOrder::MODAL_DIALOG, true);
    }

    void showManageBrushesPopup(f32 rightEdge, f32 y) {
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

        // Right-align popup with button (x = rightEdge - popupWidth)
        f32 x = rightEdge - manageBrushesPopup->preferredSize.x;
        manageBrushesPopup->show(x, y);
        OverlayManager::instance().registerOverlay(manageBrushesPopup, ZOrder::POPUP_MENU,
            [this]() {
                manageBrushesPopup->hide();
                manageBrushesPopupCloseTime = Platform::getMilliseconds();
            });
    }

    void showBrushTipPopup(f32 x, f32 y) {
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

    void render(Framebuffer& fb) override {
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
                doc->height,
                doc->getMemoryUsage()
            );
        }

        Widget::render(fb);
        // Note: Overlays (popups, dialogs) are rendered by OverlayManager in Application::render()
    }
};

#endif
