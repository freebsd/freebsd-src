/* This file is in the public domain. */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <crypto/chacha20/chacha.h>
#include <opencrypto/xform_enc.h>

static int
chacha20_xform_setkey(void *ctx, const uint8_t *key, int len)
{

	if (len != CHACHA_MINKEYLEN && len != 32)
		return (EINVAL);

	chacha_keysetup(ctx, key, len * 8);
	return (0);
}

static void
chacha20_xform_reinit(void *ctx, const uint8_t *iv, size_t ivlen)
{
	KASSERT(ivlen == CHACHA_NONCELEN + CHACHA_CTRLEN,
	    ("%s: invalid IV length", __func__));
	chacha_ivsetup(ctx, iv + 8, iv);
}

static void
chacha20_xform_crypt(void *ctx, const uint8_t *in, uint8_t *out)
{

	chacha_encrypt_bytes(ctx, in, out, CHACHA_BLOCKLEN);
}

static void
chacha20_xform_crypt_last(void *ctx, const uint8_t *in, uint8_t *out,
    size_t len)
{

	chacha_encrypt_bytes(ctx, in, out, len);
}

const struct enc_xform enc_xform_chacha20 = {
	.type = CRYPTO_CHACHA20,
	.name = "chacha20",
	.ctxsize = sizeof(struct chacha_ctx),
	.blocksize = 1,
	.native_blocksize = CHACHA_BLOCKLEN,
	.ivsize = CHACHA_NONCELEN + CHACHA_CTRLEN,
	.minkey = CHACHA_MINKEYLEN,
	.maxkey = 32,
	.encrypt = chacha20_xform_crypt,
	.decrypt = chacha20_xform_crypt,
	.setkey = chacha20_xform_setkey,
	.reinit = chacha20_xform_reinit,
	.encrypt_last = chacha20_xform_crypt_last,
	.decrypt_last = chacha20_xform_crypt_last,
};
