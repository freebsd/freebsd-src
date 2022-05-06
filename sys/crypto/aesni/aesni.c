/*-
 * Copyright (c) 2005-2008 Pawel Jakub Dawidek <pjd@FreeBSD.org>
 * Copyright (c) 2010 Konstantin Belousov <kib@FreeBSD.org>
 * Copyright (c) 2014-2021 The FreeBSD Foundation
 * Copyright (c) 2017 Conrad Meyer <cem@FreeBSD.org>
 * All rights reserved.
 *
 * Portions of this software were developed by John-Mark Gurney
 * under sponsorship of the FreeBSD Foundation and
 * Rubicon Communications, LLC (Netgate).
 *
 * Portions of this software were developed by Ararat River
 * Consulting, LLC under sponsorship of the FreeBSD Foundation.
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
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/kobj.h>
#include <sys/libkern.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/smp.h>
#include <sys/systm.h>
#include <sys/uio.h>

#include <crypto/aesni/aesni.h>
#include <crypto/aesni/sha_sse.h>
#include <crypto/sha1.h>
#include <crypto/sha2/sha224.h>
#include <crypto/sha2/sha256.h>

#include <opencrypto/cryptodev.h>
#include <opencrypto/gmac.h>
#include <cryptodev_if.h>

#include <machine/md_var.h>
#include <machine/specialreg.h>
#include <machine/fpu.h>

static struct mtx_padalign *ctx_mtx;
static struct fpu_kern_ctx **ctx_fpu;

struct aesni_softc {
	int32_t cid;
	bool	has_aes;
	bool	has_sha;
};

#define ACQUIRE_CTX(i, ctx)					\
	do {							\
		(i) = PCPU_GET(cpuid);				\
		mtx_lock(&ctx_mtx[(i)]);			\
		(ctx) = ctx_fpu[(i)];				\
	} while (0)
#define RELEASE_CTX(i, ctx)					\
	do {							\
		mtx_unlock(&ctx_mtx[(i)]);			\
		(i) = -1;					\
		(ctx) = NULL;					\
	} while (0)

static int aesni_cipher_setup(struct aesni_session *ses,
    const struct crypto_session_params *csp);
static int aesni_cipher_process(struct aesni_session *ses, struct cryptop *crp);
static int aesni_cipher_crypt(struct aesni_session *ses, struct cryptop *crp,
    const struct crypto_session_params *csp);
static int aesni_cipher_mac(struct aesni_session *ses, struct cryptop *crp,
    const struct crypto_session_params *csp);

MALLOC_DEFINE(M_AESNI, "aesni_data", "AESNI Data");

static void
aesni_identify(driver_t *drv, device_t parent)
{

	/* NB: order 10 is so we get attached after h/w devices */
	if (device_find_child(parent, "aesni", -1) == NULL &&
	    BUS_ADD_CHILD(parent, 10, "aesni", -1) == 0)
		panic("aesni: could not attach");
}

static void
detect_cpu_features(bool *has_aes, bool *has_sha)
{

	*has_aes = ((cpu_feature2 & CPUID2_AESNI) != 0 &&
	    (cpu_feature2 & CPUID2_SSE41) != 0);
	*has_sha = ((cpu_stdext_feature & CPUID_STDEXT_SHA) != 0 &&
	    (cpu_feature2 & CPUID2_SSSE3) != 0);
}

static int
aesni_probe(device_t dev)
{
	bool has_aes, has_sha;

	detect_cpu_features(&has_aes, &has_sha);
	if (!has_aes && !has_sha) {
		device_printf(dev, "No AES or SHA support.\n");
		return (EINVAL);
	} else if (has_aes && has_sha)
		device_set_desc(dev,
		    "AES-CBC,AES-CCM,AES-GCM,AES-ICM,AES-XTS,SHA1,SHA256");
	else if (has_aes)
		device_set_desc(dev,
		    "AES-CBC,AES-CCM,AES-GCM,AES-ICM,AES-XTS");
	else
		device_set_desc(dev, "SHA1,SHA256");

	return (0);
}

static void
aesni_cleanctx(void)
{
	int i;

	/* XXX - no way to return driverid */
	CPU_FOREACH(i) {
		if (ctx_fpu[i] != NULL) {
			mtx_destroy(&ctx_mtx[i]);
			fpu_kern_free_ctx(ctx_fpu[i]);
		}
		ctx_fpu[i] = NULL;
	}
	free(ctx_mtx, M_AESNI);
	ctx_mtx = NULL;
	free(ctx_fpu, M_AESNI);
	ctx_fpu = NULL;
}

