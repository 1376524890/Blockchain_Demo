# RBFT Chain Demo - 综合测试报告

**测试日期**: 2026-05-30  
**分支**: dev  
**测试环境**: 4节点 RBFT 网络 (f=1, 2f+1=3 quorum)

---

## 一、测试覆盖摘要

| 测试类别 | 测试项 | 结果 |
|---------|--------|------|
| 多节点同步 | 4节点P2P网络启动 | ✅ 通过 |
| 多节点同步 | 区块高度一致性 | ✅ 通过 (高度=13) |
| 多节点同步 | 区块哈希一致性 | ✅ 通过 (所有区块) |
| 多节点同步 | Merkle根一致性 | ✅ 通过 (所有区块) |
| 多节点同步 | P2P分叉修复(链重组) | ✅ 通过 |
| 共识机制 | 用户注册广播 | ✅ 通过 |
| 共识机制 | 转账交易共识 | ✅ 通过 |
| 认证机制 | Ed25519签名验证 | ✅ 通过 |
| 认证机制 | 密钥对生成/加解密 | ✅ 通过 |
| 认证机制 | 节点签名验证 | ✅ 通过 |
| 拜占庭容错 | 9种攻击模式测试 | ✅ 通过 |
| 拜占庭容错 | 恶意节点隔离 | ✅ 通过 |
| 拜占庭容错 | 交易回滚验证 | ✅ 通过 |
| CLI展示 | 菜单/横幅显示 | ✅ 通过 |
| CLI展示 | 节点状态总览 | ✅ 通过 |
| CLI展示 | 用户列表(分页) | ✅ 通过 |
| CLI展示 | 共识验证报告 | ✅ 通过 |
| CLI展示 | 不一致检测警告 | ✅ 通过 |

---

## 二、9种攻击模式测试详情

| # | 攻击模式 | 描述 | 攻击效果 | 防御结果 |
|---|---------|------|---------|---------|
| 1 | `bad_merkle_root` | 篡改Merkle根 | 区块被诚实节点拒绝 | ✅ 隔离 |
| 2 | `bad_state_root` | 篡改状态根 | 区块被诚实节点拒绝 | ✅ 隔离 |
| 3 | `invalid_block_hash` | 篡改区块哈希 | 区块被诚实节点拒绝 | ✅ 隔离 |
| 4 | `invalid_node_signature` | 无效节点签名 | 消息被拒绝 | ✅ 拒绝 |
| 5 | `drop_prepare` | 丢弃PREPARE | 无法影响quorum | ✅ 容错 |
| 6 | `drop_commit` | 丢弃COMMIT | 无法影响quorum | ✅ 容错 |
| 7 | `delay_preprepare` | 延迟PRE_PREPARE | 共识超时后view change | ✅ 恢复 |
| 8 | `double_proposal` | 双重提议 | 诚实节点检测并拒绝 | ✅ 检测 |
| 9 | `equivocation_prepare` | 矛盾PREPARE投票 | 被检测并隔离 | ✅ 隔离 |

### 关键验证: 恶意节点作为主节点

当node1(主节点)设置为`bad_merkle_root`模式时：
- node1产生篡改区块(height=12, hash=5efc02aa...)
- 诚实节点(node2/3/4)检测到篡改并拒绝
- 诚实节点产生正确区块(height=13, hash=016ba9...)
- node1被所有诚实节点隔离(quarantine)
- Merkle证明在恶意节点返回N/A

### 分叉修复验证

通过P2P `/p2p/sync/block` API同步正确区块后：
- node1成功从分叉链切换到诚实链
- 所有4个节点恢复到高度13，哈希一致
- 账户余额恢复一致 (alice=909, bob=1091)

---

## 三、共识机制验证

### RBFT三阶段共识
- **PRE_PREPARE**: 主节点提议区块 → ✅
- **PREPARE**: 备份节点投票 → ✅ (quorum=3 with f=1)
- **COMMIT**: 确认提交 → ✅
- **View Change**: 超时/恶意行为触发 → ✅

### 账户余额一致性
```
节点   alice余额   bob余额
node1     909       1091
node2     909       1091
node3     909       1091
node4     909       1091
```
初始余额1000 → 经多次转账后所有节点完全一致 ✅

### Merkle证明验证
所有13个区块的Merkle根在所有节点上完全一致 ✅

---

## 四、CLI可用性验证

### 显示功能
- ✅ ASCII艺术横幅 (青色RGB)
- ✅ 13项菜单 (中文，彩色)
- ✅ 节点连接状态检测
- ✅ 4节点端点显示

### 交互功能  
- ✅ 用户注册 (广播到所有节点)
- ✅ 用户登录 (本地钱包管理)
- ✅ 转账交易 (选择目标节点)
- ✅ 节点状态总览 (表格格式)
- ✅ 用户列表 (分页功能)
- ✅ 区块查询 (含哈希对比)
- ✅ Merkle证明对比
- ✅ 攻击模拟 (5种交互式模式)
- ✅ 多节点共识验证 (6项检查)

### 错误检测
- ✅ 区块不一致警告 ("⚠ 区块哈希不一致! 存在拜占庭节点")
- ✅ 高度不一致检测
- ✅ 余额不一致检测
- ✅ 节点离线检测

---

## 五、已知问题

1. **节点工作目录**: `rbft_node`从`build/`目录启动时，配置文件中的相对路径 (`data/node{}/node.key`) 解析失败。需从项目根目录启动，或使用绝对路径。

2. **CLI交易历史**: CLI的`recent_txs_`仅记录CLI会话中的交易，不查询链上历史。需通过API获取完整历史。

3. **攻击模式持久化**: 攻击模式存储在内存中，重启后恢复为normal。但恶意节点被隔离的状态在DB持久化，需手动`/api/admin/unquarantine`。

---

## 六、测试命令参考

```bash
# 启动4节点网络
cd /home/marktom/rbft_chain_demo
for i in 1 2 3 4; do
  nohup ./build/rbft_node --config "config/node${i}.json" \
    > "data/node${i}/node.log" 2>&1 &
done

# 运行CLI
./build/rbft_cli

# 运行共识验证(CLI命令12)

# 攻击模拟(CLI命令9)
# 支持模式: bad_merkle_root, bad_state_root, invalid_block_hash, 
#           drop_prepare, double_proposal

# 停止节点
for i in 1 2 3 4; do
  kill $(cat "data/node${i}/node.pid") 2>/dev/null
done
```
