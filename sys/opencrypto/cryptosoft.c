/*	$OpenBSD: cryptosoft.c,v 1.35 2002/04/26 08:43:50 deraadt Exp $	*/

/*-
 * The author of this code is Angelos D. Keromytis (angelos@cis.upenn.edu)
 * Copyright (c) 2002-2006 Sam Leffler, Errno Consulting
 *
 * This code was written by Angelos D. Keromytis in Athens, Greece, in
 * February 2000. Network Security Technologies Inc. (NSTI) kindly
 * supported the development of this code.
 *
 * Copyright (c) 2000, 2001 Angelos D. Keromytis
 * Copyright (c) 2014 The FreeBSD Foundation
 * All rights reserved.
 *
 * Portions of this software were developed by John-Mark Gurney
 * under sponsorship of the FreeBSD Foundation and
 * Rubicon Communications, LLC (Netgate).
 *
 * Permission to use, copy, and modify this software with or without fee
 * is hereby granted, provided that this entire notice is included in
 * all source code copies of any software which is or includes a copy or
 * modification of this software.
 *
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTY. IN PARTICULAR, NONE OF THE AUTHORS MAKES ANY
 * REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE
 * MERCHANTABILITY OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR
 * PURPOSE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/sysctl.h>
#include <sys/errno.h>
#include <sys/random.h>
#include <sys/kernel.h>
#include <sys/uio.h>
#include <sys/lock.h>
#include <sys/rwlock.h>
#include <sys/endian.h>
#include <sys/limits.h>
#include <sys/mutex.h>

#include <crypto/blowfish/blowfish.h>
#include <crypto/sha1.h>
#include <opencrypto/rmd160.h>
#include <opencrypto/skipjack.h>
#include <sys/md5.h>

#include <opencrypto/cryptodev.h>
#include <opencrypto/xform.h>

#include <sys/kobj.h>
#include <sys/bus.h>
#include "cryptodev_if.h"

struct swcr_auth {
	void		*sw_ictx;
	void		*sw_octx;
	struct auth_hash *sw_axf;
	uint16_t	sw_mlen;
	uint16_t	sw_octx_len;
};

struct swcr_encdec {
	uint8_t		*sw_kschedule;
	struct enc_xform *sw_exf;
};

struct swcr_compdec {
	struct comp_algo *sw_cxf;
};

struct swcr_session {
	struct mtx	swcr_lock;
	int	(*swcr_process)(struct swcr_session *, struct cryptop *);

	struct swcr_auth swcr_auth;
	struct swcr_encdec swcr_encdec;
	struct swcr_compdec swcr_compdec;
};

static	int32_t swcr_id;

static	void swcr_freesession(device_t dev, crypto_session_t cses);

/* Used for CRYPTO_NULL_CBC. */
static int
swcr_null(struct swcr_session *ses, struct cryptop *crp)
{

	return (0);
}

/*
 * Apply a symmetric encryption/decryption algorithm.
 */
