#pragma once

#include <cstdint>
#include <string>

#include <nlohmann/json.hpp>

namespace rbft {

struct Transaction {
    std::string tx_id;
    std::string type;
    std::string from;
    std::string to;
    uint64_t amount{};
    std::string data_hash;
    uint64_t nonce{};
    uint64_t timestamp{};
    std::string public_key_hex;
    std::string signature_hex;
};

std::string SerializeTransactionBody(const Transaction& tx);
std::string SerializeTransactionFull(const Transaction& tx);
std::string ComputeTransactionId(const Transaction& tx);
bool VerifyTransactionSignature(const Transaction& tx);
nlohmann::json TransactionToJson(const Transaction& tx);
Transaction TransactionFromJson(const nlohmann::json& j);

} // namespace rbft
