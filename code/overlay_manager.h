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
    constexpr i32 MODAL_DROPDOWN = 110; // Dropdowns inside modal dialogs
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

    OverlayManager() = default;
    void sortOverlays();

public:
    static OverlayManager& instance();

    // Register an overlay widget with a z-order
    void registerOverlay(Widget* widget, i32 zOrder = 0, bool blockInput = false);

    // Register with click-outside callback
    void registerOverlay(Widget* widget, i32 zOrder, std::function<void()> onClickOutside);

    // Unregister an overlay
    void unregisterOverlay(Widget* widget);

    // Check if any overlays are visible
    bool hasVisibleOverlays() const;

    // Check if there's a visible blocking modal overlay
    bool hasBlockingModal() const;

    // Get the topmost visible overlay
    Widget* getTopmostOverlay() const;

    // Render all visible overlays (call after main UI render)
    void renderOverlays(Framebuffer& fb);

    // Route mouse down event - returns true if consumed by an overlay
    bool routeMouseDown(const MouseEvent& e);

    // Route mouse up event - returns true if consumed by an overlay
    bool routeMouseUp(const MouseEvent& e);

    // Route mouse move event - returns true if consumed by an overlay
    bool routeMouseMove(const MouseEvent& e);

    // Route mouse drag event to overlays - returns true if consumed
    bool routeMouseDrag(const MouseEvent& e);

    // Hide all overlays at or below a certain z-order
    void hideOverlaysBelow(i32 zOrder);

    // Hide all overlays
    void hideAllOverlays();

    // Clear all registrations
    void clear();
};

#endif
