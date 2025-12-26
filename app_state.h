#ifndef _H_APP_STATE_
#define _H_APP_STATE_

#include "types.h"
#include "primitives.h"
#include "tiled_canvas.h"
#include "brush_tip.h"
#include <vector>
#include <memory>
#include <string>
#include <algorithm>
#include <functional>

// Forward declarations
class Document;
class DocumentView;
class Tool;

// Clipboard data for copy/paste
struct Clipboard {
    std::unique_ptr<TiledCanvas> pixels;
    u32 width = 0;
    u32 height = 0;
    i32 originX = 0;  // Original X position for paste in place
    i32 originY = 0;  // Original Y position for paste in place

    bool hasContent() const { return pixels != nullptr && width > 0 && height > 0; }

    void clear() {
        pixels.reset();
        width = 0;
        height = 0;
        originX = 0;
        originY = 0;
    }
};

// Deferred file dialog request (to avoid X11 mouse grab issues on Linux)
struct PendingFileDialog {
    bool active = false;
    bool isSaveDialog = false;
    std::string title;
    std::string defaultName;  // For save dialogs
    std::string filters;
    std::function<void(const std::string&)> callback;
};

// Application-wide state
struct AppState {
    // Open documents
    std::vector<std::unique_ptr<Document>> documents;
    Document* activeDocument = nullptr;
    i32 activeDocumentIndex = -1;

    // Global colors
    Color foregroundColor = Color::black();
    Color backgroundColor = Color::white();

    // Global tool settings
    f32 brushSize = 10.0f;
    f32 brushHardness = 0.8f;
    f32 brushOpacity = 1.0f;     // Stroke ceiling - max coverage per stroke
    f32 brushFlow = 1.0f;        // Per-dab opacity (accumulates within stroke)
    f32 brushSpacing = 0.25f;

    // Pressure sensitivity: 0=None, 1=Size, 2=Opacity, 3=Flow
    i32 brushPressureMode = 0;
    i32 eraserPressureMode = 0;

    // Pressure curve (cubic bezier control points, default = linear)
    Vec2 pressureCurveCP1 = Vec2(0.33f, 0.33f);
    Vec2 pressureCurveCP2 = Vec2(0.66f, 0.66f);

    // Custom brush tips
    BrushLibrary brushLibrary;
    i32 currentBrushTipIndex = -1;  // -1 = round brush (default)
    f32 brushAngle = 0.0f;          // Current tip angle in degrees
    BrushDynamics brushDynamics;    // Jitter and scattering settings
    bool brushShowBoundingBox = false;  // Show rectangular cursor for custom tips

    // Selection settings
    bool selectionAntiAlias = true;

    // Move tool settings
    bool moveSelectionContent = true;  // When true, moving a selection also moves pixels

    // Fill tool settings
    i32 fillMode = 0;             // 0=Solid, 1=Linear Gradient, 2=Radial Gradient
    f32 fillTolerance = 32.0f;    // 0-510 color difference threshold (Euclidean in RGBA)
    bool fillContiguous = true;   // If false, fills all similar colors globally

    // Magic wand tool settings
    f32 wandTolerance = 32.0f;    // 0-510 color difference threshold
    bool wandContiguous = true;   // If false, selects all similar colors globally

    // Clone stamp tool settings
    bool cloneSampleMode = true;  // When true, next click will sample source point
    bool cloneSourceSet = false;  // Whether a source point has been sampled
    Vec2 cloneSourcePos;          // Absolute source position sampled
    i32 clonePressureMode = 0;    // 0=None, 1=Size, 2=Opacity, 3=Flow

    // Smudge tool settings
    i32 smudgePressureMode = 0;   // 0=None, 1=Size, 2=Opacity, 3=Flow

    // Dodge/Burn tool settings (shared since they're similar)
    i32 dodgeBurnPressureMode = 0;  // 0=None, 1=Size, 2=Exposure, 3=Flow

    // Zoom tool settings
    i32 zoomClickMode = 0;  // 0=Zoom In, 1=Zoom Out

    // Color picker tool settings
    i32 colorPickerSampleMode = 0;  // 0=Current Layer, 1=Current & Below, 2=All Layers

    // Current tool type
    i32 currentToolType = 0;

