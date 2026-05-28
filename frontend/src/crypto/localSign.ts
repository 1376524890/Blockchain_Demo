import { bytesToHex, ensureSodium, hexToBytes, sodium } from './sodium'

export async function signDetachedHex(message: string, privateKeyHex: string) {
  await ensureSodium()
  const signature = sodium.crypto_sign_detached(message, hexToBytes(privateKeyHex))
  return bytesToHex(signature)
}
