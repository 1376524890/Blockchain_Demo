import { request } from './http'
import type { Transaction } from '@/types/tx'

export const submitTransfer = (nodeId: string, body: Record<string, unknown>) =>
  request<{ tx_id: string; status: string; block_height?: number }>(nodeId, { method: 'POST', url: '/api/transactions/transfer', data: body }, {
    title: '提交转账交易',
    description: '交易进入 mempool，并在 normal 模式下自动打包提交',
    algorithm: 'storage',
    input: body
  })

export const getTransaction = (nodeId: string, txId: string) =>
  request<Transaction>(nodeId, { method: 'GET', url: `/api/transactions/${txId}` }, {
    title: '查询交易',
    description: '读取交易落库后的完整 JSON',
    algorithm: 'api'
  })

export const getPendingTransactions = (nodeId: string) =>
  request<Transaction[]>(nodeId, { method: 'GET', url: '/api/transactions/pending' })
