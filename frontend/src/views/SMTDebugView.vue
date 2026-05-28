<script setup lang="ts">
import { ref } from 'vue'
import { useNodeStore } from '@/stores/nodeStore'
import JsonViewer from '@/components/common/JsonViewer.vue'
import SMTPathGraph from '@/components/smt/SMTPathGraph.vue'
import SMTProofTable from '@/components/smt/SMTProofTable.vue'
import SMTVerifyPanel from '@/components/smt/SMTVerifyPanel.vue'
import { getAccountState, getNonExistenceProof, getStateProof, verifyNonExistenceProof, verifyStateProof } from '@/api/state'
import { getDebugSmt, getDebugSmtNonExistence } from '@/api/debug'

const nodes = useNodeStore()
const address = ref('')
const debug = ref<any>(null)
const proof = ref<any>(null)
const state = ref<any>(null)
const valid = ref<boolean | null>(null)
async function loadExistence() {
  state.value = await getAccountState(nodes.selectedNodeId, address.value)
  proof.value = await getStateProof(nodes.selectedNodeId, address.value)
  debug.value = await getDebugSmt(nodes.selectedNodeId, address.value)
  valid.value = (await verifyStateProof(nodes.selectedNodeId, proof.value)).valid
}
async function loadNonExistence() {
  proof.value = await getNonExistenceProof(nodes.selectedNodeId, address.value)
  debug.value = await getDebugSmtNonExistence(nodes.selectedNodeId, address.value)
  valid.value = (await verifyNonExistenceProof(nodes.selectedNodeId, proof.value)).valid
}
</script>

<template>
  <div class="page">
    <div class="page-title"><h2>SMT Proof 调试</h2><p class="muted">当前后端 SMT Proof 偏教学演示；严格绑定最新 state_root 需要全局 SMT 持久化。</p></div>
    <section class="panel row"><el-input v-model="address" placeholder="address" /><el-button type="primary" @click="loadExistence">存在证明</el-button><el-button @click="loadNonExistence">不存在证明</el-button></section>
    <div class="grid-2"><SMTVerifyPanel :valid="valid" :note="debug?.note" /><section class="panel"><h3>账户状态</h3><JsonViewer :value="state" /></section></div>
    <div class="grid-2"><section class="panel"><h3>Path 回算</h3><SMTPathGraph :debug="debug" /></section><section class="panel"><h3>Siblings</h3><SMTProofTable :siblings="debug?.siblings" /></section></div>
    <section class="panel"><h3>Debug JSON</h3><JsonViewer :value="{ proof, debug }" /></section>
  </div>
</template>
