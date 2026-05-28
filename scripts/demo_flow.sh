#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
BASE_URL="${BASE_URL:-http://127.0.0.1:8001}" python3 - <<'PY'
import os, time, requests

base = os.environ["BASE_URL"]
def post(path, body):
    r = requests.post(base + path, json=body, timeout=5)
    r.raise_for_status()
    j = r.json()
    if not j["ok"]:
        raise RuntimeError(j["error"])
    return j["data"]
def get(path):
    r = requests.get(base + path, timeout=5)
    r.raise_for_status()
    j = r.json()
    if not j["ok"]:
        raise RuntimeError(j["error"])
    return j["data"]

suffix = str(int(time.time()))
alice = post("/api/users/register", {"username": "alice" + suffix, "password": "secret1"})
bob = post("/api/users/register", {"username": "bob" + suffix, "password": "secret1"})
login = post("/api/users/login", {"username": "alice" + suffix, "password": "secret1"})
tx = post("/api/transactions/transfer", {
    "from": alice["address"],
    "to": bob["address"],
    "amount": 10,
    "nonce": 1,
    "timestamp": int(time.time() * 1000),
    "public_key": login["public_key"],
    "private_key": login["private_key"]
})
block = get("/api/blocks/latest")
proof = get("/api/proofs/tx/" + tx["tx_id"])
state = get("/api/state/" + alice["address"])
state_proof = get("/api/state/" + alice["address"] + "/proof")
print("PASS: normal transfer committed")
print("PASS: latest block", block["header"]["height"])
print("PASS: tx proof root", proof["root"])
print("PASS: state", state["balance"], state["nonce"])
print("PASS: smt proof root", state_proof["root"])
PY