static int
aesni_attach(device_t dev)
{
	struct aesni_softc *sc;
	int i;

	sc = device_get_softc(dev);

	sc->cid = crypto_get_driverid(dev, sizeof(struct aesni_session),
	    CRYPTOCAP_F_SOFTWARE | CRYPTOCAP_F_SYNC |
	    CRYPTOCAP_F_ACCEL_SOFTWARE);
	if (sc->cid < 0) {
		device_printf(dev, "Could not get crypto driver id.\n");
		return (ENOMEM);
	}

	ctx_mtx = malloc(sizeof *ctx_mtx * (mp_maxid + 1), M_AESNI,
	    M_WAITOK|M_ZERO);
	ctx_fpu = malloc(sizeof *ctx_fpu * (mp_maxid + 1), M_AESNI,
	    M_WAITOK|M_ZERO);

	CPU_FOREACH(i) {
#ifdef __amd64__
		ctx_fpu[i] = fpu_kern_alloc_ctx_domain(
		    pcpu_find(i)->pc_domain, FPU_KERN_NORMAL);
#else
		ctx_fpu[i] = fpu_kern_alloc_ctx(FPU_KERN_NORMAL);
#endif
		mtx_init(&ctx_mtx[i], "anifpumtx", NULL, MTX_DEF|MTX_NEW);
	}

	detect_cpu_features(&sc->has_aes, &sc->has_sha);
	return (0);
}

static int
aesni_detach(device_t dev)
{
	struct aesni_softc *sc;

	sc = device_get_softc(dev);

	crypto_unregister_all(sc->cid);

	aesni_cleanctx();

	return (0);
}

static bool
aesni_auth_supported(struct aesni_softc *sc,
    const struct crypto_session_params *csp)
{

	if (!sc->has_sha)
		return (false);

	switch (csp->csp_auth_alg) {
	case CRYPTO_SHA1:
	case CRYPTO_SHA2_224:
	case CRYPTO_SHA2_256:
	case CRYPTO_SHA1_HMAC:
	case CRYPTO_SHA2_224_HMAC:
	case CRYPTO_SHA2_256_HMAC:
		break;
	default:
		return (false);
	}

	return (true);
}

static bool
aesni_cipher_supported(struct aesni_softc *sc,
    const struct crypto_session_params *csp)
{

	if (!sc->has_aes)
		return (false);

	switch (csp->csp_cipher_alg) {
	case CRYPTO_AES_CBC:
	case CRYPTO_AES_ICM:
		switch (csp->csp_cipher_klen * 8) {
		case 128:
		case 192:
		case 256:
			break;
		default:
			CRYPTDEB("invalid CBC/ICM key length");
			return (false);
		}
		if (csp->csp_ivlen != AES_BLOCK_LEN)
			return (false);
		break;
	case CRYPTO_AES_XTS:
		switch (csp->csp_cipher_klen * 8) {
		case 256:
		case 512:
			break;
		default:
			CRYPTDEB("invalid XTS key length");
			return (false);
		}
		if (csp->csp_ivlen != AES_XTS_IV_LEN)
			return (false);
		break;
	default:
		return (false);
	}

	return (true);
}

#define SUPPORTED_SES (CSP_F_SEPARATE_OUTPUT | CSP_F_SEPARATE_AAD | CSP_F_ESN)

