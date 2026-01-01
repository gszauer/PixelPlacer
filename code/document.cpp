#include "document.h"
#include "tool.h"
#include "compositor.h"
#include "app_state.h"
#include "sampler.h"
#include "dialogs.h"

Document::Document(u32 w, u32 h, const std::string& n)
    : name(n), width(w), height(h), selection(w, h) {

    // Create initial background layer
    addPixelLayer("Background");
}

LayerBase* Document::addLayer(std::unique_ptr<LayerBase> layer, i32 index) {
    LayerBase* ptr = layer.get();

    if (index < 0 || index >= static_cast<i32>(layers.size())) {
        layers.push_back(std::move(layer));
        index = layers.size() - 1;
    } else {
        layers.insert(layers.begin() + index, std::move(layer));
    }

    if (activeLayerIndex < 0) {
        activeLayerIndex = index;
    }

    notifyLayerAdded(index);
    return ptr;
}

PixelLayer* Document::addPixelLayer(const std::string& layerName, i32 index) {
    auto layer = std::make_unique<PixelLayer>(width, height);
    layer->name = layerName.empty() ? "Layer " + std::to_string(layers.size() + 1) : layerName;
    PixelLayer* ptr = layer.get();
    addLayer(std::move(layer), index);
    return ptr;
}

TextLayer* Document::addTextLayer(const std::string& text, i32 index) {
    auto layer = std::make_unique<TextLayer>();
    layer->name = "Text";
    layer->text = text;
    TextLayer* ptr = layer.get();
    addLayer(std::move(layer), index);
    return ptr;
}

AdjustmentLayer* Document::addAdjustmentLayer(AdjustmentType type, i32 index) {
    auto layer = std::make_unique<AdjustmentLayer>(type);

    // Set name based on type
    switch (type) {
        case AdjustmentType::BrightnessContrast: layer->name = "Brightness/Contrast"; break;
        case AdjustmentType::TemperatureTint: layer->name = "Temperature/Tint"; break;
        case AdjustmentType::HueSaturation: layer->name = "Hue/Saturation"; break;
        case AdjustmentType::Vibrance: layer->name = "Vibrance"; break;
        case AdjustmentType::ColorBalance: layer->name = "Color Balance"; break;
        case AdjustmentType::HighlightsShadows: layer->name = "Highlights/Shadows"; break;
        case AdjustmentType::Exposure: layer->name = "Exposure"; break;
        case AdjustmentType::Levels: layer->name = "Levels"; break;
        case AdjustmentType::Invert: layer->name = "Invert"; break;
        case AdjustmentType::BlackAndWhite: layer->name = "Black & White"; break;
    }

    AdjustmentLayer* ptr = layer.get();
    addLayer(std::move(layer), index);
    return ptr;
}

void Document::removeLayer(i32 index) {
    if (index < 0 || index >= static_cast<i32>(layers.size())) return;
    if (layers.size() <= 1) return; // Keep at least one layer

    layers.erase(layers.begin() + index);

    // Adjust active layer index
    if (activeLayerIndex >= static_cast<i32>(layers.size())) {
        activeLayerIndex = layers.size() - 1;
    }

    notifyLayerRemoved(index);
}

void Document::moveLayer(i32 fromIndex, i32 toIndex) {
    if (fromIndex < 0 || fromIndex >= static_cast<i32>(layers.size())) return;
    if (toIndex < 0 || toIndex >= static_cast<i32>(layers.size())) return;
    if (fromIndex == toIndex) return;

    auto layer = std::move(layers[fromIndex]);
    layers.erase(layers.begin() + fromIndex);
    layers.insert(layers.begin() + toIndex, std::move(layer));

    // Update active layer index if needed
    if (activeLayerIndex == fromIndex) {
        activeLayerIndex = toIndex;
    } else if (fromIndex < activeLayerIndex && toIndex >= activeLayerIndex) {
        activeLayerIndex--;
    } else if (fromIndex > activeLayerIndex && toIndex <= activeLayerIndex) {
        activeLayerIndex++;
    }

    notifyLayerMoved(fromIndex, toIndex);
}

void Document::duplicateLayer(i32 index) {
    if (index < 0 || index >= static_cast<i32>(layers.size())) return;

    auto copy = layers[index]->clone();
    copy->name = layers[index]->name + " Copy";
    addLayer(std::move(copy), index + 1);
}

