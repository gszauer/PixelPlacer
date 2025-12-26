#ifndef _H_PANELS_
#define _H_PANELS_

#include "widget.h"
#include "layouts.h"
#include "basic_widgets.h"
#include "dialogs.h"
#include "document.h"
#include "document_view.h"
#include "app_state.h"
#include "config.h"
#include "blend.h"

// Styled panel header with background bar
class PanelHeader : public Widget {
public:
    std::string title;
    u32 bgColor = Config::COLOR_PANEL_HEADER;
    u32 textColor = Config::COLOR_TEXT;

    PanelHeader(const std::string& text) : title(text) {
        preferredSize = Vec2(0, Config::panelHeaderHeight());
        verticalPolicy = SizePolicy::Fixed;
    }

    void render(Framebuffer& fb) override;
};

// Navigator thumbnail widget - renders document preview with viewport rectangle
class NavigatorThumbnail : public Widget {
public:
    DocumentView* view = nullptr;
    bool dragging = false;

    // Cached thumbnail state
    i32 thumbX = 0, thumbY = 0;  // Thumbnail position in widget
    i32 thumbW = 0, thumbH = 0;  // Thumbnail dimensions
    f32 thumbScale = 1.0f;       // Scale from document to thumbnail

    void renderSelf(Framebuffer& fb) override;
    bool onMouseDown(const MouseEvent& e) override;
    bool onMouseDrag(const MouseEvent& e) override;
    bool onMouseUp(const MouseEvent& e) override;

private:
    u32 sampleDocumentAt(Document* doc, f32 docX, f32 docY);
    void drawViewportRect(Framebuffer& fb);
    void panToThumbnailPos(Vec2 localPos);
};

// Navigator panel - document thumbnail and zoom control
class NavigatorPanel : public Panel {
public:
    DocumentView* view = nullptr;
    NavigatorThumbnail* thumbnail = nullptr;
    Slider* zoomSlider = nullptr;
    Label* zoomLabel = nullptr;

    NavigatorPanel() {
        bgColor = Config::COLOR_PANEL;
        preferredSize = Vec2(Config::rightSidebarWidth(), 150 * Config::uiScale);

        auto layout = createChild<VBoxLayout>(0);

        // Header
        layout->createChild<PanelHeader>("Navigator");

        // Content area with padding
        auto content = layout->createChild<VBoxLayout>(4 * Config::uiScale);
        content->setPadding(4 * Config::uiScale);
        content->verticalPolicy = SizePolicy::Expanding;

        // Thumbnail area with document preview (expands to fill available space)
        thumbnail = content->createChild<NavigatorThumbnail>();
        thumbnail->verticalPolicy = SizePolicy::Expanding;
        thumbnail->minSize = Vec2(0, 40 * Config::uiScale);  // Minimum height

        // Zoom controls (pinned to bottom)
        auto zoomRow = content->createChild<HBoxLayout>(4 * Config::uiScale);
        zoomRow->preferredSize = Vec2(0, 24 * Config::uiScale);
        zoomRow->verticalPolicy = SizePolicy::Fixed;

        zoomLabel = zoomRow->createChild<Label>("100%");
        zoomLabel->minSize = Vec2(55 * Config::uiScale, 24 * Config::uiScale);
        zoomLabel->preferredSize = Vec2(55 * Config::uiScale, 24 * Config::uiScale);
        zoomLabel->horizontalPolicy = SizePolicy::Fixed;

        zoomSlider = zoomRow->createChild<Slider>(Config::MIN_ZOOM, Config::MAX_ZOOM, 1.0f);
        zoomSlider->horizontalPolicy = SizePolicy::Expanding;
        zoomSlider->minSize.x = 20 * Config::uiScale;  // Allow slider to shrink more
        zoomSlider->onChanged = [this](f32 value) {
            if (view) {
                view->setZoom(value);
                updateZoomLabel();
                getAppState().needsRedraw = true;
            }
        };
    }

    void setView(DocumentView* v) {
        view = v;
        if (thumbnail) thumbnail->view = v;
        updateZoomLabel();
    }

