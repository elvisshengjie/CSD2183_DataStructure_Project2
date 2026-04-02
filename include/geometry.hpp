#ifndef GEOMETRY_HPP
#define GEOMETRY_HPP

#include <cstddef>
#include <vector>

namespace apsc {

    struct Point {
        double x = 0.0;
        double y = 0.0;
    };

    struct Segment {
        Point a;
        Point b;
    };

    struct Ring {
        int ring_id = -1;
        std::vector<Point> vertices;
    };

    struct Polygon {
        std::vector<Ring> rings;

        std::size_t total_vertices() const;
    };

    double signed_area(const Ring& ring);
    double total_signed_area(const Polygon& polygon);

    bool is_counter_clockwise(const Ring& ring);
    bool is_clockwise(const Ring& ring);

    bool ring_is_simple(const Ring& ring);
    bool polygon_topology_is_valid(const Polygon& polygon);

}  // namespace apsc

#endif