import { request, NODE_ENDPOINTS } from './http'

export const setAttackMode = (nodeId: string, mode: string) =>
  request<{ mode: string }>(nodeId, { method: 'POST', url: '/api/admin/attack-mode', data: { mode } }, {
    title: '设置攻击模式',
    description: `${nodeId} 切换到 ${mode}`,
    algorithm: 'rbft',
    input: { nodeId, mode }
  })

export const stopConsensus = (nodeId: string) =>
  request<Record<string, unknown>>(nodeId, { method: 'POST', url: '/api/admin/stop-consensus' })

export const startConsensus = (nodeId: string) =>
  request<Record<string, unknown>>(nodeId, { method: 'POST', url: '/api/admin/start-consensus' })

export const restoreAllNormal = () => Promise.all(NODE_ENDPOINTS.map((node) => setAttackMode(node.id, 'normal')))