    void updateZoomLabel() {
        if (view && zoomLabel) {
            zoomLabel->setText(view->getZoomString());
        }
        // Also sync slider to current zoom (clamped to slider range)
        if (view && zoomSlider) {
            f32 zoom = view->zoom;
            // Clamp to slider range for display
            f32 clampedZoom = std::max(Config::MIN_ZOOM, std::min(Config::MAX_ZOOM, zoom));
            zoomSlider->setValue(clampedZoom);
        }
    }

    void render(Framebuffer& fb) override {
        // Sync zoom controls with document view before rendering
        // This ensures slider/label stay in sync when zoom changes externally
        updateZoomLabel();
        Panel::render(fb);
    }

    void setEnabled(bool isEnabled) {
        if (zoomSlider) {
            zoomSlider->enabled = isEnabled;
        }
        if (!isEnabled && zoomLabel) {
            zoomLabel->setText("100%");
        }
    }
};

// Layer properties panel - context sensitive to layer type
class LayerPropsPanel : public Panel, public DocumentObserver {
public:
    // Common controls
    Slider* opacitySlider = nullptr;
    ComboBox* blendModeCombo = nullptr;

    // Dynamic section for layer-type-specific controls
    ScrollView* scrollView = nullptr;
    VBoxLayout* scrollContent = nullptr;
    VBoxLayout* typeSpecificContainer = nullptr;
    Label* layerTypeLabel = nullptr;

    // Text layer color swatch (for updating after color pick)
    ColorSwatch* textColorSwatch = nullptr;

    // Font dropdown for text layers
    ComboBox* fontCombo = nullptr;

    // Callback for requesting color picker from main window
    std::function<void(const Color&, std::function<void(const Color&)>)> onRequestColorPicker;

    // Callback for requesting font file dialog (returns path and data on success)
    std::function<void(std::function<void(const std::string&, std::vector<u8>&)>)> onRequestLoadFont;

    Document* document = nullptr;

    LayerPropsPanel() {
        bgColor = Config::COLOR_PANEL;
        preferredSize = Vec2(Config::rightSidebarWidth(), 300 * Config::uiScale);
        verticalPolicy = SizePolicy::Expanding;

        auto layout = createChild<VBoxLayout>(0);

        // Header
        layout->createChild<PanelHeader>("Layer Properties");

        // ScrollView for all content (enables scrolling when many controls)
        scrollView = layout->createChild<ScrollView>();
        scrollView->verticalPolicy = SizePolicy::Expanding;

        // Content inside scroll view
        scrollContent = scrollView->createChild<VBoxLayout>(4 * Config::uiScale);
        scrollContent->setPadding(4 * Config::uiScale);

        // Common controls section
        buildCommonControls(scrollContent);

        // Separator
        scrollContent->createChild<Separator>(true);

        // Type-specific container
        typeSpecificContainer = scrollContent->createChild<VBoxLayout>(4 * Config::uiScale);
    }

    void buildCommonControls(VBoxLayout* layout);

    void setDocument(Document* doc) {
        if (document) {
            document->removeObserver(this);
        }
        document = doc;
        if (document) {
            document->addObserver(this);
        }
        rebuildForActiveLayer();
    }

    // DocumentObserver implementation
    void onActiveLayerChanged(i32 index) override {
        rebuildForActiveLayer();
    }

    void onLayerChanged(i32 index) override {
        if (document && index == document->activeLayerIndex) {
            updateCommonControls();
        }
    }

    void rebuildForActiveLayer();

    void updateCommonControls() {
        if (!document) return;
        LayerBase* layer = document->getActiveLayer();
        if (!layer) return;

        if (opacitySlider) opacitySlider->setValue(layer->opacity);
        if (blendModeCombo) blendModeCombo->selectedIndex = static_cast<i32>(layer->blend);

        // Update enabled state based on lock
        updateLockedState();
    }

    void updateLockedState() {
        if (!document) return;
        LayerBase* layer = document->getActiveLayer();
        bool locked = layer ? layer->locked : false;

        // Disable common controls when locked
        if (opacitySlider) opacitySlider->enabled = !locked;
        if (blendModeCombo) blendModeCombo->enabled = !locked;

        // Disable all type-specific controls
        if (typeSpecificContainer) {
            setEnabledRecursive(typeSpecificContainer, !locked);
        }
    }

