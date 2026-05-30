#pragma once

#include "common/types.h"

#include <array>
#include <optional>
#include <vector>

namespace rbft {

enum class SMTNodeType { EMPTY, LEAF, INTERNAL };
enum class SMTProofType { EXISTENCE, NON_EXISTENCE };

struct SMTNode {
    SMTNodeType type{SMTNodeType::EMPTY};
    Hash hash{};
    Hash key{};
    Hash value_hash{};
    Hash left_hash{};
    Hash right_hash{};
};

struct SMTProof {
    SMTProofType type{SMTProofType::EXISTENCE};
    Hash key{};
    Hash value_hash{};
    std::vector<Hash> sibling_hashes;
    bool has_collision{false};
    Hash collision_leaf_key{};
    Hash collision_leaf_value_hash{};
};

class SparseMerkleTree {
public:
    SparseMerkleTree();

    Hash GetRoot() const;
    void Update(const Hash& key, const std::vector<unsigned char>& value);
    bool Get(const Hash& key, std::vector<unsigned char>& value_out) const;
    SMTProof GenerateExistenceProof(const Hash& key) const;
    SMTProof GenerateNonExistenceProof(const Hash& key) const;

    static bool VerifyExistenceProof(const Hash& root, const Hash& key, const std::vector<unsigned char>& value, const SMTProof& proof);
    static bool VerifyNonExistenceProof(const Hash& root, const Hash& key, const SMTProof& proof);
    static Hash LeafHash(const Hash& key, const Hash& value_hash);
    static Hash ParentHash(const Hash& left, const Hash& right);

    // 持久化支持：导出/加载叶子数据
    struct LeafData { Hash key; Hash value_hash; std::vector<unsigned char> value; };
    std::vector<LeafData> GetAllLeaves() const;
    void LoadLeaf(const Hash& key, const Hash& value_hash, const std::vector<unsigned char>& value);

private:
    struct Leaf {
        Hash key;
        Hash value_hash;
        std::vector<unsigned char> value;
    };

    static bool GetBit(const Hash& key, size_t depth);
    Hash ComputeRootForLeaves(const std::vector<Leaf>& leaves, size_t depth, std::vector<Hash>* proof, const Hash* proof_key) const;
    std::optional<size_t> FindLeaf(const Hash& key) const;
    Hash DefaultHash(size_t depth) const;

    std::vector<Leaf> leaves_;
    std::array<Hash, 257> default_hashes_{};
};

} // namespace rbft
