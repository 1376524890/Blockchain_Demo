<script setup lang="ts">
import { useNodeStore } from '@/stores/nodeStore'
const nodes = useNodeStore()
const nav = [
  ['/', 'Dashboard'],
  ['/auth', '注册登录'],
  ['/tx', '交易调试'],
  ['/explorer', '区块浏览'],
  ['/merkle', 'Merkle'],
  ['/smt', 'SMT'],
  ['/rbft', 'RBFT'],
  ['/attack', '攻击实验']
]
</script>

<template>
  <el-container class="app-shell">
    <el-aside width="232px" class="side">
      <h1>RBFT Debug Lab</h1>
      <el-select v-model="nodes.selectedNodeId" class="node-select">
        <el-option v-for="node in nodes.endpoints" :key="node.id" :label="`${node.id} · ${node.baseURL}`" :value="node.id" />
      </el-select>
      <nav>
        <RouterLink v-for="[path, label] in nav" :key="path" :to="path">{{ label }}</RouterLink>
      </nav>
    </el-aside>
    <el-main>
      <RouterView />
    </el-main>
  </el-container>
</template>
