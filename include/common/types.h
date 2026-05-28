#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace rbft {

using Hash = std::array<unsigned char, 32>;

Hash ZeroHash();
std::string BytesToHex(const unsigned char* data, size_t len);
std::vector<unsigned char> HexToBytes(const std::string& hex);
std::string HashToHex(const Hash& hash);
Hash HexToHash(const std::string& hex);
uint64_t NowMillis();

} // namespace rbft
