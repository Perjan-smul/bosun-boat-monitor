// mbedtls-AES voor de pc-tests, via OpenSSL (op het board zit mbedtls in de ESP-IDF).
// Alleen wat sensors.cpp gebruikt: AES-128 in CTR-modus.
#pragma once
#include <openssl/evp.h>
#include <stddef.h>
#include <string.h>

struct mbedtls_aes_context {
  unsigned char key[32];
  int bits;
};

inline void mbedtls_aes_init(mbedtls_aes_context *c) { memset(c, 0, sizeof(*c)); }
inline void mbedtls_aes_free(mbedtls_aes_context *) {}
inline int mbedtls_aes_setkey_enc(mbedtls_aes_context *c, const unsigned char *key, unsigned bits) {
  memcpy(c->key, key, bits / 8);
  c->bits = bits;
  return 0;
}

// Eén blok versleutelen (ECB) met OpenSSL
inline void aes_block(const mbedtls_aes_context *c, const unsigned char in[16], unsigned char out[16]) {
  EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
  int n = 0;
  EVP_EncryptInit_ex(ctx, EVP_aes_128_ecb(), nullptr, c->key, nullptr);
  EVP_CIPHER_CTX_set_padding(ctx, 0);
  EVP_EncryptUpdate(ctx, out, &n, in, 16);
  EVP_CIPHER_CTX_free(ctx);
}

// Zelfde gedrag als mbedtls: teller big-endian ophogen, nc_off en stream_block bijhouden
inline int mbedtls_aes_crypt_ctr(mbedtls_aes_context *c, size_t length, size_t *nc_off, unsigned char nonce_counter[16],
                                 unsigned char stream_block[16], const unsigned char *input, unsigned char *output) {
  size_t n = *nc_off;
  for (size_t i = 0; i < length; i++) {
    if (n == 0) {
      aes_block(c, nonce_counter, stream_block);
      for (int k = 16; k > 0; k--)
        if (++nonce_counter[k - 1] != 0) break;
    }
    output[i] = input[i] ^ stream_block[n];
    n = (n + 1) & 0x0F;
  }
  *nc_off = n;
  return 0;
}
