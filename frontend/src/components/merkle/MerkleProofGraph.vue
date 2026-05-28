<script setup lang="ts">
import HashText from '@/components/common/HashText.vue'
defineProps<{ steps?: any[] }>()
</script>

<template>
  <div class="proof-graph">
    <div v-for="step in steps || []" :key="step.level" class="proof-step">
      <strong>Level {{ step.level }}</strong>
      <span>{{ step.concat_order }}</span>
      <HashText :value="step.current_before" />
      <HashText :value="step.sibling" />
      <HashText :value="step.parent" />
    </div>
  </div>
</template>
