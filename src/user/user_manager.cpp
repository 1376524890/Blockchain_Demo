#include "user/user_manager.h"

#include "common/types.h"
#include "crypto/crypto_utils.h"

#include <sqlite3.h>
#include <stdexcept>

namespace rbft {

UserManager::UserManager(SQLiteStorage* storage) : storage_(storage) {}

UserRecord UserManager::Register(const std::string& username, const std::string& password) {
    if (username.size() < 3 || password.size() < 6) {
        throw std::invalid_argument("username or password too short");
    }
    if (username_index_.Contains(username)) {
        throw std::runtime_error("username exists");
    }

    auto kp = crypto::GenerateEd25519KeyPair();
    const std::string address = crypto::Sha256Hex(kp.public_key_hex).substr(0, 40);
    const std::string password_hash = crypto::PasswordHash(password);
    const uint64_t now = NowMillis();

    // 用户注册必须在一个事务内同时写 users 与初始 accounts，避免索引和账户状态不一致。
    storage_->Begin();
    try {
        sqlite3_stmt* stmt = nullptr;
        const char* sql = "INSERT INTO users(username,password_hash,address,public_key,private_key_encrypted,created_at) VALUES(?,?,?,?,?,?);";
        sqlite3_prepare_v2(storage_->Raw(), sql, -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, password_hash.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, address.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, kp.public_key_hex.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 5, kp.private_key_hex.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 6, static_cast<sqlite3_int64>(now));
        if (sqlite3_step(stmt) != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            throw std::runtime_error("insert user failed");
        }
        int64_t user_id = sqlite3_last_insert_rowid(storage_->Raw());
        sqlite3_finalize(stmt);
        storage_->PutAccount(AccountState{address, 1000, 0}, 0);
        storage_->Commit();
        username_index_.Put(username, user_id);
        address_index_.Put(address, user_id);
        return UserRecord{user_id, username, address, kp.public_key_hex, kp.private_key_hex};
    } catch (...) {
        storage_->Rollback();
        throw;
    }
}

LoginResult UserManager::Login(const std::string& username, const std::string& password) {
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(storage_->Raw(), "SELECT user_id,password_hash,address,public_key,private_key_encrypted FROM users WHERE username=?;", -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("invalid credential");
    }
    const int64_t user_id = sqlite3_column_int64(stmt, 0);
    const std::string password_hash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    UserRecord user{user_id, username,
                    reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)),
                    reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3)),
                    reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4))};
    sqlite3_finalize(stmt);
    if (!crypto::PasswordVerify(password_hash, password)) {
        throw std::runtime_error("invalid credential");
    }
    const std::string token = crypto::RandomTokenHex();
    const uint64_t now = NowMillis();
    sqlite3_prepare_v2(storage_->Raw(), "INSERT OR REPLACE INTO sessions(token,user_id,created_at,expires_at) VALUES(?,?,?,?);", -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, token.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, user_id);
    sqlite3_bind_int64(stmt, 3, static_cast<sqlite3_int64>(now));
    sqlite3_bind_int64(stmt, 4, static_cast<sqlite3_int64>(now + 24ULL * 60 * 60 * 1000));
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("insert session failed");
    }
    sqlite3_finalize(stmt);
    return LoginResult{token, user};
}

std::optional<UserRecord> UserManager::GetByAddress(const std::string& address) const {
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(storage_->Raw(), "SELECT user_id,username,public_key,private_key_encrypted FROM users WHERE address=?;", -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, address.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return std::nullopt;
    }
    UserRecord user{sqlite3_column_int64(stmt, 0),
                    reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)),
                    address,
                    reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)),
                    reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3))};
    sqlite3_finalize(stmt);
    return user;
}

} // namespace rbft
