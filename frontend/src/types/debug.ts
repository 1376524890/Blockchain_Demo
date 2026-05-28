export type DebugAlgorithm = 'user' | 'signature' | 'hash' | 'merkle' | 'smt' | 'rbft' | 'storage' | 'api'

export interface DebugStep {
  id: string
  title: string
  status: 'pending' | 'running' | 'success' | 'error'
  description: string
  algorithm: DebugAlgorithm
  input?: unknown
  output?: unknown
  request?: {
    method: string
    url: string
    body?: unknown
  }
  response?: unknown
  error?: string
  startedAt?: number
  endedAt?: number
  durationMs?: number
}
