#include "dialogs.h"
#include "platform.h"

// Dialog base class

Dialog::Dialog(const std::string& t) : title(t) {
    visible = false;  // Start hidden (sets Widget::visible)
    bgColor = Config::COLOR_PANEL;
    setPadding(8 * Config::uiScale);
}

void Dialog::show() {
    visible = true;
    getAppState().needsRedraw = true;
}

void Dialog::hide() {
    visible = false;
    OverlayManager::instance().unregisterOverlay(this);
    if (onClose) onClose();
    getAppState().needsRedraw = true;
}

void Dialog::renderSelf(Framebuffer& fb) {
    // Note: Widget::render() already checks visible, so we don't need to here
    // Draw modal background
    if (modal) {
        fb.fillRect(0, 0, fb.width, fb.height, 0x00000080);  // Semi-transparent black
    }

    Panel::renderSelf(fb);
}

bool Dialog::onMouseDown(const MouseEvent& e) {
    // Find focusable widget under click and set focus
    Widget* target = findWidgetAt(e.globalPosition);
    while (target && target != this) {
        if (target->focusable) {
            AppState& state = getAppState();
            if (target != state.focusedWidget) {
                if (state.focusedWidget) {
                    state.focusedWidget->onBlur();
                }
                state.focusedWidget = target;
                target->onFocus();
            }
            break;
        }
        target = target->parent;
    }

    // Then route the event normally
    return Panel::onMouseDown(e);
}

// NewDocumentDialog

NewDocumentDialog::NewDocumentDialog() : Dialog("New Document") {
    preferredSize = Vec2(320 * Config::uiScale, 220 * Config::uiScale);

    auto layout = createChild<VBoxLayout>(8 * Config::uiScale);

    // Header with background
    auto headerPanel = layout->createChild<Panel>();
    headerPanel->bgColor = Config::COLOR_PANEL_HEADER;
    headerPanel->preferredSize = Vec2(0, 24 * Config::uiScale);
    headerPanel->setPadding(4 * Config::uiScale);
    headerPanel->createChild<Label>("New Document");

    layout->createChild<Separator>();

    // Name row
    auto nameRow = layout->createChild<HBoxLayout>(8 * Config::uiScale);
    nameRow->preferredSize = Vec2(0, 28 * Config::uiScale);
    nameRow->createChild<Label>("Name:")->preferredSize = Vec2(60 * Config::uiScale, 24 * Config::uiScale);
    nameField = nameRow->createChild<TextField>();
    nameField->text = "Untitled";
    nameField->horizontalPolicy = SizePolicy::Expanding;

    // Width row
    auto widthRow = layout->createChild<HBoxLayout>(8 * Config::uiScale);
    widthRow->preferredSize = Vec2(0, 28 * Config::uiScale);
    widthRow->createChild<Label>("Width:")->preferredSize = Vec2(60 * Config::uiScale, 24 * Config::uiScale);
    widthField = widthRow->createChild<TextField>();
    widthField->text = std::to_string(Config::DEFAULT_CANVAS_WIDTH);
    widthField->horizontalPolicy = SizePolicy::Expanding;
    widthRow->createChild<Label>("px")->preferredSize = Vec2(24 * Config::uiScale, 24 * Config::uiScale);

    // Height row
    auto heightRow = layout->createChild<HBoxLayout>(8 * Config::uiScale);
    heightRow->preferredSize = Vec2(0, 28 * Config::uiScale);
    heightRow->createChild<Label>("Height:")->preferredSize = Vec2(60 * Config::uiScale, 24 * Config::uiScale);
    heightField = heightRow->createChild<TextField>();
    heightField->text = std::to_string(Config::DEFAULT_CANVAS_HEIGHT);
    heightField->horizontalPolicy = SizePolicy::Expanding;
    heightRow->createChild<Label>("px")->preferredSize = Vec2(24 * Config::uiScale, 24 * Config::uiScale);

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
    createBtn->onClick = [this]() {
        std::string name = nameField->text;
        if (name.empty()) name = "Untitled";
        u32 width = std::atoi(widthField->text.c_str());
        u32 height = std::atoi(heightField->text.c_str());
        if (width == 0) width = Config::DEFAULT_CANVAS_WIDTH;
        if (height == 0) height = Config::DEFAULT_CANVAS_HEIGHT;
        if (width > Config::MAX_CANVAS_SIZE) width = Config::MAX_CANVAS_SIZE;
        if (height > Config::MAX_CANVAS_SIZE) height = Config::MAX_CANVAS_SIZE;
        if (onConfirm) {
            onConfirm(name, width, height);
        }
        hide();
    };
}