    void setEnabledRecursive(Widget* widget, bool enabled) {
        if (!widget) return;
        widget->enabled = enabled;
        for (auto& child : widget->children) {
            setEnabledRecursive(child.get(), enabled);
        }
    }

    // ======== Type-specific builders ========
    void buildPixelLayerControls(PixelLayer* layer);
    void buildTextLayerControls(TextLayer* layer);
    void buildAdjustmentControls(AdjustmentLayer* layer);
    const char* getAdjustmentTypeName(AdjustmentType type);
    Slider* addSliderRow(const char* labelText, f32 min, f32 max, f32 value, std::function<void(f32)> onChange);
    void updateTypeSpecificContainerSize();

    void notifyAdjustmentChanged() {
        if (document) {
            document->notifyLayerChanged(document->activeLayerIndex);
            getAppState().needsRedraw = true;
        }
    }

    void buildBrightnessContrastControls(AdjustmentLayer* layer);
    void buildTemperatureTintControls(AdjustmentLayer* layer);
    void buildHueSaturationControls(AdjustmentLayer* layer);
    void buildVibranceControls(AdjustmentLayer* layer);
    void buildColorBalanceControls(AdjustmentLayer* layer);
    void buildHighlightsShadowsControls(AdjustmentLayer* layer);
    void buildExposureControls(AdjustmentLayer* layer);
    void buildLevelsControls(AdjustmentLayer* layer);
    void buildInvertControls(AdjustmentLayer* layer);
    void buildBlackAndWhiteControls(AdjustmentLayer* layer);

    void setEnabled(bool isEnabled) {
        // Hide all controls when disabled (no document open)
        if (scrollContent) {
            scrollContent->visible = isEnabled;
        }

        if (opacitySlider) opacitySlider->enabled = isEnabled;
        if (blendModeCombo) blendModeCombo->enabled = isEnabled;

        // When disabled, clear type-specific controls
        if (!isEnabled && typeSpecificContainer) {
            typeSpecificContainer->children.clear();
        }
        if (!isEnabled && layerTypeLabel) {
            layerTypeLabel->setText("");
        }
    }
};

// Forward declaration for edit mode callback
class LayerPanel;

// Layer thumbnail widget - renders layer preview
class LayerThumbnail : public Widget {
public:
    i32 layerIndex = -1;

    void renderSelf(Framebuffer& fb) override;

private:
    u32 sampleBilinear(const TiledCanvas& canvas, f32 x, f32 y);
    void renderPixelLayer(Framebuffer& fb, PixelLayer* layer, i32 thumbX, i32 thumbY, i32 thumbW, i32 thumbH);
    void renderTextLayer(Framebuffer& fb, TextLayer* layer, i32 thumbX, i32 thumbY, i32 thumbW, i32 thumbH);
    void renderAdjustmentLayer(Framebuffer& fb, AdjustmentLayer* layer, i32 thumbX, i32 thumbY, i32 thumbW, i32 thumbH);
};

// Layer list item with inline rename support
class LayerListItem : public Panel {
public:
    i32 layerIndex = -1;
    Document* document = nullptr;
    bool selected = false;
    bool editing = false;
    bool disabled = false;  // When another layer is being edited

    // View mode widgets
    HBoxLayout* viewLayout = nullptr;
    IconButton* visBtn = nullptr;
    LayerThumbnail* thumbnail = nullptr;
    Label* nameLabel = nullptr;
    IconButton* lockBtn = nullptr;

    // Edit mode widgets
    HBoxLayout* editLayout = nullptr;
    TextField* nameField = nullptr;
    IconButton* confirmBtn = nullptr;
    IconButton* cancelBtn = nullptr;

    // Double-click detection
    u64 lastClickTime = 0;

    // Drag state
    bool dragPending = false;
    Vec2 dragStartPos;

    std::function<void(i32)> onSelect;
    std::function<void(LayerListItem*)> onEditStart;
    std::function<void()> onEditEnd;
    std::function<void(i32, Vec2)> onDragStart;      // (layerIndex, globalPos)
    std::function<void(Vec2)> onDragMove;            // (globalPos)
    std::function<void()> onDragEnd;                 // commit drag
    std::function<void()> onDragCancel;              // cancel drag

