/*
 * Copyright (c) 2000 Markus Friedl.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 */

#include "includes.h"
RCSID("$OpenBSD: dsa.c,v 1.11 2000/09/07 20:27:51 deraadt Exp $");

#include "ssh.h"
#include "xmalloc.h"
#include "buffer.h"
#include "bufaux.h"
#include "compat.h"

#include <openssl/bn.h>
#include <openssl/dh.h>
#include <openssl/rsa.h>
#include <openssl/dsa.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/pem.h>

#include <openssl/hmac.h>
#include "kex.h"
#include "key.h"
#include "uuencode.h"

#define INTBLOB_LEN	20
#define SIGBLOB_LEN	(2*INTBLOB_LEN)

Key *
dsa_key_from_blob(char *blob, int blen)
{
	Buffer b;
	char *ktype;
	int rlen;
	DSA *dsa;
	Key *key;

#ifdef DEBUG_DSS
	dump_base64(stderr, blob, blen);
#endif
	/* fetch & parse DSA/DSS pubkey */
	buffer_init(&b);
	buffer_append(&b, blob, blen);
	ktype = buffer_get_string(&b, NULL);
	if (strcmp(KEX_DSS, ktype) != 0) {
		error("dsa_key_from_blob: cannot handle type %s", ktype);
		buffer_free(&b);
		xfree(ktype);
		return NULL;
	}
	key = key_new(KEY_DSA);
	dsa = key->dsa;
	buffer_get_bignum2(&b, dsa->p);
	buffer_get_bignum2(&b, dsa->q);
	buffer_get_bignum2(&b, dsa->g);
	buffer_get_bignum2(&b, dsa->pub_key);
	rlen = buffer_len(&b);
	if(rlen != 0)
		error("dsa_key_from_blob: remaining bytes in key blob %d", rlen);
	buffer_free(&b);
	xfree(ktype);

#ifdef DEBUG_DSS
	DSA_print_fp(stderr, dsa, 8);
#endif
	return key;
}
int
dsa_make_key_blob(Key *key, unsigned char **blobp, unsigned int *lenp)
{
	Buffer b;
	int len;
	unsigned char *buf;

	if (key == NULL || key->type != KEY_DSA)
		return 0;
	buffer_init(&b);
	buffer_put_cstring(&b, KEX_DSS);
	buffer_put_bignum2(&b, key->dsa->p);
	buffer_put_bignum2(&b, key->dsa->q);
	buffer_put_bignum2(&b, key->dsa->g);
	buffer_put_bignum2(&b, key->dsa->pub_key);
	len = buffer_len(&b);
	buf = xmalloc(len);
	memcpy(buf, buffer_ptr(&b), len);
	memset(buffer_ptr(&b), 0, len);
	buffer_free(&b);
	if (lenp != NULL)
		*lenp = len;
	if (blobp != NULL)
		*blobp = buf;
	return len;
}
int
dsa_sign(
    Key *key,
    unsigned char **sigp, int *lenp,
    unsigned char *data, int datalen)
{
	unsigned char *digest;
	unsigned char *ret;
	DSA_SIG *sig;
	EVP_MD *evp_md = EVP_sha1();
	EVP_MD_CTX md;
	unsigned int rlen;
	unsigned int slen;
	unsigned int len;
	unsigned char sigblob[SIGBLOB_LEN];
	Buffer b;

	if (key == NULL || key->type != KEY_DSA || key->dsa == NULL) {
		error("dsa_sign: no DSA key");
		return -1;
	}
	digest = xmalloc(evp_md->md_size);
	EVP_DigestInit(&md, evp_md);
	EVP_DigestUpdate(&md, data, datalen);
	EVP_DigestFinal(&md, digest, NULL);

	sig = DSA_do_sign(digest, evp_md->md_size, key->dsa);
	if (sig == NULL) {
		fatal("dsa_sign: cannot sign");
	}

	rlen = BN_num_bytes(sig->r);
	slen = BN_num_bytes(sig->s);
	if (rlen > INTBLOB_LEN || slen > INTBLOB_LEN) {
		error("bad sig size %d %d", rlen, slen);
		DSA_SIG_free(sig);
		return -1;
	}
	debug("sig size %d %d", rlen, slen);

	memset(sigblob, 0, SIGBLOB_LEN);
	BN_bn2bin(sig->r, sigblob+ SIGBLOB_LEN - INTBLOB_LEN - rlen);
	BN_bn2bin(sig->s, sigblob+ SIGBLOB_LEN - slen);
	DSA_SIG_free(sig);

	if (datafellows & SSH_BUG_SIGBLOB) {
		debug("datafellows");
		ret = xmalloc(SIGBLOB_LEN);
		memcpy(ret, sigblob, SIGBLOB_LEN);
		if (lenp != NULL)
			*lenp = SIGBLOB_LEN;
		if (sigp != NULL)
			*sigp = ret;
	} else {
		/* ietf-drafts */
		buffer_init(&b);
		buffer_put_cstring(&b, KEX_DSS);
		buffer_put_string(&b, sigblob, SIGBLOB_LEN);
		len = buffer_len(&b);
		ret = xmalloc(len);
		memcpy(ret, buffer_ptr(&b), len);
		buffer_free(&b);
		if (lenp != NULL)
			*lenp = len;
		if (sigp != NULL)
			*sigp = ret;
	}
	return 0;
}
int
dsa_verify(
    Key *key,
    unsigned char *signature, int signaturelen,
    unsigned char *data, int datalen)
{
	Buffer b;
	unsigned char *digest;
	DSA_SIG *sig;
	EVP_MD *evp_md = EVP_sha1();
	EVP_MD_CTX md;
	unsigned char *sigblob;
	char *txt;
	unsigned int len;
	int rlen;
	int ret;

	if (key == NULL || key->type != KEY_DSA || key->dsa == NULL) {
		error("dsa_verify: no DSA key");
		return -1;
	}

	if (!(datafellows & SSH_BUG_SIGBLOB) &&
	    signaturelen == SIGBLOB_LEN) {
		datafellows |= ~SSH_BUG_SIGBLOB;
		log("autodetect SSH_BUG_SIGBLOB");
	} else if ((datafellows & SSH_BUG_SIGBLOB) &&
	    signaturelen != SIGBLOB_LEN) {
		log("autoremove SSH_BUG_SIGBLOB");
		datafellows &= ~SSH_BUG_SIGBLOB;
	}

	debug("len %d datafellows %d", signaturelen, datafellows);

	/* fetch signature */
	if (datafellows & SSH_BUG_SIGBLOB) {
		sigblob = signature;
		len = signaturelen;
	} else {
		/* ietf-drafts */
		char *ktype;
		buffer_init(&b);
		buffer_append(&b, (char *) signature, signaturelen);
		ktype = buffer_get_string(&b, NULL);
		if (strcmp(KEX_DSS, ktype) != 0) {
			error("dsa_verify: cannot handle type %s", ktype);
			buffer_free(&b);
			return -1;
		}
		sigblob = (unsigned char *)buffer_get_string(&b, &len);
		rlen = buffer_len(&b);
		if(rlen != 0) {
			error("remaining bytes in signature %d", rlen);
			buffer_free(&b);
			return -1;
		}
		buffer_free(&b);
		xfree(ktype);
	}

	if (len != SIGBLOB_LEN) {
		fatal("bad sigbloblen %d != SIGBLOB_LEN", len);
	}

	/* parse signature */
	sig = DSA_SIG_new();
	sig->r = BN_new();
	sig->s = BN_new();
	BN_bin2bn(sigblob, INTBLOB_LEN, sig->r);
	BN_bin2bn(sigblob+ INTBLOB_LEN, INTBLOB_LEN, sig->s);

	if (!(datafellows & SSH_BUG_SIGBLOB)) {
		memset(sigblob, 0, len);
		xfree(sigblob);
	}
	
	/* sha1 the data */
	digest = xmalloc(evp_md->md_size);
	EVP_DigestInit(&md, evp_md);
	EVP_DigestUpdate(&md, data, datalen);
	EVP_DigestFinal(&md, digest, NULL);

	ret = DSA_do_verify(digest, evp_md->md_size, sig, key->dsa);

	memset(digest, 0, evp_md->md_size);
	xfree(digest);
	DSA_SIG_free(sig);

	switch (ret) {
	case 1:
		txt = "correct";
		break;
	case 0:
		txt = "incorrect";
		break;
	case -1:
	default:
		txt = "error";
		break;
	}
	debug("dsa_verify: signature %s", txt);
	return ret;
}

Key *
dsa_generate_key(unsigned int bits)
{
	DSA *dsa = DSA_generate_parameters(bits, NULL, 0, NULL, NULL, NULL, NULL);
	Key *k;
	if (dsa == NULL) {
		fatal("DSA_generate_parameters failed");
	}
	if (!DSA_generate_key(dsa)) {
		fatal("DSA_generate_keys failed");
	}

	k = key_new(KEY_EMPTY);
	k->type = KEY_DSA;
	k->dsa = dsa;
	return k;
}
