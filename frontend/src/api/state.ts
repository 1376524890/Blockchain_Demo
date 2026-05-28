import { request } from './http'

export interface AccountState {
  address: string
  balance: number
  nonce: number
}

export const getAccountState = (nodeId: string, address: string) =>
  request<AccountState>(nodeId, { method: 'GET', url: `/api/state/${address}` }, {
    title: '读取账户状态',
    description: '查询地址当前余额与 nonce',
    algorithm: 'storage'
  })

export const getStateProof = (nodeId: string, address: string) =>
  request<Record<string, unknown>>(nodeId, { method: 'GET', url: `/api/state/${address}/proof` }, {
    title: '读取 SMT 存在证明',
    description: '获取账户状态演示 proof',
    algorithm: 'smt'
  })

export const getNonExistenceProof = (nodeId: string, address: string) =>
  request<Record<string, unknown>>(nodeId, { method: 'GET', url: `/api/state/${address}/non-existence-proof` })

export const verifyStateProof = (nodeId: string, body: unknown) =>
  request<{ valid: boolean }>(nodeId, { method: 'POST', url: '/api/state/proof/verify', data: body }, {
    title: '验证 SMT 存在证明',
    description: '后端按 siblings 回算演示 SMT root',
    algorithm: 'smt',
    input: body
  })

export const verifyNonExistenceProof = (nodeId: string, body: unknown) =>
  request<{ valid: boolean }>(nodeId, { method: 'POST', url: '/api/state/non-existence-proof/verify', data: body })
