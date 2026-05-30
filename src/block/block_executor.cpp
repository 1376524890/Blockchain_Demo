#include "block/block_executor.h"

#include "crypto/crypto_utils.h"
#include "user/account_state.h"

#include <stdexcept>

namespace rbft {

BlockExecutor::BlockExecutor(SQLiteStorage* storage) : storage_(storage) {}

void BlockExecutor::LoadSMTFromStorage() {
    if (smt_loaded_) return;
    auto leaves = storage_->GetAllSMTLeaves();
    for (const auto& [key, vh, value] : leaves) {
        smt_.LoadLeaf(key, vh, value);
    }
    smt_loaded_ = true;
}

std::string BlockExecutor::ExecuteForStateRoot(const std::vector<Transaction>& txs) {
    LoadSMTFromStorage();
    // 在 smt_ 的副本上执行，避免验证失败时污染持久化状态。
    SparseMerkleTree tmp = smt_;
    pending_changes_.clear();

    for (const auto& tx : txs) {
        if (!VerifyTransactionSignature(tx)) {
            throw std::runtime_error("invalid transaction signature");
        }
        if (tx.type == "TRANSFER") {
            auto from = storage_->GetAccount(tx.from).value_or(AccountState{tx.from, 0, 0});
            if (tx.nonce <= from.nonce) {
                throw std::runtime_error("replay nonce");
            }
            if (from.balance < tx.amount) {
                throw std::runtime_error("insufficient balance");
            }
            bool self_transfer = (tx.from == tx.to);
            from.balance -= tx.amount;
            from.nonce = tx.nonce;
            if (!self_transfer) {
                auto to = storage_->GetAccount(tx.to).value_or(AccountState{tx.to, 0, 0});
                to.balance += tx.amount;
                auto to_enc = EncodeAccountState(to);
                auto to_key = crypto::Sha256String(to.address);
                tmp.Update(to_key, to_enc);
                pending_changes_.push_back({to_key, crypto::Sha256(to_enc), std::move(to_enc)});
            }
            // 自转账时 from 余额不变 (扣了又加回=net 0)，仅消耗 nonce
            if (self_transfer) {
                from.balance += tx.amount;
            }
            auto from_enc = EncodeAccountState(from);
            auto from_key = crypto::Sha256String(from.address);
            tmp.Update(from_key, from_enc);
            pending_changes_.push_back({from_key, crypto::Sha256(from_enc), std::move(from_enc)});
        } else if (tx.type == "STORE_DATA") {
            auto from = storage_->GetAccount(tx.from).value_or(AccountState{tx.from, 0, 0});
            if (tx.nonce <= from.nonce) {
                throw std::runtime_error("replay nonce");
            }
            from.nonce = tx.nonce;
            auto from_enc = EncodeAccountState(from);
            auto from_key = crypto::Sha256String(from.address);
            tmp.Update(from_key, from_enc);
            pending_changes_.push_back({from_key, crypto::Sha256(from_enc), std::move(from_enc)});
        } else {
            throw std::runtime_error("unsupported transaction type");
        }
    }
    return HashToHex(tmp.GetRoot());
}

void BlockExecutor::CommitBlock(const Block& block) {
    LoadSMTFromStorage();
    storage_->Begin();
    try {
        storage_->PutBlock(block);
        for (size_t i = 0; i < block.transactions.size(); ++i) {
            const auto& tx = block.transactions[i];
            if (tx.type == "TRANSFER") {
                auto from = storage_->GetAccount(tx.from).value_or(AccountState{tx.from, 0, 0});
                // 使用有符号运算避免下溢（链同步时本地账户可能还没有初始余额，
                // 但区块已经过 peer 验证，余额结果是可信的）
                bool self_transfer = (tx.from == tx.to);
                if (!self_transfer) {
                    auto from_bal = static_cast<int64_t>(from.balance) - static_cast<int64_t>(tx.amount);
                    from.balance = from_bal >= 0 ? static_cast<uint64_t>(from_bal) : 0;
                }
                // 自转账: 余额不变，仅消耗 nonce
                from.nonce = tx.nonce;
                storage_->PutAccount(from, block.header.height);
                if (!self_transfer) {
                    auto to = storage_->GetAccount(tx.to).value_or(AccountState{tx.to, 0, 0});
                    to.balance += tx.amount;
                    storage_->PutAccount(to, block.header.height);
                }
            } else if (tx.type == "STORE_DATA") {
                auto from = storage_->GetAccount(tx.from).value_or(AccountState{tx.from, 0, 0});
                from.nonce = tx.nonce;
                storage_->PutAccount(from, block.header.height);
            }
            storage_->PutTransaction(tx, "COMMITTED", block.header.height, i);
        }
        // 持久化 SMT 变更和 state root
        for (const auto& ch : pending_changes_) {
            smt_.Update(ch.key, ch.value);
            storage_->PutSMTLeaf(ch.key, ch.value_hash, ch.value);
        }
        pending_changes_.clear();
        storage_->PutStateRoot(block.header.height, block.header.state_root);
        storage_->PutMetadata("latest_height", std::to_string(block.header.height));
        storage_->Commit();
    } catch (...) {
        storage_->Rollback();
        throw;
    }
}

void BlockExecutor::ResetSMT() {
    // 链重组时调用：清空内存 SMT 和 DB 中的 SMT 节点，下次 ExecuteForStateRoot 会从空树开始。
    smt_ = SparseMerkleTree();
    smt_loaded_ = true;  // 标记已加载（空树）
    pending_changes_.clear();
    storage_->ClearSMTLeaves();
}

} // namespace rbft
