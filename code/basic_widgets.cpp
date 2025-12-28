#include "basic_widgets.h"
#include "tiled_canvas.h"
#include "platform.h"
#include "keycodes.h"

// Include stb_truetype for font rendering
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

// Default embedded font (DejaVu Sans subset - we'll use system font if available)
// This is a placeholder - actual implementation will load system fonts

FontRenderer& FontRenderer::instance() {
    static FontRenderer instance;
    return instance;
}

void FontRenderer::loadFont(const u8* data, i32 size) {
    fontData.assign(data, data + size);
    fontInfo = std::make_unique<stbtt_fontinfo>();
    if (stbtt_InitFont(fontInfo.get(), fontData.data(), 0)) {
        fontLoaded = true;
    }
}

bool FontRenderer::loadCustomFont(const std::string& fontName, const u8* data, i32 size) {
    if (fontName.empty() || fontName == "Internal Font") {
        return false;  // Can't use reserved names
    }

    // Check if already loaded
    if (customFonts.find(fontName) != customFonts.end()) {
        return true;  // Already loaded
    }

    LoadedFont font;
    font.data.assign(data, data + size);
    font.info = std::make_unique<stbtt_fontinfo>();

    if (!stbtt_InitFont(font.info.get(), font.data.data(), 0)) {
        return false;  // Invalid font file
    }

    customFonts[fontName] = std::move(font);
    return true;
}

bool FontRenderer::hasFont(const std::string& fontName) const {
    if (fontName.empty() || fontName == "Internal Font") {
        return fontLoaded;
    }
    return customFonts.find(fontName) != customFonts.end();
}

stbtt_fontinfo* FontRenderer::getFont(const std::string& fontName) {
    if (fontName.empty() || fontName == "Internal Font") {
        return fontLoaded ? fontInfo.get() : nullptr;
    }

    auto it = customFonts.find(fontName);
    if (it != customFonts.end()) {
        return it->second.info.get();
    }

    // Fallback to default font
    return fontLoaded ? fontInfo.get() : nullptr;
}

std::vector<std::string> FontRenderer::getFontNames() const {
    std::vector<std::string> names;
    for (const auto& [name, font] : customFonts) {
        names.push_back(name);
    }
    return names;
}

void FontRenderer::renderText(Framebuffer& fb, const std::string& text, i32 x, i32 y, u32 color, f32 size) {
    if (!fontLoaded || text.empty()) return;

    f32 scale = stbtt_ScaleForPixelHeight(fontInfo.get(), size);

    i32 ascent, descent, lineGap;
    stbtt_GetFontVMetrics(fontInfo.get(), &ascent, &descent, &lineGap);
    ascent = static_cast<i32>(ascent * scale);

    f32 xpos = static_cast<f32>(x);

    u8 cr, cg, cb, ca;
    Blend::unpack(color, cr, cg, cb, ca);

    for (size_t i = 0; i < text.size(); ++i) {
        i32 c = static_cast<u8>(text[i]);
        if (c < 32) continue;

        i32 advance, lsb;
        stbtt_GetCodepointHMetrics(fontInfo.get(), c, &advance, &lsb);

        i32 x0, y0, x1, y1;
        stbtt_GetCodepointBitmapBox(fontInfo.get(), c, scale, scale, &x0, &y0, &x1, &y1);

        i32 bw = x1 - x0;
        i32 bh = y1 - y0;

        if (bw > 0 && bh > 0) {
            std::vector<u8> bitmap(bw * bh);
            stbtt_MakeCodepointBitmap(fontInfo.get(), bitmap.data(), bw, bh, bw, scale, scale, c);

            i32 gx = static_cast<i32>(xpos) + x0;
            i32 gy = y + ascent + y0;

            for (i32 by = 0; by < bh; ++by) {
                for (i32 bx = 0; bx < bw; ++bx) {
                    u8 alpha = bitmap[by * bw + bx];
                    if (alpha > 0) {
                        u32 pixelColor = Blend::pack(cr, cg, cb, static_cast<u8>((alpha * ca) / 255));
                        fb.blendPixel(gx + bx, gy + by, pixelColor);
                    }
                }
            }
        }

        xpos += advance * scale;

        if (i + 1 < text.size()) {
            i32 kern = stbtt_GetCodepointKernAdvance(fontInfo.get(), c, static_cast<u8>(text[i + 1]));
            xpos += kern * scale;
        }
    }
}

void FontRenderer::renderTextWithFont(Framebuffer& fb, const std::string& text, i32 x, i32 y, u32 color, f32 size, const std::string& fontName) {
    stbtt_fontinfo* font = getFont(fontName);
    if (!font || text.empty()) return;

    f32 scale = stbtt_ScaleForPixelHeight(font, size);

    i32 ascent, descent, lineGap;
    stbtt_GetFontVMetrics(font, &ascent, &descent, &lineGap);
    ascent = static_cast<i32>(ascent * scale);

    f32 xpos = static_cast<f32>(x);

    u8 cr, cg, cb, ca;
    Blend::unpack(color, cr, cg, cb, ca);

    // For icon fonts, we need to handle Unicode codepoints
    size_t i = 0;
    while (i < text.size()) {
        i32 c;
        // Decode UTF-8
        u8 b0 = static_cast<u8>(text[i]);
        if ((b0 & 0x80) == 0) {
            c = b0;
            i += 1;
        } else if ((b0 & 0xE0) == 0xC0 && i + 1 < text.size()) {
            c = ((b0 & 0x1F) << 6) | (static_cast<u8>(text[i + 1]) & 0x3F);
            i += 2;
        } else if ((b0 & 0xF0) == 0xE0 && i + 2 < text.size()) {
            c = ((b0 & 0x0F) << 12) | ((static_cast<u8>(text[i + 1]) & 0x3F) << 6) | (static_cast<u8>(text[i + 2]) & 0x3F);
            i += 3;
        } else if ((b0 & 0xF8) == 0xF0 && i + 3 < text.size()) {
            c = ((b0 & 0x07) << 18) | ((static_cast<u8>(text[i + 1]) & 0x3F) << 12) | ((static_cast<u8>(text[i + 2]) & 0x3F) << 6) | (static_cast<u8>(text[i + 3]) & 0x3F);
            i += 4;
        } else {
            i += 1;
            continue;
        }

        i32 advance, lsb;
        stbtt_GetCodepointHMetrics(font, c, &advance, &lsb);

        i32 x0, y0, x1, y1;
        stbtt_GetCodepointBitmapBox(font, c, scale, scale, &x0, &y0, &x1, &y1);

        i32 bw = x1 - x0;
        i32 bh = y1 - y0;

        if (bw > 0 && bh > 0) {
            std::vector<u8> bitmap(bw * bh);
            stbtt_MakeCodepointBitmap(font, bitmap.data(), bw, bh, bw, scale, scale, c);

            i32 gx = static_cast<i32>(xpos) + x0;
            i32 gy = y + ascent + y0;

            for (i32 by = 0; by < bh; ++by) {
                for (i32 bx = 0; bx < bw; ++bx) {
                    u8 alpha = bitmap[by * bw + bx];
                    if (alpha > 0) {
                        u32 pixelColor = Blend::pack(cr, cg, cb, static_cast<u8>((alpha * ca) / 255));
                        fb.blendPixel(gx + bx, gy + by, pixelColor);
                    }
                }
            }
        }

        xpos += advance * scale;
    }
}

Vec2 FontRenderer::measureText(const std::string& text, f32 size) {
    if (!fontLoaded || text.empty()) return Vec2(0, size);

    f32 scale = stbtt_ScaleForPixelHeight(fontInfo.get(), size);

    i32 ascent, descent, lineGap;
    stbtt_GetFontVMetrics(fontInfo.get(), &ascent, &descent, &lineGap);

    f32 width = 0;

    for (size_t i = 0; i < text.size(); ++i) {
        i32 c = static_cast<u8>(text[i]);
        i32 advance, lsb;
        stbtt_GetCodepointHMetrics(fontInfo.get(), c, &advance, &lsb);
        width += advance * scale;

        if (i + 1 < text.size()) {
            i32 kern = stbtt_GetCodepointKernAdvance(fontInfo.get(), c, static_cast<u8>(text[i + 1]));
            width += kern * scale;
        }
    }

    return Vec2(width, (ascent - descent) * scale);
}

void FontRenderer::renderTextVertical(Framebuffer& fb, const std::string& text, i32 x, i32 y, u32 color, f32 size) {
    if (!fontLoaded || text.empty()) return;

    f32 scale = stbtt_ScaleForPixelHeight(fontInfo.get(), size);

    i32 ascent, descent, lineGap;
    stbtt_GetFontVMetrics(fontInfo.get(), &ascent, &descent, &lineGap);
    ascent = static_cast<i32>(ascent * scale);

    f32 lineHeight = size * 0.9f;  // Tighter vertical spacing for stacked chars
    f32 ypos = static_cast<f32>(y);

    u8 cr, cg, cb, ca;
    Blend::unpack(color, cr, cg, cb, ca);

    for (size_t i = 0; i < text.size(); ++i) {
        i32 c = static_cast<u8>(text[i]);
        if (c < 32) continue;

        i32 advance, lsb;
        stbtt_GetCodepointHMetrics(fontInfo.get(), c, &advance, &lsb);

        i32 x0, y0, x1, y1;
        stbtt_GetCodepointBitmapBox(fontInfo.get(), c, scale, scale, &x0, &y0, &x1, &y1);

        i32 bw = x1 - x0;
        i32 bh = y1 - y0;

        if (bw > 0 && bh > 0) {
            std::vector<u8> bitmap(bw * bh);
            stbtt_MakeCodepointBitmap(fontInfo.get(), bitmap.data(), bw, bh, bw, scale, scale, c);

            // Center each character horizontally around x
            f32 charWidth = advance * scale;
            i32 gx = x + x0 - static_cast<i32>(charWidth / 4);  // Rough centering
            i32 gy = static_cast<i32>(ypos) + ascent + y0;

            for (i32 by = 0; by < bh; ++by) {
                for (i32 bx = 0; bx < bw; ++bx) {
                    u8 alpha = bitmap[by * bw + bx];
                    if (alpha > 0) {
                        u32 pixelColor = Blend::pack(cr, cg, cb, static_cast<u8>((alpha * ca) / 255));
                        fb.blendPixel(gx + bx, gy + by, pixelColor);
                    }
                }
            }
        }

        ypos += lineHeight;
    }
}

