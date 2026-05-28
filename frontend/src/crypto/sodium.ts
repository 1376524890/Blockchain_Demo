import sodium from 'libsodium-wrappers-sumo'

let ready: Promise<void> | null = null

export function ensureSodium() {
  ready ??= sodium.ready
  return ready
}

export function hexToBytes(hex: string) {
  return Uint8Array.from(hex.match(/.{1,2}/g)?.map((byte) => Number.parseInt(byte, 16)) ?? [])
}

export function bytesToHex(bytes: Uint8Array) {
  return Array.from(bytes).map((byte) => byte.toString(16).padStart(2, '0')).join('')
}

export { sodium }
