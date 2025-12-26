#ifndef _H_BRUSH_DIALOGS_
#define _H_BRUSH_DIALOGS_

#include "dialogs.h"
#include "layer.h"
#include "image_io.h"

// Pressure curve editor widget - displays and allows editing bezier curve
class PressureCurveWidget : public Widget {
public:
    Vec2 cp1 = Vec2(0.33f, 0.33f);  // Control point 1
    Vec2 cp2 = Vec2(0.66f, 0.66f);  // Control point 2

    i32 draggingPoint = -1;  // -1 = none, 0 = cp1, 1 = cp2
    bool showAxisLabels = false;  // Whether to show "Out" and "Input" labels

    std::function<void()> onChanged;

    PressureCurveWidget() {
    }

    // Get graph area bounds accounting for axis labels
    void getGraphBounds(Rect& global, f32& leftMargin, f32& topMargin, f32& graphW, f32& graphH) const {
        global = globalBounds();
        f32 margin = 8 * Config::uiScale;
        leftMargin = showAxisLabels ? (20 * Config::uiScale) : margin;  // Extra space for vertical "Out" label
        f32 bottomMargin = showAxisLabels ? (18 * Config::uiScale) : margin;  // Extra space for "Input" label
        topMargin = margin;
        f32 rightMargin = margin;
        graphW = global.w - leftMargin - rightMargin;
        graphH = global.h - topMargin - bottomMargin;
    }

    // Convert normalized coords (0-1) to global screen pixels
    Vec2 toPixel(const Vec2& normalized) const {
        Rect global;
        f32 leftMargin, topMargin, graphW, graphH;
        getGraphBounds(global, leftMargin, topMargin, graphW, graphH);
        return Vec2(
            global.x + leftMargin + normalized.x * graphW,
            global.y + topMargin + (1.0f - normalized.y) * graphH  // Flip Y
        );
    }

    // Convert global screen pixels to normalized coords (0-1)
    Vec2 toNormalized(const Vec2& pixel) const {
        Rect global;
        f32 leftMargin, topMargin, graphW, graphH;
        getGraphBounds(global, leftMargin, topMargin, graphW, graphH);
        f32 nx = (pixel.x - global.x - leftMargin) / graphW;
        f32 ny = 1.0f - (pixel.y - global.y - topMargin) / graphH;  // Flip Y
        return Vec2(
            std::max(0.0f, std::min(1.0f, nx)),
            std::max(0.0f, std::min(1.0f, ny))
        );
    }

    void renderSelf(Framebuffer& fb) override {
        Widget::renderSelf(fb);

        Rect global;
        f32 leftMargin, topMargin, gw, gh;
        getGraphBounds(global, leftMargin, topMargin, gw, gh);

        i32 graphX = static_cast<i32>(global.x + leftMargin);
        i32 graphY = static_cast<i32>(global.y + topMargin);
        i32 graphW = static_cast<i32>(gw);
        i32 graphH = static_cast<i32>(gh);

        // Draw graph background
        fb.fillRect(graphX, graphY, graphW, graphH, 0x252525FF);

        // Draw grid lines (subtle)
        u32 gridColor = 0x383838FF;
        for (int i = 1; i < 4; i++) {
            i32 gx = graphX + graphW * i / 4;
            i32 gy = graphY + graphH * i / 4;
            fb.drawVerticalLine(gx, graphY, graphY + graphH, gridColor);
            fb.drawHorizontalLine(graphX, graphX + graphW, gy, gridColor);
        }

        // Draw diagonal reference line (linear) - dashed effect
        u32 refColor = 0x505050FF;
        for (i32 i = 0; i < graphW; i += 2) {  // Skip every other pixel for dashed look
            i32 px = graphX + i;
            i32 py = graphY + graphH - i * graphH / graphW;
            if (py >= graphY && py < graphY + graphH) {
                fb.setPixel(px, py, refColor);
            }
        }

        // Draw bezier curve (thicker - draw multiple times offset)
        u32 curveColor = 0x4A90D9FF;  // Blue accent color
        for (int offset = -1; offset <= 1; offset++) {
            Vec2 prev = toPixel(Vec2(0, 0));
            for (int i = 1; i <= 64; i++) {
                f32 t = i / 64.0f;

                // Cubic bezier calculation
                f32 mt = 1.0f - t;
                f32 mt2 = mt * mt;
                f32 t2 = t * t;
                f32 t3 = t2 * t;

                // P = (1-t)³P0 + 3(1-t)²tP1 + 3(1-t)t²P2 + t³P3
                f32 x = 3.0f * mt2 * t * cp1.x + 3.0f * mt * t2 * cp2.x + t3;
                f32 y = 3.0f * mt2 * t * cp1.y + 3.0f * mt * t2 * cp2.y + t3;

                Vec2 curr = toPixel(Vec2(x, y));

                fb.drawLine(
                    static_cast<i32>(prev.x), static_cast<i32>(prev.y) + offset,
                    static_cast<i32>(curr.x), static_cast<i32>(curr.y) + offset,
                    curveColor
                );
                prev = curr;
            }
        }

        // Draw control point handles (lines from endpoints to control points)
        u32 handleColor = 0x606060FF;
        Vec2 p0 = toPixel(Vec2(0, 0));
        Vec2 p1 = toPixel(cp1);
        Vec2 p2 = toPixel(cp2);
        Vec2 p3 = toPixel(Vec2(1, 1));

        fb.drawLine(static_cast<i32>(p0.x), static_cast<i32>(p0.y),
                    static_cast<i32>(p1.x), static_cast<i32>(p1.y), handleColor);
        fb.drawLine(static_cast<i32>(p3.x), static_cast<i32>(p3.y),
                    static_cast<i32>(p2.x), static_cast<i32>(p2.y), handleColor);

        // Draw control point circles
        i32 cpRadius = static_cast<i32>(6 * Config::uiScale);
        u32 cp1Color = (draggingPoint == 0) ? 0xFFAA44FF : 0xFF6600FF;  // Orange
        u32 cp2Color = (draggingPoint == 1) ? 0xAA66FFFF : 0x8844CCFF;  // Purple

        fb.fillCircle(static_cast<i32>(p1.x), static_cast<i32>(p1.y), cpRadius, cp1Color);
        fb.fillCircle(static_cast<i32>(p2.x), static_cast<i32>(p2.y), cpRadius, cp2Color);

        // Draw endpoint circles (fixed)
        i32 endRadius = static_cast<i32>(4 * Config::uiScale);
        fb.fillCircle(static_cast<i32>(p0.x), static_cast<i32>(p0.y), endRadius, 0xAAAAAAFF);
        fb.fillCircle(static_cast<i32>(p3.x), static_cast<i32>(p3.y), endRadius, 0xAAAAAAFF);

        // Draw border
        fb.drawRect(Recti(graphX, graphY, graphW, graphH), 0x606060FF);

        // Draw 0/1 labels at corners
        u32 labelColor = 0x666666FF;
        FontRenderer::instance().renderText(fb, "0", graphX + 3, graphY + graphH - 14, labelColor);
        FontRenderer::instance().renderText(fb, "1", graphX + graphW - 10, graphY + 3, labelColor);

        // Draw axis labels if enabled
        if (showAxisLabels) {
            u32 axisLabelColor = 0x888888FF;

            // Rotated "Out" label on the left (90 degrees CCW, reads bottom-to-top)
            Vec2 outSize = FontRenderer::instance().measureText("Out", Config::defaultFontSize());
            i32 outX = static_cast<i32>(global.x + 2);
            i32 outY = graphY + (graphH + static_cast<i32>(outSize.x)) / 2;  // Center vertically
            FontRenderer::instance().renderTextRotated90(fb, "Out", outX, outY, axisLabelColor);

            // "Input" label below the graph (centered)
            Vec2 inputSize = FontRenderer::instance().measureText("Input", Config::defaultFontSize());
            i32 inputX = graphX + (graphW - static_cast<i32>(inputSize.x)) / 2;
            i32 inputY = graphY + graphH + 3;
            FontRenderer::instance().renderText(fb, "Input", inputX, inputY, axisLabelColor);
        }
    }