Vec2 FontRenderer::measureTextVertical(const std::string& text, f32 size) {
    if (!fontLoaded || text.empty()) return Vec2(size, 0);

    f32 lineHeight = size * 0.9f;
    f32 height = text.size() * lineHeight;

    return Vec2(size, height);
}

void FontRenderer::renderTextRotated90(Framebuffer& fb, const std::string& text, i32 x, i32 y, u32 color, f32 size) {
    // Renders text rotated 90 degrees counter-clockwise (reads bottom-to-top)
    if (!fontLoaded || text.empty()) return;

    f32 scale = stbtt_ScaleForPixelHeight(fontInfo.get(), size);

    i32 ascent, descent, lineGap;
    stbtt_GetFontVMetrics(fontInfo.get(), &ascent, &descent, &lineGap);
    ascent = static_cast<i32>(ascent * scale);

    // Measure total width first to position correctly
    Vec2 textSize = measureText(text, size);
    f32 xpos = 0;

    u8 cr, cg, cb, ca;
    Blend::unpack(color, cr, cg, cb, ca);

    for (size_t i = 0; i < text.size(); ++i) {
        i32 c = static_cast<u8>(text[i]);
        if (c < 32) continue;

        i32 advance, lsb;
        stbtt_GetCodepointHMetrics(fontInfo.get(), c, &advance, &lsb);

        i32 x0, y0, x1, y1;
        stbtt_GetCodepointBitmapBox(fontInfo.get(), c, scale, scale, &x0, &y0, &x1, &y1);

        i32 bw = x1 - x0;
        i32 bh = y1 - y0;

        if (bw > 0 && bh > 0) {
            std::vector<u8> bitmap(bw * bh);
            stbtt_MakeCodepointBitmap(fontInfo.get(), bitmap.data(), bw, bh, bw, scale, scale, c);

            // Rotated 90 CCW: original (bx, by) -> rotated (by, bw-1-bx)
            // The rotated bitmap has dimensions (bh, bw)
            // Position: start from bottom, go up
            i32 gx = x + static_cast<i32>(ascent + y0);  // y offset becomes x
            i32 gy = y + static_cast<i32>(textSize.x - xpos - x0 - bw);  // x offset becomes y (flipped)

            for (i32 by = 0; by < bh; ++by) {
                for (i32 bx = 0; bx < bw; ++bx) {
                    u8 alpha = bitmap[by * bw + bx];
                    if (alpha > 0) {
                        // Rotate 90 CCW: (bx, by) -> (by, bw-1-bx)
                        i32 rx = gx + by;
                        i32 ry = gy + (bw - 1 - bx);
                        u32 pixelColor = Blend::pack(cr, cg, cb, static_cast<u8>((alpha * ca) / 255));
                        fb.blendPixel(rx, ry, pixelColor);
                    }
                }
            }
        }

        xpos += advance * scale;

        if (i + 1 < text.size()) {
            i32 kern = stbtt_GetCodepointKernAdvance(fontInfo.get(), c, static_cast<u8>(text[i + 1]));
            xpos += kern * scale;
        }
    }
}

void FontRenderer::renderToCanvas(TiledCanvas& canvas, const std::string& text, i32 x, i32 y, u32 color, f32 size, const std::string& fontName) {
    stbtt_fontinfo* font = getFont(fontName);
    if (!font || text.empty()) return;

    f32 scale = stbtt_ScaleForPixelHeight(font, size);

    i32 ascent, descent, lineGap;
    stbtt_GetFontVMetrics(font, &ascent, &descent, &lineGap);
    i32 scaledAscent = static_cast<i32>(ascent * scale);
    f32 lineHeight = (ascent - descent + lineGap) * scale;

    // Calculate tab width (4 spaces)
    i32 spaceAdvance, spaceLsb;
    stbtt_GetCodepointHMetrics(font, ' ', &spaceAdvance, &spaceLsb);
    f32 tabWidth = spaceAdvance * scale * 4;

    f32 xpos = static_cast<f32>(x);
    f32 ypos = static_cast<f32>(y);

    u8 cr, cg, cb, ca;
    Blend::unpack(color, cr, cg, cb, ca);

    for (size_t i = 0; i < text.size(); ++i) {
        i32 c = static_cast<u8>(text[i]);

        // Handle newline
        if (c == '\n') {
            xpos = static_cast<f32>(x);
            ypos += lineHeight;
            continue;
        }

        // Handle tab
        if (c == '\t') {
            xpos += tabWidth;
            continue;
        }

        // Skip other control characters
        if (c < 32) continue;

        i32 advance, lsb;
        stbtt_GetCodepointHMetrics(font, c, &advance, &lsb);

        i32 x0, y0, x1, y1;
        stbtt_GetCodepointBitmapBox(font, c, scale, scale, &x0, &y0, &x1, &y1);

        i32 bw = x1 - x0;
        i32 bh = y1 - y0;

        if (bw > 0 && bh > 0) {
            std::vector<u8> bitmap(bw * bh);
            stbtt_MakeCodepointBitmap(font, bitmap.data(), bw, bh, bw, scale, scale, c);

            i32 gx = static_cast<i32>(xpos) + x0;
            i32 gy = static_cast<i32>(ypos) + scaledAscent + y0;

            for (i32 by = 0; by < bh; ++by) {
                for (i32 bx = 0; bx < bw; ++bx) {
                    u8 alpha = bitmap[by * bw + bx];
                    if (alpha > 0) {
                        u32 pixelColor = Blend::pack(cr, cg, cb, static_cast<u8>((alpha * ca) / 255));
                        canvas.alphaBlendPixel(gx + bx, gy + by, pixelColor);
                    }
                }
            }
        }

        xpos += advance * scale;

        if (i + 1 < text.size()) {
            i32 kern = stbtt_GetCodepointKernAdvance(font, c, static_cast<u8>(text[i + 1]));
            xpos += kern * scale;
        }
    }
}

Vec2 FontRenderer::measureTextWithFont(const std::string& text, f32 size, const std::string& fontName) {
    stbtt_fontinfo* font = getFont(fontName);
    if (!font || text.empty()) return Vec2(0, size);

    f32 scale = stbtt_ScaleForPixelHeight(font, size);

    i32 ascent, descent, lineGap;
    stbtt_GetFontVMetrics(font, &ascent, &descent, &lineGap);
    f32 lineHeight = (ascent - descent + lineGap) * scale;
    f32 singleLineHeight = (ascent - descent) * scale;

    // Calculate tab width (4 spaces)
    i32 spaceAdvance, spaceLsb;
    stbtt_GetCodepointHMetrics(font, ' ', &spaceAdvance, &spaceLsb);
    f32 tabWidth = spaceAdvance * scale * 4;

    f32 maxWidth = 0;
    f32 currentWidth = 0;
    i32 lineCount = 1;

    for (size_t i = 0; i < text.size(); ++i) {
        i32 c = static_cast<u8>(text[i]);

        // Handle newline
        if (c == '\n') {
            maxWidth = std::max(maxWidth, currentWidth);
            currentWidth = 0;
            lineCount++;
            continue;
        }

        // Handle tab
        if (c == '\t') {
            currentWidth += tabWidth;
            continue;
        }

        // Skip other control characters
        if (c < 32) continue;

        i32 advance, lsb;
        stbtt_GetCodepointHMetrics(font, c, &advance, &lsb);
        currentWidth += advance * scale;

        if (i + 1 < text.size()) {
            i32 nextC = static_cast<u8>(text[i + 1]);
            if (nextC >= 32) {  // Only kern with printable chars
                i32 kern = stbtt_GetCodepointKernAdvance(font, c, nextC);
                currentWidth += kern * scale;
            }
        }
    }

    maxWidth = std::max(maxWidth, currentWidth);
    f32 totalHeight = singleLineHeight + (lineCount - 1) * lineHeight;

    return Vec2(maxWidth, totalHeight);
}

