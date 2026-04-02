#ifndef SIMPLIFIER_H
#define SIMPLIFIER_H

#include "geometry.hpp"
#include <vector>
#include <queue>


class Simplifier {
private:
    std::vector<std::vector<Vertex*>>& rings;
    std::priority_queue<Candidate, std::vector<Candidate>, std::greater<Candidate>> pq;
    int current_vertices;
    double total_areal_displacement;

    Point calculateE(Vertex* vA, Vertex* vB, Vertex* vC, Vertex* vD);
    double calculateDisplacement(Vertex* vA, Vertex* vB, Vertex* vC, Vertex* vD, const Point& E);
    void buildQueue();

public:
    Simplifier(std::vector<std::vector<Vertex*>>& input_rings, int initial_vertices);
    
    void simplify(int target_vertices);
    double getTotalDisplacement() const;
};

#endif // SIMPLIFIER_H