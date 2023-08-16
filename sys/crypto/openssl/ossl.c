/*-
 * SPDX-License-Identifier: BSD-2-Clause
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
#include <crypto/openssl/ossl_cipher.h>

#include "cryptodev_if.h"

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

	sc->has_aes = sc->has_aes_gcm = false;

	ossl_cpuid(sc);
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

static struct ossl_cipher*
ossl_lookup_cipher(const struct crypto_session_params *csp)
{

	switch (csp->csp_cipher_alg) {
	case CRYPTO_AES_CBC:
		switch (csp->csp_cipher_klen * 8) {
		case 128:
		case 192:
		case 256:
			break;
		default:
			return (NULL);
		}
		return (&ossl_cipher_aes_cbc);
	case CRYPTO_AES_NIST_GCM_16:
		switch (csp->csp_cipher_klen * 8) {
		case 128:
		case 192:
		case 256:
			break;
		default:
			return (NULL);
		}
		return (&ossl_cipher_aes_gcm);
	case CRYPTO_CHACHA20:
		if (csp->csp_cipher_klen != CHACHA_KEY_SIZE)
			return (NULL);
		return (&ossl_cipher_chacha20);
	default:
		return (NULL);
	}
}

static int
ossl_probesession(device_t dev, const struct crypto_session_params *csp)
{
	struct ossl_softc *sc = device_get_softc(dev);

	if ((csp->csp_flags & ~(CSP_F_SEPARATE_OUTPUT | CSP_F_SEPARATE_AAD)) !=
	    0)
		return (EINVAL);
	switch (csp->csp_mode) {
	case CSP_MODE_DIGEST:
		if (ossl_lookup_hash(csp) == NULL)
			return (EINVAL);
		break;
	case CSP_MODE_CIPHER:
		if (csp->csp_cipher_alg != CRYPTO_CHACHA20 && !sc->has_aes)
			return (EINVAL);
		if (ossl_lookup_cipher(csp) == NULL)
			return (EINVAL);
		break;
	case CSP_MODE_ETA:
		if (!sc->has_aes ||
		    csp->csp_cipher_alg == CRYPTO_CHACHA20 ||
		    ossl_lookup_hash(csp) == NULL ||
		    ossl_lookup_cipher(csp) == NULL)
			return (EINVAL);
		break;
	case CSP_MODE_AEAD:
		switch (csp->csp_cipher_alg) {
		case CRYPTO_CHACHA20_POLY1305:
			break;
		case CRYPTO_AES_NIST_GCM_16:
			if (!sc->has_aes_gcm || ossl_lookup_cipher(csp) == NULL)
				return (EINVAL);
			if (csp->csp_ivlen != AES_GCM_IV_LEN)
				return (EINVAL);
			if (csp->csp_auth_mlen != 0 &&
			    csp->csp_auth_mlen != GMAC_DIGEST_LEN)
				return (EINVAL);
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
ossl_newsession_cipher(struct ossl_session *s,
    const struct crypto_session_params *csp)
{
	struct ossl_cipher *cipher;
	int error = 0;

	cipher = ossl_lookup_cipher(csp);
	if (cipher == NULL)
		return (EINVAL);

	s->cipher.cipher = cipher;

	if (csp->csp_cipher_key == NULL)
		return (0);

	fpu_kern_enter(curthread, NULL, FPU_KERN_NOCTX);
	if (cipher->set_encrypt_key != NULL) {
		error = cipher->set_encrypt_key(csp->csp_cipher_key,
		    8 * csp->csp_cipher_klen, &s->cipher.enc_ctx);
		if (error != 0) {
			fpu_kern_leave(curthread, NULL);
			return (error);
		}
	}
	if (cipher->set_decrypt_key != NULL)
		error = cipher->set_decrypt_key(csp->csp_cipher_key,
		    8 * csp->csp_cipher_klen, &s->cipher.dec_ctx);
	fpu_kern_leave(curthread, NULL);

	return (error);
}

static int
ossl_newsession(device_t dev, crypto_session_t cses,
    const struct crypto_session_params *csp)
{
	struct ossl_session *s;
	int error = 0;

	s = crypto_get_driver_session(cses);
	switch (csp->csp_mode) {
	case CSP_MODE_DIGEST:
		ossl_newsession_hash(s, csp);
		break;
	case CSP_MODE_CIPHER:
		error = ossl_newsession_cipher(s, csp);
		break;
	case CSP_MODE_ETA:
		ossl_newsession_hash(s, csp);
		error = ossl_newsession_cipher(s, csp);
		break;
	case CSP_MODE_AEAD:
		if (csp->csp_cipher_alg != CRYPTO_CHACHA20_POLY1305)
			error = ossl_newsession_cipher(s, csp);
		break;
	default:
		__assert_unreachable();
	}

	return (error);
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
ossl_process_cipher(struct ossl_session *s, struct cryptop *crp,
    const struct crypto_session_params *csp)
{
	return (s->cipher.cipher->process(&s->cipher, crp, csp));
}

static int
ossl_process_eta(struct ossl_session *s, struct cryptop *crp,
    const struct crypto_session_params *csp)
{
	int error;

	if (CRYPTO_OP_IS_ENCRYPT(crp->crp_op)) {
		error = s->cipher.cipher->process(&s->cipher, crp, csp);
		if (error == 0)
			error = ossl_process_hash(s, crp, csp);
	} else {
		error = ossl_process_hash(s, crp, csp);
		if (error == 0)
			error = s->cipher.cipher->process(&s->cipher, crp, csp);
	}

	return (error);
}

static int
ossl_process_aead(struct ossl_session *s, struct cryptop *crp,
    const struct crypto_session_params *csp)
{
	if (csp->csp_cipher_alg == CRYPTO_CHACHA20_POLY1305) {
		if (CRYPTO_OP_IS_ENCRYPT(crp->crp_op))
			return (ossl_chacha20_poly1305_encrypt(crp, csp));
		else
			return (ossl_chacha20_poly1305_decrypt(crp, csp));
	} else {
		return (s->cipher.cipher->process(&s->cipher, crp, csp));
	}
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
		error = ossl_process_cipher(s, crp, csp);
		break;
	case CSP_MODE_ETA:
		error = ossl_process_eta(s, crp, csp);
		break;
	case CSP_MODE_AEAD:
		error = ossl_process_aead(s, crp, csp);
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

DRIVER_MODULE(ossl, nexus, ossl_driver, NULL, NULL);
MODULE_VERSION(ossl, 1);
MODULE_DEPEND(ossl, crypto, 1, 1, 1);