void Document::mergeDown(i32 index) {
    if (index <= 0 || index >= static_cast<i32>(layers.size())) return;

    LayerBase* upper = layers[index].get();
    LayerBase* lower = layers[index - 1].get();

    // Helper to rasterize a text layer into a pixel layer
    auto rasterizeTextLayer = [this](TextLayer* text) -> std::unique_ptr<PixelLayer> {
        text->ensureCacheValid();
        auto pixel = std::make_unique<PixelLayer>(width, height);
        pixel->name = text->name;
        pixel->transform = text->transform;
        pixel->opacity = text->opacity;
        pixel->blend = text->blend;
        pixel->visible = text->visible;
        pixel->locked = text->locked;

        i32 offsetX = static_cast<i32>(text->transform.position.x);
        i32 offsetY = static_cast<i32>(text->transform.position.y);

        text->rasterizedCache.forEachPixel([&](u32 x, u32 y, u32 pix) {
            if ((pix & 0xFF) > 0) {
                i32 destX = static_cast<i32>(x) + offsetX;
                i32 destY = static_cast<i32>(y) + offsetY;
                if (destX >= 0 && destY >= 0 &&
                    destX < static_cast<i32>(width) && destY < static_cast<i32>(height)) {
                    pixel->canvas.setPixel(destX, destY, pix);
                }
            }
        });
        // Reset transform since pixels are now at absolute positions
        pixel->transform.position = Vec2(0, 0);
        return pixel;
    };

    // Helper to apply adjustment to a pixel layer
    auto applyAdjustmentToLayer = [](PixelLayer* pixel, const AdjustmentLayer* adj) {
        for (auto& [key, tile] : pixel->canvas.tiles) {
            for (u32 py = 0; py < Config::TILE_SIZE; ++py) {
                for (u32 px = 0; px < Config::TILE_SIZE; ++px) {
                    u32 pix = tile->pixels[py * Config::TILE_SIZE + px];
                    if ((pix & 0xFF) > 0) {
                        tile->pixels[py * Config::TILE_SIZE + px] = Compositor::applyAdjustment(pix, *adj);
                    }
                }
            }
        }
    };

    // Case 1: Upper is adjustment layer - apply to lower
    if (upper->isAdjustmentLayer()) {
        const AdjustmentLayer* adj = static_cast<const AdjustmentLayer*>(upper);

        if (lower->isPixelLayer()) {
            applyAdjustmentToLayer(static_cast<PixelLayer*>(lower), adj);
        }
        else if (lower->isTextLayer()) {
            // Rasterize text first, then apply adjustment
            auto rasterized = rasterizeTextLayer(static_cast<TextLayer*>(lower));
            applyAdjustmentToLayer(rasterized.get(), adj);
            layers[index - 1] = std::move(rasterized);
        }
        // Remove adjustment layer
        removeLayer(index);
            return;
    }

    // Case 2: Lower is adjustment layer - can't really merge onto an adjustment
    // Just remove upper and keep adjustment (or skip)
    if (lower->isAdjustmentLayer()) {
        // This doesn't make sense - can't merge content onto an adjustment
        return;
    }

    // Case 3: Upper is text layer
    if (upper->isTextLayer()) {
        TextLayer* upperText = static_cast<TextLayer*>(upper);

        if (lower->isPixelLayer()) {
            // Rasterize text and composite onto pixel layer
            PixelLayer* lowerPixel = static_cast<PixelLayer*>(lower);
            upperText->ensureCacheValid();

            i32 offsetX = static_cast<i32>(upper->transform.position.x);
            i32 offsetY = static_cast<i32>(upper->transform.position.y);

            upperText->rasterizedCache.forEachPixel([&](u32 x, u32 y, u32 pix) {
                if ((pix & 0xFF) > 0) {
                    i32 destX = static_cast<i32>(x) + offsetX;
                    i32 destY = static_cast<i32>(y) + offsetY;
                    if (destX >= 0 && destY >= 0 &&
                        destX < static_cast<i32>(width) && destY < static_cast<i32>(height)) {
                        u32 existing = lowerPixel->canvas.getPixel(destX, destY);
                        u32 blended = Blend::blend(existing, pix, upper->blend, upper->opacity);
                        lowerPixel->canvas.setPixel(destX, destY, blended);
                    }
                }
            });
        }
        else if (lower->isTextLayer()) {
            // Both are text - rasterize lower, then composite upper
            auto rasterized = rasterizeTextLayer(static_cast<TextLayer*>(lower));
            upperText->ensureCacheValid();

            i32 offsetX = static_cast<i32>(upper->transform.position.x);
            i32 offsetY = static_cast<i32>(upper->transform.position.y);

            upperText->rasterizedCache.forEachPixel([&](u32 x, u32 y, u32 pix) {
                if ((pix & 0xFF) > 0) {
                    i32 destX = static_cast<i32>(x) + offsetX;
                    i32 destY = static_cast<i32>(y) + offsetY;
                    if (destX >= 0 && destY >= 0 &&
                        destX < static_cast<i32>(width) && destY < static_cast<i32>(height)) {
                        u32 existing = rasterized->canvas.getPixel(destX, destY);
                        u32 blended = Blend::blend(existing, pix, upper->blend, upper->opacity);
                        rasterized->canvas.setPixel(destX, destY, blended);
                    }
                }
            });

            layers[index - 1] = std::move(rasterized);
        }

        removeLayer(index);
            return;
    }

    // Case 4: Upper is pixel layer
    if (upper->isPixelLayer()) {
        PixelLayer* upperPixel = static_cast<PixelLayer*>(upper);

        if (lower->isPixelLayer()) {
            // Both pixel - original behavior
            PixelLayer* lowerPixel = static_cast<PixelLayer*>(lower);
            Compositor::compositeLayer(lowerPixel->canvas, upperPixel->canvas,
                                       upper->blend, upper->opacity);
        }
        else if (lower->isTextLayer()) {
            // Rasterize text first, then composite pixel onto it
            auto rasterized = rasterizeTextLayer(static_cast<TextLayer*>(lower));
            Compositor::compositeLayer(rasterized->canvas, upperPixel->canvas,
                                       upper->blend, upper->opacity);
            layers[index - 1] = std::move(rasterized);
        }

        removeLayer(index);
            return;
    }
}

void Document::mergeVisible() {
    // Create new layer with all visible content
    auto merged = std::make_unique<PixelLayer>(width, height);
    merged->name = "Merged";

    // Composite all visible layers (bottom to top)
    for (const auto& layer : layers) {
        if (!layer->visible) continue;

        if (layer->isPixelLayer()) {
            PixelLayer* pixel = static_cast<PixelLayer*>(layer.get());
            Compositor::compositeLayer(merged->canvas, pixel->canvas,
                                       layer->blend, layer->opacity);
        }
        else if (layer->isTextLayer()) {
            // Rasterize text layer and composite
            TextLayer* text = static_cast<TextLayer*>(layer.get());
            text->ensureCacheValid();

            // Composite the rasterized text cache onto merged canvas
            // Account for layer position
            i32 offsetX = static_cast<i32>(layer->transform.position.x);
            i32 offsetY = static_cast<i32>(layer->transform.position.y);

            text->rasterizedCache.forEachPixel([&](u32 x, u32 y, u32 pixel) {
                if ((pixel & 0xFF) > 0) {
                    i32 destX = static_cast<i32>(x) + offsetX;
                    i32 destY = static_cast<i32>(y) + offsetY;
                    if (destX >= 0 && destY >= 0 &&
                        destX < static_cast<i32>(width) && destY < static_cast<i32>(height)) {
                        u32 existing = merged->canvas.getPixel(destX, destY);
                        u32 blended = Blend::blend(existing, pixel, layer->blend, layer->opacity);
                        merged->canvas.setPixel(destX, destY, blended);
                    }
                }
            });
        }
        else if (layer->isAdjustmentLayer()) {
            // Apply adjustment to all existing pixels in merged canvas
            const AdjustmentLayer* adj = static_cast<const AdjustmentLayer*>(layer.get());

            // Iterate all tiles and apply adjustment
            for (auto& [key, tile] : merged->canvas.tiles) {
                for (u32 py = 0; py < Config::TILE_SIZE; ++py) {
                    for (u32 px = 0; px < Config::TILE_SIZE; ++px) {
                        u32 pixel = tile->pixels[py * Config::TILE_SIZE + px];
                        if ((pixel & 0xFF) > 0) {
                            tile->pixels[py * Config::TILE_SIZE + px] = Compositor::applyAdjustment(pixel, *adj);
                        }
                    }
                }
            }
        }
    }

    // Remove all layers and add merged
    layers.clear();
    addLayer(std::move(merged));
    activeLayerIndex = 0;
}

