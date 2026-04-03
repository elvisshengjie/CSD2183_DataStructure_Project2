/*
 * server.cpp  —  APSC Dashboard HTTP Server (CSD2183 Project 2)
 *
 * Endpoints
 * ─────────
 *  GET  /                      → dashboard.html
 *  GET  /dashboard.html        → dashboard.html
 *  GET  /api/list-tests        → JSON list of input CSV files found in tests/
 *  GET  /api/get-file?f=<rel>  → raw content of a file under tests/
 *  POST /api/run-test          → body: {"file":"<rel>","target":<int>}
 *                                runs ./simplify and returns JSON result
 *
 * Build:
 *   g++ -std=c++17 -Wall -Wextra -O2 -o dashboard_server server.cpp -lws2_32   (Windows)
 *   g++ -std=c++17 -Wall -Wextra -O2 -o dashboard_server server.cpp             (Linux/macOS)
 */

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  pragma comment(lib, "ws2_32.lib")
typedef int socklen_t;
#else
#  include <arpa/inet.h>
#  include <fcntl.h>
#  include <netinet/in.h>
#  include <sys/socket.h>
#  include <unistd.h>
#  define SOCKET            int
#  define INVALID_SOCKET    (-1)
#  define SOCKET_ERROR      (-1)
#  define closesocket       close
#endif

namespace fs = std::filesystem;

// ─────────────────────────────────────────────────────────────────────────────
//  Utilities
// ─────────────────────────────────────────────────────────────────────────────

static std::string read_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static std::string url_decode(const std::string& enc) {
    std::string out;
    for (std::size_t i = 0; i < enc.size(); ++i) {
        if (enc[i] == '%' && i + 2 < enc.size()) {
            int v = 0;
            std::istringstream iss(enc.substr(i + 1, 2));
            iss >> std::hex >> v;
            out += static_cast<char>(v);
            i += 2;
        }
        else if (enc[i] == '+') {
            out += ' ';
        }
        else {
            out += enc[i];
        }
    }
    return out;
}

// Minimal JSON string escape
static std::string json_str(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 2);
    out += '"';
    for (char c : s) {
        switch (c) {
        case '"':  out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n";  break;
        case '\r': out += "\\r";  break;
        case '\t': out += "\\t";  break;
        default:   out += c;
        }
    }
    out += '"';
    return out;
}

// Very small JSON object parser — just pulls a string or number value by key.
static std::string json_get(const std::string& body, const std::string& key) {
    const std::string needle = '"' + key + '"';
    auto pos = body.find(needle);
    if (pos == std::string::npos) return {};
    pos += needle.size();
    // skip whitespace and colon
    while (pos < body.size() && (body[pos] == ' ' || body[pos] == ':')) ++pos;
    if (pos >= body.size()) return {};
    if (body[pos] == '"') {
        // string value
        ++pos;
        std::string val;
        while (pos < body.size() && body[pos] != '"') {
            if (body[pos] == '\\') { ++pos; }
            val += body[pos++];
        }
        return val;
    }
    // number/bool value
    std::string val;
    while (pos < body.size() && body[pos] != ',' && body[pos] != '}') {
        val += body[pos++];
    }
    // trim
    while (!val.empty() && (val.back() == ' ' || val.back() == '\t')) val.pop_back();
    return val;
}

static std::string get_mime(const std::string& path) {
    if (path.size() >= 5 && path.substr(path.size() - 5) == ".html") return "text/html; charset=utf-8";
    if (path.size() >= 4 && path.substr(path.size() - 4) == ".css")  return "text/css";
    if (path.size() >= 3 && path.substr(path.size() - 3) == ".js")   return "application/javascript";
    if (path.size() >= 4 && path.substr(path.size() - 4) == ".csv")  return "text/plain; charset=utf-8";
    if (path.size() >= 4 && path.substr(path.size() - 4) == ".png")  return "image/png";
    return "text/plain; charset=utf-8";
}

// ─────────────────────────────────────────────────────────────────────────────
//  Path / security helpers
// ─────────────────────────────────────────────────────────────────────────────

// Strip query string and leading slash, return empty string on traversal attack.
static std::string sanitize_path(const std::string& raw) {
    std::string p = raw;
    auto q = p.find('?');
    if (q != std::string::npos) p = p.substr(0, q);
    if (!p.empty() && p[0] == '/') p = p.substr(1);
    if (p.find("..") != std::string::npos) return {};
    return p;
}