void FontRenderer::renderIconCentered(Framebuffer& fb, const std::string& icon, const Rect& bounds, u32 color, f32 size, const std::string& fontName) {
    stbtt_fontinfo* font = getFont(fontName);
    if (!font || icon.empty()) return;

    // Decode UTF-8 to get the codepoint
    i32 codepoint = 0;
    u8 b0 = static_cast<u8>(icon[0]);
    if ((b0 & 0x80) == 0) {
        codepoint = b0;
    } else if ((b0 & 0xE0) == 0xC0 && icon.size() >= 2) {
        codepoint = ((b0 & 0x1F) << 6) | (static_cast<u8>(icon[1]) & 0x3F);
    } else if ((b0 & 0xF0) == 0xE0 && icon.size() >= 3) {
        codepoint = ((b0 & 0x0F) << 12) | ((static_cast<u8>(icon[1]) & 0x3F) << 6) | (static_cast<u8>(icon[2]) & 0x3F);
    } else if ((b0 & 0xF8) == 0xF0 && icon.size() >= 4) {
        codepoint = ((b0 & 0x07) << 18) | ((static_cast<u8>(icon[1]) & 0x3F) << 12) | ((static_cast<u8>(icon[2]) & 0x3F) << 6) | (static_cast<u8>(icon[3]) & 0x3F);
    }
    if (codepoint == 0) return;

    f32 scale = stbtt_ScaleForPixelHeight(font, size);

    // Get the actual glyph bounding box
    i32 x0, y0, x1, y1;
    stbtt_GetCodepointBitmapBox(font, codepoint, scale, scale, &x0, &y0, &x1, &y1);

    i32 glyphW = x1 - x0;
    i32 glyphH = y1 - y0;
    if (glyphW <= 0 || glyphH <= 0) return;

    // Render the glyph bitmap
    std::vector<u8> bitmap(glyphW * glyphH);
    stbtt_MakeCodepointBitmap(font, bitmap.data(), glyphW, glyphH, glyphW, scale, scale, codepoint);

    // Center the glyph visually within bounds
    // The bitmap represents the actual visible pixels of the glyph
    i32 drawX = static_cast<i32>(bounds.x + (bounds.w - glyphW) / 2);
    i32 drawY = static_cast<i32>(bounds.y + (bounds.h - glyphH) / 2);

    u8 cr, cg, cb, ca;
    Blend::unpack(color, cr, cg, cb, ca);

    for (i32 by = 0; by < glyphH; ++by) {
        for (i32 bx = 0; bx < glyphW; ++bx) {
            u8 alpha = bitmap[by * glyphW + bx];
            if (alpha > 0) {
                u32 pixelColor = Blend::pack(cr, cg, cb, static_cast<u8>((alpha * ca) / 255));
                fb.blendPixel(drawX + bx, drawY + by, pixelColor);
            }
        }
    }
}

// Label implementation
void Label::renderSelf(Framebuffer& fb) {
    Rect global = globalBounds();

    // Truncate text if too wide
    f32 maxTextWidth = global.w - 8 * Config::uiScale;  // 4px padding on each side
    std::string displayText = text;
    Vec2 textSize = FontRenderer::instance().measureText(displayText, fontSize);

    if (textSize.x > maxTextWidth && displayText.length() > 3) {
        // Truncate with ellipsis
        while (textSize.x > maxTextWidth && displayText.length() > 3) {
            displayText = displayText.substr(0, displayText.length() - 4) + "...";
            textSize = FontRenderer::instance().measureText(displayText, fontSize);
        }
    }

    f32 tx = global.x + 4 * Config::uiScale;
    f32 ty = global.y;

    if (centerHorizontal) {
        tx = global.x + (global.w - textSize.x) / 2;
    }
    if (centerVertical) {
        ty = global.y + (global.h - textSize.y) / 2;
    }

    // Use dimmed color when disabled
    u32 actualTextColor = enabled ? textColor : Config::COLOR_TEXT_DIM;
    FontRenderer::instance().renderText(fb, displayText, static_cast<i32>(tx), static_cast<i32>(ty), actualTextColor, fontSize);
}

// Button implementation
void Button::renderSelf(Framebuffer& fb) {
    Rect global = globalBounds();

    u32 bgColor = normalColor;
    if (!enabled) bgColor = Config::COLOR_BACKGROUND_DISABLED;
    else if (pressed) bgColor = pressedColor;
    else if (hovered) bgColor = hoverColor;

    fb.fillRect(Recti(global), bgColor);
    fb.drawRect(Recti(global), borderColor);

    // Truncate text if too wide (leave padding for border)
    f32 maxTextWidth = global.w - 8;  // 4px padding on each side
    std::string displayText = text;
    Vec2 textSize = FontRenderer::instance().measureText(displayText, fontSize);

    if (textSize.x > maxTextWidth && displayText.length() > 3) {
        // Truncate with ellipsis
        while (textSize.x > maxTextWidth && displayText.length() > 3) {
            displayText = displayText.substr(0, displayText.length() - 4) + "...";
            textSize = FontRenderer::instance().measureText(displayText, fontSize);
        }
    }

    f32 tx;
    if (textAlign == 0) {  // Left
        tx = global.x + 4;
    } else if (textAlign == 2) {  // Right
        tx = global.x + global.w - textSize.x - 4;
    } else {  // Center (default)
        tx = global.x + (global.w - textSize.x) / 2;
    }
    f32 ty = global.y + (global.h - textSize.y) / 2;

    // Use dimmed color when disabled
    u32 actualTextColor = enabled ? textColor : Config::COLOR_TEXT_DIM;
    FontRenderer::instance().renderText(fb, displayText, static_cast<i32>(tx), static_cast<i32>(ty), actualTextColor, fontSize);
}

bool Button::onMouseDown(const MouseEvent& e) {
    if (!enabled) return false;
    if (e.button == MouseButton::Left && bounds.containsLocal(e.position)) {
        pressed = true;
        return true;
    }
    return false;
}

bool Button::onMouseUp(const MouseEvent& e) {
    if (pressed && e.button == MouseButton::Left) {
        pressed = false;
        if (bounds.containsLocal(e.position)) {
            // Check for double-click
            u64 currentTime = Platform::getMilliseconds();
            if (onDoubleClick && (currentTime - lastClickTime) < DOUBLE_CLICK_TIME) {
                onDoubleClick();
                lastClickTime = 0;  // Reset to prevent triple-click triggering
            } else {
                if (onClick) onClick();
                lastClickTime = currentTime;
            }
        }
        return true;
    }
    return false;
}

void Button::onMouseLeave(const MouseEvent& e) {
    Widget::onMouseLeave(e);
    pressed = false;
}

// IconButton implementation
void IconButton::renderSelf(Framebuffer& fb) {
    Rect global = globalBounds();

    u32 bgColor = normalColor;
    if (!enabled) bgColor = Config::COLOR_BACKGROUND_DISABLED;
    else if (selected) bgColor = selectedColor;
    else if (pressed) bgColor = pressedColor;
    else if (hovered) bgColor = hoverColor;

    if ((bgColor & 0xFF) > 0) {
        fb.fillRect(Recti(global), bgColor);
    }

    if (renderIcon) {
        // Use dimmed color when disabled
        u32 actualIconColor = enabled ? iconColor : Config::COLOR_TEXT_DIM;
        renderIcon(fb, global, actualIconColor);
    }
}

bool IconButton::onMouseDown(const MouseEvent& e) {
    if (!enabled) return false;
    if (e.button == MouseButton::Left && bounds.containsLocal(e.position)) {
        pressed = true;
        return true;
    }
    return false;
}

bool IconButton::onMouseUp(const MouseEvent& e) {
    if (pressed && e.button == MouseButton::Left) {
        pressed = false;
        if (bounds.containsLocal(e.position)) {
            if (toggleMode) {
                selected = !selected;
            }

            // Check for double-click
            u64 currentTime = Platform::getMilliseconds();
            if (onDoubleClick && (currentTime - lastClickTime) < DOUBLE_CLICK_TIME) {
                onDoubleClick();
                lastClickTime = 0;  // Reset to prevent triple-click triggering
            } else {
                if (onClick) onClick();
                lastClickTime = currentTime;
            }
        }
        return true;
    }
    return false;
}

void IconButton::onMouseLeave(const MouseEvent& e) {
    Widget::onMouseLeave(e);
    pressed = false;
}

// Checkbox implementation
void Checkbox::renderSelf(Framebuffer& fb) {
    Rect global = globalBounds();

    // Draw checkbox box
    i32 boxSize = static_cast<i32>(16 * Config::uiScale);
    i32 boxX = static_cast<i32>(global.x);
    i32 boxY = static_cast<i32>(global.y + (global.h - boxSize) / 2);

    fb.fillRect(boxX, boxY, boxSize, boxSize, boxColor);
    fb.drawRect(Recti(boxX, boxY, boxSize, boxSize), Config::COLOR_BORDER);

    // Draw check mark
    if (checked) {
        i32 padding = static_cast<i32>(3 * Config::uiScale);
        fb.fillRect(boxX + padding, boxY + padding, boxSize - padding * 2, boxSize - padding * 2, checkColor);
    }

    // Draw label
    if (!label.empty()) {
        f32 tx = global.x + boxSize + 6 * Config::uiScale;
        f32 ty = global.y + (global.h - Config::defaultFontSize()) / 2;
        FontRenderer::instance().renderText(fb, label, static_cast<i32>(tx), static_cast<i32>(ty), textColor);
    }
}

bool Checkbox::onMouseDown(const MouseEvent& e) {
    if (!enabled) return false;  // Ignore clicks when disabled
    if (e.button == MouseButton::Left && bounds.containsLocal(e.position)) {
        checked = !checked;
        if (onChanged) onChanged(checked);
        return true;
    }
    return false;
}

