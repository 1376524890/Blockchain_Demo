#!/usr/bin/env bash
# ──────────────────────────────────────────────────────────────────────────────
# RBFT Chain Demo - CLI 交互式演示脚本
# 自动执行完整流程: 注册 → 登录 → 转账 → 查询 → 证明 → 攻击模拟
# ──────────────────────────────────────────────────────────────────────────────
set -euo pipefail
cd "$(dirname "$0")/.."

echo "=========================================="
echo "  RBFT Chain Demo - CLI 演示"
echo "=========================================="
echo ""

# 检查 rbft_cli 是否已编译
if [ ! -f build/rbft_cli ]; then
    echo "[*] 编译 rbft_cli..."
    cd build && cmake .. > /dev/null 2>&1 && make rbft_cli -j$(nproc) && cd ..
    echo "[✓] 编译完成"
fi

# 清理旧数据以获得干净的演示环境
echo "[*] 清理旧数据..."
rm -f data/node1/chain.db data/node1/cli_debug.log

echo "[*] 启动 CLI 演示流程..."
echo ""

# 通过管道输入自动执行完整流程
# 每个操作后需要额外空行用于 "按 Enter 继续..." 提示
# 流程: 注册alice → 注册bob → 登录alice → 查询alice账户 → 查询区块 → 查询共识状态 → 退出
INPUT=$(cat <<'EOF'
1
alice
pass123

1
bob
pass456

2
alice
pass123

5


6


13


0
EOF
)

echo "$INPUT" | timeout 30 ./build/rbft_cli --config config/node1.json 2>/dev/null

echo ""
echo "=========================================="
echo "  演示完成!"
echo "=========================================="
echo ""
echo "调试日志已写入: data/node1/cli_debug.log"
echo ""
echo "查看调试日志:"
echo "  cat data/node1/cli_debug.log"
echo ""
