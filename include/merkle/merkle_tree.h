#pragma once

#include "common/types.h"
#include "tx/transaction.h"

#include <vector>

namespace rbft {

struct MerkleProofItem {
    enum class Position { LEFT, RIGHT };
    Position position;
    Hash sibling_hash;
};

class MerkleTree {
public:
    static Hash ComputeRoot(const std::vector<Transaction>& txs);
    static std::vector<std::vector<Hash>> BuildLevels(const std::vector<Transaction>& txs);
    static std::vector<MerkleProofItem> GenerateProof(const std::vector<Transaction>& txs, size_t tx_index);
    static bool VerifyProof(const Transaction& tx, const std::vector<MerkleProofItem>& proof, const Hash& expected_root);
    static Hash LeafHash(const Transaction& tx);
    static Hash ParentHash(const Hash& left, const Hash& right);
};

} // namespace rbft
