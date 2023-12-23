/*-
 * Copyright (c) 2005-2008 Pawel Jakub Dawidek <pjd@FreeBSD.org>
 * Copyright (c) 2010 Konstantin Belousov <kib@FreeBSD.org>
 * Copyright (c) 2014,2016 The FreeBSD Foundation
 * Copyright (c) 2020 Ampere Computing
 * All rights reserved.
 *
 * Portions of this software were developed by John-Mark Gurney
 * under sponsorship of the FreeBSD Foundation and
 * Rubicon Communications, LLC (Netgate).
 *
 * This software was developed by Andrew Turner under
 * sponsorship from the FreeBSD Foundation.
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

/*
 * This is based on the aesni code.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/queue.h>
#include <sys/smp.h>
#include <sys/uio.h>

#include <machine/vfp.h>

#include <opencrypto/cryptodev.h>
#include <opencrypto/gmac.h>
#include <cryptodev_if.h>
#include <crypto/armv8/armv8_crypto.h>
#include <crypto/rijndael/rijndael.h>

struct armv8_crypto_softc {
	int32_t		cid;
	bool		has_pmul;
};

static int armv8_crypto_cipher_process(struct armv8_crypto_session *,
    struct cryptop *);

MALLOC_DEFINE(M_ARMV8_CRYPTO, "armv8_crypto", "ARMv8 Crypto Data");

static void
armv8_crypto_identify(driver_t *drv, device_t parent)
{

	/* NB: order 10 is so we get attached after h/w devices */
	if (device_find_child(parent, "armv8crypto", -1) == NULL &&
	    BUS_ADD_CHILD(parent, 10, "armv8crypto", -1) == 0)
		panic("ARMv8 crypto: could not attach");
}

static int
armv8_crypto_probe(device_t dev)
{
	uint64_t reg;
	int ret = ENXIO;

	reg = READ_SPECIALREG(id_aa64isar0_el1);

	switch (ID_AA64ISAR0_AES_VAL(reg)) {
	case ID_AA64ISAR0_AES_BASE:
		ret = 0;
		device_set_desc(dev, "AES-CBC,AES-XTS");
		break;
	case ID_AA64ISAR0_AES_PMULL:
		ret = 0;
		device_set_desc(dev, "AES-CBC,AES-XTS,AES-GCM");
		break;
	default:
		break;
	case ID_AA64ISAR0_AES_NONE:
		device_printf(dev, "CPU lacks AES instructions\n");
		break;
	}

	/* TODO: Check more fields as we support more features */

	return (ret);
}

static int
armv8_crypto_attach(device_t dev)
{
	struct armv8_crypto_softc *sc;
	uint64_t reg;

	sc = device_get_softc(dev);

	reg = READ_SPECIALREG(id_aa64isar0_el1);

	if (ID_AA64ISAR0_AES_VAL(reg) == ID_AA64ISAR0_AES_PMULL)
		sc->has_pmul = true;

	sc->cid = crypto_get_driverid(dev, sizeof(struct armv8_crypto_session),
	    CRYPTOCAP_F_SOFTWARE | CRYPTOCAP_F_SYNC | CRYPTOCAP_F_ACCEL_SOFTWARE);
	if (sc->cid < 0) {
		device_printf(dev, "Could not get crypto driver id.\n");
		return (ENOMEM);
	}

	return (0);
}

static int
armv8_crypto_detach(device_t dev)
{
	struct armv8_crypto_softc *sc;

	sc = device_get_softc(dev);

	crypto_unregister_all(sc->cid);

	return (0);
}

#define SUPPORTED_SES (CSP_F_SEPARATE_OUTPUT | CSP_F_SEPARATE_AAD)