static int
swcr_encdec(struct swcr_session *ses, struct cryptop *crp)
{
	unsigned char iv[EALG_MAX_BLOCK_LEN], blk[EALG_MAX_BLOCK_LEN];
	unsigned char *ivp, *nivp, iv2[EALG_MAX_BLOCK_LEN];
	const struct crypto_session_params *csp;
	struct swcr_encdec *sw;
	struct enc_xform *exf;
	int i, j, k, blks, ind, count, ivlen;
	struct uio *uio, uiolcl;
	struct iovec iovlcl[4];
	struct iovec *iov;
	int iovcnt, iovalloc;
	int error;
	bool encrypting;

	error = 0;

	sw = &ses->swcr_encdec;
	exf = sw->sw_exf;
	blks = exf->blocksize;
	ivlen = exf->ivsize;

	/* Check for non-padded data */
	if ((crp->crp_payload_length % blks) != 0)
		return EINVAL;

	if (exf == &enc_xform_aes_icm &&
	    (crp->crp_flags & CRYPTO_F_IV_SEPARATE) == 0)
		return (EINVAL);

	crypto_read_iv(crp, iv);

	if (crp->crp_cipher_key != NULL) {
		if (sw->sw_kschedule)
			exf->zerokey(&(sw->sw_kschedule));

		csp = crypto_get_params(crp->crp_session);
		error = exf->setkey(&sw->sw_kschedule,
		    crp->crp_cipher_key, csp->csp_cipher_klen);
		if (error)
			return (error);
	}

	iov = iovlcl;
	iovcnt = nitems(iovlcl);
	iovalloc = 0;
	uio = &uiolcl;
	switch (crp->crp_buf_type) {
	case CRYPTO_BUF_MBUF:
		error = crypto_mbuftoiov(crp->crp_mbuf, &iov, &iovcnt,
		    &iovalloc);
		if (error)
			return (error);
		uio->uio_iov = iov;
		uio->uio_iovcnt = iovcnt;
		break;
	case CRYPTO_BUF_UIO:
		uio = crp->crp_uio;
		break;
	case CRYPTO_BUF_CONTIG:
		iov[0].iov_base = crp->crp_buf;
		iov[0].iov_len = crp->crp_ilen;
		uio->uio_iov = iov;
		uio->uio_iovcnt = 1;
		break;
	}

	ivp = iv;

	if (exf->reinit) {
		/*
		 * xforms that provide a reinit method perform all IV
		 * handling themselves.
		 */
		exf->reinit(sw->sw_kschedule, iv);
	}

	count = crp->crp_payload_start;
	ind = cuio_getptr(uio, count, &k);
	if (ind == -1) {
		error = EINVAL;
		goto out;
	}

	i = crp->crp_payload_length;
	encrypting = CRYPTO_OP_IS_ENCRYPT(crp->crp_op);

	while (i > 0) {
		/*
		 * If there's insufficient data at the end of
		 * an iovec, we have to do some copying.
		 */
		if (uio->uio_iov[ind].iov_len < k + blks &&
		    uio->uio_iov[ind].iov_len != k) {
			cuio_copydata(uio, count, blks, blk);

			/* Actual encryption/decryption */
			if (exf->reinit) {
				if (encrypting) {
					exf->encrypt(sw->sw_kschedule,
					    blk);
				} else {
					exf->decrypt(sw->sw_kschedule,
					    blk);
				}
			} else if (encrypting) {
				/* XOR with previous block */
				for (j = 0; j < blks; j++)
					blk[j] ^= ivp[j];

				exf->encrypt(sw->sw_kschedule, blk);

				/*
				 * Keep encrypted block for XOR'ing
				 * with next block
				 */
				bcopy(blk, iv, blks);
				ivp = iv;
			} else {	/* decrypt */
				/*	
				 * Keep encrypted block for XOR'ing
				 * with next block
				 */
				nivp = (ivp == iv) ? iv2 : iv;
				bcopy(blk, nivp, blks);

				exf->decrypt(sw->sw_kschedule, blk);

				/* XOR with previous block */
				for (j = 0; j < blks; j++)
					blk[j] ^= ivp[j];

				ivp = nivp;
			}

			/* Copy back decrypted block */
			cuio_copyback(uio, count, blks, blk);

			count += blks;

			/* Advance pointer */
			ind = cuio_getptr(uio, count, &k);
			if (ind == -1) {
				error = EINVAL;
				goto out;
			}

			i -= blks;

			/* Could be done... */
			if (i == 0)
				break;
		}

		while (uio->uio_iov[ind].iov_len >= k + blks && i > 0) {
			uint8_t *idat;
			size_t nb, rem;

			nb = blks;
			rem = MIN((size_t)i,
			    uio->uio_iov[ind].iov_len - (size_t)k);
			idat = (uint8_t *)uio->uio_iov[ind].iov_base + k;

			if (exf->reinit) {
				if (encrypting && exf->encrypt_multi == NULL)
					exf->encrypt(sw->sw_kschedule,
					    idat);
				else if (encrypting) {
					nb = rounddown(rem, blks);
					exf->encrypt_multi(sw->sw_kschedule,
					    idat, nb);
				} else if (exf->decrypt_multi == NULL)
					exf->decrypt(sw->sw_kschedule,
					    idat);
				else {
					nb = rounddown(rem, blks);
					exf->decrypt_multi(sw->sw_kschedule,
					    idat, nb);
				}
			} else if (encrypting) {
				/* XOR with previous block/IV */
				for (j = 0; j < blks; j++)
					idat[j] ^= ivp[j];

				exf->encrypt(sw->sw_kschedule, idat);
				ivp = idat;
			} else {	/* decrypt */
				/*
				 * Keep encrypted block to be used
				 * in next block's processing.
				 */
				nivp = (ivp == iv) ? iv2 : iv;
				bcopy(idat, nivp, blks);

				exf->decrypt(sw->sw_kschedule, idat);

				/* XOR with previous block/IV */
				for (j = 0; j < blks; j++)
					idat[j] ^= ivp[j];

				ivp = nivp;
			}

			count += nb;
			k += nb;
			i -= nb;
		}

		/*
		 * Advance to the next iov if the end of the current iov
		 * is aligned with the end of a cipher block.
		 * Note that the code is equivalent to calling:
		 *      ind = cuio_getptr(uio, count, &k);
		 */
		if (i > 0 && k == uio->uio_iov[ind].iov_len) {
			k = 0;
			ind++;
			if (ind >= uio->uio_iovcnt) {
				error = EINVAL;
				goto out;
			}
		}
	}

out:
	if (iovalloc)
		free(iov, M_CRYPTO_DATA);

	return (error);
}

static void
swcr_authprepare(struct auth_hash *axf, struct swcr_auth *sw,
    const uint8_t *key, int klen)
{

	switch (axf->type) {
	case CRYPTO_MD5_HMAC:
	case CRYPTO_SHA1_HMAC:
	case CRYPTO_SHA2_224_HMAC:
	case CRYPTO_SHA2_256_HMAC:
	case CRYPTO_SHA2_384_HMAC:
	case CRYPTO_SHA2_512_HMAC:
	case CRYPTO_NULL_HMAC:
	case CRYPTO_RIPEMD160_HMAC:
		hmac_init_ipad(axf, key, klen, sw->sw_ictx);
		hmac_init_opad(axf, key, klen, sw->sw_octx);
		break;
	case CRYPTO_MD5_KPDK:
	case CRYPTO_SHA1_KPDK:
	{
		/* 
		 * We need a buffer that can hold an md5 and a sha1 result
		 * just to throw it away.
		 * What we do here is the initial part of:
		 *   ALGO( key, keyfill, .. )
		 * adding the key to sw_ictx and abusing Final() to get the
		 * "keyfill" padding.
		 * In addition we abuse the sw_octx to save the key to have
		 * it to be able to append it at the end in swcr_authcompute().
		 */
		u_char buf[SHA1_RESULTLEN];

		bcopy(key, sw->sw_octx, klen);
		axf->Init(sw->sw_ictx);
		axf->Update(sw->sw_ictx, key, klen);
		axf->Final(buf, sw->sw_ictx);
		break;
	}
	case CRYPTO_POLY1305:
	case CRYPTO_BLAKE2B:
	case CRYPTO_BLAKE2S:
		axf->Setkey(sw->sw_ictx, key, klen);
		axf->Init(sw->sw_ictx);
		break;
	default:
		panic("%s: algorithm %d doesn't use keys", __func__, axf->type);
	}
}

/*
 * Compute or verify hash.
 */
