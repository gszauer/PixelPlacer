#include "framebuffer.h"
#include <cstring>
#include <algorithm>

void Framebuffer::pushClip(const Recti& rect) {
    if (clipStack.empty()) {
        clipStack.push_back(rect);
    } else {
        // Intersect with current top of stack
        const Recti& current = clipStack.back();
        i32 x0 = std::max(current.x, rect.x);
        i32 y0 = std::max(current.y, rect.y);
        i32 x1 = std::min(current.x + current.w, rect.x + rect.w);
        i32 y1 = std::min(current.y + current.h, rect.y + rect.h);
        clipStack.push_back(Recti(x0, y0, std::max(0, x1 - x0), std::max(0, y1 - y0)));
    }
}

void Framebuffer::popClip() {
    if (!clipStack.empty()) {
        clipStack.pop_back();
    }
}

bool Framebuffer::isClipped(i32 x, i32 y) const {
    if (clipStack.empty()) return false;
    const Recti& clip = clipStack.back();
    return x < clip.x || x >= clip.x + clip.w ||
           y < clip.y || y >= clip.y + clip.h;
}

void Framebuffer::resize(u32 w, u32 h) {
    width = w;
    height = h;
#ifdef __EMSCRIPTEN__
    pixels.resize(w * h * 4);
#else
    pixels.resize(w * h);
#endif
}

void Framebuffer::clear(u32 color) {
#ifdef __EMSCRIPTEN__
    // WASM: Store in canvas RGBA byte order
    u8 r = (color >> 24) & 0xFF;
    u8 g = (color >> 16) & 0xFF;
    u8 b = (color >> 8) & 0xFF;
    u8 a = color & 0xFF;
    size_t count = width * height;
    for (size_t i = 0; i < count; ++i) {
        size_t idx = i * 4;
        pixels[idx + 0] = r;
        pixels[idx + 1] = g;
        pixels[idx + 2] = b;
        pixels[idx + 3] = a;
    }
#else
    std::fill(pixels.begin(), pixels.end(), color);
#endif
}

void Framebuffer::clearRect(const Recti& rect, u32 color) {
    i32 x0 = std::max(0, rect.x);
    i32 y0 = std::max(0, rect.y);
    i32 x1 = std::min(static_cast<i32>(width), rect.x + rect.w);
    i32 y1 = std::min(static_cast<i32>(height), rect.y + rect.h);

    // Apply clipping if active
    if (!clipStack.empty()) {
        const Recti& clip = clipStack.back();
        x0 = std::max(x0, clip.x);
        y0 = std::max(y0, clip.y);
        x1 = std::min(x1, clip.x + clip.w);
        y1 = std::min(y1, clip.y + clip.h);
    }

#ifdef __EMSCRIPTEN__
    u8 r = (color >> 24) & 0xFF;
    u8 g = (color >> 16) & 0xFF;
    u8 b = (color >> 8) & 0xFF;
    u8 a = color & 0xFF;
    for (i32 y = y0; y < y1; ++y) {
        for (i32 x = x0; x < x1; ++x) {
            size_t idx = (y * width + x) * 4;
            pixels[idx + 0] = r;
            pixels[idx + 1] = g;
            pixels[idx + 2] = b;
            pixels[idx + 3] = a;
        }
    }
#else
    for (i32 y = y0; y < y1; ++y) {
        for (i32 x = x0; x < x1; ++x) {
            pixels[y * width + x] = color;
        }
    }
#endif
}

u32 Framebuffer::getPixel(i32 x, i32 y) const {
    if (x < 0 || y < 0 || x >= static_cast<i32>(width) || y >= static_cast<i32>(height)) {
        return 0;
    }
#ifdef __EMSCRIPTEN__
    size_t idx = (y * width + x) * 4;
    return (static_cast<u32>(pixels[idx + 0]) << 24) |
           (static_cast<u32>(pixels[idx + 1]) << 16) |
           (static_cast<u32>(pixels[idx + 2]) << 8) |
           static_cast<u32>(pixels[idx + 3]);
#else
    return pixels[y * width + x];
#endif
}

void Framebuffer::setPixel(i32 x, i32 y, u32 color) {
    if (x < 0 || y < 0 || x >= static_cast<i32>(width) || y >= static_cast<i32>(height)) {
        return;
    }
    if (isClipped(x, y)) return;
#ifdef __EMSCRIPTEN__
    size_t idx = (y * width + x) * 4;
    pixels[idx + 0] = (color >> 24) & 0xFF;
    pixels[idx + 1] = (color >> 16) & 0xFF;
    pixels[idx + 2] = (color >> 8) & 0xFF;
    pixels[idx + 3] = color & 0xFF;
#else
    pixels[y * width + x] = color;
#endif
}

