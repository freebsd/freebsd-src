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
RCSID("$OpenBSD: key.c,v 1.55 2003/11/10 16:23:41 jakob Exp $");

#include <openssl/evp.h>

#include "xmalloc.h"
#include "key.h"
#include "rsa.h"
#include "uuencode.h"
#include "buffer.h"
#include "bufaux.h"
#include "log.h"

Key *
key_new(int type)
{
	Key *k;
	RSA *rsa;
	DSA *dsa;
	k = xmalloc(sizeof(*k));
	k->type = type;
	k->flags = 0;
	k->dsa = NULL;
	k->rsa = NULL;
	switch (k->type) {
	case KEY_RSA1:
	case KEY_RSA:
		if ((rsa = RSA_new()) == NULL)
			fatal("key_new: RSA_new failed");
		if ((rsa->n = BN_new()) == NULL)
			fatal("key_new: BN_new failed");
		if ((rsa->e = BN_new()) == NULL)
			fatal("key_new: BN_new failed");
		k->rsa = rsa;
		break;
	case KEY_DSA:
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
	case KEY_UNSPEC:
		break;
	default:
		fatal("key_new: bad key type %d", k->type);
		break;
	}
	return k;
}

Key *
key_new_private(int type)
{
	Key *k = key_new(type);
	switch (k->type) {
	case KEY_RSA1:
	case KEY_RSA:
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
		if ((k->dsa->priv_key = BN_new()) == NULL)
			fatal("key_new_private: BN_new failed");
		break;
	case KEY_UNSPEC:
		break;
	default:
		break;
	}
	return k;
}

void
key_free(Key *k)
{
	switch (k->type) {
	case KEY_RSA1:
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
	case KEY_UNSPEC:
		break;
	default:
		fatal("key_free: bad key type %d", k->type);
		break;
	}
	xfree(k);
}

