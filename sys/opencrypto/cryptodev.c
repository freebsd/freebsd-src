/*	$OpenBSD: cryptodev.c,v 1.52 2002/06/19 07:22:46 deraadt Exp $	*/

/*-
 * Copyright (c) 2001 Theo de Raadt
 * Copyright (c) 2002-2006 Sam Leffler, Errno Consulting
 * Copyright (c) 2014-2021 The FreeBSD Foundation
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
 *
 * 1. Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Effort sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F30602-01-2-0537.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/errno.h>
#include <sys/random.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/fcntl.h>
#include <sys/bus.h>
#include <sys/sdt.h>
#include <sys/syscallsubr.h>

#include <opencrypto/cryptodev.h>
#include <opencrypto/xform.h>

SDT_PROVIDER_DECLARE(opencrypto);

SDT_PROBE_DEFINE1(opencrypto, dev, ioctl, error, "int"/*line number*/);

#ifdef COMPAT_FREEBSD12
/*
 * Previously, most ioctls were performed against a cloned descriptor
 * of /dev/crypto obtained via CRIOGET.  Now all ioctls are performed
 * against /dev/crypto directly.
 */
#define	CRIOGET		_IOWR('c', 100, uint32_t)
#endif

/* the following are done against the cloned descriptor */

#ifdef COMPAT_FREEBSD32
#include <sys/mount.h>
#include <compat/freebsd32/freebsd32.h>

struct session_op32 {
	uint32_t	cipher;
	uint32_t	mac;
	uint32_t	keylen;
	uint32_t	key;
	int		mackeylen;
	uint32_t	mackey;
	uint32_t	ses;
};

struct session2_op32 {
	uint32_t	cipher;
	uint32_t	mac;
	uint32_t	keylen;
	uint32_t	key;
	int		mackeylen;
	uint32_t	mackey;
	uint32_t	ses;
	int		crid;
	int		ivlen;
	int		maclen;
	int		pad[2];
};

struct crypt_op32 {
	uint32_t	ses;
	uint16_t	op;
	uint16_t	flags;
	u_int		len;
	uint32_t	src, dst;
	uint32_t	mac;
	uint32_t	iv;
};

struct crypt_aead32 {
	uint32_t	ses;
	uint16_t	op;
	uint16_t	flags;
	u_int		len;
	u_int		aadlen;
	u_int		ivlen;
	uint32_t	src;
	uint32_t	dst;
	uint32_t	aad;
	uint32_t	tag;
	uint32_t	iv;
};

#define	CIOCGSESSION32	_IOWR('c', 101, struct session_op32)
#define	CIOCCRYPT32	_IOWR('c', 103, struct crypt_op32)
#define	CIOCGSESSION232	_IOWR('c', 106, struct session2_op32)
#define	CIOCCRYPTAEAD32	_IOWR('c', 109, struct crypt_aead32)

static void
session_op_from_32(const struct session_op32 *from, struct session2_op *to)
{

	memset(to, 0, sizeof(*to));
	CP(*from, *to, cipher);
	CP(*from, *to, mac);
	CP(*from, *to, keylen);
	PTRIN_CP(*from, *to, key);
	CP(*from, *to, mackeylen);
	PTRIN_CP(*from, *to, mackey);
	CP(*from, *to, ses);
	to->crid = CRYPTOCAP_F_HARDWARE;
}

static void
session2_op_from_32(const struct session2_op32 *from, struct session2_op *to)
{

	session_op_from_32((const struct session_op32 *)from, to);
	CP(*from, *to, crid);
	CP(*from, *to, ivlen);
	CP(*from, *to, maclen);
}

static void
session_op_to_32(const struct session2_op *from, struct session_op32 *to)
{

	CP(*from, *to, cipher);
	CP(*from, *to, mac);
	CP(*from, *to, keylen);
	PTROUT_CP(*from, *to, key);
	CP(*from, *to, mackeylen);
	PTROUT_CP(*from, *to, mackey);
	CP(*from, *to, ses);
}

static void
session2_op_to_32(const struct session2_op *from, struct session2_op32 *to)
{

	session_op_to_32(from, (struct session_op32 *)to);
	CP(*from, *to, crid);
}

static void
crypt_op_from_32(const struct crypt_op32 *from, struct crypt_op *to)
{

	CP(*from, *to, ses);
	CP(*from, *to, op);
	CP(*from, *to, flags);
	CP(*from, *to, len);
	PTRIN_CP(*from, *to, src);
	PTRIN_CP(*from, *to, dst);
	PTRIN_CP(*from, *to, mac);
	PTRIN_CP(*from, *to, iv);
}

static void
crypt_op_to_32(const struct crypt_op *from, struct crypt_op32 *to)
{

	CP(*from, *to, ses);
	CP(*from, *to, op);
	CP(*from, *to, flags);
	CP(*from, *to, len);
	PTROUT_CP(*from, *to, src);
	PTROUT_CP(*from, *to, dst);
	PTROUT_CP(*from, *to, mac);
	PTROUT_CP(*from, *to, iv);
}

static void
crypt_aead_from_32(const struct crypt_aead32 *from, struct crypt_aead *to)
{

	CP(*from, *to, ses);
	CP(*from, *to, op);
	CP(*from, *to, flags);
	CP(*from, *to, len);
	CP(*from, *to, aadlen);
	CP(*from, *to, ivlen);
	PTRIN_CP(*from, *to, src);
	PTRIN_CP(*from, *to, dst);
	PTRIN_CP(*from, *to, aad);
	PTRIN_CP(*from, *to, tag);
	PTRIN_CP(*from, *to, iv);
}

