#include "overlay_manager.h"

OverlayManager& OverlayManager::instance() {
    static OverlayManager mgr;
    return mgr;
}

void OverlayManager::sortOverlays() {
    std::stable_sort(overlays.begin(), overlays.end(),
        [](const OverlayEntry& a, const OverlayEntry& b) {
            return a.zOrder < b.zOrder;
        });
}

void OverlayManager::registerOverlay(Widget* widget, i32 zOrder, bool blockInput) {
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

void OverlayManager::registerOverlay(Widget* widget, i32 zOrder, std::function<void()> onClickOutside) {
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

void OverlayManager::unregisterOverlay(Widget* widget) {
    overlays.erase(
        std::remove_if(overlays.begin(), overlays.end(),
            [widget](const OverlayEntry& e) { return e.widget == widget; }),
        overlays.end()
    );
}

bool OverlayManager::hasVisibleOverlays() const {
    for (const auto& entry : overlays) {
        if (entry.widget && entry.widget->visible) {
            return true;
        }
    }
    return false;
}

bool OverlayManager::hasBlockingModal() const {
    for (const auto& entry : overlays) {
        if (entry.widget && entry.widget->visible && entry.blockInput) {
            return true;
        }
    }
    return false;
}

Widget* OverlayManager::getTopmostOverlay() const {
    for (auto it = overlays.rbegin(); it != overlays.rend(); ++it) {
        if (it->widget && it->widget->visible) {
            return it->widget;
        }
    }
    return nullptr;
}

void OverlayManager::renderOverlays(Framebuffer& fb) {
    for (const auto& entry : overlays) {
        if (entry.widget && entry.widget->visible) {
            entry.widget->render(fb);
        }
    }
}

bool OverlayManager::routeMouseDown(const MouseEvent& e) {
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

bool OverlayManager::routeMouseUp(const MouseEvent& e) {
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

bool OverlayManager::routeMouseMove(const MouseEvent& e) {
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

bool OverlayManager::routeMouseDrag(const MouseEvent& e) {
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

void OverlayManager::hideOverlaysBelow(i32 zOrder) {
    for (auto& entry : overlays) {
        if (entry.widget && entry.zOrder <= zOrder) {
            entry.widget->visible = false;
        }
    }
}

void OverlayManager::hideAllOverlays() {
    for (auto& entry : overlays) {
        if (entry.widget) {
            entry.widget->visible = false;
        }
    }
}

void OverlayManager::clear() {
    overlays.clear();
}
