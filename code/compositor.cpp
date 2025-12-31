#include "compositor.h"
#include "sampler.h"
#include "selection.h"
#include "platform.h"
#include "brush_tool.h"
#include "eraser_tool.h"
#include <cmath>

namespace Compositor {

// Pre-computed layer data to avoid per-pixel virtual calls and matrix inversions
struct LayerRenderData {
    enum class Type { Pixel, Text, Adjustment };
    Type type;
    BlendMode blend;
    f32 opacity;

    // Transform data (pre-computed)
    bool hasTransform;
    Matrix3x2 invMatrix;
    Vec2 position;  // For position-only layers

    // Type-specific pointers
    const TiledCanvas* canvas = nullptr;         // For pixel layers
    TiledCanvas* textCache = nullptr;            // For text layers (non-const for ensureCacheValid)
    const AdjustmentLayer* adjustment = nullptr; // For adjustment layers
    u32 canvasWidth = 0;
    u32 canvasHeight = 0;

    // Stroke overlay (only for the layer being painted)
    TiledCanvas* strokeBuffer = nullptr;
    f32 strokeOpacity = 1.0f;
    bool isEraserStroke = false;
};

void compositeLayer(TiledCanvas& dst, const TiledCanvas& src, BlendMode mode, f32 opacity) {
    if (opacity <= 0.0f) return;

    src.forEachPixel([&](u32 x, u32 y, u32 srcPixel) {
        if ((srcPixel & 0xFF) == 0) return; // Skip transparent pixels

        u32 dstPixel = dst.getPixel(x, y);
        u32 result = Blend::blend(dstPixel, srcPixel, mode, opacity);
        dst.setPixel(x, y, result);
    });
}

void compositeDocument(Framebuffer& fb, const Document& doc,
                       const Rect& viewport, f32 zoom, const Vec2& pan) {

    // Calculate visible document region (no bounds clamping - truly sparse canvas)
    f32 docLeft = -pan.x / zoom;
    f32 docTop = -pan.y / zoom;
    f32 docRight = (viewport.w - pan.x) / zoom;
    f32 docBottom = (viewport.h - pan.y) / zoom;

    // Check for active stroke buffer from brush or eraser tool
    TiledCanvas* strokeBuffer = nullptr;
    PixelLayer* strokeLayer = nullptr;
    f32 strokeOpacity = 1.0f;
    bool isEraserStroke = false;
    if (Tool* tool = const_cast<Document&>(doc).getTool()) {
        if (BrushTool* brushTool = dynamic_cast<BrushTool*>(tool)) {
            if (brushTool->isStroking()) {
                strokeBuffer = brushTool->getStrokeBuffer();
                strokeLayer = brushTool->getStrokeLayer();
                strokeOpacity = brushTool->getStrokeOpacity();
            }
        } else if (EraserTool* eraserTool = dynamic_cast<EraserTool*>(tool)) {
            if (eraserTool->isStroking()) {
                strokeBuffer = eraserTool->getStrokeBuffer();
                strokeLayer = eraserTool->getStrokeLayer();
                strokeOpacity = eraserTool->getStrokeOpacity();
                isEraserStroke = true;
            }
        }
    }

    // Draw checkerboard background for document area only
    Rect docScreenRect(
        pan.x + viewport.x,
        pan.y + viewport.y,
        doc.width * zoom,
        doc.height * zoom
    );
    Rect clippedDocRect = docScreenRect.intersection(viewport);
    drawCheckerboard(fb, clippedDocRect);

    // Choose sampling mode based on zoom
    SampleMode sampleMode = zoom < 1.0f ? SampleMode::Bilinear : SampleMode::Nearest;

    // Calculate screen bounds of document area for clipping
    i32 docScreenX0 = static_cast<i32>(viewport.x + pan.x);
    i32 docScreenY0 = static_cast<i32>(viewport.y + pan.y);
    i32 docScreenX1 = static_cast<i32>(std::ceil(viewport.x + pan.x + doc.width * zoom));
    i32 docScreenY1 = static_cast<i32>(std::ceil(viewport.y + pan.y + doc.height * zoom));

    // Clamp to viewport
    i32 renderX0 = std::max(static_cast<i32>(viewport.x), docScreenX0);
    i32 renderY0 = std::max(static_cast<i32>(viewport.y), docScreenY0);
    i32 renderX1 = std::min(static_cast<i32>(viewport.x + viewport.w), docScreenX1);
    i32 renderY1 = std::min(static_cast<i32>(viewport.y + viewport.h), docScreenY1);

    // Pre-compute layer render data to avoid per-pixel virtual calls and matrix inversions
    std::vector<LayerRenderData> layerCache;
    layerCache.reserve(doc.layers.size());

    for (const auto& layer : doc.layers) {
        if (!layer->visible) continue;

        LayerRenderData data;
        data.blend = layer->blend;
        data.opacity = layer->opacity;
        data.position = layer->transform.position;

        if (layer->isPixelLayer()) {
            data.type = LayerRenderData::Type::Pixel;
            const PixelLayer* pixelLayer = static_cast<const PixelLayer*>(layer.get());
            data.canvas = &pixelLayer->canvas;
            data.canvasWidth = pixelLayer->canvas.width;
            data.canvasHeight = pixelLayer->canvas.height;

            // Pre-compute transform
            data.hasTransform = layer->transform.rotation != 0.0f ||
                                layer->transform.scale.x != 1.0f ||
                                layer->transform.scale.y != 1.0f;

            if (data.hasTransform) {
                Matrix3x2 mat = layer->transform.toMatrix(data.canvasWidth, data.canvasHeight);
                data.invMatrix = mat.inverted();
            }

            // Check if this layer has an active stroke buffer
            if (strokeBuffer && strokeLayer == pixelLayer) {
                data.strokeBuffer = strokeBuffer;
                data.strokeOpacity = strokeOpacity;
                data.isEraserStroke = isEraserStroke;
            }
        }
        else if (layer->isTextLayer()) {
            data.type = LayerRenderData::Type::Text;
            TextLayer* textLayer = const_cast<TextLayer*>(static_cast<const TextLayer*>(layer.get()));
            textLayer->ensureCacheValid();
            data.textCache = &textLayer->rasterizedCache;
            data.canvasWidth = textLayer->rasterizedCache.width;
            data.canvasHeight = textLayer->rasterizedCache.height;

            // Pre-compute transform
            data.hasTransform = layer->transform.rotation != 0.0f ||
                                layer->transform.scale.x != 1.0f ||
                                layer->transform.scale.y != 1.0f;

            if (data.hasTransform) {
                Matrix3x2 mat = layer->transform.toMatrix(data.canvasWidth, data.canvasHeight);
                data.invMatrix = mat.inverted();
            }
        }
        else if (layer->isAdjustmentLayer()) {
            data.type = LayerRenderData::Type::Adjustment;
            data.adjustment = static_cast<const AdjustmentLayer*>(layer.get());
            data.hasTransform = false;
        }

        layerCache.push_back(data);
    }

    // Pre-compute floating content offset if active
    const bool hasFloating = doc.floatingContent.active && doc.floatingContent.pixels;
    f32 floatOffsetX = 0, floatOffsetY = 0;
    if (hasFloating) {
        floatOffsetX = doc.floatingContent.originalBounds.x + doc.floatingContent.currentOffset.x;
        floatOffsetY = doc.floatingContent.originalBounds.y + doc.floatingContent.currentOffset.y;
    }

    // Only render within document bounds (clipped to viewport)
    for (i32 screenY = renderY0; screenY < renderY1; ++screenY) {
        f32 docY = (screenY - viewport.y - pan.y) / zoom;

        for (i32 screenX = renderX0; screenX < renderX1; ++screenX) {
            f32 docX = (screenX - viewport.x - pan.x) / zoom;

            // Composite all layers at this pixel using cached data
            u32 composited = 0;

            for (const auto& data : layerCache) {
                if (data.type == LayerRenderData::Type::Adjustment) {
                    composited = applyAdjustment(composited, *data.adjustment);
                    continue;
                }

                u32 layerPixel = 0;
                f32 layerX, layerY;
                SampleMode actualMode = sampleMode;

                if (data.hasTransform) {
                    Vec2 srcCoord = data.invMatrix.transform(Vec2(docX, docY));
                    layerX = srcCoord.x;
                    layerY = srcCoord.y;
                    actualMode = SampleMode::Bilinear;
                } else {
                    layerX = docX - data.position.x;
                    layerY = docY - data.position.y;
                }

                if (data.type == LayerRenderData::Type::Pixel) {
                    if (actualMode == SampleMode::Nearest) {
                        i32 ix = static_cast<i32>(std::floor(layerX));
                        i32 iy = static_cast<i32>(std::floor(layerY));
                        layerPixel = data.canvas->getPixel(ix, iy);
                    } else {
                        layerPixel = Sampler::sample(*data.canvas, layerX, layerY, actualMode);
                    }

                    // Composite stroke buffer preview if this layer has one
                    if (data.strokeBuffer) {
                        i32 ix = static_cast<i32>(std::floor(layerX));
                        i32 iy = static_cast<i32>(std::floor(layerY));
                        u32 strokePixel = data.strokeBuffer->getPixel(ix, iy);
                        if ((strokePixel & 0xFF) > 0) {
                            if (data.isEraserStroke) {
                                u8 eraseAmount = strokePixel & 0xFF;
                                f32 eraseFactor = (eraseAmount / 255.0f) * data.strokeOpacity;
                                u8 r, g, b, a;
                                Blend::unpack(layerPixel, r, g, b, a);
                                a = static_cast<u8>(a * (1.0f - eraseFactor));
                                layerPixel = Blend::pack(r, g, b, a);
                            } else {
                                layerPixel = Blend::blend(layerPixel, strokePixel, BlendMode::Normal, data.strokeOpacity);
                            }
                        }
                    }
                }
                else if (data.type == LayerRenderData::Type::Text) {
                    if (actualMode == SampleMode::Nearest) {
                        i32 ix = static_cast<i32>(layerX);
                        i32 iy = static_cast<i32>(layerY);
                        if (ix >= 0 && iy >= 0 && ix < static_cast<i32>(data.canvasWidth) &&
                            iy < static_cast<i32>(data.canvasHeight)) {
                            layerPixel = data.textCache->getPixel(ix, iy);
                        }
                    } else {
                        layerPixel = Sampler::sample(*data.textCache, layerX, layerY, actualMode);
                    }
                }

                if ((layerPixel & 0xFF) > 0) {
                    composited = Blend::blend(composited, layerPixel, data.blend, data.opacity);
                }
            }

            // Composite floating content (selection being moved)
            if (hasFloating) {
                f32 floatX = docX - floatOffsetX;
                f32 floatY = docY - floatOffsetY;

                i32 ix = static_cast<i32>(std::floor(floatX));
                i32 iy = static_cast<i32>(std::floor(floatY));

                if (ix >= 0 && iy >= 0 &&
                    ix < static_cast<i32>(doc.floatingContent.pixels->width) &&
                    iy < static_cast<i32>(doc.floatingContent.pixels->height)) {
                    u32 floatPixel = doc.floatingContent.pixels->getPixel(ix, iy);
                    if ((floatPixel & 0xFF) > 0) {
                        composited = Blend::blend(composited, floatPixel, BlendMode::Normal, 1.0f);
                    }
                }
            }

            // Blend composited pixel onto framebuffer (checkerboard background)
            if ((composited & 0xFF) > 0) {
                fb.blendPixel(screenX, screenY, composited);
            }
        }
    }

    // Draw selection marching ants if selection exists
    if (doc.selection.hasSelection) {
        u64 time = Platform::getMilliseconds();
        drawMarchingAnts(fb, doc.selection, viewport, zoom, pan, time);
    }
}

void drawCheckerboard(Framebuffer& fb, const Rect& rect,
                      u32 color1, u32 color2) {
    // Use framebuffer's optimized checkerboard drawing
    Recti intRect(static_cast<i32>(rect.x), static_cast<i32>(rect.y),
                  static_cast<i32>(rect.w), static_cast<i32>(rect.h));
    fb.drawCheckerboard(intRect, color1, color2, Config::CHECKER_SIZE);
}

u32 applyAdjustment(u32 pixel, const AdjustmentLayer& adj) {
    if ((pixel & 0xFF) == 0) return pixel;

    u8 r, g, b, a;
    Blend::unpack(pixel, r, g, b, a);

    f32 fr = r / 255.0f;
    f32 fg = g / 255.0f;
    f32 fb_color = b / 255.0f;

    switch (adj.type) {
        case AdjustmentType::BrightnessContrast: {
            const auto* params = std::get_if<BrightnessContrastParams>(&adj.params);
            if (params) {
                f32 brightness = params->brightness / 100.0f;
                f32 contrast = (params->contrast + 100.0f) / 100.0f;

                fr = (fr - 0.5f) * contrast + 0.5f + brightness;
                fg = (fg - 0.5f) * contrast + 0.5f + brightness;
                fb_color = (fb_color - 0.5f) * contrast + 0.5f + brightness;
            }
            break;
        }

        case AdjustmentType::HueSaturation: {
            const auto* params = std::get_if<HueSaturationParams>(&adj.params);
            if (params) {
                // Convert to HSL
                f32 maxC = std::max({fr, fg, fb_color});
                f32 minC = std::min({fr, fg, fb_color});
                f32 l = (maxC + minC) / 2.0f;

                if (maxC != minC) {
                    f32 d = maxC - minC;
                    f32 s = l > 0.5f ? d / (2.0f - maxC - minC) : d / (maxC + minC);

                    f32 h = 0;
                    if (maxC == fr) {
                        h = (fg - fb_color) / d + (fg < fb_color ? 6.0f : 0.0f);
                    } else if (maxC == fg) {
                        h = (fb_color - fr) / d + 2.0f;
                    } else {
                        h = (fr - fg) / d + 4.0f;
                    }
                    h /= 6.0f;

                    // Apply adjustments
                    h += params->hue / 360.0f;
                    while (h < 0) h += 1.0f;
                    while (h >= 1.0f) h -= 1.0f;

                    s *= 1.0f + params->saturation / 100.0f;
                    s = clamp(s, 0.0f, 1.0f);

                    l += params->lightness / 100.0f;
                    l = clamp(l, 0.0f, 1.0f);

                    // Convert back to RGB
                    auto hue2rgb = [](f32 p, f32 q, f32 t) {
                        if (t < 0) t += 1.0f;
                        if (t > 1) t -= 1.0f;
                        if (t < 1.0f/6.0f) return p + (q - p) * 6.0f * t;
                        if (t < 1.0f/2.0f) return q;
                        if (t < 2.0f/3.0f) return p + (q - p) * (2.0f/3.0f - t) * 6.0f;
                        return p;
                    };

                    f32 q = l < 0.5f ? l * (1.0f + s) : l + s - l * s;
                    f32 p = 2.0f * l - q;

                    fr = hue2rgb(p, q, h + 1.0f/3.0f);
                    fg = hue2rgb(p, q, h);
                    fb_color = hue2rgb(p, q, h - 1.0f/3.0f);
                }
            }
            break;
        }

        case AdjustmentType::Invert: {
            fr = 1.0f - fr;
            fg = 1.0f - fg;
            fb_color = 1.0f - fb_color;
            break;
        }

        case AdjustmentType::Exposure: {
            const auto* params = std::get_if<ExposureParams>(&adj.params);
            if (params) {
                f32 exposure = std::pow(2.0f, params->exposure);
                fr = std::pow(fr * exposure + params->offset, 1.0f / params->gamma);
                fg = std::pow(fg * exposure + params->offset, 1.0f / params->gamma);
                fb_color = std::pow(fb_color * exposure + params->offset, 1.0f / params->gamma);
            }
            break;
        }

        case AdjustmentType::BlackAndWhite: {
            const auto* params = std::get_if<BlackAndWhiteParams>(&adj.params);
            if (params) {
                // Simple grayscale conversion weighted by color channels
                f32 gray = fr * (params->reds / 100.0f) +
                           fg * (params->greens / 100.0f) +
                           fb_color * (params->blues / 100.0f);
                gray = clamp(gray, 0.0f, 1.0f);

                if (params->tintAmount > 0) {
                    // Apply tint (simplified)
                    f32 t = params->tintAmount / 100.0f;
                    // Tint towards sepia-like color based on hue
                    f32 tintR = 1.0f, tintG = 0.9f, tintB = 0.7f;
                    fr = gray * (1.0f - t) + gray * tintR * t;
                    fg = gray * (1.0f - t) + gray * tintG * t;
                    fb_color = gray * (1.0f - t) + gray * tintB * t;
                } else {
                    fr = fg = fb_color = gray;
                }
            }
            break;
        }

        case AdjustmentType::TemperatureTint: {
            const auto* params = std::get_if<TemperatureTintParams>(&adj.params);
            if (params) {
                // Temperature: negative = cooler (blue), positive = warmer (orange)
                f32 temp = params->temperature / 100.0f;
                // Tint: negative = green, positive = magenta
                f32 tint = params->tint / 100.0f;

                fr += temp * 0.3f;
                fb_color -= temp * 0.3f;
                fg -= tint * 0.3f;
                fr += tint * 0.15f;
                fb_color += tint * 0.15f;
            }
            break;
        }

        case AdjustmentType::Vibrance: {
            const auto* params = std::get_if<VibranceParams>(&adj.params);
            if (params) {
                // Vibrance increases saturation of less-saturated colors more
                f32 maxC = std::max({fr, fg, fb_color});
                f32 minC = std::min({fr, fg, fb_color});
                f32 sat = (maxC > 0) ? (maxC - minC) / maxC : 0.0f;

                // Less saturated colors get more boost
                f32 boost = (1.0f - sat) * (params->vibrance / 100.0f);
                f32 avg = (fr + fg + fb_color) / 3.0f;

                fr = fr + (fr - avg) * boost;
                fg = fg + (fg - avg) * boost;
                fb_color = fb_color + (fb_color - avg) * boost;
            }
            break;
        }

        case AdjustmentType::ColorBalance: {
            const auto* params = std::get_if<ColorBalanceParams>(&adj.params);
            if (params) {
                // Determine luminance to weight shadows/midtones/highlights
                f32 lum = 0.299f * fr + 0.587f * fg + 0.114f * fb_color;

                // Shadow influence (more for dark pixels)
                f32 shadowWeight = 1.0f - clamp(lum * 2.0f, 0.0f, 1.0f);
                // Highlight influence (more for bright pixels)
                f32 highlightWeight = clamp((lum - 0.5f) * 2.0f, 0.0f, 1.0f);
                // Midtone influence (peak at middle luminance)
                f32 midtoneWeight = 1.0f - std::abs(lum - 0.5f) * 2.0f;
                midtoneWeight = clamp(midtoneWeight, 0.0f, 1.0f);

                // Apply shadows adjustments
                fr += shadowWeight * params->shadowsCyanRed / 100.0f * 0.5f;
                fg += shadowWeight * params->shadowsMagentaGreen / 100.0f * 0.5f;
                fb_color += shadowWeight * params->shadowsYellowBlue / 100.0f * 0.5f;

                // Apply midtones adjustments
                fr += midtoneWeight * params->midtonesCyanRed / 100.0f * 0.5f;
                fg += midtoneWeight * params->midtonesMagentaGreen / 100.0f * 0.5f;
                fb_color += midtoneWeight * params->midtonesYellowBlue / 100.0f * 0.5f;

                // Apply highlights adjustments
                fr += highlightWeight * params->highlightsCyanRed / 100.0f * 0.5f;
                fg += highlightWeight * params->highlightsMagentaGreen / 100.0f * 0.5f;
                fb_color += highlightWeight * params->highlightsYellowBlue / 100.0f * 0.5f;
            }
            break;
        }

        case AdjustmentType::HighlightsShadows: {
            const auto* params = std::get_if<HighlightsShadowsParams>(&adj.params);
            if (params) {
                f32 lum = 0.299f * fr + 0.587f * fg + 0.114f * fb_color;

                // Shadow recovery (brighten dark areas)
                f32 shadowMask = 1.0f - clamp(lum * 2.0f, 0.0f, 1.0f);
                f32 shadowBoost = shadowMask * params->shadows / 100.0f;

                // Highlight recovery (darken bright areas)
                f32 highlightMask = clamp((lum - 0.5f) * 2.0f, 0.0f, 1.0f);
                f32 highlightBoost = -highlightMask * params->highlights / 100.0f;

                f32 adjustment = shadowBoost + highlightBoost;
                fr += adjustment;
                fg += adjustment;
                fb_color += adjustment;
            }
            break;
        }

        case AdjustmentType::Levels: {
            const auto* params = std::get_if<LevelsParams>(&adj.params);
            if (params) {
                // Input levels: remap input range
                f32 inBlack = params->inputBlack / 255.0f;
                f32 inWhite = params->inputWhite / 255.0f;
                f32 gamma = params->inputGamma;

                // Output levels
                f32 outBlack = params->outputBlack / 255.0f;
                f32 outWhite = params->outputWhite / 255.0f;

                auto applyLevels = [&](f32 val) {
                    // Input mapping
                    val = (val - inBlack) / (inWhite - inBlack);
                    val = clamp(val, 0.0f, 1.0f);
                    // Gamma
                    val = std::pow(val, 1.0f / gamma);
                    // Output mapping
                    val = val * (outWhite - outBlack) + outBlack;
                    return val;
                };

                fr = applyLevels(fr);
                fg = applyLevels(fg);
                fb_color = applyLevels(fb_color);
            }
            break;
        }

        default:
            // Unknown adjustment type
            break;
    }

    // Clamp and convert back
    fr = clamp(fr, 0.0f, 1.0f);
    fg = clamp(fg, 0.0f, 1.0f);
    fb_color = clamp(fb_color, 0.0f, 1.0f);

    return Blend::pack(
        static_cast<u8>(fr * 255.0f),
        static_cast<u8>(fg * 255.0f),
        static_cast<u8>(fb_color * 255.0f),
        a
    );
}

void drawMarchingAnts(Framebuffer& fb, const Selection& sel,
                      const Rect& viewport, f32 zoom, const Vec2& pan, u64 time) {
    if (!sel.hasSelection) return;

    // Animation phase - cycles every 200ms, 8 positions
    u32 phase = static_cast<u32>((time / 100) % 8);

    // Colors for marching ants (RGBA8888 format: 0xRRGGBBAA)
    const u32 COLOR_BLACK = 0x000000FF;  // Black with full alpha
    const u32 COLOR_WHITE = 0xFFFFFFFF;  // White with full alpha

    // Line thickness - use UI_SCALE for proper HiDPI visibility
    i32 lineThickness = std::max(3, static_cast<i32>(Config::uiScale + 0.5f));

    // Helper to draw a horizontal line segment with marching ants pattern
    auto drawHLine = [&](i32 screenX1, i32 screenX2, i32 screenY) {
        for (i32 t = 0; t < lineThickness; ++t) {
            i32 py = screenY + t;
            if (py < static_cast<i32>(viewport.y) || py >= static_cast<i32>(viewport.y + viewport.h)) continue;
            if (py < 0 || py >= static_cast<i32>(fb.height)) continue;

            for (i32 px = screenX1; px < screenX2; ++px) {
                if (px < static_cast<i32>(viewport.x) || px >= static_cast<i32>(viewport.x + viewport.w)) continue;
                if (px < 0 || px >= static_cast<i32>(fb.width)) continue;

                // Marching ants pattern based on screen position
                u32 pattern = (px + py + static_cast<i32>(phase * 2)) % 8;
                u32 color = (pattern < 4) ? COLOR_BLACK : COLOR_WHITE;
                fb.setPixel(px, py, color);
            }
        }
    };

    // Helper to draw a vertical line segment with marching ants pattern
    auto drawVLine = [&](i32 screenX, i32 screenY1, i32 screenY2) {
        for (i32 t = 0; t < lineThickness; ++t) {
            i32 px = screenX + t;
            if (px < static_cast<i32>(viewport.x) || px >= static_cast<i32>(viewport.x + viewport.w)) continue;
            if (px < 0 || px >= static_cast<i32>(fb.width)) continue;

            for (i32 py = screenY1; py < screenY2; ++py) {
                if (py < static_cast<i32>(viewport.y) || py >= static_cast<i32>(viewport.y + viewport.h)) continue;
                if (py < 0 || py >= static_cast<i32>(fb.height)) continue;

                // Marching ants pattern based on screen position
                u32 pattern = (px + py + static_cast<i32>(phase * 2)) % 8;
                u32 color = (pattern < 4) ? COLOR_BLACK : COLOR_WHITE;
                fb.setPixel(px, py, color);
            }
        }
    };

    // Iterate over selection bounds
    const Recti& bounds = sel.bounds;

    for (i32 docY = bounds.y; docY < bounds.y + bounds.h; ++docY) {
        for (i32 docX = bounds.x; docX < bounds.x + bounds.w; ++docX) {
            // Skip if not selected
            if (sel.getValue(docX, docY) == 0) continue;

            // Calculate screen rectangle for this document pixel
            i32 sx1 = static_cast<i32>(viewport.x + pan.x + docX * zoom);
            i32 sy1 = static_cast<i32>(viewport.y + pan.y + docY * zoom);
            i32 sx2 = static_cast<i32>(viewport.x + pan.x + (docX + 1) * zoom);
            i32 sy2 = static_cast<i32>(viewport.y + pan.y + (docY + 1) * zoom);

            // Check each edge - draw line if neighbor is not selected
            // Left edge
            if (docX == 0 || sel.getValue(docX - 1, docY) == 0) {
                drawVLine(sx1, sy1, sy2);
            }
            // Right edge
            if (docX == static_cast<i32>(sel.width) - 1 || sel.getValue(docX + 1, docY) == 0) {
                drawVLine(sx2 - lineThickness, sy1, sy2);
            }
            // Top edge
            if (docY == 0 || sel.getValue(docX, docY - 1) == 0) {
                drawHLine(sx1, sx2, sy1);
            }
            // Bottom edge
            if (docY == static_cast<i32>(sel.height) - 1 || sel.getValue(docX, docY + 1) == 0) {
                drawHLine(sx1, sx2, sy2 - lineThickness);
            }
        }
    }
}

}