    bool onMouseDown(const MouseEvent& e) override {
        if (e.button != MouseButton::Left) return false;

        // Check if clicking on a control point
        Vec2 pos = e.globalPosition;
        Vec2 p1 = toPixel(cp1);
        Vec2 p2 = toPixel(cp2);
        f32 hitRadius = 10 * Config::uiScale;

        f32 d1 = (pos - p1).length();
        f32 d2 = (pos - p2).length();

        if (d1 < hitRadius && d1 < d2) {
            draggingPoint = 0;
            getAppState().capturedWidget = this;
            return true;
        } else if (d2 < hitRadius) {
            draggingPoint = 1;
            getAppState().capturedWidget = this;
            return true;
        }

        return false;
    }

    bool onMouseDrag(const MouseEvent& e) override {
        if (draggingPoint < 0) return false;

        Vec2 pos = e.globalPosition;
        Vec2 normalized = toNormalized(pos);

        if (draggingPoint == 0) {
            cp1 = normalized;
        } else {
            cp2 = normalized;
        }

        getAppState().needsRedraw = true;
        return true;
    }

    bool onMouseUp(const MouseEvent& e) override {
        if (draggingPoint >= 0) {
            draggingPoint = -1;
            getAppState().capturedWidget = nullptr;
            if (onChanged) onChanged();
            return true;
        }
        return false;
    }

    void reset() {
        cp1 = Vec2(0.33f, 0.33f);
        cp2 = Vec2(0.66f, 0.66f);
        getAppState().needsRedraw = true;
    }
};

// Pressure curve dialog
class PressureCurvePopup : public Dialog {
public:
    PressureCurveWidget* curveWidget = nullptr;

