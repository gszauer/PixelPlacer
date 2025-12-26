#include "layouts.h"
#include "app_state.h"

// HBoxLayout implementation
void HBoxLayout::layout() {
    if (children.empty()) return;

    f32 availableWidth = bounds.w - paddingLeft - paddingRight;
    f32 availableHeight = bounds.h - paddingTop - paddingBottom;
    f32 totalSpacing = spacing * (children.size() - 1);

    // Calculate total fixed/preferred width and count expanding widgets
    f32 fixedWidth = 0;
    i32 expandingCount = 0;

    for (auto& child : children) {
        if (!child->visible) continue;
        if (child->horizontalPolicy == SizePolicy::Expanding) {
            expandingCount++;
        } else {
            f32 w = child->preferredSize.x > 0 ? child->preferredSize.x : child->minSize.x;
            fixedWidth += w + child->marginLeft + child->marginRight;
        }
    }

    f32 remainingWidth = availableWidth - fixedWidth - totalSpacing;
    f32 expandWidth = expandingCount > 0 ? remainingWidth / expandingCount : 0;

    f32 x = paddingLeft;
    for (auto& child : children) {
        if (!child->visible) continue;

        f32 w;
        if (child->horizontalPolicy == SizePolicy::Expanding) {
            w = expandWidth - child->marginLeft - child->marginRight;
        } else {
            w = child->preferredSize.x > 0 ? child->preferredSize.x : child->minSize.x;
        }
        w = std::max(w, child->minSize.x);
        w = std::min(w, child->maxSize.x);

        f32 h = stretch ? availableHeight - child->marginTop - child->marginBottom
                        : (child->preferredSize.y > 0 ? child->preferredSize.y : child->minSize.y);
        h = std::max(h, child->minSize.y);
        h = std::min(h, child->maxSize.y);

        f32 y = paddingTop + child->marginTop;
        if (!stretch && h < availableHeight) {
            y = paddingTop + (availableHeight - h) / 2;  // Center vertically
        }

        child->setBounds(x + child->marginLeft, y, w, h);
        child->layout();

        x += w + child->marginLeft + child->marginRight + spacing;
    }
}

// VBoxLayout implementation
void VBoxLayout::layout() {
    if (children.empty()) return;

    f32 availableWidth = bounds.w - paddingLeft - paddingRight;
    f32 availableHeight = bounds.h - paddingTop - paddingBottom;
    f32 totalSpacing = spacing * (children.size() - 1);

    // Calculate total fixed/preferred height and count expanding widgets
    f32 fixedHeight = 0;
    i32 expandingCount = 0;

    for (auto& child : children) {
        if (!child->visible) continue;
        if (child->verticalPolicy == SizePolicy::Expanding) {
            expandingCount++;
        } else {
            f32 h = child->preferredSize.y > 0 ? child->preferredSize.y : child->minSize.y;
            fixedHeight += h + child->marginTop + child->marginBottom;
        }
    }

    f32 remainingHeight = availableHeight - fixedHeight - totalSpacing;
    f32 expandHeight = expandingCount > 0 ? remainingHeight / expandingCount : 0;

    f32 y = paddingTop;
    for (auto& child : children) {
        if (!child->visible) continue;

        f32 h;
        if (child->verticalPolicy == SizePolicy::Expanding) {
            h = expandHeight - child->marginTop - child->marginBottom;
        } else {
            h = child->preferredSize.y > 0 ? child->preferredSize.y : child->minSize.y;
        }
        h = std::max(h, child->minSize.y);
        h = std::min(h, child->maxSize.y);

        f32 w = stretch ? availableWidth - child->marginLeft - child->marginRight
                        : (child->preferredSize.x > 0 ? child->preferredSize.x : child->minSize.x);
        w = std::max(w, child->minSize.x);
        w = std::min(w, child->maxSize.x);

        f32 x = paddingLeft + child->marginLeft;
        if (!stretch && w < availableWidth) {
            x = paddingLeft + (availableWidth - w) / 2;  // Center horizontally
        }

        child->setBounds(x, y + child->marginTop, w, h);
        child->layout();

        y += h + child->marginTop + child->marginBottom + spacing;
    }
}