void Document::flattenImage() {
    mergeVisible();
}

LayerBase* Document::getLayer(i32 index) {
    if (index < 0 || index >= static_cast<i32>(layers.size())) return nullptr;
    return layers[index].get();
}

const LayerBase* Document::getLayer(i32 index) const {
    if (index < 0 || index >= static_cast<i32>(layers.size())) return nullptr;
    return layers[index].get();
}

LayerBase* Document::getActiveLayer() {
    return getLayer(activeLayerIndex);
}

const LayerBase* Document::getActiveLayer() const {
    return getLayer(activeLayerIndex);
}

PixelLayer* Document::getActivePixelLayer() {
    LayerBase* layer = getActiveLayer();
    if (layer && layer->isPixelLayer()) {
        return static_cast<PixelLayer*>(layer);
    }
    return nullptr;
}

void Document::setActiveLayer(i32 index) {
    if (index < 0 || index >= static_cast<i32>(layers.size())) return;
    if (index == activeLayerIndex) return;

    activeLayerIndex = index;
    notifyActiveLayerChanged(index);
}

void Document::rasterizeLayer(i32 index) {
    LayerBase* layer = getLayer(index);
    if (!layer) return;
    if (layer->locked) return;  // Cannot rasterize locked layer

    // For pixel layers, rasterize any transform
    if (layer->isPixelLayer()) {
        rasterizePixelLayerTransform(index);
        return;
    }

    // Create pixel layer from text/adjustment layer
    auto pixel = std::make_unique<PixelLayer>(width, height);
    pixel->name = layer->name;
    pixel->opacity = layer->opacity;
    pixel->blend = layer->blend;
    pixel->visible = layer->visible;
    pixel->locked = layer->locked;

    if (layer->isTextLayer()) {
        TextLayer* text = static_cast<TextLayer*>(layer);
        text->ensureCacheValid();

        // Copy the rasterized cache to the pixel layer
        // Apply the text layer's transform position
        i32 offsetX = static_cast<i32>(text->transform.position.x);
        i32 offsetY = static_cast<i32>(text->transform.position.y);

        for (const auto& [key, tile] : text->rasterizedCache.tiles) {
            i32 tileX, tileY;
            extractTileCoords(key, tileX, tileY);

            for (u32 py = 0; py < Config::TILE_SIZE; ++py) {
                for (u32 px = 0; px < Config::TILE_SIZE; ++px) {
                    u32 color = tile->pixels[py * Config::TILE_SIZE + px];
                    if ((color & 0xFF) > 0) {  // Has alpha
                        i32 destX = tileX * Config::TILE_SIZE + px + offsetX;
                        i32 destY = tileY * Config::TILE_SIZE + py + offsetY;
                        if (destX >= 0 && destY >= 0) {
                            pixel->canvas.setPixel(destX, destY, color);
                        }
                    }
                }
            }
        }

        // Clear transform since we've baked the position in
        pixel->transform = Transform();
    }

    // Replace layer
    layers[index] = std::move(pixel);
    notifyLayerChanged(index);
}

void Document::rasterizePixelLayerTransform(i32 layerIndex) {
    LayerBase* layer = getLayer(layerIndex);
    if (!layer || !layer->isPixelLayer()) return;
    if (layer->locked) return;  // Cannot rasterize locked layer

    PixelLayer* pixelLayer = static_cast<PixelLayer*>(layer);
    Transform& xform = pixelLayer->transform;

    // Nothing to do if no rotation or scale (position-only is fine)
    if (xform.rotation == 0.0f && xform.scale.x == 1.0f && xform.scale.y == 1.0f) {
        return;
    }

    f32 srcW = static_cast<f32>(pixelLayer->canvas.width);
    f32 srcH = static_cast<f32>(pixelLayer->canvas.height);

    // Skip if canvas is empty
    if (srcW <= 0 || srcH <= 0) return;

    // Get transform matrix
    Matrix3x2 mat = xform.toMatrix(srcW, srcH);

    // Transform the 4 corners to find new bounds
    Vec2 corners[4] = {
        mat.transform(Vec2(0, 0)),
        mat.transform(Vec2(srcW, 0)),
        mat.transform(Vec2(srcW, srcH)),
        mat.transform(Vec2(0, srcH))
    };

    // Find bounding box
    f32 minX = corners[0].x, maxX = corners[0].x;
    f32 minY = corners[0].y, maxY = corners[0].y;
    for (int i = 1; i < 4; i++) {
        minX = std::min(minX, corners[i].x);
        maxX = std::max(maxX, corners[i].x);
        minY = std::min(minY, corners[i].y);
        maxY = std::max(maxY, corners[i].y);
    }

    // New canvas dimensions
    i32 newW = static_cast<i32>(std::ceil(maxX - minX));
    i32 newH = static_cast<i32>(std::ceil(maxY - minY));
    if (newW <= 0 || newH <= 0) return;

    // Reasonable size limits
    newW = std::min(newW, static_cast<i32>(Config::MAX_CANVAS_SIZE));
    newH = std::min(newH, static_cast<i32>(Config::MAX_CANVAS_SIZE));

    // Create new canvas
    TiledCanvas newCanvas(static_cast<u32>(newW), static_cast<u32>(newH));

    // Inverse matrix for sampling
    Matrix3x2 invMat = mat.inverted();

    // Offset for the new canvas origin
    f32 offsetX = minX;
    f32 offsetY = minY;

    // Rasterize: for each destination pixel, sample from source
    for (i32 dy = 0; dy < newH; dy++) {
        for (i32 dx = 0; dx < newW; dx++) {
            // Destination in document space
            f32 docX = static_cast<f32>(dx) + offsetX;
            f32 docY = static_cast<f32>(dy) + offsetY;

            // Transform to source coordinates
            Vec2 srcCoord = invMat.transform(Vec2(docX, docY));

            // Sample with bilinear interpolation
            u32 pixel = Sampler::sample(pixelLayer->canvas, srcCoord.x, srcCoord.y,
                                        SampleMode::Bilinear);

            if (pixel & 0xFF) {  // Has alpha
                newCanvas.setPixel(dx, dy, pixel);
            }
        }
    }

    // Replace canvas
    pixelLayer->canvas = std::move(newCanvas);

    // Reset transform, keeping position as offset
    xform.position.x = offsetX;
    xform.position.y = offsetY;
    xform.rotation = 0.0f;
    xform.scale = Vec2(1.0f, 1.0f);
    xform.pivot = Vec2(0.5f, 0.5f);

    notifyLayerChanged(layerIndex);
}

