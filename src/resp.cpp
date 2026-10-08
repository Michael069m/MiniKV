#include "miniredis/resp.hpp"
#include <sstream>
#include <stdexcept>

namespace miniredis {

RespValue RespValue::make_simple_string(const std::string& str) {
    RespValue v;
    v.type = RespType::SimpleString;
    v.string_val = str;
    return v;
}

RespValue RespValue::make_error(const std::string& err) {
    RespValue v;
    v.type = RespType::Error;
    v.string_val = err;
    return v;
}

RespValue RespValue::make_integer(int64_t val) {
    RespValue v;
    v.type = RespType::Integer;
    v.int_val = val;
    return v;
}

RespValue RespValue::make_bulk_string(const std::string& str) {
    RespValue v;
    v.type = RespType::BulkString;
    v.string_val = str;
    return v;
}

RespValue RespValue::make_null() {
    RespValue v;
    v.type = RespType::Null;
    return v;
}

RespValue RespValue::make_array(const std::vector<RespValue>& arr) {
    RespValue v;
    v.type = RespType::Array;
    v.array_val = arr;
    return v;
}

std::string RespValue::serialize() const {
    switch (type) {
        case RespType::SimpleString:
            return "+" + string_val + "\r\n";
        case RespType::Error:
            return "-" + string_val + "\r\n";
        case RespType::Integer:
            return ":" + std::to_string(int_val) + "\r\n";
        case RespType::BulkString:
            return "$" + std::to_string(string_val.size()) + "\r\n" + string_val + "\r\n";
        case RespType::Null:
            return "$-1\r\n";
        case RespType::Array: {
            std::string out = "*" + std::to_string(array_val.size()) + "\r\n";
            for (const auto& elem : array_val) {
                out += elem.serialize();
            }
            return out;
        }
    }
    return "$-1\r\n";
}

static size_t find_crlf(std::string_view sv, size_t start = 0) {
    return sv.find("\r\n", start);
}

std::optional<RespParser::ParseResult> RespParser::parse(std::string_view buffer) {
    if (buffer.empty()) {
        return std::nullopt;
    }

    char prefix = buffer[0];
    size_t crlf_pos = find_crlf(buffer);

    switch (prefix) {
        case '+': { // Simple String
            if (crlf_pos == std::string_view::npos) return std::nullopt;
            std::string str(buffer.substr(1, crlf_pos - 1));
            return ParseResult{RespValue::make_simple_string(str), crlf_pos + 2};
        }
        case '-': { // Error
            if (crlf_pos == std::string_view::npos) return std::nullopt;
            std::string err(buffer.substr(1, crlf_pos - 1));
            return ParseResult{RespValue::make_error(err), crlf_pos + 2};
        }
        case ':': { // Integer
            if (crlf_pos == std::string_view::npos) return std::nullopt;
            std::string num_str(buffer.substr(1, crlf_pos - 1));
            try {
                int64_t val = std::stoll(num_str);
                return ParseResult{RespValue::make_integer(val), crlf_pos + 2};
            } catch (...) {
                return std::nullopt;
            }
        }
        case '$': { // Bulk String
            if (crlf_pos == std::string_view::npos) return std::nullopt;
            std::string len_str(buffer.substr(1, crlf_pos - 1));
            int len = 0;
            try {
                len = std::stoi(len_str);
            } catch (...) {
                return std::nullopt;
            }

            if (len == -1) {
                return ParseResult{RespValue::make_null(), crlf_pos + 2};
            }

            size_t total_needed = crlf_pos + 2 + static_cast<size_t>(len) + 2;
            if (buffer.size() < total_needed) {
                return std::nullopt; // Incomplete chunk
            }

            std::string payload(buffer.substr(crlf_pos + 2, len));
            return ParseResult{RespValue::make_bulk_string(payload), total_needed};
        }
        case '*': { // Array
            if (crlf_pos == std::string_view::npos) return std::nullopt;
            std::string count_str(buffer.substr(1, crlf_pos - 1));
            int count = 0;
            try {
                count = std::stoi(count_str);
            } catch (...) {
                return std::nullopt;
            }

            if (count == -1) {
                return ParseResult{RespValue::make_null(), crlf_pos + 2};
            }

            size_t current_offset = crlf_pos + 2;
            std::vector<RespValue> elements;
            elements.reserve(count);

            for (int i = 0; i < count; ++i) {
                std::string_view remaining = buffer.substr(current_offset);
                auto res = parse(remaining);
                if (!res.has_value()) {
                    return std::nullopt;
                }
                elements.push_back(res->value);
                current_offset += res->bytes_consumed;
            }

            return ParseResult{RespValue::make_array(elements), current_offset};
        }
        default:
            // Inline commands (e.g. raw PING\r\n) sent by simple clients/telnet
            if (crlf_pos != std::string_view::npos) {
                std::string line(buffer.substr(0, crlf_pos));
                std::istringstream iss(line);
                std::string token;
                std::vector<RespValue> elements;
                while (iss >> token) {
                    elements.push_back(RespValue::make_bulk_string(token));
                }
                if (!elements.empty()) {
                    return ParseResult{RespValue::make_array(elements), crlf_pos + 2};
                }
            }
            return std::nullopt;
    }
}

} // namespace miniredis
