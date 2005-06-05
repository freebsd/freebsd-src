#ifndef MD5_H
#define MD5_H

#ifdef EAP_TLS_FUNCS

#include <openssl/md5.h>

#define MD5Init MD5_Init
#define MD5Update MD5_Update
#define MD5Final MD5_Final
#define MD5Transform MD5_Transform

#define MD5_MAC_LEN MD5_DIGEST_LENGTH

#else /* EAP_TLS_FUNCS */

#define MD5_MAC_LEN 16

struct MD5Context {
	u32 buf[4];
	u32 bits[2];
	u8 in[64];
};

void MD5Init(struct MD5Context *context);
void MD5Update(struct MD5Context *context, unsigned char const *buf,
	       unsigned len);
void MD5Final(unsigned char digest[16], struct MD5Context *context);
void MD5Transform(u32 buf[4], u32 const in[16]);

typedef struct MD5Context MD5_CTX;

#endif /* EAP_TLS_FUNCS */


void md5_mac(const u8 *key, size_t key_len, const u8 *data, size_t data_len,
	     u8 *mac);
void hmac_md5_vector(const u8 *key, size_t key_len, size_t num_elem,
		     const u8 *addr[], const size_t *len, u8 *mac);
void hmac_md5(const u8 *key, size_t key_len, const u8 *data, size_t data_len,
	      u8 *mac);

#endif /* MD5_H */
