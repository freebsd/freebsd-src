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

#include <crypto/sha1.h>
#include <opencrypto/rmd160.h>

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
};

struct swcr_encdec {
	void		*sw_kschedule;
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
	int i, blks, inlen, ivlen, outlen, resid;
	struct crypto_buffer_cursor cc_in, cc_out;
	const unsigned char *inblk;
	unsigned char *outblk;
	int error;
	bool encrypting;

	error = 0;

	sw = &ses->swcr_encdec;
	exf = sw->sw_exf;
	ivlen = exf->ivsize;

	if (exf->native_blocksize == 0) {
		/* Check for non-padded data */
		if ((crp->crp_payload_length % exf->blocksize) != 0)
			return (EINVAL);

		blks = exf->blocksize;
	} else
		blks = exf->native_blocksize;

	if (exf == &enc_xform_aes_icm &&
	    (crp->crp_flags & CRYPTO_F_IV_SEPARATE) == 0)
		return (EINVAL);

	if (crp->crp_cipher_key != NULL) {
		csp = crypto_get_params(crp->crp_session);
		error = exf->setkey(sw->sw_kschedule,
		    crp->crp_cipher_key, csp->csp_cipher_klen);
		if (error)
			return (error);
	}

	crypto_read_iv(crp, iv);

	if (exf->reinit) {
		/*
		 * xforms that provide a reinit method perform all IV
		 * handling themselves.
		 */
		exf->reinit(sw->sw_kschedule, iv);
	}

	ivp = iv;

	crypto_cursor_init(&cc_in, &crp->crp_buf);
	crypto_cursor_advance(&cc_in, crp->crp_payload_start);
	inlen = crypto_cursor_seglen(&cc_in);
	inblk = crypto_cursor_segbase(&cc_in);
	if (CRYPTO_HAS_OUTPUT_BUFFER(crp)) {
		crypto_cursor_init(&cc_out, &crp->crp_obuf);
		crypto_cursor_advance(&cc_out, crp->crp_payload_output_start);
	} else
		cc_out = cc_in;
	outlen = crypto_cursor_seglen(&cc_out);
	outblk = crypto_cursor_segbase(&cc_out);

	resid = crp->crp_payload_length;
	encrypting = CRYPTO_OP_IS_ENCRYPT(crp->crp_op);

	/*
	 * Loop through encrypting blocks.  'inlen' is the remaining
	 * length of the current segment in the input buffer.
	 * 'outlen' is the remaining length of current segment in the
	 * output buffer.
	 */
	while (resid >= blks) {
		/*
		 * If the current block is not contained within the
		 * current input/output segment, use 'blk' as a local
		 * buffer.
		 */
		if (inlen < blks) {
			crypto_cursor_copydata(&cc_in, blks, blk);
			inblk = blk;
		}
		if (outlen < blks)
			outblk = blk;

		/*
		 * Ciphers without a 'reinit' hook are assumed to be
		 * used in CBC mode where the chaining is done here.
		 */
		if (exf->reinit != NULL) {
			if (encrypting)
				exf->encrypt(sw->sw_kschedule, inblk, outblk);
			else
				exf->decrypt(sw->sw_kschedule, inblk, outblk);
		} else if (encrypting) {
			/* XOR with previous block */
			for (i = 0; i < blks; i++)
				outblk[i] = inblk[i] ^ ivp[i];

			exf->encrypt(sw->sw_kschedule, outblk, outblk);

			/*
			 * Keep encrypted block for XOR'ing
			 * with next block
			 */
			memcpy(iv, outblk, blks);
			ivp = iv;
		} else {	/* decrypt */
			/*
			 * Keep encrypted block for XOR'ing
			 * with next block
			 */
			nivp = (ivp == iv) ? iv2 : iv;
			memcpy(nivp, inblk, blks);

			exf->decrypt(sw->sw_kschedule, inblk, outblk);

			/* XOR with previous block */
			for (i = 0; i < blks; i++)
				outblk[i] ^= ivp[i];

			ivp = nivp;
		}

		if (inlen < blks) {
			inlen = crypto_cursor_seglen(&cc_in);
			inblk = crypto_cursor_segbase(&cc_in);
		} else {
			crypto_cursor_advance(&cc_in, blks);
			inlen -= blks;
			inblk += blks;
		}

		if (outlen < blks) {
			crypto_cursor_copyback(&cc_out, blks, blk);
			outlen = crypto_cursor_seglen(&cc_out);
			outblk = crypto_cursor_segbase(&cc_out);
		} else {
			crypto_cursor_advance(&cc_out, blks);
			outlen -= blks;
			outblk += blks;
		}

		resid -= blks;
	}

