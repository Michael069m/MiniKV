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

    if (cmd == "PING")   return handle_ping(req.array_val);
    if (cmd == "SET")    return handle_set(req.array_val);
    if (cmd == "GET")    return handle_get(req.array_val);
    if (cmd == "DEL")    return handle_del(req.array_val);
    if (cmd == "EXISTS") return handle_exists(req.array_val);
    if (cmd == "INCR")   return handle_incr(req.array_val);
    if (cmd == "KEYS")   return handle_keys(req.array_val);
    if (cmd == "EXPIRE") return handle_expire(req.array_val);
    if (cmd == "TTL")    return handle_ttl(req.array_val);
    if (cmd == "COMMAND") return RespValue::make_array({});

    return RespValue::make_error("ERR unknown command '" + cmd + "'");
}

RespValue CommandHandler::handle_ping(const std::vector<RespValue>& args) {
    if (args.size() > 1) return RespValue::make_bulk_string(args[1].string_val);
    return RespValue::make_simple_string("PONG");
}

RespValue CommandHandler::handle_set(const std::vector<RespValue>& args) {
    if (args.size() < 3) return RespValue::make_error("ERR wrong number of arguments for 'set'");
    
    std::string key = args[1].string_val;
    std::string val = args[2].string_val;
    std::optional<uint64_t> ttl_seconds = std::nullopt;

    if (args.size() >= 5) {
        std::string opt = args[3].string_val;
        std::transform(opt.begin(), opt.end(), opt.begin(), [](unsigned char c) { return std::toupper(c); });
        if (opt == "EX") {
            try {
                ttl_seconds = std::stoull(args[4].string_val);
            } catch (...) {
                return RespValue::make_error("ERR invalid expire time in 'set'");
            }
        }
    }

    store_.set(key, val, ttl_seconds);
    return RespValue::make_simple_string("OK");
}

RespValue CommandHandler::handle_get(const std::vector<RespValue>& args) {
    if (args.size() < 2) return RespValue::make_error("ERR wrong number of arguments for 'get'");
    auto val = store_.get(args[1].string_val);
    if (val.has_value()) return RespValue::make_bulk_string(*val);
    return RespValue::make_null();
}

RespValue CommandHandler::handle_del(const std::vector<RespValue>& args) {
    if (args.size() < 2) return RespValue::make_error("ERR wrong number of arguments for 'del'");
    int64_t count = 0;
    for (size_t i = 1; i < args.size(); ++i) {
        if (store_.del(args[i].string_val)) count++;
    }
    return RespValue::make_integer(count);
}

RespValue CommandHandler::handle_exists(const std::vector<RespValue>& args) {
    if (args.size() < 2) return RespValue::make_error("ERR wrong number of arguments for 'exists'");
    int64_t count = 0;
    for (size_t i = 1; i < args.size(); ++i) {
        if (store_.exists(args[i].string_val)) count++;
    }
    return RespValue::make_integer(count);
}

RespValue CommandHandler::handle_incr(const std::vector<RespValue>& args) {
    if (args.size() < 2) return RespValue::make_error("ERR wrong number of arguments for 'incr'");
    std::string key = args[1].string_val;
    auto val = store_.get(key);
    int64_t num = 0;
    if (val.has_value()) {
        try { num = std::stoll(*val); }
        catch (...) { return RespValue::make_error("ERR value is not an integer or out of range"); }
    }
    num++;
    store_.set(key, std::to_string(num));
    return RespValue::make_integer(num);
}

RespValue CommandHandler::handle_keys(const std::vector<RespValue>& args) {
    if (args.size() < 2) return RespValue::make_error("ERR wrong number of arguments for 'keys'");
    std::string pattern = args[1].string_val;
    auto all_keys = store_.keys();
    std::vector<RespValue> resp_keys;
    for (const auto& k : all_keys) {
        if (pattern == "*" || k == pattern) resp_keys.push_back(RespValue::make_bulk_string(k));
    }
    return RespValue::make_array(resp_keys);
}

RespValue CommandHandler::handle_expire(const std::vector<RespValue>& args) {
    if (args.size() < 3) return RespValue::make_error("ERR wrong number of arguments for 'expire'");
    std::string key = args[1].string_val;
    uint64_t seconds = 0;
    try {
        seconds = std::stoull(args[2].string_val);
    } catch (...) {
        return RespValue::make_error("ERR value is not an integer or out of range");
    }
    bool ok = store_.expire(key, seconds);
    return RespValue::make_integer(ok ? 1 : 0);
}

RespValue CommandHandler::handle_ttl(const std::vector<RespValue>& args) {
    if (args.size() < 2) return RespValue::make_error("ERR wrong number of arguments for 'ttl'");
    std::string key = args[1].string_val;
    int64_t remaining = store_.ttl(key);
    return RespValue::make_integer(remaining);
}

} // namespace miniredis
