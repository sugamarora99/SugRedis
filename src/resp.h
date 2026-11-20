#ifndef RESP_PARSER
#define RESP_PARSER

#include <vector>
#include <string>
#include <cstdint>
#include <utility>

namespace SugRedis {

    /*
        https://github.com/redis/redis-specifications/blob/master/protocol/RESP2.md

        Clients send commands to a Redis server as a RESP Array of Bulk Strings.

        example: 
            GET foo becomes:
            *2\r\n          ← array with 2 elements
            $3\r\nGET\r\n    ← element 1: bulk string "GET"  (3 bytes)
            $3\r\nfoo\r\n    ← element 2: bulk string "foo"  (3 bytes)
    */

    constexpr size_t   kMaxBulkLen = 64 * 1024;
    constexpr uint32_t kMaxArgs    = 1024;

    enum class ParseResult{
        OK,
        NeedMoreData,
        Error
    };

    // Parse ASCII decimal digits into a uint32_t.
    // Returns true on success; false on empty, invalid, or out-of-range input.
    inline bool parseUint(const std::string& digits, uint32_t& out) {
        if (digits.empty()) return false;

        uint32_t value = 0;
        for (char c : digits) {
            if (c < '0' || c > '9') return false;                 // non-digit char
            uint32_t digit = static_cast<uint32_t>(c - '0');
            if (value > (UINT32_MAX - digit) / 10) return false;  // overflow guard
            value = value * 10 + digit;
        }
        out = value;
        return true;
    }

    // Reads one \r\n-terminated line from `buf`, starting at `*pos`.
    //
    // On success: copies the line's bytes (everything between *pos and the \r\n,
    // NOT including the \r\n itself) into `out`, advances *pos to just past the
    // \r\n, and returns true.
    //
    // On failure: the buffer doesn't yet contain a complete \r\n-terminated line,
    // so it leaves `out` and *pos untouched and returns false — the caller should
    // treat this as "need more data" and stop parsing until more bytes arrive.
    inline bool readLine(const std::vector<uint8_t>& buf, size_t& pos, std::string& out){
        size_t e = pos;
        while (e + 1 < buf.size()) {
            if (buf[e] == '\r' && buf[e+1] == '\n') {
                out.assign(buf.begin() + pos, buf.begin() + e);
                pos = e + 2;
                return true;
            }
            ++e;
        }
        return false;
    }

    // Parse one complete RESP command (an array of bulk strings) from `rawData`
    // starting at `pos`. On success advances `pos` past the command and fills
    // `out` with the command's arguments. On NeedMoreData/Error, leaves `pos`
    // and `out` untouched.
    inline ParseResult parseRawInput(const std::vector<uint8_t>& rawData, size_t& pos,
                                     std::vector<std::string>& out){
        size_t cursor = pos;
        std::string line;

        if (!readLine(rawData, cursor, line)) return ParseResult::NeedMoreData;
        if (line.empty() || line[0] != '*') return ParseResult::Error;
        uint32_t count = 0;
        if (!parseUint(line.substr(1), count)) return ParseResult::Error;
        if (count > kMaxArgs) return ParseResult::Error;

        std::vector<std::string> args;
        args.reserve(count);
        for (uint32_t i = 0; i < count; ++i) {
            if (!readLine(rawData, cursor, line)) return ParseResult::NeedMoreData;
            if (line.empty() || line[0] != '$') return ParseResult::Error;
            uint32_t len = 0;
            if (!parseUint(line.substr(1), len)) return ParseResult::Error;
            if (len > kMaxBulkLen) return ParseResult::Error;
            if (cursor + len + 2 > rawData.size()) return ParseResult::NeedMoreData;
            args.emplace_back(rawData.begin() + cursor, rawData.begin() + cursor + len);
            cursor += len + 2;
        }

        out = std::move(args);
        pos = cursor;
        return ParseResult::OK;
    }

    inline std::string respSimpleString(const std::string& s){ return "+" + s + "\r\n"; }
    inline std::string respError(const std::string& m)       { return "-" + m + "\r\n"; }
    inline std::string respInteger(long long n)              { return ":" + std::to_string(n) + "\r\n"; }
    inline std::string respBulkString(const std::string& s)  { return "$" + std::to_string(s.size()) + "\r\n" + s + "\r\n"; }
    inline std::string respNullBulk()                        { return "$-1\r\n"; }

};  // namespace SugRedis

#endif
