#include "brush_renderer.h"
#include <cmath>
#include <algorithm>
#include <random>

namespace BrushRenderer {

// Generate a circular brush stamp with given diameter and hardness
BrushStamp generateStamp(f32 diameter, f32 hardness) {
    u32 size = static_cast<u32>(std::ceil(diameter));
    if (size < 1) size = 1;

    BrushStamp stamp(size);
    f32 radius = diameter / 2.0f;
    f32 center = (size - 1) / 2.0f;

    // Hardness controls the falloff curve
    // hardness = 1.0: sharp edge (no falloff)
    // hardness = 0.0: very soft (linear falloff from center)
    f32 hardnessRadius = radius * hardness;
    f32 softRadius = radius - hardnessRadius;

    for (u32 y = 0; y < size; ++y) {
        for (u32 x = 0; x < size; ++x) {
            f32 dx = x - center;
            f32 dy = y - center;
            f32 dist = std::sqrt(dx * dx + dy * dy);

            f32 alpha = 0.0f;
            if (dist <= hardnessRadius) {
                alpha = 1.0f;
            } else if (dist < radius) {
                // Smooth falloff in the soft region
                f32 t = (dist - hardnessRadius) / softRadius;
                alpha = 1.0f - t * t;  // Quadratic falloff
            }

            stamp.setAlpha(x, y, alpha);
        }
    }

    return stamp;
}

// Apply brush stamp to canvas at position
void stamp(TiledCanvas& canvas, const BrushStamp& brush,
           const Vec2& pos, u32 color, f32 opacity,
           BlendMode mode, const Selection* selection) {
    i32 startX = static_cast<i32>(pos.x - brush.size / 2.0f);
    i32 startY = static_cast<i32>(pos.y - brush.size / 2.0f);

    u8 cr, cg, cb, ca;
    Blend::unpack(color, cr, cg, cb, ca);

    for (u32 by = 0; by < brush.size; ++by) {
        i32 canvasY = startY + by;
        for (u32 bx = 0; bx < brush.size; ++bx) {
            i32 canvasX = startX + bx;

            f32 brushAlpha = brush.getAlpha(bx, by);
            if (brushAlpha <= 0.0f) continue;

            // Check selection mask
            if (selection && selection->hasSelection) {
                if (canvasX < 0 || canvasY < 0 ||
                    canvasX >= static_cast<i32>(selection->width) ||
                    canvasY >= static_cast<i32>(selection->height)) continue;
                f32 selAlpha = selection->getValue(canvasX, canvasY) / 255.0f;
                if (selAlpha <= 0.0f) continue;
                brushAlpha *= selAlpha;
            }

            // Combine brush alpha with opacity and color alpha
            f32 finalAlpha = brushAlpha * opacity * (ca / 255.0f);
            u8 newAlpha = static_cast<u8>(std::min(255.0f, finalAlpha * 255.0f));
            u32 brushColor = Blend::pack(cr, cg, cb, newAlpha);

            canvas.blendPixel(canvasX, canvasY, brushColor, mode, 1.0f);
        }
    }
}

// Stamp to stroke buffer with flow
// Uses MAX blending to prevent overlapping dabs from accumulating opacity
void stampToBuffer(TiledCanvas& buffer, const BrushStamp& brush,
                   const Vec2& pos, u32 color, f32 flow,
                   BlendMode mode, const Selection* selection) {
    i32 startX = static_cast<i32>(pos.x - brush.size / 2.0f);
    i32 startY = static_cast<i32>(pos.y - brush.size / 2.0f);

    u8 cr, cg, cb, ca;
    Blend::unpack(color, cr, cg, cb, ca);

    for (u32 by = 0; by < brush.size; ++by) {
        i32 canvasY = startY + by;
        for (u32 bx = 0; bx < brush.size; ++bx) {
            i32 canvasX = startX + bx;

            f32 brushAlpha = brush.getAlpha(bx, by);
            if (brushAlpha <= 0.0f) continue;

            // Check selection mask
            if (selection && selection->hasSelection) {
                if (canvasX < 0 || canvasY < 0 ||
                    canvasX >= static_cast<i32>(selection->width) ||
                    canvasY >= static_cast<i32>(selection->height)) continue;
                f32 selAlpha = selection->getValue(canvasX, canvasY) / 255.0f;
                if (selAlpha <= 0.0f) continue;
                brushAlpha *= selAlpha;
            }

            // Apply flow to brush alpha
            f32 finalAlpha = brushAlpha * flow * (ca / 255.0f);
            u8 newAlpha = static_cast<u8>(std::min(255.0f, finalAlpha * 255.0f));

            // Use MAX blending: only replace if new alpha is greater
            // This prevents overlapping dabs from building up opacity
            u32 existing = buffer.getPixel(canvasX, canvasY);
            u8 existingAlpha = existing & 0xFF;
            if (newAlpha > existingAlpha) {
                u32 brushColor = Blend::pack(cr, cg, cb, newAlpha);
                buffer.setPixel(canvasX, canvasY, brushColor);
            }
        }
    }
}

// Stroke line to buffer with flow
void strokeLineToBuffer(TiledCanvas& buffer, const BrushStamp& brush,
                        const Vec2& from, const Vec2& to, u32 color,
                        f32 flow, f32 spacing, BlendMode mode,
                        const Selection* selection) {
    Vec2 delta = to - from;
    f32 distance = delta.length();

    if (distance < 0.001f) {
        stampToBuffer(buffer, brush, to, color, flow, mode, selection);
        return;
    }

    f32 step = std::max(1.0f, brush.size * spacing);
    i32 steps = static_cast<i32>(distance / step);

    for (i32 i = 0; i <= steps; ++i) {
        f32 t = (steps > 0) ? static_cast<f32>(i) / steps : 1.0f;
        Vec2 pos = from + delta * t;
        stampToBuffer(buffer, brush, pos, color, flow, mode, selection);
    }
}

// Composite stroke buffer onto layer canvas with opacity
void compositeStrokeToLayer(TiledCanvas& layer, const TiledCanvas& stroke,
                            f32 opacity, BlendMode mode) {
    stroke.forEachTile([&](i32 tileX, i32 tileY, const Tile& tile) {
        i32 baseX = tileX * static_cast<i32>(Config::TILE_SIZE);
        i32 baseY = tileY * static_cast<i32>(Config::TILE_SIZE);

        for (u32 ly = 0; ly < Config::TILE_SIZE; ++ly) {
            i32 y = baseY + static_cast<i32>(ly);
            for (u32 lx = 0; lx < Config::TILE_SIZE; ++lx) {
                i32 x = baseX + static_cast<i32>(lx);
                u32 strokePixel = tile.getPixel(lx, ly);
                u8 strokeAlpha = strokePixel & 0xFF;

                if (strokeAlpha > 0) {
                    layer.blendPixel(x, y, strokePixel, mode, opacity);
                }
            }
        }
    });
}

// Erase stamp to buffer
void eraseStampToBuffer(TiledCanvas& buffer, const BrushStamp& brush,
                        const Vec2& pos, f32 flow, const Selection* selection) {
    i32 startX = static_cast<i32>(pos.x - brush.size / 2.0f);
    i32 startY = static_cast<i32>(pos.y - brush.size / 2.0f);

    for (u32 by = 0; by < brush.size; ++by) {
        i32 canvasY = startY + by;
        for (u32 bx = 0; bx < brush.size; ++bx) {
            i32 canvasX = startX + bx;

            f32 brushAlpha = brush.getAlpha(bx, by);
            if (brushAlpha <= 0.0f) continue;

            // Check selection mask
            if (selection && selection->hasSelection) {
                if (canvasX < 0 || canvasY < 0 ||
                    canvasX >= static_cast<i32>(selection->width) ||
                    canvasY >= static_cast<i32>(selection->height)) continue;
                f32 selAlpha = selection->getValue(canvasX, canvasY) / 255.0f;
                if (selAlpha <= 0.0f) continue;
                brushAlpha *= selAlpha;
            }

            // Store erase intensity as white with alpha
            f32 eraseAlpha = brushAlpha * flow;
            u8 alpha = static_cast<u8>(std::min(255.0f, eraseAlpha * 255.0f));
            u32 eraseColor = Blend::pack(255, 255, 255, alpha);

            // Blend into buffer (max alpha)
            u32 existing = buffer.getPixel(canvasX, canvasY);
            u8 existingAlpha = existing & 0xFF;
            if (alpha > existingAlpha) {
                buffer.setPixel(canvasX, canvasY, eraseColor);
            }
        }
    }
}

// Erase line to buffer
void eraseLineToBuffer(TiledCanvas& buffer, const BrushStamp& brush,
                       const Vec2& from, const Vec2& to, f32 flow, f32 spacing,
                       const Selection* selection) {
    Vec2 delta = to - from;
    f32 distance = delta.length();

    if (distance < 0.001f) {
        eraseStampToBuffer(buffer, brush, to, flow, selection);
        return;
    }

    f32 step = std::max(1.0f, brush.size * spacing);
    i32 steps = static_cast<i32>(distance / step);

    for (i32 i = 0; i <= steps; ++i) {
        f32 t = (steps > 0) ? static_cast<f32>(i) / steps : 1.0f;
        Vec2 pos = from + delta * t;
        eraseStampToBuffer(buffer, brush, pos, flow, selection);
    }
}

// Composite erase buffer to layer
void compositeEraseBufferToLayer(TiledCanvas& layer, const TiledCanvas& eraseBuffer,
                                 f32 opacity) {
    eraseBuffer.forEachTile([&](i32 tileX, i32 tileY, const Tile& tile) {
        i32 baseX = tileX * static_cast<i32>(Config::TILE_SIZE);
        i32 baseY = tileY * static_cast<i32>(Config::TILE_SIZE);

        for (u32 ly = 0; ly < Config::TILE_SIZE; ++ly) {
            i32 y = baseY + static_cast<i32>(ly);
            for (u32 lx = 0; lx < Config::TILE_SIZE; ++lx) {
                i32 x = baseX + static_cast<i32>(lx);
                u32 erasePixel = tile.getPixel(lx, ly);
                u8 eraseAlpha = erasePixel & 0xFF;

                if (eraseAlpha > 0) {
                    u32 layerPixel = layer.getPixel(x, y);
                    u8 lr, lg, lb, la;
                    Blend::unpack(layerPixel, lr, lg, lb, la);

                    // Reduce layer alpha based on erase intensity
                    f32 reduction = (eraseAlpha / 255.0f) * opacity;
                    f32 newAlpha = la * (1.0f - reduction);
                    la = static_cast<u8>(std::max(0.0f, newAlpha));

                    layer.setPixel(x, y, Blend::pack(lr, lg, lb, la));
                }
            }
        }
    });
}

// Direct erase (immediate mode)
void erase(TiledCanvas& canvas, const BrushStamp& brush,
           const Vec2& pos, f32 opacity, const Selection* selection) {
    i32 startX = static_cast<i32>(pos.x - brush.size / 2.0f);
    i32 startY = static_cast<i32>(pos.y - brush.size / 2.0f);

    for (u32 by = 0; by < brush.size; ++by) {
        i32 canvasY = startY + by;
        for (u32 bx = 0; bx < brush.size; ++bx) {
            i32 canvasX = startX + bx;

            f32 brushAlpha = brush.getAlpha(bx, by);
            if (brushAlpha <= 0.0f) continue;

            // Check selection mask
            if (selection && selection->hasSelection) {
                if (canvasX < 0 || canvasY < 0 ||
                    canvasX >= static_cast<i32>(selection->width) ||
                    canvasY >= static_cast<i32>(selection->height)) continue;
                f32 selAlpha = selection->getValue(canvasX, canvasY) / 255.0f;
                if (selAlpha <= 0.0f) continue;
                brushAlpha *= selAlpha;
            }

            u32 pixel = canvas.getPixel(canvasX, canvasY);
            u8 r, g, b, a;
            Blend::unpack(pixel, r, g, b, a);

            f32 reduction = brushAlpha * opacity;
            f32 newAlpha = a * (1.0f - reduction);
            a = static_cast<u8>(std::max(0.0f, newAlpha));

            canvas.setPixel(canvasX, canvasY, Blend::pack(r, g, b, a));
        }
    }
}

// Interpolate brush strokes between two points
void strokeLine(TiledCanvas& canvas, const BrushStamp& brush,
                const Vec2& from, const Vec2& to, u32 color, f32 opacity,
                f32 spacing, BlendMode mode, const Selection* selection) {
    Vec2 delta = to - from;
    f32 distance = delta.length();

    if (distance < 0.001f) {
        stamp(canvas, brush, to, color, opacity, mode, selection);
        return;
    }

    f32 step = std::max(1.0f, brush.size * spacing);
    i32 steps = static_cast<i32>(distance / step);

    for (i32 i = 0; i <= steps; ++i) {
        f32 t = (steps > 0) ? static_cast<f32>(i) / steps : 1.0f;
        Vec2 pos = from + delta * t;
        stamp(canvas, brush, pos, color, opacity, mode, selection);
    }
}

// Erase line (immediate mode)
void eraseLineTool(TiledCanvas& canvas, const BrushStamp& brush,
                   const Vec2& from, const Vec2& to, f32 opacity, f32 spacing,
                   const Selection* selection) {
    Vec2 delta = to - from;
    f32 distance = delta.length();

    if (distance < 0.001f) {
        erase(canvas, brush, to, opacity, selection);
        return;
    }

    f32 step = std::max(1.0f, brush.size * spacing);
    i32 steps = static_cast<i32>(distance / step);

    for (i32 i = 0; i <= steps; ++i) {
        f32 t = (steps > 0) ? static_cast<f32>(i) / steps : 1.0f;
        Vec2 pos = from + delta * t;
        erase(canvas, brush, pos, opacity, selection);
    }
}

// Pencil mode functions
void pencilPixel(TiledCanvas& canvas, i32 x, i32 y, u32 color, f32 opacity,
                 const Selection* selection) {
    // Check selection mask
    if (selection && selection->hasSelection) {
        if (x < 0 || y < 0 ||
            x >= static_cast<i32>(selection->width) ||
            y >= static_cast<i32>(selection->height)) return;
        if (selection->getValue(x, y) == 0) return;
    }

    u8 cr, cg, cb, ca;
    Blend::unpack(color, cr, cg, cb, ca);
    u8 newAlpha = static_cast<u8>(std::min(255.0f, ca * opacity));
    u32 finalColor = Blend::pack(cr, cg, cb, newAlpha);

    canvas.alphaBlendPixel(x, y, finalColor);
}

void pencilErase(TiledCanvas& canvas, i32 x, i32 y, f32 opacity,
                 const Selection* selection) {
    // Check selection mask
    if (selection && selection->hasSelection) {
        if (x < 0 || y < 0 ||
            x >= static_cast<i32>(selection->width) ||
            y >= static_cast<i32>(selection->height)) return;
        if (selection->getValue(x, y) == 0) return;
    }

    u32 pixel = canvas.getPixel(x, y);
    u8 r, g, b, a;
    Blend::unpack(pixel, r, g, b, a);

    f32 newAlpha = a * (1.0f - opacity);
    a = static_cast<u8>(std::max(0.0f, newAlpha));

    canvas.setPixel(x, y, Blend::pack(r, g, b, a));
}

void pencilLine(TiledCanvas& canvas, i32 x0, i32 y0, i32 x1, i32 y1,
                u32 color, f32 opacity, const Selection* selection) {
    // Bresenham's line algorithm
    i32 dx = std::abs(x1 - x0);
    i32 dy = -std::abs(y1 - y0);
    i32 sx = x0 < x1 ? 1 : -1;
    i32 sy = y0 < y1 ? 1 : -1;
    i32 err = dx + dy;

    while (true) {
        pencilPixel(canvas, x0, y0, color, opacity, selection);

        if (x0 == x1 && y0 == y1) break;

        i32 e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void pencilEraseLine(TiledCanvas& canvas, i32 x0, i32 y0, i32 x1, i32 y1,
                     f32 opacity, const Selection* selection) {
    // Bresenham's line algorithm
    i32 dx = std::abs(x1 - x0);
    i32 dy = -std::abs(y1 - y0);
    i32 sx = x0 < x1 ? 1 : -1;
    i32 sy = y0 < y1 ? 1 : -1;
    i32 err = dx + dy;

    while (true) {
        pencilErase(canvas, x0, y0, opacity, selection);

        if (x0 == x1 && y0 == y1) break;

        i32 e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

// Opacity-limited functions (for stroke opacity ceiling)
void stampWithOpacityLimit(TiledCanvas& canvas, const BrushStamp& brush,
                           const Vec2& pos, u32 color, f32 flow, f32 strokeOpacity,
                           std::unordered_map<u64, f32>& strokeAlphaMap,
                           BlendMode mode, const Selection* selection) {
    i32 startX = static_cast<i32>(pos.x - brush.size / 2.0f);
    i32 startY = static_cast<i32>(pos.y - brush.size / 2.0f);

    u8 cr, cg, cb, ca;
    Blend::unpack(color, cr, cg, cb, ca);

    for (u32 by = 0; by < brush.size; ++by) {
        i32 canvasY = startY + by;
        for (u32 bx = 0; bx < brush.size; ++bx) {
            i32 canvasX = startX + bx;

            f32 brushAlpha = brush.getAlpha(bx, by);
            if (brushAlpha <= 0.0f) continue;

            // Check selection mask
            if (selection && selection->hasSelection) {
                if (canvasX < 0 || canvasY < 0 ||
                    canvasX >= static_cast<i32>(selection->width) ||
                    canvasY >= static_cast<i32>(selection->height)) continue;
                f32 selAlpha = selection->getValue(canvasX, canvasY) / 255.0f;
                if (selAlpha <= 0.0f) continue;
                brushAlpha *= selAlpha;
            }

            // Calculate desired alpha for this dab
            f32 dabAlpha = brushAlpha * flow * (ca / 255.0f);

            // Get current stroke alpha at this pixel
            u64 key = packCoords(canvasX, canvasY);
            f32& currentAlpha = strokeAlphaMap[key];

            // Only apply if we haven't reached the stroke opacity ceiling
            if (currentAlpha < strokeOpacity) {
                f32 remaining = strokeOpacity - currentAlpha;
                f32 applyAlpha = std::min(dabAlpha, remaining);

                u8 newAlpha = static_cast<u8>(std::min(255.0f, applyAlpha * 255.0f));
                u32 brushColor = Blend::pack(cr, cg, cb, newAlpha);

                canvas.blendPixel(canvasX, canvasY, brushColor, mode, 1.0f);
                currentAlpha += applyAlpha;
            }
        }
    }
}

void strokeLineWithOpacityLimit(TiledCanvas& canvas, const BrushStamp& brush,
                                const Vec2& from, const Vec2& to, u32 color,
                                f32 flow, f32 strokeOpacity, f32 spacing,
                                std::unordered_map<u64, f32>& strokeAlphaMap,
                                BlendMode mode, const Selection* selection) {
    Vec2 delta = to - from;
    f32 distance = delta.length();

    if (distance < 0.001f) {
        stampWithOpacityLimit(canvas, brush, to, color, flow, strokeOpacity,
                              strokeAlphaMap, mode, selection);
        return;
    }

    f32 step = std::max(1.0f, brush.size * spacing);
    i32 steps = static_cast<i32>(distance / step);

    for (i32 i = 0; i <= steps; ++i) {
        f32 t = (steps > 0) ? static_cast<f32>(i) / steps : 1.0f;
        Vec2 pos = from + delta * t;
        stampWithOpacityLimit(canvas, brush, pos, color, flow, strokeOpacity,
                              strokeAlphaMap, mode, selection);
    }
}

void eraseWithOpacityLimit(TiledCanvas& canvas, const BrushStamp& brush,
                           const Vec2& pos, f32 flow, f32 strokeOpacity,
                           std::unordered_map<u64, f32>& strokeAlphaMap,
                           const Selection* selection) {
    i32 startX = static_cast<i32>(pos.x - brush.size / 2.0f);
    i32 startY = static_cast<i32>(pos.y - brush.size / 2.0f);

    for (u32 by = 0; by < brush.size; ++by) {
        i32 canvasY = startY + by;
        for (u32 bx = 0; bx < brush.size; ++bx) {
            i32 canvasX = startX + bx;

            f32 brushAlpha = brush.getAlpha(bx, by);
            if (brushAlpha <= 0.0f) continue;

            // Check selection mask
            if (selection && selection->hasSelection) {
                if (canvasX < 0 || canvasY < 0 ||
                    canvasX >= static_cast<i32>(selection->width) ||
                    canvasY >= static_cast<i32>(selection->height)) continue;
                f32 selAlpha = selection->getValue(canvasX, canvasY) / 255.0f;
                if (selAlpha <= 0.0f) continue;
                brushAlpha *= selAlpha;
            }

            f32 dabAlpha = brushAlpha * flow;

            u64 key = packCoords(canvasX, canvasY);
            f32& currentAlpha = strokeAlphaMap[key];

            if (currentAlpha < strokeOpacity) {
                f32 remaining = strokeOpacity - currentAlpha;
                f32 applyAlpha = std::min(dabAlpha, remaining);

                u32 pixel = canvas.getPixel(canvasX, canvasY);
                u8 r, g, b, a;
                Blend::unpack(pixel, r, g, b, a);

                f32 newAlpha = a * (1.0f - applyAlpha);
                a = static_cast<u8>(std::max(0.0f, newAlpha));

                canvas.setPixel(canvasX, canvasY, Blend::pack(r, g, b, a));
                currentAlpha += applyAlpha;
            }
        }
    }
}

void eraseLineWithOpacityLimit(TiledCanvas& canvas, const BrushStamp& brush,
                               const Vec2& from, const Vec2& to,
                               f32 flow, f32 strokeOpacity, f32 spacing,
                               std::unordered_map<u64, f32>& strokeAlphaMap,
                               const Selection* selection) {
    Vec2 delta = to - from;
    f32 distance = delta.length();

    if (distance < 0.001f) {
        eraseWithOpacityLimit(canvas, brush, to, flow, strokeOpacity,
                              strokeAlphaMap, selection);
        return;
    }

    f32 step = std::max(1.0f, brush.size * spacing);
    i32 steps = static_cast<i32>(distance / step);

    for (i32 i = 0; i <= steps; ++i) {
        f32 t = (steps > 0) ? static_cast<f32>(i) / steps : 1.0f;
        Vec2 pos = from + delta * t;
        eraseWithOpacityLimit(canvas, brush, pos, flow, strokeOpacity,
                              strokeAlphaMap, selection);
    }
}

void pencilPixelWithOpacityLimit(TiledCanvas& canvas, i32 x, i32 y,
                                 u32 color, f32 flow, f32 strokeOpacity,
                                 std::unordered_map<u64, f32>& strokeAlphaMap,
                                 const Selection* selection) {
    // Check selection mask
    if (selection && selection->hasSelection) {
        if (x < 0 || y < 0 ||
            x >= static_cast<i32>(selection->width) ||
            y >= static_cast<i32>(selection->height)) return;
        if (selection->getValue(x, y) == 0) return;
    }

    u8 cr, cg, cb, ca;
    Blend::unpack(color, cr, cg, cb, ca);

    f32 dabAlpha = flow * (ca / 255.0f);

    u64 key = packCoords(x, y);
    f32& currentAlpha = strokeAlphaMap[key];

    if (currentAlpha < strokeOpacity) {
        f32 remaining = strokeOpacity - currentAlpha;
        f32 applyAlpha = std::min(dabAlpha, remaining);

        u8 newAlpha = static_cast<u8>(std::min(255.0f, applyAlpha * 255.0f));
        u32 finalColor = Blend::pack(cr, cg, cb, newAlpha);

        canvas.alphaBlendPixel(x, y, finalColor);
        currentAlpha += applyAlpha;
    }
}

void pencilLineWithOpacityLimit(TiledCanvas& canvas, i32 x0, i32 y0, i32 x1, i32 y1,
                                u32 color, f32 flow, f32 strokeOpacity,
                                std::unordered_map<u64, f32>& strokeAlphaMap,
                                const Selection* selection) {
    // Bresenham's line algorithm
    i32 dx = std::abs(x1 - x0);
    i32 dy = -std::abs(y1 - y0);
    i32 sx = x0 < x1 ? 1 : -1;
    i32 sy = y0 < y1 ? 1 : -1;
    i32 err = dx + dy;

    while (true) {
        pencilPixelWithOpacityLimit(canvas, x0, y0, color, flow, strokeOpacity,
                                    strokeAlphaMap, selection);

        if (x0 == x1 && y0 == y1) break;

        i32 e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void pencilEraseWithOpacityLimit(TiledCanvas& canvas, i32 x, i32 y,
                                 f32 flow, f32 strokeOpacity,
                                 std::unordered_map<u64, f32>& strokeAlphaMap,
                                 const Selection* selection) {
    // Check selection mask
    if (selection && selection->hasSelection) {
        if (x < 0 || y < 0 ||
            x >= static_cast<i32>(selection->width) ||
            y >= static_cast<i32>(selection->height)) return;
        if (selection->getValue(x, y) == 0) return;
    }

    u64 key = packCoords(x, y);
    f32& currentAlpha = strokeAlphaMap[key];

    if (currentAlpha < strokeOpacity) {
        f32 remaining = strokeOpacity - currentAlpha;
        f32 applyAlpha = std::min(flow, remaining);

        u32 pixel = canvas.getPixel(x, y);
        u8 r, g, b, a;
        Blend::unpack(pixel, r, g, b, a);

        f32 newAlpha = a * (1.0f - applyAlpha);
        a = static_cast<u8>(std::max(0.0f, newAlpha));

        canvas.setPixel(x, y, Blend::pack(r, g, b, a));
        currentAlpha += applyAlpha;
    }
}

void pencilEraseLineWithOpacityLimit(TiledCanvas& canvas, i32 x0, i32 y0, i32 x1, i32 y1,
                                     f32 flow, f32 strokeOpacity,
                                     std::unordered_map<u64, f32>& strokeAlphaMap,
                                     const Selection* selection) {
    // Bresenham's line algorithm
    i32 dx = std::abs(x1 - x0);
    i32 dy = -std::abs(y1 - y0);
    i32 sx = x0 < x1 ? 1 : -1;
    i32 sy = y0 < y1 ? 1 : -1;
    i32 err = dx + dy;

    while (true) {
        pencilEraseWithOpacityLimit(canvas, x0, y0, flow, strokeOpacity,
                                    strokeAlphaMap, selection);

        if (x0 == x1 && y0 == y1) break;

        i32 e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

// Custom brush tip functions
static std::mt19937 rng(std::random_device{}());
static std::uniform_real_distribution<f32> uniformDist(0.0f, 1.0f);

f32 randomFloat() {
    return uniformDist(rng);
}

static CachedCustomStamp stampCache;

CachedCustomStamp& getStampCache() {
    return stampCache;
}

f32 sampleTipBilinear(const CustomBrushTip& tip, f32 x, f32 y) {
    if (x < 0 || y < 0 || x >= tip.width - 1 || y >= tip.height - 1) {
        // Clamp to edge
        x = clamp(x, 0.0f, static_cast<f32>(tip.width - 1));
        y = clamp(y, 0.0f, static_cast<f32>(tip.height - 1));
    }

    i32 x0 = static_cast<i32>(x);
    i32 y0 = static_cast<i32>(y);
    i32 x1 = std::min(x0 + 1, static_cast<i32>(tip.width - 1));
    i32 y1 = std::min(y0 + 1, static_cast<i32>(tip.height - 1));

    f32 fx = x - x0;
    f32 fy = y - y0;

    f32 v00 = tip.getAlpha(x0, y0);
    f32 v10 = tip.getAlpha(x1, y0);
    f32 v01 = tip.getAlpha(x0, y1);
    f32 v11 = tip.getAlpha(x1, y1);

    f32 v0 = v00 * (1 - fx) + v10 * fx;
    f32 v1 = v01 * (1 - fx) + v11 * fx;

    return v0 * (1 - fy) + v1 * fy;
}

BrushStamp generateStampFromTip(const CustomBrushTip& tip,
                                f32 diameter, f32 angleDegrees) {
    u32 size = static_cast<u32>(std::ceil(diameter));
    if (size < 1) size = 1;

    BrushStamp stamp(size);

    f32 scale = diameter / static_cast<f32>(std::max(tip.width, tip.height));
    f32 centerStamp = (size - 1) / 2.0f;
    f32 centerTipX = (tip.width - 1) / 2.0f;
    f32 centerTipY = (tip.height - 1) / 2.0f;

    // Rotation
    constexpr f32 PI = 3.14159265358979323846f;
    f32 rad = -angleDegrees * (PI / 180.0f);  // Negative for clockwise rotation
    f32 cosA = std::cos(rad);
    f32 sinA = std::sin(rad);

    for (u32 sy = 0; sy < size; ++sy) {
        for (u32 sx = 0; sx < size; ++sx) {
            // Position relative to stamp center
            f32 dx = sx - centerStamp;
            f32 dy = sy - centerStamp;

            // Rotate and scale to tip coordinates
            f32 tx = (dx * cosA - dy * sinA) / scale + centerTipX;
            f32 ty = (dx * sinA + dy * cosA) / scale + centerTipY;

            // Sample tip with bilinear interpolation
            if (tx >= 0 && tx < tip.width && ty >= 0 && ty < tip.height) {
                f32 alpha = sampleTipBilinear(tip, tx, ty);
                stamp.setAlpha(sx, sy, alpha);
            }
        }
    }

    return stamp;
}

const BrushStamp& getCachedStampFromTip(const CustomBrushTip& tip,
                                        f32 diameter, f32 angleDegrees) {
    if (!stampCache.matches(&tip, diameter, angleDegrees)) {
        stampCache.stamp = generateStampFromTip(tip, diameter, angleDegrees);
        stampCache.tip = &tip;
        stampCache.size = diameter;
        stampCache.angle = angleDegrees;
        stampCache.valid = true;
    }
    return stampCache.stamp;
}

void strokeLineToBufferWithDynamics(
    TiledCanvas& buffer,
    const BrushStamp& baseStamp,
    const CustomBrushTip* tip,
    const Vec2& from, const Vec2& to,
    u32 color, f32 flow, f32 spacing,
    f32 baseSize, f32 baseAngle, f32 hardness,
    const BrushDynamics& dynamics,
    BlendMode mode, const Selection* selection) {

    Vec2 delta = to - from;
    f32 distance = delta.length();

    if (distance < 0.001f) {
        stampToBufferWithDynamics(buffer, baseStamp, tip, to, color, flow,
                                  baseSize, baseAngle, hardness, dynamics, mode, selection);
        return;
    }

    f32 step = std::max(1.0f, baseSize * spacing);
    i32 steps = static_cast<i32>(distance / step);

    // Calculate perpendicular for scattering
    Vec2 dir = delta.normalized();
    Vec2 perp(-dir.y, dir.x);

    for (i32 i = 0; i <= steps; ++i) {
        f32 t = (steps > 0) ? static_cast<f32>(i) / steps : 1.0f;
        Vec2 pos = from + delta * t;

        // Apply scattering
        if (dynamics.scatterAmount > 0) {
            f32 scatter = (randomFloat() * 2.0f - 1.0f) * dynamics.scatterAmount * baseSize;
            pos = pos + perp * scatter;

            if (dynamics.scatterBothAxes) {
                f32 scatter2 = (randomFloat() * 2.0f - 1.0f) * dynamics.scatterAmount * baseSize;
                pos = pos + dir * scatter2;
            }
        }

        stampToBufferWithDynamics(buffer, baseStamp, tip, pos, color, flow,
                                  baseSize, baseAngle, hardness, dynamics, mode, selection);
    }
}

void stampToBufferWithDynamics(
    TiledCanvas& buffer,
    const BrushStamp& baseStamp,
    const CustomBrushTip* tip,
    const Vec2& pos,
    u32 color, f32 flow,
    f32 baseSize, f32 baseAngle, f32 hardness,
    const BrushDynamics& dynamics,
    BlendMode mode, const Selection* selection) {

    // Apply size jitter
    f32 size = baseSize;
    if (dynamics.sizeJitter > 0) {
        f32 jitterRange = dynamics.sizeJitter * baseSize;
        f32 minSize = baseSize * dynamics.sizeJitterMin;
        size = minSize + randomFloat() * (baseSize - minSize);
    }

    // Apply angle jitter
    f32 angle = baseAngle;
    if (dynamics.angleJitter > 0) {
        angle = baseAngle + (randomFloat() * 2.0f - 1.0f) * dynamics.angleJitter;
    }

    // Generate or get stamp for this size/angle
    const BrushStamp* stamp = &baseStamp;
    BrushStamp dynamicStamp;

    if (size != baseSize || angle != baseAngle) {
        if (tip) {
            dynamicStamp = generateStampFromTip(*tip, size, angle);
        } else {
            dynamicStamp = generateStamp(size, hardness);
        }
        stamp = &dynamicStamp;
    }

    // Use the stamp
    stampToBuffer(buffer, *stamp, pos, color, flow, mode, selection);
}

} // namespace BrushRenderer