static int
swcr_authcompute(struct swcr_session *ses, struct cryptop *crp)
{
	u_char aalg[HASH_MAX_LEN];
	u_char uaalg[HASH_MAX_LEN];
	const struct crypto_session_params *csp;
	struct swcr_auth *sw;
	struct auth_hash *axf;
	union authctx ctx;
	int err;

	sw = &ses->swcr_auth;

	axf = sw->sw_axf;

	if (crp->crp_auth_key != NULL) {
		csp = crypto_get_params(crp->crp_session);
		swcr_authprepare(axf, sw, crp->crp_auth_key,
		    csp->csp_auth_klen);
	}

	bcopy(sw->sw_ictx, &ctx, axf->ctxsize);

	err = crypto_apply(crp, crp->crp_aad_start, crp->crp_aad_length,
	    (int (*)(void *, void *, unsigned int))axf->Update, &ctx);
	if (err)
		return err;

	err = crypto_apply(crp, crp->crp_payload_start, crp->crp_payload_length,
	    (int (*)(void *, void *, unsigned int))axf->Update, &ctx);
	if (err)
		return err;

	switch (axf->type) {
	case CRYPTO_SHA1:
	case CRYPTO_SHA2_224:
	case CRYPTO_SHA2_256:
	case CRYPTO_SHA2_384:
	case CRYPTO_SHA2_512:
		axf->Final(aalg, &ctx);
		break;

	case CRYPTO_MD5_HMAC:
	case CRYPTO_SHA1_HMAC:
	case CRYPTO_SHA2_224_HMAC:
	case CRYPTO_SHA2_256_HMAC:
	case CRYPTO_SHA2_384_HMAC:
	case CRYPTO_SHA2_512_HMAC:
	case CRYPTO_RIPEMD160_HMAC:
		if (sw->sw_octx == NULL)
			return EINVAL;

		axf->Final(aalg, &ctx);
		bcopy(sw->sw_octx, &ctx, axf->ctxsize);
		axf->Update(&ctx, aalg, axf->hashsize);
		axf->Final(aalg, &ctx);
		break;

	case CRYPTO_MD5_KPDK:
	case CRYPTO_SHA1_KPDK:
		/* If we have no key saved, return error. */
		if (sw->sw_octx == NULL)
			return EINVAL;

		/*
		 * Add the trailing copy of the key (see comment in
		 * swcr_authprepare()) after the data:
		 *   ALGO( .., key, algofill )
		 * and let Final() do the proper, natural "algofill"
		 * padding.
		 */
		axf->Update(&ctx, sw->sw_octx, sw->sw_octx_len);
		axf->Final(aalg, &ctx);
		break;

	case CRYPTO_BLAKE2B:
	case CRYPTO_BLAKE2S:
	case CRYPTO_NULL_HMAC:
	case CRYPTO_POLY1305:
		axf->Final(aalg, &ctx);
		break;
	}

	if (crp->crp_op & CRYPTO_OP_VERIFY_DIGEST) {
		crypto_copydata(crp, crp->crp_digest_start, sw->sw_mlen, uaalg);
		if (timingsafe_bcmp(aalg, uaalg, sw->sw_mlen) != 0)
			return (EBADMSG);
	} else {
		/* Inject the authentication data */
		crypto_copyback(crp, crp->crp_digest_start, sw->sw_mlen, aalg);
	}
	return (0);
}

CTASSERT(INT_MAX <= (1ll<<39) - 256);	/* GCM: plain text < 2^39-256 */
CTASSERT(INT_MAX <= (uint64_t)-1);	/* GCM: associated data <= 2^64-1 */

static int
swcr_gmac(struct swcr_session *ses, struct cryptop *crp)
{
	uint32_t blkbuf[howmany(EALG_MAX_BLOCK_LEN, sizeof(uint32_t))];
	u_char *blk = (u_char *)blkbuf;
	u_char aalg[AALG_MAX_RESULT_LEN];
	u_char uaalg[AALG_MAX_RESULT_LEN];
	u_char iv[EALG_MAX_BLOCK_LEN];
	union authctx ctx;
	struct swcr_auth *swa;
	struct auth_hash *axf;
	uint32_t *blkp;
	int blksz, i, ivlen, len;

	swa = &ses->swcr_auth;
	axf = swa->sw_axf;

	bcopy(swa->sw_ictx, &ctx, axf->ctxsize);
	blksz = axf->blocksize;

	/* Initialize the IV */
	ivlen = AES_GCM_IV_LEN;
	crypto_read_iv(crp, iv);

	axf->Reinit(&ctx, iv, ivlen);
	for (i = 0; i < crp->crp_payload_length; i += blksz) {
		len = MIN(crp->crp_payload_length - i, blksz);
		crypto_copydata(crp, crp->crp_payload_start + i, len, blk);
		bzero(blk + len, blksz - len);
		axf->Update(&ctx, blk, blksz);
	}

	/* length block */
	bzero(blk, blksz);
	blkp = (uint32_t *)blk + 1;
	*blkp = htobe32(crp->crp_payload_length * 8);
	axf->Update(&ctx, blk, blksz);

	/* Finalize MAC */
	axf->Final(aalg, &ctx);

	if (crp->crp_op & CRYPTO_OP_VERIFY_DIGEST) {
		crypto_copydata(crp, crp->crp_digest_start, swa->sw_mlen,
		    uaalg);
		if (timingsafe_bcmp(aalg, uaalg, swa->sw_mlen) != 0)
			return (EBADMSG);
	} else {
		/* Inject the authentication data */
		crypto_copyback(crp, crp->crp_digest_start, swa->sw_mlen, aalg);
	}
	return (0);
}

