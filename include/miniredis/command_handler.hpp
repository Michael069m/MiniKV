#pragma once

#include "miniredis/kvstore.hpp"
#include "miniredis/resp.hpp"

namespace miniredis {

class CommandHandler {
public:
    explicit CommandHandler(KVStore& store);

    RespValue handle_command(const RespValue& req);

private:
    KVStore& store_;

    RespValue handle_ping(const std::vector<RespValue>& args);
    RespValue handle_set(const std::vector<RespValue>& args);
    RespValue handle_get(const std::vector<RespValue>& args);
    RespValue handle_del(const std::vector<RespValue>& args);
    RespValue handle_exists(const std::vector<RespValue>& args);
    RespValue handle_incr(const std::vector<RespValue>& args);
    RespValue handle_keys(const std::vector<RespValue>& args);
};

} // namespace miniredis
