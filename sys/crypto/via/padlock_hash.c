/*-
 * Copyright (c) 2006 Pawel Jakub Dawidek <pjd@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/libkern.h>
#include <sys/endian.h>
#include <sys/pcpu.h>
#if defined(__amd64__) || defined(__i386__)
#include <machine/cpufunc.h>
#include <machine/cputypes.h>
#include <machine/md_var.h>
#include <machine/specialreg.h>
#endif
#include <machine/pcb.h>

#include <opencrypto/cryptodev.h>
#include <opencrypto/xform.h>

#include <crypto/via/padlock.h>

/*
 * Implementation notes.
 *
 * Some VIA CPUs provides SHA1 and SHA256 acceleration.
 * We implement all HMAC algorithms provided by crypto(9) framework, but we do
 * the crypto work in software unless this is HMAC/SHA1 or HMAC/SHA256 and
 * our CPU can accelerate it.
 *
 * Additional CPU instructions, which preform SHA1 and SHA256 are one-shot
 * functions - we have only one chance to give the data, CPU itself will add
 * the padding and calculate hash automatically.
 * This means, it is not possible to implement common init(), update(), final()
 * methods.
 * The way I've choosen is to keep adding data to the buffer on update()
 * (reallocating the buffer if necessary) and call XSHA{1,256} instruction on
 * final().
 */

struct padlock_sha_ctx {
	uint8_t	*psc_buf;
	int	 psc_offset;
	int	 psc_size;
};
CTASSERT(sizeof(struct padlock_sha_ctx) <= sizeof(union authctx));

static void padlock_sha_init(void *vctx);
static int padlock_sha_update(void *vctx, const void *buf, u_int bufsize);
static void padlock_sha1_final(uint8_t *hash, void *vctx);
static void padlock_sha256_final(uint8_t *hash, void *vctx);

static struct auth_hash padlock_hmac_sha1 = {
	.type = CRYPTO_SHA1_HMAC,
	.name = "HMAC-SHA1",
	.keysize = SHA1_BLOCK_LEN,
	.hashsize = SHA1_HASH_LEN,
	.ctxsize = sizeof(struct padlock_sha_ctx),
	.blocksize = SHA1_BLOCK_LEN,
        .Init = padlock_sha_init,
	.Update = padlock_sha_update,
	.Final = padlock_sha1_final,
};

static struct auth_hash padlock_hmac_sha256 = {
	.type = CRYPTO_SHA2_256_HMAC,
	.name = "HMAC-SHA2-256",
	.keysize = SHA2_256_BLOCK_LEN,
	.hashsize = SHA2_256_HASH_LEN,
	.ctxsize = sizeof(struct padlock_sha_ctx),
	.blocksize = SHA2_256_BLOCK_LEN,
        .Init = padlock_sha_init,
	.Update = padlock_sha_update,
	.Final = padlock_sha256_final,
};

MALLOC_DECLARE(M_PADLOCK);

static __inline void
padlock_output_block(uint32_t *src, uint32_t *dst, size_t count)
{

	while (count-- > 0)
		*dst++ = bswap32(*src++);
}

static void
padlock_do_sha1(const u_char *in, u_char *out, int count)
{
	u_char buf[128+16];	/* PadLock needs at least 128 bytes buffer. */
	u_char *result = PADLOCK_ALIGN(buf);

	((uint32_t *)result)[0] = 0x67452301;
	((uint32_t *)result)[1] = 0xEFCDAB89;
	((uint32_t *)result)[2] = 0x98BADCFE;
	((uint32_t *)result)[3] = 0x10325476;
	((uint32_t *)result)[4] = 0xC3D2E1F0;

#ifdef __GNUCLIKE_ASM
	__asm __volatile(
		".byte  0xf3, 0x0f, 0xa6, 0xc8" /* rep xsha1 */
			: "+S"(in), "+D"(result)
			: "c"(count), "a"(0)
		);
#endif

	padlock_output_block((uint32_t *)result, (uint32_t *)out,
	    SHA1_HASH_LEN / sizeof(uint32_t));
}

static void
padlock_do_sha256(const char *in, char *out, int count)
{
	char buf[128+16];	/* PadLock needs at least 128 bytes buffer. */
	char *result = PADLOCK_ALIGN(buf);

	((uint32_t *)result)[0] = 0x6A09E667;
	((uint32_t *)result)[1] = 0xBB67AE85;
	((uint32_t *)result)[2] = 0x3C6EF372;
	((uint32_t *)result)[3] = 0xA54FF53A;
	((uint32_t *)result)[4] = 0x510E527F;
	((uint32_t *)result)[5] = 0x9B05688C;
	((uint32_t *)result)[6] = 0x1F83D9AB;
	((uint32_t *)result)[7] = 0x5BE0CD19;

#ifdef __GNUCLIKE_ASM
	__asm __volatile(
		".byte  0xf3, 0x0f, 0xa6, 0xd0" /* rep xsha256 */
			: "+S"(in), "+D"(result)
			: "c"(count), "a"(0)
		);
#endif

	padlock_output_block((uint32_t *)result, (uint32_t *)out,
	    SHA2_256_HASH_LEN / sizeof(uint32_t));
}