    LayerListItem(i32 index, Document* doc) : layerIndex(index), document(doc) {
        bgColor = Config::COLOR_PANEL;
        preferredSize = Vec2(0, Config::layerItemHeight());
        verticalPolicy = SizePolicy::Fixed;

        // === View mode layout ===
        viewLayout = createChild<HBoxLayout>(4 * Config::uiScale);
        viewLayout->setPadding(4 * Config::uiScale);

        // Visibility icon button (left side)
        visBtn = viewLayout->createChild<IconButton>();
        visBtn->preferredSize = Vec2(24 * Config::uiScale, 24 * Config::uiScale);
        visBtn->renderIcon = [this](Framebuffer& fb, const Rect& r, u32 color) {
            bool visible = true;
            if (document && layerIndex >= 0 && layerIndex < static_cast<i32>(document->layers.size())) {
                LayerBase* layer = document->getLayer(layerIndex);
                if (layer) visible = layer->visible;
            }
            // U+F06D0 = visible eye, U+F06D1 = hidden eye
            const char* icon = visible ? "\xF3\xB0\x9B\x90" : "\xF3\xB0\x9B\x91";
            FontRenderer::instance().renderIconCentered(fb, icon, r, color, Config::defaultFontSize(), "Material Icons");
        };
        visBtn->onClick = [this]() {
            if (disabled) return;  // Block when disabled
            if (!document || layerIndex < 0 || layerIndex >= static_cast<i32>(document->layers.size())) return;
            LayerBase* layer = document->getLayer(layerIndex);
            if (layer) {
                layer->visible = !layer->visible;
                document->notifyLayerChanged(layerIndex);
                getAppState().needsRedraw = true;
            }
        };

        // Thumbnail (shows layer content)
        thumbnail = viewLayout->createChild<LayerThumbnail>();
        thumbnail->layerIndex = index;
        thumbnail->preferredSize = Vec2(40 * Config::uiScale, 40 * Config::uiScale);
        thumbnail->horizontalPolicy = SizePolicy::Fixed;

        // Name (expands to fill, clips if too long)
        nameLabel = viewLayout->createChild<Label>("");
        nameLabel->horizontalPolicy = SizePolicy::Expanding;

        // Lock icon button (right side)
        lockBtn = viewLayout->createChild<IconButton>();
        lockBtn->preferredSize = Vec2(24 * Config::uiScale, 24 * Config::uiScale);
        lockBtn->renderIcon = [this](Framebuffer& fb, const Rect& r, u32 color) {
            bool locked = false;
            if (document && layerIndex >= 0 && layerIndex < static_cast<i32>(document->layers.size())) {
                LayerBase* layer = document->getLayer(layerIndex);
                if (layer) locked = layer->locked;
            }
            // U+F0341 = locked, U+F0FC7 = unlocked
            const char* icon = locked ? "\xF3\xB0\x8D\x81" : "\xF3\xB0\xBF\x87";
            FontRenderer::instance().renderIconCentered(fb, icon, r, color, Config::defaultFontSize(), "Material Icons");
        };
        lockBtn->onClick = [this]() {
            if (disabled) return;  // Block when disabled
            if (!document || layerIndex < 0 || layerIndex >= static_cast<i32>(document->layers.size())) return;
            LayerBase* layer = document->getLayer(layerIndex);
            if (layer) {
                layer->locked = !layer->locked;
                document->notifyLayerChanged(layerIndex);  // Notify so properties panel updates
                getAppState().needsRedraw = true;
            }
        };

        // === Edit mode layout (hidden by default) ===
        editLayout = createChild<HBoxLayout>(4 * Config::uiScale);
        editLayout->setPadding(4 * Config::uiScale);
        editLayout->visible = false;

        // Text field for name editing
        nameField = editLayout->createChild<TextField>();
        nameField->horizontalPolicy = SizePolicy::Expanding;
        nameField->preferredSize = Vec2(0, 24 * Config::uiScale);

        // Confirm button
        confirmBtn = editLayout->createChild<IconButton>();
        confirmBtn->preferredSize = Vec2(28 * Config::uiScale, 24 * Config::uiScale);
        confirmBtn->renderIcon = [](Framebuffer& fb, const Rect& r, u32 color) {
            FontRenderer::instance().renderIconCentered(fb, "\xF3\xB0\x84\xAC", r, color, Config::defaultFontSize(), "Material Icons");  // U+F012C check
        };
        confirmBtn->onClick = [this]() { confirmEdit(); };

        // Cancel button
        cancelBtn = editLayout->createChild<IconButton>();
        cancelBtn->preferredSize = Vec2(28 * Config::uiScale, 24 * Config::uiScale);
        cancelBtn->renderIcon = [](Framebuffer& fb, const Rect& r, u32 color) {
            FontRenderer::instance().renderIconCentered(fb, "\xF3\xB0\x96\xAD", r, color, Config::defaultFontSize(), "Material Icons");  // U+F05AD close
        };
        cancelBtn->onClick = [this]() { cancelEdit(); };

        updateFromLayer();
    }

