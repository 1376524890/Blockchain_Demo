export interface ConsensusStatus {
  node_id: string
  attack_mode: string
  running: boolean
  quorum: number
  evidence_count: number
}

export interface NodeStatus {
  node_id: string
  rest_port: number
  p2p_port: number
  latest_height: string
  consensus: ConsensusStatus
}

export interface ConsensusEvent {
  id: number
  timestamp: number
  node_id: string
  height: number
  view: number
  instance_id: number
  event_type: string
  from: string
  to: string
  block_hash: string
  accepted: boolean
  reason: string
  attack_mode: string
}
