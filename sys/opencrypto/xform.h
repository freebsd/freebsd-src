/*	$FreeBSD$	*/
/*	$OpenBSD: xform.h,v 1.8 2001/08/28 12:20:43 ben Exp $	*/

/*-
 * The author of this code is Angelos D. Keromytis (angelos@cis.upenn.edu)
 *
 * This code was written by Angelos D. Keromytis in Athens, Greece, in
 * February 2000. Network Security Technologies Inc. (NSTI) kindly
 * supported the development of this code.
 *
 * Copyright (c) 2000 Angelos D. Keromytis
 *
 * Permission to use, copy, and modify this software without fee
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

#ifndef _CRYPTO_XFORM_H_
#define _CRYPTO_XFORM_H_

#include <sys/md5.h>
#include <crypto/sha1.h>
#include <crypto/sha2/sha2.h>
#include <opencrypto/rmd160.h>

/* Declarations */
struct auth_hash {
	int type;
	char *name;
	u_int16_t keysize;
	u_int16_t hashsize; 
	u_int16_t blocksize;
	u_int16_t ctxsize;
	void (*Init) (void *);
	int  (*Update) (void *, u_int8_t *, u_int16_t);
	void (*Final) (u_int8_t *, void *);
};

#define	AH_ALEN_MAX	20	/* max authenticator hash length */

struct enc_xform {
	int type;
	char *name;
	u_int16_t blocksize;
	u_int16_t minkey, maxkey;
	void (*encrypt) (caddr_t, u_int8_t *);
	void (*decrypt) (caddr_t, u_int8_t *);
	int (*setkey) (u_int8_t **, u_int8_t *, int len);
	void (*zerokey) (u_int8_t **);
	void (*reinit) (caddr_t, u_int8_t *);
};

struct comp_algo {
	int type;
	char *name;
	size_t minlen;
	u_int32_t (*compress) (u_int8_t *, u_int32_t, u_int8_t **);
	u_int32_t (*decompress) (u_int8_t *, u_int32_t, u_int8_t **);
};

union authctx {
	MD5_CTX md5ctx;
	SHA1_CTX sha1ctx;
	RMD160_CTX rmd160ctx;
	SHA256_CTX sha256ctx;
	SHA384_CTX sha384ctx;
	SHA512_CTX sha512ctx;
};

extern struct enc_xform enc_xform_null;
extern struct enc_xform enc_xform_des;
extern struct enc_xform enc_xform_3des;
extern struct enc_xform enc_xform_blf;
extern struct enc_xform enc_xform_cast5;
extern struct enc_xform enc_xform_skipjack;
extern struct enc_xform enc_xform_rijndael128;
extern struct enc_xform enc_xform_aes_xts;
extern struct enc_xform enc_xform_arc4;
extern struct enc_xform enc_xform_camellia;

extern struct auth_hash auth_hash_null;
extern struct auth_hash auth_hash_key_md5;
extern struct auth_hash auth_hash_key_sha1;
extern struct auth_hash auth_hash_hmac_md5;
extern struct auth_hash auth_hash_hmac_sha1;
extern struct auth_hash auth_hash_hmac_ripemd_160;
extern struct auth_hash auth_hash_hmac_sha2_256;
extern struct auth_hash auth_hash_hmac_sha2_384;
extern struct auth_hash auth_hash_hmac_sha2_512;

extern struct comp_algo comp_algo_deflate;

#ifdef _KERNEL
#include <sys/malloc.h>
MALLOC_DECLARE(M_XDATA);
#endif
#endif /* _CRYPTO_XFORM_H_ */
