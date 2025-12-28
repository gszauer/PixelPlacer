#ifndef _H_PRIMITIVES_
#define _H_PRIMITIVES_

#include "types.h"
#include <cmath>
#include <algorithm>

struct Vec2 {
    f32 x = 0.0f;
    f32 y = 0.0f;

    Vec2() = default;
    Vec2(f32 x_, f32 y_) : x(x_), y(y_) {}

    Vec2 operator+(const Vec2& other) const { return Vec2(x + other.x, y + other.y); }
    Vec2 operator-(const Vec2& other) const { return Vec2(x - other.x, y - other.y); }
    Vec2 operator*(f32 scalar) const { return Vec2(x * scalar, y * scalar); }
    Vec2 operator/(f32 scalar) const { return Vec2(x / scalar, y / scalar); }

    Vec2& operator+=(const Vec2& other) { x += other.x; y += other.y; return *this; }
    Vec2& operator-=(const Vec2& other) { x -= other.x; y -= other.y; return *this; }
    Vec2& operator*=(f32 scalar) { x *= scalar; y *= scalar; return *this; }
    Vec2& operator/=(f32 scalar) { x /= scalar; y /= scalar; return *this; }

    bool operator==(const Vec2& other) const { return x == other.x && y == other.y; }
    bool operator!=(const Vec2& other) const { return !(*this == other); }

    f32 length() const { return std::sqrt(x * x + y * y); }
    f32 lengthSquared() const { return x * x + y * y; }
    Vec2 normalized() const {
        f32 len = length();
        return len > 0.0f ? *this / len : Vec2(0.0f, 0.0f);
    }

    f32 dot(const Vec2& other) const { return x * other.x + y * other.y; }
    f32 cross(const Vec2& other) const { return x * other.y - y * other.x; }

    Vec2 perpendicular() const { return Vec2(-y, x); }
    Vec2 floor() const { return Vec2(std::floor(x), std::floor(y)); }
    Vec2 ceil() const { return Vec2(std::ceil(x), std::ceil(y)); }
    Vec2 round() const { return Vec2(std::round(x), std::round(y)); }

    static Vec2 lerp(const Vec2& a, const Vec2& b, f32 t) {
        return Vec2(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t);
    }

    static f32 distance(const Vec2& a, const Vec2& b) {
        return (b - a).length();
    }
};

struct Vec2i {
    i32 x = 0;
    i32 y = 0;

    Vec2i() = default;
    Vec2i(i32 x_, i32 y_) : x(x_), y(y_) {}
    explicit Vec2i(const Vec2& v) : x(static_cast<i32>(v.x)), y(static_cast<i32>(v.y)) {}

    Vec2i operator+(const Vec2i& other) const { return Vec2i(x + other.x, y + other.y); }
    Vec2i operator-(const Vec2i& other) const { return Vec2i(x - other.x, y - other.y); }

    bool operator==(const Vec2i& other) const { return x == other.x && y == other.y; }
    bool operator!=(const Vec2i& other) const { return !(*this == other); }

    Vec2 toVec2() const { return Vec2(static_cast<f32>(x), static_cast<f32>(y)); }
};

struct Rect {
    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 w = 0.0f;
    f32 h = 0.0f;

    Rect() = default;
    Rect(f32 x_, f32 y_, f32 w_, f32 h_) : x(x_), y(y_), w(w_), h(h_) {}
    Rect(const Vec2& pos, const Vec2& size) : x(pos.x), y(pos.y), w(size.x), h(size.y) {}

    f32 left() const { return x; }
    f32 right() const { return x + w; }
    f32 top() const { return y; }
    f32 bottom() const { return y + h; }

    Vec2 position() const { return Vec2(x, y); }
    Vec2 size() const { return Vec2(w, h); }
    Vec2 center() const { return Vec2(x + w * 0.5f, y + h * 0.5f); }

    bool contains(const Vec2& point) const {
        return point.x >= x && point.x < x + w && point.y >= y && point.y < y + h;
    }

    bool contains(f32 px, f32 py) const {
        return px >= x && px < x + w && py >= y && py < y + h;
    }

    // Check if point is within widget size only (for already-localized coordinates)
    bool containsLocal(const Vec2& point) const {
        return point.x >= 0 && point.x < w && point.y >= 0 && point.y < h;
    }

    bool intersects(const Rect& other) const {
        return !(other.x >= x + w || other.x + other.w <= x ||
                 other.y >= y + h || other.y + other.h <= y);
    }

