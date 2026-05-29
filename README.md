# RBFT Chain Demo

C++17 联盟链演示系统，支持 4 个本地节点、REST API、SQLite 持久化、libsodium 密码学、手写 Merkle Tree、手写 Sparse Merkle Tree、手写 HashTable 和简化 RBFT 共识。

## 功能

- 用户注册、登录、自动生成 Ed25519 用户密钥（私钥 XSalsa20 加密存储）。
- TRANSFER 和 STORE_DATA 交易。
- 交易池去重、签名校验、nonce 校验。
- 区块交易 Merkle Root 和 Merkle Proof。
- 账户状态 SMT Root、存在证明和不存在证明。
- 4 节点简化 RBFT：PRE_PREPARE、PREPARE、COMMIT、基础 view change。
- 攻击模式：错误 Merkle Root、错误 state_root、双重提案、伪造签名、丢弃/延迟消息、节点宕机模拟。
- 交互式命令行客户端（单节点模式 + 多节点 HTTP 客户端模式）。

---

## Linux 编译与运行

### 安装依赖

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake pkg-config libsodium-dev libsqlite3-dev python3 python3-requests curl
```

### 编译

```bash
./scripts/build.sh
```

### 启动 4 节点

```bash
./scripts/init_db.sh
./scripts/gen_node_keys.sh
./scripts/start_4nodes.sh
```

### 运行演示

```bash
./scripts/demo_flow.sh
```

### 运行测试

```bash
./scripts/run_unit_tests.sh
./scripts/run_integration_tests.sh
./scripts/run_attack_tests.sh
```

### 停止节点

```bash
./scripts/stop_4nodes.sh
```

---

## Windows 编译与运行

### 环境要求

- **Visual Studio 2022**（勾选"使用 C++ 的桌面开发"工作负载）
- **CMake 3.16+**（VS 自带，或从 [cmake.org](https://cmake.org/download/) 安装）
- **vcpkg**（C++ 包管理器）

### 1. 安装 vcpkg

```powershell
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
cd C:\vcpkg
bootstrap-vcpkg.bat
```

### 2. 安装依赖

```powershell
vcpkg install libsodium sqlite3
```

### 3. 编译

在项目根目录执行：

```powershell
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
```

编译产物位于 `build/Release/` 目录。

### 4. 生成节点密钥

```powershell
build\Release\rbft_node.exe --gen-node-key data\node1\node.key
build\Release\rbft_node.exe --gen-node-key data\node2\node.key
build\Release\rbft_node.exe --gen-node-key data\node3\node.key
build\Release\rbft_node.exe --gen-node-key data\node4\node.key
```

### 5. 初始化数据库

```powershell
build\Release\rbft_node.exe --config config\node1.json --init-db
build\Release\rbft_node.exe --config config\node2.json --init-db
build\Release\rbft_node.exe --config config\node3.json --init-db
build\Release\rbft_node.exe --config config\node4.json --init-db
```

### 6. 启动 4 个节点

打开 4 个 PowerShell 窗口，分别执行：

```powershell
# 窗口 1
build\Release\rbft_node.exe --config config\node1.json

# 窗口 2
build\Release\rbft_node.exe --config config\node2.json

# 窗口 3
build\Release\rbft_node.exe --config config\node3.json

# 窗口 4
build\Release\rbft_node.exe --config config\node4.json
```

### 7. 启动命令行客户端

再开一个 PowerShell 窗口：

```powershell
# 单节点交互模式
build\Release\rbft_cli.exe

# 多节点客户端模式（需先启动 4 个 rbft_node）
build\Release\rbft_cli.exe --nodes localhost:8001,8002,8003,8004
```

### 8. 清理数据重新开始

```powershell
Remove-Item -Recurse -Force data\node1, data\node2, data\node3, data\node4
```

---

## 端口

| 节点 | REST | P2P |
|------|------|-----|
| node1 | 8001 | 9001 |
| node2 | 8002 | 9002 |
| node3 | 8003 | 9003 |
| node4 | 8004 | 9004 |

## 命令行客户端

### 单节点模式（`rbft_cli` 旧版）

直接调用本地核心类，适合单机演示：

```bash
./build/rbft_cli                          # 默认 node1
./build/rbft_cli --config config/node2.json
```

### 多节点客户端模式（`rbft_cli` 新版）

通过 HTTP 连接多个 `rbft_node`，演示拜占庭容错：

```bash
# 先启动 4 个节点
bash scripts/multi_node_demo.sh

# 启动客户端
./build/rbft_cli
```

菜单功能：
1. 注册用户（广播到所有节点）
2. 用户登录
3. 转账交易（选择目标节点或广播）
4. 存储数据
5. 查询所有节点账户状态
6. 查询所有节点最新区块（对比一致性）
7. Merkle 证明对比
8. 所有节点状态总览
9. 攻击模拟（设置恶意节点 + 验证拜占庭容错）
10. 最近交易记录

### 调试日志

- 单节点模式：`data/node1/cli_debug.log`
- 多节点模式：`data/cli_client_debug.log`

---

## API 端点

### REST API

| 方法 | 路径 | 说明 |
|------|------|------|
| POST | `/api/users/register` | 注册用户 |
| POST | `/api/users/login` | 用户登录 |
| GET | `/api/users/{address}` | 查询用户 |
| POST | `/api/transactions/transfer` | 转账 |
| POST | `/api/transactions/store` | 存储数据 |
| GET | `/api/transactions/pending` | 待处理交易 |
| GET | `/api/transactions/{tx_id}` | 查询交易 |
| GET | `/api/blocks/latest` | 最新区块 |
| GET | `/api/blocks/{height}` | 按高度查区块 |
| GET | `/api/proofs/tx/{tx_id}` | Merkle 证明 |
| GET | `/api/state/{address}` | 账户状态 |
| GET | `/api/state/{address}/proof` | SMT 存在证明 |
| GET | `/api/node/status` | 节点状态 |
| POST | `/api/admin/attack-mode` | 设置攻击模式 |

### 调试 API

| 方法 | 路径 | 说明 |
|------|------|------|
| POST | `/api/debug/tx/serialize` | 交易序列化详情 |
| GET | `/api/debug/merkle/block/{height}` | Merkle 树完整结构 |
| GET | `/api/debug/merkle/tx/{tx_id}` | Merkle 证明重算步骤 |
| GET | `/api/debug/smt/{address}` | SMT 证明重算步骤 |
| GET | `/api/debug/block/{height}/trace` | 区块追踪 |
| GET | `/api/node/consensus/events` | 共识事件时间线 |

---

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
