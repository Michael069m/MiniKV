#pragma once

#include <string>
#include <vector>
#include <optional>
#include <string_view>
#include <cstdint>

namespace miniredis {

enum class RespType {
    SimpleString,
    Error,
    Integer,
    BulkString,
    Array,
    Null
};

struct RespValue {
    RespType type{RespType::Null};
    std::string string_val;
    int64_t int_val{0};
    std::vector<RespValue> array_val;

    static RespValue make_simple_string(const std::string& str);
    static RespValue make_error(const std::string& err);
    static RespValue make_integer(int64_t val);
    static RespValue make_bulk_string(const std::string& str);
    static RespValue make_null();
    static RespValue make_array(const std::vector<RespValue>& arr);

    std::string serialize() const;
};

class RespParser {
public:
    struct ParseResult {
        RespValue value;
        size_t bytes_consumed{0};
    };

    // Parses a single RESP command frame from input buffer.
    // Returns std::nullopt if buffer contains an incomplete frame.
    static std::optional<ParseResult> parse(std::string_view buffer);
};

} // namespace miniredis