static int
armv8_crypto_probesession(device_t dev,
    const struct crypto_session_params *csp)
{
	struct armv8_crypto_softc *sc;

	sc = device_get_softc(dev);

	if ((csp->csp_flags & ~(SUPPORTED_SES)) != 0)
		return (EINVAL);

	switch (csp->csp_mode) {
	case CSP_MODE_AEAD:
		switch (csp->csp_cipher_alg) {
		case CRYPTO_AES_NIST_GCM_16:
			if (!sc->has_pmul)
				return (EINVAL);
			if (csp->csp_auth_mlen != 0 &&
			    csp->csp_auth_mlen != GMAC_DIGEST_LEN)
				return (EINVAL);
			switch (csp->csp_cipher_klen * 8) {
			case 128:
			case 192:
			case 256:
				break;
			default:
				return (EINVAL);
			}
			break;
		default:
			return (EINVAL);
		}
		break;
	case CSP_MODE_CIPHER:
		switch (csp->csp_cipher_alg) {
		case CRYPTO_AES_CBC:
			if (csp->csp_ivlen != AES_BLOCK_LEN)
				return (EINVAL);
			switch (csp->csp_cipher_klen * 8) {
			case 128:
			case 192:
			case 256:
				break;
			default:
				return (EINVAL);
			}
			break;
		case CRYPTO_AES_XTS:
			if (csp->csp_ivlen != AES_XTS_IV_LEN)
				return (EINVAL);
			switch (csp->csp_cipher_klen * 8) {
			case 256:
			case 512:
				break;
			default:
				return (EINVAL);
			}
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

static int
armv8_crypto_cipher_setup(struct armv8_crypto_session *ses,
    const struct crypto_session_params *csp, const uint8_t *key, int keylen)
{
	__uint128_val_t H;

	if (csp->csp_cipher_alg == CRYPTO_AES_XTS)
		keylen /= 2;

	switch (keylen * 8) {
	case 128:
	case 192:
	case 256:
		break;
	default:
		return (EINVAL);
	}

	fpu_kern_enter(curthread, NULL, FPU_KERN_NORMAL | FPU_KERN_NOCTX);

	aes_v8_set_encrypt_key(key,
	    keylen * 8, &ses->enc_schedule);

	if ((csp->csp_cipher_alg == CRYPTO_AES_XTS) ||
	    (csp->csp_cipher_alg == CRYPTO_AES_CBC))
		aes_v8_set_decrypt_key(key,
		    keylen * 8, &ses->dec_schedule);

	if (csp->csp_cipher_alg == CRYPTO_AES_XTS)
		aes_v8_set_encrypt_key(key + keylen, keylen * 8, &ses->xts_schedule);

	if (csp->csp_cipher_alg == CRYPTO_AES_NIST_GCM_16) {
		memset(H.c, 0, sizeof(H.c));
		aes_v8_encrypt(H.c, H.c, &ses->enc_schedule);
		H.u[0] = bswap64(H.u[0]);
		H.u[1] = bswap64(H.u[1]);
		gcm_init_v8(ses->Htable, H.u);
	}

	fpu_kern_leave(curthread, NULL);

	return (0);
}

static int
armv8_crypto_newsession(device_t dev, crypto_session_t cses,
    const struct crypto_session_params *csp)
{
	struct armv8_crypto_session *ses;
	int error;

	ses = crypto_get_driver_session(cses);
	error = armv8_crypto_cipher_setup(ses, csp, csp->csp_cipher_key,
	    csp->csp_cipher_klen);
	return (error);
}

static int
armv8_crypto_process(device_t dev, struct cryptop *crp, int hint __unused)
{
	struct armv8_crypto_session *ses;

	ses = crypto_get_driver_session(crp->crp_session);
	crp->crp_etype = armv8_crypto_cipher_process(ses, crp);
	crypto_done(crp);
	return (0);
}

static uint8_t *
armv8_crypto_cipher_alloc(struct cryptop *crp, int start, int length, int *allocated)
{
	uint8_t *addr;

	addr = crypto_contiguous_subsegment(crp, start, length);
	if (addr != NULL) {
		*allocated = 0;
		return (addr);
	}
	addr = malloc(crp->crp_payload_length, M_ARMV8_CRYPTO, M_NOWAIT);
	if (addr != NULL) {
		*allocated = 1;
		crypto_copydata(crp, start, length, addr);
	} else
		*allocated = 0;
	return (addr);
}

static int
armv8_crypto_cipher_process(struct armv8_crypto_session *ses,
    struct cryptop *crp)
{
	struct crypto_buffer_cursor fromc, toc;
	const struct crypto_session_params *csp;
	uint8_t *authbuf;
	uint8_t iv[AES_BLOCK_LEN], tag[GMAC_DIGEST_LEN];
	int authallocated;
	int encflag;
	int error;

	csp = crypto_get_params(crp->crp_session);
	encflag = CRYPTO_OP_IS_ENCRYPT(crp->crp_op);

	authallocated = 0;
	authbuf = NULL;

	if (csp->csp_cipher_alg == CRYPTO_AES_NIST_GCM_16) {
		if (crp->crp_aad != NULL)
			authbuf = crp->crp_aad;
		else
			authbuf = armv8_crypto_cipher_alloc(crp, crp->crp_aad_start,
			    crp->crp_aad_length, &authallocated);
		if (authbuf == NULL)
			return (ENOMEM);
	}
	crypto_cursor_init(&fromc, &crp->crp_buf);
	crypto_cursor_advance(&fromc, crp->crp_payload_start);
	if (CRYPTO_HAS_OUTPUT_BUFFER(crp)) {
		crypto_cursor_init(&toc, &crp->crp_obuf);
		crypto_cursor_advance(&toc, crp->crp_payload_output_start);
	} else {
		crypto_cursor_copy(&fromc, &toc);
	}

	if (crp->crp_cipher_key != NULL) {
		armv8_crypto_cipher_setup(ses, csp, crp->crp_cipher_key,
		    csp->csp_cipher_klen);
	}

	crypto_read_iv(crp, iv);

	fpu_kern_enter(curthread, NULL, FPU_KERN_NORMAL | FPU_KERN_NOCTX);

	error = 0;
	switch (csp->csp_cipher_alg) {
	case CRYPTO_AES_CBC:
		if ((crp->crp_payload_length % AES_BLOCK_LEN) != 0) {
			error = EINVAL;
			break;
		}
		if (encflag)
			armv8_aes_encrypt_cbc(&ses->enc_schedule,
			    crp->crp_payload_length, &fromc, &toc, iv);
		else
			armv8_aes_decrypt_cbc(&ses->dec_schedule,
			    crp->crp_payload_length, &fromc, &toc, iv);
		break;
	case CRYPTO_AES_XTS:
		if (encflag)
			armv8_aes_encrypt_xts(&ses->enc_schedule,
			    &ses->xts_schedule.aes_key, crp->crp_payload_length,
			    &fromc, &toc, iv);
		else
			armv8_aes_decrypt_xts(&ses->dec_schedule,
			    &ses->xts_schedule.aes_key, crp->crp_payload_length,
			    &fromc, &toc, iv);
		break;
	case CRYPTO_AES_NIST_GCM_16:
		if (encflag) {
			memset(tag, 0, sizeof(tag));
			armv8_aes_encrypt_gcm(&ses->enc_schedule,
			    crp->crp_payload_length, &fromc, &toc,
			    crp->crp_aad_length, authbuf, tag, iv, ses->Htable);
			crypto_copyback(crp, crp->crp_digest_start, sizeof(tag),
			    tag);
		} else {
			crypto_copydata(crp, crp->crp_digest_start, sizeof(tag),
			    tag);
			error = armv8_aes_decrypt_gcm(&ses->enc_schedule,
			    crp->crp_payload_length, &fromc, &toc,
			    crp->crp_aad_length, authbuf, tag, iv, ses->Htable);
		}
		break;
	}

	fpu_kern_leave(curthread, NULL);

	if (authallocated)
		zfree(authbuf, M_ARMV8_CRYPTO);
	explicit_bzero(iv, sizeof(iv));
	explicit_bzero(tag, sizeof(tag));

	return (error);
}

static device_method_t armv8_crypto_methods[] = {
	DEVMETHOD(device_identify,	armv8_crypto_identify),
	DEVMETHOD(device_probe,		armv8_crypto_probe),
	DEVMETHOD(device_attach,	armv8_crypto_attach),
	DEVMETHOD(device_detach,	armv8_crypto_detach),

	DEVMETHOD(cryptodev_probesession, armv8_crypto_probesession),
	DEVMETHOD(cryptodev_newsession,	armv8_crypto_newsession),
	DEVMETHOD(cryptodev_process,	armv8_crypto_process),

	DEVMETHOD_END,
};

static DEFINE_CLASS_0(armv8crypto, armv8_crypto_driver, armv8_crypto_methods,
    sizeof(struct armv8_crypto_softc));

DRIVER_MODULE(armv8crypto, nexus, armv8_crypto_driver, 0, 0);
