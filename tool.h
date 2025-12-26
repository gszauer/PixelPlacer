#ifndef _H_TOOL_
#define _H_TOOL_

#include "types.h"
#include "primitives.h"
#include "document.h"
#include "app_state.h"
#include "config.h"
#include <string>
#include <cmath>
#include <cstdio>  // Debug logging

// Tool types
enum class ToolType {
    // Selection
    RectangleSelect,
    EllipseSelect,
    FreeSelect,
    PolygonSelect,
    MagicWand,

    // Transform
    Move,
    Crop,

    // Painting
    Brush,
    Eraser,
    Fill,
    Gradient,

    // Retouching
    Clone,
    Heal,
    Smudge,
    Dodge,
    Burn,

    // Other
    ColorPicker,
    Pan,
    Zoom
};

// Base tool class
class Tool {
public:
    ToolType type;
    std::string name;
    std::string tooltip;

    Tool(ToolType t, const std::string& n) : type(t), name(n) {}
    virtual ~Tool() = default;

    // Event handlers
    virtual void onMouseDown(Document& doc, const ToolEvent& e) {}
    virtual void onMouseDrag(Document& doc, const ToolEvent& e) {}
    virtual void onMouseUp(Document& doc, const ToolEvent& e) {}
    virtual void onMouseMove(Document& doc, const ToolEvent& e) {}
    virtual void onKeyDown(Document& doc, i32 keyCode) {}
    virtual void onKeyUp(Document& doc, i32 keyCode) {}

    // Cursor rendering (tool-specific cursor overlay)
    // cursorPos is the cursor position in screen coordinates
    // pan is the screen offset for converting document to screen coords: screenPos = docPos * zoom + pan
    virtual bool hasOverlay() const { return false; }
    virtual void renderOverlay(Framebuffer& fb, const Vec2& cursorPos, f32 zoom, const Vec2& pan, const Recti& clipRect = Recti()) {}

    // Tool options (shown in tool options bar)
    virtual void renderOptions(Widget& container) {}
};

// Pan tool
class PanTool : public Tool {
public:
    Vec2 lastPos;
    bool dragging = false;

    PanTool() : Tool(ToolType::Pan, "Pan") {}

    void onMouseDown(Document& doc, const ToolEvent& e) override {
        lastPos = e.position;
        dragging = true;
    }

    void onMouseDrag(Document& doc, const ToolEvent& e) override {
        if (dragging) {
            // Pan is handled by the view, not the document
            lastPos = e.position;
        }
    }

    void onMouseUp(Document& doc, const ToolEvent& e) override {
        dragging = false;
    }
};

// Zoom tool
class ZoomTool : public Tool {
public:
    ZoomTool() : Tool(ToolType::Zoom, "Zoom") {}

    void onMouseDown(Document& doc, const ToolEvent& e) override {
        // Zoom is handled by the view
        // Left click = zoom in, Alt+click = zoom out
    }
};

// Transform handle types
enum class TransformHandle {
    None,
    Move,           // Clicking inside bounds
    TopLeft,        // Corner handles (rotate or scale based on mode)
    TopRight,
    BottomLeft,
    BottomRight,
    Top,            // Scale handles (edges)
    Bottom,
    Left,
    Right,
    Pivot           // Center pivot point
};

// Corner behavior mode
enum class CornerBehavior {
    Rotate,
    Scale
};

// Move/Transform tool
class MoveTool : public Tool {
public:
    Vec2 startPos;
    Vec2 lastPos;
    Transform originalTransform;
    bool dragging = false;
    bool movingSelection = false;  // True if moving selection, false if moving layer
    bool movingContent = false;    // True if also moving pixels with selection

    // Floating content when moving selection + content
    std::unique_ptr<TiledCanvas> floatingPixels;
    Recti floatingOrigin;  // Original bounds of the floating content

    // Corner behavior (configurable via tool options)
    CornerBehavior cornerBehavior = CornerBehavior::Rotate;

    // Transform tool state
    TransformHandle activeHandle = TransformHandle::None;
    Vec2 corners[4];      // TL, TR, BR, BL in document space after transform
    Vec2 pivotPos;        // Pivot in document space
    Vec2 center;          // Center of bounding box
    f32 startAngle = 0.0f;
    f32 originalRotation = 0.0f;
    Vec2 originalScale;
    Vec2 scaleAnchor;     // Anchor point for scaling (the pivot)

