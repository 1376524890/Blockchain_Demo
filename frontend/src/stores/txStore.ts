import { defineStore } from 'pinia'
import type { Transaction } from '@/types/tx'

export const useTxStore = defineStore('tx', {
  state: () => ({
    lastTx: null as Transaction | null,
    lastTxId: '',
    lastBlockHeight: 0
  }),
  actions: {
    remember(txId: string, blockHeight?: number) {
      this.lastTxId = txId
      this.lastBlockHeight = blockHeight ?? 0
    }
  }
})
