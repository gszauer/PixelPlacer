#include "tool.h"
#include "app_state.h"
#include "compositor.h"
#include "blend.h"
#include "sampler.h"
#include "keycodes.h"

void ColorPickerTool::pickColor(Document& doc, const ToolEvent& e, bool) {
    i32 x = static_cast<i32>(e.position.x);
    i32 y = static_cast<i32>(e.position.y);

    if (x < 0 || y < 0 || x >= static_cast<i32>(doc.width) || y >= static_cast<i32>(doc.height)) {
        return;
    }

    AppState& state = getAppState();
    u32 sampledPixel = 0;
    f32 docX = static_cast<f32>(x);
    f32 docY = static_cast<f32>(y);

    // Helper to sample a pixel layer at document coordinates
    auto sampleLayer = [&](const PixelLayer* layer) -> u32 {
        if (!layer) return 0;

        // Check if layer has rotation or scale
        bool hasTransform = layer->transform.rotation != 0.0f ||
                            layer->transform.scale.x != 1.0f ||
                            layer->transform.scale.y != 1.0f;

        f32 layerX, layerY;
        if (hasTransform) {
            // Apply full inverse transform
            Matrix3x2 mat = layer->transform.toMatrix(
                layer->canvas.width, layer->canvas.height);
            Matrix3x2 invMat = mat.inverted();
            Vec2 srcCoord = invMat.transform(Vec2(docX, docY));
            layerX = srcCoord.x;
            layerY = srcCoord.y;
        } else {
            // Position-only offset
            layerX = docX - layer->transform.position.x;
            layerY = docY - layer->transform.position.y;
        }

        i32 ix = static_cast<i32>(std::floor(layerX));
        i32 iy = static_cast<i32>(std::floor(layerY));
        return layer->canvas.getPixel(ix, iy);
    };

    switch (state.colorPickerSampleMode) {
        case 0: {
            // Current Layer only
            PixelLayer* layer = doc.getActivePixelLayer();
            if (layer) {
                sampledPixel = sampleLayer(layer);
            }
            break;
        }

        case 1: {
            // Current Layer and Below
            for (i32 i = 0; i <= doc.activeLayerIndex && i < static_cast<i32>(doc.layers.size()); ++i) {
                const auto& layer = doc.layers[i];
                if (!layer->visible) continue;

                if (layer->isPixelLayer()) {
                    const PixelLayer* pixelLayer = static_cast<const PixelLayer*>(layer.get());
                    u32 layerPixel = sampleLayer(pixelLayer);
                    if ((layerPixel & 0xFF) > 0) {
                        sampledPixel = Blend::blend(sampledPixel, layerPixel, layer->blend, layer->opacity);
                    }
                }
            }
            break;
        }

        case 2:
        default: {
            // All Layers
            for (const auto& layer : doc.layers) {
                if (!layer->visible) continue;

                if (layer->isPixelLayer()) {
                    const PixelLayer* pixelLayer = static_cast<const PixelLayer*>(layer.get());
                    u32 layerPixel = sampleLayer(pixelLayer);
                    if ((layerPixel & 0xFF) > 0) {
                        sampledPixel = Blend::blend(sampledPixel, layerPixel, layer->blend, layer->opacity);
                    }
                }
            }
            break;
        }
    }

    // Only update color if we got a non-transparent pixel
    if ((sampledPixel & 0xFF) > 0) {
        Color color = Color::fromRGBA(sampledPixel);

        if (e.altHeld) {
            state.backgroundColor = color;
        } else {
            state.foregroundColor = color;
        }

        state.needsRedraw = true;
    }
}

// MoveTool implementations

void MoveTool::initializePivotToContentCenter(PixelLayer* layer) {
    if (!layer) return;

    // Get content bounds
    contentBounds = layer->canvas.getContentBounds();
    hasContent = (contentBounds.w > 0 && contentBounds.h > 0);
    canvasWidth = static_cast<f32>(layer->canvas.width);
    canvasHeight = static_cast<f32>(layer->canvas.height);

    if (hasContent) {
        // Set pivot to center of content (normalized 0-1)
        f32 centerX = contentBounds.x + contentBounds.w * 0.5f;
        f32 centerY = contentBounds.y + contentBounds.h * 0.5f;
        layer->transform.pivot.x = centerX / canvasWidth;
        layer->transform.pivot.y = centerY / canvasHeight;
    } else {
        // No content, use canvas center
        layer->transform.pivot = Vec2(0.5f, 0.5f);
    }

    lastInitializedLayer = layer;
}

