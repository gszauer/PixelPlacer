#include "selection.h"
#include <cmath>
#include <algorithm>

Selection::Selection(u32 w, u32 h) : width(w), height(h), mask(w * h, 0) {
    bounds = {0, 0, 0, 0};
}

void Selection::resize(u32 w, u32 h) {
    width = w;
    height = h;
    mask.resize(w * h, 0);
    clear();
}

void Selection::clear() {
    std::fill(mask.begin(), mask.end(), 0);
    hasSelection = false;
    bounds = {0, 0, 0, 0};
}

void Selection::selectAll() {
    std::fill(mask.begin(), mask.end(), 255);
    hasSelection = true;
    bounds = {0, 0, (i32)width, (i32)height};
}

void Selection::invert() {
    for (auto& v : mask) {
        v = 255 - v;
    }
    updateBounds();
}

u8 Selection::getValue(u32 x, u32 y) const {
    if (x >= width || y >= height) return 0;
    return mask[y * width + x];
}

void Selection::setValue(u32 x, u32 y, u8 value) {
    if (x >= width || y >= height) return;
    mask[y * width + x] = value;
}

bool Selection::isSelected(u32 x, u32 y) const {
    return getValue(x, y) > 0;
}

bool Selection::isFullySelected(u32 x, u32 y) const {
    return getValue(x, y) == 255;
}

void Selection::setRectangle(const Recti& rect, bool add, bool subtract, bool antiAlias) {
    if (!add && !subtract) {
        clear();
    }

    i32 x1 = std::max(0, rect.x);
    i32 y1 = std::max(0, rect.y);
    i32 x2 = std::min((i32)width, rect.x + rect.w);
    i32 y2 = std::min((i32)height, rect.y + rect.h);

    for (i32 y = y1; y < y2; y++) {
        for (i32 x = x1; x < x2; x++) {
            u8 value = 255;

            // Anti-alias edges
            if (antiAlias) {
                f32 edgeDist = std::min({
                    (f32)(x - x1 + 0.5f),
                    (f32)(x2 - x - 0.5f),
                    (f32)(y - y1 + 0.5f),
                    (f32)(y2 - y - 0.5f)
                });
                if (edgeDist < 1.0f) {
                    value = (u8)(edgeDist * 255.0f);
                }
            }

            if (subtract) {
                u8 current = getValue(x, y);
                setValue(x, y, (u8)std::max(0, (i32)current - (i32)value));
            } else if (add) {
                u8 current = getValue(x, y);
                setValue(x, y, (u8)std::min(255, (i32)current + (i32)value));
            } else {
                setValue(x, y, value);
            }
        }
    }

    hasSelection = true;
    updateBounds();
}

void Selection::setEllipse(const Recti& rect, bool add, bool subtract, bool antiAlias) {
    if (!add && !subtract) {
        clear();
    }

    f32 cx = rect.x + rect.w * 0.5f;
    f32 cy = rect.y + rect.h * 0.5f;
    f32 rx = rect.w * 0.5f;
    f32 ry = rect.h * 0.5f;

    if (rx <= 0 || ry <= 0) return;

    i32 x1 = std::max(0, rect.x);
    i32 y1 = std::max(0, rect.y);
    i32 x2 = std::min((i32)width, rect.x + rect.w);
    i32 y2 = std::min((i32)height, rect.y + rect.h);

    for (i32 y = y1; y < y2; y++) {
        for (i32 x = x1; x < x2; x++) {
            // Normalized distance from center (1.0 at edge)
            f32 dx = (x + 0.5f - cx) / rx;
            f32 dy = (y + 0.5f - cy) / ry;
            f32 dist = std::sqrt(dx * dx + dy * dy);

            u8 value = 0;
            if (dist <= 1.0f) {
                value = 255;
                if (antiAlias && dist > 0.0f) {
                    // Smooth edge over ~1 pixel
                    f32 edgeDist = (1.0f - dist) * std::min(rx, ry);
                    if (edgeDist < 1.0f) {
                        value = (u8)(edgeDist * 255.0f);
                    }
                }
            }

            if (value > 0) {
                if (subtract) {
                    u8 current = getValue(x, y);
                    setValue(x, y, (u8)std::max(0, (i32)current - (i32)value));
                } else if (add) {
                    u8 current = getValue(x, y);
                    setValue(x, y, (u8)std::min(255, (i32)current + (i32)value));
                } else {
                    setValue(x, y, value);
                }
            }
        }
    }

    hasSelection = true;
    updateBounds();
}

