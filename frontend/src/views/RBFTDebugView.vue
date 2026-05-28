<script setup lang="ts">
import { onMounted, onUnmounted } from 'vue'
import { useNodeStore } from '@/stores/nodeStore'
import NodeTopology from '@/components/node/NodeTopology.vue'
import ConsensusStatusPanel from '@/components/consensus/ConsensusStatusPanel.vue'
import ConsensusEventTimeline from '@/components/consensus/ConsensusEventTimeline.vue'
import RBFTFlowGraph from '@/components/consensus/RBFTFlowGraph.vue'

const nodes = useNodeStore()
let timer: number | undefined
onMounted(() => { nodes.refresh(100); timer = window.setInterval(() => nodes.refresh(100), 2500) })
onUnmounted(() => window.clearInterval(timer))
</script>

<template>
  <div class="page">
    <div class="page-title"><h2>RBFT 共识调试</h2><p class="muted">height 是区块高度，view 是主节点任期，instance_id 是并行实例编号，quorum=2f+1。</p></div>
    <div class="grid-2"><section class="panel"><h3>4 节点拓扑</h3><NodeTopology :selected="nodes.selectedNodeId" /></section><ConsensusStatusPanel :status="nodes.consensus[nodes.selectedNodeId]" /></div>
    <section class="panel"><h3>消息流</h3><RBFTFlowGraph :events="nodes.events" /></section>
    <section class="panel"><h3>事件时间线</h3><ConsensusEventTimeline :events="nodes.events" /></section>
  </div>
</template>