static int
swcr_gcm(struct swcr_session *ses, struct cryptop *crp)
{
	uint32_t blkbuf[howmany(EALG_MAX_BLOCK_LEN, sizeof(uint32_t))];
	u_char *blk = (u_char *)blkbuf;
	u_char aalg[AALG_MAX_RESULT_LEN];
	u_char uaalg[AALG_MAX_RESULT_LEN];
	u_char iv[EALG_MAX_BLOCK_LEN];
	union authctx ctx;
	struct swcr_auth *swa;
	struct swcr_encdec *swe;
	struct auth_hash *axf;
	struct enc_xform *exf;
	uint32_t *blkp;
	int blksz, i, ivlen, len, r;

	swa = &ses->swcr_auth;
	axf = swa->sw_axf;

	bcopy(swa->sw_ictx, &ctx, axf->ctxsize);
	blksz = axf->blocksize;
	
	swe = &ses->swcr_encdec;
	exf = swe->sw_exf;

	if ((crp->crp_flags & CRYPTO_F_IV_SEPARATE) == 0)
		return (EINVAL);

	/* Initialize the IV */
	ivlen = AES_GCM_IV_LEN;
	bcopy(crp->crp_iv, iv, ivlen);

	/* Supply MAC with IV */
	axf->Reinit(&ctx, iv, ivlen);

	/* Supply MAC with AAD */
	for (i = 0; i < crp->crp_aad_length; i += blksz) {
		len = MIN(crp->crp_aad_length - i, blksz);
		crypto_copydata(crp, crp->crp_aad_start + i, len, blk);
		bzero(blk + len, blksz - len);
		axf->Update(&ctx, blk, blksz);
	}

	exf->reinit(swe->sw_kschedule, iv);

	/* Do encryption with MAC */
	for (i = 0; i < crp->crp_payload_length; i += len) {
		len = MIN(crp->crp_payload_length - i, blksz);
		if (len < blksz)
			bzero(blk, blksz);
		crypto_copydata(crp, crp->crp_payload_start + i, len, blk);
		if (CRYPTO_OP_IS_ENCRYPT(crp->crp_op)) {
			exf->encrypt(swe->sw_kschedule, blk);
			axf->Update(&ctx, blk, len);
			crypto_copyback(crp, crp->crp_payload_start + i, len,
			    blk);
		} else {
			axf->Update(&ctx, blk, len);
		}
	}

	/* length block */
	bzero(blk, blksz);
	blkp = (uint32_t *)blk + 1;
	*blkp = htobe32(crp->crp_aad_length * 8);
	blkp = (uint32_t *)blk + 3;
	*blkp = htobe32(crp->crp_payload_length * 8);
	axf->Update(&ctx, blk, blksz);

	/* Finalize MAC */
	axf->Final(aalg, &ctx);

	/* Validate tag */
	if (!CRYPTO_OP_IS_ENCRYPT(crp->crp_op)) {
		crypto_copydata(crp, crp->crp_digest_start, swa->sw_mlen,
		    uaalg);

		r = timingsafe_bcmp(aalg, uaalg, swa->sw_mlen);
		if (r != 0)
			return (EBADMSG);
		
		/* tag matches, decrypt data */
		for (i = 0; i < crp->crp_payload_length; i += blksz) {
			len = MIN(crp->crp_payload_length - i, blksz);
			if (len < blksz)
				bzero(blk, blksz);
			crypto_copydata(crp, crp->crp_payload_start + i, len,
			    blk);
			exf->decrypt(swe->sw_kschedule, blk);
			crypto_copyback(crp, crp->crp_payload_start + i, len,
			    blk);
		}
	} else {
		/* Inject the authentication data */
		crypto_copyback(crp, crp->crp_digest_start, swa->sw_mlen,
		    aalg);
	}

	return (0);
}

static int
swcr_ccm_cbc_mac(struct swcr_session *ses, struct cryptop *crp)
{
	uint32_t blkbuf[howmany(EALG_MAX_BLOCK_LEN, sizeof(uint32_t))];
	u_char *blk = (u_char *)blkbuf;
	u_char aalg[AALG_MAX_RESULT_LEN];
	u_char uaalg[AALG_MAX_RESULT_LEN];
	u_char iv[EALG_MAX_BLOCK_LEN];
	union authctx ctx;
	struct swcr_auth *swa;
	struct auth_hash *axf;
	int blksz, i, ivlen, len;

	swa = &ses->swcr_auth;
	axf = swa->sw_axf;

	bcopy(swa->sw_ictx, &ctx, axf->ctxsize);
	blksz = axf->blocksize;

	/* Initialize the IV */
	ivlen = AES_CCM_IV_LEN;
	crypto_read_iv(crp, iv);

	/*
	 * AES CCM-CBC-MAC needs to know the length of both the auth
	 * data and payload data before doing the auth computation.
	 */
	ctx.aes_cbc_mac_ctx.authDataLength = crp->crp_payload_length;
	ctx.aes_cbc_mac_ctx.cryptDataLength = 0;

	axf->Reinit(&ctx, iv, ivlen);
	for (i = 0; i < crp->crp_payload_length; i += blksz) {
		len = MIN(crp->crp_payload_length - i, blksz);
		crypto_copydata(crp, crp->crp_payload_start + i, len, blk);
		bzero(blk + len, blksz - len);
		axf->Update(&ctx, blk, blksz);
	}

	/* Finalize MAC */
	axf->Final(aalg, &ctx);

	if (crp->crp_op & CRYPTO_OP_VERIFY_DIGEST) {
		crypto_copydata(crp, crp->crp_digest_start, swa->sw_mlen,
		    uaalg);
		if (timingsafe_bcmp(aalg, uaalg, swa->sw_mlen) != 0)
			return (EBADMSG);
	} else {
		/* Inject the authentication data */
		crypto_copyback(crp, crp->crp_digest_start, swa->sw_mlen, aalg);
	}
	return (0);
}

