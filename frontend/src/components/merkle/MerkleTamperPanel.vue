<script setup lang="ts">
const tamper = defineModel<any>({ required: true })
defineEmits<{ verify: [] }>()
</script>

<template>
  <section class="panel danger-panel">
    <h3>篡改验证</h3>
    <el-form label-position="top">
      <el-form-item label="tx.amount"><el-input-number v-model="tamper.amount" :min="0" /></el-form-item>
      <el-form-item label="proof[0].hash"><el-input v-model="tamper.hash" /></el-form-item>
      <el-form-item label="proof[0].position"><el-select v-model="tamper.position"><el-option label="LEFT" value="LEFT" /><el-option label="RIGHT" value="RIGHT" /></el-select></el-form-item>
      <el-button type="danger" @click="$emit('verify')">重新验证</el-button>
    </el-form>
  </section>
</template>
