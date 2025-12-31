#ifndef _H_FRAMEBUFFER_
#define _H_FRAMEBUFFER_

#include "types.h"
#include "primitives.h"
#include "blend.h"
#include "config.h"
#include <vector>

class Framebuffer {
public:
#ifdef __EMSCRIPTEN__
    // WASM: Store in canvas RGBA byte order for zero-copy blit to JavaScript
    std::vector<u8> pixels;
#else
    std::vector<u32> pixels;
#endif
    u32 width = 0;
    u32 height = 0;

    // Clipping stack (in screen coordinates)
    std::vector<Recti> clipStack;

    Framebuffer() = default;
#ifdef __EMSCRIPTEN__
    Framebuffer(u32 w, u32 h) : width(w), height(h), pixels(w * h * 4, 0) {}
#else
    Framebuffer(u32 w, u32 h) : width(w), height(h), pixels(w * h, 0) {}
#endif

    // Clipping support
    void pushClip(const Recti& rect);
    void popClip();
    bool hasClip() const { return !clipStack.empty(); }
    const Recti& currentClip() const { return clipStack.back(); }
    bool isClipped(i32 x, i32 y) const;

    // Basic operations
    void resize(u32 w, u32 h);
    void clear(u32 color = 0xFF000000);
    void clearRect(const Recti& rect, u32 color);

    // Pixel access
    u32 getPixel(i32 x, i32 y) const;
    void setPixel(i32 x, i32 y, u32 color);
    void blendPixel(i32 x, i32 y, u32 color);

    // Drawing primitives
    void fillRect(const Recti& rect, u32 color);
    void fillRect(i32 x, i32 y, i32 w, i32 h, u32 color);
    void drawRect(const Recti& rect, u32 color, i32 thickness = 1);

    void drawLine(i32 x0, i32 y0, i32 x1, i32 y1, u32 color);
    void drawLine(const Vec2& from, const Vec2& to, u32 color);

    void drawCircle(i32 cx, i32 cy, i32 radius, u32 color, i32 thickness = 1);
    void fillCircle(i32 cx, i32 cy, i32 radius, u32 color);

    void drawHorizontalLine(i32 x0, i32 x1, i32 y, u32 color);
    void drawVerticalLine(i32 x, i32 y0, i32 y1, u32 color);

    // Patterns
    void drawCheckerboard(const Recti& rect, u32 color1 = Config::CHECKER_COLOR1,
                          u32 color2 = Config::CHECKER_COLOR2, u32 size = Config::CHECKER_SIZE);

    // Blitting
    void blit(const Framebuffer& src, i32 dx, i32 dy);
    void blit(const Framebuffer& src, i32 dx, i32 dy, const Recti& srcRect);
    void blitBlend(const Framebuffer& src, i32 dx, i32 dy);

    // Data access
#ifdef __EMSCRIPTEN__
    u8* data() { return pixels.data(); }
    const u8* data() const { return pixels.data(); }
    size_t size() const { return width * height; }  // Pixel count
    size_t byteSize() const { return pixels.size(); }
#else
    u32* data() { return pixels.data(); }
    const u32* data() const { return pixels.data(); }
    size_t size() const { return pixels.size(); }
    size_t byteSize() const { return pixels.size() * sizeof(u32); }
#endif

private:
    void drawCircleSingle(i32 cx, i32 cy, i32 radius, u32 color);
};

#endif
