/* $OpenBSD: key.c,v 1.96 2011/02/04 00:44:21 djm Exp $ */
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
 * Copyright (c) 2000, 2001 Markus Friedl.  All rights reserved.
 * Copyright (c) 2008 Alexander von Gernler.  All rights reserved.
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

#include <sys/param.h>
#include <sys/types.h>

#include <openssl/evp.h>
#include <openbsd-compat/openssl-compat.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "xmalloc.h"
#include "key.h"
#include "rsa.h"
#include "uuencode.h"
#include "buffer.h"
#include "log.h"
#include "misc.h"
#include "ssh2.h"

static struct KeyCert *
cert_new(void)
{
	struct KeyCert *cert;

	cert = xcalloc(1, sizeof(*cert));
	buffer_init(&cert->certblob);
	buffer_init(&cert->critical);
	buffer_init(&cert->extensions);
	cert->key_id = NULL;
	cert->principals = NULL;
	cert->signature_key = NULL;
	return cert;
}

Key *
key_new(int type)
{
	Key *k;
	RSA *rsa;
	DSA *dsa;
	k = xcalloc(1, sizeof(*k));
	k->type = type;
	k->ecdsa = NULL;
	k->ecdsa_nid = -1;
	k->dsa = NULL;
	k->rsa = NULL;
	k->cert = NULL;
	switch (k->type) {
	case KEY_RSA1:
	case KEY_RSA:
	case KEY_RSA_CERT_V00:
	case KEY_RSA_CERT:
		if ((rsa = RSA_new()) == NULL)
			fatal("key_new: RSA_new failed");
		if ((rsa->n = BN_new()) == NULL)
			fatal("key_new: BN_new failed");
		if ((rsa->e = BN_new()) == NULL)
			fatal("key_new: BN_new failed");
		k->rsa = rsa;
		break;
	case KEY_DSA:
	case KEY_DSA_CERT_V00:
	case KEY_DSA_CERT:
		if ((dsa = DSA_new()) == NULL)
			fatal("key_new: DSA_new failed");
		if ((dsa->p = BN_new()) == NULL)
			fatal("key_new: BN_new failed");
		if ((dsa->q = BN_new()) == NULL)
			fatal("key_new: BN_new failed");
		if ((dsa->g = BN_new()) == NULL)
			fatal("key_new: BN_new failed");
		if ((dsa->pub_key = BN_new()) == NULL)
			fatal("key_new: BN_new failed");
		k->dsa = dsa;
		break;
#ifdef OPENSSL_HAS_ECC
	case KEY_ECDSA:
	case KEY_ECDSA_CERT:
		/* Cannot do anything until we know the group */
		break;
#endif
	case KEY_UNSPEC:
		break;
	default:
		fatal("key_new: bad key type %d", k->type);
		break;
	}

	if (key_is_cert(k))
		k->cert = cert_new();

	return k;
}

void
key_add_private(Key *k)
{
	switch (k->type) {
	case KEY_RSA1:
	case KEY_RSA:
	case KEY_RSA_CERT_V00:
	case KEY_RSA_CERT:
		if ((k->rsa->d = BN_new()) == NULL)
			fatal("key_new_private: BN_new failed");
		if ((k->rsa->iqmp = BN_new()) == NULL)
			fatal("key_new_private: BN_new failed");
		if ((k->rsa->q = BN_new()) == NULL)
			fatal("key_new_private: BN_new failed");
		if ((k->rsa->p = BN_new()) == NULL)
			fatal("key_new_private: BN_new failed");
		if ((k->rsa->dmq1 = BN_new()) == NULL)
			fatal("key_new_private: BN_new failed");
		if ((k->rsa->dmp1 = BN_new()) == NULL)
			fatal("key_new_private: BN_new failed");
		break;
	case KEY_DSA:
	case KEY_DSA_CERT_V00:
	case KEY_DSA_CERT:
		if ((k->dsa->priv_key = BN_new()) == NULL)
			fatal("key_new_private: BN_new failed");
		break;
	case KEY_ECDSA:
	case KEY_ECDSA_CERT:
		/* Cannot do anything until we know the group */
		break;
	case KEY_UNSPEC:
		break;
	default:
		break;
	}
}

Key *
key_new_private(int type)
{
	Key *k = key_new(type);

	key_add_private(k);
	return k;
}

static void
cert_free(struct KeyCert *cert)
{
	u_int i;

	buffer_free(&cert->certblob);
	buffer_free(&cert->critical);
	buffer_free(&cert->extensions);
	if (cert->key_id != NULL)
		xfree(cert->key_id);
	for (i = 0; i < cert->nprincipals; i++)
		xfree(cert->principals[i]);
	if (cert->principals != NULL)
		xfree(cert->principals);
	if (cert->signature_key != NULL)
		key_free(cert->signature_key);
}

void
key_free(Key *k)
{
	if (k == NULL)
		fatal("key_free: key is NULL");
	switch (k->type) {
	case KEY_RSA1:
	case KEY_RSA:
	case KEY_RSA_CERT_V00:
	case KEY_RSA_CERT:
		if (k->rsa != NULL)
			RSA_free(k->rsa);
		k->rsa = NULL;
		break;
	case KEY_DSA:
	case KEY_DSA_CERT_V00:
	case KEY_DSA_CERT:
		if (k->dsa != NULL)
			DSA_free(k->dsa);
		k->dsa = NULL;
		break;
#ifdef OPENSSL_HAS_ECC
	case KEY_ECDSA:
	case KEY_ECDSA_CERT:
		if (k->ecdsa != NULL)
			EC_KEY_free(k->ecdsa);
		k->ecdsa = NULL;
		break;
#endif
	case KEY_UNSPEC:
		break;
	default:
		fatal("key_free: bad key type %d", k->type);
		break;
	}
	if (key_is_cert(k)) {
		if (k->cert != NULL)
			cert_free(k->cert);
		k->cert = NULL;
	}

	xfree(k);
}

static int
cert_compare(struct KeyCert *a, struct KeyCert *b)
{
	if (a == NULL && b == NULL)
		return 1;
	if (a == NULL || b == NULL)
		return 0;
	if (buffer_len(&a->certblob) != buffer_len(&b->certblob))
		return 0;
	if (timingsafe_bcmp(buffer_ptr(&a->certblob), buffer_ptr(&b->certblob),
	    buffer_len(&a->certblob)) != 0)
		return 0;
	return 1;
}

/*
 * Compare public portions of key only, allowing comparisons between
 * certificates and plain keys too.
 */
int
key_equal_public(const Key *a, const Key *b)
{
#ifdef OPENSSL_HAS_ECC
	BN_CTX *bnctx;
#endif

	if (a == NULL || b == NULL ||
	    key_type_plain(a->type) != key_type_plain(b->type))
		return 0;

	switch (a->type) {
	case KEY_RSA1:
	case KEY_RSA_CERT_V00:
	case KEY_RSA_CERT:
	case KEY_RSA:
		return a->rsa != NULL && b->rsa != NULL &&
		    BN_cmp(a->rsa->e, b->rsa->e) == 0 &&
		    BN_cmp(a->rsa->n, b->rsa->n) == 0;
	case KEY_DSA_CERT_V00:
	case KEY_DSA_CERT:
	case KEY_DSA:
		return a->dsa != NULL && b->dsa != NULL &&
		    BN_cmp(a->dsa->p, b->dsa->p) == 0 &&
		    BN_cmp(a->dsa->q, b->dsa->q) == 0 &&
		    BN_cmp(a->dsa->g, b->dsa->g) == 0 &&
		    BN_cmp(a->dsa->pub_key, b->dsa->pub_key) == 0;
#ifdef OPENSSL_HAS_ECC
	case KEY_ECDSA_CERT:
	case KEY_ECDSA:
		if (a->ecdsa == NULL || b->ecdsa == NULL ||
		    EC_KEY_get0_public_key(a->ecdsa) == NULL ||
		    EC_KEY_get0_public_key(b->ecdsa) == NULL)
			return 0;
		if ((bnctx = BN_CTX_new()) == NULL)
			fatal("%s: BN_CTX_new failed", __func__);
		if (EC_GROUP_cmp(EC_KEY_get0_group(a->ecdsa),
		    EC_KEY_get0_group(b->ecdsa), bnctx) != 0 ||
		    EC_POINT_cmp(EC_KEY_get0_group(a->ecdsa),
		    EC_KEY_get0_public_key(a->ecdsa),
		    EC_KEY_get0_public_key(b->ecdsa), bnctx) != 0) {
			BN_CTX_free(bnctx);
			return 0;
		}
		BN_CTX_free(bnctx);
		return 1;
#endif /* OPENSSL_HAS_ECC */
	default:
		fatal("key_equal: bad key type %d", a->type);
	}
	/* NOTREACHED */
}

int
key_equal(const Key *a, const Key *b)
{
	if (a == NULL || b == NULL || a->type != b->type)
		return 0;
	if (key_is_cert(a)) {
		if (!cert_compare(a->cert, b->cert))
			return 0;
	}
	return key_equal_public(a, b);
}

