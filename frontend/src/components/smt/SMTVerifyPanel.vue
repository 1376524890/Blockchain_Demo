<script setup lang="ts">
defineProps<{ valid?: boolean | null; note?: string }>()
</script>

<template>
  <section class="panel">
    <h3>SMT 验证</h3>
    <el-alert :type="valid ? 'success' : valid === false ? 'error' : 'info'" :title="valid === null || valid === undefined ? '等待验证' : valid ? 'valid=true' : 'valid=false'" show-icon />
    <p class="muted">{{ note }}</p>
  </section>
</template>
