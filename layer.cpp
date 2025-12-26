#include "layer.h"
#include "basic_widgets.h"

// TextLayer cache management
void TextLayer::ensureCacheValid() const {
    if (cacheValid) return;

    // Use fontFamily to select font (empty or "Internal Font" uses default)
    const std::string& font = fontFamily;

    // Measure text to determine cache size
    Vec2 textSize = FontRenderer::instance().measureTextWithFont(text, static_cast<f32>(fontSize), font);

    // Add some padding for descenders and safety
    u32 cacheWidth = static_cast<u32>(textSize.x + 4);
    u32 cacheHeight = static_cast<u32>(textSize.y + 8);

    if (cacheWidth == 0 || cacheHeight == 0) {
        cacheValid = true;
        return;
    }

    // Resize and clear the cache
    rasterizedCache.resize(cacheWidth, cacheHeight);
    rasterizedCache.clear();

    // Render text to cache with specified font
    FontRenderer::instance().renderToCanvas(
        rasterizedCache,
        text,
        0, 0,
        textColor.toRGBA(),
        static_cast<f32>(fontSize),
        font
    );

    cacheValid = true;
}