// Resolve a relative path that the client claims is under tests/.
// Returns empty string if it escapes the sandbox.
static std::string safe_test_path(const std::string& rel) {
    if (rel.find("..") != std::string::npos) return {};
    fs::path base = fs::path("tests");
    fs::path full = base / rel;
    // Canonical check: make sure the resolved path starts with base.
    std::error_code ec;
    auto cb = fs::weakly_canonical(base, ec);
    auto cf = fs::weakly_canonical(full, ec);
    if (cf.string().substr(0, cb.string().size()) != cb.string()) return {};
    return full.string();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Test-file discovery
// ─────────────────────────────────────────────────────────────────────────────

struct TestFile {
    std::string rel;   // path relative to tests/
    std::string name;  // filename only
    std::size_t bytes;
    std::string label;
    int default_target = 0;
    std::string notes;
};

struct CaseMeta {
    std::string rel;
    std::string label;
    int default_target = 0;
    std::string notes;
};

static std::string normalize_test_rel(std::string path) {
    std::replace(path.begin(), path.end(), '\\', '/');
    if (path.rfind("tests/", 0) == 0) {
        path = path.substr(6);
    }
    return path;
}

static std::map<std::string, CaseMeta> load_case_manifest() {
    std::map<std::string, CaseMeta> manifest;
    std::ifstream f(fs::path("benchmarks") / "cases.csv");
    if (!f) return manifest;

    std::string line;
    std::getline(f, line); // skip header
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        std::stringstream ss(line);
        std::string label, input_path, target_s, notes;
        if (!std::getline(ss, label, ',')) continue;
        if (!std::getline(ss, input_path, ',')) continue;
        if (!std::getline(ss, target_s, ',')) continue;
        std::getline(ss, notes);

        CaseMeta meta;
        meta.rel = normalize_test_rel(input_path);
        meta.label = label;
        try {
            meta.default_target = std::stoi(target_s);
        }
        catch (...) {
            meta.default_target = 0;
        }
        meta.notes = notes;
        manifest[meta.rel] = meta;
    }
    return manifest;
}