static int
aesni_probesession(device_t dev, const struct crypto_session_params *csp)
{
	struct aesni_softc *sc;

	sc = device_get_softc(dev);
	if ((csp->csp_flags & ~(SUPPORTED_SES)) != 0)
		return (EINVAL);
	switch (csp->csp_mode) {
	case CSP_MODE_DIGEST:
		if (!aesni_auth_supported(sc, csp))
			return (EINVAL);
		break;
	case CSP_MODE_CIPHER:
		if (!aesni_cipher_supported(sc, csp))
			return (EINVAL);
		break;
	case CSP_MODE_AEAD:
		switch (csp->csp_cipher_alg) {
		case CRYPTO_AES_NIST_GCM_16:
			switch (csp->csp_cipher_klen * 8) {
			case 128:
			case 192:
			case 256:
				break;
			default:
				CRYPTDEB("invalid GCM key length");
				return (EINVAL);
			}
			if (csp->csp_auth_mlen != 0 &&
			    csp->csp_auth_mlen != GMAC_DIGEST_LEN)
				return (EINVAL);
			if (!sc->has_aes)
				return (EINVAL);
			break;
		case CRYPTO_AES_CCM_16:
			switch (csp->csp_cipher_klen * 8) {
			case 128:
			case 192:
			case 256:
				break;
			default:
				CRYPTDEB("invalid CCM key length");
				return (EINVAL);
			}
			if (!sc->has_aes)
				return (EINVAL);
			break;
		default:
			return (EINVAL);
		}
		break;
	case CSP_MODE_ETA:
		if (!aesni_auth_supported(sc, csp) ||
		    !aesni_cipher_supported(sc, csp))
			return (EINVAL);
		break;
	default:
		return (EINVAL);
	}

	return (CRYPTODEV_PROBE_ACCEL_SOFTWARE);
}

static int
aesni_newsession(device_t dev, crypto_session_t cses,
    const struct crypto_session_params *csp)
{
	struct aesni_session *ses;
	int error;

	ses = crypto_get_driver_session(cses);

	switch (csp->csp_mode) {
	case CSP_MODE_DIGEST:
	case CSP_MODE_CIPHER:
	case CSP_MODE_AEAD:
	case CSP_MODE_ETA:
		break;
	default:
		return (EINVAL);
	}
	error = aesni_cipher_setup(ses, csp);
	if (error != 0) {
		CRYPTDEB("setup failed");
		return (error);
	}

	return (0);
}

static int
aesni_process(device_t dev, struct cryptop *crp, int hint __unused)
{
	struct aesni_session *ses;
	int error;

	ses = crypto_get_driver_session(crp->crp_session);

	error = aesni_cipher_process(ses, crp);

	crp->crp_etype = error;
	crypto_done(crp);
	return (0);
}

static uint8_t *
aesni_cipher_alloc(struct cryptop *crp, int start, int length, bool *allocated)
{
	uint8_t *addr;

	addr = crypto_contiguous_subsegment(crp, start, length);
	if (addr != NULL) {
		*allocated = false;
		return (addr);
	}
	addr = malloc(length, M_AESNI, M_NOWAIT);
	if (addr != NULL) {
		*allocated = true;
		crypto_copydata(crp, start, length, addr);
	} else
		*allocated = false;
	return (addr);
}

static device_method_t aesni_methods[] = {
	DEVMETHOD(device_identify, aesni_identify),
	DEVMETHOD(device_probe, aesni_probe),
	DEVMETHOD(device_attach, aesni_attach),
	DEVMETHOD(device_detach, aesni_detach),

	DEVMETHOD(cryptodev_probesession, aesni_probesession),
	DEVMETHOD(cryptodev_newsession, aesni_newsession),
	DEVMETHOD(cryptodev_process, aesni_process),

	DEVMETHOD_END
};

static driver_t aesni_driver = {
	"aesni",
	aesni_methods,
	sizeof(struct aesni_softc),
};

DRIVER_MODULE(aesni, nexus, aesni_driver, 0, 0);
MODULE_VERSION(aesni, 1);
MODULE_DEPEND(aesni, crypto, 1, 1, 1);

static int
intel_sha1_update(void *vctx, const void *vdata, u_int datalen)
{
	struct sha1_ctxt *ctx = vctx;
	const char *data = vdata;
	size_t gaplen;
	size_t gapstart;
	size_t off;
	size_t copysiz;
	u_int blocks;

	off = 0;
	/* Do any aligned blocks without redundant copying. */
	if (datalen >= 64 && ctx->count % 64 == 0) {
		blocks = datalen / 64;
		ctx->c.b64[0] += blocks * 64 * 8;
		intel_sha1_step(ctx->h.b32, data + off, blocks);
		off += blocks * 64;
	}

	while (off < datalen) {
		gapstart = ctx->count % 64;
		gaplen = 64 - gapstart;

		copysiz = (gaplen < datalen - off) ? gaplen : datalen - off;
		bcopy(&data[off], &ctx->m.b8[gapstart], copysiz);
		ctx->count += copysiz;
		ctx->count %= 64;
		ctx->c.b64[0] += copysiz * 8;
		if (ctx->count % 64 == 0)
			intel_sha1_step(ctx->h.b32, (void *)ctx->m.b8, 1);
		off += copysiz;
	}

	return (0);
}

