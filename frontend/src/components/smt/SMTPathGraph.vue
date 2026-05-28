<script setup lang="ts">
import HashText from '@/components/common/HashText.vue'
defineProps<{ debug?: any }>()
</script>

<template>
  <div class="proof-graph">
    <div class="kv"><span>key</span><HashText :value="debug?.key" /></div>
    <div class="kv"><span>path prefix/suffix</span><code>{{ debug?.path_bits_prefix }} ... {{ debug?.path_bits_suffix }}</code></div>
    <div v-for="step in debug?.recompute_steps || []" :key="step.level" class="proof-step">
      <strong>Level {{ step.level }} bit={{ step.bit }}</strong>
      <span>{{ step.concat_order }}</span>
      <HashText :value="step.parent" />
    </div>
  </div>
</template>
