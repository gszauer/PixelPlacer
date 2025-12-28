#ifndef _H_SELECTION_
#define _H_SELECTION_

#include "types.h"
#include "primitives.h"
#include <vector>
#include <memory>

enum class SelectionType {
    None,
    Rectangle,
    Ellipse,
    Polygon,
    Freehand,
    MagicWand
};

// Selection represented as a grayscale mask (0 = not selected, 255 = fully selected)
// Supports feathered/anti-aliased selections
class Selection {
public:
    std::vector<u8> mask;
    u32 width = 0;
    u32 height = 0;
    Recti bounds;  // Bounding rect of selection (optimization)
    bool hasSelection = false;

    Selection() = default;
    Selection(u32 w, u32 h);

    void resize(u32 w, u32 h);
    void clear();
    void selectAll();
    void invert();

    u8 getValue(u32 x, u32 y) const;
    void setValue(u32 x, u32 y, u8 value);
    bool isSelected(u32 x, u32 y) const;
    bool isFullySelected(u32 x, u32 y) const;

    // Set rectangular selection
    void setRectangle(const Recti& rect, bool add = false, bool subtract = false, bool antiAlias = false);

    // Set elliptical selection
    void setEllipse(const Recti& rect, bool add = false, bool subtract = false, bool antiAlias = true);

    // Add polygon selection (list of points)
    void setPolygon(const std::vector<Vec2>& points, bool add = false, bool subtract = false, bool antiAlias = true);

    // Feather the selection edge
    void feather(f32 radius);

    // Expand/contract selection
    void grow(i32 pixels);

    // Offset/translate the selection by given delta
    void offset(i32 dx, i32 dy);

    void updateBounds();

    // Create a copy
    std::unique_ptr<Selection> clone() const;

private:
    // Point-in-polygon test using ray casting
    static bool pointInPolygon(const Vec2& point, const std::vector<Vec2>& polygon);

    // Calculate distance from point to nearest polygon edge
    static f32 distanceToPolygonEdge(const Vec2& point, const std::vector<Vec2>& polygon);
};

#endif
