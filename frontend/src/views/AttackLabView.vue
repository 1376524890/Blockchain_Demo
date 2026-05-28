<script setup lang="ts">
import { reactive, ref } from 'vue'
import { useNodeStore } from '@/stores/nodeStore'
import { useAuthStore } from '@/stores/authStore'
import AttackModeSelector from '@/components/node/AttackModeSelector.vue'
import JsonViewer from '@/components/common/JsonViewer.vue'
import { restoreAllNormal, setAttackMode, startConsensus, stopConsensus } from '@/api/admin'
import { submitTransfer } from '@/api/transactions'

const nodes = useNodeStore()
const auth = useAuthStore()
const mode = ref('normal')
const result = ref<any>(null)
const demo = reactive({
  principle: '通过 double_proposal 模拟主节点对同一高度/视图发出冲突提案。',
  steps: ['设置 node1 double_proposal', '提交测试交易', '轮询 consensus events', '观察 evidence_count'],
  expected: '出现冲突证据或交易保持未提交，系统不应不安全提交。',
  actual: '',
  pass: null as boolean | null
})
async function applyMode() { result.value = await setAttackMode(nodes.selectedNodeId, mode.value); await nodes.refresh(100) }
async function runDemo() {
  await setAttackMode('node1', 'double_proposal')
  if (auth.user) {
    await submitTransfer(nodes.selectedNodeId, { from: auth.user.address, to: auth.user.address, amount: 1, nonce: Date.now(), public_key: auth.user.public_key, private_key: auth.user.private_key })
      .catch((error) => { demo.actual = error.message })
  }
  await nodes.refresh(100)
  const evidence = nodes.consensus.node1?.evidence_count ?? 0
  demo.actual = `node1 evidence_count=${evidence}, events=${nodes.events.length}`
  demo.pass = evidence > 0 || nodes.events.some((event) => !event.accepted || event.event_type.includes('CONFLICT'))
}
</script>

<template>
  <div class="page">
    <div class="page-title"><h2>攻击实验室</h2><p class="muted">设置攻击模式，观察事件、证据和 PASS/FAIL。</p></div>
    <section class="panel danger-panel">
      <div class="form-grid">
        <el-form-item label="节点"><el-select v-model="nodes.selectedNodeId"><el-option v-for="node in nodes.endpoints" :key="node.id" :label="node.id" :value="node.id" /></el-select></el-form-item>
        <el-form-item label="攻击模式"><AttackModeSelector v-model="mode" /></el-form-item>
      </div>
      <div class="row">
        <el-button type="danger" @click="applyMode">设置 attack-mode</el-button>
        <el-button @click="stopConsensus(nodes.selectedNodeId)">停止共识</el-button>
        <el-button @click="startConsensus(nodes.selectedNodeId)">启动共识</el-button>
        <el-button type="success" @click="restoreAllNormal().then(() => nodes.refresh(100))">恢复所有 normal</el-button>
      </div>
    </section>
    <section class="panel">
      <h3>一键 double_proposal 演示</h3>
      <p><strong>攻击原理：</strong>{{ demo.principle }}</p>
      <p><strong>操作步骤：</strong>{{ demo.steps.join(' → ') }}</p>
      <p><strong>预期结果：</strong>{{ demo.expected }}</p>
      <p><strong>实际结果：</strong>{{ demo.actual || '-' }}</p>
      <el-tag :type="demo.pass ? 'success' : demo.pass === false ? 'danger' : 'info'">{{ demo.pass === null ? 'WAITING' : demo.pass ? 'PASS' : 'FAIL' }}</el-tag>
      <el-button type="primary" @click="runDemo">运行攻击演示</el-button>
    </section>
    <section class="panel"><h3>结果 JSON</h3><JsonViewer :value="{ result, events: nodes.events, consensus: nodes.consensus }" /></section>
  </div>
</template>