    PressureCurvePopup() : Dialog("Pressure Curve") {
        preferredSize = Vec2(280 * Config::uiScale, 320 * Config::uiScale);
        modal = false;  // Non-modal popup
        bgColor = Config::COLOR_PANEL;

        auto layout = createChild<VBoxLayout>(8 * Config::uiScale);

        // Header
        auto header = layout->createChild<Label>("Pressure Curve");
        header->preferredSize = Vec2(0, 20 * Config::uiScale);

        layout->createChild<Separator>();

        // Curve widget (includes axis labels internally)
        curveWidget = layout->createChild<PressureCurveWidget>();
        curveWidget->preferredSize = Vec2(0, 180 * Config::uiScale);
        curveWidget->horizontalPolicy = SizePolicy::Expanding;
        curveWidget->showAxisLabels = true;

        // Apply changes immediately when curve is modified
        curveWidget->onChanged = []() {
            // Curve widget stores values, we sync on any change
        };

        layout->createChild<Separator>();

        // Preset buttons - span full width with equal sizing
        auto presetRow = layout->createChild<HBoxLayout>(4 * Config::uiScale);
        presetRow->preferredSize = Vec2(0, 28 * Config::uiScale);

        auto linearBtn = presetRow->createChild<Button>("Linear");
        linearBtn->horizontalPolicy = SizePolicy::Expanding;
        linearBtn->preferredSize = Vec2(0, 24 * Config::uiScale);
        linearBtn->onClick = [this]() {
            curveWidget->cp1 = Vec2(0.33f, 0.33f);
            curveWidget->cp2 = Vec2(0.66f, 0.66f);
            applyCurve();
        };

        auto softBtn = presetRow->createChild<Button>("Soft");
        softBtn->horizontalPolicy = SizePolicy::Expanding;
        softBtn->preferredSize = Vec2(0, 24 * Config::uiScale);
        softBtn->onClick = [this]() {
            curveWidget->cp1 = Vec2(0.25f, 0.5f);
            curveWidget->cp2 = Vec2(0.5f, 0.9f);
            applyCurve();
        };

        auto hardBtn = presetRow->createChild<Button>("Hard");
        hardBtn->horizontalPolicy = SizePolicy::Expanding;
        hardBtn->preferredSize = Vec2(0, 24 * Config::uiScale);
        hardBtn->onClick = [this]() {
            curveWidget->cp1 = Vec2(0.5f, 0.1f);
            curveWidget->cp2 = Vec2(0.75f, 0.5f);
            applyCurve();
        };

        auto scurveBtn = presetRow->createChild<Button>("S-Curve");
        scurveBtn->horizontalPolicy = SizePolicy::Expanding;
        scurveBtn->preferredSize = Vec2(0, 24 * Config::uiScale);
        scurveBtn->onClick = [this]() {
            curveWidget->cp1 = Vec2(0.25f, 0.1f);
            curveWidget->cp2 = Vec2(0.75f, 0.9f);
            applyCurve();
        };

        layout->createChild<Spacer>();

        // Bottom buttons
        auto btnRow = layout->createChild<HBoxLayout>(8 * Config::uiScale);
        btnRow->preferredSize = Vec2(0, 28 * Config::uiScale);

        auto resetBtn = btnRow->createChild<Button>("Reset");
        resetBtn->preferredSize = Vec2(60 * Config::uiScale, 24 * Config::uiScale);
        resetBtn->onClick = [this]() {
            curveWidget->reset();
            applyCurve();
        };

        btnRow->createChild<Spacer>();

        auto closeBtn = btnRow->createChild<Button>("Close");
        closeBtn->preferredSize = Vec2(60 * Config::uiScale, 24 * Config::uiScale);
        closeBtn->onClick = [this]() { hide(); };
    }

    void applyCurve() {
        AppState& state = getAppState();
        state.pressureCurveCP1 = curveWidget->cp1;
        state.pressureCurveCP2 = curveWidget->cp2;
        state.needsRedraw = true;
    }

    void renderSelf(Framebuffer& fb) override {
        // Draw solid background for popup
        Rect global = globalBounds();
        fb.fillRect(Recti(global), bgColor);
        fb.drawRect(Recti(global), Config::COLOR_BORDER, 1);
    }

    void show() override {
        // Load current curve from app state
        AppState& state = getAppState();
        curveWidget->cp1 = state.pressureCurveCP1;
        curveWidget->cp2 = state.pressureCurveCP2;
        Dialog::show();
    }

    void show(f32 x, f32 y) {
        setBounds(x, y, preferredSize.x, preferredSize.y);
        show();
        layout();
    }

    // Apply curve when hiding (in case user dragged control points)
    void hide() override {
        applyCurve();
        Dialog::hide();
    }
};

// ========== Brush Tip Dialogs ==========

// Preview widget for brush tip alpha mask
class BrushTipPreviewWidget : public Widget {
public:
    std::vector<f32>* alphaMask = nullptr;
    u32 maskWidth = 0;
    u32 maskHeight = 0;

    BrushTipPreviewWidget() {
        preferredSize = Vec2(64 * Config::uiScale, 64 * Config::uiScale);
    }

    void render(Framebuffer& fb) override {
        if (!visible) return;
        Rect global = globalBounds();

        // Draw checkerboard background
        fb.drawCheckerboard(Recti(global));

        if (!alphaMask || maskWidth == 0 || maskHeight == 0) return;

        // Draw the alpha mask scaled to fit
        f32 scaleX = global.w / static_cast<f32>(maskWidth);
        f32 scaleY = global.h / static_cast<f32>(maskHeight);
        f32 scale = std::min(scaleX, scaleY);

        f32 offsetX = global.x + (global.w - maskWidth * scale) / 2;
        f32 offsetY = global.y + (global.h - maskHeight * scale) / 2;

        for (u32 my = 0; my < maskHeight; ++my) {
            for (u32 mx = 0; mx < maskWidth; ++mx) {
                f32 alpha = (*alphaMask)[my * maskWidth + mx];
                if (alpha <= 0.0f) continue;

                i32 px = static_cast<i32>(offsetX + mx * scale);
                i32 py = static_cast<i32>(offsetY + my * scale);
                i32 pw = static_cast<i32>(scale) + 1;
                i32 ph = static_cast<i32>(scale) + 1;

                u8 a = static_cast<u8>(alpha * 255.0f);
                fb.fillRect(px, py, pw, ph, Blend::pack(0, 0, 0, a));
            }
        }
    }
};

// Dialog for creating new brush from file
class NewBrushDialog : public Dialog {
public:
    Panel* headerPanel = nullptr;
    Label* headerLabel = nullptr;
    Widget* fileRow = nullptr;  // Hidden when fromCurrentCanvas
    TextField* pathField = nullptr;
    TextField* nameField = nullptr;
    Checkbox* channelChecks[4] = {nullptr};  // R, G, B, A only
    i32 selectedChannel = 3;  // Default to Alpha
    BrushTipPreviewWidget* previewWidget = nullptr;

