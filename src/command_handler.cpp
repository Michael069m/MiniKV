#include "miniredis/command_handler.hpp"
#include <algorithm>
#include <cctype>

namespace miniredis {

CommandHandler::CommandHandler(KVStore& store) : store_(store) {}

RespValue CommandHandler::handle_command(const RespValue& req) {
    if (req.type != RespType::Array || req.array_val.empty()) {
        return RespValue::make_error("ERR invalid command format");
    }

    std::string cmd = req.array_val[0].string_val;
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), [](unsigned char c) { return std::toupper(c); });

    if (cmd == "PING") {
        return handle_ping(req.array_val);
    } else if (cmd == "SET") {
        return handle_set(req.array_val);
    } else if (cmd == "GET") {
        return handle_get(req.array_val);
    } else if (cmd == "DEL") {
        return handle_del(req.array_val);
    } else if (cmd == "EXISTS") {
        return handle_exists(req.array_val);
    } else if (cmd == "INCR") {
        return handle_incr(req.array_val);
    } else if (cmd == "KEYS") {
        return handle_keys(req.array_val);
    } else if (cmd == "COMMAND") {
        return RespValue::make_array({});
    }

    return RespValue::make_error("ERR unknown command '" + cmd + "'");
}

RespValue CommandHandler::handle_ping(const std::vector<RespValue>& args) {
    if (args.size() > 1) {
        return RespValue::make_bulk_string(args[1].string_val);
    }
    return RespValue::make_simple_string("PONG");
}

RespValue CommandHandler::handle_set(const std::vector<RespValue>& args) {
    if (args.size() < 3) {
        return RespValue::make_error("ERR wrong number of arguments for 'set' command");
    }
    store_.set(args[1].string_val, args[2].string_val);
    return RespValue::make_simple_string("OK");
}

RespValue CommandHandler::handle_get(const std::vector<RespValue>& args) {
    if (args.size() < 2) {
        return RespValue::make_error("ERR wrong number of arguments for 'get' command");
    }
    auto val = store_.get(args[1].string_val);
    if (val.has_value()) {
        return RespValue::make_bulk_string(*val);
    }
    return RespValue::make_null();
}

RespValue CommandHandler::handle_del(const std::vector<RespValue>& args) {
    if (args.size() < 2) {
        return RespValue::make_error("ERR wrong number of arguments for 'del' command");
    }
    int64_t count = 0;
    for (size_t i = 1; i < args.size(); ++i) {
        if (store_.del(args[i].string_val)) {
            count++;
        }
    }
    return RespValue::make_integer(count);
}

RespValue CommandHandler::handle_exists(const std::vector<RespValue>& args) {
    if (args.size() < 2) {
        return RespValue::make_error("ERR wrong number of arguments for 'exists' command");
    }
    int64_t count = 0;
    for (size_t i = 1; i < args.size(); ++i) {
        if (store_.exists(args[i].string_val)) {
            count++;
        }
    }
    return RespValue::make_integer(count);
}

RespValue CommandHandler::handle_incr(const std::vector<RespValue>& args) {
    if (args.size() < 2) {
        return RespValue::make_error("ERR wrong number of arguments for 'incr' command");
    }
    std::string key = args[1].string_val;
    auto val = store_.get(key);
    int64_t num = 0;
    if (val.has_value()) {
        try {
            num = std::stoll(*val);
        } catch (...) {
            return RespValue::make_error("ERR value is not an integer or out of range");
        }
    }
    num++;
    store_.set(key, std::to_string(num));
    return RespValue::make_integer(num);
}

RespValue CommandHandler::handle_keys(const std::vector<RespValue>& args) {
    if (args.size() < 2) {
        return RespValue::make_error("ERR wrong number of arguments for 'keys' command");
    }
    std::string pattern = args[1].string_val;
    auto all_keys = store_.keys();
    std::vector<RespValue> resp_keys;
    for (const auto& k : all_keys) {
        if (pattern == "*" || k == pattern) {
            resp_keys.push_back(RespValue::make_bulk_string(k));
        }
    }
    return RespValue::make_array(resp_keys);
}

} // namespace miniredis