int
key_equal(const Key *a, const Key *b)
{
	if (a == NULL || b == NULL || a->type != b->type)
		return 0;
	switch (a->type) {
	case KEY_RSA1:
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

u_char*
key_fingerprint_raw(const Key *k, enum fp_type dgst_type,
    u_int *dgst_raw_length)
{
	const EVP_MD *md = NULL;
	EVP_MD_CTX ctx;
	u_char *blob = NULL;
	u_char *retval = NULL;
	u_int len = 0;
	int nlen, elen;

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
	case KEY_RSA:
		key_to_blob(k, &blob, &len);
		break;
	case KEY_UNSPEC:
		return retval;
		break;
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
	int i;

	retval = xmalloc(dgst_raw_len * 3 + 1);
	retval[0] = '\0';
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
	retval = xmalloc(sizeof(char) * (rounds*6));
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

char *
key_fingerprint(const Key *k, enum fp_type dgst_type, enum fp_rep dgst_rep)
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
	default:
		fatal("key_fingerprint_ex: bad digest representation %d",
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
		success = 1;
		break;
	case KEY_UNSPEC:
	case KEY_RSA:
	case KEY_DSA:
		space = strchr(cp, ' ');
		if (space == NULL) {
			debug3("key_read: missing whitespace");
			return -1;
		}
		*space = '\0';
		type = key_type_from_name(cp);
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
/*XXXX*/
		if (ret->type == KEY_RSA) {
			if (ret->rsa != NULL)
				RSA_free(ret->rsa);
			ret->rsa = k->rsa;
			k->rsa = NULL;
			success = 1;
#ifdef DEBUG_PK
			RSA_print_fp(stderr, ret->rsa, 8);
#endif
		} else {
			if (ret->dsa != NULL)
				DSA_free(ret->dsa);
			ret->dsa = k->dsa;
			k->dsa = NULL;
			success = 1;
#ifdef DEBUG_PK
			DSA_print_fp(stderr, ret->dsa, 8);
#endif
		}
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

	if (key->type == KEY_RSA1 && key->rsa != NULL) {
		/* size of modulus 'n' */
		bits = BN_num_bits(key->rsa->n);
		fprintf(f, "%u", bits);
		if (write_bignum(f, key->rsa->e) &&
		    write_bignum(f, key->rsa->n)) {
			success = 1;
		} else {
			error("key_write: failed for RSA key");
		}
	} else if ((key->type == KEY_DSA && key->dsa != NULL) ||
	    (key->type == KEY_RSA && key->rsa != NULL)) {
		key_to_blob(key, &blob, &len);
		uu = xmalloc(2*len);
		n = uuencode(blob, len, uu, 2*len);
		if (n > 0) {
			fprintf(f, "%s %s", key_ssh_name(key), uu);
			success = 1;
		}
		xfree(blob);
		xfree(uu);
	}
	return success;
}

const char *
key_type(const Key *k)
{
	switch (k->type) {
	case KEY_RSA1:
		return "RSA1";
		break;
	case KEY_RSA:
		return "RSA";
		break;
	case KEY_DSA:
		return "DSA";
		break;
	}
	return "unknown";
}

const char *
key_ssh_name(const Key *k)
{
	switch (k->type) {
	case KEY_RSA:
		return "ssh-rsa";
		break;
	case KEY_DSA:
		return "ssh-dss";
		break;
	}
	return "ssh-unknown";
}

u_int
key_size(const Key *k)
{
	switch (k->type) {
	case KEY_RSA1:
	case KEY_RSA:
		return BN_num_bits(k->rsa->n);
		break;
	case KEY_DSA:
		return BN_num_bits(k->dsa->p);
		break;
	}
	return 0;
}

static RSA *
rsa_generate_private_key(u_int bits)
{
	RSA *private;
	private = RSA_generate_key(bits, 35, NULL, NULL);
	if (private == NULL)
		fatal("rsa_generate_private_key: key generation failed.");
	return private;
}

static DSA*
dsa_generate_private_key(u_int bits)
{
	DSA *private = DSA_generate_parameters(bits, NULL, 0, NULL, NULL, NULL, NULL);
	if (private == NULL)
		fatal("dsa_generate_private_key: DSA_generate_parameters failed");
	if (!DSA_generate_key(private))
		fatal("dsa_generate_private_key: DSA_generate_key failed.");
	if (private == NULL)
		fatal("dsa_generate_private_key: NULL.");
	return private;
}

Key *
key_generate(int type, u_int bits)
{
	Key *k = key_new(KEY_UNSPEC);
	switch (type) {
	case KEY_DSA:
		k->dsa = dsa_generate_private_key(bits);
		break;
	case KEY_RSA:
	case KEY_RSA1:
		k->rsa = rsa_generate_private_key(bits);
		break;
	default:
		fatal("key_generate: unknown type %d", type);
	}
	k->type = type;
	return k;
}

Key *
key_from_private(const Key *k)
{
	Key *n = NULL;
	switch (k->type) {
	case KEY_DSA:
		n = key_new(k->type);
		BN_copy(n->dsa->p, k->dsa->p);
		BN_copy(n->dsa->q, k->dsa->q);
		BN_copy(n->dsa->g, k->dsa->g);
		BN_copy(n->dsa->pub_key, k->dsa->pub_key);
		break;
	case KEY_RSA:
	case KEY_RSA1:
		n = key_new(k->type);
		BN_copy(n->rsa->n, k->rsa->n);
		BN_copy(n->rsa->e, k->rsa->e);
		break;
	default:
		fatal("key_from_private: unknown type %d", k->type);
		break;
	}
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
	}
	debug2("key_type_from_name: unknown key type '%s'", name);
	return KEY_UNSPEC;
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

Key *
key_from_blob(const u_char *blob, u_int blen)
{
	Buffer b;
	char *ktype;
	int rlen, type;
	Key *key = NULL;

#ifdef DEBUG_PK
	dump_base64(stderr, blob, blen);
#endif
	buffer_init(&b);
	buffer_append(&b, blob, blen);
	ktype = buffer_get_string(&b, NULL);
	type = key_type_from_name(ktype);

	switch (type) {
	case KEY_RSA:
		key = key_new(type);
		buffer_get_bignum2(&b, key->rsa->e);
		buffer_get_bignum2(&b, key->rsa->n);
#ifdef DEBUG_PK
		RSA_print_fp(stderr, key->rsa, 8);
#endif
		break;
	case KEY_DSA:
		key = key_new(type);
		buffer_get_bignum2(&b, key->dsa->p);
		buffer_get_bignum2(&b, key->dsa->q);
		buffer_get_bignum2(&b, key->dsa->g);
		buffer_get_bignum2(&b, key->dsa->pub_key);
#ifdef DEBUG_PK
		DSA_print_fp(stderr, key->dsa, 8);
#endif
		break;
	case KEY_UNSPEC:
		key = key_new(type);
		break;
	default:
		error("key_from_blob: cannot handle type %s", ktype);
		break;
	}
	rlen = buffer_len(&b);
	if (key != NULL && rlen != 0)
		error("key_from_blob: remaining bytes in key blob %d", rlen);
	xfree(ktype);
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
	case KEY_DSA:
		buffer_put_cstring(&b, key_ssh_name(key));
		buffer_put_bignum2(&b, key->dsa->p);
		buffer_put_bignum2(&b, key->dsa->q);
		buffer_put_bignum2(&b, key->dsa->g);
		buffer_put_bignum2(&b, key->dsa->pub_key);
		break;
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
	case KEY_DSA:
		return ssh_dss_sign(key, sigp, lenp, data, datalen);
		break;
	case KEY_RSA:
		return ssh_rsa_sign(key, sigp, lenp, data, datalen);
		break;
	default:
		error("key_sign: illegal key type %d", key->type);
		return -1;
		break;
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
	case KEY_DSA:
		return ssh_dss_verify(key, signature, signaturelen, data, datalen);
		break;
	case KEY_RSA:
		return ssh_rsa_verify(key, signature, signaturelen, data, datalen);
		break;
	default:
		error("key_verify: illegal key type %d", key->type);
		return -1;
		break;
	}
}

/* Converts a private to a public key */
Key *
key_demote(const Key *k)
{
	Key *pk;

	pk = xmalloc(sizeof(*pk));
	pk->type = k->type;
	pk->flags = k->flags;
	pk->dsa = NULL;
	pk->rsa = NULL;

	switch (k->type) {
	case KEY_RSA1:
	case KEY_RSA:
		if ((pk->rsa = RSA_new()) == NULL)
			fatal("key_demote: RSA_new failed");
		if ((pk->rsa->e = BN_dup(k->rsa->e)) == NULL)
			fatal("key_demote: BN_dup failed");
		if ((pk->rsa->n = BN_dup(k->rsa->n)) == NULL)
			fatal("key_demote: BN_dup failed");
		break;
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
	default:
		fatal("key_free: bad key type %d", k->type);
		break;
	}

	return (pk);
}
