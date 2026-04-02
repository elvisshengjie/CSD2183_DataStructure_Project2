#include "csv_io.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>

using namespace std;

namespace CSV_IO {

    void loadCSV(const string& filename, vector<vector<Vertex*>>& rings, double& initial_signed_area, int& current_vertices) {
        ifstream file(filename);
        string line;
        getline(file, line); // Skip header

        int current_ring = -1;
        vector<Vertex*> current_ring_vertices;

        while (getline(file, line)) {
            stringstream ss(line);
            string token;
            int r_id, v_id;
            double x, y;

            getline(ss, token, ','); r_id = stoi(token);
            getline(ss, token, ','); v_id = stoi(token);
            getline(ss, token, ','); x = stod(token);
            getline(ss, token, ','); y = stod(token);

            if (r_id != current_ring) {
                if (!current_ring_vertices.empty()) rings.push_back(current_ring_vertices);
                current_ring_vertices.clear();
                current_ring = r_id;
            }
            Vertex* v = new Vertex(v_id, r_id, {x, y});
            current_ring_vertices.push_back(v);
            current_vertices++;
        }
        if (!current_ring_vertices.empty()) rings.push_back(current_ring_vertices);

        for (auto& ring : rings) {
            int n = ring.size();
            vector<Point> pts;
            for (int i = 0; i < n; i++) {
                ring[i]->prev = ring[(i - 1 + n) % n];
                ring[i]->next = ring[(i + 1) % n];
                pts.push_back(ring[i]->p);
            }
            initial_signed_area += polygonArea(pts);
        }
    }

    void printOutput(const vector<vector<Vertex*>>& rings, double initial_signed_area, double total_areal_displacement) {
        double final_signed_area = 0;

        cout << "ring_id,vertex_id,x,y\n";
        int out_ring_id = 0;
        
        for (const auto& ring : rings) {
            vector<Point> final_ring;
            Vertex* curr = ring[0];
            while(curr && !curr->active) curr = curr->next;
            if (!curr) continue;

            Vertex* start = curr;
            int v_id = 0;
            do {
                if (curr->active) {
                    cout << out_ring_id << "," << v_id++ << "," << curr->p.x << "," << curr->p.y << "\n";
                    final_ring.push_back(curr->p);
                }
                curr = curr->next;
            } while (curr != start);
            
            final_signed_area += polygonArea(final_ring);
            out_ring_id++;
        }
        
        cout << scientific << setprecision(6);
        cout << "Total signed area in input: " << initial_signed_area << "\n"; 
        cout << "Total signed area in output: " << final_signed_area << "\n";
        cout << "Total areal displacement: " << total_areal_displacement << "\n";
    }

    void cleanup(vector<vector<Vertex*>>& rings) {
        for (auto& ring : rings) {
            for (Vertex* v : ring) {
                delete v;
            }
        }
    }
}