	/* Handle trailing partial block for stream ciphers. */
	if (resid > 0) {
		KASSERT(exf->native_blocksize != 0,
		    ("%s: partial block of %d bytes for cipher %s",
		    __func__, i, exf->name));
		KASSERT(exf->reinit != NULL,
		    ("%s: partial block cipher %s without reinit hook",
		    __func__, exf->name));
		KASSERT(resid < blks, ("%s: partial block too big", __func__));

		inlen = crypto_cursor_seglen(&cc_in);
		outlen = crypto_cursor_seglen(&cc_out);
		if (inlen < resid) {
			crypto_cursor_copydata(&cc_in, resid, blk);
			inblk = blk;
		} else
			inblk = crypto_cursor_segbase(&cc_in);
		if (outlen < resid)
			outblk = blk;
		else
			outblk = crypto_cursor_segbase(&cc_out);
		if (encrypting)
			exf->encrypt_last(sw->sw_kschedule, inblk, outblk,
			    resid);
		else
			exf->decrypt_last(sw->sw_kschedule, inblk, outblk,
			    resid);
		if (outlen < resid)
			crypto_cursor_copyback(&cc_out, resid, blk);
	}

	explicit_bzero(blk, sizeof(blk));
	explicit_bzero(iv, sizeof(iv));
	explicit_bzero(iv2, sizeof(iv2));
	return (0);
}

static void
swcr_authprepare(struct auth_hash *axf, struct swcr_auth *sw,
    const uint8_t *key, int klen)
{

	switch (axf->type) {
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

	if (crp->crp_aad != NULL)
		err = axf->Update(&ctx, crp->crp_aad, crp->crp_aad_length);
	else
		err = crypto_apply(crp, crp->crp_aad_start, crp->crp_aad_length,
		    axf->Update, &ctx);
	if (err)
		return err;

	if (CRYPTO_HAS_OUTPUT_BUFFER(crp) &&
	    CRYPTO_OP_IS_ENCRYPT(crp->crp_op))
		err = crypto_apply_buf(&crp->crp_obuf,
		    crp->crp_payload_output_start, crp->crp_payload_length,
		    axf->Update, &ctx);
	else
		err = crypto_apply(crp, crp->crp_payload_start,
		    crp->crp_payload_length, axf->Update, &ctx);
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

	case CRYPTO_BLAKE2B:
	case CRYPTO_BLAKE2S:
	case CRYPTO_NULL_HMAC:
	case CRYPTO_POLY1305:
		axf->Final(aalg, &ctx);
		break;
	}

	if (crp->crp_op & CRYPTO_OP_VERIFY_DIGEST) {
		u_char uaalg[HASH_MAX_LEN];

		crypto_copydata(crp, crp->crp_digest_start, sw->sw_mlen, uaalg);
		if (timingsafe_bcmp(aalg, uaalg, sw->sw_mlen) != 0)
			err = EBADMSG;
		explicit_bzero(uaalg, sizeof(uaalg));
	} else {
		/* Inject the authentication data */
		crypto_copyback(crp, crp->crp_digest_start, sw->sw_mlen, aalg);
	}
	explicit_bzero(aalg, sizeof(aalg));
	return (err);
}

CTASSERT(INT_MAX <= (1ll<<39) - 256);	/* GCM: plain text < 2^39-256 */
CTASSERT(INT_MAX <= (uint64_t)-1);	/* GCM: associated data <= 2^64-1 */

