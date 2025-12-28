#ifndef _H_BASIC_WIDGETS_
#define _H_BASIC_WIDGETS_

#include "widget.h"
#include "config.h"
#include "overlay_manager.h"
#include <functional>
#include <string>
#include <unordered_map>

// Forward declarations
struct stbtt_fontinfo;
class TiledCanvas;

// Simple font renderer (using stb_truetype)
class FontRenderer {
public:
    static FontRenderer& instance();

    // Load the default internal font
    void loadFont(const u8* data, i32 size);

    // Custom font management
    bool loadCustomFont(const std::string& fontName, const u8* data, i32 size);
    bool hasFont(const std::string& fontName) const;
    stbtt_fontinfo* getFont(const std::string& fontName);  // nullptr or empty = default
    std::vector<std::string> getFontNames() const;

    void renderText(Framebuffer& fb, const std::string& text, i32 x, i32 y, u32 color, f32 size = Config::defaultFontSize());
    void renderTextWithFont(Framebuffer& fb, const std::string& text, i32 x, i32 y, u32 color, f32 size, const std::string& fontName);
    void renderTextVertical(Framebuffer& fb, const std::string& text, i32 x, i32 y, u32 color, f32 size = Config::defaultFontSize());
    void renderTextRotated90(Framebuffer& fb, const std::string& text, i32 x, i32 y, u32 color, f32 size = Config::defaultFontSize());
    Vec2 measureText(const std::string& text, f32 size = Config::defaultFontSize());
    Vec2 measureTextVertical(const std::string& text, f32 size = Config::defaultFontSize());

    // Render text to a TiledCanvas (for TextLayer caching)
    // fontName: empty or "Internal Font" uses default, otherwise uses custom font
    void renderToCanvas(TiledCanvas& canvas, const std::string& text, i32 x, i32 y, u32 color, f32 size = Config::defaultFontSize(), const std::string& fontName = "");

    // Measure text with a specific font
    Vec2 measureTextWithFont(const std::string& text, f32 size, const std::string& fontName);

    // Render icon centered by its actual glyph bounding box (for proper visual centering)
    void renderIconCentered(Framebuffer& fb, const std::string& icon, const Rect& bounds, u32 color, f32 size, const std::string& fontName);

    bool isLoaded() const { return fontLoaded; }

private:
    FontRenderer() = default;

    // Default internal font
    std::vector<u8> fontData;
    std::unique_ptr<stbtt_fontinfo> fontInfo;
    bool fontLoaded = false;

    // Custom fonts keyed by filename
    struct LoadedFont {
        std::vector<u8> data;
        std::unique_ptr<stbtt_fontinfo> info;
    };
    std::unordered_map<std::string, LoadedFont> customFonts;
};

// Label widget
class Label : public Widget {
public:
    std::string text;
    u32 textColor = Config::COLOR_TEXT;
    f32 fontSize = Config::defaultFontSize();
    bool centerHorizontal = false;
    bool centerVertical = true;

    Label() = default;
    explicit Label(const std::string& t) : text(t) {
        preferredSize = Vec2(100 * Config::uiScale, 20 * Config::uiScale);
    }

    void setText(const std::string& t) {
        text = t;
        updatePreferredSize();
    }

    void updatePreferredSize() {
        Vec2 size = FontRenderer::instance().measureText(text, fontSize);
        preferredSize = Vec2(size.x + 8, size.y + 4);
    }

    void renderSelf(Framebuffer& fb) override;
};

// Button widget
class Button : public Widget {
public:
    std::string text;
    u32 normalColor = Config::COLOR_BUTTON;
    u32 hoverColor = Config::COLOR_BUTTON_HOVER;
    u32 pressedColor = Config::COLOR_BUTTON_PRESSED;
    u32 textColor = Config::COLOR_TEXT;
    u32 borderColor = 0x00000000;  // No border by default (Spectrum style)
    f32 fontSize = Config::defaultFontSize();
    i32 textAlign = 1;  // 0=left, 1=center, 2=right

    bool pressed = false;

    std::function<void()> onClick;
    std::function<void()> onDoubleClick;

    // For double-click detection
    u64 lastClickTime = 0;
    static constexpr u64 DOUBLE_CLICK_TIME = 400;  // ms

    Button() {
        preferredSize = Vec2(80 * Config::uiScale, 24 * Config::uiScale);
    }
    explicit Button(const std::string& t) : text(t) {
        preferredSize = Vec2(80 * Config::uiScale, 24 * Config::uiScale);
    }