static int
swcr_ccm(struct swcr_session *ses, struct cryptop *crp)
{
	uint32_t blkbuf[howmany(EALG_MAX_BLOCK_LEN, sizeof(uint32_t))];
	u_char *blk = (u_char *)blkbuf;
	u_char aalg[AALG_MAX_RESULT_LEN];
	u_char uaalg[AALG_MAX_RESULT_LEN];
	u_char iv[EALG_MAX_BLOCK_LEN];
	union authctx ctx;
	struct swcr_auth *swa;
	struct swcr_encdec *swe;
	struct auth_hash *axf;
	struct enc_xform *exf;
	int blksz, i, ivlen, len, r;

	swa = &ses->swcr_auth;
	axf = swa->sw_axf;

	bcopy(swa->sw_ictx, &ctx, axf->ctxsize);
	blksz = axf->blocksize;
	
	swe = &ses->swcr_encdec;
	exf = swe->sw_exf;

	if ((crp->crp_flags & CRYPTO_F_IV_SEPARATE) == 0)
		return (EINVAL);

	/* Initialize the IV */
	ivlen = AES_CCM_IV_LEN;
	bcopy(crp->crp_iv, iv, ivlen);

	/*
	 * AES CCM-CBC-MAC needs to know the length of both the auth
	 * data and payload data before doing the auth computation.
	 */
	ctx.aes_cbc_mac_ctx.authDataLength = crp->crp_aad_length;
	ctx.aes_cbc_mac_ctx.cryptDataLength = crp->crp_payload_length;

	/* Supply MAC with IV */
	axf->Reinit(&ctx, iv, ivlen);

	/* Supply MAC with AAD */
	for (i = 0; i < crp->crp_aad_length; i += blksz) {
		len = MIN(crp->crp_aad_length - i, blksz);
		crypto_copydata(crp, crp->crp_aad_start + i, len, blk);
		bzero(blk + len, blksz - len);
		axf->Update(&ctx, blk, blksz);
	}

	exf->reinit(swe->sw_kschedule, iv);

	/* Do encryption/decryption with MAC */
	for (i = 0; i < crp->crp_payload_length; i += len) {
		len = MIN(crp->crp_payload_length - i, blksz);
		if (len < blksz)
			bzero(blk, blksz);
		crypto_copydata(crp, crp->crp_payload_start + i, len, blk);
		if (CRYPTO_OP_IS_ENCRYPT(crp->crp_op)) {
			axf->Update(&ctx, blk, len);
			exf->encrypt(swe->sw_kschedule, blk);
			crypto_copyback(crp, crp->crp_payload_start + i, len,
			    blk);
		} else {
			/*
			 * One of the problems with CCM+CBC is that
			 * the authentication is done on the
			 * unecncrypted data.  As a result, we have to
			 * decrypt the data twice: once to generate
			 * the tag and a second time after the tag is
			 * verified.
			 */
			exf->decrypt(swe->sw_kschedule, blk);
			axf->Update(&ctx, blk, len);
		}
	}

	/* Finalize MAC */
	axf->Final(aalg, &ctx);

	/* Validate tag */
	if (!CRYPTO_OP_IS_ENCRYPT(crp->crp_op)) {
		crypto_copydata(crp, crp->crp_digest_start, swa->sw_mlen,
		    uaalg);

		r = timingsafe_bcmp(aalg, uaalg, swa->sw_mlen);
		if (r != 0)
			return (EBADMSG);
		
		/* tag matches, decrypt data */
		exf->reinit(swe->sw_kschedule, iv);
		for (i = 0; i < crp->crp_payload_length; i += blksz) {
			len = MIN(crp->crp_payload_length - i, blksz);
			if (len < blksz)
				bzero(blk, blksz);
			crypto_copydata(crp, crp->crp_payload_start + i, len,
			    blk);
			exf->decrypt(swe->sw_kschedule, blk);
			crypto_copyback(crp, crp->crp_payload_start + i, len,
			    blk);
		}
	} else {
		/* Inject the authentication data */
		crypto_copyback(crp, crp->crp_digest_start, swa->sw_mlen,
		    aalg);
	}

	return (0);
}

/*
 * Apply a cipher and a digest to perform EtA.
 */
static int
swcr_eta(struct swcr_session *ses, struct cryptop *crp)
{
	int error;

	if (CRYPTO_OP_IS_ENCRYPT(crp->crp_op)) {
		error = swcr_encdec(ses, crp);
		if (error == 0)
			error = swcr_authcompute(ses, crp);
	} else {
		error = swcr_authcompute(ses, crp);
		if (error == 0)
			error = swcr_encdec(ses, crp);
	}
	return (error);
}

/*
 * Apply a compression/decompression algorithm
 */
static int
swcr_compdec(struct swcr_session *ses, struct cryptop *crp)
{
	u_int8_t *data, *out;
	struct comp_algo *cxf;
	int adj;
	u_int32_t result;

	cxf = ses->swcr_compdec.sw_cxf;

	/* We must handle the whole buffer of data in one time
	 * then if there is not all the data in the mbuf, we must
	 * copy in a buffer.
	 */

	data = malloc(crp->crp_payload_length, M_CRYPTO_DATA,  M_NOWAIT);
	if (data == NULL)
		return (EINVAL);
	crypto_copydata(crp, crp->crp_payload_start, crp->crp_payload_length,
	    data);

	if (CRYPTO_OP_IS_COMPRESS(crp->crp_op))
		result = cxf->compress(data, crp->crp_payload_length, &out);
	else
		result = cxf->decompress(data, crp->crp_payload_length, &out);

	free(data, M_CRYPTO_DATA);
	if (result == 0)
		return (EINVAL);
	crp->crp_olen = result;

	/* Check the compressed size when doing compression */
	if (CRYPTO_OP_IS_COMPRESS(crp->crp_op)) {
		if (result >= crp->crp_payload_length) {
			/* Compression was useless, we lost time */
			free(out, M_CRYPTO_DATA);
			return (0);
		}
	}

	/* Copy back the (de)compressed data. m_copyback is
	 * extending the mbuf as necessary.
	 */
	crypto_copyback(crp, crp->crp_payload_start, result, out);
	if (result < crp->crp_payload_length) {
		switch (crp->crp_buf_type) {
		case CRYPTO_BUF_MBUF:
			adj = result - crp->crp_payload_length;
			m_adj(crp->crp_mbuf, adj);
			break;
		case CRYPTO_BUF_UIO: {
			struct uio *uio = crp->crp_uio;
			int ind;

			adj = crp->crp_payload_length - result;
			ind = uio->uio_iovcnt - 1;

			while (adj > 0 && ind >= 0) {
				if (adj < uio->uio_iov[ind].iov_len) {
					uio->uio_iov[ind].iov_len -= adj;
					break;
				}

				adj -= uio->uio_iov[ind].iov_len;
				uio->uio_iov[ind].iov_len = 0;
				ind--;
				uio->uio_iovcnt--;
			}
			}
			break;
		}
	}
	free(out, M_CRYPTO_DATA);
	return 0;
}

static int
swcr_setup_encdec(struct swcr_session *ses,
    const struct crypto_session_params *csp)
{
	struct swcr_encdec *swe;
	struct enc_xform *txf;
	int error;

	swe = &ses->swcr_encdec;
	txf = crypto_cipher(csp);
	MPASS(txf->ivsize == csp->csp_ivlen);
	if (csp->csp_cipher_key != NULL) {
		error = txf->setkey(&swe->sw_kschedule,
		    csp->csp_cipher_key, csp->csp_cipher_klen);
		if (error)
			return (error);
	}
	swe->sw_exf = txf;
	return (0);
}