static void
crypt_aead_to_32(const struct crypt_aead *from, struct crypt_aead32 *to)
{

	CP(*from, *to, ses);
	CP(*from, *to, op);
	CP(*from, *to, flags);
	CP(*from, *to, len);
	CP(*from, *to, aadlen);
	CP(*from, *to, ivlen);
	PTROUT_CP(*from, *to, src);
	PTROUT_CP(*from, *to, dst);
	PTROUT_CP(*from, *to, aad);
	PTROUT_CP(*from, *to, tag);
	PTROUT_CP(*from, *to, iv);
}
#endif

static void
session2_op_from_op(const struct session_op *from, struct session2_op *to)
{

	memset(to, 0, sizeof(*to));
	memcpy(to, from, sizeof(*from));
	to->crid = CRYPTOCAP_F_HARDWARE;
}

static void
session2_op_to_op(const struct session2_op *from, struct session_op *to)
{

	memcpy(to, from, sizeof(*to));
}

struct csession {
	TAILQ_ENTRY(csession) next;
	crypto_session_t cses;
	volatile u_int	refs;
	uint32_t	ses;
	struct mtx	lock;		/* for op submission */

	const struct enc_xform *txform;
	int		hashsize;
	int		ivsize;

	void		*key;
	void		*mackey;
};

struct cryptop_data {
	struct csession *cse;

	char		*buf;
	char		*obuf;
	char		*aad;
	bool		done;
};

struct fcrypt {
	TAILQ_HEAD(csessionlist, csession) csessions;
	int		sesn;
	struct mtx	lock;
};

static bool use_outputbuffers;
SYSCTL_BOOL(_kern_crypto, OID_AUTO, cryptodev_use_output, CTLFLAG_RW,
    &use_outputbuffers, 0,
    "Use separate output buffers for /dev/crypto requests.");

static bool use_separate_aad;
SYSCTL_BOOL(_kern_crypto, OID_AUTO, cryptodev_separate_aad, CTLFLAG_RW,
    &use_separate_aad, 0,
    "Use separate AAD buffer for /dev/crypto requests.");

/*
 * Check a crypto identifier to see if it requested
 * a software device/driver.  This can be done either
 * by device name/class or through search constraints.
 */
static int
checkforsoftware(int *cridp)
{
	int crid;

	crid = *cridp;

	if (!crypto_devallowsoft) {
		if (crid & CRYPTOCAP_F_SOFTWARE) {
			if (crid & CRYPTOCAP_F_HARDWARE) {
				*cridp = CRYPTOCAP_F_HARDWARE;
				return 0;
			}
			return EINVAL;
		}
		if ((crid & CRYPTOCAP_F_HARDWARE) == 0 &&
		    (crypto_getcaps(crid) & CRYPTOCAP_F_HARDWARE) == 0)
			return EINVAL;
	}
	return 0;
}