    bool fromCurrentCanvas = false;  // True for "Brush from Current"

    std::function<void(std::unique_ptr<CustomBrushTip>)> onBrushCreated;

    // Temporary storage for loaded image
    std::unique_ptr<TiledCanvas> loadedImage;

    NewBrushDialog() : Dialog("New Brush") {
        preferredSize = Vec2(300 * Config::uiScale, 300 * Config::uiScale);

        auto layout = createChild<VBoxLayout>(8 * Config::uiScale);

        // Header with background
        headerPanel = layout->createChild<Panel>();
        headerPanel->bgColor = Config::COLOR_PANEL_HEADER;
        headerPanel->preferredSize = Vec2(0, 28 * Config::uiScale);
        headerPanel->setPadding(4 * Config::uiScale);
        headerLabel = headerPanel->createChild<Label>("New Brush");

        layout->createChild<Separator>();

        // File row (hidden when fromCurrentCanvas)
        fileRow = layout->createChild<HBoxLayout>(8 * Config::uiScale);
        fileRow->preferredSize = Vec2(0, 28 * Config::uiScale);
        static_cast<HBoxLayout*>(fileRow)->createChild<Label>("File:")->preferredSize = Vec2(50 * Config::uiScale, 24 * Config::uiScale);
        pathField = static_cast<HBoxLayout*>(fileRow)->createChild<TextField>();
        pathField->horizontalPolicy = SizePolicy::Expanding;
        pathField->readOnly = true;

        auto browseBtn = static_cast<HBoxLayout*>(fileRow)->createChild<Button>("...");
        browseBtn->preferredSize = Vec2(30 * Config::uiScale, 24 * Config::uiScale);
        browseBtn->onClick = [this]() { browseForFile(); };

        // Name row
        auto nameRow = layout->createChild<HBoxLayout>(8 * Config::uiScale);
        nameRow->preferredSize = Vec2(0, 28 * Config::uiScale);
        nameRow->createChild<Label>("Name:")->preferredSize = Vec2(50 * Config::uiScale, 24 * Config::uiScale);
        nameField = nameRow->createChild<TextField>();
        nameField->text = "Custom Brush";
        nameField->horizontalPolicy = SizePolicy::Expanding;

        // Channel selection (radio-style checkboxes) - R, G, B, A only
        auto channelRow = layout->createChild<HBoxLayout>(4 * Config::uiScale);
        channelRow->preferredSize = Vec2(0, 24 * Config::uiScale);
        channelRow->createChild<Label>("Channel:")->preferredSize = Vec2(55 * Config::uiScale, 20 * Config::uiScale);

        const char* channelNames[4] = {"R", "G", "B", "A"};
        for (i32 i = 0; i < 4; ++i) {
            channelChecks[i] = channelRow->createChild<Checkbox>(channelNames[i], i == 3);  // Alpha default
            channelChecks[i]->preferredSize = Vec2(40 * Config::uiScale, 20 * Config::uiScale);
            i32 channelIndex = i;
            channelChecks[i]->onChanged = [this, channelIndex](bool checked) {
                if (checked) {
                    selectChannel(channelIndex);
                } else if (selectedChannel == channelIndex) {
                    // Don't allow unchecking the selected one
                    channelChecks[channelIndex]->checked = true;
                }
            };
        }

        // Preview
        auto previewRow = layout->createChild<HBoxLayout>(8 * Config::uiScale);
        previewRow->preferredSize = Vec2(0, 80 * Config::uiScale);
        previewRow->createChild<Label>("Preview:")->preferredSize = Vec2(50 * Config::uiScale, 24 * Config::uiScale);
        previewWidget = previewRow->createChild<BrushTipPreviewWidget>();
        previewWidget->preferredSize = Vec2(80 * Config::uiScale, 80 * Config::uiScale);

        layout->createChild<Spacer>();

        // Buttons
        auto btnRow = layout->createChild<HBoxLayout>(8 * Config::uiScale);
        btnRow->preferredSize = Vec2(0, 32 * Config::uiScale);

        btnRow->createChild<Spacer>();

        auto cancelBtn = btnRow->createChild<Button>("Cancel");
        cancelBtn->preferredSize = Vec2(80 * Config::uiScale, 28 * Config::uiScale);
        cancelBtn->onClick = [this]() { hide(); };

        auto createBtn = btnRow->createChild<Button>("Create");
        createBtn->preferredSize = Vec2(80 * Config::uiScale, 28 * Config::uiScale);
        createBtn->onClick = [this]() { createBrush(); };
    }

    void show() override {
        // Update UI based on mode
        if (fromCurrentCanvas) {
            headerLabel->text = "Brush from Current";
            fileRow->visible = false;
        } else {
            headerLabel->text = "New Brush";
            fileRow->visible = true;
            // Reset state for new brush
            pathField->text = "";
            nameField->text = "Custom Brush";
            loadedImage.reset();
            previewWidget->alphaMask = nullptr;
        }
        layout();
        Dialog::show();
    }

    void browseForFile() {
        // Use deferred dialog to avoid X11 mouse grab issues
        getAppState().requestOpenFileDialog("Select Brush Image", "*.png *.jpg *.bmp",
            [this](const std::string& path) {
                if (!path.empty()) {
                    pathField->text = path;
                    loadImageFromPath(path);
                }
            });
    }