    Rect intersection(const Rect& other) const {
        f32 nx = std::max(x, other.x);
        f32 ny = std::max(y, other.y);
        f32 nw = std::min(x + w, other.x + other.w) - nx;
        f32 nh = std::min(y + h, other.y + other.h) - ny;
        if (nw <= 0 || nh <= 0) return Rect();
        return Rect(nx, ny, nw, nh);
    }

    Rect united(const Rect& other) const {
        if (w <= 0 || h <= 0) return other;
        if (other.w <= 0 || other.h <= 0) return *this;
        f32 nx = std::min(x, other.x);
        f32 ny = std::min(y, other.y);
        f32 nw = std::max(x + w, other.x + other.w) - nx;
        f32 nh = std::max(y + h, other.y + other.h) - ny;
        return Rect(nx, ny, nw, nh);
    }

    Rect expanded(f32 amount) const {
        return Rect(x - amount, y - amount, w + amount * 2, h + amount * 2);
    }

    Rect translated(f32 dx, f32 dy) const {
        return Rect(x + dx, y + dy, w, h);
    }

    Rect translated(const Vec2& delta) const {
        return Rect(x + delta.x, y + delta.y, w, h);
    }

    bool isEmpty() const { return w <= 0 || h <= 0; }

    bool operator==(const Rect& other) const {
        return x == other.x && y == other.y && w == other.w && h == other.h;
    }

    bool operator!=(const Rect& other) const { return !(*this == other); }
};

struct Recti {
    i32 x = 0;
    i32 y = 0;
    i32 w = 0;
    i32 h = 0;

    Recti() = default;
    Recti(i32 x_, i32 y_, i32 w_, i32 h_) : x(x_), y(y_), w(w_), h(h_) {}
    explicit Recti(const Rect& r)
        : x(static_cast<i32>(r.x)), y(static_cast<i32>(r.y)),
          w(static_cast<i32>(r.w)), h(static_cast<i32>(r.h)) {}

    Rect toRect() const {
        return Rect(static_cast<f32>(x), static_cast<f32>(y),
                    static_cast<f32>(w), static_cast<f32>(h));
    }

    bool contains(i32 px, i32 py) const {
        return px >= x && px < x + w && py >= y && py < y + h;
    }
};

struct Color {
    u8 r = 0;
    u8 g = 0;
    u8 b = 0;
    u8 a = 255;

    Color() = default;
    Color(u8 r_, u8 g_, u8 b_, u8 a_ = 255) : r(r_), g(g_), b(b_), a(a_) {}

    explicit Color(u32 rgba) {
        r = (rgba >> 24) & 0xFF;
        g = (rgba >> 16) & 0xFF;
        b = (rgba >> 8) & 0xFF;
        a = rgba & 0xFF;
    }

    u32 toRGBA() const {
        return (static_cast<u32>(r) << 24) | (static_cast<u32>(g) << 16) |
               (static_cast<u32>(b) << 8) | static_cast<u32>(a);
    }

    u32 toABGR() const {
        return (static_cast<u32>(a) << 24) | (static_cast<u32>(b) << 16) |
               (static_cast<u32>(g) << 8) | static_cast<u32>(r);
    }

    static Color fromRGBA(u32 rgba) { return Color(rgba); }

    static Color fromABGR(u32 abgr) {
        return Color(
            abgr & 0xFF,
            (abgr >> 8) & 0xFF,
            (abgr >> 16) & 0xFF,
            (abgr >> 24) & 0xFF
        );
    }

    static Color lerp(const Color& a, const Color& b, f32 t) {
        return Color(
            static_cast<u8>(a.r + (b.r - a.r) * t),
            static_cast<u8>(a.g + (b.g - a.g) * t),
            static_cast<u8>(a.b + (b.b - a.b) * t),
            static_cast<u8>(a.a + (b.a - a.a) * t)
        );
    }

    Color withAlpha(u8 newAlpha) const { return Color(r, g, b, newAlpha); }

    bool operator==(const Color& other) const {
        return r == other.r && g == other.g && b == other.b && a == other.a;
    }

    bool operator!=(const Color& other) const { return !(*this == other); }

    // Common colors
    static Color black() { return Color(0, 0, 0, 255); }
    static Color white() { return Color(255, 255, 255, 255); }
    static Color red() { return Color(255, 0, 0, 255); }
    static Color green() { return Color(0, 255, 0, 255); }
    static Color blue() { return Color(0, 0, 255, 255); }
    static Color transparent() { return Color(0, 0, 0, 0); }
};

struct Matrix3x2 {
    f32 m[6] = {1, 0, 0, 1, 0, 0}; // a, b, c, d, tx, ty

    Matrix3x2() = default;
    Matrix3x2(f32 a, f32 b, f32 c, f32 d, f32 tx, f32 ty) {
        m[0] = a; m[1] = b; m[2] = c; m[3] = d; m[4] = tx; m[5] = ty;
    }

