# RBFT Debug Frontend

## Install

```bash
npm install
```

## Start

```bash
npm run dev
```

The default node endpoints are:

- node1: `http://localhost:8001`
- node2: `http://localhost:8002`
- node3: `http://localhost:8003`
- node4: `http://localhost:8004`

## Backend Startup

```bash
./scripts/init_db.sh
./scripts/gen_node_keys.sh
./scripts/start_4nodes.sh
```

## Demo Flow

1. Register users A and B.
2. Log in as A.
3. Transfer from A to B.
4. Inspect `tx_body`, `signature`, and `tx_id`.
5. Inspect block trace.
6. Inspect Merkle Proof.
7. Tamper with proof data and verify failure.
8. Inspect SMT Proof.
9. Set an attack mode.
10. Restore all nodes to `normal`.

## Known Limits

- Current P2P routes reuse the REST port.
- Current SMT Proof is demonstration-oriented unless global SMT persistence is enabled.
- `private_key` is returned only for demos. Real systems must never expose private keys through APIs.
