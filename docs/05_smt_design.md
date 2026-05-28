# Sparse Merkle Tree 设计

## 用途

Sparse Merkle Tree 用于账户状态承诺和证明。区块头保存 `state_root`，客户端可验证账户存在或不存在。

## key 和 value

- key：`SHA256(address)`。
- value：`deterministic_encode(AccountState)`。
- `AccountState` 序列化：`address|balance|nonce`。
- value_hash：`SHA256(value)`。

## 节点类型

- EMPTY：空子树，不写完整节点，使用预计算 default hash 表示。
- LEAF：保存 `key` 和 `value_hash`。
- INTERNAL：保存 `left_hash` 和 `right_hash`。

## 哈希规则

- 叶子：`Hash(0x00 || key || value_hash)`。
- 内部：`Hash(0x01 || left_hash || right_hash)`。
- 空节点：`default_hashes[depth]`。

## default_hashes 预计算

`default_hashes[256]` 为叶子深度空值，通常为 `Hash("SMT_EMPTY_LEAF")` 或全 0 哈希的规范派生值。向上递推：

```text
default_hashes[d] = Hash(0x01 || default_hashes[d + 1] || default_hashes[d + 1])
```

根深度为 0，叶子深度为 256。

## Update

更新流程：

1. 根据 key 的每一 bit 从根走到叶子。
2. 如果遇到空节点，创建新叶子。
3. 如果遇到相同 key 的叶子，替换 value_hash。
4. 如果遇到不同 key 的叶子，按两 key 的分叉 bit 展开内部节点。
5. 自底向上重新计算内部节点 hash。
6. 将新节点写入 `StateNodeStore`，更新 `root_`。

## Get

从 root 按 key bit 查找：

- 到空节点：不存在。
- 到相同 key 的叶子：返回 store 中保存的 value。
- 到不同 key 的叶子：不存在。

## Existence Proof

存在证明包含：

- target key。
- value_hash。
- 从叶子到根方向的 sibling hashes。
- proof type = EXISTENCE。

验证时从 `Hash(0x00 || key || value_hash)` 开始，按 sibling 和 key bit 重算 root。

## Non-existence Proof

不存在证明分两类：

- 空路径证明：目标路径遇到空节点，proof 中记录该空节点深度和沿途 sibling。
- 冲突叶证明：目标路径遇到另一个 key 的叶子，proof 中记录 collision leaf key 和 collision leaf value_hash。

验证时重建空节点或冲突叶对应 hash，并向上计算 root，确认等于给定 root。

## SQLite 存储

`smt_nodes` 表按 `node_hash` 存储节点类型和节点内容：

- LEAF：`key_hash`、`value_hash`、`node_blob`。
- INTERNAL：`left_hash`、`right_hash`、`node_blob`。
- EMPTY：通常不落盘，使用 default hash。

`state_roots` 表保存每个高度的状态根。

## 实现约束

- 不允许使用第三方 SMT 库。
- default hash、update、proof、verify 均手写。
- 其他节点投票前必须独立执行交易并比较本地计算的 state root。
