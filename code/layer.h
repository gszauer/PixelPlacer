#ifndef _H_LAYER_
#define _H_LAYER_

#include "types.h"
#include "primitives.h"
#include "tiled_canvas.h"
#include "blend.h"
#include <string>
#include <memory>
#include <variant>

// Adjustment types
enum class AdjustmentType {
    BrightnessContrast,
    TemperatureTint,
    HueSaturation,
    Vibrance,
    ColorBalance,
    HighlightsShadows,
    Exposure,
    Levels,
    Invert,
    BlackAndWhite
};

// Adjustment parameters for each type
struct BrightnessContrastParams {
    f32 brightness = 0.0f;  // -100 to 100
    f32 contrast = 0.0f;    // -100 to 100
};

struct TemperatureTintParams {
    f32 temperature = 0.0f; // -100 to 100
    f32 tint = 0.0f;        // -100 to 100
};

struct HueSaturationParams {
    f32 hue = 0.0f;         // -180 to 180
    f32 saturation = 0.0f;  // -100 to 100
    f32 lightness = 0.0f;   // -100 to 100
};

struct VibranceParams {
    f32 vibrance = 0.0f;    // -100 to 100
};

struct ColorBalanceParams {
    f32 shadowsCyanRed = 0.0f;
    f32 shadowsMagentaGreen = 0.0f;
    f32 shadowsYellowBlue = 0.0f;
    f32 midtonesCyanRed = 0.0f;
    f32 midtonesMagentaGreen = 0.0f;
    f32 midtonesYellowBlue = 0.0f;
    f32 highlightsCyanRed = 0.0f;
    f32 highlightsMagentaGreen = 0.0f;
    f32 highlightsYellowBlue = 0.0f;
};

struct HighlightsShadowsParams {
    f32 highlights = 0.0f;  // -100 to 100
    f32 shadows = 0.0f;     // -100 to 100
};

struct ExposureParams {
    f32 exposure = 0.0f;    // -5 to 5
    f32 offset = 0.0f;      // -0.1 to 0.1
    f32 gamma = 1.0f;       // 0.01 to 9.99
};

struct LevelsParams {
    // Input levels: black point, gamma, white point (0-255)
    f32 inputBlack = 0.0f;
    f32 inputGamma = 1.0f;
    f32 inputWhite = 255.0f;
    // Output levels
    f32 outputBlack = 0.0f;
    f32 outputWhite = 255.0f;
};

struct InvertParams {
    // No parameters needed
};

struct BlackAndWhiteParams {
    f32 reds = 40.0f;
    f32 yellows = 60.0f;
    f32 greens = 40.0f;
    f32 cyans = 60.0f;
    f32 blues = 20.0f;
    f32 magentas = 80.0f;
    f32 tintHue = 0.0f;
    f32 tintAmount = 0.0f;
};

// Variant type for all adjustment params
using AdjustmentParams = std::variant<
    BrightnessContrastParams,
    TemperatureTintParams,
    HueSaturationParams,
    VibranceParams,
    ColorBalanceParams,
    HighlightsShadowsParams,
    ExposureParams,
    LevelsParams,
    InvertParams,
    BlackAndWhiteParams
>;

// Base layer class
struct LayerBase {
    std::string name;
    Transform transform;
    f32 opacity = 1.0f;
    bool locked = false;
    bool visible = true;
    BlendMode blend = BlendMode::Normal;

    virtual ~LayerBase() = default;

    // Layer type identification
    virtual bool isPixelLayer() const { return false; }
    virtual bool isTextLayer() const { return false; }
    virtual bool isAdjustmentLayer() const { return false; }

    // Clone layer
    virtual std::unique_ptr<LayerBase> clone() const = 0;
};

// Pixel layer - contains actual bitmap data
struct PixelLayer : LayerBase {
    TiledCanvas canvas;

    PixelLayer() = default;
    PixelLayer(u32 w, u32 h) : canvas(w, h) {}

    bool isPixelLayer() const override { return true; }

    std::unique_ptr<LayerBase> clone() const override {
        auto copy = std::make_unique<PixelLayer>();
        copy->name = name;
        copy->transform = transform;
        copy->opacity = opacity;
        copy->locked = locked;
        copy->visible = visible;
        copy->blend = blend;
        // Deep copy canvas
        copy->canvas.width = canvas.width;
        copy->canvas.height = canvas.height;
        for (const auto& [key, tile] : canvas.tiles) {
            copy->canvas.tiles[key] = tile->clone();
        }
        return copy;
    }
};

// Text layer - vector text that can be rasterized
struct TextLayer : LayerBase {
    std::string text;
    std::string fontFamily = "DejaVu Sans";
    u32 fontSize = 24;
    Color textColor = Color::black();
    bool bold = false;
    bool italic = false;

    // Cached rasterized version
    mutable TiledCanvas rasterizedCache;
    mutable bool cacheValid = false;

    bool isTextLayer() const override { return true; }

    void invalidateCache() { cacheValid = false; }

    // Ensure the rasterized cache is valid (implemented in layer.cpp)
    void ensureCacheValid() const;

    std::unique_ptr<LayerBase> clone() const override {
        auto copy = std::make_unique<TextLayer>();
        copy->name = name;
        copy->transform = transform;
        copy->opacity = opacity;
        copy->locked = locked;
        copy->visible = visible;
        copy->blend = blend;
        copy->text = text;
        copy->fontFamily = fontFamily;
        copy->fontSize = fontSize;
        copy->textColor = textColor;
        copy->bold = bold;
        copy->italic = italic;
        copy->cacheValid = false;
        return copy;
    }
};

// Adjustment layer - non-destructive image adjustments
struct AdjustmentLayer : LayerBase {
    AdjustmentType type = AdjustmentType::BrightnessContrast;
    AdjustmentParams params;

    AdjustmentLayer() {
        params = BrightnessContrastParams();
    }

    explicit AdjustmentLayer(AdjustmentType t) : type(t) {
        setDefaultParams(t);
    }

    bool isAdjustmentLayer() const override { return true; }

    void setDefaultParams(AdjustmentType t) {
        type = t;
        switch (t) {
            case AdjustmentType::BrightnessContrast:
                params = BrightnessContrastParams(); break;
            case AdjustmentType::TemperatureTint:
                params = TemperatureTintParams(); break;
            case AdjustmentType::HueSaturation:
                params = HueSaturationParams(); break;
            case AdjustmentType::Vibrance:
                params = VibranceParams(); break;
            case AdjustmentType::ColorBalance:
                params = ColorBalanceParams(); break;
            case AdjustmentType::HighlightsShadows:
                params = HighlightsShadowsParams(); break;
            case AdjustmentType::Exposure:
                params = ExposureParams(); break;
            case AdjustmentType::Levels:
                params = LevelsParams(); break;
            case AdjustmentType::Invert:
                params = InvertParams(); break;
            case AdjustmentType::BlackAndWhite:
                params = BlackAndWhiteParams(); break;
        }
    }

    std::unique_ptr<LayerBase> clone() const override {
        auto copy = std::make_unique<AdjustmentLayer>(type);
        copy->name = name;
        copy->transform = transform;
        copy->opacity = opacity;
        copy->locked = locked;
        copy->visible = visible;
        copy->blend = blend;
        copy->params = params;
        return copy;
    }
};

// Helper to get typed adjustment params
template<typename T>
T* getAdjustmentParams(AdjustmentLayer* layer) {
    return std::get_if<T>(&layer->params);
}

template<typename T>
const T* getAdjustmentParams(const AdjustmentLayer* layer) {
    return std::get_if<T>(&layer->params);
}

#endif
