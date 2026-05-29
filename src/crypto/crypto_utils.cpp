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

// ── 对称加密: 密码派生密钥 + XSalsa20-Poly1305 ──

static std::array<unsigned char, crypto_secretbox_KEYBYTES>
DeriveKey(const std::string& password, const unsigned char* salt) {
    std::array<unsigned char, crypto_secretbox_KEYBYTES> key{};
    if (crypto_pwhash(key.data(), key.size(), password.c_str(), password.size(),
                      salt, crypto_pwhash_OPSLIMIT_INTERACTIVE,
                      crypto_pwhash_MEMLIMIT_INTERACTIVE, crypto_pwhash_ALG_ARGON2ID13) != 0) {
        throw std::runtime_error("密钥派生失败 (内存不足)");
    }
    return key;
}

std::string EncryptSecret(const std::string& plaintext, const std::string& password) {
    Init();
    // 1. 随机 salt + nonce
    unsigned char salt[crypto_pwhash_SALTBYTES];
    randombytes_buf(salt, sizeof(salt));
    unsigned char nonce[crypto_secretbox_NONCEBYTES];
    randombytes_buf(nonce, sizeof(nonce));
    // 2. 派生密钥
    auto key = DeriveKey(password, salt);
    // 3. 加密 (密文 = 明文 + MAC tag)
    std::vector<unsigned char> ciphertext(plaintext.size() + crypto_secretbox_MACBYTES);
    crypto_secretbox_easy(ciphertext.data(),
                          reinterpret_cast<const unsigned char*>(plaintext.data()),
                          plaintext.size(), nonce, key.data());
    // 4. 拼接: salt(16) + nonce(24) + ciphertext+tag
    std::vector<unsigned char> packed;
    packed.insert(packed.end(), salt, salt + sizeof(salt));
    packed.insert(packed.end(), nonce, nonce + sizeof(nonce));
    packed.insert(packed.end(), ciphertext.begin(), ciphertext.end());
    sodium_memzero(key.data(), key.size());
    return BytesToHex(packed.data(), packed.size());
}

std::string DecryptSecret(const std::string& encrypted_hex, const std::string& password) {
    Init();
    auto packed = HexToBytes(encrypted_hex);
    const size_t header = crypto_pwhash_SALTBYTES + crypto_secretbox_NONCEBYTES;
    if (packed.size() < header + crypto_secretbox_MACBYTES) {
        throw std::runtime_error("加密数据格式错误");
    }
    const unsigned char* salt = packed.data();
    const unsigned char* nonce = packed.data() + crypto_pwhash_SALTBYTES;
    const unsigned char* ct = packed.data() + header;
    const size_t ct_len = packed.size() - header;
    // 派生密钥
    auto key = DeriveKey(password, salt);
    // 解密
    std::vector<unsigned char> plaintext(ct_len - crypto_secretbox_MACBYTES);
    if (crypto_secretbox_open_easy(plaintext.data(), ct, ct_len, nonce, key.data()) != 0) {
        sodium_memzero(key.data(), key.size());
        throw std::runtime_error("解密失败: 密码错误或数据被篡改");
    }
    sodium_memzero(key.data(), key.size());
    return std::string(plaintext.begin(), plaintext.end());
}

} // namespace rbft::crypto
