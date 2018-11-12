/* $OpenBSD: sshkey.c,v 1.31 2015/12/11 04:21:12 mmcc Exp $ */
/*
 * Copyright (c) 2000, 2001 Markus Friedl.  All rights reserved.
 * Copyright (c) 2008 Alexander von Gernler.  All rights reserved.
 * Copyright (c) 2010,2011 Damien Miller.  All rights reserved.
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

#include <sys/param.h>	/* MIN MAX */
#include <sys/types.h>
#include <netinet/in.h>

#ifdef WITH_OPENSSL
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#endif

#include "crypto_api.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <resolv.h>
#ifdef HAVE_UTIL_H
#include <util.h>
#endif /* HAVE_UTIL_H */

#include "ssh2.h"
#include "ssherr.h"
#include "misc.h"
#include "sshbuf.h"
#include "rsa.h"
#include "cipher.h"
#include "digest.h"
#define SSHKEY_INTERNAL
#include "sshkey.h"
#include "match.h"

/* openssh private key file format */
#define MARK_BEGIN		"-----BEGIN OPENSSH PRIVATE KEY-----\n"
#define MARK_END		"-----END OPENSSH PRIVATE KEY-----\n"
#define MARK_BEGIN_LEN		(sizeof(MARK_BEGIN) - 1)
#define MARK_END_LEN		(sizeof(MARK_END) - 1)
#define KDFNAME			"bcrypt"
#define AUTH_MAGIC		"openssh-key-v1"
#define SALT_LEN		16
#define DEFAULT_CIPHERNAME	"aes256-cbc"
#define	DEFAULT_ROUNDS		16

/* Version identification string for SSH v1 identity files. */
#define LEGACY_BEGIN		"SSH PRIVATE KEY FILE FORMAT 1.1\n"

static int sshkey_from_blob_internal(struct sshbuf *buf,
    struct sshkey **keyp, int allow_cert);

/* Supported key types */
struct keytype {
	const char *name;
	const char *shortname;
	int type;
	int nid;
	int cert;
	int sigonly;
};
static const struct keytype keytypes[] = {
	{ "ssh-ed25519", "ED25519", KEY_ED25519, 0, 0, 0 },
	{ "ssh-ed25519-cert-v01@openssh.com", "ED25519-CERT",
	    KEY_ED25519_CERT, 0, 1, 0 },
#ifdef WITH_OPENSSL
	{ NULL, "RSA1", KEY_RSA1, 0, 0, 0 },
	{ "ssh-rsa", "RSA", KEY_RSA, 0, 0, 0 },
	{ "rsa-sha2-256", "RSA", KEY_RSA, 0, 0, 1 },
	{ "rsa-sha2-512", "RSA", KEY_RSA, 0, 0, 1 },
	{ "ssh-dss", "DSA", KEY_DSA, 0, 0, 0 },
# ifdef OPENSSL_HAS_ECC
	{ "ecdsa-sha2-nistp256", "ECDSA", KEY_ECDSA, NID_X9_62_prime256v1, 0, 0 },
	{ "ecdsa-sha2-nistp384", "ECDSA", KEY_ECDSA, NID_secp384r1, 0, 0 },
#  ifdef OPENSSL_HAS_NISTP521
	{ "ecdsa-sha2-nistp521", "ECDSA", KEY_ECDSA, NID_secp521r1, 0, 0 },
#  endif /* OPENSSL_HAS_NISTP521 */
# endif /* OPENSSL_HAS_ECC */
	{ "ssh-rsa-cert-v01@openssh.com", "RSA-CERT", KEY_RSA_CERT, 0, 1, 0 },
	{ "ssh-dss-cert-v01@openssh.com", "DSA-CERT", KEY_DSA_CERT, 0, 1, 0 },
# ifdef OPENSSL_HAS_ECC
	{ "ecdsa-sha2-nistp256-cert-v01@openssh.com", "ECDSA-CERT",
	    KEY_ECDSA_CERT, NID_X9_62_prime256v1, 1, 0 },
	{ "ecdsa-sha2-nistp384-cert-v01@openssh.com", "ECDSA-CERT",
	    KEY_ECDSA_CERT, NID_secp384r1, 1, 0 },
#  ifdef OPENSSL_HAS_NISTP521
	{ "ecdsa-sha2-nistp521-cert-v01@openssh.com", "ECDSA-CERT",
	    KEY_ECDSA_CERT, NID_secp521r1, 1, 0 },
#  endif /* OPENSSL_HAS_NISTP521 */
# endif /* OPENSSL_HAS_ECC */
#endif /* WITH_OPENSSL */
	{ NULL, NULL, -1, -1, 0, 0 }
};

const char *
sshkey_type(const struct sshkey *k)
{
	const struct keytype *kt;

	for (kt = keytypes; kt->type != -1; kt++) {
		if (kt->type == k->type)
			return kt->shortname;
	}
	return "unknown";
}

static const char *
sshkey_ssh_name_from_type_nid(int type, int nid)
{
	const struct keytype *kt;

	for (kt = keytypes; kt->type != -1; kt++) {
		if (kt->type == type && (kt->nid == 0 || kt->nid == nid))
			return kt->name;
	}
	return "ssh-unknown";
}

int
sshkey_type_is_cert(int type)
{
	const struct keytype *kt;

	for (kt = keytypes; kt->type != -1; kt++) {
		if (kt->type == type)
			return kt->cert;
	}
	return 0;
}

const char *
sshkey_ssh_name(const struct sshkey *k)
{
	return sshkey_ssh_name_from_type_nid(k->type, k->ecdsa_nid);
}

const char *
sshkey_ssh_name_plain(const struct sshkey *k)
{
	return sshkey_ssh_name_from_type_nid(sshkey_type_plain(k->type),
	    k->ecdsa_nid);
}

int
sshkey_type_from_name(const char *name)
{
	const struct keytype *kt;

	for (kt = keytypes; kt->type != -1; kt++) {
		/* Only allow shortname matches for plain key types */
		if ((kt->name != NULL && strcmp(name, kt->name) == 0) ||
		    (!kt->cert && strcasecmp(kt->shortname, name) == 0))
			return kt->type;
	}
	return KEY_UNSPEC;
}

int
sshkey_ecdsa_nid_from_name(const char *name)
{
	const struct keytype *kt;

	for (kt = keytypes; kt->type != -1; kt++) {
		if (kt->type != KEY_ECDSA && kt->type != KEY_ECDSA_CERT)
			continue;
		if (kt->name != NULL && strcmp(name, kt->name) == 0)
			return kt->nid;
	}
	return -1;
}

char *
key_alg_list(int certs_only, int plain_only)
{
	char *tmp, *ret = NULL;
	size_t nlen, rlen = 0;
	const struct keytype *kt;

	for (kt = keytypes; kt->type != -1; kt++) {
		if (kt->name == NULL || kt->sigonly)
			continue;
		if ((certs_only && !kt->cert) || (plain_only && kt->cert))
			continue;
		if (ret != NULL)
			ret[rlen++] = '\n';
		nlen = strlen(kt->name);
		if ((tmp = realloc(ret, rlen + nlen + 2)) == NULL) {
			free(ret);
			return NULL;
		}
		ret = tmp;
		memcpy(ret + rlen, kt->name, nlen + 1);
		rlen += nlen;
	}
	return ret;
}

int
sshkey_names_valid2(const char *names, int allow_wildcard)
{
	char *s, *cp, *p;
	const struct keytype *kt;
	int type;

	if (names == NULL || strcmp(names, "") == 0)
		return 0;
	if ((s = cp = strdup(names)) == NULL)
		return 0;
	for ((p = strsep(&cp, ",")); p && *p != '\0';
	    (p = strsep(&cp, ","))) {
		type = sshkey_type_from_name(p);
		if (type == KEY_RSA1) {
			free(s);
			return 0;
		}
		if (type == KEY_UNSPEC) {
			if (allow_wildcard) {
				/*
				 * Try matching key types against the string.
				 * If any has a positive or negative match then
				 * the component is accepted.
				 */
				for (kt = keytypes; kt->type != -1; kt++) {
					if (kt->type == KEY_RSA1)
						continue;
					if (match_pattern_list(kt->name,
					    p, 0) != 0)
						break;
				}
				if (kt->type != -1)
					continue;
			}
			free(s);
			return 0;
		}
	}
	free(s);
	return 1;
}

u_int
sshkey_size(const struct sshkey *k)
{
	switch (k->type) {
#ifdef WITH_OPENSSL
	case KEY_RSA1:
	case KEY_RSA:
	case KEY_RSA_CERT:
		return BN_num_bits(k->rsa->n);
	case KEY_DSA:
	case KEY_DSA_CERT:
		return BN_num_bits(k->dsa->p);
	case KEY_ECDSA:
	case KEY_ECDSA_CERT:
		return sshkey_curve_nid_to_bits(k->ecdsa_nid);
#endif /* WITH_OPENSSL */
	case KEY_ED25519:
	case KEY_ED25519_CERT:
		return 256;	/* XXX */
	}
	return 0;
}

static int
sshkey_type_is_valid_ca(int type)
{
	switch (type) {
	case KEY_RSA:
	case KEY_DSA:
	case KEY_ECDSA:
	case KEY_ED25519:
		return 1;
	default:
		return 0;
	}
}

int
sshkey_is_cert(const struct sshkey *k)
{
	if (k == NULL)
		return 0;
	return sshkey_type_is_cert(k->type);
}

/* Return the cert-less equivalent to a certified key type */
int
sshkey_type_plain(int type)
{
	switch (type) {
	case KEY_RSA_CERT:
		return KEY_RSA;
	case KEY_DSA_CERT:
		return KEY_DSA;
	case KEY_ECDSA_CERT:
		return KEY_ECDSA;
	case KEY_ED25519_CERT:
		return KEY_ED25519;
	default:
		return type;
	}
}

#ifdef WITH_OPENSSL
/* XXX: these are really begging for a table-driven approach */
int
sshkey_curve_name_to_nid(const char *name)
{
	if (strcmp(name, "nistp256") == 0)
		return NID_X9_62_prime256v1;
	else if (strcmp(name, "nistp384") == 0)
		return NID_secp384r1;
# ifdef OPENSSL_HAS_NISTP521
	else if (strcmp(name, "nistp521") == 0)
		return NID_secp521r1;
# endif /* OPENSSL_HAS_NISTP521 */
	else
		return -1;
}

u_int
sshkey_curve_nid_to_bits(int nid)
{
	switch (nid) {
	case NID_X9_62_prime256v1:
		return 256;
	case NID_secp384r1:
		return 384;
# ifdef OPENSSL_HAS_NISTP521
	case NID_secp521r1:
		return 521;
# endif /* OPENSSL_HAS_NISTP521 */
	default:
		return 0;
	}
}

int
sshkey_ecdsa_bits_to_nid(int bits)
{
	switch (bits) {
	case 256:
		return NID_X9_62_prime256v1;
	case 384:
		return NID_secp384r1;
# ifdef OPENSSL_HAS_NISTP521
	case 521:
		return NID_secp521r1;
# endif /* OPENSSL_HAS_NISTP521 */
	default:
		return -1;
	}
}

const char *
sshkey_curve_nid_to_name(int nid)
{
	switch (nid) {
	case NID_X9_62_prime256v1:
		return "nistp256";
	case NID_secp384r1:
		return "nistp384";
# ifdef OPENSSL_HAS_NISTP521
	case NID_secp521r1:
		return "nistp521";
# endif /* OPENSSL_HAS_NISTP521 */
	default:
		return NULL;
	}
}

int
sshkey_ec_nid_to_hash_alg(int nid)
{
	int kbits = sshkey_curve_nid_to_bits(nid);

	if (kbits <= 0)
		return -1;

	/* RFC5656 section 6.2.1 */
	if (kbits <= 256)
		return SSH_DIGEST_SHA256;
	else if (kbits <= 384)
		return SSH_DIGEST_SHA384;
	else
		return SSH_DIGEST_SHA512;
}
#endif /* WITH_OPENSSL */

static void
cert_free(struct sshkey_cert *cert)
{
	u_int i;

	if (cert == NULL)
		return;
	sshbuf_free(cert->certblob);
	sshbuf_free(cert->critical);
	sshbuf_free(cert->extensions);
	free(cert->key_id);
	for (i = 0; i < cert->nprincipals; i++)
		free(cert->principals[i]);
	free(cert->principals);
	sshkey_free(cert->signature_key);
	explicit_bzero(cert, sizeof(*cert));
	free(cert);
}

static struct sshkey_cert *
cert_new(void)
{
	struct sshkey_cert *cert;

	if ((cert = calloc(1, sizeof(*cert))) == NULL)
		return NULL;
	if ((cert->certblob = sshbuf_new()) == NULL ||
	    (cert->critical = sshbuf_new()) == NULL ||
	    (cert->extensions = sshbuf_new()) == NULL) {
		cert_free(cert);
		return NULL;
	}
	cert->key_id = NULL;
	cert->principals = NULL;
	cert->signature_key = NULL;
	return cert;
}

struct sshkey *
sshkey_new(int type)
{
	struct sshkey *k;
#ifdef WITH_OPENSSL
	RSA *rsa;
	DSA *dsa;
#endif /* WITH_OPENSSL */

	if ((k = calloc(1, sizeof(*k))) == NULL)
		return NULL;
	k->type = type;
	k->ecdsa = NULL;
	k->ecdsa_nid = -1;
	k->dsa = NULL;
	k->rsa = NULL;
	k->cert = NULL;
	k->ed25519_sk = NULL;
	k->ed25519_pk = NULL;
	switch (k->type) {
#ifdef WITH_OPENSSL
	case KEY_RSA1:
	case KEY_RSA:
	case KEY_RSA_CERT:
		if ((rsa = RSA_new()) == NULL ||
		    (rsa->n = BN_new()) == NULL ||
		    (rsa->e = BN_new()) == NULL) {
			if (rsa != NULL)
				RSA_free(rsa);
			free(k);
			return NULL;
		}
		k->rsa = rsa;
		break;
	case KEY_DSA:
	case KEY_DSA_CERT:
		if ((dsa = DSA_new()) == NULL ||
		    (dsa->p = BN_new()) == NULL ||
		    (dsa->q = BN_new()) == NULL ||
		    (dsa->g = BN_new()) == NULL ||
		    (dsa->pub_key = BN_new()) == NULL) {
			if (dsa != NULL)
				DSA_free(dsa);
			free(k);
			return NULL;
		}
		k->dsa = dsa;
		break;
	case KEY_ECDSA:
	case KEY_ECDSA_CERT:
		/* Cannot do anything until we know the group */
		break;
#endif /* WITH_OPENSSL */
	case KEY_ED25519:
	case KEY_ED25519_CERT:
		/* no need to prealloc */
		break;
	case KEY_UNSPEC:
		break;
	default:
		free(k);
		return NULL;
		break;
	}

	if (sshkey_is_cert(k)) {
		if ((k->cert = cert_new()) == NULL) {
			sshkey_free(k);
			return NULL;
		}
	}

	return k;
}

int
sshkey_add_private(struct sshkey *k)
{
	switch (k->type) {
#ifdef WITH_OPENSSL
	case KEY_RSA1:
	case KEY_RSA:
	case KEY_RSA_CERT:
#define bn_maybe_alloc_failed(p) (p == NULL && (p = BN_new()) == NULL)
		if (bn_maybe_alloc_failed(k->rsa->d) ||
		    bn_maybe_alloc_failed(k->rsa->iqmp) ||
		    bn_maybe_alloc_failed(k->rsa->q) ||
		    bn_maybe_alloc_failed(k->rsa->p) ||
		    bn_maybe_alloc_failed(k->rsa->dmq1) ||
		    bn_maybe_alloc_failed(k->rsa->dmp1))
			return SSH_ERR_ALLOC_FAIL;
		break;
	case KEY_DSA:
	case KEY_DSA_CERT:
		if (bn_maybe_alloc_failed(k->dsa->priv_key))
			return SSH_ERR_ALLOC_FAIL;
		break;
#undef bn_maybe_alloc_failed
	case KEY_ECDSA:
	case KEY_ECDSA_CERT:
		/* Cannot do anything until we know the group */
		break;
#endif /* WITH_OPENSSL */
	case KEY_ED25519:
	case KEY_ED25519_CERT:
		/* no need to prealloc */
		break;
	case KEY_UNSPEC:
		break;
	default:
		return SSH_ERR_INVALID_ARGUMENT;
	}
	return 0;
}

struct sshkey *
sshkey_new_private(int type)
{
	struct sshkey *k = sshkey_new(type);

	if (k == NULL)
		return NULL;
	if (sshkey_add_private(k) != 0) {
		sshkey_free(k);
		return NULL;
	}
	return k;
}