// Slider implementation
void Slider::renderSelf(Framebuffer& fb) {
    Rect global = globalBounds();

    i32 trackHeight = static_cast<i32>(4 * Config::uiScale);
    i32 thumbWidth = static_cast<i32>(12 * Config::uiScale);
    i32 thumbHeight = static_cast<i32>(global.h) - static_cast<i32>(4 * Config::uiScale);

    i32 trackY = static_cast<i32>(global.y + (global.h - trackHeight) / 2);

    // Use dimmed colors when disabled
    u32 actualTrackColor = enabled ? trackColor : Config::COLOR_BACKGROUND_DISABLED;
    u32 actualFillColor = enabled ? fillColor : Config::COLOR_TEXT_DIM;
    u32 actualThumbColor = enabled ? thumbColor : Config::COLOR_TEXT_DIM;

    // Draw track background
    fb.fillRect(static_cast<i32>(global.x), trackY, static_cast<i32>(global.w), trackHeight, actualTrackColor);

    // Draw filled portion (use rounding to prevent jumps on resize)
    f32 normalized = getNormalizedValue();
    i32 fillWidth = static_cast<i32>(std::round((global.w - thumbWidth) * normalized));
    fb.fillRect(static_cast<i32>(global.x), trackY, fillWidth + thumbWidth / 2, trackHeight, actualFillColor);

    // Draw thumb
    i32 thumbX = static_cast<i32>(std::round(global.x + fillWidth));
    i32 thumbY = static_cast<i32>(global.y + (global.h - thumbHeight) / 2);
    fb.fillRect(thumbX, thumbY, thumbWidth, thumbHeight, actualThumbColor);
}

bool Slider::onMouseDown(const MouseEvent& e) {
    if (!enabled) return false;  // Ignore when disabled
    if (e.button == MouseButton::Left && bounds.containsLocal(e.position)) {
        dragging = true;
        getAppState().capturedWidget = this;  // Capture mouse while dragging
        updateValueFromMouse(e.position.x);
        return true;
    }
    return false;
}

bool Slider::onMouseUp(const MouseEvent& e) {
    if (dragging) {
        dragging = false;
        getAppState().capturedWidget = nullptr;  // Release mouse capture
        if (onDragEnd) onDragEnd();  // Notify that drag operation completed
        return true;
    }
    return false;
}

bool Slider::onMouseDrag(const MouseEvent& e) {
    if (dragging) {
        updateValueFromMouse(e.position.x);
        return true;
    }
    return false;
}

void Slider::updateValueFromMouse(f32 x) {
    f32 thumbWidth = 12.0f * Config::uiScale;
    f32 normalized = (x - thumbWidth / 2) / (bounds.w - thumbWidth);
    normalized = clamp(normalized, 0.0f, 1.0f);
    f32 newValue = minValue + normalized * (maxValue - minValue);
    if (newValue != value) {
        value = newValue;
        if (onChanged) onChanged(value);
    }
}

// NumberSliderPopup implementation
NumberSliderPopup::NumberSliderPopup() {
    preferredSize = Vec2(100 * Config::uiScale, 20 * Config::uiScale);
    visible = false;
}

void NumberSliderPopup::renderSelf(Framebuffer& fb) {
    if (!visible || !owner) return;

    Rect global = globalBounds();
    f32 padding = 4 * Config::uiScale;
    f32 trackHeight = 8 * Config::uiScale;
    f32 thumbWidth = 12 * Config::uiScale;

    // Background
    fb.fillRect(Recti(global), bgColor);
    fb.drawRect(Recti(global), borderColor);

    // Track
    f32 trackY = global.y + (global.h - trackHeight) / 2;
    Rect trackRect(global.x + padding, trackY, global.w - padding * 2, trackHeight);
    fb.fillRect(Recti(trackRect), trackColor);

    // Fill (up to current value)
    f32 normalized = owner->getNormalizedValue();
    f32 fillWidth = (trackRect.w - thumbWidth) * normalized;
    if (fillWidth > 0) {
        Rect fillRect(trackRect.x, trackRect.y, fillWidth + thumbWidth / 2, trackHeight);
        fb.fillRect(Recti(fillRect), fillColor);
    }

    // Thumb
    f32 thumbX = trackRect.x + (trackRect.w - thumbWidth) * normalized;
    Rect thumbRect(thumbX, global.y + 2 * Config::uiScale, thumbWidth, global.h - 4 * Config::uiScale);
    fb.fillRect(Recti(thumbRect), thumbColor);
}

bool NumberSliderPopup::onMouseDown(const MouseEvent& e) {
    if (!visible || !owner || e.button != MouseButton::Left) return false;

    if (bounds.containsLocal(e.position)) {
        dragging = true;
        getAppState().capturedWidget = this;  // Capture mouse while dragging
        updateValueFromMouse(e.position.x);
        return true;
    }
    return false;
}

bool NumberSliderPopup::onMouseDrag(const MouseEvent& e) {
    if (!dragging || !owner) return false;
    updateValueFromMouse(e.position.x);
    return true;
}

bool NumberSliderPopup::onMouseUp(const MouseEvent& e) {
    if (dragging) {
        dragging = false;
        getAppState().capturedWidget = nullptr;  // Release mouse capture
        return true;
    }
    return false;
}

void NumberSliderPopup::updateValueFromMouse(f32 x) {
    if (!owner) return;

    f32 padding = 4 * Config::uiScale;
    f32 thumbWidth = 12 * Config::uiScale;
    f32 trackWidth = bounds.w - padding * 2 - thumbWidth;

    f32 normalized = (x - padding - thumbWidth / 2) / trackWidth;
    normalized = clamp(normalized, 0.0f, 1.0f);

    f32 newValue = owner->minValue + normalized * (owner->maxValue - owner->minValue);

    // Clamp to bounds
    if (!owner->minUnbound) newValue = std::max(newValue, owner->minValue);
    if (!owner->maxUnbound) newValue = std::min(newValue, owner->maxValue);

    if (newValue != owner->value) {
        owner->value = newValue;
        owner->editText = owner->getDisplayText();
        owner->cursorPos = 0;
        if (owner->onChanged) owner->onChanged(owner->value);
        getAppState().needsRedraw = true;
    }
}

// NumberSlider implementation
NumberSlider::NumberSlider() {
    focusable = true;
    preferredSize = Vec2(60 * Config::uiScale, 24 * Config::uiScale);
    minSize = Vec2(40 * Config::uiScale, 20 * Config::uiScale);
}

NumberSlider::NumberSlider(f32 min, f32 max, f32 initial, i32 decimalPlaces)
    : value(initial), minValue(min), maxValue(max), decimals(decimalPlaces) {
    focusable = true;
    preferredSize = Vec2(60 * Config::uiScale, 24 * Config::uiScale);
    minSize = Vec2(40 * Config::uiScale, 20 * Config::uiScale);
    editText = getDisplayText();
}

void NumberSlider::setValue(f32 v) {
    if (!minUnbound) v = std::max(v, minValue);
    if (!maxUnbound) v = std::min(v, maxValue);
    value = v;
    if (!editing) {
        editText = getDisplayText();
        cursorPos = 0;
    }
}

f32 NumberSlider::getNormalizedValue() const {
    if (maxValue == minValue) return 0.0f;
    return clamp((value - minValue) / (maxValue - minValue), 0.0f, 1.0f);
}

std::string NumberSlider::getDisplayText() const {
    char buf[32];
    if (decimals == 0) {
        snprintf(buf, sizeof(buf), "%d", static_cast<i32>(std::round(value)));
    } else {
        snprintf(buf, sizeof(buf), "%.*f", decimals, value);
    }
    return std::string(buf);
}

void NumberSlider::showPopup() {
    if (!popup) {
        popup = std::make_unique<NumberSliderPopup>();
        popup->owner = this;
    }

    Rect global = globalBounds();
    f32 popupHeight = 20 * Config::uiScale;
    f32 minPopupWidth = 150 * Config::uiScale;
    f32 popupWidth = std::max(global.w, minPopupWidth);
    // Center popup under the text field
    f32 popupX = global.x + (global.w - popupWidth) / 2;
    popup->setBounds(popupX, global.bottom() + 2, popupWidth, popupHeight);
    popup->visible = true;

    OverlayManager::instance().registerOverlay(popup.get(), ZOrder::DROPDOWN,
        [this]() { hidePopup(); });

    getAppState().needsRedraw = true;
}

void NumberSlider::hidePopup() {
    if (popup) {
        popup->visible = false;
        OverlayManager::instance().unregisterOverlay(popup.get());
    }
    getAppState().needsRedraw = true;
}

void NumberSlider::commitEdit() {
    // Try to parse the edited text
    if (editText.empty()) {
        // Empty text - revert to current value
        editText = getDisplayText();
        cursorPos = 0;
        return;
    }

    try {
        f32 newValue = std::stof(editText);

        // Clamp to bounds
        if (!minUnbound) newValue = std::max(newValue, minValue);
        if (!maxUnbound) newValue = std::min(newValue, maxValue);

        if (newValue != value) {
            value = newValue;
            if (onChanged) onChanged(value);
        }
    } catch (...) {
        // Invalid number - revert
    }

    // Always update display text to normalized form
    editText = getDisplayText();
    cursorPos = 0;
    editing = false;
}

