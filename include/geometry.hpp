#pragma once

#include <cstddef>
#include <vector>

namespace apsc {

    struct Point {
        double x{};
        double y{};
    };

    struct Segment {
        Point a{};
        Point b{};
    };

    struct Ring {
        int ring_id{};
        std::vector<Point> vertices;
    };

    struct Polygon {
        std::vector<Ring> rings;

        std::size_t total_vertices() const;
    };

    // Signed area of a single ring using the shoelace formula.
    // Positive  → counter-clockwise (exterior ring convention).
    // Negative  → clockwise (hole convention).
    double signed_area(const Ring& ring);

    // Sum of signed areas across all rings (exterior positive, holes negative).
    double total_signed_area(const Polygon& polygon);

    bool is_counter_clockwise(const Ring& ring);
    bool is_clockwise(const Ring& ring);

    // Returns true when a ring has no self-intersections.
    bool ring_is_simple(const Ring& ring);

    // Full OGC-style topology check: each ring must be simple, exterior CCW,
    // holes CW, every hole lies inside the exterior, and no two rings intersect.
    bool polygon_topology_is_valid(const Polygon& polygon);

}  // namespace apsc