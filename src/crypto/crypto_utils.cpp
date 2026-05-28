#include "crypto/crypto_utils.h"

#include <sodium.h>

#include <stdexcept>

namespace rbft::crypto {

void Init() {
    if (sodium_init() < 0) {
        throw std::runtime_error("libsodium initialization failed");
    }
}

Hash Sha256(const std::vector<unsigned char>& data) {
    Init();
    Hash out{};
    crypto_hash_sha256(out.data(), data.data(), data.size());
    return out;
}

Hash Sha256String(const std::string& data) {
    return Sha256(std::vector<unsigned char>(data.begin(), data.end()));
}

std::string Sha256Hex(const std::string& data) {
    return HashToHex(Sha256String(data));
}

KeyPair GenerateEd25519KeyPair() {
    Init();
    std::array<unsigned char, crypto_sign_PUBLICKEYBYTES> pk{};
    std::array<unsigned char, crypto_sign_SECRETKEYBYTES> sk{};
    crypto_sign_keypair(pk.data(), sk.data());
    return {BytesToHex(pk.data(), pk.size()), BytesToHex(sk.data(), sk.size())};
}

std::string SignDetachedHex(const std::string& message, const std::string& private_key_hex) {
    Init();
    auto sk = HexToBytes(private_key_hex);
    if (sk.size() != crypto_sign_SECRETKEYBYTES) {
        throw std::invalid_argument("invalid Ed25519 private key size");
    }
    std::array<unsigned char, crypto_sign_BYTES> sig{};
    crypto_sign_detached(sig.data(), nullptr,
                         reinterpret_cast<const unsigned char*>(message.data()),
                         message.size(), sk.data());
    return BytesToHex(sig.data(), sig.size());
}

bool VerifyDetachedHex(const std::string& message, const std::string& signature_hex, const std::string& public_key_hex) {
    Init();
    auto sig = HexToBytes(signature_hex);
    auto pk = HexToBytes(public_key_hex);
    if (sig.size() != crypto_sign_BYTES || pk.size() != crypto_sign_PUBLICKEYBYTES) {
        return false;
    }
    return crypto_sign_verify_detached(sig.data(),
                                       reinterpret_cast<const unsigned char*>(message.data()),
                                       message.size(), pk.data()) == 0;
}

std::string PasswordHash(const std::string& password) {
    Init();
    char out[crypto_pwhash_STRBYTES]{};
    if (crypto_pwhash_str(out, password.c_str(), password.size(),
                          crypto_pwhash_OPSLIMIT_INTERACTIVE,
                          crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0) {
        throw std::runtime_error("password hash failed");
    }
    return std::string(out);
}

bool PasswordVerify(const std::string& hash, const std::string& password) {
    Init();
    return crypto_pwhash_str_verify(hash.c_str(), password.c_str(), password.size()) == 0;
}

std::string RandomTokenHex(size_t bytes) {
    Init();
    std::vector<unsigned char> buf(bytes);
    randombytes_buf(buf.data(), buf.size());
    return BytesToHex(buf.data(), buf.size());
}

} // namespace rbft::crypto
