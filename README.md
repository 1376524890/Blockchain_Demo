# RBFT Chain Demo

## Frontend Debug Demo APIs

The demo frontend uses REST/P2P routes on the REST ports only:

- node1: `http://localhost:8001`
- node2: `http://localhost:8002`
- node3: `http://localhost:8003`
- node4: `http://localhost:8004`

New visualization endpoints:

- `POST /api/debug/tx/serialize`
- `GET /api/debug/merkle/block/{height}`
- `GET /api/debug/merkle/tx/{tx_id}`
- `GET /api/debug/smt/{address}`
- `GET /api/debug/smt/{address}/non-existence`
- `GET /api/node/consensus/events?limit=100`
- `GET /api/debug/block/{height}/trace`

These routes keep the existing response envelope and are intended for algorithm debugging: tx signing body, hashes, signatures, Merkle proof/root recomputation, SMT proof/root recomputation, RBFT event timelines, and attack-mode demonstrations. The current P2P routes are served on the same REST port in this demo.

本项目是一个 C++17 联盟链演示系统，支持 4 个本地节点、REST API、SQLite 持久化、libsodium 密码学、手写 Merkle Tree、手写 Sparse Merkle Tree、手写 HashTable 和简化 RBFT 共识。

## 功能

- 用户注册、登录、自动生成 Ed25519 用户密钥。
- TRANSFER 和 STORE_DATA 交易。
- 交易池去重、签名校验、nonce 校验。
- 区块交易 Merkle Root 和 Merkle Proof。
- 账户状态 SMT Root、存在证明和不存在证明。
- 4 节点简化 RBFT：PRE_PREPARE、PREPARE、COMMIT、基础 view change。
- 攻击模式：错误 Merkle Root、错误 state_root、双重提案、伪造签名、丢弃/延迟消息、节点宕机模拟。

## 依赖

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake pkg-config libsodium-dev libsqlite3-dev python3 python3-requests curl
```

CMake 会拉取 cpp-httplib、nlohmann/json 和 GoogleTest。

## 编译

```bash
./scripts/build.sh
```

## 启动 4 节点

```bash
./scripts/init_db.sh
./scripts/gen_node_keys.sh
./scripts/start_4nodes.sh
```

端口：

- node1 REST 8001，P2P 9001。
- node2 REST 8002，P2P 9002。
- node3 REST 8003，P2P 9003。
- node4 REST 8004，P2P 9004。

## 运行演示

```bash
./scripts/demo_flow.sh
```

## 运行测试

```bash
./scripts/run_unit_tests.sh
./scripts/run_integration_tests.sh
./scripts/run_attack_tests.sh
```

## 停止节点

```bash
./scripts/stop_4nodes.sh
```

## 文档

- [需求文档](docs/01_requirements.md)
- [系统架构](docs/02_architecture.md)
- [数据结构设计](docs/03_data_structure_design.md)
- [Merkle Tree 设计](docs/04_merkle_tree_design.md)
- [SMT 设计](docs/05_smt_design.md)
- [RBFT 共识设计](docs/06_rbft_consensus_design.md)
- [API 设计](docs/07_api_design.md)
- [数据库设计](docs/08_database_design.md)
- [安全与攻击测试计划](docs/09_security_and_attack_test_plan.md)
- [部署指南](docs/10_deployment_guide.md)
- [Git 工作流](docs/11_git_workflow.md)

## 说明

这是教学演示系统。为了便于前端演示，注册和登录响应可返回 demo-only 私钥。真实系统不得通过 API 返回私钥，应使用本地 keystore 或硬件签名设备。
