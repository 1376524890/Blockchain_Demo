import axios, { type AxiosRequestConfig } from 'axios'
import type { ApiResponse, NodeEndpoint } from '@/types/api'
import { useDebugStore } from '@/stores/debugStore'

export const NODE_ENDPOINTS: NodeEndpoint[] = [
  { id: 'node1', baseURL: '/node-api/node1' },
  { id: 'node2', baseURL: '/node-api/node2' },
  { id: 'node3', baseURL: '/node-api/node3' },
  { id: 'node4', baseURL: '/node-api/node4' }
]

export async function request<T>(
  nodeId: string,
  config: AxiosRequestConfig,
  trace?: { title: string; description: string; algorithm?: 'user' | 'signature' | 'hash' | 'merkle' | 'smt' | 'rbft' | 'storage' | 'api'; input?: unknown }
): Promise<T> {
  const endpoint = NODE_ENDPOINTS.find((item) => item.id === nodeId) ?? NODE_ENDPOINTS[0]
  const method = (config.method ?? 'GET').toUpperCase()
  const url = `${endpoint.baseURL}${config.url}`
  const debug = useDebugStore()
  const stepId = trace
    ? debug.addStep({
        title: trace.title,
        description: trace.description,
        algorithm: trace.algorithm ?? 'api',
        input: trace.input,
        request: { method, url, body: config.data }
      })
    : ''
  if (stepId) debug.startStep(stepId)
  const startedAt = performance.now()
  try {
    const response = await axios.request<ApiResponse<T>>({ ...config, baseURL: endpoint.baseURL, timeout: 8000 })
    if (!response.data.ok) {
      throw new Error(response.data.error || 'request failed')
    }
    if (stepId) debug.finishStep(stepId, response.data.data, response.data)
    return response.data.data as T
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error)
    if (stepId) debug.failStep(stepId, message, { durationMs: Math.round(performance.now() - startedAt) })
    throw new Error(message)
  }
}
