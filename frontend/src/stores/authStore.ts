import { defineStore } from 'pinia'
import { loginUser, registerUser } from '@/api/users'
import type { DemoUser } from '@/types/user'

const STORAGE_KEY = 'rbft-demo-user'

export const useAuthStore = defineStore('auth', {
  state: () => ({
    user: JSON.parse(localStorage.getItem(STORAGE_KEY) || 'null') as DemoUser | null
  }),
  actions: {
    async register(nodeId: string, username: string, password: string) {
      this.user = await registerUser(nodeId, username, password)
      this.user.username = username
      localStorage.setItem(STORAGE_KEY, JSON.stringify(this.user))
    },
    async login(nodeId: string, username: string, password: string) {
      this.user = await loginUser(nodeId, username, password)
      this.user.username = username
      localStorage.setItem(STORAGE_KEY, JSON.stringify(this.user))
    },
    logout() {
      this.user = null
      localStorage.removeItem(STORAGE_KEY)
    }
  }
})
