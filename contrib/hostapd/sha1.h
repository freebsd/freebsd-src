#ifndef SHA1_H
#define SHA1_H

#ifdef EAP_TLS_FUNCS

#include <openssl/sha.h>

#define SHA1_CTX SHA_CTX
#define SHA1Init SHA1_Init
#define SHA1Update SHA1_Update
#define SHA1Final SHA1_Final
#define SHA1Transform SHA1_Transform
#define SHA1_MAC_LEN SHA_DIGEST_LENGTH

#else /* EAP_TLS_FUNCS */

#define SHA1_MAC_LEN 20

typedef struct {
	u32 state[5];
	u32 count[2];
	unsigned char buffer[64];
} SHA1_CTX;

void SHA1Init(SHA1_CTX *context);
void SHA1Update(SHA1_CTX *context, const void *data, u32 len);
void SHA1Final(unsigned char digest[20], SHA1_CTX* context);
void SHA1Transform(u32 state[5], const unsigned char buffer[64]);

#endif /* EAP_TLS_FUNCS */

void sha1_mac(const u8 *key, size_t key_len, const u8 *data, size_t data_len,
	      u8 *mac);
void hmac_sha1_vector(const u8 *key, size_t key_len, size_t num_elem,
		      const u8 *addr[], const size_t *len, u8 *mac);
void hmac_sha1(const u8 *key, size_t key_len, const u8 *data, size_t data_len,
	       u8 *mac);
void sha1_prf(const u8 *key, size_t key_len, const char *label,
	      const u8 *data, size_t data_len, u8 *buf, size_t buf_len);
void sha1_t_prf(const u8 *key, size_t key_len, const char *label,
		const u8 *seed, size_t seed_len, u8 *buf, size_t buf_len);
int tls_prf(const u8 *secret, size_t secret_len, const char *label,
	    const u8 *seed, size_t seed_len, u8 *out, size_t outlen);
void pbkdf2_sha1(const char *passphrase, const char *ssid, size_t ssid_len,
		 int iterations, u8 *buf, size_t buflen);
void sha1_transform(u8 *state, u8 data[64]);
void sha1_vector(size_t num_elem, const u8 *addr[], const size_t *len,
		 u8 *mac);

#endif /* SHA1_H */
