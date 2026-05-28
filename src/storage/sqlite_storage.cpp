#include "storage/sqlite_storage.h"

#include <nlohmann/json.hpp>

#include <stdexcept>

namespace rbft {

SQLiteStorage::~SQLiteStorage() {
    if (db_) {
        sqlite3_close(db_);
    }
}

void SQLiteStorage::Open(const std::string& path) {
    if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) {
        throw std::runtime_error("sqlite open failed: " + std::string(sqlite3_errmsg(db_)));
    }
}

void SQLiteStorage::Exec(const std::string& sql) const {
    char* err = nullptr;
    if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
        std::string msg = err ? err : "unknown sqlite error";
        sqlite3_free(err);
        throw std::runtime_error(msg);
    }
}

void SQLiteStorage::InitializeSchema() {
    // SQL 建表语句显式保留在代码中，便于演示和审计数据库结构。
    Exec("CREATE TABLE IF NOT EXISTS users (user_id INTEGER PRIMARY KEY AUTOINCREMENT, username TEXT UNIQUE NOT NULL, password_hash TEXT NOT NULL, address TEXT UNIQUE NOT NULL, public_key TEXT NOT NULL, private_key_encrypted TEXT, created_at INTEGER NOT NULL);");
    Exec("CREATE TABLE IF NOT EXISTS sessions (token TEXT PRIMARY KEY, user_id INTEGER NOT NULL, created_at INTEGER NOT NULL, expires_at INTEGER NOT NULL);");
    Exec("CREATE TABLE IF NOT EXISTS accounts (address TEXT PRIMARY KEY, balance INTEGER NOT NULL, nonce INTEGER NOT NULL, state_value BLOB NOT NULL, updated_height INTEGER NOT NULL);");
    Exec("CREATE TABLE IF NOT EXISTS transactions (tx_id TEXT PRIMARY KEY, type TEXT NOT NULL, sender TEXT NOT NULL, receiver TEXT, amount INTEGER, data_hash TEXT, nonce INTEGER NOT NULL, timestamp INTEGER NOT NULL, public_key TEXT NOT NULL, signature TEXT NOT NULL, block_height INTEGER, tx_index INTEGER, status TEXT NOT NULL, tx_json TEXT NOT NULL);");
    Exec("CREATE TABLE IF NOT EXISTS blocks (height INTEGER PRIMARY KEY, block_hash TEXT UNIQUE NOT NULL, previous_block_hash TEXT NOT NULL, tx_merkle_root TEXT NOT NULL, state_root TEXT NOT NULL, timestamp INTEGER NOT NULL, view INTEGER NOT NULL, instance_id INTEGER NOT NULL, proposer_id TEXT NOT NULL, block_json TEXT NOT NULL);");
    Exec("CREATE TABLE IF NOT EXISTS smt_nodes (node_hash TEXT PRIMARY KEY, node_type TEXT NOT NULL, key_hash TEXT, value_hash TEXT, left_hash TEXT, right_hash TEXT, node_blob BLOB NOT NULL);");
    Exec("CREATE TABLE IF NOT EXISTS state_roots (height INTEGER PRIMARY KEY, state_root TEXT NOT NULL);");
    Exec("CREATE TABLE IF NOT EXISTS consensus_logs (id INTEGER PRIMARY KEY AUTOINCREMENT, height INTEGER NOT NULL, view INTEGER NOT NULL, instance_id INTEGER NOT NULL, msg_type TEXT NOT NULL, block_hash TEXT NOT NULL, sender_id TEXT NOT NULL, message_json TEXT NOT NULL, created_at INTEGER NOT NULL);");
    Exec("CREATE TABLE IF NOT EXISTS byzantine_evidence (id INTEGER PRIMARY KEY AUTOINCREMENT, evidence_type TEXT NOT NULL, height INTEGER NOT NULL, view INTEGER NOT NULL, instance_id INTEGER NOT NULL, sender_id TEXT NOT NULL, evidence_json TEXT NOT NULL, created_at INTEGER NOT NULL);");
    Exec("CREATE TABLE IF NOT EXISTS node_metadata (key TEXT PRIMARY KEY, value TEXT NOT NULL);");
    Exec("CREATE TABLE IF NOT EXISTS node_status (node_id TEXT PRIMARY KEY, latest_height INTEGER NOT NULL, current_view INTEGER NOT NULL, attack_mode TEXT NOT NULL, consensus_running INTEGER NOT NULL, updated_at INTEGER NOT NULL);");
}

void SQLiteStorage::Begin() { Exec("BEGIN IMMEDIATE;"); }
void SQLiteStorage::Commit() { Exec("COMMIT;"); }
void SQLiteStorage::Rollback() { Exec("ROLLBACK;"); }

void SQLiteStorage::PutAccount(const AccountState& state, uint64_t height) {
    // 账户状态与 SMT state root 共同构成区块执行结果，提交区块时由上层事务包裹。
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "INSERT OR REPLACE INTO accounts(address,balance,nonce,state_value,updated_height) VALUES(?,?,?,?,?);";
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    auto encoded = EncodeAccountState(state);
    sqlite3_bind_text(stmt, 1, state.address.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, static_cast<sqlite3_int64>(state.balance));
    sqlite3_bind_int64(stmt, 3, static_cast<sqlite3_int64>(state.nonce));
    sqlite3_bind_blob(stmt, 4, encoded.data(), static_cast<int>(encoded.size()), SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 5, static_cast<sqlite3_int64>(height));
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("put account failed");
    }
    sqlite3_finalize(stmt);
}

