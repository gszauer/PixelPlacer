#include "project_file.h"
#include "layer.h"
#include "blend.h"
#include "platform.h"
#include <cstring>

namespace ProjectFile {

// Simple buffer writer for binary serialization
class BufferWriter {
    std::vector<u8>& buffer;
public:
    explicit BufferWriter(std::vector<u8>& buf) : buffer(buf) {}

    template<typename T>
    void write(T value) {
        const u8* ptr = reinterpret_cast<const u8*>(&value);
        buffer.insert(buffer.end(), ptr, ptr + sizeof(T));
    }

    void writeBytes(const void* data, size_t size) {
        const u8* ptr = static_cast<const u8*>(data);
        buffer.insert(buffer.end(), ptr, ptr + size);
    }

    void writeString(const std::string& str) {
        write(static_cast<u32>(str.size()));
        writeBytes(str.data(), str.size());
    }
};

// Simple buffer reader for binary deserialization
class BufferReader {
    const u8* data;
    size_t size;
    size_t pos = 0;
public:
    BufferReader(const u8* d, size_t s) : data(d), size(s) {}
    BufferReader(const std::vector<u8>& buf) : data(buf.data()), size(buf.size()) {}

    bool good() const { return pos <= size; }
    bool eof() const { return pos >= size; }

    template<typename T>
    T read() {
        if (pos + sizeof(T) > size) {
            pos = size + 1;  // Mark as bad
            return T{};
        }
        T value;
        std::memcpy(&value, data + pos, sizeof(T));
        pos += sizeof(T);
        return value;
    }

    void readBytes(void* dest, size_t count) {
        if (pos + count > size) {
            pos = size + 1;
            return;
        }
        std::memcpy(dest, data + pos, count);
        pos += count;
    }

