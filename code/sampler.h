#ifndef _H_SAMPLER_
#define _H_SAMPLER_

#include "types.h"
#include "primitives.h"
#include "tiled_canvas.h"
#include "blend.h"
#include <cmath>

enum class SampleMode {
    Nearest,
    Bilinear,
    Bicubic
};

namespace Sampler {
    // Sample a single pixel using nearest neighbor
    inline u32 sampleNearest(const TiledCanvas& canvas, f32 x, f32 y) {
        i32 ix = static_cast<i32>(std::floor(x));
        i32 iy = static_cast<i32>(std::floor(y));
        // No bounds check - canvas returns 0 (transparent) for non-existent tiles
        return canvas.getPixel(ix, iy);
    }

    // Bilinear interpolation (transparent outside bounds)
    inline u32 sampleBilinear(const TiledCanvas& canvas, f32 x, f32 y) {
        f32 fx = std::floor(x);
        f32 fy = std::floor(y);
        f32 tx = x - fx;
        f32 ty = y - fy;

        i32 x0 = static_cast<i32>(fx);
        i32 y0 = static_cast<i32>(fy);
        i32 x1 = x0 + 1;
        i32 y1 = y0 + 1;

        i32 w = static_cast<i32>(canvas.width);
        i32 h = static_cast<i32>(canvas.height);

        // Helper to get pixel or transparent if out of bounds
        auto getPixelSafe = [&](i32 px, i32 py) -> u32 {
            if (px < 0 || py < 0 || px >= w || py >= h) return 0;
            return canvas.getPixel(px, py);
        };

        // Get four corner pixels (transparent if out of bounds)
        u32 c00 = getPixelSafe(x0, y0);
        u32 c10 = getPixelSafe(x1, y0);
        u32 c01 = getPixelSafe(x0, y1);
        u32 c11 = getPixelSafe(x1, y1);

        // Interpolate each channel
        u8 r00, g00, b00, a00;
        u8 r10, g10, b10, a10;
        u8 r01, g01, b01, a01;
        u8 r11, g11, b11, a11;
        Blend::unpack(c00, r00, g00, b00, a00);
        Blend::unpack(c10, r10, g10, b10, a10);
        Blend::unpack(c01, r01, g01, b01, a01);
        Blend::unpack(c11, r11, g11, b11, a11);

        f32 invTx = 1.0f - tx;
        f32 invTy = 1.0f - ty;

        auto interpChannel = [&](u8 v00, u8 v10, u8 v01, u8 v11) -> u8 {
            f32 top = v00 * invTx + v10 * tx;
            f32 bottom = v01 * invTx + v11 * tx;
            return static_cast<u8>(clamp(top * invTy + bottom * ty, 0.0f, 255.0f));
        };

        return Blend::pack(
            interpChannel(r00, r10, r01, r11),
            interpChannel(g00, g10, g01, g11),
            interpChannel(b00, b10, b01, b11),
            interpChannel(a00, a10, a01, a11)
        );
    }

    // Bicubic interpolation weight function
    inline f32 cubicWeight(f32 t) {
        f32 at = std::abs(t);
        if (at <= 1.0f) {
            return (1.5f * at - 2.5f) * at * at + 1.0f;
        } else if (at < 2.0f) {
            return ((-0.5f * at + 2.5f) * at - 4.0f) * at + 2.0f;
        }
        return 0.0f;
    }

    inline u32 sampleBicubic(const TiledCanvas& canvas, f32 x, f32 y) {
        i32 ix = static_cast<i32>(std::floor(x));
        i32 iy = static_cast<i32>(std::floor(y));
        f32 fx = x - ix;
        f32 fy = y - iy;

        i32 w = static_cast<i32>(canvas.width);
        i32 h = static_cast<i32>(canvas.height);

        f32 r = 0, g = 0, b = 0, a = 0;

        for (i32 dy = -1; dy <= 2; ++dy) {
            f32 wy = cubicWeight(fy - dy);
            for (i32 dx = -1; dx <= 2; ++dx) {
                f32 wx = cubicWeight(fx - dx);
                f32 wt = wx * wy;

                i32 sx = ix + dx;
                i32 sy = iy + dy;

                // Return transparent for out-of-bounds pixels
                u32 pixel = 0;
                if (sx >= 0 && sy >= 0 && sx < w && sy < h) {
                    pixel = canvas.getPixel(sx, sy);
                }

                u8 pr, pg, pb, pa;
                Blend::unpack(pixel, pr, pg, pb, pa);

                r += pr * wt;
                g += pg * wt;
                b += pb * wt;
                a += pa * wt;
            }
        }

        return Blend::pack(
            static_cast<u8>(clamp(r, 0.0f, 255.0f)),
            static_cast<u8>(clamp(g, 0.0f, 255.0f)),
            static_cast<u8>(clamp(b, 0.0f, 255.0f)),
            static_cast<u8>(clamp(a, 0.0f, 255.0f))
        );
    }

    // Main sampling function
    inline u32 sample(const TiledCanvas& canvas, f32 x, f32 y, SampleMode mode) {
        switch (mode) {
            case SampleMode::Nearest:  return sampleNearest(canvas, x, y);
            case SampleMode::Bilinear: return sampleBilinear(canvas, x, y);
            case SampleMode::Bicubic:  return sampleBicubic(canvas, x, y);
        }
        return sampleNearest(canvas, x, y);
    }
}

#endif
