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
#include <opencrypto/xform_auth.h>
#include <opencrypto/xform_enc.h>

static	int null_setkey(void *, const uint8_t *, int);
static	void null_crypt(void *, const uint8_t *, uint8_t *);
static	void null_crypt_multi(void *, const uint8_t *, uint8_t *, size_t);

static	void null_init(void *);
static	void null_reinit(void *ctx, const uint8_t *buf, u_int len);
static	int null_update(void *, const void *, u_int);
static	void null_final(uint8_t *, void *);

/* Encryption instances */
const struct enc_xform enc_xform_null = {
	.type = CRYPTO_NULL_CBC,
	.name = "NULL",
	/* NB: blocksize of 4 is to generate a properly aligned ESP header */
	.blocksize = NULL_BLOCK_LEN,
	.ivsize = 0,
	.minkey = NULL_MIN_KEY,
	.maxkey = NULL_MAX_KEY,
	.setkey = null_setkey,
	.encrypt = null_crypt,
	.decrypt = null_crypt,
	.encrypt_multi = null_crypt_multi,
	.decrypt_multi = null_crypt_multi,
};

/* Authentication instances */
const struct auth_hash auth_hash_null = {
	.type = CRYPTO_NULL_HMAC,
	.name = "NULL-HMAC",
	.keysize = 0,
	.hashsize = NULL_HASH_LEN,
	.ctxsize = sizeof(int),	/* NB: context isn't used */
	.blocksize = NULL_HMAC_BLOCK_LEN,
	.Init = null_init,
	.Setkey = null_reinit,
	.Reinit = null_reinit,
	.Update = null_update,
	.Final = null_final,
};

/*
 * Encryption wrapper routines.
 */
static void
null_crypt(void *key, const uint8_t *in, uint8_t *out)
{
	if (in != out)
		memcpy(out, in, NULL_BLOCK_LEN);
}

static void
null_crypt_multi(void *key, const uint8_t *in, uint8_t *out, size_t len)
{
	if (in != out)
		memcpy(out, in, len);
}

static int
null_setkey(void *sched, const uint8_t *key, int len)
{
	return (0);
}

/*
 * And now for auth.
 */

static void
null_init(void *ctx)
{
}

static void
null_reinit(void *ctx, const uint8_t *buf, u_int len)
{
}

static int
null_update(void *ctx, const void *buf, u_int len)
{
	return (0);
}

static void
null_final(uint8_t *buf, void *ctx)
{
	bzero(buf, NULL_HASH_LEN);
}
