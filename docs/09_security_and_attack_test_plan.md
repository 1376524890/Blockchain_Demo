# 安全与攻击测试计划

## 正常流程

- 正常注册登录：注册 A/B，登录 A，校验 token 和地址。
- 正常转账：A 向 B 转账，签名正确，nonce 递增，余额更新。
- 正常存证：提交 STORE_DATA，data_hash 写入交易。
- 正常出块：4 节点达到 3 个 COMMIT 后提交。
- 查询交易证明：返回交易、区块高度、Merkle Proof。
- 查询状态证明：返回 A/B 账户状态和 SMT existence proof。
- 查询不存在地址证明：返回 SMT non-existence proof。

## 攻击测试

- 错误 Merkle Root：主节点篡改 `tx_merkle_root`，诚实节点重算失败，拒绝 PREPARE。
- 错误 state_root：主节点篡改 `state_root`，诚实节点独立执行失败，拒绝 PREPARE。
- 主节点双重提案：同一 height/view/instance_id 向不同节点发送不同 block_hash，记录 double proposal evidence，不提交冲突区块。
- 伪造用户签名：交易签名与 public_key 不匹配，拒绝进入 mempool。
- 伪造节点签名：共识消息签名验证失败，丢弃并记录日志。
- nonce 重放：重复提交旧 nonce，拒绝交易。
- 节点宕机攻击：一个节点 stop-consensus，剩余 3 节点仍可达到 quorum。
- 超过 f 个节点异常：两个节点停止或恶意，系统停止提交新区块但不提交错误区块。
- 大交易或垃圾交易：超过交易大小或 pending 限制，返回 429/400。
- 延迟消息和丢弃消息：delay_preprepare 触发 timeout 或 view change；drop_prepare/drop_commit 验证单节点异常仍安全。

## 输出规范

集成和攻击脚本输出：

```text
PASS: normal transfer committed
PASS: bad_merkle_root rejected
PASS: bad_state_root rejected
PASS: double_proposal detected
PASS: invalid_user_signature rejected
PASS: replay_nonce rejected
PASS: node_crash_simulated quorum maintained
PASS: exceed_fault_limit no unsafe commit
```

失败时输出：

```text
FAIL: <case> <reason>
```

## 验收判定

- 安全测试不要求超过 f 个异常时继续出块。
- 必须保证不会提交错误 Merkle Root、错误 state_root 或双重提案导致的冲突区块。
- 进程不得因恶意输入崩溃。