static int
cse_create(struct fcrypt *fcr, struct session2_op *sop)
{
	struct crypto_session_params csp;
	struct csession *cse;
	const struct enc_xform *txform;
	const struct auth_hash *thash;
	void *key = NULL;
	void *mackey = NULL;
	crypto_session_t cses;
	int crid, error;

	switch (sop->cipher) {
	case 0:
		txform = NULL;
		break;
	case CRYPTO_AES_CBC:
		txform = &enc_xform_rijndael128;
		break;
	case CRYPTO_AES_XTS:
		txform = &enc_xform_aes_xts;
		break;
	case CRYPTO_NULL_CBC:
		txform = &enc_xform_null;
		break;
	case CRYPTO_CAMELLIA_CBC:
		txform = &enc_xform_camellia;
		break;
	case CRYPTO_AES_ICM:
		txform = &enc_xform_aes_icm;
		break;
	case CRYPTO_AES_NIST_GCM_16:
		txform = &enc_xform_aes_nist_gcm;
		break;
	case CRYPTO_CHACHA20:
		txform = &enc_xform_chacha20;
		break;
	case CRYPTO_AES_CCM_16:
		txform = &enc_xform_ccm;
		break;
	case CRYPTO_CHACHA20_POLY1305:
		txform = &enc_xform_chacha20_poly1305;
		break;
	default:
		CRYPTDEB("invalid cipher");
		SDT_PROBE1(opencrypto, dev, ioctl, error, __LINE__);
		return (EINVAL);
	}

	switch (sop->mac) {
	case 0:
		thash = NULL;
		break;
	case CRYPTO_POLY1305:
		thash = &auth_hash_poly1305;
		break;
	case CRYPTO_SHA1_HMAC:
		thash = &auth_hash_hmac_sha1;
		break;
	case CRYPTO_SHA2_224_HMAC:
		thash = &auth_hash_hmac_sha2_224;
		break;
	case CRYPTO_SHA2_256_HMAC:
		thash = &auth_hash_hmac_sha2_256;
		break;
	case CRYPTO_SHA2_384_HMAC:
		thash = &auth_hash_hmac_sha2_384;
		break;
	case CRYPTO_SHA2_512_HMAC:
		thash = &auth_hash_hmac_sha2_512;
		break;
	case CRYPTO_RIPEMD160_HMAC:
		thash = &auth_hash_hmac_ripemd_160;
		break;
#ifdef COMPAT_FREEBSD12
	case CRYPTO_AES_128_NIST_GMAC:
	case CRYPTO_AES_192_NIST_GMAC:
	case CRYPTO_AES_256_NIST_GMAC:
		/* Should always be paired with GCM. */
		if (sop->cipher != CRYPTO_AES_NIST_GCM_16) {
			CRYPTDEB("GMAC without GCM");
			SDT_PROBE1(opencrypto, dev, ioctl, error, __LINE__);
			return (EINVAL);
		}
		break;
#endif
	case CRYPTO_AES_NIST_GMAC:
		switch (sop->mackeylen * 8) {
		case 128:
			thash = &auth_hash_nist_gmac_aes_128;
			break;
		case 192:
			thash = &auth_hash_nist_gmac_aes_192;
			break;
		case 256:
			thash = &auth_hash_nist_gmac_aes_256;
			break;
		default:
			CRYPTDEB("invalid GMAC key length");
			SDT_PROBE1(opencrypto, dev, ioctl, error, __LINE__);
			return (EINVAL);
		}
		break;
	case CRYPTO_AES_CCM_CBC_MAC:
		switch (sop->mackeylen) {
		case 16:
			thash = &auth_hash_ccm_cbc_mac_128;
			break;
		case 24:
			thash = &auth_hash_ccm_cbc_mac_192;
			break;
		case 32:
			thash = &auth_hash_ccm_cbc_mac_256;
			break;
		default:
			CRYPTDEB("Invalid CBC MAC key size %d", sop->keylen);
			SDT_PROBE1(opencrypto, dev, ioctl, error, __LINE__);
			return (EINVAL);
		}
		break;
	case CRYPTO_SHA1:
		thash = &auth_hash_sha1;
		break;
	case CRYPTO_SHA2_224:
		thash = &auth_hash_sha2_224;
		break;
	case CRYPTO_SHA2_256:
		thash = &auth_hash_sha2_256;
		break;
	case CRYPTO_SHA2_384:
		thash = &auth_hash_sha2_384;
		break;
	case CRYPTO_SHA2_512:
		thash = &auth_hash_sha2_512;
		break;

	case CRYPTO_NULL_HMAC:
		thash = &auth_hash_null;
		break;

	case CRYPTO_BLAKE2B:
		thash = &auth_hash_blake2b;
		break;
	case CRYPTO_BLAKE2S:
		thash = &auth_hash_blake2s;
		break;

	default:
		CRYPTDEB("invalid mac");
		SDT_PROBE1(opencrypto, dev, ioctl, error, __LINE__);
		return (EINVAL);
	}

	if (txform == NULL && thash == NULL) {
		SDT_PROBE1(opencrypto, dev, ioctl, error, __LINE__);
		return (EINVAL);
	}

	memset(&csp, 0, sizeof(csp));
	if (use_outputbuffers)
		csp.csp_flags |= CSP_F_SEPARATE_OUTPUT;

	if (sop->cipher == CRYPTO_AES_NIST_GCM_16) {
		switch (sop->mac) {
#ifdef COMPAT_FREEBSD12
		case CRYPTO_AES_128_NIST_GMAC:
		case CRYPTO_AES_192_NIST_GMAC:
		case CRYPTO_AES_256_NIST_GMAC:
			if (sop->keylen != sop->mackeylen) {
				SDT_PROBE1(opencrypto, dev, ioctl, error,
				    __LINE__);
				return (EINVAL);
			}
			break;
#endif
		case 0:
			break;
		default:
			SDT_PROBE1(opencrypto, dev, ioctl, error, __LINE__);
			return (EINVAL);
		}
		csp.csp_mode = CSP_MODE_AEAD;
	} else if (sop->cipher == CRYPTO_AES_CCM_16) {
		switch (sop->mac) {
#ifdef COMPAT_FREEBSD12
		case CRYPTO_AES_CCM_CBC_MAC:
			if (sop->keylen != sop->mackeylen) {
				SDT_PROBE1(opencrypto, dev, ioctl, error,
				    __LINE__);
				return (EINVAL);
			}
			thash = NULL;
			break;
#endif
		case 0:
			break;
		default:
			SDT_PROBE1(opencrypto, dev, ioctl, error, __LINE__);
			return (EINVAL);
		}
		csp.csp_mode = CSP_MODE_AEAD;
	} else if (sop->cipher == CRYPTO_CHACHA20_POLY1305) {
		if (sop->mac != 0) {
			SDT_PROBE1(opencrypto, dev, ioctl, error, __LINE__);
			return (EINVAL);
		}
		csp.csp_mode = CSP_MODE_AEAD;
	} else if (txform != NULL && thash != NULL)
		csp.csp_mode = CSP_MODE_ETA;
	else if (txform != NULL)
		csp.csp_mode = CSP_MODE_CIPHER;
	else
		csp.csp_mode = CSP_MODE_DIGEST;

	switch (csp.csp_mode) {
	case CSP_MODE_AEAD:
	case CSP_MODE_ETA:
		if (use_separate_aad)
			csp.csp_flags |= CSP_F_SEPARATE_AAD;
		break;
	}

	if (txform != NULL) {
		csp.csp_cipher_alg = txform->type;
		csp.csp_cipher_klen = sop->keylen;
		if (sop->keylen > txform->maxkey ||
		    sop->keylen < txform->minkey) {
			CRYPTDEB("invalid cipher parameters");
			error = EINVAL;
			SDT_PROBE1(opencrypto, dev, ioctl, error, __LINE__);
			goto bail;
		}

		key = malloc(csp.csp_cipher_klen, M_XDATA, M_WAITOK);
		error = copyin(sop->key, key, csp.csp_cipher_klen);
		if (error) {
			CRYPTDEB("invalid key");
			SDT_PROBE1(opencrypto, dev, ioctl, error, __LINE__);
			goto bail;
		}
		csp.csp_cipher_key = key;
		csp.csp_ivlen = txform->ivsize;
	}

	if (thash != NULL) {
		csp.csp_auth_alg = thash->type;
		csp.csp_auth_klen = sop->mackeylen;
		if (sop->mackeylen > thash->keysize || sop->mackeylen < 0) {
			CRYPTDEB("invalid mac key length");
			error = EINVAL;
			SDT_PROBE1(opencrypto, dev, ioctl, error, __LINE__);
			goto bail;
		}

		if (csp.csp_auth_klen != 0) {
			mackey = malloc(csp.csp_auth_klen, M_XDATA, M_WAITOK);
			error = copyin(sop->mackey, mackey, csp.csp_auth_klen);
			if (error) {
				CRYPTDEB("invalid mac key");
				SDT_PROBE1(opencrypto, dev, ioctl, error,
				    __LINE__);
				goto bail;
			}
			csp.csp_auth_key = mackey;
		}

		if (csp.csp_auth_alg == CRYPTO_AES_NIST_GMAC)
			csp.csp_ivlen = AES_GCM_IV_LEN;
		if (csp.csp_auth_alg == CRYPTO_AES_CCM_CBC_MAC)
			csp.csp_ivlen = AES_CCM_IV_LEN;
	}

	if (sop->ivlen != 0) {
		if (csp.csp_ivlen == 0) {
			CRYPTDEB("does not support an IV");
			error = EINVAL;
			SDT_PROBE1(opencrypto, dev, ioctl, error, __LINE__);
			goto bail;
		}
		csp.csp_ivlen = sop->ivlen;
	}
	if (sop->maclen != 0) {
		if (!(thash != NULL || csp.csp_mode == CSP_MODE_AEAD)) {
			CRYPTDEB("does not support a MAC");
			error = EINVAL;
			SDT_PROBE1(opencrypto, dev, ioctl, error, __LINE__);
			goto bail;
		}
		csp.csp_auth_mlen = sop->maclen;
	}

	crid = sop->crid;
	error = checkforsoftware(&crid);
	if (error) {
		CRYPTDEB("checkforsoftware");
		SDT_PROBE1(opencrypto, dev, ioctl, error, __LINE__);
		goto bail;
	}
	error = crypto_newsession(&cses, &csp, crid);
	if (error) {
		CRYPTDEB("crypto_newsession");
		SDT_PROBE1(opencrypto, dev, ioctl, error, __LINE__);
		goto bail;
	}

	cse = malloc(sizeof(struct csession), M_XDATA, M_WAITOK | M_ZERO);
	mtx_init(&cse->lock, "cryptodev", "crypto session lock", MTX_DEF);
	refcount_init(&cse->refs, 1);
	cse->key = key;
	cse->mackey = mackey;
	cse->cses = cses;
	cse->txform = txform;
	if (sop->maclen != 0)
		cse->hashsize = sop->maclen;
	else if (thash != NULL)
		cse->hashsize = thash->hashsize;
	else if (csp.csp_cipher_alg == CRYPTO_AES_NIST_GCM_16)
		cse->hashsize = AES_GMAC_HASH_LEN;
	else if (csp.csp_cipher_alg == CRYPTO_AES_CCM_16)
		cse->hashsize = AES_CBC_MAC_HASH_LEN;
	else if (csp.csp_cipher_alg == CRYPTO_CHACHA20_POLY1305)
		cse->hashsize = POLY1305_HASH_LEN;
	cse->ivsize = csp.csp_ivlen;

	mtx_lock(&fcr->lock);
	TAILQ_INSERT_TAIL(&fcr->csessions, cse, next);
	cse->ses = fcr->sesn++;
	mtx_unlock(&fcr->lock);

	sop->ses = cse->ses;

	/* return hardware/driver id */
	sop->crid = crypto_ses2hid(cse->cses);
bail:
	if (error) {
		free(key, M_XDATA);
		free(mackey, M_XDATA);
	}
	return (error);
}

