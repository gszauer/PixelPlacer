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

    DocumentViewWidget();
    ~DocumentViewWidget() override;

    void setDocument(Document* doc);
    void layout() override;
    void renderSelf(Framebuffer& fb) override;
    void drawEllipseOutline(Framebuffer& fb, i32 cx, i32 cy, i32 rx, i32 ry, u32 color);
    void renderSelectionPreview(Framebuffer& fb, Tool* tool);

    bool onMouseDown(const MouseEvent& e) override;
    bool onMouseUp(const MouseEvent& e) override;
    bool onMouseDrag(const MouseEvent& e) override;
    bool onMouseMove(const MouseEvent& e) override;
    bool onMouseWheel(const MouseEvent& e) override;
    void onMouseEnter(const MouseEvent& e) override;
    void onMouseLeave(const MouseEvent& e) override;

    // DocumentObserver
    void onDocumentChanged(const Rect& dirtyRect) override;
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
    static ToolType getButtonTypeForTool(ToolType type);

    ToolPalette();
    void updateColors();
    void layout() override;
    void addToolButton(ToolType type, const char* label);
    void selectTool(ToolType type);
    void setEnabled(bool isEnabled);
    void clearSelection();
};

// Tool options bar - context-sensitive to current tool
class ToolOptionsBar : public Panel {
public:
    HBoxLayout* layout = nullptr;
    i32 currentToolType = -1;  // Track current tool to detect changes
    bool lastHadSelection = false;  // Track selection state for Move tool
    bool pendingRebuild = false;  // Defer rebuild to avoid destroying widgets during callbacks

    // Common widget references (may be null depending on tool)
    NumberSlider* sizeSlider = nullptr;
    Label* hardnessLabel = nullptr;
    NumberSlider* hardnessSlider = nullptr;
    NumberSlider* opacitySlider = nullptr;
    NumberSlider* toleranceSlider = nullptr;
    Checkbox* contiguousCheck = nullptr;
    Checkbox* antiAliasCheck = nullptr;
    ComboBox* shapeCombo = nullptr;
    ComboBox* fillModeCombo = nullptr;
    Button* curveBtn = nullptr;
    ComboBox* pressureCombo = nullptr;
    i32 lastFillMode = -1;  // Track fill mode changes

    // Callback for switching tools (set by MainWindow)
    std::function<void(ToolType)> onSelectTool;

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

    // Clone-specific checkbox for sample mode
    Checkbox* sampleModeCheck = nullptr;

    // Dynamic sizing constants and helpers
    static constexpr f32 TOOLBAR_LABEL_PADDING = 6.0f;
    static constexpr f32 TOOLBAR_BTN_PADDING = 14.0f;
    static constexpr f32 TOOLBAR_ITEM_SPACING = 4.0f;
    static constexpr f32 TOOLBAR_GROUP_SPACING = 4.0f;

    f32 itemHeight() const { return 24 * Config::uiScale; }
    f32 sliderHeight() const { return 20 * Config::uiScale; }

    // Small inline helper methods
    Label* addLabel(const char* text) {
        auto* label = layout->createChild<Label>(text);
        Vec2 textSize = FontRenderer::instance().measureText(text, Config::defaultFontSize());
        f32 padding = TOOLBAR_LABEL_PADDING * Config::uiScale;
        Vec2 size = Vec2(textSize.x + padding * 2, itemHeight());
        label->preferredSize = size;
        label->minSize = size;  // Prevent shrinking below text width
        label->horizontalPolicy = SizePolicy::Fixed;
        return label;
    }

    Button* addButton(const char* text) {
        auto* btn = layout->createChild<Button>(text);
        Vec2 textSize = FontRenderer::instance().measureText(text, Config::defaultFontSize());
        f32 padding = TOOLBAR_BTN_PADDING * Config::uiScale;
        btn->preferredSize = Vec2(textSize.x + padding * 2, itemHeight());
        return btn;
    }

    void addItemSpacing() {
        layout->createChild<Spacer>(TOOLBAR_ITEM_SPACING * Config::uiScale, true);
    }

    void addGroupSpacing() {
        layout->createChild<Spacer>(TOOLBAR_GROUP_SPACING * Config::uiScale, true);
    }

    Slider* addSlider(f32 min, f32 max, f32 value, f32 width = 80.0f) {
        auto* slider = layout->createChild<Slider>(min, max, value);
        slider->preferredSize = Vec2(width * Config::uiScale, sliderHeight());
        return slider;
    }

