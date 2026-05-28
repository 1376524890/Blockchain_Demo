<script setup lang="ts">
defineProps<{ siblings?: string[] }>()
</script>

<template>
  <el-table :data="(siblings || []).map((hash, index) => ({ index, hash }))" size="small" height="260">
    <el-table-column prop="index" label="#" width="80" />
    <el-table-column prop="hash" label="Sibling" />
  </el-table>
</template>
