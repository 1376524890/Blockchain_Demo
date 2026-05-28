#include "user/account_state.h"

#include <sstream>

namespace rbft {

std::string SerializeAccountState(const AccountState& state) {
    std::ostringstream os;
    os << state.address << '|' << state.balance << '|' << state.nonce;
    return os.str();
}

std::vector<unsigned char> EncodeAccountState(const AccountState& state) {
    const auto s = SerializeAccountState(state);
    return std::vector<unsigned char>(s.begin(), s.end());
}

} // namespace rbft
