export interface Transaction {
  tx_id?: string
  type: string
  from: string
  to?: string
  amount?: number
  data_hash?: string
  nonce: number
  timestamp: number
  public_key: string
  signature?: string
}

export interface TxSerializeDebug {
  tx_body: string
  body_hash: string
  field_order: string[]
  note: string
}
