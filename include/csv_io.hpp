#ifndef CSV_IO_H
#define CSV_IO_H

#include "geometry.hpp"
#include <string>
#include <vector>

namespace CSV_IO {
    // Loads CSV and populates the rings, initial area, and vertex count
    void loadCSV(const std::string& filename, std::vector<std::vector<Vertex*>>& rings, double& initial_signed_area, int& current_vertices);
    
    // Prints the simplified rings to standard output
    void printOutput(const std::vector<std::vector<Vertex*>>& rings, const std::string& input_file, double initial_signed_area, double total_areal_displacement);
    
    // Cleans up dynamically allocated memory
    void cleanup(std::vector<std::vector<Vertex*>>& rings);
}

#endif // CSV_IO_H
