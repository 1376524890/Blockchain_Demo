<script setup lang="ts">
import { reactive, ref } from 'vue'
import { useNodeStore } from '@/stores/nodeStore'
import JsonViewer from '@/components/common/JsonViewer.vue'
import MerkleTreeGraph from '@/components/merkle/MerkleTreeGraph.vue'
import MerkleProofGraph from '@/components/merkle/MerkleProofGraph.vue'
import MerkleProofTable from '@/components/merkle/MerkleProofTable.vue'
import MerkleTamperPanel from '@/components/merkle/MerkleTamperPanel.vue'
import { getTxProof, verifyTxProof } from '@/api/proofs'
import { getDebugMerkleBlock, getDebugMerkleTx } from '@/api/debug'

const nodes = useNodeStore()
const txId = ref('')
const proof = ref<any>(null)
const debugTx = ref<any>(null)
const tree = ref<any>(null)
const verify = ref<any>(null)
const tamper = reactive({ amount: 0, hash: '', position: 'RIGHT' })

async function load() {
  proof.value = await getTxProof(nodes.selectedNodeId, txId.value)
  debugTx.value = await getDebugMerkleTx(nodes.selectedNodeId, txId.value)
  tree.value = await getDebugMerkleBlock(nodes.selectedNodeId, debugTx.value.block_height)
  verify.value = await verifyTxProof(nodes.selectedNodeId, { tx: proof.value.tx, proof: proof.value.proof, root: proof.value.root })
  tamper.amount = proof.value.tx.amount || 0
  tamper.hash = proof.value.proof[0]?.hash || ''
  tamper.position = proof.value.proof[0]?.position || 'RIGHT'
}
async function verifyTampered() {
  const tx = { ...proof.value.tx, amount: tamper.amount }
  const proofCopy = proof.value.proof.map((item: any, index: number) => index === 0 ? { hash: tamper.hash, position: tamper.position } : item)
  verify.value = await verifyTxProof(nodes.selectedNodeId, { tx, proof: proofCopy, root: proof.value.root })
}
</script>

<template>
  <div class="page">
    <div class="page-title"><h2>Merkle Tree 调试</h2><p class="muted">Merkle Tree 证明“某笔交易是否属于某个区块”。</p></div>
    <section class="panel row"><el-input v-model="txId" placeholder="tx_id" /><el-button type="primary" @click="load">加载 Proof</el-button><el-tag :type="verify?.valid ? 'success' : 'danger'">valid={{ verify?.valid }}</el-tag></section>
    <div class="grid-2"><section class="panel"><h3>完整 Merkle Tree</h3><MerkleTreeGraph :tree="tree" :highlight="debugTx?.proof?.map((p:any) => p.hash)" /></section><MerkleTamperPanel v-model="tamper" @verify="verifyTampered" /></div>
    <div class="grid-2"><section class="panel"><h3>Proof Path</h3><MerkleProofTable :proof="debugTx?.proof" /><MerkleProofGraph :steps="debugTx?.recompute_steps" /></section><section class="panel"><h3>Debug JSON</h3><JsonViewer :value="debugTx" /></section></div>
  </div>
</template>
