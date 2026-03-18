#pragma once

#include <cstddef>
#include <vector>

namespace apsc {

// Basic 2D point used throughout the project.
struct Point {
    double x {};
    double y {};
};

// A ring is one closed boundary: ring 0 is the exterior, later rings are holes.
struct Ring {
    int ring_id {};
    std::vector<Point> vertices;
};

// The assignment polygon is represented as one exterior ring plus zero or more holes.
struct Polygon {
    std::vector<Ring> rings;

    // Returns the total number of vertices across all rings.
    [[nodiscard]] std::size_t total_vertices() const;
};

// Convenience structure used by the intersection helpers.
struct Segment {
    Point a;
    Point b;
};

// Shoelace-formula signed area. Exterior rings should be positive, holes negative.
[[nodiscard]] double signed_area(const Ring& ring);

// Sum of the signed areas of all rings in the polygon.
[[nodiscard]] double total_signed_area(const Polygon& polygon);

// Orientation helpers used to normalize and validate input/output rings.
[[nodiscard]] bool is_counter_clockwise(const Ring& ring);
[[nodiscard]] bool is_clockwise(const Ring& ring);

// True when a ring has no self-intersections.
[[nodiscard]] bool ring_is_simple(const Ring& ring);

// Checks the project-level topology rules: valid ring orientation, no self-crossing,
// holes inside the exterior ring, and no ring-ring intersections.
[[nodiscard]] bool polygon_topology_is_valid(const Polygon& polygon);

}  // namespace apsc
