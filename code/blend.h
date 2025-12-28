#ifndef _H_BLEND_
#define _H_BLEND_

#include "types.h"
#include "primitives.h"

enum class BlendMode {
    Normal,
    Multiply,
    Screen,
    Overlay,
    Darken,
    Lighten,
    ColorDodge,
    ColorBurn,
    HardLight,
    SoftLight,
    Difference,
    Exclusion
};

namespace Blend {
    // Extract color components from packed RGBA
    inline void unpack(u32 color, u8& r, u8& g, u8& b, u8& a) {
        r = (color >> 24) & 0xFF;
        g = (color >> 16) & 0xFF;
        b = (color >> 8) & 0xFF;
        a = color & 0xFF;
    }

    inline u32 pack(u8 r, u8 g, u8 b, u8 a) {
        return (static_cast<u32>(r) << 24) | (static_cast<u32>(g) << 16) |
               (static_cast<u32>(b) << 8) | static_cast<u32>(a);
    }

    // Porter-Duff "over" compositing
    inline u32 alphaBlend(u32 dst, u32 src) {
        u8 sr, sg, sb, sa;
        u8 dr, dg, db, da;
        unpack(src, sr, sg, sb, sa);
        unpack(dst, dr, dg, db, da);

        if (sa == 0) return dst;
        if (sa == 255) return src;

        u32 srcA = sa;
        u32 dstA = da;
        u32 outA = srcA + (dstA * (255 - srcA)) / 255;

        if (outA == 0) return 0;

        u32 outR = (sr * srcA + dr * dstA * (255 - srcA) / 255) / outA;
        u32 outG = (sg * srcA + dg * dstA * (255 - srcA) / 255) / outA;
        u32 outB = (sb * srcA + db * dstA * (255 - srcA) / 255) / outA;

        return pack(
            static_cast<u8>(std::min(outR, 255u)),
            static_cast<u8>(std::min(outG, 255u)),
            static_cast<u8>(std::min(outB, 255u)),
            static_cast<u8>(std::min(outA, 255u))
        );
    }

    // Blend mode helper functions (operate on normalized 0-1 values)
    inline f32 blendMultiply(f32 a, f32 b) { return a * b; }
    inline f32 blendScreen(f32 a, f32 b) { return 1.0f - (1.0f - a) * (1.0f - b); }
    inline f32 blendOverlay(f32 a, f32 b) {
        return a < 0.5f ? 2.0f * a * b : 1.0f - 2.0f * (1.0f - a) * (1.0f - b);
    }
    inline f32 blendDarken(f32 a, f32 b) { return std::min(a, b); }
    inline f32 blendLighten(f32 a, f32 b) { return std::max(a, b); }
    inline f32 blendColorDodge(f32 a, f32 b) {
        return b >= 1.0f ? 1.0f : std::min(1.0f, a / (1.0f - b));
    }
    inline f32 blendColorBurn(f32 a, f32 b) {
        return b <= 0.0f ? 0.0f : std::max(0.0f, 1.0f - (1.0f - a) / b);
    }
    inline f32 blendHardLight(f32 a, f32 b) {
        return b < 0.5f ? 2.0f * a * b : 1.0f - 2.0f * (1.0f - a) * (1.0f - b);
    }
    inline f32 blendSoftLight(f32 a, f32 b) {
        f32 d = (a <= 0.25f) ? ((16.0f * a - 12.0f) * a + 4.0f) * a : std::sqrt(a);
        return b < 0.5f ? a - (1.0f - 2.0f * b) * a * (1.0f - a)
                        : a + (2.0f * b - 1.0f) * (d - a);
    }
    inline f32 blendDifference(f32 a, f32 b) { return std::abs(a - b); }
    inline f32 blendExclusion(f32 a, f32 b) { return a + b - 2.0f * a * b; }

    // Apply blend mode to a single channel
    inline f32 applyBlendMode(f32 dst, f32 src, BlendMode mode) {
        switch (mode) {
            case BlendMode::Normal:     return src;
            case BlendMode::Multiply:   return blendMultiply(dst, src);
            case BlendMode::Screen:     return blendScreen(dst, src);
            case BlendMode::Overlay:    return blendOverlay(dst, src);
            case BlendMode::Darken:     return blendDarken(dst, src);
            case BlendMode::Lighten:    return blendLighten(dst, src);
            case BlendMode::ColorDodge: return blendColorDodge(dst, src);
            case BlendMode::ColorBurn:  return blendColorBurn(dst, src);
            case BlendMode::HardLight:  return blendHardLight(dst, src);
            case BlendMode::SoftLight:  return blendSoftLight(dst, src);
            case BlendMode::Difference: return blendDifference(dst, src);
            case BlendMode::Exclusion:  return blendExclusion(dst, src);
        }
        return src;
    }

    // Full blend with mode and opacity
    inline u32 blend(u32 dst, u32 src, BlendMode mode, f32 opacity) {
        u8 sr, sg, sb, sa;
        u8 dr, dg, db, da;
        unpack(src, sr, sg, sb, sa);
        unpack(dst, dr, dg, db, da);

        // Apply opacity to source alpha
        f32 srcAlpha = (sa / 255.0f) * opacity;
        if (srcAlpha <= 0.0f) return dst;

        // Normalize colors
        f32 srcR = sr / 255.0f, srcG = sg / 255.0f, srcB = sb / 255.0f;
        f32 dstR = dr / 255.0f, dstG = dg / 255.0f, dstB = db / 255.0f;

        // Apply blend mode
        f32 blendR = applyBlendMode(dstR, srcR, mode);
        f32 blendG = applyBlendMode(dstG, srcG, mode);
        f32 blendB = applyBlendMode(dstB, srcB, mode);

        // Composite result
        f32 dstAlpha = da / 255.0f;
        f32 outAlpha = srcAlpha + dstAlpha * (1.0f - srcAlpha);

        if (outAlpha <= 0.0f) return 0;

        f32 outR = (blendR * srcAlpha + dstR * dstAlpha * (1.0f - srcAlpha)) / outAlpha;
        f32 outG = (blendG * srcAlpha + dstG * dstAlpha * (1.0f - srcAlpha)) / outAlpha;
        f32 outB = (blendB * srcAlpha + dstB * dstAlpha * (1.0f - srcAlpha)) / outAlpha;

        return pack(
            static_cast<u8>(clamp(outR * 255.0f, 0.0f, 255.0f)),
            static_cast<u8>(clamp(outG * 255.0f, 0.0f, 255.0f)),
            static_cast<u8>(clamp(outB * 255.0f, 0.0f, 255.0f)),
            static_cast<u8>(clamp(outAlpha * 255.0f, 0.0f, 255.0f))
        );
    }

    // Premultiplied alpha blend (more efficient for compositing)
    inline u32 blendPremultiplied(u32 dst, u32 src) {
        u8 sr, sg, sb, sa;
        u8 dr, dg, db, da;
        unpack(src, sr, sg, sb, sa);
        unpack(dst, dr, dg, db, da);

        u32 invSrcA = 255 - sa;
        return pack(
            static_cast<u8>((sr + dr * invSrcA / 255)),
            static_cast<u8>((sg + dg * invSrcA / 255)),
            static_cast<u8>((sb + db * invSrcA / 255)),
            static_cast<u8>((sa + da * invSrcA / 255))
        );
    }
}

#endif