    // Visual sizes (in screen pixels, will be scaled by UI_SCALE)
    static constexpr f32 LINE_THICKNESS = 2.0f;      // * UI_SCALE
    static constexpr f32 CORNER_NOTCH_SIZE = 6.0f;   // * UI_SCALE - bigger corner notches
    static constexpr f32 EDGE_HANDLE_SIZE = 4.0f;    // * UI_SCALE - edge midpoint indicators
    static constexpr f32 EDGE_INTERACT_RADIUS = 4.0f;   // * UI_SCALE - hit detection for edges
    static constexpr f32 CORNER_INTERACT_RADIUS = 10.0f; // * UI_SCALE - larger grab area for corners
    static constexpr f32 PIVOT_INTERACT_RADIUS = 12.0f;  // * UI_SCALE - largest grab area for pivot

    MoveTool() : Tool(ToolType::Move, "Move") {}

    // Cached content bounds for the current layer
    Recti contentBounds;
    bool hasContent = false;

    // Cached canvas dimensions for the current layer
    f32 canvasWidth = 0, canvasHeight = 0;

    // Track which layer we last initialized pivot for
    const LayerBase* lastInitializedLayer = nullptr;

    // Initialize pivot to center of content bounds for pixel layer
    void initializePivotToContentCenter(PixelLayer* layer) {
        if (!layer) return;

        Recti bounds = layer->canvas.getContentBounds();
        if (bounds.w <= 0 || bounds.h <= 0) {
            // No content, use canvas center
            layer->transform.pivot = Vec2(0.5f, 0.5f);
        } else {
            // Set pivot to center of content bounds (normalized to canvas)
            f32 centerX = bounds.x + bounds.w * 0.5f;
            f32 centerY = bounds.y + bounds.h * 0.5f;
            layer->transform.pivot.x = centerX / layer->canvas.width;
            layer->transform.pivot.y = centerY / layer->canvas.height;
        }
    }

    // Initialize pivot to center for text layer
    void initializePivotToContentCenter(TextLayer* layer) {
        if (!layer) return;
        layer->ensureCacheValid();
        // Text layer pivot is always centered
        layer->transform.pivot = Vec2(0.5f, 0.5f);
    }

    void updateCorners(const PixelLayer* layer) {
        if (!layer) {
            hasContent = false;
            return;
        }

        // Get content bounds (pixel-accurate bounding box of non-empty pixels)
        contentBounds = layer->canvas.getContentBounds();
        hasContent = contentBounds.w > 0 && contentBounds.h > 0;

        if (!hasContent) {
            // No content - use full canvas as fallback
            contentBounds = Recti(0, 0, layer->canvas.width, layer->canvas.height);
        }

        f32 canvasW = static_cast<f32>(layer->canvas.width);
        f32 canvasH = static_cast<f32>(layer->canvas.height);

        // Get transform matrix (uses canvas size for pivot calculation)
        Matrix3x2 mat = layer->transform.toMatrix(canvasW, canvasH);

        // Transform the content bounds corners
        f32 x0 = static_cast<f32>(contentBounds.x);
        f32 y0 = static_cast<f32>(contentBounds.y);
        f32 x1 = static_cast<f32>(contentBounds.x + contentBounds.w);
        f32 y1 = static_cast<f32>(contentBounds.y + contentBounds.h);

        corners[0] = mat.transform(Vec2(x0, y0));   // Top-left
        corners[1] = mat.transform(Vec2(x1, y0));   // Top-right
        corners[2] = mat.transform(Vec2(x1, y1));   // Bottom-right
        corners[3] = mat.transform(Vec2(x0, y1));   // Bottom-left

        // Calculate center of content bounds and pivot position
        center = (corners[0] + corners[1] + corners[2] + corners[3]) * 0.25f;

        // Pivot in canvas coordinates
        Vec2 pivotCanvas(layer->transform.pivot.x * canvasW, layer->transform.pivot.y * canvasH);
        pivotPos = mat.transform(pivotCanvas);

        // Cache canvas dimensions
        canvasWidth = canvasW;
        canvasHeight = canvasH;
    }

    void updateCorners(TextLayer* layer) {
        if (!layer) {
            hasContent = false;
            return;
        }

        layer->ensureCacheValid();

        // Text layer content bounds are the rasterized cache bounds
        contentBounds = Recti(0, 0, layer->rasterizedCache.width, layer->rasterizedCache.height);
        hasContent = contentBounds.w > 0 && contentBounds.h > 0;

        if (!hasContent) {
            // No text content
            return;
        }

        f32 cacheW = static_cast<f32>(layer->rasterizedCache.width);
        f32 cacheH = static_cast<f32>(layer->rasterizedCache.height);

        // Get transform matrix (uses cache size for pivot calculation)
        Matrix3x2 mat = layer->transform.toMatrix(cacheW, cacheH);

        // Transform the content bounds corners
        corners[0] = mat.transform(Vec2(0, 0));       // Top-left
        corners[1] = mat.transform(Vec2(cacheW, 0));  // Top-right
        corners[2] = mat.transform(Vec2(cacheW, cacheH)); // Bottom-right
        corners[3] = mat.transform(Vec2(0, cacheH)); // Bottom-left

        // Calculate center of content bounds and pivot position
        center = (corners[0] + corners[1] + corners[2] + corners[3]) * 0.25f;

        // Pivot in cache coordinates
        Vec2 pivotCanvas(layer->transform.pivot.x * cacheW, layer->transform.pivot.y * cacheH);
        pivotPos = mat.transform(pivotCanvas);

        // Cache canvas dimensions
        canvasWidth = cacheW;
        canvasHeight = cacheH;
    }