static int
swcr_setup_auth(struct swcr_session *ses,
    const struct crypto_session_params *csp)
{
	struct swcr_auth *swa;
	struct auth_hash *axf;

	swa = &ses->swcr_auth;

	axf = crypto_auth_hash(csp);
	swa->sw_axf = axf;
	if (csp->csp_auth_mlen < 0 || csp->csp_auth_mlen > axf->hashsize)
		return (EINVAL);
	if (csp->csp_auth_mlen == 0)
		swa->sw_mlen = axf->hashsize;
	else
		swa->sw_mlen = csp->csp_auth_mlen;
	swa->sw_ictx = malloc(axf->ctxsize, M_CRYPTO_DATA, M_NOWAIT);
	if (swa->sw_ictx == NULL)
		return (ENOBUFS);
	
	switch (csp->csp_auth_alg) {
	case CRYPTO_MD5_HMAC:
	case CRYPTO_SHA1_HMAC:
	case CRYPTO_SHA2_224_HMAC:
	case CRYPTO_SHA2_256_HMAC:
	case CRYPTO_SHA2_384_HMAC:
	case CRYPTO_SHA2_512_HMAC:
	case CRYPTO_NULL_HMAC:
	case CRYPTO_RIPEMD160_HMAC:
		swa->sw_octx_len = axf->ctxsize;
		swa->sw_octx = malloc(swa->sw_octx_len, M_CRYPTO_DATA,
		    M_NOWAIT);
		if (swa->sw_octx == NULL)
			return (ENOBUFS);

		if (csp->csp_auth_key != NULL) {
			swcr_authprepare(axf, swa, csp->csp_auth_key,
			    csp->csp_auth_klen);
		}

		if (csp->csp_mode == CSP_MODE_DIGEST)
			ses->swcr_process = swcr_authcompute;
		break;
	case CRYPTO_MD5_KPDK:
	case CRYPTO_SHA1_KPDK:
		swa->sw_octx_len = csp->csp_auth_klen;
		swa->sw_octx = malloc(swa->sw_octx_len, M_CRYPTO_DATA,
		    M_NOWAIT);
		if (swa->sw_octx == NULL)
			return (ENOBUFS);

		/* Store the key so we can "append" it to the payload */
		if (csp->csp_auth_key != NULL) {
			swcr_authprepare(axf, swa, csp->csp_auth_key,
			    csp->csp_auth_klen);
		}

		if (csp->csp_mode == CSP_MODE_DIGEST)
			ses->swcr_process = swcr_authcompute;
		break;
	case CRYPTO_SHA1:
	case CRYPTO_SHA2_224:
	case CRYPTO_SHA2_256:
	case CRYPTO_SHA2_384:
	case CRYPTO_SHA2_512:
		axf->Init(swa->sw_ictx);
		if (csp->csp_mode == CSP_MODE_DIGEST)
			ses->swcr_process = swcr_authcompute;
		break;
	case CRYPTO_AES_NIST_GMAC:
		axf->Init(swa->sw_ictx);
		axf->Setkey(swa->sw_ictx, csp->csp_auth_key,
		    csp->csp_auth_klen);
		if (csp->csp_mode == CSP_MODE_DIGEST)
			ses->swcr_process = swcr_gmac;
		break;
	case CRYPTO_POLY1305:
	case CRYPTO_BLAKE2B:
	case CRYPTO_BLAKE2S:
		/*
		 * Blake2b and Blake2s support an optional key but do
		 * not require one.
		 */
		if (csp->csp_auth_klen == 0 || csp->csp_auth_key != NULL)
			axf->Setkey(swa->sw_ictx, csp->csp_auth_key,
			    csp->csp_auth_klen);
		axf->Init(swa->sw_ictx);
		if (csp->csp_mode == CSP_MODE_DIGEST)
			ses->swcr_process = swcr_authcompute;
		break;
	case CRYPTO_AES_CCM_CBC_MAC:
		axf->Init(swa->sw_ictx);
		axf->Setkey(swa->sw_ictx, csp->csp_auth_key,
		    csp->csp_auth_klen);
		if (csp->csp_mode == CSP_MODE_DIGEST)
			ses->swcr_process = swcr_ccm_cbc_mac;
		break;
	}

	return (0);
}

static int
swcr_setup_gcm(struct swcr_session *ses,
    const struct crypto_session_params *csp)
{
	struct swcr_encdec *swe;
	struct swcr_auth *swa;
	struct enc_xform *txf;
	struct auth_hash *axf;
	int error;

	if (csp->csp_ivlen != AES_GCM_IV_LEN)
		return (EINVAL);

	/* First, setup the auth side. */
	swa = &ses->swcr_auth;
	switch (csp->csp_cipher_klen * 8) {
	case 128:
		axf = &auth_hash_nist_gmac_aes_128;
		break;
	case 192:
		axf = &auth_hash_nist_gmac_aes_192;
		break;
	case 256:
		axf = &auth_hash_nist_gmac_aes_256;
		break;
	default:
		return (EINVAL);
	}
	swa->sw_axf = axf;
	if (csp->csp_auth_mlen < 0 || csp->csp_auth_mlen > axf->hashsize)
		return (EINVAL);
	if (csp->csp_auth_mlen == 0)
		swa->sw_mlen = axf->hashsize;
	else
		swa->sw_mlen = csp->csp_auth_mlen;
	swa->sw_ictx = malloc(axf->ctxsize, M_CRYPTO_DATA, M_NOWAIT);
	if (swa->sw_ictx == NULL)
		return (ENOBUFS);
	axf->Init(swa->sw_ictx);
	if (csp->csp_cipher_key != NULL)
		axf->Setkey(swa->sw_ictx, csp->csp_cipher_key,
		    csp->csp_cipher_klen);

	/* Second, setup the cipher side. */
	swe = &ses->swcr_encdec;
	txf = &enc_xform_aes_nist_gcm;
	if (csp->csp_cipher_key != NULL) {
		error = txf->setkey(&swe->sw_kschedule,
		    csp->csp_cipher_key, csp->csp_cipher_klen);
		if (error)
			return (error);
	}
	swe->sw_exf = txf;

	return (0);
}

