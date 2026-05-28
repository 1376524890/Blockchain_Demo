#include "block/block.h"

#include <gtest/gtest.h>

TEST(BlockTest, HashExcludesSelf) {
    rbft::BlockHeader h;
    h.chain_id = "c";
    h.height = 1;
    h.previous_block_hash = "0";
    h.tx_merkle_root = "m";
    h.state_root = "s";
    h.timestamp = 1;
    h.view = 0;
    h.instance_id = 0;
    h.proposer_id = "node1";
    auto a = rbft::ComputeBlockHash(h);
    h.block_hash = "different";
    auto b = rbft::ComputeBlockHash(h);
    EXPECT_EQ(a, b);
}
