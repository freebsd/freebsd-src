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
#include <sys/types.h>
#include <sys/systm.h>
#include <opencrypto/xform_enc.h>

static	int aes_xts_setkey(void *, const uint8_t *, int);
static	void aes_xts_encrypt(void *, const uint8_t *, uint8_t *);
static	void aes_xts_decrypt(void *, const uint8_t *, uint8_t *);
static	void aes_xts_encrypt_multi(void *, const uint8_t *, uint8_t *, size_t);
static	void aes_xts_decrypt_multi(void *, const uint8_t *, uint8_t *, size_t);
static	void aes_xts_reinit(void *, const uint8_t *, size_t);

/* Encryption instances */
const struct enc_xform enc_xform_aes_xts = {
	.type = CRYPTO_AES_XTS,
	.name = "AES-XTS",
	.ctxsize = sizeof(struct aes_xts_ctx),
	.blocksize = AES_BLOCK_LEN,
	.ivsize = AES_XTS_IV_LEN,
	.minkey = AES_XTS_MIN_KEY,
	.maxkey = AES_XTS_MAX_KEY,
	.setkey = aes_xts_setkey,
	.reinit = aes_xts_reinit,
	.encrypt = aes_xts_encrypt,
	.decrypt = aes_xts_decrypt,
	.encrypt_multi = aes_xts_encrypt_multi,
	.decrypt_multi = aes_xts_decrypt_multi,
};

/*
 * Encryption wrapper routines.
 */
static void
aes_xts_reinit(void *key, const uint8_t *iv, size_t ivlen)
{
	struct aes_xts_ctx *ctx = key;
	uint64_t blocknum;
	u_int i;

	KASSERT(ivlen == sizeof(blocknum),
	    ("%s: invalid IV length", __func__));

	/*
	 * Prepare tweak as E_k2(IV). IV is specified as LE representation
	 * of a 64-bit block number which we allow to be passed in directly.
	 */
	bcopy(iv, &blocknum, AES_XTS_IVSIZE);
	for (i = 0; i < AES_XTS_IVSIZE; i++) {
		ctx->tweak[i] = blocknum & 0xff;
		blocknum >>= 8;
	}
	/* Last 64 bits of IV are always zero */
	bzero(ctx->tweak + AES_XTS_IVSIZE, AES_XTS_IVSIZE);

	rijndael_encrypt(&ctx->key2, ctx->tweak, ctx->tweak);
}

static void
aes_xts_crypt(struct aes_xts_ctx *ctx, const uint8_t *in, uint8_t *out,
    size_t len, bool do_encrypt)
{
	uint8_t block[AES_XTS_BLOCKSIZE];
	u_int i, carry_in, carry_out;

	KASSERT(len % AES_XTS_BLOCKSIZE == 0, ("%s: invalid length", __func__));
	while (len > 0) {
		for (i = 0; i < AES_XTS_BLOCKSIZE; i++)
			block[i] = in[i] ^ ctx->tweak[i];

		if (do_encrypt)
			rijndael_encrypt(&ctx->key1, block, out);
		else
			rijndael_decrypt(&ctx->key1, block, out);

		for (i = 0; i < AES_XTS_BLOCKSIZE; i++)
			out[i] ^= ctx->tweak[i];

		/* Exponentiate tweak */
		carry_in = 0;
		for (i = 0; i < AES_XTS_BLOCKSIZE; i++) {
			carry_out = ctx->tweak[i] & 0x80;
			ctx->tweak[i] = (ctx->tweak[i] << 1) | (carry_in ? 1 : 0);
			carry_in = carry_out;
		}
		if (carry_in)
			ctx->tweak[0] ^= AES_XTS_ALPHA;

		in += AES_XTS_BLOCKSIZE;
		out += AES_XTS_BLOCKSIZE;
		len -= AES_XTS_BLOCKSIZE;
	}
	explicit_bzero(block, sizeof(block));
}

static void
aes_xts_encrypt(void *key, const uint8_t *in, uint8_t *out)
{
	aes_xts_crypt(key, in, out, AES_XTS_BLOCKSIZE, true);
}

static void
aes_xts_decrypt(void *key, const uint8_t *in, uint8_t *out)
{
	aes_xts_crypt(key, in, out, AES_XTS_BLOCKSIZE, false);
}

static void
aes_xts_encrypt_multi(void *vctx, const uint8_t *in, uint8_t *out, size_t len)
{
	aes_xts_crypt(vctx, in, out, len, true);
}

static void
aes_xts_decrypt_multi(void *vctx, const uint8_t *in, uint8_t *out, size_t len)
{
	aes_xts_crypt(vctx, in, out, len, false);
}

static int
aes_xts_setkey(void *sched, const uint8_t *key, int len)
{
	struct aes_xts_ctx *ctx;

	if (len != 32 && len != 64)
		return (EINVAL);

	ctx = sched;

	rijndael_set_key(&ctx->key1, key, len * 4);
	rijndael_set_key(&ctx->key2, key + (len / 2), len * 4);

	return (0);
}