void NewDocumentDialog::show() {
    nameField->text = "Untitled";
    widthField->text = std::to_string(Config::DEFAULT_CANVAS_WIDTH);
    heightField->text = std::to_string(Config::DEFAULT_CANVAS_HEIGHT);
    Dialog::show();
}

// AnchorGridWidget

AnchorGridWidget::AnchorGridWidget() {
    preferredSize = Vec2(60 * Config::uiScale, 60 * Config::uiScale);
}

void AnchorGridWidget::render(Framebuffer& fb) {
    if (!visible) return;

    Rect global = globalBounds();
    f32 cellW = global.w / 3.0f;
    f32 cellH = global.h / 3.0f;

    // Draw background
    fb.fillRect(Recti(global), 0x404040FF);

    // Draw grid cells
    for (i32 row = 0; row < 3; ++row) {
        for (i32 col = 0; col < 3; ++col) {
            i32 ax = col - 1;  // -1, 0, 1
            i32 ay = row - 1;  // -1, 0, 1

            f32 x = global.x + col * cellW;
            f32 y = global.y + row * cellH;

            bool selected = (ax == selectedX && ay == selectedY);

            // Cell background
            u32 cellColor = selected ? Config::GRAY_500 : 0x505050FF;
            i32 margin = static_cast<i32>(2 * Config::uiScale);
            fb.fillRect(
                static_cast<i32>(x) + margin,
                static_cast<i32>(y) + margin,
                static_cast<i32>(cellW) - margin * 2,
                static_cast<i32>(cellH) - margin * 2,
                cellColor
            );

            // Draw dot in center of cell
            i32 cx = static_cast<i32>(x + cellW / 2);
            i32 cy = static_cast<i32>(y + cellH / 2);
            i32 dotRadius = static_cast<i32>(3 * Config::uiScale);
            u32 dotColor = selected ? 0xFFFFFFFF : 0x808080FF;
            fb.fillCircle(cx, cy, dotRadius, dotColor);
        }
    }

    // Draw border
    fb.drawRect(Recti(global), Config::COLOR_BORDER, 1);
}

bool AnchorGridWidget::onMouseDown(const MouseEvent& e) {
    if (e.button != MouseButton::Left) return false;

    Rect global = globalBounds();
    f32 cellW = global.w / 3.0f;
    f32 cellH = global.h / 3.0f;

    // Determine which cell was clicked
    i32 col = static_cast<i32>((e.globalPosition.x - global.x) / cellW);
    i32 row = static_cast<i32>((e.globalPosition.y - global.y) / cellH);

    if (col >= 0 && col < 3 && row >= 0 && row < 3) {
        selectedX = col - 1;  // -1, 0, 1
        selectedY = row - 1;  // -1, 0, 1
        if (onChanged) onChanged();
        getAppState().needsRedraw = true;
        return true;
    }

    return false;
}

// CanvasSizeDialog

