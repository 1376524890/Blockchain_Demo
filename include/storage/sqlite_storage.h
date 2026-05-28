#pragma once

#include "block/block.h"
#include "user/account_state.h"

#include <optional>
#include <sqlite3.h>
#include <string>
#include <vector>

namespace rbft {

class SQLiteStorage {
public:
    SQLiteStorage() = default;
    ~SQLiteStorage();

    void Open(const std::string& path);
    void InitializeSchema();
    void Begin();
    void Commit();
    void Rollback();

    void PutAccount(const AccountState& state, uint64_t height);
    std::optional<AccountState> GetAccount(const std::string& address) const;
    void PutTransaction(const Transaction& tx, const std::string& status, std::optional<uint64_t> height, std::optional<uint64_t> index);
    std::optional<Transaction> GetTransaction(const std::string& tx_id) const;
    std::optional<uint64_t> GetTransactionBlockHeight(const std::string& tx_id) const;
    void PutBlock(const Block& block);
    std::optional<Block> GetBlockByHeight(uint64_t height) const;
    std::optional<Block> GetBlockByHash(const std::string& block_hash) const;
    std::optional<Block> GetLatestBlock() const;
    void PutMetadata(const std::string& key, const std::string& value);
    std::string GetMetadata(const std::string& key, const std::string& fallback = "") const;
    sqlite3* Raw() const { return db_; }

private:
    void Exec(const std::string& sql) const;
    sqlite3* db_{nullptr};
};

} // namespace rbft
