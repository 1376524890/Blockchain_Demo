#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
tests=(
  test_attack_bad_merkle.py
  test_attack_bad_state_root.py
  test_attack_double_proposal.py
  test_attack_invalid_signature.py
  test_attack_replay_nonce.py
  test_attack_node_crash.py
  test_attack_exceed_fault_limit.py
)
for t in "${tests[@]}"; do
  python3 "tests/integration/${t}"
done
echo "PASS: attack test suite completed"
