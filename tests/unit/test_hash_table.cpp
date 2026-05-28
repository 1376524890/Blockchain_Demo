#include "datastructure/custom_hash_table.h"

#include <gtest/gtest.h>
#include <string>

TEST(CustomHashTableTest, PutGetContains) {
    rbft::CustomHashTable<std::string, int> table(2);
    table.Put("alice", 1);
    table.Put("bob", 2);
    int value = 0;
    EXPECT_TRUE(table.Get("alice", value));
    EXPECT_EQ(value, 1);
    EXPECT_TRUE(table.Contains("bob"));
    EXPECT_FALSE(table.Contains("carol"));
}

TEST(CustomHashTableTest, UpdateRemoveRehash) {
    rbft::CustomHashTable<int, std::string> table(2);
    for (int i = 0; i < 50; ++i) {
        table.Put(i, std::to_string(i));
    }
    table.Put(1, "one");
    EXPECT_GE(table.BucketCount(), 4u);
    auto v = table.Get(1);
    ASSERT_TRUE(v.has_value());
    EXPECT_EQ(*v, "one");
    EXPECT_TRUE(table.Remove(1));
    EXPECT_FALSE(table.Contains(1));
    EXPECT_EQ(table.Size(), 49u);
}