    Vec2 getEdgeMidpoint(i32 edge) const {
        // 0=top, 1=right, 2=bottom, 3=left
        return (corners[edge] + corners[(edge + 1) % 4]) * 0.5f;
    }

    bool pointInQuad(const Vec2& p) const {
        // Check if point is inside the quadrilateral using cross products
        auto sign = [](const Vec2& p1, const Vec2& p2, const Vec2& p3) {
            return (p1.x - p3.x) * (p2.y - p3.y) - (p2.x - p3.x) * (p1.y - p3.y);
        };

        bool b1 = sign(p, corners[0], corners[1]) < 0.0f;
        bool b2 = sign(p, corners[1], corners[2]) < 0.0f;
        bool b3 = sign(p, corners[2], corners[3]) < 0.0f;
        bool b4 = sign(p, corners[3], corners[0]) < 0.0f;

        return (b1 == b2) && (b2 == b3) && (b3 == b4);
    }

    // Distance from point to line segment
    f32 distanceToEdge(const Vec2& p, const Vec2& a, const Vec2& b) const {
        Vec2 ab = b - a;
        Vec2 ap = p - a;
        f32 t = clamp(ap.dot(ab) / ab.lengthSquared(), 0.0f, 1.0f);
        Vec2 closest = a + ab * t;
        return Vec2::distance(p, closest);
    }

    TransformHandle hitTest(const Vec2& pos, f32 zoom) {
        // Interaction radii in document space (screen pixels / zoom)
        f32 pivotRadius = (PIVOT_INTERACT_RADIUS * Config::uiScale) / zoom;
        f32 cornerRadius = (CORNER_INTERACT_RADIUS * Config::uiScale) / zoom;
        f32 edgeRadius = (EDGE_INTERACT_RADIUS * Config::uiScale) / zoom;

        // Check pivot first (largest grab area)
        if (Vec2::distance(pos, pivotPos) < pivotRadius)
            return TransformHandle::Pivot;

        // Check corner handles - larger grab area, checked before edges
        if (Vec2::distance(pos, corners[0]) < cornerRadius)
            return TransformHandle::TopLeft;
        if (Vec2::distance(pos, corners[1]) < cornerRadius)
            return TransformHandle::TopRight;
        if (Vec2::distance(pos, corners[2]) < cornerRadius)
            return TransformHandle::BottomRight;
        if (Vec2::distance(pos, corners[3]) < cornerRadius)
            return TransformHandle::BottomLeft;

        // Check edge lines (smaller grab area)
        // Edge 0: TL to TR (top)
        if (distanceToEdge(pos, corners[0], corners[1]) < edgeRadius)
            return TransformHandle::Top;
        // Edge 1: TR to BR (right)
        if (distanceToEdge(pos, corners[1], corners[2]) < edgeRadius)
            return TransformHandle::Right;
        // Edge 2: BR to BL (bottom)
        if (distanceToEdge(pos, corners[2], corners[3]) < edgeRadius)
            return TransformHandle::Bottom;
        // Edge 3: BL to TL (left)
        if (distanceToEdge(pos, corners[3], corners[0]) < edgeRadius)
            return TransformHandle::Left;

        // Check if inside bounds (for move)
        if (pointInQuad(pos))
            return TransformHandle::Move;

        return TransformHandle::None;
    }