void NumberSlider::renderSelf(Framebuffer& fb) {
    Rect global = globalBounds();

    // Background
    fb.fillRect(Recti(global), bgColor);
    fb.drawRect(Recti(global), focused ? focusBorderColor : borderColor);

    // Text
    f32 padding = 4 * Config::uiScale;
    std::string displayText = editing ? editText : getDisplayText();
    if (!suffix.empty() && !editing) {
        displayText += suffix;
    }

    Vec2 textSize = FontRenderer::instance().measureText(displayText, Config::defaultFontSize());
    i32 textX = static_cast<i32>(global.x + padding);
    i32 textY = static_cast<i32>(global.y + (global.h - textSize.y) / 2);

    // Draw selection highlight (when editing)
    if (focused && editing && hasSelection()) {
        i32 selStart = std::min(selectionStart, cursorPos);
        i32 selEnd = std::max(selectionStart, cursorPos);
        std::string beforeSel = editText.substr(0, selStart);
        std::string inSel = editText.substr(selStart, selEnd - selStart);
        f32 selStartX = FontRenderer::instance().measureText(beforeSel, Config::defaultFontSize()).x;
        f32 selWidth = FontRenderer::instance().measureText(inSel, Config::defaultFontSize()).x;
        fb.fillRect(Recti(textX + static_cast<i32>(selStartX), textY,
                          static_cast<i32>(selWidth), static_cast<i32>(textSize.y)),
                    Config::COLOR_SELECTION);
    }

    FontRenderer::instance().renderText(fb, displayText, textX, textY, textColor, Config::defaultFontSize());

    // Cursor (when editing)
    if (focused && editing) {
        u64 now = Platform::getMilliseconds();
        if ((now - cursorBlinkTime) % 1000 < 500) {
            // Measure text up to cursor
            std::string beforeCursor = editText.substr(0, cursorPos);
            Vec2 beforeSize = FontRenderer::instance().measureText(beforeCursor, Config::defaultFontSize());
            i32 cursorX = textX + static_cast<i32>(beforeSize.x);
            fb.fillRect(Recti(cursorX, textY, 1, static_cast<i32>(textSize.y)), textColor);
        }
    }

    // Update popup position if visible (in case parent scrolled)
    if (popup && popup->visible) {
        f32 popupHeight = 20 * Config::uiScale;
        f32 minPopupWidth = 150 * Config::uiScale;
        f32 popupWidth = std::max(global.w, minPopupWidth);
        f32 popupX = global.x + (global.w - popupWidth) / 2;
        popup->setBounds(popupX, global.bottom() + 2, popupWidth, popupHeight);
    }
}

i32 NumberSlider::positionFromX(f32 localX) {
    f32 padding = 4 * Config::uiScale;
    f32 clickX = localX - padding;
    i32 pos = 0;
    for (size_t i = 0; i <= editText.size(); ++i) {
        std::string substr = editText.substr(0, i);
        f32 w = FontRenderer::instance().measureText(substr, Config::defaultFontSize()).x;
        if (w > clickX) {
            pos = static_cast<i32>(i > 0 ? i - 1 : 0);
            return pos;
        }
        pos = static_cast<i32>(i);
    }
    return pos;
}

void NumberSlider::deleteSelection() {
    if (!hasSelection()) return;
    i32 selStart = std::min(selectionStart, cursorPos);
    i32 selEnd = std::max(selectionStart, cursorPos);
    editText.erase(selStart, selEnd - selStart);
    cursorPos = selStart;
    selectionStart = -1;
}

bool NumberSlider::onMouseDown(const MouseEvent& e) {
    if (!enabled || e.button != MouseButton::Left) return false;

    if (bounds.containsLocal(e.position)) {
        // Start editing mode
        if (!editing) {
            editing = true;
            editText = getDisplayText();
        }

        i32 newCursorPos = positionFromX(e.position.x);

        // Double-click detection for select all
        static u64 lastClickTime = 0;
        static i32 lastClickPos = -1;
        u64 now = Platform::getMilliseconds();

        if (now - lastClickTime < 300 && newCursorPos == lastClickPos) {
            // Double click - select all
            selectionStart = 0;
            cursorPos = static_cast<i32>(editText.size());
            draggingSelection = false;
            lastClickTime = 0;  // Reset to prevent triple-click
        } else {
            // Single click - position cursor and start drag
            cursorPos = newCursorPos;
            selectionStart = cursorPos;
            draggingSelection = true;
            lastClickTime = now;
            lastClickPos = newCursorPos;
        }

        cursorBlinkTime = Platform::getMilliseconds();
        showPopup();
        getAppState().needsRedraw = true;
        return true;
    }
    return false;
}

bool NumberSlider::onMouseDrag(const MouseEvent& e) {
    if (!enabled || !focused || !draggingSelection) return false;

    // Update cursor position to extend selection
    cursorPos = positionFromX(e.position.x);
    cursorBlinkTime = Platform::getMilliseconds();
    getAppState().needsRedraw = true;
    return true;
}

bool NumberSlider::onMouseUp(const MouseEvent& e) {
    if (draggingSelection) {
        draggingSelection = false;
        // If selection start equals cursor position, clear selection
        if (selectionStart == cursorPos) {
            selectionStart = -1;
        }
        getAppState().needsRedraw = true;
    }
    return Widget::onMouseUp(e);
}

bool NumberSlider::onKeyDown(const KeyEvent& e) {
    if (!enabled || !focused) return false;

    // Ensure cursorPos is valid
    if (cursorPos < 0) cursorPos = 0;
    if (cursorPos > static_cast<i32>(editText.size())) cursorPos = static_cast<i32>(editText.size());

    if (e.keyCode == Key::RETURN) {
        commitEdit();
        selectionStart = -1;
        // Release focus
        getAppState().focusedWidget = nullptr;
        onBlur();
        return true;
    }

    if (e.keyCode == Key::ESCAPE) {
        editText = getDisplayText();
        cursorPos = 0;
        selectionStart = -1;
        editing = false;
        // Release focus
        getAppState().focusedWidget = nullptr;
        onBlur();
        return true;
    }

    if (e.keyCode == Key::TAB) {
        commitEdit();
        selectionStart = -1;
        // Let parent handle tab navigation
        return false;
    }

    // Ctrl+A to select all
    if (e.keyCode == Key::A && e.mods.ctrl) {
        selectionStart = 0;
        cursorPos = static_cast<i32>(editText.size());
        cursorBlinkTime = Platform::getMilliseconds();
        getAppState().needsRedraw = true;
        return true;
    }

    if (e.keyCode == Key::BACKSPACE) {
        if (hasSelection()) {
            deleteSelection();
        } else if (cursorPos > 0 && cursorPos <= static_cast<i32>(editText.size())) {
            editText.erase(cursorPos - 1, 1);
            cursorPos--;
        }
        cursorBlinkTime = Platform::getMilliseconds();
        getAppState().needsRedraw = true;
        return true;
    }

    if (e.keyCode == Key::DELETE) {
        if (hasSelection()) {
            deleteSelection();
        } else if (cursorPos >= 0 && cursorPos < static_cast<i32>(editText.size())) {
            editText.erase(cursorPos, 1);
        }
        cursorBlinkTime = Platform::getMilliseconds();
        getAppState().needsRedraw = true;
        return true;
    }

    if (e.keyCode == Key::LEFT) {
        if (hasSelection() && !e.mods.shift) {
            // Move to start of selection
            cursorPos = std::min(selectionStart, cursorPos);
            selectionStart = -1;
        } else {
            if (e.mods.shift && selectionStart < 0) {
                selectionStart = cursorPos;
            }
            if (cursorPos > 0) cursorPos--;
            if (!e.mods.shift) selectionStart = -1;
        }
        cursorBlinkTime = Platform::getMilliseconds();
        getAppState().needsRedraw = true;
        return true;
    }

    if (e.keyCode == Key::RIGHT) {
        if (hasSelection() && !e.mods.shift) {
            // Move to end of selection
            cursorPos = std::max(selectionStart, cursorPos);
            selectionStart = -1;
        } else {
            if (e.mods.shift && selectionStart < 0) {
                selectionStart = cursorPos;
            }
            if (cursorPos < static_cast<i32>(editText.size())) cursorPos++;
            if (!e.mods.shift) selectionStart = -1;
        }
        cursorBlinkTime = Platform::getMilliseconds();
        getAppState().needsRedraw = true;
        return true;
    }

    if (e.keyCode == Key::HOME) {
        if (e.mods.shift && selectionStart < 0) {
            selectionStart = cursorPos;
        }
        cursorPos = 0;
        if (!e.mods.shift) selectionStart = -1;
        cursorBlinkTime = Platform::getMilliseconds();
        getAppState().needsRedraw = true;
        return true;
    }

    if (e.keyCode == Key::END) {
        if (e.mods.shift && selectionStart < 0) {
            selectionStart = cursorPos;
        }
        cursorPos = static_cast<i32>(editText.size());
        if (!e.mods.shift) selectionStart = -1;
        cursorBlinkTime = Platform::getMilliseconds();
        getAppState().needsRedraw = true;
        return true;
    }

    return false;
}

bool NumberSlider::onTextInput(const std::string& input) {
    if (!enabled || !focused) return false;

    // Delete selection first if any
    if (hasSelection()) {
        deleteSelection();
    }

    // Filter for valid numeric characters only
    for (char c : input) {
        bool valid = false;

        // Digits are always valid
        if (c >= '0' && c <= '9') {
            valid = true;
        }
        // Minus sign only at start
        else if (c == '-') {
            if (cursorPos == 0 && editText.find('-') == std::string::npos) {
                valid = true;
            }
        }
        // Decimal point only if decimals > 0 and not already present
        else if (c == '.') {
            if (decimals > 0 && editText.find('.') == std::string::npos) {
                valid = true;
            }
        }

        if (valid) {
            // Clamp cursorPos to valid range
            if (cursorPos < 0) cursorPos = 0;
            if (cursorPos > static_cast<i32>(editText.size())) cursorPos = static_cast<i32>(editText.size());

            editText.insert(cursorPos, 1, c);
            cursorPos++;
        }
    }

    cursorBlinkTime = Platform::getMilliseconds();
    getAppState().needsRedraw = true;
    return true;
}

