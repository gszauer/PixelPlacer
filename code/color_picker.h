#ifndef _H_COLOR_PICKER_
#define _H_COLOR_PICKER_

#include "widget.h"
#include "layouts.h"
#include "basic_widgets.h"
#include "overlay_manager.h"
#include "config.h"
#include "dialogs.h"
#include <functional>

// HSV to RGB conversion helper
inline Color hsvToRgb(f32 h, f32 s, f32 v, u8 a = 255) {
    f32 c = v * s;
    f32 x = c * (1.0f - std::abs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
    f32 m = v - c;

    f32 r, g, b;
    if (h < 60) { r = c; g = x; b = 0; }
    else if (h < 120) { r = x; g = c; b = 0; }
    else if (h < 180) { r = 0; g = c; b = x; }
    else if (h < 240) { r = 0; g = x; b = c; }
    else if (h < 300) { r = x; g = 0; b = c; }
    else { r = c; g = 0; b = x; }

    return Color(
        static_cast<u8>((r + m) * 255),
        static_cast<u8>((g + m) * 255),
        static_cast<u8>((b + m) * 255),
        a
    );
}

// RGB to HSV conversion helper
inline void rgbToHsv(const Color& c, f32& h, f32& s, f32& v) {
    f32 r = c.r / 255.0f;
    f32 g = c.g / 255.0f;
    f32 b = c.b / 255.0f;

    f32 maxC = std::max({r, g, b});
    f32 minC = std::min({r, g, b});
    f32 delta = maxC - minC;

    v = maxC;
    s = (maxC > 0) ? (delta / maxC) : 0;

    if (delta < 0.00001f) {
        h = 0;
    } else if (maxC == r) {
        h = 60.0f * std::fmod((g - b) / delta, 6.0f);
    } else if (maxC == g) {
        h = 60.0f * ((b - r) / delta + 2.0f);
    } else {
        h = 60.0f * ((r - g) / delta + 4.0f);
    }
    if (h < 0) h += 360.0f;
}

// Saturation/Value square widget (Photoshop-style)
class SaturationValueWidget : public Widget {
public:
    f32 hue = 0.0f;        // Current hue (0-360)
    f32 saturation = 1.0f; // Selected saturation (0-1)
    f32 value = 1.0f;      // Selected value (0-1)
    bool dragging = false;

    std::function<void()> onChanged;

    void render(Framebuffer& fb) override {
        // Use globalBounds for rendering (bounds is in local coordinates)
        Rect gb = globalBounds();
        Recti r(static_cast<i32>(gb.x), static_cast<i32>(gb.y),
                static_cast<i32>(gb.w), static_cast<i32>(gb.h));

        // Draw saturation/value gradient
        for (i32 y = 0; y < r.h; ++y) {
            for (i32 x = 0; x < r.w; ++x) {
                f32 s = static_cast<f32>(x) / (r.w - 1);
                f32 v = 1.0f - static_cast<f32>(y) / (r.h - 1);
                Color c = hsvToRgb(hue, s, v);
                fb.setPixel(r.x + x, r.y + y, c.toRGBA());
            }
        }

        // Draw border
        i32 borderThick = static_cast<i32>(Config::uiScale);
        fb.drawRect(r, 0x808080FF, borderThick);

        // Draw crosshair at current position
        i32 cx = r.x + static_cast<i32>(saturation * (r.w - 1));
        i32 cy = r.y + static_cast<i32>((1.0f - value) * (r.h - 1));

        // Draw circle indicator (more visible than crosshair)
        i32 circleRadius = static_cast<i32>(6 * Config::uiScale);
        i32 thickness = static_cast<i32>(2 * Config::uiScale);
        // White outer circle
        for (i32 t = 0; t < thickness; ++t) {
            fb.drawCircle(cx, cy, circleRadius + t, 0xFFFFFFFF);
        }
        // Black inner circle
        fb.drawCircle(cx, cy, circleRadius - 1, 0x000000FF);
    }

    bool onMouseDown(const MouseEvent& e) override {
        // e.position is in local coordinates (relative to this widget)
        if (e.position.x >= 0 && e.position.x < bounds.w &&
            e.position.y >= 0 && e.position.y < bounds.h) {
            dragging = true;
            updateFromMouse(e.position.x, e.position.y);
            return true;
        }
        return false;
    }

    bool onMouseDrag(const MouseEvent& e) override {
        if (dragging) {
            updateFromMouse(e.position.x, e.position.y);
            return true;
        }
        return false;
    }

    bool onMouseUp(const MouseEvent& e) override {
        dragging = false;
        return false;
    }

private:
    void updateFromMouse(f32 localX, f32 localY) {
        // localX, localY are in widget-local coordinates
        saturation = std::clamp(localX / bounds.w, 0.0f, 1.0f);
        value = 1.0f - std::clamp(localY / bounds.h, 0.0f, 1.0f);
        if (onChanged) onChanged();
        getAppState().needsRedraw = true;
    }
};

// Vertical hue strip widget
class HueStripWidget : public Widget {
public:
    f32 hue = 0.0f;  // 0-360
    bool dragging = false;

    std::function<void()> onChanged;

    void render(Framebuffer& fb) override {
        // Use globalBounds for rendering (bounds is in local coordinates)
        Rect gb = globalBounds();
        Recti r(static_cast<i32>(gb.x), static_cast<i32>(gb.y),
                static_cast<i32>(gb.w), static_cast<i32>(gb.h));

        // Draw hue gradient (vertical)
        for (i32 y = 0; y < r.h; ++y) {
            f32 h = (static_cast<f32>(y) / (r.h - 1)) * 360.0f;
            Color c = hsvToRgb(h, 1.0f, 1.0f);
            for (i32 x = 0; x < r.w; ++x) {
                fb.setPixel(r.x + x, r.y + y, c.toRGBA());
            }
        }

        // Draw border
        i32 borderThick = static_cast<i32>(Config::uiScale);
        fb.drawRect(r, 0x808080FF, borderThick);

        // Draw indicator at current hue - horizontal line across strip with arrows
        i32 hy = r.y + static_cast<i32>((hue / 360.0f) * (r.h - 1));
        i32 arrowSize = static_cast<i32>(6 * Config::uiScale);
        i32 lineThick = static_cast<i32>(2 * Config::uiScale);

        // Draw horizontal line across the strip (white with black outline)
        for (i32 t = -1; t <= lineThick; ++t) {
            u32 color = (t == -1 || t == lineThick) ? 0x000000FF : 0xFFFFFFFF;
            for (i32 x = 0; x < r.w; ++x) {
                fb.setPixel(r.x + x, hy + t, color);
            }
        }

        // Draw triangular arrows on left side
        for (i32 i = 0; i < arrowSize; ++i) {
            for (i32 j = -i - 1; j <= i + 1; ++j) {
                // Black outline
                fb.setPixel(r.x - 2 - i, hy + j, 0x000000FF);
            }
            for (i32 j = -i; j <= i; ++j) {
                // White fill
                fb.setPixel(r.x - 2 - i, hy + j, 0xFFFFFFFF);
            }
        }

        // Draw triangular arrows on right side
        for (i32 i = 0; i < arrowSize; ++i) {
            for (i32 j = -i - 1; j <= i + 1; ++j) {
                // Black outline
                fb.setPixel(r.x + r.w + 1 + i, hy + j, 0x000000FF);
            }
            for (i32 j = -i; j <= i; ++j) {
                // White fill
                fb.setPixel(r.x + r.w + 1 + i, hy + j, 0xFFFFFFFF);
            }
        }
    }

    bool onMouseDown(const MouseEvent& e) override {
        // e.position is in local coordinates (relative to this widget)
        if (e.position.x >= 0 && e.position.x < bounds.w &&
            e.position.y >= 0 && e.position.y < bounds.h) {
            dragging = true;
            updateFromMouse(e.position.y);
            return true;
        }
        return false;
    }

    bool onMouseDrag(const MouseEvent& e) override {
        if (dragging) {
            updateFromMouse(e.position.y);
            return true;
        }
        return false;
    }

    bool onMouseUp(const MouseEvent& e) override {
        dragging = false;
        return false;
    }

private:
    void updateFromMouse(f32 localY) {
        // localY is already in widget-local coordinates
        hue = std::clamp(localY / bounds.h, 0.0f, 1.0f) * 360.0f;
        if (onChanged) onChanged();
        getAppState().needsRedraw = true;
    }
};

class ColorPickerDialog : public Dialog {
public:
    Color selectedColor = Color::black();
    f32 hue = 0.0f, saturation = 1.0f, value = 1.0f;

    SaturationValueWidget* svWidget = nullptr;
    HueStripWidget* hueStrip = nullptr;
    ColorSwatch* previewSwatch = nullptr;
    TextField* hexField = nullptr;
    TextField* rField = nullptr;
    TextField* gField = nullptr;
    TextField* bField = nullptr;
    TextField* aField = nullptr;

    std::function<void(const Color&)> onColorSelected;

    ColorPickerDialog() : Dialog("Color Picker") {
        // Compact height with preview swatch spanning RGBA rows
        preferredSize = Vec2(228 * Config::uiScale, 390 * Config::uiScale);

        auto layout = createChild<VBoxLayout>(8 * Config::uiScale);

        // Header with background (like other dialogs)
        auto headerPanel = layout->createChild<Panel>();
        headerPanel->bgColor = Config::COLOR_PANEL_HEADER;
        headerPanel->preferredSize = Vec2(0, 28 * Config::uiScale);
        headerPanel->setPadding(4 * Config::uiScale);
        headerPanel->createChild<Label>("Color Picker");

        layout->createChild<Separator>();

        // SV square + Hue strip row (fixed width, no spacer)
        auto colorRow = layout->createChild<HBoxLayout>(8 * Config::uiScale);
        colorRow->preferredSize = Vec2(0, 180 * Config::uiScale);
        colorRow->verticalPolicy = SizePolicy::Fixed;

        svWidget = colorRow->createChild<SaturationValueWidget>();
        svWidget->preferredSize = Vec2(180 * Config::uiScale, 180 * Config::uiScale);
        svWidget->horizontalPolicy = SizePolicy::Fixed;
        svWidget->onChanged = [this]() { updateFromSV(); };

        hueStrip = colorRow->createChild<HueStripWidget>();
        hueStrip->preferredSize = Vec2(24 * Config::uiScale, 180 * Config::uiScale);
        hueStrip->horizontalPolicy = SizePolicy::Fixed;
        hueStrip->onChanged = [this]() { updateFromHue(); };

        layout->createChild<Separator>();

        // Hex input row
        auto hexRow = layout->createChild<HBoxLayout>(4 * Config::uiScale);
        hexRow->preferredSize = Vec2(0, 26 * Config::uiScale);
        hexRow->createChild<Label>("Hex:")->preferredSize = Vec2(32 * Config::uiScale, 24 * Config::uiScale);
        hexField = hexRow->createChild<TextField>();
        hexField->text = "#000000FF";
        hexField->horizontalPolicy = SizePolicy::Expanding;
        hexField->onSubmit = [this]() { updateFromHex(); };

        // RGBA section: left side has R/G/B/A fields, right side has preview swatch
        auto rgbaSection = layout->createChild<HBoxLayout>(4 * Config::uiScale);
        rgbaSection->preferredSize = Vec2(0, 56 * Config::uiScale);  // Two rows height
        rgbaSection->verticalPolicy = SizePolicy::Fixed;

        // Left side: stacked R/G and B/A rows
        auto rgbaLeft = rgbaSection->createChild<VBoxLayout>(4 * Config::uiScale);
        rgbaLeft->horizontalPolicy = SizePolicy::Expanding;

        // R/G row
        auto rgRow = rgbaLeft->createChild<HBoxLayout>(4 * Config::uiScale);
        rgRow->preferredSize = Vec2(0, 26 * Config::uiScale);

        rgRow->createChild<Label>("R:")->preferredSize = Vec2(20 * Config::uiScale, 24 * Config::uiScale);
        rField = rgRow->createChild<TextField>();
        rField->text = "0";
        rField->preferredSize = Vec2(40 * Config::uiScale, 24 * Config::uiScale);
        rField->horizontalPolicy = SizePolicy::Fixed;
        rField->onSubmit = [this]() { updateFromRGBA(); };

        rgRow->createChild<Label>("G:")->preferredSize = Vec2(20 * Config::uiScale, 24 * Config::uiScale);
        gField = rgRow->createChild<TextField>();
        gField->text = "0";
        gField->preferredSize = Vec2(40 * Config::uiScale, 24 * Config::uiScale);
        gField->horizontalPolicy = SizePolicy::Fixed;
        gField->onSubmit = [this]() { updateFromRGBA(); };

        // B/A row
        auto baRow = rgbaLeft->createChild<HBoxLayout>(4 * Config::uiScale);
        baRow->preferredSize = Vec2(0, 26 * Config::uiScale);

        baRow->createChild<Label>("B:")->preferredSize = Vec2(20 * Config::uiScale, 24 * Config::uiScale);
        bField = baRow->createChild<TextField>();
        bField->text = "0";
        bField->preferredSize = Vec2(40 * Config::uiScale, 24 * Config::uiScale);
        bField->horizontalPolicy = SizePolicy::Fixed;
        bField->onSubmit = [this]() { updateFromRGBA(); };

        baRow->createChild<Label>("A:")->preferredSize = Vec2(20 * Config::uiScale, 24 * Config::uiScale);
        aField = baRow->createChild<TextField>();
        aField->text = "255";
        aField->preferredSize = Vec2(40 * Config::uiScale, 24 * Config::uiScale);
        aField->horizontalPolicy = SizePolicy::Fixed;
        aField->onSubmit = [this]() { updateFromRGBA(); };

        // Right side: preview swatch spanning both rows
        previewSwatch = rgbaSection->createChild<ColorSwatch>(Color::black());
        previewSwatch->preferredSize = Vec2(56 * Config::uiScale, 56 * Config::uiScale);
        previewSwatch->horizontalPolicy = SizePolicy::Fixed;
        previewSwatch->verticalPolicy = SizePolicy::Fixed;

        layout->createChild<Spacer>();

        // Buttons
        auto btnRow = layout->createChild<HBoxLayout>(8 * Config::uiScale);
        btnRow->preferredSize = Vec2(0, 32 * Config::uiScale);
        btnRow->verticalPolicy = SizePolicy::Fixed;

        btnRow->createChild<Spacer>();

        auto cancelBtn = btnRow->createChild<Button>("Cancel");
        cancelBtn->preferredSize = Vec2(70 * Config::uiScale, 28 * Config::uiScale);
        cancelBtn->onClick = [this]() {
            hide();
        };

        auto okBtn = btnRow->createChild<Button>("OK");
        okBtn->preferredSize = Vec2(70 * Config::uiScale, 28 * Config::uiScale);
        okBtn->onClick = [this]() {
            if (onColorSelected) {
                onColorSelected(selectedColor);
            }
            hide();
        };
    }

    void setColor(const Color& c) {
        selectedColor = c;
        rgbToHsv(c, hue, saturation, value);
        syncWidgets();
    }

private:
    void syncWidgets() {
        if (svWidget) {
            svWidget->hue = hue;
            svWidget->saturation = saturation;
            svWidget->value = value;
        }
        if (hueStrip) {
            hueStrip->hue = hue;
        }
        if (previewSwatch) {
            previewSwatch->color = selectedColor;
        }
        updateHexField();
        updateRGBAFields();
    }

    void updateFromSV() {
        saturation = svWidget->saturation;
        value = svWidget->value;
        selectedColor = hsvToRgb(hue, saturation, value, selectedColor.a);
        if (previewSwatch) previewSwatch->color = selectedColor;
        updateHexField();
        updateRGBAFields();
    }

    void updateFromHue() {
        hue = hueStrip->hue;
        if (svWidget) svWidget->hue = hue;
        selectedColor = hsvToRgb(hue, saturation, value, selectedColor.a);
        if (previewSwatch) previewSwatch->color = selectedColor;
        updateHexField();
        updateRGBAFields();
    }

    void updateFromHex() {
        std::string hex = hexField->text;
        if (!hex.empty() && hex[0] == '#') hex = hex.substr(1);

        try {
            if (hex.length() >= 6) {
                u32 rgb = std::stoul(hex.substr(0, 6), nullptr, 16);
                selectedColor.r = (rgb >> 16) & 0xFF;
                selectedColor.g = (rgb >> 8) & 0xFF;
                selectedColor.b = rgb & 0xFF;
                if (hex.length() >= 8) {
                    selectedColor.a = std::stoul(hex.substr(6, 2), nullptr, 16);
                }
                rgbToHsv(selectedColor, hue, saturation, value);
                syncWidgets();
            }
        } catch (...) {}
    }

    void updateFromRGBA() {
        try {
            selectedColor.r = static_cast<u8>(std::clamp(std::stoi(rField->text), 0, 255));
            selectedColor.g = static_cast<u8>(std::clamp(std::stoi(gField->text), 0, 255));
            selectedColor.b = static_cast<u8>(std::clamp(std::stoi(bField->text), 0, 255));
            selectedColor.a = static_cast<u8>(std::clamp(std::stoi(aField->text), 0, 255));
            rgbToHsv(selectedColor, hue, saturation, value);
            syncWidgets();
        } catch (...) {}
    }

    void updateHexField() {
        char hex[16];
        snprintf(hex, sizeof(hex), "#%02X%02X%02X%02X",
                 selectedColor.r, selectedColor.g, selectedColor.b, selectedColor.a);
        if (hexField) hexField->text = hex;
    }

    void updateRGBAFields() {
        if (rField) rField->text = std::to_string(selectedColor.r);
        if (gField) gField->text = std::to_string(selectedColor.g);
        if (bField) bField->text = std::to_string(selectedColor.b);
        if (aField) aField->text = std::to_string(selectedColor.a);
    }
};

#endif
