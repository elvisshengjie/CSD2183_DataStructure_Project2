#include <iostream>
#include <string>
#include <vector>
#include "geometry.hpp"
#include "csv_io.hpp"
#include "simplifier.hpp"

using namespace std;


int main(int argc, char* argv[]) {
    if (argc != 3) {
        cerr << "Usage: " << argv[0] << " <input_file.csv> <target_vertices>\n";
        return 1;
    }

    string input_file = argv[1];
    int target_vertices = stoi(argv[2]);

    vector<vector<Vertex*>> rings;
    double initial_area = 0.0;
    int current_vertices = 0;

    // Load data from CSV
    CSV_IO::loadCSV(input_file, rings, initial_area, current_vertices);

    // Run simplification
    Simplifier simplifier(rings, current_vertices);
    simplifier.simplify(target_vertices);

    // Print output and metrics
    CSV_IO::printOutput(rings, input_file, initial_area, simplifier.getTotalDisplacement());

    // Clean up memory
    CSV_IO::cleanup(rings);

    return 0;
}
