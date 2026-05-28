#include "smt/sparse_merkle_tree.h"
#include "crypto/crypto_utils.h"

#include <gtest/gtest.h>

TEST(SparseMerkleTreeTest, UpdateGetAndProofs) {
    rbft::SparseMerkleTree smt;
    auto key = rbft::crypto::Sha256String("address-a");
    std::vector<unsigned char> value{'v', '1'};
    smt.Update(key, value);
    std::vector<unsigned char> out;
    EXPECT_TRUE(smt.Get(key, out));
    EXPECT_EQ(out, value);
    auto root = smt.GetRoot();
    auto proof = smt.GenerateExistenceProof(key);
    EXPECT_TRUE(rbft::SparseMerkleTree::VerifyExistenceProof(root, key, value, proof));
    value[0] = 'x';
    EXPECT_FALSE(rbft::SparseMerkleTree::VerifyExistenceProof(root, key, value, proof));
}

TEST(SparseMerkleTreeTest, NonExistenceProof) {
    rbft::SparseMerkleTree smt;
    auto key = rbft::crypto::Sha256String("address-a");
    auto missing = rbft::crypto::Sha256String("address-b");
    smt.Update(key, std::vector<unsigned char>{'v'});
    auto proof = smt.GenerateNonExistenceProof(missing);
    EXPECT_TRUE(rbft::SparseMerkleTree::VerifyNonExistenceProof(smt.GetRoot(), missing, proof));
}
