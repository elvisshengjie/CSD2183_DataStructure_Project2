#include "../include/simplifier.hpp"
#include <cmath>
#include <limits>
#include <algorithm>

using namespace std;

namespace {

constexpr double kEpsilon = 1e-9;
constexpr int kStrictRingCheckThreshold = 512;

bool pointsEqual(const Point& lhs, const Point& rhs) {
    return abs(lhs.x - rhs.x) < kEpsilon && abs(lhs.y - rhs.y) < kEpsilon;
}

bool segmentsIntersectStrict(const Point& a, const Point& b, const Point& c, const Point& d) {
    auto orient = [](const Point& p, const Point& q, const Point& r) {
        return crossProduct(p, q, r);
    };

    auto onSegment = [](const Point& p, const Point& q, const Point& r) {
        return q.x <= max(p.x, r.x) + kEpsilon && q.x >= min(p.x, r.x) - kEpsilon &&
               q.y <= max(p.y, r.y) + kEpsilon && q.y >= min(p.y, r.y) - kEpsilon;
    };

    double o1 = orient(a, b, c);
    double o2 = orient(a, b, d);
    double o3 = orient(c, d, a);
    double o4 = orient(c, d, b);

    if (((o1 > kEpsilon && o2 < -kEpsilon) || (o1 < -kEpsilon && o2 > kEpsilon)) &&
        ((o3 > kEpsilon && o4 < -kEpsilon) || (o3 < -kEpsilon && o4 > kEpsilon))) {
        return true;
    }

    if (abs(o1) < kEpsilon && onSegment(a, c, b)) return true;
    if (abs(o2) < kEpsilon && onSegment(a, d, b)) return true;
    if (abs(o3) < kEpsilon && onSegment(c, a, d)) return true;
    if (abs(o4) < kEpsilon && onSegment(c, b, d)) return true;
    return false;
}

bool edgesAreAdjacent(size_t i, size_t j, size_t n) {
    if (i == j) return true;
    if ((i + 1) % n == j) return true;
    if ((j + 1) % n == i) return true;
    return false;
}

vector<Point> concatenateOriginalChains(const vector<Point>& lhs, const vector<Point>& rhs) {
    if (lhs.empty()) return rhs;
    if (rhs.empty()) return lhs;

    vector<Point> out = lhs;
    size_t rhsStart = pointsEqual(lhs.back(), rhs.front()) ? 1 : 0;
    out.insert(out.end(), rhs.begin() + static_cast<ptrdiff_t>(rhsStart), rhs.end());
    return out;
}

} // namespace

// --- INTERNAL DYNAMIC SPATIAL GRID ---
class SpatialGrid {
private:
    double min_x, min_y, cell_size;
    int cols, rows;
    
    struct EdgeRecord {
        Vertex* u;
        Vertex* v;
        int version_u;
        int version_v;
    };
    vector<vector<vector<EdgeRecord>>> grid;

    void getCellCoords(const Point& p, int& cx, int& cy) {
        cx = max(0, min(cols - 1, (int)((p.x - min_x) / cell_size)));
        cy = max(0, min(rows - 1, (int)((p.y - min_y) / cell_size)));
    }

public:
    SpatialGrid(double mx, double my, double maxx, double maxy, int num_segments) {
        min_x = mx; min_y = my;
        cell_size = max((maxx - min_x) / sqrt(num_segments), (maxy - min_y) / sqrt(num_segments));
        if (cell_size < 1e-6) cell_size = 1.0; 
        
        cols = ceil((maxx - min_x) / cell_size) + 1;
        rows = ceil((maxy - min_y) / cell_size) + 1;
        grid.resize(cols, vector<vector<EdgeRecord>>(rows));
    }

    void insert(Vertex* a, Vertex* b) {
        int cx1, cy1, cx2, cy2;
        getCellCoords(a->p, cx1, cy1);
        getCellCoords(b->p, cx2, cy2);
        
        int min_cx = min(cx1, cx2), max_cx = max(cx1, cx2);
        int min_cy = min(cy1, cy2), max_cy = max(cy1, cy2);

        for (int i = min_cx; i <= max_cx; i++) {
            for (int j = min_cy; j <= max_cy; j++) {
                grid[i][j].push_back({a, b, a->version, b->version});
            }
        }
    }