    void onMouseDown(Document& doc, const ToolEvent& e) override {
        startPos = e.position;
        lastPos = e.position;

        // If there's an active selection, move the selection
        if (doc.selection.hasSelection) {
            movingSelection = true;
            dragging = true;
            activeHandle = TransformHandle::Move;

            // Check if we should also move content
            AppState& state = getAppState();
            PixelLayer* layer = doc.getActivePixelLayer();
            if (state.moveSelectionContent && layer && !layer->locked) {
                movingContent = true;

                // Capture the pixels within selection
                const Recti& bounds = doc.selection.bounds;
                floatingOrigin = bounds;
                floatingPixels = std::make_unique<TiledCanvas>(bounds.w, bounds.h);

                // Layer position offset (from rasterization)
                i32 layerOffsetX = static_cast<i32>(layer->transform.position.x);
                i32 layerOffsetY = static_cast<i32>(layer->transform.position.y);

                // Copy selected pixels and clear originals
                for (i32 y = bounds.y; y < bounds.y + bounds.h; ++y) {
                    for (i32 x = bounds.x; x < bounds.x + bounds.w; ++x) {
                        if (doc.selection.isSelected(x, y)) {
                            // Convert document coords to layer coords
                            i32 layerX = x - layerOffsetX;
                            i32 layerY = y - layerOffsetY;
                            u32 pixel = layer->canvas.getPixel(layerX, layerY);
                            if (pixel & 0xFF) {  // Has alpha
                                floatingPixels->setPixel(x - bounds.x, y - bounds.y, pixel);
                                layer->canvas.setPixel(layerX, layerY, 0);  // Clear original
                            }
                        }
                    }
                }

                // Prune empty tiles from layer after clearing
                layer->canvas.pruneEmptyTiles();

                // Set up floating content for compositor preview
                doc.floatingContent.pixels = floatingPixels.get();
                doc.floatingContent.originalBounds = floatingOrigin;
                doc.floatingContent.currentOffset = Vec2(0, 0);
                doc.floatingContent.sourceLayer = layer;
                doc.floatingContent.active = true;

                doc.notifyChanged(bounds.toRect());
            } else {
                movingContent = false;
            }

            return;
        }

        // Otherwise, transform the layer
        LayerBase* layer = doc.getActiveLayer();
        if (!layer || layer->locked) return;

        // Handle both pixel and text layers
        PixelLayer* pixelLayer = doc.getActivePixelLayer();
        TextLayer* textLayer = layer->isTextLayer() ? static_cast<TextLayer*>(layer) : nullptr;

        if (pixelLayer) {
            // Initialize pivot to content center when first interacting with this layer
            if (pixelLayer != lastInitializedLayer) {
                initializePivotToContentCenter(pixelLayer);
                lastInitializedLayer = pixelLayer;
            }
            updateCorners(pixelLayer);
        } else if (textLayer) {
            // Initialize pivot to content center when first interacting with this layer
            if (textLayer != lastInitializedLayer) {
                initializePivotToContentCenter(textLayer);
                lastInitializedLayer = textLayer;
            }
            updateCorners(textLayer);
        } else {
            return;  // Unsupported layer type
        }

        activeHandle = hitTest(e.position, e.zoom);

        if (activeHandle != TransformHandle::None) {
            originalTransform = layer->transform;
            originalScale = layer->transform.scale;
            originalRotation = layer->transform.rotation;
            dragging = true;

            // Corner handles: behavior depends on cornerBehavior setting
            if (activeHandle == TransformHandle::TopLeft ||
                activeHandle == TransformHandle::TopRight ||
                activeHandle == TransformHandle::BottomLeft ||
                activeHandle == TransformHandle::BottomRight) {
                if (cornerBehavior == CornerBehavior::Rotate) {
                    // Calculate starting angle from pivot for rotation
                    Vec2 diff = e.position - pivotPos;
                    startAngle = std::atan2(diff.y, diff.x);
                }
                // For scale mode, we use the same logic as edge handles
            }

            // Scale anchor is always the pivot (scaling happens around pivot)
            scaleAnchor = pivotPos;
        }
    }

