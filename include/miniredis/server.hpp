#pragma once

#include "miniredis/kvstore.hpp"
#include "miniredis/thread_pool.hpp"
#include "miniredis/command_handler.hpp"
#include <atomic>
#include <string>

namespace miniredis {

class Server {
public:
    Server(KVStore& store, int port = 6379, size_t num_threads = 4);
    ~Server();

    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    void start();
    void stop();

private:
    int port_;
    int server_fd_{-1};
    std::atomic<bool> running_{false};
    ThreadPool pool_;
    CommandHandler handler_;

    void handle_client(int client_fd);
};

} // namespace miniredis
