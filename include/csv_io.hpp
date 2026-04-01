#pragma once

#include <ostream>
#include <string>

#include "geometry.hpp"

namespace apsc {

    // Reads a polygon from a CSV file with header: ring_id,vertex_id,x,y
    // Throws std::runtime_error on any format or validation error.
    Polygon read_polygon_csv(const std::string& path);

    // Writes the simplified polygon plus the three metric lines to `out`.
    // Format matches the assignment specification exactly.
    void write_polygon_csv(
        std::ostream& out,
        const Polygon& polygon,
        double input_area,
        double output_area,
        double areal_displacement);

}  // namespace apsc