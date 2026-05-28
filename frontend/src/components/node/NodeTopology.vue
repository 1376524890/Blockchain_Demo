<script setup lang="ts">
defineProps<{ selected?: string }>()
const nodes = ['node1', 'node2', 'node3', 'node4']
</script>

<template>
  <div class="topology">
    <div v-for="node in nodes" :key="node" class="topology-node" :class="{ selected: node === selected }">{{ node }}</div>
  </div>
</template>
