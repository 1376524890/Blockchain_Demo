#!/usr/bin/env bash
# ──────────────────────────────────────────────────────────────────────────────
# reset_and_start.sh — 清空全部用户/钱包/数据，重建并启动4节点RBFT网络
# ──────────────────────────────────────────────────────────────────────────────
set -euo pipefail
cd "$(dirname "$0")/.."

MODE="${1:-full}"   # full | clean | start

echo "=========================================="
echo "  RBFT Chain Demo — 重置 & 启动"
echo "=========================================="

# ── 停止所有节点 ──────────────────────────────────────────────────────────
stop_nodes() {
    echo ""
    echo "[*] 停止所有节点..."
    for i in 1 2 3 4; do
        local pidfile="data/node${i}/node.pid"
        if [[ -f "$pidfile" ]]; then
            local pid
            pid=$(cat "$pidfile")
            if kill -0 "$pid" 2>/dev/null; then
                kill "$pid" 2>/dev/null && echo "    node${i} (pid=$pid) 已停止"
            else
                echo "    node${i} 已不在运行"
            fi
            rm -f "$pidfile"
        fi
    done
    # 确保没有残留进程
    pkill -f "rbft_node" 2>/dev/null || true
    sleep 1
}

# ── 清理所有数据 ──────────────────────────────────────────────────────────
clean_data() {
    echo ""
    echo "[*] 清理全部数据..."
    rm -rf data/node1 data/node2 data/node3 data/node4
    rm -rf wallets
    rm -f data/cli_client_debug.log
    echo "    data/node{1..4}/  — 区块/状态数据库"
    echo "    wallets/          — 用户钱包文件"
    echo "    data/cli_client_debug.log — CLI调试日志"
    echo "    ✓ 清理完成"
}

# ── 编译 ──────────────────────────────────────────────────────────────────
build() {
    echo ""
    echo "[*] 编译..."
    mkdir -p build
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug > /dev/null 2>&1
    make -C build rbft_node rbft_cli -j"$(nproc)" > /dev/null 2>&1
    echo "    ✓ 编译完成"
}

# ── 初始化节点 ────────────────────────────────────────────────────────────
init_nodes() {
    echo ""
    echo "[*] 初始化节点..."

    # 创建数据目录
    for i in 1 2 3 4; do
        mkdir -p "data/node${i}"
    done

    # 生成节点密钥对
    echo "    生成 Ed25519 节点密钥对..."
    for i in 1 2 3 4; do
        ./build/rbft_node --config "config/node${i}.json" \
            --gen-node-key "data/node${i}/node.key"
    done

    # 读取公钥并更新配置
    echo "    更新节点配置中的公钥..."
    PUB1=$(cat data/node1/node.key.pub)
    PUB2=$(cat data/node2/node.key.pub)
    PUB3=$(cat data/node3/node.key.pub)
    PUB4=$(cat data/node4/node.key.pub)

    python3 << PYEOF
import json
pubs = {"node1": "$PUB1", "node2": "$PUB2", "node3": "$PUB3", "node4": "$PUB4"}
for i in range(1, 5):
    path = f"config/node{i}.json"
    with open(path) as f:
        cfg = json.load(f)
    nid = f"node{i}"
    cfg["node_public_key"] = pubs[nid]
    for peer in cfg["peers"]:
        peer["public_key"] = pubs[peer["node_id"]]
    with open(path, "w") as f:
        json.dump(cfg, f, indent=2)
print(f"    已更新 node1~4.json")
PYEOF

    # 初始化数据库
    echo "    初始化 SQLite 数据库..."
    for i in 1 2 3 4; do
        ./build/rbft_node --config "config/node${i}.json" --init-db 2>/dev/null
    done

    # 创建钱包目录
    mkdir -p wallets

    echo "    ✓ 初始化完成"
}

# ── 启动节点 ──────────────────────────────────────────────────────────────
start_nodes() {
    echo ""
    echo "[*] 启动 4 节点 RBFT 网络..."
    for i in 1 2 3 4; do
        nohup ./build/rbft_node --config "config/node${i}.json" \
            > "data/node${i}/node.log" 2>&1 &
        echo $! > "data/node${i}/node.pid"
        echo "    node${i} → :$((8000+i))  pid=$(cat "data/node${i}/node.pid")"
    done

    # 等待启动
    echo ""
    echo "[*] 等待节点启动..."
    sleep 4

    # 验证
    local online=0
    for i in 1 2 3 4; do
        local port=$((8000+i))
        if curl -s --max-time 2 "http://localhost:${port}/api/node/status" \
            | python3 -c "import sys,json; assert json.load(sys.stdin)['ok']" 2>/dev/null; then
            echo "    node${i}:${port} ✓"
            online=$((online + 1))
        else
            echo "    node${i}:${port} ✗ 启动失败"
        fi
    done

    echo ""
    echo "=========================================="
    echo "  ${online}/4 节点在线"
    echo "=========================================="
    echo ""
    if [[ "$online" -eq 4 ]]; then
        echo "  CLI 客户端:"
        echo "    ./build/rbft_cli"
        echo ""
        echo "  停止所有节点:"
        echo "    bash scripts/reset_and_start.sh clean"
        echo ""
        echo "  完全重置:"
        echo "    bash scripts/reset_and_start.sh full"
    fi
}

# ── 主流程 ────────────────────────────────────────────────────────────────

case "$MODE" in
    full)
        stop_nodes
        clean_data
        build
        init_nodes
        start_nodes
        ;;
    clean)
        stop_nodes
        clean_data
        echo ""
        echo "  数据已清空。重新初始化:"
        echo "    bash scripts/reset_and_start.sh start"
        ;;
    start)
        build
        init_nodes
        start_nodes
        ;;
    restart)
        # 仅重启节点，保留数据
        stop_nodes
        start_nodes
        ;;
    *)
        echo "用法: $0 {full|clean|start|restart}"
        echo ""
        echo "  full    — 完全重置: 停止+清空+编译+初始化+启动"
        echo "  clean   — 仅停止+清空数据"
        echo "  start   — 编译+初始化+启动 (不清空已有数据)"
        echo "  restart — 重启节点 (保留所有数据)"
        exit 1
        ;;
esac
