#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace rbft {

struct AccountState {
    std::string address;
    uint64_t balance{};
    uint64_t nonce{};
};

std::string SerializeAccountState(const AccountState& state);
std::vector<unsigned char> EncodeAccountState(const AccountState& state);

} // namespace rbft