static int
swcr_setup_ccm(struct swcr_session *ses,
    const struct crypto_session_params *csp)
{
	struct swcr_encdec *swe;
	struct swcr_auth *swa;
	struct enc_xform *txf;
	struct auth_hash *axf;
	int error;

	if (csp->csp_ivlen != AES_CCM_IV_LEN)
		return (EINVAL);

	/* First, setup the auth side. */
	swa = &ses->swcr_auth;
	switch (csp->csp_cipher_klen * 8) {
	case 128:
		axf = &auth_hash_ccm_cbc_mac_128;
		break;
	case 192:
		axf = &auth_hash_ccm_cbc_mac_192;
		break;
	case 256:
		axf = &auth_hash_ccm_cbc_mac_256;
		break;
	default:
		return (EINVAL);
	}
	swa->sw_axf = axf;
	if (csp->csp_auth_mlen < 0 || csp->csp_auth_mlen > axf->hashsize)
		return (EINVAL);
	if (csp->csp_auth_mlen == 0)
		swa->sw_mlen = axf->hashsize;
	else
		swa->sw_mlen = csp->csp_auth_mlen;
	swa->sw_ictx = malloc(axf->ctxsize, M_CRYPTO_DATA, M_NOWAIT);
	if (swa->sw_ictx == NULL)
		return (ENOBUFS);
	axf->Init(swa->sw_ictx);
	if (csp->csp_cipher_key != NULL)
		axf->Setkey(swa->sw_ictx, csp->csp_cipher_key,
		    csp->csp_cipher_klen);

	/* Second, setup the cipher side. */
	swe = &ses->swcr_encdec;
	txf = &enc_xform_ccm;
	if (csp->csp_cipher_key != NULL) {
		error = txf->setkey(&swe->sw_kschedule,
		    csp->csp_cipher_key, csp->csp_cipher_klen);
		if (error)
			return (error);
	}
	swe->sw_exf = txf;

	return (0);
}

static bool
swcr_auth_supported(const struct crypto_session_params *csp)
{
	struct auth_hash *axf;

	axf = crypto_auth_hash(csp);
	if (axf == NULL)
		return (false);
	switch (csp->csp_auth_alg) {
	case CRYPTO_MD5_HMAC:
	case CRYPTO_SHA1_HMAC:
	case CRYPTO_SHA2_224_HMAC:
	case CRYPTO_SHA2_256_HMAC:
	case CRYPTO_SHA2_384_HMAC:
	case CRYPTO_SHA2_512_HMAC:
	case CRYPTO_NULL_HMAC:
	case CRYPTO_RIPEMD160_HMAC:
	case CRYPTO_MD5_KPDK:
	case CRYPTO_SHA1_KPDK:
		break;
	case CRYPTO_AES_NIST_GMAC:
		switch (csp->csp_auth_klen * 8) {
		case 128:
		case 192:
		case 256:
			break;
		default:
			return (false);
		}
		if (csp->csp_auth_key == NULL)
			return (false);
		if (csp->csp_ivlen != AES_GCM_IV_LEN)
			return (false);
		break;
	case CRYPTO_POLY1305:
		if (csp->csp_auth_klen != POLY1305_KEY_LEN)
			return (false);
		break;
	case CRYPTO_AES_CCM_CBC_MAC:
		switch (csp->csp_auth_klen * 8) {
		case 128:
		case 192:
		case 256:
			break;
		default:
			return (false);
		}
		if (csp->csp_auth_key == NULL)
			return (false);
		if (csp->csp_ivlen != AES_CCM_IV_LEN)
			return (false);
		break;
	}
	return (true);
}

static bool
swcr_cipher_supported(const struct crypto_session_params *csp)
{
	struct enc_xform *txf;

	txf = crypto_cipher(csp);
	if (txf == NULL)
		return (false);
	if (csp->csp_cipher_alg != CRYPTO_NULL_CBC &&
	    txf->ivsize != csp->csp_ivlen)
		return (false);
	return (true);
}

static int
swcr_probesession(device_t dev, const struct crypto_session_params *csp)
{

	if (csp->csp_flags != 0)
		return (EINVAL);
	switch (csp->csp_mode) {
	case CSP_MODE_COMPRESS:
		switch (csp->csp_cipher_alg) {
		case CRYPTO_DEFLATE_COMP:
			break;
		default:
			return (EINVAL);
		}
		break;
	case CSP_MODE_CIPHER:
		switch (csp->csp_cipher_alg) {
		case CRYPTO_AES_NIST_GCM_16:
		case CRYPTO_AES_CCM_16:
			return (EINVAL);
		default:
			if (!swcr_cipher_supported(csp))
				return (EINVAL);
			break;
		}
		break;
	case CSP_MODE_DIGEST:
		if (!swcr_auth_supported(csp))
			return (EINVAL);
		break;
	case CSP_MODE_AEAD:
		switch (csp->csp_cipher_alg) {
		case CRYPTO_AES_NIST_GCM_16:
		case CRYPTO_AES_CCM_16:
			break;
		default:
			return (EINVAL);
		}
		break;
	case CSP_MODE_ETA:
		/* AEAD algorithms cannot be used for EtA. */
		switch (csp->csp_cipher_alg) {
		case CRYPTO_AES_NIST_GCM_16:
		case CRYPTO_AES_CCM_16:
			return (EINVAL);
		}
		switch (csp->csp_auth_alg) {
		case CRYPTO_AES_NIST_GMAC:
		case CRYPTO_AES_CCM_CBC_MAC:
			return (EINVAL);
		}

		if (!swcr_cipher_supported(csp) ||
		    !swcr_auth_supported(csp))
			return (EINVAL);
		break;
	default:
		return (EINVAL);
	}

	return (CRYPTODEV_PROBE_SOFTWARE);
}

/*
 * Generate a new software session.
 */