    void onMouseDrag(Document& doc, const ToolEvent& e) override {
        if (!dragging) return;

        if (movingSelection) {
            // Move selection incrementally
            Vec2 delta = e.position - lastPos;
            i32 dx = static_cast<i32>(delta.x);
            i32 dy = static_cast<i32>(delta.y);
            if (dx != 0 || dy != 0) {
                doc.selection.offset(dx, dy);
                lastPos.x += dx;
                lastPos.y += dy;

                // Update floating content offset for preview
                if (movingContent && doc.floatingContent.active) {
                    doc.floatingContent.currentOffset.x += dx;
                    doc.floatingContent.currentOffset.y += dy;
                }

                doc.notifySelectionChanged();
            }
            return;
        }

        LayerBase* layer = doc.getActiveLayer();
        if (!layer) return;

        // Use cached canvas dimensions (works for both pixel and text layers)
        if (canvasWidth <= 0 || canvasHeight <= 0) return;

        f32 w = canvasWidth;
        f32 h = canvasHeight;

        switch (activeHandle) {
            case TransformHandle::Move: {
                // Move layer
                Vec2 delta = e.position - startPos;
                layer->transform.position = originalTransform.position + delta;
                break;
            }

            case TransformHandle::Pivot: {
                // Move pivot point while keeping layer visually stable

                // 1. Save old pivot in canvas coordinates
                Vec2 oldPivotCanvas = Vec2(layer->transform.pivot.x * w,
                                           layer->transform.pivot.y * h);

                // 2. Compute reference point (old pivot) in document space with current transform
                Matrix3x2 oldMat = layer->transform.toMatrix(w, h);
                Vec2 refPointDoc = oldMat.transform(oldPivotCanvas);

                // 3. Compute new pivot in canvas/layer space
                Matrix3x2 invMat = oldMat.inverted();
                Vec2 newPivotCanvas = invMat.transform(e.position);

                // 4. Update pivot (normalized, can be outside 0-1 range for free-floating)
                layer->transform.pivot.x = newPivotCanvas.x / w;
                layer->transform.pivot.y = newPivotCanvas.y / h;

                // 5. Compute where reference point would be with new transform
                Matrix3x2 newMat = layer->transform.toMatrix(w, h);
                Vec2 refPointDocNew = newMat.transform(oldPivotCanvas);

                // 6. Adjust position to keep the reference point in the same document location
                layer->transform.position = layer->transform.position + (refPointDoc - refPointDocNew);
                break;
            }

            case TransformHandle::TopLeft:
            case TransformHandle::TopRight:
            case TransformHandle::BottomLeft:
            case TransformHandle::BottomRight: {
                if (cornerBehavior == CornerBehavior::Rotate) {
                    // ROTATE around pivot
                    Vec2 diff = e.position - pivotPos;
                    f32 currentAngle = std::atan2(diff.y, diff.x);
                    f32 deltaAngle = currentAngle - startAngle;

                    // Snap to 15 degrees if shift held
                    if (e.shiftHeld) {
                        f32 snapAngle = 15.0f * DEG_TO_RAD;
                        f32 newRotation = originalRotation + deltaAngle;
                        newRotation = std::round(newRotation / snapAngle) * snapAngle;
                        layer->transform.rotation = newRotation;
                    } else {
                        layer->transform.rotation = originalRotation + deltaAngle;
                    }
                } else {
                    // SCALE uniformly (both X and Y)
                    Vec2 startDist = startPos - scaleAnchor;
                    Vec2 currentDist = e.position - scaleAnchor;

                    f32 startLen = startDist.length();
                    f32 currentLen = currentDist.length();

                    if (startLen > 0.001f) {
                        f32 scale = currentLen / startLen;
                        layer->transform.scale.x = originalScale.x * scale;
                        layer->transform.scale.y = originalScale.y * scale;
                    }
                }
                break;
            }

            case TransformHandle::Top:
            case TransformHandle::Bottom: {
                // Scale Y only
                Vec2 startDist = startPos - scaleAnchor;
                Vec2 currentDist = e.position - scaleAnchor;

                // Project onto Y axis (rotated)
                f32 cos_r = std::cos(-originalRotation);
                f32 sin_r = std::sin(-originalRotation);
                f32 startY = startDist.x * sin_r + startDist.y * cos_r;
                f32 currentY = currentDist.x * sin_r + currentDist.y * cos_r;

                if (std::abs(startY) > 0.001f) {
                    f32 scaleY = currentY / startY;
                    layer->transform.scale.y = originalScale.y * std::abs(scaleY);
                }
                break;
            }

            case TransformHandle::Left:
            case TransformHandle::Right: {
                // Scale X only
                Vec2 startDist = startPos - scaleAnchor;
                Vec2 currentDist = e.position - scaleAnchor;

                // Project onto X axis (rotated)
                f32 cos_r = std::cos(-originalRotation);
                f32 sin_r = std::sin(-originalRotation);
                f32 startX = startDist.x * cos_r - startDist.y * sin_r;
                f32 currentX = currentDist.x * cos_r - currentDist.y * sin_r;

                if (std::abs(startX) > 0.001f) {
                    f32 scaleX = currentX / startX;
                    layer->transform.scale.x = originalScale.x * std::abs(scaleX);
                }
                break;
            }

            default:
                break;
        }

        doc.notifyChanged(Rect(0, 0, doc.width, doc.height));
    }

