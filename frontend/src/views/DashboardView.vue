<script setup lang="ts">
import { onMounted, onUnmounted, ref } from 'vue'
import { useNodeStore } from '@/stores/nodeStore'
import NodeStatusCard from '@/components/node/NodeStatusCard.vue'
import ConsensusEventTimeline from '@/components/consensus/ConsensusEventTimeline.vue'
import JsonViewer from '@/components/common/JsonViewer.vue'
import { getLatestBlock } from '@/api/blocks'
import { getPendingTransactions } from '@/api/transactions'

const nodes = useNodeStore()
const latest = ref<unknown>(null)
const pending = ref<unknown[]>([])
let timer: number | undefined

async function refresh() {
  await nodes.refresh(20)
  latest.value = await getLatestBlock(nodes.selectedNodeId).catch(() => null)
  pending.value = await getPendingTransactions(nodes.selectedNodeId).catch(() => [])
}
onMounted(() => { refresh(); timer = window.setInterval(refresh, 3000) })
onUnmounted(() => window.clearInterval(timer))
</script>

<template>
  <div class="page">
    <div class="page-title"><h2>Dashboard</h2><p class="muted">4 节点状态、最新区块、pending 交易和最新共识事件</p></div>
    <div class="grid-4 grid-3">
      <NodeStatusCard v-for="node in nodes.endpoints" :key="node.id" :node-id="node.id" :status="nodes.statuses[node.id]" :selected="nodes.selectedNodeId === node.id" @click="nodes.selectNode(node.id)" />
    </div>
    <div class="grid-2">
      <section class="panel"><h3>最新区块</h3><JsonViewer :value="latest" /></section>
      <section class="panel"><h3>Pending 交易</h3><JsonViewer :value="pending" /></section>
    </div>
    <section class="panel"><h3>Consensus Events</h3><ConsensusEventTimeline :events="nodes.events" /></section>
  </div>
</template>
