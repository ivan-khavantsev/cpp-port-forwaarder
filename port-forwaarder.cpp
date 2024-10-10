#include <iostream>
#include <thread>
#include <vector>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>

void handle_connection(int client_sock, const std::string& target_host, int target_port) {
    int target_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (target_sock < 0) {
        std::cerr << "Failed to create target socket\n";
        close(client_sock);
        return;
    }

    sockaddr_in target_addr;
    target_addr.sin_family = AF_INET;
    target_addr.sin_port = htons(target_port);

    if (inet_pton(AF_INET, target_host.c_str(), &target_addr.sin_addr) <= 0) {
        std::cerr << "Invalid target address\n";
        close(client_sock);
        close(target_sock);
        return;
    }

    if (connect(target_sock, (sockaddr*)&target_addr, sizeof(target_addr)) < 0) {
        std::cerr << "Failed to connect to target\n";
        close(client_sock);
        close(target_sock);
        return;
    }

    std::thread forward([client_sock, target_sock]() {
        char buffer[4096];
        ssize_t bytes_read;
        while ((bytes_read = read(client_sock, buffer, sizeof(buffer))) > 0) {
            write(target_sock, buffer, bytes_read);
        }
        close(client_sock);
        close(target_sock);
    });

    char buffer[4096];
    ssize_t bytes_read;
    while ((bytes_read = read(target_sock, buffer, sizeof(buffer))) > 0) {
        write(client_sock, buffer, bytes_read);
    }

    forward.join();
    close(client_sock);
    close(target_sock);
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <listen_port> <target_host> <target_port>\n";
        return 1;
    }

    int listen_port = std::stoi(argv[1]);
    std::string target_host = argv[2];
    int target_port = std::stoi(argv[3]);

    int listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock < 0) {
        std::cerr << "Failed to create listening socket\n";
        return 1;
    }

    sockaddr_in listen_addr;
    listen_addr.sin_family = AF_INET;
    listen_addr.sin_addr.s_addr = INADDR_ANY;
    listen_addr.sin_port = htons(listen_port);

    if (bind(listen_sock, (sockaddr*)&listen_addr, sizeof(listen_addr)) < 0) {
        std::cerr << "Failed to bind to port " << listen_port << "\n";
        close(listen_sock);
        return 1;
    }

    if (listen(listen_sock, SOMAXCONN) < 0) {
        std::cerr << "Failed to listen on port " << listen_port << "\n";
        close(listen_sock);
        return 1;
    }

    std::cout << "Listening on port " << listen_port << "\n";

    while (true) {
        int client_sock = accept(listen_sock, nullptr, nullptr);
        if (client_sock < 0) {
            std::cerr << "Failed to accept connection\n";
            continue;
        }

        std::thread(handle_connection, client_sock, target_host, target_port).detach();
    }

    close(listen_sock);
    return 0;
}