static void
padlock_sha_init(void *vctx)
{
	struct padlock_sha_ctx *ctx;

	ctx = vctx;
	ctx->psc_buf = NULL;
	ctx->psc_offset = 0;
	ctx->psc_size = 0;
}

static int
padlock_sha_update(void *vctx, const void *buf, u_int bufsize)
{
	struct padlock_sha_ctx *ctx;

	ctx = vctx;
	if (ctx->psc_size - ctx->psc_offset < bufsize) {
		ctx->psc_size = MAX(ctx->psc_size * 2, ctx->psc_size + bufsize);
		ctx->psc_buf = realloc(ctx->psc_buf, ctx->psc_size, M_PADLOCK,
		    M_NOWAIT);
		if(ctx->psc_buf == NULL)
			return (ENOMEM);
	}
	bcopy(buf, ctx->psc_buf + ctx->psc_offset, bufsize);
	ctx->psc_offset += bufsize;
	return (0);
}

static void
padlock_sha_free(void *vctx)
{
	struct padlock_sha_ctx *ctx;

	ctx = vctx;
	if (ctx->psc_buf != NULL) {
		//bzero(ctx->psc_buf, ctx->psc_size);
		free(ctx->psc_buf, M_PADLOCK);
		ctx->psc_buf = NULL;
		ctx->psc_offset = 0;
		ctx->psc_size = 0;
	}
}

static void
padlock_sha1_final(uint8_t *hash, void *vctx)
{
	struct padlock_sha_ctx *ctx;

	ctx = vctx;
	padlock_do_sha1(ctx->psc_buf, hash, ctx->psc_offset);
	padlock_sha_free(ctx);
}

static void
padlock_sha256_final(uint8_t *hash, void *vctx)
{
	struct padlock_sha_ctx *ctx;

	ctx = vctx;
	padlock_do_sha256(ctx->psc_buf, hash, ctx->psc_offset);
	padlock_sha_free(ctx);
}

static void
padlock_copy_ctx(struct auth_hash *axf, void *sctx, void *dctx)
{

	if ((via_feature_xcrypt & VIA_HAS_SHA) != 0 &&
	    (axf->type == CRYPTO_SHA1_HMAC ||
	     axf->type == CRYPTO_SHA2_256_HMAC)) {
		struct padlock_sha_ctx *spctx = sctx, *dpctx = dctx;

		dpctx->psc_offset = spctx->psc_offset;
		dpctx->psc_size = spctx->psc_size;
		dpctx->psc_buf = malloc(dpctx->psc_size, M_PADLOCK, M_WAITOK);
		bcopy(spctx->psc_buf, dpctx->psc_buf, dpctx->psc_size);
	} else {
		bcopy(sctx, dctx, axf->ctxsize);
	}
}

static void
padlock_free_ctx(struct auth_hash *axf, void *ctx)
{

	if ((via_feature_xcrypt & VIA_HAS_SHA) != 0 &&
	    (axf->type == CRYPTO_SHA1_HMAC ||
	     axf->type == CRYPTO_SHA2_256_HMAC)) {
		padlock_sha_free(ctx);
	}
}

static void
padlock_hash_key_setup(struct padlock_session *ses, const uint8_t *key,
    int klen)
{
	struct auth_hash *axf;

	axf = ses->ses_axf;

	/*
	 * Try to free contexts before using them, because
	 * padlock_hash_key_setup() can be called twice - once from
	 * padlock_newsession() and again from padlock_process().
	 */
	padlock_free_ctx(axf, ses->ses_ictx);
	padlock_free_ctx(axf, ses->ses_octx);

	hmac_init_ipad(axf, key, klen, ses->ses_ictx);
	hmac_init_opad(axf, key, klen, ses->ses_octx);
}

/*
 * Compute keyed-hash authenticator.
 */
