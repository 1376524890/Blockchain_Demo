#include "block/block_executor.h"

#include "crypto/crypto_utils.h"
#include "user/account_state.h"

#include <stdexcept>

namespace rbft {

BlockExecutor::BlockExecutor(SQLiteStorage* storage) : storage_(storage) {}

std::string BlockExecutor::ExecuteForStateRoot(const std::vector<Transaction>& txs) {
    SparseMerkleTree smt;
    // 演示版按交易顺序重放账户状态，诚实节点投票前会独立执行并比较 state_root。
    for (const auto& tx : txs) {
        if (!VerifyTransactionSignature(tx)) {
            throw std::runtime_error("invalid transaction signature");
        }
        if (tx.type == "TRANSFER") {
            auto from = storage_->GetAccount(tx.from).value_or(AccountState{tx.from, 0, 0});
            auto to = storage_->GetAccount(tx.to).value_or(AccountState{tx.to, 0, 0});
            if (tx.nonce <= from.nonce) {
                throw std::runtime_error("replay nonce");
            }
            if (from.balance < tx.amount) {
                throw std::runtime_error("insufficient balance");
            }
            from.balance -= tx.amount;
            from.nonce = tx.nonce;
            to.balance += tx.amount;
            smt.Update(crypto::Sha256String(from.address), EncodeAccountState(from));
            smt.Update(crypto::Sha256String(to.address), EncodeAccountState(to));
        } else if (tx.type == "STORE_DATA") {
            auto from = storage_->GetAccount(tx.from).value_or(AccountState{tx.from, 0, 0});
            if (tx.nonce <= from.nonce) {
                throw std::runtime_error("replay nonce");
            }
            from.nonce = tx.nonce;
            smt.Update(crypto::Sha256String(from.address), EncodeAccountState(from));
        } else {
            throw std::runtime_error("unsupported transaction type");
        }
    }
    return HashToHex(smt.GetRoot());
}

void BlockExecutor::CommitBlock(const Block& block) {
    storage_->Begin();
    try {
        storage_->PutBlock(block);
        for (size_t i = 0; i < block.transactions.size(); ++i) {
            const auto& tx = block.transactions[i];
            if (tx.type == "TRANSFER") {
                auto from = storage_->GetAccount(tx.from).value_or(AccountState{tx.from, 0, 0});
                auto to = storage_->GetAccount(tx.to).value_or(AccountState{tx.to, 0, 0});
                from.balance -= tx.amount;
                from.nonce = tx.nonce;
                to.balance += tx.amount;
                storage_->PutAccount(from, block.header.height);
                storage_->PutAccount(to, block.header.height);
            } else if (tx.type == "STORE_DATA") {
                auto from = storage_->GetAccount(tx.from).value_or(AccountState{tx.from, 0, 0});
                from.nonce = tx.nonce;
                storage_->PutAccount(from, block.header.height);
            }
            storage_->PutTransaction(tx, "COMMITTED", block.header.height, i);
        }
        storage_->PutMetadata("latest_height", std::to_string(block.header.height));
        storage_->Commit();
    } catch (...) {
        storage_->Rollback();
        throw;
    }
}

} // namespace rbft
