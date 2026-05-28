<script setup lang="ts">
import type { ConsensusEvent } from '@/types/node'
defineProps<{ events: ConsensusEvent[] }>()
</script>

<template>
  <div class="rbft-flow">
    <el-empty v-if="!events.length" description="等待共识事件" />
    <div v-for="event in events" :key="`${event.node_id}-${event.id}`" class="event-pill" :class="event.event_type.toLowerCase()">
      <strong>{{ event.event_type }}</strong>
      <span>{{ event.from }} → {{ event.to }}</span>
      <small>h{{ event.height }} v{{ event.view }} i{{ event.instance_id }}</small>
    </div>
  </div>
</template>