    void renderSelf(Framebuffer& fb) override;
    bool onMouseDown(const MouseEvent& e) override;
    bool onMouseUp(const MouseEvent& e) override;
    void onMouseLeave(const MouseEvent& e) override;
};

// Icon button (square button with icon)
class IconButton : public Widget {
public:
    u32 iconColor = Config::COLOR_TEXT;
    u32 normalColor = 0x00000000;  // Transparent by default
    u32 hoverColor = Config::COLOR_BUTTON_HOVER;
    u32 pressedColor = Config::COLOR_BUTTON_PRESSED;
    u32 selectedColor = Config::GRAY_500;

    bool pressed = false;
    bool selected = false;
    bool toggleMode = false;

    // Icon is rendered via callback
    std::function<void(Framebuffer&, const Rect&, u32)> renderIcon;
    std::function<void()> onClick;
    std::function<void()> onDoubleClick;

    // For double-click detection
    u64 lastClickTime = 0;
    static constexpr u64 DOUBLE_CLICK_TIME = 400;  // ms

    IconButton() {
        preferredSize = Vec2(32 * Config::uiScale, 32 * Config::uiScale);
        minSize = Vec2(24 * Config::uiScale, 24 * Config::uiScale);
    }

    void renderSelf(Framebuffer& fb) override;
    bool onMouseDown(const MouseEvent& e) override;
    bool onMouseUp(const MouseEvent& e) override;
    void onMouseLeave(const MouseEvent& e) override;
};

// Checkbox
class Checkbox : public Widget {
public:
    std::string label;
    bool checked = false;
    u32 boxColor = Config::COLOR_INPUT;  // Dark inset for checkbox box
    u32 checkColor = Config::GRAY_700;   // Muted gray check mark
    u32 textColor = Config::COLOR_TEXT;

    std::function<void(bool)> onChanged;

    Checkbox() {
        preferredSize = Vec2(100 * Config::uiScale, 20 * Config::uiScale);
    }
    explicit Checkbox(const std::string& lbl, bool initial = false)
        : label(lbl), checked(initial) {
        preferredSize = Vec2(100 * Config::uiScale, 20 * Config::uiScale);
    }

    void renderSelf(Framebuffer& fb) override;
    bool onMouseDown(const MouseEvent& e) override;
};

// Slider widget
class Slider : public Widget {
public:
    f32 value = 0.5f;
    f32 minValue = 0.0f;
    f32 maxValue = 1.0f;

    u32 trackColor = Config::COLOR_INPUT;  // Dark inset track
    u32 fillColor = Config::GRAY_500;     // Visible against dark track
    u32 thumbColor = Config::GRAY_600;    // Slightly lighter knob

    bool dragging = false;

    std::function<void(f32)> onChanged;
    std::function<void()> onDragEnd;  // Called when drag ends (for deferred updates)

    Slider() {
        preferredSize = Vec2(120 * Config::uiScale, 20 * Config::uiScale);
        minSize = Vec2(60 * Config::uiScale, 16 * Config::uiScale);
    }

    Slider(f32 min, f32 max, f32 initial = 0.5f)
        : value(initial), minValue(min), maxValue(max) {
        preferredSize = Vec2(120 * Config::uiScale, 20 * Config::uiScale);
        minSize = Vec2(60 * Config::uiScale, 16 * Config::uiScale);
    }

    void setValue(f32 v) {
        value = clamp(v, minValue, maxValue);
    }

    f32 getNormalizedValue() const {
        return (value - minValue) / (maxValue - minValue);
    }

    void renderSelf(Framebuffer& fb) override;
    bool onMouseDown(const MouseEvent& e) override;
    bool onMouseUp(const MouseEvent& e) override;
    bool onMouseDrag(const MouseEvent& e) override;

private:
    void updateValueFromMouse(f32 x);
};

// Forward declaration
class NumberSlider;

// Popup slider for NumberSlider
class NumberSliderPopup : public Widget {
public:
    NumberSlider* owner = nullptr;
    bool dragging = false;

    u32 bgColor = Config::COLOR_PANEL;
    u32 trackColor = Config::COLOR_INPUT;
    u32 fillColor = Config::GRAY_500;
    u32 thumbColor = Config::GRAY_600;
    u32 borderColor = Config::COLOR_BORDER;

    NumberSliderPopup();

    void renderSelf(Framebuffer& fb) override;
    bool onMouseDown(const MouseEvent& e) override;
    bool onMouseDrag(const MouseEvent& e) override;
    bool onMouseUp(const MouseEvent& e) override;

private:
    void updateValueFromMouse(f32 x);
};

