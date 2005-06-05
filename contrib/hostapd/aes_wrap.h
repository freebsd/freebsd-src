#ifndef AES_WRAP_H
#define AES_WRAP_H

void aes_wrap(u8 *kek, int n, u8 *plain, u8 *cipher);
int aes_unwrap(u8 *kek, int n, u8 *cipher, u8 *plain);
void omac1_aes_128(const u8 *key, const u8 *data, size_t data_len, u8 *mac);
void aes_128_encrypt_block(const u8 *key, const u8 *in, u8 *out);
void aes_128_ctr_encrypt(const u8 *key, const u8 *nonce,
			 u8 *data, size_t data_len);
int aes_128_eax_encrypt(const u8 *key, const u8 *nonce, size_t nonce_len,
			const u8 *hdr, size_t hdr_len,
			u8 *data, size_t data_len, u8 *tag);
int aes_128_eax_decrypt(const u8 *key, const u8 *nonce, size_t nonce_len,
			const u8 *hdr, size_t hdr_len,
			u8 *data, size_t data_len, const u8 *tag);
void aes_128_cbc_encrypt(const u8 *key, const u8 *iv, u8 *data,
			 size_t data_len);
void aes_128_cbc_decrypt(const u8 *key, const u8 *iv, u8 *data,
			 size_t data_len);

#endif /* AES_WRAP_H */
