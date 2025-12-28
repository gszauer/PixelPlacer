#ifndef _H_WIDGET_
#define _H_WIDGET_

#include "types.h"
#include "primitives.h"
#include "framebuffer.h"
#include "config.h"
#include <vector>
#include <memory>
#include <string>
#include <functional>

// Forward declarations
class Widget;
class Framebuffer;

// Mouse button identifiers
enum class MouseButton {
    None = 0,
    Left = 1,
    Middle = 2,
    Right = 3
};

// Key modifiers
struct KeyMods {
    bool shift = false;
    bool ctrl = false;
    bool alt = false;

    KeyMods() = default;
    KeyMods(bool s, bool c, bool a) : shift(s), ctrl(c), alt(a) {}
};

// Mouse event
struct MouseEvent {
    Vec2 position;       // Local coordinates relative to widget
    Vec2 globalPosition; // Screen coordinates
    MouseButton button = MouseButton::None;
    KeyMods mods;
    i32 wheelDelta = 0;  // Scroll wheel

    MouseEvent() = default;
    MouseEvent(const Vec2& pos, const Vec2& globalPos, MouseButton btn = MouseButton::None)
        : position(pos), globalPosition(globalPos), button(btn) {}

    MouseEvent translated(const Vec2& offset) const {
        MouseEvent e = *this;
        e.position = position - offset;
        return e;
    }
};

// Key event
struct KeyEvent {
    i32 keyCode = 0;
    i32 scanCode = 0;
    KeyMods mods;
    bool repeat = false;
    char character = 0;  // For text input
};

// Widget size policy
enum class SizePolicy {
    Fixed,      // Use exact size
    Minimum,    // At least minimum size
    Expanding,  // Expand to fill available space
    Preferred   // Prefer given size but can shrink/grow
};

// Base Widget class
class Widget {
public:
    Rect bounds;
    Widget* parent = nullptr;
    std::vector<std::unique_ptr<Widget>> children;

    bool visible = true;
    bool enabled = true;
    bool focusable = false;
    bool focused = false;
    bool hovered = false;

    std::string id;  // Optional identifier

    // Size hints
    Vec2 minSize = Vec2(0, 0);
    Vec2 maxSize = Vec2(10000, 10000);
    Vec2 preferredSize = Vec2(0, 0);
    SizePolicy horizontalPolicy = SizePolicy::Preferred;
    SizePolicy verticalPolicy = SizePolicy::Preferred;

    // Margins and padding
    f32 marginLeft = 0, marginRight = 0, marginTop = 0, marginBottom = 0;
    f32 paddingLeft = 0, paddingRight = 0, paddingTop = 0, paddingBottom = 0;

    Widget() = default;
    virtual ~Widget();

    // Non-copyable, movable
    Widget(const Widget&) = delete;
    Widget& operator=(const Widget&) = delete;
    Widget(Widget&&) = default;
    Widget& operator=(Widget&&) = default;

    // Child management
    Widget* addChild(std::unique_ptr<Widget> child) {
        child->parent = this;
        Widget* ptr = child.get();
        children.push_back(std::move(child));
        return ptr;
    }

    template<typename T, typename... Args>
    T* createChild(Args&&... args) {
        auto child = std::make_unique<T>(std::forward<Args>(args)...);
        T* ptr = child.get();
        addChild(std::move(child));
        return ptr;
    }

    void removeChild(Widget* child) {
        for (auto it = children.begin(); it != children.end(); ++it) {
            if (it->get() == child) {
                children.erase(it);
                return;
            }
        }
    }

    void clearChildren() {
        children.clear();
    }

    // Position and size
    void setPosition(f32 x, f32 y) { bounds.x = x; bounds.y = y; }
    void setPosition(const Vec2& pos) { bounds.x = pos.x; bounds.y = pos.y; }
    void setSize(f32 w, f32 h) { bounds.w = w; bounds.h = h; }
    void setSize(const Vec2& size) { bounds.w = size.x; bounds.h = size.y; }
    void setBounds(const Rect& r) { bounds = r; }
    void setBounds(f32 x, f32 y, f32 w, f32 h) { bounds = Rect(x, y, w, h); }

    Vec2 position() const { return Vec2(bounds.x, bounds.y); }
    Vec2 size() const { return Vec2(bounds.w, bounds.h); }

    void setMargins(f32 all) { marginLeft = marginRight = marginTop = marginBottom = all; }
    void setMargins(f32 h, f32 v) { marginLeft = marginRight = h; marginTop = marginBottom = v; }
    void setPadding(f32 all) { paddingLeft = paddingRight = paddingTop = paddingBottom = all; }
    void setPadding(f32 h, f32 v) { paddingLeft = paddingRight = h; paddingTop = paddingBottom = v; }

