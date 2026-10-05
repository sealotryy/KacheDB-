#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string> 

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

    // Create space in memory where recv() can place bytes from the client.
    // The buffer can hold up to 1023 characters plus one '\0' terminator.
    char buffer[1024];

    // Try to receive up to 1023 bytes from this connected client.
    // recv() returns:
    //   > 0 : number of bytes received
    //   = 0 : client closed its side of the connection
    //   = -1: an error occurred
    ssize_t bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);

    if (bytes_received == -1) {
        std::cerr << "recv failed: " << std::strerror(errno) << "\n";
    } else if (bytes_received == 0) {
        std::cout << "Client disconnected without sending data.\n";
    } else {
        // recv() gives us raw bytes, not a C++ string.
        // Add '\0' so std::cout can safely treat the buffer as text.
        buffer[bytes_received] = '\0';

        std::cout << "Received: " << buffer << "\n";

        // This is the fixed response for now.
        // The '\n' matters: later our protocol uses newlines to mark responses.
        const std::string response = "OK\n";

        // Send the response bytes back through this client's socket.
        ssize_t bytes_sent = send(
            client_fd,
            response.data(),
            response.size(),
            0
        );

        if (bytes_sent == -1) {
            std::cerr << "send failed: " << std::strerror(errno) << "\n";
        } else {
            std::cout << "Sent " << bytes_sent << " bytes to client.\n";
        }
    }

    // Close only this connected client's socket.
    close(client_fd);

    // Close the listening socket because this first server exits after one client.
    close(server_fd);

    std::cout << "Server stopped.\n";
    return 0;
}