    static Matrix3x2 identity() { return Matrix3x2(); }

    static Matrix3x2 translation(f32 tx, f32 ty) {
        return Matrix3x2(1, 0, 0, 1, tx, ty);
    }

    static Matrix3x2 translation(const Vec2& t) {
        return translation(t.x, t.y);
    }

    static Matrix3x2 scaling(f32 sx, f32 sy) {
        return Matrix3x2(sx, 0, 0, sy, 0, 0);
    }

    static Matrix3x2 scaling(f32 s) {
        return scaling(s, s);
    }

    static Matrix3x2 rotation(f32 radians) {
        f32 c = std::cos(radians);
        f32 s = std::sin(radians);
        return Matrix3x2(c, s, -s, c, 0, 0);
    }

    Matrix3x2 operator*(const Matrix3x2& other) const {
        return Matrix3x2(
            m[0] * other.m[0] + m[1] * other.m[2],
            m[0] * other.m[1] + m[1] * other.m[3],
            m[2] * other.m[0] + m[3] * other.m[2],
            m[2] * other.m[1] + m[3] * other.m[3],
            m[4] * other.m[0] + m[5] * other.m[2] + other.m[4],
            m[4] * other.m[1] + m[5] * other.m[3] + other.m[5]
        );
    }

    Vec2 transform(const Vec2& p) const {
        return Vec2(
            p.x * m[0] + p.y * m[2] + m[4],
            p.x * m[1] + p.y * m[3] + m[5]
        );
    }

    Vec2 transformVector(const Vec2& v) const {
        return Vec2(v.x * m[0] + v.y * m[2], v.x * m[1] + v.y * m[3]);
    }

    f32 determinant() const {
        return m[0] * m[3] - m[1] * m[2];
    }

    Matrix3x2 inverted() const {
        f32 det = determinant();
        if (std::abs(det) < 1e-6f) return identity();
        f32 invDet = 1.0f / det;
        return Matrix3x2(
            m[3] * invDet,
            -m[1] * invDet,
            -m[2] * invDet,
            m[0] * invDet,
            (m[2] * m[5] - m[3] * m[4]) * invDet,
            (m[1] * m[4] - m[0] * m[5]) * invDet
        );
    }
};

struct Transform {
    Vec2 position = Vec2(0.0f, 0.0f);
    Vec2 scale = Vec2(1.0f, 1.0f);
    f32 rotation = 0.0f; // radians
    Vec2 pivot = Vec2(0.5f, 0.5f);  // Normalized (0-1) relative to layer bounds

    Transform() = default;

    // Legacy toMatrix (without pivot - for backward compatibility)
    Matrix3x2 toMatrix() const {
        // Note: Our matrix * operator applies left operand first (reversed from standard math)
        return Matrix3x2::scaling(scale.x, scale.y) *
               Matrix3x2::rotation(rotation) *
               Matrix3x2::translation(position);
    }

    // Full transform with pivot point
    Matrix3x2 toMatrix(f32 layerWidth, f32 layerHeight) const {
        // Convert normalized pivot to actual coordinates
        f32 px = pivot.x * layerWidth;
        f32 py = pivot.y * layerHeight;

        // Note: Our matrix * operator applies left operand first (reversed from standard math)
        // Intended order: T(-pivot) -> S -> R -> T(pivot) -> T(position)
        return Matrix3x2::translation(-px, -py) *
               Matrix3x2::scaling(scale.x, scale.y) *
               Matrix3x2::rotation(rotation) *
               Matrix3x2::translation(px, py) *
               Matrix3x2::translation(position);
    }

    bool isIdentity() const {
        return position.x == 0.0f && position.y == 0.0f &&
               scale.x == 1.0f && scale.y == 1.0f &&
               rotation == 0.0f;
    }

    static Transform identity() { return Transform(); }
};

// Utility functions
inline f32 clamp(f32 value, f32 min, f32 max) {
    return std::max(min, std::min(max, value));
}

inline i32 clamp(i32 value, i32 min, i32 max) {
    return std::max(min, std::min(max, value));
}

inline f32 lerp(f32 a, f32 b, f32 t) {
    return a + (b - a) * t;
}

inline f32 smoothstep(f32 edge0, f32 edge1, f32 x) {
    f32 t = clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

constexpr f32 PI = 3.14159265358979323846f;
constexpr f32 TAU = PI * 2.0f;
constexpr f32 DEG_TO_RAD = PI / 180.0f;
constexpr f32 RAD_TO_DEG = 180.0f / PI;

#endif
