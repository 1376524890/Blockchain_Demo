declare module 'libsodium-wrappers-sumo' {
  const sodium: {
    ready: Promise<void>
    crypto_sign_detached(message: string | Uint8Array, secretKey: Uint8Array): Uint8Array
  }
  export default sodium
}