static struct csession *
cse_find(struct fcrypt *fcr, u_int ses)
{
	struct csession *cse;

	mtx_lock(&fcr->lock);
	TAILQ_FOREACH(cse, &fcr->csessions, next) {
		if (cse->ses == ses) {
			refcount_acquire(&cse->refs);
			mtx_unlock(&fcr->lock);
			return (cse);
		}
	}
	mtx_unlock(&fcr->lock);
	return (NULL);
}

static void
cse_free(struct csession *cse)
{

	if (!refcount_release(&cse->refs))
		return;
	crypto_freesession(cse->cses);
	mtx_destroy(&cse->lock);
	if (cse->key)
		free(cse->key, M_XDATA);
	if (cse->mackey)
		free(cse->mackey, M_XDATA);
	free(cse, M_XDATA);
}

static bool
cse_delete(struct fcrypt *fcr, u_int ses)
{
	struct csession *cse;

	mtx_lock(&fcr->lock);
	TAILQ_FOREACH(cse, &fcr->csessions, next) {
		if (cse->ses == ses) {
			TAILQ_REMOVE(&fcr->csessions, cse, next);
			mtx_unlock(&fcr->lock);
			cse_free(cse);
			return (true);
		}
	}
	mtx_unlock(&fcr->lock);
	return (false);
}

static struct cryptop_data *
cod_alloc(struct csession *cse, size_t aad_len, size_t len)
{
	struct cryptop_data *cod;

	cod = malloc(sizeof(struct cryptop_data), M_XDATA, M_WAITOK | M_ZERO);

	cod->cse = cse;
	if (crypto_get_params(cse->cses)->csp_flags & CSP_F_SEPARATE_AAD) {
		if (aad_len != 0)
			cod->aad = malloc(aad_len, M_XDATA, M_WAITOK);
		cod->buf = malloc(len, M_XDATA, M_WAITOK);
	} else
		cod->buf = malloc(aad_len + len, M_XDATA, M_WAITOK);
	if (crypto_get_params(cse->cses)->csp_flags & CSP_F_SEPARATE_OUTPUT)
		cod->obuf = malloc(len, M_XDATA, M_WAITOK);
	return (cod);
}

static void
cod_free(struct cryptop_data *cod)
{

	free(cod->aad, M_XDATA);
	free(cod->obuf, M_XDATA);
	free(cod->buf, M_XDATA);
	free(cod, M_XDATA);
}

