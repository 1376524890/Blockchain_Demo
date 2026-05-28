import { createRouter, createWebHistory } from 'vue-router'

const routes = [
  { path: '/', name: 'dashboard', component: () => import('@/views/DashboardView.vue') },
  { path: '/auth', name: 'auth', component: () => import('@/views/AuthView.vue') },
  { path: '/tx', name: 'tx', component: () => import('@/views/TxDebugView.vue') },
  { path: '/explorer', name: 'explorer', component: () => import('@/views/ExplorerView.vue') },
  { path: '/merkle', name: 'merkle', component: () => import('@/views/MerkleDebugView.vue') },
  { path: '/smt', name: 'smt', component: () => import('@/views/SMTDebugView.vue') },
  { path: '/rbft', name: 'rbft', component: () => import('@/views/RBFTDebugView.vue') },
  { path: '/attack', name: 'attack', component: () => import('@/views/AttackLabView.vue') }
]

export default createRouter({ history: createWebHistory(), routes })
