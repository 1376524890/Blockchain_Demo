<script setup lang="ts">
defineProps<{ disabled?: boolean }>()
const form = defineModel<{ to: string; amount: number; nonce: number; mode: 'frontend_sign' | 'backend_demo_sign' }>({ required: true })
</script>

<template>
  <el-form label-position="top" class="form-grid">
    <el-form-item label="收款地址"><el-input v-model="form.to" /></el-form-item>
    <el-form-item label="金额"><el-input-number v-model="form.amount" :min="1" /></el-form-item>
    <el-form-item label="Nonce"><el-input-number v-model="form.nonce" :min="1" /></el-form-item>
    <el-form-item label="签名模式">
      <el-segmented v-model="form.mode" :options="['frontend_sign', 'backend_demo_sign']" />
    </el-form-item>
  </el-form>
</template>