static int
padlock_authcompute(struct padlock_session *ses, struct cryptop *crp)
{
	u_char hash[HASH_MAX_LEN], hash2[HASH_MAX_LEN];
	struct auth_hash *axf;
	union authctx ctx;
	int error;

	axf = ses->ses_axf;

	padlock_copy_ctx(axf, ses->ses_ictx, &ctx);
	error = crypto_apply(crp, crp->crp_aad_start, crp->crp_aad_length,
	    axf->Update, &ctx);
	if (error != 0) {
		padlock_free_ctx(axf, &ctx);
		return (error);
	}
	error = crypto_apply(crp, crp->crp_payload_start,
	    crp->crp_payload_length, axf->Update, &ctx);
	if (error != 0) {
		padlock_free_ctx(axf, &ctx);
		return (error);
	}
	axf->Final(hash, &ctx);

	padlock_copy_ctx(axf, ses->ses_octx, &ctx);
	axf->Update(&ctx, hash, axf->hashsize);
	axf->Final(hash, &ctx);

	if (crp->crp_op & CRYPTO_OP_VERIFY_DIGEST) {
		crypto_copydata(crp, crp->crp_digest_start, ses->ses_mlen,
		    hash2);
		if (timingsafe_bcmp(hash, hash2, ses->ses_mlen) != 0)
			return (EBADMSG);
	} else
		crypto_copyback(crp, crp->crp_digest_start, ses->ses_mlen,
		    hash);
	return (0);
}

/* Find software structure which describes HMAC algorithm. */
static struct auth_hash *
padlock_hash_lookup(int alg)
{
	struct auth_hash *axf;

	switch (alg) {
	case CRYPTO_NULL_HMAC:
		axf = &auth_hash_null;
		break;
	case CRYPTO_SHA1_HMAC:
		if ((via_feature_xcrypt & VIA_HAS_SHA) != 0)
			axf = &padlock_hmac_sha1;
		else
			axf = &auth_hash_hmac_sha1;
		break;
	case CRYPTO_RIPEMD160_HMAC:
		axf = &auth_hash_hmac_ripemd_160;
		break;
	case CRYPTO_SHA2_256_HMAC:
		if ((via_feature_xcrypt & VIA_HAS_SHA) != 0)
			axf = &padlock_hmac_sha256;
		else
			axf = &auth_hash_hmac_sha2_256;
		break;
	case CRYPTO_SHA2_384_HMAC:
		axf = &auth_hash_hmac_sha2_384;
		break;
	case CRYPTO_SHA2_512_HMAC:
		axf = &auth_hash_hmac_sha2_512;
		break;
	default:
		axf = NULL;
		break;
	}
	return (axf);
}

bool
padlock_hash_check(const struct crypto_session_params *csp)
{

	return (padlock_hash_lookup(csp->csp_auth_alg) != NULL);
}

int
padlock_hash_setup(struct padlock_session *ses,
    const struct crypto_session_params *csp)
{

	ses->ses_axf = padlock_hash_lookup(csp->csp_auth_alg);
	if (csp->csp_auth_mlen == 0)
		ses->ses_mlen = ses->ses_axf->hashsize;
	else
		ses->ses_mlen = csp->csp_auth_mlen;

	/* Allocate memory for HMAC inner and outer contexts. */
	ses->ses_ictx = malloc(ses->ses_axf->ctxsize, M_PADLOCK,
	    M_ZERO | M_NOWAIT);
	ses->ses_octx = malloc(ses->ses_axf->ctxsize, M_PADLOCK,
	    M_ZERO | M_NOWAIT);
	if (ses->ses_ictx == NULL || ses->ses_octx == NULL)
		return (ENOMEM);

	/* Setup key if given. */
	if (csp->csp_auth_key != NULL) {
		padlock_hash_key_setup(ses, csp->csp_auth_key,
		    csp->csp_auth_klen);
	}
	return (0);
}

int
padlock_hash_process(struct padlock_session *ses, struct cryptop *crp,
    const struct crypto_session_params *csp)
{
	struct thread *td;
	int error;

	td = curthread;
	fpu_kern_enter(td, ses->ses_fpu_ctx, FPU_KERN_NORMAL | FPU_KERN_KTHR);
	if (crp->crp_auth_key != NULL)
		padlock_hash_key_setup(ses, crp->crp_auth_key,
		    csp->csp_auth_klen);

	error = padlock_authcompute(ses, crp);
	fpu_kern_leave(td, ses->ses_fpu_ctx);
	return (error);
}

void
padlock_hash_free(struct padlock_session *ses)
{

	if (ses->ses_ictx != NULL) {
		padlock_free_ctx(ses->ses_axf, ses->ses_ictx);
		bzero(ses->ses_ictx, ses->ses_axf->ctxsize);
		free(ses->ses_ictx, M_PADLOCK);
		ses->ses_ictx = NULL;
	}
	if (ses->ses_octx != NULL) {
		padlock_free_ctx(ses->ses_axf, ses->ses_octx);
		bzero(ses->ses_octx, ses->ses_axf->ctxsize);
		free(ses->ses_octx, M_PADLOCK);
		ses->ses_octx = NULL;
	}
}