    void onMouseUp(Document& doc, const ToolEvent& e) override {
        // If we were moving content, place the floating pixels at new location
        if (movingContent && floatingPixels) {
            PixelLayer* layer = doc.getActivePixelLayer();
            if (layer) {
                // Use actual tracked offset, not clipped selection bounds difference
                // (selection bounds get clipped when moved outside canvas, but we need
                // the true move distance to correctly map source pixels to destinations)
                i32 offsetX = static_cast<i32>(doc.floatingContent.currentOffset.x);
                i32 offsetY = static_cast<i32>(doc.floatingContent.currentOffset.y);

                // Layer position offset (from rasterization)
                i32 layerOffsetX = static_cast<i32>(layer->transform.position.x);
                i32 layerOffsetY = static_cast<i32>(layer->transform.position.y);

                // Place floating pixels at new location
                // Only pixels that land inside canvas AND current selection are placed
                floatingPixels->forEachPixel([&](u32 x, u32 y, u32 pixel) {
                    if (pixel & 0xFF) {  // Has alpha
                        // Calculate where this pixel should go in document space
                        i32 destX = floatingOrigin.x + static_cast<i32>(x) + offsetX;
                        i32 destY = floatingOrigin.y + static_cast<i32>(y) + offsetY;

                        // Only place if destination is within canvas bounds
                        if (destX >= 0 && destX < static_cast<i32>(doc.width) &&
                            destY >= 0 && destY < static_cast<i32>(doc.height) &&
                            doc.selection.isSelected(destX, destY)) {
                            // Convert document coords to layer coords
                            i32 layerX = destX - layerOffsetX;
                            i32 layerY = destY - layerOffsetY;
                            layer->canvas.blendPixel(layerX, layerY, pixel, BlendMode::Normal, 1.0f);
                        }
                    }
                });

                doc.notifyChanged(doc.selection.bounds.toRect());
                    }

            // Clear floating content from both tool and document
            doc.floatingContent.clear();
            floatingPixels.reset();
            movingContent = false;
        }

        // Don't auto-rasterize - keep transform live until Enter is pressed
        // or user switches tools/starts painting

        dragging = false;
        movingSelection = false;
        activeHandle = TransformHandle::None;
    }

    void onKeyDown(Document& doc, i32 keyCode) override {
        PixelLayer* layer = doc.getActivePixelLayer();
        if (!layer) return;

        // Enter (13) or Return - apply transform
        if (keyCode == 13) {
            if (layer->transform.rotation != 0.0f ||
                layer->transform.scale.x != 1.0f ||
                layer->transform.scale.y != 1.0f) {
                doc.rasterizePixelLayerTransform(doc.activeLayerIndex);
                lastInitializedLayer = nullptr;  // Recalculate pivot for new content
                getAppState().needsRedraw = true;
            }
        }
        // Escape (27) - reset transform to identity (keep position)
        else if (keyCode == 27) {
            if (layer->transform.rotation != 0.0f ||
                layer->transform.scale.x != 1.0f ||
                layer->transform.scale.y != 1.0f) {
                layer->transform.rotation = 0.0f;
                layer->transform.scale = Vec2(1.0f, 1.0f);
                layer->transform.pivot = Vec2(0.5f, 0.5f);
                doc.notifyChanged(Rect(0, 0, doc.width, doc.height));
                getAppState().needsRedraw = true;
            }
        }
    }

    bool hasOverlay() const override { return true; }