void Framebuffer::blendPixel(i32 x, i32 y, u32 color) {
    if (x < 0 || y < 0 || x >= static_cast<i32>(width) || y >= static_cast<i32>(height)) {
        return;
    }
    if (isClipped(x, y)) return;
#ifdef __EMSCRIPTEN__
    size_t idx = (y * width + x) * 4;
    u32 dst = (static_cast<u32>(pixels[idx + 0]) << 24) |
              (static_cast<u32>(pixels[idx + 1]) << 16) |
              (static_cast<u32>(pixels[idx + 2]) << 8) |
              static_cast<u32>(pixels[idx + 3]);
    u32 result = Blend::alphaBlend(dst, color);
    pixels[idx + 0] = (result >> 24) & 0xFF;
    pixels[idx + 1] = (result >> 16) & 0xFF;
    pixels[idx + 2] = (result >> 8) & 0xFF;
    pixels[idx + 3] = result & 0xFF;
#else
    u32 dst = pixels[y * width + x];
    pixels[y * width + x] = Blend::alphaBlend(dst, color);
#endif
}

void Framebuffer::fillRect(const Recti& rect, u32 color) {
    i32 x0 = std::max(0, rect.x);
    i32 y0 = std::max(0, rect.y);
    i32 x1 = std::min(static_cast<i32>(width), rect.x + rect.w);
    i32 y1 = std::min(static_cast<i32>(height), rect.y + rect.h);

    // Apply clipping if active
    if (!clipStack.empty()) {
        const Recti& clip = clipStack.back();
        x0 = std::max(x0, clip.x);
        y0 = std::max(y0, clip.y);
        x1 = std::min(x1, clip.x + clip.w);
        y1 = std::min(y1, clip.y + clip.h);
    }

    u8 alpha = color & 0xFF;
#ifdef __EMSCRIPTEN__
    u8 r = (color >> 24) & 0xFF;
    u8 g = (color >> 16) & 0xFF;
    u8 b = (color >> 8) & 0xFF;
    if (alpha == 255) {
        for (i32 y = y0; y < y1; ++y) {
            for (i32 x = x0; x < x1; ++x) {
                size_t idx = (y * width + x) * 4;
                pixels[idx + 0] = r;
                pixels[idx + 1] = g;
                pixels[idx + 2] = b;
                pixels[idx + 3] = alpha;
            }
        }
    } else if (alpha > 0) {
        for (i32 y = y0; y < y1; ++y) {
            for (i32 x = x0; x < x1; ++x) {
                size_t idx = (y * width + x) * 4;
                u32 dst = (static_cast<u32>(pixels[idx + 0]) << 24) |
                          (static_cast<u32>(pixels[idx + 1]) << 16) |
                          (static_cast<u32>(pixels[idx + 2]) << 8) |
                          static_cast<u32>(pixels[idx + 3]);
                u32 result = Blend::alphaBlend(dst, color);
                pixels[idx + 0] = (result >> 24) & 0xFF;
                pixels[idx + 1] = (result >> 16) & 0xFF;
                pixels[idx + 2] = (result >> 8) & 0xFF;
                pixels[idx + 3] = result & 0xFF;
            }
        }
    }
#else
    if (alpha == 255) {
        for (i32 y = y0; y < y1; ++y) {
            for (i32 x = x0; x < x1; ++x) {
                pixels[y * width + x] = color;
            }
        }
    } else if (alpha > 0) {
        for (i32 y = y0; y < y1; ++y) {
            for (i32 x = x0; x < x1; ++x) {
                pixels[y * width + x] = Blend::alphaBlend(pixels[y * width + x], color);
            }
        }
    }
#endif
}

void Framebuffer::fillRect(i32 x, i32 y, i32 w, i32 h, u32 color) {
    fillRect(Recti(x, y, w, h), color);
}

void Framebuffer::drawRect(const Recti& rect, u32 color, i32 thickness) {
    // Top
    fillRect(Recti(rect.x, rect.y, rect.w, thickness), color);
    // Bottom
    fillRect(Recti(rect.x, rect.y + rect.h - thickness, rect.w, thickness), color);
    // Left
    fillRect(Recti(rect.x, rect.y + thickness, thickness, rect.h - 2 * thickness), color);
    // Right
    fillRect(Recti(rect.x + rect.w - thickness, rect.y + thickness, thickness, rect.h - 2 * thickness), color);
}