static int
swcr_gmac(struct swcr_session *ses, struct cryptop *crp)
{
	uint32_t blkbuf[howmany(AES_BLOCK_LEN, sizeof(uint32_t))];
	u_char *blk = (u_char *)blkbuf;
	u_char tag[GMAC_DIGEST_LEN];
	u_char iv[AES_BLOCK_LEN];
	struct crypto_buffer_cursor cc;
	const u_char *inblk;
	union authctx ctx;
	struct swcr_auth *swa;
	struct auth_hash *axf;
	uint32_t *blkp;
	int blksz, error, ivlen, len, resid;

	swa = &ses->swcr_auth;
	axf = swa->sw_axf;

	bcopy(swa->sw_ictx, &ctx, axf->ctxsize);
	blksz = GMAC_BLOCK_LEN;
	KASSERT(axf->blocksize == blksz, ("%s: axf block size mismatch",
	    __func__));

	/* Initialize the IV */
	ivlen = AES_GCM_IV_LEN;
	crypto_read_iv(crp, iv);

	axf->Reinit(&ctx, iv, ivlen);
	crypto_cursor_init(&cc, &crp->crp_buf);
	crypto_cursor_advance(&cc, crp->crp_payload_start);
	for (resid = crp->crp_payload_length; resid >= blksz; resid -= len) {
		len = crypto_cursor_seglen(&cc);
		if (len >= blksz) {
			inblk = crypto_cursor_segbase(&cc);
			len = rounddown(MIN(len, resid), blksz);
			crypto_cursor_advance(&cc, len);
		} else {
			len = blksz;
			crypto_cursor_copydata(&cc, len, blk);
			inblk = blk;
		}
		axf->Update(&ctx, inblk, len);
	}
	if (resid > 0) {
		memset(blk, 0, blksz);
		crypto_cursor_copydata(&cc, resid, blk);
		axf->Update(&ctx, blk, blksz);
	}

	/* length block */
	memset(blk, 0, blksz);
	blkp = (uint32_t *)blk + 1;
	*blkp = htobe32(crp->crp_payload_length * 8);
	axf->Update(&ctx, blk, blksz);

	/* Finalize MAC */
	axf->Final(tag, &ctx);

	error = 0;
	if (crp->crp_op & CRYPTO_OP_VERIFY_DIGEST) {
		u_char tag2[GMAC_DIGEST_LEN];

		crypto_copydata(crp, crp->crp_digest_start, swa->sw_mlen,
		    tag2);
		if (timingsafe_bcmp(tag, tag2, swa->sw_mlen) != 0)
			error = EBADMSG;
		explicit_bzero(tag2, sizeof(tag2));
	} else {
		/* Inject the authentication data */
		crypto_copyback(crp, crp->crp_digest_start, swa->sw_mlen, tag);
	}
	explicit_bzero(blkbuf, sizeof(blkbuf));
	explicit_bzero(tag, sizeof(tag));
	explicit_bzero(iv, sizeof(iv));
	return (error);
}