    void loadImageFromPath(const std::string& path) {
        loadedImage = std::make_unique<TiledCanvas>(1, 1);
        if (ImageIO::loadImage(path, *loadedImage)) {
            // Extract just the filename for the brush name
            size_t lastSlash = path.find_last_of("/\\");
            std::string filename = (lastSlash != std::string::npos) ? path.substr(lastSlash + 1) : path;
            size_t lastDot = filename.find_last_of('.');
            if (lastDot != std::string::npos) {
                filename = filename.substr(0, lastDot);
            }
            nameField->text = filename;
            updatePreview();
        } else {
            loadedImage.reset();
        }
        getAppState().needsRedraw = true;
    }

    void loadFromCanvas(TiledCanvas* canvas, u32 width, u32 height) {
        nameField->text = "Canvas Brush";

        // Copy the canvas
        loadedImage = std::make_unique<TiledCanvas>(width, height);
        for (u32 y = 0; y < height; ++y) {
            for (u32 x = 0; x < width; ++x) {
                loadedImage->setPixel(x, y, canvas->getPixel(x, y));
            }
        }
        updatePreview();
    }

    void selectChannel(i32 index) {
        if (index == selectedChannel) return;

        // Uncheck previous
        if (selectedChannel >= 0 && selectedChannel < 4) {
            channelChecks[selectedChannel]->checked = false;
        }

        // Check new
        selectedChannel = index;
        if (selectedChannel >= 0 && selectedChannel < 4) {
            channelChecks[selectedChannel]->checked = true;
        }

        updatePreview();
        getAppState().needsRedraw = true;
    }

    void updatePreview() {
        if (!loadedImage) return;

        BrushChannel channel = static_cast<BrushChannel>(selectedChannel);

        // Build preview alpha mask
        previewMask.clear();
        previewMask.resize(loadedImage->width * loadedImage->height);

        for (u32 y = 0; y < loadedImage->height; ++y) {
            for (u32 x = 0; x < loadedImage->width; ++x) {
                u32 pixel = loadedImage->getPixel(x, y);
                f32 alpha = extractBrushAlpha(pixel, channel);
                previewMask[y * loadedImage->width + x] = alpha;
            }
        }

        previewWidget->alphaMask = &previewMask;
        previewWidget->maskWidth = loadedImage->width;
        previewWidget->maskHeight = loadedImage->height;
        getAppState().needsRedraw = true;
    }

    void createBrush() {
        if (!loadedImage || loadedImage->width == 0) {
            hide();
            return;
        }

        BrushChannel channel = static_cast<BrushChannel>(selectedChannel);

        auto tip = std::make_unique<CustomBrushTip>();
        tip->name = nameField->text.empty() ? "Custom Brush" : nameField->text;
        tip->width = loadedImage->width;
        tip->height = loadedImage->height;
        tip->alphaMask.resize(tip->width * tip->height);

        for (u32 y = 0; y < tip->height; ++y) {
            for (u32 x = 0; x < tip->width; ++x) {
                u32 pixel = loadedImage->getPixel(x, y);
                f32 alpha = extractBrushAlpha(pixel, channel);
                tip->alphaMask[y * tip->width + x] = alpha;
            }
        }

        if (onBrushCreated) {
            onBrushCreated(std::move(tip));
        }

        hide();
    }

    void hide() override {
        loadedImage.reset();
        previewMask.clear();
        previewWidget->alphaMask = nullptr;
        Dialog::hide();
    }

private:
    std::vector<f32> previewMask;
};

// Popup for managing brush library
class ManageBrushesPopup : public Dialog {
public:
    VBoxLayout* brushList = nullptr;
    i32 editingIndex = -1;  // Index of brush being edited, -1 if none
    std::string editingName;  // Name being edited

    // Callbacks for opening other dialogs
    std::function<void()> onNewFromFile;
    std::function<void()> onNewFromCanvas;
    std::function<void()> onBrushDeleted;  // Called when a brush is deleted (for UI update)

    ManageBrushesPopup() : Dialog("Manage Brushes") {
        preferredSize = Vec2(400 * Config::uiScale, 400 * Config::uiScale);
        modal = false;  // Non-modal popup
        bgColor = Config::COLOR_PANEL;

        auto layout = createChild<VBoxLayout>(8 * Config::uiScale);

        // Header with background
        auto headerPanel = layout->createChild<Panel>();
        headerPanel->bgColor = Config::COLOR_PANEL_HEADER;
        headerPanel->preferredSize = Vec2(0, 28 * Config::uiScale);
        headerPanel->setPadding(4 * Config::uiScale);
        headerPanel->createChild<Label>("Manage Brushes");

        layout->createChild<Separator>();

        // Brush list (scrollable area would be nice, but simple list for now)
        brushList = layout->createChild<VBoxLayout>(0);  // No spacing - rows handle their own padding
        brushList->verticalPolicy = SizePolicy::Expanding;

        layout->createChild<Separator>();

        // Footer row with new buttons on left, close on right
        auto btnRow = layout->createChild<HBoxLayout>(8 * Config::uiScale);
        btnRow->preferredSize = Vec2(0, 32 * Config::uiScale);

        auto newFileBtn = btnRow->createChild<Button>("New from File");
        newFileBtn->preferredSize = Vec2(110 * Config::uiScale, 28 * Config::uiScale);
        newFileBtn->onClick = [this]() {
            hide();
            if (onNewFromFile) onNewFromFile();
        };

        auto newCanvasBtn = btnRow->createChild<Button>("New from Canvas");
        newCanvasBtn->preferredSize = Vec2(130 * Config::uiScale, 28 * Config::uiScale);
        newCanvasBtn->onClick = [this]() {
            hide();
            if (onNewFromCanvas) onNewFromCanvas();
        };

        btnRow->createChild<Spacer>();

        auto closeBtn = btnRow->createChild<Button>("Close");
        closeBtn->preferredSize = Vec2(70 * Config::uiScale, 28 * Config::uiScale);
        closeBtn->onClick = [this]() { hide(); };
    }

