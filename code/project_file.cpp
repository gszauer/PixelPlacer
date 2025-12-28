#include "project_file.h"
#include "layer.h"
#include "blend.h"
#include "platform.h"
#include <fstream>
#include <cstring>

namespace ProjectFile {

// Helper to write primitives
template<typename T>
void writeValue(std::ostream& out, T value) {
    out.write(reinterpret_cast<const char*>(&value), sizeof(T));
}

template<typename T>
T readValue(std::istream& in) {
    T value;
    in.read(reinterpret_cast<char*>(&value), sizeof(T));
    return value;
}

void writeString(std::ostream& out, const std::string& str) {
    u32 len = str.size();
    writeValue(out, len);
    out.write(str.c_str(), len);
}

std::string readString(std::istream& in) {
    u32 len = readValue<u32>(in);
    std::string str(len, '\0');
    in.read(&str[0], len);
    return str;
}

bool save(const std::string& path, const Document& doc) {
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    // Header
    writeValue(file, MAGIC);
    writeValue(file, VERSION);
    writeValue(file, doc.width);
    writeValue(file, doc.height);
    writeValue(file, static_cast<u32>(doc.layers.size()));

    // Embedded fonts (VERSION 2+)
    writeValue(file, static_cast<u32>(doc.embeddedFonts.size()));
    for (const auto& [fontName, fontData] : doc.embeddedFonts) {
        writeString(file, fontName);
        writeValue(file, static_cast<u32>(fontData.size()));
        file.write(reinterpret_cast<const char*>(fontData.data()), fontData.size());
    }

    // Layers
    for (const auto& layer : doc.layers) {
        // Layer type
        u8 layerType = 0;
        if (layer->isPixelLayer()) layerType = 0;
        else if (layer->isTextLayer()) layerType = 1;
        else if (layer->isAdjustmentLayer()) layerType = 2;
        writeValue(file, layerType);

        // Common properties
        writeString(file, layer->name);
        writeValue(file, layer->visible);
        writeValue(file, layer->locked);
        writeValue(file, layer->opacity);
        writeValue(file, static_cast<u8>(layer->blend));
        writeValue(file, layer->transform.position.x);
        writeValue(file, layer->transform.position.y);
        writeValue(file, layer->transform.scale.x);
        writeValue(file, layer->transform.scale.y);
        writeValue(file, layer->transform.rotation);

        if (layer->isPixelLayer()) {
            const PixelLayer* pixel = static_cast<const PixelLayer*>(layer.get());
            const TiledCanvas& canvas = pixel->canvas;

            writeValue(file, static_cast<u32>(canvas.tiles.size()));

            for (const auto& [key, tile] : canvas.tiles) {
                i32 tileX, tileY;
                extractTileCoords(key, tileX, tileY);
                writeValue(file, tileX);  // Signed coordinates
                writeValue(file, tileY);  // Signed coordinates

                // Write tile data
                file.write(reinterpret_cast<const char*>(tile->pixels), sizeof(tile->pixels));
            }
        }
        else if (layer->isTextLayer()) {
            const TextLayer* text = static_cast<const TextLayer*>(layer.get());
            writeString(file, text->text);
            writeString(file, text->fontFamily);
            writeValue(file, text->fontSize);
            writeValue(file, text->textColor.r);
            writeValue(file, text->textColor.g);
            writeValue(file, text->textColor.b);
            writeValue(file, text->textColor.a);
            writeValue(file, text->bold);
            writeValue(file, text->italic);
        }
        else if (layer->isAdjustmentLayer()) {
            const AdjustmentLayer* adj = static_cast<const AdjustmentLayer*>(layer.get());
            writeValue(file, static_cast<u8>(adj->type));
            // Write params based on type using variant visitor
            std::visit([&file](const auto& p) {
                using T = std::decay_t<decltype(p)>;
                if constexpr (std::is_same_v<T, BrightnessContrastParams>) {
                    writeValue(file, p.brightness);
                    writeValue(file, p.contrast);
                }
                else if constexpr (std::is_same_v<T, TemperatureTintParams>) {
                    writeValue(file, p.temperature);
                    writeValue(file, p.tint);
                }
                else if constexpr (std::is_same_v<T, HueSaturationParams>) {
                    writeValue(file, p.hue);
                    writeValue(file, p.saturation);
                    writeValue(file, p.lightness);
                }
                else if constexpr (std::is_same_v<T, VibranceParams>) {
                    writeValue(file, p.vibrance);
                }
                else if constexpr (std::is_same_v<T, ColorBalanceParams>) {
                    writeValue(file, p.shadowsCyanRed);
                    writeValue(file, p.shadowsMagentaGreen);
                    writeValue(file, p.shadowsYellowBlue);
                    writeValue(file, p.midtonesCyanRed);
                    writeValue(file, p.midtonesMagentaGreen);
                    writeValue(file, p.midtonesYellowBlue);
                    writeValue(file, p.highlightsCyanRed);
                    writeValue(file, p.highlightsMagentaGreen);
                    writeValue(file, p.highlightsYellowBlue);
                }
                else if constexpr (std::is_same_v<T, HighlightsShadowsParams>) {
                    writeValue(file, p.highlights);
                    writeValue(file, p.shadows);
                }
                else if constexpr (std::is_same_v<T, ExposureParams>) {
                    writeValue(file, p.exposure);
                    writeValue(file, p.offset);
                    writeValue(file, p.gamma);
                }
                else if constexpr (std::is_same_v<T, LevelsParams>) {
                    writeValue(file, p.inputBlack);
                    writeValue(file, p.inputGamma);
                    writeValue(file, p.inputWhite);
                    writeValue(file, p.outputBlack);
                    writeValue(file, p.outputWhite);
                }
                else if constexpr (std::is_same_v<T, InvertParams>) {
                    // No parameters
                }
                else if constexpr (std::is_same_v<T, BlackAndWhiteParams>) {
                    writeValue(file, p.reds);
                    writeValue(file, p.yellows);
                    writeValue(file, p.greens);
                    writeValue(file, p.cyans);
                    writeValue(file, p.blues);
                    writeValue(file, p.magentas);
                    writeValue(file, p.tintHue);
                    writeValue(file, p.tintAmount);
                }
            }, adj->params);
        }
    }

    return file.good();
}

std::unique_ptr<Document> load(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return nullptr;

    // Verify header
    u32 magic = readValue<u32>(file);
    if (magic != MAGIC) return nullptr;

    u32 version = readValue<u32>(file);
    if (version > VERSION) return nullptr;  // Newer format

    u32 width = readValue<u32>(file);
    u32 height = readValue<u32>(file);
    u32 layerCount = readValue<u32>(file);

    auto doc = std::make_unique<Document>();
    doc->width = width;
    doc->height = height;
    doc->selection.resize(width, height);
    doc->filePath = path;
    doc->name = Platform::getFileName(path);
    doc->layers.clear();  // Remove default layer

    // Read embedded fonts (VERSION 2+)
    if (version >= 2) {
        u32 fontCount = readValue<u32>(file);
        for (u32 f = 0; f < fontCount; ++f) {
            std::string fontName = readString(file);
            u32 dataSize = readValue<u32>(file);
            std::vector<u8> fontData(dataSize);
            file.read(reinterpret_cast<char*>(fontData.data()), dataSize);
            doc->embeddedFonts[fontName] = std::move(fontData);
        }
    }

    // Read layers
    for (u32 i = 0; i < layerCount; ++i) {
        u8 layerType = readValue<u8>(file);

        std::unique_ptr<LayerBase> layer;

        // Read common properties first
        std::string name = readString(file);
        bool visible = readValue<bool>(file);
        bool locked = readValue<bool>(file);
        f32 opacity = readValue<f32>(file);
        BlendMode blend = static_cast<BlendMode>(readValue<u8>(file));
        f32 posX = readValue<f32>(file);
        f32 posY = readValue<f32>(file);
        f32 scaleX = readValue<f32>(file);
        f32 scaleY = readValue<f32>(file);
        f32 rotation = readValue<f32>(file);

        if (layerType == 0) {
            // Pixel layer
            auto pixel = std::make_unique<PixelLayer>(width, height);
            u32 tileCount = readValue<u32>(file);

            for (u32 t = 0; t < tileCount; ++t) {
                i32 tileX = readValue<i32>(file);  // Signed coordinates
                i32 tileY = readValue<i32>(file);  // Signed coordinates

                auto tile = std::make_unique<Tile>();
                file.read(reinterpret_cast<char*>(tile->pixels), sizeof(tile->pixels));

                u64 key = makeTileKey(tileX, tileY);
                pixel->canvas.tiles[key] = std::move(tile);
            }

            layer = std::move(pixel);
        }
        else if (layerType == 1) {
            // Text layer
            auto text = std::make_unique<TextLayer>();
            text->text = readString(file);
            text->fontFamily = readString(file);
            text->fontSize = readValue<u32>(file);
            text->textColor.r = readValue<u8>(file);
            text->textColor.g = readValue<u8>(file);
            text->textColor.b = readValue<u8>(file);
            text->textColor.a = readValue<u8>(file);
            text->bold = readValue<bool>(file);
            text->italic = readValue<bool>(file);
            layer = std::move(text);
        }
        else if (layerType == 2) {
            // Adjustment layer
            AdjustmentType adjType = static_cast<AdjustmentType>(readValue<u8>(file));
            auto adj = std::make_unique<AdjustmentLayer>(adjType);

            // Deserialize params based on type
            switch (adjType) {
                case AdjustmentType::BrightnessContrast: {
                    BrightnessContrastParams p;
                    p.brightness = readValue<f32>(file);
                    p.contrast = readValue<f32>(file);
                    adj->params = p;
                    break;
                }
                case AdjustmentType::TemperatureTint: {
                    TemperatureTintParams p;
                    p.temperature = readValue<f32>(file);
                    p.tint = readValue<f32>(file);
                    adj->params = p;
                    break;
                }
                case AdjustmentType::HueSaturation: {
                    HueSaturationParams p;
                    p.hue = readValue<f32>(file);
                    p.saturation = readValue<f32>(file);
                    p.lightness = readValue<f32>(file);
                    adj->params = p;
                    break;
                }
                case AdjustmentType::Vibrance: {
                    VibranceParams p;
                    p.vibrance = readValue<f32>(file);
                    adj->params = p;
                    break;
                }
                case AdjustmentType::ColorBalance: {
                    ColorBalanceParams p;
                    p.shadowsCyanRed = readValue<f32>(file);
                    p.shadowsMagentaGreen = readValue<f32>(file);
                    p.shadowsYellowBlue = readValue<f32>(file);
                    p.midtonesCyanRed = readValue<f32>(file);
                    p.midtonesMagentaGreen = readValue<f32>(file);
                    p.midtonesYellowBlue = readValue<f32>(file);
                    p.highlightsCyanRed = readValue<f32>(file);
                    p.highlightsMagentaGreen = readValue<f32>(file);
                    p.highlightsYellowBlue = readValue<f32>(file);
                    adj->params = p;
                    break;
                }
                case AdjustmentType::HighlightsShadows: {
                    HighlightsShadowsParams p;
                    p.highlights = readValue<f32>(file);
                    p.shadows = readValue<f32>(file);
                    adj->params = p;
                    break;
                }
                case AdjustmentType::Exposure: {
                    ExposureParams p;
                    p.exposure = readValue<f32>(file);
                    p.offset = readValue<f32>(file);
                    p.gamma = readValue<f32>(file);
                    adj->params = p;
                    break;
                }
                case AdjustmentType::Levels: {
                    LevelsParams p;
                    p.inputBlack = readValue<f32>(file);
                    p.inputGamma = readValue<f32>(file);
                    p.inputWhite = readValue<f32>(file);
                    p.outputBlack = readValue<f32>(file);
                    p.outputWhite = readValue<f32>(file);
                    adj->params = p;
                    break;
                }
                case AdjustmentType::Invert: {
                    adj->params = InvertParams();
                    break;
                }
                case AdjustmentType::BlackAndWhite: {
                    BlackAndWhiteParams p;
                    p.reds = readValue<f32>(file);
                    p.yellows = readValue<f32>(file);
                    p.greens = readValue<f32>(file);
                    p.cyans = readValue<f32>(file);
                    p.blues = readValue<f32>(file);
                    p.magentas = readValue<f32>(file);
                    p.tintHue = readValue<f32>(file);
                    p.tintAmount = readValue<f32>(file);
                    adj->params = p;
                    break;
                }
            }
            layer = std::move(adj);
        }

        if (layer) {
            layer->name = name;
            layer->visible = visible;
            layer->locked = locked;
            layer->opacity = opacity;
            layer->blend = blend;
            layer->transform.position = Vec2(posX, posY);
            layer->transform.scale = Vec2(scaleX, scaleY);
            layer->transform.rotation = rotation;
            doc->layers.push_back(std::move(layer));
        }
    }

    if (!doc->layers.empty()) {
        doc->activeLayerIndex = 0;
    }

    return doc;
}

bool isProjectFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    u32 magic = readValue<u32>(file);
    return magic == MAGIC;
}

}
