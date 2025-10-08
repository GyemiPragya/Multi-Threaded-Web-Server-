/*
#include <iostream>              // For input/output
#include <cstring>               // For memset
#include <sys/socket.h>          // For sockets
#include <netinet/in.h>          // For sockaddr_in
#include <unistd.h>              // For close()

int main() {
    // 1. Create a socket (IPv4, TCP)
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::cerr << "Socket creation failed!\n";
        return 1;
    }

    // 2. Define server address and port
    sockaddr_in address;
    memset(&address, 0, sizeof(address)); // Zero-fill structure
    address.sin_family = AF_INET; // IPv4
    address.sin_addr.s_addr = INADDR_ANY; // Listen on any network interface
    address.sin_port = htons(8080); // Port 8080

    // 3. Bind the socket to network address and port
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "Bind failed!\n";
        return 1;
    }

    // 4. Listen for incoming connections
    if (listen(server_fd, 10) < 0) {
        std::cerr << "Listen failed!\n";
        return 1;
    }
    std::cout << "Server is listening on port 8080...\n";

    // 5. Accept one incoming connection
    int addrlen = sizeof(address);
    int client_fd = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
    if (client_fd < 0) {
        std::cerr << "Accept failed!\n";
        return 1;
    }

    // 6. Read the HTTP request from client
    char buffer[2048] = {0};
    ssize_t valread = read(client_fd, buffer, sizeof(buffer)-1);
    std::cout << "Received request:\n" << buffer << "\n";

    // 7. Send hardcoded HTTP response
    std::string http_response = 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Connection: close\r\n"
        "\r\n"
        "<h1>Hello from your C++ web server!</h1>\n";
    send(client_fd, http_response.c_str(), http_response.length(), 0);

    // 8. Clean up
    close(client_fd);
    close(server_fd);
    std::cout << "Connection closed. Server shutting down...\n";
    return 0;
}
    */
/*
#include <iostream>
#include <winsock2.h>           // Windows Socket API
#pragma comment(lib, "ws2_32.lib") // Link Winsock library
#include <thread>

#include <sstream>
#include <fstream>

void handle_client(SOCKET client_fd) {
    char buffer[2048] = {0};
    int received = recv(client_fd, buffer, sizeof(buffer)-1, 0);

    std::string method, path;
    std::istringstream request_stream(buffer);
    request_stream >> method >> path;
    if (method != "GET") path = "/";

    std::string filename = (path == "/" ? "index.html" : path.substr(1));
    if (filename.empty()) filename = "index.html";

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

    // Multi-client, multi-thread accept loop:
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
*/
#include <iostream>
#include <winsock2.h>          // Windows Socket API
#pragma comment(lib, "ws2_32.lib") // Link Winsock library
#include <thread>
#include <sstream>
#include <fstream>

void handle_client(SOCKET client_fd) {
    char buffer[2048] = {0};
    int received = recv(client_fd, buffer, sizeof(buffer)-1, 0);

    std::string method, path;
    std::istringstream request_stream(buffer);
    request_stream >> method >> path;
    if (method != "GET") path = "/";

    std::cout << "Received request: " << buffer << "\n";
    std::cout << "Thread ID: " << std::this_thread::get_id() << "\n";
    std::cout << "Requested path: " << path << std::endl;

    // Determine filename
    std::string filename = (path == "/" ? "index.html" : path.substr(1));
    if (filename.empty()) filename = "index.html";
    if (filename.find('.') == std::string::npos) {
        filename += ".html";
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