    NumberSlider* addNumberSlider(f32 min, f32 max, f32 value, i32 decimals = 0, f32 width = 50.0f) {
        auto* slider = layout->createChild<NumberSlider>(min, max, value, decimals);
        slider->preferredSize = Vec2(width * Config::uiScale, itemHeight());
        return slider;
    }

    ComboBox* addComboBox(const std::vector<const char*>& items, i32 selectedIndex = 0) {
        auto* combo = layout->createChild<ComboBox>();
        f32 maxWidth = 0;
        for (const char* item : items) {
            combo->addItem(item);
            Vec2 textSize = FontRenderer::instance().measureText(item, Config::defaultFontSize());
            maxWidth = std::max(maxWidth, textSize.x);
        }
        combo->preferredSize = Vec2(maxWidth + 30 * Config::uiScale, itemHeight());
        combo->selectedIndex = selectedIndex;
        return combo;
    }

    Checkbox* addCheckbox(const char* text) {
        auto* check = layout->createChild<Checkbox>(text);
        Vec2 textSize = FontRenderer::instance().measureText(text, Config::defaultFontSize());
        f32 padding = TOOLBAR_LABEL_PADDING * Config::uiScale;
        check->preferredSize = Vec2(24 * Config::uiScale + textSize.x + padding, itemHeight());
        return check;
    }

    static bool isSelectionTool(ToolType t) {
        return t == ToolType::RectangleSelect || t == ToolType::EllipseSelect ||
               t == ToolType::FreeSelect || t == ToolType::PolygonSelect;
    }

    static bool isFillTool(ToolType t) {
        return t == ToolType::Fill || t == ToolType::Gradient;
    }

    // Constructor and method declarations
    ToolOptionsBar();
    void update();
    void applyPendingChanges();
    void clearOptions();
    void clear();
    void updateHardnessVisibility();
    void updateCurveVisibility();
    const char* getToolName(ToolType tool);
    void rebuildOptions();
    void buildBrushOptions();
    void buildCropOptions();
    void buildPanOptions();
    void buildEraserOptions();
    void updateEraserCurveVisibility();
    void buildDodgeBurnOptions();
    void updateDodgeBurnCurveVisibility();
    void buildZoomOptions();
    void buildCloneOptions();
    void updateCloneCurveVisibility();
    void buildSmudgeOptions();
    void updateSmudgeCurveVisibility();
    void buildFillOptions();
    void buildGradientOptions();
    void buildMoveOptions();
    void buildSelectionOptions();
    void buildMagicWandOptions();
    void buildColorPickerOptions();
};

// Drag area for custom title bar
class TitleBarDragArea : public Widget {
public:
    std::function<void(i32, i32)> onStartDrag;  // Called with root coordinates
    std::function<void()> onDoubleClick;
    u64 lastClickTime = 0;

    TitleBarDragArea();
    bool onMouseDown(const MouseEvent& e) override;
    void renderSelf(Framebuffer& fb) override;
};

// Window control button (minimize, maximize, close)
class WindowControlButton : public Widget {
public:
    enum class Type { Minimize, Maximize, Restore, Close };
    Type type;
    bool hovered = false;
    bool pressed = false;
    std::function<void()> onClick;

    WindowControlButton(Type t);
    void setType(Type t) { type = t; }
    void renderSelf(Framebuffer& fb) override;
    void onMouseEnter(const MouseEvent& e) override;
    void onMouseLeave(const MouseEvent& e) override;
    bool onMouseDown(const MouseEvent& e) override;
    bool onMouseUp(const MouseEvent& e) override;
};

// Menu bar with integrated title bar
class MenuBar : public Panel {
public:
    PopupMenu* activeMenu = nullptr;
    std::vector<std::pair<Button*, PopupMenu*>> menus;
    bool menuModeActive = false;
    u64 lastMenuCloseTime = 0;

    TitleBarDragArea* dragArea = nullptr;
    WindowControlButton* minimizeBtn = nullptr;
    WindowControlButton* maximizeBtn = nullptr;
    WindowControlButton* closeBtn = nullptr;
    HBoxLayout* menuLayout = nullptr;
    HBoxLayout* controlLayout = nullptr;

    std::function<void()> onNewDocument;
    std::function<void()> onCanvasSize;
    std::function<void()> onFitToScreen;
    std::function<void()> onRenameDocument;
    std::function<void()> onAbout;

    std::function<void(i32, i32)> onWindowDrag;
    std::function<void()> onWindowMinimize;
    std::function<void()> onWindowMaximize;
    std::function<void()> onWindowClose;
    std::function<bool()> isWindowMaximized;

    MenuBar();
    void updateMaximizeButton();
    void doLayout();
    void layout() override;
    bool onMouseMove(const MouseEvent& e) override;
    void closeActiveMenu();
    void setDocumentMenusEnabled(bool enabled);

