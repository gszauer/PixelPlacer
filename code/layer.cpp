#include "layer.h"
#include "basic_widgets.h"

// Helper to process escape sequences in text
static std::string processEscapeSequences(const std::string& input) {
    std::string result;
    result.reserve(input.size());

    for (size_t i = 0; i < input.size(); ++i) {
        if (input[i] == '\\' && i + 1 < input.size()) {
            char next = input[i + 1];
            if (next == 'n') {
                result += '\n';
                ++i;
                continue;
            } else if (next == 't') {
                result += '\t';
                ++i;
                continue;
            } else if (next == '\\') {
                result += '\\';
                ++i;
                continue;
            }
        }
        result += input[i];
    }

    return result;
}

// TextLayer cache management
void TextLayer::ensureCacheValid() const {
    if (cacheValid) return;

    // Use fontFamily to select font (empty or "Internal Font" uses default)
    const std::string& font = fontFamily;

    // Process escape sequences in text
    std::string processedText = processEscapeSequences(text);

    // Measure text to determine cache size
    Vec2 textSize = FontRenderer::instance().measureTextWithFont(processedText, static_cast<f32>(fontSize), font);

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
        processedText,
        0, 0,
        textColor.toRGBA(),
        static_cast<f32>(fontSize),
        font
    );

    cacheValid = true;
}
