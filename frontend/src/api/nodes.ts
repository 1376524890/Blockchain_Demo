import { request } from './http'
import type { ConsensusEvent, ConsensusStatus, NodeStatus } from '@/types/node'

export const getNodeStatus = (nodeId: string) => request<NodeStatus>(nodeId, { method: 'GET', url: '/api/node/status' })
export const getConsensusStatus = (nodeId: string) => request<ConsensusStatus>(nodeId, { method: 'GET', url: '/api/node/consensus' })
export const getConsensusEvents = (nodeId: string, limit = 100) =>
  request<ConsensusEvent[]>(nodeId, { method: 'GET', url: `/api/node/consensus/events?limit=${limit}` })
