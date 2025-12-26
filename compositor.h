#ifndef _H_COMPOSITOR_
#define _H_COMPOSITOR_

#include "types.h"
#include "primitives.h"
#include "tiled_canvas.h"
#include "layer.h"
#include "blend.h"
#include "framebuffer.h"
#include "document.h"
#include "config.h"

// Forward declaration
class Selection;

namespace Compositor {
    // Composite one canvas onto another
    void compositeLayer(TiledCanvas& dst, const TiledCanvas& src,
                        BlendMode mode = BlendMode::Normal, f32 opacity = 1.0f);

    // Composite all layers to a framebuffer
    void compositeDocument(Framebuffer& fb, const Document& doc,
                           const Rect& viewport, f32 zoom, const Vec2& pan);

    // Draw checkerboard pattern for transparency
    void drawCheckerboard(Framebuffer& fb, const Rect& rect,
                          u32 color1 = Config::CHECKER_COLOR1,
                          u32 color2 = Config::CHECKER_COLOR2);

    // Apply adjustment layer to a pixel
    u32 applyAdjustment(u32 pixel, const AdjustmentLayer& adj);

    // Draw marching ants selection outline
    void drawMarchingAnts(Framebuffer& fb, const Selection& sel,
                          const Rect& viewport, f32 zoom, const Vec2& pan, u64 time);
}

#endif
