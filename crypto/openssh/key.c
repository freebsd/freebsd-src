/*
 * read_bignum():
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 *
 *
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
#include "ssh.h"
#include <openssl/rsa.h>
#include <openssl/dsa.h>
#include <openssl/evp.h>
#include "xmalloc.h"
#include "key.h"
#include "dsa.h"
#include "uuencode.h"

RCSID("$OpenBSD: key.c,v 1.11 2000/09/07 20:27:51 deraadt Exp $");

#define SSH_DSS "ssh-dss"

Key *
key_new(int type)
{
	Key *k;
	RSA *rsa;
	DSA *dsa;
	k = xmalloc(sizeof(*k));
	k->type = type;
	k->dsa = NULL;
	k->rsa = NULL;
	switch (k->type) {
	case KEY_RSA:
		rsa = RSA_new();
		rsa->n = BN_new();
		rsa->e = BN_new();
		k->rsa = rsa;
		break;
	case KEY_DSA:
		dsa = DSA_new();
		dsa->p = BN_new();
		dsa->q = BN_new();
		dsa->g = BN_new();
		dsa->pub_key = BN_new();
		k->dsa = dsa;
		break;
	case KEY_EMPTY:
		break;
	default:
		fatal("key_new: bad key type %d", k->type);
		break;
	}
	return k;
}
void
key_free(Key *k)
{
	switch (k->type) {
	case KEY_RSA:
		if (k->rsa != NULL)
			RSA_free(k->rsa);
		k->rsa = NULL;
		break;
	case KEY_DSA:
		if (k->dsa != NULL)
			DSA_free(k->dsa);
		k->dsa = NULL;
		break;
	default:
		fatal("key_free: bad key type %d", k->type);
		break;
	}
	xfree(k);
}
int
key_equal(Key *a, Key *b)
{
	if (a == NULL || b == NULL || a->type != b->type)
		return 0;
	switch (a->type) {
	case KEY_RSA:
		return a->rsa != NULL && b->rsa != NULL &&
		    BN_cmp(a->rsa->e, b->rsa->e) == 0 &&
		    BN_cmp(a->rsa->n, b->rsa->n) == 0;
		break;
	case KEY_DSA:
		return a->dsa != NULL && b->dsa != NULL &&
		    BN_cmp(a->dsa->p, b->dsa->p) == 0 &&
		    BN_cmp(a->dsa->q, b->dsa->q) == 0 &&
		    BN_cmp(a->dsa->g, b->dsa->g) == 0 &&
		    BN_cmp(a->dsa->pub_key, b->dsa->pub_key) == 0;
		break;
	default:
		fatal("key_equal: bad key type %d", a->type);
		break;
	}
	return 0;
}

/*
 * Generate key fingerprint in ascii format.
 * Based on ideas and code from Bjoern Groenvall <bg@sics.se>
 */
char *
key_fingerprint(Key *k)
{
	static char retval[(EVP_MAX_MD_SIZE+1)*3];
	unsigned char *blob = NULL;
	int len = 0;
	int nlen, elen;

	switch (k->type) {
	case KEY_RSA:
		nlen = BN_num_bytes(k->rsa->n);
		elen = BN_num_bytes(k->rsa->e);
		len = nlen + elen;
		blob = xmalloc(len);
		BN_bn2bin(k->rsa->n, blob);
		BN_bn2bin(k->rsa->e, blob + nlen);
		break;
	case KEY_DSA:
		dsa_make_key_blob(k, &blob, &len);
		break;
	default:
		fatal("key_fingerprint: bad key type %d", k->type);
		break;
	}
	retval[0] = '\0';

	if (blob != NULL) {
		int i;
		unsigned char digest[EVP_MAX_MD_SIZE];
		EVP_MD *md = EVP_md5();
		EVP_MD_CTX ctx;
		EVP_DigestInit(&ctx, md);
		EVP_DigestUpdate(&ctx, blob, len);
		EVP_DigestFinal(&ctx, digest, NULL);
		for(i = 0; i < md->md_size; i++) {
			char hex[4];
			snprintf(hex, sizeof(hex), "%02x:", digest[i]);
			strlcat(retval, hex, sizeof(retval));
		}
		retval[strlen(retval) - 1] = '\0';
		memset(blob, 0, len);
		xfree(blob);
	}
	return retval;
}

/*
 * Reads a multiple-precision integer in decimal from the buffer, and advances
 * the pointer.  The integer must already be initialized.  This function is
 * permitted to modify the buffer.  This leaves *cpp to point just beyond the
 * last processed (and maybe modified) character.  Note that this may modify
 * the buffer containing the number.
 */
