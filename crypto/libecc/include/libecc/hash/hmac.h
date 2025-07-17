#ifndef __HMAC_H__
#define __HMAC_H__

#include <libecc/lib_ecc_config.h>
#ifdef WITH_HMAC

#include <libecc/words/words.h>
#include <libecc/utils/utils.h>
#include <libecc/hash/hash_algs.h>

#define HMAC_MAGIC ((word_t)(0x9849020187612083ULL))
#define HMAC_CHECK_INITIALIZED(A, ret, err) \
        MUST_HAVE((((void *)(A)) != NULL) && ((A)->magic == HMAC_MAGIC) && ((A)->hash != NULL), ret, err)

/* The HMAC structure is made of two hash contexts */
typedef struct {
	/* The hash mapping associated with the hmac */
        const hash_mapping *hash;
	/* The two hash contexts (inner and outer) */
        hash_context in_ctx;
        hash_context out_ctx;
        /* Initialization magic value */
        word_t magic;
} hmac_context;

ATTRIBUTE_WARN_UNUSED_RET int hmac_init(hmac_context *ctx, const u8 *hmackey, u32 hmackey_len, hash_alg_type hash_type);

ATTRIBUTE_WARN_UNUSED_RET int hmac_update(hmac_context *ctx, const u8 *input, u32 ilen);

ATTRIBUTE_WARN_UNUSED_RET int hmac_finalize(hmac_context *ctx, u8 *output, u8 *outlen);

ATTRIBUTE_WARN_UNUSED_RET int hmac(const u8 *hmackey, u32 hmackey_len, hash_alg_type hash_type, const u8 *input, u32 ilen, u8 *output, u8 *outlen);

ATTRIBUTE_WARN_UNUSED_RET int hmac_scattered(const u8 *hmackey, u32 hmackey_len, hash_alg_type hash_type, const u8 **inputs, const u32 *ilens, u8 *output, u8 *outlen);

#endif /* WITH_HMAC */

#endif /* __HMAC_H__ */
