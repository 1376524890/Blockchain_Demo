# 部署指南

## 依赖安装

Ubuntu/Debian：

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake pkg-config libsodium-dev libsqlite3-dev python3 python3-requests curl
```

项目通过 CMake FetchContent 获取 header-only 依赖和 GoogleTest：

- cpp-httplib
- nlohmann/json
- GoogleTest

如果无法访问 GitHub，可将对应头文件放入系统 include 路径或改用包管理器安装。

## 编译

```bash
cd rbft_chain_demo
./scripts/build.sh
```

## 初始化数据库

```bash
./scripts/init_db.sh
```

该脚本会为 `data/node1` 至 `data/node4` 创建目录并调用节点初始化表结构。

## 生成节点密钥

```bash
./scripts/gen_node_keys.sh
```

生成的 Ed25519 私钥保存在 `data/nodeX/node.key`，公钥写入配置文件或旁路 `.pub` 文件。

## 启动 4 节点

```bash
./scripts/start_4nodes.sh
```

日志：

- `data/node1/node.log`
- `data/node2/node.log`
- `data/node3/node.log`
- `data/node4/node.log`

PID：

- `data/nodeX/node.pid`

## 停止节点

```bash
./scripts/stop_4nodes.sh
```

## 运行 demo

```bash
./scripts/demo_flow.sh
```

## 运行测试

单元测试：

```bash
./scripts/run_unit_tests.sh
```

集成测试：

```bash
./scripts/run_integration_tests.sh
```

攻击测试：

```bash
./scripts/run_attack_tests.sh
```

## 常见错误排查

- `sodium.h not found`：安装 `libsodium-dev`。
- `sqlite3.h not found`：安装 `libsqlite3-dev`。
- 端口占用：执行 `./scripts/stop_4nodes.sh`，或检查 `lsof -i :8001`。
- Python 缺少 requests：执行 `python3 -m pip install requests`。
- FetchContent 下载失败：检查网络，或预先安装 nlohmann/json、cpp-httplib、GoogleTest。
- 节点高度不增长：检查主节点攻击模式、日志中的 PRE_PREPARE 拒绝原因和 quorum 计数。