// GridLayout implementation
void GridLayout::layout() {
    if (children.empty() || columns == 0) return;

    f32 availableWidth = bounds.w - paddingLeft - paddingRight;
    f32 availableHeight = bounds.h - paddingTop - paddingBottom;

    u32 rows = (children.size() + columns - 1) / columns;

    f32 totalHSpacing = hSpacing * (columns - 1);
    f32 totalVSpacing = vSpacing * (rows - 1);

    f32 cellWidth = (availableWidth - totalHSpacing) / columns;
    f32 cellHeight = (availableHeight - totalVSpacing) / rows;

    for (size_t i = 0; i < children.size(); ++i) {
        if (!children[i]->visible) continue;

        u32 col = i % columns;
        u32 row = i / columns;

        f32 x = paddingLeft + col * (cellWidth + hSpacing);
        f32 y = paddingTop + row * (cellHeight + vSpacing);

        f32 w = cellWidth - children[i]->marginLeft - children[i]->marginRight;
        f32 h = cellHeight - children[i]->marginTop - children[i]->marginBottom;

        // Respect child's size policies
        Widget* child = children[i].get();
        if (child->horizontalPolicy == SizePolicy::Fixed) {
            w = std::min(w, child->preferredSize.x);
        }
        w = clamp(w, child->minSize.x, child->maxSize.x);

        if (child->verticalPolicy == SizePolicy::Fixed) {
            h = std::min(h, child->preferredSize.y);
        }
        h = clamp(h, child->minSize.y, child->maxSize.y);

        child->setBounds(x + child->marginLeft, y + child->marginTop, w, h);
        child->layout();
    }
}

// StackLayout implementation
void StackLayout::layout() {
    Rect content = contentRect();

    for (auto& child : children) {
        if (!child->visible) continue;
        child->setBounds(
            content.x + child->marginLeft,
            content.y + child->marginTop,
            content.w - child->marginLeft - child->marginRight,
            content.h - child->marginTop - child->marginBottom
        );
        child->layout();
    }
}

// ScrollView implementation
f32 ScrollView::getMaxScroll() const {
    return std::max(0.0f, contentHeight - bounds.h);
}

f32 ScrollView::getViewportHeight() const {
    return bounds.h;
}

void ScrollView::clampScroll() {
    f32 maxScroll = getMaxScroll();
    scrollOffset = std::max(0.0f, std::min(scrollOffset, maxScroll));
}

Rect ScrollView::getScrollbarThumbRect() const {
    Rect gb = globalBounds();
    f32 trackX = gb.x + gb.w - scrollbarWidth;

    if (contentHeight <= 0) {
        return Rect(trackX, gb.y, scrollbarWidth, gb.h);
    }

    f32 viewportRatio = bounds.h / contentHeight;
    f32 thumbHeight = std::max(20.0f, bounds.h * viewportRatio);

    f32 maxScroll = getMaxScroll();
    f32 scrollRatio = (maxScroll > 0) ? (scrollOffset / maxScroll) : 0.0f;
    f32 thumbY = gb.y + scrollRatio * (bounds.h - thumbHeight);

    return Rect(trackX, thumbY, scrollbarWidth, thumbHeight);
}

void ScrollView::layout() {
    if (children.empty()) return;

    Widget* content = children[0].get();
    if (!content) return;

    // Content width = our width minus scrollbar (if needed)
    f32 contentWidth = bounds.w;

    // First pass: layout content to get its natural height
    // Give it a very tall height so it can expand naturally
    content->bounds.x = 0;
    content->bounds.y = 0;
    content->bounds.w = contentWidth;
    content->bounds.h = 10000;  // Large height to allow natural sizing
    content->layout();

    // Calculate actual content height by finding the bottom of all children
    contentHeight = calculateContentHeight(content);

    // If content fits, hide scrollbar and use full width
    bool needsScrollbar = contentHeight > bounds.h;
    if (needsScrollbar && showScrollbar) {
        contentWidth = bounds.w - scrollbarWidth - scrollbarMargin;
    } else {
        scrollOffset = 0;
    }

    // Second pass: layout with correct width and scroll offset
    content->bounds.x = 0;
    content->bounds.y = -scrollOffset;
    content->bounds.w = contentWidth;
    content->bounds.h = contentHeight;
    content->layout();

    clampScroll();
}

