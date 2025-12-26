#include "tool.h"
#include "app_state.h"
#include "compositor.h"
#include "blend.h"
#include "sampler.h"

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
