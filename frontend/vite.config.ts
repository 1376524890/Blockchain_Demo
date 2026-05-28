import { fileURLToPath, URL } from 'node:url'
import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'

export default defineConfig({
  plugins: [vue()],
  build: {
    target: 'esnext'
  },
  optimizeDeps: {
    exclude: ['libsodium-wrappers-sumo', 'libsodium-sumo']
  },
  resolve: {
    alias: {
      '@': fileURLToPath(new URL('./src', import.meta.url)),
      './libsodium-sumo.mjs': fileURLToPath(new URL('./node_modules/libsodium-sumo/dist/modules-sumo-esm/libsodium-sumo.mjs', import.meta.url))
    }
  },
  server: {
    port: 5173
  }
})
