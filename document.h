#ifndef _H_DOCUMENT_
#define _H_DOCUMENT_

#include "types.h"
#include "primitives.h"
#include "layer.h"
#include "selection.h"
#include "tiled_canvas.h"
#include <vector>
#include <memory>
#include <string>
#include <functional>
#include <unordered_map>

// Forward declarations
class Tool;
class DocumentObserver;

// Tool event data
enum class PointerType {
    Mouse,
    Pen,
    Eraser,
    Touch
};

struct ToolEvent {
    Vec2 position;           // Document coordinates
    f32 pressure = 1.0f;     // 0-1, tablet pressure
    f32 tiltX = 0.0f;        // -1 to 1
    f32 tiltY = 0.0f;
    f32 zoom = 1.0f;         // Current view zoom level
    PointerType pointerType = PointerType::Mouse;
    bool shiftHeld = false;
    bool ctrlHeld = false;
    bool altHeld = false;

    ToolEvent() = default;
    ToolEvent(const Vec2& pos) : position(pos) {}
};

// Observer interface for document changes
class DocumentObserver {
public:
    virtual ~DocumentObserver() = default;
    virtual void onDocumentChanged(const Rect& dirtyRect) {}
    virtual void onLayerAdded(i32 index) {}
    virtual void onLayerRemoved(i32 index) {}
    virtual void onLayerMoved(i32 fromIndex, i32 toIndex) {}
    virtual void onLayerChanged(i32 index) {}
    virtual void onActiveLayerChanged(i32 index) {}
    virtual void onSelectionChanged() {}
};

// Document class - owns all layer data
class Document {
public:
    // Document properties
    std::string name;
    std::string filePath;
    u32 width = 0;
    u32 height = 0;

    // Layers (owned)
    std::vector<std::unique_ptr<LayerBase>> layers;
    i32 activeLayerIndex = -1;

    // Selection
    Selection selection;

    // Embedded fonts (fontName -> font data)
    std::unordered_map<std::string, std::vector<u8>> embeddedFonts;

    // Floating content (for move selection preview)
    struct FloatingContent {
        TiledCanvas* pixels = nullptr;      // Non-owning - MoveTool owns this
        Recti originalBounds;               // Where pixels were cut from
        Vec2 currentOffset;                 // Current offset from original position
        const PixelLayer* sourceLayer = nullptr;  // Layer pixels came from
        bool active = false;

        void clear() {
            pixels = nullptr;
            sourceLayer = nullptr;
            active = false;
            currentOffset = Vec2(0, 0);
        }
    } floatingContent;

    // Current tool (owned)
    std::unique_ptr<Tool> currentTool;

    // Observers (non-owning)
    std::vector<DocumentObserver*> observers;

    Document() = default;
    Document(u32 w, u32 h, const std::string& n = "Untitled");

    // Destructor - clear observers to prevent dangling notifications
    ~Document() {
        observers.clear();
    }

    // Non-copyable
    Document(const Document&) = delete;
    Document& operator=(const Document&) = delete;

    // Layer management
    LayerBase* addLayer(std::unique_ptr<LayerBase> layer, i32 index = -1);
    PixelLayer* addPixelLayer(const std::string& name = "", i32 index = -1);
    TextLayer* addTextLayer(const std::string& text, i32 index = -1);
    AdjustmentLayer* addAdjustmentLayer(AdjustmentType type, i32 index = -1);

    void removeLayer(i32 index);
    void moveLayer(i32 fromIndex, i32 toIndex);
    void duplicateLayer(i32 index);
    void mergeDown(i32 index);
    void mergeVisible();
    void flattenImage();

    LayerBase* getLayer(i32 index);
    const LayerBase* getLayer(i32 index) const;
    LayerBase* getActiveLayer();
    const LayerBase* getActiveLayer() const;
    PixelLayer* getActivePixelLayer();

    void setActiveLayer(i32 index);
    i32 getLayerCount() const { return static_cast<i32>(layers.size()); }

    // Get total memory usage of all layers
    size_t getMemoryUsage() const {
        size_t total = 0;
        for (const auto& layer : layers) {
            if (layer->isPixelLayer()) {
                total += static_cast<const PixelLayer*>(layer.get())->canvas.getMemoryUsage();
            } else if (layer->isTextLayer()) {
                total += static_cast<const TextLayer*>(layer.get())->rasterizedCache.getMemoryUsage();
            }
        }
        return total;
    }

    // Rasterize layer
    void rasterizeLayer(i32 index);
    void rasterizePixelLayerTransform(i32 index);  // Bake transform into pixel data

    // Tool handling
    void setTool(std::unique_ptr<Tool> tool);
    Tool* getTool() { return currentTool.get(); }

    void handleMouseDown(const ToolEvent& e);
    void handleMouseDrag(const ToolEvent& e);
    void handleMouseUp(const ToolEvent& e);
    void handleMouseMove(const ToolEvent& e);
    void handleKeyDown(i32 keyCode);
    void handleKeyUp(i32 keyCode);

    // Selection operations
    void selectAll();
    void deselect();
    void invertSelection();

    // Canvas operations
    void resizeCanvas(u32 newWidth, u32 newHeight, i32 anchorX = 0, i32 anchorY = 0);
    void cropToSelection();

    // Edit operations (operate on active layer and selection)
    void cut();
    void copy();
    void paste();
    void pasteInPlace();
    void deleteSelection();
    void fill(u32 color);

    // Transform operations (canvas-level - resizes canvas)
    void flipHorizontal();
    void flipVertical();
    void rotateLeft();
    void rotateRight();

    // Layer-only transform operations (doesn't resize canvas)
    void rotateLayerLeft();
    void rotateLayerRight();
    void flipLayerHorizontal();
    void flipLayerVertical();

    // Font management
    bool addFont(const std::string& fontName, std::vector<u8> data);
    bool hasFont(const std::string& fontName) const;
    const std::vector<u8>* getFontData(const std::string& fontName) const;
    std::vector<std::string> getFontNames() const;

    // Observer management
    void addObserver(DocumentObserver* observer);
    void removeObserver(DocumentObserver* observer);

    // Notification helpers
    void notifyChanged(const Rect& dirtyRect);
    void notifyLayerAdded(i32 index);
    void notifyLayerRemoved(i32 index);
    void notifyLayerMoved(i32 fromIndex, i32 toIndex);
    void notifyLayerChanged(i32 index);
    void notifyActiveLayerChanged(i32 index);
    void notifySelectionChanged();
};

#endif