static int
cryptodev_cb(struct cryptop *crp)
{
	struct cryptop_data *cod = crp->crp_opaque;

	/*
	 * Lock to ensure the wakeup() is not missed by the loops
	 * waiting on cod->done in cryptodev_op() and
	 * cryptodev_aead().
	 */
	mtx_lock(&cod->cse->lock);
	cod->done = true;
	mtx_unlock(&cod->cse->lock);
	wakeup(cod);
	return (0);
}

static int
cryptodev_op(struct csession *cse, const struct crypt_op *cop)
{
	const struct crypto_session_params *csp;
	struct cryptop_data *cod = NULL;
	struct cryptop *crp = NULL;
	char *dst;
	int error;

	if (cop->len > 256*1024-4) {
		SDT_PROBE1(opencrypto, dev, ioctl, error, __LINE__);
		return (E2BIG);
	}

	if (cse->txform) {
		if ((cop->len % cse->txform->blocksize) != 0) {
			SDT_PROBE1(opencrypto, dev, ioctl, error, __LINE__);
			return (EINVAL);
		}
	}

	if (cop->mac && cse->hashsize == 0) {
		SDT_PROBE1(opencrypto, dev, ioctl, error, __LINE__);
		return (EINVAL);
	}

	/*
	 * The COP_F_CIPHER_FIRST flag predates explicit session
	 * modes, but the only way it was used was for EtA so allow it
	 * as long as it is consistent with EtA.
	 */
	if (cop->flags & COP_F_CIPHER_FIRST) {
		if (cop->op != COP_ENCRYPT) {
			SDT_PROBE1(opencrypto, dev, ioctl, error,  __LINE__);
			return (EINVAL);
		}
	}

	cod = cod_alloc(cse, 0, cop->len + cse->hashsize);
	dst = cop->dst;

	crp = crypto_getreq(cse->cses, M_WAITOK);

	error = copyin(cop->src, cod->buf, cop->len);
	if (error) {
		SDT_PROBE1(opencrypto, dev, ioctl, error, __LINE__);
		goto bail;
	}
	crp->crp_payload_start = 0;
	crp->crp_payload_length = cop->len;
	if (cse->hashsize)
		crp->crp_digest_start = cop->len;

	csp = crypto_get_params(cse->cses);
	switch (csp->csp_mode) {
	case CSP_MODE_COMPRESS:
		switch (cop->op) {
		case COP_ENCRYPT:
			crp->crp_op = CRYPTO_OP_COMPRESS;
			break;
		case COP_DECRYPT:
			crp->crp_op = CRYPTO_OP_DECOMPRESS;
			break;
		default:
			SDT_PROBE1(opencrypto, dev, ioctl, error, __LINE__);
			error = EINVAL;
			goto bail;
		}
		break;
	case CSP_MODE_CIPHER:
		if (cop->len == 0 ||
		    (cop->iv == NULL && cop->len == cse->ivsize)) {
			SDT_PROBE1(opencrypto, dev, ioctl, error, __LINE__);
			error = EINVAL;
			goto bail;
		}
		switch (cop->op) {
		case COP_ENCRYPT:
			crp->crp_op = CRYPTO_OP_ENCRYPT;
			break;
		case COP_DECRYPT:
			crp->crp_op = CRYPTO_OP_DECRYPT;
			break;
		default:
			SDT_PROBE1(opencrypto, dev, ioctl, error, __LINE__);
			error = EINVAL;
			goto bail;
		}
		break;
	case CSP_MODE_DIGEST:
		switch (cop->op) {
		case 0:
		case COP_ENCRYPT:
		case COP_DECRYPT:
			crp->crp_op = CRYPTO_OP_COMPUTE_DIGEST;
			if (cod->obuf != NULL)
				crp->crp_digest_start = 0;
			break;
		default:
			SDT_PROBE1(opencrypto, dev, ioctl, error, __LINE__);
			error = EINVAL;
			goto bail;
		}
		break;
	case CSP_MODE_AEAD:
		if (cse->ivsize != 0 && cop->iv == NULL) {
			SDT_PROBE1(opencrypto, dev, ioctl, error, __LINE__);
			error = EINVAL;
			goto bail;
		}
		/* FALLTHROUGH */
	case CSP_MODE_ETA:
		switch (cop->op) {
		case COP_ENCRYPT:
			crp->crp_op = CRYPTO_OP_ENCRYPT |
			    CRYPTO_OP_COMPUTE_DIGEST;
			break;
		case COP_DECRYPT:
			crp->crp_op = CRYPTO_OP_DECRYPT |
			    CRYPTO_OP_VERIFY_DIGEST;
			break;
		default:
			SDT_PROBE1(opencrypto, dev, ioctl, error, __LINE__);
			error = EINVAL;
			goto bail;
		}
		break;
	default:
		SDT_PROBE1(opencrypto, dev, ioctl, error, __LINE__);
		error = EINVAL;
		goto bail;
	}

	crp->crp_flags = CRYPTO_F_CBIMM | (cop->flags & COP_F_BATCH);
	crypto_use_buf(crp, cod->buf, cop->len + cse->hashsize);
	if (cod->obuf)
		crypto_use_output_buf(crp, cod->obuf, cop->len + cse->hashsize);
	crp->crp_callback = cryptodev_cb;
	crp->crp_opaque = cod;

	if (cop->iv) {
		if (cse->ivsize == 0) {
			SDT_PROBE1(opencrypto, dev, ioctl, error, __LINE__);
			error = EINVAL;
			goto bail;
		}
		error = copyin(cop->iv, crp->crp_iv, cse->ivsize);
		if (error) {
			SDT_PROBE1(opencrypto, dev, ioctl, error, __LINE__);
			goto bail;
		}
		crp->crp_flags |= CRYPTO_F_IV_SEPARATE;
	} else if (cse->ivsize != 0) {
		if (crp->crp_payload_length < cse->ivsize) {
			SDT_PROBE1(opencrypto, dev, ioctl, error, __LINE__);
			error = EINVAL;
			goto bail;
		}
		crp->crp_iv_start = 0;
		crp->crp_payload_length -= cse->ivsize;
		if (crp->crp_payload_length != 0)
			crp->crp_payload_start = cse->ivsize;
		dst += cse->ivsize;
	}

	if (crp->crp_op & CRYPTO_OP_VERIFY_DIGEST) {
		error = copyin(cop->mac, cod->buf + crp->crp_digest_start,
		    cse->hashsize);
		if (error) {
			SDT_PROBE1(opencrypto, dev, ioctl, error, __LINE__);
			goto bail;
		}
	}
again:
	/*
	 * Let the dispatch run unlocked, then, interlock against the
	 * callback before checking if the operation completed and going
	 * to sleep.  This insures drivers don't inherit our lock which
	 * results in a lock order reversal between crypto_dispatch forced
	 * entry and the crypto_done callback into us.
	 */
	error = crypto_dispatch(crp);
	if (error != 0) {
		SDT_PROBE1(opencrypto, dev, ioctl, error, __LINE__);
		goto bail;
	}

	mtx_lock(&cse->lock);
	while (!cod->done)
		mtx_sleep(cod, &cse->lock, PWAIT, "crydev", 0);
	mtx_unlock(&cse->lock);

	if (crp->crp_etype == EAGAIN) {
		crp->crp_etype = 0;
		crp->crp_flags &= ~CRYPTO_F_DONE;
		cod->done = false;
		goto again;
	}

	if (crp->crp_etype != 0) {
		SDT_PROBE1(opencrypto, dev, ioctl, error, __LINE__);
		error = crp->crp_etype;
		goto bail;
	}

	if (cop->dst != NULL) {
		error = copyout(cod->obuf != NULL ? cod->obuf :
		    cod->buf + crp->crp_payload_start, dst,
		    crp->crp_payload_length);
		if (error) {
			SDT_PROBE1(opencrypto, dev, ioctl, error, __LINE__);
			goto bail;
		}
	}

	if (cop->mac != NULL && (crp->crp_op & CRYPTO_OP_VERIFY_DIGEST) == 0) {
		error = copyout((cod->obuf != NULL ? cod->obuf : cod->buf) +
		    crp->crp_digest_start, cop->mac, cse->hashsize);
		if (error) {
			SDT_PROBE1(opencrypto, dev, ioctl, error, __LINE__);
			goto bail;
		}
	}

bail:
	crypto_freereq(crp);
	cod_free(cod);

	return (error);
}

