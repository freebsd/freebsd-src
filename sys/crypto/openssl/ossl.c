/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2020 Netflix, Inc
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    similar to the "NO WARRANTY" disclaimer below ("Disclaimer") and any
 *    redistribution must be conditioned upon including a substantially
 *    similar Disclaimer requirement for further binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT, MERCHANTIBILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES.
 */

/*
 * A driver for the OpenCrypto framework which uses assembly routines
 * from OpenSSL.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>

#include <machine/fpu.h>

#include <opencrypto/cryptodev.h>
#include <opencrypto/xform_auth.h>

#include <crypto/openssl/ossl.h>
#include <crypto/openssl/ossl_chacha.h>

#include "cryptodev_if.h"

struct ossl_softc {
	int32_t sc_cid;
};

struct ossl_session_hash {
	struct ossl_hash_context ictx;
	struct ossl_hash_context octx;
	struct auth_hash *axf;
	u_int mlen;
};

struct ossl_session {
	struct ossl_session_hash hash;
};

static MALLOC_DEFINE(M_OSSL, "ossl", "OpenSSL crypto");

static void
ossl_identify(driver_t *driver, device_t parent)
{

	if (device_find_child(parent, "ossl", -1) == NULL)
		BUS_ADD_CHILD(parent, 10, "ossl", -1);
}

static int
ossl_probe(device_t dev)
{

	device_set_desc(dev, "OpenSSL crypto");
	return (BUS_PROBE_DEFAULT);
}

static int
ossl_attach(device_t dev)
{
	struct ossl_softc *sc;

	sc = device_get_softc(dev);

	ossl_cpuid();
	sc->sc_cid = crypto_get_driverid(dev, sizeof(struct ossl_session),
	    CRYPTOCAP_F_SOFTWARE | CRYPTOCAP_F_SYNC |
	    CRYPTOCAP_F_ACCEL_SOFTWARE);
	if (sc->sc_cid < 0) {
		device_printf(dev, "failed to allocate crypto driver id\n");
		return (ENXIO);
	}

	return (0);
}

static int
ossl_detach(device_t dev)
{
	struct ossl_softc *sc;

	sc = device_get_softc(dev);

	crypto_unregister_all(sc->sc_cid);

	return (0);
}

static struct auth_hash *
ossl_lookup_hash(const struct crypto_session_params *csp)
{

	switch (csp->csp_auth_alg) {
	case CRYPTO_SHA1:
	case CRYPTO_SHA1_HMAC:
		return (&ossl_hash_sha1);
	case CRYPTO_SHA2_224:
	case CRYPTO_SHA2_224_HMAC:
		return (&ossl_hash_sha224);
	case CRYPTO_SHA2_256:
	case CRYPTO_SHA2_256_HMAC:
		return (&ossl_hash_sha256);
	case CRYPTO_SHA2_384:
	case CRYPTO_SHA2_384_HMAC:
		return (&ossl_hash_sha384);
	case CRYPTO_SHA2_512:
	case CRYPTO_SHA2_512_HMAC:
		return (&ossl_hash_sha512);
	case CRYPTO_POLY1305:
		return (&ossl_hash_poly1305);
	default:
		return (NULL);
	}
}

static int
ossl_probesession(device_t dev, const struct crypto_session_params *csp)
{

	if ((csp->csp_flags & ~(CSP_F_SEPARATE_OUTPUT | CSP_F_SEPARATE_AAD)) !=
	    0)
		return (EINVAL);
	switch (csp->csp_mode) {
	case CSP_MODE_DIGEST:
		if (ossl_lookup_hash(csp) == NULL)
			return (EINVAL);
		break;
	case CSP_MODE_CIPHER:
		switch (csp->csp_cipher_alg) {
		case CRYPTO_CHACHA20:
			if (csp->csp_cipher_klen != CHACHA_KEY_SIZE)
				return (EINVAL);
			break;
		default:
			return (EINVAL);
		}
		break;
	case CSP_MODE_AEAD:
		switch (csp->csp_cipher_alg) {
		case CRYPTO_CHACHA20_POLY1305:
			break;
		default:
			return (EINVAL);
		}
		break;
	default:
		return (EINVAL);
	}

	return (CRYPTODEV_PROBE_ACCEL_SOFTWARE);
}

static void
ossl_newsession_hash(struct ossl_session *s,
    const struct crypto_session_params *csp)
{
	struct auth_hash *axf;

	axf = ossl_lookup_hash(csp);
	s->hash.axf = axf;
	if (csp->csp_auth_mlen == 0)
		s->hash.mlen = axf->hashsize;
	else
		s->hash.mlen = csp->csp_auth_mlen;

	if (csp->csp_auth_klen == 0) {
		axf->Init(&s->hash.ictx);
	} else {
		if (csp->csp_auth_key != NULL) {
			fpu_kern_enter(curthread, NULL, FPU_KERN_NOCTX);
			if (axf->Setkey != NULL) {
				axf->Init(&s->hash.ictx);
				axf->Setkey(&s->hash.ictx, csp->csp_auth_key,
				    csp->csp_auth_klen);
			} else {
				hmac_init_ipad(axf, csp->csp_auth_key,
				    csp->csp_auth_klen, &s->hash.ictx);
				hmac_init_opad(axf, csp->csp_auth_key,
				    csp->csp_auth_klen, &s->hash.octx);
			}
			fpu_kern_leave(curthread, NULL);
		}
	}
}

static int
ossl_newsession(device_t dev, crypto_session_t cses,
    const struct crypto_session_params *csp)
{
	struct ossl_session *s;

	s = crypto_get_driver_session(cses);
	switch (csp->csp_mode) {
	case CSP_MODE_DIGEST:
		ossl_newsession_hash(s, csp);
		break;
	}

	return (0);
}

static int
ossl_process_hash(struct ossl_session *s, struct cryptop *crp,
    const struct crypto_session_params *csp)
{
	struct ossl_hash_context ctx;
	char digest[HASH_MAX_LEN];
	struct auth_hash *axf;
	int error;

	axf = s->hash.axf;

	if (crp->crp_auth_key == NULL) {
		ctx = s->hash.ictx;
	} else {
		if (axf->Setkey != NULL) {
			axf->Init(&ctx);
			axf->Setkey(&ctx, crp->crp_auth_key,
			    csp->csp_auth_klen);
		} else {
			hmac_init_ipad(axf, crp->crp_auth_key,
			    csp->csp_auth_klen, &ctx);
		}
	}

	if (crp->crp_aad != NULL)
		error = axf->Update(&ctx, crp->crp_aad, crp->crp_aad_length);
	else
		error = crypto_apply(crp, crp->crp_aad_start,
		    crp->crp_aad_length, axf->Update, &ctx);
	if (error)
		goto out;

	error = crypto_apply(crp, crp->crp_payload_start,
	    crp->crp_payload_length, axf->Update, &ctx);
	if (error)
		goto out;

	axf->Final(digest, &ctx);

	if (csp->csp_auth_klen != 0 && axf->Setkey == NULL) {
		if (crp->crp_auth_key == NULL)
			ctx = s->hash.octx;
		else
			hmac_init_opad(axf, crp->crp_auth_key,
			    csp->csp_auth_klen, &ctx);
		axf->Update(&ctx, digest, axf->hashsize);
		axf->Final(digest, &ctx);
	}

	if (crp->crp_op & CRYPTO_OP_VERIFY_DIGEST) {
		char digest2[HASH_MAX_LEN];

		crypto_copydata(crp, crp->crp_digest_start, s->hash.mlen,
		    digest2);
		if (timingsafe_bcmp(digest, digest2, s->hash.mlen) != 0)
			error = EBADMSG;
		explicit_bzero(digest2, sizeof(digest2));
	} else {
		crypto_copyback(crp, crp->crp_digest_start, s->hash.mlen,
		    digest);
	}
	explicit_bzero(digest, sizeof(digest));

out:
	explicit_bzero(&ctx, sizeof(ctx));
	return (error);
}

static int
ossl_process(device_t dev, struct cryptop *crp, int hint)
{
	const struct crypto_session_params *csp;
	struct ossl_session *s;
	int error;
	bool fpu_entered;

	s = crypto_get_driver_session(crp->crp_session);
	csp = crypto_get_params(crp->crp_session);

	if (is_fpu_kern_thread(0)) {
		fpu_entered = false;
	} else {
		fpu_kern_enter(curthread, NULL, FPU_KERN_NOCTX);
		fpu_entered = true;
	}

	switch (csp->csp_mode) {
	case CSP_MODE_DIGEST:
		error = ossl_process_hash(s, crp, csp);
		break;
	case CSP_MODE_CIPHER:
		error = ossl_chacha20(crp, csp);
		break;
	case CSP_MODE_AEAD:
		if (CRYPTO_OP_IS_ENCRYPT(crp->crp_op))
			error = ossl_chacha20_poly1305_encrypt(crp, csp);
		else
			error = ossl_chacha20_poly1305_decrypt(crp, csp);
		break;
	default:
		__assert_unreachable();
	}

	if (fpu_entered)
		fpu_kern_leave(curthread, NULL);

	crp->crp_etype = error;
	crypto_done(crp);

	return (0);
}

static device_method_t ossl_methods[] = {
	DEVMETHOD(device_identify,	ossl_identify),
	DEVMETHOD(device_probe,		ossl_probe),
	DEVMETHOD(device_attach,	ossl_attach),
	DEVMETHOD(device_detach,	ossl_detach),

	DEVMETHOD(cryptodev_probesession, ossl_probesession),
	DEVMETHOD(cryptodev_newsession,	ossl_newsession),
	DEVMETHOD(cryptodev_process,	ossl_process),

	DEVMETHOD_END
};

static driver_t ossl_driver = {
	"ossl",
	ossl_methods,
	sizeof(struct ossl_softc)
};

static devclass_t ossl_devclass;

DRIVER_MODULE(ossl, nexus, ossl_driver, ossl_devclass, NULL, NULL);
MODULE_VERSION(ossl, 1);
MODULE_DEPEND(ossl, crypto, 1, 1, 1);
