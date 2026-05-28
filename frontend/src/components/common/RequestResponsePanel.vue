<script setup lang="ts">
import { useDebugStore } from '@/stores/debugStore'
import DebugStepCard from './DebugStepCard.vue'
import CopyableCode from './CopyableCode.vue'
const debug = useDebugStore()
</script>

<template>
  <div class="panel">
    <div class="panel-head">
      <h3>调试轨迹</h3>
      <div class="row">
        <el-button size="small" @click="debug.clearSteps()">清空</el-button>
      </div>
    </div>
    <CopyableCode v-if="debug.steps.length" :value="debug.exportTraceJson()" />
    <DebugStepCard v-for="step in debug.steps" :key="step.id" :step="step" />
    <el-empty v-if="!debug.steps.length" description="暂无请求轨迹" />
  </div>
</template>
