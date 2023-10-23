#pragma once

#include <compare>

namespace geom {

using Dimension = int;
using Coord = Dimension;

struct PointInt {
    Coord x{}, y{};
    auto operator<=>(const PointInt&) const = default;
};

struct PointDouble {
    double x{}, y{};

    PointDouble operator+(const PointDouble& other) const {
        return PointDouble{.x = this->x + other.x, .y = this->y + other.y};
    }

    PointDouble& operator+=(const PointDouble& rhs) {
        x += rhs.x;
        y += rhs.y;
        return *this;
    }

    PointDouble& operator*=(double scale) {
        x *= scale;
        y *= scale;
        return *this;
    }

    auto operator<=>(const PointDouble&) const = default;
};

inline PointDouble operator*(PointDouble lhs, double rhs) {
    return lhs *= rhs;
}

inline PointDouble operator*(double lhs, PointDouble rhs) {
    return rhs *= lhs;
}

}  // namespace geom
