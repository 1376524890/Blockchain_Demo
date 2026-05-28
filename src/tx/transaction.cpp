#include "tx/transaction.h"

#include "crypto/crypto_utils.h"

#include <sstream>

namespace rbft {

std::string SerializeTransactionBody(const Transaction& tx) {
    // 交易签名体必须排除 tx_id 和 signature，字段顺序固定，保证所有节点计算结果一致。
    std::ostringstream os;
    os << tx.type << '|' << tx.from << '|' << tx.to << '|' << tx.amount << '|'
       << tx.data_hash << '|' << tx.nonce << '|' << tx.timestamp << '|'
       << tx.public_key_hex;
    return os.str();
}

std::string SerializeTransactionFull(const Transaction& tx) {
    return SerializeTransactionBody(tx) + "|" + tx.signature_hex;
}

std::string ComputeTransactionId(const Transaction& tx) {
    // tx_id 绑定签名结果，防止同一交易体被不同签名复用时产生相同 ID。
    return crypto::Sha256Hex(SerializeTransactionFull(tx));
}

bool VerifyTransactionSignature(const Transaction& tx) {
    if (tx.public_key_hex.empty() || tx.signature_hex.empty()) {
        return false;
    }
    return crypto::VerifyDetachedHex(SerializeTransactionBody(tx), tx.signature_hex, tx.public_key_hex);
}

nlohmann::json TransactionToJson(const Transaction& tx) {
    return {
        {"tx_id", tx.tx_id}, {"type", tx.type}, {"from", tx.from}, {"to", tx.to},
        {"amount", tx.amount}, {"data_hash", tx.data_hash}, {"nonce", tx.nonce},
        {"timestamp", tx.timestamp}, {"public_key", tx.public_key_hex},
        {"signature", tx.signature_hex}
    };
}

Transaction TransactionFromJson(const nlohmann::json& j) {
    Transaction tx;
    tx.tx_id = j.value("tx_id", "");
    tx.type = j.at("type").get<std::string>();
    tx.from = j.value("from", "");
    tx.to = j.value("to", "");
    tx.amount = j.value("amount", 0ULL);
    tx.data_hash = j.value("data_hash", "");
    tx.nonce = j.value("nonce", 0ULL);
    tx.timestamp = j.value("timestamp", 0ULL);
    tx.public_key_hex = j.value("public_key", j.value("public_key_hex", ""));
    tx.signature_hex = j.value("signature", j.value("signature_hex", ""));
    if (tx.tx_id.empty() && !tx.signature_hex.empty()) {
        tx.tx_id = ComputeTransactionId(tx);
    }
    return tx;
}

} // namespace rbft
