#include "merkle/merkle_tree.h"
#include "crypto/crypto_utils.h"

#include <gtest/gtest.h>

static rbft::Transaction TxWithId(int n) {
    auto kp = rbft::crypto::GenerateEd25519KeyPair();
    rbft::Transaction tx;
    tx.type = "STORE_DATA";
    tx.from = "a";
    tx.data_hash = rbft::crypto::Sha256Hex("data" + std::to_string(n));
    tx.nonce = static_cast<uint64_t>(n);
    tx.timestamp = 100 + n;
    tx.public_key_hex = kp.public_key_hex;
    tx.signature_hex = rbft::crypto::SignDetachedHex(rbft::SerializeTransactionBody(tx), kp.private_key_hex);
    tx.tx_id = rbft::ComputeTransactionId(tx);
    return tx;
}

TEST(MerkleTreeTest, RootsAndProofs) {
    std::vector<rbft::Transaction> one{TxWithId(1)};
    EXPECT_EQ(rbft::MerkleTree::ComputeRoot(one), rbft::MerkleTree::LeafHash(one[0]));
    std::vector<rbft::Transaction> odd{TxWithId(1), TxWithId(2), TxWithId(3)};
    auto root = rbft::MerkleTree::ComputeRoot(odd);
    auto proof = rbft::MerkleTree::GenerateProof(odd, 2);
    EXPECT_TRUE(rbft::MerkleTree::VerifyProof(odd[2], proof, root));
    odd[2].data_hash = "bad";
    EXPECT_FALSE(rbft::MerkleTree::VerifyProof(odd[2], proof, root));
    proof[0].sibling_hash[0] ^= 0xff;
    odd[2] = TxWithId(3);
    EXPECT_FALSE(rbft::MerkleTree::VerifyProof(odd[2], proof, root));
}