void Document::setTool(std::unique_ptr<Tool> tool) {
    // Commit any floating content before switching tools
    if (floatingContent.active && floatingContent.pixels) {
        PixelLayer* layer = getActivePixelLayer();
        if (layer) {
            // Calculate final position
            i32 offsetX = static_cast<i32>(std::round(floatingContent.currentOffset.x));
            i32 offsetY = static_cast<i32>(std::round(floatingContent.currentOffset.y));

            // Compute document-to-layer transform
            Matrix3x2 layerToDoc = layer->transform.toMatrix(layer->canvas.width, layer->canvas.height);
            Matrix3x2 docToLayer = layerToDoc.inverted();

            // Paste floating pixels back to layer at new position
            Recti origBounds = floatingContent.originalBounds;
            for (i32 y = 0; y < static_cast<i32>(floatingContent.pixels->height); ++y) {
                for (i32 x = 0; x < static_cast<i32>(floatingContent.pixels->width); ++x) {
                    u32 pixel = floatingContent.pixels->getPixel(x, y);
                    if ((pixel & 0xFF) > 0) {
                        // Document coordinates
                        i32 docX = origBounds.x + x + offsetX;
                        i32 docY = origBounds.y + y + offsetY;

                        // Transform to layer coordinates
                        Vec2 layerCoord = docToLayer.transform(Vec2(static_cast<f32>(docX), static_cast<f32>(docY)));
                        i32 layerX = static_cast<i32>(std::floor(layerCoord.x));
                        i32 layerY = static_cast<i32>(std::floor(layerCoord.y));

                        layer->canvas.setPixel(layerX, layerY, pixel);
                    }
                }
            }

            // Move the selection mask to match
            selection.offset(offsetX, offsetY);
        }
        floatingContent.clear();
    }

    currentTool = std::move(tool);
}

void Document::handleMouseDown(const ToolEvent& e) {
    if (currentTool) {
        currentTool->onMouseDown(*this, e);
    }
}

void Document::handleMouseDrag(const ToolEvent& e) {
    if (currentTool) {
        currentTool->onMouseDrag(*this, e);
    }
}

void Document::handleMouseUp(const ToolEvent& e) {
    if (currentTool) {
        currentTool->onMouseUp(*this, e);
    }
}

void Document::handleMouseMove(const ToolEvent& e) {
    if (currentTool) {
        currentTool->onMouseMove(*this, e);
    }
}

void Document::handleKeyDown(i32 keyCode) {
    if (currentTool) {
        currentTool->onKeyDown(*this, keyCode);
    }
}

void Document::handleKeyUp(i32 keyCode) {
    if (currentTool) {
        currentTool->onKeyUp(*this, keyCode);
    }
}

void Document::selectAll() {
    selection.selectAll();
    notifySelectionChanged();
}

void Document::deselect() {
    selection.clear();
    notifySelectionChanged();
}

void Document::invertSelection() {
    selection.invert();
    notifySelectionChanged();
}

void Document::resizeCanvas(u32 newWidth, u32 newHeight, i32 anchorX, i32 anchorY, CanvasResizeMode mode) {
    // Handle scaling modes
    if (mode == CanvasResizeMode::ScaleBilinear || mode == CanvasResizeMode::ScaleNearest) {
        f32 scaleX = static_cast<f32>(newWidth) / static_cast<f32>(width);
        f32 scaleY = static_cast<f32>(newHeight) / static_cast<f32>(height);

        for (auto& layer : layers) {
            if (layer->isPixelLayer()) {
                PixelLayer* pixelLayer = static_cast<PixelLayer*>(layer.get());
                TiledCanvas newCanvas(newWidth, newHeight);

                // Scale using sampler
                for (u32 y = 0; y < newHeight; ++y) {
                    for (u32 x = 0; x < newWidth; ++x) {
                        // Map destination to source coordinates
                        f32 srcX = (static_cast<f32>(x) + 0.5f) / scaleX - 0.5f;
                        f32 srcY = (static_cast<f32>(y) + 0.5f) / scaleY - 0.5f;

                        u32 pixel;
                        if (mode == CanvasResizeMode::ScaleBilinear) {
                            pixel = Sampler::sampleBilinear(pixelLayer->canvas, srcX, srcY);
                        } else {
                            pixel = Sampler::sampleNearest(pixelLayer->canvas, srcX, srcY);
                        }

                        if (pixel != 0) {
                            newCanvas.setPixel(x, y, pixel);
                        }
                    }
                }

                pixelLayer->canvas = std::move(newCanvas);
            }
            else if (layer->isTextLayer()) {
                // Scale text layer transform (position and scale), not font size
                layer->transform.position.x *= scaleX;
                layer->transform.position.y *= scaleY;
                layer->transform.scale.x *= scaleX;
                layer->transform.scale.y *= scaleY;
            }
            // Adjustment layers don't need any changes
        }

        width = newWidth;
        height = newHeight;
        selection.resize(newWidth, newHeight);

        notifyChanged(Rect(0, 0, newWidth, newHeight));
        return;
    }

    // Crop mode (original behavior)
    // anchorX/Y: -1 = left/top, 0 = center, 1 = right/bottom
    // Calculate offset for existing content
    i32 offsetX = 0;
    i32 offsetY = 0;

    if (anchorX == 0) {
        offsetX = (static_cast<i32>(newWidth) - static_cast<i32>(width)) / 2;
    } else if (anchorX == 1) {
        offsetX = static_cast<i32>(newWidth) - static_cast<i32>(width);
    }

    if (anchorY == 0) {
        offsetY = (static_cast<i32>(newHeight) - static_cast<i32>(height)) / 2;
    } else if (anchorY == 1) {
        offsetY = static_cast<i32>(newHeight) - static_cast<i32>(height);
    }

    // Resize each layer with offset
    for (auto& layer : layers) {
        if (layer->isPixelLayer()) {
            PixelLayer* pixelLayer = static_cast<PixelLayer*>(layer.get());
            TiledCanvas newCanvas(newWidth, newHeight);

            // Copy pixels with offset
            pixelLayer->canvas.forEachPixel([&](u32 x, u32 y, u32 pixel) {
                i32 newX = static_cast<i32>(x) + offsetX;
                i32 newY = static_cast<i32>(y) + offsetY;
                if (newX >= 0 && newX < static_cast<i32>(newWidth) &&
                    newY >= 0 && newY < static_cast<i32>(newHeight)) {
                    newCanvas.setPixel(newX, newY, pixel);
                }
            });

            pixelLayer->canvas = std::move(newCanvas);
        }
        else if (layer->isTextLayer()) {
            // Adjust text layer position
            layer->transform.position.x += offsetX;
            layer->transform.position.y += offsetY;
        }
    }

    width = newWidth;
    height = newHeight;
    selection.resize(newWidth, newHeight);

    notifyChanged(Rect(0, 0, newWidth, newHeight));
}