f32 ScrollView::calculateContentHeight(Widget* widget) {
    if (!widget || !widget->visible) return 0;

    f32 maxBottom = 0;
    for (auto& child : widget->children) {
        if (!child->visible) continue;
        // Get child's bottom edge relative to widget
        f32 childBottom = child->bounds.y + child->bounds.h;
        // For nested containers, also check their children
        if (!child->children.empty()) {
            f32 nestedHeight = calculateContentHeight(child.get());
            // If nested content is taller than the child's bounds, use that
            if (nestedHeight > child->bounds.h) {
                childBottom = child->bounds.y + nestedHeight;
            }
        }
        maxBottom = std::max(maxBottom, childBottom);
    }

    // Add padding
    maxBottom += widget->paddingBottom;

    return maxBottom;
}

void ScrollView::renderSelf(Framebuffer& fb) {
    // Draw scrollbar if content exceeds viewport
    if (showScrollbar && contentHeight > bounds.h) {
        Rect gb = globalBounds();

        // Scrollbar track
        f32 trackX = gb.x + gb.w - scrollbarWidth;
        Recti trackRect(
            static_cast<i32>(trackX),
            static_cast<i32>(gb.y),
            static_cast<i32>(scrollbarWidth),
            static_cast<i32>(gb.h)
        );
        fb.fillRect(trackRect, Config::COLOR_BACKGROUND);

        // Scrollbar thumb
        Rect thumbRect = getScrollbarThumbRect();
        Recti thumb(
            static_cast<i32>(thumbRect.x),
            static_cast<i32>(thumbRect.y),
            static_cast<i32>(thumbRect.w),
            static_cast<i32>(thumbRect.h)
        );
        fb.fillRect(thumb, draggingScrollbar ? Config::COLOR_ACTIVE : Config::COLOR_HOVER);
    }
}

void ScrollView::render(Framebuffer& fb) {
    if (!visible) return;

    // Render children with clipping
    if (!children.empty()) {
        Rect gb = globalBounds();
        f32 contentWidth = bounds.w - (showScrollbar && contentHeight > bounds.h ? scrollbarWidth + scrollbarMargin : 0);

        // Set clip rectangle
        Recti clipRect(
            static_cast<i32>(gb.x),
            static_cast<i32>(gb.y),
            static_cast<i32>(contentWidth),
            static_cast<i32>(gb.h)
        );
        fb.pushClip(clipRect);

        // Render content
        children[0]->render(fb);

        fb.popClip();
    }

    // Render scrollbar on top (not clipped)
    renderSelf(fb);
}

bool ScrollView::onMouseWheel(const MouseEvent& e) {
    if (contentHeight <= bounds.h) return false;  // Nothing to scroll

    scrollOffset -= e.wheelDelta * scrollSpeed;
    clampScroll();

    // Re-layout to update content position
    layout();
    getAppState().needsRedraw = true;
    return true;
}

bool ScrollView::onMouseDown(const MouseEvent& e) {
    if (e.button != MouseButton::Left) return false;

    // Check if clicking scrollbar area (including margin)
    if (showScrollbar && contentHeight > bounds.h) {
        f32 scrollbarX = bounds.w - scrollbarWidth - scrollbarMargin;
        if (e.position.x >= scrollbarX) {
            // Check if clicking scrollbar thumb
            Vec2 globalPos = localToGlobal(e.position);
            Rect thumbRect = getScrollbarThumbRect();

            if (thumbRect.contains(globalPos)) {
                draggingScrollbar = true;
                dragStartY = globalPos.y;
                dragStartOffset = scrollOffset;
                getAppState().capturedWidget = this;
                return true;
            }

            // Click on scrollbar track - page up/down
            if (globalPos.y < thumbRect.y) {
                scrollOffset -= bounds.h * 0.9f;
            } else if (globalPos.y > thumbRect.y + thumbRect.h) {
                scrollOffset += bounds.h * 0.9f;
            }
            clampScroll();
            layout();
            getAppState().needsRedraw = true;
            return true;
        }
    }

    // Route to content - translated() handles the scroll offset since
    // content's position is (0, -scrollOffset)
    if (!children.empty()) {
        return children[0]->onMouseDown(e.translated(children[0]->position()));
    }

    return false;
}

