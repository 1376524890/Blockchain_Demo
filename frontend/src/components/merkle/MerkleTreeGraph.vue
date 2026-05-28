<script setup lang="ts">
import HashText from '@/components/common/HashText.vue'
defineProps<{ tree?: any; highlight?: string[] }>()
</script>

<template>
  <div class="tree-graph">
    <div v-for="(level, levelIndex) in tree?.levels || []" :key="levelIndex" class="tree-level">
      <span class="level-label">L{{ levelIndex }}</span>
      <div v-for="node in level" :key="`${levelIndex}-${node.index}`" class="tree-node" :class="{ proof: highlight?.includes(node.hash), duplicated: node.duplicated }">
        <small>{{ node.node_type }} #{{ node.index }}</small>
        <HashText :value="node.hash" :chars="12" />
      </div>
    </div>
  </div>
</template>
