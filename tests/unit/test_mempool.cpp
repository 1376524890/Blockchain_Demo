#include "tx/mempool.h"
#include "crypto/crypto_utils.h"

#include <gtest/gtest.h>

static rbft::Transaction MakeSignedTx() {
    auto kp = rbft::crypto::GenerateEd25519KeyPair();
    rbft::Transaction tx;
    tx.type = "STORE_DATA";
    tx.from = "a";
    tx.data_hash = rbft::crypto::Sha256Hex("data");
    tx.nonce = 1;
    tx.timestamp = 100;
    tx.public_key_hex = kp.public_key_hex;
    tx.signature_hex = rbft::crypto::SignDetachedHex(rbft::SerializeTransactionBody(tx), kp.private_key_hex);
    tx.tx_id = rbft::ComputeTransactionId(tx);
    return tx;
}

TEST(MempoolTest, AddDeduplicateRemove) {
    rbft::Mempool pool;
    auto tx = MakeSignedTx();
    std::string error;
    EXPECT_TRUE(pool.AddTransaction(tx, error));
    EXPECT_FALSE(pool.AddTransaction(tx, error));
    EXPECT_EQ(pool.Size(), 1u);
    pool.RemoveCommitted({tx});
    EXPECT_EQ(pool.Size(), 0u);
}