static int
swcr_gcm(struct swcr_session *ses, struct cryptop *crp)
{
	uint32_t blkbuf[howmany(AES_BLOCK_LEN, sizeof(uint32_t))];
	u_char *blk = (u_char *)blkbuf;
	u_char tag[GMAC_DIGEST_LEN];
	u_char iv[AES_BLOCK_LEN];
	struct crypto_buffer_cursor cc_in, cc_out;
	const u_char *inblk;
	u_char *outblk;
	union authctx ctx;
	struct swcr_auth *swa;
	struct swcr_encdec *swe;
	struct auth_hash *axf;
	struct enc_xform *exf;
	uint32_t *blkp;
	int blksz, error, ivlen, len, r, resid;

	swa = &ses->swcr_auth;
	axf = swa->sw_axf;

	bcopy(swa->sw_ictx, &ctx, axf->ctxsize);
	blksz = GMAC_BLOCK_LEN;
	KASSERT(axf->blocksize == blksz, ("%s: axf block size mismatch",
	    __func__));

	swe = &ses->swcr_encdec;
	exf = swe->sw_exf;
	KASSERT(axf->blocksize == exf->native_blocksize,
	    ("%s: blocksize mismatch", __func__));

	if ((crp->crp_flags & CRYPTO_F_IV_SEPARATE) == 0)
		return (EINVAL);

	/* Initialize the IV */
	ivlen = AES_GCM_IV_LEN;
	bcopy(crp->crp_iv, iv, ivlen);

	/* Supply MAC with IV */
	axf->Reinit(&ctx, iv, ivlen);

	/* Supply MAC with AAD */
	if (crp->crp_aad != NULL) {
		len = rounddown(crp->crp_aad_length, blksz);
		if (len != 0)
			axf->Update(&ctx, crp->crp_aad, len);
		if (crp->crp_aad_length != len) {
			memset(blk, 0, blksz);
			memcpy(blk, (char *)crp->crp_aad + len,
			    crp->crp_aad_length - len);
			axf->Update(&ctx, blk, blksz);
		}
	} else {
		crypto_cursor_init(&cc_in, &crp->crp_buf);
		crypto_cursor_advance(&cc_in, crp->crp_aad_start);
		for (resid = crp->crp_aad_length; resid >= blksz;
		     resid -= len) {
			len = crypto_cursor_seglen(&cc_in);
			if (len >= blksz) {
				inblk = crypto_cursor_segbase(&cc_in);
				len = rounddown(MIN(len, resid), blksz);
				crypto_cursor_advance(&cc_in, len);
			} else {
				len = blksz;
				crypto_cursor_copydata(&cc_in, len, blk);
				inblk = blk;
			}
			axf->Update(&ctx, inblk, len);
		}
		if (resid > 0) {
			memset(blk, 0, blksz);
			crypto_cursor_copydata(&cc_in, resid, blk);
			axf->Update(&ctx, blk, blksz);
		}
	}

	exf->reinit(swe->sw_kschedule, iv);

	/* Do encryption with MAC */
	crypto_cursor_init(&cc_in, &crp->crp_buf);
	crypto_cursor_advance(&cc_in, crp->crp_payload_start);
	if (CRYPTO_HAS_OUTPUT_BUFFER(crp)) {
		crypto_cursor_init(&cc_out, &crp->crp_obuf);
		crypto_cursor_advance(&cc_out, crp->crp_payload_output_start);
	} else
		cc_out = cc_in;
	for (resid = crp->crp_payload_length; resid >= blksz; resid -= blksz) {
		if (crypto_cursor_seglen(&cc_in) < blksz) {
			crypto_cursor_copydata(&cc_in, blksz, blk);
			inblk = blk;
		} else {
			inblk = crypto_cursor_segbase(&cc_in);
			crypto_cursor_advance(&cc_in, blksz);
		}
		if (CRYPTO_OP_IS_ENCRYPT(crp->crp_op)) {
			if (crypto_cursor_seglen(&cc_out) < blksz)
				outblk = blk;
			else
				outblk = crypto_cursor_segbase(&cc_out);
			exf->encrypt(swe->sw_kschedule, inblk, outblk);
			axf->Update(&ctx, outblk, blksz);
			if (outblk == blk)
				crypto_cursor_copyback(&cc_out, blksz, blk);
			else
				crypto_cursor_advance(&cc_out, blksz);
		} else {
			axf->Update(&ctx, inblk, blksz);
		}
	}
	if (resid > 0) {
		crypto_cursor_copydata(&cc_in, resid, blk);
		if (CRYPTO_OP_IS_ENCRYPT(crp->crp_op)) {
			exf->encrypt_last(swe->sw_kschedule, blk, blk, resid);
			crypto_cursor_copyback(&cc_out, resid, blk);
		}
		axf->Update(&ctx, blk, resid);
	}

	/* length block */
	memset(blk, 0, blksz);
	blkp = (uint32_t *)blk + 1;
	*blkp = htobe32(crp->crp_aad_length * 8);
	blkp = (uint32_t *)blk + 3;
	*blkp = htobe32(crp->crp_payload_length * 8);
	axf->Update(&ctx, blk, blksz);

	/* Finalize MAC */
	axf->Final(tag, &ctx);

	/* Validate tag */
	error = 0;
	if (!CRYPTO_OP_IS_ENCRYPT(crp->crp_op)) {
		u_char tag2[GMAC_DIGEST_LEN];

		crypto_copydata(crp, crp->crp_digest_start, swa->sw_mlen, tag2);

		r = timingsafe_bcmp(tag, tag2, swa->sw_mlen);
		explicit_bzero(tag2, sizeof(tag2));
		if (r != 0) {
			error = EBADMSG;
			goto out;
		}

		/* tag matches, decrypt data */
		crypto_cursor_init(&cc_in, &crp->crp_buf);
		crypto_cursor_advance(&cc_in, crp->crp_payload_start);
		for (resid = crp->crp_payload_length; resid > blksz;
		     resid -= blksz) {
			if (crypto_cursor_seglen(&cc_in) < blksz) {
				crypto_cursor_copydata(&cc_in, blksz, blk);
				inblk = blk;
			} else {
				inblk = crypto_cursor_segbase(&cc_in);
				crypto_cursor_advance(&cc_in, blksz);
			}
			if (crypto_cursor_seglen(&cc_out) < blksz)
				outblk = blk;
			else
				outblk = crypto_cursor_segbase(&cc_out);
			exf->decrypt(swe->sw_kschedule, inblk, outblk);
			if (outblk == blk)
				crypto_cursor_copyback(&cc_out, blksz, blk);
			else
				crypto_cursor_advance(&cc_out, blksz);
		}
		if (resid > 0) {
			crypto_cursor_copydata(&cc_in, resid, blk);
			exf->decrypt_last(swe->sw_kschedule, blk, blk, resid);
			crypto_cursor_copyback(&cc_out, resid, blk);
		}
	} else {
		/* Inject the authentication data */
		crypto_copyback(crp, crp->crp_digest_start, swa->sw_mlen, tag);
	}

out:
	explicit_bzero(blkbuf, sizeof(blkbuf));
	explicit_bzero(tag, sizeof(tag));
	explicit_bzero(iv, sizeof(iv));

	return (error);
}

