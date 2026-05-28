<script setup lang="ts">
import type { NodeStatus } from '@/types/node'
defineProps<{ nodeId: string; status?: NodeStatus | null; selected?: boolean }>()
</script>

<template>
  <button class="node-card" :class="{ selected, attack: status?.consensus.attack_mode !== 'normal', stopped: status && !status.consensus.running }">
    <strong>{{ nodeId }}</strong>
    <span>height {{ status?.latest_height ?? '-' }}</span>
    <span>{{ status?.consensus.attack_mode ?? 'offline' }}</span>
    <small>quorum {{ status?.consensus.quorum ?? '-' }} · evidence {{ status?.consensus.evidence_count ?? 0 }}</small>
  </button>
</template>