void MoveTool::initializePivotToContentCenter(TextLayer* layer) {
    if (!layer) return;

    layer->ensureCacheValid();
    contentBounds = layer->rasterizedCache.getContentBounds();
    hasContent = (contentBounds.w > 0 && contentBounds.h > 0);
    canvasWidth = static_cast<f32>(layer->rasterizedCache.width);
    canvasHeight = static_cast<f32>(layer->rasterizedCache.height);

    if (hasContent) {
        f32 centerX = contentBounds.x + contentBounds.w * 0.5f;
        f32 centerY = contentBounds.y + contentBounds.h * 0.5f;
        layer->transform.pivot.x = (canvasWidth > 0) ? centerX / canvasWidth : 0.5f;
        layer->transform.pivot.y = (canvasHeight > 0) ? centerY / canvasHeight : 0.5f;
    } else {
        layer->transform.pivot = Vec2(0.5f, 0.5f);
    }

    lastInitializedLayer = layer;
}

void MoveTool::updateCorners(const PixelLayer* layer) {
    if (!layer) return;

    f32 w = static_cast<f32>(layer->canvas.width);
    f32 h = static_cast<f32>(layer->canvas.height);
    canvasWidth = w;
    canvasHeight = h;

    // Get the transformation matrix
    Matrix3x2 mat = layer->transform.toMatrix(w, h);

    // Transform the four corners of the canvas
    corners[0] = mat.transform(Vec2(0, 0));         // Top-left
    corners[1] = mat.transform(Vec2(w, 0));         // Top-right
    corners[2] = mat.transform(Vec2(w, h));         // Bottom-right
    corners[3] = mat.transform(Vec2(0, h));         // Bottom-left

    // Calculate pivot position in document space
    f32 px = layer->transform.pivot.x * w;
    f32 py = layer->transform.pivot.y * h;
    pivotPos = mat.transform(Vec2(px, py));

    // Center is the average of corners
    center = (corners[0] + corners[1] + corners[2] + corners[3]) * 0.25f;
}

void MoveTool::updateCorners(TextLayer* layer) {
    if (!layer) return;

    layer->ensureCacheValid();
    f32 w = static_cast<f32>(layer->rasterizedCache.width);
    f32 h = static_cast<f32>(layer->rasterizedCache.height);
    canvasWidth = w;
    canvasHeight = h;

    Matrix3x2 mat = layer->transform.toMatrix(w, h);

    corners[0] = mat.transform(Vec2(0, 0));
    corners[1] = mat.transform(Vec2(w, 0));
    corners[2] = mat.transform(Vec2(w, h));
    corners[3] = mat.transform(Vec2(0, h));

    f32 px = layer->transform.pivot.x * w;
    f32 py = layer->transform.pivot.y * h;
    pivotPos = mat.transform(Vec2(px, py));

    center = (corners[0] + corners[1] + corners[2] + corners[3]) * 0.25f;
}

bool MoveTool::pointInQuad(const Vec2& p) const {
    // Use cross product sign to check if point is on same side of all edges
    for (i32 i = 0; i < 4; ++i) {
        Vec2 a = corners[i];
        Vec2 b = corners[(i + 1) % 4];
        Vec2 edge = b - a;
        Vec2 toPoint = p - a;
        if (edge.cross(toPoint) < 0) {
            return false;
        }
    }
    return true;
}

f32 MoveTool::distanceToEdge(const Vec2& p, const Vec2& a, const Vec2& b) const {
    Vec2 ab = b - a;
    Vec2 ap = p - a;
    f32 len2 = ab.lengthSquared();
    if (len2 < 1e-6f) return Vec2::distance(p, a);

    f32 t = clamp(ap.dot(ab) / len2, 0.0f, 1.0f);
    Vec2 closest = a + ab * t;
    return Vec2::distance(p, closest);
}

