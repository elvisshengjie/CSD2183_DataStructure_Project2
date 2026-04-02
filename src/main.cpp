#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>

#include "csv_io.hpp"
#include "geometry.hpp"
#include "simplifier.hpp"

int main(int argc, char* argv[]) {
    try {
        if (argc != 3) {
            std::cerr << "Usage: ./simplify <input_file.csv> <target_vertices>\n";
            return EXIT_FAILURE;
        }

        const std::string input_path = argv[1];
        const long long parsed_target = std::stoll(argv[2]);
        if (parsed_target < 0) {
            throw std::runtime_error("target_vertices must be non-negative.");
        }
        const std::size_t target_vertices = static_cast<std::size_t>(parsed_target);

        const apsc::Polygon polygon = apsc::read_polygon_csv(input_path);

        const apsc::Simplifier simplifier;
        const apsc::SimplificationResult result =
            simplifier.simplify(polygon, target_vertices);

        apsc::write_polygon_csv(
            std::cout,
            result.polygon,
            result.input_area,
            result.output_area,
            result.areal_displacement);
    }
    catch (const std::exception& exception) {
        std::cerr << "Error: " << exception.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}