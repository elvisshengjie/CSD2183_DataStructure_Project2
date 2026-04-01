#include <cstdio>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

#include "csv_io.hpp"
#include "geometry.hpp"
#include "simplifier.hpp"

namespace {

std::string format_coordinate(double value) {
    if (std::abs(value) < 5e-13) {
        value = 0.0;
    }

    std::ostringstream formatter;
    formatter << std::setprecision(10) << std::defaultfloat << value;
    return formatter.str();
}

void write_polygon_only_csv(const apsc::Polygon& polygon, const std::string& path) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("Failed to write temporary polygon file.");
    }

    out << "ring_id,vertex_id,x,y\n";
    for (const apsc::Ring& ring : polygon.rings) {
        for (std::size_t vertex_id = 0; vertex_id < ring.vertices.size(); ++vertex_id) {
            const apsc::Point& point = ring.vertices[vertex_id];
            out << ring.ring_id << ',' << vertex_id << ','
                << format_coordinate(point.x) << ','
                << format_coordinate(point.y) << '\n';
        }
    }
}

std::string make_temp_path(const char* pattern) {
    std::string path = pattern;
    std::vector<char> writable(path.begin(), path.end());
    writable.push_back('\0');

    const int fd = mkstemp(writable.data());
    if (fd == -1) {
        throw std::runtime_error("Failed to create temporary file.");
    }
    close(fd);
    return std::string(writable.data());
}

std::optional<double> compute_true_areal_displacement(
    const apsc::Polygon& input,
    const apsc::Polygon& output) {
    const std::string input_path = make_temp_path(".apsc_input_XXXXXX");
    const std::string output_path = make_temp_path(".apsc_output_XXXXXX");

    try {
        write_polygon_only_csv(input, input_path);
        write_polygon_only_csv(output, output_path);

        const std::string command =
            "python.exe tools/compute_symdiff_area.py " + input_path + " " + output_path;

        FILE* pipe = popen(command.c_str(), "r");
        if (!pipe) {
            std::remove(input_path.c_str());
            std::remove(output_path.c_str());
            return std::nullopt;
        }

        std::string output_text;
        char buffer[256];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            output_text += buffer;
        }

        const int exit_code = pclose(pipe);
        std::remove(input_path.c_str());
        std::remove(output_path.c_str());

        if (exit_code != 0) {
            return std::nullopt;
        }

        return std::stod(output_text);
    } catch (...) {
        std::remove(input_path.c_str());
        std::remove(output_path.c_str());
        return std::nullopt;
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        // The assignment requires exactly two arguments after the program name.
        if (argc != 3) {
            std::cerr << "Usage: ./simplify <input_file.csv> <target_vertices>\n";
            return EXIT_FAILURE;
        }

        const std::string input_path = argv[1];
        const long long parsed_target = std::stoll(argv[2]);
        if (parsed_target < 0) {
            throw std::runtime_error("target_vertices must be non-negative.");
        }

        // Parse and validate the input before running any simplification logic.
        const apsc::Polygon polygon = apsc::read_polygon_csv(input_path);
        if (!apsc::polygon_topology_is_valid(polygon)) {
            throw std::runtime_error("Input polygon failed basic topology validation.");
        }

        // Run the simplifier and print exactly the stdout format from the brief.
        const apsc::Simplifier simplifier;
        const apsc::SimplificationResult result =
            simplifier.simplify(polygon, static_cast<std::size_t>(parsed_target));
        const double areal_displacement =
            compute_true_areal_displacement(polygon, result.polygon)
                .value_or(result.areal_displacement);

        apsc::write_polygon_csv(
            std::cout,
            result.polygon,
            result.input_area,
            result.output_area,
            areal_displacement);
    } catch (const std::exception& exception) {
        std::cerr << "Error: " << exception.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
