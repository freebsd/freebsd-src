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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Markus Friedl.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
/*
 * read_bignum():
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 */

#include "includes.h"
#include "ssh.h"
#include <ssl/rsa.h>
#include <ssl/dsa.h>
#include <ssl/evp.h>
#include "xmalloc.h"
#include "key.h"

Key *
key_new(int type)
{
	Key *k;
	RSA *rsa;
	DSA *dsa;
	k = xmalloc(sizeof(*k));
	k->type = type;
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
		k->dsa = NULL;
		k->rsa = NULL;
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
		fatal("key_free: bad key type %d", a->type);
		break;
	}
	return 0;
}

#define FPRINT "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x"

/*
 * Generate key fingerprint in ascii format.
 * Based on ideas and code from Bjoern Groenvall <bg@sics.se>
 */
char *
key_fingerprint(Key *k)
{
	static char retval[80];
	unsigned char *buf = NULL;
	int len = 0;
	int nlen, elen, plen, qlen, glen, publen;

	switch (k->type) {
	case KEY_RSA:
		nlen = BN_num_bytes(k->rsa->n);
		elen = BN_num_bytes(k->rsa->e);
		len = nlen + elen;
		buf = xmalloc(len);
		BN_bn2bin(k->rsa->n, buf);
		BN_bn2bin(k->rsa->e, buf + nlen);
		break;
	case KEY_DSA:
		plen = BN_num_bytes(k->dsa->p);
		qlen = BN_num_bytes(k->dsa->q);
		glen = BN_num_bytes(k->dsa->g);
		publen = BN_num_bytes(k->dsa->pub_key);
		len = qlen + qlen + glen + publen;
		buf = xmalloc(len);
		BN_bn2bin(k->dsa->p, buf);
		BN_bn2bin(k->dsa->q, buf + plen);
		BN_bn2bin(k->dsa->g, buf + plen + qlen);
		BN_bn2bin(k->dsa->pub_key , buf + plen + qlen + glen);
		break;
	default:
		fatal("key_fingerprint: bad key type %d", k->type);
		break;
	}
	if (buf != NULL) {
		unsigned char d[16];
		EVP_MD_CTX md;
		EVP_DigestInit(&md, EVP_md5());
		EVP_DigestUpdate(&md, buf, len);
		EVP_DigestFinal(&md, d, NULL);
		snprintf(retval, sizeof(retval), FPRINT,
		    d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7],
		    d[8], d[9], d[10], d[11], d[12], d[13], d[14], d[15]);
		memset(buf, 0, len);
		xfree(buf);
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
int
key_read(Key *ret, unsigned int bits, char **cpp)
{
	switch(ret->type) {
	case KEY_RSA:
		if (bits == 0)
			return 0;
		/* Get public exponent, public modulus. */
		if (!read_bignum(cpp, ret->rsa->e))
			return 0;
		if (!read_bignum(cpp, ret->rsa->n))
			return 0;
		break;
	case KEY_DSA:
		if (bits != 0)
			return 0;
		if (!read_bignum(cpp, ret->dsa->p))
			return 0;
		if (!read_bignum(cpp, ret->dsa->q))
			return 0;
		if (!read_bignum(cpp, ret->dsa->g))
			return 0;
		if (!read_bignum(cpp, ret->dsa->pub_key))
			return 0;
		break;
	default:
		fatal("bad key type: %d", ret->type);
		break;
	}
	return 1;
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
		/* bits == 0 means DSA key */
		bits = 0;
		fprintf(f, "%u", bits);
		if (write_bignum(f, key->dsa->p) &&
		    write_bignum(f, key->dsa->q) &&
		    write_bignum(f, key->dsa->g) &&
		    write_bignum(f, key->dsa->pub_key)) {
			success = 1;
		} else {
			error("key_write: failed for DSA key");
		}
	}
	return success;
}