TransformHandle MoveTool::hitTest(const Vec2& pos, f32 zoom) {
    // Calculate screen-space interaction radii
    f32 cornerRadius = (CORNER_INTERACT_RADIUS * Config::uiScale) / zoom;
    f32 edgeRadius = (EDGE_INTERACT_RADIUS * Config::uiScale) / zoom;
    f32 pivotRadius = (PIVOT_INTERACT_RADIUS * Config::uiScale) / zoom;

    // Check pivot first (highest priority)
    if (Vec2::distance(pos, pivotPos) < pivotRadius) {
        return TransformHandle::Pivot;
    }

    // Check corners
    if (Vec2::distance(pos, corners[0]) < cornerRadius) return TransformHandle::TopLeft;
    if (Vec2::distance(pos, corners[1]) < cornerRadius) return TransformHandle::TopRight;
    if (Vec2::distance(pos, corners[2]) < cornerRadius) return TransformHandle::BottomRight;
    if (Vec2::distance(pos, corners[3]) < cornerRadius) return TransformHandle::BottomLeft;

    // Check edge midpoints
    Vec2 topMid = getEdgeMidpoint(0);
    Vec2 rightMid = getEdgeMidpoint(1);
    Vec2 bottomMid = getEdgeMidpoint(2);
    Vec2 leftMid = getEdgeMidpoint(3);

    if (Vec2::distance(pos, topMid) < edgeRadius) return TransformHandle::Top;
    if (Vec2::distance(pos, rightMid) < edgeRadius) return TransformHandle::Right;
    if (Vec2::distance(pos, bottomMid) < edgeRadius) return TransformHandle::Bottom;
    if (Vec2::distance(pos, leftMid) < edgeRadius) return TransformHandle::Left;

    // Check if inside quad (move operation)
    if (pointInQuad(pos)) {
        return TransformHandle::Move;
    }

    return TransformHandle::None;
}

void MoveTool::onMouseDown(Document& doc, const ToolEvent& e) {
    LayerBase* layer = doc.getActiveLayer();
    if (!layer || layer->locked) return;

    // Initialize pivot if this is a new layer
    if (layer != lastInitializedLayer) {
        if (layer->isPixelLayer()) {
            initializePivotToContentCenter(static_cast<PixelLayer*>(layer));
        } else if (layer->isTextLayer()) {
            initializePivotToContentCenter(static_cast<TextLayer*>(layer));
        }
    }

    // Update corners based on current transform
    if (layer->isPixelLayer()) {
        updateCorners(static_cast<const PixelLayer*>(layer));
    } else if (layer->isTextLayer()) {
        updateCorners(static_cast<TextLayer*>(layer));
    }

    // Hit test to determine what we're interacting with
    activeHandle = hitTest(e.position, e.zoom);

    if (activeHandle != TransformHandle::None) {
        startPos = e.position;
        lastPos = e.position;
        originalTransform = layer->transform;
        originalRotation = layer->transform.rotation;
        originalScale = layer->transform.scale;
        scaleAnchor = pivotPos;
        dragging = true;

        // Calculate starting angle for rotation
        Vec2 toMouse = e.position - pivotPos;
        startAngle = std::atan2(toMouse.y, toMouse.x);
    }

    getAppState().needsRedraw = true;
}

void MoveTool::onMouseDrag(Document& doc, const ToolEvent& e) {
    if (!dragging || activeHandle == TransformHandle::None) return;

    LayerBase* layer = doc.getActiveLayer();
    if (!layer || layer->locked) return;

    Vec2 delta = e.position - startPos;

    switch (activeHandle) {
        case TransformHandle::Move: {
            // Simple translation
            layer->transform.position = originalTransform.position + delta;
            break;
        }

        case TransformHandle::Pivot: {
            // Moving the pivot point
            // Calculate new pivot in normalized coordinates
            f32 w = canvasWidth;
            f32 h = canvasHeight;
            if (w > 0 && h > 0) {
                // Transform mouse position back to layer space
                Matrix3x2 mat = originalTransform.toMatrix(w, h);
                Matrix3x2 invMat = mat.inverted();
                Vec2 localPos = invMat.transform(e.position);

                // Clamp to canvas bounds and normalize
                layer->transform.pivot.x = clamp(localPos.x / w, 0.0f, 1.0f);
                layer->transform.pivot.y = clamp(localPos.y / h, 0.0f, 1.0f);

                // Update pivot position for display
                pivotPos = e.position;
            }
            break;
        }

        case TransformHandle::TopLeft:
        case TransformHandle::TopRight:
        case TransformHandle::BottomLeft:
        case TransformHandle::BottomRight: {
            if (cornerBehavior == CornerBehavior::Rotate) {
                // Rotation around pivot
                Vec2 toMouse = e.position - pivotPos;
                f32 currentAngle = std::atan2(toMouse.y, toMouse.x);
                f32 angleDelta = currentAngle - startAngle;
                layer->transform.rotation = originalRotation + angleDelta;
            } else {
                // Corner scaling
                Vec2 toStart = startPos - scaleAnchor;
                Vec2 toCurrent = e.position - scaleAnchor;
                f32 startDist = toStart.length();
                f32 currentDist = toCurrent.length();

                if (startDist > 1e-6f) {
                    f32 scaleFactor = currentDist / startDist;
                    if (e.shiftHeld) {
                        // Uniform scaling
                        layer->transform.scale = originalScale * scaleFactor;
                    } else {
                        layer->transform.scale = originalScale * scaleFactor;
                    }
                }
            }
            break;
        }

        case TransformHandle::Top:
        case TransformHandle::Bottom: {
            // Vertical edge scaling
            Vec2 toStart = startPos - scaleAnchor;
            Vec2 toCurrent = e.position - scaleAnchor;
            f32 startY = std::abs(toStart.y);
            f32 currentY = std::abs(toCurrent.y);

            if (startY > 1e-6f) {
                f32 scaleFactor = currentY / startY;
                layer->transform.scale.y = originalScale.y * scaleFactor;
                if (e.shiftHeld) {
                    layer->transform.scale.x = originalScale.x * scaleFactor;
                }
            }
            break;
        }

        case TransformHandle::Left:
        case TransformHandle::Right: {
            // Horizontal edge scaling
            Vec2 toStart = startPos - scaleAnchor;
            Vec2 toCurrent = e.position - scaleAnchor;
            f32 startX = std::abs(toStart.x);
            f32 currentX = std::abs(toCurrent.x);

            if (startX > 1e-6f) {
                f32 scaleFactor = currentX / startX;
                layer->transform.scale.x = originalScale.x * scaleFactor;
                if (e.shiftHeld) {
                    layer->transform.scale.y = originalScale.y * scaleFactor;
                }
            }
            break;
        }

        default:
            break;
    }

    // Update corners after transform change
    if (layer->isPixelLayer()) {
        updateCorners(static_cast<const PixelLayer*>(layer));
    } else if (layer->isTextLayer()) {
        updateCorners(static_cast<TextLayer*>(layer));
    }

    lastPos = e.position;
    doc.notifyChanged(Rect(0, 0, doc.width, doc.height));
    getAppState().needsRedraw = true;
}

