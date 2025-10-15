#include <iostream>
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#include <thread>
#include <sstream>
#include <fstream>
#include <atomic>
#include <vector>
#include <chrono>
#include <mutex>
#include <iomanip>
#include <algorithm>

// === Data Structures for Logging and Thread Tracking ===
struct RequestInfo {
    std::string ip;
    std::string path;
    std::string timestamp;
};

std::vector<RequestInfo> request_log;
std::mutex log_mutex;
std::atomic<int> active_threads(0);

auto server_start_time = std::chrono::steady_clock::now();

// === Helper: Get Current Time as String ===
std::string get_current_time_string() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::tm* parts = std::localtime(&now_c);
    std::ostringstream ss;
    ss << std::put_time(parts, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

// === Helper: Get POST Body from HTTP Request ===
std::string get_post_body(const char* buffer) {
    const char* end_of_headers = strstr(buffer, "\r\n\r\n");
    if (end_of_headers) {
        return std::string(end_of_headers + 4);
    }
    return "";
}

// === Helper: Convert CSV Text to JSON String ===
std::string csv_to_json(const std::string& csv_content) {
    std::istringstream iss(csv_content);
    std::string line;
    std::vector<std::string> headers;
    std::vector<std::vector<std::string>> rows;

    // Extract first row as headers
    if (std::getline(iss, line)) {
        std::istringstream header_stream(line);
        std::string col;
        while (std::getline(header_stream, col, ',')) {
            headers.push_back(col);
        }
    }
    // Process remaining lines as data rows
    while (std::getline(iss, line)) {
        std::istringstream row_stream(line);
        std::string cell;
        std::vector<std::string> row;
        while (std::getline(row_stream, cell, ',')) {
            row.push_back(cell);
        }
        rows.push_back(row);
    }
    // Make JSON array from headers + rows
    std::ostringstream json;
    json << "[";
    for (size_t i = 0; i < rows.size(); ++i) {
        json << "{";
        for (size_t j = 0; j < headers.size(); ++j) {
            json << "\"" << headers[j] << "\":\"" << (j < rows[i].size() ? rows[i][j] : "") << "\"";
            if (j != headers.size() - 1) json << ",";
        }
        json << "}";
        if (i != rows.size() - 1) json << ",";
    }
    json << "]";
    return json.str();
}

// === Main Client Handler ===
void handle_client(SOCKET client_fd) {
    active_threads++;
    char buffer[2048] = {0};
    int received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);

    // Parse HTTP request line for method and path
    std::string method, path;
    std::istringstream request_stream(buffer);
    request_stream >> method >> path;
    if (method != "GET" && method != "POST") path = "/";

    // Get client IP address
    sockaddr_in addr;
    int addr_size = sizeof(addr);
    getpeername(client_fd, (sockaddr*)&addr, &addr_size);
    std::string ip = inet_ntoa(addr.sin_addr);

    // Log each request
    {
        std::lock_guard<std::mutex> lock(log_mutex);
        request_log.push_back({ ip, path, get_current_time_string() });
    }

    // === Handle static file finding logic ===
    std::string filename = (path == "/" ? "index.html" : path.substr(1));
    if (filename.empty()) filename = "index.html";
    if (filename.find('.') == std::string::npos) {
        filename += ".html";
    }

    // === Admin Dashboard Page ===
    if (path == "/admin") {
        std::ostringstream html;
        html << "<html><head><title>Server Admin Dashboard</title><style>body{font-family:sans-serif}table{border-collapse:collapse}td,th{border:1px solid #ccc;padding:4px;}</style></head><body>";
        html << "<h1>Admin Dashboard</h1>";
        auto now = std::chrono::steady_clock::now();
        auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - server_start_time).count();
        html << "<p><b>Server uptime:</b> " << uptime << " seconds</p>";
        html << "<p><b>Active threads:</b> " << active_threads.load() << "</p>";
        html << "<p><a href='/admin/download'>Download log as CSV</a></p>";
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

    // === Admin Log Download API ===
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

    // === API Endpoint: CSV to JSON Conversion ===
    if (path == "/api/csv_to_json" && method == "POST") {
        std::string csv_content = get_post_body(buffer); // Get CSV from request
        std::string json_result = csv_to_json(csv_content); // Convert CSV to JSON string
        std::string http_response =
            "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n" + json_result;
        send(client_fd, http_response.c_str(), http_response.length(), 0);
        closesocket(client_fd);
        active_threads--;
        return;
    }

    // === Handle Static File Requests (default) ===
    std::ifstream file(filename);
    std::string html_contents;
    if (file) {
        html_contents.assign((std::istreambuf_iterator<char>(file)),
            std::istreambuf_iterator<char>());
        file.close();
    }
    else {
        html_contents = "<h1>404 Not Found</h1>";
    }

    // Serve the file or 404
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

// === Server Main: Setup Listening, Accept Clients ===
int main() {
    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        std::cerr << "WSAStartup failed: " << iResult << "\n";
        return 1;
    }

    SOCKET server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_fd == INVALID_SOCKET) {
        std::cerr << "Socket creation failed!\n";
        WSACleanup();
        return 1;
    }

    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8080);

    if (bind(server_fd, (SOCKADDR*)&address, sizeof(address)) == SOCKET_ERROR) {
        std::cerr << "Bind failed!\n";
        closesocket(server_fd);
        WSACleanup();
        return 1;
    }

    if (listen(server_fd, 1) == SOCKET_ERROR) {
        std::cerr << "Listen failed!\n";
        closesocket(server_fd);
        WSACleanup();
        return 1;
    }
    std::cout << "Server listening on port 8080...\n";

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
