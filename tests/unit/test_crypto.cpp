#include "crypto/crypto_utils.h"

#include <gtest/gtest.h>

TEST(CryptoTest, Sha256KnownVector) {
    auto hex = rbft::crypto::Sha256Hex("abc");
    EXPECT_EQ(hex, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TEST(CryptoTest, Ed25519SignVerify) {
    auto kp = rbft::crypto::GenerateEd25519KeyPair();
    auto sig = rbft::crypto::SignDetachedHex("message", kp.private_key_hex);
    EXPECT_TRUE(rbft::crypto::VerifyDetachedHex("message", sig, kp.public_key_hex));
    EXPECT_FALSE(rbft::crypto::VerifyDetachedHex("tampered", sig, kp.public_key_hex));
}

TEST(CryptoTest, PasswordHashVerify) {
    auto hash = rbft::crypto::PasswordHash("secret");
    EXPECT_TRUE(rbft::crypto::PasswordVerify(hash, "secret"));
    EXPECT_FALSE(rbft::crypto::PasswordVerify(hash, "bad"));
}

TEST(CryptoTest, RandomToken) {
    auto a = rbft::crypto::RandomTokenHex();
    auto b = rbft::crypto::RandomTokenHex();
    EXPECT_EQ(a.size(), 64u);
    EXPECT_NE(a, b);
}