void NumberSlider::onFocus() {
    Widget::onFocus();
    editing = true;
    editText = getDisplayText();
    cursorPos = static_cast<i32>(editText.size());  // Cursor at end
    cursorBlinkTime = Platform::getMilliseconds();
    showPopup();
}

void NumberSlider::onBlur() {
    Widget::onBlur();
    commitEdit();
    hidePopup();
}

// TextField implementation
void TextField::renderSelf(Framebuffer& fb) {
    Rect global = globalBounds();

    // Use disabled colors when readOnly or disabled
    bool isDisabled = readOnly || !enabled;
    u32 bg = isDisabled ? Config::COLOR_BACKGROUND_DISABLED : bgColor;
    u32 txtColor = isDisabled ? Config::COLOR_TEXT_DIM : textColor;

    fb.fillRect(Recti(global), bg);
    fb.drawRect(Recti(global), focused ? focusBorderColor : borderColor);

    f32 padding = 4 * Config::uiScale;
    f32 textX = global.x + padding - scrollOffset;  // Apply scroll offset
    f32 textY = global.y + (global.h - fontSize) / 2;

    // Clip text to field bounds (inset by padding and border)
    Recti textClip(
        static_cast<i32>(global.x + padding),
        static_cast<i32>(global.y + 1),
        static_cast<i32>(global.w - padding * 2),
        static_cast<i32>(global.h - 2)
    );
    fb.pushClip(textClip);

    // Draw selection highlight
    if (focused && hasSelection()) {
        i32 selStart = std::min(selectionStart, cursorPos);
        i32 selEnd = std::max(selectionStart, cursorPos);

        // Clamp to valid range
        selStart = clamp(selStart, 0, static_cast<i32>(text.size()));
        selEnd = clamp(selEnd, 0, static_cast<i32>(text.size()));

        // Measure text positions
        f32 startX = FontRenderer::instance().measureText(text.substr(0, selStart), fontSize).x;
        f32 endX = FontRenderer::instance().measureText(text.substr(0, selEnd), fontSize).x;

        // Calculate screen positions (accounting for scroll)
        i32 highlightX1 = static_cast<i32>(textX + startX);
        i32 highlightX2 = static_cast<i32>(textX + endX);
        i32 highlightY = static_cast<i32>(global.y + padding);
        i32 highlightH = static_cast<i32>(global.h - padding * 2);

        if (highlightX2 > highlightX1) {
            Recti selRect(highlightX1, highlightY, highlightX2 - highlightX1, highlightH);
            fb.fillRect(selRect, Config::GRAY_500);
        }
    }

    if (text.empty() && !focused && !placeholder.empty()) {
        FontRenderer::instance().renderText(fb, placeholder, static_cast<i32>(textX), static_cast<i32>(textY), placeholderColor, fontSize);
    } else {
        FontRenderer::instance().renderText(fb, text, static_cast<i32>(textX), static_cast<i32>(textY), txtColor, fontSize);
    }

    // Draw cursor
    if (focused && showCursor) {
        // Clamp cursorPos to valid range
        i32 safePos = std::max(0, std::min(cursorPos, static_cast<i32>(text.size())));
        std::string beforeCursor = text.substr(0, safePos);
        Vec2 cursorOffset = FontRenderer::instance().measureText(beforeCursor, fontSize);
        i32 cursorX = static_cast<i32>(textX + cursorOffset.x);
        i32 cursorY = static_cast<i32>(global.y + padding);
        i32 cursorH = static_cast<i32>(global.h - padding * 2);
        fb.drawVerticalLine(cursorX, cursorY, cursorY + cursorH, textColor);
    }

    fb.popClip();
}

bool TextField::onMouseDown(const MouseEvent& e) {
    if (!enabled) return false;
    if (e.button == MouseButton::Left && bounds.containsLocal(e.position)) {
        // Call onClick callback if set (e.g., for browse buttons on readOnly fields)
        if (onClick) {
            onClick();
            // If readOnly or onClick might have blocked (like file dialogs), return early
            if (readOnly) return true;
        }

        // Calculate cursor position from click location
        i32 newCursorPos = positionFromX(e.position.x);

        // Double-click detection for select all
        static u64 lastClickTime = 0;
        static i32 lastClickPos = -1;
        u64 now = Platform::getMilliseconds();

        if (now - lastClickTime < 300 && newCursorPos == lastClickPos) {
            // Double click - select all
            selectionStart = 0;
            cursorPos = static_cast<i32>(text.size());
            draggingSelection = false;
            lastClickTime = 0;  // Reset to prevent triple-click
        } else {
            // Single click - position cursor and start drag
            cursorPos = newCursorPos;
            selectionStart = cursorPos;  // Start selection from click position
            draggingSelection = true;
            lastClickTime = now;
            lastClickPos = newCursorPos;
        }

        ensureCaretVisible();
        getAppState().needsRedraw = true;
        return true;
    }
    return false;
}

bool TextField::onMouseDrag(const MouseEvent& e) {
    if (!enabled || !focused || readOnly || !draggingSelection) return false;

    // Update cursor position to extend selection
    cursorPos = positionFromX(e.position.x);

    ensureCaretVisible();
    getAppState().needsRedraw = true;
    return true;
}

bool TextField::onMouseUp(const MouseEvent& e) {
    if (draggingSelection) {
        draggingSelection = false;
        // If selection start equals cursor position, clear selection (no drag happened)
        if (selectionStart == cursorPos) {
            selectionStart = -1;
        }
        getAppState().needsRedraw = true;
    }
    return Widget::onMouseUp(e);
}

bool TextField::onKeyDown(const KeyEvent& e) {
    if (!enabled || !focused) return false;

    // Ensure cursorPos is valid
    if (cursorPos < 0) cursorPos = 0;
    if (cursorPos > static_cast<i32>(text.size())) cursorPos = static_cast<i32>(text.size());

    if (e.keyCode == Key::BACKSPACE) {
        if (readOnly) return true;
        if (hasSelection()) {
            deleteSelection();
        } else if (cursorPos > 0 && cursorPos <= static_cast<i32>(text.size())) {
            text.erase(cursorPos - 1, 1);
            cursorPos--;
            if (onChanged) onChanged(text);
        }
        ensureCaretVisible();
        return true;
    }

    if (e.keyCode == Key::DELETE) {
        if (readOnly) return true;
        if (hasSelection()) {
            deleteSelection();
        } else if (cursorPos >= 0 && cursorPos < static_cast<i32>(text.size())) {
            text.erase(cursorPos, 1);
            if (onChanged) onChanged(text);
        }
        ensureCaretVisible();
        return true;
    }

    if (e.keyCode == Key::LEFT) {
        if (e.mods.shift) {
            // Start or extend selection
            if (selectionStart < 0) selectionStart = cursorPos;
            if (cursorPos > 0) cursorPos--;
        } else {
            // If selection exists, move to start of selection
            if (hasSelection()) {
                cursorPos = std::min(selectionStart, cursorPos);
            } else if (cursorPos > 0) {
                cursorPos--;
            }
            selectionStart = -1;
        }
        ensureCaretVisible();
        return true;
    }

    if (e.keyCode == Key::RIGHT) {
        if (e.mods.shift) {
            // Start or extend selection
            if (selectionStart < 0) selectionStart = cursorPos;
            if (cursorPos < static_cast<i32>(text.size())) cursorPos++;
        } else {
            // If selection exists, move to end of selection
            if (hasSelection()) {
                cursorPos = std::max(selectionStart, cursorPos);
            } else if (cursorPos < static_cast<i32>(text.size())) {
                cursorPos++;
            }
            selectionStart = -1;
        }
        ensureCaretVisible();
        return true;
    }

    if (e.keyCode == Key::HOME) {
        if (e.mods.shift) {
            // Start or extend selection to beginning
            if (selectionStart < 0) selectionStart = cursorPos;
        } else {
            selectionStart = -1;
        }
        cursorPos = 0;
        ensureCaretVisible();
        return true;
    }

    if (e.keyCode == Key::END) {
        if (e.mods.shift) {
            // Start or extend selection to end
            if (selectionStart < 0) selectionStart = cursorPos;
        } else {
            selectionStart = -1;
        }
        cursorPos = static_cast<i32>(text.size());
        ensureCaretVisible();
        return true;
    }

    if (e.keyCode == Key::RETURN) {
        if (onSubmit) onSubmit();
        return true;
    }

    // Ctrl+A: Select all
    if (e.mods.ctrl && e.keyCode == Key::A) {
        selectionStart = 0;
        cursorPos = text.size();
        return true;
    }

    return false;
}

bool TextField::onTextInput(const std::string& input) {
    if (!enabled || !focused || readOnly) return false;

    // Ensure cursorPos is valid before inserting
    if (cursorPos < 0) cursorPos = 0;
    if (cursorPos > static_cast<i32>(text.size())) cursorPos = static_cast<i32>(text.size());

    insertText(input);
    ensureCaretVisible();
    return true;
}

void TextField::onFocus() {
    Widget::onFocus();
    showCursor = true;
    cursorBlinkTime = Platform::getMilliseconds();
    // Clamp cursorPos in case text was modified externally
    if (cursorPos < 0) cursorPos = 0;
    if (cursorPos > static_cast<i32>(text.size())) cursorPos = static_cast<i32>(text.size());
    ensureCaretVisible();
}