    PopupMenu* createFileMenu();
    PopupMenu* createEditMenu();
    PopupMenu* createCanvasMenu();
    PopupMenu* createLayerMenu();
    PopupMenu* createSelectMenu();
    PopupMenu* createViewMenu();
    PopupMenu* createHelpMenu();
    void addMenu(HBoxLayout* layout, const char* name, PopupMenu* popup);

private:
    bool switchingMenus = false;
};

// Status bar
class StatusBar : public Panel {
public:
    // Container layouts
    HBoxLayout* leftLayout = nullptr;
    HBoxLayout* rightLayout = nullptr;

    // Left side: zoom, size, position
    Button* zoomButton = nullptr;
    Widget* zoomSeparator = nullptr;
    Label* sizeLabel = nullptr;
    Widget* sizeSeparator = nullptr;
    Label* positionLabel = nullptr;

    // Right side: UI scale controls
    Widget* scaleSeparator = nullptr;
    Label* scaleLabel = nullptr;
    Slider* scaleSlider = nullptr;
    Button* scale1xBtn = nullptr;
    Button* scale2xBtn = nullptr;
    Button* scale4xBtn = nullptr;

    std::function<void()> onFitToScreen;
    std::function<void(f32)> onScaleChanged;

    StatusBar();
    void layout() override;
    void update(const Vec2& mousePos, f32 zoom, u32 width, u32 height);
    void setEnabled(bool isEnabled);
};

// Vertical resize divider for resizing panels
class ResizeDivider : public Widget {
public:
    bool dragging = false;
    f32 dragStartX = 0;
    f32 dragStartWidth = 0;
    Widget* targetWidget = nullptr;
    f32 minWidth = 150.0f * Config::uiScale;
    f32 maxWidth = 600.0f * Config::uiScale;
    std::function<void()> onResized;

    ResizeDivider();
    void renderSelf(Framebuffer& fb) override;
    bool onMouseDown(const MouseEvent& e) override;
    bool onMouseDrag(const MouseEvent& e) override;
    bool onMouseUp(const MouseEvent& e) override;
    void onMouseEnter(const MouseEvent& e) override;
    void onMouseLeave(const MouseEvent& e) override;
};

// Horizontal resize divider for resizing panels vertically (drag up/down)
class VPanelResizer : public Widget {
public:
    bool dragging = false;
    f32 dragStartY = 0;
    f32 dragStartHeightAbove = 0;
    f32 dragStartHeightBelow = 0;
    Widget* aboveWidget = nullptr;
    Widget* belowWidget = nullptr;
    f32 minHeight = 50.0f * Config::uiScale;
    std::function<void()> onResized;

    VPanelResizer();
    void renderSelf(Framebuffer& fb) override;
    bool onMouseDown(const MouseEvent& e) override;
    bool onMouseDrag(const MouseEvent& e) override;
    bool onMouseUp(const MouseEvent& e) override;
    void onMouseEnter(const MouseEvent& e) override;
    void onMouseLeave(const MouseEvent& e) override;
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
    AboutDialog* aboutDialog = nullptr;

    // Track whether editing foreground or background color
    bool editingForegroundColor = true;

    // Track panel visibility state for change detection
    bool prevShowNavigator = true;
    bool prevShowProperties = true;
    bool prevShowLayers = true;

    // Constructor and method declarations
    MainWindow();
    void createDialogs();
    void buildUI();
    void applyDeferredChanges();
    void connectToDocument();
    void syncTabs();
    void switchToDocument(i32 index);
    void closeDocumentTab(i32 index);
    void addDocumentTab(Document* doc);
    void layout() override;
    void clampSidebarWidth();
    void repositionDialogs();
    Dialog* getActiveDialog();

    // Get selection bounds in screen coordinates for dirty region tracking
    // Returns empty rect if no selection or no document
    Recti getSelectionScreenBounds() const;
    bool onMouseDown(const MouseEvent& e) override;
    bool onMouseMove(const MouseEvent& e) override;
    void centerDialog(Dialog* dialog);
    void showNewDocumentDialog();
    void showCanvasSizeDialog();
    void showColorPickerDialog(const Color& initialColor);
    void showAboutDialog();
    void showPressureCurvePopup(f32 x, f32 y);
    void showNewBrushDialog(bool fromCurrentCanvas);
    void showRenameDocumentDialog();
    void showManageBrushesPopup(f32 x, f32 y);
    void showBrushTipPopup(f32 x, f32 y);
    void render(Framebuffer& fb) override;
};

#endif