void Selection::setPolygon(const std::vector<Vec2>& points, bool add, bool subtract, bool antiAlias) {
    if (points.size() < 3) return;

    if (!add && !subtract) {
        clear();
    }

    // Find bounding box
    f32 minX = points[0].x, maxX = points[0].x;
    f32 minY = points[0].y, maxY = points[0].y;
    for (const auto& p : points) {
        minX = std::min(minX, p.x);
        maxX = std::max(maxX, p.x);
        minY = std::min(minY, p.y);
        maxY = std::max(maxY, p.y);
    }

    i32 x1 = std::max(0, (i32)minX);
    i32 y1 = std::max(0, (i32)minY);
    i32 x2 = std::min((i32)width, (i32)maxX + 1);
    i32 y2 = std::min((i32)height, (i32)maxY + 1);

    for (i32 y = y1; y < y2; y++) {
        for (i32 x = x1; x < x2; x++) {
            Vec2 point{x + 0.5f, y + 0.5f};

            u8 value = 0;
            if (pointInPolygon(point, points)) {
                value = 255;
                if (antiAlias) {
                    f32 edgeDist = distanceToPolygonEdge(point, points);
                    if (edgeDist < 1.0f) {
                        value = (u8)(edgeDist * 255.0f);
                    }
                }
            } else if (antiAlias) {
                // Check if we're very close to edge (outside but within 1 pixel)
                f32 edgeDist = distanceToPolygonEdge(point, points);
                if (edgeDist < 1.0f) {
                    // Partial coverage for anti-aliasing
                    value = (u8)((1.0f - edgeDist) * 128.0f);
                }
            }

            if (value > 0) {
                if (subtract) {
                    u8 current = getValue(x, y);
                    setValue(x, y, (u8)std::max(0, (i32)current - (i32)value));
                } else if (add) {
                    u8 current = getValue(x, y);
                    setValue(x, y, (u8)std::min(255, (i32)current + (i32)value));
                } else {
                    setValue(x, y, value);
                }
            }
        }
    }

    hasSelection = true;
    updateBounds();
}

void Selection::feather(f32 radius) {
    if (radius <= 0 || !hasSelection) return;

    // Create a copy of the mask for reading
    std::vector<u8> original = mask;

    i32 kernelSize = (i32)std::ceil(radius * 2) + 1;
    if (kernelSize % 2 == 0) kernelSize++;
    i32 halfKernel = kernelSize / 2;

    // Gaussian kernel
    std::vector<f32> kernel(kernelSize);
    f32 sigma = radius / 2.0f;
    f32 sum = 0;
    for (i32 i = 0; i < kernelSize; i++) {
        f32 x = (f32)(i - halfKernel);
        kernel[i] = std::exp(-(x * x) / (2 * sigma * sigma));
        sum += kernel[i];
    }
    for (auto& k : kernel) k /= sum;

    // Horizontal pass
    std::vector<u8> temp(width * height);
    for (u32 y = 0; y < height; y++) {
        for (u32 x = 0; x < width; x++) {
            f32 val = 0;
            for (i32 k = -halfKernel; k <= halfKernel; k++) {
                i32 sx = (i32)x + k;
                if (sx < 0) sx = 0;
                if (sx >= (i32)width) sx = (i32)width - 1;
                val += original[y * width + sx] * kernel[k + halfKernel];
            }
            temp[y * width + x] = (u8)std::clamp(val, 0.0f, 255.0f);
        }
    }

    // Vertical pass
    for (u32 y = 0; y < height; y++) {
        for (u32 x = 0; x < width; x++) {
            f32 val = 0;
            for (i32 k = -halfKernel; k <= halfKernel; k++) {
                i32 sy = (i32)y + k;
                if (sy < 0) sy = 0;
                if (sy >= (i32)height) sy = (i32)height - 1;
                val += temp[sy * width + x] * kernel[k + halfKernel];
            }
            mask[y * width + x] = (u8)std::clamp(val, 0.0f, 255.0f);
        }
    }

    updateBounds();
}

