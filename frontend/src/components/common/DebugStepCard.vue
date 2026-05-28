<script setup lang="ts">
import type { DebugStep } from '@/types/debug'
import JsonViewer from './JsonViewer.vue'
defineProps<{ step: DebugStep }>()
</script>

<template>
  <section class="debug-step" :class="step.status">
    <div class="step-head">
      <span class="badge">{{ step.algorithm }}</span>
      <strong>{{ step.title }}</strong>
      <small>{{ step.status }} · {{ step.durationMs ?? 0 }}ms</small>
    </div>
    <p>{{ step.description }}</p>
    <div class="debug-grid">
      <JsonViewer v-if="step.request" :value="step.request" />
      <JsonViewer v-if="step.response || step.output || step.error" :value="step.response ?? step.output ?? step.error" />
    </div>
  </section>
</template>