    bool segmentsIntersect(const Point& a, const Point& b, const Point& c, const Point& d) {
        double cp1 = crossProduct(a, b, c);
        double cp2 = crossProduct(a, b, d);
        double cp3 = crossProduct(c, d, a);
        double cp4 = crossProduct(c, d, b);

        if (((cp1 > 1e-9 && cp2 < -1e-9) || (cp1 < -1e-9 && cp2 > 1e-9)) &&
            ((cp3 > 1e-9 && cp4 < -1e-9) || (cp3 < -1e-9 && cp4 > 1e-9))) {
            return true;
        }

        auto onSegment = [](const Point& p, const Point& q, const Point& r) {
            return q.x <= max(p.x, r.x) + 1e-9 && q.x >= min(p.x, r.x) - 1e-9 &&
                   q.y <= max(p.y, r.y) + 1e-9 && q.y >= min(p.y, r.y) - 1e-9;
        };

        if (abs(cp1) < 1e-9 && onSegment(a, c, b)) return true;
        if (abs(cp2) < 1e-9 && onSegment(a, d, b)) return true;
        if (abs(cp3) < 1e-9 && onSegment(c, a, d)) return true;
        if (abs(cp4) < 1e-9 && onSegment(c, b, d)) return true;

        return false;
    }

    bool checkIntersection(const Point& p1, const Point& p2, Vertex* ignore1, Vertex* ignore2, Vertex* ignore3) {
        int cx1, cy1, cx2, cy2;
        getCellCoords(p1, cx1, cy1);
        getCellCoords(p2, cx2, cy2);
        
        int min_cx = min(cx1, cx2), max_cx = max(cx1, cx2);
        int min_cy = min(cy1, cy2), max_cy = max(cy1, cy2);

        for (int i = min_cx; i <= max_cx; i++) {
            for (int j = min_cy; j <= max_cy; j++) {
                for (auto& edge : grid[i][j]) {
                    Vertex* u = edge.u;
                    Vertex* v = edge.v;
                    
                    if (!u->active || !v->active) continue;
                    if (u->version != edge.version_u || v->version != edge.version_v) continue;
                    
                    if (u == ignore1 || u == ignore2 || u == ignore3 ||
                        v == ignore1 || v == ignore2 || v == ignore3) continue;
                    
                    if (segmentsIntersect(p1, p2, u->p, v->p)) return true;
                }
            }
        }
        return false;
    }
};

// --- SIMPLIFIER IMPLEMENTATION ---

Simplifier::Simplifier(vector<vector<Vertex*>>& input_rings, int initial_vertices) 
    : rings(input_rings), current_vertices(initial_vertices), total_areal_displacement(0.0) {
    ring_vertex_counts.reserve(rings.size());
    for (const auto& ring : rings) {
        ring_vertex_counts.push_back(static_cast<int>(ring.size()));
    }
    buildQueue();
}

double Simplifier::getTotalDisplacement() const {
    return total_areal_displacement;
}

void Simplifier::pushCandidate(Vertex* vB) {
    if (vB == nullptr || !vB->active) return;
    int ringId = vB->ring_id;
    if (ringId < 0 || ringId >= static_cast<int>(ring_vertex_counts.size())) return;
    if (ring_vertex_counts[ringId] <= 3) return;

    Vertex* vA = vB->prev;
    Vertex* vC = vB->next;
    Vertex* vD = vC->next;

    if (vA == nullptr || vC == nullptr || vD == nullptr) return;
    if (!vA->active || !vC->active || !vD->active) return;
    if (vA == vB || vB == vC || vC == vD || vD == vA) return;

    Point e = calculateE(vA, vB, vC, vD);
    double disp = calculateDisplacement(vA, vB, vC, vD, e);
    pq.push({vB, vC, e, disp, vB->version, vC->version});
}

