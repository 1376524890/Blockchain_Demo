<script setup lang="ts">
import type { ConsensusStatus } from '@/types/node'
defineProps<{ status?: ConsensusStatus | null }>()
</script>

<template>
  <section class="panel">
    <h3>共识状态</h3>
    <div class="metric-grid">
      <div><span>running</span><strong>{{ status?.running }}</strong></div>
      <div><span>attack_mode</span><strong>{{ status?.attack_mode }}</strong></div>
      <div><span>quorum</span><strong>{{ status?.quorum }}</strong></div>
      <div><span>evidence</span><strong>{{ status?.evidence_count }}</strong></div>
    </div>
  </section>
</template>