void MoveTool::onMouseUp(Document& doc, const ToolEvent& e) {
    dragging = false;
    activeHandle = TransformHandle::None;
    getAppState().needsRedraw = true;
}

void MoveTool::onKeyDown(Document& doc, i32 keyCode) {
    LayerBase* layer = doc.getActiveLayer();
    if (!layer || layer->locked) return;

    // Arrow key nudging
    f32 nudge = 1.0f;
    bool moved = false;

    switch (keyCode) {
        case Key::LEFT:
            layer->transform.position.x -= nudge;
            moved = true;
            break;
        case Key::UP:
            layer->transform.position.y -= nudge;
            moved = true;
            break;
        case Key::RIGHT:
            layer->transform.position.x += nudge;
            moved = true;
            break;
        case Key::DOWN:
            layer->transform.position.y += nudge;
            moved = true;
            break;
        case Key::ESCAPE:
            layer->transform = Transform::identity();
            moved = true;
            break;
    }

    if (moved) {
        if (layer->isPixelLayer()) {
            updateCorners(static_cast<const PixelLayer*>(layer));
        } else if (layer->isTextLayer()) {
            updateCorners(static_cast<TextLayer*>(layer));
        }
        doc.notifyChanged(Rect(0, 0, doc.width, doc.height));
        getAppState().needsRedraw = true;
    }
}

