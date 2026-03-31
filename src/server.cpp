#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstring>
#include <map>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef int socklen_t;
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket close
#endif

class SimpleHTTPServer {
private:
    SOCKET server_socket;
    int port;
    std::string base_path;

    std::string get_mime_type(const std::string& path) {
        if (path.find(".html") != std::string::npos) return "text/html";
        if (path.find(".css") != std::string::npos) return "text/css";
        if (path.find(".js") != std::string::npos) return "application/javascript";
        if (path.find(".png") != std::string::npos) return "image/png";
        if (path.find(".jpg") != std::string::npos) return "image/jpeg";
        if (path.find(".csv") != std::string::npos) return "text/csv";
        return "text/plain";
    }

    std::string read_file(const std::string& filename) {
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            return "";
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    std::string url_decode(const std::string& encoded) {
        std::string decoded;
        for (size_t i = 0; i < encoded.length(); ++i) {
            if (encoded[i] == '%' && i + 2 < encoded.length()) {
                int value;
                std::istringstream is(encoded.substr(i + 1, 2));
                if (is >> std::hex >> value) {
                    decoded += static_cast<char>(value);
                    i += 2;
                }
                else {
                    decoded += encoded[i];
                }
            }
            else if (encoded[i] == '+') {
                decoded += ' ';
            }
            else {
                decoded += encoded[i];
            }
        }
        return decoded;
    }

    void send_response(SOCKET client, const std::string& content, const std::string& content_type) {
        std::string response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: " + content_type + "\r\n";
        response += "Content-Length: " + std::to_string(content.length()) + "\r\n";
        response += "Connection: close\r\n";
        response += "Access-Control-Allow-Origin: *\r\n";
        response += "\r\n";
        response += content;

        send(client, response.c_str(), response.length(), 0);
    }

    void send_404(SOCKET client) {
        std::string content = "<html><body><h1>404 - File Not Found</h1></body></html>";
        send_response(client, content, "text/html");
    }

    void handle_request(SOCKET client, const std::string& request) {
        // Parse the request line
        std::istringstream request_stream(request);
        std::string method, path, version;
        request_stream >> method >> path >> version;

        // Default to dashboard.html
        if (path == "/" || path == "") {
            path = "/dashboard.html";
        }

        // Remove query string
        size_t query_pos = path.find('?');
        if (query_pos != std::string::npos) {
            path = path.substr(0, query_pos);
        }

        // Security: prevent directory traversal
        if (path.find("..") != std::string::npos) {
            send_404(client);
            return;
        }

        // Build file path
        std::string file_path = "src" + path;

        // Read and send file
        std::string content = read_file(file_path);
        if (content.empty()) {
            std::cerr << "File not found: " << file_path << std::endl;
            send_404(client);
        }
        else {
            std::string content_type = get_mime_type(file_path);
            send_response(client, content, content_type);
            std::cout << "Served: " << path << " (" << content.length() << " bytes)" << std::endl;
        }
    }

public:
    SimpleHTTPServer(int port_num = 8080) : server_socket(INVALID_SOCKET), port(port_num) {}

    ~SimpleHTTPServer() {
        stop();
    }

    bool start() {
#ifdef _WIN32
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            std::cerr << "WSAStartup failed" << std::endl;
            return false;
        }
#endif

        server_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (server_socket == INVALID_SOCKET) {
            std::cerr << "Failed to create socket" << std::endl;
            return false;
        }

        int opt = 1;
        if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt)) < 0) {
            std::cerr << "Failed to set socket options" << std::endl;
            closesocket(server_socket);
            return false;
        }

        struct sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(port);

        if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
            std::cerr << "Failed to bind to port " << port << std::endl;
            closesocket(server_socket);
            return false;
        }

        if (listen(server_socket, 10) == SOCKET_ERROR) {
            std::cerr << "Failed to listen" << std::endl;
            closesocket(server_socket);
            return false;
        }

        std::cout << "\n========================================\n";
        std::cout << "  APSC POLYGON SIMPLIFIER DASHBOARD\n";
        std::cout << "========================================\n";
        std::cout << " Server running at: http://localhost:" << port << "\n";
        std::cout << " Open this URL in your browser\n";
        std::cout << " Press Ctrl+C to stop the server\n";
        std::cout << "========================================\n\n";

        return true;
    }

    void run() {
        while (true) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);

            SOCKET client = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
            if (client == INVALID_SOCKET) {
                continue;
            }

            char buffer[8192];
            int bytes = recv(client, buffer, sizeof(buffer) - 1, 0);

            if (bytes > 0) {
                buffer[bytes] = '\0';
                handle_request(client, std::string(buffer));
            }

            closesocket(client);
        }
    }

    void stop() {
        if (server_socket != INVALID_SOCKET) {
            closesocket(server_socket);
            server_socket = INVALID_SOCKET;
        }
#ifdef _WIN32
        WSACleanup();
#endif
    }
};

int main(int argc, char* argv[]) {
    int port = 8080;
    if (argc > 1) {
        port = std::atoi(argv[1]);
        if (port <= 0 || port > 65535) {
            std::cerr << "Invalid port, using 8080" << std::endl;
            port = 8080;
        }
    }

    // Check if dashboard.html exists
    std::ifstream test("src/dashboard.html");
    if (!test.is_open()) {
        std::cerr << "Error: src/dashboard.html not found!" << std::endl;
        std::cerr << "Make sure dashboard.html is in the src folder." << std::endl;
        return 1;
    }
    test.close();

    SimpleHTTPServer server(port);

    if (!server.start()) {
        return 1;
    }

    server.run();

    return 0;
}