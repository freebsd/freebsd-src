/*	$OpenBSD: xform.c,v 1.16 2001/08/28 12:20:43 ben Exp $	*/
/*-
 * The authors of this code are John Ioannidis (ji@tla.org),
 * Angelos D. Keromytis (kermit@csd.uch.gr) and
 * Niels Provos (provos@physnet.uni-hamburg.de).
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
 * Copyright (C) 1995, 1996, 1997, 1998, 1999 by John Ioannidis,
 * Angelos D. Keromytis and Niels Provos.
 *
 * Copyright (C) 2001, Angelos D. Keromytis.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/sysctl.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <machine/cpu.h>

#include <crypto/blowfish/blowfish.h>
#include <crypto/des/des.h>
#include <crypto/rijndael/rijndael.h>
#include <crypto/sha1.h>

#include <opencrypto/cast.h>
#include <opencrypto/deflate.h>
#include <opencrypto/rmd160.h>
#include <opencrypto/skipjack.h>

#include <sys/md5.h>

#include <opencrypto/cryptodev.h>
#include <opencrypto/xform.h>

static void null_encrypt(caddr_t, u_int8_t *);
static void null_decrypt(caddr_t, u_int8_t *);
static int null_setkey(u_int8_t **, u_int8_t *, int);
static void null_zerokey(u_int8_t **);

static	int des1_setkey(u_int8_t **, u_int8_t *, int);
static	int des3_setkey(u_int8_t **, u_int8_t *, int);
static	int blf_setkey(u_int8_t **, u_int8_t *, int);
static	int cast5_setkey(u_int8_t **, u_int8_t *, int);
static	int skipjack_setkey(u_int8_t **, u_int8_t *, int);
static	int rijndael128_setkey(u_int8_t **, u_int8_t *, int);
static	void des1_encrypt(caddr_t, u_int8_t *);
static	void des3_encrypt(caddr_t, u_int8_t *);
static	void blf_encrypt(caddr_t, u_int8_t *);
static	void cast5_encrypt(caddr_t, u_int8_t *);
static	void skipjack_encrypt(caddr_t, u_int8_t *);
static	void rijndael128_encrypt(caddr_t, u_int8_t *);
static	void des1_decrypt(caddr_t, u_int8_t *);
static	void des3_decrypt(caddr_t, u_int8_t *);
static	void blf_decrypt(caddr_t, u_int8_t *);
static	void cast5_decrypt(caddr_t, u_int8_t *);
static	void skipjack_decrypt(caddr_t, u_int8_t *);
static	void rijndael128_decrypt(caddr_t, u_int8_t *);
static	void des1_zerokey(u_int8_t **);
static	void des3_zerokey(u_int8_t **);
static	void blf_zerokey(u_int8_t **);
static	void cast5_zerokey(u_int8_t **);
static	void skipjack_zerokey(u_int8_t **);
static	void rijndael128_zerokey(u_int8_t **);

static	void null_init(void *);
static	int null_update(void *, u_int8_t *, u_int16_t);
static	void null_final(u_int8_t *, void *);
static	int MD5Update_int(void *, u_int8_t *, u_int16_t);
static	void SHA1Init_int(void *);
static	int SHA1Update_int(void *, u_int8_t *, u_int16_t);
static	void SHA1Final_int(u_int8_t *, void *);
static	int RMD160Update_int(void *, u_int8_t *, u_int16_t);
static	int SHA256Update_int(void *, u_int8_t *, u_int16_t);
static	int SHA384Update_int(void *, u_int8_t *, u_int16_t);
static	int SHA512Update_int(void *, u_int8_t *, u_int16_t);

static	u_int32_t deflate_compress(u_int8_t *, u_int32_t, u_int8_t **);
static	u_int32_t deflate_decompress(u_int8_t *, u_int32_t, u_int8_t **);

MALLOC_DEFINE(M_XDATA, "xform", "xform data buffers");

/* Encryption instances */
struct enc_xform enc_xform_null = {
	CRYPTO_NULL_CBC, "NULL",
	/* NB: blocksize of 4 is to generate a properly aligned ESP header */
	4, 0, 256, /* 2048 bits, max key */
	null_encrypt,
	null_decrypt,
	null_setkey,
	null_zerokey,
};

struct enc_xform enc_xform_des = {
	CRYPTO_DES_CBC, "DES",
	8, 8, 8,
	des1_encrypt,
	des1_decrypt,
	des1_setkey,
	des1_zerokey,
};

struct enc_xform enc_xform_3des = {
	CRYPTO_3DES_CBC, "3DES",
	8, 24, 24,
	des3_encrypt,
	des3_decrypt,
	des3_setkey,
	des3_zerokey
};

