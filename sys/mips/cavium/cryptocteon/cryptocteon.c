/*
 * Octeon Crypto for OCF
 *
 * Written by David McCullough <david_mccullough@securecomputing.com>
 * Copyright (C) 2009 David McCullough
 *
 * LICENSE TERMS
 *
 * The free distribution and use of this software in both source and binary
 * form is allowed (with or without changes) provided that:
 *
 *   1. distributions of this source code include the above copyright
 *      notice, this list of conditions and the following disclaimer;
 *
 *   2. distributions in binary form include the above copyright
 *      notice, this list of conditions and the following disclaimer
 *      in the documentation and/or other associated materials;
 *
 *   3. the copyright holder's name is not used to endorse products
 *      built using this software without specific written permission.
 *
 * DISCLAIMER
 *
 * This software is provided 'as is' with no explicit or implied warranties
 * in respect of its properties, including, but not limited to, correctness
 * and/or fitness for purpose.
 * ---------------------------------------------------------------------------
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/uio.h>

#include <opencrypto/cryptodev.h>

#include <contrib/octeon-sdk/cvmx.h>

#include <mips/cavium/cryptocteon/cryptocteonvar.h>

#include "cryptodev_if.h"

struct cryptocteon_softc {
	int32_t			sc_cid;		/* opencrypto id */
};

int cryptocteon_debug = 0;
TUNABLE_INT("hw.cryptocteon.debug", &cryptocteon_debug);

static void cryptocteon_identify(driver_t *, device_t);
static int cryptocteon_probe(device_t);
static int cryptocteon_attach(device_t);

static int cryptocteon_process(device_t, struct cryptop *, int);
static int cryptocteon_probesession(device_t,
    const struct crypto_session_params *);
static int cryptocteon_newsession(device_t, crypto_session_t,
    const struct crypto_session_params *);

static void
cryptocteon_identify(driver_t *drv, device_t parent)
{
	if (octeon_has_feature(OCTEON_FEATURE_CRYPTO))
		BUS_ADD_CHILD(parent, 0, "cryptocteon", 0);
}

static int
cryptocteon_probe(device_t dev)
{
	device_set_desc(dev, "Octeon Secure Coprocessor");
	return (0);
}

static int
cryptocteon_attach(device_t dev)
{
	struct cryptocteon_softc *sc;

	sc = device_get_softc(dev);

	sc->sc_cid = crypto_get_driverid(dev, sizeof(struct octo_sess),
	    CRYPTOCAP_F_SOFTWARE | CRYPTOCAP_F_SYNC |
	    CRYPTOCAP_F_ACCEL_SOFTWARE);
	if (sc->sc_cid < 0) {
		device_printf(dev, "crypto_get_driverid ret %d\n", sc->sc_cid);
		return (ENXIO);
	}

	return (0);
}

static bool
cryptocteon_auth_supported(const struct crypto_session_params *csp)
{
	u_int hash_len;

	switch (csp->csp_auth_alg) {
	case CRYPTO_SHA1_HMAC:
		hash_len = SHA1_HASH_LEN;
		break;
	default:
		return (false);
	}

	if (csp->csp_auth_klen > hash_len)
		return (false);
	return (true);
}

static bool
cryptocteon_cipher_supported(const struct crypto_session_params *csp)
{

	switch (csp->csp_cipher_alg) {
	case CRYPTO_AES_CBC:
		if (csp->csp_ivlen != 16)
			return (false);
		if (csp->csp_cipher_klen != 16 &&
		    csp->csp_cipher_klen != 24 &&
		    csp->csp_cipher_klen != 32)
			return (false);
		break;
	default:
		return (false);
	}

	return (true);
}

static int
cryptocteon_probesession(device_t dev, const struct crypto_session_params *csp)
{

	if (csp->csp_flags != 0)
		return (EINVAL);
	switch (csp->csp_mode) {
	case CSP_MODE_DIGEST:
		if (!cryptocteon_auth_supported(csp))
			return (EINVAL);
		break;
	case CSP_MODE_CIPHER:
		if (!cryptocteon_cipher_supported(csp))
			return (EINVAL);
		break;
	case CSP_MODE_ETA:
		if (!cryptocteon_auth_supported(csp) ||
		    !cryptocteon_cipher_supported(csp))
			return (EINVAL);
		break;
	default:
		return (EINVAL);
	}
	return (CRYPTODEV_PROBE_ACCEL_SOFTWARE);
}

static void
cryptocteon_calc_hash(const struct crypto_session_params *csp, const char *key,
    struct octo_sess *ocd)
{
	char hash_key[SHA1_HASH_LEN];