void MoveTool::renderOverlay(Framebuffer& fb, const Vec2& cursorPos, f32 zoom, const Vec2& pan, const Recti& clipRect) {
    AppState& state = getAppState();
    Document* doc = state.activeDocument;
    if (!doc) return;

    LayerBase* layer = doc->getActiveLayer();
    if (!layer) return;

    // Update corners
    if (layer->isPixelLayer()) {
        updateCorners(static_cast<const PixelLayer*>(layer));
    } else if (layer->isTextLayer()) {
        updateCorners(static_cast<TextLayer*>(layer));
    } else {
        return; // No overlay for adjustment layers
    }

    // Convert document corners to screen coordinates
    auto docToScreen = [zoom, &pan](const Vec2& docPos) -> Vec2 {
        return Vec2(docPos.x * zoom + pan.x, docPos.y * zoom + pan.y);
    };

    Vec2 screenCorners[4];
    for (i32 i = 0; i < 4; ++i) {
        screenCorners[i] = docToScreen(corners[i]);
    }
    Vec2 screenPivot = docToScreen(pivotPos);

    // Clipping helper
    auto clipLine = [&clipRect](i32& x1, i32& y1, i32& x2, i32& y2) -> bool {
        if (clipRect.w <= 0 || clipRect.h <= 0) return true;
        // Simple bounds check - if both points outside same edge, skip
        if ((x1 < clipRect.x && x2 < clipRect.x) ||
            (x1 >= clipRect.x + clipRect.w && x2 >= clipRect.x + clipRect.w) ||
            (y1 < clipRect.y && y2 < clipRect.y) ||
            (y1 >= clipRect.y + clipRect.h && y2 >= clipRect.y + clipRect.h)) {
            return false;
        }
        return true;
    };

    // Draw transform box edges
    u32 lineColor = 0x2680EBFF; // Blue

    for (i32 i = 0; i < 4; ++i) {
        i32 x1 = static_cast<i32>(screenCorners[i].x);
        i32 y1 = static_cast<i32>(screenCorners[i].y);
        i32 x2 = static_cast<i32>(screenCorners[(i + 1) % 4].x);
        i32 y2 = static_cast<i32>(screenCorners[(i + 1) % 4].y);

        if (clipLine(x1, y1, x2, y2)) {
            fb.drawLine(x1, y1, x2, y2, lineColor);
        }
    }

    // Draw corner handles
    i32 cornerSize = static_cast<i32>(CORNER_NOTCH_SIZE * Config::uiScale);
    u32 handleFill = 0xFFFFFFFF;
    u32 handleBorder = 0x000000FF;

    for (i32 i = 0; i < 4; ++i) {
        i32 cx = static_cast<i32>(screenCorners[i].x);
        i32 cy = static_cast<i32>(screenCorners[i].y);

        Recti handleRect(cx - cornerSize, cy - cornerSize, cornerSize * 2, cornerSize * 2);

        // Check if handle is in clip rect
        if (clipRect.w > 0 && clipRect.h > 0) {
            if (handleRect.x + handleRect.w < clipRect.x ||
                handleRect.x >= clipRect.x + clipRect.w ||
                handleRect.y + handleRect.h < clipRect.y ||
                handleRect.y >= clipRect.y + clipRect.h) {
                continue;
            }
        }

        fb.fillRect(handleRect, handleFill);
        fb.drawRect(handleRect, handleBorder, 1);
    }

    // Draw edge handles (smaller squares at midpoints)
    i32 edgeSize = static_cast<i32>(EDGE_HANDLE_SIZE * Config::uiScale);

    for (i32 i = 0; i < 4; ++i) {
        Vec2 mid = (screenCorners[i] + screenCorners[(i + 1) % 4]) * 0.5f;
        i32 cx = static_cast<i32>(mid.x);
        i32 cy = static_cast<i32>(mid.y);

        Recti handleRect(cx - edgeSize, cy - edgeSize, edgeSize * 2, edgeSize * 2);

        if (clipRect.w > 0 && clipRect.h > 0) {
            if (handleRect.x + handleRect.w < clipRect.x ||
                handleRect.x >= clipRect.x + clipRect.w ||
                handleRect.y + handleRect.h < clipRect.y ||
                handleRect.y >= clipRect.y + clipRect.h) {
                continue;
            }
        }

        fb.fillRect(handleRect, handleFill);
        fb.drawRect(handleRect, handleBorder, 1);
    }

    // Draw pivot point (circle/crosshair)
    i32 pivotSize = static_cast<i32>(6 * Config::uiScale);
    i32 px = static_cast<i32>(screenPivot.x);
    i32 py = static_cast<i32>(screenPivot.y);

    // Check if pivot is in clip rect
    bool drawPivot = true;
    if (clipRect.w > 0 && clipRect.h > 0) {
        if (px - pivotSize >= clipRect.x + clipRect.w ||
            px + pivotSize < clipRect.x ||
            py - pivotSize >= clipRect.y + clipRect.h ||
            py + pivotSize < clipRect.y) {
            drawPivot = false;
        }
    }

    if (drawPivot) {
        // Draw crosshair
        fb.drawLine(px - pivotSize, py, px + pivotSize, py, handleBorder);
        fb.drawLine(px, py - pivotSize, px, py + pivotSize, handleBorder);

        // Draw circle (approximated with lines)
        i32 circleRadius = pivotSize - 2;
        for (i32 j = 0; j < 16; ++j) {
            f32 a1 = (j / 16.0f) * TAU;
            f32 a2 = ((j + 1) / 16.0f) * TAU;
            i32 x1 = px + static_cast<i32>(std::cos(a1) * circleRadius);
            i32 y1 = py + static_cast<i32>(std::sin(a1) * circleRadius);
            i32 x2 = px + static_cast<i32>(std::cos(a2) * circleRadius);
            i32 y2 = py + static_cast<i32>(std::sin(a2) * circleRadius);
            fb.drawLine(x1, y1, x2, y2, lineColor);
        }
    }
}