struct enc_xform enc_xform_blf = {
	CRYPTO_BLF_CBC, "Blowfish",
	8, 5, 56 /* 448 bits, max key */,
	blf_encrypt,
	blf_decrypt,
	blf_setkey,
	blf_zerokey
};

struct enc_xform enc_xform_cast5 = {
	CRYPTO_CAST_CBC, "CAST-128",
	8, 5, 16,
	cast5_encrypt,
	cast5_decrypt,
	cast5_setkey,
	cast5_zerokey
};

struct enc_xform enc_xform_skipjack = {
	CRYPTO_SKIPJACK_CBC, "Skipjack",
	8, 10, 10,
	skipjack_encrypt,
	skipjack_decrypt,
	skipjack_setkey,
	skipjack_zerokey
};

struct enc_xform enc_xform_rijndael128 = {
	CRYPTO_RIJNDAEL128_CBC, "Rijndael-128/AES",
	16, 8, 32,
	rijndael128_encrypt,
	rijndael128_decrypt,
	rijndael128_setkey,
	rijndael128_zerokey,
};

struct enc_xform enc_xform_arc4 = {
	CRYPTO_ARC4, "ARC4",
	1, 1, 32,
	NULL,
	NULL,
	NULL,
	NULL,
};

/* Authentication instances */
struct auth_hash auth_hash_null = {
	CRYPTO_NULL_HMAC, "NULL-HMAC",
	0, 0, 12, sizeof(int),			/* NB: context isn't used */
	null_init, null_update, null_final
};

struct auth_hash auth_hash_hmac_md5_96 = {
	CRYPTO_MD5_HMAC, "HMAC-MD5",
	16, 16, 12, sizeof(MD5_CTX),
	(void (*) (void *)) MD5Init, MD5Update_int,
	(void (*) (u_int8_t *, void *)) MD5Final
};

struct auth_hash auth_hash_hmac_sha1_96 = {
	CRYPTO_SHA1_HMAC, "HMAC-SHA1",
	20, 20, 12, sizeof(SHA1_CTX),
	SHA1Init_int, SHA1Update_int, SHA1Final_int
};

struct auth_hash auth_hash_hmac_ripemd_160_96 = {
	CRYPTO_RIPEMD160_HMAC, "HMAC-RIPEMD-160",
	20, 20, 12, sizeof(RMD160_CTX),
	(void (*)(void *)) RMD160Init, RMD160Update_int,
	(void (*)(u_int8_t *, void *)) RMD160Final
};

struct auth_hash auth_hash_key_md5 = {
	CRYPTO_MD5_KPDK, "Keyed MD5", 
	0, 16, 12, sizeof(MD5_CTX),
	(void (*)(void *)) MD5Init, MD5Update_int,
	(void (*)(u_int8_t *, void *)) MD5Final
};

struct auth_hash auth_hash_key_sha1 = {
	CRYPTO_SHA1_KPDK, "Keyed SHA1",
	0, 20, 12, sizeof(SHA1_CTX),
	SHA1Init_int, SHA1Update_int, SHA1Final_int
};

struct auth_hash auth_hash_hmac_sha2_256 = {
	CRYPTO_SHA2_HMAC, "HMAC-SHA2",
	32, 32, 12, sizeof(SHA256_CTX),
	(void (*)(void *)) SHA256_Init, SHA256Update_int,
	(void (*)(u_int8_t *, void *)) SHA256_Final
};

struct auth_hash auth_hash_hmac_sha2_384 = {
	CRYPTO_SHA2_HMAC, "HMAC-SHA2-384",
	48, 48, 12, sizeof(SHA384_CTX),
	(void (*)(void *)) SHA384_Init, SHA384Update_int,
	(void (*)(u_int8_t *, void *)) SHA384_Final
};

struct auth_hash auth_hash_hmac_sha2_512 = {
	CRYPTO_SHA2_HMAC, "HMAC-SHA2-512",
	64, 64, 12, sizeof(SHA512_CTX),
	(void (*)(void *)) SHA512_Init, SHA512Update_int,
	(void (*)(u_int8_t *, void *)) SHA512_Final
};

/* Compression instance */
struct comp_algo comp_algo_deflate = {
	CRYPTO_DEFLATE_COMP, "Deflate",
	90, deflate_compress,
	deflate_decompress
};

/*
 * Encryption wrapper routines.
 */
static void
null_encrypt(caddr_t key, u_int8_t *blk)
{
}
static void
null_decrypt(caddr_t key, u_int8_t *blk)
{
}
static int
null_setkey(u_int8_t **sched, u_int8_t *key, int len)
{
	*sched = NULL;
	return 0;
}
static void
null_zerokey(u_int8_t **sched)
{
	*sched = NULL;
}