void Selection::grow(i32 pixels) {
    if (pixels == 0 || !hasSelection) return;

    std::vector<u8> original = mask;

    if (pixels > 0) {
        // Expand: dilate
        for (u32 y = 0; y < height; y++) {
            for (u32 x = 0; x < width; x++) {
                u8 maxVal = 0;
                for (i32 dy = -pixels; dy <= pixels; dy++) {
                    for (i32 dx = -pixels; dx <= pixels; dx++) {
                        // Circular kernel
                        if (dx * dx + dy * dy <= pixels * pixels) {
                            i32 sx = (i32)x + dx;
                            i32 sy = (i32)y + dy;
                            if (sx >= 0 && sx < (i32)width && sy >= 0 && sy < (i32)height) {
                                maxVal = std::max(maxVal, original[sy * width + sx]);
                            }
                        }
                    }
                }
                mask[y * width + x] = maxVal;
            }
        }
    } else {
        // Contract: erode
        pixels = -pixels;
        for (u32 y = 0; y < height; y++) {
            for (u32 x = 0; x < width; x++) {
                u8 minVal = 255;
                for (i32 dy = -pixels; dy <= pixels; dy++) {
                    for (i32 dx = -pixels; dx <= pixels; dx++) {
                        if (dx * dx + dy * dy <= pixels * pixels) {
                            i32 sx = (i32)x + dx;
                            i32 sy = (i32)y + dy;
                            if (sx >= 0 && sx < (i32)width && sy >= 0 && sy < (i32)height) {
                                minVal = std::min(minVal, original[sy * width + sx]);
                            } else {
                                minVal = 0; // Outside bounds treated as unselected
                            }
                        }
                    }
                }
                mask[y * width + x] = minVal;
            }
        }
    }

    updateBounds();
}

void Selection::offset(i32 dx, i32 dy) {
    if ((dx == 0 && dy == 0) || !hasSelection) return;

    std::vector<u8> original = mask;
    std::fill(mask.begin(), mask.end(), 0);

    for (u32 y = 0; y < height; y++) {
        for (u32 x = 0; x < width; x++) {
            i32 srcX = (i32)x - dx;
            i32 srcY = (i32)y - dy;
            if (srcX >= 0 && srcX < (i32)width && srcY >= 0 && srcY < (i32)height) {
                mask[y * width + x] = original[srcY * width + srcX];
            }
        }
    }

    updateBounds();
}

void Selection::updateBounds() {
    bounds = {(i32)width, (i32)height, 0, 0};
    hasSelection = false;

    for (u32 y = 0; y < height; y++) {
        for (u32 x = 0; x < width; x++) {
            if (mask[y * width + x] > 0) {
                hasSelection = true;
                bounds.x = std::min(bounds.x, (i32)x);
                bounds.y = std::min(bounds.y, (i32)y);
                bounds.w = std::max(bounds.w, (i32)x + 1);
                bounds.h = std::max(bounds.h, (i32)y + 1);
            }
        }
    }

    if (hasSelection) {
        // Convert from max coords to width/height
        bounds.w -= bounds.x;
        bounds.h -= bounds.y;
    } else {
        bounds = {0, 0, 0, 0};
    }
}

std::unique_ptr<Selection> Selection::clone() const {
    auto copy = std::make_unique<Selection>();
    copy->mask = mask;
    copy->width = width;
    copy->height = height;
    copy->bounds = bounds;
    copy->hasSelection = hasSelection;
    return copy;
}

bool Selection::pointInPolygon(const Vec2& point, const std::vector<Vec2>& polygon) {
    // Ray casting algorithm
    bool inside = false;
    size_t n = polygon.size();

    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        const Vec2& pi = polygon[i];
        const Vec2& pj = polygon[j];

        if (((pi.y > point.y) != (pj.y > point.y)) &&
            (point.x < (pj.x - pi.x) * (point.y - pi.y) / (pj.y - pi.y) + pi.x)) {
            inside = !inside;
        }
    }

    return inside;
}

f32 Selection::distanceToPolygonEdge(const Vec2& point, const std::vector<Vec2>& polygon) {
    f32 minDist = std::numeric_limits<f32>::max();
    size_t n = polygon.size();

    for (size_t i = 0; i < n; i++) {
        const Vec2& p1 = polygon[i];
        const Vec2& p2 = polygon[(i + 1) % n];

        // Distance to line segment
        f32 dx = p2.x - p1.x;
        f32 dy = p2.y - p1.y;
        f32 len2 = dx * dx + dy * dy;

        f32 t = 0;
        if (len2 > 0) {
            t = std::clamp(((point.x - p1.x) * dx + (point.y - p1.y) * dy) / len2, 0.0f, 1.0f);
        }

        f32 nearX = p1.x + t * dx;
        f32 nearY = p1.y + t * dy;
        f32 dist = std::sqrt((point.x - nearX) * (point.x - nearX) +
                             (point.y - nearY) * (point.y - nearY));

        minDist = std::min(minDist, dist);
    }

    return minDist;
}
