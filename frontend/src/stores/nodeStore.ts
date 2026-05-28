import { defineStore } from 'pinia'
import { NODE_ENDPOINTS } from '@/api/http'
import { getConsensusEvents, getConsensusStatus, getNodeStatus } from '@/api/nodes'
import type { ConsensusEvent, ConsensusStatus, NodeStatus } from '@/types/node'

export const useNodeStore = defineStore('nodes', {
  state: () => ({
    endpoints: NODE_ENDPOINTS,
    selectedNodeId: 'node1',
    statuses: {} as Record<string, NodeStatus | null>,
    consensus: {} as Record<string, ConsensusStatus | null>,
    events: [] as ConsensusEvent[]
  }),
  actions: {
    selectNode(id: string) {
      this.selectedNodeId = id
    },
    async refresh(limit = 20) {
      const rows = await Promise.allSettled(this.endpoints.map(async (node) => {
        const [status, consensus, events] = await Promise.all([
          getNodeStatus(node.id),
          getConsensusStatus(node.id),
          getConsensusEvents(node.id, limit)
        ])
        return { node: node.id, status, consensus, events }
      }))
      const merged: ConsensusEvent[] = []
      for (const row of rows) {
        if (row.status === 'fulfilled') {
          this.statuses[row.value.node] = row.value.status
          this.consensus[row.value.node] = row.value.consensus
          merged.push(...row.value.events)
        }
      }
      this.events = merged.sort((a, b) => b.timestamp - a.timestamp).slice(0, limit)
    }
  }
})
