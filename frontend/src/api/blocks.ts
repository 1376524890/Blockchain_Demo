import { request } from './http'
import type { Block } from '@/types/block'

export const getLatestBlock = (nodeId: string) =>
  request<Block>(nodeId, { method: 'GET', url: '/api/blocks/latest' }, {
    title: '读取最新区块',
    description: '获取最新提交区块头和交易列表',
    algorithm: 'storage'
  })

export const getBlockByHeight = (nodeId: string, height: number) =>
  request<Block>(nodeId, { method: 'GET', url: `/api/blocks/${height}` })
