#include "tx/transaction.h"
#include "crypto/crypto_utils.h"

#include <gtest/gtest.h>

TEST(TransactionTest, SignAndId) {
    auto kp = rbft::crypto::GenerateEd25519KeyPair();
    rbft::Transaction tx;
    tx.type = "TRANSFER";
    tx.from = "a";
    tx.to = "b";
    tx.amount = 10;
    tx.nonce = 1;
    tx.timestamp = 100;
    tx.public_key_hex = kp.public_key_hex;
    tx.signature_hex = rbft::crypto::SignDetachedHex(rbft::SerializeTransactionBody(tx), kp.private_key_hex);
    tx.tx_id = rbft::ComputeTransactionId(tx);
    EXPECT_TRUE(rbft::VerifyTransactionSignature(tx));
    EXPECT_FALSE(tx.tx_id.empty());
    tx.amount = 11;
    EXPECT_FALSE(rbft::VerifyTransactionSignature(tx));
}