	memset(hash_key, 0, sizeof(hash_key));
	memcpy(hash_key, key, csp->csp_auth_klen);
	octo_calc_hash(csp->csp_auth_alg == CRYPTO_SHA1_HMAC, hash_key,
	    ocd->octo_hminner, ocd->octo_hmouter);
}

/* Generate a new octo session. */
static int
cryptocteon_newsession(device_t dev, crypto_session_t cses,
    const struct crypto_session_params *csp)
{
	struct cryptocteon_softc *sc;
	struct octo_sess *ocd;

	sc = device_get_softc(dev);

	ocd = crypto_get_driver_session(cses);

	ocd->octo_encklen = csp->csp_cipher_klen;
	if (csp->csp_cipher_key != NULL)
		memcpy(ocd->octo_enckey, csp->csp_cipher_key,
		    ocd->octo_encklen);

	if (csp->csp_auth_key != NULL)
		cryptocteon_calc_hash(csp, csp->csp_auth_key, ocd);

	ocd->octo_mlen = csp->csp_auth_mlen;
	if (csp->csp_auth_mlen == 0) {
		switch (csp->csp_auth_alg) {
		case CRYPTO_SHA1_HMAC:
			ocd->octo_mlen = SHA1_HASH_LEN;
			break;
		}
	}

	switch (csp->csp_mode) {
	case CSP_MODE_DIGEST:
		switch (csp->csp_auth_alg) {
		case CRYPTO_SHA1_HMAC:
			ocd->octo_encrypt = octo_null_sha1_encrypt;
			ocd->octo_decrypt = octo_null_sha1_encrypt;
			break;
		}
		break;
	case CSP_MODE_CIPHER:
		switch (csp->csp_cipher_alg) {
		case CRYPTO_AES_CBC:
			ocd->octo_encrypt = octo_aes_cbc_encrypt;
			ocd->octo_decrypt = octo_aes_cbc_decrypt;
			break;
		}
		break;
	case CSP_MODE_ETA:
		switch (csp->csp_cipher_alg) {
		case CRYPTO_AES_CBC:
			switch (csp->csp_auth_alg) {
			case CRYPTO_SHA1_HMAC:
				ocd->octo_encrypt = octo_aes_cbc_sha1_encrypt;
				ocd->octo_decrypt = octo_aes_cbc_sha1_decrypt;
				break;
			}
			break;
		}
		break;
	}

	KASSERT(ocd->octo_encrypt != NULL && ocd->octo_decrypt != NULL,
	    ("%s: missing function pointers", __func__));

	return (0);
}

/*
 * Process a request.
 */