    void setDisabled(bool d) {
        disabled = d;
        if (visBtn) visBtn->enabled = !d;
        if (lockBtn) lockBtn->enabled = !d;
        if (nameLabel) nameLabel->textColor = d ? Config::COLOR_TEXT_DIM : Config::COLOR_TEXT;
    }

    void startEditing();

    void confirmEdit() {
        if (!editing) return;
        if (document && layerIndex >= 0 && layerIndex < static_cast<i32>(document->layers.size())) {
            LayerBase* layer = document->getLayer(layerIndex);
            if (layer && !nameField->text.empty()) {
                layer->name = nameField->text;
                document->notifyLayerChanged(layerIndex);
            }
        }
        endEditing();
    }

    void cancelEdit() { endEditing(); }
    void endEditing();

    void updateFromLayer() {
        if (editing) return;
        if (!document || layerIndex < 0 || layerIndex >= static_cast<i32>(document->layers.size())) return;
        LayerBase* layer = document->getLayer(layerIndex);
        if (layer && nameLabel) nameLabel->setText(layer->name);
    }

    bool onMouseDown(const MouseEvent& e) override;
    bool onMouseDrag(const MouseEvent& e) override;
    bool onMouseUp(const MouseEvent& e) override;
    bool onKeyDown(const KeyEvent& e) override;

    void renderSelf(Framebuffer& fb) override {
        if (disabled) bgColor = Config::COLOR_BACKGROUND_DISABLED;
        else if (selected) bgColor = Config::COLOR_ACCENT;
        else if (hovered) bgColor = Config::COLOR_HOVER;
        else bgColor = Config::COLOR_PANEL;
        Panel::renderSelf(fb);
    }
};

// Layer panel - layer list and controls
class LayerPanel : public Panel, public DocumentObserver {
public:
    Document* document = nullptr;
    ScrollView* scrollView = nullptr;  // Scrollable container for layer list
    VBoxLayout* layerList = nullptr;
    HBoxLayout* toolbar = nullptr;
    LayerListItem* editingItem = nullptr;  // Currently editing item (for modal behavior)
    PopupMenu* adjustmentMenu = nullptr;   // Popup for adjustment layer types
    IconButton* adjustmentBtn = nullptr;       // Reference to A button for positioning
    IconButton* addPixelBtn = nullptr;
    IconButton* addTextBtn = nullptr;
    IconButton* dupBtn = nullptr;
    IconButton* delBtn = nullptr;

    // Drag and drop state
    bool dragging = false;
    i32 dragSourceIndex = -1;      // Document layer index being dragged
    i32 dropTargetIndex = -1;      // Where to insert (-1 = none)
    i32 dropGapIndex = -1;         // UI gap position for rendering insert line
    Vec2 dragStartPos;             // Initial mouse position (global)
    bool dragPending = false;      // Waiting for threshold to start drag
    static constexpr f32 DRAG_THRESHOLD = 5.0f;

