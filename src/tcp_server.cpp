#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <string>
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>

#include "command_handler.h"
#include "kv_store.h"

namespace {

constexpr int port = 6379;
constexpr std::size_t buffer_size = 1024;
constexpr std::size_t max_command_size = 4096;

bool send_all(int client_fd, const std::string& message) {
    std::size_t total_sent = 0;

    while (total_sent < message.size()) {
        const ssize_t bytes_sent = send(
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

void close_client(
    std::vector<pollfd>& poll_fds,
    std::unordered_map<int, std::string>& pending_by_client,
    std::size_t index
) {
    const int client_fd = poll_fds[index].fd;

    std::cout << "Client disconnected.\n";
    close(client_fd);
    pending_by_client.erase(client_fd);

    // Erase the client from the poll list. Index 0 is always server_fd.
    poll_fds.erase(poll_fds.begin() + static_cast<long>(index));
}

}  // namespace

int main() {
    KvStore store;

    const int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd == -1) {
        std::cerr << "socket failed: " << std::strerror(errno) << "\n";
        return 1;
    }

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

    // Element 0 is permanently the listening socket.
    std::vector<pollfd> poll_fds;
    poll_fds.push_back({server_fd, POLLIN, 0});

    // Each client needs an independent incomplete-command buffer.
    std::unordered_map<int, std::string> pending_by_client;

    while (true) {
        const int ready_count = poll(
            poll_fds.data(),
            static_cast<nfds_t>(poll_fds.size()),
            -1
        );

        if (ready_count == -1) {
            if (errno == EINTR) {
                continue;
            }

            std::cerr << "poll failed: " << std::strerror(errno) << "\n";
            break;
        }

        // A new connection is waiting on the listening socket.
        if ((poll_fds[0].revents & POLLIN) != 0) {
            const int client_fd = accept(server_fd, nullptr, nullptr);

            if (client_fd == -1) {
                std::cerr << "accept failed: " << std::strerror(errno) << "\n";
            } else {
                poll_fds.push_back({client_fd, POLLIN, 0});
                pending_by_client.emplace(client_fd, "");

                std::cout << "Client connected!\n";
            }
        }

        // Start at index 1 because index 0 is server_fd.
        // Count backward because close_client() erases entries.
        for (std::size_t i = poll_fds.size(); i-- > 1;) {
            const short events = poll_fds[i].revents;

            if (events == 0) {
                continue;
            }

            const int client_fd = poll_fds[i].fd;

            // Error or hang-up: close this client.
            if ((events & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
                close_client(poll_fds, pending_by_client, i);
                continue;
            }

            if ((events & POLLIN) == 0) {
                continue;
            }

            char buffer[buffer_size];
            const ssize_t bytes_received = recv(
                client_fd,
                buffer,
                sizeof(buffer),
                0
            );

            if (bytes_received == 0) {
                close_client(poll_fds, pending_by_client, i);
                continue;
            }

            if (bytes_received == -1) {
                std::cerr << "recv failed: " << std::strerror(errno) << "\n";
                close_client(poll_fds, pending_by_client, i);
                continue;
            }

            std::string& pending = pending_by_client.at(client_fd);
            pending.append(
                buffer,
                static_cast<std::size_t>(bytes_received)
            );

            // Reject unfinished commands that exceed the protocol limit.
            if (pending.size() > max_command_size) {
                send_all(client_fd, "ERROR: command too long\n");
                close_client(poll_fds, pending_by_client, i);
                continue;
            }

            bool client_ok = true;
            std::size_t newline_pos;

            // One recv() may contain zero, one, or many command lines.
            while ((newline_pos = pending.find('\n')) != std::string::npos) {
                std::string command = pending.substr(0, newline_pos);
                pending.erase(0, newline_pos + 1);

                // Accept both Unix LF and Windows CRLF line endings.
                if (!command.empty() && command.back() == '\r') {
                    command.pop_back();
                }

                std::cout << "Received: " << command << "\n";

                // execute_command() already includes a trailing newline.
                const std::string response = execute_command(store, command);

                std::cout << "Responding with: " << response;

                if (!send_all(client_fd, response)) {
                    client_ok = false;
                    break;
                }
            }

            if (!client_ok) {
                close_client(poll_fds, pending_by_client, i);
            }
        }
    }

    // Reached only after a fatal poll() error.
    for (const pollfd& entry : poll_fds) {
        close(entry.fd);
    }

    return 0;
}