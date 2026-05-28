# 数据结构设计

## 自定义 HashTable

`CustomHashTable<K,V>` 使用链地址法：

- 桶数组：`std::vector<std::vector<Entry>> buckets_`。
- Entry：`key`、`value`、`occupied` 由实现自行维护。
- 哈希：默认使用 `std::hash<K>` 得到桶下标，核心索引不使用 `std::unordered_map`。
- 冲突：同一桶内线性查找 key。
- 扩容：负载因子超过 0.75 时 rehash 到 2 倍桶数。
- API：`put`、`get`、`contains`、`remove`、`rehash`、`size`。

## 用户索引 HashTable

用户注册成功后建立两个索引：

- `username_index`: username -> user_id。
- `address_index`: address -> user_id。

索引只作为内存加速层，权威数据仍以 SQLite `users` 表为准。节点启动时从数据库重建索引。

## 交易池索引 HashTable

交易池以 `std::vector<Transaction>` 维护提交顺序，以 `CustomHashTable<std::string, size_t>` 维护 `tx_id -> index`。删除已提交交易后重建索引，避免 vector erase 导致旧下标失效。

## Merkle Tree 层级

Merkle Tree 使用 `std::vector<std::vector<Hash>>`：

- `levels[0]` 是叶子层。
- `levels[i + 1]` 是 `levels[i]` 两两哈希后的父层。
- 根为 `levels.back()[0]`。
- 奇数节点复制最后一个节点参与计算。

## SMT 节点结构

```cpp
enum class SMTNodeType { EMPTY, LEAF, INTERNAL };

struct SMTNode {
    SMTNodeType type;
    Hash hash;
    Hash key;
    Hash value_hash;
    Hash left_hash;
    Hash right_hash;
};
```

SMT 使用 256 bit key 路径。空子树用 `default_hashes[depth]` 表示。节点可持久化到 SQLite `smt_nodes`。

## 核心业务结构

### 用户

```cpp
struct User {
    int64_t user_id;
    std::string username;
    std::string password_hash;
    std::string address;
    std::string public_key_hex;
    std::string private_key_hex; // demo-only
    uint64_t created_at;
};
```

### 账户状态

```cpp
struct AccountState {
    std::string address;
    uint64_t balance;
    uint64_t nonce;
};
```

序列化规则：`address|balance|nonce`。

### 交易

```cpp
struct Transaction {
    std::string tx_id;
    std::string type;
    std::string from;
    std::string to;
    uint64_t amount;
    std::string data_hash;
    uint64_t nonce;
    uint64_t timestamp;
    std::string public_key_hex;
    std::string signature_hex;
};
```

签名体：`type|from|to|amount|data_hash|nonce|timestamp|public_key`。

### 区块

```cpp
struct BlockHeader {
    std::string chain_id;
    uint64_t height;
    std::string previous_block_hash;
    std::string tx_merkle_root;
    std::string state_root;
    uint64_t timestamp;
    uint64_t view;
    uint32_t instance_id;
    std::string proposer_id;
    std::string block_hash;
};
```

`block_hash` 不包含自身和 `commit_signatures`。

### 共识消息

```cpp
struct ConsensusMessage {
    std::string msg_id;
    std::string type;
    std::string chain_id;
    uint64_t height;
    uint64_t view;
    uint32_t instance_id;
    std::string block_hash;
    std::optional<Block> block;
    std::string sender_id;
    uint64_t timestamp;
    std::string signature_hex;
};
```

签名体：`type|chain_id|height|view|instance_id|block_hash|sender_id|timestamp`。
