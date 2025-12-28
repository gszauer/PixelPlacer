#include "brush_dialogs.h"

// ============================================================================
// PressureCurveWidget implementation
// ============================================================================

PressureCurveWidget::PressureCurveWidget() {
}

void PressureCurveWidget::getGraphBounds(Rect& global, f32& leftMargin, f32& topMargin, f32& graphW, f32& graphH) const {
    global = globalBounds();
    f32 margin = 8 * Config::uiScale;
    leftMargin = showAxisLabels ? (20 * Config::uiScale) : margin;
    f32 bottomMargin = showAxisLabels ? (18 * Config::uiScale) : margin;
    topMargin = margin;
    f32 rightMargin = margin;
    graphW = global.w - leftMargin - rightMargin;
    graphH = global.h - topMargin - bottomMargin;
}

Vec2 PressureCurveWidget::toPixel(const Vec2& normalized) const {
    Rect global;
    f32 leftMargin, topMargin, graphW, graphH;
    getGraphBounds(global, leftMargin, topMargin, graphW, graphH);
    return Vec2(
        global.x + leftMargin + normalized.x * graphW,
        global.y + topMargin + (1.0f - normalized.y) * graphH
    );
}

Vec2 PressureCurveWidget::toNormalized(const Vec2& pixel) const {
    Rect global;
    f32 leftMargin, topMargin, graphW, graphH;
    getGraphBounds(global, leftMargin, topMargin, graphW, graphH);
    f32 nx = (pixel.x - global.x - leftMargin) / graphW;
    f32 ny = 1.0f - (pixel.y - global.y - topMargin) / graphH;
    return Vec2(
        std::max(0.0f, std::min(1.0f, nx)),
        std::max(0.0f, std::min(1.0f, ny))
    );
}

