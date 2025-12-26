#ifndef _H_OVERLAY_MANAGER_
#define _H_OVERLAY_MANAGER_

#include "types.h"
#include "primitives.h"
#include "widget.h"
#include <vector>
#include <algorithm>

// Z-Order levels for overlays
namespace ZOrder {
    constexpr i32 DROPDOWN = 0;       // ComboBox dropdowns
    constexpr i32 POPUP_MENU = 10;    // PopupMenus
    constexpr i32 MODAL_DIALOG = 100; // Modal dialogs
}

// Manages rendering and event routing for overlay widgets (popups, dropdowns, dialogs)
class OverlayManager {
public:
    struct OverlayEntry {
        Widget* widget = nullptr;
        i32 zOrder = 0;
        bool blockInput = false;  // If true, blocks input to widgets below

        // Optional callback when clicking outside the overlay
        std::function<void()> onClickOutside;
    };

private:
    std::vector<OverlayEntry> overlays;

public:
    static OverlayManager& instance() {
        static OverlayManager mgr;
        return mgr;
    }

    // Register an overlay widget with a z-order
    void registerOverlay(Widget* widget, i32 zOrder = 0, bool blockInput = false) {
        if (!widget) return;

        // Check if already registered
        for (auto& entry : overlays) {
            if (entry.widget == widget) {
                entry.zOrder = zOrder;
                entry.blockInput = blockInput;
                sortOverlays();
                return;
            }
        }

        OverlayEntry entry;
        entry.widget = widget;
        entry.zOrder = zOrder;
        entry.blockInput = blockInput;
        overlays.push_back(entry);
        sortOverlays();
    }

    // Register with click-outside callback
    void registerOverlay(Widget* widget, i32 zOrder, std::function<void()> onClickOutside) {
        if (!widget) return;

        for (auto& entry : overlays) {
            if (entry.widget == widget) {
                entry.zOrder = zOrder;
                entry.onClickOutside = onClickOutside;
                sortOverlays();
                return;
            }
        }

        OverlayEntry entry;
        entry.widget = widget;
        entry.zOrder = zOrder;
        entry.onClickOutside = onClickOutside;
        overlays.push_back(entry);
        sortOverlays();
    }

    // Unregister an overlay
    void unregisterOverlay(Widget* widget) {
        overlays.erase(
            std::remove_if(overlays.begin(), overlays.end(),
                [widget](const OverlayEntry& e) { return e.widget == widget; }),
            overlays.end()
        );
    }

    // Check if any overlays are visible
    bool hasVisibleOverlays() const {
        for (const auto& entry : overlays) {
            if (entry.widget && entry.widget->visible) {
                return true;
            }
        }
        return false;
    }

    // Check if there's a visible blocking modal overlay
    bool hasBlockingModal() const {
        for (const auto& entry : overlays) {
            if (entry.widget && entry.widget->visible && entry.blockInput) {
                return true;
            }
        }
        return false;
    }

    // Get the topmost visible overlay
    Widget* getTopmostOverlay() const {
        for (auto it = overlays.rbegin(); it != overlays.rend(); ++it) {
            if (it->widget && it->widget->visible) {
                return it->widget;
            }
        }
        return nullptr;
    }

    // Render all visible overlays (call after main UI render)
    void renderOverlays(Framebuffer& fb) {
        for (const auto& entry : overlays) {
            if (entry.widget && entry.widget->visible) {
                entry.widget->render(fb);
            }
        }
    }

    // Route mouse down event - returns true if consumed by an overlay
    bool routeMouseDown(const MouseEvent& e) {
        // Check overlays from top to bottom
        for (auto it = overlays.rbegin(); it != overlays.rend(); ++it) {
            if (!it->widget || !it->widget->visible) continue;

            Rect globalBounds = it->widget->globalBounds();

            if (globalBounds.contains(e.globalPosition)) {
                // Click inside overlay - route to it
                Vec2 local = it->widget->globalToLocal(e.globalPosition);
                MouseEvent localEvent = e;
                localEvent.position = local;
                it->widget->onMouseDown(localEvent);
                return true;
            } else {
                // Click outside overlay
                if (it->onClickOutside) {
                    it->onClickOutside();
                }
                // If this overlay blocks input, stop here
                if (it->blockInput) {
                    return true;
                }
            }
        }
        return false;
    }

    // Route mouse up event - returns true if consumed by an overlay
    bool routeMouseUp(const MouseEvent& e) {
        for (auto it = overlays.rbegin(); it != overlays.rend(); ++it) {
            if (!it->widget || !it->widget->visible) continue;

            Rect globalBounds = it->widget->globalBounds();

            if (globalBounds.contains(e.globalPosition)) {
                Vec2 local = it->widget->globalToLocal(e.globalPosition);
                MouseEvent localEvent = e;
                localEvent.position = local;
                it->widget->onMouseUp(localEvent);
                return true;
            }

            if (it->blockInput) {
                return true;
            }
        }
        return false;
    }

    // Route mouse move event - returns true if consumed by an overlay
    bool routeMouseMove(const MouseEvent& e) {
        bool consumed = false;

        for (auto it = overlays.rbegin(); it != overlays.rend(); ++it) {
            if (!it->widget || !it->widget->visible) continue;

            Rect globalBounds = it->widget->globalBounds();

            // Always send mouse move for hover tracking
            Vec2 local = it->widget->globalToLocal(e.globalPosition);
            MouseEvent localEvent = e;
            localEvent.position = local;
            it->widget->onMouseMove(localEvent);

            if (globalBounds.contains(e.globalPosition)) {
                consumed = true;
            }

            if (it->blockInput) {
                consumed = true;
            }
        }
        return consumed;
    }

    // Route mouse drag event to overlays - returns true if consumed
    bool routeMouseDrag(const MouseEvent& e) {
        for (auto it = overlays.rbegin(); it != overlays.rend(); ++it) {
            if (!it->widget || !it->widget->visible) continue;

            // Find widget under mouse within the overlay and send drag event
            Widget* target = it->widget->findWidgetAt(e.globalPosition);
            if (!target) {
                // Even if not directly over widget, send to overlay for drag tracking
                target = it->widget;
            }

            while (target) {
                Vec2 local = target->globalToLocal(e.globalPosition);
                MouseEvent localEvent = e;
                localEvent.position = local;
                if (target->onMouseDrag(localEvent)) {
                    return true;
                }
                target = target->parent;
                // Stop bubbling when we reach the overlay root
                if (target && target->parent == nullptr) break;
            }

            if (it->blockInput) {
                return true;
            }
        }
        return false;
    }

    // Hide all overlays at or below a certain z-order
    void hideOverlaysBelow(i32 zOrder) {
        for (auto& entry : overlays) {
            if (entry.widget && entry.zOrder <= zOrder) {
                entry.widget->visible = false;
            }
        }
    }

    // Hide all overlays
    void hideAllOverlays() {
        for (auto& entry : overlays) {
            if (entry.widget) {
                entry.widget->visible = false;
            }
        }
    }

    // Clear all registrations
    void clear() {
        overlays.clear();
    }

private:
    OverlayManager() = default;

    void sortOverlays() {
        std::stable_sort(overlays.begin(), overlays.end(),
            [](const OverlayEntry& a, const OverlayEntry& b) {
                return a.zOrder < b.zOrder;
            });
    }
};

#endif
