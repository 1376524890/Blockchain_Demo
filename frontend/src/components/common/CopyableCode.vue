<script setup lang="ts">
const props = defineProps<{ value: string }>()
const copy = () => navigator.clipboard?.writeText(props.value)
</script>

<template>
  <div class="code-box">
    <button class="copy-btn" type="button" @click="copy">复制</button>
    <pre><code>{{ value }}</code></pre>
  </div>
</template>
