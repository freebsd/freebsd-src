/* This file is in the public domain. */

#include <sys/cdefs.h>
#include <contrib/libb2/blake2.h>
#include <opencrypto/xform_auth.h>

extern int blake2b_init_ref(blake2b_state *S, size_t outlen);
extern int blake2b_init_param_ref(blake2b_state *S, const blake2b_param *P);
extern int blake2b_init_key_ref(blake2b_state *S, size_t outlen,
    const void *key, size_t keylen);
extern int blake2b_update_ref(blake2b_state *S, const uint8_t *in,
    size_t inlen);
extern int blake2b_final_ref(blake2b_state *S, uint8_t *out, size_t outlen);
extern int blake2b_ref(uint8_t *out, const void *in, const void *key,
    size_t outlen, size_t inlen, size_t keylen);

extern int blake2s_init_ref(blake2s_state *S, size_t outlen);
extern int blake2s_init_param_ref(blake2s_state *S, const blake2s_param *P);
extern int blake2s_init_key_ref(blake2s_state *S, size_t outlen,
    const void *key, size_t keylen);
extern int blake2s_update_ref(blake2s_state *S, const uint8_t *in,
    size_t inlen);
extern int blake2s_final_ref(blake2s_state *S, uint8_t *out, size_t outlen);
extern int blake2s_ref(uint8_t *out, const void *in, const void *key,
    size_t outlen, size_t inlen, size_t keylen);

struct blake2b_xform_ctx {
	blake2b_state state;
};
CTASSERT(sizeof(union authctx) >= sizeof(struct blake2b_xform_ctx));

static void
blake2b_xform_init(void *vctx)
{
	struct blake2b_xform_ctx *ctx = vctx;
	int rc;

	rc = blake2b_init_ref(&ctx->state, BLAKE2B_OUTBYTES);
	if (rc != 0)
		panic("blake2b_init: invalid arguments");
}

static void
blake2b_xform_setkey(void *vctx, const uint8_t *key, u_int klen)
{
	struct blake2b_xform_ctx *ctx = vctx;
	int rc;

	rc = blake2b_init_key_ref(&ctx->state, BLAKE2B_OUTBYTES, key,
	    klen);
	if (rc != 0)
		panic("blake2b_init_key: invalid arguments");
}

static int
blake2b_xform_update(void *vctx, const void *data, u_int len)
{
	struct blake2b_xform_ctx *ctx = vctx;
	int rc;

	rc = blake2b_update_ref(&ctx->state, data, len);
	if (rc != 0)
		return (EINVAL);
	return (0);
}

static void
blake2b_xform_final(uint8_t *out, void *vctx)
{
	struct blake2b_xform_ctx *ctx = vctx;
	int rc;

	rc = blake2b_final_ref(&ctx->state, out, BLAKE2B_OUTBYTES);
	if (rc != 0)
		panic("blake2b_final: invalid");
}

const struct auth_hash auth_hash_blake2b = {
	.type = CRYPTO_BLAKE2B,
	.name = "Blake2b",
	.keysize = BLAKE2B_KEYBYTES,
	.hashsize = BLAKE2B_OUTBYTES,
	.ctxsize = sizeof(struct blake2b_xform_ctx),
	.Setkey = blake2b_xform_setkey,
	.Init = blake2b_xform_init,
	.Update = blake2b_xform_update,
	.Final = blake2b_xform_final,
};

struct blake2s_xform_ctx {
	blake2s_state state;
};
CTASSERT(sizeof(union authctx) >= sizeof(struct blake2s_xform_ctx));

static void
blake2s_xform_init(void *vctx)
{
	struct blake2s_xform_ctx *ctx = vctx;
	int rc;

	rc = blake2s_init_ref(&ctx->state, BLAKE2S_OUTBYTES);
	if (rc != 0)
		panic("blake2s_init: invalid arguments");
}

static void
blake2s_xform_setkey(void *vctx, const uint8_t *key, u_int klen)
{
	struct blake2s_xform_ctx *ctx = vctx;
	int rc;

	rc = blake2s_init_key_ref(&ctx->state, BLAKE2S_OUTBYTES, key,
	    klen);
	if (rc != 0)
		panic("blake2s_init_key: invalid arguments");
}

static int
blake2s_xform_update(void *vctx, const void *data, u_int len)
{
	struct blake2s_xform_ctx *ctx = vctx;
	int rc;

	rc = blake2s_update_ref(&ctx->state, data, len);
	if (rc != 0)
		return (EINVAL);
	return (0);
}

static void
blake2s_xform_final(uint8_t *out, void *vctx)
{
	struct blake2s_xform_ctx *ctx = vctx;
	int rc;

	rc = blake2s_final_ref(&ctx->state, out, BLAKE2S_OUTBYTES);
	if (rc != 0)
		panic("blake2s_final: invalid");
}

const struct auth_hash auth_hash_blake2s = {
	.type = CRYPTO_BLAKE2S,
	.name = "Blake2s",
	.keysize = BLAKE2S_KEYBYTES,
	.hashsize = BLAKE2S_OUTBYTES,
	.ctxsize = sizeof(struct blake2s_xform_ctx),
	.Setkey = blake2s_xform_setkey,
	.Init = blake2s_xform_init,
	.Update = blake2s_xform_update,
	.Final = blake2s_xform_final,
};