static int
cryptodev_aead(struct csession *cse, struct crypt_aead *caead)
{
	const struct crypto_session_params *csp;
	struct cryptop_data *cod = NULL;
	struct cryptop *crp = NULL;
	char *dst;
	int error;

	if (caead->len > 256*1024-4 || caead->aadlen > 256*1024-4) {
		SDT_PROBE1(opencrypto, dev, ioctl, error, __LINE__);
		return (E2BIG);
	}

	if (cse->txform == NULL || cse->hashsize == 0 || caead->tag == NULL ||
	    (caead->len % cse->txform->blocksize) != 0) {
		SDT_PROBE1(opencrypto, dev, ioctl, error, __LINE__);
		return (EINVAL);
	}

	/*
	 * The COP_F_CIPHER_FIRST flag predates explicit session
	 * modes, but the only way it was used was for EtA so allow it
	 * as long as it is consistent with EtA.
	 */
	if (caead->flags & COP_F_CIPHER_FIRST) {
		if (caead->op != COP_ENCRYPT) {
			SDT_PROBE1(opencrypto, dev, ioctl, error,  __LINE__);
			return (EINVAL);
		}
	}

	cod = cod_alloc(cse, caead->aadlen, caead->len + cse->hashsize);
	dst = caead->dst;

	crp = crypto_getreq(cse->cses, M_WAITOK);

	if (cod->aad != NULL)
		error = copyin(caead->aad, cod->aad, caead->aadlen);
	else
		error = copyin(caead->aad, cod->buf, caead->aadlen);
	if (error) {
		SDT_PROBE1(opencrypto, dev, ioctl, error, __LINE__);
		goto bail;
	}
	crp->crp_aad = cod->aad;
	crp->crp_aad_start = 0;
	crp->crp_aad_length = caead->aadlen;

	if (cod->aad != NULL)
		crp->crp_payload_start = 0;
	else
		crp->crp_payload_start = caead->aadlen;
	error = copyin(caead->src, cod->buf + crp->crp_payload_start,
	    caead->len);
	if (error) {
		SDT_PROBE1(opencrypto, dev, ioctl, error, __LINE__);
		goto bail;
	}
	crp->crp_payload_length = caead->len;
	if (caead->op == COP_ENCRYPT && cod->obuf != NULL)
		crp->crp_digest_start = crp->crp_payload_output_start +
		    caead->len;
	else
		crp->crp_digest_start = crp->crp_payload_start + caead->len;

	csp = crypto_get_params(cse->cses);
	switch (csp->csp_mode) {
	case CSP_MODE_AEAD:
	case CSP_MODE_ETA:
		switch (caead->op) {
		case COP_ENCRYPT:
			crp->crp_op = CRYPTO_OP_ENCRYPT |
			    CRYPTO_OP_COMPUTE_DIGEST;
			break;
		case COP_DECRYPT:
			crp->crp_op = CRYPTO_OP_DECRYPT |
			    CRYPTO_OP_VERIFY_DIGEST;
			break;
		default:
			SDT_PROBE1(opencrypto, dev, ioctl, error, __LINE__);
			error = EINVAL;
			goto bail;
		}
		break;
	default:
		SDT_PROBE1(opencrypto, dev, ioctl, error, __LINE__);
		error = EINVAL;
		goto bail;
	}

	crp->crp_flags = CRYPTO_F_CBIMM | (caead->flags & COP_F_BATCH);
	crypto_use_buf(crp, cod->buf, crp->crp_payload_start + caead->len +
	    cse->hashsize);
	if (cod->obuf != NULL)
		crypto_use_output_buf(crp, cod->obuf, caead->len +
		    cse->hashsize);
	crp->crp_callback = cryptodev_cb;
	crp->crp_opaque = cod;

	if (caead->iv) {
		/*
		 * Permit a 16-byte IV for AES-XTS, but only use the
		 * first 8 bytes as a block number.
		 */
		if (csp->csp_mode == CSP_MODE_ETA &&
		    csp->csp_cipher_alg == CRYPTO_AES_XTS &&
		    caead->ivlen == AES_BLOCK_LEN)
			caead->ivlen = AES_XTS_IV_LEN;

		if (cse->ivsize == 0) {
			SDT_PROBE1(opencrypto, dev, ioctl, error, __LINE__);
			error = EINVAL;
			goto bail;
		}
		if (caead->ivlen != cse->ivsize) {
			error = EINVAL;
			SDT_PROBE1(opencrypto, dev, ioctl, error, __LINE__);
			goto bail;
		}

		error = copyin(caead->iv, crp->crp_iv, cse->ivsize);
		if (error) {
			SDT_PROBE1(opencrypto, dev, ioctl, error, __LINE__);
			goto bail;
		}
		crp->crp_flags |= CRYPTO_F_IV_SEPARATE;
	} else {
		error = EINVAL;
		SDT_PROBE1(opencrypto, dev, ioctl, error, __LINE__);
		goto bail;
	}

	if (crp->crp_op & CRYPTO_OP_VERIFY_DIGEST) {
		error = copyin(caead->tag, cod->buf + crp->crp_digest_start,
		    cse->hashsize);
		if (error) {
			SDT_PROBE1(opencrypto, dev, ioctl, error, __LINE__);
			goto bail;
		}
	}
again:
	/*
	 * Let the dispatch run unlocked, then, interlock against the
	 * callback before checking if the operation completed and going
	 * to sleep.  This insures drivers don't inherit our lock which
	 * results in a lock order reversal between crypto_dispatch forced
	 * entry and the crypto_done callback into us.
	 */
	error = crypto_dispatch(crp);
	if (error != 0) {
		SDT_PROBE1(opencrypto, dev, ioctl, error, __LINE__);
		goto bail;
	}

	mtx_lock(&cse->lock);
	while (!cod->done)
		mtx_sleep(cod, &cse->lock, PWAIT, "crydev", 0);
	mtx_unlock(&cse->lock);

	if (crp->crp_etype == EAGAIN) {
		crp->crp_etype = 0;
		crp->crp_flags &= ~CRYPTO_F_DONE;
		cod->done = false;
		goto again;
	}

	if (crp->crp_etype != 0) {
		error = crp->crp_etype;
		SDT_PROBE1(opencrypto, dev, ioctl, error, __LINE__);
		goto bail;
	}

	if (caead->dst != NULL) {
		error = copyout(cod->obuf != NULL ? cod->obuf :
		    cod->buf + crp->crp_payload_start, dst,
		    crp->crp_payload_length);
		if (error) {
			SDT_PROBE1(opencrypto, dev, ioctl, error, __LINE__);
			goto bail;
		}
	}

	if ((crp->crp_op & CRYPTO_OP_VERIFY_DIGEST) == 0) {
		error = copyout((cod->obuf != NULL ? cod->obuf : cod->buf) +
		    crp->crp_digest_start, caead->tag, cse->hashsize);
		if (error) {
			SDT_PROBE1(opencrypto, dev, ioctl, error, __LINE__);
			goto bail;
		}
	}

bail:
	crypto_freereq(crp);
	cod_free(cod);

	return (error);
}