bool Simplifier::ringWouldStayValidAfterCollapse(Vertex* vB, const Point& replacement) const {
    int ringId = vB->ring_id;
    if (ringId < 0 || ringId >= static_cast<int>(rings.size())) return true;
    if (ring_vertex_counts[ringId] > kStrictRingCheckThreshold) return true;

    Vertex* vA = vB->prev;
    Vertex* vC = vB->next;

    vector<Point> ringPoints;
    Vertex* start = vA;
    Vertex* curr = start;
    do {
        if (!curr->active) {
            curr = curr->next;
            continue;
        }

        if (curr == vB) {
            ringPoints.push_back(replacement);
        } else if (curr != vC) {
            ringPoints.push_back(curr->p);
        }
        curr = curr->next;
    } while (curr != start);

    if (ringPoints.size() < 3) return false;

    for (size_t i = 0; i < ringPoints.size(); ++i) {
        if (pointsEqual(ringPoints[i], ringPoints[(i + 1) % ringPoints.size()])) {
            return false;
        }
    }

    for (size_t i = 0; i < ringPoints.size(); ++i) {
        Point a1 = ringPoints[i];
        Point a2 = ringPoints[(i + 1) % ringPoints.size()];
        for (size_t j = i + 1; j < ringPoints.size(); ++j) {
            if (edgesAreAdjacent(i, j, ringPoints.size())) continue;
            Point b1 = ringPoints[j];
            Point b2 = ringPoints[(j + 1) % ringPoints.size()];
            if (segmentsIntersectStrict(a1, a2, b1, b2)) {
                return false;
            }
        }
    }

    return true;
}

Point Simplifier::calculateE(Vertex* vA, Vertex* vB, Vertex* vC, Vertex* vD) {
    Point A = vA->p, B = vB->p, C = vC->p, D = vD->p;

    double a_E = D.y - A.y;
    double b_E = A.x - D.x;
    double c_E = A.x*B.y - B.x*A.y + B.x*C.y - C.x*B.y + C.x*D.y - D.x*C.y;

    double a_AD = D.y - A.y;
    double b_AD = A.x - D.x;
    double c_AD = A.x*D.y - D.x*A.y;

    double a_AB = B.y - A.y;
    double b_AB = A.x - B.x;
    double c_AB = A.x*B.y - B.x*A.y;

    double a_CD = D.y - C.y;
    double b_CD = C.x - D.x;
    double c_CD = C.x*D.y - D.x*C.y;

    auto eval = [](double a, double b, double c, Point P) { return a * P.x + b * P.y - c; };
    auto intersect = [&](double a1, double b1, double c1, double a2, double b2, double c2, bool& valid) -> Point {
        double det = a1 * b2 - a2 * b1;
        if (abs(det) < 1e-9) { valid = false; return {0,0}; }
        valid = true;
        return {(c1 * b2 - c2 * b1) / det, (a1 * c2 - a2 * c1) / det};
    };

    bool vAB, vCD;
    Point E_AB = intersect(a_E, b_E, c_E, a_AB, b_AB, c_AB, vAB);
    Point E_CD = intersect(a_E, b_E, c_E, a_CD, b_CD, c_CD, vCD);

    double eval_AD_B = eval(a_AD, b_AD, c_AD, B);
    double eval_AD_C = eval(a_AD, b_AD, c_AD, C);

    if (eval_AD_B * eval_AD_C >= 0) { 
        if (abs(eval_AD_B) > abs(eval_AD_C)) {
            if (vCD) return E_CD;
            if (vAB) return E_AB;
        } else {
            if (vAB) return E_AB;
            if (vCD) return E_CD;
        }
    } else {
        double eval_AD_E = c_E - c_AD; 
        if (eval_AD_B * eval_AD_E > 0) {
            if (vAB) return E_AB;
            if (vCD) return E_CD;
        } else {
            if (vCD) return E_CD;
            if (vAB) return E_AB;
        }
    }
    return B; 
}

