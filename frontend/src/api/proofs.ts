import { request } from './http'
import type { TxProof } from '@/types/proof'

export const getTxProof = (nodeId: string, txId: string) =>
  request<TxProof>(nodeId, { method: 'GET', url: `/api/proofs/tx/${txId}` }, {
    title: '读取 Merkle Proof',
    description: '后端返回交易所在区块的 sibling path',
    algorithm: 'merkle'
  })

export const verifyTxProof = (nodeId: string, body: unknown) =>
  request<{ valid: boolean }>(nodeId, { method: 'POST', url: '/api/proofs/tx/verify', data: body }, {
    title: '验证 Merkle Proof',
    description: '后端按 proof 回算 root 并比较区块头',
    algorithm: 'merkle',
    input: body
  })