void Document::cropToSelection() {
    if (!selection.hasSelection) return;

    Recti bounds = selection.bounds;
    resizeCanvas(bounds.w, bounds.h, -bounds.x, -bounds.y);
}

void Document::cut() {
    copy();
    deleteSelection();
}

void Document::copy() {
    PixelLayer* layer = getActivePixelLayer();
    if (!layer) return;

    AppState& state = getAppState();
    Clipboard& clipboard = state.clipboard;

    if (selection.hasSelection) {
        // Copy selected region
        clipboard.width = selection.bounds.w;
        clipboard.height = selection.bounds.h;
        clipboard.originX = selection.bounds.x;
        clipboard.originY = selection.bounds.y;
        clipboard.pixels = std::make_unique<TiledCanvas>(clipboard.width, clipboard.height);

        // Compute document-to-layer transform for transformed layers
        Matrix3x2 layerToDoc = layer->transform.toMatrix(layer->canvas.width, layer->canvas.height);
        Matrix3x2 docToLayer = layerToDoc.inverted();

        for (i32 y = 0; y < selection.bounds.h; ++y) {
            for (i32 x = 0; x < selection.bounds.w; ++x) {
                i32 docX = selection.bounds.x + x;
                i32 docY = selection.bounds.y + y;

                if (selection.isSelected(docX, docY)) {
                    // Transform document coords to layer coords
                    Vec2 layerCoord = docToLayer.transform(Vec2(static_cast<f32>(docX), static_cast<f32>(docY)));
                    i32 layerX = static_cast<i32>(std::floor(layerCoord.x));
                    i32 layerY = static_cast<i32>(std::floor(layerCoord.y));

                    u32 pixel = layer->canvas.getPixel(layerX, layerY);
                    clipboard.pixels->setPixel(x, y, pixel);
                }
            }
        }
    } else {
        // Copy entire layer
        clipboard.width = layer->canvas.width;
        clipboard.height = layer->canvas.height;
        clipboard.pixels = std::make_unique<TiledCanvas>(clipboard.width, clipboard.height);

        layer->canvas.forEachPixel([&](u32 x, u32 y, u32 pixel) {
            clipboard.pixels->setPixel(x, y, pixel);
        });
    }
}

void Document::paste() {
    AppState& state = getAppState();
    Clipboard& clipboard = state.clipboard;

    if (!clipboard.hasContent()) return;

    // Create new layer with clipboard contents
    auto newLayer = std::make_unique<PixelLayer>(width, height);
    newLayer->name = "Pasted";

    // Center the pasted content
    i32 offsetX = (static_cast<i32>(width) - static_cast<i32>(clipboard.width)) / 2;
    i32 offsetY = (static_cast<i32>(height) - static_cast<i32>(clipboard.height)) / 2;

    // Copy clipboard pixels to new layer
    clipboard.pixels->forEachPixel([&](u32 x, u32 y, u32 pixel) {
        i32 destX = static_cast<i32>(x) + offsetX;
        i32 destY = static_cast<i32>(y) + offsetY;

        if (destX >= 0 && destX < static_cast<i32>(width) &&
            destY >= 0 && destY < static_cast<i32>(height)) {
            newLayer->canvas.setPixel(destX, destY, pixel);
        }
    });

    // Add layer above active layer and make it active
    i32 newIndex = activeLayerIndex + 1;
    addLayer(std::move(newLayer), newIndex);
    activeLayerIndex = newIndex;

    // Create selection around pasted content
    Recti pasteRect(
        std::max(0, offsetX),
        std::max(0, offsetY),
        std::min(static_cast<i32>(clipboard.width), static_cast<i32>(width) - offsetX),
        std::min(static_cast<i32>(clipboard.height), static_cast<i32>(height) - offsetY)
    );
    selection.setRectangle(pasteRect);

    notifySelectionChanged();
    notifyChanged(Rect(0, 0, width, height));
}

void Document::pasteInPlace() {
    AppState& state = getAppState();
    Clipboard& clipboard = state.clipboard;

    if (!clipboard.hasContent()) return;

    // Create new layer with clipboard contents
    auto newLayer = std::make_unique<PixelLayer>(width, height);
    newLayer->name = "Pasted";

    // Paste at original position
    clipboard.pixels->forEachPixel([&](u32 x, u32 y, u32 pixel) {
        i32 destX = clipboard.originX + static_cast<i32>(x);
        i32 destY = clipboard.originY + static_cast<i32>(y);

        if (destX >= 0 && destX < static_cast<i32>(width) &&
            destY >= 0 && destY < static_cast<i32>(height)) {
            newLayer->canvas.setPixel(destX, destY, pixel);
        }
    });

    // Add layer above active layer and make it active
    i32 newIndex = activeLayerIndex + 1;
    addLayer(std::move(newLayer), newIndex);
    activeLayerIndex = newIndex;

    // Create selection around pasted content at original position
    Recti pasteRect(
        std::max(0, clipboard.originX),
        std::max(0, clipboard.originY),
        std::min(static_cast<i32>(clipboard.width), static_cast<i32>(width) - clipboard.originX),
        std::min(static_cast<i32>(clipboard.height), static_cast<i32>(height) - clipboard.originY)
    );
    selection.setRectangle(pasteRect);

    notifySelectionChanged();
    notifyChanged(Rect(0, 0, width, height));
}

void Document::deleteSelection() {
    PixelLayer* layer = getActivePixelLayer();
    if (!layer || !selection.hasSelection) return;

    // Compute document-to-layer transform for transformed layers
    Matrix3x2 layerToDoc = layer->transform.toMatrix(layer->canvas.width, layer->canvas.height);
    Matrix3x2 docToLayer = layerToDoc.inverted();

    // Clear pixels in selection
    for (i32 y = selection.bounds.y; y < selection.bounds.y + selection.bounds.h; ++y) {
        for (i32 x = selection.bounds.x; x < selection.bounds.x + selection.bounds.w; ++x) {
            if (selection.isSelected(x, y)) {
                // Transform document coords to layer coords
                Vec2 layerCoord = docToLayer.transform(Vec2(static_cast<f32>(x), static_cast<f32>(y)));
                i32 layerX = static_cast<i32>(std::floor(layerCoord.x));
                i32 layerY = static_cast<i32>(std::floor(layerCoord.y));

                layer->canvas.setPixel(layerX, layerY, 0);
            }
        }
    }

    layer->canvas.pruneEmptyTiles();
    notifyChanged(selection.bounds.toRect());
}