    LayerPanel() {
        bgColor = Config::COLOR_PANEL;
        preferredSize = Vec2(Config::rightSidebarWidth(), 200 * Config::uiScale);
        verticalPolicy = SizePolicy::Expanding;

        auto layout = createChild<VBoxLayout>(0);

        // Header
        layout->createChild<PanelHeader>("Layers");

        // Content area with padding
        auto content = layout->createChild<VBoxLayout>(4 * Config::uiScale);
        content->setPadding(4 * Config::uiScale);
        content->verticalPolicy = SizePolicy::Expanding;

        // ScrollView for layer list (enables scrolling when many layers)
        scrollView = content->createChild<ScrollView>();
        scrollView->verticalPolicy = SizePolicy::Expanding;

        // Layer list inside scroll view (can grow beyond viewport)
        layerList = scrollView->createChild<VBoxLayout>(2 * Config::uiScale);

        // Footer toolbar with buttons
        toolbar = content->createChild<HBoxLayout>(4 * Config::uiScale);
        toolbar->preferredSize = Vec2(0, 28 * Config::uiScale);
        toolbar->verticalPolicy = SizePolicy::Fixed;

        // Left side buttons: P (pixel layer), A (adjustment), T (text layer)
        addPixelBtn = toolbar->createChild<IconButton>();
        addPixelBtn->preferredSize = Vec2(28 * Config::uiScale, 24 * Config::uiScale);
        addPixelBtn->renderIcon = [](Framebuffer& fb, const Rect& r, u32 color) {
            FontRenderer::instance().renderIconCentered(fb, "\xF3\xB0\x84\xBA", r, color, Config::defaultFontSize(), "Material Icons");  // U+F013A pixel layer
        };
        addPixelBtn->onClick = [this]() {
            if (editingItem) return;  // Block during edit
            if (document) {
                document->addPixelLayer();
                rebuildLayerList();
                getAppState().needsRedraw = true;
            }
        };

        adjustmentBtn = toolbar->createChild<IconButton>();
        adjustmentBtn->preferredSize = Vec2(28 * Config::uiScale, 24 * Config::uiScale);
        adjustmentBtn->renderIcon = [](Framebuffer& fb, const Rect& r, u32 color) {
            FontRenderer::instance().renderIconCentered(fb, "\xF3\xB0\xBF\x81", r, color, Config::defaultFontSize(), "Material Icons");  // U+F0FC1 adjustment layer
        };
        adjustmentBtn->onClick = [this]() {
            if (editingItem) return;  // Block during edit
            showAdjustmentMenu();
        };

        // Create adjustment popup menu
        adjustmentMenu = createChild<PopupMenu>();
        auto addAdjustment = [this](AdjustmentType type) {
            if (document) {
                document->addAdjustmentLayer(type);
                rebuildLayerList();
                getAppState().needsRedraw = true;
            }
            closeAdjustmentMenu();
        };
        adjustmentMenu->addItem("Brightness & Contrast", "", [=]() { addAdjustment(AdjustmentType::BrightnessContrast); });
        adjustmentMenu->addItem("Temperature & Tint", "", [=]() { addAdjustment(AdjustmentType::TemperatureTint); });
        adjustmentMenu->addItem("Hue & Saturation", "", [=]() { addAdjustment(AdjustmentType::HueSaturation); });
        adjustmentMenu->addItem("Vibrance", "", [=]() { addAdjustment(AdjustmentType::Vibrance); });
        adjustmentMenu->addItem("Color Balance", "", [=]() { addAdjustment(AdjustmentType::ColorBalance); });
        adjustmentMenu->addItem("Highlights & Shadows", "", [=]() { addAdjustment(AdjustmentType::HighlightsShadows); });
        adjustmentMenu->addItem("Exposure", "", [=]() { addAdjustment(AdjustmentType::Exposure); });
        adjustmentMenu->addSeparator();
        adjustmentMenu->addItem("Levels", "", [=]() { addAdjustment(AdjustmentType::Levels); });
        adjustmentMenu->addSeparator();
        adjustmentMenu->addItem("Invert", "", [=]() { addAdjustment(AdjustmentType::Invert); });
        adjustmentMenu->addItem("Black & White", "", [=]() { addAdjustment(AdjustmentType::BlackAndWhite); });

        addTextBtn = toolbar->createChild<IconButton>();
        addTextBtn->preferredSize = Vec2(28 * Config::uiScale, 24 * Config::uiScale);
        addTextBtn->renderIcon = [](Framebuffer& fb, const Rect& r, u32 color) {
            FontRenderer::instance().renderIconCentered(fb, "\xF3\xB0\xBE\xB9", r, color, Config::defaultFontSize(), "Material Icons");  // U+F0FB9 text layer
        };
        addTextBtn->onClick = [this]() {
            if (editingItem) return;  // Block during edit
            if (document) {
                document->addTextLayer("Text");
                rebuildLayerList();
                getAppState().needsRedraw = true;
            }
        };

        // Spacer pushes buttons to the right
        toolbar->createChild<Spacer>();

        // Right side: duplicate button, then delete button
        dupBtn = toolbar->createChild<IconButton>();
        dupBtn->preferredSize = Vec2(28 * Config::uiScale, 24 * Config::uiScale);
        dupBtn->renderIcon = [](Framebuffer& fb, const Rect& r, u32 color) {
            FontRenderer::instance().renderIconCentered(fb, "\xF3\xB0\x86\x8F", r, color, Config::defaultFontSize(), "Material Icons");  // U+F018F clone/duplicate
        };
        dupBtn->onClick = [this]() {
            if (editingItem) return;  // Block during edit
            if (document && document->activeLayerIndex >= 0) {
                document->duplicateLayer(document->activeLayerIndex);
                rebuildLayerList();
                getAppState().needsRedraw = true;
            }
        };

        delBtn = toolbar->createChild<IconButton>();
        delBtn->preferredSize = Vec2(28 * Config::uiScale, 24 * Config::uiScale);
        delBtn->renderIcon = [](Framebuffer& fb, const Rect& r, u32 color) {
            FontRenderer::instance().renderIconCentered(fb, "\xF3\xB0\xA9\xBA", r, color, Config::defaultFontSize(), "Material Icons");  // U+F0A7A remove layer
        };
        delBtn->onClick = [this]() {
            if (editingItem) return;  // Block during edit
            if (document && document->layers.size() > 1) {
                document->removeLayer(document->activeLayerIndex);
                rebuildLayerList();
                getAppState().needsRedraw = true;
            }
        };
    }

