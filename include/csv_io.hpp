#pragma once

#include <iosfwd>
#include <string>

#include "geometry.hpp"

namespace apsc {

// Reads the assignment CSV format into the in-memory polygon representation.
Polygon read_polygon_csv(const std::string& path);

// Writes the exact output format required by the assignment specification.
void write_polygon_csv(
    std::ostream& out,
    const Polygon& polygon,
    double input_area,
    double output_area,
    double areal_displacement);

}  // namespace apsc
