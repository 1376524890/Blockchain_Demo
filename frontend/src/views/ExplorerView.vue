<script setup lang="ts">
import { ref } from 'vue'
import { useNodeStore } from '@/stores/nodeStore'
import JsonViewer from '@/components/common/JsonViewer.vue'
import HashText from '@/components/common/HashText.vue'
import { getBlockByHeight, getLatestBlock } from '@/api/blocks'
import { getTransaction } from '@/api/transactions'
import { getBlockTrace } from '@/api/debug'

const nodes = useNodeStore()
const height = ref(1)
const txId = ref('')
const block = ref<any>(null)
const tx = ref<any>(null)
const trace = ref<any>(null)
async function latest() { block.value = await getLatestBlock(nodes.selectedNodeId); trace.value = await getBlockTrace(nodes.selectedNodeId, block.value.header.height) }
async function byHeight() { block.value = await getBlockByHeight(nodes.selectedNodeId, height.value); trace.value = await getBlockTrace(nodes.selectedNodeId, height.value) }
async function byTx() { tx.value = await getTransaction(nodes.selectedNodeId, txId.value) }
</script>

<template>
  <div class="page">
    <div class="page-title"><h2>区块浏览</h2><p class="muted">查询区块头、交易表和 block_hash 计算输入。</p></div>
    <section class="panel row">
      <el-button type="primary" @click="latest">最新区块</el-button>
      <el-input-number v-model="height" :min="1" /><el-button @click="byHeight">按高度查询</el-button>
      <el-input v-model="txId" placeholder="tx_id" /><el-button @click="byTx">查交易</el-button>
    </section>
    <section v-if="block" class="panel">
      <h3>区块头</h3>
      <div class="kv" v-for="(value, key) in block.header" :key="key"><span>{{ key }}</span><HashText :value="String(value)" /></div>
      <el-table :data="block.transactions"><el-table-column prop="tx_id" label="tx_id" /><el-table-column prop="type" label="type" /><el-table-column prop="from" label="from" /><el-table-column prop="to" label="to" /><el-table-column prop="amount" label="amount" /></el-table>
    </section>
    <div class="grid-2"><section class="panel"><h3>Block Trace</h3><JsonViewer :value="trace" /></section><section class="panel"><h3>交易</h3><JsonViewer :value="tx" /></section></div>
  </div>
</template>
