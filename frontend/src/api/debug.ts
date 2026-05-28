import { request } from './http'
import type { TxSerializeDebug } from '@/types/tx'

export const serializeTx = (nodeId: string, body: Record<string, unknown>) =>
  request<TxSerializeDebug>(nodeId, { method: 'POST', url: '/api/debug/tx/serialize', data: body }, {
    title: '预览后端签名体',
    description: '调用 SerializeTransactionBody 并返回 body_hash',
    algorithm: 'signature',
    input: body
  })

export const getBlockTrace = (nodeId: string, height: number) =>
  request<Record<string, unknown>>(nodeId, { method: 'GET', url: `/api/debug/block/${height}/trace` }, {
    title: '读取区块生成 trace',
    description: '展示交易列表、Merkle root、state_root 与区块头序列化',
    algorithm: 'hash'
  })

export const getDebugMerkleTx = (nodeId: string, txId: string) =>
  request<Record<string, unknown>>(nodeId, { method: 'GET', url: `/api/debug/merkle/tx/${txId}` }, {
    title: 'Merkle Proof 回算',
    description: '从 leaf_hash 逐层回算到 tx_merkle_root',
    algorithm: 'merkle'
  })

export const getDebugMerkleBlock = (nodeId: string, height: number) =>
  request<Record<string, unknown>>(nodeId, { method: 'GET', url: `/api/debug/merkle/block/${height}` }, {
    title: '读取完整 Merkle Tree',
    description: '获取每一层节点 hash 与左右子节点关系',
    algorithm: 'merkle'
  })

export const getDebugSmt = (nodeId: string, address: string) =>
  request<Record<string, unknown>>(nodeId, { method: 'GET', url: `/api/debug/smt/${address}` }, {
    title: 'SMT 存在证明回算',
    description: '展示 key bits、siblings 和 root 回算过程',
    algorithm: 'smt'
  })

export const getDebugSmtNonExistence = (nodeId: string, address: string) =>
  request<Record<string, unknown>>(nodeId, { method: 'GET', url: `/api/debug/smt/${address}/non-existence` }, {
    title: 'SMT 不存在证明回算',
    description: '展示不存在 proof 的 siblings 和 root 回算过程',
    algorithm: 'smt'
  })
