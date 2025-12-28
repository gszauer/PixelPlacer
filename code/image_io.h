#ifndef _H_IMAGE_IO_
#define _H_IMAGE_IO_

#include "types.h"
#include "primitives.h"
#include "tiled_canvas.h"
#include "document.h"
#include "layer.h"
#include <string>
#include <vector>
#include <memory>

namespace ImageIO {
    // Load image from file into a TiledCanvas
    bool loadImage(const std::string& path, TiledCanvas& canvas);

    // Save canvas to PNG file
    bool saveImagePNG(const std::string& path, const TiledCanvas& canvas);

    // Load image as new document
    std::unique_ptr<Document> loadAsDocument(const std::string& path);

    // Export document to PNG (flattens all layers)
    bool exportPNG(const std::string& path, const Document& doc);

    // Get image dimensions without loading full image
    bool getImageSize(const std::string& path, u32& width, u32& height);

    // Supported formats
    bool isSupported(const std::string& path);
}

#endif