void
sshkey_free(struct sshkey *k)
{
	if (k == NULL)
		return;
	switch (k->type) {
#ifdef WITH_OPENSSL
	case KEY_RSA1:
	case KEY_RSA:
	case KEY_RSA_CERT:
		if (k->rsa != NULL)
			RSA_free(k->rsa);
		k->rsa = NULL;
		break;
	case KEY_DSA:
	case KEY_DSA_CERT:
		if (k->dsa != NULL)
			DSA_free(k->dsa);
		k->dsa = NULL;
		break;
# ifdef OPENSSL_HAS_ECC
	case KEY_ECDSA:
	case KEY_ECDSA_CERT:
		if (k->ecdsa != NULL)
			EC_KEY_free(k->ecdsa);
		k->ecdsa = NULL;
		break;
# endif /* OPENSSL_HAS_ECC */
#endif /* WITH_OPENSSL */
	case KEY_ED25519:
	case KEY_ED25519_CERT:
		if (k->ed25519_pk) {
			explicit_bzero(k->ed25519_pk, ED25519_PK_SZ);
			free(k->ed25519_pk);
			k->ed25519_pk = NULL;
		}
		if (k->ed25519_sk) {
			explicit_bzero(k->ed25519_sk, ED25519_SK_SZ);
			free(k->ed25519_sk);
			k->ed25519_sk = NULL;
		}
		break;
	case KEY_UNSPEC:
		break;
	default:
		break;
	}
	if (sshkey_is_cert(k))
		cert_free(k->cert);
	explicit_bzero(k, sizeof(*k));
	free(k);
}

static int
cert_compare(struct sshkey_cert *a, struct sshkey_cert *b)
{
	if (a == NULL && b == NULL)
		return 1;
	if (a == NULL || b == NULL)
		return 0;
	if (sshbuf_len(a->certblob) != sshbuf_len(b->certblob))
		return 0;
	if (timingsafe_bcmp(sshbuf_ptr(a->certblob), sshbuf_ptr(b->certblob),
	    sshbuf_len(a->certblob)) != 0)
		return 0;
	return 1;
}

/*
 * Compare public portions of key only, allowing comparisons between
 * certificates and plain keys too.
 */
int
sshkey_equal_public(const struct sshkey *a, const struct sshkey *b)
{
#if defined(WITH_OPENSSL) && defined(OPENSSL_HAS_ECC)
	BN_CTX *bnctx;
#endif /* WITH_OPENSSL && OPENSSL_HAS_ECC */

	if (a == NULL || b == NULL ||
	    sshkey_type_plain(a->type) != sshkey_type_plain(b->type))
		return 0;

	switch (a->type) {
#ifdef WITH_OPENSSL
	case KEY_RSA1:
	case KEY_RSA_CERT:
	case KEY_RSA:
		return a->rsa != NULL && b->rsa != NULL &&
		    BN_cmp(a->rsa->e, b->rsa->e) == 0 &&
		    BN_cmp(a->rsa->n, b->rsa->n) == 0;
	case KEY_DSA_CERT:
	case KEY_DSA:
		return a->dsa != NULL && b->dsa != NULL &&
		    BN_cmp(a->dsa->p, b->dsa->p) == 0 &&
		    BN_cmp(a->dsa->q, b->dsa->q) == 0 &&
		    BN_cmp(a->dsa->g, b->dsa->g) == 0 &&
		    BN_cmp(a->dsa->pub_key, b->dsa->pub_key) == 0;
# ifdef OPENSSL_HAS_ECC
	case KEY_ECDSA_CERT:
	case KEY_ECDSA:
		if (a->ecdsa == NULL || b->ecdsa == NULL ||
		    EC_KEY_get0_public_key(a->ecdsa) == NULL ||
		    EC_KEY_get0_public_key(b->ecdsa) == NULL)
			return 0;
		if ((bnctx = BN_CTX_new()) == NULL)
			return 0;
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
# endif /* OPENSSL_HAS_ECC */
#endif /* WITH_OPENSSL */
	case KEY_ED25519:
	case KEY_ED25519_CERT:
		return a->ed25519_pk != NULL && b->ed25519_pk != NULL &&
		    memcmp(a->ed25519_pk, b->ed25519_pk, ED25519_PK_SZ) == 0;
	default:
		return 0;
	}
	/* NOTREACHED */
}

int
sshkey_equal(const struct sshkey *a, const struct sshkey *b)
{
	if (a == NULL || b == NULL || a->type != b->type)
		return 0;
	if (sshkey_is_cert(a)) {
		if (!cert_compare(a->cert, b->cert))
			return 0;
	}
	return sshkey_equal_public(a, b);
}

static int
to_blob_buf(const struct sshkey *key, struct sshbuf *b, int force_plain)
{
	int type, ret = SSH_ERR_INTERNAL_ERROR;
	const char *typename;

	if (key == NULL)
		return SSH_ERR_INVALID_ARGUMENT;

	if (sshkey_is_cert(key)) {
		if (key->cert == NULL)
			return SSH_ERR_EXPECTED_CERT;
		if (sshbuf_len(key->cert->certblob) == 0)
			return SSH_ERR_KEY_LACKS_CERTBLOB;
	}
	type = force_plain ? sshkey_type_plain(key->type) : key->type;
	typename = sshkey_ssh_name_from_type_nid(type, key->ecdsa_nid);

	switch (type) {
#ifdef WITH_OPENSSL
	case KEY_DSA_CERT:
	case KEY_ECDSA_CERT:
	case KEY_RSA_CERT:
#endif /* WITH_OPENSSL */
	case KEY_ED25519_CERT:
		/* Use the existing blob */
		/* XXX modified flag? */
		if ((ret = sshbuf_putb(b, key->cert->certblob)) != 0)
			return ret;
		break;
#ifdef WITH_OPENSSL
	case KEY_DSA:
		if (key->dsa == NULL)
			return SSH_ERR_INVALID_ARGUMENT;
		if ((ret = sshbuf_put_cstring(b, typename)) != 0 ||
		    (ret = sshbuf_put_bignum2(b, key->dsa->p)) != 0 ||
		    (ret = sshbuf_put_bignum2(b, key->dsa->q)) != 0 ||
		    (ret = sshbuf_put_bignum2(b, key->dsa->g)) != 0 ||
		    (ret = sshbuf_put_bignum2(b, key->dsa->pub_key)) != 0)
			return ret;
		break;
# ifdef OPENSSL_HAS_ECC
	case KEY_ECDSA:
		if (key->ecdsa == NULL)
			return SSH_ERR_INVALID_ARGUMENT;
		if ((ret = sshbuf_put_cstring(b, typename)) != 0 ||
		    (ret = sshbuf_put_cstring(b,
		    sshkey_curve_nid_to_name(key->ecdsa_nid))) != 0 ||
		    (ret = sshbuf_put_eckey(b, key->ecdsa)) != 0)
			return ret;
		break;
# endif
	case KEY_RSA:
		if (key->rsa == NULL)
			return SSH_ERR_INVALID_ARGUMENT;
		if ((ret = sshbuf_put_cstring(b, typename)) != 0 ||
		    (ret = sshbuf_put_bignum2(b, key->rsa->e)) != 0 ||
		    (ret = sshbuf_put_bignum2(b, key->rsa->n)) != 0)
			return ret;
		break;
#endif /* WITH_OPENSSL */
	case KEY_ED25519:
		if (key->ed25519_pk == NULL)
			return SSH_ERR_INVALID_ARGUMENT;
		if ((ret = sshbuf_put_cstring(b, typename)) != 0 ||
		    (ret = sshbuf_put_string(b,
		    key->ed25519_pk, ED25519_PK_SZ)) != 0)
			return ret;
		break;
	default:
		return SSH_ERR_KEY_TYPE_UNKNOWN;
	}
	return 0;
}

int
sshkey_putb(const struct sshkey *key, struct sshbuf *b)
{
	return to_blob_buf(key, b, 0);
}

int
sshkey_puts(const struct sshkey *key, struct sshbuf *b)
{
	struct sshbuf *tmp;
	int r;

	if ((tmp = sshbuf_new()) == NULL)
		return SSH_ERR_ALLOC_FAIL;
	r = to_blob_buf(key, tmp, 0);
	if (r == 0)
		r = sshbuf_put_stringb(b, tmp);
	sshbuf_free(tmp);
	return r;
}

int
sshkey_putb_plain(const struct sshkey *key, struct sshbuf *b)
{
	return to_blob_buf(key, b, 1);
}

static int
to_blob(const struct sshkey *key, u_char **blobp, size_t *lenp, int force_plain)
{
	int ret = SSH_ERR_INTERNAL_ERROR;
	size_t len;
	struct sshbuf *b = NULL;

	if (lenp != NULL)
		*lenp = 0;
	if (blobp != NULL)
		*blobp = NULL;
	if ((b = sshbuf_new()) == NULL)
		return SSH_ERR_ALLOC_FAIL;
	if ((ret = to_blob_buf(key, b, force_plain)) != 0)
		goto out;
	len = sshbuf_len(b);
	if (lenp != NULL)
		*lenp = len;
	if (blobp != NULL) {
		if ((*blobp = malloc(len)) == NULL) {
			ret = SSH_ERR_ALLOC_FAIL;
			goto out;
		}
		memcpy(*blobp, sshbuf_ptr(b), len);
	}
	ret = 0;
 out:
	sshbuf_free(b);
	return ret;
}

int
sshkey_to_blob(const struct sshkey *key, u_char **blobp, size_t *lenp)
{
	return to_blob(key, blobp, lenp, 0);
}

int
sshkey_plain_to_blob(const struct sshkey *key, u_char **blobp, size_t *lenp)
{
	return to_blob(key, blobp, lenp, 1);
}

