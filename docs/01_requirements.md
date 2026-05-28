# 需求文档

## 项目背景

本项目是一个联盟链演示系统，用于在本地以 4 个进程模拟分布式节点，展示用户注册登录、交易提交、区块共识、交易 Merkle Proof、账户状态 Sparse Merkle Tree Proof 以及简化 RBFT 对拜占庭攻击的拒绝能力。系统定位为教学和演示，不作为生产链使用。

## 系统目标

- 使用 C++17、CMake、libsodium、SQLite3 实现可本地运行的联盟链节点。
- 通过 8001-8004 暴露 REST API，通过 9001-9004 暴露节点间 P2P API。
- 支持 4 节点、f=1、quorum=3 的简化 RBFT 共识。
- 手写 Merkle Tree、Sparse Merkle Tree、用户索引 HashTable、交易池索引 HashTable。
- 提供正常流程、攻击流程、单元测试、集成测试和运行脚本。

## 功能需求

- 用户注册：用户名密码注册，自动生成 Ed25519 公私钥，创建账户状态。
- 用户登录：密码哈希验证，生成 session token。
- 交易：支持 `TRANSFER` 和 `STORE_DATA`，交易签名体确定性序列化。
- 交易池：按提交顺序维护交易，拒绝重复 tx_id、非法签名、非法 nonce 和超大交易。
- 区块：包含交易 Merkle Root、账户 state_root、提交签名。
- 查询：交易、区块、账户状态、节点状态、共识状态。
- Proof：生成和验证交易 Merkle Proof、SMT existence proof、SMT non-existence proof。
- 共识：PRE_PREPARE、PREPARE、COMMIT 三阶段，检测冲突投票和双重提案。
- 攻击模式：支持错误 Merkle Root、错误 state_root、伪造签名、丢弃消息、延迟消息、双重提案、节点宕机模拟等。

## 非功能需求

- 本地一键编译、一键启动 4 节点、一键 demo、一键攻击测试。
- REST 响应统一为 `{ "ok": bool, "data": any, "error": string }`。
- 数据库写区块、交易、账户、SMT 节点时使用事务。
- 进程不得因非法网络输入、超过容错攻击或格式错误请求无保护崩溃。
- 日志必须能定位节点、端口、共识高度和攻击拒绝原因。

## 安全需求

- SHA256 使用 `crypto_hash_sha256`。
- 密码哈希使用 `crypto_pwhash_str` 和 `crypto_pwhash_str_verify`。
- 用户交易签名和节点共识消息签名使用 Ed25519。
- session token 使用 `randombytes_buf`。
- 所有签名和哈希输入必须使用确定性序列化。
- 真实系统不应通过 API 返回私钥；本演示可返回私钥以便前端发起签名交易，但文档和 API 响应字段标注为 demo-only。

## 演示需求

- 浏览器或前端可调用任一节点 REST API。
- 4 节点可在同机不同端口启动。
- demo 脚本能完成注册、登录、发起转账、查询区块、查询交易 proof、查询状态 proof。
- 攻击脚本输出明确 PASS/FAIL。

## 约束条件

- 语言固定 C++17。
- 构建固定 CMake。
- 数据库固定 SQLite3。
- REST 优先 cpp-httplib。
- JSON 使用 nlohmann/json。
- 测试使用 GoogleTest 和 Python requests/curl。

## 第三方库允许范围

- 允许：libsodium、SQLite3、cpp-httplib、nlohmann/json、GoogleTest、可选 spdlog。
- 不允许：第三方 Merkle Tree、第三方 SMT、第三方哈希表替代核心索引。

## 必须手写的数据结构范围

- `CustomHashTable<K,V>`：桶数组使用 `std::vector<std::vector<Entry>>`，手写 put/get/remove/rehash。
- 用户索引：username -> user_id、address -> user_id。
- 交易池索引：tx_id -> mempool index。
- Merkle Tree：层级使用 `std::vector<std::vector<Hash>>`。
- Sparse Merkle Tree：节点、默认哈希、更新、存在证明和不存在证明逻辑手写。