static int
swcr_ccm_cbc_mac(struct swcr_session *ses, struct cryptop *crp)
{
	u_char tag[AES_CBC_MAC_HASH_LEN];
	u_char iv[AES_BLOCK_LEN];
	union authctx ctx;
	struct swcr_auth *swa;
	struct auth_hash *axf;
	int error, ivlen;

	swa = &ses->swcr_auth;
	axf = swa->sw_axf;

	bcopy(swa->sw_ictx, &ctx, axf->ctxsize);

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
	if (crp->crp_aad != NULL)
		error = axf->Update(&ctx, crp->crp_aad, crp->crp_aad_length);
	else
		error = crypto_apply(crp, crp->crp_payload_start,
		    crp->crp_payload_length, axf->Update, &ctx);
	if (error)
		return (error);

	/* Finalize MAC */
	axf->Final(tag, &ctx);

	if (crp->crp_op & CRYPTO_OP_VERIFY_DIGEST) {
		u_char tag2[AES_CBC_MAC_HASH_LEN];

		crypto_copydata(crp, crp->crp_digest_start, swa->sw_mlen,
		    tag2);
		if (timingsafe_bcmp(tag, tag2, swa->sw_mlen) != 0)
			error = EBADMSG;
		explicit_bzero(tag2, sizeof(tag));
	} else {
		/* Inject the authentication data */
		crypto_copyback(crp, crp->crp_digest_start, swa->sw_mlen, tag);
	}
	explicit_bzero(tag, sizeof(tag));
	explicit_bzero(iv, sizeof(iv));
	return (error);
}