static int
cryptocteon_process(device_t dev, struct cryptop *crp, int hint)
{
	const struct crypto_session_params *csp;
	struct octo_sess *od;
	size_t iovcnt, iovlen;
	struct mbuf *m = NULL;
	struct uio *uiop = NULL;
	unsigned char *ivp = NULL;
	unsigned char iv_data[16];
	unsigned char icv[SHA1_HASH_LEN], icv2[SHA1_HASH_LEN];
	int auth_off, auth_len, crypt_off, crypt_len;
	struct cryptocteon_softc *sc;

	sc = device_get_softc(dev);

	crp->crp_etype = 0;

	od = crypto_get_driver_session(crp->crp_session);
	csp = crypto_get_params(crp->crp_session);

	/*
	 * The crypto routines assume that the regions to auth and
	 * cipher are exactly 8 byte multiples and aligned on 8
	 * byte logical boundaries within the iovecs.
	 */
	if (crp->crp_aad_length % 8 != 0 || crp->crp_payload_length % 8 != 0) {
		crp->crp_etype = EFBIG;
		goto done;
	}

	/*
	 * As currently written, the crypto routines assume the AAD and
	 * payload are adjacent.
	 */
	if (crp->crp_aad_length != 0 && crp->crp_payload_start !=
	    crp->crp_aad_start + crp->crp_aad_length) {
		crp->crp_etype = EFBIG;
		goto done;
	}

	crypt_off = crp->crp_payload_start;
	crypt_len = crp->crp_payload_length;
	if (crp->crp_aad_length != 0) {
		auth_off = crp->crp_aad_start;
		auth_len = crp->crp_aad_length + crp->crp_payload_length;
	} else {
		auth_off = crypt_off;
		auth_len = crypt_len;
	}

	/*
	 * do some error checking outside of the loop for m and IOV processing
	 * this leaves us with valid m or uiop pointers for later
	 */
	switch (crp->crp_buf.cb_type) {
	case CRYPTO_BUF_MBUF:
	{
		unsigned frags;

		m = crp->crp_buf.cb_mbuf;
		for (frags = 0; m != NULL; frags++)
			m = m->m_next;

		if (frags >= UIO_MAXIOV) {
			printf("%s,%d: %d frags > UIO_MAXIOV", __FILE__, __LINE__, frags);
			crp->crp_etype = EFBIG;
			goto done;
		}

		m = crp->crp_buf.cb_mbuf;
		break;
	}
	case CRYPTO_BUF_UIO:
		uiop = crp->crp_buf.cb_uio;
		if (uiop->uio_iovcnt > UIO_MAXIOV) {
			printf("%s,%d: %d uio_iovcnt > UIO_MAXIOV", __FILE__, __LINE__,
			       uiop->uio_iovcnt);
			crp->crp_etype = EFBIG;
			goto done;
		}
		break;
	default:
		break;
	}

	if (csp->csp_cipher_alg != 0) {
		if (crp->crp_flags & CRYPTO_F_IV_SEPARATE)
			ivp = crp->crp_iv;
		else {
			crypto_copydata(crp, crp->crp_iv_start, csp->csp_ivlen,
			    iv_data);
			ivp = iv_data;
		}
	}

	/*
	 * setup the I/O vector to cover the buffer
	 */
	switch (crp->crp_buf.cb_type) {
	case CRYPTO_BUF_MBUF:
		iovcnt = 0;
		iovlen = 0;

		while (m != NULL) {
			od->octo_iov[iovcnt].iov_base = mtod(m, void *);
			od->octo_iov[iovcnt].iov_len = m->m_len;

			m = m->m_next;
			iovlen += od->octo_iov[iovcnt++].iov_len;
		}
		break;
	case CRYPTO_BUF_UIO:
		iovlen = 0;
		for (iovcnt = 0; iovcnt < uiop->uio_iovcnt; iovcnt++) {
			od->octo_iov[iovcnt].iov_base = uiop->uio_iov[iovcnt].iov_base;
			od->octo_iov[iovcnt].iov_len = uiop->uio_iov[iovcnt].iov_len;

			iovlen += od->octo_iov[iovcnt].iov_len;
		}
		break;
	case CRYPTO_BUF_CONTIG:
		iovlen = crp->crp_buf.cb_buf_len;
		od->octo_iov[0].iov_base = crp->crp_buf.cb_buf;
		od->octo_iov[0].iov_len = crp->crp_buf.cb_buf_len;
		iovcnt = 1;
		break;
	default:
		panic("can't happen");
	}

	/*
	 * setup a new explicit key
	 */
	if (crp->crp_cipher_key != NULL)
		memcpy(od->octo_enckey, crp->crp_cipher_key, od->octo_encklen);
	if (crp->crp_auth_key != NULL)
		cryptocteon_calc_hash(csp, crp->crp_auth_key, od);

	if (CRYPTO_OP_IS_ENCRYPT(crp->crp_op))
		(*od->octo_encrypt)(od, od->octo_iov, iovcnt, iovlen,
		    auth_off, auth_len, crypt_off, crypt_len, icv, ivp);
	else
		(*od->octo_decrypt)(od, od->octo_iov, iovcnt, iovlen,
		    auth_off, auth_len, crypt_off, crypt_len, icv, ivp);

	if (csp->csp_auth_alg != 0) {
		if (crp->crp_op & CRYPTO_OP_VERIFY_DIGEST) {
			crypto_copydata(crp, crp->crp_digest_start,
			    od->octo_mlen, icv2);
			if (timingsafe_bcmp(icv, icv2, od->octo_mlen) != 0)
				crp->crp_etype = EBADMSG;
		} else
			crypto_copyback(crp, crp->crp_digest_start,
			    od->octo_mlen, icv);
	}
done:
	crypto_done(crp);
	return (0);
}

static device_method_t cryptocteon_methods[] = {
	/* device methods */
	DEVMETHOD(device_identify,	cryptocteon_identify),
	DEVMETHOD(device_probe,		cryptocteon_probe),
	DEVMETHOD(device_attach,	cryptocteon_attach),

	/* crypto device methods */
	DEVMETHOD(cryptodev_probesession, cryptocteon_probesession),
	DEVMETHOD(cryptodev_newsession,	cryptocteon_newsession),
	DEVMETHOD(cryptodev_process,	cryptocteon_process),
	{ 0, 0 }
};

static driver_t cryptocteon_driver = {
	"cryptocteon",
	cryptocteon_methods,
	sizeof (struct cryptocteon_softc),
};
static devclass_t cryptocteon_devclass;
DRIVER_MODULE(cryptocteon, nexus, cryptocteon_driver, cryptocteon_devclass, 0, 0);
