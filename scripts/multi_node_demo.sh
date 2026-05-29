#!/usr/bin/env bash
# ──────────────────────────────────────────────────────────────────────────────
# RBFT 多节点拜占庭容错演示
# 启动 4 个 rbft_node + rbft_cli 客户端交互
# ──────────────────────────────────────────────────────────────────────────────
set -euo pipefail
cd "$(dirname "$0")/.."

echo "=========================================="
echo "  RBFT 多节点拜占庭容错演示"
echo "=========================================="
echo ""

# 编译
if [ ! -f build/rbft_node ] || [ ! -f rbft_cli ]; then
    echo "[*] 编译..."
    cd build && cmake .. > /dev/null 2>&1 && make rbft_node rbft_cli -j$(nproc) && cd ..
    echo "[✓] 编译完成"
fi

# 清理旧数据
echo "[*] 清理旧数据..."
for i in 1 2 3 4; do
    [ -f "data/node$i/node.pid" ] && kill "$(cat "data/node$i/node.pid")" 2>/dev/null || true
done
rm -rf data/node{1,2,3,4}
mkdir -p data/node{1,2,3,4}

# 启动 4 个节点
echo "[*] 启动 4 个 rbft_node..."
for i in 1 2 3 4; do
    ./build/rbft_node --config "config/node$i.json" > "data/node$i/node.log" 2>&1 &
    echo $! > "data/node$i/node.pid"
    echo "    node$i → :$((8000+i)) pid=$!"
done
sleep 2

# 验证节点在线
echo "[*] 验证节点连接..."
for i in 1 2 3 4; do
    port=$((8000+i))
    if curl -s "http://localhost:$port/api/node/status" | python3 -c "import sys,json; d=json.load(sys.stdin); assert d['ok']" 2>/dev/null; then
        echo "    node$i ✓"
    else
        echo "    node$i ✗ (启动失败)"
    fi
done

echo ""
echo "=========================================="
echo "  所有节点已启动!"
echo "=========================================="
echo ""
echo "启动 rbft_cli 客户端:"
echo "  ./rbft_cli"
echo ""
echo "或多节点指定:"
echo "  ./rbft_cli --nodes localhost:8001,8002,8003,8004"
echo ""
echo "停止所有节点:"
echo "  for i in 1 2 3 4; do kill \$(cat data/node\$i/node.pid); done"
echo ""
