#pragma once

#include "datastructure/custom_hash_table.h"
#include "tx/transaction.h"

#include <mutex>
#include <vector>

namespace rbft {

class Mempool {
public:
    bool AddTransaction(const Transaction& tx, std::string& error);
    bool Exists(const std::string& tx_id) const;
    std::vector<Transaction> PickTransactions(size_t max_count) const;
    void RemoveCommitted(const std::vector<Transaction>& txs);
    std::vector<Transaction> Pending() const;
    size_t Size() const;

private:
    void RebuildIndex();

    mutable std::mutex mutex_;
    std::vector<Transaction> txs_;
    CustomHashTable<std::string, size_t> index_{32};
};

} // namespace rbft
