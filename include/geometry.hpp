#ifndef GEOMETRY_H
#define GEOMETRY_H

#include <vector>

// --- GEOMETRY PRIMITIVES ---
struct Point {
    double x, y;
    bool operator==(const Point& other) const;
};

// --- DOUBLY LINKED LIST NODE (DCEL approach) ---
struct Vertex {
    int id;
    int ring_id;
    Point p;
    Vertex* prev;
    Vertex* next;
    bool active;
    int version; // Tracks changes for the lazy priority queue

    Vertex(int i, int r, Point pt);
};

// --- CANDIDATE FOR PRIORITY QUEUE ---
struct Candidate {
    Vertex* B;
    Vertex* C;
    Point E;
    double areal_displacement;
    int version_B;
    int version_C;

    bool operator>(const Candidate& other) const;
};

// --- GEOMETRY UTILITIES ---
double crossProduct(const Point& a, const Point& b, const Point& c);
double polygonArea(const std::vector<Point>& ring);

#endif // GEOMETRY_H