# 简化 RBFT 共识设计

## 边界

本系统实现演示用简化 RBFT，不实现生产级 checkpoint、稳定存储视图恢复、复杂流水线和动态成员变更。目标是展示 4 节点 f=1 的拜占庭容错提交、安全投票规则、基础 view change 和攻击拒绝。

## 参数

- n = 4。
- f = 1。
- quorum = 2f + 1 = 3。
- instance_count = f + 1 = 2。

## 字段含义

- height：正在共识的区块高度。
- view：主节点任期。
- instance_id：RBFT 并行实例编号。
- instance_id=0：master instance，负责正式提交账本。
- instance_id=1：backup instance，用于性能监控，不直接写账本。

主节点计算：

```text
primary(view, instance_id) = nodes[(view + instance_id) % n]
```

## 消息阶段

### PRE_PREPARE

master primary 从交易池取交易，生成候选区块，广播给其他节点。节点验证：

- proposer 是否为当前 primary。
- height/view/instance_id 是否匹配。
- previous_block_hash 是否匹配本地链头。
- 交易签名、nonce、交易大小。
- tx_merkle_root 是否等于本地重算值。
- state_root 是否等于本地执行交易后的重算值。
- block_hash 是否等于确定性区块头哈希。

### PREPARE

验证通过后节点广播 PREPARE。收到同一 block_hash 的 3 个 PREPARE 后广播 COMMIT。

### COMMIT

收到同一 block_hash 的 3 个 COMMIT 后提交区块。提交时写入 SQLite、更新账户、删除交易池已提交交易。

## 投票安全规则

- 同一节点在同一 `(height, view, instance_id)` 下不能对两个不同 block_hash 投 PREPARE。
- 同一节点在同一 `(height, view, instance_id)` 下不能对两个不同 block_hash 投 COMMIT。
- 如果收到同一 sender 的冲突投票，记录 ByzantineEvidence。
- master instance 才能写账本，backup instance 只记录性能和冲突观察。

## 基础 view change

触发条件：

- PRE_PREPARE 超时。
- primary 发送错误 Merkle Root 或 state_root。
- 检测到 primary 双重提案。
- 超过阈值的消息延迟或丢弃。

流程：

1. 节点广播 VIEW_CHANGE，包含当前 height、view、最高锁定 block_hash。
2. 收到 3 个 VIEW_CHANGE 后进入 `view + 1`。
3. 新 primary 发送 NEW_VIEW。
4. 新 view 下重新选择合法交易提案。

## 恶意主节点检测

- 同一 `(height, view, instance_id)` 收到不同 PRE_PREPARE，记录 double proposal。
- PRE_PREPARE 的 tx_merkle_root 与本地重算值不同，拒绝 PREPARE。
- PRE_PREPARE 的 state_root 与本地执行结果不同，拒绝 PREPARE。
- block_hash 与 header 重算不同，拒绝。
- 节点签名验证失败，丢弃消息并记录日志。

## 错误区块拒绝

错误区块不会进入 PREPARE quorum。若已有少数恶意 PREPARE/COMMIT，诚实节点因投票安全规则不会提交。超过 f 个异常时系统可以停止提交新区块，但不得提交错误区块。
