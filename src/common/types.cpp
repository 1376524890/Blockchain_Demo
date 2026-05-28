#include "common/types.h"

#include <chrono>
#include <stdexcept>

namespace rbft {

Hash ZeroHash() {
    Hash h{};
    h.fill(0);
    return h;
}

static unsigned char FromHex(char c) {
    if (c >= '0' && c <= '9') return static_cast<unsigned char>(c - '0');
    if (c >= 'a' && c <= 'f') return static_cast<unsigned char>(c - 'a' + 10);
    if (c >= 'A' && c <= 'F') return static_cast<unsigned char>(c - 'A' + 10);
    throw std::invalid_argument("invalid hex character");
}

std::string BytesToHex(const unsigned char* data, size_t len) {
    static const char* table = "0123456789abcdef";
    std::string out;
    out.reserve(len * 2);
    for (size_t i = 0; i < len; ++i) {
        out.push_back(table[data[i] >> 4]);
        out.push_back(table[data[i] & 0x0f]);
    }
    return out;
}

std::vector<unsigned char> HexToBytes(const std::string& hex) {
    if (hex.size() % 2 != 0) {
        throw std::invalid_argument("hex length must be even");
    }
    std::vector<unsigned char> out;
    out.reserve(hex.size() / 2);
    for (size_t i = 0; i < hex.size(); i += 2) {
        out.push_back(static_cast<unsigned char>((FromHex(hex[i]) << 4) | FromHex(hex[i + 1])));
    }
    return out;
}

std::string HashToHex(const Hash& hash) {
    return BytesToHex(hash.data(), hash.size());
}

Hash HexToHash(const std::string& hex) {
    auto bytes = HexToBytes(hex);
    if (bytes.size() != 32) {
        throw std::invalid_argument("hash hex must be 32 bytes");
    }
    Hash h{};
    std::copy(bytes.begin(), bytes.end(), h.begin());
    return h;
}

uint64_t NowMillis() {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
}

} // namespace rbft