    void cancelEdit() {
        editingIndex = -1;
        editingName.clear();
    }

    void confirmEdit() {
        if (editingIndex >= 0 && !editingName.empty()) {
            AppState& state = getAppState();
            state.brushLibrary.renameTip(editingIndex, editingName);
        }
        cancelEdit();
    }

    void startEdit(i32 index) {
        // Cancel any existing edit first
        cancelEdit();

        AppState& state = getAppState();
        const CustomBrushTip* tip = state.brushLibrary.getTip(index);
        if (tip) {
            editingIndex = index;
            editingName = tip->name;
            rebuild();
        }
    }

    void deleteBrush(i32 index) {
        // Cancel any existing edit first
        cancelEdit();

        AppState& state = getAppState();
        bool wasActive = (state.currentBrushTipIndex == index);
        state.brushLibrary.removeTip(index);

        // Reset current brush tip if we deleted the active one
        if (wasActive) {
            state.currentBrushTipIndex = -1;
            if (onBrushDeleted) onBrushDeleted();  // Notify to update UI
        } else if (state.currentBrushTipIndex > index) {
            state.currentBrushTipIndex--;
        }

        rebuild();
        state.needsRedraw = true;
    }

    void rebuild() {
        // Clear existing items
        brushList->clearChildren();

        // Alternating row colors - odd is 50% between panel and background
        u32 rowColorEven = Config::COLOR_PANEL;
        // Blend panel and background colors 50%
        u8 panelR = (Config::COLOR_PANEL >> 24) & 0xFF;
        u8 panelG = (Config::COLOR_PANEL >> 16) & 0xFF;
        u8 panelB = (Config::COLOR_PANEL >> 8) & 0xFF;
        u8 bgR = (Config::COLOR_BACKGROUND >> 24) & 0xFF;
        u8 bgG = (Config::COLOR_BACKGROUND >> 16) & 0xFF;
        u8 bgB = (Config::COLOR_BACKGROUND >> 8) & 0xFF;
        u32 rowColorOdd = ((panelR + bgR) / 2) << 24 |
                          ((panelG + bgG) / 2) << 16 |
                          ((panelB + bgB) / 2) << 8 |
                          0xFF;

        AppState& state = getAppState();
        for (size_t i = 0; i < state.brushLibrary.count(); ++i) {
            const CustomBrushTip* tip = state.brushLibrary.getTip(i);
            if (!tip) continue;

            // Use Panel for alternating background color
            auto rowPanel = brushList->createChild<Panel>();
            rowPanel->bgColor = (i % 2 == 0) ? rowColorEven : rowColorOdd;
            rowPanel->preferredSize = Vec2(0, 28 * Config::uiScale);
            rowPanel->setPadding(2 * Config::uiScale);

            auto row = rowPanel->createChild<HBoxLayout>(4 * Config::uiScale);

            i32 index = static_cast<i32>(i);
            bool isEditing = (index == editingIndex);

            if (isEditing) {
                // Text field for editing name
                auto field = row->createChild<TextField>();
                field->text = editingName;
                field->horizontalPolicy = SizePolicy::Expanding;
                field->onChanged = [this](const std::string& newText) {
                    editingName = newText;
                };

                // Confirm button (Y)
                auto confirmBtn = row->createChild<Button>("Y");
                confirmBtn->preferredSize = Vec2(28 * Config::uiScale, 24 * Config::uiScale);
                confirmBtn->onClick = [this]() {
                    confirmEdit();
                    rebuild();
                    getAppState().needsRedraw = true;
                };

                // Cancel button (C)
                auto cancelBtn = row->createChild<Button>("C");
                cancelBtn->preferredSize = Vec2(28 * Config::uiScale, 24 * Config::uiScale);
                cancelBtn->onClick = [this]() {
                    cancelEdit();
                    rebuild();
                    getAppState().needsRedraw = true;
                };
            } else {
                // Label showing name
                auto label = row->createChild<Label>(tip->name);
                label->horizontalPolicy = SizePolicy::Expanding;

                // Edit button (E)
                auto editBtn = row->createChild<Button>("E");
                editBtn->preferredSize = Vec2(28 * Config::uiScale, 24 * Config::uiScale);
                editBtn->onClick = [this, index]() {
                    startEdit(index);
                    getAppState().needsRedraw = true;
                };

                // Delete button (X)
                auto deleteBtn = row->createChild<Button>("X");
                deleteBtn->preferredSize = Vec2(28 * Config::uiScale, 24 * Config::uiScale);
                deleteBtn->onClick = [this, index]() {
                    deleteBrush(index);
                };
            }
        }

        if (state.brushLibrary.count() == 0) {
            auto label = brushList->createChild<Label>("No custom brushes");
            label->preferredSize = Vec2(0, 24 * Config::uiScale);
        }

        // Re-layout after rebuilding
        layout();
        getAppState().needsRedraw = true;
    }

    void hide() override {
        // Discard any pending edit when closing
        cancelEdit();
        Dialog::hide();
    }

    void show() override {
        cancelEdit();
        rebuild();
        Dialog::show();
    }

    void show(f32 x, f32 y) {
        setBounds(x, y, preferredSize.x, preferredSize.y);
        show();
        layout();
    }