// Number input with popup slider
class NumberSlider : public Widget {
public:
    f32 value = 50.0f;
    f32 minValue = 1.0f;
    f32 maxValue = 100.0f;
    bool minUnbound = false;
    bool maxUnbound = false;
    i32 decimals = 0;           // 0 = integer display
    std::string suffix;         // e.g., "px", "%"

    std::string editText;       // Text being edited
    i32 cursorPos = 0;
    i32 selectionStart = -1;    // Selection start (-1 = no selection)
    bool editing = false;
    bool showCursor = true;
    bool draggingSelection = false;
    u64 cursorBlinkTime = 0;

    u32 bgColor = Config::COLOR_INPUT;
    u32 textColor = Config::COLOR_TEXT;
    u32 borderColor = Config::COLOR_BORDER;
    u32 focusBorderColor = Config::COLOR_FOCUS;

    std::function<void(f32)> onChanged;

    std::unique_ptr<NumberSliderPopup> popup;

    NumberSlider();
    NumberSlider(f32 min, f32 max, f32 initial, i32 decimalPlaces = 0);

    void setValue(f32 v);
    f32 getNormalizedValue() const;
    std::string getDisplayText() const;

    void showPopup();
    void hidePopup();
    void commitEdit();

    void renderSelf(Framebuffer& fb) override;
    bool onMouseDown(const MouseEvent& e) override;
    bool onMouseDrag(const MouseEvent& e) override;
    bool onMouseUp(const MouseEvent& e) override;
    bool onKeyDown(const KeyEvent& e) override;
    bool onTextInput(const std::string& input) override;
    void onFocus() override;
    void onBlur() override;

private:
    bool hasSelection() const { return selectionStart >= 0 && selectionStart != cursorPos; }
    void deleteSelection();
    i32 positionFromX(f32 localX);
};

// Text field (single line)
class TextField : public Widget {
public:
    std::string text;
    std::string placeholder;
    u32 bgColor = Config::COLOR_INPUT;  // Dark inset (Spectrum style)
    u32 textColor = Config::COLOR_TEXT;
    u32 placeholderColor = Config::COLOR_TEXT_DIM;
    u32 borderColor = Config::COLOR_BORDER;
    u32 focusBorderColor = Config::COLOR_FOCUS;
    f32 fontSize = Config::defaultFontSize();

    i32 cursorPos = 0;
    i32 selectionStart = -1;
    bool showCursor = true;
    bool readOnly = false;
    u64 cursorBlinkTime = 0;
    f32 scrollOffset = 0.0f;  // Horizontal scroll offset for long text
    bool draggingSelection = false;  // Track if mouse is dragging to select

    std::function<void(const std::string&)> onChanged;
    std::function<void()> onSubmit;
    std::function<void()> onClick;  // Called when clicked (useful for readOnly fields)

    TextField() {
        focusable = true;
        preferredSize = Vec2(150 * Config::uiScale, 24 * Config::uiScale);
        minSize = Vec2(50 * Config::uiScale, 20 * Config::uiScale);
    }

    void renderSelf(Framebuffer& fb) override;
    bool onMouseDown(const MouseEvent& e) override;
    bool onMouseDrag(const MouseEvent& e) override;
    bool onMouseUp(const MouseEvent& e) override;
    bool onKeyDown(const KeyEvent& e) override;
    bool onTextInput(const std::string& input) override;
    void onFocus() override;
    void onBlur() override;

private:
    void insertText(const std::string& t);
    void deleteSelection();
    void ensureCaretVisible();  // Scroll to keep caret in view
    i32 positionFromX(f32 localX);  // Convert local X coordinate to text position
    bool hasSelection() const { return selectionStart >= 0 && selectionStart != cursorPos; }
};

// Color swatch (displays a color, clickable)
class ColorSwatch : public Widget {
public:
    Color color = Color::black();
    u32 borderColor = Config::COLOR_BORDER;
    bool showCheckerboard = true;

    std::function<void()> onClick;

    ColorSwatch() {
        preferredSize = Vec2(32 * Config::uiScale, 32 * Config::uiScale);
        minSize = Vec2(16 * Config::uiScale, 16 * Config::uiScale);
    }
    explicit ColorSwatch(const Color& c) : color(c) {
        preferredSize = Vec2(32 * Config::uiScale, 32 * Config::uiScale);
        minSize = Vec2(16 * Config::uiScale, 16 * Config::uiScale);
    }

    void renderSelf(Framebuffer& fb) override;
    bool onMouseDown(const MouseEvent& e) override;
};

// Forward declaration
class ComboBox;

