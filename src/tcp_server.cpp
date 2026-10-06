#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

#include "command_handler.h"
#include "kv_store.h"

namespace {

// send() may write fewer bytes than requested, so keep sending until the
// entire response is sent or an error occurs.
bool send_all(int client_fd, const std::string& message) {
    std::size_t total_sent = 0;

    while (total_sent < message.size()) {
        ssize_t bytes_sent = send(
            client_fd,
            message.data() + total_sent,
            message.size() - total_sent,
            MSG_NOSIGNAL
        );

        if (bytes_sent == -1) {
            std::cerr << "send failed: " << std::strerror(errno) << "\n";
            return false;
        }

        total_sent += static_cast<std::size_t>(bytes_sent);
    }

    return true;
}

}  // namespace

int main() {
    constexpr int port = 6379;

    // One store for the lifetime of this server process.
    KvStore store;

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd == -1) {
        std::cerr << "socket failed: " << std::strerror(errno) << "\n";
        return 1;
    }

    // Allows a quick restart after a recently closed connection.
    int reuse_address = 1;
    if (setsockopt(
            server_fd,
            SOL_SOCKET,
            SO_REUSEADDR,
            &reuse_address,
            sizeof(reuse_address)
        ) == -1) {
        std::cerr << "setsockopt failed: " << std::strerror(errno) << "\n";
        close(server_fd);
        return 1;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(port);

    if (bind(
            server_fd,
            reinterpret_cast<sockaddr*>(&address),
            sizeof(address)
        ) == -1) {
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
    std::cout << "Press Ctrl+C to stop the server.\n";

    // Accept clients one at a time.
    while (true) {
        std::cout << "Waiting for a client...\n";

        int client_fd = accept(server_fd, nullptr, nullptr);

        if (client_fd == -1) {
            std::cerr << "accept failed: " << std::strerror(errno) << "\n";
            continue;
        }

        std::cout << "Client connected!\n";

        // This belongs to one client connection. It preserves a partial
        // command if TCP delivers it across multiple recv() calls.
        std::string pending;
        char buffer[1024];
        bool client_ok = true;

        // Keep reading commands until this client disconnects or errors.
        while (client_ok) {
            ssize_t bytes_received = recv(
                client_fd,
                buffer,
                sizeof(buffer),
                0
            );

            if (bytes_received == 0) {
                std::cout << "Client disconnected.\n";
                break;
            }

            if (bytes_received == -1) {
                std::cerr << "recv failed: " << std::strerror(errno) << "\n";
                break;
            }

            // Append exactly the bytes recv() returned.
            pending.append(
                buffer,
                static_cast<std::size_t>(bytes_received)
            );

            // Process every complete command in the input buffer.
            std::size_t newline_pos;
            while ((newline_pos = pending.find('\n')) != std::string::npos) {
                std::string command = pending.substr(0, newline_pos);
                pending.erase(0, newline_pos + 1);

                // Let both Unix ("\n") and Windows ("\r\n") clients work.
                if (!command.empty() && command.back() == '\r') {
                    command.pop_back();
                }

                std::cout << "Received: " << command << "\n";

                std::string response = execute_command(store, command);
                response += '\n';

                std::cout << "Responding with: " << response;

                if (!send_all(client_fd, response)) {
                    client_ok = false;
                    break;
                }
            }
        }

        close(client_fd);
        std::cout << "Client connection closed.\n";
    }

    close(server_fd);
    return 0;
}