static void
SHA1_Init_fn(void *ctx)
{
	sha1_init(ctx);
}

static void
SHA1_Finalize_fn(void *digest, void *ctx)
{
	sha1_result(ctx, digest);
}

static int
intel_sha256_update(void *vctx, const void *vdata, u_int len)
{
	SHA256_CTX *ctx = vctx;
	uint64_t bitlen;
	uint32_t r;
	u_int blocks;
	const unsigned char *src = vdata;

	/* Number of bytes left in the buffer from previous updates */
	r = (ctx->count >> 3) & 0x3f;

	/* Convert the length into a number of bits */
	bitlen = len << 3;

	/* Update number of bits */
	ctx->count += bitlen;

	/* Handle the case where we don't need to perform any transforms */
	if (len < 64 - r) {
		memcpy(&ctx->buf[r], src, len);
		return (0);
	}

	/* Finish the current block */
	memcpy(&ctx->buf[r], src, 64 - r);
	intel_sha256_step(ctx->state, ctx->buf, 1);
	src += 64 - r;
	len -= 64 - r;

	/* Perform complete blocks */
	if (len >= 64) {
		blocks = len / 64;
		intel_sha256_step(ctx->state, src, blocks);
		src += blocks * 64;
		len -= blocks * 64;
	}

	/* Copy left over data into buffer */
	memcpy(ctx->buf, src, len);

	return (0);
}

static void
SHA224_Init_fn(void *ctx)
{
	SHA224_Init(ctx);
}

static void
SHA224_Finalize_fn(void *digest, void *ctx)
{
	SHA224_Final(digest, ctx);
}

static void
SHA256_Init_fn(void *ctx)
{
	SHA256_Init(ctx);
}

static void
SHA256_Finalize_fn(void *digest, void *ctx)
{
	SHA256_Final(digest, ctx);
}

static int
aesni_authprepare(struct aesni_session *ses, int klen)
{

	if (klen > SHA1_BLOCK_LEN)
		return (EINVAL);
	if ((ses->hmac && klen == 0) || (!ses->hmac && klen != 0))
		return (EINVAL);
	return (0);
}

static int
aesni_cipher_setup(struct aesni_session *ses,
    const struct crypto_session_params *csp)
{
	struct fpu_kern_ctx *ctx;
	uint8_t *schedbase;
	int kt, ctxidx, error;

	schedbase = (uint8_t *)roundup2((uintptr_t)ses->schedules,
	    AES_SCHED_ALIGN);
	ses->enc_schedule = schedbase;
	ses->dec_schedule = schedbase + AES_SCHED_LEN;
	ses->xts_schedule = schedbase + AES_SCHED_LEN * 2;

	switch (csp->csp_auth_alg) {
	case CRYPTO_SHA1_HMAC:
		ses->hmac = true;
		/* FALLTHROUGH */
	case CRYPTO_SHA1:
		ses->hash_len = SHA1_HASH_LEN;
		ses->hash_init = SHA1_Init_fn;
		ses->hash_update = intel_sha1_update;
		ses->hash_finalize = SHA1_Finalize_fn;
		break;
	case CRYPTO_SHA2_224_HMAC:
		ses->hmac = true;
		/* FALLTHROUGH */
	case CRYPTO_SHA2_224:
		ses->hash_len = SHA2_224_HASH_LEN;
		ses->hash_init = SHA224_Init_fn;
		ses->hash_update = intel_sha256_update;
		ses->hash_finalize = SHA224_Finalize_fn;
		break;
	case CRYPTO_SHA2_256_HMAC:
		ses->hmac = true;
		/* FALLTHROUGH */
	case CRYPTO_SHA2_256:
		ses->hash_len = SHA2_256_HASH_LEN;
		ses->hash_init = SHA256_Init_fn;
		ses->hash_update = intel_sha256_update;
		ses->hash_finalize = SHA256_Finalize_fn;
		break;
	}

	if (ses->hash_len != 0) {
		if (csp->csp_auth_mlen == 0)
			ses->mlen = ses->hash_len;
		else
			ses->mlen = csp->csp_auth_mlen;

		error = aesni_authprepare(ses, csp->csp_auth_klen);
		if (error != 0)
			return (error);
	} else if (csp->csp_cipher_alg == CRYPTO_AES_CCM_16) {
		if (csp->csp_auth_mlen == 0)
			ses->mlen = AES_CBC_MAC_HASH_LEN;
		else
			ses->mlen = csp->csp_auth_mlen;
	}

	kt = is_fpu_kern_thread(0) || (csp->csp_cipher_alg == 0);
	if (!kt) {
		ACQUIRE_CTX(ctxidx, ctx);
		fpu_kern_enter(curthread, ctx,
		    FPU_KERN_NORMAL | FPU_KERN_KTHR);
	}

	error = 0;
	if (csp->csp_cipher_key != NULL)
		aesni_cipher_setup_common(ses, csp, csp->csp_cipher_key,
		    csp->csp_cipher_klen);

	if (!kt) {
		fpu_kern_leave(curthread, ctx);
		RELEASE_CTX(ctxidx, ctx);
	}
	return (error);
}

