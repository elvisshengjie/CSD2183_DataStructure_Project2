#include "csv_io.hpp"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace apsc {
namespace {

// Removes surrounding whitespace so CSV parsing is tolerant of minor formatting noise.
std::string trim(const std::string& value) {
    const auto begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return "";
    }

    const auto end = value.find_last_not_of(" \t\r\n");
    return value.substr(begin, end - begin + 1);
}

std::string format_coordinate(double value) {
    if (std::abs(value) < 5e-13) {
        value = 0.0;
    }

    std::ostringstream formatter;
    formatter << std::setprecision(10) << std::defaultfloat << value;
    return formatter.str();
}

// The assignment guarantees the orientation convention, but normalizing here makes the
// rest of the code simpler and more robust to slightly inconsistent input files.
void normalize_ring_orientation(Polygon& polygon) {
    for (std::size_t i = 0; i < polygon.rings.size(); ++i) {
        Ring& ring = polygon.rings[i];
        const bool should_be_ccw = i == 0;
        const bool orientation_ok =
            should_be_ccw ? is_counter_clockwise(ring) : is_clockwise(ring);

        if (!orientation_ok) {
            std::reverse(ring.vertices.begin(), ring.vertices.end());
        }
    }
}

}  // namespace

Polygon read_polygon_csv(const std::string& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("Failed to open input file: " + path);
    }

    std::string line;
    if (!std::getline(input, line)) {
        throw std::runtime_error("Input file is empty: " + path);
    }

    if (trim(line) != "ring_id,vertex_id,x,y") {
        throw std::runtime_error("Unexpected CSV header. Expected: ring_id,vertex_id,x,y");
    }

    Polygon polygon;
    std::vector<int> expected_vertex_ids;

    while (std::getline(input, line)) {
        if (trim(line).empty()) {
            continue;
        }

        std::stringstream row(line);
        std::string ring_id_text;
        std::string vertex_id_text;
        std::string x_text;
        std::string y_text;

        if (!std::getline(row, ring_id_text, ',') ||
            !std::getline(row, vertex_id_text, ',') ||
            !std::getline(row, x_text, ',') ||
            !std::getline(row, y_text, ',')) {
            throw std::runtime_error("Malformed CSV row: " + line);
        }

        const int ring_id = std::stoi(trim(ring_id_text));
        const int vertex_id = std::stoi(trim(vertex_id_text));
        const double x = std::stod(trim(x_text));
        const double y = std::stod(trim(y_text));

        if (ring_id < 0) {
            throw std::runtime_error("ring_id must be non-negative.");
        }

        // Rings are created on demand, but only if the file keeps the required contiguous
        // ring numbering 0, 1, 2, ...
        if (static_cast<std::size_t>(ring_id) >= polygon.rings.size()) {
            if (ring_id != static_cast<int>(polygon.rings.size())) {
                throw std::runtime_error("ring_id values must be contiguous and start at 0.");
            }
            polygon.rings.push_back(Ring {ring_id, {}});
            expected_vertex_ids.push_back(0);
        }

        // Each ring must also keep contiguous vertex numbering as required by the brief.
        if (vertex_id != expected_vertex_ids[ring_id]) {
            throw std::runtime_error("vertex_id values must be contiguous within each ring.");
        }

        polygon.rings[ring_id].vertices.push_back(Point {x, y});
        ++expected_vertex_ids[ring_id];
    }

    if (polygon.rings.empty()) {
        throw std::runtime_error("The input polygon contains no rings.");
    }

    for (const Ring& ring : polygon.rings) {
        if (ring.vertices.size() < 3) {
            throw std::runtime_error("Each ring must contain at least 3 vertices.");
        }
    }

    // Store rings in a normalized form so downstream geometry code can assume
    // exterior = counter-clockwise and holes = clockwise.
    normalize_ring_orientation(polygon);
    return polygon;
}

void write_polygon_csv(
    std::ostream& out,
    const Polygon& polygon,
    double input_area,
    double output_area,
    double areal_displacement) {
    out << "ring_id,vertex_id,x,y\n";
    // The assignment expects rows grouped by ring and vertex ids restarted from 0.
    for (const Ring& ring : polygon.rings) {
        for (std::size_t vertex_id = 0; vertex_id < ring.vertices.size(); ++vertex_id) {
            const Point& point = ring.vertices[vertex_id];
            out << ring.ring_id << ',' << vertex_id << ','
                << format_coordinate(point.x) << ','
                << format_coordinate(point.y) << '\n';
        }
    }

    // The final three report lines must be printed in scientific notation.
    out << std::scientific << std::setprecision(6);
    out << "Total signed area in input: " << input_area << '\n';
    out << "Total signed area in output: " << output_area << '\n';
    out << "Total areal displacement: " << areal_displacement << '\n';
}

}  // namespace apsc