void Document::fill(u32 color) {
    PixelLayer* layer = getActivePixelLayer();
    if (!layer) return;

    if (selection.hasSelection) {
        // Compute document-to-layer transform for transformed layers
        Matrix3x2 layerToDoc = layer->transform.toMatrix(layer->canvas.width, layer->canvas.height);
        Matrix3x2 docToLayer = layerToDoc.inverted();

        // Fill only selection
        for (i32 y = selection.bounds.y; y < selection.bounds.y + selection.bounds.h; ++y) {
            for (i32 x = selection.bounds.x; x < selection.bounds.x + selection.bounds.w; ++x) {
                u8 selValue = selection.getValue(x, y);
                if (selValue > 0) {
                    // Apply selection as opacity
                    u32 adjustedColor = color;
                    if (selValue < 255) {
                        u8 alpha = (color & 0xFF) * selValue / 255;
                        adjustedColor = (color & 0xFFFFFF00) | alpha;
                    }

                    // Transform document coords to layer coords
                    Vec2 layerCoord = docToLayer.transform(Vec2(static_cast<f32>(x), static_cast<f32>(y)));
                    i32 layerX = static_cast<i32>(std::floor(layerCoord.x));
                    i32 layerY = static_cast<i32>(std::floor(layerCoord.y));

                    layer->canvas.blendPixel(layerX, layerY, adjustedColor);
                }
            }
        }
        notifyChanged(selection.bounds.toRect());
    } else {
        // Fill entire layer
        layer->canvas.fill(color);
        notifyChanged(Rect(0, 0, width, height));
    }

}

void Document::flipHorizontal() {
    for (auto& layer : layers) {
        if (layer->isPixelLayer()) {
            PixelLayer* pixelLayer = static_cast<PixelLayer*>(layer.get());
            TiledCanvas flipped(pixelLayer->canvas.width, pixelLayer->canvas.height);

            pixelLayer->canvas.forEachPixel([&](u32 x, u32 y, u32 pixel) {
                flipped.setPixel(pixelLayer->canvas.width - 1 - x, y, pixel);
            });

            pixelLayer->canvas = std::move(flipped);
        }
        else if (layer->isTextLayer()) {
            // Flip text layer position and apply horizontal flip via scale
            TextLayer* textLayer = static_cast<TextLayer*>(layer.get());
            textLayer->ensureCacheValid();

            f32 cw = static_cast<f32>(textLayer->rasterizedCache.width);
            f32 ch = static_cast<f32>(textLayer->rasterizedCache.height);

            // Find visual center
            f32 oldCenterX = layer->transform.position.x + cw / 2.0f;
            f32 oldCenterY = layer->transform.position.y + ch / 2.0f;

            // Transform center (horizontal flip)
            f32 newCenterX = static_cast<f32>(width) - oldCenterX;
            f32 newCenterY = oldCenterY;

            // Set new position from center
            layer->transform.position.x = newCenterX - cw / 2.0f;
            layer->transform.position.y = newCenterY - ch / 2.0f;
            layer->transform.scale.x *= -1.0f;
            textLayer->invalidateCache();
        }
    }
    notifyChanged(Rect(0, 0, width, height));
}

void Document::flipVertical() {
    for (auto& layer : layers) {
        if (layer->isPixelLayer()) {
            PixelLayer* pixelLayer = static_cast<PixelLayer*>(layer.get());
            TiledCanvas flipped(pixelLayer->canvas.width, pixelLayer->canvas.height);

            pixelLayer->canvas.forEachPixel([&](u32 x, u32 y, u32 pixel) {
                flipped.setPixel(x, pixelLayer->canvas.height - 1 - y, pixel);
            });

            pixelLayer->canvas = std::move(flipped);
        }
        else if (layer->isTextLayer()) {
            // Flip text layer position and apply vertical flip via scale
            TextLayer* textLayer = static_cast<TextLayer*>(layer.get());
            textLayer->ensureCacheValid();

            f32 cw = static_cast<f32>(textLayer->rasterizedCache.width);
            f32 ch = static_cast<f32>(textLayer->rasterizedCache.height);

            // Find visual center
            f32 oldCenterX = layer->transform.position.x + cw / 2.0f;
            f32 oldCenterY = layer->transform.position.y + ch / 2.0f;

            // Transform center (vertical flip)
            f32 newCenterX = oldCenterX;
            f32 newCenterY = static_cast<f32>(height) - oldCenterY;

            // Set new position from center
            layer->transform.position.x = newCenterX - cw / 2.0f;
            layer->transform.position.y = newCenterY - ch / 2.0f;
            layer->transform.scale.y *= -1.0f;
            textLayer->invalidateCache();
        }
    }
    notifyChanged(Rect(0, 0, width, height));
}

void Document::rotateLeft() {
    // Rotate 90 degrees counter-clockwise
    // New dimensions: width becomes height, height becomes width
    u32 newWidth = height;
    u32 newHeight = width;

    for (auto& layer : layers) {
        if (layer->isPixelLayer()) {
            PixelLayer* pixelLayer = static_cast<PixelLayer*>(layer.get());
            TiledCanvas rotated(newWidth, newHeight);

            pixelLayer->canvas.forEachPixel([&](u32 x, u32 y, u32 pixel) {
                // Rotate left: (x, y) -> (y, width - 1 - x)
                u32 newX = y;
                u32 newY = width - 1 - x;
                rotated.setPixel(newX, newY, pixel);
            });

            pixelLayer->canvas = std::move(rotated);
        }
        else if (layer->isTextLayer()) {
            // Rotate text layer position and orientation
            TextLayer* textLayer = static_cast<TextLayer*>(layer.get());
            textLayer->ensureCacheValid();

            f32 cw = static_cast<f32>(textLayer->rasterizedCache.width);
            f32 ch = static_cast<f32>(textLayer->rasterizedCache.height);

            // Find visual center in old canvas (pivot is at center)
            f32 oldCenterX = layer->transform.position.x + cw / 2.0f;
            f32 oldCenterY = layer->transform.position.y + ch / 2.0f;

            // Transform center to new canvas (CCW 90°)
            f32 newCenterX = oldCenterY;
            f32 newCenterY = static_cast<f32>(width) - oldCenterX;

            // Set new position from center
            layer->transform.position.x = newCenterX - cw / 2.0f;
            layer->transform.position.y = newCenterY - ch / 2.0f;
            layer->transform.rotation -= 3.14159265f / 2.0f;
            textLayer->invalidateCache();
        }
    }

    std::swap(width, height);
    selection.resize(width, height);
    selection.clear();

    notifyChanged(Rect(0, 0, width, height));
}

