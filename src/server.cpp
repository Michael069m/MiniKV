#include "miniredis/server.hpp"
#include "miniredis/resp.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <iostream>

namespace miniredis {

Server::Server(KVStore& store, int port, size_t num_threads)
    : port_(port), pool_(num_threads), handler_(store) {}

Server::~Server() {
    stop();
}

void Server::start() {
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        throw std::runtime_error("Failed to create socket");
    }

    int opt = 1;
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port_);

    if (bind(server_fd_, (struct sockaddr*)&address, sizeof(address)) < 0) {
        close(server_fd_);
        throw std::runtime_error("Failed to bind socket to port " + std::to_string(port_));
    }

    if (listen(server_fd_, 128) < 0) {
        close(server_fd_);
        throw std::runtime_error("Failed to listen on socket");
    }

    running_.store(true);
    std::cout << "MiniRedis Server listening on 0.0.0.0:" << port_ << "...\n";

    while (running_.load()) {
        sockaddr_in client_addr{};
        socklen_t addrlen = sizeof(client_addr);
        int client_fd = accept(server_fd_, (struct sockaddr*)&client_addr, &addrlen);
        if (client_fd < 0) {
            if (!running_.load()) break;
            continue;
        }

        // Delegate client handling to ThreadPool
        pool_.enqueue([this, client_fd]() {
            handle_client(client_fd);
        });
    }
}

void Server::stop() {
    if (running_.exchange(false)) {
        if (server_fd_ >= 0) {
            close(server_fd_);
            server_fd_ = -1;
        }
    }
}

void Server::handle_client(int client_fd) {
    std::string read_buffer;
    char chunk[4096];

    while (running_.load()) {
        ssize_t bytes_read = read(client_fd, chunk, sizeof(chunk));
        if (bytes_read <= 0) {
            break; // Client disconnected or error
        }

        read_buffer.append(chunk, bytes_read);

        while (!read_buffer.empty()) {
            auto parse_res = RespParser::parse(read_buffer);
            if (!parse_res.has_value()) {
                break; // Incomplete command frame, wait for more TCP data
            }

            RespValue response = handler_.handle_command(parse_res->value);
            std::string resp_bytes = response.serialize();

            write(client_fd, resp_bytes.data(), resp_bytes.size());
            read_buffer.erase(0, parse_res->bytes_consumed);
        }
    }

    close(client_fd);
}

} // namespace miniredis
