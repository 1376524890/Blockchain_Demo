#include "block/block.h"

#include "crypto/crypto_utils.h"

#include <sstream>

namespace rbft {

std::string SerializeBlockHeaderForHash(const BlockHeader& header) {
    // block_hash 不包含 block_hash 自身，也不包含 commit_signatures，保证提案阶段即可确定。
    std::ostringstream os;
    os << header.chain_id << '|' << header.height << '|' << header.previous_block_hash << '|'
       << header.tx_merkle_root << '|' << header.state_root << '|' << header.timestamp << '|'
       << header.view << '|' << header.instance_id << '|' << header.proposer_id;
    return os.str();
}

std::string ComputeBlockHash(const BlockHeader& header) {
    return crypto::Sha256Hex(SerializeBlockHeaderForHash(header));
}

nlohmann::json BlockToJson(const Block& block) {
    nlohmann::json txs = nlohmann::json::array();
    for (const auto& tx : block.transactions) {
        txs.push_back(TransactionToJson(tx));
    }
    nlohmann::json sigs = nlohmann::json::array();
    for (const auto& sig : block.commit_signatures) {
        sigs.push_back({{"node_id", sig.node_id}, {"public_key", sig.public_key_hex}, {"signature", sig.signature_hex}});
    }
    return {
        {"header", {
            {"chain_id", block.header.chain_id},
            {"height", block.header.height},
            {"previous_block_hash", block.header.previous_block_hash},
            {"tx_merkle_root", block.header.tx_merkle_root},
            {"state_root", block.header.state_root},
            {"timestamp", block.header.timestamp},
            {"view", block.header.view},
            {"instance_id", block.header.instance_id},
            {"proposer_id", block.header.proposer_id},
            {"block_hash", block.header.block_hash}
        }},
        {"transactions", txs},
        {"commit_signatures", sigs}
    };
}

Block BlockFromJson(const nlohmann::json& j) {
    Block b;
    const auto& h = j.at("header");
    b.header.chain_id = h.at("chain_id").get<std::string>();
    b.header.height = h.at("height").get<uint64_t>();
    b.header.previous_block_hash = h.at("previous_block_hash").get<std::string>();
    b.header.tx_merkle_root = h.at("tx_merkle_root").get<std::string>();
    b.header.state_root = h.at("state_root").get<std::string>();
    b.header.timestamp = h.at("timestamp").get<uint64_t>();
    b.header.view = h.at("view").get<uint64_t>();
    b.header.instance_id = h.at("instance_id").get<uint32_t>();
    b.header.proposer_id = h.at("proposer_id").get<std::string>();
    b.header.block_hash = h.at("block_hash").get<std::string>();
    for (const auto& tx : j.value("transactions", nlohmann::json::array())) {
        b.transactions.push_back(TransactionFromJson(tx));
    }
    return b;
}

} // namespace rbft
