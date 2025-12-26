#include "image_io.h"
#include "compositor.h"
#include "blend.h"
#include "platform.h"
#include "sampler.h"
#include <cmath>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace ImageIO {

bool loadImage(const std::string& path, TiledCanvas& canvas) {
    i32 w, h, channels;
    u8* data = stbi_load(path.c_str(), &w, &h, &channels, 4);  // Force RGBA

    if (!data) {
        return false;
    }

    canvas.resize(w, h);
    canvas.clear();

    // Copy pixel data
    for (i32 y = 0; y < h; ++y) {
        for (i32 x = 0; x < w; ++x) {
            i32 idx = (y * w + x) * 4;
            u8 r = data[idx];
            u8 g = data[idx + 1];
            u8 b = data[idx + 2];
            u8 a = data[idx + 3];

            if (a > 0) {
                canvas.setPixel(x, y, Blend::pack(r, g, b, a));
            }
        }
    }

    stbi_image_free(data);
    return true;
}

bool saveImagePNG(const std::string& path, const TiledCanvas& canvas) {
    std::vector<u8> pixels(canvas.width * canvas.height * 4);

    // Convert from RGBA packed to RGBA bytes
    for (u32 y = 0; y < canvas.height; ++y) {
        for (u32 x = 0; x < canvas.width; ++x) {
            u32 pixel = canvas.getPixel(x, y);
            u8 r, g, b, a;
            Blend::unpack(pixel, r, g, b, a);

            i32 idx = (y * canvas.width + x) * 4;
            pixels[idx] = r;
            pixels[idx + 1] = g;
            pixels[idx + 2] = b;
            pixels[idx + 3] = a;
        }
    }

    return stbi_write_png(path.c_str(), canvas.width, canvas.height, 4, pixels.data(),
                          canvas.width * 4) != 0;
}

std::unique_ptr<Document> loadAsDocument(const std::string& path) {
    i32 w, h, channels;
    u8* data = stbi_load(path.c_str(), &w, &h, &channels, 4);

    if (!data) {
        return nullptr;
    }

    auto doc = std::make_unique<Document>(w, h, Platform::getFileName(path));
    doc->filePath = path;

    // Get the background layer
    PixelLayer* layer = doc->getActivePixelLayer();
    if (!layer) {
        stbi_image_free(data);
        return nullptr;
    }

    // Copy pixel data
    for (i32 y = 0; y < h; ++y) {
        for (i32 x = 0; x < w; ++x) {
            i32 idx = (y * w + x) * 4;
            u8 r = data[idx];
            u8 g = data[idx + 1];
            u8 b = data[idx + 2];
            u8 a = data[idx + 3];

            if (a > 0) {
                layer->canvas.setPixel(x, y, Blend::pack(r, g, b, a));
            }
        }
    }

    stbi_image_free(data);
    return doc;
}

