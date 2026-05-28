<script setup lang="ts">
import { ref, computed } from 'vue'

const props = defineProps<{ value?: string; chars?: number }>()
const expanded = ref(false)
const text = computed(() => props.value || '')
const shown = computed(() => expanded.value || text.value.length <= (props.chars ?? 18)
  ? text.value
  : `${text.value.slice(0, props.chars ?? 18)}...${text.value.slice(-8)}`)
</script>

<template>
  <button class="hash-text" type="button" @click="expanded = !expanded">{{ shown || '-' }}</button>
</template>
