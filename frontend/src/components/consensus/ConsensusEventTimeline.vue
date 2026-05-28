<script setup lang="ts">
import dayjs from 'dayjs'
import type { ConsensusEvent } from '@/types/node'
defineProps<{ events: ConsensusEvent[] }>()
</script>

<template>
  <el-timeline>
    <el-timeline-item v-for="event in events" :key="`${event.node_id}-${event.id}`" :type="event.accepted ? 'success' : 'danger'" :timestamp="dayjs(event.timestamp).format('HH:mm:ss.SSS')">
      <strong>{{ event.node_id }} · {{ event.event_type }}</strong>
      <p>{{ event.from }} → {{ event.to }} · {{ event.attack_mode }} {{ event.reason }}</p>
    </el-timeline-item>
  </el-timeline>
</template>