static int
swcr_ccm(struct swcr_session *ses, struct cryptop *crp)
{
	uint32_t blkbuf[howmany(AES_BLOCK_LEN, sizeof(uint32_t))];
	u_char *blk = (u_char *)blkbuf;
	u_char tag[AES_CBC_MAC_HASH_LEN];
	u_char iv[AES_BLOCK_LEN];
	struct crypto_buffer_cursor cc_in, cc_out;
	const u_char *inblk;
	u_char *outblk;
	union authctx ctx;
	struct swcr_auth *swa;
	struct swcr_encdec *swe;
	struct auth_hash *axf;
	struct enc_xform *exf;
	int blksz, error, ivlen, r, resid;

	swa = &ses->swcr_auth;
	axf = swa->sw_axf;

	bcopy(swa->sw_ictx, &ctx, axf->ctxsize);
	blksz = AES_BLOCK_LEN;
	KASSERT(axf->blocksize == blksz, ("%s: axf block size mismatch",
	    __func__));

	swe = &ses->swcr_encdec;
	exf = swe->sw_exf;
	KASSERT(axf->blocksize == exf->native_blocksize,
	    ("%s: blocksize mismatch", __func__));

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
	if (crp->crp_aad != NULL)
		error = axf->Update(&ctx, crp->crp_aad, crp->crp_aad_length);
	else
		error = crypto_apply(crp, crp->crp_aad_start,
		    crp->crp_aad_length, axf->Update, &ctx);
	if (error)
		return (error);

	exf->reinit(swe->sw_kschedule, iv);

	/* Do encryption/decryption with MAC */
	crypto_cursor_init(&cc_in, &crp->crp_buf);
	crypto_cursor_advance(&cc_in, crp->crp_payload_start);
	if (CRYPTO_HAS_OUTPUT_BUFFER(crp)) {
		crypto_cursor_init(&cc_out, &crp->crp_obuf);
		crypto_cursor_advance(&cc_out, crp->crp_payload_output_start);
	} else
		cc_out = cc_in;
	for (resid = crp->crp_payload_length; resid >= blksz; resid -= blksz) {
		if (crypto_cursor_seglen(&cc_in) < blksz) {
			crypto_cursor_copydata(&cc_in, blksz, blk);
			inblk = blk;
		} else {
			inblk = crypto_cursor_segbase(&cc_in);
			crypto_cursor_advance(&cc_in, blksz);
		}
		if (CRYPTO_OP_IS_ENCRYPT(crp->crp_op)) {
			if (crypto_cursor_seglen(&cc_out) < blksz)
				outblk = blk;
			else
				outblk = crypto_cursor_segbase(&cc_out);
			axf->Update(&ctx, inblk, blksz);
			exf->encrypt(swe->sw_kschedule, inblk, outblk);
			if (outblk == blk)
				crypto_cursor_copyback(&cc_out, blksz, blk);
			else
				crypto_cursor_advance(&cc_out, blksz);
		} else {
			/*
			 * One of the problems with CCM+CBC is that
			 * the authentication is done on the
			 * unencrypted data.  As a result, we have to
			 * decrypt the data twice: once to generate
			 * the tag and a second time after the tag is
			 * verified.
			 */
			exf->decrypt(swe->sw_kschedule, inblk, blk);
			axf->Update(&ctx, blk, blksz);
		}
	}
	if (resid > 0) {
		crypto_cursor_copydata(&cc_in, resid, blk);
		if (CRYPTO_OP_IS_ENCRYPT(crp->crp_op)) {
			axf->Update(&ctx, blk, resid);
			exf->encrypt_last(swe->sw_kschedule, blk, blk, resid);
			crypto_cursor_copyback(&cc_out, resid, blk);
		} else {
			exf->decrypt_last(swe->sw_kschedule, blk, blk, resid);
			axf->Update(&ctx, blk, resid);
		}
	}

	/* Finalize MAC */
	axf->Final(tag, &ctx);

	/* Validate tag */
	error = 0;
	if (!CRYPTO_OP_IS_ENCRYPT(crp->crp_op)) {
		u_char tag2[AES_CBC_MAC_HASH_LEN];

		crypto_copydata(crp, crp->crp_digest_start, swa->sw_mlen,
		    tag2);

		r = timingsafe_bcmp(tag, tag2, swa->sw_mlen);
		explicit_bzero(tag2, sizeof(tag2));
		if (r != 0) {
			error = EBADMSG;
			goto out;
		}

		/* tag matches, decrypt data */
		exf->reinit(swe->sw_kschedule, iv);
		crypto_cursor_init(&cc_in, &crp->crp_buf);
		crypto_cursor_advance(&cc_in, crp->crp_payload_start);
		for (resid = crp->crp_payload_length; resid > blksz;
		     resid -= blksz) {
			if (crypto_cursor_seglen(&cc_in) < blksz) {
				crypto_cursor_copydata(&cc_in, blksz, blk);
				inblk = blk;
			} else {
				inblk = crypto_cursor_segbase(&cc_in);
				crypto_cursor_advance(&cc_in, blksz);
			}
			if (crypto_cursor_seglen(&cc_out) < blksz)
				outblk = blk;
			else
				outblk = crypto_cursor_segbase(&cc_out);
			exf->decrypt(swe->sw_kschedule, inblk, outblk);
			if (outblk == blk)
				crypto_cursor_copyback(&cc_out, blksz, blk);
			else
				crypto_cursor_advance(&cc_out, blksz);
		}
		if (resid > 0) {
			crypto_cursor_copydata(&cc_in, resid, blk);
			exf->decrypt_last(swe->sw_kschedule, blk, blk, resid);
			crypto_cursor_copyback(&cc_out, resid, blk);
		}
	} else {
		/* Inject the authentication data */
		crypto_copyback(crp, crp->crp_digest_start, swa->sw_mlen, tag);
	}

out:
	explicit_bzero(blkbuf, sizeof(blkbuf));
	explicit_bzero(tag, sizeof(tag));
	explicit_bzero(iv, sizeof(iv));
	return (error);
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
		switch (crp->crp_buf.cb_type) {
		case CRYPTO_BUF_MBUF:
			adj = result - crp->crp_payload_length;
			m_adj(crp->crp_buf.cb_mbuf, adj);
			break;
		case CRYPTO_BUF_UIO: {
			struct uio *uio = crp->crp_buf.cb_uio;
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
		default:
			break;
		}
	}
	free(out, M_CRYPTO_DATA);
	return 0;
}