void PressureCurveWidget::renderSelf(Framebuffer& fb) {
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
    for (i32 i = 0; i < graphW; i += 2) {
        i32 px = graphX + i;
        i32 py = graphY + graphH - i * graphH / graphW;
        if (py >= graphY && py < graphY + graphH) {
            fb.setPixel(px, py, refColor);
        }
    }

    // Draw bezier curve (thicker - draw multiple times offset)
    u32 curveColor = 0x4A90D9FF;
    for (int offset = -1; offset <= 1; offset++) {
        Vec2 prev = toPixel(Vec2(0, 0));
        for (int i = 1; i <= 64; i++) {
            f32 t = i / 64.0f;

            f32 mt = 1.0f - t;
            f32 mt2 = mt * mt;
            f32 t2 = t * t;
            f32 t3 = t2 * t;

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

    // Draw control point handles
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
    u32 cp1Color = (draggingPoint == 0) ? 0xFFAA44FF : 0xFF6600FF;
    u32 cp2Color = (draggingPoint == 1) ? 0xAA66FFFF : 0x8844CCFF;

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

        Vec2 outSize = FontRenderer::instance().measureText("Out", Config::defaultFontSize());
        i32 outX = static_cast<i32>(global.x + 2);
        i32 outY = graphY + (graphH + static_cast<i32>(outSize.x)) / 2;
        FontRenderer::instance().renderTextRotated90(fb, "Out", outX, outY, axisLabelColor);

        Vec2 inputSize = FontRenderer::instance().measureText("Input", Config::defaultFontSize());
        i32 inputX = graphX + (graphW - static_cast<i32>(inputSize.x)) / 2;
        i32 inputY = graphY + graphH + 3;
        FontRenderer::instance().renderText(fb, "Input", inputX, inputY, axisLabelColor);
    }
}

bool PressureCurveWidget::onMouseDown(const MouseEvent& e) {
    if (e.button != MouseButton::Left) return false;

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

bool PressureCurveWidget::onMouseDrag(const MouseEvent& e) {
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

bool PressureCurveWidget::onMouseUp(const MouseEvent& e) {
    if (draggingPoint >= 0) {
        draggingPoint = -1;
        getAppState().capturedWidget = nullptr;
        if (onChanged) onChanged();
        return true;
    }
    return false;
}

void PressureCurveWidget::reset() {
    cp1 = Vec2(0.33f, 0.33f);
    cp2 = Vec2(0.66f, 0.66f);
    getAppState().needsRedraw = true;
}

// ============================================================================
// PressureCurvePopup implementation
// ============================================================================

PressureCurvePopup::PressureCurvePopup() : Dialog("Pressure Curve") {
    preferredSize = Vec2(280 * Config::uiScale, 320 * Config::uiScale);
    modal = false;
    bgColor = Config::COLOR_PANEL;

    auto layout = createChild<VBoxLayout>(8 * Config::uiScale);

    auto header = layout->createChild<Label>("Pressure Curve");
    header->preferredSize = Vec2(0, 20 * Config::uiScale);

    layout->createChild<Separator>();

    curveWidget = layout->createChild<PressureCurveWidget>();
    curveWidget->preferredSize = Vec2(0, 180 * Config::uiScale);
    curveWidget->horizontalPolicy = SizePolicy::Expanding;
    curveWidget->showAxisLabels = true;

    curveWidget->onChanged = []() {};

    layout->createChild<Separator>();

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

void PressureCurvePopup::applyCurve() {
    AppState& state = getAppState();
    state.pressureCurveCP1 = curveWidget->cp1;
    state.pressureCurveCP2 = curveWidget->cp2;
    state.needsRedraw = true;
}

void PressureCurvePopup::renderSelf(Framebuffer& fb) {
    Rect global = globalBounds();
    fb.fillRect(Recti(global), bgColor);
    fb.drawRect(Recti(global), Config::COLOR_BORDER, 1);
}

void PressureCurvePopup::show() {
    AppState& state = getAppState();
    curveWidget->cp1 = state.pressureCurveCP1;
    curveWidget->cp2 = state.pressureCurveCP2;
    Dialog::show();
}

void PressureCurvePopup::show(f32 x, f32 y) {
    // Align right edge of popup with x (popup opens to the left)
    f32 popupX = x - preferredSize.x;
    setBounds(popupX, y, preferredSize.x, preferredSize.y);
    show();
    layout();
}

void PressureCurvePopup::hide() {
    applyCurve();
    Dialog::hide();
}

// ============================================================================
// BrushTipPreviewWidget implementation
// ============================================================================

BrushTipPreviewWidget::BrushTipPreviewWidget() {
    preferredSize = Vec2(64 * Config::uiScale, 64 * Config::uiScale);
}

void BrushTipPreviewWidget::render(Framebuffer& fb) {
    if (!visible) return;
    Rect global = globalBounds();

    fb.drawCheckerboard(Recti(global));

    if (!alphaMask || maskWidth == 0 || maskHeight == 0) return;

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

// ============================================================================
// NewBrushDialog implementation
// ============================================================================

NewBrushDialog::NewBrushDialog() : Dialog("New Brush") {
    preferredSize = Vec2(300 * Config::uiScale, 300 * Config::uiScale);

    auto layout = createChild<VBoxLayout>(8 * Config::uiScale);

    headerPanel = layout->createChild<Panel>();
    headerPanel->bgColor = Config::COLOR_PANEL_HEADER;
    headerPanel->preferredSize = Vec2(0, 28 * Config::uiScale);
    headerPanel->setPadding(4 * Config::uiScale);
    headerLabel = headerPanel->createChild<Label>("New Brush");

    layout->createChild<Separator>();

    fileRow = layout->createChild<HBoxLayout>(8 * Config::uiScale);
    fileRow->preferredSize = Vec2(0, 28 * Config::uiScale);
    static_cast<HBoxLayout*>(fileRow)->createChild<Label>("File:")->preferredSize = Vec2(50 * Config::uiScale, 24 * Config::uiScale);
    pathField = static_cast<HBoxLayout*>(fileRow)->createChild<TextField>();
    pathField->horizontalPolicy = SizePolicy::Expanding;
    pathField->readOnly = true;

    auto browseBtn = static_cast<HBoxLayout*>(fileRow)->createChild<Button>("...");
    browseBtn->preferredSize = Vec2(30 * Config::uiScale, 24 * Config::uiScale);
    browseBtn->onClick = [this]() { browseForFile(); };

    auto nameRow = layout->createChild<HBoxLayout>(8 * Config::uiScale);
    nameRow->preferredSize = Vec2(0, 28 * Config::uiScale);
    nameRow->createChild<Label>("Name:")->preferredSize = Vec2(50 * Config::uiScale, 24 * Config::uiScale);
    nameField = nameRow->createChild<TextField>();
    nameField->text = "Custom Brush";
    nameField->horizontalPolicy = SizePolicy::Expanding;

    auto channelRow = layout->createChild<HBoxLayout>(4 * Config::uiScale);
    channelRow->preferredSize = Vec2(0, 24 * Config::uiScale);
    channelRow->createChild<Label>("Channel:")->preferredSize = Vec2(55 * Config::uiScale, 20 * Config::uiScale);

    const char* channelNames[4] = {"R", "G", "B", "A"};
    for (i32 i = 0; i < 4; ++i) {
        channelChecks[i] = channelRow->createChild<Checkbox>(channelNames[i], i == 3);
        channelChecks[i]->preferredSize = Vec2(40 * Config::uiScale, 20 * Config::uiScale);
        i32 channelIndex = i;
        channelChecks[i]->onChanged = [this, channelIndex](bool checked) {
            if (checked) {
                selectChannel(channelIndex);
            } else if (selectedChannel == channelIndex) {
                channelChecks[channelIndex]->checked = true;
            }
        };
    }

    auto previewRow = layout->createChild<HBoxLayout>(8 * Config::uiScale);
    previewRow->preferredSize = Vec2(0, 80 * Config::uiScale);
    previewRow->createChild<Label>("Preview:")->preferredSize = Vec2(50 * Config::uiScale, 24 * Config::uiScale);
    previewWidget = previewRow->createChild<BrushTipPreviewWidget>();
    previewWidget->preferredSize = Vec2(80 * Config::uiScale, 80 * Config::uiScale);

    layout->createChild<Spacer>();

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

void NewBrushDialog::show() {
    if (fromCurrentCanvas) {
        headerLabel->text = "Brush from Current";
        fileRow->visible = false;
    } else {
        headerLabel->text = "New Brush";
        fileRow->visible = true;
        pathField->text = "";
        nameField->text = "Custom Brush";
        loadedImage.reset();
        previewWidget->alphaMask = nullptr;
    }
    layout();
    Dialog::show();
}

void NewBrushDialog::browseForFile() {
    getAppState().requestOpenFileDialog("Select Brush Image", "*.png *.jpg *.bmp",
        [this](const std::string& path) {
            if (!path.empty()) {
                pathField->text = path;
                loadImageFromPath(path);
            }
        });
}

void NewBrushDialog::loadImageFromPath(const std::string& path) {
    loadedImage = std::make_unique<TiledCanvas>(1, 1);
    if (ImageIO::loadImage(path, *loadedImage)) {
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

void NewBrushDialog::loadFromCanvas(TiledCanvas* canvas, u32 width, u32 height) {
    nameField->text = "Canvas Brush";

    loadedImage = std::make_unique<TiledCanvas>(width, height);
    for (u32 y = 0; y < height; ++y) {
        for (u32 x = 0; x < width; ++x) {
            loadedImage->setPixel(x, y, canvas->getPixel(x, y));
        }
    }
    updatePreview();
}

void NewBrushDialog::selectChannel(i32 index) {
    if (index == selectedChannel) return;

    if (selectedChannel >= 0 && selectedChannel < 4) {
        channelChecks[selectedChannel]->checked = false;
    }

    selectedChannel = index;
    if (selectedChannel >= 0 && selectedChannel < 4) {
        channelChecks[selectedChannel]->checked = true;
    }

    updatePreview();
    getAppState().needsRedraw = true;
}

void NewBrushDialog::updatePreview() {
    if (!loadedImage) return;

    BrushChannel channel = static_cast<BrushChannel>(selectedChannel);

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

void NewBrushDialog::createBrush() {
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

void NewBrushDialog::hide() {
    loadedImage.reset();
    previewMask.clear();
    previewWidget->alphaMask = nullptr;
    Dialog::hide();
}

// ============================================================================
// ManageBrushesPopup implementation
// ============================================================================

ManageBrushesPopup::ManageBrushesPopup() : Dialog("Manage Brushes") {
    preferredSize = Vec2(280 * Config::uiScale, 230 * Config::uiScale);
    modal = false;
    bgColor = Config::COLOR_PANEL;

    auto layout = createChild<VBoxLayout>(8 * Config::uiScale);

    brushScrollView = layout->createChild<ScrollView>();
    brushScrollView->preferredSize = Vec2(0, 170 * Config::uiScale);
    brushScrollView->verticalPolicy = SizePolicy::Fixed;
    brushList = brushScrollView->createChild<VBoxLayout>(0);

    layout->createChild<Separator>();

    auto btnRow = layout->createChild<HBoxLayout>(8 * Config::uiScale);
    btnRow->preferredSize = Vec2(0, 32 * Config::uiScale);

    auto newFileBtn = btnRow->createChild<Button>("New from File");
    newFileBtn->preferredSize = Vec2(120 * Config::uiScale, 28 * Config::uiScale);
    newFileBtn->onClick = [this]() {
        hide();
        if (onNewFromFile) onNewFromFile();
    };

    auto newCanvasBtn = btnRow->createChild<Button>("New from Canvas");
    newCanvasBtn->preferredSize = Vec2(140 * Config::uiScale, 28 * Config::uiScale);
    newCanvasBtn->onClick = [this]() {
        hide();
        if (onNewFromCanvas) onNewFromCanvas();
    };
}

void ManageBrushesPopup::cancelEdit() {
    editingIndex = -1;
    editingName.clear();
}

void ManageBrushesPopup::confirmEdit() {
    if (editingIndex >= 0 && !editingName.empty()) {
        AppState& state = getAppState();
        state.brushLibrary.renameTip(editingIndex, editingName);
    }
    cancelEdit();
}

void ManageBrushesPopup::startEdit(i32 index) {
    cancelEdit();

    AppState& state = getAppState();
    const CustomBrushTip* tip = state.brushLibrary.getTip(index);
    if (tip) {
        editingIndex = index;
        editingName = tip->name;
        rebuild();
    }
}

void ManageBrushesPopup::deleteBrush(i32 index) {
    cancelEdit();

    AppState& state = getAppState();
    bool wasActive = (state.currentBrushTipIndex == index);
    state.brushLibrary.removeTip(index);

    if (wasActive) {
        state.currentBrushTipIndex = -1;
        if (onBrushDeleted) onBrushDeleted();
    } else if (state.currentBrushTipIndex > index) {
        state.currentBrushTipIndex--;
    }

    rebuild();
    state.needsRedraw = true;
}

void ManageBrushesPopup::rebuild() {
    brushList->clearChildren();

    u32 rowColorEven = Config::COLOR_PANEL;
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

        auto rowPanel = brushList->createChild<Panel>();
        rowPanel->bgColor = (i % 2 == 0) ? rowColorEven : rowColorOdd;
        rowPanel->preferredSize = Vec2(0, 28 * Config::uiScale);
        rowPanel->setPadding(2 * Config::uiScale);

        auto row = rowPanel->createChild<HBoxLayout>(4 * Config::uiScale);

        i32 index = static_cast<i32>(i);
        bool isEditing = (index == editingIndex);

        if (isEditing) {
            auto field = row->createChild<TextField>();
            field->text = editingName;
            field->horizontalPolicy = SizePolicy::Expanding;
            field->onChanged = [this](const std::string& newText) {
                editingName = newText;
            };

            auto confirmBtn = row->createChild<IconButton>();
            confirmBtn->preferredSize = Vec2(28 * Config::uiScale, 24 * Config::uiScale);
            confirmBtn->renderIcon = [](Framebuffer& fb, const Rect& r, u32 color) {
                FontRenderer::instance().renderIconCentered(fb, "\xF3\xB0\x84\xAC", r, color, Config::defaultFontSize(), "Material Icons");
            };
            confirmBtn->onClick = [this]() {
                confirmEdit();
                rebuild();
                getAppState().needsRedraw = true;
            };

            auto cancelBtn = row->createChild<IconButton>();
            cancelBtn->preferredSize = Vec2(28 * Config::uiScale, 24 * Config::uiScale);
            cancelBtn->renderIcon = [](Framebuffer& fb, const Rect& r, u32 color) {
                FontRenderer::instance().renderIconCentered(fb, "\xF3\xB0\x96\xAD", r, color, Config::defaultFontSize(), "Material Icons");
            };
            cancelBtn->onClick = [this]() {
                cancelEdit();
                rebuild();
                getAppState().needsRedraw = true;
            };
        } else {
            auto nameBtn = row->createChild<Button>(tip->name);
            nameBtn->horizontalPolicy = SizePolicy::Expanding;
            nameBtn->normalColor = 0x00000000;
            nameBtn->hoverColor = Config::COLOR_HOVER;
            nameBtn->textAlign = 0;
            nameBtn->onDoubleClick = [this, index]() {
                startEdit(index);
                getAppState().needsRedraw = true;
            };

            auto deleteBtn = row->createChild<IconButton>();
            deleteBtn->preferredSize = Vec2(28 * Config::uiScale, 24 * Config::uiScale);
            deleteBtn->renderIcon = [](Framebuffer& fb, const Rect& r, u32 color) {
                FontRenderer::instance().renderIconCentered(fb, "\xF3\xB0\xA9\xBA", r, color, Config::defaultFontSize(), "Material Icons");
            };
            deleteBtn->onClick = [this, index]() {
                deleteBrush(index);
            };
        }
    }

    if (state.brushLibrary.count() == 0) {
        auto label = brushList->createChild<Label>("No custom brushes");
        label->preferredSize = Vec2(0, 24 * Config::uiScale);
    }

    layout();
    getAppState().needsRedraw = true;
}

void ManageBrushesPopup::hide() {
    cancelEdit();
    Dialog::hide();
}

void ManageBrushesPopup::show() {
    cancelEdit();
    rebuild();
    Dialog::show();
}

void ManageBrushesPopup::show(f32 x, f32 y) {
    setBounds(x, y, preferredSize.x, preferredSize.y);
    show();
    layout();
}

void ManageBrushesPopup::renderSelf(Framebuffer& fb) {
    Rect global = globalBounds();
    fb.fillRect(Recti(global), bgColor);
    fb.drawRect(Recti(global), Config::COLOR_BORDER, 1);
}

// ============================================================================
// BrushTipSelectorPopup implementation
// ============================================================================

BrushTipSelectorPopup::BrushTipSelectorPopup() : Dialog("Brush Tip") {
    preferredSize = Vec2(280 * Config::uiScale, 390 * Config::uiScale);
    modal = false;
    bgColor = Config::COLOR_PANEL;

    auto mainLayout = createChild<VBoxLayout>(8 * Config::uiScale);

    tipScrollView = mainLayout->createChild<ScrollView>();
    tipScrollView->preferredSize = Vec2(0, 100 * Config::uiScale);
    tipScrollView->verticalPolicy = SizePolicy::Fixed;
    tipGrid = tipScrollView->createChild<VBoxLayout>(4 * Config::uiScale);

    mainLayout->createChild<Separator>();

    auto angleRow = mainLayout->createChild<HBoxLayout>(4 * Config::uiScale);
    angleRow->preferredSize = Vec2(0, 24 * Config::uiScale);
    angleRow->createChild<Label>("Angle")->preferredSize = Vec2(70 * Config::uiScale, 20 * Config::uiScale);
    angleSlider = angleRow->createChild<Slider>(0.0f, 360.0f, 0.0f);
    angleSlider->horizontalPolicy = SizePolicy::Expanding;
    angleSlider->onChanged = [](f32 val) {
        getAppState().brushAngle = val;
        getAppState().needsRedraw = true;
    };

    auto boundingBoxRow = mainLayout->createChild<HBoxLayout>(4 * Config::uiScale);
    boundingBoxRow->preferredSize = Vec2(0, 24 * Config::uiScale);
    boundingBoxRow->createChild<Label>("")->preferredSize = Vec2(70 * Config::uiScale, 20 * Config::uiScale);
    showBoundingBoxCheck = boundingBoxRow->createChild<Checkbox>("Show Bounding Box");
    showBoundingBoxCheck->onChanged = [](bool val) {
        getAppState().brushShowBoundingBox = val;
        getAppState().needsRedraw = true;
    };

    mainLayout->createChild<Separator>();

    auto dynamicsRow = mainLayout->createChild<HBoxLayout>(4 * Config::uiScale);
    dynamicsRow->preferredSize = Vec2(0, 24 * Config::uiScale);
    dynamicsEnabledCheck = dynamicsRow->createChild<Checkbox>("Dynamics");
    dynamicsEnabledCheck->checked = true;
    dynamicsEnabledCheck->onChanged = [](bool val) {
        getAppState().brushDynamics.enabled = val;
        getAppState().needsRedraw = true;
    };
    dynamicsRow->createChild<Spacer>();

    auto sizeJitterRow = mainLayout->createChild<HBoxLayout>(4 * Config::uiScale);
    sizeJitterRow->preferredSize = Vec2(0, 24 * Config::uiScale);
    sizeJitterRow->createChild<Label>("Size Jitter")->preferredSize = Vec2(80 * Config::uiScale, 20 * Config::uiScale);
    sizeJitterSlider = sizeJitterRow->createChild<Slider>(0.0f, 1.0f, 0.0f);
    sizeJitterSlider->horizontalPolicy = SizePolicy::Expanding;
    sizeJitterSlider->onChanged = [](f32 val) {
        getAppState().brushDynamics.sizeJitter = val;
        getAppState().needsRedraw = true;
    };

    auto sizeMinRow = mainLayout->createChild<HBoxLayout>(4 * Config::uiScale);
    sizeMinRow->preferredSize = Vec2(0, 24 * Config::uiScale);
    sizeMinRow->createChild<Label>("Min Size")->preferredSize = Vec2(70 * Config::uiScale, 20 * Config::uiScale);
    sizeJitterMinSlider = sizeMinRow->createChild<Slider>(0.0f, 1.0f, 0.0f);
    sizeJitterMinSlider->horizontalPolicy = SizePolicy::Expanding;
    sizeJitterMinSlider->onChanged = [](f32 val) {
        getAppState().brushDynamics.sizeJitterMin = val;
        getAppState().needsRedraw = true;
    };

    auto angleJitterRow = mainLayout->createChild<HBoxLayout>(4 * Config::uiScale);
    angleJitterRow->preferredSize = Vec2(0, 24 * Config::uiScale);
    angleJitterRow->createChild<Label>("Angle Jitter")->preferredSize = Vec2(80 * Config::uiScale, 20 * Config::uiScale);
    angleJitterSlider = angleJitterRow->createChild<Slider>(0.0f, 180.0f, 0.0f);
    angleJitterSlider->horizontalPolicy = SizePolicy::Expanding;
    angleJitterSlider->onChanged = [](f32 val) {
        getAppState().brushDynamics.angleJitter = val;
        getAppState().needsRedraw = true;
    };

    auto scatterRow = mainLayout->createChild<HBoxLayout>(4 * Config::uiScale);
    scatterRow->preferredSize = Vec2(0, 24 * Config::uiScale);
    scatterRow->createChild<Label>("Scatter")->preferredSize = Vec2(70 * Config::uiScale, 20 * Config::uiScale);
    scatterSlider = scatterRow->createChild<Slider>(0.0f, 1.0f, 0.0f);
    scatterSlider->horizontalPolicy = SizePolicy::Expanding;
    scatterSlider->onChanged = [](f32 val) {
        getAppState().brushDynamics.scatterAmount = val;
        getAppState().needsRedraw = true;
    };

    auto scatterBothRow = mainLayout->createChild<HBoxLayout>(4 * Config::uiScale);
    scatterBothRow->preferredSize = Vec2(0, 24 * Config::uiScale);
    scatterBothRow->createChild<Label>("")->preferredSize = Vec2(70 * Config::uiScale, 20 * Config::uiScale);
    scatterBothAxesCheck = scatterBothRow->createChild<Checkbox>("Both Axes");
    scatterBothAxesCheck->onChanged = [](bool val) {
        getAppState().brushDynamics.scatterBothAxes = val;
        getAppState().needsRedraw = true;
    };
}

void BrushTipSelectorPopup::rebuild() {
    tipGrid->children.clear();

    AppState& state = getAppState();

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

    if (state.currentBrushTipIndex == -1) {
        roundBtn->normalColor = Config::GRAY_500;
    }

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

        if (state.currentBrushTipIndex == static_cast<i32>(i)) {
            btn->normalColor = Config::GRAY_500;
        }
    }

    layout();
    getAppState().needsRedraw = true;
}

void BrushTipSelectorPopup::renderSelf(Framebuffer& fb) {
    Rect global = globalBounds();
    fb.fillRect(Recti(global), bgColor);
    fb.drawRect(Recti(global), Config::COLOR_BORDER, 1);
}

void BrushTipSelectorPopup::updateFromState() {
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

void BrushTipSelectorPopup::show() {
    rebuild();
    updateFromState();
    Dialog::show();
    layout();
}

void BrushTipSelectorPopup::show(f32 x, f32 y) {
    setBounds(x, y, preferredSize.x, preferredSize.y);
    rebuild();
    updateFromState();
    Dialog::show();
    layout();
}