u_char*
key_fingerprint_raw(Key *k, enum fp_type dgst_type, u_int *dgst_raw_length)
{
	const EVP_MD *md = NULL;
	EVP_MD_CTX ctx;
	u_char *blob = NULL;
	u_char *retval = NULL;
	u_int len = 0;
	int nlen, elen, otype;

	*dgst_raw_length = 0;

	switch (dgst_type) {
	case SSH_FP_MD5:
		md = EVP_md5();
		break;
	case SSH_FP_SHA1:
		md = EVP_sha1();
		break;
	default:
		fatal("key_fingerprint_raw: bad digest type %d",
		    dgst_type);
	}
	switch (k->type) {
	case KEY_RSA1:
		nlen = BN_num_bytes(k->rsa->n);
		elen = BN_num_bytes(k->rsa->e);
		len = nlen + elen;
		blob = xmalloc(len);
		BN_bn2bin(k->rsa->n, blob);
		BN_bn2bin(k->rsa->e, blob + nlen);
		break;
	case KEY_DSA:
	case KEY_ECDSA:
	case KEY_RSA:
		key_to_blob(k, &blob, &len);
		break;
	case KEY_DSA_CERT_V00:
	case KEY_RSA_CERT_V00:
	case KEY_DSA_CERT:
	case KEY_ECDSA_CERT:
	case KEY_RSA_CERT:
		/* We want a fingerprint of the _key_ not of the cert */
		otype = k->type;
		k->type = key_type_plain(k->type);
		key_to_blob(k, &blob, &len);
		k->type = otype;
		break;
	case KEY_UNSPEC:
		return retval;
	default:
		fatal("key_fingerprint_raw: bad key type %d", k->type);
		break;
	}
	if (blob != NULL) {
		retval = xmalloc(EVP_MAX_MD_SIZE);
		EVP_DigestInit(&ctx, md);
		EVP_DigestUpdate(&ctx, blob, len);
		EVP_DigestFinal(&ctx, retval, dgst_raw_length);
		memset(blob, 0, len);
		xfree(blob);
	} else {
		fatal("key_fingerprint_raw: blob is null");
	}
	return retval;
}

static char *
key_fingerprint_hex(u_char *dgst_raw, u_int dgst_raw_len)
{
	char *retval;
	u_int i;

	retval = xcalloc(1, dgst_raw_len * 3 + 1);
	for (i = 0; i < dgst_raw_len; i++) {
		char hex[4];
		snprintf(hex, sizeof(hex), "%02x:", dgst_raw[i]);
		strlcat(retval, hex, dgst_raw_len * 3 + 1);
	}

	/* Remove the trailing ':' character */
	retval[(dgst_raw_len * 3) - 1] = '\0';
	return retval;
}

static char *
key_fingerprint_bubblebabble(u_char *dgst_raw, u_int dgst_raw_len)
{
	char vowels[] = { 'a', 'e', 'i', 'o', 'u', 'y' };
	char consonants[] = { 'b', 'c', 'd', 'f', 'g', 'h', 'k', 'l', 'm',
	    'n', 'p', 'r', 's', 't', 'v', 'z', 'x' };
	u_int i, j = 0, rounds, seed = 1;
	char *retval;

	rounds = (dgst_raw_len / 2) + 1;
	retval = xcalloc((rounds * 6), sizeof(char));
	retval[j++] = 'x';
	for (i = 0; i < rounds; i++) {
		u_int idx0, idx1, idx2, idx3, idx4;
		if ((i + 1 < rounds) || (dgst_raw_len % 2 != 0)) {
			idx0 = (((((u_int)(dgst_raw[2 * i])) >> 6) & 3) +
			    seed) % 6;
			idx1 = (((u_int)(dgst_raw[2 * i])) >> 2) & 15;
			idx2 = ((((u_int)(dgst_raw[2 * i])) & 3) +
			    (seed / 6)) % 6;
			retval[j++] = vowels[idx0];
			retval[j++] = consonants[idx1];
			retval[j++] = vowels[idx2];
			if ((i + 1) < rounds) {
				idx3 = (((u_int)(dgst_raw[(2 * i) + 1])) >> 4) & 15;
				idx4 = (((u_int)(dgst_raw[(2 * i) + 1]))) & 15;
				retval[j++] = consonants[idx3];
				retval[j++] = '-';
				retval[j++] = consonants[idx4];
				seed = ((seed * 5) +
				    ((((u_int)(dgst_raw[2 * i])) * 7) +
				    ((u_int)(dgst_raw[(2 * i) + 1])))) % 36;
			}
		} else {
			idx0 = seed % 6;
			idx1 = 16;
			idx2 = seed / 6;
			retval[j++] = vowels[idx0];
			retval[j++] = consonants[idx1];
			retval[j++] = vowels[idx2];
		}
	}
	retval[j++] = 'x';
	retval[j++] = '\0';
	return retval;
}

/*
 * Draw an ASCII-Art representing the fingerprint so human brain can
 * profit from its built-in pattern recognition ability.
 * This technique is called "random art" and can be found in some
 * scientific publications like this original paper:
 *
 * "Hash Visualization: a New Technique to improve Real-World Security",
 * Perrig A. and Song D., 1999, International Workshop on Cryptographic
 * Techniques and E-Commerce (CrypTEC '99)
 * sparrow.ece.cmu.edu/~adrian/projects/validation/validation.pdf
 *
 * The subject came up in a talk by Dan Kaminsky, too.
 *
 * If you see the picture is different, the key is different.
 * If the picture looks the same, you still know nothing.
 *
 * The algorithm used here is a worm crawling over a discrete plane,
 * leaving a trace (augmenting the field) everywhere it goes.
 * Movement is taken from dgst_raw 2bit-wise.  Bumping into walls
 * makes the respective movement vector be ignored for this turn.
 * Graphs are not unambiguous, because circles in graphs can be
 * walked in either direction.
 */

/*
 * Field sizes for the random art.  Have to be odd, so the starting point
 * can be in the exact middle of the picture, and FLDBASE should be >=8 .
 * Else pictures would be too dense, and drawing the frame would
 * fail, too, because the key type would not fit in anymore.
 */
#define	FLDBASE		8
#define	FLDSIZE_Y	(FLDBASE + 1)
#define	FLDSIZE_X	(FLDBASE * 2 + 1)
static char *
key_fingerprint_randomart(u_char *dgst_raw, u_int dgst_raw_len, const Key *k)
{
	/*
	 * Chars to be used after each other every time the worm
	 * intersects with itself.  Matter of taste.
	 */
	char	*augmentation_string = " .o+=*BOX@%&#/^SE";
	char	*retval, *p;
	u_char	 field[FLDSIZE_X][FLDSIZE_Y];
	u_int	 i, b;
	int	 x, y;
	size_t	 len = strlen(augmentation_string) - 1;

	retval = xcalloc(1, (FLDSIZE_X + 3) * (FLDSIZE_Y + 2));

	/* initialize field */
	memset(field, 0, FLDSIZE_X * FLDSIZE_Y * sizeof(char));
	x = FLDSIZE_X / 2;
	y = FLDSIZE_Y / 2;

	/* process raw key */
	for (i = 0; i < dgst_raw_len; i++) {
		int input;
		/* each byte conveys four 2-bit move commands */
		input = dgst_raw[i];
		for (b = 0; b < 4; b++) {
			/* evaluate 2 bit, rest is shifted later */
			x += (input & 0x1) ? 1 : -1;
			y += (input & 0x2) ? 1 : -1;

			/* assure we are still in bounds */
			x = MAX(x, 0);
			y = MAX(y, 0);
			x = MIN(x, FLDSIZE_X - 1);
			y = MIN(y, FLDSIZE_Y - 1);

			/* augment the field */
			if (field[x][y] < len - 2)
				field[x][y]++;
			input = input >> 2;
		}
	}

	/* mark starting point and end point*/
	field[FLDSIZE_X / 2][FLDSIZE_Y / 2] = len - 1;
	field[x][y] = len;

	/* fill in retval */
	snprintf(retval, FLDSIZE_X, "+--[%4s %4u]", key_type(k), key_size(k));
	p = strchr(retval, '\0');

	/* output upper border */
	for (i = p - retval - 1; i < FLDSIZE_X; i++)
		*p++ = '-';
	*p++ = '+';
	*p++ = '\n';

	/* output content */
	for (y = 0; y < FLDSIZE_Y; y++) {
		*p++ = '|';
		for (x = 0; x < FLDSIZE_X; x++)
			*p++ = augmentation_string[MIN(field[x][y], len)];
		*p++ = '|';
		*p++ = '\n';
	}

	/* output lower border */
	*p++ = '+';
	for (i = 0; i < FLDSIZE_X; i++)
		*p++ = '-';
	*p++ = '+';

	return retval;
}

char *
key_fingerprint(Key *k, enum fp_type dgst_type, enum fp_rep dgst_rep)
{
	char *retval = NULL;
	u_char *dgst_raw;
	u_int dgst_raw_len;

	dgst_raw = key_fingerprint_raw(k, dgst_type, &dgst_raw_len);
	if (!dgst_raw)
		fatal("key_fingerprint: null from key_fingerprint_raw()");
	switch (dgst_rep) {
	case SSH_FP_HEX:
		retval = key_fingerprint_hex(dgst_raw, dgst_raw_len);
		break;
	case SSH_FP_BUBBLEBABBLE:
		retval = key_fingerprint_bubblebabble(dgst_raw, dgst_raw_len);
		break;
	case SSH_FP_RANDOMART:
		retval = key_fingerprint_randomart(dgst_raw, dgst_raw_len, k);
		break;
	default:
		fatal("key_fingerprint: bad digest representation %d",
		    dgst_rep);
		break;
	}
	memset(dgst_raw, 0, dgst_raw_len);
	xfree(dgst_raw);
	return retval;
}

