#include "tx/mempool.h"

namespace rbft {

bool Mempool::AddTransaction(const Transaction& tx, std::string& error) {
    std::lock_guard<std::mutex> lock(mutex_);
    // 交易池先做本地快速防护：去重、限长、验签。账户余额和严格 nonce 在执行阶段复核。
    if (tx.tx_id.empty()) {
        error = "empty tx_id";
        return false;
    }
    if (index_.Contains(tx.tx_id)) {
        error = "duplicate tx_id";
        return false;
    }
    if (SerializeTransactionFull(tx).size() > 16 * 1024) {
        error = "transaction too large";
        return false;
    }
    if (!VerifyTransactionSignature(tx)) {
        error = "invalid transaction signature";
        return false;
    }
    txs_.push_back(tx);
    index_.Put(tx.tx_id, txs_.size() - 1);
    return true;
}

bool Mempool::Exists(const std::string& tx_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return index_.Contains(tx_id);
}

std::vector<Transaction> Mempool::PickTransactions(size_t max_count) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const size_t count = std::min(max_count, txs_.size());
    return std::vector<Transaction>(txs_.begin(), txs_.begin() + static_cast<long>(count));
}

void Mempool::RemoveCommitted(const std::vector<Transaction>& txs) {
    std::lock_guard<std::mutex> lock(mutex_);
    // vector 删除会改变后续元素下标，因此提交删除后统一重建 tx_id -> index 索引。
    for (const auto& tx : txs) {
        for (auto it = txs_.begin(); it != txs_.end(); ++it) {
            if (it->tx_id == tx.tx_id) {
                txs_.erase(it);
                break;
            }
        }
    }
    RebuildIndex();
}

std::vector<Transaction> Mempool::Pending() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return txs_;
}

size_t Mempool::Size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return txs_.size();
}

void Mempool::RebuildIndex() {
    index_.Clear();
    for (size_t i = 0; i < txs_.size(); ++i) {
        index_.Put(txs_[i].tx_id, i);
    }
}

} // namespace rbft