CanvasSizeDialog::CanvasSizeDialog() : Dialog("Canvas Size") {
    preferredSize = Vec2(220 * Config::uiScale, 260 * Config::uiScale);

    auto layout = createChild<VBoxLayout>(6 * Config::uiScale);

    // Header with background
    auto headerPanel = layout->createChild<Panel>();
    headerPanel->bgColor = Config::COLOR_PANEL_HEADER;
    headerPanel->preferredSize = Vec2(0, 24 * Config::uiScale);
    headerPanel->setPadding(4 * Config::uiScale);
    headerPanel->createChild<Label>("Canvas Size");

    layout->createChild<Separator>();

    // Width row
    auto widthRow = layout->createChild<HBoxLayout>(6 * Config::uiScale);
    widthRow->preferredSize = Vec2(0, 26 * Config::uiScale);
    widthRow->createChild<Label>("Width:")->preferredSize = Vec2(55 * Config::uiScale, 22 * Config::uiScale);
    widthField = widthRow->createChild<TextField>();
    widthField->text = "1920";
    widthField->preferredSize = Vec2(60 * Config::uiScale, 22 * Config::uiScale);
    widthField->horizontalPolicy = SizePolicy::Fixed;
    widthRow->createChild<Label>("px")->preferredSize = Vec2(18 * Config::uiScale, 22 * Config::uiScale);

    // Height row
    auto heightRow = layout->createChild<HBoxLayout>(6 * Config::uiScale);
    heightRow->preferredSize = Vec2(0, 26 * Config::uiScale);
    heightRow->createChild<Label>("Height:")->preferredSize = Vec2(55 * Config::uiScale, 22 * Config::uiScale);
    heightField = heightRow->createChild<TextField>();
    heightField->text = "1080";
    heightField->preferredSize = Vec2(60 * Config::uiScale, 22 * Config::uiScale);
    heightField->horizontalPolicy = SizePolicy::Fixed;
    heightRow->createChild<Label>("px")->preferredSize = Vec2(18 * Config::uiScale, 22 * Config::uiScale);

    // Resize mode row
    auto resizeRow = layout->createChild<HBoxLayout>(6 * Config::uiScale);
    resizeRow->preferredSize = Vec2(0, 26 * Config::uiScale);
    resizeRow->createChild<Label>("Resize:")->preferredSize = Vec2(55 * Config::uiScale, 22 * Config::uiScale);
    resizeModeCombo = resizeRow->createChild<ComboBox>();
    resizeModeCombo->addItem("Crop");
    resizeModeCombo->addItem("Scale (Bilinear)");
    resizeModeCombo->addItem("Scale (Step)");
    resizeModeCombo->selectedIndex = 0;
    resizeModeCombo->preferredSize = Vec2(130 * Config::uiScale, 22 * Config::uiScale);
    resizeModeCombo->horizontalPolicy = SizePolicy::Fixed;

    // Anchor section below size fields
    auto anchorRow = layout->createChild<HBoxLayout>(6 * Config::uiScale);
    anchorRow->preferredSize = Vec2(0, 56 * Config::uiScale);

    auto anchorLabel = anchorRow->createChild<Label>("Anchor:");
    anchorLabel->preferredSize = Vec2(65 * Config::uiScale, 22 * Config::uiScale);

    anchorGrid = anchorRow->createChild<AnchorGridWidget>();
    anchorGrid->preferredSize = Vec2(50 * Config::uiScale, 50 * Config::uiScale);
    anchorGrid->horizontalPolicy = SizePolicy::Fixed;
    anchorGrid->verticalPolicy = SizePolicy::Fixed;

    anchorRow->createChild<Spacer>();

    layout->createChild<Spacer>();

    // Buttons
    auto btnRow = layout->createChild<HBoxLayout>(8 * Config::uiScale);
    btnRow->preferredSize = Vec2(0, 32 * Config::uiScale);

    btnRow->createChild<Spacer>();

    auto cancelBtn = btnRow->createChild<Button>("Cancel");
    cancelBtn->preferredSize = Vec2(80 * Config::uiScale, 28 * Config::uiScale);
    cancelBtn->onClick = [this]() { hide(); };

    auto okBtn = btnRow->createChild<Button>("OK");
    okBtn->preferredSize = Vec2(80 * Config::uiScale, 28 * Config::uiScale);
    okBtn->onClick = [this]() {
        newWidth = std::atoi(widthField->text.c_str());
        newHeight = std::atoi(heightField->text.c_str());
        // Map combo index to resize mode
        switch (resizeModeCombo->selectedIndex) {
            case 0: resizeMode = CanvasResizeMode::Crop; break;
            case 1: resizeMode = CanvasResizeMode::ScaleBilinear; break;
            case 2: resizeMode = CanvasResizeMode::ScaleNearest; break;
            default: resizeMode = CanvasResizeMode::Crop; break;
        }
        if (newWidth > 0 && newHeight > 0 && onConfirm) {
            onConfirm(newWidth, newHeight, anchorGrid->selectedX, anchorGrid->selectedY, resizeMode);
        }
        hide();
    };
}