static int
aesni_cipher_process(struct aesni_session *ses, struct cryptop *crp)
{
	const struct crypto_session_params *csp;
	struct fpu_kern_ctx *ctx;
	int error, ctxidx;
	bool kt;

	csp = crypto_get_params(crp->crp_session);
	switch (csp->csp_cipher_alg) {
	case CRYPTO_AES_CCM_16:
		if (crp->crp_payload_length > ccm_max_payload_length(csp))
			return (EMSGSIZE);
		/* FALLTHROUGH */
	case CRYPTO_AES_ICM:
	case CRYPTO_AES_NIST_GCM_16:
		if ((crp->crp_flags & CRYPTO_F_IV_SEPARATE) == 0)
			return (EINVAL);
		break;
	case CRYPTO_AES_CBC:
	case CRYPTO_AES_XTS:
		/* CBC & XTS can only handle full blocks for now */
		if ((crp->crp_payload_length % AES_BLOCK_LEN) != 0)
			return (EINVAL);
		break;
	}

	ctx = NULL;
	ctxidx = 0;
	error = 0;
	kt = is_fpu_kern_thread(0);
	if (!kt) {
		ACQUIRE_CTX(ctxidx, ctx);
		fpu_kern_enter(curthread, ctx,
		    FPU_KERN_NORMAL | FPU_KERN_KTHR);
	}

	/* Do work */
	if (csp->csp_mode == CSP_MODE_ETA) {
		if (CRYPTO_OP_IS_ENCRYPT(crp->crp_op)) {
			error = aesni_cipher_crypt(ses, crp, csp);
			if (error == 0)
				error = aesni_cipher_mac(ses, crp, csp);
		} else {
			error = aesni_cipher_mac(ses, crp, csp);
			if (error == 0)
				error = aesni_cipher_crypt(ses, crp, csp);
		}
	} else if (csp->csp_mode == CSP_MODE_DIGEST)
		error = aesni_cipher_mac(ses, crp, csp);
	else
		error = aesni_cipher_crypt(ses, crp, csp);

	if (!kt) {
		fpu_kern_leave(curthread, ctx);
		RELEASE_CTX(ctxidx, ctx);
	}
	return (error);
}

