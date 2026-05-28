#include "user/user_manager.h"

#include <gtest/gtest.h>

TEST(UserManagerTest, RegisterLogin) {
    rbft::SQLiteStorage storage;
    storage.Open(":memory:");
    storage.InitializeSchema();
    rbft::UserManager users(&storage);
    auto user = users.Register("alice", "secret1");
    EXPECT_FALSE(user.address.empty());
    auto login = users.Login("alice", "secret1");
    EXPECT_EQ(login.user.address, user.address);
    EXPECT_FALSE(login.token.empty());
}