    void setDocument(Document* doc) {
        if (document) {
            document->removeObserver(this);
        }
        document = doc;
        if (document) {
            document->addObserver(this);
            rebuildLayerList();
        }
    }

    void showAdjustmentMenu();

    void closeAdjustmentMenu() {
        if (adjustmentMenu && adjustmentMenu->visible) {
            adjustmentMenu->hide();
            OverlayManager::instance().unregisterOverlay(adjustmentMenu);
        }
    }

    void setEditMode(LayerListItem* item) {
        editingItem = item;
        updateDisabledState();
    }

    void clearEditMode() {
        editingItem = nullptr;
        updateDisabledState();
    }

    void updateDisabledState();

    void rebuildLayerList();

    void updateSelection() {
        if (!layerList || !document) return;

        for (auto& child : layerList->children) {
            LayerListItem* item = dynamic_cast<LayerListItem*>(child.get());
            if (item) {
                item->selected = (item->layerIndex == document->activeLayerIndex);
            }
        }
    }

    // DocumentObserver implementation
    void onLayerAdded(i32 index) override { if (!editingItem) rebuildLayerList(); }
    void onLayerRemoved(i32 index) override { if (!editingItem) rebuildLayerList(); }
    void onLayerMoved(i32 from, i32 to) override { if (!editingItem) rebuildLayerList(); }
    void onLayerChanged(i32 index) override { /* update thumbnail */ }
    void onActiveLayerChanged(i32 index) override { if (!editingItem) updateSelection(); }

    // Drag and drop helpers
    void startDrag(i32 layerIndex, Vec2 globalPos);
    void updateDropTarget(Vec2 globalPos);
    void commitDrag();
    void clearDragState();

    void render(Framebuffer& fb) override;

    void setEnabled(bool isEnabled) {
        if (addPixelBtn) addPixelBtn->enabled = isEnabled;
        if (addTextBtn) addTextBtn->enabled = isEnabled;
        if (adjustmentBtn) adjustmentBtn->enabled = isEnabled;
        if (dupBtn) dupBtn->enabled = isEnabled;
        if (delBtn) delBtn->enabled = isEnabled;

        // When disabled (no document), clear the layer list
        if (!isEnabled && layerList) {
            layerList->children.clear();
        }
    }
};

#endif
