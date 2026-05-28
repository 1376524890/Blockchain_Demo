#pragma once

#include "common/types.h"

#include <string>
#include <vector>

namespace rbft::crypto {

struct KeyPair {
    std::string public_key_hex;
    std::string private_key_hex;
};

void Init();
Hash Sha256(const std::vector<unsigned char>& data);
Hash Sha256String(const std::string& data);
std::string Sha256Hex(const std::string& data);
KeyPair GenerateEd25519KeyPair();
std::string SignDetachedHex(const std::string& message, const std::string& private_key_hex);
bool VerifyDetachedHex(const std::string& message, const std::string& signature_hex, const std::string& public_key_hex);
std::string PasswordHash(const std::string& password);
bool PasswordVerify(const std::string& hash, const std::string& password);
std::string RandomTokenHex(size_t bytes = 32);

} // namespace rbft::crypto
