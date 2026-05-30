#!/usr/bin/env python3
"""
RBFT Chain Demo - 性能压测脚本
================================
基于 CLI HTTP API 的自动化性能测试。
模拟大量用户注册、随机转账，测量系统吞吐量、延迟和资源消耗。

用法:
  python3 perf_test.py --users 50 --txs 500 --batch 4
"""

import requests
import time
import json
import random
import sys
import os
import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
from dataclasses import dataclass, field
from typing import Optional

BASE_URLS = [
    "http://localhost:8001",
    "http://localhost:8002",
    "http://localhost:8003",
    "http://localhost:8004",
]

@dataclass
class TestUser:
    username: str
    password: str
    address: str
    public_key: str
    private_key: str

@dataclass
class Metrics:
    name: str
    total_ops: int = 0
    total_time_ms: float = 0.0
    success: int = 0
    failure: int = 0
    latencies_ms: list = field(default_factory=list)

    @property
    def tps(self) -> float:
        if self.total_time_ms == 0: return 0.0
        return (self.success / self.total_time_ms) * 1000.0

    @property
    def avg_latency_ms(self) -> float:
        if not self.latencies_ms: return 0.0
        return sum(self.latencies_ms) / len(self.latencies_ms)

    @property
    def p50_latency_ms(self) -> float:
        if not self.latencies_ms: return 0.0
        s = sorted(self.latencies_ms); return s[len(s) // 2]

    @property
    def p95_latency_ms(self) -> float:
        if not self.latencies_ms: return 0.0
        s = sorted(self.latencies_ms); return s[int(len(s) * 0.95)]

    @property
    def p99_latency_ms(self) -> float:
        if not self.latencies_ms: return 0.0
        s = sorted(self.latencies_ms); return s[min(int(len(s) * 0.99), len(s) - 1)]

    @property
    def min_latency_ms(self) -> float: return min(self.latencies_ms) if self.latencies_ms else 0.0
    @property
    def max_latency_ms(self) -> float: return max(self.latencies_ms) if self.latencies_ms else 0.0


def http_post(url, path, body, timeout=10):
    try:
        r = requests.post(f"{url}{path}", json=body, timeout=timeout)
        d = r.json()
        return d.get("ok", False), d.get("data", {}), d.get("error", "")
    except Exception as e:
        return False, {}, str(e)

def http_get(url, path, timeout=5):
    try:
        r = requests.get(f"{url}{path}", timeout=timeout)
        d = r.json()
        return d.get("ok", False), d.get("data", {}), d.get("error", "")
    except Exception as e:
        return False, {}, str(e)

def check_nodes():
    alive = []
    for i, url in enumerate(BASE_URLS):
        ok, data, _ = http_get(url, "/api/node/status", timeout=3)
        if ok:
            alive.append(i)
            print(f"  node{i+1}: OK  height={data.get('latest_height', '?')}")
        else:
            print(f"  node{i+1}: OFFLINE")
    return alive


# ═══════════════════════════════════════════════
# Core operations
# ═══════════════════════════════════════════════

def register_user_sequential(username, password="test123456"):
    """顺序注册一个用户到所有节点（确保数据一致性）"""
    # Step 1: 生成密钥对
    ok, data, err = http_post(BASE_URLS[0], "/api/crypto/generate-keypair", {}, timeout=10)
    if not ok:
        return None

    address = data["address"]
    pub_key = data["public_key"]
    priv_key = data["private_key"]

    reg_body = {
        "username": username, "password": password,
        "address": address, "public_key": pub_key, "private_key": priv_key,
    }

    # Step 2: 顺序注册到所有节点
    for url in BASE_URLS:
        for retry in range(3):
            ok2, _, err2 = http_post(url, "/api/users/register", reg_body, timeout=15)
            if ok2:
                break
            time.sleep(0.3)

    return TestUser(username=username, password=password, address=address,
                    public_key=pub_key, private_key=priv_key)


def get_account(address, url_idx=0):
    ok, data, _ = http_get(BASE_URLS[url_idx], f"/api/state/{address}")
    if ok:
        return data.get("balance", 0), data.get("nonce", 0)
    return 0, 0


def transfer(sender, to_addr, amount, nonce, url_idx=0):
    tx = {
        "type": "TRANSFER", "from": sender.address, "to": to_addr,
        "amount": amount, "nonce": nonce, "timestamp": 0,
        "public_key": sender.public_key, "private_key": sender.private_key,
    }
    ok, data, err = http_post(BASE_URLS[url_idx], "/api/transactions/transfer", tx, timeout=15)
    return ok, data, err


def get_height(url_idx=0):
    ok, data, _ = http_get(BASE_URLS[url_idx], "/api/node/status")
    return int(data.get("latest_height", "0")) if ok else 0


def get_block(h, url_idx=0):
    ok, data, _ = http_get(BASE_URLS[url_idx], f"/api/blocks/{h}")
    return data if ok else None


def merkle_proof(tx_id, url_idx=0):
    t0 = time.time()
    ok, _, _ = http_get(BASE_URLS[url_idx], f"/api/proofs/tx/{tx_id}", timeout=5)
    return ok, (time.time() - t0) * 1000


def smt_proof(address, url_idx=0):
    t0 = time.time()
    ok, _, _ = http_get(BASE_URLS[url_idx], f"/api/state/{address}/proof", timeout=5)
    return ok, (time.time() - t0) * 1000


def db_size(node_id):
    try: return os.path.getsize(f"data/node{node_id}/chain.db")
    except: return 0


# ═══════════════════════════════════════════════
# Test Phases
# ═══════════════════════════════════════════════

def phase1_register(num_users):
    """顺序注册用户，确保数据一致性"""
    print(f"\n{'='*60}")
    print(f"Phase 1: 顺序注册 {num_users} 个用户")
    print(f"{'='*60}")

    m = Metrics(name="UserRegistration")
    users = []
    t0 = time.time()

    for i in range(num_users):
        t1 = time.time()
        u = register_user_sequential(f"user_{i:05d}")
        if u:
            users.append(u)
            m.success += 1
            m.latencies_ms.append((time.time() - t1) * 1000)
        else:
            m.failure += 1

        if (i + 1) % max(1, num_users // 5) == 0:
            print(f"  [{i+1}/{num_users}] tps={m.tps:.1f}")

    m.total_time_ms = (time.time() - t0) * 1000
    m.total_ops = num_users

    # Verify consistency
    counts = {}
    for idx in range(len(BASE_URLS)):
        ok, d, _ = http_get(BASE_URLS[idx], "/api/users?page=1&page_size=1", timeout=5)
        counts[f"n{idx+1}"] = d.get("total", 0) if ok else -1
    print(f"  Node user counts: {counts}")

    print(f"  Done: {m.success} ok, tps={m.tps:.1f}, time={m.total_time_ms/1000:.1f}s")
    return m, users


def phase2_transfers(users, num_txs, batch):
    """并发转账（全部发往主节点确保 mempool 一致）"""
    print(f"\n{'='*60}")
    print(f"Phase 2: 随机转账 {num_txs} 笔 (金额 1-20, batch={batch})")
    print(f"{'='*60}")

    m = Metrics(name="TransferSubmit")
    if len(users) < 2:
        return m, []

    # Track nonces locally
    nonces = {}
    for u in users:
        _, nc = get_account(u.address, 0)
        nonces[u.address] = nc

    t0 = time.time()
    tx_count = 0
    submitted = []
    blacklist = set()  # 余额不足的用户

    with ThreadPoolExecutor(max_workers=batch) as ex:
        futs = {}
        while tx_count < num_txs:
            avail = [u for u in users if u.address not in blacklist]
            if len(avail) < 2:
                print(f"  ⚠ Only {len(avail)} usable senders left")
                break

            sender = random.choice(avail)
            receiver = random.choice([u for u in avail if u.address != sender.address])
            amount = random.randint(1, 20)

            nc = nonces.get(sender.address, 0) + 1
            nonces[sender.address] = nc

            # Send to primary (node1)
            fut = ex.submit(transfer, sender, receiver.address, amount, nc, 0)
            futs[fut] = (sender, receiver, amount, time.time())
            tx_count += 1

            if len(futs) >= batch * 3:
                done = [f for f in futs if f.done()]
                for f in done[:batch]:
                    _collect_future(f, futs, m, submitted, blacklist)
                    del futs[f]
                time.sleep(0.05)

            if tx_count % 100 == 0:
                print(f"  [{tx_count}/{num_txs}] tps={m.tps:.1f}")

        for f in as_completed(futs):
            _collect_future(f, futs, m, submitted, blacklist)

    m.total_time_ms = (time.time() - t0) * 1000
    m.total_ops = num_txs

    print(f"  Done: {m.success} ok/{m.failure} fail, tps={m.tps:.1f}, time={m.total_time_ms/1000:.1f}s")
    print(f"  Latency: avg={m.avg_latency_ms:.0f}ms p50={m.p50_latency_ms:.0f}ms p95={m.p95_latency_ms:.0f}ms")
    return m, submitted


def _collect_future(fut, futs, m, submitted, blacklist):
    entry = futs.get(fut, (None, None, 0, 0))
    sender = entry[0]
    try:
        ok, data, err = fut.result()
        if ok:
            m.success += 1
            submitted.append(data.get("tx_id", ""))
        else:
            m.failure += 1
            if "insufficient balance" in err.lower() and sender:
                blacklist.add(sender.address)
    except Exception:
        m.failure += 1


def phase3_wait_blocks(target, max_wait):
    """等待出块"""
    print(f"\n{'='*60}")
    print(f"Phase 3: 等待共识出块 (target={target}, max_wait={max_wait}s)")
    print(f"{'='*60}")

    init_h = get_height(0)
    print(f"  Initial height: {init_h}")
    last_h = init_h
    waited = 0
    while waited < max_wait:
        time.sleep(3)
        waited += 3
        h = get_height(0)
        if h > last_h:
            print(f"  Height {last_h} -> {h} (+{h - last_h} blocks) at {waited}s")
            last_h = h
            if h >= init_h + target:
                break

    final_h = get_height(0)
    produced = final_h - init_h
    print(f"  Final height: {final_h}, produced: {produced} blocks in {waited}s")
    return final_h, produced


def phase4_block_analysis(max_blocks):
    """区块与证明分析"""
    print(f"\n{'='*60}")
    print(f"Phase 4: Block & Proof Analysis")
    print(f"{'='*60}")

    h = get_height(0)
    print(f"  Current height: {h}")

    result = {"latest_height": h, "blocks": [], "merkle_times": [], "smt_times": []}

    if h == 0:
        print("  No blocks to analyze")
        return result

    start = max(1, h - max_blocks + 1)
    total_txs = 0
    for bh in range(start, h + 1):
        block = get_block(bh, 0)
        if not block:
            continue
        hdr = block.get("header", {})
        txs = block.get("transactions", [])
        n = len(txs)
        total_txs += n
        result["blocks"].append({
            "height": bh, "tx_count": n,
            "merkle_root": hdr.get("tx_merkle_root", "")[:16],
            "hash": hdr.get("block_hash", "")[:16],
        })
        if txs:
            ok, ms = merkle_proof(txs[0].get("tx_id", ""), 0)
            if ok:
                result["merkle_times"].append(ms)
        if n > 0:
            print(f"  h={bh:3d}: {n:3d} txs, hash={hdr.get('block_hash', '')[:16]}..")

    if result["blocks"]:
        r = result
        r["avg_txs_per_block"] = total_txs / len(result["blocks"])
        r["total_txs"] = total_txs
        print(f"\n  {len(result['blocks'])} blocks, {total_txs} txs, avg={r['avg_txs_per_block']:.1f} tx/block")

    if result["merkle_times"]:
        r["avg_merkle_ms"] = sum(r["merkle_times"]) / len(r["merkle_times"])
        print(f"  Avg Merkle proof: {r['avg_merkle_ms']:.2f}ms")

    # SMT proofs
    ok, d, _ = http_get(BASE_URLS[0], "/api/users?page=1&page_size=3", timeout=5)
    if ok:
        for u in d.get("users", [])[:3]:
            addr = u.get("address", "")
            ok2, ms = smt_proof(addr, 0)
            if ok2:
                result["smt_times"].append(ms)
                print(f"  SMT proof for {u.get('username','?')}: {ms:.2f}ms")
    if result["smt_times"]:
        result["avg_smt_ms"] = sum(result["smt_times"]) / len(result["smt_times"])
        print(f"  Avg SMT proof: {result['avg_smt_ms']:.2f}ms")

    return result


def phase5_consensus_check():
    """共识一致性验证"""
    print(f"\n{'='*60}")
    print(f"Phase 5: Consensus Verification")
    print(f"{'='*60}")

    heights = {}
    hashes = {}
    for i, url in enumerate(BASE_URLS):
        ok, d, _ = http_get(url, "/api/node/status", timeout=3)
        n = f"node{i+1}"
        if ok:
            heights[n] = int(d.get("latest_height", "0"))
            ok2, bd, _ = http_get(url, "/api/blocks/latest", timeout=3)
            hashes[n] = bd.get("header", {}).get("block_hash", "") if ok2 else ""
            print(f"  {n}: h={heights[n]}, hash={hashes[n][:16]}..")
        else:
            heights[n] = -1
            print(f"  {n}: OFFLINE")

    hv = [v for v in heights.values() if v >= 0]
    hashv = [v for v in hashes.values() if v]
    h_ok = len(set(hv)) <= 1 if hv else False
    hash_ok = len(set(hashv)) <= 1 if hashv else False

    print(f"\n  Height consistent: {'YES' if h_ok else 'NO'} {hv}")
    print(f"  Hash consistent: {'YES' if hash_ok else 'NO'}")
    return {"heights": heights, "h_ok": h_ok, "hash_ok": hash_ok}


def phase6_mempool_stress(users, num_txs):
    """MemPool 压力测试"""
    print(f"\n{'='*60}")
    print(f"Phase 6: MemPool Stress ({num_txs} txs, batch=16)")
    print(f"{'='*60}")

    m = Metrics(name="MemPoolStress")
    if len(users) < 2:
        return m

    nonces = {}
    for u in users:
        _, nc = get_account(u.address, 0)
        nonces[u.address] = nc

    t0 = time.time()
    blacklist = set()

    with ThreadPoolExecutor(max_workers=16) as ex:
        futs = {}
        for i in range(num_txs):
            avail = [u for u in users if u.address not in blacklist]
            if len(avail) < 2:
                break
            s = random.choice(avail)
            r = random.choice([u for u in avail if u.address != s.address])
            amt = random.randint(1, 5)
            nc = nonces.get(s.address, 0) + 1
            nonces[s.address] = nc
            futs[ex.submit(transfer, s, r.address, amt, nc, 0)] = (s, r, amt)

            if len(futs) >= 50:
                done = [f for f in futs if f.done()]
                for f in done[:20]:
                    _collect_future(f, futs, m, [], blacklist)
                    del futs[f]

            if (i + 1) % 100 == 0:
                print(f"  [{i+1}/{num_txs}] tps={m.tps:.1f}")

        for f in as_completed(futs):
            _collect_future(f, futs, m, [], blacklist)

    m.total_time_ms = (time.time() - t0) * 1000
    m.total_ops = num_txs
    print(f"  Done: {m.success} ok, tps={m.tps:.1f}, p95={m.p95_latency_ms:.0f}ms")
    return m


# ═══════════════════════════════════════════════
# Report Generation
# ═══════════════════════════════════════════════

COMPLEXITY = """
## 6. Core Data Structure Complexity Analysis

### 6.1 Transaction
**Space**: O(1) ~500 bytes per tx (fixed fields + Ed25519 sig)
| Operation | Time | Note |
|-----------|------|------|
| SerializeBody | O(1) | Fixed field concatenation |
| Sign/Verify | O(1) | Ed25519 constant-time |
| ComputeTxId | O(1) | SHA-256 over ~250 bytes |

### 6.2 CustomHashTable (Chained Hashing)
Load factor > 0.75 triggers 2x rehash.
| Operation | Average | Worst | Note |
|-----------|---------|-------|------|
| Put/Get/Contains | O(1) | O(n) | Collision → linked list |
| Rehash | O(n) | O(n) | Re-insert all keys |

### 6.3 Mempool
vector<Transaction> + CustomHashTable<tx_id, index>
| Operation | Time | Note |
|-----------|------|------|
| AddTransaction | O(1) avg | Hash dedup + vector push |
| Exists | O(1) avg | Hash lookup |
| PickTransactions(k) | O(k) | Take first k |
| RemoveCommitted | O(k*m) | k removed, m pool size, rebuilds index |

### 6.4 MerkleTree (Transaction Proof Tree)
Binary tree. Leaf = Hash(0x00||tx_body). Internal = Hash(0x01||left||right).
| Operation | Time | Space | Note |
|-----------|------|-------|------|
| ComputeRoot | O(n) | O(n) | Build all levels |
| GenerateProof | O(log n) | O(log n) | ceil(log2(n)) siblings |
| VerifyProof | O(log n) | O(1) | Recompute log2(n) SHA-256 |
| Proof size: ceil(log2(n)) * 32 bytes |
| n=10: 128B | n=100: 224B | n=1000: 320B |

### 6.5 SparseMerkleTree (State Tree)
256-depth binary sparse tree. Pre-computed 257 default hashes (8KB). Only stores non-empty leaves.
| Operation | Current | Optimized |
|-----------|---------|-----------|
| Update | O(n) linear FindLeaf | O(log N) path update |
| Get | O(n) | O(log N) binary search |
| GenerateProof | O(k log k) per-level grouping | O(k log k) |
| VerifyProof | O(256)=O(1) fixed rounds | O(1) |
| Proof size: fixed 256*32 = 8KB |

### 6.6 ConsensusEngine (PBFT)
| Operation | Time | Note |
|-----------|------|------|
| OnPrePrepare/Prepare/Commit | O(1) | Vote counting |
| QuarantineNode | O(log n) | std::set |
| Quorum | O(1) | 2f+1 formula |
| PBFT message complexity: O(N^2) per block |

### 6.7 Block & SQLiteStorage
Block space: Header(~400B) + n*~500B + s*~200B
| Operation | Time | Note |
|-----------|------|------|
| GetAccount | O(log n) | SQLite B-tree |
| GetBlockByHeight | O(log n) | B-tree index |
| GetAllSMTLeaves | O(n) | Full table scan |
"""


def generate_report(metrics_list, block_analysis, consensus, db_growth, total_time,
                    n_users, n_txs, n_stress):
    lines = []
    lines.append("# RBFT Chain Demo - Performance Test Report\n")
    lines.append(f"**Test Time**: {time.strftime('%Y-%m-%d %H:%M:%S')}")
    lines.append(f"**Config**: 4-node PBFT (f=1, Quorum=3), block_interval=1500ms")
    lines.append(f"**Scale**: {n_users} users, {n_txs} transfers, {n_stress} mempool stress txs")
    lines.append(f"**Total Duration**: {total_time:.1f}s\n")

    lines.append("## 1. Summary\n")
    lines.append("| Metric | Value |")
    lines.append("|--------|-------|")
    for m in metrics_list:
        lines.append(f"| {m.name} - TPS | {m.tps:.1f} tx/s |")
        lines.append(f"| {m.name} - Avg Latency | {m.avg_latency_ms:.1f} ms |")
        lines.append(f"| {m.name} - P95 Latency | {m.p95_latency_ms:.1f} ms |")
        lines.append(f"| {m.name} - Success Rate | {m.success}/{m.total_ops} ({100*m.success/max(1,m.total_ops):.1f}%) |")
        lines.append("")

    lines.append("## 2. Detailed Metrics\n")
    for m in metrics_list:
        lines.append(f"### {m.name}\n")
        lines.append("| Metric | Value |")
        lines.append("|--------|-------|")
        lines.append(f"| Total Ops | {m.total_ops} |")
        lines.append(f"| Success | {m.success} |")
        lines.append(f"| Failure | {m.failure} |")
        lines.append(f"| Duration | {m.total_time_ms/1000:.2f}s |")
        lines.append(f"| **TPS** | **{m.tps:.1f}** |")
        lines.append(f"| Avg Latency | {m.avg_latency_ms:.1f}ms |")
        lines.append(f"| P50 | {m.p50_latency_ms:.1f}ms |")
        lines.append(f"| P95 | {m.p95_latency_ms:.1f}ms |")
        lines.append(f"| P99 | {m.p99_latency_ms:.1f}ms |")
        lines.append(f"| Min | {m.min_latency_ms:.1f}ms |")
        lines.append(f"| Max | {m.max_latency_ms:.1f}ms |")
        lines.append("")

    lines.append("## 3. Block Analysis\n")
    lines.append(f"- Final height: **{block_analysis.get('latest_height', 0)}**")
    if block_analysis.get('avg_txs_per_block'):
        lines.append(f"- Avg tx/block: **{block_analysis['avg_txs_per_block']:.1f}**")
        lines.append(f"- Total on-chain txs: **{block_analysis.get('total_txs', 0)}**")
    if block_analysis.get('avg_merkle_ms'):
        lines.append(f"- Avg Merkle proof: **{block_analysis['avg_merkle_ms']:.2f}ms**")
    if block_analysis.get('avg_smt_ms'):
        lines.append(f"- Avg SMT proof: **{block_analysis['avg_smt_ms']:.2f}ms**")

    if block_analysis.get("blocks"):
        lines.append("\n| Height | Tx Count | Merkle Root | Block Hash |")
        lines.append("|--------|----------|-------------|------------|")
        for b in block_analysis["blocks"][-10:]:
            lines.append(f"| {b['height']} | {b['tx_count']} | {b['merkle_root']}.. | {b['hash']}.. |")

    lines.append("\n## 4. Consensus Verification\n")
    lines.append(f"- Height consistent: **{'YES' if consensus.get('h_ok') else 'NO'}**")
    lines.append(f"- Hash consistent: **{'YES' if consensus.get('hash_ok') else 'NO'}**")

    lines.append("\n## 5. Storage Growth\n")
    lines.append("| Node | Initial | Final | Growth |")
    lines.append("|------|---------|-------|--------|")
    for node, info in db_growth.items():
        lines.append(f"| {node} | {info['initial_bytes']/1024:.1f}KB | {info['final_bytes']/1024:.1f}KB | +{info['growth_mb']:.2f}MB |")

    lines.append(COMPLEXITY)

    lines.append("## 7. Recommendations\n")
    lines.append("1. **SMT FindLeaf**: Current O(n) linear scan is the primary bottleneck for state operations. Implement sorted + binary search O(log n).")
    lines.append("2. **SMT Path Persistence**: Current Update recalculates full root. Implement path-based updates for O(log N) per update.")
    lines.append("3. **Mempool RemoveCommitted**: Current O(k*m) with full index rebuild. Use swap-remove + selective index update for O(k).")
    lines.append("4. **TryProposeBlock Validation**: Currently validates each tx individually via ExecuteForStateRoot, doubling execution cost. Batch validate once.")
    lines.append("5. **SQLite WAL Mode**: Enable WAL journal mode for concurrent read/write performance improvement.")
    lines.append("6. **Connection Pooling**: httplib creates new TCP connection per request. Introduce connection pool to reduce handshake overhead.")

    return "\n".join(lines)


# ═══════════════════════════════════════════════
# Main
# ═══════════════════════════════════════════════

def main():
    ap = argparse.ArgumentParser(description="RBFT Chain Demo Perf Test")
    ap.add_argument("--users", type=int, default=50)
    ap.add_argument("--txs", type=int, default=500)
    ap.add_argument("--stress", type=int, default=300)
    ap.add_argument("--batch", type=int, default=4)
    ap.add_argument("--report", type=str, default="PERF_REPORT.md")
    args = ap.parse_args()

    print("=" * 60)
    print("  RBFT Chain Demo - Performance Test")
    print("=" * 60)
    print(f"  Users={args.users}  Transfers={args.txs}  Stress={args.stress}")

    print("\nChecking nodes...")
    alive = check_nodes()
    if len(alive) < 4:
        print(f"⚠ Only {len(alive)}/4 nodes online")
    if len(alive) < 1:
        print("❌ No nodes available!")
        sys.exit(1)

    all_metrics = []
    total_start = time.time()
    initial_dbs = {f"node{i}": db_size(i) for i in range(1, 5)}

    # Phase 1: Register users (sequential for consistency)
    m1, users = phase1_register(args.users)
    all_metrics.append(m1)
    if len(users) < 2:
        print("❌ Not enough users")
        sys.exit(1)

    # Phase 2: Random transfers
    m2, tx_ids = phase2_transfers(users, args.txs, args.batch)
    all_metrics.append(m2)

    # Phase 3: Wait for blocks
    final_h, produced = phase3_wait_blocks(target=10, max_wait=60)

    # Phase 4: Block analysis
    block_result = phase4_block_analysis(max_blocks=30)

    # Phase 5: Consensus verification
    consensus_result = phase5_consensus_check()

    # Phase 6: MemPool stress
    if args.stress > 0:
        m6 = phase6_mempool_stress(users, args.stress)
        all_metrics.append(m6)

    # DB growth
    final_dbs = {f"node{i}": db_size(i) for i in range(1, 5)}
    db_growth = {}
    for node, final_sz in final_dbs.items():
        init_sz = initial_dbs.get(node, 0)
        db_growth[node] = {"initial_bytes": init_sz, "final_bytes": final_sz,
                           "growth_mb": (final_sz - init_sz) / (1024 * 1024)}

    total_time = time.time() - total_start

    report = generate_report(all_metrics, block_result, consensus_result, db_growth,
                             total_time, args.users, args.txs, args.stress)
    with open(args.report, "w") as f:
        f.write(report)

    print(f"\n{'='*60}")
    print(f"  Done! Total: {total_time:.1f}s  Report: {args.report}")
    print(f"{'='*60}")


if __name__ == "__main__":
    main()
