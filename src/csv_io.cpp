#include "csv_io.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <limits>
#include <filesystem>
#include <cstdlib>

using namespace std;

namespace CSV_IO {

    namespace {
        void writeGeometryCSV(const vector<vector<Vertex*>>& rings, const filesystem::path& output_path) {
            ofstream out(output_path);
            out << setprecision(numeric_limits<double>::max_digits10);
            out << "ring_id,vertex_id,x,y\n";

            int out_ring_id = 0;
            for (const auto& ring : rings) {
                if (ring.empty()) continue;

                Vertex* start = ring[0];
                while (start && !start->active) start = start->next;
                if (!start) continue;

                Vertex* curr = start;
                int vertex_id = 0;
                do {
                    if (curr->active) {
                        out << out_ring_id << "," << vertex_id++ << "," << curr->p.x << "," << curr->p.y << "\n";
                    }
                    curr = curr->next;
                } while (curr != start);

                out_ring_id++;
            }
        }

        double tryComputeExactDisplacement(
            const string& input_file,
            const vector<vector<Vertex*>>& rings,
            double fallback_value
        ) {
            std::error_code ec;
            for (int attempt = 0; attempt < 2; ++attempt) {
                filesystem::path temp_output =
                    filesystem::temp_directory_path() / ("simplify_exact_displacement_output_" + to_string(attempt) + ".csv");
                filesystem::path temp_metric =
                    filesystem::temp_directory_path() / ("simplify_exact_displacement_value_" + to_string(attempt) + ".txt");
                writeGeometryCSV(rings, temp_output);

                ostringstream cmd;
                cmd << "sh tools/compute_symdiff_area.sh "
                    << quoted(input_file) << " "
                    << quoted(temp_output.string())
                    << " > " << quoted(temp_metric.string())
                    << " 2>/dev/null";

                int status = system(cmd.str().c_str());
                if (status != 0) {
                    filesystem::remove(temp_output, ec);
                    filesystem::remove(temp_metric, ec);
                    continue;
                }

                ifstream in(temp_metric);
                string output;
                getline(in, output);

                filesystem::remove(temp_output, ec);
                filesystem::remove(temp_metric, ec);

                try {
                    return stod(output);
                } catch (...) {
                    continue;
                }
            }

            return fallback_value;
        }

        double displacementFromOriginalChains(const vector<vector<Vertex*>>& rings) {
            double total = 0.0;

            for (const auto& ring : rings) {
                if (ring.empty()) continue;

                Vertex* start = ring[0];
                while (start && !start->active) start = start->next;
                if (!start) continue;

                Vertex* curr = start;
                do {
                    if (curr->active && curr->next && curr->next->active) {
                        vector<Point> channel;
                        channel.push_back(curr->p);
                        channel.insert(
                            channel.end(),
                            curr->original_chain_to_next.begin(),
                            curr->original_chain_to_next.end()
                        );
                        channel.push_back(curr->next->p);

                        if (channel.size() >= 3) {
                            total += abs(polygonArea(channel));
                        }
                    }
                    curr = curr->next;
                } while (curr != start);
            }

            return total;
        }
    }

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
                ring[i]->original_chain_to_next = {ring[i]->original_p, ring[(i + 1) % n]->original_p};
                pts.push_back(ring[i]->p);
            }
            initial_signed_area += polygonArea(pts);
        }
    }

    void printOutput(const vector<vector<Vertex*>>& rings, const string& input_file, double initial_signed_area, double total_areal_displacement) {
        double final_signed_area = 0;
        total_areal_displacement = tryComputeExactDisplacement(
            input_file,
            rings,
            displacementFromOriginalChains(rings)
        );

        cout << setprecision(numeric_limits<double>::max_digits10);
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
        
        cout << scientific << setprecision(15);
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
