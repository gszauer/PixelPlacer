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
    Selection(u32 w, u32 h) : width(w), height(h), mask(w * h, 0) {}

    void resize(u32 w, u32 h) {
        width = w;
        height = h;
        mask.resize(w * h, 0);
        clear();
    }

    void clear() {
        std::fill(mask.begin(), mask.end(), 0);
        bounds = Recti(0, 0, 0, 0);
        hasSelection = false;
    }

    void selectAll() {
        std::fill(mask.begin(), mask.end(), 255);
        bounds = Recti(0, 0, width, height);
        hasSelection = true;
    }

    void invert() {
        for (auto& v : mask) {
            v = 255 - v;
        }
        updateBounds();
    }

    u8 getValue(u32 x, u32 y) const {
        if (x >= width || y >= height) return 0;
        return mask[y * width + x];
    }

    void setValue(u32 x, u32 y, u8 value) {
        if (x >= width || y >= height) return;
        mask[y * width + x] = value;
    }

    bool isSelected(u32 x, u32 y) const {
        return getValue(x, y) > 0;
    }

    bool isFullySelected(u32 x, u32 y) const {
        return getValue(x, y) == 255;
    }

    // Set rectangular selection
    void setRectangle(const Recti& rect, bool add = false, bool subtract = false, bool antiAlias = false) {
        if (!add && !subtract) clear();

        i32 x0 = clamp(rect.x, 0, static_cast<i32>(width));
        i32 y0 = clamp(rect.y, 0, static_cast<i32>(height));
        i32 x1 = clamp(rect.x + rect.w, 0, static_cast<i32>(width));
        i32 y1 = clamp(rect.y + rect.h, 0, static_cast<i32>(height));

        for (i32 y = y0; y < y1; ++y) {
            for (i32 x = x0; x < x1; ++x) {
                if (subtract) {
                    mask[y * width + x] = 0;
                } else {
                    mask[y * width + x] = 255;
                }
            }
        }

        updateBounds();
    }

    // Set elliptical selection
    void setEllipse(const Recti& rect, bool add = false, bool subtract = false, bool antiAlias = true) {
        if (!add && !subtract) clear();

        f32 cx = rect.x + rect.w * 0.5f;
        f32 cy = rect.y + rect.h * 0.5f;
        f32 rx = rect.w * 0.5f;
        f32 ry = rect.h * 0.5f;

        if (rx <= 0 || ry <= 0) return;

        i32 x0 = clamp(rect.x - 1, 0, static_cast<i32>(width));
        i32 y0 = clamp(rect.y - 1, 0, static_cast<i32>(height));
        i32 x1 = clamp(rect.x + rect.w + 1, 0, static_cast<i32>(width));
        i32 y1 = clamp(rect.y + rect.h + 1, 0, static_cast<i32>(height));

        for (i32 y = y0; y < y1; ++y) {
            for (i32 x = x0; x < x1; ++x) {
                f32 dx = (x + 0.5f - cx) / rx;
                f32 dy = (y + 0.5f - cy) / ry;
                f32 d = dx * dx + dy * dy;

                if (d <= 1.0f) {
                    if (subtract) {
                        mask[y * width + x] = 0;
                    } else {
                        u8 alpha = 255;
                        // Anti-aliasing at edge
                        if (antiAlias && d > 0.9f) {
                            alpha = static_cast<u8>((1.0f - d) * 10.0f * 255.0f);
                        }
                        if (add) {
                            mask[y * width + x] = std::max(mask[y * width + x], alpha);
                        } else {
                            mask[y * width + x] = alpha;
                        }
                    }
                }
            }
        }

        updateBounds();
    }

    // Add polygon selection (list of points)
    void setPolygon(const std::vector<Vec2>& points, bool add = false, bool subtract = false, bool antiAlias = true) {
        if (points.size() < 3) return;
        if (!add && !subtract) clear();

        // Find bounding box
        f32 minX = points[0].x, maxX = points[0].x;
        f32 minY = points[0].y, maxY = points[0].y;
        for (const auto& p : points) {
            minX = std::min(minX, p.x);
            maxX = std::max(maxX, p.x);
            minY = std::min(minY, p.y);
            maxY = std::max(maxY, p.y);
        }

        i32 x0 = clamp(static_cast<i32>(minX) - 1, 0, static_cast<i32>(width));
        i32 y0 = clamp(static_cast<i32>(minY) - 1, 0, static_cast<i32>(height));
        i32 x1 = clamp(static_cast<i32>(maxX) + 2, 0, static_cast<i32>(width));
        i32 y1 = clamp(static_cast<i32>(maxY) + 2, 0, static_cast<i32>(height));

        // Point-in-polygon test for each pixel
        for (i32 y = y0; y < y1; ++y) {
            for (i32 x = x0; x < x1; ++x) {
                Vec2 pixelCenter(x + 0.5f, y + 0.5f);
                bool inside = pointInPolygon(pixelCenter, points);

                if (antiAlias) {
                    // Calculate distance to nearest edge for anti-aliasing
                    f32 minDist = distanceToPolygonEdge(pixelCenter, points);

                    if (inside) {
                        if (subtract) {
                            mask[y * width + x] = 0;
                        } else {
                            // Inside - full or anti-aliased based on edge distance
                            u8 alpha = 255;
                            if (minDist < 1.0f) {
                                alpha = static_cast<u8>(minDist * 255.0f);
                            }
                            if (add) {
                                mask[y * width + x] = std::max(mask[y * width + x], alpha);
                            } else {
                                mask[y * width + x] = alpha;
                            }
                        }
                    } else if (minDist < 1.0f && !subtract) {
                        // Outside but close to edge - partial coverage for anti-aliasing
                        u8 alpha = static_cast<u8>((1.0f - minDist) * 128.0f);
                        if (add) {
                            mask[y * width + x] = std::max(mask[y * width + x], alpha);
                        } else {
                            mask[y * width + x] = std::max(mask[y * width + x], alpha);
                        }
                    }
                } else {
                    // No anti-aliasing
                    if (inside) {
                        if (subtract) {
                            mask[y * width + x] = 0;
                        } else if (add) {
                            mask[y * width + x] = std::max(mask[y * width + x], static_cast<u8>(255));
                        } else {
                            mask[y * width + x] = 255;
                        }
                    }
                }
            }
        }

        updateBounds();
    }

    // Feather the selection edge
    void feather(f32 radius) {
        if (radius <= 0) return;

        std::vector<u8> temp = mask;
        i32 r = static_cast<i32>(std::ceil(radius));

        for (i32 y = 0; y < static_cast<i32>(height); ++y) {
            for (i32 x = 0; x < static_cast<i32>(width); ++x) {
                f32 sum = 0;
                f32 weight = 0;

                for (i32 dy = -r; dy <= r; ++dy) {
                    for (i32 dx = -r; dx <= r; ++dx) {
                        i32 nx = x + dx;
                        i32 ny = y + dy;
                        if (nx < 0 || ny < 0 || nx >= static_cast<i32>(width) || ny >= static_cast<i32>(height)) continue;

                        f32 d = std::sqrt(static_cast<f32>(dx * dx + dy * dy));
                        if (d <= radius) {
                            f32 w = 1.0f - d / radius;
                            sum += temp[ny * width + nx] * w;
                            weight += w;
                        }
                    }
                }

                if (weight > 0) {
                    mask[y * width + x] = static_cast<u8>(clamp(sum / weight, 0.0f, 255.0f));
                }
            }
        }

        updateBounds();
    }

    // Expand/contract selection
    void grow(i32 pixels) {
        if (pixels == 0) return;

        std::vector<u8> temp = mask;
        bool expanding = pixels > 0;
        i32 r = std::abs(pixels);

        for (i32 y = 0; y < static_cast<i32>(height); ++y) {
            for (i32 x = 0; x < static_cast<i32>(width); ++x) {
                u8 best = expanding ? 0 : 255;

                for (i32 dy = -r; dy <= r; ++dy) {
                    for (i32 dx = -r; dx <= r; ++dx) {
                        i32 nx = x + dx;
                        i32 ny = y + dy;
                        if (nx < 0 || ny < 0 || nx >= static_cast<i32>(width) || ny >= static_cast<i32>(height)) continue;

                        f32 d = std::sqrt(static_cast<f32>(dx * dx + dy * dy));
                        if (d <= r) {
                            u8 val = temp[ny * width + nx];
                            if (expanding) {
                                best = std::max(best, val);
                            } else {
                                best = std::min(best, val);
                            }
                        }
                    }
                }

                mask[y * width + x] = best;
            }
        }

        updateBounds();
    }

    // Offset/translate the selection by given delta
    void offset(i32 dx, i32 dy) {
        if (!hasSelection || (dx == 0 && dy == 0)) return;

        std::vector<u8> newMask(width * height, 0);

        for (i32 y = 0; y < static_cast<i32>(height); ++y) {
            for (i32 x = 0; x < static_cast<i32>(width); ++x) {
                i32 srcX = x - dx;
                i32 srcY = y - dy;
                if (srcX >= 0 && srcX < static_cast<i32>(width) &&
                    srcY >= 0 && srcY < static_cast<i32>(height)) {
                    newMask[y * width + x] = mask[srcY * width + srcX];
                }
            }
        }

        mask = std::move(newMask);
        updateBounds();
    }

    void updateBounds() {
        i32 minX = width, minY = height;
        i32 maxX = 0, maxY = 0;
        hasSelection = false;

        for (i32 y = 0; y < static_cast<i32>(height); ++y) {
            for (i32 x = 0; x < static_cast<i32>(width); ++x) {
                if (mask[y * width + x] > 0) {
                    hasSelection = true;
                    minX = std::min(minX, x);
                    minY = std::min(minY, y);
                    maxX = std::max(maxX, x + 1);
                    maxY = std::max(maxY, y + 1);
                }
            }
        }

        if (hasSelection) {
            bounds = Recti(minX, minY, maxX - minX, maxY - minY);
        } else {
            bounds = Recti(0, 0, 0, 0);
        }
    }

    // Create a copy
    std::unique_ptr<Selection> clone() const {
        auto copy = std::make_unique<Selection>(width, height);
        copy->mask = mask;
        copy->bounds = bounds;
        copy->hasSelection = hasSelection;
        return copy;
    }