static int
swcr_newsession(device_t dev, crypto_session_t cses,
    const struct crypto_session_params *csp)
{
	struct swcr_session *ses;
	struct swcr_encdec *swe;
	struct swcr_auth *swa;
	struct comp_algo *cxf;
	int error;

	ses = crypto_get_driver_session(cses);
	mtx_init(&ses->swcr_lock, "swcr session lock", NULL, MTX_DEF);

	error = 0;
	swe = &ses->swcr_encdec;
	swa = &ses->swcr_auth;
	switch (csp->csp_mode) {
	case CSP_MODE_COMPRESS:
		switch (csp->csp_cipher_alg) {
		case CRYPTO_DEFLATE_COMP:
			cxf = &comp_algo_deflate;
			break;
#ifdef INVARIANTS
		default:
			panic("bad compression algo");
#endif
		}
		ses->swcr_compdec.sw_cxf = cxf;
		ses->swcr_process = swcr_compdec;
		break;
	case CSP_MODE_CIPHER:
		switch (csp->csp_cipher_alg) {
		case CRYPTO_NULL_CBC:
			ses->swcr_process = swcr_null;
			break;
#ifdef INVARIANTS
		case CRYPTO_AES_NIST_GCM_16:
		case CRYPTO_AES_CCM_16:
			panic("bad cipher algo");
#endif
		default:
			error = swcr_setup_encdec(ses, csp);
			if (error == 0)
				ses->swcr_process = swcr_encdec;
		}
		break;
	case CSP_MODE_DIGEST:
		error = swcr_setup_auth(ses, csp);
		break;
	case CSP_MODE_AEAD:
		switch (csp->csp_cipher_alg) {
		case CRYPTO_AES_NIST_GCM_16:
			error = swcr_setup_gcm(ses, csp);
			if (error == 0)
				ses->swcr_process = swcr_gcm;
			break;
		case CRYPTO_AES_CCM_16:
			error = swcr_setup_ccm(ses, csp);
			if (error == 0)
				ses->swcr_process = swcr_ccm;
			break;
#ifdef INVARIANTS
		default:
			panic("bad aead algo");
#endif
		}
		break;
	case CSP_MODE_ETA:
#ifdef INVARIANTS
		switch (csp->csp_cipher_alg) {
		case CRYPTO_AES_NIST_GCM_16:
		case CRYPTO_AES_CCM_16:
			panic("bad eta cipher algo");
		}
		switch (csp->csp_auth_alg) {
		case CRYPTO_AES_NIST_GMAC:
		case CRYPTO_AES_CCM_CBC_MAC:
			panic("bad eta auth algo");
		}
#endif

		error = swcr_setup_auth(ses, csp);
		if (error)
			break;
		if (csp->csp_cipher_alg == CRYPTO_NULL_CBC) {
			/* Effectively degrade to digest mode. */
			ses->swcr_process = swcr_authcompute;
			break;
		}

		error = swcr_setup_encdec(ses, csp);
		if (error == 0)
			ses->swcr_process = swcr_eta;
		break;
	default:
		error = EINVAL;
	}

	if (error)
		swcr_freesession(dev, cses);
	return (error);
}

static void
swcr_freesession(device_t dev, crypto_session_t cses)
{
	struct swcr_session *ses;
	struct swcr_auth *swa;
	struct enc_xform *txf;
	struct auth_hash *axf;

	ses = crypto_get_driver_session(cses);

	mtx_destroy(&ses->swcr_lock);

	txf = ses->swcr_encdec.sw_exf;
	if (txf != NULL) {
		if (ses->swcr_encdec.sw_kschedule != NULL)
			txf->zerokey(&(ses->swcr_encdec.sw_kschedule));
	}

	axf = ses->swcr_auth.sw_axf;
	if (axf != NULL) {
		swa = &ses->swcr_auth;
		if (swa->sw_ictx != NULL) {
			explicit_bzero(swa->sw_ictx, axf->ctxsize);
			free(swa->sw_ictx, M_CRYPTO_DATA);
		}
		if (swa->sw_octx != NULL) {
			explicit_bzero(swa->sw_octx, swa->sw_octx_len);
			free(swa->sw_octx, M_CRYPTO_DATA);
		}
	}
}

/*
 * Process a software request.
 */
static int
swcr_process(device_t dev, struct cryptop *crp, int hint)
{
	struct swcr_session *ses;

	ses = crypto_get_driver_session(crp->crp_session);
	mtx_lock(&ses->swcr_lock);

	crp->crp_etype = ses->swcr_process(ses, crp);

	mtx_unlock(&ses->swcr_lock);
	crypto_done(crp);
	return (0);
}

static void
swcr_identify(driver_t *drv, device_t parent)
{
	/* NB: order 10 is so we get attached after h/w devices */
	if (device_find_child(parent, "cryptosoft", -1) == NULL &&
	    BUS_ADD_CHILD(parent, 10, "cryptosoft", 0) == 0)
		panic("cryptosoft: could not attach");
}

static int
swcr_probe(device_t dev)
{
	device_set_desc(dev, "software crypto");
	return (BUS_PROBE_NOWILDCARD);
}

static int
swcr_attach(device_t dev)
{

	swcr_id = crypto_get_driverid(dev, sizeof(struct swcr_session),
			CRYPTOCAP_F_SOFTWARE | CRYPTOCAP_F_SYNC);
	if (swcr_id < 0) {
		device_printf(dev, "cannot initialize!");
		return (ENXIO);
	}

	return (0);
}

static int
swcr_detach(device_t dev)
{
	crypto_unregister_all(swcr_id);
	return 0;
}

static device_method_t swcr_methods[] = {
	DEVMETHOD(device_identify,	swcr_identify),
	DEVMETHOD(device_probe,		swcr_probe),
	DEVMETHOD(device_attach,	swcr_attach),
	DEVMETHOD(device_detach,	swcr_detach),

	DEVMETHOD(cryptodev_probesession, swcr_probesession),
	DEVMETHOD(cryptodev_newsession,	swcr_newsession),
	DEVMETHOD(cryptodev_freesession,swcr_freesession),
	DEVMETHOD(cryptodev_process,	swcr_process),

	{0, 0},
};

static driver_t swcr_driver = {
	"cryptosoft",
	swcr_methods,
	0,		/* NB: no softc */
};
static devclass_t swcr_devclass;

/*
 * NB: We explicitly reference the crypto module so we
 * get the necessary ordering when built as a loadable
 * module.  This is required because we bundle the crypto
 * module code together with the cryptosoft driver (otherwise
 * normal module dependencies would handle things).
 */
extern int crypto_modevent(struct module *, int, void *);
/* XXX where to attach */
DRIVER_MODULE(cryptosoft, nexus, swcr_driver, swcr_devclass, crypto_modevent,0);
MODULE_VERSION(cryptosoft, 1);
MODULE_DEPEND(cryptosoft, crypto, 1, 1, 1);
