#include <gtest/gtest.h>
#include "miniredis/resp.hpp"

using namespace miniredis;

TEST(RespTest, ParseSimpleString) {
    auto res = RespParser::parse("+OK\r\n");
    ASSERT_TRUE(res.has_value());
    EXPECT_EQ(res->value.type, RespType::SimpleString);
    EXPECT_EQ(res->value.string_val, "OK");
    EXPECT_EQ(res->bytes_consumed, 5);
}

TEST(RespTest, ParseError) {
    auto res = RespParser::parse("-ERR unknown command 'FOO'\r\n");
    ASSERT_TRUE(res.has_value());
    EXPECT_EQ(res->value.type, RespType::Error);
    EXPECT_EQ(res->value.string_val, "ERR unknown command 'FOO'");
}

TEST(RespTest, ParseInteger) {
    auto res = RespParser::parse(":1000\r\n");
    ASSERT_TRUE(res.has_value());
    EXPECT_EQ(res->value.type, RespType::Integer);
    EXPECT_EQ(res->value.int_val, 1000);
}

TEST(RespTest, ParseBulkString) {
    auto res = RespParser::parse("$5\r\nhello\r\n");
    ASSERT_TRUE(res.has_value());
    EXPECT_EQ(res->value.type, RespType::BulkString);
    EXPECT_EQ(res->value.string_val, "hello");
    EXPECT_EQ(res->bytes_consumed, 11);
}

TEST(RespTest, ParseNullBulkString) {
    auto res = RespParser::parse("$-1\r\n");
    ASSERT_TRUE(res.has_value());
    EXPECT_EQ(res->value.type, RespType::Null);
}

TEST(RespTest, ParseArray) {
    // *2\r\n$3\r\nGET\r\n$4\r\nname\r\n
    std::string input = "*2\r\n$3\r\nGET\r\n$4\r\nname\r\n";
    auto res = RespParser::parse(input);
    ASSERT_TRUE(res.has_value());
    EXPECT_EQ(res->value.type, RespType::Array);
    ASSERT_EQ(res->value.array_val.size(), 2);
    EXPECT_EQ(res->value.array_val[0].string_val, "GET");
    EXPECT_EQ(res->value.array_val[1].string_val, "name");
    EXPECT_EQ(res->bytes_consumed, input.size());
}

TEST(RespTest, PartialBufferReturnsNullopt) {
    // Incomplete payload for $5\r\nhello\r\n
    auto res1 = RespParser::parse("$5\r\nhel");
    EXPECT_FALSE(res1.has_value());

    // Incomplete array payload
    auto res2 = RespParser::parse("*2\r\n$3\r\nGET\r\n$4\r\nna");
    EXPECT_FALSE(res2.has_value());
}

TEST(RespTest, SerializeRoundtrip) {
    auto set_cmd = RespValue::make_array({
        RespValue::make_bulk_string("SET"),
        RespValue::make_bulk_string("mykey"),
        RespValue::make_bulk_string("myval")
    });

    std::string bytes = set_cmd.serialize();
    EXPECT_EQ(bytes, "*3\r\n$3\r\nSET\r\n$5\r\nmykey\r\n$5\r\nmyval\r\n");

    auto parsed = RespParser::parse(bytes);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->value.array_val.size(), 3);
    EXPECT_EQ(parsed->value.array_val[0].string_val, "SET");
    EXPECT_EQ(parsed->value.array_val[1].string_val, "mykey");
    EXPECT_EQ(parsed->value.array_val[2].string_val, "myval");
}

TEST(RespTest, InlineCommandParsing) {
    auto res = RespParser::parse("PING\r\n");
    ASSERT_TRUE(res.has_value());
    EXPECT_EQ(res->value.type, RespType::Array);
    ASSERT_EQ(res->value.array_val.size(), 1);
    EXPECT_EQ(res->value.array_val[0].string_val, "PING");
}
