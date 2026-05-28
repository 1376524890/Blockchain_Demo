# Merkle Tree 设计

## 用途

交易 Merkle Tree 用于证明某笔交易属于某个区块。区块头保存 `tx_merkle_root`，客户端通过交易和 proof 可独立验证包含性。

## 哈希类型

```cpp
using Hash = std::array<unsigned char, 32>;
```

## 叶子哈希规则

叶子哈希为：

```text
Hash(0x00 || deterministic_serialize(transaction))
```

交易序列化使用确定性字段顺序，避免 JSON 字段顺序影响 root。

## 内部节点哈希规则

内部节点为：

```text
Hash(0x01 || left_hash || right_hash)
```

`0x00` 和 `0x01` 前缀用于区分叶子与内部节点，避免二义性。

## 奇数节点处理

如果某一层节点数量为奇数，则复制最后一个节点作为右兄弟参与父节点计算。

## 空交易列表

空交易列表 root 使用全 0 的 `ZERO_HASH`。

## proof 生成

从叶子下标开始逐层向上：

- 当前节点为左节点，则 sibling 为右节点，位置记录为 `RIGHT`。
- 当前节点为右节点，则 sibling 为左节点，位置记录为 `LEFT`。
- 如果右节点不存在，则 sibling 为当前节点自身。

Proof 类型：

```cpp
struct MerkleProofItem {
    enum class Position { LEFT, RIGHT };
    Position position;
    Hash sibling_hash;
};
```

## proof 验证

验证端先计算交易叶子哈希，然后按 proof 顺序迭代：

- sibling 在 LEFT：`Hash(0x01 || sibling || current)`。
- sibling 在 RIGHT：`Hash(0x01 || current || sibling)`。

最终结果必须等于区块头 `tx_merkle_root`。

## 实现约束

- 不允许使用第三方 Merkle Tree 库。
- 使用 `std::vector<std::vector<Hash>>` 管理树层级。
- 单元测试覆盖 1 笔、2 笔、奇数笔、proof 成功、交易篡改失败、兄弟哈希篡改失败。