private:
    // Point-in-polygon test using ray casting
    static bool pointInPolygon(const Vec2& point, const std::vector<Vec2>& polygon) {
        bool inside = false;
        size_t n = polygon.size();

        for (size_t i = 0, j = n - 1; i < n; j = i++) {
            const Vec2& vi = polygon[i];
            const Vec2& vj = polygon[j];

            if (((vi.y > point.y) != (vj.y > point.y)) &&
                (point.x < (vj.x - vi.x) * (point.y - vi.y) / (vj.y - vi.y) + vi.x)) {
                inside = !inside;
            }
        }

        return inside;
    }

    // Calculate distance from point to nearest polygon edge
    static f32 distanceToPolygonEdge(const Vec2& point, const std::vector<Vec2>& polygon) {
        f32 minDist = 1e10f;
        size_t n = polygon.size();

        for (size_t i = 0; i < n; ++i) {
            const Vec2& a = polygon[i];
            const Vec2& b = polygon[(i + 1) % n];

            // Distance from point to line segment a-b
            Vec2 ab = b - a;
            Vec2 ap = point - a;
            f32 t = clamp(ap.dot(ab) / ab.lengthSquared(), 0.0f, 1.0f);
            Vec2 closest = a + ab * t;
            f32 dist = Vec2::distance(point, closest);
            minDist = std::min(minDist, dist);
        }

        return minDist;
    }
};

#endif
