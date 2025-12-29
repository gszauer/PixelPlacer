#ifndef _H_BRUSH_RENDERER_
#define _H_BRUSH_RENDERER_

#include "types.h"
#include "primitives.h"
#include "tiled_canvas.h"
#include "selection.h"
#include "blend.h"
#include "brush_tip.h"
#include <vector>
#include <unordered_map>

namespace BrushRenderer {
    // Brush stamp - precomputed alpha values for a circular brush
    struct BrushStamp {
        std::vector<f32> alpha;  // Normalized 0-1 alpha values
        u32 size = 0;            // Diameter in pixels

        BrushStamp() = default;
        BrushStamp(u32 s) : size(s), alpha(s * s, 0.0f) {}

        f32 getAlpha(u32 x, u32 y) const {
            if (x >= size || y >= size) return 0.0f;
            return alpha[y * size + x];
        }

        void setAlpha(u32 x, u32 y, f32 a) {
            if (x >= size || y >= size) return;
            alpha[y * size + x] = a;
        }
    };

    // Pack coordinates into u64 key for stroke alpha map
    inline u64 packCoords(i32 x, i32 y) {
        return (static_cast<u64>(static_cast<u32>(y)) << 32) |
                static_cast<u64>(static_cast<u32>(x));
    }

    // Generate a circular brush stamp
    BrushStamp generateStamp(f32 diameter, f32 hardness);

    // Apply brush stamp to canvas at position
    void stamp(TiledCanvas& canvas, const BrushStamp& brush,
               const Vec2& pos, u32 color, f32 opacity,
               BlendMode mode = BlendMode::Normal,
               const Selection* selection = nullptr);

    // Stamp to stroke buffer with flow
    void stampToBuffer(TiledCanvas& buffer, const BrushStamp& brush,
                       const Vec2& pos, u32 color, f32 flow,
                       BlendMode mode = BlendMode::Normal,
                       const Selection* selection = nullptr,
                       const Matrix3x2* layerToDoc = nullptr);

    // Stroke line to buffer with flow
    void strokeLineToBuffer(TiledCanvas& buffer, const BrushStamp& brush,
                            const Vec2& from, const Vec2& to, u32 color,
                            f32 flow, f32 spacing,
                            BlendMode mode = BlendMode::Normal,
                            const Selection* selection = nullptr,
                            const Matrix3x2* layerToDoc = nullptr);

    // Composite stroke buffer onto layer canvas with opacity
    void compositeStrokeToLayer(TiledCanvas& layer, const TiledCanvas& stroke,
                                f32 opacity, BlendMode mode = BlendMode::Normal);

    // Erase functions
    void eraseStampToBuffer(TiledCanvas& buffer, const BrushStamp& brush,
                            const Vec2& pos, f32 flow,
                            const Selection* selection = nullptr,
                            const Matrix3x2* layerToDoc = nullptr);

    void eraseLineToBuffer(TiledCanvas& buffer, const BrushStamp& brush,
                           const Vec2& from, const Vec2& to, f32 flow, f32 spacing,
                           const Selection* selection = nullptr,
                           const Matrix3x2* layerToDoc = nullptr);

    void compositeEraseBufferToLayer(TiledCanvas& layer, const TiledCanvas& eraseBuffer,
                                     f32 opacity);

    void erase(TiledCanvas& canvas, const BrushStamp& brush,
               const Vec2& pos, f32 opacity,
               const Selection* selection = nullptr);

    // Interpolate brush strokes between two points
    void strokeLine(TiledCanvas& canvas, const BrushStamp& brush,
                    const Vec2& from, const Vec2& to, u32 color, f32 opacity,
                    f32 spacing, BlendMode mode = BlendMode::Normal,
                    const Selection* selection = nullptr);

    void eraseLineTool(TiledCanvas& canvas, const BrushStamp& brush,
                       const Vec2& from, const Vec2& to, f32 opacity, f32 spacing,
                       const Selection* selection = nullptr);

    // Pencil mode functions
    void pencilPixel(TiledCanvas& canvas, i32 x, i32 y, u32 color, f32 opacity,
                     const Selection* selection = nullptr,
                     const Matrix3x2* layerToDoc = nullptr);

    void pencilErase(TiledCanvas& canvas, i32 x, i32 y, f32 opacity,
                     const Selection* selection = nullptr,
                     const Matrix3x2* layerToDoc = nullptr);

    void pencilLine(TiledCanvas& canvas, i32 x0, i32 y0, i32 x1, i32 y1,
                    u32 color, f32 opacity,
                    const Selection* selection = nullptr,
                    const Matrix3x2* layerToDoc = nullptr);