static std::vector<TestFile> list_test_files() {
    std::vector<TestFile> files;
    const auto manifest = load_case_manifest();
    const fs::path base = fs::path("tests");
    std::error_code ec;
    if (!fs::exists(base, ec)) return files;
    for (auto& entry : fs::recursive_directory_iterator(base, ec)) {
        if (entry.is_regular_file(ec)) {
            const std::string ext = entry.path().extension().string();
            const std::string name = entry.path().filename().string();
            const fs::path parent = entry.path().parent_path();
            if ((ext == ".csv" || ext == ".CSV") &&
                name.rfind("input_", 0) == 0 &&
                parent.filename() != ".rubric_tmp" &&
                parent.filename() != "data") {
                TestFile tf;
                tf.rel = fs::relative(entry.path(), base, ec).string();
                // normalize to forward slashes for JSON
                std::replace(tf.rel.begin(), tf.rel.end(), '\\', '/');
                tf.name = name;
                tf.bytes = static_cast<std::size_t>(entry.file_size(ec));
                auto it = manifest.find(tf.rel);
                if (it != manifest.end()) {
                    tf.label = it->second.label;
                    tf.default_target = it->second.default_target;
                    tf.notes = it->second.notes;
                }
                files.push_back(std::move(tf));
            }
        }
    }
    std::sort(files.begin(), files.end(), [](const TestFile& a, const TestFile& b) {
        return a.rel < b.rel;
        });
    return files;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Run the simplifier binary as a child process
// ─────────────────────────────────────────────────────────────────────────────

struct RunResult {
    bool     ok = false;
    int      exit_code = -1;
    std::string stdout_text;
    std::string stderr_text;
    double   elapsed_ms = 0.0;
};

static RunResult run_simplifier(const std::string& csv_path, int target) {
    RunResult r;

    // Build command — use simplify.exe on Windows, ./simplify elsewhere.
#ifdef _WIN32
    const std::string exe = "simplify.exe";
#else
    const std::string exe = "./simplify";
#endif
    const std::string cmd = exe + " " + csv_path + " " + std::to_string(target);

    // Capture both stdout and stderr via temporary files (portable).
    const std::string out_tmp = ".__apsc_stdout.tmp";
    const std::string err_tmp = ".__apsc_stderr.tmp";

#ifdef _WIN32
    const std::string full_cmd = cmd + " > " + out_tmp + " 2> " + err_tmp;
#else
    const std::string full_cmd = cmd + " > " + out_tmp + " 2> " + err_tmp;
#endif

    const auto t0 = std::chrono::steady_clock::now();
    r.exit_code = std::system(full_cmd.c_str());
    const auto t1 = std::chrono::steady_clock::now();

    r.elapsed_ms =
        static_cast<double>(
            std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count()) /
        1000.0;

    r.stdout_text = read_file(out_tmp);
    r.stderr_text = read_file(err_tmp);
    std::remove(out_tmp.c_str());
    std::remove(err_tmp.c_str());

    r.ok = (r.exit_code == 0);
    return r;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Parse the known output lines from simplify stdout
// ─────────────────────────────────────────────────────────────────────────────

struct ParsedOutput {
    std::string csv_body;   // everything before the metric lines
    double input_area = 0.0;
    double output_area = 0.0;
    double areal_disp = 0.0;
    int    input_verts = 0;
    int    output_verts = 0;
};

static ParsedOutput parse_output(const std::string& text) {
    ParsedOutput p;
    std::istringstream ss(text);
    std::string line;
    std::string csv_lines;
    while (std::getline(ss, line)) {
        if (line.rfind("Total signed area in input:", 0) == 0) {
            p.input_area = std::stod(line.substr(line.rfind(' ') + 1));
        }
        else if (line.rfind("Total signed area in output:", 0) == 0) {
            p.output_area = std::stod(line.substr(line.rfind(' ') + 1));
        }
        else if (line.rfind("Total areal displacement:", 0) == 0) {
            p.areal_disp = std::stod(line.substr(line.rfind(' ') + 1));
        }
        else {
            // CSV rows
            if (!line.empty() && line[0] != 'T') {
                csv_lines += line + "\n";
                // count output vertices (skip header)
                if (line.rfind("ring_id", 0) != 0) {
                    ++p.output_verts;
                }
            }
        }
    }
    p.csv_body = csv_lines;
    return p;
}

// Count vertices in an input CSV
static int count_input_verts(const std::string& path) {
    std::ifstream f(path);
    if (!f) return 0;
    int n = 0;
    std::string line;
    std::getline(f, line); // skip header
    while (std::getline(f, line)) {
        if (!line.empty()) ++n;
    }
    return n;
}

// ─────────────────────────────────────────────────────────────────────────────
//  HTTP helpers
// ─────────────────────────────────────────────────────────────────────────────

static void send_raw(SOCKET client, const std::string& response) {
    std::size_t total = 0;
    while (total < response.size()) {
        int sent = send(client, response.c_str() + total,
            static_cast<int>(response.size() - total), 0);
        if (sent <= 0) break;
        total += static_cast<std::size_t>(sent);
    }
}

static void send_http(SOCKET client,
    const std::string& status,
    const std::string& content_type,
    const std::string& body) {
    std::string resp =
        "HTTP/1.1 " + status + "\r\n"
        "Content-Type: " + content_type + "\r\n"
        "Content-Length: " + std::to_string(body.size()) + "\r\n"
        "Connection: close\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Cache-Control: no-cache\r\n"
        "\r\n" + body;
    send_raw(client, resp);
}

static void send_200(SOCKET client, const std::string& ct, const std::string& body) {
    send_http(client, "200 OK", ct, body);
}

static void send_400(SOCKET client, const std::string& msg) {
    send_http(client, "400 Bad Request", "text/plain", msg);
}

static void send_404(SOCKET client) {
    send_http(client, "404 Not Found", "text/html",
        "<html><body><h1>404 - Not Found</h1></body></html>");
}

static void send_500(SOCKET client, const std::string& msg) {
    send_http(client, "500 Internal Server Error", "text/plain", msg);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Request handler
// ─────────────────────────────────────────────────────────────────────────────

static void handle(SOCKET client, const std::string& raw_request) {
    // Parse request line
    std::istringstream rs(raw_request);
    std::string method, raw_path, http_ver;
    rs >> method >> raw_path >> http_ver;

    // Read headers so we can get Content-Length for POST body
    std::string headers_section;
    {
        // Find blank line separating headers from body
        auto sep = raw_request.find("\r\n\r\n");
        if (sep == std::string::npos) sep = raw_request.find("\n\n");
        if (sep != std::string::npos) {
            headers_section = raw_request.substr(0, sep);
        }
    }

    // Extract POST body (everything after the blank line)
    std::string post_body;
    {
        auto sep = raw_request.find("\r\n\r\n");
        if (sep != std::string::npos) post_body = raw_request.substr(sep + 4);
        else {
            auto sep2 = raw_request.find("\n\n");
            if (sep2 != std::string::npos) post_body = raw_request.substr(sep2 + 2);
        }
    }

    // Separate path from query string
    std::string path = raw_path;
    std::string query;
    {
        auto qpos = path.find('?');
        if (qpos != std::string::npos) {
            query = path.substr(qpos + 1);
            path = path.substr(0, qpos);
        }
    }

    // ── Default page ──────────────────────────────────────────────────────────
    if (path == "/" || path == "") path = "/dashboard.html";

    // ── API: list test CSV files ──────────────────────────────────────────────
    if (path == "/api/list-tests") {
        auto files = list_test_files();
        std::string json = "[";
        for (std::size_t i = 0; i < files.size(); ++i) {
            if (i) json += ",";
            json += "{\"rel\":" + json_str(files[i].rel) +
                ",\"name\":" + json_str(files[i].name) +
                ",\"bytes\":" + std::to_string(files[i].bytes) +
                ",\"label\":" + json_str(files[i].label) +
                ",\"default_target\":" + std::to_string(files[i].default_target) +
                ",\"notes\":" + json_str(files[i].notes) + "}";
        }
        json += "]";
        send_200(client, "application/json", json);
        return;
    }

    // ── API: get raw file content ─────────────────────────────────────────────
    if (path == "/api/get-file") {
        // query: f=<rel-path-under-tests>
        std::string rel;
        {
            // parse f= from query
            auto fpos = query.find("f=");
            if (fpos != std::string::npos) {
                rel = url_decode(query.substr(fpos + 2));
                auto amp = rel.find('&');
                if (amp != std::string::npos) rel = rel.substr(0, amp);
            }
        }
        if (rel.empty()) { send_400(client, "Missing f= parameter"); return; }
        std::string full = safe_test_path(rel);
        if (full.empty()) { send_400(client, "Invalid path"); return; }
        std::string content = read_file(full);
        if (content.empty()) { send_404(client); return; }
        send_200(client, "text/plain; charset=utf-8", content);
        return;
    }

    // ── API: run simplifier on a test file ───────────────────────────────────
    if (path == "/api/run-test") {
        if (method != "POST") { send_400(client, "POST required"); return; }

        const std::string rel_file = json_get(post_body, "file");
        const std::string target_s = json_get(post_body, "target");

        if (rel_file.empty()) { send_400(client, "Missing 'file' field"); return; }
        if (target_s.empty()) { send_400(client, "Missing 'target' field"); return; }

        int target = 0;
        try { target = std::stoi(target_s); }
        catch (...) {
            send_400(client, "Invalid 'target' value"); return;
        }
        if (target < 3) { send_400(client, "'target' must be >= 3"); return; }

        std::string full = safe_test_path(rel_file);
        if (full.empty()) { send_400(client, "Invalid file path"); return; }

        // Count input vertices before running
        const int in_verts = count_input_verts(full);

        RunResult r = run_simplifier(full, target);
        ParsedOutput po = parse_output(r.stdout_text);
        po.input_verts = in_verts;

        // Build JSON response
        std::string json =
            "{"
            "\"ok\":" + std::string(r.ok ? "true" : "false") +
            ",\"exit_code\":" + std::to_string(r.exit_code) +
            ",\"elapsed_ms\":" + std::to_string(po.input_area == 0.0 ? r.elapsed_ms : r.elapsed_ms) +
            // we always report elapsed from timing
            ",\"elapsed_ms_real\":" + [&] { std::ostringstream s; s << r.elapsed_ms; return s.str(); }() +
            ",\"input_vertices\":" + std::to_string(po.input_verts) +
            ",\"output_vertices\":" + std::to_string(po.output_verts) +
            ",\"input_area\":" + [&] { std::ostringstream s; s << po.input_area; return s.str(); }() +
            ",\"output_area\":" + [&] { std::ostringstream s; s << po.output_area; return s.str(); }() +
            ",\"areal_displacement\":" + [&] { std::ostringstream s; s << po.areal_disp; return s.str(); }() +
            ",\"stdout\":" + json_str(r.stdout_text) +
            ",\"stderr\":" + json_str(r.stderr_text) +
            ",\"csv_output\":" + json_str(po.csv_body) +
            "}";

        send_200(client, "application/json", json);
        return;
    }

    // ── Static file serving ───────────────────────────────────────────────────
    // Security: block traversal
    if (path.find("..") != std::string::npos) { send_404(client); return; }

    std::string rel = path.substr(1); // strip leading /
    std::string file_path = "src/" + rel;

    std::string content = read_file(file_path);
    if (content.empty()) {
        // Try without src/ prefix (e.g., for the root dashboard.html)
        content = read_file(rel);
    }
    if (content.empty()) {
        std::cerr << "404: " << file_path << "\n";
        send_404(client);
        return;
    }
    send_200(client, get_mime(file_path), content);
    std::cout << "Served " << path << " (" << content.size() << " bytes)\n";
}

// ─────────────────────────────────────────────────────────────────────────────
//  Receive a full HTTP request from the socket
// ─────────────────────────────────────────────────────────────────────────────

static std::string recv_request(SOCKET client) {
    std::string data;
    data.reserve(8192);
    char buf[4096];
    // Read until we have headers + complete body (for POST we need Content-Length)
    while (true) {
        int n = recv(client, buf, sizeof(buf), 0);
        if (n <= 0) break;
        data.append(buf, static_cast<std::size_t>(n));

        // Check if we have the full headers
        auto hend = data.find("\r\n\r\n");
        if (hend == std::string::npos) continue;

        // Check Content-Length
        std::string lower = data.substr(0, hend);
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        auto clpos = lower.find("content-length:");
        if (clpos == std::string::npos) break; // no body expected

        std::size_t cl_val = 0;
        try {
            cl_val = std::stoul(data.substr(clpos + 15));
        }
        catch (...) { break; }

        std::size_t body_received = data.size() - (hend + 4);
        if (body_received >= cl_val) break;
        // Still reading body...
    }
    return data;
}

// ─────────────────────────────────────────────────────────────────────────────
//  main
// ─────────────────────────────────────────────────────────────────────────────

#include <chrono>  // used in run_simplifier above

int main(int argc, char* argv[]) {
    int port = 8080;
    if (argc > 1) {
        port = std::atoi(argv[1]);
        if (port <= 0 || port > 65535) { port = 8080; }
    }

    // Sanity check — dashboard must exist
    if (!fs::exists("src/dashboard.html")) {
        std::cerr << "Error: src/dashboard.html not found. "
            "Run the server from the project root.\n";
        return 1;
    }

    // Warn if tests directory is missing (not fatal)
    if (!fs::exists(fs::path("tests"))) {
        std::cerr << "Warning: tests/ directory not found. "
            "Test runner will show no files.\n";
    }

#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "WSAStartup failed\n"; return 1;
    }
#endif

    SOCKET srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv == INVALID_SOCKET) { std::cerr << "socket() failed\n"; return 1; }

    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*>(&opt), sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(static_cast<uint16_t>(port));

    if (bind(srv, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        std::cerr << "bind() failed on port " << port << "\n";
        closesocket(srv); return 1;
    }
    if (listen(srv, 16) == SOCKET_ERROR) {
        std::cerr << "listen() failed\n"; closesocket(srv); return 1;
    }

    std::cout << "\n╔══════════════════════════════════════════╗\n"
        << "║   APSC Polygon Simplifier Dashboard      ║\n"
        << "╠══════════════════════════════════════════╣\n"
        << "║  http://localhost:" << port;
    // pad to 26 chars
    int pad = 26 - static_cast<int>(std::to_string(port).size());
    for (int i = 0; i < pad; ++i) std::cout << ' ';
    std::cout << "║\n"
        << "║  Ctrl+C to stop                          ║\n"
        << "╚══════════════════════════════════════════╝\n\n";

    while (true) {
        sockaddr_in cli_addr{};
        socklen_t cli_len = sizeof(cli_addr);
        SOCKET client = accept(srv, reinterpret_cast<sockaddr*>(&cli_addr), &cli_len);
        if (client == INVALID_SOCKET) continue;

        std::string req = recv_request(client);
        if (!req.empty()) handle(client, req);

        closesocket(client);
    }

#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
