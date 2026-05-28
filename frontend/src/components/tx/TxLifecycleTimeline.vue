<script setup lang="ts">
import { useDebugStore } from '@/stores/debugStore'
const debug = useDebugStore()
</script>

<template>
  <el-timeline>
    <el-timeline-item v-for="step in debug.steps" :key="step.id" :type="step.status === 'error' ? 'danger' : step.status === 'success' ? 'success' : 'primary'" :timestamp="step.title">
      {{ step.description }}
    </el-timeline-item>
  </el-timeline>
</template>