    void renderSelf(Framebuffer& fb) override {
        // Draw solid background for popup
        Rect global = globalBounds();
        fb.fillRect(Recti(global), bgColor);
        fb.drawRect(Recti(global), Config::COLOR_BORDER, 1);
    }
};

// Popup for selecting brush tip and adjusting dynamics
class BrushTipSelectorPopup : public Dialog {
public:
    VBoxLayout* tipGrid = nullptr;
    Slider* angleSlider = nullptr;
    Checkbox* showBoundingBoxCheck = nullptr;
    Checkbox* dynamicsEnabledCheck = nullptr;
    Slider* sizeJitterSlider = nullptr;
    Slider* sizeJitterMinSlider = nullptr;
    Slider* angleJitterSlider = nullptr;
    Slider* scatterSlider = nullptr;
    Checkbox* scatterBothAxesCheck = nullptr;

    // Callback when tip selection changes (for updating hardness visibility)
    std::function<void()> onTipChanged;

    BrushTipSelectorPopup() : Dialog("Brush Tip") {
        preferredSize = Vec2(280 * Config::uiScale, 450 * Config::uiScale);
        modal = false;  // Non-modal popup
        bgColor = Config::COLOR_PANEL;  // Ensure solid background

        auto mainLayout = createChild<VBoxLayout>(8 * Config::uiScale);

        auto header = mainLayout->createChild<Label>("Brush Tip");
        header->preferredSize = Vec2(0, 20 * Config::uiScale);

        mainLayout->createChild<Separator>();

        // Tip selection area - will be dynamically sized
        tipGrid = mainLayout->createChild<VBoxLayout>(4 * Config::uiScale);
        tipGrid->preferredSize = Vec2(0, 40 * Config::uiScale);  // Minimum height
        tipGrid->verticalPolicy = SizePolicy::Fixed;

        mainLayout->createChild<Separator>();

        // Angle slider
        auto angleRow = mainLayout->createChild<HBoxLayout>(4 * Config::uiScale);
        angleRow->preferredSize = Vec2(0, 24 * Config::uiScale);
        angleRow->createChild<Label>("Angle")->preferredSize = Vec2(70 * Config::uiScale, 20 * Config::uiScale);
        angleSlider = angleRow->createChild<Slider>(0.0f, 360.0f, 0.0f);
        angleSlider->horizontalPolicy = SizePolicy::Expanding;
        angleSlider->onChanged = [](f32 val) {
            getAppState().brushAngle = val;
            getAppState().needsRedraw = true;
        };

        // Show bounding box checkbox (for custom tips)
        auto boundingBoxRow = mainLayout->createChild<HBoxLayout>(4 * Config::uiScale);
        boundingBoxRow->preferredSize = Vec2(0, 24 * Config::uiScale);
        boundingBoxRow->createChild<Label>("")->preferredSize = Vec2(70 * Config::uiScale, 20 * Config::uiScale);
        showBoundingBoxCheck = boundingBoxRow->createChild<Checkbox>("Show Bounding Box");
        showBoundingBoxCheck->onChanged = [](bool val) {
            getAppState().brushShowBoundingBox = val;
            getAppState().needsRedraw = true;
        };

        mainLayout->createChild<Separator>();

        // Dynamics header with enable checkbox
        auto dynamicsRow = mainLayout->createChild<HBoxLayout>(4 * Config::uiScale);
        dynamicsRow->preferredSize = Vec2(0, 24 * Config::uiScale);
        dynamicsEnabledCheck = dynamicsRow->createChild<Checkbox>("Dynamics");
        dynamicsEnabledCheck->checked = true;
        dynamicsEnabledCheck->onChanged = [](bool val) {
            getAppState().brushDynamics.enabled = val;
            getAppState().needsRedraw = true;
        };
        dynamicsRow->createChild<Spacer>();

        // Size jitter
        auto sizeJitterRow = mainLayout->createChild<HBoxLayout>(4 * Config::uiScale);
        sizeJitterRow->preferredSize = Vec2(0, 24 * Config::uiScale);
        sizeJitterRow->createChild<Label>("Size Jitter")->preferredSize = Vec2(70 * Config::uiScale, 20 * Config::uiScale);
        sizeJitterSlider = sizeJitterRow->createChild<Slider>(0.0f, 1.0f, 0.0f);
        sizeJitterSlider->horizontalPolicy = SizePolicy::Expanding;
        sizeJitterSlider->onChanged = [](f32 val) {
            getAppState().brushDynamics.sizeJitter = val;
            getAppState().needsRedraw = true;
        };

        // Size jitter min
        auto sizeMinRow = mainLayout->createChild<HBoxLayout>(4 * Config::uiScale);
        sizeMinRow->preferredSize = Vec2(0, 24 * Config::uiScale);
        sizeMinRow->createChild<Label>("Min Size")->preferredSize = Vec2(70 * Config::uiScale, 20 * Config::uiScale);
        sizeJitterMinSlider = sizeMinRow->createChild<Slider>(0.0f, 1.0f, 0.0f);
        sizeJitterMinSlider->horizontalPolicy = SizePolicy::Expanding;
        sizeJitterMinSlider->onChanged = [](f32 val) {
            getAppState().brushDynamics.sizeJitterMin = val;
            getAppState().needsRedraw = true;
        };

        // Angle jitter
        auto angleJitterRow = mainLayout->createChild<HBoxLayout>(4 * Config::uiScale);
        angleJitterRow->preferredSize = Vec2(0, 24 * Config::uiScale);
        angleJitterRow->createChild<Label>("Angle Jitter")->preferredSize = Vec2(70 * Config::uiScale, 20 * Config::uiScale);
        angleJitterSlider = angleJitterRow->createChild<Slider>(0.0f, 180.0f, 0.0f);
        angleJitterSlider->horizontalPolicy = SizePolicy::Expanding;
        angleJitterSlider->onChanged = [](f32 val) {
            getAppState().brushDynamics.angleJitter = val;
            getAppState().needsRedraw = true;
        };

        // Scatter
        auto scatterRow = mainLayout->createChild<HBoxLayout>(4 * Config::uiScale);
        scatterRow->preferredSize = Vec2(0, 24 * Config::uiScale);
        scatterRow->createChild<Label>("Scatter")->preferredSize = Vec2(70 * Config::uiScale, 20 * Config::uiScale);
        scatterSlider = scatterRow->createChild<Slider>(0.0f, 1.0f, 0.0f);
        scatterSlider->horizontalPolicy = SizePolicy::Expanding;
        scatterSlider->onChanged = [](f32 val) {
            getAppState().brushDynamics.scatterAmount = val;
            getAppState().needsRedraw = true;
        };

        // Scatter both axes checkbox
        auto scatterBothRow = mainLayout->createChild<HBoxLayout>(4 * Config::uiScale);
        scatterBothRow->preferredSize = Vec2(0, 24 * Config::uiScale);
        scatterBothRow->createChild<Label>("")->preferredSize = Vec2(70 * Config::uiScale, 20 * Config::uiScale);
        scatterBothAxesCheck = scatterBothRow->createChild<Checkbox>("Both Axes");
        scatterBothAxesCheck->onChanged = [](bool val) {
            getAppState().brushDynamics.scatterBothAxes = val;
            getAppState().needsRedraw = true;
        };

        mainLayout->createChild<Spacer>();

        // Close button
        auto btnRow = mainLayout->createChild<HBoxLayout>(8 * Config::uiScale);
        btnRow->preferredSize = Vec2(0, 28 * Config::uiScale);
        btnRow->createChild<Spacer>();
        auto closeBtn = btnRow->createChild<Button>("Close");
        closeBtn->preferredSize = Vec2(70 * Config::uiScale, 24 * Config::uiScale);
        closeBtn->onClick = [this]() { hide(); };
    }