    void pencilEraseLine(TiledCanvas& canvas, i32 x0, i32 y0, i32 x1, i32 y1,
                         f32 opacity, const Selection* selection = nullptr,
                         const Matrix3x2* layerToDoc = nullptr);

    // Opacity-limited functions
    void stampWithOpacityLimit(TiledCanvas& canvas, const BrushStamp& brush,
                               const Vec2& pos, u32 color, f32 flow, f32 strokeOpacity,
                               std::unordered_map<u64, f32>& strokeAlphaMap,
                               BlendMode mode = BlendMode::Normal,
                               const Selection* selection = nullptr);

    void strokeLineWithOpacityLimit(TiledCanvas& canvas, const BrushStamp& brush,
                                    const Vec2& from, const Vec2& to, u32 color,
                                    f32 flow, f32 strokeOpacity, f32 spacing,
                                    std::unordered_map<u64, f32>& strokeAlphaMap,
                                    BlendMode mode = BlendMode::Normal,
                                    const Selection* selection = nullptr);

    void eraseWithOpacityLimit(TiledCanvas& canvas, const BrushStamp& brush,
                               const Vec2& pos, f32 flow, f32 strokeOpacity,
                               std::unordered_map<u64, f32>& strokeAlphaMap,
                               const Selection* selection = nullptr);

    void eraseLineWithOpacityLimit(TiledCanvas& canvas, const BrushStamp& brush,
                                   const Vec2& from, const Vec2& to,
                                   f32 flow, f32 strokeOpacity, f32 spacing,
                                   std::unordered_map<u64, f32>& strokeAlphaMap,
                                   const Selection* selection = nullptr);

    void pencilPixelWithOpacityLimit(TiledCanvas& canvas, i32 x, i32 y,
                                     u32 color, f32 flow, f32 strokeOpacity,
                                     std::unordered_map<u64, f32>& strokeAlphaMap,
                                     const Selection* selection = nullptr);

    void pencilLineWithOpacityLimit(TiledCanvas& canvas, i32 x0, i32 y0, i32 x1, i32 y1,
                                    u32 color, f32 flow, f32 strokeOpacity,
                                    std::unordered_map<u64, f32>& strokeAlphaMap,
                                    const Selection* selection = nullptr);

    void pencilEraseWithOpacityLimit(TiledCanvas& canvas, i32 x, i32 y,
                                     f32 flow, f32 strokeOpacity,
                                     std::unordered_map<u64, f32>& strokeAlphaMap,
                                     const Selection* selection = nullptr);

    void pencilEraseLineWithOpacityLimit(TiledCanvas& canvas, i32 x0, i32 y0, i32 x1, i32 y1,
                                         f32 flow, f32 strokeOpacity,
                                         std::unordered_map<u64, f32>& strokeAlphaMap,
                                         const Selection* selection = nullptr);

    // Custom brush tip functions
    f32 randomFloat();

    struct CachedCustomStamp {
        BrushStamp stamp;
        const CustomBrushTip* tip = nullptr;
        f32 size = 0.0f;
        f32 angle = 0.0f;
        bool valid = false;

        void invalidate() {
            valid = false;
            tip = nullptr;
        }

        bool matches(const CustomBrushTip* t, f32 s, f32 a) const {
            if (!valid || tip != t) return false;
            return std::abs(size - s) < 0.01f && std::abs(angle - a) < 0.01f;
        }
    };

    CachedCustomStamp& getStampCache();

    f32 sampleTipBilinear(const CustomBrushTip& tip, f32 x, f32 y);

    BrushStamp generateStampFromTip(const CustomBrushTip& tip,
                                    f32 diameter, f32 angleDegrees);

    const BrushStamp& getCachedStampFromTip(const CustomBrushTip& tip,
                                            f32 diameter, f32 angleDegrees);

    void strokeLineToBufferWithDynamics(
        TiledCanvas& buffer,
        const BrushStamp& baseStamp,
        const CustomBrushTip* tip,
        const Vec2& from, const Vec2& to,
        u32 color, f32 flow, f32 spacing,
        f32 baseSize, f32 baseAngle, f32 hardness,
        const BrushDynamics& dynamics,
        BlendMode mode = BlendMode::Normal,
        const Selection* selection = nullptr,
        const Matrix3x2* layerToDoc = nullptr);

    void stampToBufferWithDynamics(
        TiledCanvas& buffer,
        const BrushStamp& baseStamp,
        const CustomBrushTip* tip,
        const Vec2& pos,
        u32 color, f32 flow,
        f32 baseSize, f32 baseAngle, f32 hardness,
        const BrushDynamics& dynamics,
        BlendMode mode = BlendMode::Normal,
        const Selection* selection = nullptr,
        const Matrix3x2* layerToDoc = nullptr);
}

#endif
