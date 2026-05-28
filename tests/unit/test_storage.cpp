#include "storage/sqlite_storage.h"

#include <gtest/gtest.h>

TEST(StorageTest, AccountRoundTrip) {
    rbft::SQLiteStorage storage;
    storage.Open(":memory:");
    storage.InitializeSchema();
    storage.PutAccount(rbft::AccountState{"addr", 7, 2}, 1);
    auto account = storage.GetAccount("addr");
    ASSERT_TRUE(account.has_value());
    EXPECT_EQ(account->balance, 7u);
    EXPECT_EQ(account->nonce, 2u);
}
