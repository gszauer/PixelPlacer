#ifndef _H_DOCUMENT_VIEW_
#define _H_DOCUMENT_VIEW_

#include "types.h"
#include "primitives.h"
#include "document.h"
#include "config.h"

// DocumentView - non-owning view of a document with its own transform
class DocumentView : public DocumentObserver {
public:
    Document* document = nullptr;  // Non-owning

    // View transform
    Vec2 pan = Vec2(0, 0);         // Pan offset in screen pixels
    f32 zoom = 1.0f;               // Zoom level (1.0 = 100%)

    // Viewport
    Rect viewport;                 // Screen-space viewport rectangle

    DocumentView() = default;
    explicit DocumentView(Document* doc) : document(doc) {
        if (document) {
            document->addObserver(this);
        }
    }

    ~DocumentView() override {
        if (document) {
            document->removeObserver(this);
        }
    }

    // Attach to document
    void setDocument(Document* doc) {
        if (document) {
            document->removeObserver(this);
        }
        document = doc;
        if (document) {
            document->addObserver(this);
            centerDocument();
        }
    }

    // Coordinate transforms
    Vec2 screenToDocument(const Vec2& screenPos) const {
        return Vec2(
            (screenPos.x - viewport.x - pan.x) / zoom,
            (screenPos.y - viewport.y - pan.y) / zoom
        );
    }

    Vec2 documentToScreen(const Vec2& docPos) const {
        return Vec2(
            docPos.x * zoom + pan.x + viewport.x,
            docPos.y * zoom + pan.y + viewport.y
        );
    }

    Rect screenToDocument(const Rect& screenRect) const {
        Vec2 topLeft = screenToDocument(screenRect.position());
        Vec2 bottomRight = screenToDocument(Vec2(screenRect.right(), screenRect.bottom()));
        return Rect(topLeft.x, topLeft.y, bottomRight.x - topLeft.x, bottomRight.y - topLeft.y);
    }

    Rect documentToScreen(const Rect& docRect) const {
        Vec2 topLeft = documentToScreen(docRect.position());
        Vec2 bottomRight = documentToScreen(Vec2(docRect.right(), docRect.bottom()));
        return Rect(topLeft.x, topLeft.y, bottomRight.x - topLeft.x, bottomRight.y - topLeft.y);
    }

    // Get visible document region
    Rect getVisibleDocumentRect() const {
        return screenToDocument(viewport);
    }

    // Zoom operations
    void setZoom(f32 newZoom) {
        zoom = clamp(newZoom, Config::MIN_ZOOM, Config::MAX_ZOOM);
    }

    void zoomIn() {
        setZoom(zoom * Config::ZOOM_STEP);
    }

    void zoomOut() {
        setZoom(zoom / Config::ZOOM_STEP);
    }

    void zoomToFit() {
        if (!document) return;

        f32 scaleX = viewport.w / document->width;
        f32 scaleY = viewport.h / document->height;
        setZoom(std::min(scaleX, scaleY) * 0.9f);  // 90% to leave margin
        centerDocument();
    }

    void zoomToFill() {
        if (!document) return;

        f32 scaleX = viewport.w / document->width;
        f32 scaleY = viewport.h / document->height;
        setZoom(std::max(scaleX, scaleY));
        centerDocument();
    }

    void zoomTo100() {
        setZoom(1.0f);
        centerDocument();
    }

    // Zoom at point (keeps point stationary on screen)
    void zoomAtPoint(const Vec2& screenPoint, f32 newZoom) {
        Vec2 docPoint = screenToDocument(screenPoint);
        setZoom(newZoom);

        // Adjust pan to keep docPoint at screenPoint
        pan.x = screenPoint.x - viewport.x - docPoint.x * zoom;
        pan.y = screenPoint.y - viewport.y - docPoint.y * zoom;
    }

    // Pan operations
    void panBy(const Vec2& delta) {
        pan += delta;
    }

    void centerDocument() {
        if (!document) return;

        f32 docScreenWidth = document->width * zoom;
        f32 docScreenHeight = document->height * zoom;

        pan.x = (viewport.w - docScreenWidth) / 2;
        pan.y = (viewport.h - docScreenHeight) / 2;
    }

    // Ensure point is visible
    void ensureVisible(const Vec2& docPoint) {
        Vec2 screenPoint = documentToScreen(docPoint);
        f32 margin = 50.0f;

        if (screenPoint.x < viewport.x + margin) {
            pan.x += viewport.x + margin - screenPoint.x;
        } else if (screenPoint.x > viewport.right() - margin) {
            pan.x -= screenPoint.x - (viewport.right() - margin);
        }

        if (screenPoint.y < viewport.y + margin) {
            pan.y += viewport.y + margin - screenPoint.y;
        } else if (screenPoint.y > viewport.bottom() - margin) {
            pan.y -= screenPoint.y - (viewport.bottom() - margin);
        }
    }

    // Check if document point is visible
    bool isVisible(const Vec2& docPoint) const {
        Vec2 screenPoint = documentToScreen(docPoint);
        return viewport.contains(screenPoint);
    }

    // Get zoom percentage string
    std::string getZoomString() const {
        i32 percent = static_cast<i32>(zoom * 100 + 0.5f);
        return std::to_string(percent) + "%";
    }

    // DocumentObserver implementation
    void onDocumentChanged(const Rect& dirtyRect) override {
        // View will handle redraw
    }
};

#endif