void TextField::onBlur() {
    Widget::onBlur();
    selectionStart = -1;
}

void TextField::insertText(const std::string& t) {
    if (hasSelection()) {
        deleteSelection();
    }
    // Clamp cursorPos to valid range
    if (cursorPos < 0) cursorPos = 0;
    if (cursorPos > static_cast<i32>(text.size())) cursorPos = static_cast<i32>(text.size());

    text.insert(cursorPos, t);
    cursorPos += t.size();
    if (onChanged) onChanged(text);
}

void TextField::deleteSelection() {
    if (!hasSelection()) return;
    i32 start = std::min(selectionStart, cursorPos);
    i32 end = std::max(selectionStart, cursorPos);

    // Clamp to valid range
    i32 textLen = static_cast<i32>(text.size());
    start = std::max(0, std::min(start, textLen));
    end = std::max(0, std::min(end, textLen));

    if (start < end && start < textLen) {
        text.erase(start, end - start);
    }
    cursorPos = start;
    selectionStart = -1;
    if (onChanged) onChanged(text);
}

void TextField::ensureCaretVisible() {
    f32 padding = 4 * Config::uiScale;
    f32 visibleWidth = bounds.w - padding * 2;
    if (visibleWidth <= 0) return;

    // Calculate caret X position relative to text start
    i32 safePos = std::max(0, std::min(cursorPos, static_cast<i32>(text.size())));
    std::string beforeCursor = text.substr(0, safePos);
    f32 caretX = FontRenderer::instance().measureText(beforeCursor, fontSize).x;

    // If caret is to the left of visible area, scroll left
    if (caretX < scrollOffset) {
        scrollOffset = caretX;
    }

    // If caret is to the right of visible area, scroll right
    if (caretX > scrollOffset + visibleWidth) {
        scrollOffset = caretX - visibleWidth;
    }

    // Clamp scroll offset (can't scroll past start)
    if (scrollOffset < 0) scrollOffset = 0;
}

i32 TextField::positionFromX(f32 localX) {
    f32 padding = 4 * Config::uiScale;
    f32 clickX = localX - padding + scrollOffset;

    if (clickX <= 0 || text.empty()) return 0;

    for (size_t i = 0; i <= text.size(); ++i) {
        Vec2 textSize = FontRenderer::instance().measureText(text.substr(0, i), fontSize);
        if (textSize.x >= clickX) {
            if (i > 0) {
                Vec2 prevSize = FontRenderer::instance().measureText(text.substr(0, i - 1), fontSize);
                f32 midpoint = (prevSize.x + textSize.x) / 2.0f;
                return (clickX < midpoint) ? static_cast<i32>(i - 1) : static_cast<i32>(i);
            }
            return 0;
        }
    }
    return static_cast<i32>(text.size());
}

// ColorSwatch implementation
void ColorSwatch::renderSelf(Framebuffer& fb) {
    Rect global = globalBounds();

    // Draw checkerboard background for transparency
    if (showCheckerboard && color.a < 255) {
        fb.drawCheckerboard(Recti(global));
    }

    // Draw color
    fb.fillRect(Recti(global), color.toRGBA());

    // Draw border
    fb.drawRect(Recti(global), borderColor);
}

bool ColorSwatch::onMouseDown(const MouseEvent& e) {
    if (!enabled) return false;
    if (e.button == MouseButton::Left && bounds.containsLocal(e.position)) {
        if (onClick) onClick();
        return true;
    }
    return false;
}

// ComboBoxDropdown implementation
void ComboBoxDropdown::renderSelf(Framebuffer& fb) {
    if (!visible || !owner || owner->items.empty()) return;

    Rect global = globalBounds();
    f32 itemHeight = 24 * Config::uiScale;
    f32 padding = 4 * Config::uiScale;

    // Background
    fb.fillRect(Recti(global), bgColor);
    fb.drawRect(Recti(global), borderColor);

    // Items
    for (size_t i = 0; i < owner->items.size(); ++i) {
        f32 itemY = global.y + i * itemHeight;

        // Hover highlight
        if (static_cast<i32>(i) == hoveredIndex) {
            fb.fillRect(
                static_cast<i32>(global.x + 1),
                static_cast<i32>(itemY),
                static_cast<i32>(global.w - 2),
                static_cast<i32>(itemHeight),
                hoverColor
            );
        }

        // Text
        FontRenderer::instance().renderText(fb, owner->items[i],
            static_cast<i32>(global.x + padding),
            static_cast<i32>(itemY + (itemHeight - Config::defaultFontSize()) / 2),
            textColor);
    }
}

bool ComboBoxDropdown::onMouseDown(const MouseEvent& e) {
    if (!visible || !owner || e.button != MouseButton::Left) return false;

    f32 itemHeight = 24 * Config::uiScale;

    // Check if click is inside dropdown
    if (bounds.containsLocal(e.position)) {
        // Find which item was clicked
        i32 clickedIndex = static_cast<i32>(e.position.y / itemHeight);
        if (clickedIndex >= 0 && clickedIndex < static_cast<i32>(owner->items.size())) {
            owner->selectedIndex = clickedIndex;
            if (owner->onSelectionChanged) owner->onSelectionChanged(owner->selectedIndex);
        }
        owner->hideDropdown();
        return true;
    }

    return false;
}

bool ComboBoxDropdown::onMouseMove(const MouseEvent& e) {
    if (!visible || !owner) return false;

    f32 itemHeight = 24 * Config::uiScale;

    if (bounds.containsLocal(e.position)) {
        hoveredIndex = static_cast<i32>(e.position.y / itemHeight);
        if (hoveredIndex >= static_cast<i32>(owner->items.size())) {
            hoveredIndex = -1;
        }
        getAppState().needsRedraw = true;
    } else {
        if (hoveredIndex != -1) {
            hoveredIndex = -1;
            getAppState().needsRedraw = true;
        }
    }

    return true;
}

// ComboBox implementation
void ComboBox::renderSelf(Framebuffer& fb) {
    Rect global = globalBounds();

    // Use dimmed colors when disabled
    u32 actualBgColor = enabled ? bgColor : Config::COLOR_BACKGROUND_DISABLED;
    u32 actualTextColor = enabled ? textColor : Config::COLOR_TEXT_DIM;

    fb.fillRect(Recti(global), actualBgColor);
    fb.drawRect(Recti(global), borderColor);

    f32 padding = 6 * Config::uiScale;

    // Draw selected text
    if (selectedIndex >= 0 && selectedIndex < static_cast<i32>(items.size())) {
        f32 tx = global.x + padding;
        f32 ty = global.y + (global.h - Config::defaultFontSize()) / 2;
        FontRenderer::instance().renderText(fb, items[selectedIndex], static_cast<i32>(tx), static_cast<i32>(ty), actualTextColor);
    }

    // Draw dropdown arrow
    i32 arrowSize = static_cast<i32>(8 * Config::uiScale);
    i32 ax = static_cast<i32>(global.x + global.w - arrowSize - padding);
    i32 ay = static_cast<i32>(global.y + (global.h - arrowSize / 2) / 2);
    fb.drawLine(ax, ay, ax + arrowSize / 2, ay + arrowSize / 2, actualTextColor);
    fb.drawLine(ax + arrowSize / 2, ay + arrowSize / 2, ax + arrowSize, ay, actualTextColor);

    // Note: Dropdown is now rendered by OverlayManager
}

bool ComboBox::onMouseDown(const MouseEvent& e) {
    if (!enabled) return false;  // Ignore when disabled
    if (e.button != MouseButton::Left) return false;

    // Check if click is on combo box itself
    if (bounds.containsLocal(e.position)) {
        if (expanded) {
            hideDropdown();
        } else {
            showDropdown();
        }
        return true;
    }

    return false;
}

bool ComboBox::onMouseMove(const MouseEvent& e) {
    return Widget::onMouseMove(e);
}

void ComboBox::showDropdown() {
    if (!dropdownOverlay) {
        dropdownOverlay = std::make_unique<ComboBoxDropdown>();
        dropdownOverlay->owner = this;
        dropdownOverlay->bgColor = bgColor;
        dropdownOverlay->textColor = textColor;
        dropdownOverlay->borderColor = borderColor;
        dropdownOverlay->hoverColor = hoverColor;
    }

    Rect dropBounds = getDropdownBounds();
    dropdownOverlay->setBounds(dropBounds);
    dropdownOverlay->visible = true;
    dropdownOverlay->hoveredIndex = -1;
    expanded = true;

    OverlayManager::instance().registerOverlay(dropdownOverlay.get(), ZOrder::DROPDOWN,
        [this]() { hideDropdown(); });

    getAppState().needsRedraw = true;
}

void ComboBox::hideDropdown() {
    if (dropdownOverlay) {
        dropdownOverlay->visible = false;
        OverlayManager::instance().unregisterOverlay(dropdownOverlay.get());
    }
    expanded = false;
    getAppState().needsRedraw = true;
}

// Panel implementations
void Panel::layout() {
    // Set children to fill content area
    Rect content = contentRect();
    for (auto& child : children) {
        if (!child->visible) continue;
        child->setBounds(
            content.x + child->marginLeft,
            content.y + child->marginTop,
            content.w - child->marginLeft - child->marginRight,
            content.h - child->marginTop - child->marginBottom
        );
        child->layout();
    }
}

void Panel::renderSelf(Framebuffer& fb) {
    Rect global = globalBounds();
    fb.fillRect(Recti(global), bgColor);
    if (borderWidth > 0 && borderColor != 0) {
        fb.drawRect(Recti(global), borderColor, borderWidth);
    }
}

