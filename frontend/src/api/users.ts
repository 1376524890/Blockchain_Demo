import { request } from './http'
import type { DemoUser } from '@/types/user'

export const registerUser = (nodeId: string, username: string, password: string) =>
  request<DemoUser>(nodeId, { method: 'POST', url: '/api/users/register', data: { username, password } }, {
    title: '注册用户',
    description: '后端生成 demo 用户地址和 Ed25519 密钥',
    algorithm: 'user',
    input: { username }
  })

export const loginUser = (nodeId: string, username: string, password: string) =>
  request<DemoUser>(nodeId, { method: 'POST', url: '/api/users/login', data: { username, password } }, {
    title: '登录用户',
    description: '获取演示 token、地址、公钥和 demo-only 私钥',
    algorithm: 'user',
    input: { username }
  })
