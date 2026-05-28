#pragma once

#include "block/block.h"
#include "smt/sparse_merkle_tree.h"
#include "storage/sqlite_storage.h"

namespace rbft {

class BlockExecutor {
public:
    explicit BlockExecutor(SQLiteStorage* storage);
    std::string ExecuteForStateRoot(const std::vector<Transaction>& txs);
    void CommitBlock(const Block& block);

private:
    SQLiteStorage* storage_;
};

} // namespace rbft