void Document::rotateRight() {
    // Rotate 90 degrees clockwise
    // New dimensions: width becomes height, height becomes width
    u32 newWidth = height;
    u32 newHeight = width;

    for (auto& layer : layers) {
        if (layer->isPixelLayer()) {
            PixelLayer* pixelLayer = static_cast<PixelLayer*>(layer.get());
            TiledCanvas rotated(newWidth, newHeight);

            pixelLayer->canvas.forEachPixel([&](u32 x, u32 y, u32 pixel) {
                // Rotate right: (x, y) -> (height - 1 - y, x)
                u32 newX = height - 1 - y;
                u32 newY = x;
                rotated.setPixel(newX, newY, pixel);
            });

            pixelLayer->canvas = std::move(rotated);
        }
        else if (layer->isTextLayer()) {
            // Rotate text layer position and orientation
            TextLayer* textLayer = static_cast<TextLayer*>(layer.get());
            textLayer->ensureCacheValid();

            f32 cw = static_cast<f32>(textLayer->rasterizedCache.width);
            f32 ch = static_cast<f32>(textLayer->rasterizedCache.height);

            // Find visual center in old canvas (pivot is at center)
            f32 oldCenterX = layer->transform.position.x + cw / 2.0f;
            f32 oldCenterY = layer->transform.position.y + ch / 2.0f;

            // Transform center to new canvas (CW 90°)
            f32 newCenterX = static_cast<f32>(height) - oldCenterY;
            f32 newCenterY = oldCenterX;

            // Set new position from center
            layer->transform.position.x = newCenterX - cw / 2.0f;
            layer->transform.position.y = newCenterY - ch / 2.0f;
            layer->transform.rotation += 3.14159265f / 2.0f;
            textLayer->invalidateCache();
        }
    }

    std::swap(width, height);
    selection.resize(width, height);
    selection.clear();

    notifyChanged(Rect(0, 0, width, height));
}

void Document::rotateLayerLeft() {
    LayerBase* baseLayer = getActiveLayer();
    if (!baseLayer) return;

    // Handle text layers via transform rotation
    if (baseLayer->isTextLayer()) {
        baseLayer->transform.rotation -= 3.14159265f / 2.0f;  // -90 degrees
        notifyLayerChanged(activeLayerIndex);
        notifyChanged(Rect(0, 0, width, height));
            return;
    }

    PixelLayer* layer = dynamic_cast<PixelLayer*>(baseLayer);
    if (!layer) return;

    // Get content bounds to find center of actual content
    Recti contentBounds = layer->canvas.getContentBounds();
    if (contentBounds.w <= 0 || contentBounds.h <= 0) return;  // No content

    // Calculate content center in document coordinates
    f32 contentCenterX = layer->transform.position.x + contentBounds.x + contentBounds.w * 0.5f;
    f32 contentCenterY = layer->transform.position.y + contentBounds.y + contentBounds.h * 0.5f;

    // Rotate layer 90 degrees counter-clockwise
    u32 oldW = layer->canvas.width;
    u32 oldH = layer->canvas.height;
    u32 newW = oldH;
    u32 newH = oldW;

    TiledCanvas rotated(newW, newH);

    layer->canvas.forEachPixel([&](u32 x, u32 y, u32 pixel) {
        // Rotate left: (x, y) -> (y, oldW - 1 - x)
        u32 newX = y;
        u32 newY = oldW - 1 - x;
        rotated.setPixel(newX, newY, pixel);
    });

    layer->canvas = std::move(rotated);

    // Calculate new content bounds after rotation
    // Old content at (contentBounds.x, contentBounds.y) with size (contentBounds.w, contentBounds.h)
    // After rotate left: new position is (contentBounds.y, oldW - contentBounds.x - contentBounds.w)
    // New size is (contentBounds.h, contentBounds.w)
    i32 newContentX = contentBounds.y;
    i32 newContentY = static_cast<i32>(oldW) - contentBounds.x - contentBounds.w;
    f32 newContentW = static_cast<f32>(contentBounds.h);
    f32 newContentH = static_cast<f32>(contentBounds.w);

    // Adjust layer position so content center stays at same document position
    layer->transform.position.x = contentCenterX - newContentX - newContentW * 0.5f;
    layer->transform.position.y = contentCenterY - newContentY - newContentH * 0.5f;

    notifyLayerChanged(activeLayerIndex);
    notifyChanged(Rect(0, 0, width, height));
}

void Document::rotateLayerRight() {
    LayerBase* baseLayer = getActiveLayer();
    if (!baseLayer) return;

    // Handle text layers via transform rotation
    if (baseLayer->isTextLayer()) {
        baseLayer->transform.rotation += 3.14159265f / 2.0f;  // +90 degrees
        notifyLayerChanged(activeLayerIndex);
        notifyChanged(Rect(0, 0, width, height));
            return;
    }

    PixelLayer* layer = dynamic_cast<PixelLayer*>(baseLayer);
    if (!layer) return;

    // Get content bounds to find center of actual content
    Recti contentBounds = layer->canvas.getContentBounds();
    if (contentBounds.w <= 0 || contentBounds.h <= 0) return;  // No content

    // Calculate content center in document coordinates
    f32 contentCenterX = layer->transform.position.x + contentBounds.x + contentBounds.w * 0.5f;
    f32 contentCenterY = layer->transform.position.y + contentBounds.y + contentBounds.h * 0.5f;

    // Rotate layer 90 degrees clockwise
    u32 oldW = layer->canvas.width;
    u32 oldH = layer->canvas.height;
    u32 newW = oldH;
    u32 newH = oldW;

    TiledCanvas rotated(newW, newH);

    layer->canvas.forEachPixel([&](u32 x, u32 y, u32 pixel) {
        // Rotate right: (x, y) -> (oldH - 1 - y, x)
        u32 newX = oldH - 1 - y;
        u32 newY = x;
        rotated.setPixel(newX, newY, pixel);
    });

    layer->canvas = std::move(rotated);

    // Calculate new content bounds after rotation
    // Old content at (contentBounds.x, contentBounds.y) with size (contentBounds.w, contentBounds.h)
    // After rotate right: new position is (oldH - contentBounds.y - contentBounds.h, contentBounds.x)
    // New size is (contentBounds.h, contentBounds.w)
    i32 newContentX = static_cast<i32>(oldH) - contentBounds.y - contentBounds.h;
    i32 newContentY = contentBounds.x;
    f32 newContentW = static_cast<f32>(contentBounds.h);
    f32 newContentH = static_cast<f32>(contentBounds.w);

    // Adjust layer position so content center stays at same document position
    layer->transform.position.x = contentCenterX - newContentX - newContentW * 0.5f;
    layer->transform.position.y = contentCenterY - newContentY - newContentH * 0.5f;

    notifyLayerChanged(activeLayerIndex);
    notifyChanged(Rect(0, 0, width, height));
}