int
sshkey_fingerprint_raw(const struct sshkey *k, int dgst_alg,
    u_char **retp, size_t *lenp)
{
	u_char *blob = NULL, *ret = NULL;
	size_t blob_len = 0;
	int r = SSH_ERR_INTERNAL_ERROR;

	if (retp != NULL)
		*retp = NULL;
	if (lenp != NULL)
		*lenp = 0;
	if (ssh_digest_bytes(dgst_alg) == 0) {
		r = SSH_ERR_INVALID_ARGUMENT;
		goto out;
	}

	if (k->type == KEY_RSA1) {
#ifdef WITH_OPENSSL
		int nlen = BN_num_bytes(k->rsa->n);
		int elen = BN_num_bytes(k->rsa->e);

		blob_len = nlen + elen;
		if (nlen >= INT_MAX - elen ||
		    (blob = malloc(blob_len)) == NULL) {
			r = SSH_ERR_ALLOC_FAIL;
			goto out;
		}
		BN_bn2bin(k->rsa->n, blob);
		BN_bn2bin(k->rsa->e, blob + nlen);
#endif /* WITH_OPENSSL */
	} else if ((r = to_blob(k, &blob, &blob_len, 1)) != 0)
		goto out;
	if ((ret = calloc(1, SSH_DIGEST_MAX_LENGTH)) == NULL) {
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	if ((r = ssh_digest_memory(dgst_alg, blob, blob_len,
	    ret, SSH_DIGEST_MAX_LENGTH)) != 0)
		goto out;
	/* success */
	if (retp != NULL) {
		*retp = ret;
		ret = NULL;
	}
	if (lenp != NULL)
		*lenp = ssh_digest_bytes(dgst_alg);
	r = 0;
 out:
	free(ret);
	if (blob != NULL) {
		explicit_bzero(blob, blob_len);
		free(blob);
	}
	return r;
}

static char *
fingerprint_b64(const char *alg, u_char *dgst_raw, size_t dgst_raw_len)
{
	char *ret;
	size_t plen = strlen(alg) + 1;
	size_t rlen = ((dgst_raw_len + 2) / 3) * 4 + plen + 1;
	int r;

	if (dgst_raw_len > 65536 || (ret = calloc(1, rlen)) == NULL)
		return NULL;
	strlcpy(ret, alg, rlen);
	strlcat(ret, ":", rlen);
	if (dgst_raw_len == 0)
		return ret;
	if ((r = b64_ntop(dgst_raw, dgst_raw_len,
	    ret + plen, rlen - plen)) == -1) {
		explicit_bzero(ret, rlen);
		free(ret);
		return NULL;
	}
	/* Trim padding characters from end */
	ret[strcspn(ret, "=")] = '\0';
	return ret;
}

static char *
fingerprint_hex(const char *alg, u_char *dgst_raw, size_t dgst_raw_len)
{
	char *retval, hex[5];
	size_t i, rlen = dgst_raw_len * 3 + strlen(alg) + 2;

	if (dgst_raw_len > 65536 || (retval = calloc(1, rlen)) == NULL)
		return NULL;
	strlcpy(retval, alg, rlen);
	strlcat(retval, ":", rlen);
	for (i = 0; i < dgst_raw_len; i++) {
		snprintf(hex, sizeof(hex), "%s%02x",
		    i > 0 ? ":" : "", dgst_raw[i]);
		strlcat(retval, hex, rlen);
	}
	return retval;
}

static char *
fingerprint_bubblebabble(u_char *dgst_raw, size_t dgst_raw_len)
{
	char vowels[] = { 'a', 'e', 'i', 'o', 'u', 'y' };
	char consonants[] = { 'b', 'c', 'd', 'f', 'g', 'h', 'k', 'l', 'm',
	    'n', 'p', 'r', 's', 't', 'v', 'z', 'x' };
	u_int i, j = 0, rounds, seed = 1;
	char *retval;

	rounds = (dgst_raw_len / 2) + 1;
	if ((retval = calloc(rounds, 6)) == NULL)
		return NULL;
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
fingerprint_randomart(const char *alg, u_char *dgst_raw, size_t dgst_raw_len,
    const struct sshkey *k)
{
	/*
	 * Chars to be used after each other every time the worm
	 * intersects with itself.  Matter of taste.
	 */
	char	*augmentation_string = " .o+=*BOX@%&#/^SE";
	char	*retval, *p, title[FLDSIZE_X], hash[FLDSIZE_X];
	u_char	 field[FLDSIZE_X][FLDSIZE_Y];
	size_t	 i, tlen, hlen;
	u_int	 b;
	int	 x, y, r;
	size_t	 len = strlen(augmentation_string) - 1;

	if ((retval = calloc((FLDSIZE_X + 3), (FLDSIZE_Y + 2))) == NULL)
		return NULL;

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

	/* assemble title */
	r = snprintf(title, sizeof(title), "[%s %u]",
		sshkey_type(k), sshkey_size(k));
	/* If [type size] won't fit, then try [type]; fits "[ED25519-CERT]" */
	if (r < 0 || r > (int)sizeof(title))
		r = snprintf(title, sizeof(title), "[%s]", sshkey_type(k));
	tlen = (r <= 0) ? 0 : strlen(title);

	/* assemble hash ID. */
	r = snprintf(hash, sizeof(hash), "[%s]", alg);
	hlen = (r <= 0) ? 0 : strlen(hash);

	/* output upper border */
	p = retval;
	*p++ = '+';
	for (i = 0; i < (FLDSIZE_X - tlen) / 2; i++)
		*p++ = '-';
	memcpy(p, title, tlen);
	p += tlen;
	for (i += tlen; i < FLDSIZE_X; i++)
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
	for (i = 0; i < (FLDSIZE_X - hlen) / 2; i++)
		*p++ = '-';
	memcpy(p, hash, hlen);
	p += hlen;
	for (i += hlen; i < FLDSIZE_X; i++)
		*p++ = '-';
	*p++ = '+';

	return retval;
}

char *
sshkey_fingerprint(const struct sshkey *k, int dgst_alg,
    enum sshkey_fp_rep dgst_rep)
{
	char *retval = NULL;
	u_char *dgst_raw;
	size_t dgst_raw_len;

	if (sshkey_fingerprint_raw(k, dgst_alg, &dgst_raw, &dgst_raw_len) != 0)
		return NULL;
	switch (dgst_rep) {
	case SSH_FP_DEFAULT:
		if (dgst_alg == SSH_DIGEST_MD5) {
			retval = fingerprint_hex(ssh_digest_alg_name(dgst_alg),
			    dgst_raw, dgst_raw_len);
		} else {
			retval = fingerprint_b64(ssh_digest_alg_name(dgst_alg),
			    dgst_raw, dgst_raw_len);
		}
		break;
	case SSH_FP_HEX:
		retval = fingerprint_hex(ssh_digest_alg_name(dgst_alg),
		    dgst_raw, dgst_raw_len);
		break;
	case SSH_FP_BASE64:
		retval = fingerprint_b64(ssh_digest_alg_name(dgst_alg),
		    dgst_raw, dgst_raw_len);
		break;
	case SSH_FP_BUBBLEBABBLE:
		retval = fingerprint_bubblebabble(dgst_raw, dgst_raw_len);
		break;
	case SSH_FP_RANDOMART:
		retval = fingerprint_randomart(ssh_digest_alg_name(dgst_alg),
		    dgst_raw, dgst_raw_len, k);
		break;
	default:
		explicit_bzero(dgst_raw, dgst_raw_len);
		free(dgst_raw);
		return NULL;
	}
	explicit_bzero(dgst_raw, dgst_raw_len);
	free(dgst_raw);
	return retval;
}

#ifdef WITH_SSH1
/*
 * Reads a multiple-precision integer in decimal from the buffer, and advances
 * the pointer.  The integer must already be initialized.  This function is
 * permitted to modify the buffer.  This leaves *cpp to point just beyond the
 * last processed character.
 */
static int
read_decimal_bignum(char **cpp, BIGNUM *v)
{
	char *cp;
	size_t e;
	int skip = 1;	/* skip white space */

	cp = *cpp;
	while (*cp == ' ' || *cp == '\t')
		cp++;
	e = strspn(cp, "0123456789");
	if (e == 0)
		return SSH_ERR_INVALID_FORMAT;
	if (e > SSHBUF_MAX_BIGNUM * 3)
		return SSH_ERR_BIGNUM_TOO_LARGE;
	if (cp[e] == '\0')
		skip = 0;
	else if (strchr(" \t\r\n", cp[e]) == NULL)
		return SSH_ERR_INVALID_FORMAT;
	cp[e] = '\0';
	if (BN_dec2bn(&v, cp) <= 0)
		return SSH_ERR_INVALID_FORMAT;
	*cpp = cp + e + skip;
	return 0;
}
#endif /* WITH_SSH1 */

/* returns 0 ok, and < 0 error */
int
sshkey_read(struct sshkey *ret, char **cpp)
{
	struct sshkey *k;
	int retval = SSH_ERR_INVALID_FORMAT;
	char *ep, *cp, *space;
	int r, type, curve_nid = -1;
	struct sshbuf *blob;
#ifdef WITH_SSH1
	u_long bits;
#endif /* WITH_SSH1 */

	cp = *cpp;

	switch (ret->type) {
	case KEY_RSA1:
#ifdef WITH_SSH1
		/* Get number of bits. */
		bits = strtoul(cp, &ep, 10);
		if (*cp == '\0' || strchr(" \t\r\n", *ep) == NULL ||
		    bits == 0 || bits > SSHBUF_MAX_BIGNUM * 8)
			return SSH_ERR_INVALID_FORMAT;	/* Bad bit count... */
		/* Get public exponent, public modulus. */
		if ((r = read_decimal_bignum(&ep, ret->rsa->e)) < 0)
			return r;
		if ((r = read_decimal_bignum(&ep, ret->rsa->n)) < 0)
			return r;
		/* validate the claimed number of bits */
		if (BN_num_bits(ret->rsa->n) != (int)bits)
			return SSH_ERR_KEY_BITS_MISMATCH;
		*cpp = ep;
		retval = 0;
#endif /* WITH_SSH1 */
		break;
	case KEY_UNSPEC:
	case KEY_RSA:
	case KEY_DSA:
	case KEY_ECDSA:
	case KEY_ED25519:
	case KEY_DSA_CERT:
	case KEY_ECDSA_CERT:
	case KEY_RSA_CERT:
	case KEY_ED25519_CERT:
		space = strchr(cp, ' ');
		if (space == NULL)
			return SSH_ERR_INVALID_FORMAT;
		*space = '\0';
		type = sshkey_type_from_name(cp);
		if (sshkey_type_plain(type) == KEY_ECDSA &&
		    (curve_nid = sshkey_ecdsa_nid_from_name(cp)) == -1)
			return SSH_ERR_EC_CURVE_INVALID;
		*space = ' ';
		if (type == KEY_UNSPEC)
			return SSH_ERR_INVALID_FORMAT;
		cp = space+1;
		if (*cp == '\0')
			return SSH_ERR_INVALID_FORMAT;
		if (ret->type != KEY_UNSPEC && ret->type != type)
			return SSH_ERR_KEY_TYPE_MISMATCH;
		if ((blob = sshbuf_new()) == NULL)
			return SSH_ERR_ALLOC_FAIL;
		/* trim comment */
		space = strchr(cp, ' ');
		if (space) {
			/* advance 'space': skip whitespace */
			*space++ = '\0';
			while (*space == ' ' || *space == '\t')
				space++;
			ep = space;
		} else
			ep = cp + strlen(cp);
		if ((r = sshbuf_b64tod(blob, cp)) != 0) {
			sshbuf_free(blob);
			return r;
		}
		if ((r = sshkey_from_blob(sshbuf_ptr(blob),
		    sshbuf_len(blob), &k)) != 0) {
			sshbuf_free(blob);
			return r;
		}
		sshbuf_free(blob);
		if (k->type != type) {
			sshkey_free(k);
			return SSH_ERR_KEY_TYPE_MISMATCH;
		}
		if (sshkey_type_plain(type) == KEY_ECDSA &&
		    curve_nid != k->ecdsa_nid) {
			sshkey_free(k);
			return SSH_ERR_EC_CURVE_MISMATCH;
		}
		ret->type = type;
		if (sshkey_is_cert(ret)) {
			if (!sshkey_is_cert(k)) {
				sshkey_free(k);
				return SSH_ERR_EXPECTED_CERT;
			}
			if (ret->cert != NULL)
				cert_free(ret->cert);
			ret->cert = k->cert;
			k->cert = NULL;
		}
		switch (sshkey_type_plain(ret->type)) {
#ifdef WITH_OPENSSL
		case KEY_RSA:
			if (ret->rsa != NULL)
				RSA_free(ret->rsa);
			ret->rsa = k->rsa;
			k->rsa = NULL;
#ifdef DEBUG_PK
			RSA_print_fp(stderr, ret->rsa, 8);
#endif
			break;
		case KEY_DSA:
			if (ret->dsa != NULL)
				DSA_free(ret->dsa);
			ret->dsa = k->dsa;
			k->dsa = NULL;
#ifdef DEBUG_PK
			DSA_print_fp(stderr, ret->dsa, 8);
#endif
			break;
# ifdef OPENSSL_HAS_ECC
		case KEY_ECDSA:
			if (ret->ecdsa != NULL)
				EC_KEY_free(ret->ecdsa);
			ret->ecdsa = k->ecdsa;
			ret->ecdsa_nid = k->ecdsa_nid;
			k->ecdsa = NULL;
			k->ecdsa_nid = -1;
#ifdef DEBUG_PK
			sshkey_dump_ec_key(ret->ecdsa);
#endif
			break;
# endif /* OPENSSL_HAS_ECC */
#endif /* WITH_OPENSSL */
		case KEY_ED25519:
			free(ret->ed25519_pk);
			ret->ed25519_pk = k->ed25519_pk;
			k->ed25519_pk = NULL;
#ifdef DEBUG_PK
			/* XXX */
#endif
			break;
		}
		*cpp = ep;
		retval = 0;
/*XXXX*/
		sshkey_free(k);
		if (retval != 0)
			break;
		break;
	default:
		return SSH_ERR_INVALID_ARGUMENT;
	}
	return retval;
}

int
sshkey_to_base64(const struct sshkey *key, char **b64p)
{
	int r = SSH_ERR_INTERNAL_ERROR;
	struct sshbuf *b = NULL;
	char *uu = NULL;

	if (b64p != NULL)
		*b64p = NULL;
	if ((b = sshbuf_new()) == NULL)
		return SSH_ERR_ALLOC_FAIL;
	if ((r = sshkey_putb(key, b)) != 0)
		goto out;
	if ((uu = sshbuf_dtob64(b)) == NULL) {
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	/* Success */
	if (b64p != NULL) {
		*b64p = uu;
		uu = NULL;
	}
	r = 0;
 out:
	sshbuf_free(b);
	free(uu);
	return r;
}

static int
sshkey_format_rsa1(const struct sshkey *key, struct sshbuf *b)
{
	int r = SSH_ERR_INTERNAL_ERROR;
#ifdef WITH_SSH1
	u_int bits = 0;
	char *dec_e = NULL, *dec_n = NULL;

	if (key->rsa == NULL || key->rsa->e == NULL ||
	    key->rsa->n == NULL) {
		r = SSH_ERR_INVALID_ARGUMENT;
		goto out;
	}
	if ((dec_e = BN_bn2dec(key->rsa->e)) == NULL ||
	    (dec_n = BN_bn2dec(key->rsa->n)) == NULL) {
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	/* size of modulus 'n' */
	if ((bits = BN_num_bits(key->rsa->n)) <= 0) {
		r = SSH_ERR_INVALID_ARGUMENT;
		goto out;
	}
	if ((r = sshbuf_putf(b, "%u %s %s", bits, dec_e, dec_n)) != 0)
		goto out;

	/* Success */
	r = 0;
 out:
	if (dec_e != NULL)
		OPENSSL_free(dec_e);
	if (dec_n != NULL)
		OPENSSL_free(dec_n);
#endif /* WITH_SSH1 */

	return r;
}

static int
sshkey_format_text(const struct sshkey *key, struct sshbuf *b)
{
	int r = SSH_ERR_INTERNAL_ERROR;
	char *uu = NULL;

	if (key->type == KEY_RSA1) {
		if ((r = sshkey_format_rsa1(key, b)) != 0)
			goto out;
	} else {
		/* Unsupported key types handled in sshkey_to_base64() */
		if ((r = sshkey_to_base64(key, &uu)) != 0)
			goto out;
		if ((r = sshbuf_putf(b, "%s %s",
		    sshkey_ssh_name(key), uu)) != 0)
			goto out;
	}
	r = 0;
 out:
	free(uu);
	return r;
}

int
sshkey_write(const struct sshkey *key, FILE *f)
{
	struct sshbuf *b = NULL;
	int r = SSH_ERR_INTERNAL_ERROR;

	if ((b = sshbuf_new()) == NULL)
		return SSH_ERR_ALLOC_FAIL;
	if ((r = sshkey_format_text(key, b)) != 0)
		goto out;
	if (fwrite(sshbuf_ptr(b), sshbuf_len(b), 1, f) != 1) {
		if (feof(f))
			errno = EPIPE;
		r = SSH_ERR_SYSTEM_ERROR;
		goto out;
	}
	/* Success */
	r = 0;
 out:
	sshbuf_free(b);
	return r;
}

const char *
sshkey_cert_type(const struct sshkey *k)
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

#ifdef WITH_OPENSSL
static int
rsa_generate_private_key(u_int bits, RSA **rsap)
{
	RSA *private = NULL;
	BIGNUM *f4 = NULL;
	int ret = SSH_ERR_INTERNAL_ERROR;

	if (rsap == NULL ||
	    bits < SSH_RSA_MINIMUM_MODULUS_SIZE ||
	    bits > SSHBUF_MAX_BIGNUM * 8)
		return SSH_ERR_INVALID_ARGUMENT;
	*rsap = NULL;
	if ((private = RSA_new()) == NULL || (f4 = BN_new()) == NULL) {
		ret = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	if (!BN_set_word(f4, RSA_F4) ||
	    !RSA_generate_key_ex(private, bits, f4, NULL)) {
		ret = SSH_ERR_LIBCRYPTO_ERROR;
		goto out;
	}
	*rsap = private;
	private = NULL;
	ret = 0;
 out:
	if (private != NULL)
		RSA_free(private);
	if (f4 != NULL)
		BN_free(f4);
	return ret;
}

static int
dsa_generate_private_key(u_int bits, DSA **dsap)
{
	DSA *private;
	int ret = SSH_ERR_INTERNAL_ERROR;

	if (dsap == NULL || bits != 1024)
		return SSH_ERR_INVALID_ARGUMENT;
	if ((private = DSA_new()) == NULL) {
		ret = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	*dsap = NULL;
	if (!DSA_generate_parameters_ex(private, bits, NULL, 0, NULL,
	    NULL, NULL) || !DSA_generate_key(private)) {
		ret = SSH_ERR_LIBCRYPTO_ERROR;
		goto out;
	}
	*dsap = private;
	private = NULL;
	ret = 0;
 out:
	if (private != NULL)
		DSA_free(private);
	return ret;
}

# ifdef OPENSSL_HAS_ECC
int
sshkey_ecdsa_key_to_nid(EC_KEY *k)
{
	EC_GROUP *eg;
	int nids[] = {
		NID_X9_62_prime256v1,
		NID_secp384r1,
#  ifdef OPENSSL_HAS_NISTP521
		NID_secp521r1,
#  endif /* OPENSSL_HAS_NISTP521 */
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
		return -1;
	for (i = 0; nids[i] != -1; i++) {
		if ((eg = EC_GROUP_new_by_curve_name(nids[i])) == NULL) {
			BN_CTX_free(bnctx);
			return -1;
		}
		if (EC_GROUP_cmp(g, eg, bnctx) == 0)
			break;
		EC_GROUP_free(eg);
	}
	BN_CTX_free(bnctx);
	if (nids[i] != -1) {
		/* Use the group with the NID attached */
		EC_GROUP_set_asn1_flag(eg, OPENSSL_EC_NAMED_CURVE);
		if (EC_KEY_set_group(k, eg) != 1) {
			EC_GROUP_free(eg);
			return -1;
		}
	}
	return nids[i];
}

static int
ecdsa_generate_private_key(u_int bits, int *nid, EC_KEY **ecdsap)
{
	EC_KEY *private;
	int ret = SSH_ERR_INTERNAL_ERROR;

	if (nid == NULL || ecdsap == NULL ||
	    (*nid = sshkey_ecdsa_bits_to_nid(bits)) == -1)
		return SSH_ERR_INVALID_ARGUMENT;
	*ecdsap = NULL;
	if ((private = EC_KEY_new_by_curve_name(*nid)) == NULL) {
		ret = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	if (EC_KEY_generate_key(private) != 1) {
		ret = SSH_ERR_LIBCRYPTO_ERROR;
		goto out;
	}
	EC_KEY_set_asn1_flag(private, OPENSSL_EC_NAMED_CURVE);
	*ecdsap = private;
	private = NULL;
	ret = 0;
 out:
	if (private != NULL)
		EC_KEY_free(private);
	return ret;
}
# endif /* OPENSSL_HAS_ECC */
#endif /* WITH_OPENSSL */

int
sshkey_generate(int type, u_int bits, struct sshkey **keyp)
{
	struct sshkey *k;
	int ret = SSH_ERR_INTERNAL_ERROR;

	if (keyp == NULL)
		return SSH_ERR_INVALID_ARGUMENT;
	*keyp = NULL;
	if ((k = sshkey_new(KEY_UNSPEC)) == NULL)
		return SSH_ERR_ALLOC_FAIL;
	switch (type) {
	case KEY_ED25519:
		if ((k->ed25519_pk = malloc(ED25519_PK_SZ)) == NULL ||
		    (k->ed25519_sk = malloc(ED25519_SK_SZ)) == NULL) {
			ret = SSH_ERR_ALLOC_FAIL;
			break;
		}
		crypto_sign_ed25519_keypair(k->ed25519_pk, k->ed25519_sk);
		ret = 0;
		break;
#ifdef WITH_OPENSSL
	case KEY_DSA:
		ret = dsa_generate_private_key(bits, &k->dsa);
		break;
# ifdef OPENSSL_HAS_ECC
	case KEY_ECDSA:
		ret = ecdsa_generate_private_key(bits, &k->ecdsa_nid,
		    &k->ecdsa);
		break;
# endif /* OPENSSL_HAS_ECC */
	case KEY_RSA:
	case KEY_RSA1:
		ret = rsa_generate_private_key(bits, &k->rsa);
		break;
#endif /* WITH_OPENSSL */
	default:
		ret = SSH_ERR_INVALID_ARGUMENT;
	}
	if (ret == 0) {
		k->type = type;
		*keyp = k;
	} else
		sshkey_free(k);
	return ret;
}

int
sshkey_cert_copy(const struct sshkey *from_key, struct sshkey *to_key)
{
	u_int i;
	const struct sshkey_cert *from;
	struct sshkey_cert *to;
	int ret = SSH_ERR_INTERNAL_ERROR;

	if (to_key->cert != NULL) {
		cert_free(to_key->cert);
		to_key->cert = NULL;
	}

	if ((from = from_key->cert) == NULL)
		return SSH_ERR_INVALID_ARGUMENT;

	if ((to = to_key->cert = cert_new()) == NULL)
		return SSH_ERR_ALLOC_FAIL;

	if ((ret = sshbuf_putb(to->certblob, from->certblob)) != 0 ||
	    (ret = sshbuf_putb(to->critical, from->critical)) != 0 ||
	    (ret = sshbuf_putb(to->extensions, from->extensions)) != 0)
		return ret;

	to->serial = from->serial;
	to->type = from->type;
	if (from->key_id == NULL)
		to->key_id = NULL;
	else if ((to->key_id = strdup(from->key_id)) == NULL)
		return SSH_ERR_ALLOC_FAIL;
	to->valid_after = from->valid_after;
	to->valid_before = from->valid_before;
	if (from->signature_key == NULL)
		to->signature_key = NULL;
	else if ((ret = sshkey_from_private(from->signature_key,
	    &to->signature_key)) != 0)
		return ret;

	if (from->nprincipals > SSHKEY_CERT_MAX_PRINCIPALS)
		return SSH_ERR_INVALID_ARGUMENT;
	if (from->nprincipals > 0) {
		if ((to->principals = calloc(from->nprincipals,
		    sizeof(*to->principals))) == NULL)
			return SSH_ERR_ALLOC_FAIL;
		for (i = 0; i < from->nprincipals; i++) {
			to->principals[i] = strdup(from->principals[i]);
			if (to->principals[i] == NULL) {
				to->nprincipals = i;
				return SSH_ERR_ALLOC_FAIL;
			}
		}
	}
	to->nprincipals = from->nprincipals;
	return 0;
}

int
sshkey_from_private(const struct sshkey *k, struct sshkey **pkp)
{
	struct sshkey *n = NULL;
	int ret = SSH_ERR_INTERNAL_ERROR;

	*pkp = NULL;
	switch (k->type) {
#ifdef WITH_OPENSSL
	case KEY_DSA:
	case KEY_DSA_CERT:
		if ((n = sshkey_new(k->type)) == NULL)
			return SSH_ERR_ALLOC_FAIL;
		if ((BN_copy(n->dsa->p, k->dsa->p) == NULL) ||
		    (BN_copy(n->dsa->q, k->dsa->q) == NULL) ||
		    (BN_copy(n->dsa->g, k->dsa->g) == NULL) ||
		    (BN_copy(n->dsa->pub_key, k->dsa->pub_key) == NULL)) {
			sshkey_free(n);
			return SSH_ERR_ALLOC_FAIL;
		}
		break;
# ifdef OPENSSL_HAS_ECC
	case KEY_ECDSA:
	case KEY_ECDSA_CERT:
		if ((n = sshkey_new(k->type)) == NULL)
			return SSH_ERR_ALLOC_FAIL;
		n->ecdsa_nid = k->ecdsa_nid;
		n->ecdsa = EC_KEY_new_by_curve_name(k->ecdsa_nid);
		if (n->ecdsa == NULL) {
			sshkey_free(n);
			return SSH_ERR_ALLOC_FAIL;
		}
		if (EC_KEY_set_public_key(n->ecdsa,
		    EC_KEY_get0_public_key(k->ecdsa)) != 1) {
			sshkey_free(n);
			return SSH_ERR_LIBCRYPTO_ERROR;
		}
		break;
# endif /* OPENSSL_HAS_ECC */
	case KEY_RSA:
	case KEY_RSA1:
	case KEY_RSA_CERT:
		if ((n = sshkey_new(k->type)) == NULL)
			return SSH_ERR_ALLOC_FAIL;
		if ((BN_copy(n->rsa->n, k->rsa->n) == NULL) ||
		    (BN_copy(n->rsa->e, k->rsa->e) == NULL)) {
			sshkey_free(n);
			return SSH_ERR_ALLOC_FAIL;
		}
		break;
#endif /* WITH_OPENSSL */
	case KEY_ED25519:
	case KEY_ED25519_CERT:
		if ((n = sshkey_new(k->type)) == NULL)
			return SSH_ERR_ALLOC_FAIL;
		if (k->ed25519_pk != NULL) {
			if ((n->ed25519_pk = malloc(ED25519_PK_SZ)) == NULL) {
				sshkey_free(n);
				return SSH_ERR_ALLOC_FAIL;
			}
			memcpy(n->ed25519_pk, k->ed25519_pk, ED25519_PK_SZ);
		}
		break;
	default:
		return SSH_ERR_KEY_TYPE_UNKNOWN;
	}
	if (sshkey_is_cert(k)) {
		if ((ret = sshkey_cert_copy(k, n)) != 0) {
			sshkey_free(n);
			return ret;
		}
	}
	*pkp = n;
	return 0;
}

static int
cert_parse(struct sshbuf *b, struct sshkey *key, struct sshbuf *certbuf)
{
	struct sshbuf *principals = NULL, *crit = NULL;
	struct sshbuf *exts = NULL, *ca = NULL;
	u_char *sig = NULL;
	size_t signed_len = 0, slen = 0, kidlen = 0;
	int ret = SSH_ERR_INTERNAL_ERROR;

	/* Copy the entire key blob for verification and later serialisation */
	if ((ret = sshbuf_putb(key->cert->certblob, certbuf)) != 0)
		return ret;

	/* Parse body of certificate up to signature */
	if ((ret = sshbuf_get_u64(b, &key->cert->serial)) != 0 ||
	    (ret = sshbuf_get_u32(b, &key->cert->type)) != 0 ||
	    (ret = sshbuf_get_cstring(b, &key->cert->key_id, &kidlen)) != 0 ||
	    (ret = sshbuf_froms(b, &principals)) != 0 ||
	    (ret = sshbuf_get_u64(b, &key->cert->valid_after)) != 0 ||
	    (ret = sshbuf_get_u64(b, &key->cert->valid_before)) != 0 ||
	    (ret = sshbuf_froms(b, &crit)) != 0 ||
	    (ret = sshbuf_froms(b, &exts)) != 0 ||
	    (ret = sshbuf_get_string_direct(b, NULL, NULL)) != 0 ||
	    (ret = sshbuf_froms(b, &ca)) != 0) {
		/* XXX debug print error for ret */
		ret = SSH_ERR_INVALID_FORMAT;
		goto out;
	}

	/* Signature is left in the buffer so we can calculate this length */
	signed_len = sshbuf_len(key->cert->certblob) - sshbuf_len(b);

	if ((ret = sshbuf_get_string(b, &sig, &slen)) != 0) {
		ret = SSH_ERR_INVALID_FORMAT;
		goto out;
	}

	if (key->cert->type != SSH2_CERT_TYPE_USER &&
	    key->cert->type != SSH2_CERT_TYPE_HOST) {
		ret = SSH_ERR_KEY_CERT_UNKNOWN_TYPE;
		goto out;
	}

	/* Parse principals section */
	while (sshbuf_len(principals) > 0) {
		char *principal = NULL;
		char **oprincipals = NULL;

		if (key->cert->nprincipals >= SSHKEY_CERT_MAX_PRINCIPALS) {
			ret = SSH_ERR_INVALID_FORMAT;
			goto out;
		}
		if ((ret = sshbuf_get_cstring(principals, &principal,
		    NULL)) != 0) {
			ret = SSH_ERR_INVALID_FORMAT;
			goto out;
		}
		oprincipals = key->cert->principals;
		key->cert->principals = reallocarray(key->cert->principals,
		    key->cert->nprincipals + 1, sizeof(*key->cert->principals));
		if (key->cert->principals == NULL) {
			free(principal);
			key->cert->principals = oprincipals;
			ret = SSH_ERR_ALLOC_FAIL;
			goto out;
		}
		key->cert->principals[key->cert->nprincipals++] = principal;
	}

	/*
	 * Stash a copies of the critical options and extensions sections
	 * for later use.
	 */
	if ((ret = sshbuf_putb(key->cert->critical, crit)) != 0 ||
	    (exts != NULL &&
	    (ret = sshbuf_putb(key->cert->extensions, exts)) != 0))
		goto out;

	/*
	 * Validate critical options and extensions sections format.
	 */
	while (sshbuf_len(crit) != 0) {
		if ((ret = sshbuf_get_string_direct(crit, NULL, NULL)) != 0 ||
		    (ret = sshbuf_get_string_direct(crit, NULL, NULL)) != 0) {
			sshbuf_reset(key->cert->critical);
			ret = SSH_ERR_INVALID_FORMAT;
			goto out;
		}
	}
	while (exts != NULL && sshbuf_len(exts) != 0) {
		if ((ret = sshbuf_get_string_direct(exts, NULL, NULL)) != 0 ||
		    (ret = sshbuf_get_string_direct(exts, NULL, NULL)) != 0) {
			sshbuf_reset(key->cert->extensions);
			ret = SSH_ERR_INVALID_FORMAT;
			goto out;
		}
	}

	/* Parse CA key and check signature */
	if (sshkey_from_blob_internal(ca, &key->cert->signature_key, 0) != 0) {
		ret = SSH_ERR_KEY_CERT_INVALID_SIGN_KEY;
		goto out;
	}
	if (!sshkey_type_is_valid_ca(key->cert->signature_key->type)) {
		ret = SSH_ERR_KEY_CERT_INVALID_SIGN_KEY;
		goto out;
	}
	if ((ret = sshkey_verify(key->cert->signature_key, sig, slen,
	    sshbuf_ptr(key->cert->certblob), signed_len, 0)) != 0)
		goto out;

	/* Success */
	ret = 0;
 out:
	sshbuf_free(ca);
	sshbuf_free(crit);
	sshbuf_free(exts);
	sshbuf_free(principals);
	free(sig);
	return ret;
}

static int
sshkey_from_blob_internal(struct sshbuf *b, struct sshkey **keyp,
    int allow_cert)
{
	int type, ret = SSH_ERR_INTERNAL_ERROR;
	char *ktype = NULL, *curve = NULL;
	struct sshkey *key = NULL;
	size_t len;
	u_char *pk = NULL;
	struct sshbuf *copy;
#if defined(WITH_OPENSSL) && defined(OPENSSL_HAS_ECC)
	EC_POINT *q = NULL;
#endif /* WITH_OPENSSL && OPENSSL_HAS_ECC */

#ifdef DEBUG_PK /* XXX */
	sshbuf_dump(b, stderr);
#endif
	*keyp = NULL;
	if ((copy = sshbuf_fromb(b)) == NULL) {
		ret = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	if (sshbuf_get_cstring(b, &ktype, NULL) != 0) {
		ret = SSH_ERR_INVALID_FORMAT;
		goto out;
	}

	type = sshkey_type_from_name(ktype);
	if (!allow_cert && sshkey_type_is_cert(type)) {
		ret = SSH_ERR_KEY_CERT_INVALID_SIGN_KEY;
		goto out;
	}
	switch (type) {
#ifdef WITH_OPENSSL
	case KEY_RSA_CERT:
		/* Skip nonce */
		if (sshbuf_get_string_direct(b, NULL, NULL) != 0) {
			ret = SSH_ERR_INVALID_FORMAT;
			goto out;
		}
		/* FALLTHROUGH */
	case KEY_RSA:
		if ((key = sshkey_new(type)) == NULL) {
			ret = SSH_ERR_ALLOC_FAIL;
			goto out;
		}
		if (sshbuf_get_bignum2(b, key->rsa->e) != 0 ||
		    sshbuf_get_bignum2(b, key->rsa->n) != 0) {
			ret = SSH_ERR_INVALID_FORMAT;
			goto out;
		}
#ifdef DEBUG_PK
		RSA_print_fp(stderr, key->rsa, 8);
#endif
		break;
	case KEY_DSA_CERT:
		/* Skip nonce */
		if (sshbuf_get_string_direct(b, NULL, NULL) != 0) {
			ret = SSH_ERR_INVALID_FORMAT;
			goto out;
		}
		/* FALLTHROUGH */
	case KEY_DSA:
		if ((key = sshkey_new(type)) == NULL) {
			ret = SSH_ERR_ALLOC_FAIL;
			goto out;
		}
		if (sshbuf_get_bignum2(b, key->dsa->p) != 0 ||
		    sshbuf_get_bignum2(b, key->dsa->q) != 0 ||
		    sshbuf_get_bignum2(b, key->dsa->g) != 0 ||
		    sshbuf_get_bignum2(b, key->dsa->pub_key) != 0) {
			ret = SSH_ERR_INVALID_FORMAT;
			goto out;
		}
#ifdef DEBUG_PK
		DSA_print_fp(stderr, key->dsa, 8);
#endif
		break;
	case KEY_ECDSA_CERT:
		/* Skip nonce */
		if (sshbuf_get_string_direct(b, NULL, NULL) != 0) {
			ret = SSH_ERR_INVALID_FORMAT;
			goto out;
		}
		/* FALLTHROUGH */
# ifdef OPENSSL_HAS_ECC
	case KEY_ECDSA:
		if ((key = sshkey_new(type)) == NULL) {
			ret = SSH_ERR_ALLOC_FAIL;
			goto out;
		}
		key->ecdsa_nid = sshkey_ecdsa_nid_from_name(ktype);
		if (sshbuf_get_cstring(b, &curve, NULL) != 0) {
			ret = SSH_ERR_INVALID_FORMAT;
			goto out;
		}
		if (key->ecdsa_nid != sshkey_curve_name_to_nid(curve)) {
			ret = SSH_ERR_EC_CURVE_MISMATCH;
			goto out;
		}
		if (key->ecdsa != NULL)
			EC_KEY_free(key->ecdsa);
		if ((key->ecdsa = EC_KEY_new_by_curve_name(key->ecdsa_nid))
		    == NULL) {
			ret = SSH_ERR_EC_CURVE_INVALID;
			goto out;
		}
		if ((q = EC_POINT_new(EC_KEY_get0_group(key->ecdsa))) == NULL) {
			ret = SSH_ERR_ALLOC_FAIL;
			goto out;
		}
		if (sshbuf_get_ec(b, q, EC_KEY_get0_group(key->ecdsa)) != 0) {
			ret = SSH_ERR_INVALID_FORMAT;
			goto out;
		}
		if (sshkey_ec_validate_public(EC_KEY_get0_group(key->ecdsa),
		    q) != 0) {
			ret = SSH_ERR_KEY_INVALID_EC_VALUE;
			goto out;
		}
		if (EC_KEY_set_public_key(key->ecdsa, q) != 1) {
			/* XXX assume it is a allocation error */
			ret = SSH_ERR_ALLOC_FAIL;
			goto out;
		}
#ifdef DEBUG_PK
		sshkey_dump_ec_point(EC_KEY_get0_group(key->ecdsa), q);
#endif
		break;
# endif /* OPENSSL_HAS_ECC */
#endif /* WITH_OPENSSL */
	case KEY_ED25519_CERT:
		/* Skip nonce */
		if (sshbuf_get_string_direct(b, NULL, NULL) != 0) {
			ret = SSH_ERR_INVALID_FORMAT;
			goto out;
		}
		/* FALLTHROUGH */
	case KEY_ED25519:
		if ((ret = sshbuf_get_string(b, &pk, &len)) != 0)
			goto out;
		if (len != ED25519_PK_SZ) {
			ret = SSH_ERR_INVALID_FORMAT;
			goto out;
		}
		if ((key = sshkey_new(type)) == NULL) {
			ret = SSH_ERR_ALLOC_FAIL;
			goto out;
		}
		key->ed25519_pk = pk;
		pk = NULL;
		break;
	case KEY_UNSPEC:
		if ((key = sshkey_new(type)) == NULL) {
			ret = SSH_ERR_ALLOC_FAIL;
			goto out;
		}
		break;
	default:
		ret = SSH_ERR_KEY_TYPE_UNKNOWN;
		goto out;
	}

	/* Parse certificate potion */
	if (sshkey_is_cert(key) && (ret = cert_parse(b, key, copy)) != 0)
		goto out;

	if (key != NULL && sshbuf_len(b) != 0) {
		ret = SSH_ERR_INVALID_FORMAT;
		goto out;
	}
	ret = 0;
	*keyp = key;
	key = NULL;
 out:
	sshbuf_free(copy);
	sshkey_free(key);
	free(ktype);
	free(curve);
	free(pk);
#if defined(WITH_OPENSSL) && defined(OPENSSL_HAS_ECC)
	if (q != NULL)
		EC_POINT_free(q);
#endif /* WITH_OPENSSL && OPENSSL_HAS_ECC */
	return ret;
}

int
sshkey_from_blob(const u_char *blob, size_t blen, struct sshkey **keyp)
{
	struct sshbuf *b;
	int r;

	if ((b = sshbuf_from(blob, blen)) == NULL)
		return SSH_ERR_ALLOC_FAIL;
	r = sshkey_from_blob_internal(b, keyp, 1);
	sshbuf_free(b);
	return r;
}

int
sshkey_fromb(struct sshbuf *b, struct sshkey **keyp)
{
	return sshkey_from_blob_internal(b, keyp, 1);
}

int
sshkey_froms(struct sshbuf *buf, struct sshkey **keyp)
{
	struct sshbuf *b;
	int r;

	if ((r = sshbuf_froms(buf, &b)) != 0)
		return r;
	r = sshkey_from_blob_internal(b, keyp, 1);
	sshbuf_free(b);
	return r;
}

int
sshkey_sign(const struct sshkey *key,
    u_char **sigp, size_t *lenp,
    const u_char *data, size_t datalen, const char *alg, u_int compat)
{
	if (sigp != NULL)
		*sigp = NULL;
	if (lenp != NULL)
		*lenp = 0;
	if (datalen > SSH_KEY_MAX_SIGN_DATA_SIZE)
		return SSH_ERR_INVALID_ARGUMENT;
	switch (key->type) {
#ifdef WITH_OPENSSL
	case KEY_DSA_CERT:
	case KEY_DSA:
		return ssh_dss_sign(key, sigp, lenp, data, datalen, compat);
# ifdef OPENSSL_HAS_ECC
	case KEY_ECDSA_CERT:
	case KEY_ECDSA:
		return ssh_ecdsa_sign(key, sigp, lenp, data, datalen, compat);
# endif /* OPENSSL_HAS_ECC */
	case KEY_RSA_CERT:
	case KEY_RSA:
		return ssh_rsa_sign(key, sigp, lenp, data, datalen, alg);
#endif /* WITH_OPENSSL */
	case KEY_ED25519:
	case KEY_ED25519_CERT:
		return ssh_ed25519_sign(key, sigp, lenp, data, datalen, compat);
	default:
		return SSH_ERR_KEY_TYPE_UNKNOWN;
	}
}

/*
 * ssh_key_verify returns 0 for a correct signature  and < 0 on error.
 */
int
sshkey_verify(const struct sshkey *key,
    const u_char *sig, size_t siglen,
    const u_char *data, size_t dlen, u_int compat)
{
	if (siglen == 0 || dlen > SSH_KEY_MAX_SIGN_DATA_SIZE)
		return SSH_ERR_INVALID_ARGUMENT;
	switch (key->type) {
#ifdef WITH_OPENSSL
	case KEY_DSA_CERT:
	case KEY_DSA:
		return ssh_dss_verify(key, sig, siglen, data, dlen, compat);
# ifdef OPENSSL_HAS_ECC
	case KEY_ECDSA_CERT:
	case KEY_ECDSA:
		return ssh_ecdsa_verify(key, sig, siglen, data, dlen, compat);
# endif /* OPENSSL_HAS_ECC */
	case KEY_RSA_CERT:
	case KEY_RSA:
		return ssh_rsa_verify(key, sig, siglen, data, dlen);
#endif /* WITH_OPENSSL */
	case KEY_ED25519:
	case KEY_ED25519_CERT:
		return ssh_ed25519_verify(key, sig, siglen, data, dlen, compat);
	default:
		return SSH_ERR_KEY_TYPE_UNKNOWN;
	}
}

/* Converts a private to a public key */
int
sshkey_demote(const struct sshkey *k, struct sshkey **dkp)
{
	struct sshkey *pk;
	int ret = SSH_ERR_INTERNAL_ERROR;

	*dkp = NULL;
	if ((pk = calloc(1, sizeof(*pk))) == NULL)
		return SSH_ERR_ALLOC_FAIL;
	pk->type = k->type;
	pk->flags = k->flags;
	pk->ecdsa_nid = k->ecdsa_nid;
	pk->dsa = NULL;
	pk->ecdsa = NULL;
	pk->rsa = NULL;
	pk->ed25519_pk = NULL;
	pk->ed25519_sk = NULL;

	switch (k->type) {
#ifdef WITH_OPENSSL
	case KEY_RSA_CERT:
		if ((ret = sshkey_cert_copy(k, pk)) != 0)
			goto fail;
		/* FALLTHROUGH */
	case KEY_RSA1:
	case KEY_RSA:
		if ((pk->rsa = RSA_new()) == NULL ||
		    (pk->rsa->e = BN_dup(k->rsa->e)) == NULL ||
		    (pk->rsa->n = BN_dup(k->rsa->n)) == NULL) {
			ret = SSH_ERR_ALLOC_FAIL;
			goto fail;
			}
		break;
	case KEY_DSA_CERT:
		if ((ret = sshkey_cert_copy(k, pk)) != 0)
			goto fail;
		/* FALLTHROUGH */
	case KEY_DSA:
		if ((pk->dsa = DSA_new()) == NULL ||
		    (pk->dsa->p = BN_dup(k->dsa->p)) == NULL ||
		    (pk->dsa->q = BN_dup(k->dsa->q)) == NULL ||
		    (pk->dsa->g = BN_dup(k->dsa->g)) == NULL ||
		    (pk->dsa->pub_key = BN_dup(k->dsa->pub_key)) == NULL) {
			ret = SSH_ERR_ALLOC_FAIL;
			goto fail;
		}
		break;
	case KEY_ECDSA_CERT:
		if ((ret = sshkey_cert_copy(k, pk)) != 0)
			goto fail;
		/* FALLTHROUGH */
# ifdef OPENSSL_HAS_ECC
	case KEY_ECDSA:
		pk->ecdsa = EC_KEY_new_by_curve_name(pk->ecdsa_nid);
		if (pk->ecdsa == NULL) {
			ret = SSH_ERR_ALLOC_FAIL;
			goto fail;
		}
		if (EC_KEY_set_public_key(pk->ecdsa,
		    EC_KEY_get0_public_key(k->ecdsa)) != 1) {
			ret = SSH_ERR_LIBCRYPTO_ERROR;
			goto fail;
		}
		break;
# endif /* OPENSSL_HAS_ECC */
#endif /* WITH_OPENSSL */
	case KEY_ED25519_CERT:
		if ((ret = sshkey_cert_copy(k, pk)) != 0)
			goto fail;
		/* FALLTHROUGH */
	case KEY_ED25519:
		if (k->ed25519_pk != NULL) {
			if ((pk->ed25519_pk = malloc(ED25519_PK_SZ)) == NULL) {
				ret = SSH_ERR_ALLOC_FAIL;
				goto fail;
			}
			memcpy(pk->ed25519_pk, k->ed25519_pk, ED25519_PK_SZ);
		}
		break;
	default:
		ret = SSH_ERR_KEY_TYPE_UNKNOWN;
 fail:
		sshkey_free(pk);
		return ret;
	}
	*dkp = pk;
	return 0;
}

/* Convert a plain key to their _CERT equivalent */
int
sshkey_to_certified(struct sshkey *k)
{
	int newtype;

	switch (k->type) {
#ifdef WITH_OPENSSL
	case KEY_RSA:
		newtype = KEY_RSA_CERT;
		break;
	case KEY_DSA:
		newtype = KEY_DSA_CERT;
		break;
	case KEY_ECDSA:
		newtype = KEY_ECDSA_CERT;
		break;
#endif /* WITH_OPENSSL */
	case KEY_ED25519:
		newtype = KEY_ED25519_CERT;
		break;
	default:
		return SSH_ERR_INVALID_ARGUMENT;
	}
	if ((k->cert = cert_new()) == NULL)
		return SSH_ERR_ALLOC_FAIL;
	k->type = newtype;
	return 0;
}

/* Convert a certificate to its raw key equivalent */
int
sshkey_drop_cert(struct sshkey *k)
{
	if (!sshkey_type_is_cert(k->type))
		return SSH_ERR_KEY_TYPE_UNKNOWN;
	cert_free(k->cert);
	k->cert = NULL;
	k->type = sshkey_type_plain(k->type);
	return 0;
}

/* Sign a certified key, (re-)generating the signed certblob. */
int
sshkey_certify(struct sshkey *k, struct sshkey *ca)
{
	struct sshbuf *principals = NULL;
	u_char *ca_blob = NULL, *sig_blob = NULL, nonce[32];
	size_t i, ca_len, sig_len;
	int ret = SSH_ERR_INTERNAL_ERROR;
	struct sshbuf *cert;

	if (k == NULL || k->cert == NULL ||
	    k->cert->certblob == NULL || ca == NULL)
		return SSH_ERR_INVALID_ARGUMENT;
	if (!sshkey_is_cert(k))
		return SSH_ERR_KEY_TYPE_UNKNOWN;
	if (!sshkey_type_is_valid_ca(ca->type))
		return SSH_ERR_KEY_CERT_INVALID_SIGN_KEY;

	if ((ret = sshkey_to_blob(ca, &ca_blob, &ca_len)) != 0)
		return SSH_ERR_KEY_CERT_INVALID_SIGN_KEY;

	cert = k->cert->certblob; /* for readability */
	sshbuf_reset(cert);
	if ((ret = sshbuf_put_cstring(cert, sshkey_ssh_name(k))) != 0)
		goto out;

	/* -v01 certs put nonce first */
	arc4random_buf(&nonce, sizeof(nonce));
	if ((ret = sshbuf_put_string(cert, nonce, sizeof(nonce))) != 0)
		goto out;

	/* XXX this substantially duplicates to_blob(); refactor */
	switch (k->type) {
#ifdef WITH_OPENSSL
	case KEY_DSA_CERT:
		if ((ret = sshbuf_put_bignum2(cert, k->dsa->p)) != 0 ||
		    (ret = sshbuf_put_bignum2(cert, k->dsa->q)) != 0 ||
		    (ret = sshbuf_put_bignum2(cert, k->dsa->g)) != 0 ||
		    (ret = sshbuf_put_bignum2(cert, k->dsa->pub_key)) != 0)
			goto out;
		break;
# ifdef OPENSSL_HAS_ECC
	case KEY_ECDSA_CERT:
		if ((ret = sshbuf_put_cstring(cert,
		    sshkey_curve_nid_to_name(k->ecdsa_nid))) != 0 ||
		    (ret = sshbuf_put_ec(cert,
		    EC_KEY_get0_public_key(k->ecdsa),
		    EC_KEY_get0_group(k->ecdsa))) != 0)
			goto out;
		break;
# endif /* OPENSSL_HAS_ECC */
	case KEY_RSA_CERT:
		if ((ret = sshbuf_put_bignum2(cert, k->rsa->e)) != 0 ||
		    (ret = sshbuf_put_bignum2(cert, k->rsa->n)) != 0)
			goto out;
		break;
#endif /* WITH_OPENSSL */
	case KEY_ED25519_CERT:
		if ((ret = sshbuf_put_string(cert,
		    k->ed25519_pk, ED25519_PK_SZ)) != 0)
			goto out;
		break;
	default:
		ret = SSH_ERR_INVALID_ARGUMENT;
		goto out;
	}

	if ((ret = sshbuf_put_u64(cert, k->cert->serial)) != 0 ||
	    (ret = sshbuf_put_u32(cert, k->cert->type)) != 0 ||
	    (ret = sshbuf_put_cstring(cert, k->cert->key_id)) != 0)
		goto out;

	if ((principals = sshbuf_new()) == NULL) {
		ret = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	for (i = 0; i < k->cert->nprincipals; i++) {
		if ((ret = sshbuf_put_cstring(principals,
		    k->cert->principals[i])) != 0)
			goto out;
	}
	if ((ret = sshbuf_put_stringb(cert, principals)) != 0 ||
	    (ret = sshbuf_put_u64(cert, k->cert->valid_after)) != 0 ||
	    (ret = sshbuf_put_u64(cert, k->cert->valid_before)) != 0 ||
	    (ret = sshbuf_put_stringb(cert, k->cert->critical)) != 0 ||
	    (ret = sshbuf_put_stringb(cert, k->cert->extensions)) != 0 ||
	    (ret = sshbuf_put_string(cert, NULL, 0)) != 0 || /* Reserved */
	    (ret = sshbuf_put_string(cert, ca_blob, ca_len)) != 0)
		goto out;

	/* Sign the whole mess */
	if ((ret = sshkey_sign(ca, &sig_blob, &sig_len, sshbuf_ptr(cert),
	    sshbuf_len(cert), NULL, 0)) != 0)
		goto out;

	/* Append signature and we are done */
	if ((ret = sshbuf_put_string(cert, sig_blob, sig_len)) != 0)
		goto out;
	ret = 0;
 out:
	if (ret != 0)
		sshbuf_reset(cert);
	free(sig_blob);
	free(ca_blob);
	sshbuf_free(principals);
	return ret;
}

int
sshkey_cert_check_authority(const struct sshkey *k,
    int want_host, int require_principal,
    const char *name, const char **reason)
{
	u_int i, principal_matches;
	time_t now = time(NULL);

	if (reason != NULL)
		*reason = NULL;

	if (want_host) {
		if (k->cert->type != SSH2_CERT_TYPE_HOST) {
			*reason = "Certificate invalid: not a host certificate";
			return SSH_ERR_KEY_CERT_INVALID;
		}
	} else {
		if (k->cert->type != SSH2_CERT_TYPE_USER) {
			*reason = "Certificate invalid: not a user certificate";
			return SSH_ERR_KEY_CERT_INVALID;
		}
	}
	if (now < 0) {
		/* yikes - system clock before epoch! */
		*reason = "Certificate invalid: not yet valid";
		return SSH_ERR_KEY_CERT_INVALID;
	}
	if ((u_int64_t)now < k->cert->valid_after) {
		*reason = "Certificate invalid: not yet valid";
		return SSH_ERR_KEY_CERT_INVALID;
	}
	if ((u_int64_t)now >= k->cert->valid_before) {
		*reason = "Certificate invalid: expired";
		return SSH_ERR_KEY_CERT_INVALID;
	}
	if (k->cert->nprincipals == 0) {
		if (require_principal) {
			*reason = "Certificate lacks principal list";
			return SSH_ERR_KEY_CERT_INVALID;
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
			return SSH_ERR_KEY_CERT_INVALID;
		}
	}
	return 0;
}

size_t
sshkey_format_cert_validity(const struct sshkey_cert *cert, char *s, size_t l)
{
	char from[32], to[32], ret[64];
	time_t tt;
	struct tm *tm;

	*from = *to = '\0';
	if (cert->valid_after == 0 &&
	    cert->valid_before == 0xffffffffffffffffULL)
		return strlcpy(s, "forever", l);

	if (cert->valid_after != 0) {
		/* XXX revisit INT_MAX in 2038 :) */
		tt = cert->valid_after > INT_MAX ?
		    INT_MAX : cert->valid_after;
		tm = localtime(&tt);
		strftime(from, sizeof(from), "%Y-%m-%dT%H:%M:%S", tm);
	}
	if (cert->valid_before != 0xffffffffffffffffULL) {
		/* XXX revisit INT_MAX in 2038 :) */
		tt = cert->valid_before > INT_MAX ?
		    INT_MAX : cert->valid_before;
		tm = localtime(&tt);
		strftime(to, sizeof(to), "%Y-%m-%dT%H:%M:%S", tm);
	}

	if (cert->valid_after == 0)
		snprintf(ret, sizeof(ret), "before %s", to);
	else if (cert->valid_before == 0xffffffffffffffffULL)
		snprintf(ret, sizeof(ret), "after %s", from);
	else
		snprintf(ret, sizeof(ret), "from %s to %s", from, to);

	return strlcpy(s, ret, l);
}

int
sshkey_private_serialize(const struct sshkey *key, struct sshbuf *b)
{
	int r = SSH_ERR_INTERNAL_ERROR;

	if ((r = sshbuf_put_cstring(b, sshkey_ssh_name(key))) != 0)
		goto out;
	switch (key->type) {
#ifdef WITH_OPENSSL
	case KEY_RSA:
		if ((r = sshbuf_put_bignum2(b, key->rsa->n)) != 0 ||
		    (r = sshbuf_put_bignum2(b, key->rsa->e)) != 0 ||
		    (r = sshbuf_put_bignum2(b, key->rsa->d)) != 0 ||
		    (r = sshbuf_put_bignum2(b, key->rsa->iqmp)) != 0 ||
		    (r = sshbuf_put_bignum2(b, key->rsa->p)) != 0 ||
		    (r = sshbuf_put_bignum2(b, key->rsa->q)) != 0)
			goto out;
		break;
	case KEY_RSA_CERT:
		if (key->cert == NULL || sshbuf_len(key->cert->certblob) == 0) {
			r = SSH_ERR_INVALID_ARGUMENT;
			goto out;
		}
		if ((r = sshbuf_put_stringb(b, key->cert->certblob)) != 0 ||
		    (r = sshbuf_put_bignum2(b, key->rsa->d)) != 0 ||
		    (r = sshbuf_put_bignum2(b, key->rsa->iqmp)) != 0 ||
		    (r = sshbuf_put_bignum2(b, key->rsa->p)) != 0 ||
		    (r = sshbuf_put_bignum2(b, key->rsa->q)) != 0)
			goto out;
		break;
	case KEY_DSA:
		if ((r = sshbuf_put_bignum2(b, key->dsa->p)) != 0 ||
		    (r = sshbuf_put_bignum2(b, key->dsa->q)) != 0 ||
		    (r = sshbuf_put_bignum2(b, key->dsa->g)) != 0 ||
		    (r = sshbuf_put_bignum2(b, key->dsa->pub_key)) != 0 ||
		    (r = sshbuf_put_bignum2(b, key->dsa->priv_key)) != 0)
			goto out;
		break;
	case KEY_DSA_CERT:
		if (key->cert == NULL || sshbuf_len(key->cert->certblob) == 0) {
			r = SSH_ERR_INVALID_ARGUMENT;
			goto out;
		}
		if ((r = sshbuf_put_stringb(b, key->cert->certblob)) != 0 ||
		    (r = sshbuf_put_bignum2(b, key->dsa->priv_key)) != 0)
			goto out;
		break;
# ifdef OPENSSL_HAS_ECC
	case KEY_ECDSA:
		if ((r = sshbuf_put_cstring(b,
		    sshkey_curve_nid_to_name(key->ecdsa_nid))) != 0 ||
		    (r = sshbuf_put_eckey(b, key->ecdsa)) != 0 ||
		    (r = sshbuf_put_bignum2(b,
		    EC_KEY_get0_private_key(key->ecdsa))) != 0)
			goto out;
		break;
	case KEY_ECDSA_CERT:
		if (key->cert == NULL || sshbuf_len(key->cert->certblob) == 0) {
			r = SSH_ERR_INVALID_ARGUMENT;
			goto out;
		}
		if ((r = sshbuf_put_stringb(b, key->cert->certblob)) != 0 ||
		    (r = sshbuf_put_bignum2(b,
		    EC_KEY_get0_private_key(key->ecdsa))) != 0)
			goto out;
		break;
# endif /* OPENSSL_HAS_ECC */
#endif /* WITH_OPENSSL */
	case KEY_ED25519:
		if ((r = sshbuf_put_string(b, key->ed25519_pk,
		    ED25519_PK_SZ)) != 0 ||
		    (r = sshbuf_put_string(b, key->ed25519_sk,
		    ED25519_SK_SZ)) != 0)
			goto out;
		break;
	case KEY_ED25519_CERT:
		if (key->cert == NULL || sshbuf_len(key->cert->certblob) == 0) {
			r = SSH_ERR_INVALID_ARGUMENT;
			goto out;
		}
		if ((r = sshbuf_put_stringb(b, key->cert->certblob)) != 0 ||
		    (r = sshbuf_put_string(b, key->ed25519_pk,
		    ED25519_PK_SZ)) != 0 ||
		    (r = sshbuf_put_string(b, key->ed25519_sk,
		    ED25519_SK_SZ)) != 0)
			goto out;
		break;
	default:
		r = SSH_ERR_INVALID_ARGUMENT;
		goto out;
	}
	/* success */
	r = 0;
 out:
	return r;
}

int
sshkey_private_deserialize(struct sshbuf *buf, struct sshkey **kp)
{
	char *tname = NULL, *curve = NULL;
	struct sshkey *k = NULL;
	size_t pklen = 0, sklen = 0;
	int type, r = SSH_ERR_INTERNAL_ERROR;
	u_char *ed25519_pk = NULL, *ed25519_sk = NULL;
#ifdef WITH_OPENSSL
	BIGNUM *exponent = NULL;
#endif /* WITH_OPENSSL */

	if (kp != NULL)
		*kp = NULL;
	if ((r = sshbuf_get_cstring(buf, &tname, NULL)) != 0)
		goto out;
	type = sshkey_type_from_name(tname);
	switch (type) {
#ifdef WITH_OPENSSL
	case KEY_DSA:
		if ((k = sshkey_new_private(type)) == NULL) {
			r = SSH_ERR_ALLOC_FAIL;
			goto out;
		}
		if ((r = sshbuf_get_bignum2(buf, k->dsa->p)) != 0 ||
		    (r = sshbuf_get_bignum2(buf, k->dsa->q)) != 0 ||
		    (r = sshbuf_get_bignum2(buf, k->dsa->g)) != 0 ||
		    (r = sshbuf_get_bignum2(buf, k->dsa->pub_key)) != 0 ||
		    (r = sshbuf_get_bignum2(buf, k->dsa->priv_key)) != 0)
			goto out;
		break;
	case KEY_DSA_CERT:
		if ((r = sshkey_froms(buf, &k)) != 0 ||
		    (r = sshkey_add_private(k)) != 0 ||
		    (r = sshbuf_get_bignum2(buf, k->dsa->priv_key)) != 0)
			goto out;
		break;
# ifdef OPENSSL_HAS_ECC
	case KEY_ECDSA:
		if ((k = sshkey_new_private(type)) == NULL) {
			r = SSH_ERR_ALLOC_FAIL;
			goto out;
		}
		if ((k->ecdsa_nid = sshkey_ecdsa_nid_from_name(tname)) == -1) {
			r = SSH_ERR_INVALID_ARGUMENT;
			goto out;
		}
		if ((r = sshbuf_get_cstring(buf, &curve, NULL)) != 0)
			goto out;
		if (k->ecdsa_nid != sshkey_curve_name_to_nid(curve)) {
			r = SSH_ERR_EC_CURVE_MISMATCH;
			goto out;
		}
		k->ecdsa = EC_KEY_new_by_curve_name(k->ecdsa_nid);
		if (k->ecdsa  == NULL || (exponent = BN_new()) == NULL) {
			r = SSH_ERR_LIBCRYPTO_ERROR;
			goto out;
		}
		if ((r = sshbuf_get_eckey(buf, k->ecdsa)) != 0 ||
		    (r = sshbuf_get_bignum2(buf, exponent)))
			goto out;
		if (EC_KEY_set_private_key(k->ecdsa, exponent) != 1) {
			r = SSH_ERR_LIBCRYPTO_ERROR;
			goto out;
		}
		if ((r = sshkey_ec_validate_public(EC_KEY_get0_group(k->ecdsa),
		    EC_KEY_get0_public_key(k->ecdsa))) != 0 ||
		    (r = sshkey_ec_validate_private(k->ecdsa)) != 0)
			goto out;
		break;
	case KEY_ECDSA_CERT:
		if ((exponent = BN_new()) == NULL) {
			r = SSH_ERR_LIBCRYPTO_ERROR;
			goto out;
		}
		if ((r = sshkey_froms(buf, &k)) != 0 ||
		    (r = sshkey_add_private(k)) != 0 ||
		    (r = sshbuf_get_bignum2(buf, exponent)) != 0)
			goto out;
		if (EC_KEY_set_private_key(k->ecdsa, exponent) != 1) {
			r = SSH_ERR_LIBCRYPTO_ERROR;
			goto out;
		}
		if ((r = sshkey_ec_validate_public(EC_KEY_get0_group(k->ecdsa),
		    EC_KEY_get0_public_key(k->ecdsa))) != 0 ||
		    (r = sshkey_ec_validate_private(k->ecdsa)) != 0)
			goto out;
		break;
# endif /* OPENSSL_HAS_ECC */
	case KEY_RSA:
		if ((k = sshkey_new_private(type)) == NULL) {
			r = SSH_ERR_ALLOC_FAIL;
			goto out;
		}
		if ((r = sshbuf_get_bignum2(buf, k->rsa->n)) != 0 ||
		    (r = sshbuf_get_bignum2(buf, k->rsa->e)) != 0 ||
		    (r = sshbuf_get_bignum2(buf, k->rsa->d)) != 0 ||
		    (r = sshbuf_get_bignum2(buf, k->rsa->iqmp)) != 0 ||
		    (r = sshbuf_get_bignum2(buf, k->rsa->p)) != 0 ||
		    (r = sshbuf_get_bignum2(buf, k->rsa->q)) != 0 ||
		    (r = rsa_generate_additional_parameters(k->rsa)) != 0)
			goto out;
		break;
	case KEY_RSA_CERT:
		if ((r = sshkey_froms(buf, &k)) != 0 ||
		    (r = sshkey_add_private(k)) != 0 ||
		    (r = sshbuf_get_bignum2(buf, k->rsa->d)) != 0 ||
		    (r = sshbuf_get_bignum2(buf, k->rsa->iqmp)) != 0 ||
		    (r = sshbuf_get_bignum2(buf, k->rsa->p)) != 0 ||
		    (r = sshbuf_get_bignum2(buf, k->rsa->q)) != 0 ||
		    (r = rsa_generate_additional_parameters(k->rsa)) != 0)
			goto out;
		break;
#endif /* WITH_OPENSSL */
	case KEY_ED25519:
		if ((k = sshkey_new_private(type)) == NULL) {
			r = SSH_ERR_ALLOC_FAIL;
			goto out;
		}
		if ((r = sshbuf_get_string(buf, &ed25519_pk, &pklen)) != 0 ||
		    (r = sshbuf_get_string(buf, &ed25519_sk, &sklen)) != 0)
			goto out;
		if (pklen != ED25519_PK_SZ || sklen != ED25519_SK_SZ) {
			r = SSH_ERR_INVALID_FORMAT;
			goto out;
		}
		k->ed25519_pk = ed25519_pk;
		k->ed25519_sk = ed25519_sk;
		ed25519_pk = ed25519_sk = NULL;
		break;
	case KEY_ED25519_CERT:
		if ((r = sshkey_froms(buf, &k)) != 0 ||
		    (r = sshkey_add_private(k)) != 0 ||
		    (r = sshbuf_get_string(buf, &ed25519_pk, &pklen)) != 0 ||
		    (r = sshbuf_get_string(buf, &ed25519_sk, &sklen)) != 0)
			goto out;
		if (pklen != ED25519_PK_SZ || sklen != ED25519_SK_SZ) {
			r = SSH_ERR_INVALID_FORMAT;
			goto out;
		}
		k->ed25519_pk = ed25519_pk;
		k->ed25519_sk = ed25519_sk;
		ed25519_pk = ed25519_sk = NULL;
		break;
	default:
		r = SSH_ERR_KEY_TYPE_UNKNOWN;
		goto out;
	}
#ifdef WITH_OPENSSL
	/* enable blinding */
	switch (k->type) {
	case KEY_RSA:
	case KEY_RSA_CERT:
	case KEY_RSA1:
		if (RSA_blinding_on(k->rsa, NULL) != 1) {
			r = SSH_ERR_LIBCRYPTO_ERROR;
			goto out;
		}
		break;
	}
#endif /* WITH_OPENSSL */
	/* success */
	r = 0;
	if (kp != NULL) {
		*kp = k;
		k = NULL;
	}
 out:
	free(tname);
	free(curve);
#ifdef WITH_OPENSSL
	if (exponent != NULL)
		BN_clear_free(exponent);
#endif /* WITH_OPENSSL */
	sshkey_free(k);
	if (ed25519_pk != NULL) {
		explicit_bzero(ed25519_pk, pklen);
		free(ed25519_pk);
	}
	if (ed25519_sk != NULL) {
		explicit_bzero(ed25519_sk, sklen);
		free(ed25519_sk);
	}
	return r;
}

#if defined(WITH_OPENSSL) && defined(OPENSSL_HAS_ECC)
int
sshkey_ec_validate_public(const EC_GROUP *group, const EC_POINT *public)
{
	BN_CTX *bnctx;
	EC_POINT *nq = NULL;
	BIGNUM *order, *x, *y, *tmp;
	int ret = SSH_ERR_KEY_INVALID_EC_VALUE;

	if ((bnctx = BN_CTX_new()) == NULL)
		return SSH_ERR_ALLOC_FAIL;
	BN_CTX_start(bnctx);

	/*
	 * We shouldn't ever hit this case because bignum_get_ecpoint()
	 * refuses to load GF2m points.
	 */
	if (EC_METHOD_get_field_type(EC_GROUP_method_of(group)) !=
	    NID_X9_62_prime_field)
		goto out;

	/* Q != infinity */
	if (EC_POINT_is_at_infinity(group, public))
		goto out;

	if ((x = BN_CTX_get(bnctx)) == NULL ||
	    (y = BN_CTX_get(bnctx)) == NULL ||
	    (order = BN_CTX_get(bnctx)) == NULL ||
	    (tmp = BN_CTX_get(bnctx)) == NULL) {
		ret = SSH_ERR_ALLOC_FAIL;
		goto out;
	}

	/* log2(x) > log2(order)/2, log2(y) > log2(order)/2 */
	if (EC_GROUP_get_order(group, order, bnctx) != 1 ||
	    EC_POINT_get_affine_coordinates_GFp(group, public,
	    x, y, bnctx) != 1) {
		ret = SSH_ERR_LIBCRYPTO_ERROR;
		goto out;
	}
	if (BN_num_bits(x) <= BN_num_bits(order) / 2 ||
	    BN_num_bits(y) <= BN_num_bits(order) / 2)
		goto out;

	/* nQ == infinity (n == order of subgroup) */
	if ((nq = EC_POINT_new(group)) == NULL) {
		ret = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	if (EC_POINT_mul(group, nq, NULL, public, order, bnctx) != 1) {
		ret = SSH_ERR_LIBCRYPTO_ERROR;
		goto out;
	}
	if (EC_POINT_is_at_infinity(group, nq) != 1)
		goto out;

	/* x < order - 1, y < order - 1 */
	if (!BN_sub(tmp, order, BN_value_one())) {
		ret = SSH_ERR_LIBCRYPTO_ERROR;
		goto out;
	}
	if (BN_cmp(x, tmp) >= 0 || BN_cmp(y, tmp) >= 0)
		goto out;
	ret = 0;
 out:
	BN_CTX_free(bnctx);
	if (nq != NULL)
		EC_POINT_free(nq);
	return ret;
}

int
sshkey_ec_validate_private(const EC_KEY *key)
{
	BN_CTX *bnctx;
	BIGNUM *order, *tmp;
	int ret = SSH_ERR_KEY_INVALID_EC_VALUE;

	if ((bnctx = BN_CTX_new()) == NULL)
		return SSH_ERR_ALLOC_FAIL;
	BN_CTX_start(bnctx);

	if ((order = BN_CTX_get(bnctx)) == NULL ||
	    (tmp = BN_CTX_get(bnctx)) == NULL) {
		ret = SSH_ERR_ALLOC_FAIL;
		goto out;
	}

	/* log2(private) > log2(order)/2 */
	if (EC_GROUP_get_order(EC_KEY_get0_group(key), order, bnctx) != 1) {
		ret = SSH_ERR_LIBCRYPTO_ERROR;
		goto out;
	}
	if (BN_num_bits(EC_KEY_get0_private_key(key)) <=
	    BN_num_bits(order) / 2)
		goto out;

	/* private < order - 1 */
	if (!BN_sub(tmp, order, BN_value_one())) {
		ret = SSH_ERR_LIBCRYPTO_ERROR;
		goto out;
	}
	if (BN_cmp(EC_KEY_get0_private_key(key), tmp) >= 0)
		goto out;
	ret = 0;
 out:
	BN_CTX_free(bnctx);
	return ret;
}

void
sshkey_dump_ec_point(const EC_GROUP *group, const EC_POINT *point)
{
	BIGNUM *x, *y;
	BN_CTX *bnctx;

	if (point == NULL) {
		fputs("point=(NULL)\n", stderr);
		return;
	}
	if ((bnctx = BN_CTX_new()) == NULL) {
		fprintf(stderr, "%s: BN_CTX_new failed\n", __func__);
		return;
	}
	BN_CTX_start(bnctx);
	if ((x = BN_CTX_get(bnctx)) == NULL ||
	    (y = BN_CTX_get(bnctx)) == NULL) {
		fprintf(stderr, "%s: BN_CTX_get failed\n", __func__);
		return;
	}
	if (EC_METHOD_get_field_type(EC_GROUP_method_of(group)) !=
	    NID_X9_62_prime_field) {
		fprintf(stderr, "%s: group is not a prime field\n", __func__);
		return;
	}
	if (EC_POINT_get_affine_coordinates_GFp(group, point, x, y,
	    bnctx) != 1) {
		fprintf(stderr, "%s: EC_POINT_get_affine_coordinates_GFp\n",
		    __func__);
		return;
	}
	fputs("x=", stderr);
	BN_print_fp(stderr, x);
	fputs("\ny=", stderr);
	BN_print_fp(stderr, y);
	fputs("\n", stderr);
	BN_CTX_free(bnctx);
}

void
sshkey_dump_ec_key(const EC_KEY *key)
{
	const BIGNUM *exponent;

	sshkey_dump_ec_point(EC_KEY_get0_group(key),
	    EC_KEY_get0_public_key(key));
	fputs("exponent=", stderr);
	if ((exponent = EC_KEY_get0_private_key(key)) == NULL)
		fputs("(NULL)", stderr);
	else
		BN_print_fp(stderr, EC_KEY_get0_private_key(key));
	fputs("\n", stderr);
}
#endif /* WITH_OPENSSL && OPENSSL_HAS_ECC */

static int
sshkey_private_to_blob2(const struct sshkey *prv, struct sshbuf *blob,
    const char *passphrase, const char *comment, const char *ciphername,
    int rounds)
{
	u_char *cp, *key = NULL, *pubkeyblob = NULL;
	u_char salt[SALT_LEN];
	char *b64 = NULL;
	size_t i, pubkeylen, keylen, ivlen, blocksize, authlen;
	u_int check;
	int r = SSH_ERR_INTERNAL_ERROR;
	struct sshcipher_ctx ciphercontext;
	const struct sshcipher *cipher;
	const char *kdfname = KDFNAME;
	struct sshbuf *encoded = NULL, *encrypted = NULL, *kdf = NULL;

	memset(&ciphercontext, 0, sizeof(ciphercontext));

	if (rounds <= 0)
		rounds = DEFAULT_ROUNDS;
	if (passphrase == NULL || !strlen(passphrase)) {
		ciphername = "none";
		kdfname = "none";
	} else if (ciphername == NULL)
		ciphername = DEFAULT_CIPHERNAME;
	else if (cipher_number(ciphername) != SSH_CIPHER_SSH2) {
		r = SSH_ERR_INVALID_ARGUMENT;
		goto out;
	}
	if ((cipher = cipher_by_name(ciphername)) == NULL) {
		r = SSH_ERR_INTERNAL_ERROR;
		goto out;
	}

	if ((kdf = sshbuf_new()) == NULL ||
	    (encoded = sshbuf_new()) == NULL ||
	    (encrypted = sshbuf_new()) == NULL) {
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	blocksize = cipher_blocksize(cipher);
	keylen = cipher_keylen(cipher);
	ivlen = cipher_ivlen(cipher);
	authlen = cipher_authlen(cipher);
	if ((key = calloc(1, keylen + ivlen)) == NULL) {
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	if (strcmp(kdfname, "bcrypt") == 0) {
		arc4random_buf(salt, SALT_LEN);
		if (bcrypt_pbkdf(passphrase, strlen(passphrase),
		    salt, SALT_LEN, key, keylen + ivlen, rounds) < 0) {
			r = SSH_ERR_INVALID_ARGUMENT;
			goto out;
		}
		if ((r = sshbuf_put_string(kdf, salt, SALT_LEN)) != 0 ||
		    (r = sshbuf_put_u32(kdf, rounds)) != 0)
			goto out;
	} else if (strcmp(kdfname, "none") != 0) {
		/* Unsupported KDF type */
		r = SSH_ERR_KEY_UNKNOWN_CIPHER;
		goto out;
	}
	if ((r = cipher_init(&ciphercontext, cipher, key, keylen,
	    key + keylen, ivlen, 1)) != 0)
		goto out;

	if ((r = sshbuf_put(encoded, AUTH_MAGIC, sizeof(AUTH_MAGIC))) != 0 ||
	    (r = sshbuf_put_cstring(encoded, ciphername)) != 0 ||
	    (r = sshbuf_put_cstring(encoded, kdfname)) != 0 ||
	    (r = sshbuf_put_stringb(encoded, kdf)) != 0 ||
	    (r = sshbuf_put_u32(encoded, 1)) != 0 ||	/* number of keys */
	    (r = sshkey_to_blob(prv, &pubkeyblob, &pubkeylen)) != 0 ||
	    (r = sshbuf_put_string(encoded, pubkeyblob, pubkeylen)) != 0)
		goto out;

	/* set up the buffer that will be encrypted */

	/* Random check bytes */
	check = arc4random();
	if ((r = sshbuf_put_u32(encrypted, check)) != 0 ||
	    (r = sshbuf_put_u32(encrypted, check)) != 0)
		goto out;

	/* append private key and comment*/
	if ((r = sshkey_private_serialize(prv, encrypted)) != 0 ||
	    (r = sshbuf_put_cstring(encrypted, comment)) != 0)
		goto out;

	/* padding */
	i = 0;
	while (sshbuf_len(encrypted) % blocksize) {
		if ((r = sshbuf_put_u8(encrypted, ++i & 0xff)) != 0)
			goto out;
	}

	/* length in destination buffer */
	if ((r = sshbuf_put_u32(encoded, sshbuf_len(encrypted))) != 0)
		goto out;

	/* encrypt */
	if ((r = sshbuf_reserve(encoded,
	    sshbuf_len(encrypted) + authlen, &cp)) != 0)
		goto out;
	if ((r = cipher_crypt(&ciphercontext, 0, cp,
	    sshbuf_ptr(encrypted), sshbuf_len(encrypted), 0, authlen)) != 0)
		goto out;

	/* uuencode */
	if ((b64 = sshbuf_dtob64(encoded)) == NULL) {
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}

	sshbuf_reset(blob);
	if ((r = sshbuf_put(blob, MARK_BEGIN, MARK_BEGIN_LEN)) != 0)
		goto out;
	for (i = 0; i < strlen(b64); i++) {
		if ((r = sshbuf_put_u8(blob, b64[i])) != 0)
			goto out;
		/* insert line breaks */
		if (i % 70 == 69 && (r = sshbuf_put_u8(blob, '\n')) != 0)
			goto out;
	}
	if (i % 70 != 69 && (r = sshbuf_put_u8(blob, '\n')) != 0)
		goto out;
	if ((r = sshbuf_put(blob, MARK_END, MARK_END_LEN)) != 0)
		goto out;

	/* success */
	r = 0;

 out:
	sshbuf_free(kdf);
	sshbuf_free(encoded);
	sshbuf_free(encrypted);
	cipher_cleanup(&ciphercontext);
	explicit_bzero(salt, sizeof(salt));
	if (key != NULL) {
		explicit_bzero(key, keylen + ivlen);
		free(key);
	}
	if (pubkeyblob != NULL) {
		explicit_bzero(pubkeyblob, pubkeylen);
		free(pubkeyblob);
	}
	if (b64 != NULL) {
		explicit_bzero(b64, strlen(b64));
		free(b64);
	}
	return r;
}

static int
sshkey_parse_private2(struct sshbuf *blob, int type, const char *passphrase,
    struct sshkey **keyp, char **commentp)
{
	char *comment = NULL, *ciphername = NULL, *kdfname = NULL;
	const struct sshcipher *cipher = NULL;
	const u_char *cp;
	int r = SSH_ERR_INTERNAL_ERROR;
	size_t encoded_len;
	size_t i, keylen = 0, ivlen = 0, authlen = 0, slen = 0;
	struct sshbuf *encoded = NULL, *decoded = NULL;
	struct sshbuf *kdf = NULL, *decrypted = NULL;
	struct sshcipher_ctx ciphercontext;
	struct sshkey *k = NULL;
	u_char *key = NULL, *salt = NULL, *dp, pad, last;
	u_int blocksize, rounds, nkeys, encrypted_len, check1, check2;

	memset(&ciphercontext, 0, sizeof(ciphercontext));
	if (keyp != NULL)
		*keyp = NULL;
	if (commentp != NULL)
		*commentp = NULL;

	if ((encoded = sshbuf_new()) == NULL ||
	    (decoded = sshbuf_new()) == NULL ||
	    (decrypted = sshbuf_new()) == NULL) {
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}

	/* check preamble */
	cp = sshbuf_ptr(blob);
	encoded_len = sshbuf_len(blob);
	if (encoded_len < (MARK_BEGIN_LEN + MARK_END_LEN) ||
	    memcmp(cp, MARK_BEGIN, MARK_BEGIN_LEN) != 0) {
		r = SSH_ERR_INVALID_FORMAT;
		goto out;
	}
	cp += MARK_BEGIN_LEN;
	encoded_len -= MARK_BEGIN_LEN;

	/* Look for end marker, removing whitespace as we go */
	while (encoded_len > 0) {
		if (*cp != '\n' && *cp != '\r') {
			if ((r = sshbuf_put_u8(encoded, *cp)) != 0)
				goto out;
		}
		last = *cp;
		encoded_len--;
		cp++;
		if (last == '\n') {
			if (encoded_len >= MARK_END_LEN &&
			    memcmp(cp, MARK_END, MARK_END_LEN) == 0) {
				/* \0 terminate */
				if ((r = sshbuf_put_u8(encoded, 0)) != 0)
					goto out;
				break;
			}
		}
	}
	if (encoded_len == 0) {
		r = SSH_ERR_INVALID_FORMAT;
		goto out;
	}

	/* decode base64 */
	if ((r = sshbuf_b64tod(decoded, (char *)sshbuf_ptr(encoded))) != 0)
		goto out;

	/* check magic */
	if (sshbuf_len(decoded) < sizeof(AUTH_MAGIC) ||
	    memcmp(sshbuf_ptr(decoded), AUTH_MAGIC, sizeof(AUTH_MAGIC))) {
		r = SSH_ERR_INVALID_FORMAT;
		goto out;
	}
	/* parse public portion of key */
	if ((r = sshbuf_consume(decoded, sizeof(AUTH_MAGIC))) != 0 ||
	    (r = sshbuf_get_cstring(decoded, &ciphername, NULL)) != 0 ||
	    (r = sshbuf_get_cstring(decoded, &kdfname, NULL)) != 0 ||
	    (r = sshbuf_froms(decoded, &kdf)) != 0 ||
	    (r = sshbuf_get_u32(decoded, &nkeys)) != 0 ||
	    (r = sshbuf_skip_string(decoded)) != 0 || /* pubkey */
	    (r = sshbuf_get_u32(decoded, &encrypted_len)) != 0)
		goto out;

	if ((cipher = cipher_by_name(ciphername)) == NULL) {
		r = SSH_ERR_KEY_UNKNOWN_CIPHER;
		goto out;
	}
	if ((passphrase == NULL || strlen(passphrase) == 0) &&
	    strcmp(ciphername, "none") != 0) {
		/* passphrase required */
		r = SSH_ERR_KEY_WRONG_PASSPHRASE;
		goto out;
	}
	if (strcmp(kdfname, "none") != 0 && strcmp(kdfname, "bcrypt") != 0) {
		r = SSH_ERR_KEY_UNKNOWN_CIPHER;
		goto out;
	}
	if (!strcmp(kdfname, "none") && strcmp(ciphername, "none") != 0) {
		r = SSH_ERR_INVALID_FORMAT;
		goto out;
	}
	if (nkeys != 1) {
		/* XXX only one key supported */
		r = SSH_ERR_INVALID_FORMAT;
		goto out;
	}

	/* check size of encrypted key blob */
	blocksize = cipher_blocksize(cipher);
	if (encrypted_len < blocksize || (encrypted_len % blocksize) != 0) {
		r = SSH_ERR_INVALID_FORMAT;
		goto out;
	}

	/* setup key */
	keylen = cipher_keylen(cipher);
	ivlen = cipher_ivlen(cipher);
	authlen = cipher_authlen(cipher);
	if ((key = calloc(1, keylen + ivlen)) == NULL) {
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	if (strcmp(kdfname, "bcrypt") == 0) {
		if ((r = sshbuf_get_string(kdf, &salt, &slen)) != 0 ||
		    (r = sshbuf_get_u32(kdf, &rounds)) != 0)
			goto out;
		if (bcrypt_pbkdf(passphrase, strlen(passphrase), salt, slen,
		    key, keylen + ivlen, rounds) < 0) {
			r = SSH_ERR_INVALID_FORMAT;
			goto out;
		}
	}

	/* check that an appropriate amount of auth data is present */
	if (sshbuf_len(decoded) < encrypted_len + authlen) {
		r = SSH_ERR_INVALID_FORMAT;
		goto out;
	}

	/* decrypt private portion of key */
	if ((r = sshbuf_reserve(decrypted, encrypted_len, &dp)) != 0 ||
	    (r = cipher_init(&ciphercontext, cipher, key, keylen,
	    key + keylen, ivlen, 0)) != 0)
		goto out;
	if ((r = cipher_crypt(&ciphercontext, 0, dp, sshbuf_ptr(decoded),
	    encrypted_len, 0, authlen)) != 0) {
		/* an integrity error here indicates an incorrect passphrase */
		if (r == SSH_ERR_MAC_INVALID)
			r = SSH_ERR_KEY_WRONG_PASSPHRASE;
		goto out;
	}
	if ((r = sshbuf_consume(decoded, encrypted_len + authlen)) != 0)
		goto out;
	/* there should be no trailing data */
	if (sshbuf_len(decoded) != 0) {
		r = SSH_ERR_INVALID_FORMAT;
		goto out;
	}

	/* check check bytes */
	if ((r = sshbuf_get_u32(decrypted, &check1)) != 0 ||
	    (r = sshbuf_get_u32(decrypted, &check2)) != 0)
		goto out;
	if (check1 != check2) {
		r = SSH_ERR_KEY_WRONG_PASSPHRASE;
		goto out;
	}

	/* Load the private key and comment */
	if ((r = sshkey_private_deserialize(decrypted, &k)) != 0 ||
	    (r = sshbuf_get_cstring(decrypted, &comment, NULL)) != 0)
		goto out;

	/* Check deterministic padding */
	i = 0;
	while (sshbuf_len(decrypted)) {
		if ((r = sshbuf_get_u8(decrypted, &pad)) != 0)
			goto out;
		if (pad != (++i & 0xff)) {
			r = SSH_ERR_INVALID_FORMAT;
			goto out;
		}
	}

	/* XXX decode pubkey and check against private */

	/* success */
	r = 0;
	if (keyp != NULL) {
		*keyp = k;
		k = NULL;
	}
	if (commentp != NULL) {
		*commentp = comment;
		comment = NULL;
	}
 out:
	pad = 0;
	cipher_cleanup(&ciphercontext);
	free(ciphername);
	free(kdfname);
	free(comment);
	if (salt != NULL) {
		explicit_bzero(salt, slen);
		free(salt);
	}
	if (key != NULL) {
		explicit_bzero(key, keylen + ivlen);
		free(key);
	}
	sshbuf_free(encoded);
	sshbuf_free(decoded);
	sshbuf_free(kdf);
	sshbuf_free(decrypted);
	sshkey_free(k);
	return r;
}

#if WITH_SSH1
/*
 * Serialises the authentication (private) key to a blob, encrypting it with
 * passphrase.  The identification of the blob (lowest 64 bits of n) will
 * precede the key to provide identification of the key without needing a
 * passphrase.
 */
static int
sshkey_private_rsa1_to_blob(struct sshkey *key, struct sshbuf *blob,
    const char *passphrase, const char *comment)
{
	struct sshbuf *buffer = NULL, *encrypted = NULL;
	u_char buf[8];
	int r, cipher_num;
	struct sshcipher_ctx ciphercontext;
	const struct sshcipher *cipher;
	u_char *cp;

	/*
	 * If the passphrase is empty, use SSH_CIPHER_NONE to ease converting
	 * to another cipher; otherwise use SSH_AUTHFILE_CIPHER.
	 */
	cipher_num = (strcmp(passphrase, "") == 0) ?
	    SSH_CIPHER_NONE : SSH_CIPHER_3DES;
	if ((cipher = cipher_by_number(cipher_num)) == NULL)
		return SSH_ERR_INTERNAL_ERROR;

	/* This buffer is used to build the secret part of the private key. */
	if ((buffer = sshbuf_new()) == NULL)
		return SSH_ERR_ALLOC_FAIL;

	/* Put checkbytes for checking passphrase validity. */
	if ((r = sshbuf_reserve(buffer, 4, &cp)) != 0)
		goto out;
	arc4random_buf(cp, 2);
	memcpy(cp + 2, cp, 2);

	/*
	 * Store the private key (n and e will not be stored because they
	 * will be stored in plain text, and storing them also in encrypted
	 * format would just give known plaintext).
	 * Note: q and p are stored in reverse order to SSL.
	 */
	if ((r = sshbuf_put_bignum1(buffer, key->rsa->d)) != 0 ||
	    (r = sshbuf_put_bignum1(buffer, key->rsa->iqmp)) != 0 ||
	    (r = sshbuf_put_bignum1(buffer, key->rsa->q)) != 0 ||
	    (r = sshbuf_put_bignum1(buffer, key->rsa->p)) != 0)
		goto out;

	/* Pad the part to be encrypted to a size that is a multiple of 8. */
	explicit_bzero(buf, 8);
	if ((r = sshbuf_put(buffer, buf, 8 - (sshbuf_len(buffer) % 8))) != 0)
		goto out;

	/* This buffer will be used to contain the data in the file. */
	if ((encrypted = sshbuf_new()) == NULL) {
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}

	/* First store keyfile id string. */
	if ((r = sshbuf_put(encrypted, LEGACY_BEGIN,
	    sizeof(LEGACY_BEGIN))) != 0)
		goto out;

	/* Store cipher type and "reserved" field. */
	if ((r = sshbuf_put_u8(encrypted, cipher_num)) != 0 ||
	    (r = sshbuf_put_u32(encrypted, 0)) != 0)
		goto out;

	/* Store public key.  This will be in plain text. */
	if ((r = sshbuf_put_u32(encrypted, BN_num_bits(key->rsa->n))) != 0 ||
	    (r = sshbuf_put_bignum1(encrypted, key->rsa->n)) != 0 ||
	    (r = sshbuf_put_bignum1(encrypted, key->rsa->e)) != 0 ||
	    (r = sshbuf_put_cstring(encrypted, comment)) != 0)
		goto out;

	/* Allocate space for the private part of the key in the buffer. */
	if ((r = sshbuf_reserve(encrypted, sshbuf_len(buffer), &cp)) != 0)
		goto out;

	if ((r = cipher_set_key_string(&ciphercontext, cipher, passphrase,
	    CIPHER_ENCRYPT)) != 0)
		goto out;
	if ((r = cipher_crypt(&ciphercontext, 0, cp,
	    sshbuf_ptr(buffer), sshbuf_len(buffer), 0, 0)) != 0)
		goto out;
	if ((r = cipher_cleanup(&ciphercontext)) != 0)
		goto out;

	r = sshbuf_putb(blob, encrypted);

 out:
	explicit_bzero(&ciphercontext, sizeof(ciphercontext));
	explicit_bzero(buf, sizeof(buf));
	sshbuf_free(buffer);
	sshbuf_free(encrypted);

	return r;
}
#endif /* WITH_SSH1 */

#ifdef WITH_OPENSSL
/* convert SSH v2 key in OpenSSL PEM format */
static int
sshkey_private_pem_to_blob(struct sshkey *key, struct sshbuf *blob,
    const char *_passphrase, const char *comment)
{
	int success, r;
	int blen, len = strlen(_passphrase);
	u_char *passphrase = (len > 0) ? (u_char *)_passphrase : NULL;
#if (OPENSSL_VERSION_NUMBER < 0x00907000L)
	const EVP_CIPHER *cipher = (len > 0) ? EVP_des_ede3_cbc() : NULL;
#else
 	const EVP_CIPHER *cipher = (len > 0) ? EVP_aes_128_cbc() : NULL;
#endif
	const u_char *bptr;
	BIO *bio = NULL;

	if (len > 0 && len <= 4)
		return SSH_ERR_PASSPHRASE_TOO_SHORT;
	if ((bio = BIO_new(BIO_s_mem())) == NULL)
		return SSH_ERR_ALLOC_FAIL;

	switch (key->type) {
	case KEY_DSA:
		success = PEM_write_bio_DSAPrivateKey(bio, key->dsa,
		    cipher, passphrase, len, NULL, NULL);
		break;
#ifdef OPENSSL_HAS_ECC
	case KEY_ECDSA:
		success = PEM_write_bio_ECPrivateKey(bio, key->ecdsa,
		    cipher, passphrase, len, NULL, NULL);
		break;
#endif
	case KEY_RSA:
		success = PEM_write_bio_RSAPrivateKey(bio, key->rsa,
		    cipher, passphrase, len, NULL, NULL);
		break;
	default:
		success = 0;
		break;
	}
	if (success == 0) {
		r = SSH_ERR_LIBCRYPTO_ERROR;
		goto out;
	}
	if ((blen = BIO_get_mem_data(bio, &bptr)) <= 0) {
		r = SSH_ERR_INTERNAL_ERROR;
		goto out;
	}
	if ((r = sshbuf_put(blob, bptr, blen)) != 0)
		goto out;
	r = 0;
 out:
	BIO_free(bio);
	return r;
}
#endif /* WITH_OPENSSL */

/* Serialise "key" to buffer "blob" */
int
sshkey_private_to_fileblob(struct sshkey *key, struct sshbuf *blob,
    const char *passphrase, const char *comment,
    int force_new_format, const char *new_format_cipher, int new_format_rounds)
{
	switch (key->type) {
#ifdef WITH_SSH1
	case KEY_RSA1:
		return sshkey_private_rsa1_to_blob(key, blob,
		    passphrase, comment);
#endif /* WITH_SSH1 */
#ifdef WITH_OPENSSL
	case KEY_DSA:
	case KEY_ECDSA:
	case KEY_RSA:
		if (force_new_format) {
			return sshkey_private_to_blob2(key, blob, passphrase,
			    comment, new_format_cipher, new_format_rounds);
		}
		return sshkey_private_pem_to_blob(key, blob,
		    passphrase, comment);
#endif /* WITH_OPENSSL */
	case KEY_ED25519:
		return sshkey_private_to_blob2(key, blob, passphrase,
		    comment, new_format_cipher, new_format_rounds);
	default:
		return SSH_ERR_KEY_TYPE_UNKNOWN;
	}
}

#ifdef WITH_SSH1
/*
 * Parse the public, unencrypted portion of a RSA1 key.
 */
int
sshkey_parse_public_rsa1_fileblob(struct sshbuf *blob,
    struct sshkey **keyp, char **commentp)
{
	int r;
	struct sshkey *pub = NULL;
	struct sshbuf *copy = NULL;

	if (keyp != NULL)
		*keyp = NULL;
	if (commentp != NULL)
		*commentp = NULL;

	/* Check that it is at least big enough to contain the ID string. */
	if (sshbuf_len(blob) < sizeof(LEGACY_BEGIN))
		return SSH_ERR_INVALID_FORMAT;

	/*
	 * Make sure it begins with the id string.  Consume the id string
	 * from the buffer.
	 */
	if (memcmp(sshbuf_ptr(blob), LEGACY_BEGIN, sizeof(LEGACY_BEGIN)) != 0)
		return SSH_ERR_INVALID_FORMAT;
	/* Make a working copy of the keyblob and skip past the magic */
	if ((copy = sshbuf_fromb(blob)) == NULL)
		return SSH_ERR_ALLOC_FAIL;
	if ((r = sshbuf_consume(copy, sizeof(LEGACY_BEGIN))) != 0)
		goto out;

	/* Skip cipher type, reserved data and key bits. */
	if ((r = sshbuf_get_u8(copy, NULL)) != 0 ||	/* cipher type */
	    (r = sshbuf_get_u32(copy, NULL)) != 0 ||	/* reserved */
	    (r = sshbuf_get_u32(copy, NULL)) != 0)	/* key bits */
		goto out;

	/* Read the public key from the buffer. */
	if ((pub = sshkey_new(KEY_RSA1)) == NULL ||
	    (r = sshbuf_get_bignum1(copy, pub->rsa->n)) != 0 ||
	    (r = sshbuf_get_bignum1(copy, pub->rsa->e)) != 0)
		goto out;

	/* Finally, the comment */
	if ((r = sshbuf_get_string(copy, (u_char**)commentp, NULL)) != 0)
		goto out;

	/* The encrypted private part is not parsed by this function. */

	r = 0;
	if (keyp != NULL)
		*keyp = pub;
	else
		sshkey_free(pub);
	pub = NULL;

 out:
	sshbuf_free(copy);
	sshkey_free(pub);
	return r;
}

static int
sshkey_parse_private_rsa1(struct sshbuf *blob, const char *passphrase,
    struct sshkey **keyp, char **commentp)
{
	int r;
	u_int16_t check1, check2;
	u_int8_t cipher_type;
	struct sshbuf *decrypted = NULL, *copy = NULL;
	u_char *cp;
	char *comment = NULL;
	struct sshcipher_ctx ciphercontext;
	const struct sshcipher *cipher;
	struct sshkey *prv = NULL;

	*keyp = NULL;
	if (commentp != NULL)
		*commentp = NULL;

	/* Check that it is at least big enough to contain the ID string. */
	if (sshbuf_len(blob) < sizeof(LEGACY_BEGIN))
		return SSH_ERR_INVALID_FORMAT;

	/*
	 * Make sure it begins with the id string.  Consume the id string
	 * from the buffer.
	 */
	if (memcmp(sshbuf_ptr(blob), LEGACY_BEGIN, sizeof(LEGACY_BEGIN)) != 0)
		return SSH_ERR_INVALID_FORMAT;

	if ((prv = sshkey_new_private(KEY_RSA1)) == NULL) {
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	if ((copy = sshbuf_fromb(blob)) == NULL ||
	    (decrypted = sshbuf_new()) == NULL) {
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	if ((r = sshbuf_consume(copy, sizeof(LEGACY_BEGIN))) != 0)
		goto out;

	/* Read cipher type. */
	if ((r = sshbuf_get_u8(copy, &cipher_type)) != 0 ||
	    (r = sshbuf_get_u32(copy, NULL)) != 0)	/* reserved */
		goto out;

	/* Read the public key and comment from the buffer. */
	if ((r = sshbuf_get_u32(copy, NULL)) != 0 ||	/* key bits */
	    (r = sshbuf_get_bignum1(copy, prv->rsa->n)) != 0 ||
	    (r = sshbuf_get_bignum1(copy, prv->rsa->e)) != 0 ||
	    (r = sshbuf_get_cstring(copy, &comment, NULL)) != 0)
		goto out;

	/* Check that it is a supported cipher. */
	cipher = cipher_by_number(cipher_type);
	if (cipher == NULL) {
		r = SSH_ERR_KEY_UNKNOWN_CIPHER;
		goto out;
	}
	/* Initialize space for decrypted data. */
	if ((r = sshbuf_reserve(decrypted, sshbuf_len(copy), &cp)) != 0)
		goto out;

	/* Rest of the buffer is encrypted.  Decrypt it using the passphrase. */
	if ((r = cipher_set_key_string(&ciphercontext, cipher, passphrase,
	    CIPHER_DECRYPT)) != 0)
		goto out;
	if ((r = cipher_crypt(&ciphercontext, 0, cp,
	    sshbuf_ptr(copy), sshbuf_len(copy), 0, 0)) != 0) {
		cipher_cleanup(&ciphercontext);
		goto out;
	}
	if ((r = cipher_cleanup(&ciphercontext)) != 0)
		goto out;

	if ((r = sshbuf_get_u16(decrypted, &check1)) != 0 ||
	    (r = sshbuf_get_u16(decrypted, &check2)) != 0)
		goto out;
	if (check1 != check2) {
		r = SSH_ERR_KEY_WRONG_PASSPHRASE;
		goto out;
	}

	/* Read the rest of the private key. */
	if ((r = sshbuf_get_bignum1(decrypted, prv->rsa->d)) != 0 ||
	    (r = sshbuf_get_bignum1(decrypted, prv->rsa->iqmp)) != 0 ||
	    (r = sshbuf_get_bignum1(decrypted, prv->rsa->q)) != 0 ||
	    (r = sshbuf_get_bignum1(decrypted, prv->rsa->p)) != 0)
		goto out;

	/* calculate p-1 and q-1 */
	if ((r = rsa_generate_additional_parameters(prv->rsa)) != 0)
		goto out;

	/* enable blinding */
	if (RSA_blinding_on(prv->rsa, NULL) != 1) {
		r = SSH_ERR_LIBCRYPTO_ERROR;
		goto out;
	}
	r = 0;
	*keyp = prv;
	prv = NULL;
	if (commentp != NULL) {
		*commentp = comment;
		comment = NULL;
	}
 out:
	explicit_bzero(&ciphercontext, sizeof(ciphercontext));
	free(comment);
	sshkey_free(prv);
	sshbuf_free(copy);
	sshbuf_free(decrypted);
	return r;
}
#endif /* WITH_SSH1 */

#ifdef WITH_OPENSSL
static int
sshkey_parse_private_pem_fileblob(struct sshbuf *blob, int type,
    const char *passphrase, struct sshkey **keyp)
{
	EVP_PKEY *pk = NULL;
	struct sshkey *prv = NULL;
	BIO *bio = NULL;
	int r;

	*keyp = NULL;

	if ((bio = BIO_new(BIO_s_mem())) == NULL || sshbuf_len(blob) > INT_MAX)
		return SSH_ERR_ALLOC_FAIL;
	if (BIO_write(bio, sshbuf_ptr(blob), sshbuf_len(blob)) !=
	    (int)sshbuf_len(blob)) {
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}

	if ((pk = PEM_read_bio_PrivateKey(bio, NULL, NULL,
	    (char *)passphrase)) == NULL) {
		r = SSH_ERR_KEY_WRONG_PASSPHRASE;
		goto out;
	}
	if (pk->type == EVP_PKEY_RSA &&
	    (type == KEY_UNSPEC || type == KEY_RSA)) {
		if ((prv = sshkey_new(KEY_UNSPEC)) == NULL) {
			r = SSH_ERR_ALLOC_FAIL;
			goto out;
		}
		prv->rsa = EVP_PKEY_get1_RSA(pk);
		prv->type = KEY_RSA;
#ifdef DEBUG_PK
		RSA_print_fp(stderr, prv->rsa, 8);
#endif
		if (RSA_blinding_on(prv->rsa, NULL) != 1) {
			r = SSH_ERR_LIBCRYPTO_ERROR;
			goto out;
		}
	} else if (pk->type == EVP_PKEY_DSA &&
	    (type == KEY_UNSPEC || type == KEY_DSA)) {
		if ((prv = sshkey_new(KEY_UNSPEC)) == NULL) {
			r = SSH_ERR_ALLOC_FAIL;
			goto out;
		}
		prv->dsa = EVP_PKEY_get1_DSA(pk);
		prv->type = KEY_DSA;
#ifdef DEBUG_PK
		DSA_print_fp(stderr, prv->dsa, 8);
#endif
#ifdef OPENSSL_HAS_ECC
	} else if (pk->type == EVP_PKEY_EC &&
	    (type == KEY_UNSPEC || type == KEY_ECDSA)) {
		if ((prv = sshkey_new(KEY_UNSPEC)) == NULL) {
			r = SSH_ERR_ALLOC_FAIL;
			goto out;
		}
		prv->ecdsa = EVP_PKEY_get1_EC_KEY(pk);
		prv->type = KEY_ECDSA;
		prv->ecdsa_nid = sshkey_ecdsa_key_to_nid(prv->ecdsa);
		if (prv->ecdsa_nid == -1 ||
		    sshkey_curve_nid_to_name(prv->ecdsa_nid) == NULL ||
		    sshkey_ec_validate_public(EC_KEY_get0_group(prv->ecdsa),
		    EC_KEY_get0_public_key(prv->ecdsa)) != 0 ||
		    sshkey_ec_validate_private(prv->ecdsa) != 0) {
			r = SSH_ERR_INVALID_FORMAT;
			goto out;
		}
# ifdef DEBUG_PK
		if (prv != NULL && prv->ecdsa != NULL)
			sshkey_dump_ec_key(prv->ecdsa);
# endif
#endif /* OPENSSL_HAS_ECC */
	} else {
		r = SSH_ERR_INVALID_FORMAT;
		goto out;
	}
	r = 0;
	*keyp = prv;
	prv = NULL;
 out:
	BIO_free(bio);
	if (pk != NULL)
		EVP_PKEY_free(pk);
	sshkey_free(prv);
	return r;
}
#endif /* WITH_OPENSSL */

int
sshkey_parse_private_fileblob_type(struct sshbuf *blob, int type,
    const char *passphrase, struct sshkey **keyp, char **commentp)
{
	*keyp = NULL;
	if (commentp != NULL)
		*commentp = NULL;

	switch (type) {
#ifdef WITH_SSH1
	case KEY_RSA1:
		return sshkey_parse_private_rsa1(blob, passphrase,
		    keyp, commentp);
#endif /* WITH_SSH1 */
#ifdef WITH_OPENSSL
	case KEY_DSA:
	case KEY_ECDSA:
	case KEY_RSA:
		return sshkey_parse_private_pem_fileblob(blob, type,
		    passphrase, keyp);
#endif /* WITH_OPENSSL */
	case KEY_ED25519:
		return sshkey_parse_private2(blob, type, passphrase,
		    keyp, commentp);
	case KEY_UNSPEC:
		if (sshkey_parse_private2(blob, type, passphrase, keyp,
		    commentp) == 0)
			return 0;
#ifdef WITH_OPENSSL
		return sshkey_parse_private_pem_fileblob(blob, type,
		    passphrase, keyp);
#else
		return SSH_ERR_INVALID_FORMAT;
#endif /* WITH_OPENSSL */
	default:
		return SSH_ERR_KEY_TYPE_UNKNOWN;
	}
}

int
sshkey_parse_private_fileblob(struct sshbuf *buffer, const char *passphrase,
    struct sshkey **keyp, char **commentp)
{
	if (keyp != NULL)
		*keyp = NULL;
	if (commentp != NULL)
		*commentp = NULL;

#ifdef WITH_SSH1
	/* it's a SSH v1 key if the public key part is readable */
	if (sshkey_parse_public_rsa1_fileblob(buffer, NULL, NULL) == 0) {
		return sshkey_parse_private_fileblob_type(buffer, KEY_RSA1,
		    passphrase, keyp, commentp);
	}
#endif /* WITH_SSH1 */
	return sshkey_parse_private_fileblob_type(buffer, KEY_UNSPEC,
	    passphrase, keyp, commentp);
}
