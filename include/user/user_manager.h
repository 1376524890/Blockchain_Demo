#pragma once

#include "datastructure/custom_hash_table.h"
#include "storage/sqlite_storage.h"

#include <optional>
#include <string>

namespace rbft {

struct UserRecord {
    int64_t user_id{};
    std::string username;
    std::string address;
    std::string public_key_hex;
    std::string private_key_hex;
};

struct LoginResult {
    std::string token;
    UserRecord user;
};

class UserManager {
public:
    explicit UserManager(SQLiteStorage* storage);
    UserRecord Register(const std::string& username, const std::string& password);
    // 使用客户端提供的密钥注册 (多节点同步时用)
    UserRecord RegisterWithKey(const std::string& username, const std::string& password,
                               const std::string& address, const std::string& public_key_hex,
                               const std::string& private_key_hex);
    LoginResult Login(const std::string& username, const std::string& password);
    std::optional<UserRecord> GetByAddress(const std::string& address) const;

private:
    SQLiteStorage* storage_;
    CustomHashTable<std::string, int64_t> username_index_{64};
    CustomHashTable<std::string, int64_t> address_index_{64};
};

} // namespace rbft
