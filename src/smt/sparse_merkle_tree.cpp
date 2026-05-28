#include "smt/sparse_merkle_tree.h"

#include "crypto/crypto_utils.h"

#include <algorithm>

namespace rbft {

SparseMerkleTree::SparseMerkleTree() {
    // default_hashes[d] 表示深度 d 的全空子树根，避免显式存储 2^256 个空节点。
    default_hashes_[256] = crypto::Sha256String("SMT_EMPTY_LEAF");
    for (int i = 255; i >= 0; --i) {
        default_hashes_[static_cast<size_t>(i)] = ParentHash(default_hashes_[static_cast<size_t>(i + 1)],
                                                            default_hashes_[static_cast<size_t>(i + 1)]);
    }
}

Hash SparseMerkleTree::GetRoot() const {
    return ComputeRootForLeaves(leaves_, 0, nullptr, nullptr);
}

void SparseMerkleTree::Update(const Hash& key, const std::vector<unsigned char>& value) {
    // 演示实现用叶子集合重算根，接口和证明规则保持 SMT 语义；后续可替换为持久化路径更新。
    Hash value_hash = crypto::Sha256(value);
    auto idx = FindLeaf(key);
    if (idx) {
        leaves_[*idx].value_hash = value_hash;
        leaves_[*idx].value = value;
    } else {
        leaves_.push_back(Leaf{key, value_hash, value});
    }
}

bool SparseMerkleTree::Get(const Hash& key, std::vector<unsigned char>& value_out) const {
    auto idx = FindLeaf(key);
    if (!idx) {
        return false;
    }
    value_out = leaves_[*idx].value;
    return true;
}

SMTProof SparseMerkleTree::GenerateExistenceProof(const Hash& key) const {
    SMTProof proof;
    proof.type = SMTProofType::EXISTENCE;
    proof.key = key;
    auto idx = FindLeaf(key);
    if (idx) {
        proof.value_hash = leaves_[*idx].value_hash;
    }
    // sibling_hashes 按叶子到根保存，验证端可从叶子哈希逐层向上重算 root。
    std::vector<Leaf> current = leaves_;
    std::vector<Hash> top_down;
    for (size_t depth = 0; depth < 256; ++depth) {
        std::vector<Leaf> left;
        std::vector<Leaf> right;
        for (const auto& leaf : current) {
            (GetBit(leaf.key, depth) ? right : left).push_back(leaf);
        }
        const bool target_right = GetBit(key, depth);
        top_down.push_back(ComputeRootForLeaves(target_right ? left : right, depth + 1, nullptr, nullptr));
        current = target_right ? right : left;
    }
    proof.sibling_hashes.assign(top_down.rbegin(), top_down.rend());
    return proof;
}

SMTProof SparseMerkleTree::GenerateNonExistenceProof(const Hash& key) const {
    SMTProof proof;
    proof.type = SMTProofType::NON_EXISTENCE;
    proof.key = key;
    // 不存在证明支持两类：路径落空，或路径遇到不同 key 的冲突叶子。
    std::vector<Leaf> current = leaves_;
    std::vector<Hash> top_down;
    for (size_t depth = 0; depth < 256; ++depth) {
        std::vector<Leaf> left;
        std::vector<Leaf> right;
        for (const auto& leaf : current) {
            (GetBit(leaf.key, depth) ? right : left).push_back(leaf);
        }
        const bool target_right = GetBit(key, depth);
        top_down.push_back(ComputeRootForLeaves(target_right ? left : right, depth + 1, nullptr, nullptr));
        current = target_right ? right : left;
    }
    proof.sibling_hashes.assign(top_down.rbegin(), top_down.rend());
    for (const auto& leaf : leaves_) {
        if (leaf.key != key) {
            proof.has_collision = true;
            proof.collision_leaf_key = leaf.key;
            proof.collision_leaf_value_hash = leaf.value_hash;
            break;
        }
    }
    return proof;
}

bool SparseMerkleTree::VerifyExistenceProof(const Hash& root, const Hash& key, const std::vector<unsigned char>& value, const SMTProof& proof) {
    if (proof.type != SMTProofType::EXISTENCE) {
        return false;
    }
    // 从目标叶子开始，按 key 的路径方向和 sibling 列表逐层还原根。
    Hash current = LeafHash(key, crypto::Sha256(value));
    for (size_t i = 0; i < proof.sibling_hashes.size(); ++i) {
        const size_t depth = 255 - i;
        if (GetBit(key, depth)) {
            current = ParentHash(proof.sibling_hashes[i], current);
        } else {
            current = ParentHash(current, proof.sibling_hashes[i]);
        }
    }
    return current == root;
}

bool SparseMerkleTree::VerifyNonExistenceProof(const Hash& root, const Hash& key, const SMTProof& proof) {
    if (proof.type != SMTProofType::NON_EXISTENCE) {
        return false;
    }
    Hash current = proof.has_collision ? LeafHash(proof.collision_leaf_key, proof.collision_leaf_value_hash)
                                       : crypto::Sha256String("SMT_EMPTY_LEAF");
    for (size_t i = 0; i < proof.sibling_hashes.size(); ++i) {
        const size_t depth = 255 - i;
        if (GetBit(key, depth)) {
            current = ParentHash(proof.sibling_hashes[i], current);
        } else {
            current = ParentHash(current, proof.sibling_hashes[i]);
        }
    }
    return current == root && (!proof.has_collision || proof.collision_leaf_key != key);
}

Hash SparseMerkleTree::LeafHash(const Hash& key, const Hash& value_hash) {
    std::vector<unsigned char> data;
    data.reserve(65);
    data.push_back(0x00);
    data.insert(data.end(), key.begin(), key.end());
    data.insert(data.end(), value_hash.begin(), value_hash.end());
    return crypto::Sha256(data);
}

Hash SparseMerkleTree::ParentHash(const Hash& left, const Hash& right) {
    std::vector<unsigned char> data;
    data.reserve(65);
    data.push_back(0x01);
    data.insert(data.end(), left.begin(), left.end());
    data.insert(data.end(), right.begin(), right.end());
    return crypto::Sha256(data);
}

bool SparseMerkleTree::GetBit(const Hash& key, size_t depth) {
    // SMT 路径按 key 的高位到低位选择左右分支。
    const size_t byte_index = depth / 8;
    const size_t bit_index = 7 - (depth % 8);
    return ((key[byte_index] >> bit_index) & 1U) == 1U;
}

Hash SparseMerkleTree::ComputeRootForLeaves(const std::vector<Leaf>& leaves, size_t depth, std::vector<Hash>* proof, const Hash* proof_key) const {
    if (leaves.empty()) {
        return DefaultHash(depth);
    }
    if (depth == 256 || leaves.size() == 1) {
        return LeafHash(leaves.front().key, leaves.front().value_hash);
    }

    std::vector<Leaf> left;
    std::vector<Leaf> right;
    for (const auto& leaf : leaves) {
        (GetBit(leaf.key, depth) ? right : left).push_back(leaf);
    }
    Hash left_hash = ComputeRootForLeaves(left, depth + 1, proof, proof_key);
    Hash right_hash = ComputeRootForLeaves(right, depth + 1, proof, proof_key);
    (void)proof;
    (void)proof_key;
    return ParentHash(left_hash, right_hash);
}

std::optional<size_t> SparseMerkleTree::FindLeaf(const Hash& key) const {
    for (size_t i = 0; i < leaves_.size(); ++i) {
        if (leaves_[i].key == key) {
            return i;
        }
    }
    return std::nullopt;
}

Hash SparseMerkleTree::DefaultHash(size_t depth) const {
    return default_hashes_[std::min(depth, static_cast<size_t>(256))];
}

} // namespace rbft
