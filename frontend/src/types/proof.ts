import type { Transaction } from './tx'

export interface MerkleProofItem {
  position: 'LEFT' | 'RIGHT'
  hash: string
}

export interface TxProof {
  tx: Transaction
  block_height: number
  root: string
  proof: MerkleProofItem[]
}
