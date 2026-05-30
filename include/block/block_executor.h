#pragma once

#include "block/block.h"
#include "smt/sparse_merkle_tree.h"
#include "storage/sqlite_storage.h"

#include <vector>

namespace rbft {

class BlockExecutor {
public:
    explicit BlockExecutor(SQLiteStorage* storage);
    std::string ExecuteForStateRoot(const std::vector<Transaction>& txs);
    void CommitBlock(const Block& block);
    void ResetSMT();

private:
    void LoadSMTFromStorage();

    SQLiteStorage* storage_;
    SparseMerkleTree smt_;
    bool smt_loaded_{false};
    // ExecuteForStateRoot 暂存的变更，CommitBlock 时才真正写入 smt_ 和 DB。
    struct PendingChange { Hash key; Hash value_hash; std::vector<unsigned char> value; };
    std::vector<PendingChange> pending_changes_;
};

} // namespace rbft