void Framebuffer::drawLine(i32 x0, i32 y0, i32 x1, i32 y1, u32 color) {
    // Bresenham's line algorithm
    i32 dx = std::abs(x1 - x0);
    i32 dy = -std::abs(y1 - y0);
    i32 sx = x0 < x1 ? 1 : -1;
    i32 sy = y0 < y1 ? 1 : -1;
    i32 err = dx + dy;

    while (true) {
        blendPixel(x0, y0, color);
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

void Framebuffer::drawLine(const Vec2& from, const Vec2& to, u32 color) {
    drawLine(static_cast<i32>(from.x), static_cast<i32>(from.y),
             static_cast<i32>(to.x), static_cast<i32>(to.y), color);
}

void Framebuffer::drawCircle(i32 cx, i32 cy, i32 radius, u32 color, i32 thickness) {
    // Draw multiple concentric circles for thickness
    for (i32 t = 0; t < thickness; ++t) {
        i32 r = radius - t;
        if (r <= 0) break;
        drawCircleSingle(cx, cy, r, color);
    }
}

void Framebuffer::drawCircleSingle(i32 cx, i32 cy, i32 radius, u32 color) {
    // Midpoint circle algorithm
    i32 x = radius;
    i32 y = 0;
    i32 err = 0;

    while (x >= y) {
        blendPixel(cx + x, cy + y, color);
        blendPixel(cx + y, cy + x, color);
        blendPixel(cx - y, cy + x, color);
        blendPixel(cx - x, cy + y, color);
        blendPixel(cx - x, cy - y, color);
        blendPixel(cx - y, cy - x, color);
        blendPixel(cx + y, cy - x, color);
        blendPixel(cx + x, cy - y, color);

        y++;
        err += 1 + 2 * y;
        if (2 * (err - x) + 1 > 0) {
            x--;
            err += 1 - 2 * x;
        }
    }
}

void Framebuffer::fillCircle(i32 cx, i32 cy, i32 radius, u32 color) {
    i32 x = radius;
    i32 y = 0;
    i32 err = 0;

    while (x >= y) {
        drawHorizontalLine(cx - x, cx + x, cy + y, color);
        drawHorizontalLine(cx - x, cx + x, cy - y, color);
        drawHorizontalLine(cx - y, cx + y, cy + x, color);
        drawHorizontalLine(cx - y, cx + y, cy - x, color);

        y++;
        err += 1 + 2 * y;
        if (2 * (err - x) + 1 > 0) {
            x--;
            err += 1 - 2 * x;
        }
    }
}

void Framebuffer::drawHorizontalLine(i32 x0, i32 x1, i32 y, u32 color) {
    if (y < 0 || y >= static_cast<i32>(height)) return;
    if (x0 > x1) std::swap(x0, x1);
    x0 = std::max(0, x0);
    x1 = std::min(static_cast<i32>(width) - 1, x1);

    // Apply clipping if active
    if (!clipStack.empty()) {
        const Recti& clip = clipStack.back();
        if (y < clip.y || y >= clip.y + clip.h) return;
        x0 = std::max(x0, clip.x);
        x1 = std::min(x1, clip.x + clip.w - 1);
    }

    if (x0 > x1) return;

    u8 alpha = color & 0xFF;
#ifdef __EMSCRIPTEN__
    u8 r = (color >> 24) & 0xFF;
    u8 g = (color >> 16) & 0xFF;
    u8 b = (color >> 8) & 0xFF;
    if (alpha == 255) {
        for (i32 x = x0; x <= x1; ++x) {
            size_t idx = (y * width + x) * 4;
            pixels[idx + 0] = r;
            pixels[idx + 1] = g;
            pixels[idx + 2] = b;
            pixels[idx + 3] = alpha;
        }
    } else if (alpha > 0) {
        for (i32 x = x0; x <= x1; ++x) {
            size_t idx = (y * width + x) * 4;
            u32 dst = (static_cast<u32>(pixels[idx + 0]) << 24) |
                      (static_cast<u32>(pixels[idx + 1]) << 16) |
                      (static_cast<u32>(pixels[idx + 2]) << 8) |
                      static_cast<u32>(pixels[idx + 3]);
            u32 result = Blend::alphaBlend(dst, color);
            pixels[idx + 0] = (result >> 24) & 0xFF;
            pixels[idx + 1] = (result >> 16) & 0xFF;
            pixels[idx + 2] = (result >> 8) & 0xFF;
            pixels[idx + 3] = result & 0xFF;
        }
    }
#else
    if (alpha == 255) {
        for (i32 x = x0; x <= x1; ++x) {
            pixels[y * width + x] = color;
        }
    } else if (alpha > 0) {
        for (i32 x = x0; x <= x1; ++x) {
            pixels[y * width + x] = Blend::alphaBlend(pixels[y * width + x], color);
        }
    }
#endif
}

void Framebuffer::drawVerticalLine(i32 x, i32 y0, i32 y1, u32 color) {
    if (x < 0 || x >= static_cast<i32>(width)) return;
    if (y0 > y1) std::swap(y0, y1);
    y0 = std::max(0, y0);
    y1 = std::min(static_cast<i32>(height) - 1, y1);

    // Apply clipping if active
    if (!clipStack.empty()) {
        const Recti& clip = clipStack.back();
        if (x < clip.x || x >= clip.x + clip.w) return;
        y0 = std::max(y0, clip.y);
        y1 = std::min(y1, clip.y + clip.h - 1);
    }

    if (y0 > y1) return;

    u8 alpha = color & 0xFF;
#ifdef __EMSCRIPTEN__
    u8 r = (color >> 24) & 0xFF;
    u8 g = (color >> 16) & 0xFF;
    u8 b = (color >> 8) & 0xFF;
    if (alpha == 255) {
        for (i32 y = y0; y <= y1; ++y) {
            size_t idx = (y * width + x) * 4;
            pixels[idx + 0] = r;
            pixels[idx + 1] = g;
            pixels[idx + 2] = b;
            pixels[idx + 3] = alpha;
        }
    } else if (alpha > 0) {
        for (i32 y = y0; y <= y1; ++y) {
            size_t idx = (y * width + x) * 4;
            u32 dst = (static_cast<u32>(pixels[idx + 0]) << 24) |
                      (static_cast<u32>(pixels[idx + 1]) << 16) |
                      (static_cast<u32>(pixels[idx + 2]) << 8) |
                      static_cast<u32>(pixels[idx + 3]);
            u32 result = Blend::alphaBlend(dst, color);
            pixels[idx + 0] = (result >> 24) & 0xFF;
            pixels[idx + 1] = (result >> 16) & 0xFF;
            pixels[idx + 2] = (result >> 8) & 0xFF;
            pixels[idx + 3] = result & 0xFF;
        }
    }
#else
    if (alpha == 255) {
        for (i32 y = y0; y <= y1; ++y) {
            pixels[y * width + x] = color;
        }
    } else if (alpha > 0) {
        for (i32 y = y0; y <= y1; ++y) {
            pixels[y * width + x] = Blend::alphaBlend(pixels[y * width + x], color);
        }
    }
#endif
}

void Framebuffer::drawCheckerboard(const Recti& rect, u32 color1, u32 color2, u32 size) {
    i32 x0 = std::max(0, rect.x);
    i32 y0 = std::max(0, rect.y);
    i32 x1 = std::min(static_cast<i32>(width), rect.x + rect.w);
    i32 y1 = std::min(static_cast<i32>(height), rect.y + rect.h);

    // Apply clipping if active
    if (!clipStack.empty()) {
        const Recti& clip = clipStack.back();
        x0 = std::max(x0, clip.x);
        y0 = std::max(y0, clip.y);
        x1 = std::min(x1, clip.x + clip.w);
        y1 = std::min(y1, clip.y + clip.h);
    }

    i32 checkerSize = static_cast<i32>(size);

#ifdef __EMSCRIPTEN__
    // WASM: Store in canvas RGBA byte order with row-wise optimization
    u8 r1 = (color1 >> 24) & 0xFF, g1 = (color1 >> 16) & 0xFF;
    u8 b1 = (color1 >> 8) & 0xFF, a1 = color1 & 0xFF;
    u8 r2 = (color2 >> 24) & 0xFF, g2 = (color2 >> 16) & 0xFF;
    u8 b2 = (color2 >> 8) & 0xFF, a2 = color2 & 0xFF;

    for (i32 y = y0; y < y1; ++y) {
        i32 rowChecker = (y / checkerSize) & 1;
        // Fill in checker-sized runs for better cache utilization
        for (i32 x = x0; x < x1; ) {
            i32 colChecker = (x / checkerSize) & 1;
            bool useColor1 = (rowChecker ^ colChecker) != 0;
            u8 r = useColor1 ? r1 : r2;
            u8 g = useColor1 ? g1 : g2;
            u8 b = useColor1 ? b1 : b2;
            u8 a = useColor1 ? a1 : a2;

            // Calculate run length to next checker boundary
            i32 nextBoundary = ((x / checkerSize) + 1) * checkerSize;
            i32 runEnd = std::min(nextBoundary, x1);

            // Fill the run
            for (i32 rx = x; rx < runEnd; ++rx) {
                size_t idx = (y * width + rx) * 4;
                pixels[idx + 0] = r;
                pixels[idx + 1] = g;
                pixels[idx + 2] = b;
                pixels[idx + 3] = a;
            }
            x = runEnd;
        }
    }
#else
    // Native: Optimized row-wise filling
    for (i32 y = y0; y < y1; ++y) {
        i32 rowChecker = (y / checkerSize) & 1;
        u32* row = pixels.data() + y * width;

        // Fill in checker-sized runs for better cache utilization
        for (i32 x = x0; x < x1; ) {
            i32 colChecker = (x / checkerSize) & 1;
            u32 color = (rowChecker ^ colChecker) ? color1 : color2;

            // Calculate run length to next checker boundary
            i32 nextBoundary = ((x / checkerSize) + 1) * checkerSize;
            i32 runEnd = std::min(nextBoundary, x1);

            // Fill run with std::fill for better optimization
            std::fill(row + x, row + runEnd, color);
            x = runEnd;
        }
    }
#endif
}

void Framebuffer::blit(const Framebuffer& src, i32 dx, i32 dy) {
    blit(src, dx, dy, Recti(0, 0, src.width, src.height));
}

void Framebuffer::blit(const Framebuffer& src, i32 dx, i32 dy, const Recti& srcRect) {
    i32 sx0 = std::max(0, srcRect.x);
    i32 sy0 = std::max(0, srcRect.y);
    i32 sx1 = std::min(static_cast<i32>(src.width), srcRect.x + srcRect.w);
    i32 sy1 = std::min(static_cast<i32>(src.height), srcRect.y + srcRect.h);

    for (i32 sy = sy0; sy < sy1; ++sy) {
        i32 ry = dy + (sy - srcRect.y);
        if (ry < 0 || ry >= static_cast<i32>(height)) continue;

        for (i32 sx = sx0; sx < sx1; ++sx) {
            i32 rx = dx + (sx - srcRect.x);
            if (rx < 0 || rx >= static_cast<i32>(width)) continue;

#ifdef __EMSCRIPTEN__
            size_t srcIdx = (sy * src.width + sx) * 4;
            size_t dstIdx = (ry * width + rx) * 4;
            pixels[dstIdx + 0] = src.pixels[srcIdx + 0];
            pixels[dstIdx + 1] = src.pixels[srcIdx + 1];
            pixels[dstIdx + 2] = src.pixels[srcIdx + 2];
            pixels[dstIdx + 3] = src.pixels[srcIdx + 3];
#else
            u32 srcPixel = src.pixels[sy * src.width + sx];
            pixels[ry * width + rx] = srcPixel;
#endif
        }
    }
}

void Framebuffer::blitBlend(const Framebuffer& src, i32 dx, i32 dy) {
    for (u32 sy = 0; sy < src.height; ++sy) {
        i32 ry = dy + sy;
        if (ry < 0 || ry >= static_cast<i32>(height)) continue;

        for (u32 sx = 0; sx < src.width; ++sx) {
            i32 rx = dx + sx;
            if (rx < 0 || rx >= static_cast<i32>(width)) continue;

#ifdef __EMSCRIPTEN__
            size_t srcIdx = (sy * src.width + sx) * 4;
            size_t dstIdx = (ry * width + rx) * 4;
            u32 srcPixel = (static_cast<u32>(src.pixels[srcIdx + 0]) << 24) |
                           (static_cast<u32>(src.pixels[srcIdx + 1]) << 16) |
                           (static_cast<u32>(src.pixels[srcIdx + 2]) << 8) |
                           static_cast<u32>(src.pixels[srcIdx + 3]);
            u32 dstPixel = (static_cast<u32>(pixels[dstIdx + 0]) << 24) |
                           (static_cast<u32>(pixels[dstIdx + 1]) << 16) |
                           (static_cast<u32>(pixels[dstIdx + 2]) << 8) |
                           static_cast<u32>(pixels[dstIdx + 3]);
            u32 result = Blend::alphaBlend(dstPixel, srcPixel);
            pixels[dstIdx + 0] = (result >> 24) & 0xFF;
            pixels[dstIdx + 1] = (result >> 16) & 0xFF;
            pixels[dstIdx + 2] = (result >> 8) & 0xFF;
            pixels[dstIdx + 3] = result & 0xFF;
#else
            u32 srcPixel = src.pixels[sy * src.width + sx];
            u32& dstPixel = pixels[ry * width + rx];
            dstPixel = Blend::alphaBlend(dstPixel, srcPixel);
#endif
        }
    }
}
