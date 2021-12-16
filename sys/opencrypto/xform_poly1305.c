/* This file is in the public domain. */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <opencrypto/xform_auth.h>

#include <sodium/crypto_onetimeauth_poly1305.h>

struct poly1305_xform_ctx {
	struct crypto_onetimeauth_poly1305_state state;
};
CTASSERT(sizeof(union authctx) >= sizeof(struct poly1305_xform_ctx));

CTASSERT(POLY1305_KEY_LEN == crypto_onetimeauth_poly1305_KEYBYTES);
CTASSERT(POLY1305_HASH_LEN == crypto_onetimeauth_poly1305_BYTES);
CTASSERT(POLY1305_BLOCK_LEN == crypto_onetimeauth_poly1305_BYTES);

static void
xform_Poly1305_Init(void *polyctx)
{
	/* Nop */
}

static void
xform_Poly1305_Setkey(void *ctx, const uint8_t *key, u_int klen)
{
	struct poly1305_xform_ctx *polyctx = ctx;
	int rc;

	if (klen != POLY1305_KEY_LEN)
		panic("%s: Bogus keylen: %u bytes", __func__, (unsigned)klen);

	rc = crypto_onetimeauth_poly1305_init(&polyctx->state, key);
	if (rc != 0)
		panic("%s: Invariant violated: %d", __func__, rc);
}

static int
xform_Poly1305_Update(void *ctx, const void *data, u_int len)
{
	struct poly1305_xform_ctx *polyctx = ctx;
	int rc;

	rc = crypto_onetimeauth_poly1305_update(&polyctx->state, data, len);
	if (rc != 0)
		panic("%s: Invariant violated: %d", __func__, rc);
	return (0);
}

static void
xform_Poly1305_Final(uint8_t *digest, void *ctx)
{
	struct poly1305_xform_ctx *polyctx = ctx;
	int rc;

	rc = crypto_onetimeauth_poly1305_final(&polyctx->state, digest);
	if (rc != 0)
		panic("%s: Invariant violated: %d", __func__, rc);
}

const struct auth_hash auth_hash_poly1305 = {
	.type = CRYPTO_POLY1305,
	.name = "Poly-1305",
	.keysize = POLY1305_KEY_LEN,
	.hashsize = POLY1305_HASH_LEN,
	.ctxsize = sizeof(struct poly1305_xform_ctx),
	.blocksize = POLY1305_BLOCK_LEN,
	.Init = xform_Poly1305_Init,
	.Setkey = xform_Poly1305_Setkey,
	.Update = xform_Poly1305_Update,
	.Final = xform_Poly1305_Final,
};
