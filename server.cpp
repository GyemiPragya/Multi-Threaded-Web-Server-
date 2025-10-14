
#include <iostream>
#include <winsock2.h>          // Windows Socket API
#pragma comment(lib, "ws2_32.lib") // Link Winsock library
#include <thread>
#include <sstream>
#include <fstream>

#include<atomic>
#include <vector>
#include <chrono>
#include <mutex>
#include <iomanip>  // For time formatting
#include <algorithm>  // For sorting

// Keep log of requests, track active threads and start time
struct RequestInfo {
    std::string ip;
    std::string path;
    std::string timestamp;
};

std::vector<RequestInfo> request_log;
std::mutex log_mutex;
std::atomic<int> active_threads(0);

auto server_start_time = std::chrono::steady_clock::now();

std::string get_current_time_string() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::tm* parts = std::localtime(&now_c);

    std::ostringstream ss;
    ss << std::put_time(parts, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}
void handle_client(SOCKET client_fd) {
    active_threads++;
    char buffer[2048] = {0};
    int received = recv(client_fd, buffer, sizeof(buffer)-1, 0);

    std::string method, path;
    std::istringstream request_stream(buffer);
    request_stream >> method >> path;
    if (method != "GET") path = "/";

    std::cout << "Received request: " << buffer << "\n";
    std::cout << "Thread ID: " << std::this_thread::get_id() << "\n";
    std::cout << "Requested path: " << path << std::endl;

    // Get client IP
    sockaddr_in addr;
    int addr_size = sizeof(addr);
    getpeername(client_fd, (sockaddr*)&addr, &addr_size);
    std::string ip = inet_ntoa(addr.sin_addr);

    // Log every request immediately
    {
        std::lock_guard<std::mutex> lock(log_mutex);
        request_log.push_back({ip, path, get_current_time_string()});
    }

    // Determine filename for regular file serving
    std::string filename = (path == "/" ? "index.html" : path.substr(1));
    if (filename.empty()) filename = "index.html";
    if (filename.find('.') == std::string::npos) {
        filename += ".html";
    }

    if (path == "/admin") {
        // Render dashboard
        std::ostringstream html;
        html << "<html><head><title>Server Admin Dashboard</title><style>body{font-family:sans-serif}table{border-collapse:collapse}td,th{border:1px solid #ccc;padding:4px;}</style></head><body>";
        html << "<h1>Admin Dashboard</h1>";

        // Uptime
        auto now = std::chrono::steady_clock::now();
        auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - server_start_time).count();
        html << "<p><b>Server uptime:</b> " << uptime << " seconds</p>";
        html << "<p><b>Active threads:</b> " << active_threads.load() << "</p>";
        html << "<p><a href='/admin/download'>Download log as CSV</a></p>";

        // Recent requests table
        html << "<h2>Recent Requests</h2><table><tr><th>Time</th><th>IP</th><th>Path</th></tr>";
        int shown = 0;
        {
            std::lock_guard<std::mutex> lock(log_mutex);
            for (auto it = request_log.rbegin(); it != request_log.rend() && shown < 25; ++it, ++shown) {
                html << "<tr><td>" << it->timestamp << "</td><td>" << it->ip << "</td><td>" << it->path << "</td></tr>";
            }
        }
        html << "</table></body></html>";

        std::string http_response =
            "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n" + html.str();

        send(client_fd, http_response.c_str(), http_response.length(), 0);
        closesocket(client_fd);
        active_threads--;
        return;
    }

    if (path == "/admin/download") {
        std::ostringstream csv;
        csv << "Timestamp,IP,Path\n";
        {
            std::lock_guard<std::mutex> lock(log_mutex);
            for (const auto& req : request_log) {
                csv << req.timestamp << "," << req.ip << "," << req.path << "\n";
            }
        }

        std::string http_response =
            "HTTP/1.1 200 OK\r\nContent-Type: text/csv\r\nContent-Disposition: attachment; filename=\"requests.csv\"\r\nConnection: close\r\n\r\n" + csv.str();

        send(client_fd, http_response.c_str(), http_response.length(), 0);
        closesocket(client_fd);
        active_threads--;
        return;
    }

    // Read file contents or send 404 if missing
    std::ifstream file(filename);
    std::string html_contents;
    if (file) {
        html_contents.assign((std::istreambuf_iterator<char>(file)),
                             std::istreambuf_iterator<char>());
        file.close();
    } else {
        html_contents = "<h1>404 Not Found</h1>";
    }

    std::string http_response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Connection: close\r\n\r\n" +
        html_contents;

    send(client_fd, http_response.c_str(), http_response.length(), 0);
    closesocket(client_fd);
    active_threads--;
    return;
}

int main() {
    // 1. Init Winsock
    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (iResult != 0) {
        std::cerr << "WSAStartup failed: " << iResult << "\n";
        return 1;
    }

    // 2. Create a socket
    SOCKET server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_fd == INVALID_SOCKET) {
        std::cerr << "Socket creation failed!\n";
        WSACleanup();
        return 1;
    }

    // 3. Setup address
    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8080);

    // 4. Bind socket
    if (bind(server_fd, (SOCKADDR*)&address, sizeof(address)) == SOCKET_ERROR) {
        std::cerr << "Bind failed!\n";
        closesocket(server_fd);
        WSACleanup();
        return 1;
    }

    // 5. Listen
    if (listen(server_fd, 1) == SOCKET_ERROR) {
        std::cerr << "Listen failed!\n";
        closesocket(server_fd);
        WSACleanup();
        return 1;
    }
    std::cout << "Server listening on port 8080...\n";

    // Multi-client accept loop:
    while (true) {
        SOCKET client_fd = accept(server_fd, NULL, NULL);
        if (client_fd == INVALID_SOCKET) {
            std::cerr << "Accept failed!\n";
            break;
        }
        std::thread(handle_client, client_fd).detach();
    }

    closesocket(server_fd);
    WSACleanup();
    return 0;
}
