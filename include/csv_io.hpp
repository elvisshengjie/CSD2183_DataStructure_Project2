#pragma once

#include <ostream>
#include <string>

#include "geometry.hpp"

namespace apsc {

    // Parse a polygon from a CSV file with header: ring_id,vertex_id,x,y
    // Throws std::runtime_error on any parse or validation error.
    Polygon read_polygon_csv(const std::string& path);

    // Write the simplified polygon to OUT in the assignment's output format,
    // followed by the three area/displacement summary lines.
    void write_polygon_csv(
        std::ostream& out,
        const Polygon& polygon,
        double          input_area,
        double          output_area,
        double          areal_displacement);

}  // namespace apsc