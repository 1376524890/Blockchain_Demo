export interface ApiResponse<T> {
  ok: boolean
  data: T | null
  error: string
}

export interface NodeEndpoint {
  id: string
  baseURL: string
}
