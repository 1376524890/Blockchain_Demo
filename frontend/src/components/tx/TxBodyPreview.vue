<script setup lang="ts">
import JsonViewer from '@/components/common/JsonViewer.vue'
defineProps<{ backend?: unknown; localBody?: string; localHash?: string }>()
</script>

<template>
  <section class="panel">
    <h3>签名体预览</h3>
    <p class="muted">type|from|to|amount|data_hash|nonce|timestamp|public_key</p>
    <div class="kv"><span>本地 tx_body</span><code>{{ localBody || '-' }}</code></div>
    <div class="kv"><span>本地 SHA-256</span><code>{{ localHash || '-' }}</code></div>
    <JsonViewer v-if="backend" :value="backend" />
  </section>
</template>