/*
 * Reads a multiple-precision integer in decimal from the buffer, and advances
 * the pointer.  The integer must already be initialized.  This function is
 * permitted to modify the buffer.  This leaves *cpp to point just beyond the
 * last processed (and maybe modified) character.  Note that this may modify
 * the buffer containing the number.
 */
static int
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

static int
write_bignum(FILE *f, BIGNUM *num)
{
	char *buf = BN_bn2dec(num);
	if (buf == NULL) {
		error("write_bignum: BN_bn2dec() failed");
		return 0;
	}
	fprintf(f, " %s", buf);
	OPENSSL_free(buf);
	return 1;
}

/* returns 1 ok, -1 error */
int
key_read(Key *ret, char **cpp)
{
	Key *k;
	int success = -1;
	char *cp, *space;
	int len, n, type;
	u_int bits;
	u_char *blob;
#ifdef OPENSSL_HAS_ECC
	int curve_nid = -1;
#endif

	cp = *cpp;

	switch (ret->type) {
	case KEY_RSA1:
		/* Get number of bits. */
		if (*cp < '0' || *cp > '9')
			return -1;	/* Bad bit count... */
		for (bits = 0; *cp >= '0' && *cp <= '9'; cp++)
			bits = 10 * bits + *cp - '0';
		if (bits == 0)
			return -1;
		*cpp = cp;
		/* Get public exponent, public modulus. */
		if (!read_bignum(cpp, ret->rsa->e))
			return -1;
		if (!read_bignum(cpp, ret->rsa->n))
			return -1;
		/* validate the claimed number of bits */
		if ((u_int)BN_num_bits(ret->rsa->n) != bits) {
			verbose("key_read: claimed key size %d does not match "
			   "actual %d", bits, BN_num_bits(ret->rsa->n));
			return -1;
		}
		success = 1;
		break;
	case KEY_UNSPEC:
	case KEY_RSA:
	case KEY_DSA:
	case KEY_ECDSA:
	case KEY_DSA_CERT_V00:
	case KEY_RSA_CERT_V00:
	case KEY_DSA_CERT:
	case KEY_ECDSA_CERT:
	case KEY_RSA_CERT:
		space = strchr(cp, ' ');
		if (space == NULL) {
			debug3("key_read: missing whitespace");
			return -1;
		}
		*space = '\0';
		type = key_type_from_name(cp);
#ifdef OPENSSL_HAS_ECC
		if (key_type_plain(type) == KEY_ECDSA &&
		    (curve_nid = key_ecdsa_nid_from_name(cp)) == -1) {
			debug("key_read: invalid curve");
			return -1;
		}
#endif
		*space = ' ';
		if (type == KEY_UNSPEC) {
			debug3("key_read: missing keytype");
			return -1;
		}
		cp = space+1;
		if (*cp == '\0') {
			debug3("key_read: short string");
			return -1;
		}
		if (ret->type == KEY_UNSPEC) {
			ret->type = type;
		} else if (ret->type != type) {
			/* is a key, but different type */
			debug3("key_read: type mismatch");
			return -1;
		}
		len = 2*strlen(cp);
		blob = xmalloc(len);
		n = uudecode(cp, blob, len);
		if (n < 0) {
			error("key_read: uudecode %s failed", cp);
			xfree(blob);
			return -1;
		}
		k = key_from_blob(blob, (u_int)n);
		xfree(blob);
		if (k == NULL) {
			error("key_read: key_from_blob %s failed", cp);
			return -1;
		}
		if (k->type != type) {
			error("key_read: type mismatch: encoding error");
			key_free(k);
			return -1;
		}
#ifdef OPENSSL_HAS_ECC
		if (key_type_plain(type) == KEY_ECDSA &&
		    curve_nid != k->ecdsa_nid) {
			error("key_read: type mismatch: EC curve mismatch");
			key_free(k);
			return -1;
		}
#endif
/*XXXX*/
		if (key_is_cert(ret)) {
			if (!key_is_cert(k)) {
				error("key_read: loaded key is not a cert");
				key_free(k);
				return -1;
			}
			if (ret->cert != NULL)
				cert_free(ret->cert);
			ret->cert = k->cert;
			k->cert = NULL;
		}
		if (key_type_plain(ret->type) == KEY_RSA) {
			if (ret->rsa != NULL)
				RSA_free(ret->rsa);
			ret->rsa = k->rsa;
			k->rsa = NULL;
#ifdef DEBUG_PK
			RSA_print_fp(stderr, ret->rsa, 8);
#endif
		}
		if (key_type_plain(ret->type) == KEY_DSA) {
			if (ret->dsa != NULL)
				DSA_free(ret->dsa);
			ret->dsa = k->dsa;
			k->dsa = NULL;
#ifdef DEBUG_PK
			DSA_print_fp(stderr, ret->dsa, 8);
#endif
		}
#ifdef OPENSSL_HAS_ECC
		if (key_type_plain(ret->type) == KEY_ECDSA) {
			if (ret->ecdsa != NULL)
				EC_KEY_free(ret->ecdsa);
			ret->ecdsa = k->ecdsa;
			ret->ecdsa_nid = k->ecdsa_nid;
			k->ecdsa = NULL;
			k->ecdsa_nid = -1;
#ifdef DEBUG_PK
			key_dump_ec_key(ret->ecdsa);
#endif
		}
#endif
		success = 1;
/*XXXX*/
		key_free(k);
		if (success != 1)
			break;
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
	return success;
}

int
key_write(const Key *key, FILE *f)
{
	int n, success = 0;
	u_int len, bits = 0;
	u_char *blob;
	char *uu;

	if (key_is_cert(key)) {
		if (key->cert == NULL) {
			error("%s: no cert data", __func__);
			return 0;
		}
		if (buffer_len(&key->cert->certblob) == 0) {
			error("%s: no signed certificate blob", __func__);
			return 0;
		}
	}

	switch (key->type) {
	case KEY_RSA1:
		if (key->rsa == NULL)
			return 0;
		/* size of modulus 'n' */
		bits = BN_num_bits(key->rsa->n);
		fprintf(f, "%u", bits);
		if (write_bignum(f, key->rsa->e) &&
		    write_bignum(f, key->rsa->n))
			return 1;
		error("key_write: failed for RSA key");
		return 0;
	case KEY_DSA:
	case KEY_DSA_CERT_V00:
	case KEY_DSA_CERT:
		if (key->dsa == NULL)
			return 0;
		break;
#ifdef OPENSSL_HAS_ECC
	case KEY_ECDSA:
	case KEY_ECDSA_CERT:
		if (key->ecdsa == NULL)
			return 0;
		break;
#endif
	case KEY_RSA:
	case KEY_RSA_CERT_V00:
	case KEY_RSA_CERT:
		if (key->rsa == NULL)
			return 0;
		break;
	default:
		return 0;
	}

	key_to_blob(key, &blob, &len);
	uu = xmalloc(2*len);
	n = uuencode(blob, len, uu, 2*len);
	if (n > 0) {
		fprintf(f, "%s %s", key_ssh_name(key), uu);
		success = 1;
	}
	xfree(blob);
	xfree(uu);

	return success;
}

const char *
key_type(const Key *k)
{
	switch (k->type) {
	case KEY_RSA1:
		return "RSA1";
	case KEY_RSA:
		return "RSA";
	case KEY_DSA:
		return "DSA";
#ifdef OPENSSL_HAS_ECC
	case KEY_ECDSA:
		return "ECDSA";
#endif
	case KEY_RSA_CERT_V00:
		return "RSA-CERT-V00";
	case KEY_DSA_CERT_V00:
		return "DSA-CERT-V00";
	case KEY_RSA_CERT:
		return "RSA-CERT";
	case KEY_DSA_CERT:
		return "DSA-CERT";
#ifdef OPENSSL_HAS_ECC
	case KEY_ECDSA_CERT:
		return "ECDSA-CERT";
#endif
	}
	return "unknown";
}

const char *
key_cert_type(const Key *k)
{
	switch (k->cert->type) {
	case SSH2_CERT_TYPE_USER:
		return "user";
	case SSH2_CERT_TYPE_HOST:
		return "host";
	default:
		return "unknown";
	}
}

static const char *
key_ssh_name_from_type_nid(int type, int nid)
{
	switch (type) {
	case KEY_RSA:
		return "ssh-rsa";
	case KEY_DSA:
		return "ssh-dss";
	case KEY_RSA_CERT_V00:
		return "ssh-rsa-cert-v00@openssh.com";
	case KEY_DSA_CERT_V00:
		return "ssh-dss-cert-v00@openssh.com";
	case KEY_RSA_CERT:
		return "ssh-rsa-cert-v01@openssh.com";
	case KEY_DSA_CERT:
		return "ssh-dss-cert-v01@openssh.com";
#ifdef OPENSSL_HAS_ECC
	case KEY_ECDSA:
		switch (nid) {
		case NID_X9_62_prime256v1:
			return "ecdsa-sha2-nistp256";
		case NID_secp384r1:
			return "ecdsa-sha2-nistp384";
		case NID_secp521r1:
			return "ecdsa-sha2-nistp521";
		default:
			break;
		}
		break;
	case KEY_ECDSA_CERT:
		switch (nid) {
		case NID_X9_62_prime256v1:
			return "ecdsa-sha2-nistp256-cert-v01@openssh.com";
		case NID_secp384r1:
			return "ecdsa-sha2-nistp384-cert-v01@openssh.com";
		case NID_secp521r1:
			return "ecdsa-sha2-nistp521-cert-v01@openssh.com";
		default:
			break;
		}
		break;
#endif /* OPENSSL_HAS_ECC */
	}
	return "ssh-unknown";
}

const char *
key_ssh_name(const Key *k)
{
	return key_ssh_name_from_type_nid(k->type, k->ecdsa_nid);
}

const char *
key_ssh_name_plain(const Key *k)
{
	return key_ssh_name_from_type_nid(key_type_plain(k->type),
	    k->ecdsa_nid);
}

u_int
key_size(const Key *k)
{
	switch (k->type) {
	case KEY_RSA1:
	case KEY_RSA:
	case KEY_RSA_CERT_V00:
	case KEY_RSA_CERT:
		return BN_num_bits(k->rsa->n);
	case KEY_DSA:
	case KEY_DSA_CERT_V00:
	case KEY_DSA_CERT:
		return BN_num_bits(k->dsa->p);
#ifdef OPENSSL_HAS_ECC
	case KEY_ECDSA:
	case KEY_ECDSA_CERT:
		return key_curve_nid_to_bits(k->ecdsa_nid);
#endif
	}
	return 0;
}

static RSA *
rsa_generate_private_key(u_int bits)
{
	RSA *private = RSA_new();
	BIGNUM *f4 = BN_new();

	if (private == NULL)
		fatal("%s: RSA_new failed", __func__);
	if (f4 == NULL)
		fatal("%s: BN_new failed", __func__);
	if (!BN_set_word(f4, RSA_F4))
		fatal("%s: BN_new failed", __func__);
	if (!RSA_generate_key_ex(private, bits, f4, NULL))
		fatal("%s: key generation failed.", __func__);
	BN_free(f4);
	return private;
}

static DSA*
dsa_generate_private_key(u_int bits)
{
	DSA *private = DSA_new();

	if (private == NULL)
		fatal("%s: DSA_new failed", __func__);
	if (!DSA_generate_parameters_ex(private, bits, NULL, 0, NULL,
	    NULL, NULL))
		fatal("%s: DSA_generate_parameters failed", __func__);
	if (!DSA_generate_key(private))
		fatal("%s: DSA_generate_key failed.", __func__);
	return private;
}

int
key_ecdsa_bits_to_nid(int bits)
{
	switch (bits) {
#ifdef OPENSSL_HAS_ECC
	case 256:
		return NID_X9_62_prime256v1;
	case 384:
		return NID_secp384r1;
	case 521:
		return NID_secp521r1;
#endif
	default:
		return -1;
	}
}

#ifdef OPENSSL_HAS_ECC
int
key_ecdsa_key_to_nid(EC_KEY *k)
{
	EC_GROUP *eg;
	int nids[] = {
		NID_X9_62_prime256v1,
		NID_secp384r1,
		NID_secp521r1,
		-1
	};
	int nid;
	u_int i;
	BN_CTX *bnctx;
	const EC_GROUP *g = EC_KEY_get0_group(k);

	/*
	 * The group may be stored in a ASN.1 encoded private key in one of two
	 * ways: as a "named group", which is reconstituted by ASN.1 object ID
	 * or explicit group parameters encoded into the key blob. Only the
	 * "named group" case sets the group NID for us, but we can figure
	 * it out for the other case by comparing against all the groups that
	 * are supported.
	 */
	if ((nid = EC_GROUP_get_curve_name(g)) > 0)
		return nid;
	if ((bnctx = BN_CTX_new()) == NULL)
		fatal("%s: BN_CTX_new() failed", __func__);
	for (i = 0; nids[i] != -1; i++) {
		if ((eg = EC_GROUP_new_by_curve_name(nids[i])) == NULL)
			fatal("%s: EC_GROUP_new_by_curve_name failed",
			    __func__);
		if (EC_GROUP_cmp(g, eg, bnctx) == 0)
			break;
		EC_GROUP_free(eg);
	}
	BN_CTX_free(bnctx);
	debug3("%s: nid = %d", __func__, nids[i]);
	if (nids[i] != -1) {
		/* Use the group with the NID attached */
		EC_GROUP_set_asn1_flag(eg, OPENSSL_EC_NAMED_CURVE);
		if (EC_KEY_set_group(k, eg) != 1)
			fatal("%s: EC_KEY_set_group", __func__);
	}
	return nids[i];
}

static EC_KEY*
ecdsa_generate_private_key(u_int bits, int *nid)
{
	EC_KEY *private;

	if ((*nid = key_ecdsa_bits_to_nid(bits)) == -1)
		fatal("%s: invalid key length", __func__);
	if ((private = EC_KEY_new_by_curve_name(*nid)) == NULL)
		fatal("%s: EC_KEY_new_by_curve_name failed", __func__);
	if (EC_KEY_generate_key(private) != 1)
		fatal("%s: EC_KEY_generate_key failed", __func__);
	EC_KEY_set_asn1_flag(private, OPENSSL_EC_NAMED_CURVE);
	return private;
}
#endif /* OPENSSL_HAS_ECC */

Key *
key_generate(int type, u_int bits)
{
	Key *k = key_new(KEY_UNSPEC);
	switch (type) {
	case KEY_DSA:
		k->dsa = dsa_generate_private_key(bits);
		break;
#ifdef OPENSSL_HAS_ECC
	case KEY_ECDSA:
		k->ecdsa = ecdsa_generate_private_key(bits, &k->ecdsa_nid);
		break;
#endif
	case KEY_RSA:
	case KEY_RSA1:
		k->rsa = rsa_generate_private_key(bits);
		break;
	case KEY_RSA_CERT_V00:
	case KEY_DSA_CERT_V00:
	case KEY_RSA_CERT:
	case KEY_DSA_CERT:
		fatal("key_generate: cert keys cannot be generated directly");
	default:
		fatal("key_generate: unknown type %d", type);
	}
	k->type = type;
	return k;
}

void
key_cert_copy(const Key *from_key, struct Key *to_key)
{
	u_int i;
	const struct KeyCert *from;
	struct KeyCert *to;

	if (to_key->cert != NULL) {
		cert_free(to_key->cert);
		to_key->cert = NULL;
	}

	if ((from = from_key->cert) == NULL)
		return;

	to = to_key->cert = cert_new();

	buffer_append(&to->certblob, buffer_ptr(&from->certblob),
	    buffer_len(&from->certblob));

	buffer_append(&to->critical,
	    buffer_ptr(&from->critical), buffer_len(&from->critical));
	buffer_append(&to->extensions,
	    buffer_ptr(&from->extensions), buffer_len(&from->extensions));

	to->serial = from->serial;
	to->type = from->type;
	to->key_id = from->key_id == NULL ? NULL : xstrdup(from->key_id);
	to->valid_after = from->valid_after;
	to->valid_before = from->valid_before;
	to->signature_key = from->signature_key == NULL ?
	    NULL : key_from_private(from->signature_key);

	to->nprincipals = from->nprincipals;
	if (to->nprincipals > CERT_MAX_PRINCIPALS)
		fatal("%s: nprincipals (%u) > CERT_MAX_PRINCIPALS (%u)",
		    __func__, to->nprincipals, CERT_MAX_PRINCIPALS);
	if (to->nprincipals > 0) {
		to->principals = xcalloc(from->nprincipals,
		    sizeof(*to->principals));
		for (i = 0; i < to->nprincipals; i++)
			to->principals[i] = xstrdup(from->principals[i]);
	}
}

Key *
key_from_private(const Key *k)
{
	Key *n = NULL;
	switch (k->type) {
	case KEY_DSA:
	case KEY_DSA_CERT_V00:
	case KEY_DSA_CERT:
		n = key_new(k->type);
		if ((BN_copy(n->dsa->p, k->dsa->p) == NULL) ||
		    (BN_copy(n->dsa->q, k->dsa->q) == NULL) ||
		    (BN_copy(n->dsa->g, k->dsa->g) == NULL) ||
		    (BN_copy(n->dsa->pub_key, k->dsa->pub_key) == NULL))
			fatal("key_from_private: BN_copy failed");
		break;
#ifdef OPENSSL_HAS_ECC
	case KEY_ECDSA:
	case KEY_ECDSA_CERT:
		n = key_new(k->type);
		n->ecdsa_nid = k->ecdsa_nid;
		if ((n->ecdsa = EC_KEY_new_by_curve_name(k->ecdsa_nid)) == NULL)
			fatal("%s: EC_KEY_new_by_curve_name failed", __func__);
		if (EC_KEY_set_public_key(n->ecdsa,
		    EC_KEY_get0_public_key(k->ecdsa)) != 1)
			fatal("%s: EC_KEY_set_public_key failed", __func__);
		break;
#endif
	case KEY_RSA:
	case KEY_RSA1:
	case KEY_RSA_CERT_V00:
	case KEY_RSA_CERT:
		n = key_new(k->type);
		if ((BN_copy(n->rsa->n, k->rsa->n) == NULL) ||
		    (BN_copy(n->rsa->e, k->rsa->e) == NULL))
			fatal("key_from_private: BN_copy failed");
		break;
	default:
		fatal("key_from_private: unknown type %d", k->type);
		break;
	}
	if (key_is_cert(k))
		key_cert_copy(k, n);
	return n;
}

int
key_type_from_name(char *name)
{
	if (strcmp(name, "rsa1") == 0) {
		return KEY_RSA1;
	} else if (strcmp(name, "rsa") == 0) {
		return KEY_RSA;
	} else if (strcmp(name, "dsa") == 0) {
		return KEY_DSA;
	} else if (strcmp(name, "ssh-rsa") == 0) {
		return KEY_RSA;
	} else if (strcmp(name, "ssh-dss") == 0) {
		return KEY_DSA;
#ifdef OPENSSL_HAS_ECC
	} else if (strcmp(name, "ecdsa") == 0 ||
	    strcmp(name, "ecdsa-sha2-nistp256") == 0 ||
	    strcmp(name, "ecdsa-sha2-nistp384") == 0 ||
	    strcmp(name, "ecdsa-sha2-nistp521") == 0) {
		return KEY_ECDSA;
#endif
	} else if (strcmp(name, "ssh-rsa-cert-v00@openssh.com") == 0) {
		return KEY_RSA_CERT_V00;
	} else if (strcmp(name, "ssh-dss-cert-v00@openssh.com") == 0) {
		return KEY_DSA_CERT_V00;
	} else if (strcmp(name, "ssh-rsa-cert-v01@openssh.com") == 0) {
		return KEY_RSA_CERT;
	} else if (strcmp(name, "ssh-dss-cert-v01@openssh.com") == 0) {
		return KEY_DSA_CERT;
#ifdef OPENSSL_HAS_ECC
	} else if (strcmp(name, "ecdsa-sha2-nistp256-cert-v01@openssh.com") == 0 ||
	    strcmp(name, "ecdsa-sha2-nistp384-cert-v01@openssh.com") == 0 ||
	    strcmp(name, "ecdsa-sha2-nistp521-cert-v01@openssh.com") == 0) {
		return KEY_ECDSA_CERT;
#endif
	}

	debug2("key_type_from_name: unknown key type '%s'", name);
	return KEY_UNSPEC;
}

int
key_ecdsa_nid_from_name(const char *name)
{
#ifdef OPENSSL_HAS_ECC
	if (strcmp(name, "ecdsa-sha2-nistp256") == 0 ||
	    strcmp(name, "ecdsa-sha2-nistp256-cert-v01@openssh.com") == 0)
		return NID_X9_62_prime256v1;
	if (strcmp(name, "ecdsa-sha2-nistp384") == 0 ||
	    strcmp(name, "ecdsa-sha2-nistp384-cert-v01@openssh.com") == 0)
		return NID_secp384r1;
	if (strcmp(name, "ecdsa-sha2-nistp521") == 0 ||
	    strcmp(name, "ecdsa-sha2-nistp521-cert-v01@openssh.com") == 0)
		return NID_secp521r1;
#endif /* OPENSSL_HAS_ECC */

	debug2("%s: unknown/non-ECDSA key type '%s'", __func__, name);
	return -1;
}

int
key_names_valid2(const char *names)
{
	char *s, *cp, *p;

	if (names == NULL || strcmp(names, "") == 0)
		return 0;
	s = cp = xstrdup(names);
	for ((p = strsep(&cp, ",")); p && *p != '\0';
	    (p = strsep(&cp, ","))) {
		switch (key_type_from_name(p)) {
		case KEY_RSA1:
		case KEY_UNSPEC:
			xfree(s);
			return 0;
		}
	}
	debug3("key names ok: [%s]", names);
	xfree(s);
	return 1;
}

static int
cert_parse(Buffer *b, Key *key, const u_char *blob, u_int blen)
{
	u_char *principals, *critical, *exts, *sig_key, *sig;
	u_int signed_len, plen, clen, sklen, slen, kidlen, elen;
	Buffer tmp;
	char *principal;
	int ret = -1;
	int v00 = key->type == KEY_DSA_CERT_V00 ||
	    key->type == KEY_RSA_CERT_V00;

	buffer_init(&tmp);

	/* Copy the entire key blob for verification and later serialisation */
	buffer_append(&key->cert->certblob, blob, blen);

	elen = 0; /* Not touched for v00 certs */
	principals = exts = critical = sig_key = sig = NULL;
	if ((!v00 && buffer_get_int64_ret(&key->cert->serial, b) != 0) ||
	    buffer_get_int_ret(&key->cert->type, b) != 0 ||
	    (key->cert->key_id = buffer_get_cstring_ret(b, &kidlen)) == NULL ||
	    (principals = buffer_get_string_ret(b, &plen)) == NULL ||
	    buffer_get_int64_ret(&key->cert->valid_after, b) != 0 ||
	    buffer_get_int64_ret(&key->cert->valid_before, b) != 0 ||
	    (critical = buffer_get_string_ret(b, &clen)) == NULL ||
	    (!v00 && (exts = buffer_get_string_ret(b, &elen)) == NULL) ||
	    (v00 && buffer_get_string_ptr_ret(b, NULL) == NULL) || /* nonce */
	    buffer_get_string_ptr_ret(b, NULL) == NULL || /* reserved */
	    (sig_key = buffer_get_string_ret(b, &sklen)) == NULL) {
		error("%s: parse error", __func__);
		goto out;
	}

	if (kidlen != strlen(key->cert->key_id)) {
		error("%s: key ID contains \\0 character", __func__);
		goto out;
	}

	/* Signature is left in the buffer so we can calculate this length */
	signed_len = buffer_len(&key->cert->certblob) - buffer_len(b);

	if ((sig = buffer_get_string_ret(b, &slen)) == NULL) {
		error("%s: parse error", __func__);
		goto out;
	}

	if (key->cert->type != SSH2_CERT_TYPE_USER &&
	    key->cert->type != SSH2_CERT_TYPE_HOST) {
		error("Unknown certificate type %u", key->cert->type);
		goto out;
	}

	buffer_append(&tmp, principals, plen);
	while (buffer_len(&tmp) > 0) {
		if (key->cert->nprincipals >= CERT_MAX_PRINCIPALS) {
			error("%s: Too many principals", __func__);
			goto out;
		}
		if ((principal = buffer_get_cstring_ret(&tmp, &plen)) == NULL) {
			error("%s: Principals data invalid", __func__);
			goto out;
		}
		key->cert->principals = xrealloc(key->cert->principals,
		    key->cert->nprincipals + 1, sizeof(*key->cert->principals));
		key->cert->principals[key->cert->nprincipals++] = principal;
	}

	buffer_clear(&tmp);

	buffer_append(&key->cert->critical, critical, clen);
	buffer_append(&tmp, critical, clen);
	/* validate structure */
	while (buffer_len(&tmp) != 0) {
		if (buffer_get_string_ptr_ret(&tmp, NULL) == NULL ||
		    buffer_get_string_ptr_ret(&tmp, NULL) == NULL) {
			error("%s: critical option data invalid", __func__);
			goto out;
		}
	}
	buffer_clear(&tmp);

	buffer_append(&key->cert->extensions, exts, elen);
	buffer_append(&tmp, exts, elen);
	/* validate structure */
	while (buffer_len(&tmp) != 0) {
		if (buffer_get_string_ptr_ret(&tmp, NULL) == NULL ||
		    buffer_get_string_ptr_ret(&tmp, NULL) == NULL) {
			error("%s: extension data invalid", __func__);
			goto out;
		}
	}
	buffer_clear(&tmp);

	if ((key->cert->signature_key = key_from_blob(sig_key,
	    sklen)) == NULL) {
		error("%s: Signature key invalid", __func__);
		goto out;
	}
	if (key->cert->signature_key->type != KEY_RSA &&
	    key->cert->signature_key->type != KEY_DSA &&
	    key->cert->signature_key->type != KEY_ECDSA) {
		error("%s: Invalid signature key type %s (%d)", __func__,
		    key_type(key->cert->signature_key),
		    key->cert->signature_key->type);
		goto out;
	}

	switch (key_verify(key->cert->signature_key, sig, slen, 
	    buffer_ptr(&key->cert->certblob), signed_len)) {
	case 1:
		ret = 0;
		break; /* Good signature */
	case 0:
		error("%s: Invalid signature on certificate", __func__);
		goto out;
	case -1:
		error("%s: Certificate signature verification failed",
		    __func__);
		goto out;
	}

 out:
	buffer_free(&tmp);
	if (principals != NULL)
		xfree(principals);
	if (critical != NULL)
		xfree(critical);
	if (exts != NULL)
		xfree(exts);
	if (sig_key != NULL)
		xfree(sig_key);
	if (sig != NULL)
		xfree(sig);
	return ret;
}

Key *
key_from_blob(const u_char *blob, u_int blen)
{
	Buffer b;
	int rlen, type;
	char *ktype = NULL, *curve = NULL;
	Key *key = NULL;
#ifdef OPENSSL_HAS_ECC
	EC_POINT *q = NULL;
	int nid = -1;
#endif

#ifdef DEBUG_PK
	dump_base64(stderr, blob, blen);
#endif
	buffer_init(&b);
	buffer_append(&b, blob, blen);
	if ((ktype = buffer_get_cstring_ret(&b, NULL)) == NULL) {
		error("key_from_blob: can't read key type");
		goto out;
	}

	type = key_type_from_name(ktype);
#ifdef OPENSSL_HAS_ECC
	if (key_type_plain(type) == KEY_ECDSA)
		nid = key_ecdsa_nid_from_name(ktype);
#endif

	switch (type) {
	case KEY_RSA_CERT:
		(void)buffer_get_string_ptr_ret(&b, NULL); /* Skip nonce */
		/* FALLTHROUGH */
	case KEY_RSA:
	case KEY_RSA_CERT_V00:
		key = key_new(type);
		if (buffer_get_bignum2_ret(&b, key->rsa->e) == -1 ||
		    buffer_get_bignum2_ret(&b, key->rsa->n) == -1) {
			error("key_from_blob: can't read rsa key");
 badkey:
			key_free(key);
			key = NULL;
			goto out;
		}
#ifdef DEBUG_PK
		RSA_print_fp(stderr, key->rsa, 8);
#endif
		break;
	case KEY_DSA_CERT:
		(void)buffer_get_string_ptr_ret(&b, NULL); /* Skip nonce */
		/* FALLTHROUGH */
	case KEY_DSA:
	case KEY_DSA_CERT_V00:
		key = key_new(type);
		if (buffer_get_bignum2_ret(&b, key->dsa->p) == -1 ||
		    buffer_get_bignum2_ret(&b, key->dsa->q) == -1 ||
		    buffer_get_bignum2_ret(&b, key->dsa->g) == -1 ||
		    buffer_get_bignum2_ret(&b, key->dsa->pub_key) == -1) {
			error("key_from_blob: can't read dsa key");
			goto badkey;
		}
#ifdef DEBUG_PK
		DSA_print_fp(stderr, key->dsa, 8);
#endif
		break;
#ifdef OPENSSL_HAS_ECC
	case KEY_ECDSA_CERT:
		(void)buffer_get_string_ptr_ret(&b, NULL); /* Skip nonce */
		/* FALLTHROUGH */
	case KEY_ECDSA:
		key = key_new(type);
		key->ecdsa_nid = nid;
		if ((curve = buffer_get_string_ret(&b, NULL)) == NULL) {
			error("key_from_blob: can't read ecdsa curve");
			goto badkey;
		}
		if (key->ecdsa_nid != key_curve_name_to_nid(curve)) {
			error("key_from_blob: ecdsa curve doesn't match type");
			goto badkey;
		}
		if (key->ecdsa != NULL)
			EC_KEY_free(key->ecdsa);
		if ((key->ecdsa = EC_KEY_new_by_curve_name(key->ecdsa_nid))
		    == NULL)
			fatal("key_from_blob: EC_KEY_new_by_curve_name failed");
		if ((q = EC_POINT_new(EC_KEY_get0_group(key->ecdsa))) == NULL)
			fatal("key_from_blob: EC_POINT_new failed");
		if (buffer_get_ecpoint_ret(&b, EC_KEY_get0_group(key->ecdsa),
		    q) == -1) {
			error("key_from_blob: can't read ecdsa key point");
			goto badkey;
		}
		if (key_ec_validate_public(EC_KEY_get0_group(key->ecdsa),
		    q) != 0)
			goto badkey;
		if (EC_KEY_set_public_key(key->ecdsa, q) != 1)
			fatal("key_from_blob: EC_KEY_set_public_key failed");
#ifdef DEBUG_PK
		key_dump_ec_point(EC_KEY_get0_group(key->ecdsa), q);
#endif
		break;
#endif /* OPENSSL_HAS_ECC */
	case KEY_UNSPEC:
		key = key_new(type);
		break;
	default:
		error("key_from_blob: cannot handle type %s", ktype);
		goto out;
	}
	if (key_is_cert(key) && cert_parse(&b, key, blob, blen) == -1) {
		error("key_from_blob: can't parse cert data");
		goto badkey;
	}
	rlen = buffer_len(&b);
	if (key != NULL && rlen != 0)
		error("key_from_blob: remaining bytes in key blob %d", rlen);
 out:
	if (ktype != NULL)
		xfree(ktype);
	if (curve != NULL)
		xfree(curve);
#ifdef OPENSSL_HAS_ECC
	if (q != NULL)
		EC_POINT_free(q);
#endif
	buffer_free(&b);
	return key;
}

int
key_to_blob(const Key *key, u_char **blobp, u_int *lenp)
{
	Buffer b;
	int len;

	if (key == NULL) {
		error("key_to_blob: key == NULL");
		return 0;
	}
	buffer_init(&b);
	switch (key->type) {
	case KEY_DSA_CERT_V00:
	case KEY_RSA_CERT_V00:
	case KEY_DSA_CERT:
	case KEY_ECDSA_CERT:
	case KEY_RSA_CERT:
		/* Use the existing blob */
		buffer_append(&b, buffer_ptr(&key->cert->certblob),
		    buffer_len(&key->cert->certblob));
		break;
	case KEY_DSA:
		buffer_put_cstring(&b, key_ssh_name(key));
		buffer_put_bignum2(&b, key->dsa->p);
		buffer_put_bignum2(&b, key->dsa->q);
		buffer_put_bignum2(&b, key->dsa->g);
		buffer_put_bignum2(&b, key->dsa->pub_key);
		break;
#ifdef OPENSSL_HAS_ECC
	case KEY_ECDSA:
		buffer_put_cstring(&b, key_ssh_name(key));
		buffer_put_cstring(&b, key_curve_nid_to_name(key->ecdsa_nid));
		buffer_put_ecpoint(&b, EC_KEY_get0_group(key->ecdsa),
		    EC_KEY_get0_public_key(key->ecdsa));
		break;
#endif
	case KEY_RSA:
		buffer_put_cstring(&b, key_ssh_name(key));
		buffer_put_bignum2(&b, key->rsa->e);
		buffer_put_bignum2(&b, key->rsa->n);
		break;
	default:
		error("key_to_blob: unsupported key type %d", key->type);
		buffer_free(&b);
		return 0;
	}
	len = buffer_len(&b);
	if (lenp != NULL)
		*lenp = len;
	if (blobp != NULL) {
		*blobp = xmalloc(len);
		memcpy(*blobp, buffer_ptr(&b), len);
	}
	memset(buffer_ptr(&b), 0, len);
	buffer_free(&b);
	return len;
}

int
key_sign(
    const Key *key,
    u_char **sigp, u_int *lenp,
    const u_char *data, u_int datalen)
{
	switch (key->type) {
	case KEY_DSA_CERT_V00:
	case KEY_DSA_CERT:
	case KEY_DSA:
		return ssh_dss_sign(key, sigp, lenp, data, datalen);
#ifdef OPENSSL_HAS_ECC
	case KEY_ECDSA_CERT:
	case KEY_ECDSA:
		return ssh_ecdsa_sign(key, sigp, lenp, data, datalen);
#endif
	case KEY_RSA_CERT_V00:
	case KEY_RSA_CERT:
	case KEY_RSA:
		return ssh_rsa_sign(key, sigp, lenp, data, datalen);
	default:
		error("key_sign: invalid key type %d", key->type);
		return -1;
	}
}

/*
 * key_verify returns 1 for a correct signature, 0 for an incorrect signature
 * and -1 on error.
 */
int
key_verify(
    const Key *key,
    const u_char *signature, u_int signaturelen,
    const u_char *data, u_int datalen)
{
	if (signaturelen == 0)
		return -1;

	switch (key->type) {
	case KEY_DSA_CERT_V00:
	case KEY_DSA_CERT:
	case KEY_DSA:
		return ssh_dss_verify(key, signature, signaturelen, data, datalen);
#ifdef OPENSSL_HAS_ECC
	case KEY_ECDSA_CERT:
	case KEY_ECDSA:
		return ssh_ecdsa_verify(key, signature, signaturelen, data, datalen);
#endif
	case KEY_RSA_CERT_V00:
	case KEY_RSA_CERT:
	case KEY_RSA:
		return ssh_rsa_verify(key, signature, signaturelen, data, datalen);
	default:
		error("key_verify: invalid key type %d", key->type);
		return -1;
	}
}

/* Converts a private to a public key */
Key *
key_demote(const Key *k)
{
	Key *pk;

	pk = xcalloc(1, sizeof(*pk));
	pk->type = k->type;
	pk->flags = k->flags;
	pk->ecdsa_nid = k->ecdsa_nid;
	pk->dsa = NULL;
	pk->ecdsa = NULL;
	pk->rsa = NULL;

	switch (k->type) {
	case KEY_RSA_CERT_V00:
	case KEY_RSA_CERT:
		key_cert_copy(k, pk);
		/* FALLTHROUGH */
	case KEY_RSA1:
	case KEY_RSA:
		if ((pk->rsa = RSA_new()) == NULL)
			fatal("key_demote: RSA_new failed");
		if ((pk->rsa->e = BN_dup(k->rsa->e)) == NULL)
			fatal("key_demote: BN_dup failed");
		if ((pk->rsa->n = BN_dup(k->rsa->n)) == NULL)
			fatal("key_demote: BN_dup failed");
		break;
	case KEY_DSA_CERT_V00:
	case KEY_DSA_CERT:
		key_cert_copy(k, pk);
		/* FALLTHROUGH */
	case KEY_DSA:
		if ((pk->dsa = DSA_new()) == NULL)
			fatal("key_demote: DSA_new failed");
		if ((pk->dsa->p = BN_dup(k->dsa->p)) == NULL)
			fatal("key_demote: BN_dup failed");
		if ((pk->dsa->q = BN_dup(k->dsa->q)) == NULL)
			fatal("key_demote: BN_dup failed");
		if ((pk->dsa->g = BN_dup(k->dsa->g)) == NULL)
			fatal("key_demote: BN_dup failed");
		if ((pk->dsa->pub_key = BN_dup(k->dsa->pub_key)) == NULL)
			fatal("key_demote: BN_dup failed");
		break;
#ifdef OPENSSL_HAS_ECC
	case KEY_ECDSA_CERT:
		key_cert_copy(k, pk);
		/* FALLTHROUGH */
	case KEY_ECDSA:
		if ((pk->ecdsa = EC_KEY_new_by_curve_name(pk->ecdsa_nid)) == NULL)
			fatal("key_demote: EC_KEY_new_by_curve_name failed");
		if (EC_KEY_set_public_key(pk->ecdsa,
		    EC_KEY_get0_public_key(k->ecdsa)) != 1)
			fatal("key_demote: EC_KEY_set_public_key failed");
		break;
#endif
	default:
		fatal("key_free: bad key type %d", k->type);
		break;
	}

	return (pk);
}

int
key_is_cert(const Key *k)
{
	if (k == NULL)
		return 0;
	switch (k->type) {
	case KEY_RSA_CERT_V00:
	case KEY_DSA_CERT_V00:
	case KEY_RSA_CERT:
	case KEY_DSA_CERT:
	case KEY_ECDSA_CERT:
		return 1;
	default:
		return 0;
	}
}

/* Return the cert-less equivalent to a certified key type */
int
key_type_plain(int type)
{
	switch (type) {
	case KEY_RSA_CERT_V00:
	case KEY_RSA_CERT:
		return KEY_RSA;
	case KEY_DSA_CERT_V00:
	case KEY_DSA_CERT:
		return KEY_DSA;
	case KEY_ECDSA_CERT:
		return KEY_ECDSA;
	default:
		return type;
	}
}

/* Convert a KEY_RSA or KEY_DSA to their _CERT equivalent */
int
key_to_certified(Key *k, int legacy)
{
	switch (k->type) {
	case KEY_RSA:
		k->cert = cert_new();
		k->type = legacy ? KEY_RSA_CERT_V00 : KEY_RSA_CERT;
		return 0;
	case KEY_DSA:
		k->cert = cert_new();
		k->type = legacy ? KEY_DSA_CERT_V00 : KEY_DSA_CERT;
		return 0;
	case KEY_ECDSA:
		k->cert = cert_new();
		k->type = KEY_ECDSA_CERT;
		return 0;
	default:
		error("%s: key has incorrect type %s", __func__, key_type(k));
		return -1;
	}
}

/* Convert a KEY_RSA_CERT or KEY_DSA_CERT to their raw key equivalent */
int
key_drop_cert(Key *k)
{
	switch (k->type) {
	case KEY_RSA_CERT_V00:
	case KEY_RSA_CERT:
		cert_free(k->cert);
		k->type = KEY_RSA;
		return 0;
	case KEY_DSA_CERT_V00:
	case KEY_DSA_CERT:
		cert_free(k->cert);
		k->type = KEY_DSA;
		return 0;
	case KEY_ECDSA_CERT:
		cert_free(k->cert);
		k->type = KEY_ECDSA;
		return 0;
	default:
		error("%s: key has incorrect type %s", __func__, key_type(k));
		return -1;
	}
}

/*
 * Sign a KEY_RSA_CERT, KEY_DSA_CERT or KEY_ECDSA_CERT, (re-)generating
 * the signed certblob
 */
int
key_certify(Key *k, Key *ca)
{
	Buffer principals;
	u_char *ca_blob, *sig_blob, nonce[32];
	u_int i, ca_len, sig_len;

	if (k->cert == NULL) {
		error("%s: key lacks cert info", __func__);
		return -1;
	}

	if (!key_is_cert(k)) {
		error("%s: certificate has unknown type %d", __func__,
		    k->cert->type);
		return -1;
	}

	if (ca->type != KEY_RSA && ca->type != KEY_DSA &&
	    ca->type != KEY_ECDSA) {
		error("%s: CA key has unsupported type %s", __func__,
		    key_type(ca));
		return -1;
	}

	key_to_blob(ca, &ca_blob, &ca_len);

	buffer_clear(&k->cert->certblob);
	buffer_put_cstring(&k->cert->certblob, key_ssh_name(k));

	/* -v01 certs put nonce first */
	arc4random_buf(&nonce, sizeof(nonce));
	if (!key_cert_is_legacy(k))
		buffer_put_string(&k->cert->certblob, nonce, sizeof(nonce));

	switch (k->type) {
	case KEY_DSA_CERT_V00:
	case KEY_DSA_CERT:
		buffer_put_bignum2(&k->cert->certblob, k->dsa->p);
		buffer_put_bignum2(&k->cert->certblob, k->dsa->q);
		buffer_put_bignum2(&k->cert->certblob, k->dsa->g);
		buffer_put_bignum2(&k->cert->certblob, k->dsa->pub_key);
		break;
#ifdef OPENSSL_HAS_ECC
	case KEY_ECDSA_CERT:
		buffer_put_cstring(&k->cert->certblob,
		    key_curve_nid_to_name(k->ecdsa_nid));
		buffer_put_ecpoint(&k->cert->certblob,
		    EC_KEY_get0_group(k->ecdsa),
		    EC_KEY_get0_public_key(k->ecdsa));
		break;
#endif
	case KEY_RSA_CERT_V00:
	case KEY_RSA_CERT:
		buffer_put_bignum2(&k->cert->certblob, k->rsa->e);
		buffer_put_bignum2(&k->cert->certblob, k->rsa->n);
		break;
	default:
		error("%s: key has incorrect type %s", __func__, key_type(k));
		buffer_clear(&k->cert->certblob);
		xfree(ca_blob);
		return -1;
	}

	/* -v01 certs have a serial number next */
	if (!key_cert_is_legacy(k))
		buffer_put_int64(&k->cert->certblob, k->cert->serial);

	buffer_put_int(&k->cert->certblob, k->cert->type);
	buffer_put_cstring(&k->cert->certblob, k->cert->key_id);

	buffer_init(&principals);
	for (i = 0; i < k->cert->nprincipals; i++)
		buffer_put_cstring(&principals, k->cert->principals[i]);
	buffer_put_string(&k->cert->certblob, buffer_ptr(&principals),
	    buffer_len(&principals));
	buffer_free(&principals);

	buffer_put_int64(&k->cert->certblob, k->cert->valid_after);
	buffer_put_int64(&k->cert->certblob, k->cert->valid_before);
	buffer_put_string(&k->cert->certblob,
	    buffer_ptr(&k->cert->critical), buffer_len(&k->cert->critical));

	/* -v01 certs have non-critical options here */
	if (!key_cert_is_legacy(k)) {
		buffer_put_string(&k->cert->certblob,
		    buffer_ptr(&k->cert->extensions),
		    buffer_len(&k->cert->extensions));
	}

	/* -v00 certs put the nonce at the end */
	if (key_cert_is_legacy(k))
		buffer_put_string(&k->cert->certblob, nonce, sizeof(nonce));

	buffer_put_string(&k->cert->certblob, NULL, 0); /* reserved */
	buffer_put_string(&k->cert->certblob, ca_blob, ca_len);
	xfree(ca_blob);

	/* Sign the whole mess */
	if (key_sign(ca, &sig_blob, &sig_len, buffer_ptr(&k->cert->certblob),
	    buffer_len(&k->cert->certblob)) != 0) {
		error("%s: signature operation failed", __func__);
		buffer_clear(&k->cert->certblob);
		return -1;
	}
	/* Append signature and we are done */
	buffer_put_string(&k->cert->certblob, sig_blob, sig_len);
	xfree(sig_blob);

	return 0;
}

int
key_cert_check_authority(const Key *k, int want_host, int require_principal,
    const char *name, const char **reason)
{
	u_int i, principal_matches;
	time_t now = time(NULL);

	if (want_host) {
		if (k->cert->type != SSH2_CERT_TYPE_HOST) {
			*reason = "Certificate invalid: not a host certificate";
			return -1;
		}
	} else {
		if (k->cert->type != SSH2_CERT_TYPE_USER) {
			*reason = "Certificate invalid: not a user certificate";
			return -1;
		}
	}
	if (now < 0) {
		error("%s: system clock lies before epoch", __func__);
		*reason = "Certificate invalid: not yet valid";
		return -1;
	}
	if ((u_int64_t)now < k->cert->valid_after) {
		*reason = "Certificate invalid: not yet valid";
		return -1;
	}
	if ((u_int64_t)now >= k->cert->valid_before) {
		*reason = "Certificate invalid: expired";
		return -1;
	}
	if (k->cert->nprincipals == 0) {
		if (require_principal) {
			*reason = "Certificate lacks principal list";
			return -1;
		}
	} else if (name != NULL) {
		principal_matches = 0;
		for (i = 0; i < k->cert->nprincipals; i++) {
			if (strcmp(name, k->cert->principals[i]) == 0) {
				principal_matches = 1;
				break;
			}
		}
		if (!principal_matches) {
			*reason = "Certificate invalid: name is not a listed "
			    "principal";
			return -1;
		}
	}
	return 0;
}

int
key_cert_is_legacy(Key *k)
{
	switch (k->type) {
	case KEY_DSA_CERT_V00:
	case KEY_RSA_CERT_V00:
		return 1;
	default:
		return 0;
	}
}

/* XXX: these are really begging for a table-driven approach */
int
key_curve_name_to_nid(const char *name)
{
#ifdef OPENSSL_HAS_ECC
	if (strcmp(name, "nistp256") == 0)
		return NID_X9_62_prime256v1;
	else if (strcmp(name, "nistp384") == 0)
		return NID_secp384r1;
	else if (strcmp(name, "nistp521") == 0)
		return NID_secp521r1;
#endif

	debug("%s: unsupported EC curve name \"%.100s\"", __func__, name);
	return -1;
}

u_int
key_curve_nid_to_bits(int nid)
{
	switch (nid) {
#ifdef OPENSSL_HAS_ECC
	case NID_X9_62_prime256v1:
		return 256;
	case NID_secp384r1:
		return 384;
	case NID_secp521r1:
		return 521;
#endif
	default:
		error("%s: unsupported EC curve nid %d", __func__, nid);
		return 0;
	}
}

const char *
key_curve_nid_to_name(int nid)
{
#ifdef OPENSSL_HAS_ECC
	if (nid == NID_X9_62_prime256v1)
		return "nistp256";
	else if (nid == NID_secp384r1)
		return "nistp384";
	else if (nid == NID_secp521r1)
		return "nistp521";
#endif
	error("%s: unsupported EC curve nid %d", __func__, nid);
	return NULL;
}

#ifdef OPENSSL_HAS_ECC
const EVP_MD *
key_ec_nid_to_evpmd(int nid)
{
	int kbits = key_curve_nid_to_bits(nid);

	if (kbits == 0)
		fatal("%s: invalid nid %d", __func__, nid);
	/* RFC5656 section 6.2.1 */
	if (kbits <= 256)
		return EVP_sha256();
	else if (kbits <= 384)
		return EVP_sha384();
	else
		return EVP_sha512();
}

int
key_ec_validate_public(const EC_GROUP *group, const EC_POINT *public)
{
	BN_CTX *bnctx;
	EC_POINT *nq = NULL;
	BIGNUM *order, *x, *y, *tmp;
	int ret = -1;

	if ((bnctx = BN_CTX_new()) == NULL)
		fatal("%s: BN_CTX_new failed", __func__);
	BN_CTX_start(bnctx);

	/*
	 * We shouldn't ever hit this case because bignum_get_ecpoint()
	 * refuses to load GF2m points.
	 */
	if (EC_METHOD_get_field_type(EC_GROUP_method_of(group)) !=
	    NID_X9_62_prime_field) {
		error("%s: group is not a prime field", __func__);
		goto out;
	}

	/* Q != infinity */
	if (EC_POINT_is_at_infinity(group, public)) {
		error("%s: received degenerate public key (infinity)",
		    __func__);
		goto out;
	}

	if ((x = BN_CTX_get(bnctx)) == NULL ||
	    (y = BN_CTX_get(bnctx)) == NULL ||
	    (order = BN_CTX_get(bnctx)) == NULL ||
	    (tmp = BN_CTX_get(bnctx)) == NULL)
		fatal("%s: BN_CTX_get failed", __func__);

	/* log2(x) > log2(order)/2, log2(y) > log2(order)/2 */
	if (EC_GROUP_get_order(group, order, bnctx) != 1)
		fatal("%s: EC_GROUP_get_order failed", __func__);
	if (EC_POINT_get_affine_coordinates_GFp(group, public,
	    x, y, bnctx) != 1)
		fatal("%s: EC_POINT_get_affine_coordinates_GFp", __func__);
	if (BN_num_bits(x) <= BN_num_bits(order) / 2) {
		error("%s: public key x coordinate too small: "
		    "bits(x) = %d, bits(order)/2 = %d", __func__,
		    BN_num_bits(x), BN_num_bits(order) / 2);
		goto out;
	}
	if (BN_num_bits(y) <= BN_num_bits(order) / 2) {
		error("%s: public key y coordinate too small: "
		    "bits(y) = %d, bits(order)/2 = %d", __func__,
		    BN_num_bits(x), BN_num_bits(order) / 2);
		goto out;
	}

	/* nQ == infinity (n == order of subgroup) */
	if ((nq = EC_POINT_new(group)) == NULL)
		fatal("%s: BN_CTX_tmp failed", __func__);
	if (EC_POINT_mul(group, nq, NULL, public, order, bnctx) != 1)
		fatal("%s: EC_GROUP_mul failed", __func__);
	if (EC_POINT_is_at_infinity(group, nq) != 1) {
		error("%s: received degenerate public key (nQ != infinity)",
		    __func__);
		goto out;
	}

	/* x < order - 1, y < order - 1 */
	if (!BN_sub(tmp, order, BN_value_one()))
		fatal("%s: BN_sub failed", __func__);
	if (BN_cmp(x, tmp) >= 0) {
		error("%s: public key x coordinate >= group order - 1",
		    __func__);
		goto out;
	}
	if (BN_cmp(y, tmp) >= 0) {
		error("%s: public key y coordinate >= group order - 1",
		    __func__);
		goto out;
	}
	ret = 0;
 out:
	BN_CTX_free(bnctx);
	EC_POINT_free(nq);
	return ret;
}

int
key_ec_validate_private(const EC_KEY *key)
{
	BN_CTX *bnctx;
	BIGNUM *order, *tmp;
	int ret = -1;

	if ((bnctx = BN_CTX_new()) == NULL)
		fatal("%s: BN_CTX_new failed", __func__);
	BN_CTX_start(bnctx);

	if ((order = BN_CTX_get(bnctx)) == NULL ||
	    (tmp = BN_CTX_get(bnctx)) == NULL)
		fatal("%s: BN_CTX_get failed", __func__);

	/* log2(private) > log2(order)/2 */
	if (EC_GROUP_get_order(EC_KEY_get0_group(key), order, bnctx) != 1)
		fatal("%s: EC_GROUP_get_order failed", __func__);
	if (BN_num_bits(EC_KEY_get0_private_key(key)) <=
	    BN_num_bits(order) / 2) {
		error("%s: private key too small: "
		    "bits(y) = %d, bits(order)/2 = %d", __func__,
		    BN_num_bits(EC_KEY_get0_private_key(key)),
		    BN_num_bits(order) / 2);
		goto out;
	}

	/* private < order - 1 */
	if (!BN_sub(tmp, order, BN_value_one()))
		fatal("%s: BN_sub failed", __func__);
	if (BN_cmp(EC_KEY_get0_private_key(key), tmp) >= 0) {
		error("%s: private key >= group order - 1", __func__);
		goto out;
	}
	ret = 0;
 out:
	BN_CTX_free(bnctx);
	return ret;
}

#if defined(DEBUG_KEXECDH) || defined(DEBUG_PK)
void
key_dump_ec_point(const EC_GROUP *group, const EC_POINT *point)
{
	BIGNUM *x, *y;
	BN_CTX *bnctx;

	if (point == NULL) {
		fputs("point=(NULL)\n", stderr);
		return;
	}
	if ((bnctx = BN_CTX_new()) == NULL)
		fatal("%s: BN_CTX_new failed", __func__);
	BN_CTX_start(bnctx);
	if ((x = BN_CTX_get(bnctx)) == NULL || (y = BN_CTX_get(bnctx)) == NULL)
		fatal("%s: BN_CTX_get failed", __func__);
	if (EC_METHOD_get_field_type(EC_GROUP_method_of(group)) !=
	    NID_X9_62_prime_field)
		fatal("%s: group is not a prime field", __func__);
	if (EC_POINT_get_affine_coordinates_GFp(group, point, x, y, bnctx) != 1)
		fatal("%s: EC_POINT_get_affine_coordinates_GFp", __func__);
	fputs("x=", stderr);
	BN_print_fp(stderr, x);
	fputs("\ny=", stderr);
	BN_print_fp(stderr, y);
	fputs("\n", stderr);
	BN_CTX_free(bnctx);
}

void
key_dump_ec_key(const EC_KEY *key)
{
	const BIGNUM *exponent;

	key_dump_ec_point(EC_KEY_get0_group(key), EC_KEY_get0_public_key(key));
	fputs("exponent=", stderr);
	if ((exponent = EC_KEY_get0_private_key(key)) == NULL)
		fputs("(NULL)", stderr);
	else
		BN_print_fp(stderr, EC_KEY_get0_private_key(key));
	fputs("\n", stderr);
}
#endif /* defined(DEBUG_KEXECDH) || defined(DEBUG_PK) */
#endif /* OPENSSL_HAS_ECC */