static void
des1_encrypt(caddr_t key, u_int8_t *blk)
{
	des_cblock *cb = (des_cblock *) blk;
	des_key_schedule *p = (des_key_schedule *) key;

	des_ecb_encrypt(cb, cb, p[0], DES_ENCRYPT);
}

static void
des1_decrypt(caddr_t key, u_int8_t *blk)
{
	des_cblock *cb = (des_cblock *) blk;
	des_key_schedule *p = (des_key_schedule *) key;

	des_ecb_encrypt(cb, cb, p[0], DES_DECRYPT);
}

static int
des1_setkey(u_int8_t **sched, u_int8_t *key, int len)
{
	des_key_schedule *p;
	int err;

	MALLOC(p, des_key_schedule *, sizeof (des_key_schedule),
		M_CRYPTO_DATA, M_NOWAIT|M_ZERO);
	if (p != NULL) {
		des_set_key((des_cblock *) key, p[0]);
		err = 0;
	} else
		err = ENOMEM;
	*sched = (u_int8_t *) p;
	return err;
}

static void
des1_zerokey(u_int8_t **sched)
{
	bzero(*sched, sizeof (des_key_schedule));
	FREE(*sched, M_CRYPTO_DATA);
	*sched = NULL;
}

static void
des3_encrypt(caddr_t key, u_int8_t *blk)
{
	des_cblock *cb = (des_cblock *) blk;
	des_key_schedule *p = (des_key_schedule *) key;

	des_ecb3_encrypt(cb, cb, p[0], p[1], p[2], DES_ENCRYPT);
}

static void
des3_decrypt(caddr_t key, u_int8_t *blk)
{
	des_cblock *cb = (des_cblock *) blk;
	des_key_schedule *p = (des_key_schedule *) key;

	des_ecb3_encrypt(cb, cb, p[0], p[1], p[2], DES_DECRYPT);
}

static int
des3_setkey(u_int8_t **sched, u_int8_t *key, int len)
{
	des_key_schedule *p;
	int err;

	MALLOC(p, des_key_schedule *, 3*sizeof (des_key_schedule),
		M_CRYPTO_DATA, M_NOWAIT|M_ZERO);
	if (p != NULL) {
		des_set_key((des_cblock *)(key +  0), p[0]);
		des_set_key((des_cblock *)(key +  8), p[1]);
		des_set_key((des_cblock *)(key + 16), p[2]);
		err = 0;
	} else
		err = ENOMEM;
	*sched = (u_int8_t *) p;
	return err;
}

static void
des3_zerokey(u_int8_t **sched)
{
	bzero(*sched, 3*sizeof (des_key_schedule));
	FREE(*sched, M_CRYPTO_DATA);
	*sched = NULL;
}

static void
blf_encrypt(caddr_t key, u_int8_t *blk)
{
	BF_LONG t[2];

	memcpy(t, blk, sizeof (t));
	t[0] = ntohl(t[0]);
	t[1] = ntohl(t[1]);
	/* NB: BF_encrypt expects the block in host order! */
	BF_encrypt(t, (BF_KEY *) key);
	t[0] = htonl(t[0]);
	t[1] = htonl(t[1]);
	memcpy(blk, t, sizeof (t));
}

static void
blf_decrypt(caddr_t key, u_int8_t *blk)
{
	BF_LONG t[2];

	memcpy(t, blk, sizeof (t));
	t[0] = ntohl(t[0]);
	t[1] = ntohl(t[1]);
	/* NB: BF_decrypt expects the block in host order! */
	BF_decrypt(t, (BF_KEY *) key);
	t[0] = htonl(t[0]);
	t[1] = htonl(t[1]);
	memcpy(blk, t, sizeof (t));
}

static int
blf_setkey(u_int8_t **sched, u_int8_t *key, int len)
{
	int err;

	MALLOC(*sched, u_int8_t *, sizeof(BF_KEY),
		M_CRYPTO_DATA, M_NOWAIT|M_ZERO);
	if (*sched != NULL) {
		BF_set_key((BF_KEY *) *sched, len, key);
		err = 0;
	} else
		err = ENOMEM;
	return err;
}

static void
blf_zerokey(u_int8_t **sched)
{
	bzero(*sched, sizeof(BF_KEY));
	FREE(*sched, M_CRYPTO_DATA);
	*sched = NULL;
}

static void
cast5_encrypt(caddr_t key, u_int8_t *blk)
{
	cast_encrypt((cast_key *) key, blk, blk);
}

static void
cast5_decrypt(caddr_t key, u_int8_t *blk)
{
	cast_decrypt((cast_key *) key, blk, blk);
}

static int
cast5_setkey(u_int8_t **sched, u_int8_t *key, int len)
{
	int err;

	MALLOC(*sched, u_int8_t *, sizeof(cast_key), M_CRYPTO_DATA, M_NOWAIT|M_ZERO);
	if (*sched != NULL) {
		cast_setkey((cast_key *)*sched, key, len);
		err = 0;
	} else
		err = ENOMEM;
	return err;
}