// Dropdown overlay widget for ComboBox
class ComboBoxDropdown : public Widget {
public:
    ComboBox* owner = nullptr;
    i32 hoveredIndex = -1;

    u32 bgColor = Config::COLOR_PANEL;
    u32 textColor = Config::COLOR_TEXT;
    u32 borderColor = Config::COLOR_BORDER;
    u32 hoverColor = Config::GRAY_500;

    ComboBoxDropdown() {
        visible = false;
    }

    void renderSelf(Framebuffer& fb) override;
    bool onMouseDown(const MouseEvent& e) override;
    bool onMouseMove(const MouseEvent& e) override;
};

// Combo box (dropdown)
class ComboBox : public Widget {
public:
    std::vector<std::string> items;
    i32 selectedIndex = -1;
    bool expanded = false;

    u32 bgColor = Config::COLOR_PANEL;
    u32 textColor = Config::COLOR_TEXT;
    u32 borderColor = Config::COLOR_BORDER;
    u32 hoverColor = Config::GRAY_500;

    std::function<void(i32)> onSelectionChanged;

    // Dropdown overlay (created on demand)
    std::unique_ptr<ComboBoxDropdown> dropdownOverlay;

    ComboBox() {
        preferredSize = Vec2(120 * Config::uiScale, 24 * Config::uiScale);
    }

    void addItem(const std::string& item) {
        items.push_back(item);
        if (selectedIndex < 0 && !items.empty()) {
            selectedIndex = 0;
        }
    }

    std::string selectedText() const {
        if (selectedIndex >= 0 && selectedIndex < static_cast<i32>(items.size())) {
            return items[selectedIndex];
        }
        return "";
    }

    f32 getDropdownHeight() const {
        return items.size() * 24 * Config::uiScale;
    }

    Rect getDropdownBounds() const {
        Rect global = globalBounds();
        return Rect(global.x, global.bottom(), global.w, getDropdownHeight());
    }

    void showDropdown();
    void hideDropdown();

    void renderSelf(Framebuffer& fb) override;
    bool onMouseDown(const MouseEvent& e) override;
    bool onMouseMove(const MouseEvent& e) override;
};

// Separator (horizontal or vertical line)
class Separator : public Widget {
public:
    bool horizontal = true;
    u32 color = Config::COLOR_BORDER;

    Separator(bool horiz = true) : horizontal(horiz) {
        if (horizontal) {
            preferredSize = Vec2(0, 1);
            minSize = Vec2(0, 1);
            maxSize = Vec2(10000, 1);
            horizontalPolicy = SizePolicy::Expanding;
            verticalPolicy = SizePolicy::Fixed;
        } else {
            preferredSize = Vec2(1, 0);
            minSize = Vec2(1, 0);
            maxSize = Vec2(1, 10000);
            horizontalPolicy = SizePolicy::Fixed;
            verticalPolicy = SizePolicy::Expanding;
        }
    }

    void renderSelf(Framebuffer& fb) override {
        Rect global = globalBounds();
        fb.fillRect(Recti(global), color);
    }
};

// Spacer (invisible, takes up space)
class Spacer : public Widget {
public:
    Spacer() {
        horizontalPolicy = SizePolicy::Expanding;
        verticalPolicy = SizePolicy::Expanding;
    }

    explicit Spacer(f32 fixedSize, bool horizontal = true) {
        if (horizontal) {
            preferredSize = Vec2(fixedSize, 0);
            horizontalPolicy = SizePolicy::Fixed;
            verticalPolicy = SizePolicy::Expanding;
        } else {
            preferredSize = Vec2(0, fixedSize);
            horizontalPolicy = SizePolicy::Expanding;
            verticalPolicy = SizePolicy::Fixed;
        }
    }
};

// Menu item for popup menus
struct MenuItem {
    std::string label;
    std::string shortcut;
    std::function<void()> action;
    bool separator = false;
    bool enabled = true;

    MenuItem() : separator(true) {}  // Separator constructor
    MenuItem(const std::string& lbl, const std::string& sc = "", std::function<void()> act = nullptr)
        : label(lbl), shortcut(sc), action(act) {}
};

// Popup menu widget
class PopupMenu : public Widget {
public:
    std::vector<MenuItem> items;
    i32 hoveredIndex = -1;
    u32 bgColor = Config::COLOR_PANEL;
    u32 hoverColor = Config::GRAY_500;
    u32 textColor = Config::COLOR_TEXT;
    u32 disabledColor = Config::COLOR_TEXT_DIM;
    u32 borderColor = Config::COLOR_BORDER;

    std::function<void()> onClose;

    PopupMenu() {
        visible = false;
    }