    void renderOverlay(Framebuffer& fb, const Vec2& cursorPos, f32 zoom, const Vec2& pan, const Recti& clipRect = Recti()) override {
        // Need to get active document to draw transform handles
        Document* doc = getAppState().activeDocument;
        if (!doc || doc->selection.hasSelection) return;  // Don't show handles when selection active

        LayerBase* layer = doc->getActiveLayer();
        if (!layer || layer->locked) return;

        // Handle both pixel and text layers
        PixelLayer* pixelLayer = doc->getActivePixelLayer();
        TextLayer* textLayer = layer->isTextLayer() ? static_cast<TextLayer*>(layer) : nullptr;

        if (pixelLayer) {
            // Initialize pivot to content center when we first see this layer
            if (pixelLayer != lastInitializedLayer) {
                initializePivotToContentCenter(pixelLayer);
                lastInitializedLayer = pixelLayer;
            }
            updateCorners(pixelLayer);
        } else if (textLayer) {
            // Initialize pivot to content center when we first see this layer
            if (textLayer != lastInitializedLayer) {
                initializePivotToContentCenter(textLayer);
                lastInitializedLayer = textLayer;
            }
            updateCorners(textLayer);
        } else {
            return;  // Unsupported layer type
        }

        // Screen-space sizes
        i32 lineThick = static_cast<i32>(LINE_THICKNESS * Config::uiScale);
        i32 cornerSize = static_cast<i32>(CORNER_NOTCH_SIZE * Config::uiScale);
        i32 edgeSize = static_cast<i32>(EDGE_HANDLE_SIZE * Config::uiScale);

        // Helper to convert document to screen coordinates
        auto docToScreen = [&](const Vec2& docPos) -> Vec2i {
            return Vec2i(static_cast<i32>(docPos.x * zoom + pan.x),
                        static_cast<i32>(docPos.y * zoom + pan.y));
        };

        // Helper to check if a point is within the clip rect
        auto isInClip = [&](i32 x, i32 y) -> bool {
            if (clipRect.w <= 0 || clipRect.h <= 0) return true;  // No clipping
            return x >= clipRect.x && x < clipRect.x + clipRect.w &&
                   y >= clipRect.y && y < clipRect.y + clipRect.h;
        };

        // Clipped line drawing using Cohen-Sutherland algorithm
        auto clipLine = [&](i32& x0, i32& y0, i32& x1, i32& y1) -> bool {
            if (clipRect.w <= 0 || clipRect.h <= 0) return true;  // No clipping

            i32 xmin = clipRect.x, xmax = clipRect.x + clipRect.w - 1;
            i32 ymin = clipRect.y, ymax = clipRect.y + clipRect.h - 1;

            auto outcode = [&](i32 x, i32 y) -> i32 {
                i32 code = 0;
                if (x < xmin) code |= 1;
                else if (x > xmax) code |= 2;
                if (y < ymin) code |= 4;
                else if (y > ymax) code |= 8;
                return code;
            };

            i32 code0 = outcode(x0, y0);
            i32 code1 = outcode(x1, y1);

            while (true) {
                if (!(code0 | code1)) return true;  // Both inside
                if (code0 & code1) return false;     // Both outside same region

                i32 code = code0 ? code0 : code1;
                i32 x, y;

                if (code & 8) {
                    x = x0 + (x1 - x0) * (ymax - y0) / (y1 - y0);
                    y = ymax;
                } else if (code & 4) {
                    x = x0 + (x1 - x0) * (ymin - y0) / (y1 - y0);
                    y = ymin;
                } else if (code & 2) {
                    y = y0 + (y1 - y0) * (xmax - x0) / (x1 - x0);
                    x = xmax;
                } else {
                    y = y0 + (y1 - y0) * (xmin - x0) / (x1 - x0);
                    x = xmin;
                }

                if (code == code0) {
                    x0 = x; y0 = y;
                    code0 = outcode(x0, y0);
                } else {
                    x1 = x; y1 = y;
                    code1 = outcode(x1, y1);
                }
            }
        };

        // Clipped drawLine wrapper
        auto drawLineClipped = [&](i32 x0, i32 y0, i32 x1, i32 y1, u32 color) {
            if (clipLine(x0, y0, x1, y1)) {
                fb.drawLine(x0, y0, x1, y1, color);
            }
        };

        // Draw thick line between two document points (clipped)
        auto drawThickLine = [&](const Vec2& a, const Vec2& b, u32 color) {
            Vec2i sa = docToScreen(a);
            Vec2i sb = docToScreen(b);
            // Draw multiple parallel lines to create thickness
            for (i32 t = 0; t < lineThick; ++t) {
                // Offset perpendicular to line direction
                i32 dx = sb.x - sa.x;
                i32 dy = sb.y - sa.y;
                f32 len = std::sqrt(static_cast<f32>(dx * dx + dy * dy));
                if (len < 0.001f) continue;
                f32 perpX = -dy / len;
                f32 perpY = dx / len;
                i32 ox = static_cast<i32>(perpX * (t - lineThick / 2));
                i32 oy = static_cast<i32>(perpY * (t - lineThick / 2));
                drawLineClipped(sa.x + ox, sa.y + oy, sb.x + ox, sb.y + oy, color);
            }
        };

        // Clipped fillRect - intersect with clip rect before filling
        auto fillRectClipped = [&](const Recti& rect, u32 color) {
            if (clipRect.w <= 0 || clipRect.h <= 0) {
                fb.fillRect(rect, color);
                return;
            }
            i32 x0 = std::max(rect.x, clipRect.x);
            i32 y0 = std::max(rect.y, clipRect.y);
            i32 x1 = std::min(rect.x + rect.w, clipRect.x + clipRect.w);
            i32 y1 = std::min(rect.y + rect.h, clipRect.y + clipRect.h);
            if (x0 < x1 && y0 < y1) {
                fb.fillRect(Recti(x0, y0, x1 - x0, y1 - y0), color);
            }
        };

        // Clipped drawRect - draw each edge with clipping
        auto drawRectClipped = [&](const Recti& rect, u32 color, i32 thickness) {
            // Top edge
            drawLineClipped(rect.x, rect.y, rect.x + rect.w - 1, rect.y, color);
            // Bottom edge
            drawLineClipped(rect.x, rect.y + rect.h - 1, rect.x + rect.w - 1, rect.y + rect.h - 1, color);
            // Left edge
            drawLineClipped(rect.x, rect.y, rect.x, rect.y + rect.h - 1, color);
            // Right edge
            drawLineClipped(rect.x + rect.w - 1, rect.y, rect.x + rect.w - 1, rect.y + rect.h - 1, color);
        };

        // Draw corner notch (rotate handle) - bigger square at corners (clipped)
        auto drawCornerNotch = [&](const Vec2& pos) {
            Vec2i sp = docToScreen(pos);
            i32 hs = cornerSize;
            Recti handleRect(sp.x - hs, sp.y - hs, hs * 2, hs * 2);
            fillRectClipped(handleRect, 0xFFFFFFFF);
            drawRectClipped(handleRect, 0x000000FF, lineThick);
        };

        // Draw edge handle (scale indicator) - smaller square at edge midpoints (clipped)
        auto drawEdgeHandle = [&](const Vec2& pos) {
            Vec2i sp = docToScreen(pos);
            i32 hs = edgeSize;
            Recti handleRect(sp.x - hs, sp.y - hs, hs * 2, hs * 2);
            fillRectClipped(handleRect, 0xFFFFFFFF);
            drawRectClipped(handleRect, 0x000000FF, lineThick);
        };

        // Draw crosshair for pivot (thicker lines for visibility, clipped)
        auto drawPivot = [&](const Vec2& pos) {
            Vec2i sp = docToScreen(pos);
            i32 size = static_cast<i32>(10 * Config::uiScale);
            i32 r = static_cast<i32>(5 * Config::uiScale);
            i32 thick = lineThick * 2;  // Twice as thick as other lines

            // Draw thick crosshair lines with black outline and white center
            for (i32 t = 0; t < thick; ++t) {
                i32 off = t - thick / 2;
                // Horizontal line - black
                drawLineClipped(sp.x - size, sp.y + off, sp.x + size, sp.y + off, 0x000000FF);
                // Vertical line - black
                drawLineClipped(sp.x + off, sp.y - size, sp.x + off, sp.y + size, 0x000000FF);
            }
            // White center lines (thinner)
            for (i32 t = 0; t < thick / 2; ++t) {
                i32 off = t - thick / 4;
                drawLineClipped(sp.x - size + 1, sp.y + off, sp.x + size - 1, sp.y + off, 0xFFFFFFFF);
                drawLineClipped(sp.x + off, sp.y - size + 1, sp.x + off, sp.y + size - 1, 0xFFFFFFFF);
            }

            // Draw thicker circle
            for (i32 t = 0; t < thick; ++t) {
                f32 radius = r + (t - thick / 2) * 0.5f;
                for (i32 i = 0; i < 32; ++i) {
                    f32 a1 = static_cast<f32>(i) / 32.0f * TAU;
                    f32 a2 = static_cast<f32>(i + 1) / 32.0f * TAU;
                    i32 x1 = sp.x + static_cast<i32>(std::cos(a1) * radius);
                    i32 y1 = sp.y + static_cast<i32>(std::sin(a1) * radius);
                    i32 x2 = sp.x + static_cast<i32>(std::cos(a2) * radius);
                    i32 y2 = sp.y + static_cast<i32>(std::sin(a2) * radius);
                    drawLineClipped(x1, y1, x2, y2, 0x000000FF);
                }
            }
        };

        // Draw bounding box (blue lines, thick)
        u32 boxColor = 0x4488FFFF;
        drawThickLine(corners[0], corners[1], boxColor);
        drawThickLine(corners[1], corners[2], boxColor);
        drawThickLine(corners[2], corners[3], boxColor);
        drawThickLine(corners[3], corners[0], boxColor);

        // Draw corner handles (ROTATE) - bigger notches
        drawCornerNotch(corners[0]);
        drawCornerNotch(corners[1]);
        drawCornerNotch(corners[2]);
        drawCornerNotch(corners[3]);

        // Draw edge midpoint handles (SCALE) - smaller handles
        drawEdgeHandle(getEdgeMidpoint(0));
        drawEdgeHandle(getEdgeMidpoint(1));
        drawEdgeHandle(getEdgeMidpoint(2));
        drawEdgeHandle(getEdgeMidpoint(3));

        // Draw pivot point
        drawPivot(pivotPos);
    }
};

// Color picker tool
class ColorPickerTool : public Tool {
public:
    ColorPickerTool() : Tool(ToolType::ColorPicker, "Color Picker") {}

    void onMouseDown(Document& doc, const ToolEvent& e) override {
        pickColor(doc, e, false);
    }

    void onMouseDrag(Document& doc, const ToolEvent& e) override {
        pickColor(doc, e, false);
    }

    void onMouseUp(Document& doc, const ToolEvent& e) override {
        pickColor(doc, e, true);
    }

private:
    void pickColor(Document& doc, const ToolEvent& e, bool log);
};

#endif