static void
cast5_zerokey(u_int8_t **sched)
{
	bzero(*sched, sizeof(cast_key));
	FREE(*sched, M_CRYPTO_DATA);
	*sched = NULL;
}

static void
skipjack_encrypt(caddr_t key, u_int8_t *blk)
{
	skipjack_forwards(blk, blk, (u_int8_t **) key);
}

static void
skipjack_decrypt(caddr_t key, u_int8_t *blk)
{
	skipjack_backwards(blk, blk, (u_int8_t **) key);
}

static int
skipjack_setkey(u_int8_t **sched, u_int8_t *key, int len)
{
	int err;

	/* NB: allocate all the memory that's needed at once */
	MALLOC(*sched, u_int8_t *, 10 * (sizeof(u_int8_t *) + 0x100),
		M_CRYPTO_DATA, M_NOWAIT|M_ZERO);
	if (*sched != NULL) {
		u_int8_t** key_tables = (u_int8_t**) *sched;
		u_int8_t* table = (u_int8_t*) &key_tables[10];
		int k;

		for (k = 0; k < 10; k++) {
			key_tables[k] = table;
			table += 0x100;
		}
		subkey_table_gen(key, (u_int8_t **) *sched);
		err = 0;
	} else
		err = ENOMEM;
	return err;
}

static void
skipjack_zerokey(u_int8_t **sched)
{
	bzero(*sched, 10 * (sizeof(u_int8_t *) + 0x100));
	FREE(*sched, M_CRYPTO_DATA);
	*sched = NULL;
}

static void
rijndael128_encrypt(caddr_t key, u_int8_t *blk)
{
	rijndael_encrypt((rijndael_ctx *) key, (u_char *) blk, (u_char *) blk);
}

static void
rijndael128_decrypt(caddr_t key, u_int8_t *blk)
{
	rijndael_decrypt(((rijndael_ctx *) key), (u_char *) blk,
	    (u_char *) blk);
}

static int
rijndael128_setkey(u_int8_t **sched, u_int8_t *key, int len)
{
	int err;

	MALLOC(*sched, u_int8_t *, sizeof(rijndael_ctx), M_CRYPTO_DATA,
	    M_NOWAIT|M_ZERO);
	if (*sched != NULL) {
		rijndael_set_key((rijndael_ctx *) *sched, (u_char *) key,
		    len * 8);
		err = 0;
	} else
		err = ENOMEM;
	return err;
}

static void
rijndael128_zerokey(u_int8_t **sched)
{
	bzero(*sched, sizeof(rijndael_ctx));
	FREE(*sched, M_CRYPTO_DATA);
	*sched = NULL;
}

/*
 * And now for auth.
 */

static void
null_init(void *ctx)
{
}

static int
null_update(void *ctx, u_int8_t *buf, u_int16_t len)
{
	return 0;
}

static void
null_final(u_int8_t *buf, void *ctx)
{
	if (buf != (u_int8_t *) 0)
		bzero(buf, 12);
}

static int
RMD160Update_int(void *ctx, u_int8_t *buf, u_int16_t len)
{
	RMD160Update(ctx, buf, len);
	return 0;
}

static int
MD5Update_int(void *ctx, u_int8_t *buf, u_int16_t len)
{
	MD5Update(ctx, buf, len);
	return 0;
}

static void
SHA1Init_int(void *ctx)
{
	SHA1Init(ctx);
}

static int
SHA1Update_int(void *ctx, u_int8_t *buf, u_int16_t len)
{
	SHA1Update(ctx, buf, len);
	return 0;
}

static void
SHA1Final_int(u_int8_t *blk, void *ctx)
{
	SHA1Final(blk, ctx);
}

static int
SHA256Update_int(void *ctx, u_int8_t *buf, u_int16_t len)
{
	SHA256_Update(ctx, buf, len);
	return 0;
}

static int
SHA384Update_int(void *ctx, u_int8_t *buf, u_int16_t len)
{
	SHA384_Update(ctx, buf, len);
	return 0;
}

static int
SHA512Update_int(void *ctx, u_int8_t *buf, u_int16_t len)
{
	SHA512_Update(ctx, buf, len);
	return 0;
}

/*
 * And compression
 */

static u_int32_t
deflate_compress(data, size, out)
	u_int8_t *data;
	u_int32_t size;
	u_int8_t **out;
{
	return deflate_global(data, size, 0, out);
}

static u_int32_t
deflate_decompress(data, size, out)
	u_int8_t *data;
	u_int32_t size;
	u_int8_t **out;
{
	return deflate_global(data, size, 1, out);
}