static int
swcr_setup_cipher(struct swcr_session *ses,
    const struct crypto_session_params *csp)
{
	struct swcr_encdec *swe;
	struct enc_xform *txf;
	int error;

	swe = &ses->swcr_encdec;
	txf = crypto_cipher(csp);
	MPASS(txf->ivsize == csp->csp_ivlen);
	if (txf->ctxsize != 0) {
		swe->sw_kschedule = malloc(txf->ctxsize, M_CRYPTO_DATA,
		    M_NOWAIT);
		if (swe->sw_kschedule == NULL)
			return (ENOMEM);
	}
	if (csp->csp_cipher_key != NULL) {
		error = txf->setkey(swe->sw_kschedule,
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
	case CRYPTO_SHA1_HMAC:
	case CRYPTO_SHA2_224_HMAC:
	case CRYPTO_SHA2_256_HMAC:
	case CRYPTO_SHA2_384_HMAC:
	case CRYPTO_SHA2_512_HMAC:
	case CRYPTO_NULL_HMAC:
	case CRYPTO_RIPEMD160_HMAC:
		swa->sw_octx = malloc(axf->ctxsize, M_CRYPTO_DATA,
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
	struct swcr_auth *swa;
	struct auth_hash *axf;

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
	return (swcr_setup_cipher(ses, csp));
}

static int
swcr_setup_ccm(struct swcr_session *ses,
    const struct crypto_session_params *csp)
{
	struct swcr_auth *swa;
	struct auth_hash *axf;

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
	return (swcr_setup_cipher(ses, csp));
}

static bool
swcr_auth_supported(const struct crypto_session_params *csp)
{
	struct auth_hash *axf;

	axf = crypto_auth_hash(csp);
	if (axf == NULL)
		return (false);
	switch (csp->csp_auth_alg) {
	case CRYPTO_SHA1_HMAC:
	case CRYPTO_SHA2_224_HMAC:
	case CRYPTO_SHA2_256_HMAC:
	case CRYPTO_SHA2_384_HMAC:
	case CRYPTO_SHA2_512_HMAC:
	case CRYPTO_NULL_HMAC:
	case CRYPTO_RIPEMD160_HMAC:
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

	if ((csp->csp_flags & ~(CSP_F_SEPARATE_OUTPUT | CSP_F_SEPARATE_AAD)) !=
	    0)
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
			error = swcr_setup_cipher(ses, csp);
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

		error = swcr_setup_cipher(ses, csp);
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
	struct auth_hash *axf;

	ses = crypto_get_driver_session(cses);

	mtx_destroy(&ses->swcr_lock);

	zfree(ses->swcr_encdec.sw_kschedule, M_CRYPTO_DATA);

	axf = ses->swcr_auth.sw_axf;
	if (axf != NULL) {
		swa = &ses->swcr_auth;
		if (swa->sw_ictx != NULL) {
			explicit_bzero(swa->sw_ictx, axf->ctxsize);
			free(swa->sw_ictx, M_CRYPTO_DATA);
		}
		if (swa->sw_octx != NULL) {
			explicit_bzero(swa->sw_octx, axf->ctxsize);
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
