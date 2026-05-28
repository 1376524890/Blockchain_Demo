<script setup lang="ts">
import CopyableCode from './CopyableCode.vue'
const props = defineProps<{ value: unknown }>()
</script>

<template>
  <CopyableCode :value="JSON.stringify(props.value, null, 2)" />
</template>
