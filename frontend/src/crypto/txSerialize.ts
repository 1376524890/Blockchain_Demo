export interface TxBodyFields {
  type: string
  from: string
  to: string
  amount: number
  data_hash: string
  nonce: number
  timestamp: number
  public_key: string
}

export function serializeTxBody(tx: TxBodyFields) {
  return `${tx.type}|${tx.from}|${tx.to}|${tx.amount}|${tx.data_hash}|${tx.nonce}|${tx.timestamp}|${tx.public_key}`
}

export async function sha256Hex(input: string) {
  const data = new TextEncoder().encode(input)
  const digest = await crypto.subtle.digest('SHA-256', data)
  return Array.from(new Uint8Array(digest)).map((byte) => byte.toString(16).padStart(2, '0')).join('')
}