    // View panels visibility
    bool showNavigator = true;
    bool showProperties = true;
    bool showLayers = true;

    // Window state
    bool running = true;
    bool needsRedraw = true;

    // Mouse state
    Vec2 mousePosition;
    bool mouseDown = false;
    MouseButton mouseButton = MouseButton::None;
    bool spaceHeld = false;  // For temporary pan

    // Focused widget
    Widget* focusedWidget = nullptr;
    Widget* hoveredWidget = nullptr;
    Widget* capturedWidget = nullptr;  // Widget capturing mouse input

    // Clipboard for copy/paste
    Clipboard clipboard;

    // Deferred file dialog (for Linux X11 mouse grab workaround)
    PendingFileDialog pendingFileDialog;

    void requestOpenFileDialog(const std::string& title, const std::string& filters,
                               std::function<void(const std::string&)> callback) {
        pendingFileDialog.active = true;
        pendingFileDialog.isSaveDialog = false;
        pendingFileDialog.title = title;
        pendingFileDialog.defaultName = "";
        pendingFileDialog.filters = filters;
        pendingFileDialog.callback = callback;
    }

    void requestSaveFileDialog(const std::string& title, const std::string& defaultName,
                               const std::string& filters,
                               std::function<void(const std::string&)> callback) {
        pendingFileDialog.active = true;
        pendingFileDialog.isSaveDialog = true;
        pendingFileDialog.title = title;
        pendingFileDialog.defaultName = defaultName;
        pendingFileDialog.filters = filters;
        pendingFileDialog.callback = callback;
    }

    // Legacy method for compatibility
    void requestFileDialog(const std::string& title, const std::string& filters,
                           std::function<void(const std::string&)> callback) {
        requestOpenFileDialog(title, filters, callback);
    }

    // Document management
    Document* createDocument(u32 width, u32 height, const std::string& name = "Untitled");
    void closeDocument(Document* doc);
    void closeDocument(i32 index);
    void setActiveDocument(i32 index);
    void setActiveDocument(Document* doc);

    // Callback for when active document changes (for UI updates)
    std::function<void()> onActiveDocumentChanged;

    // Color swapping
    void swapColors() {
        std::swap(foregroundColor, backgroundColor);
    }

    void resetColors() {
        foregroundColor = Color::black();
        backgroundColor = Color::white();
    }
};

// Global state accessor
AppState& getAppState();

// Evaluate cubic bezier pressure curve
// Input: raw pressure (0-1), control points
// Output: adjusted pressure (0-1)
inline f32 evaluatePressureCurve(f32 inputPressure, Vec2 cp1, Vec2 cp2) {
    // Fixed endpoints: P0=(0,0), P3=(1,1)
    // Control points: P1=cp1, P2=cp2
    // We need to find Y given X (the input pressure)
    // Use binary search to find t where bezierX(t) ≈ inputPressure

    f32 t = inputPressure;  // Initial guess

    // Binary search for t
    f32 low = 0.0f, high = 1.0f;
    for (int i = 0; i < 10; ++i) {  // 10 iterations gives ~0.001 precision
        t = (low + high) * 0.5f;

        // Calculate bezier X at t
        f32 mt = 1.0f - t;
        f32 mt2 = mt * mt;
        f32 mt3 = mt2 * mt;
        f32 t2 = t * t;
        f32 t3 = t2 * t;

        // X = (1-t)³*0 + 3(1-t)²t*cp1.x + 3(1-t)t²*cp2.x + t³*1
        f32 x = 3.0f * mt2 * t * cp1.x + 3.0f * mt * t2 * cp2.x + t3;

        if (x < inputPressure) {
            low = t;
        } else {
            high = t;
        }
    }

    // Now calculate Y at the found t
    f32 mt = 1.0f - t;
    f32 mt2 = mt * mt;
    f32 mt3 = mt2 * mt;
    f32 t2 = t * t;
    f32 t3 = t2 * t;

    // Y = (1-t)³*0 + 3(1-t)²t*cp1.y + 3(1-t)t²*cp2.y + t³*1
    f32 y = 3.0f * mt2 * t * cp1.y + 3.0f * mt * t2 * cp2.y + t3;

    return std::max(0.0f, std::min(1.0f, y));
}

#endif
