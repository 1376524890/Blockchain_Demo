<script setup lang="ts">
import HashText from '@/components/common/HashText.vue'
defineProps<{ signature?: string; txId?: string }>()
</script>

<template>
  <section class="panel">
    <h3>签名与 tx_id</h3>
    <div class="kv"><span>signature</span><HashText :value="signature" /></div>
    <div class="kv"><span>tx_id</span><HashText :value="txId" /></div>
  </section>
</template>
