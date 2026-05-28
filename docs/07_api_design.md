# REST API 设计

## 统一响应

成功：

```json
{ "ok": true, "data": {}, "error": "" }
```

失败：

```json
{ "ok": false, "data": null, "error": "reason" }
```

常用错误码：`400` 请求格式错误，`401` 未认证，`404` 资源不存在，`409` 状态冲突，`429` 限流，`500` 内部错误。

## 用户 API

### POST /api/users/register

请求：

```json
{ "username": "alice", "password": "pass" }
```

响应：

```json
{ "user_id": 1, "address": "hex", "public_key": "hex", "private_key": "hex-demo-only" }
```

错误：`409 username exists`。

### POST /api/users/login

请求：

```json
{ "username": "alice", "password": "pass" }
```

响应：

```json
{ "token": "hex", "address": "hex", "public_key": "hex", "private_key": "hex-demo-only" }
```

错误：`401 invalid credential`。

### GET /api/users/{address}

响应：用户公开信息和账户状态，不返回密码哈希。

## 交易 API

### POST /api/transactions/transfer

请求：

```json
{
  "from": "addrA",
  "to": "addrB",
  "amount": 10,
  "nonce": 1,
  "public_key": "hex",
  "signature": "hex",
  "timestamp": 1710000000
}
```

响应：`{ "tx_id": "hex", "status": "PENDING" }`。

### POST /api/transactions/store

请求：

```json
{
  "from": "addrA",
  "data_hash": "sha256hex",
  "nonce": 2,
  "public_key": "hex",
  "signature": "hex",
  "timestamp": 1710000000
}
```

响应：`{ "tx_id": "hex", "status": "PENDING" }`。

### GET /api/transactions/{tx_id}

响应：交易 JSON、状态、区块高度、交易下标。

### GET /api/transactions/pending

响应：当前 mempool 交易列表。

## 区块 API

- `GET /api/blocks/latest`：返回最高区块。
- `GET /api/blocks/{height}`：按高度返回区块。
- `GET /api/blocks/hash/{block_hash}`：按哈希返回区块。

## 交易证明 API

### GET /api/proofs/tx/{tx_id}

响应：

```json
{ "tx": {}, "block_height": 1, "root": "hex", "proof": [{ "position": "LEFT", "hash": "hex" }] }
```

### POST /api/proofs/tx/verify

请求包含 `tx`、`root`、`proof`，响应 `{ "valid": true }`。

## 状态 API

- `GET /api/state/{address}`：返回账户状态。
- `GET /api/state/{address}/proof`：返回 existence proof。
- `GET /api/state/{address}/non-existence-proof`：返回 non-existence proof。
- `POST /api/state/proof/verify`：验证存在证明。
- `POST /api/state/non-existence-proof/verify`：验证不存在证明。

## 节点 API

- `GET /api/node/status`：节点 id、端口、高度、view、attack_mode、consensus_running。
- `GET /api/node/peers`：peer 列表。
- `GET /api/node/consensus`：当前 height、view、prepare/commit 计数和 evidence 计数。
- `POST /api/admin/attack-mode`：设置攻击模式。
- `POST /api/admin/stop-consensus`：停止共识循环。
- `POST /api/admin/start-consensus`：恢复共识循环。

攻击模式请求：

```json
{ "mode": "bad_merkle_root" }
```

## P2P API

- `POST /p2p/consensus/message`：接收共识消息。
- `POST /p2p/transaction/broadcast`：接收交易广播。
- `GET /p2p/status`：节点健康状态。
- `GET /p2p/block/{height}`：区块同步。
