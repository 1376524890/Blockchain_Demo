# SQLite 数据库设计

## users

```sql
CREATE TABLE users (
    user_id INTEGER PRIMARY KEY AUTOINCREMENT,
    username TEXT UNIQUE NOT NULL,
    password_hash TEXT NOT NULL,
    address TEXT UNIQUE NOT NULL,
    public_key TEXT NOT NULL,
    private_key_encrypted TEXT,
    created_at INTEGER NOT NULL
);
```

## sessions

```sql
CREATE TABLE sessions (
    token TEXT PRIMARY KEY,
    user_id INTEGER NOT NULL,
    created_at INTEGER NOT NULL,
    expires_at INTEGER NOT NULL
);
```

## accounts

```sql
CREATE TABLE accounts (
    address TEXT PRIMARY KEY,
    balance INTEGER NOT NULL,
    nonce INTEGER NOT NULL,
    state_value BLOB NOT NULL,
    updated_height INTEGER NOT NULL
);
```

## transactions

```sql
CREATE TABLE transactions (
    tx_id TEXT PRIMARY KEY,
    type TEXT NOT NULL,
    sender TEXT NOT NULL,
    receiver TEXT,
    amount INTEGER,
    data_hash TEXT,
    nonce INTEGER NOT NULL,
    timestamp INTEGER NOT NULL,
    public_key TEXT NOT NULL,
    signature TEXT NOT NULL,
    block_height INTEGER,
    tx_index INTEGER,
    status TEXT NOT NULL,
    tx_json TEXT NOT NULL
);
```

## blocks

```sql
CREATE TABLE blocks (
    height INTEGER PRIMARY KEY,
    block_hash TEXT UNIQUE NOT NULL,
    previous_block_hash TEXT NOT NULL,
    tx_merkle_root TEXT NOT NULL,
    state_root TEXT NOT NULL,
    timestamp INTEGER NOT NULL,
    view INTEGER NOT NULL,
    instance_id INTEGER NOT NULL,
    proposer_id TEXT NOT NULL,
    block_json TEXT NOT NULL
);
```

## smt_nodes

```sql
CREATE TABLE smt_nodes (
    node_hash TEXT PRIMARY KEY,
    node_type TEXT NOT NULL,
    key_hash TEXT,
    value_hash TEXT,
    left_hash TEXT,
    right_hash TEXT,
    node_blob BLOB NOT NULL
);
```

## state_roots

```sql
CREATE TABLE state_roots (
    height INTEGER PRIMARY KEY,
    state_root TEXT NOT NULL
);
```

## consensus_logs

```sql
CREATE TABLE consensus_logs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    height INTEGER NOT NULL,
    view INTEGER NOT NULL,
    instance_id INTEGER NOT NULL,
    msg_type TEXT NOT NULL,
    block_hash TEXT NOT NULL,
    sender_id TEXT NOT NULL,
    message_json TEXT NOT NULL,
    created_at INTEGER NOT NULL
);
```

## byzantine_evidence

```sql
CREATE TABLE byzantine_evidence (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    evidence_type TEXT NOT NULL,
    height INTEGER NOT NULL,
    view INTEGER NOT NULL,
    instance_id INTEGER NOT NULL,
    sender_id TEXT NOT NULL,
    evidence_json TEXT NOT NULL,
    created_at INTEGER NOT NULL
);
```

## node_metadata

```sql
CREATE TABLE node_metadata (
    key TEXT PRIMARY KEY,
    value TEXT NOT NULL
);
```

## node_status

```sql
CREATE TABLE node_status (
    node_id TEXT PRIMARY KEY,
    latest_height INTEGER NOT NULL,
    current_view INTEGER NOT NULL,
    attack_mode TEXT NOT NULL,
    consensus_running INTEGER NOT NULL,
    updated_at INTEGER NOT NULL
);
```

## 事务边界

提交区块时使用一个事务写入：

1. `blocks`
2. 已提交 `transactions`
3. 被修改 `accounts`
4. `smt_nodes`
5. `state_roots`
6. `node_metadata` latest height

任何一步失败则回滚。
