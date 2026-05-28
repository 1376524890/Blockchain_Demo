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
    LoginResult Login(const std::string& username, const std::string& password);
    std::optional<UserRecord> GetByAddress(const std::string& address) const;

private:
    SQLiteStorage* storage_;
    CustomHashTable<std::string, int64_t> username_index_{64};
    CustomHashTable<std::string, int64_t> address_index_{64};
};

} // namespace rbft