static int
cryptodev_find(struct crypt_find_op *find)
{
	device_t dev;
	size_t fnlen = sizeof find->name;

	if (find->crid != -1) {
		dev = crypto_find_device_byhid(find->crid);
		if (dev == NULL)
			return (ENOENT);
		strncpy(find->name, device_get_nameunit(dev), fnlen);
		find->name[fnlen - 1] = '\x0';
	} else {
		find->name[fnlen - 1] = '\x0';
		find->crid = crypto_find_driver(find->name);
		if (find->crid == -1)
			return (ENOENT);
	}
	return (0);
}

static void
fcrypt_dtor(void *data)
{
	struct fcrypt *fcr = data;
	struct csession *cse;

	while ((cse = TAILQ_FIRST(&fcr->csessions))) {
		TAILQ_REMOVE(&fcr->csessions, cse, next);
		KASSERT(refcount_load(&cse->refs) == 1,
		    ("%s: crypto session %p with %d refs", __func__, cse,
		    refcount_load(&cse->refs)));
		cse_free(cse);
	}
	mtx_destroy(&fcr->lock);
	free(fcr, M_XDATA);
}

static int
crypto_open(struct cdev *dev, int oflags, int devtype, struct thread *td)
{
	struct fcrypt *fcr;
	int error;

	fcr = malloc(sizeof(struct fcrypt), M_XDATA, M_WAITOK | M_ZERO);
	TAILQ_INIT(&fcr->csessions);
	mtx_init(&fcr->lock, "fcrypt", NULL, MTX_DEF);
	error = devfs_set_cdevpriv(fcr, fcrypt_dtor);
	if (error)
		fcrypt_dtor(fcr);
	return (error);
}

