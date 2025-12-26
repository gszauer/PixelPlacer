#ifndef _H_BRUSH_TIP_
#define _H_BRUSH_TIP_

#include "types.h"
#include <vector>
#include <string>
#include <memory>

// Forward declaration
namespace BrushRenderer {
    struct BrushStamp;
}

// Custom brush tip loaded from image
struct CustomBrushTip {
    std::string name;
    std::vector<f32> alphaMask;  // Original resolution alpha values (0-1)
    u32 width = 0;
    u32 height = 0;
    f32 defaultSpacing = 0.25f;  // Spacing as fraction of size
    f32 defaultAngle = 0.0f;     // Default rotation in degrees

    CustomBrushTip() = default;

    CustomBrushTip(const std::string& tipName, u32 w, u32 h)
        : name(tipName), width(w), height(h) {
        alphaMask.resize(w * h, 0.0f);
    }

    f32 getAlpha(u32 x, u32 y) const {
        if (x >= width || y >= height) return 0.0f;
        return alphaMask[y * width + x];
    }

    void setAlpha(u32 x, u32 y, f32 a) {
        if (x < width && y < height) {
            alphaMask[y * width + x] = a;
        }
    }
};

// Brush dynamics settings
struct BrushDynamics {
    bool enabled = false;        // Master toggle for dynamics (off by default)

    // Size dynamics
    f32 sizeJitter = 0.0f;       // 0-1: random size variation
    f32 sizeJitterMin = 0.0f;    // 0-1: minimum as fraction of size

    // Angle dynamics
    f32 angleJitter = 0.0f;      // 0-360: random rotation per dab

    // Scattering
    f32 scatterAmount = 0.0f;    // 0-1: perpendicular offset as fraction of size
    bool scatterBothAxes = false; // Also scatter along stroke direction

    bool hasAnyDynamics() const {
        return enabled && (sizeJitter > 0.0f || angleJitter > 0.0f || scatterAmount > 0.0f);
    }
};

// Library of custom brush tips (session only, no persistence)
struct BrushLibrary {
    std::vector<std::unique_ptr<CustomBrushTip>> tips;

    void addTip(std::unique_ptr<CustomBrushTip> tip) {
        tips.push_back(std::move(tip));
    }

    void removeTip(size_t index) {
        if (index < tips.size()) {
            tips.erase(tips.begin() + index);
        }
    }

    void renameTip(size_t index, const std::string& name) {
        if (index < tips.size()) {
            tips[index]->name = name;
        }
    }

    CustomBrushTip* getTip(size_t index) {
        if (index < tips.size()) {
            return tips[index].get();
        }
        return nullptr;
    }

    const CustomBrushTip* getTip(size_t index) const {
        if (index < tips.size()) {
            return tips[index].get();
        }
        return nullptr;
    }

    size_t count() const {
        return tips.size();
    }

    void clear() {
        tips.clear();
    }
};

// Channel selection for extracting alpha from image
enum class BrushChannel {
    Red = 0,
    Green = 1,
    Blue = 2,
    Alpha = 3,
    Luminance = 4
};

// Extract alpha value from pixel based on selected channel
inline f32 extractBrushAlpha(u32 pixel, BrushChannel channel) {
    u8 r = (pixel >> 24) & 0xFF;
    u8 g = (pixel >> 16) & 0xFF;
    u8 b = (pixel >> 8) & 0xFF;
    u8 a = pixel & 0xFF;

    switch (channel) {
        case BrushChannel::Red:
            return r / 255.0f;
        case BrushChannel::Green:
            return g / 255.0f;
        case BrushChannel::Blue:
            return b / 255.0f;
        case BrushChannel::Alpha:
            return a / 255.0f;
        case BrushChannel::Luminance:
            return (r * 0.299f + g * 0.587f + b * 0.114f) / 255.0f;
    }
    return 0.0f;
}

#endif
