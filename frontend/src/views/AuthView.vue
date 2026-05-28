<script setup lang="ts">
import { reactive } from 'vue'
import { useAuthStore } from '@/stores/authStore'
import { useNodeStore } from '@/stores/nodeStore'
import JsonViewer from '@/components/common/JsonViewer.vue'

const auth = useAuthStore()
const nodes = useNodeStore()
const form = reactive({ username: 'alice', password: 'pass' })
</script>

<template>
  <div class="page">
    <div class="page-title"><h2>注册登录</h2><p class="muted">private_key 仅用于课堂 demo，真实系统不得由 API 返回私钥。</p></div>
    <section class="panel">
      <el-form label-position="top" class="form-grid">
        <el-form-item label="用户名"><el-input v-model="form.username" /></el-form-item>
        <el-form-item label="密码"><el-input v-model="form.password" type="password" show-password /></el-form-item>
      </el-form>
      <div class="row">
        <el-button type="primary" @click="auth.register(nodes.selectedNodeId, form.username, form.password)">注册</el-button>
        <el-button @click="auth.login(nodes.selectedNodeId, form.username, form.password)">登录</el-button>
        <el-button @click="auth.logout()">退出</el-button>
      </div>
    </section>
    <el-alert type="warning" title="Demo-only 风险：private_key 暴露给前端只是为了演示交易签名流程。" show-icon />
    <section class="panel"><h3>当前登录态</h3><JsonViewer :value="auth.user" /></section>
  </div>
</template>