void CanvasSizeDialog::show() {
    // Update fields from current document
    AppState& state = getAppState();
    if (state.activeDocument) {
        widthField->text = std::to_string(state.activeDocument->width);
        heightField->text = std::to_string(state.activeDocument->height);
    }
    // Reset anchor to center
    if (anchorGrid) {
        anchorGrid->selectedX = 0;
        anchorGrid->selectedY = 0;
    }
    Dialog::show();
}

// RenameDocumentDialog

RenameDocumentDialog::RenameDocumentDialog() : Dialog("Rename Document") {
    preferredSize = Vec2(280 * Config::uiScale, 140 * Config::uiScale);

    auto layout = createChild<VBoxLayout>(6 * Config::uiScale);

    // Header with background
    auto headerPanel = layout->createChild<Panel>();
    headerPanel->bgColor = Config::COLOR_PANEL_HEADER;
    headerPanel->preferredSize = Vec2(0, 24 * Config::uiScale);
    headerPanel->setPadding(4 * Config::uiScale);
    headerPanel->createChild<Label>("Rename Document");

    layout->createChild<Separator>();

    // Name row
    auto nameRow = layout->createChild<HBoxLayout>(6 * Config::uiScale);
    nameRow->preferredSize = Vec2(0, 26 * Config::uiScale);
    nameRow->createChild<Label>("Name:")->preferredSize = Vec2(55 * Config::uiScale, 22 * Config::uiScale);
    nameField = nameRow->createChild<TextField>();
    nameField->text = "Untitled";
    nameField->horizontalPolicy = SizePolicy::Expanding;

    layout->createChild<Spacer>();

    // Buttons
    auto btnRow = layout->createChild<HBoxLayout>(8 * Config::uiScale);
    btnRow->preferredSize = Vec2(0, 32 * Config::uiScale);

    btnRow->createChild<Spacer>();

    auto cancelBtn = btnRow->createChild<Button>("Cancel");
    cancelBtn->preferredSize = Vec2(70 * Config::uiScale, 26 * Config::uiScale);
    cancelBtn->onClick = [this]() { hide(); };

    auto okBtn = btnRow->createChild<Button>("Apply");
    okBtn->preferredSize = Vec2(70 * Config::uiScale, 26 * Config::uiScale);
    okBtn->onClick = [this]() {
        if (!nameField->text.empty() && onConfirm) {
            onConfirm(nameField->text);
        }
        hide();
    };
}

void RenameDocumentDialog::show() {
    // Update field from current document
    AppState& state = getAppState();
    if (state.activeDocument) {
        nameField->text = state.activeDocument->name;
    }
    Dialog::show();
}

// ConfirmDialog

ConfirmDialog::ConfirmDialog(const std::string& message) : Dialog("Confirm") {
    preferredSize = Vec2(350 * Config::uiScale, 150 * Config::uiScale);

    auto layout = createChild<VBoxLayout>(8 * Config::uiScale);

    // Header with background
    auto headerPanel = layout->createChild<Panel>();
    headerPanel->bgColor = Config::COLOR_PANEL_HEADER;
    headerPanel->preferredSize = Vec2(0, 24 * Config::uiScale);
    headerPanel->setPadding(4 * Config::uiScale);
    headerPanel->createChild<Label>("Confirm");

    layout->createChild<Separator>();

    messageLabel = layout->createChild<Label>(message);
    messageLabel->preferredSize = Vec2(0, 40 * Config::uiScale);
    messageLabel->centerHorizontal = true;

    layout->createChild<Spacer>();

    auto btnRow = layout->createChild<HBoxLayout>(8 * Config::uiScale);
    btnRow->preferredSize = Vec2(0, 32 * Config::uiScale);

    btnRow->createChild<Spacer>();

    auto noBtn = btnRow->createChild<Button>("No");
    noBtn->preferredSize = Vec2(80 * Config::uiScale, 28 * Config::uiScale);
    noBtn->onClick = [this]() {
        if (onResult) onResult(false);
        hide();
    };

    auto yesBtn = btnRow->createChild<Button>("Yes");
    yesBtn->preferredSize = Vec2(80 * Config::uiScale, 28 * Config::uiScale);
    yesBtn->onClick = [this]() {
        if (onResult) onResult(true);
        hide();
    };
}