static int
aesni_cipher_crypt(struct aesni_session *ses, struct cryptop *crp,
    const struct crypto_session_params *csp)
{
	uint8_t iv[AES_BLOCK_LEN], tag[GMAC_DIGEST_LEN];
	uint8_t *authbuf, *buf, *outbuf;
	int error;
	bool encflag, allocated, authallocated, outallocated, outcopy;

	if (crp->crp_payload_length == 0) {
		buf = NULL;
		allocated = false;
	} else {
		buf = aesni_cipher_alloc(crp, crp->crp_payload_start,
		    crp->crp_payload_length, &allocated);
		if (buf == NULL)
			return (ENOMEM);
	}

	outallocated = false;
	authallocated = false;
	authbuf = NULL;
	if (csp->csp_cipher_alg == CRYPTO_AES_NIST_GCM_16 ||
	    csp->csp_cipher_alg == CRYPTO_AES_CCM_16) {
		if (crp->crp_aad_length == 0) {
			authbuf = NULL;
		} else if (crp->crp_aad != NULL) {
			authbuf = crp->crp_aad;
		} else {
			authbuf = aesni_cipher_alloc(crp, crp->crp_aad_start,
			    crp->crp_aad_length, &authallocated);
			if (authbuf == NULL) {
				error = ENOMEM;
				goto out;
			}
		}
	}

	if (CRYPTO_HAS_OUTPUT_BUFFER(crp) && crp->crp_payload_length > 0) {
		outbuf = crypto_buffer_contiguous_subsegment(&crp->crp_obuf,
		    crp->crp_payload_output_start, crp->crp_payload_length);
		if (outbuf == NULL) {
			outcopy = true;
			if (allocated)
				outbuf = buf;
			else {
				outbuf = malloc(crp->crp_payload_length,
				    M_AESNI, M_NOWAIT);
				if (outbuf == NULL) {
					error = ENOMEM;
					goto out;
				}
				outallocated = true;
			}
		} else
			outcopy = false;
	} else {
		outbuf = buf;
		outcopy = allocated;
	}

	error = 0;
	encflag = CRYPTO_OP_IS_ENCRYPT(crp->crp_op);
	if (crp->crp_cipher_key != NULL)
		aesni_cipher_setup_common(ses, csp, crp->crp_cipher_key,
		    csp->csp_cipher_klen);

	crypto_read_iv(crp, iv);

	switch (csp->csp_cipher_alg) {
	case CRYPTO_AES_CBC:
		if (encflag)
			aesni_encrypt_cbc(ses->rounds, ses->enc_schedule,
			    crp->crp_payload_length, buf, outbuf, iv);
		else {
			if (buf != outbuf)
				memcpy(outbuf, buf, crp->crp_payload_length);
			aesni_decrypt_cbc(ses->rounds, ses->dec_schedule,
			    crp->crp_payload_length, outbuf, iv);
		}
		break;
	case CRYPTO_AES_ICM:
		/* encryption & decryption are the same */
		aesni_encrypt_icm(ses->rounds, ses->enc_schedule,
		    crp->crp_payload_length, buf, outbuf, iv);
		break;
	case CRYPTO_AES_XTS:
		if (encflag)
			aesni_encrypt_xts(ses->rounds, ses->enc_schedule,
			    ses->xts_schedule, crp->crp_payload_length, buf,
			    outbuf, iv);
		else
			aesni_decrypt_xts(ses->rounds, ses->dec_schedule,
			    ses->xts_schedule, crp->crp_payload_length, buf,
			    outbuf, iv);
		break;
	case CRYPTO_AES_NIST_GCM_16:
		if (encflag) {
			memset(tag, 0, sizeof(tag));
			AES_GCM_encrypt(buf, outbuf, authbuf, iv, tag,
			    crp->crp_payload_length, crp->crp_aad_length,
			    csp->csp_ivlen, ses->enc_schedule, ses->rounds);
			crypto_copyback(crp, crp->crp_digest_start, sizeof(tag),
			    tag);
		} else {
			crypto_copydata(crp, crp->crp_digest_start, sizeof(tag),
			    tag);
			if (!AES_GCM_decrypt(buf, outbuf, authbuf, iv, tag,
			    crp->crp_payload_length, crp->crp_aad_length,
			    csp->csp_ivlen, ses->enc_schedule, ses->rounds))
				error = EBADMSG;
		}
		break;
	case CRYPTO_AES_CCM_16:
		if (encflag) {
			memset(tag, 0, sizeof(tag));			
			AES_CCM_encrypt(buf, outbuf, authbuf, iv, tag,
			    crp->crp_payload_length, crp->crp_aad_length,
			    csp->csp_ivlen, ses->mlen, ses->enc_schedule,
			    ses->rounds);
			crypto_copyback(crp, crp->crp_digest_start, ses->mlen,
			    tag);
		} else {
			crypto_copydata(crp, crp->crp_digest_start, ses->mlen,
			    tag);
			if (!AES_CCM_decrypt(buf, outbuf, authbuf, iv, tag,
			    crp->crp_payload_length, crp->crp_aad_length,
			    csp->csp_ivlen, ses->mlen, ses->enc_schedule,
			    ses->rounds))
				error = EBADMSG;
		}
		break;
	}
	if (outcopy && error == 0)
		crypto_copyback(crp, CRYPTO_HAS_OUTPUT_BUFFER(crp) ?
		    crp->crp_payload_output_start : crp->crp_payload_start,
		    crp->crp_payload_length, outbuf);

out:
	if (allocated)
		zfree(buf, M_AESNI);
	if (authallocated)
		zfree(authbuf, M_AESNI);
	if (outallocated)
		zfree(outbuf, M_AESNI);
	explicit_bzero(iv, sizeof(iv));
	explicit_bzero(tag, sizeof(tag));
	return (error);
}

