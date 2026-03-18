#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>

#include "csv_io.hpp"
#include "geometry.hpp"
#include "simplifier.hpp"

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

        apsc::write_polygon_csv(
            std::cout,
            result.polygon,
            result.input_area,
            result.output_area,
            result.areal_displacement);
    } catch (const std::exception& exception) {
        std::cerr << "Error: " << exception.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