// PopupMenu implementations
void PopupMenu::show(f32 x, f32 y) {
    // Calculate size based on items
    f32 itemHeight = 24 * Config::uiScale;
    f32 separatorHeight = 8 * Config::uiScale;
    f32 width = 180 * Config::uiScale;
    f32 height = 4 * Config::uiScale;  // Padding

    for (const auto& item : items) {
        height += item.separator ? separatorHeight : itemHeight;
    }

    setBounds(x, y, width, height);
    visible = true;
    hoveredIndex = -1;
    getAppState().needsRedraw = true;
}

void PopupMenu::hide() {
    visible = false;
    hoveredIndex = -1;
    if (onClose) onClose();
    getAppState().needsRedraw = true;
}

void PopupMenu::renderSelf(Framebuffer& fb) {
    if (!visible) return;

    Rect global = globalBounds();

    // Background
    fb.fillRect(Recti(global), bgColor);
    fb.drawRect(Recti(global), borderColor);

    // Items
    f32 itemHeight = 24 * Config::uiScale;
    f32 separatorHeight = 8 * Config::uiScale;
    f32 y = global.y + 2 * Config::uiScale;
    f32 padding = 8 * Config::uiScale;

    for (size_t i = 0; i < items.size(); ++i) {
        const auto& item = items[i];

        if (item.separator) {
            f32 lineY = y + separatorHeight / 2;
            fb.drawHorizontalLine(
                static_cast<i32>(global.x + padding),
                static_cast<i32>(global.x + global.w - padding),
                static_cast<i32>(lineY),
                borderColor
            );
            y += separatorHeight;
        } else {
            // Hover highlight
            if (static_cast<i32>(i) == hoveredIndex && item.enabled) {
                fb.fillRect(
                    static_cast<i32>(global.x + 2),
                    static_cast<i32>(y),
                    static_cast<i32>(global.w - 4),
                    static_cast<i32>(itemHeight),
                    hoverColor
                );
            }

            // Label
            u32 color = item.enabled ? textColor : disabledColor;
            FontRenderer::instance().renderText(fb, item.label,
                static_cast<i32>(global.x + padding),
                static_cast<i32>(y + (itemHeight - Config::defaultFontSize()) / 2),
                color);

            // Shortcut
            if (!item.shortcut.empty()) {
                Vec2 shortcutSize = FontRenderer::instance().measureText(item.shortcut, Config::defaultFontSize());
                FontRenderer::instance().renderText(fb, item.shortcut,
                    static_cast<i32>(global.x + global.w - padding - shortcutSize.x),
                    static_cast<i32>(y + (itemHeight - Config::defaultFontSize()) / 2),
                    disabledColor);
            }

            y += itemHeight;
        }
    }
}

bool PopupMenu::onMouseMove(const MouseEvent& e) {
    if (!visible) return false;

    // Check if mouse is within menu bounds horizontally
    if (!bounds.containsLocal(e.position)) {
        if (hoveredIndex != -1) {
            hoveredIndex = -1;
            getAppState().needsRedraw = true;
        }
        return true;
    }

    f32 itemHeight = 24 * Config::uiScale;
    f32 separatorHeight = 8 * Config::uiScale;
    f32 y = 2 * Config::uiScale;

    hoveredIndex = -1;
    for (size_t i = 0; i < items.size(); ++i) {
        const auto& item = items[i];
        f32 h = item.separator ? separatorHeight : itemHeight;

        if (!item.separator && e.position.y >= y && e.position.y < y + h) {
            hoveredIndex = static_cast<i32>(i);
            break;
        }
        y += h;
    }

    getAppState().needsRedraw = true;
    return true;
}

bool PopupMenu::onMouseDown(const MouseEvent& e) {
    if (!visible) return false;

    if (!bounds.containsLocal(e.position)) {
        hide();
        return false;
    }

    if (e.button == MouseButton::Left && hoveredIndex >= 0) {
        const auto& item = items[hoveredIndex];
        if (item.enabled && item.action) {
            item.action();
        }
        hide();
        return true;
    }

    return true;
}

// TabBar implementations
bool TabBar::isOverCloseButton(f32 x, f32 y, i32 tabIndex) const {
    if (tabIndex < 0 || tabIndex >= static_cast<i32>(tabs.size())) return false;
    if (!tabs[tabIndex].closable) return false;

    f32 tabX = 0;
    for (i32 i = 0; i < tabIndex; ++i) {
        tabX += getTabWidth(tabs[i]);
    }

    f32 tabWidth = getTabWidth(tabs[tabIndex]);
    f32 closeX = tabX + tabWidth - tabPadding - closeButtonSize;
    f32 closeY = (tabHeight - closeButtonSize) / 2;

    return x >= closeX && x < closeX + closeButtonSize &&
           y >= closeY && y < closeY + closeButtonSize;
}

void TabBar::renderSelf(Framebuffer& fb) {
    Rect global = globalBounds();

    // Background
    fb.fillRect(Recti(global), bgColor);

    // Draw tabs
    f32 tabX = global.x;
    for (size_t i = 0; i < tabs.size(); ++i) {
        const Tab& tab = tabs[i];
        f32 width = getTabWidth(tab);
        bool isActive = (static_cast<i32>(i) == activeIndex);
        bool isHovered = (static_cast<i32>(i) == hoveredIndex);

        // Tab background
        u32 bg = isActive ? activeTabColor : (isHovered ? hoverColor : tabColor);
        fb.fillRect(
            static_cast<i32>(tabX),
            static_cast<i32>(global.y),
            static_cast<i32>(width),
            static_cast<i32>(tabHeight),
            bg
        );

        // Tab border (bottom line for inactive tabs)
        if (!isActive) {
            fb.drawHorizontalLine(
                static_cast<i32>(tabX),
                static_cast<i32>(tabX + width),
                static_cast<i32>(global.y + tabHeight - 1),
                Config::COLOR_BORDER
            );
        }

        // Right border between tabs
        fb.drawVerticalLine(
            static_cast<i32>(tabX + width - 1),
            static_cast<i32>(global.y),
            static_cast<i32>(global.y + tabHeight),
            Config::COLOR_BORDER
        );

        // Tab label (truncated if needed)
        std::string displayLabel = getDisplayLabel(tab.label);
        Vec2 textSize = FontRenderer::instance().measureText(displayLabel, Config::defaultFontSize());
        f32 textX = tabX + tabPadding;
        f32 textY = global.y + (tabHeight - textSize.y) / 2;
        FontRenderer::instance().renderText(fb, displayLabel,
            static_cast<i32>(textX), static_cast<i32>(textY), textColor);

        // Close button
        if (tab.closable) {
            f32 closeX = tabX + width - tabPadding - closeButtonSize;
            f32 closeY = global.y + (tabHeight - closeButtonSize) / 2;
            bool closeHovered = (static_cast<i32>(i) == hoveredCloseIndex);

            u32 closeColor = closeHovered ? closeButtonHoverColor : closeButtonColor;

            // Draw X
            i32 cx = static_cast<i32>(closeX);
            i32 cy = static_cast<i32>(closeY);
            i32 cs = static_cast<i32>(closeButtonSize);
            i32 margin = cs / 4;

            // Draw the X lines
            for (i32 d = 0; d < 2; ++d) {
                fb.drawLine(cx + margin + d, cy + margin, cx + cs - margin + d, cy + cs - margin, closeColor);
                fb.drawLine(cx + cs - margin - d, cy + margin, cx + margin - d, cy + cs - margin, closeColor);
            }
        }

        tabX += width;
    }

    // Bottom border for remaining space
    if (tabX < global.x + global.w) {
        fb.drawHorizontalLine(
            static_cast<i32>(tabX),
            static_cast<i32>(global.x + global.w),
            static_cast<i32>(global.y + tabHeight - 1),
            Config::COLOR_BORDER
        );
    }
}

bool TabBar::onMouseDown(const MouseEvent& e) {
    if (!bounds.containsLocal(e.position)) return false;

    i32 tabIndex = getTabAtPosition(e.position.x);
    if (tabIndex >= 0) {
        if (isOverCloseButton(e.position.x, e.position.y, tabIndex)) {
            // Close button clicked
            if (onTabClosed) {
                onTabClosed(tabIndex);
            }
        } else {
            // Tab clicked - select it
            if (tabIndex != activeIndex) {
                activeIndex = tabIndex;
                if (onTabSelected) {
                    onTabSelected(tabIndex);
                }
            }
        }
        return true;
    }
    return false;
}

bool TabBar::onMouseMove(const MouseEvent& e) {
    if (!bounds.containsLocal(e.position)) {
        if (hoveredIndex != -1 || hoveredCloseIndex != -1) {
            hoveredIndex = -1;
            hoveredCloseIndex = -1;
            getAppState().needsRedraw = true;
        }
        return false;
    }

    i32 newHoveredIndex = getTabAtPosition(e.position.x);
    i32 newHoveredCloseIndex = -1;

    if (newHoveredIndex >= 0 && isOverCloseButton(e.position.x, e.position.y, newHoveredIndex)) {
        newHoveredCloseIndex = newHoveredIndex;
    }

    if (newHoveredIndex != hoveredIndex || newHoveredCloseIndex != hoveredCloseIndex) {
        hoveredIndex = newHoveredIndex;
        hoveredCloseIndex = newHoveredCloseIndex;
        getAppState().needsRedraw = true;
    }

    return true;
}
