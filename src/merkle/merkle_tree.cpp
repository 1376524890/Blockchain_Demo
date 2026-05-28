#include "merkle/merkle_tree.h"

#include "crypto/crypto_utils.h"

#include <stdexcept>

namespace rbft {

Hash MerkleTree::LeafHash(const Transaction& tx) {
    // 0x00 域分离前缀用于区分叶子节点，避免叶子序列化与内部节点拼接产生二义性。
    std::vector<unsigned char> data;
    data.push_back(0x00);
    const auto s = SerializeTransactionFull(tx);
    data.insert(data.end(), s.begin(), s.end());
    return crypto::Sha256(data);
}

Hash MerkleTree::ParentHash(const Hash& left, const Hash& right) {
    // 0x01 域分离前缀用于区分内部节点，左右顺序必须保留。
    std::vector<unsigned char> data;
    data.reserve(65);
    data.push_back(0x01);
    data.insert(data.end(), left.begin(), left.end());
    data.insert(data.end(), right.begin(), right.end());
    return crypto::Sha256(data);
}

std::vector<std::vector<Hash>> MerkleTree::BuildLevels(const std::vector<Transaction>& txs) {
    std::vector<std::vector<Hash>> levels;
    if (txs.empty()) {
        levels.push_back({ZeroHash()});
        return levels;
    }
    levels.emplace_back();
    for (const auto& tx : txs) {
        levels[0].push_back(LeafHash(tx));
    }
    while (levels.back().size() > 1) {
        const auto& prev = levels.back();
        std::vector<Hash> next;
        for (size_t i = 0; i < prev.size(); i += 2) {
            const auto& left = prev[i];
            // 奇数节点复制最后一个节点，保证每层都能两两合并。
            const auto& right = (i + 1 < prev.size()) ? prev[i + 1] : prev[i];
            next.push_back(ParentHash(left, right));
        }
        levels.push_back(std::move(next));
    }
    return levels;
}

Hash MerkleTree::ComputeRoot(const std::vector<Transaction>& txs) {
    return BuildLevels(txs).back().front();
}

std::vector<MerkleProofItem> MerkleTree::GenerateProof(const std::vector<Transaction>& txs, size_t tx_index) {
    if (tx_index >= txs.size()) {
        throw std::out_of_range("transaction index out of range");
    }
    auto levels = BuildLevels(txs);
    std::vector<MerkleProofItem> proof;
    size_t idx = tx_index;
    for (size_t level = 0; level + 1 < levels.size(); ++level) {
        const auto& nodes = levels[level];
        const bool is_right = (idx % 2) == 1;
        size_t sibling = is_right ? idx - 1 : idx + 1;
        if (sibling >= nodes.size()) {
            // 证明路径也遵循奇数复制规则：缺失兄弟时 sibling 为自身。
            sibling = idx;
        }
        proof.push_back({is_right ? MerkleProofItem::Position::LEFT : MerkleProofItem::Position::RIGHT, nodes[sibling]});
        idx /= 2;
    }
    return proof;
}

bool MerkleTree::VerifyProof(const Transaction& tx, const std::vector<MerkleProofItem>& proof, const Hash& expected_root) {
    Hash current = LeafHash(tx);
    for (const auto& item : proof) {
        if (item.position == MerkleProofItem::Position::LEFT) {
            current = ParentHash(item.sibling_hash, current);
        } else {
            current = ParentHash(current, item.sibling_hash);
        }
    }
    return current == expected_root;
}

} // namespace rbft
