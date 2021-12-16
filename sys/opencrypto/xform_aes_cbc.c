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

#include <crypto/rijndael/rijndael.h>
#include <opencrypto/xform_enc.h>

static	int aes_cbc_setkey(void *, const uint8_t *, int);
static	void aes_cbc_encrypt(void *, const uint8_t *, uint8_t *);
static	void aes_cbc_decrypt(void *, const uint8_t *, uint8_t *);

/* Encryption instances */
const struct enc_xform enc_xform_aes_cbc = {
	.type = CRYPTO_AES_CBC,
	.name = "AES-CBC",
	.ctxsize = sizeof(rijndael_ctx),
	.blocksize = AES_BLOCK_LEN,
	.ivsize = AES_BLOCK_LEN,
	.minkey = AES_MIN_KEY,
	.maxkey = AES_MAX_KEY,
	.encrypt = aes_cbc_encrypt,
	.decrypt = aes_cbc_decrypt,
	.setkey = aes_cbc_setkey,
};

/*
 * Encryption wrapper routines.
 */
static void
aes_cbc_encrypt(void *key, const uint8_t *in, uint8_t *out)
{
	rijndael_encrypt(key, in, out);
}

static void
aes_cbc_decrypt(void *key, const uint8_t *in, uint8_t *out)
{
	rijndael_decrypt(key, in, out);
}

static int
aes_cbc_setkey(void *sched, const uint8_t *key, int len)
{

	if (len != 16 && len != 24 && len != 32)
		return (EINVAL);

	rijndael_set_key(sched, key, len * 8);
	return (0);
}