int
read_bignum(char **cpp, BIGNUM * value)
{
	char *cp = *cpp;
	int old;

	/* Skip any leading whitespace. */
	for (; *cp == ' ' || *cp == '\t'; cp++)
		;

	/* Check that it begins with a decimal digit. */
	if (*cp < '0' || *cp > '9')
		return 0;

	/* Save starting position. */
	*cpp = cp;

	/* Move forward until all decimal digits skipped. */
	for (; *cp >= '0' && *cp <= '9'; cp++)
		;

	/* Save the old terminating character, and replace it by \0. */
	old = *cp;
	*cp = 0;

	/* Parse the number. */
	if (BN_dec2bn(&value, *cpp) == 0)
		return 0;

	/* Restore old terminating character. */
	*cp = old;

	/* Move beyond the number and return success. */
	*cpp = cp;
	return 1;
}
int
write_bignum(FILE *f, BIGNUM *num)
{
	char *buf = BN_bn2dec(num);
	if (buf == NULL) {
		error("write_bignum: BN_bn2dec() failed");
		return 0;
	}
	fprintf(f, " %s", buf);
	free(buf);
	return 1;
}
unsigned int
key_read(Key *ret, char **cpp)
{
	Key *k;
	unsigned int bits = 0;
	char *cp;
	int len, n;
	unsigned char *blob;

	cp = *cpp;

	switch(ret->type) {
	case KEY_RSA:
		/* Get number of bits. */
		if (*cp < '0' || *cp > '9')
			return 0;	/* Bad bit count... */
		for (bits = 0; *cp >= '0' && *cp <= '9'; cp++)
			bits = 10 * bits + *cp - '0';
		if (bits == 0)
			return 0;
		*cpp = cp;
		/* Get public exponent, public modulus. */
		if (!read_bignum(cpp, ret->rsa->e))
			return 0;
		if (!read_bignum(cpp, ret->rsa->n))
			return 0;
		break;
	case KEY_DSA:
		if (strncmp(cp, SSH_DSS " ", 7) != 0)
			return 0;
		cp += 7;
		len = 2*strlen(cp);
		blob = xmalloc(len);
		n = uudecode(cp, blob, len);
		if (n < 0) {
			error("key_read: uudecode %s failed", cp);
			return 0;
		}
		k = dsa_key_from_blob(blob, n);
		if (k == NULL) {
			error("key_read: dsa_key_from_blob %s failed", cp);
			return 0;
		}
		xfree(blob);
		if (ret->dsa != NULL)
			DSA_free(ret->dsa);
		ret->dsa = k->dsa;
		k->dsa = NULL;
		key_free(k);
		bits = BN_num_bits(ret->dsa->p);
		/* advance cp: skip whitespace and data */
		while (*cp == ' ' || *cp == '\t')
			cp++;
		while (*cp != '\0' && *cp != ' ' && *cp != '\t')
			cp++;
		*cpp = cp;
		break;
	default:
		fatal("key_read: bad key type: %d", ret->type);
		break;
	}
	return bits;
}
int
key_write(Key *key, FILE *f)
{
	int success = 0;
	unsigned int bits = 0;

	if (key->type == KEY_RSA && key->rsa != NULL) {
		/* size of modulus 'n' */
		bits = BN_num_bits(key->rsa->n);
		fprintf(f, "%u", bits);
		if (write_bignum(f, key->rsa->e) &&
		    write_bignum(f, key->rsa->n)) {
			success = 1;
		} else {
			error("key_write: failed for RSA key");
		}
	} else if (key->type == KEY_DSA && key->dsa != NULL) {
		int len, n;
		unsigned char *blob, *uu;
		dsa_make_key_blob(key, &blob, &len);
		uu = xmalloc(2*len);
		n = uuencode(blob, len, uu, 2*len);
		if (n > 0) {
			fprintf(f, "%s %s", SSH_DSS, uu);
			success = 1;
		}
		xfree(blob);
		xfree(uu);
	}
	return success;
}
char *
key_type(Key *k)
{
	switch (k->type) {
	case KEY_RSA:
		return "RSA";
		break;
	case KEY_DSA:
		return "DSA";
		break;
	}
	return "unknown";
}
unsigned int
key_size(Key *k){
	switch (k->type) {
	case KEY_RSA:
		return BN_num_bits(k->rsa->n);
		break;
	case KEY_DSA:
		return BN_num_bits(k->dsa->p);
		break;
	}
	return 0;
}