void Document::flipLayerHorizontal() {
    LayerBase* baseLayer = getActiveLayer();
    if (!baseLayer) return;

    // Handle text layers via transform scale
    if (baseLayer->isTextLayer()) {
        baseLayer->transform.scale.x *= -1.0f;
        notifyLayerChanged(activeLayerIndex);
        notifyChanged(Rect(0, 0, width, height));
            return;
    }

    PixelLayer* layer = dynamic_cast<PixelLayer*>(baseLayer);
    if (!layer) return;

    // Get content bounds to find center of actual content
    Recti contentBounds = layer->canvas.getContentBounds();
    if (contentBounds.w <= 0 || contentBounds.h <= 0) return;  // No content

    // Calculate content center in document coordinates
    f32 contentCenterX = layer->transform.position.x + contentBounds.x + contentBounds.w * 0.5f;

    u32 canvasW = layer->canvas.width;
    TiledCanvas flipped(canvasW, layer->canvas.height);

    layer->canvas.forEachPixel([&](u32 x, u32 y, u32 pixel) {
        flipped.setPixel(canvasW - 1 - x, y, pixel);
    });

    layer->canvas = std::move(flipped);

    // After horizontal flip, content x position changes
    // New content x = canvasW - contentBounds.x - contentBounds.w
    i32 newContentX = static_cast<i32>(canvasW) - contentBounds.x - contentBounds.w;

    // Adjust layer position so content center stays at same document position
    layer->transform.position.x = contentCenterX - newContentX - contentBounds.w * 0.5f;

    notifyLayerChanged(activeLayerIndex);
    notifyChanged(Rect(0, 0, width, height));
}

void Document::flipLayerVertical() {
    LayerBase* baseLayer = getActiveLayer();
    if (!baseLayer) return;

    // Handle text layers via transform scale
    if (baseLayer->isTextLayer()) {
        baseLayer->transform.scale.y *= -1.0f;
        notifyLayerChanged(activeLayerIndex);
        notifyChanged(Rect(0, 0, width, height));
            return;
    }

    PixelLayer* layer = dynamic_cast<PixelLayer*>(baseLayer);
    if (!layer) return;

    // Get content bounds to find center of actual content
    Recti contentBounds = layer->canvas.getContentBounds();
    if (contentBounds.w <= 0 || contentBounds.h <= 0) return;  // No content

    // Calculate content center in document coordinates
    f32 contentCenterY = layer->transform.position.y + contentBounds.y + contentBounds.h * 0.5f;

    u32 canvasH = layer->canvas.height;
    TiledCanvas flipped(layer->canvas.width, canvasH);

    layer->canvas.forEachPixel([&](u32 x, u32 y, u32 pixel) {
        flipped.setPixel(x, canvasH - 1 - y, pixel);
    });

    layer->canvas = std::move(flipped);

    // After vertical flip, content y position changes
    // New content y = canvasH - contentBounds.y - contentBounds.h
    i32 newContentY = static_cast<i32>(canvasH) - contentBounds.y - contentBounds.h;

    // Adjust layer position so content center stays at same document position
    layer->transform.position.y = contentCenterY - newContentY - contentBounds.h * 0.5f;

    notifyLayerChanged(activeLayerIndex);
    notifyChanged(Rect(0, 0, width, height));
}

bool Document::addFont(const std::string& fontName, std::vector<u8> data) {
    if (fontName.empty() || fontName == "Internal Font") {
        return false;  // Reserved names
    }

    // Check if already exists
    if (embeddedFonts.find(fontName) != embeddedFonts.end()) {
        return true;  // Already have this font
    }

    embeddedFonts[fontName] = std::move(data);
    return true;
}

bool Document::hasFont(const std::string& fontName) const {
    if (fontName.empty() || fontName == "Internal Font") {
        return true;  // Internal font always available
    }
    return embeddedFonts.find(fontName) != embeddedFonts.end();
}

const std::vector<u8>* Document::getFontData(const std::string& fontName) const {
    auto it = embeddedFonts.find(fontName);
    if (it != embeddedFonts.end()) {
        return &it->second;
    }
    return nullptr;
}

std::vector<std::string> Document::getFontNames() const {
    std::vector<std::string> names;
    for (const auto& [name, data] : embeddedFonts) {
        names.push_back(name);
    }
    return names;
}

void Document::addObserver(DocumentObserver* observer) {
    observers.push_back(observer);
}

void Document::removeObserver(DocumentObserver* observer) {
    observers.erase(std::remove(observers.begin(), observers.end(), observer), observers.end());
}

void Document::notifyChanged(const Rect& dirtyRect) {
    for (auto* observer : observers) {
        observer->onDocumentChanged(dirtyRect);
    }
}

void Document::notifyLayerAdded(i32 index) {
    for (auto* observer : observers) {
        observer->onLayerAdded(index);
    }
}

void Document::notifyLayerRemoved(i32 index) {
    for (auto* observer : observers) {
        observer->onLayerRemoved(index);
    }
}

void Document::notifyLayerMoved(i32 fromIndex, i32 toIndex) {
    for (auto* observer : observers) {
        observer->onLayerMoved(fromIndex, toIndex);
    }
}

void Document::notifyLayerChanged(i32 index) {
    for (auto* observer : observers) {
        observer->onLayerChanged(index);
    }
}

void Document::notifyActiveLayerChanged(i32 index) {
    for (auto* observer : observers) {
        observer->onActiveLayerChanged(index);
    }
}

void Document::notifySelectionChanged() {
    for (auto* observer : observers) {
        observer->onSelectionChanged();
    }
}