static int
crypto_ioctl(struct cdev *dev, u_long cmd, caddr_t data, int flag,
    struct thread *td)
{
	struct fcrypt *fcr;
	struct csession *cse;
	struct session2_op *sop;
	struct crypt_op *cop;
	struct crypt_aead *caead;
	uint32_t ses;
	int error = 0;
	union {
		struct session2_op sopc;
#ifdef COMPAT_FREEBSD32
		struct crypt_op copc;
		struct crypt_aead aeadc;
#endif
	} thunk;
#ifdef COMPAT_FREEBSD32
	u_long cmd32;
	void *data32;

	cmd32 = 0;
	data32 = NULL;
	switch (cmd) {
	case CIOCGSESSION32:
		cmd32 = cmd;
		data32 = data;
		cmd = CIOCGSESSION;
		data = (void *)&thunk.sopc;
		session_op_from_32((struct session_op32 *)data32, &thunk.sopc);
		break;
	case CIOCGSESSION232:
		cmd32 = cmd;
		data32 = data;
		cmd = CIOCGSESSION2;
		data = (void *)&thunk.sopc;
		session2_op_from_32((struct session2_op32 *)data32,
		    &thunk.sopc);
		break;
	case CIOCCRYPT32:
		cmd32 = cmd;
		data32 = data;
		cmd = CIOCCRYPT;
		data = (void *)&thunk.copc;
		crypt_op_from_32((struct crypt_op32 *)data32, &thunk.copc);
		break;
	case CIOCCRYPTAEAD32:
		cmd32 = cmd;
		data32 = data;
		cmd = CIOCCRYPTAEAD;
		data = (void *)&thunk.aeadc;
		crypt_aead_from_32((struct crypt_aead32 *)data32, &thunk.aeadc);
		break;
	}
#endif

	devfs_get_cdevpriv((void **)&fcr);

	switch (cmd) {
#ifdef COMPAT_FREEBSD12
	case CRIOGET:
		/*
		 * NB: This may fail in cases that the old
		 * implementation did not if the current process has
		 * restricted filesystem access (e.g. running in a
		 * jail that does not expose /dev/crypto or in
		 * capability mode).
		 */
		error = kern_openat(td, AT_FDCWD, "/dev/crypto", UIO_SYSSPACE,
		    O_RDWR, 0);
		if (error == 0)
			*(uint32_t *)data = td->td_retval[0];
		break;
#endif
	case CIOCGSESSION:
	case CIOCGSESSION2:
		if (cmd == CIOCGSESSION) {
			session2_op_from_op((void *)data, &thunk.sopc);
			sop = &thunk.sopc;
		} else
			sop = (struct session2_op *)data;

		error = cse_create(fcr, sop);
		if (cmd == CIOCGSESSION && error == 0)
			session2_op_to_op(sop, (void *)data);
		break;
	case CIOCFSESSION:
		ses = *(uint32_t *)data;
		if (!cse_delete(fcr, ses)) {
			SDT_PROBE1(opencrypto, dev, ioctl, error, __LINE__);
			return (EINVAL);
		}
		break;
	case CIOCCRYPT:
		cop = (struct crypt_op *)data;
		cse = cse_find(fcr, cop->ses);
		if (cse == NULL) {
			SDT_PROBE1(opencrypto, dev, ioctl, error, __LINE__);
			return (EINVAL);
		}
		error = cryptodev_op(cse, cop);
		cse_free(cse);
		break;
	case CIOCFINDDEV:
		error = cryptodev_find((struct crypt_find_op *)data);
		break;
	case CIOCCRYPTAEAD:
		caead = (struct crypt_aead *)data;
		cse = cse_find(fcr, caead->ses);
		if (cse == NULL) {
			SDT_PROBE1(opencrypto, dev, ioctl, error, __LINE__);
			return (EINVAL);
		}
		error = cryptodev_aead(cse, caead);
		cse_free(cse);
		break;
	default:
		error = EINVAL;
		SDT_PROBE1(opencrypto, dev, ioctl, error, __LINE__);
		break;
	}

#ifdef COMPAT_FREEBSD32
	switch (cmd32) {
	case CIOCGSESSION32:
		if (error == 0)
			session_op_to_32((void *)data, data32);
		break;
	case CIOCGSESSION232:
		if (error == 0)
			session2_op_to_32((void *)data, data32);
		break;
	case CIOCCRYPT32:
		if (error == 0)
			crypt_op_to_32((void *)data, data32);
		break;
	case CIOCCRYPTAEAD32:
		if (error == 0)
			crypt_aead_to_32((void *)data, data32);
		break;
	}
#endif
	return (error);
}

static struct cdevsw crypto_cdevsw = {
	.d_version =	D_VERSION,
	.d_open =	crypto_open,
	.d_ioctl =	crypto_ioctl,
	.d_name =	"crypto",
};
static struct cdev *crypto_dev;

/*
 * Initialization code, both for static and dynamic loading.
 */
static int
cryptodev_modevent(module_t mod, int type, void *unused)
{
	switch (type) {
	case MOD_LOAD:
		if (bootverbose)
			printf("crypto: <crypto device>\n");
		crypto_dev = make_dev(&crypto_cdevsw, 0, 
				      UID_ROOT, GID_WHEEL, 0666,
				      "crypto");
		return 0;
	case MOD_UNLOAD:
		/*XXX disallow if active sessions */
		destroy_dev(crypto_dev);
		return 0;
	}
	return EINVAL;
}

static moduledata_t cryptodev_mod = {
	"cryptodev",
	cryptodev_modevent,
	0
};
MODULE_VERSION(cryptodev, 1);
DECLARE_MODULE(cryptodev, cryptodev_mod, SI_SUB_PSEUDO, SI_ORDER_ANY);
MODULE_DEPEND(cryptodev, crypto, 1, 1, 1);
MODULE_DEPEND(cryptodev, zlib, 1, 1, 1);