    void rebuild() {
        tipGrid->children.clear();

        AppState& state = getAppState();

        // Calculate how many rows we need
        size_t rowCount = 1 + state.brushLibrary.count();  // Round brush + custom tips
        f32 rowHeight = 28 * Config::uiScale;
        tipGrid->preferredSize = Vec2(0, rowCount * rowHeight + (rowCount - 1) * 4 * Config::uiScale);

        // Round brush option (always first)
        auto roundBtn = tipGrid->createChild<Button>("Round Brush");
        roundBtn->preferredSize = Vec2(0, 24 * Config::uiScale);
        roundBtn->horizontalPolicy = SizePolicy::Expanding;
        roundBtn->onClick = [this]() {
            getAppState().currentBrushTipIndex = -1;
            getAppState().needsRedraw = true;
            if (onTipChanged) onTipChanged();
            rebuild();
            layout();
        };

        // Highlight selected button
        if (state.currentBrushTipIndex == -1) {
            roundBtn->normalColor = Config::COLOR_ACCENT;
            roundBtn->textColor = 0xFFFFFFFF;
        }

        // Custom brush tips
        for (size_t i = 0; i < state.brushLibrary.count(); ++i) {
            const CustomBrushTip* tip = state.brushLibrary.getTip(i);
            if (!tip) continue;

            auto btn = tipGrid->createChild<Button>(tip->name);
            btn->preferredSize = Vec2(0, 24 * Config::uiScale);
            btn->horizontalPolicy = SizePolicy::Expanding;
            size_t index = i;
            btn->onClick = [this, index]() {
                getAppState().currentBrushTipIndex = static_cast<i32>(index);
                getAppState().needsRedraw = true;
                if (onTipChanged) onTipChanged();
                rebuild();
                layout();
            };

            // Highlight selected button
            if (state.currentBrushTipIndex == static_cast<i32>(i)) {
                btn->normalColor = Config::COLOR_ACCENT;
                btn->textColor = 0xFFFFFFFF;
            }
        }

        // Force layout recalculation
        layout();
        getAppState().needsRedraw = true;
    }

    void renderSelf(Framebuffer& fb) override {
        // Draw solid background for popup (important for non-modal)
        Rect global = globalBounds();
        fb.fillRect(Recti(global), bgColor);

        // Draw a border for visibility
        fb.drawRect(Recti(global), Config::COLOR_BORDER, 1);
    }

    void updateFromState() {
        AppState& state = getAppState();
        angleSlider->value = state.brushAngle;
        showBoundingBoxCheck->checked = state.brushShowBoundingBox;
        dynamicsEnabledCheck->checked = state.brushDynamics.enabled;
        sizeJitterSlider->value = state.brushDynamics.sizeJitter;
        sizeJitterMinSlider->value = state.brushDynamics.sizeJitterMin;
        angleJitterSlider->value = state.brushDynamics.angleJitter;
        scatterSlider->value = state.brushDynamics.scatterAmount;
        scatterBothAxesCheck->checked = state.brushDynamics.scatterBothAxes;
    }

    void show() override {
        rebuild();
        updateFromState();
        Dialog::show();
        layout();  // Ensure children are properly laid out
    }

    void show(f32 x, f32 y) {
        setBounds(x, y, preferredSize.x, preferredSize.y);
        rebuild();
        updateFromState();
        Dialog::show();
        layout();  // Ensure children are properly laid out
    }
};

#endif
