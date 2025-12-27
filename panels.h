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

    PanelHeader(const std::string& text);
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

    NavigatorPanel();
    void setView(DocumentView* v);
    void updateZoomLabel();
    void render(Framebuffer& fb) override;
    void setEnabled(bool isEnabled);
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

    LayerPropsPanel();
    void buildCommonControls(VBoxLayout* layout);
    void setDocument(Document* doc);

    // DocumentObserver implementation
    void onActiveLayerChanged(i32 index) override;
    void onLayerChanged(i32 index) override;

    void rebuildForActiveLayer();
    void updateCommonControls();
    void updateLockedState();
    void setEnabledRecursive(Widget* widget, bool enabled);

    // ======== Type-specific builders ========
    void buildPixelLayerControls(PixelLayer* layer);
    void buildTextLayerControls(TextLayer* layer);
    void buildAdjustmentControls(AdjustmentLayer* layer);
    const char* getAdjustmentTypeName(AdjustmentType type);
    Slider* addSliderRow(const char* labelText, f32 min, f32 max, f32 value, std::function<void(f32)> onChange);
    void updateTypeSpecificContainerSize();
    void notifyAdjustmentChanged();

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

    void setEnabled(bool isEnabled);
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

    LayerListItem(i32 index, Document* doc);
    void setDisabled(bool d);
    void startEditing();
    void confirmEdit();
    void cancelEdit();
    void endEditing();
    void updateFromLayer();

    bool onMouseDown(const MouseEvent& e) override;
    bool onMouseDrag(const MouseEvent& e) override;
    bool onMouseUp(const MouseEvent& e) override;
    bool onKeyDown(const KeyEvent& e) override;
    void renderSelf(Framebuffer& fb) override;
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

    LayerPanel();
    void setDocument(Document* doc);
    void showAdjustmentMenu();
    void closeAdjustmentMenu();
    void setEditMode(LayerListItem* item);
    void clearEditMode();
    void updateDisabledState();
    void rebuildLayerList();
    void updateSelection();

    // DocumentObserver implementation
    void onLayerAdded(i32 index) override;
    void onLayerRemoved(i32 index) override;
    void onLayerMoved(i32 from, i32 to) override;
    void onLayerChanged(i32 index) override;
    void onActiveLayerChanged(i32 index) override;

    // Drag and drop helpers
    void startDrag(i32 layerIndex, Vec2 globalPos);
    void updateDropTarget(Vec2 globalPos);
    void commitDrag();
    void clearDragState();

    void render(Framebuffer& fb) override;
    void setEnabled(bool isEnabled);
};

#endif
