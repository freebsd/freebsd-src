/*	$OpenBSD: xform.c,v 1.16 2001/08/28 12:20:43 ben Exp $	*/
/*-
 * The authors of this code are John Ioannidis (ji@tla.org),
 * Angelos D. Keromytis (kermit@csd.uch.gr),
 * Niels Provos (provos@physnet.uni-hamburg.de) and
 * Damien Miller (djm@mindrot.org).
 *
 * This code was written by John Ioannidis for BSD/OS in Athens, Greece,
 * in November 1995.
 *
 * Ported to OpenBSD and NetBSD, with additional transforms, in December 1996,
 * by Angelos D. Keromytis.
 *
 * Additional transforms and features in 1997 and 1998 by Angelos D. Keromytis
 * and Niels Provos.
 *
 * Additional features in 1999 by Angelos D. Keromytis.
 *
 * AES XTS implementation in 2008 by Damien Miller
 *
 * Copyright (C) 1995, 1996, 1997, 1998, 1999 by John Ioannidis,
 * Angelos D. Keromytis and Niels Provos.
 *
 * Copyright (C) 2001, Angelos D. Keromytis.
 *
 * Copyright (C) 2008, Damien Miller
 * Copyright (c) 2014 The FreeBSD Foundation
 * All rights reserved.
 *
 * Portions of this software were developed by John-Mark Gurney
 * under sponsorship of the FreeBSD Foundation and
 * Rubicon Communications, LLC (Netgate).
 *
 * Permission to use, copy, and modify this software with or without fee
 * is hereby granted, provided that this entire notice is included in
 * all copies of any software which is or includes a copy or
 * modification of this software.
 * You may use this code under the GNU public license if you so wish. Please
 * contribute changes back to the authors under this freer than GPL license
 * so that we may further the use of strong encryption without limitations to
 * all.
 *
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTY. IN PARTICULAR, NONE OF THE AUTHORS MAKES ANY
 * REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE
 * MERCHANTABILITY OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR
 * PURPOSE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <opencrypto/xform_enc.h>

static	int aes_icm_setkey(void *, const uint8_t *, int);
static	void aes_icm_crypt(void *, const uint8_t *, uint8_t *);
static	void aes_icm_crypt_last(void *, const uint8_t *, uint8_t *, size_t);
static	void aes_icm_reinit(void *, const uint8_t *, size_t);
static	void aes_gcm_reinit(void *, const uint8_t *, size_t);
static	void aes_ccm_reinit(void *, const uint8_t *, size_t);

/* Encryption instances */
const struct enc_xform enc_xform_aes_icm = {
	.type = CRYPTO_AES_ICM,
	.name = "AES-ICM",
	.ctxsize = sizeof(struct aes_icm_ctx),
	.blocksize = 1,
	.native_blocksize = AES_BLOCK_LEN,
	.ivsize = AES_BLOCK_LEN,
	.minkey = AES_MIN_KEY,
	.maxkey = AES_MAX_KEY,
	.encrypt = aes_icm_crypt,
	.decrypt = aes_icm_crypt,
	.setkey = aes_icm_setkey,
	.reinit = aes_icm_reinit,
	.encrypt_last = aes_icm_crypt_last,
	.decrypt_last = aes_icm_crypt_last,
};

const struct enc_xform enc_xform_aes_nist_gcm = {
	.type = CRYPTO_AES_NIST_GCM_16,
	.name = "AES-GCM",
	.ctxsize = sizeof(struct aes_icm_ctx),
	.blocksize = 1,
	.native_blocksize = AES_BLOCK_LEN,
	.ivsize = AES_GCM_IV_LEN,
	.minkey = AES_MIN_KEY,
	.maxkey = AES_MAX_KEY,
	.encrypt = aes_icm_crypt,
	.decrypt = aes_icm_crypt,
	.setkey = aes_icm_setkey,
	.reinit = aes_gcm_reinit,
	.encrypt_last = aes_icm_crypt_last,
	.decrypt_last = aes_icm_crypt_last,
};

const struct enc_xform enc_xform_ccm = {
	.type = CRYPTO_AES_CCM_16,
	.name = "AES-CCM",
	.ctxsize = sizeof(struct aes_icm_ctx),
	.blocksize = 1,
	.native_blocksize = AES_BLOCK_LEN,
	.ivsize = AES_CCM_IV_LEN,
	.minkey = AES_MIN_KEY, .maxkey = AES_MAX_KEY,
	.encrypt = aes_icm_crypt,
	.decrypt = aes_icm_crypt,
	.setkey = aes_icm_setkey,
	.reinit = aes_ccm_reinit,
	.encrypt_last = aes_icm_crypt_last,
	.decrypt_last = aes_icm_crypt_last,
};

/*
 * Encryption wrapper routines.
 */
static void
aes_icm_reinit(void *key, const uint8_t *iv, size_t ivlen)
{
	struct aes_icm_ctx *ctx;

	ctx = key;
	KASSERT(ivlen <= sizeof(ctx->ac_block),
	    ("%s: ivlen too large", __func__));
	bcopy(iv, ctx->ac_block, ivlen);
}

static void
aes_gcm_reinit(void *key, const uint8_t *iv, size_t ivlen)
{
	struct aes_icm_ctx *ctx;

	KASSERT(ivlen == AES_GCM_IV_LEN,
	    ("%s: invalid IV length", __func__));
	aes_icm_reinit(key, iv, ivlen);

	ctx = key;
	/* GCM starts with 2 as counter 1 is used for final xor of tag. */
	bzero(&ctx->ac_block[AESICM_BLOCKSIZE - 4], 4);
	ctx->ac_block[AESICM_BLOCKSIZE - 1] = 2;
}

static void
aes_ccm_reinit(void *key, const uint8_t *iv, size_t ivlen)
{
	struct aes_icm_ctx *ctx;

	KASSERT(ivlen >= 7 && ivlen <= 13,
	    ("%s: invalid IV length", __func__));
	ctx = key;

	/* CCM has flags, then the IV, then the counter, which starts at 1 */
	bzero(ctx->ac_block, sizeof(ctx->ac_block));
	ctx->ac_block[0] = (15 - ivlen) - 1;
	bcopy(iv, ctx->ac_block + 1, ivlen);
	ctx->ac_block[AESICM_BLOCKSIZE - 1] = 1;
}

static void
aes_icm_crypt(void *key, const uint8_t *in, uint8_t *out)
{
	struct aes_icm_ctx *ctx;
	int i;

	ctx = key;
	aes_icm_crypt_last(key, in, out, AESICM_BLOCKSIZE);

	/* increment counter */
	for (i = AESICM_BLOCKSIZE - 1;
	     i >= 0; i--)
		if (++ctx->ac_block[i])   /* continue on overflow */
			break;
}

static void
aes_icm_crypt_last(void *key, const uint8_t *in, uint8_t *out, size_t len)
{
	struct aes_icm_ctx *ctx;
	uint8_t keystream[AESICM_BLOCKSIZE];
	int i;

	ctx = key;
	rijndaelEncrypt(ctx->ac_ek, ctx->ac_nr, ctx->ac_block, keystream);
	for (i = 0; i < len; i++)
		out[i] = in[i] ^ keystream[i];
	explicit_bzero(keystream, sizeof(keystream));
}

static int
aes_icm_setkey(void *sched, const uint8_t *key, int len)
{
	struct aes_icm_ctx *ctx;

	if (len != 16 && len != 24 && len != 32)
		return (EINVAL);

	ctx = sched;
	ctx->ac_nr = rijndaelKeySetupEnc(ctx->ac_ek, key, len * 8);
	return (0);
}