    std::string readString() {
        u32 len = read<u32>();
        if (!good() || pos + len > size) {
            pos = size + 1;
            return "";
        }
        std::string str(reinterpret_cast<const char*>(data + pos), len);
        pos += len;
        return str;
    }
};

bool save(const std::string& path, const Document& doc) {
    std::vector<u8> buffer;
    buffer.reserve(1024 * 1024);  // Pre-allocate 1MB
    BufferWriter writer(buffer);

    // Header
    writer.write(MAGIC);
    writer.write(VERSION);
    writer.write(doc.width);
    writer.write(doc.height);
    writer.write(static_cast<u32>(doc.layers.size()));

    // Embedded fonts (VERSION 2+)
    writer.write(static_cast<u32>(doc.embeddedFonts.size()));
    for (const auto& [fontName, fontData] : doc.embeddedFonts) {
        writer.writeString(fontName);
        writer.write(static_cast<u32>(fontData.size()));
        writer.writeBytes(fontData.data(), fontData.size());
    }

    // Layers
    for (const auto& layer : doc.layers) {
        // Layer type
        u8 layerType = 0;
        if (layer->isPixelLayer()) layerType = 0;
        else if (layer->isTextLayer()) layerType = 1;
        else if (layer->isAdjustmentLayer()) layerType = 2;
        writer.write(layerType);

        // Common properties
        writer.writeString(layer->name);
        writer.write(layer->visible);
        writer.write(layer->locked);
        writer.write(layer->opacity);
        writer.write(static_cast<u8>(layer->blend));
        writer.write(layer->transform.position.x);
        writer.write(layer->transform.position.y);
        writer.write(layer->transform.scale.x);
        writer.write(layer->transform.scale.y);
        writer.write(layer->transform.rotation);

        if (layer->isPixelLayer()) {
            const PixelLayer* pixel = static_cast<const PixelLayer*>(layer.get());
            const TiledCanvas& canvas = pixel->canvas;

            writer.write(static_cast<u32>(canvas.tiles.size()));

            for (const auto& [key, tile] : canvas.tiles) {
                i32 tileX, tileY;
                extractTileCoords(key, tileX, tileY);
                writer.write(tileX);
                writer.write(tileY);
                writer.writeBytes(tile->pixels, sizeof(tile->pixels));
            }
        }
        else if (layer->isTextLayer()) {
            const TextLayer* text = static_cast<const TextLayer*>(layer.get());
            writer.writeString(text->text);
            writer.writeString(text->fontFamily);
            writer.write(text->fontSize);
            writer.write(text->textColor.r);
            writer.write(text->textColor.g);
            writer.write(text->textColor.b);
            writer.write(text->textColor.a);
            writer.write(text->bold);
            writer.write(text->italic);
        }
        else if (layer->isAdjustmentLayer()) {
            const AdjustmentLayer* adj = static_cast<const AdjustmentLayer*>(layer.get());
            writer.write(static_cast<u8>(adj->type));

            std::visit([&writer](const auto& p) {
                using T = std::decay_t<decltype(p)>;
                if constexpr (std::is_same_v<T, BrightnessContrastParams>) {
                    writer.write(p.brightness);
                    writer.write(p.contrast);
                }
                else if constexpr (std::is_same_v<T, TemperatureTintParams>) {
                    writer.write(p.temperature);
                    writer.write(p.tint);
                }
                else if constexpr (std::is_same_v<T, HueSaturationParams>) {
                    writer.write(p.hue);
                    writer.write(p.saturation);
                    writer.write(p.lightness);
                }
                else if constexpr (std::is_same_v<T, VibranceParams>) {
                    writer.write(p.vibrance);
                }
                else if constexpr (std::is_same_v<T, ColorBalanceParams>) {
                    writer.write(p.shadowsCyanRed);
                    writer.write(p.shadowsMagentaGreen);
                    writer.write(p.shadowsYellowBlue);
                    writer.write(p.midtonesCyanRed);
                    writer.write(p.midtonesMagentaGreen);
                    writer.write(p.midtonesYellowBlue);
                    writer.write(p.highlightsCyanRed);
                    writer.write(p.highlightsMagentaGreen);
                    writer.write(p.highlightsYellowBlue);
                }
                else if constexpr (std::is_same_v<T, HighlightsShadowsParams>) {
                    writer.write(p.highlights);
                    writer.write(p.shadows);
                }
                else if constexpr (std::is_same_v<T, ExposureParams>) {
                    writer.write(p.exposure);
                    writer.write(p.offset);
                    writer.write(p.gamma);
                }
                else if constexpr (std::is_same_v<T, LevelsParams>) {
                    writer.write(p.inputBlack);
                    writer.write(p.inputGamma);
                    writer.write(p.inputWhite);
                    writer.write(p.outputBlack);
                    writer.write(p.outputWhite);
                }
                else if constexpr (std::is_same_v<T, InvertParams>) {
                    // No parameters
                }
                else if constexpr (std::is_same_v<T, BlackAndWhiteParams>) {
                    writer.write(p.reds);
                    writer.write(p.yellows);
                    writer.write(p.greens);
                    writer.write(p.cyans);
                    writer.write(p.blues);
                    writer.write(p.magentas);
                    writer.write(p.tintHue);
                    writer.write(p.tintAmount);
                }
            }, adj->params);
        }
    }

    return Platform::writeFile(path, buffer.data(), buffer.size());
}

std::unique_ptr<Document> load(const std::string& path) {
    std::vector<u8> buffer = Platform::readFile(path);
    if (buffer.empty()) return nullptr;

    BufferReader reader(buffer);

    // Verify header
    u32 magic = reader.read<u32>();
    if (magic != MAGIC) return nullptr;

    u32 version = reader.read<u32>();
    if (version > VERSION) return nullptr;

    u32 width = reader.read<u32>();
    u32 height = reader.read<u32>();
    u32 layerCount = reader.read<u32>();

    if (!reader.good()) return nullptr;

    auto doc = std::make_unique<Document>();
    doc->width = width;
    doc->height = height;
    doc->selection.resize(width, height);
    doc->filePath = path;
    doc->name = Platform::getFileName(path);
    doc->layers.clear();

    // Read embedded fonts (VERSION 2+)
    if (version >= 2) {
        u32 fontCount = reader.read<u32>();
        for (u32 f = 0; f < fontCount && reader.good(); ++f) {
            std::string fontName = reader.readString();
            u32 dataSize = reader.read<u32>();
            std::vector<u8> fontData(dataSize);
            reader.readBytes(fontData.data(), dataSize);
            if (reader.good()) {
                doc->embeddedFonts[fontName] = std::move(fontData);
            }
        }
    }

    // Read layers
    for (u32 i = 0; i < layerCount && reader.good(); ++i) {
        u8 layerType = reader.read<u8>();

        std::string name = reader.readString();
        bool visible = reader.read<bool>();
        bool locked = reader.read<bool>();
        f32 opacity = reader.read<f32>();
        BlendMode blend = static_cast<BlendMode>(reader.read<u8>());
        f32 posX = reader.read<f32>();
        f32 posY = reader.read<f32>();
        f32 scaleX = reader.read<f32>();
        f32 scaleY = reader.read<f32>();
        f32 rotation = reader.read<f32>();

        std::unique_ptr<LayerBase> layer;

        if (layerType == 0) {
            auto pixel = std::make_unique<PixelLayer>(width, height);
            u32 tileCount = reader.read<u32>();

            for (u32 t = 0; t < tileCount && reader.good(); ++t) {
                i32 tileX = reader.read<i32>();
                i32 tileY = reader.read<i32>();

                auto tile = std::make_unique<Tile>();
                reader.readBytes(tile->pixels, sizeof(tile->pixels));

                u64 key = makeTileKey(tileX, tileY);
                pixel->canvas.tiles[key] = std::move(tile);
            }
            layer = std::move(pixel);
        }
        else if (layerType == 1) {
            auto text = std::make_unique<TextLayer>();
            text->text = reader.readString();
            text->fontFamily = reader.readString();
            text->fontSize = reader.read<u32>();
            text->textColor.r = reader.read<u8>();
            text->textColor.g = reader.read<u8>();
            text->textColor.b = reader.read<u8>();
            text->textColor.a = reader.read<u8>();
            text->bold = reader.read<bool>();
            text->italic = reader.read<bool>();
            layer = std::move(text);
        }
        else if (layerType == 2) {
            AdjustmentType adjType = static_cast<AdjustmentType>(reader.read<u8>());
            auto adj = std::make_unique<AdjustmentLayer>(adjType);

            switch (adjType) {
                case AdjustmentType::BrightnessContrast: {
                    BrightnessContrastParams p;
                    p.brightness = reader.read<f32>();
                    p.contrast = reader.read<f32>();
                    adj->params = p;
                    break;
                }
                case AdjustmentType::TemperatureTint: {
                    TemperatureTintParams p;
                    p.temperature = reader.read<f32>();
                    p.tint = reader.read<f32>();
                    adj->params = p;
                    break;
                }
                case AdjustmentType::HueSaturation: {
                    HueSaturationParams p;
                    p.hue = reader.read<f32>();
                    p.saturation = reader.read<f32>();
                    p.lightness = reader.read<f32>();
                    adj->params = p;
                    break;
                }
                case AdjustmentType::Vibrance: {
                    VibranceParams p;
                    p.vibrance = reader.read<f32>();
                    adj->params = p;
                    break;
                }
                case AdjustmentType::ColorBalance: {
                    ColorBalanceParams p;
                    p.shadowsCyanRed = reader.read<f32>();
                    p.shadowsMagentaGreen = reader.read<f32>();
                    p.shadowsYellowBlue = reader.read<f32>();
                    p.midtonesCyanRed = reader.read<f32>();
                    p.midtonesMagentaGreen = reader.read<f32>();
                    p.midtonesYellowBlue = reader.read<f32>();
                    p.highlightsCyanRed = reader.read<f32>();
                    p.highlightsMagentaGreen = reader.read<f32>();
                    p.highlightsYellowBlue = reader.read<f32>();
                    adj->params = p;
                    break;
                }
                case AdjustmentType::HighlightsShadows: {
                    HighlightsShadowsParams p;
                    p.highlights = reader.read<f32>();
                    p.shadows = reader.read<f32>();
                    adj->params = p;
                    break;
                }
                case AdjustmentType::Exposure: {
                    ExposureParams p;
                    p.exposure = reader.read<f32>();
                    p.offset = reader.read<f32>();
                    p.gamma = reader.read<f32>();
                    adj->params = p;
                    break;
                }
                case AdjustmentType::Levels: {
                    LevelsParams p;
                    p.inputBlack = reader.read<f32>();
                    p.inputGamma = reader.read<f32>();
                    p.inputWhite = reader.read<f32>();
                    p.outputBlack = reader.read<f32>();
                    p.outputWhite = reader.read<f32>();
                    adj->params = p;
                    break;
                }
                case AdjustmentType::Invert: {
                    adj->params = InvertParams();
                    break;
                }
                case AdjustmentType::BlackAndWhite: {
                    BlackAndWhiteParams p;
                    p.reds = reader.read<f32>();
                    p.yellows = reader.read<f32>();
                    p.greens = reader.read<f32>();
                    p.cyans = reader.read<f32>();
                    p.blues = reader.read<f32>();
                    p.magentas = reader.read<f32>();
                    p.tintHue = reader.read<f32>();
                    p.tintAmount = reader.read<f32>();
                    adj->params = p;
                    break;
                }
            }
            layer = std::move(adj);
        }

        if (layer && reader.good()) {
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

    if (!reader.good()) return nullptr;

    if (!doc->layers.empty()) {
        doc->activeLayerIndex = 0;
    }

    return doc;
}

bool isProjectFile(const std::string& path) {
    std::vector<u8> buffer = Platform::readFile(path);
    if (buffer.size() < sizeof(u32)) return false;

    u32 magic;
    std::memcpy(&magic, buffer.data(), sizeof(u32));
    return magic == MAGIC;
}

}