    void addItem(const std::string& label, const std::string& shortcut = "", std::function<void()> action = nullptr) {
        items.push_back(MenuItem(label, shortcut, action));
    }

    void addSeparator() {
        items.push_back(MenuItem());
    }

    void show(f32 x, f32 y);
    void hide();

    void renderSelf(Framebuffer& fb) override;
    bool onMouseMove(const MouseEvent& e) override;
    bool onMouseDown(const MouseEvent& e) override;
};

// Panel (container with background)
class Panel : public Widget {
public:
    u32 bgColor = Config::COLOR_PANEL;
    u32 borderColor = 0;  // No border by default
    i32 borderWidth = 0;

    Panel() {
        setPadding(4 * Config::uiScale);
    }

    void layout() override;
    void renderSelf(Framebuffer& fb) override;
};

// Tab bar for multi-document interface
class TabBar : public Widget {
public:
    struct Tab {
        std::string label;
        bool closable = true;
        void* userData = nullptr;  // e.g., Document*
    };

    std::vector<Tab> tabs;
    i32 activeIndex = 0;
    i32 hoveredIndex = -1;
    i32 hoveredCloseIndex = -1;  // Which tab's close button is hovered

    u32 bgColor = Config::COLOR_PANEL;
    u32 tabColor = Config::COLOR_BACKGROUND;
    u32 activeTabColor = Config::COLOR_PANEL_HEADER;
    u32 textColor = Config::COLOR_TEXT;
    u32 hoverColor = Config::COLOR_HOVER;
    u32 closeButtonColor = Config::COLOR_TEXT_DIM;
    u32 closeButtonHoverColor = 0xFFFF6666;

    f32 tabHeight = 28 * Config::uiScale;
    f32 tabPadding = 12 * Config::uiScale;
    f32 closeButtonSize = 14 * Config::uiScale;

    std::function<void(i32)> onTabSelected;
    std::function<void(i32)> onTabClosed;

    TabBar() {
        preferredSize = Vec2(0, tabHeight);
        verticalPolicy = SizePolicy::Fixed;
        horizontalPolicy = SizePolicy::Expanding;
    }

    void addTab(const std::string& label, void* userData = nullptr, bool closable = true) {
        tabs.push_back({label, closable, userData});
        if (tabs.size() == 1) {
            activeIndex = 0;
        }
        getAppState().needsRedraw = true;
    }

    void removeTab(i32 index) {
        if (index < 0 || index >= static_cast<i32>(tabs.size())) return;
        tabs.erase(tabs.begin() + index);
        if (activeIndex >= static_cast<i32>(tabs.size())) {
            activeIndex = static_cast<i32>(tabs.size()) - 1;
        }
        getAppState().needsRedraw = true;
    }

    void setActiveTab(i32 index) {
        if (index >= 0 && index < static_cast<i32>(tabs.size())) {
            activeIndex = index;
            getAppState().needsRedraw = true;
        }
    }

    void setTabLabel(i32 index, const std::string& label) {
        if (index >= 0 && index < static_cast<i32>(tabs.size())) {
            tabs[index].label = label;
            getAppState().needsRedraw = true;
        }
    }

    // Truncate label to max 20 characters with "..." if needed
    std::string getDisplayLabel(const std::string& label) const {
        const size_t maxChars = 20;
        if (label.length() <= maxChars) {
            return label;
        }
        return label.substr(0, maxChars) + "...";
    }

    // Calculate tab width based on label
    f32 getTabWidth(const Tab& tab) const {
        std::string displayLabel = getDisplayLabel(tab.label);
        Vec2 textSize = FontRenderer::instance().measureText(displayLabel, Config::defaultFontSize());
        f32 width = textSize.x + tabPadding * 2;
        if (tab.closable) {
            width += closeButtonSize + tabPadding / 2;
        }
        return std::max(width, 80.0f * Config::uiScale);  // Minimum tab width
    }

    // Get tab index at position
    i32 getTabAtPosition(f32 x) const {
        f32 tabX = 0;
        for (size_t i = 0; i < tabs.size(); ++i) {
            f32 width = getTabWidth(tabs[i]);
            if (x >= tabX && x < tabX + width) {
                return static_cast<i32>(i);
            }
            tabX += width;
        }
        return -1;
    }

    // Check if position is over a close button
    bool isOverCloseButton(f32 x, f32 y, i32 tabIndex) const;

    void renderSelf(Framebuffer& fb) override;
    bool onMouseDown(const MouseEvent& e) override;
    bool onMouseMove(const MouseEvent& e) override;
};

#endif
