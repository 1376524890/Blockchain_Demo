<script setup lang="ts">
import { computed, reactive, ref } from 'vue'
import { useAuthStore } from '@/stores/authStore'
import { useNodeStore } from '@/stores/nodeStore'
import { useDebugStore } from '@/stores/debugStore'
import { useTxStore } from '@/stores/txStore'
import TransferForm from '@/components/tx/TransferForm.vue'
import TxBodyPreview from '@/components/tx/TxBodyPreview.vue'
import SignaturePanel from '@/components/tx/SignaturePanel.vue'
import TxLifecycleTimeline from '@/components/tx/TxLifecycleTimeline.vue'
import RequestResponsePanel from '@/components/common/RequestResponsePanel.vue'
import { serializeTx } from '@/api/debug'
import { getBlockTrace, getDebugMerkleTx, getDebugSmt } from '@/api/debug'
import { submitTransfer, getTransaction } from '@/api/transactions'
import { getLatestBlock } from '@/api/blocks'
import { getTxProof, verifyTxProof } from '@/api/proofs'
import { getAccountState, getStateProof, verifyStateProof } from '@/api/state'
import { serializeTxBody, sha256Hex } from '@/crypto/txSerialize'
import { signDetachedHex } from '@/crypto/localSign'

const auth = useAuthStore()
const nodes = useNodeStore()
const debug = useDebugStore()
const txStore = useTxStore()
const form = reactive({ to: '', amount: 10, nonce: 1, mode: 'frontend_sign' as 'frontend_sign' | 'backend_demo_sign' })
const backendPreview = ref<unknown>(null)
const signature = ref('')
const txId = ref('')
const localBody = ref('')
const localHash = ref('')
const canSubmit = computed(() => auth.user && form.to)

async function runTransfer() {
  if (!auth.user) return
  debug.clearSteps()
  const timestamp = Date.now()
  const body = { type: 'TRANSFER', from: auth.user.address, to: form.to, amount: form.amount, data_hash: '', nonce: form.nonce, timestamp, public_key: auth.user.public_key }
  localBody.value = serializeTxBody(body)
  localHash.value = await sha256Hex(localBody.value)
  backendPreview.value = await serializeTx(nodes.selectedNodeId, body)
  signature.value = form.mode === 'frontend_sign' ? await signDetachedHex(localBody.value, auth.user.private_key) : ''
  const submitBody: Record<string, unknown> = { ...body }
  if (form.mode === 'frontend_sign') submitBody.signature = signature.value
  else submitBody.private_key = auth.user.private_key
  const result = await submitTransfer(nodes.selectedNodeId, submitBody)
  txId.value = result.tx_id
  txStore.remember(result.tx_id, result.block_height)
  const tx = await getTransaction(nodes.selectedNodeId, result.tx_id)
  const latest = await getLatestBlock(nodes.selectedNodeId).catch(() => null)
  const height = result.block_height ?? latest?.header.height ?? 0
  if (height) await getBlockTrace(nodes.selectedNodeId, height)
  const proofDebug = await getDebugMerkleTx(nodes.selectedNodeId, result.tx_id).catch(() => null)
  const proof = await getTxProof(nodes.selectedNodeId, result.tx_id).catch(() => null)
  if (proof) await verifyTxProof(nodes.selectedNodeId, { tx: proof.tx, proof: proof.proof, root: proof.root })
  await getAccountState(nodes.selectedNodeId, auth.user.address).catch(() => null)
  const smtProof = await getStateProof(nodes.selectedNodeId, auth.user.address).catch(() => null)
  await getDebugSmt(nodes.selectedNodeId, auth.user.address).catch(() => null)
  if (smtProof) await verifyStateProof(nodes.selectedNodeId, smtProof)
  signature.value ||= tx.signature || ''
  backendPreview.value = { backendPreview: backendPreview.value, proofDebug }
}
</script>

<template>
  <div class="page">
    <div class="page-title"><h2>交易调试</h2><p class="muted">从 tx_body、签名、提交、区块、Merkle Proof 到 SMT Proof 的完整链路。</p></div>
    <el-alert v-if="!auth.user" type="warning" title="请先在注册登录页登录付款方用户" show-icon />
    <div class="grid-2">
      <section class="panel"><h3>手动付款</h3><TransferForm v-model="form" /><el-button type="primary" :disabled="!canSubmit" @click="runTransfer">生成、签名并提交</el-button></section>
      <TxBodyPreview :backend="backendPreview" :local-body="localBody" :local-hash="localHash" />
    </div>
    <div class="grid-2"><SignaturePanel :signature="signature" :tx-id="txId" /><section class="panel"><h3>生命周期</h3><TxLifecycleTimeline /></section></div>
    <RequestResponsePanel />
  </div>
</template>
