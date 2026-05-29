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
    port: 5173,
    proxy: {
      '/node-api/node1': {
        target: 'http://localhost:8001',
        changeOrigin: true,
        rewrite: (path) => path.replace(/^\/node-api\/node1/, '')
      },
      '/node-api/node2': {
        target: 'http://localhost:8002',
        changeOrigin: true,
        rewrite: (path) => path.replace(/^\/node-api\/node2/, '')
      },
      '/node-api/node3': {
        target: 'http://localhost:8003',
        changeOrigin: true,
        rewrite: (path) => path.replace(/^\/node-api\/node3/, '')
      },
      '/node-api/node4': {
        target: 'http://localhost:8004',
        changeOrigin: true,
        rewrite: (path) => path.replace(/^\/node-api\/node4/, '')
      }
    }
  }
})
