#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

int main() {
    constexpr int port = 6379;

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd == -1) {
        std::cerr << "socket failed: " << std::strerror(errno) << "\n";
        return 1;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(port);

    if (bind(server_fd,
             reinterpret_cast<sockaddr*>(&address),
             sizeof(address)) == -1) {
        std::cerr << "bind failed: " << std::strerror(errno) << "\n";
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 10) == -1) {
        std::cerr << "listen failed: " << std::strerror(errno) << "\n";
        close(server_fd);
        return 1;
    }

    std::cout << "Listening on 127.0.0.1:" << port << "\n";
    std::cout << "Waiting for one client...\n";

    int client_fd = accept(server_fd, nullptr, nullptr);

    if (client_fd == -1) {
        std::cerr << "accept failed: " << std::strerror(errno) << "\n";
        close(server_fd);
        return 1;
    }

    std::cout << "Client connected!\n";

    close(client_fd);
    close(server_fd);

    std::cout << "Server stopped.\n";
    return 0;
}