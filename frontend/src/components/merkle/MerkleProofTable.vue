<script setup lang="ts">
defineProps<{ proof?: any[] }>()
</script>

<template>
  <el-table :data="proof || []" size="small">
    <el-table-column prop="position" label="Position" width="120" />
    <el-table-column prop="hash" label="Sibling Hash" />
  </el-table>
</template>
