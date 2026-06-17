// Merkle 树数据结构调试演示 —— 独立程序，不依赖网络/共识
#include "merkle/merkle_tree.h"
#include "crypto/crypto_utils.h"
#include <iostream>

static rbft::Transaction MakeTx(const std::string& data, int n) {
    auto kp = rbft::crypto::GenerateEd25519KeyPair();
    rbft::Transaction tx;
    tx.type = "STORE_DATA";
    tx.from = "demo";
    tx.data_hash = rbft::crypto::Sha256Hex(data + std::to_string(n));
    tx.nonce = static_cast<uint64_t>(n);
    tx.timestamp = 100 + n;
    tx.public_key_hex = kp.public_key_hex;
    tx.signature_hex = rbft::crypto::SignDetachedHex(rbft::SerializeTransactionBody(tx), kp.private_key_hex);
    tx.tx_id = rbft::ComputeTransactionId(tx);
    return tx;
}

int main() {
    // 3 笔交易 → 3 个叶子 → 完整 Merkle 树
    // 叶子层: [L1, L2, L3]
    // 内部层: [P12=ParentHash(L1,L2), P33=ParentHash(L3,L3)]  ← L3奇数复制
    // 根层:   [Root=ParentHash(P12, P33)]

    std::vector<rbft::Transaction> txs;
    txs.push_back(MakeTx("hello", 1));   // ★ 断点1: LeafHash(Tx1) → L1
    txs.push_back(MakeTx("world", 2));   // ★ 断点1: LeafHash(Tx2) → L2
    txs.push_back(MakeTx("merkle", 3));  // ★ 断点1: LeafHash(Tx3) → L3

    // ═══════════════════════════════════════════════════
    // 阶段1: 构建 Merkle 树
    // ═══════════════════════════════════════════════════
    std::cout << "\n========== 阶段1: 构建 Merkle 树 ==========\n" << std::endl;
    auto root = rbft::MerkleTree::ComputeRoot(txs);       // ★ line44 命中 3 次
    std::cout << "Merkle Root: " << rbft::HashToHex(root) << std::endl;

    // ═══════════════════════════════════════════════════
    // 阶段2: 生成 Merkle 证明
    // ═══════════════════════════════════════════════════
    std::cout << "\n========== 阶段2: 生成 Merkle 证明 ==========\n" << std::endl;
    auto proof = rbft::MerkleTree::GenerateProof(txs, 2); // ★ line44 再次命中 3 次 + line65 命中 2 次
    std::cout << "Proof size: " << proof.size() << std::endl;

    // ═══════════════════════════════════════════════════
    // 阶段3: 验证 Merkle 证明
    // ═══════════════════════════════════════════════════
    std::cout << "\n========== 阶段3: 验证 Merkle 证明 ==========\n" << std::endl;
    bool valid = rbft::MerkleTree::VerifyProof(txs[2], proof, root); // ★ line80 命中 2 次
    std::cout << "Verify: " << (valid ? "PASS" : "FAIL") << std::endl;
    return 0;
}