static int
aesni_cipher_mac(struct aesni_session *ses, struct cryptop *crp,
    const struct crypto_session_params *csp)
{
	union {
		struct SHA256Context sha2 __aligned(16);
		struct sha1_ctxt sha1 __aligned(16);
	} sctx;
	uint32_t res[SHA2_256_HASH_LEN / sizeof(uint32_t)];
	const uint8_t *key;
	int i, keylen;

	if (crp->crp_auth_key != NULL)
		key = crp->crp_auth_key;
	else
		key = csp->csp_auth_key;
	keylen = csp->csp_auth_klen;

	if (ses->hmac) {
		uint8_t hmac_key[SHA1_BLOCK_LEN] __aligned(16);

		/* Inner hash: (K ^ IPAD) || data */
		ses->hash_init(&sctx);
		for (i = 0; i < keylen; i++)
			hmac_key[i] = key[i] ^ HMAC_IPAD_VAL;
		for (i = keylen; i < sizeof(hmac_key); i++)
			hmac_key[i] = 0 ^ HMAC_IPAD_VAL;
		ses->hash_update(&sctx, hmac_key, sizeof(hmac_key));

		if (crp->crp_aad != NULL)
			ses->hash_update(&sctx, crp->crp_aad,
			    crp->crp_aad_length);
		else
			crypto_apply(crp, crp->crp_aad_start,
			    crp->crp_aad_length, ses->hash_update, &sctx);
		if (CRYPTO_HAS_OUTPUT_BUFFER(crp) &&
		    CRYPTO_OP_IS_ENCRYPT(crp->crp_op))
			crypto_apply_buf(&crp->crp_obuf,
			    crp->crp_payload_output_start,
			    crp->crp_payload_length,
			    ses->hash_update, &sctx);
		else
			crypto_apply(crp, crp->crp_payload_start,
			    crp->crp_payload_length, ses->hash_update, &sctx);

		if (csp->csp_flags & CSP_F_ESN)
			ses->hash_update(&sctx, crp->crp_esn, 4);

		ses->hash_finalize(res, &sctx);

		/* Outer hash: (K ^ OPAD) || inner hash */
		ses->hash_init(&sctx);
		for (i = 0; i < keylen; i++)
			hmac_key[i] = key[i] ^ HMAC_OPAD_VAL;
		for (i = keylen; i < sizeof(hmac_key); i++)
			hmac_key[i] = 0 ^ HMAC_OPAD_VAL;
		ses->hash_update(&sctx, hmac_key, sizeof(hmac_key));
		ses->hash_update(&sctx, res, ses->hash_len);
		ses->hash_finalize(res, &sctx);
		explicit_bzero(hmac_key, sizeof(hmac_key));
	} else {
		ses->hash_init(&sctx);

		if (crp->crp_aad != NULL)
			ses->hash_update(&sctx, crp->crp_aad,
			    crp->crp_aad_length);
		else
			crypto_apply(crp, crp->crp_aad_start,
			    crp->crp_aad_length, ses->hash_update, &sctx);
		if (CRYPTO_HAS_OUTPUT_BUFFER(crp) &&
		    CRYPTO_OP_IS_ENCRYPT(crp->crp_op))
			crypto_apply_buf(&crp->crp_obuf,
			    crp->crp_payload_output_start,
			    crp->crp_payload_length,
			    ses->hash_update, &sctx);
		else
			crypto_apply(crp, crp->crp_payload_start,
			    crp->crp_payload_length,
			    ses->hash_update, &sctx);

		ses->hash_finalize(res, &sctx);
	}

	if (crp->crp_op & CRYPTO_OP_VERIFY_DIGEST) {
		uint32_t res2[SHA2_256_HASH_LEN / sizeof(uint32_t)];

		crypto_copydata(crp, crp->crp_digest_start, ses->mlen, res2);
		if (timingsafe_bcmp(res, res2, ses->mlen) != 0)
			return (EBADMSG);
		explicit_bzero(res2, sizeof(res2));
	} else
		crypto_copyback(crp, crp->crp_digest_start, ses->mlen, res);
	explicit_bzero(res, sizeof(res));
	return (0);
}
