#include "geometry.hpp"
#include <cmath>

bool Point::operator==(const Point& other) const {
    return std::abs(x - other.x) < 1e-9 && std::abs(y - other.y) < 1e-9;
}

Vertex::Vertex(int i, int r, Point pt) : id(i), ring_id(r), p(pt), prev(nullptr), next(nullptr), active(true), version(0) {}

bool Candidate::operator>(const Candidate& other) const {
    return areal_displacement > other.areal_displacement;
}

double crossProduct(const Point& a, const Point& b, const Point& c) {
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

double polygonArea(const std::vector<Point>& ring) {
    double area = 0.0;
    int n = ring.size();
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        area += (ring[i].x * ring[j].y) - (ring[j].x * ring[i].y);
    }
    return area / 2.0;
}