std::optional<AccountState> SQLiteStorage::GetAccount(const std::string& address) const {
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, "SELECT balance,nonce FROM accounts WHERE address=?;", -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, address.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return std::nullopt;
    }
    AccountState state{address, static_cast<uint64_t>(sqlite3_column_int64(stmt, 0)), static_cast<uint64_t>(sqlite3_column_int64(stmt, 1))};
    sqlite3_finalize(stmt);
    return state;
}

void SQLiteStorage::PutTransaction(const Transaction& tx, const std::string& status, std::optional<uint64_t> height, std::optional<uint64_t> index) {
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "INSERT OR REPLACE INTO transactions(tx_id,type,sender,receiver,amount,data_hash,nonce,timestamp,public_key,signature,block_height,tx_index,status,tx_json) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?);";
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    auto json = TransactionToJson(tx).dump();
    sqlite3_bind_text(stmt, 1, tx.tx_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, tx.type.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, tx.from.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, tx.to.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 5, static_cast<sqlite3_int64>(tx.amount));
    sqlite3_bind_text(stmt, 6, tx.data_hash.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 7, static_cast<sqlite3_int64>(tx.nonce));
    sqlite3_bind_int64(stmt, 8, static_cast<sqlite3_int64>(tx.timestamp));
    sqlite3_bind_text(stmt, 9, tx.public_key_hex.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 10, tx.signature_hex.c_str(), -1, SQLITE_TRANSIENT);
    if (height) sqlite3_bind_int64(stmt, 11, static_cast<sqlite3_int64>(*height)); else sqlite3_bind_null(stmt, 11);
    if (index) sqlite3_bind_int64(stmt, 12, static_cast<sqlite3_int64>(*index)); else sqlite3_bind_null(stmt, 12);
    sqlite3_bind_text(stmt, 13, status.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 14, json.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("put transaction failed");
    }
    sqlite3_finalize(stmt);
}

std::optional<Transaction> SQLiteStorage::GetTransaction(const std::string& tx_id) const {
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, "SELECT tx_json FROM transactions WHERE tx_id=?;", -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, tx_id.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return std::nullopt;
    }
    auto tx = TransactionFromJson(nlohmann::json::parse(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0))));
    sqlite3_finalize(stmt);
    return tx;
}

std::optional<uint64_t> SQLiteStorage::GetTransactionBlockHeight(const std::string& tx_id) const {
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, "SELECT block_height FROM transactions WHERE tx_id=? AND block_height IS NOT NULL;", -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, tx_id.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return std::nullopt;
    }
    uint64_t height = static_cast<uint64_t>(sqlite3_column_int64(stmt, 0));
    sqlite3_finalize(stmt);
    return height;
}

void SQLiteStorage::PutBlock(const Block& block) {
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "INSERT OR REPLACE INTO blocks(height,block_hash,previous_block_hash,tx_merkle_root,state_root,timestamp,view,instance_id,proposer_id,block_json) VALUES(?,?,?,?,?,?,?,?,?,?);";
    sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    auto json = BlockToJson(block).dump();
    sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(block.header.height));
    sqlite3_bind_text(stmt, 2, block.header.block_hash.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, block.header.previous_block_hash.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, block.header.tx_merkle_root.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, block.header.state_root.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 6, static_cast<sqlite3_int64>(block.header.timestamp));
    sqlite3_bind_int64(stmt, 7, static_cast<sqlite3_int64>(block.header.view));
    sqlite3_bind_int(stmt, 8, static_cast<int>(block.header.instance_id));
    sqlite3_bind_text(stmt, 9, block.header.proposer_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 10, json.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("put block failed");
    }
    sqlite3_finalize(stmt);
}

std::optional<Block> SQLiteStorage::GetBlockByHeight(uint64_t height) const {
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, "SELECT block_json FROM blocks WHERE height=?;", -1, &stmt, nullptr);
    sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(height));
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return std::nullopt;
    }
    auto block = BlockFromJson(nlohmann::json::parse(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0))));
    sqlite3_finalize(stmt);
    return block;
}

std::optional<Block> SQLiteStorage::GetBlockByHash(const std::string& block_hash) const {
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, "SELECT block_json FROM blocks WHERE block_hash=?;", -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, block_hash.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return std::nullopt;
    }
    auto block = BlockFromJson(nlohmann::json::parse(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0))));
    sqlite3_finalize(stmt);
    return block;
}

std::optional<Block> SQLiteStorage::GetLatestBlock() const {
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, "SELECT block_json FROM blocks ORDER BY height DESC LIMIT 1;", -1, &stmt, nullptr);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return std::nullopt;
    }
    auto block = BlockFromJson(nlohmann::json::parse(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0))));
    sqlite3_finalize(stmt);
    return block;
}

void SQLiteStorage::PutMetadata(const std::string& key, const std::string& value) {
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, "INSERT OR REPLACE INTO node_metadata(key,value) VALUES(?,?);", -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, value.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("put metadata failed");
    }
    sqlite3_finalize(stmt);
}

std::string SQLiteStorage::GetMetadata(const std::string& key, const std::string& fallback) const {
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db_, "SELECT value FROM node_metadata WHERE key=?;", -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return fallback;
    }
    std::string value = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    sqlite3_finalize(stmt);
    return value;
}

} // namespace rbft
