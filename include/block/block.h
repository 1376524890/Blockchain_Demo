#pragma once

#include "tx/transaction.h"

#include <cstdint>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace rbft {

struct NodeSignature {
    std::string node_id;
    std::string public_key_hex;
    std::string signature_hex;
};

struct BlockHeader {
    std::string chain_id;
    uint64_t height{};
    std::string previous_block_hash;
    std::string tx_merkle_root;
    std::string state_root;
    uint64_t timestamp{};
    uint64_t view{};
    uint32_t instance_id{};
    std::string proposer_id;
    std::string block_hash;
};

struct Block {
    BlockHeader header;
    std::vector<Transaction> transactions;
    std::vector<NodeSignature> commit_signatures;
};

std::string SerializeBlockHeaderForHash(const BlockHeader& header);
std::string ComputeBlockHash(const BlockHeader& header);
nlohmann::json BlockToJson(const Block& block);
Block BlockFromJson(const nlohmann::json& j);

} // namespace rbft
