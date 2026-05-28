#include "block/block.h"
#include "common/config.h"
#include "consensus/consensus.h"
#include "crypto/crypto_utils.h"
#include "merkle/merkle_tree.h"
#include "smt/sparse_merkle_tree.h"
#include "tx/transaction.h"

#include <gtest/gtest.h>

namespace {

rbft::Transaction MakeTx(uint64_t nonce) {
    auto kp = rbft::crypto::GenerateEd25519KeyPair();
    rbft::Transaction tx;
    tx.type = "TRANSFER";
    tx.from = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    tx.to = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
    tx.amount = 10 + nonce;
    tx.data_hash = "";
    tx.nonce = nonce;
    tx.timestamp = 1710000000000ULL + nonce;
    tx.public_key_hex = kp.public_key_hex;
    tx.signature_hex = rbft::crypto::SignDetachedHex(rbft::SerializeTransactionBody(tx), kp.private_key_hex);
    tx.tx_id = rbft::ComputeTransactionId(tx);
    return tx;
}

} // namespace

TEST(DebugApiTest, test_debug_tx_serialize) {
    auto tx = MakeTx(1);
    const auto body = rbft::SerializeTransactionBody(tx);
    EXPECT_EQ(body, tx.type + "|" + tx.from + "|" + tx.to + "|11||1|1710000000001|" + tx.public_key_hex);
    EXPECT_EQ(rbft::crypto::Sha256Hex(body).size(), 64U);
}

TEST(DebugApiTest, test_debug_merkle_block) {
    std::vector<rbft::Transaction> txs{MakeTx(1), MakeTx(2), MakeTx(3)};
    rbft::Block block;
    block.transactions = txs;
    block.header.tx_merkle_root = rbft::HashToHex(rbft::MerkleTree::ComputeRoot(txs));
    auto levels = rbft::MerkleTree::BuildLevels(block.transactions);
    ASSERT_GE(levels.size(), 2U);
    EXPECT_EQ(rbft::HashToHex(levels.back().front()), block.header.tx_merkle_root);
}

TEST(DebugApiTest, test_debug_merkle_tx) {
    std::vector<rbft::Transaction> txs{MakeTx(1), MakeTx(2), MakeTx(3), MakeTx(4)};
    const auto expected = rbft::MerkleTree::ComputeRoot(txs);
    auto proof = rbft::MerkleTree::GenerateProof(txs, 2);
    rbft::Hash current = rbft::MerkleTree::LeafHash(txs[2]);
    for (const auto& item : proof) {
        current = item.position == rbft::MerkleProofItem::Position::LEFT
            ? rbft::MerkleTree::ParentHash(item.sibling_hash, current)
            : rbft::MerkleTree::ParentHash(current, item.sibling_hash);
    }
    EXPECT_EQ(current, expected);
}

TEST(DebugApiTest, test_debug_smt) {
    rbft::SparseMerkleTree smt;
    auto key = rbft::crypto::Sha256String("address-a");
    std::vector<unsigned char> value{'a', '|', '1', '0', '|', '1'};
    smt.Update(key, value);
    auto proof = smt.GenerateExistenceProof(key);
    EXPECT_TRUE(rbft::SparseMerkleTree::VerifyExistenceProof(smt.GetRoot(), key, value, proof));
}

TEST(DebugApiTest, test_consensus_events) {
    rbft::NodeConfig cfg;
    cfg.node_id = "node1";
    cfg.chain_id = "demo";
    cfg.f = 1;
    rbft::ConsensusEngine consensus(cfg);
    consensus.SetAttackMode(rbft::AttackMode::DOUBLE_PROPOSAL);
    EXPECT_EQ(consensus.RecentEvents(10).size(), 1U);

    rbft::ConsensusMessage msg;
    msg.type = "PREPARE";
    msg.chain_id = "demo";
    msg.height = 1;
    msg.view = 0;
    msg.instance_id = 0;
    msg.block_hash = std::string(64, 'a');
    msg.sender_id = "node2";
    std::string evidence;
    EXPECT_TRUE(consensus.RecordVote(msg, evidence));
    EXPECT_GE(consensus.RecentEvents(10).size(), 2U);
}