bool ScrollView::onMouseDrag(const MouseEvent& e) {
    if (draggingScrollbar) {
        Vec2 globalPos = localToGlobal(e.position);
        f32 deltaY = globalPos.y - dragStartY;

        // Convert pixel delta to scroll delta
        Rect thumbRect = getScrollbarThumbRect();
        f32 trackHeight = bounds.h - thumbRect.h;
        f32 scrollRange = getMaxScroll();

        if (trackHeight > 0 && scrollRange > 0) {
            scrollOffset = dragStartOffset + (deltaY / trackHeight) * scrollRange;
        }

        clampScroll();
        layout();
        getAppState().needsRedraw = true;
        return true;
    }

    // Route to content
    if (!children.empty()) {
        return children[0]->onMouseDrag(e.translated(children[0]->position()));
    }

    return false;
}

bool ScrollView::onMouseUp(const MouseEvent& e) {
    if (draggingScrollbar) {
        draggingScrollbar = false;
        getAppState().capturedWidget = nullptr;
        getAppState().needsRedraw = true;
        return true;
    }

    // Route to content
    if (!children.empty()) {
        return children[0]->onMouseUp(e.translated(children[0]->position()));
    }

    return false;
}

bool ScrollView::onMouseMove(const MouseEvent& e) {
    // Route to content
    if (!children.empty()) {
        children[0]->onMouseMove(e.translated(children[0]->position()));
    }
    return false;
}

Widget* ScrollView::findWidgetAt(const Vec2& point) {
    if (!visible || !bounds.contains(point)) return nullptr;

    // First check if point is in scrollbar area (including margin)
    if (showScrollbar && contentHeight > bounds.h) {
        f32 scrollbarX = bounds.w - scrollbarWidth - scrollbarMargin;
        if (point.x >= scrollbarX) {
            return this;  // Scrollbar handles it
        }
    }

    // Check content - the content's bounds.y is already set to -scrollOffset in layout(),
    // so we don't need to adjust the point here. The standard Widget::findWidgetAt
    // coordinate conversion will handle it correctly.
    if (!children.empty()) {
        Vec2 local = point - position();
        Widget* found = children[0]->findWidgetAt(local);
        if (found) return found;
    }

    return this;
}

void ScrollView::ensureVisible(Widget* widget) {
    if (!widget || children.empty()) return;

    // Calculate widget's position relative to scroll view content
    f32 widgetTop = 0;
    f32 widgetBottom = 0;

    // Walk up from widget to find its offset in content
    Widget* current = widget;
    while (current && current != children[0].get() && current != this) {
        widgetTop += current->bounds.y;
        current = current->parent;
    }
    widgetBottom = widgetTop + widget->bounds.h;

    // Check if widget is above visible area
    if (widgetTop < scrollOffset) {
        scrollOffset = widgetTop;
        clampScroll();
        layout();
        getAppState().needsRedraw = true;
    }
    // Check if widget is below visible area
    else if (widgetBottom > scrollOffset + bounds.h) {
        scrollOffset = widgetBottom - bounds.h;
        clampScroll();
        layout();
        getAppState().needsRedraw = true;
    }
}

void ScrollView::scrollToTop() {
    scrollOffset = 0;
    layout();
    getAppState().needsRedraw = true;
}

void ScrollView::scrollToBottom() {
    scrollOffset = getMaxScroll();
    layout();
    getAppState().needsRedraw = true;
}