double Simplifier::calculateDisplacement(Vertex* vA, Vertex* vB, Vertex* vC, Vertex* vD, const Point& E) {
    Point A = vA->p, B = vB->p, C = vC->p, D = vD->p;

    auto get_area = [](const vector<Point>& pts) {
        double a = 0;
        for (size_t i = 0; i < pts.size(); i++) {
            Point p1 = pts[i], p2 = pts[(i+1)%pts.size()];
            a += p1.x * p2.y - p2.x * p1.y;
        }
        return a / 2.0;
    };

    vector<Point> subj = {A, B, C, D};
    vector<Point> clip = {A, E, D};

    double subj_area = get_area(subj);
    if (subj_area < 0) { reverse(subj.begin(), subj.end()); subj_area = -subj_area; }
    
    double clip_area = get_area(clip);
    if (clip_area < 0) { reverse(clip.begin(), clip.end()); clip_area = -clip_area; }

    // If the new triangle is nearly degenerate, the displacement is just the area of ABCD
    if (clip_area < 1e-9) return subj_area + clip_area;

    // Sutherland-Hodgman Polygon Clipping (Clip ABCD against the convex triangle AED)
    auto inside = [](Point p, Point a, Point b) {
        return (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x) >= -1e-9;
    };

    auto compute_intersection = [](Point p1, Point p2, Point p3, Point p4) {
        double a1 = p2.y - p1.y, b1 = p1.x - p2.x, c1 = p1.x*p2.y - p2.x*p1.y;
        double a2 = p4.y - p3.y, b2 = p3.x - p4.x, c2 = p3.x*p4.y - p4.x*p3.y;
        double det = a1 * b2 - a2 * b1;
        if (abs(det) < 1e-9) return p1; // Safe fallback
        return Point{(c1 * b2 - c2 * b1) / det, (a1 * c2 - a2 * c1) / det};
    };

    vector<Point> output = subj;
    for (size_t i = 0; i < clip.size(); i++) {
        Point edge_start = clip[i];
        Point edge_end = clip[(i + 1) % clip.size()];
        vector<Point> input = output;
        output.clear();
        if (input.empty()) break;

        Point S = input.back();
        for (Point P : input) {
            if (inside(P, edge_start, edge_end)) {
                if (!inside(S, edge_start, edge_end)) {
                    output.push_back(compute_intersection(S, P, edge_start, edge_end));
                }
                output.push_back(P);
            } else if (inside(S, edge_start, edge_end)) {
                output.push_back(compute_intersection(S, P, edge_start, edge_end));
            }
            S = P;
        }
    }

    double intersection_area = abs(get_area(output));
    
    // Displacement Area = Symmetric Difference Area = A + B - 2*(A ∩ B)
    return subj_area + clip_area - 2 * intersection_area;
}

void Simplifier::buildQueue() {
    for (auto& ring : rings) {
        if (ring.size() <= 3) continue;
        for (Vertex* B : ring) {
            pushCandidate(B);
        }
    }
}

void Simplifier::simplify(int target_vertices) {
    double mx = numeric_limits<double>::max(), my = mx, maxx = -mx, maxy = -mx;
    for(auto& ring : rings) {
        for(Vertex* v : ring) {
            mx = min(mx, v->p.x); my = min(my, v->p.y);
            maxx = max(maxx, v->p.x); maxy = max(maxy, v->p.y);
        }
    }

    SpatialGrid grid(mx, my, maxx, maxy, current_vertices);
    for(auto& ring : rings) {
        Vertex* curr = ring[0];
        do {
            if (curr->active) grid.insert(curr, curr->next);
            curr = curr->next;
        } while (curr != ring[0]);
    }

    while (current_vertices > target_vertices && !pq.empty()) {
        Candidate cand = pq.top();
        pq.pop();

        Vertex* B = cand.B;
        Vertex* C = cand.C;

        if (!B->active || !C->active || cand.version_B != B->version || cand.version_C != C->version) continue; 

        Vertex* A = B->prev;
        Vertex* D = C->next;

        if (ring_vertex_counts[B->ring_id] <= 3) continue;

        Point trueE = calculateE(A, B, C, D);
        double trueDisp = calculateDisplacement(A, B, C, D, trueE);

        if (abs(trueDisp - cand.areal_displacement) > 1e-7) {
            pq.push({B, C, trueE, trueDisp, B->version, C->version});
            continue;
        }

        if (grid.checkIntersection(A->p, trueE, A, B, C) || grid.checkIntersection(trueE, D->p, B, C, D)) continue;
        if (!ringWouldStayValidAfterCollapse(B, trueE)) continue;

        B->p = trueE;
        B->version++;
        A->version++;
        D->version++;

        total_areal_displacement += trueDisp;

        C->active = false;
        A->original_chain_to_next = concatenateOriginalChains(A->original_chain_to_next, B->original_chain_to_next);
        B->original_chain_to_next = C->original_chain_to_next;
        B->next = D;
        D->prev = B;

        ring_vertex_counts[B->ring_id]--;
        current_vertices--;

        grid.insert(A->prev, A);
        grid.insert(A, B);
        grid.insert(B, D);
        grid.insert(D, D->next);

        pushCandidate(A);
        pushCandidate(B);
        pushCandidate(D);
    }
}