bool exportPNG(const std::string& path, const Document& doc) {
    // Create output buffer (RGBA)
    std::vector<u8> pixels(doc.width * doc.height * 4, 0);

    // Composite each pixel - same logic as compositeDocument but to a flat buffer
    for (u32 y = 0; y < doc.height; ++y) {
        for (u32 x = 0; x < doc.width; ++x) {
            u32 composited = 0;  // Start transparent

            for (const auto& layer : doc.layers) {
                if (!layer->visible) continue;

                u32 layerPixel = 0;

                if (layer->isPixelLayer()) {
                    const PixelLayer* pixelLayer = static_cast<const PixelLayer*>(layer.get());

                    // Check if layer has rotation or scale
                    bool hasTransform = layer->transform.rotation != 0.0f ||
                                        layer->transform.scale.x != 1.0f ||
                                        layer->transform.scale.y != 1.0f;

                    f32 layerX, layerY;

                    if (hasTransform) {
                        // Apply full transform using inverse matrix
                        Matrix3x2 mat = layer->transform.toMatrix(
                            pixelLayer->canvas.width, pixelLayer->canvas.height);
                        Matrix3x2 invMat = mat.inverted();
                        Vec2 srcCoord = invMat.transform(Vec2(static_cast<f32>(x), static_cast<f32>(y)));
                        layerX = srcCoord.x;
                        layerY = srcCoord.y;

                        // Use bilinear sampling for transformed content
                        layerPixel = Sampler::sample(pixelLayer->canvas, layerX, layerY, SampleMode::Bilinear);
                    } else {
                        // Position-only offset
                        layerX = static_cast<f32>(x) - layer->transform.position.x;
                        layerY = static_cast<f32>(y) - layer->transform.position.y;

                        i32 ix = static_cast<i32>(std::floor(layerX));
                        i32 iy = static_cast<i32>(std::floor(layerY));
                        layerPixel = pixelLayer->canvas.getPixel(ix, iy);
                    }
                }
                else if (layer->isTextLayer()) {
                    TextLayer* textLayer = const_cast<TextLayer*>(static_cast<const TextLayer*>(layer.get()));
                    textLayer->ensureCacheValid();

                    f32 layerX, layerY;
                    bool needsBilinear = layer->transform.scale.x != 1.0f ||
                                         layer->transform.scale.y != 1.0f ||
                                         layer->transform.rotation != 0.0f;

                    if (layer->transform.isIdentity()) {
                        layerX = static_cast<f32>(x);
                        layerY = static_cast<f32>(y);
                    } else if (!needsBilinear) {
                        layerX = static_cast<f32>(x) - layer->transform.position.x;
                        layerY = static_cast<f32>(y) - layer->transform.position.y;
                    } else {
                        Matrix3x2 mat = layer->transform.toMatrix(
                            textLayer->rasterizedCache.width, textLayer->rasterizedCache.height);
                        Matrix3x2 invMat = mat.inverted();
                        Vec2 layerCoord = invMat.transform(Vec2(static_cast<f32>(x), static_cast<f32>(y)));
                        layerX = layerCoord.x;
                        layerY = layerCoord.y;
                    }

                    if (needsBilinear) {
                        layerPixel = Sampler::sample(textLayer->rasterizedCache, layerX, layerY, SampleMode::Bilinear);
                    } else {
                        i32 ix = static_cast<i32>(layerX);
                        i32 iy = static_cast<i32>(layerY);
                        if (ix >= 0 && iy >= 0 &&
                            ix < static_cast<i32>(textLayer->rasterizedCache.width) &&
                            iy < static_cast<i32>(textLayer->rasterizedCache.height)) {
                            layerPixel = textLayer->rasterizedCache.getPixel(ix, iy);
                        }
                    }
                }
                else if (layer->isAdjustmentLayer()) {
                    // Apply adjustment to composited result so far
                    const AdjustmentLayer* adj = static_cast<const AdjustmentLayer*>(layer.get());
                    composited = Compositor::applyAdjustment(composited, *adj);
                    continue;
                }

                if ((layerPixel & 0xFF) > 0) {
                    composited = Blend::blend(composited, layerPixel, layer->blend, layer->opacity);
                }
            }

            // Write to output buffer
            u8 r, g, b, a;
            Blend::unpack(composited, r, g, b, a);
            u32 idx = (y * doc.width + x) * 4;
            pixels[idx] = r;
            pixels[idx + 1] = g;
            pixels[idx + 2] = b;
            pixels[idx + 3] = a;
        }
    }

    return stbi_write_png(path.c_str(), doc.width, doc.height, 4, pixels.data(),
                          doc.width * 4) != 0;
}

bool getImageSize(const std::string& path, u32& width, u32& height) {
    i32 w, h, channels;
    if (stbi_info(path.c_str(), &w, &h, &channels)) {
        width = w;
        height = h;
        return true;
    }
    return false;
}

bool isSupported(const std::string& path) {
    std::string ext = Platform::getFileExtension(path);
    // Convert to lowercase
    for (auto& c : ext) c = std::tolower(c);

    return ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
           ext == ".bmp" || ext == ".gif" || ext == ".tga" ||
           ext == ".psd" || ext == ".hdr";
}

}
