<script setup lang="ts">
defineProps<{ modelValue: string }>()
defineEmits<{ 'update:modelValue': [value: string] }>()
const modes = ['normal', 'bad_merkle_root', 'bad_state_root', 'invalid_block_hash', 'invalid_node_signature', 'drop_prepare', 'drop_commit', 'delay_preprepare', 'double_proposal', 'node_crash_simulated', 'equivocation_prepare', 'replay_old_message']
</script>

<template>
  <el-select :model-value="modelValue" filterable @update:model-value="$emit('update:modelValue', $event)">
    <el-option v-for="mode in modes" :key="mode" :label="mode" :value="mode" />
  </el-select>
</template>