void ConfirmDialog::setMessage(const std::string& msg) {
    if (messageLabel) {
        messageLabel->setText(msg);
    }
}

// LinkLabel

LinkLabel::LinkLabel(const std::string& txt, const std::string& link)
    : text(txt), url(link), fontSize(Config::defaultFontSize()) {
    Vec2 textSize = FontRenderer::instance().measureText(text, fontSize);
    preferredSize = Vec2(textSize.x + 4 * Config::uiScale, textSize.y + 4 * Config::uiScale);
}

void LinkLabel::renderSelf(Framebuffer& fb) {
    Rect global = globalBounds();
    u32 color = hovered ? hoverColor : textColor;

    // Center text
    Vec2 textSize = FontRenderer::instance().measureText(text, fontSize);
    f32 textX = global.x + (global.w - textSize.x) / 2;
    f32 textY = global.y + (global.h - textSize.y) / 2;

    FontRenderer::instance().renderText(fb, text,
        static_cast<i32>(textX), static_cast<i32>(textY), color, fontSize);

    // Draw underline
    i32 underlineY = static_cast<i32>(textY + textSize.y);
    fb.drawHorizontalLine(static_cast<i32>(textX), static_cast<i32>(textX + textSize.x), underlineY, color);
}

bool LinkLabel::onMouseMove(const MouseEvent& e) {
    bool wasHovered = hovered;
    hovered = globalBounds().contains(e.globalPosition);
    if (wasHovered != hovered) {
        getAppState().needsRedraw = true;
    }
    return false;
}

bool LinkLabel::onMouseDown(const MouseEvent& e) {
    if (globalBounds().contains(e.globalPosition)) {
        Platform::launchBrowser(url.c_str());
        return true;
    }
    return false;
}

void LinkLabel::onMouseLeave(const MouseEvent&) {
    if (hovered) {
        hovered = false;
        getAppState().needsRedraw = true;
    }
}

// AboutDialog

AboutDialog::AboutDialog() : Dialog("About") {
    preferredSize = Vec2(280 * Config::uiScale, 170 * Config::uiScale);

    auto layout = createChild<VBoxLayout>(8 * Config::uiScale);

    // Header with background
    auto headerPanel = layout->createChild<Panel>();
    headerPanel->bgColor = Config::COLOR_PANEL_HEADER;
    headerPanel->preferredSize = Vec2(0, 24 * Config::uiScale);
    headerPanel->setPadding(4 * Config::uiScale);
    auto headerLabel = headerPanel->createChild<Label>("About");
    headerLabel->horizontalPolicy = SizePolicy::Expanding;

    layout->createChild<Separator>();

    // Content area with centered links
    auto content = layout->createChild<VBoxLayout>(4 * Config::uiScale);
    content->verticalPolicy = SizePolicy::Expanding;

    content->createChild<Spacer>();

    // Pixel Placer link
    auto link1 = content->createChild<LinkLabel>("Pixel Placer", "https://pixelplacer.app");
    link1->horizontalPolicy = SizePolicy::Expanding;

    // Author link
    auto link2 = content->createChild<LinkLabel>("Gabor Szauer", "http://gabormakesgames.com");
    link2->horizontalPolicy = SizePolicy::Expanding;

    // Claude link
    auto link3 = content->createChild<LinkLabel>("Claude 4.5-Max", "https://claude.ai");
    link3->horizontalPolicy = SizePolicy::Expanding;

    content->createChild<Spacer>();

    // Close button
    auto btnRow = layout->createChild<HBoxLayout>(8 * Config::uiScale);
    btnRow->preferredSize = Vec2(0, 32 * Config::uiScale);
    btnRow->createChild<Spacer>();
    auto closeBtn = btnRow->createChild<Button>("Close");
    closeBtn->preferredSize = Vec2(80 * Config::uiScale, 28 * Config::uiScale);
    closeBtn->onClick = [this]() { hide(); };
    btnRow->createChild<Spacer>();
}
