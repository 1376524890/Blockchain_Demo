import type { Transaction } from './tx'

export interface BlockHeader {
  chain_id: string
  height: number
  previous_block_hash: string
  tx_merkle_root: string
  state_root: string
  timestamp: number
  view: number
  instance_id: number
  proposer_id: string
  block_hash: string
}

export interface Block {
  header: BlockHeader
  transactions: Transaction[]
  commit_signatures?: unknown[]
}