    // Coordinate conversion
    Vec2 localToGlobal(const Vec2& local) const {
        Vec2 result = local + position();
        if (parent) {
            result = parent->localToGlobal(result);
        }
        return result;
    }

    Vec2 globalToLocal(const Vec2& global) const {
        Vec2 result = global;
        if (parent) {
            result = parent->globalToLocal(result);
        }
        return result - position();
    }

    Rect globalBounds() const {
        Vec2 pos = localToGlobal(Vec2(0, 0));
        return Rect(pos.x, pos.y, bounds.w, bounds.h);
    }

    // Content rect (bounds minus padding)
    Rect contentRect() const {
        return Rect(paddingLeft, paddingTop,
                    bounds.w - paddingLeft - paddingRight,
                    bounds.h - paddingTop - paddingBottom);
    }

    // Hit testing
    virtual bool contains(const Vec2& point) const {
        return bounds.contains(point);
    }

    virtual Widget* findWidgetAt(const Vec2& point) {
        if (!visible || !bounds.contains(point)) return nullptr;

        Vec2 local = point - position();

        // Check children in reverse order (topmost first)
        for (auto it = children.rbegin(); it != children.rend(); ++it) {
            if (Widget* found = (*it)->findWidgetAt(local)) {
                return found;
            }
        }

        return this;
    }

    // Layout
    virtual void layout() {
        for (auto& child : children) {
            child->layout();
        }
    }

    // Rendering
    virtual void render(Framebuffer& fb) {
        if (!visible) return;
        renderSelf(fb);
        renderChildren(fb);
    }

    virtual void renderSelf(Framebuffer& fb) {
        // Override in subclasses
    }

    void renderChildren(Framebuffer& fb) {
        for (auto& child : children) {
            child->render(fb);
        }
    }

    // Event handlers - return true if handled
    virtual bool onMouseDown(const MouseEvent& e) {
        for (auto it = children.rbegin(); it != children.rend(); ++it) {
            if (!(*it)->visible || !(*it)->enabled) continue;
            if ((*it)->bounds.contains(e.position)) {
                if ((*it)->onMouseDown(e.translated((*it)->position()))) {
                    return true;
                }
            }
        }
        return false;
    }

    virtual bool onMouseUp(const MouseEvent& e) {
        for (auto it = children.rbegin(); it != children.rend(); ++it) {
            if (!(*it)->visible || !(*it)->enabled) continue;
            if ((*it)->bounds.contains(e.position)) {
                if ((*it)->onMouseUp(e.translated((*it)->position()))) {
                    return true;
                }
            }
        }
        return false;
    }

    virtual bool onMouseMove(const MouseEvent& e) {
        bool wasHovered = hovered;
        hovered = bounds.containsLocal(e.position);

        if (hovered && !wasHovered) onMouseEnter(e);
        if (!hovered && wasHovered) onMouseLeave(e);

        for (auto it = children.rbegin(); it != children.rend(); ++it) {
            if (!(*it)->visible) continue;
            (*it)->onMouseMove(e.translated((*it)->position()));
        }
        return false;
    }

    virtual void onMouseEnter(const MouseEvent& e) {}
    virtual void onMouseLeave(const MouseEvent& e) {}

    virtual bool onMouseDrag(const MouseEvent& e) { return false; }

    virtual bool onMouseWheel(const MouseEvent& e) {
        for (auto it = children.rbegin(); it != children.rend(); ++it) {
            if (!(*it)->visible || !(*it)->enabled) continue;
            if ((*it)->bounds.contains(e.position)) {
                if ((*it)->onMouseWheel(e.translated((*it)->position()))) {
                    return true;
                }
            }
        }
        return false;
    }

    virtual bool onKeyDown(const KeyEvent& e) {
        // Try focused child first
        for (auto& child : children) {
            if (child->focused && child->onKeyDown(e)) return true;
        }
        return false;
    }

    virtual bool onKeyUp(const KeyEvent& e) {
        for (auto& child : children) {
            if (child->focused && child->onKeyUp(e)) return true;
        }
        return false;
    }

    virtual bool onTextInput(const std::string& text) {
        for (auto& child : children) {
            if (child->focused && child->onTextInput(text)) return true;
        }
        return false;
    }

    virtual void onFocus() { focused = true; }
    virtual void onBlur() { focused = false; }

    // Request parent to redraw
    void requestRedraw() {
        // This will be connected to the application's dirty tracking
    }
};

#endif
