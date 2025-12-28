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

    PressureCurveWidget();

    // Get graph area bounds accounting for axis labels
    void getGraphBounds(Rect& global, f32& leftMargin, f32& topMargin, f32& graphW, f32& graphH) const;

    // Convert normalized coords (0-1) to global screen pixels
    Vec2 toPixel(const Vec2& normalized) const;

    // Convert global screen pixels to normalized coords (0-1)
    Vec2 toNormalized(const Vec2& pixel) const;

    void renderSelf(Framebuffer& fb) override;
    bool onMouseDown(const MouseEvent& e) override;
    bool onMouseDrag(const MouseEvent& e) override;
    bool onMouseUp(const MouseEvent& e) override;
    void reset();
};

// Pressure curve dialog
class PressureCurvePopup : public Dialog {
public:
    PressureCurveWidget* curveWidget = nullptr;

    PressureCurvePopup();

    void applyCurve();
    void renderSelf(Framebuffer& fb) override;
    void show() override;
    void show(f32 x, f32 y);
    void hide() override;
};

// Preview widget for brush tip alpha mask
class BrushTipPreviewWidget : public Widget {
public:
    std::vector<f32>* alphaMask = nullptr;
    u32 maskWidth = 0;
    u32 maskHeight = 0;

    BrushTipPreviewWidget();
    void render(Framebuffer& fb) override;
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

    NewBrushDialog();

    void show() override;
    void browseForFile();
    void loadImageFromPath(const std::string& path);
    void loadFromCanvas(TiledCanvas* canvas, u32 width, u32 height);
    void selectChannel(i32 index);
    void updatePreview();
    void createBrush();
    void hide() override;

private:
    std::vector<f32> previewMask;
};

// Popup for managing brush library
class ManageBrushesPopup : public Dialog {
public:
    ScrollView* brushScrollView = nullptr;
    VBoxLayout* brushList = nullptr;
    i32 editingIndex = -1;  // Index of brush being edited, -1 if none
    std::string editingName;  // Name being edited

    // Callbacks for opening other dialogs
    std::function<void()> onNewFromFile;
    std::function<void()> onNewFromCanvas;
    std::function<void()> onBrushDeleted;  // Called when a brush is deleted (for UI update)

    ManageBrushesPopup();

    void cancelEdit();
    void confirmEdit();
    void startEdit(i32 index);
    void deleteBrush(i32 index);
    void rebuild();
    void hide() override;
    void show() override;
    void show(f32 x, f32 y);
    void renderSelf(Framebuffer& fb) override;
};

// Popup for selecting brush tip and adjusting dynamics
class BrushTipSelectorPopup : public Dialog {
public:
    ScrollView* tipScrollView = nullptr;
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

    BrushTipSelectorPopup();

    void rebuild();
    void renderSelf(Framebuffer& fb) override;
    void updateFromState();
    void show() override;
    void show(f32 x, f32 y);
};

#endif
