/* $OpenBSD: ssh-pkcs11.c,v 1.73 2025/10/08 21:02:16 djm Exp $ */
/*
 * Copyright (c) 2010 Markus Friedl.  All rights reserved.
 * Copyright (c) 2014 Pedro Martelletto. All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "includes.h"

#ifdef ENABLE_PKCS11

#include <sys/time.h>

#include <sys/types.h>
#include <stdarg.h>
#include <stdio.h>

#include <ctype.h>
#include <string.h>
#include <dlfcn.h>

#include "openbsd-compat/sys-queue.h"
#include "openbsd-compat/openssl-compat.h"

#ifdef WITH_OPENSSL
#include "openbsd-compat/openssl-compat.h"
#include <openssl/bn.h>
#include <openssl/ecdsa.h>
#include <openssl/x509.h>
#include <openssl/err.h>
#endif

#define CRYPTOKI_COMPAT
#include "pkcs11.h"

#define SSHKEY_INTERNAL
#include "sshkey.h"

#include "log.h"
#include "misc.h"
#include "sshbuf.h"
#include "ssh-pkcs11.h"
#include "digest.h"
#include "xmalloc.h"
#include "crypto_api.h"

struct pkcs11_slotinfo {
	CK_TOKEN_INFO		token;
	CK_SESSION_HANDLE	session;
	int			logged_in;
};

struct pkcs11_provider {
	char			*name;
	void			*handle;
	CK_FUNCTION_LIST	*function_list;
	CK_INFO			info;
	CK_ULONG		nslots;
	CK_SLOT_ID		*slotlist;
	struct pkcs11_slotinfo	*slotinfo;
	int			valid;
	int			refcount;
	TAILQ_ENTRY(pkcs11_provider) next;
};

TAILQ_HEAD(, pkcs11_provider) pkcs11_providers;

struct pkcs11_key {
	struct sshbuf		*keyblob;
	struct pkcs11_provider	*provider;
	CK_ULONG		slotidx;
	char			*keyid;
	int			keyid_len;
	TAILQ_ENTRY(pkcs11_key)	next;
};

TAILQ_HEAD(, pkcs11_key) pkcs11_keys; /* XXX a tree would be better */

int pkcs11_interactive = 0;

#if defined(OPENSSL_HAS_ECC) || defined(OPENSSL_HAS_ED25519)
static void
ossl_error(const char *msg)
{
	unsigned long    e;

	error_f("%s", msg);
	while ((e = ERR_get_error()) != 0)
		error_f("libcrypto error: %s", ERR_error_string(e, NULL));
}
#endif

/*
 * finalize a provider shared library, it's no longer usable.
 * however, there might still be keys referencing this provider,
 * so the actual freeing of memory is handled by pkcs11_provider_unref().
 * this is called when a provider gets unregistered.
 */
static void
pkcs11_provider_finalize(struct pkcs11_provider *p)
{
	CK_RV rv;
	CK_ULONG i;

	debug_f("provider \"%s\" refcount %d valid %d",
	    p->name, p->refcount, p->valid);
	if (!p->valid)
		return;
	for (i = 0; i < p->nslots; i++) {
		if (p->slotinfo[i].session &&
		    (rv = p->function_list->C_CloseSession(
		    p->slotinfo[i].session)) != CKR_OK)
			error("C_CloseSession failed: %lu", rv);
	}
	if ((rv = p->function_list->C_Finalize(NULL)) != CKR_OK)
		error("C_Finalize failed: %lu", rv);
	p->valid = 0;
	p->function_list = NULL;
	dlclose(p->handle);
}

/*
 * remove a reference to the provider.
 * called when a key gets destroyed or when the provider is unregistered.
 */
static void
pkcs11_provider_unref(struct pkcs11_provider *p)
{
	debug_f("provider \"%s\" refcount %d", p->name, p->refcount);
	if (--p->refcount <= 0) {
		if (p->valid)
			error_f("provider \"%s\" still valid", p->name);
		free(p->name);
		free(p->slotlist);
		free(p->slotinfo);
		free(p);
	}
}

/* lookup provider by name */
static struct pkcs11_provider *
pkcs11_provider_lookup(char *provider_id)
{
	struct pkcs11_provider *p;

	TAILQ_FOREACH(p, &pkcs11_providers, next) {
		debug("check provider \"%s\"", p->name);
		if (!strcmp(provider_id, p->name))
			return (p);
	}
	return (NULL);
}

/* unregister provider by name */
int
pkcs11_del_provider(char *provider_id)
{
	struct pkcs11_provider *p;

	if ((p = pkcs11_provider_lookup(provider_id)) != NULL) {
		TAILQ_REMOVE(&pkcs11_providers, p, next);
		pkcs11_provider_finalize(p);
		pkcs11_provider_unref(p);
		return (0);
	}
	return (-1);
}

/* release a wrapped object */
static void
pkcs11_k11_free(struct pkcs11_key *k11)
{
	if (k11 == NULL)
		return;
	if (k11->provider)
		pkcs11_provider_unref(k11->provider);
	free(k11->keyid);
	sshbuf_free(k11->keyblob);
	free(k11);
}

/* find a single 'obj' for given attributes */
static int
pkcs11_find(struct pkcs11_provider *p, CK_ULONG slotidx, CK_ATTRIBUTE *attr,
    CK_ULONG nattr, CK_OBJECT_HANDLE *obj)
{
	CK_FUNCTION_LIST	*f;
	CK_SESSION_HANDLE	session;
	CK_ULONG		nfound = 0;
	CK_RV			rv;
	int			ret = -1;

	f = p->function_list;
	session = p->slotinfo[slotidx].session;
	if ((rv = f->C_FindObjectsInit(session, attr, nattr)) != CKR_OK) {
		error("C_FindObjectsInit failed (nattr %lu): %lu", nattr, rv);
		return (-1);
	}
	if ((rv = f->C_FindObjects(session, obj, 1, &nfound)) != CKR_OK ||
	    nfound != 1) {
		debug("C_FindObjects failed (nfound %lu nattr %lu): %lu",
		    nfound, nattr, rv);
	} else
		ret = 0;
	if ((rv = f->C_FindObjectsFinal(session)) != CKR_OK)
		error("C_FindObjectsFinal failed: %lu", rv);
	return (ret);
}

static int
pkcs11_login_slot(struct pkcs11_provider *provider, struct pkcs11_slotinfo *si,
    CK_USER_TYPE type)
{
	char			*pin = NULL, prompt[1024];
	CK_RV			 rv;

	if (provider == NULL || si == NULL || !provider->valid) {
		error("no pkcs11 (valid) provider found");
		return (-1);
	}

	if (!pkcs11_interactive) {
		error("need pin entry%s",
		    (si->token.flags & CKF_PROTECTED_AUTHENTICATION_PATH) ?
		    " on reader keypad" : "");
		return (-1);
	}
	if (si->token.flags & CKF_PROTECTED_AUTHENTICATION_PATH)
		verbose("Deferring PIN entry to reader keypad.");
	else {
		snprintf(prompt, sizeof(prompt), "Enter PIN for '%s': ",
		    si->token.label);
		if ((pin = read_passphrase(prompt, RP_ALLOW_EOF)) == NULL) {
			debug_f("no pin specified");
			return (-1);	/* bail out */
		}
	}
	rv = provider->function_list->C_Login(si->session, type, (u_char *)pin,
	    (pin != NULL) ? strlen(pin) : 0);
	if (pin != NULL)
		freezero(pin, strlen(pin));

	switch (rv) {
	case CKR_OK:
	case CKR_USER_ALREADY_LOGGED_IN:
		/* success */
		break;
	case CKR_PIN_LEN_RANGE:
		error("PKCS#11 login failed: PIN length out of range");
		return -1;
	case CKR_PIN_INCORRECT:
		error("PKCS#11 login failed: PIN incorrect");
		return -1;
	case CKR_PIN_LOCKED:
		error("PKCS#11 login failed: PIN locked");
		return -1;
	default:
		error("PKCS#11 login failed: error %lu", rv);
		return -1;
	}
	si->logged_in = 1;
	return (0);
}

static int
pkcs11_login(struct pkcs11_key *k11, CK_USER_TYPE type)
{
	if (k11 == NULL || k11->provider == NULL || !k11->provider->valid) {
		error("no pkcs11 (valid) provider found");
		return (-1);
	}

	return pkcs11_login_slot(k11->provider,
	    &k11->provider->slotinfo[k11->slotidx], type);
}


static int
pkcs11_check_obj_bool_attrib(struct pkcs11_key *k11, CK_OBJECT_HANDLE obj,
    CK_ATTRIBUTE_TYPE type, int *val)
{
	struct pkcs11_slotinfo	*si;
	CK_FUNCTION_LIST	*f;
	CK_BBOOL		flag = 0;
	CK_ATTRIBUTE		attr;
	CK_RV			 rv;

	*val = 0;

	if (!k11->provider || !k11->provider->valid) {
		error("no pkcs11 (valid) provider found");
		return (-1);
	}

	f = k11->provider->function_list;
	si = &k11->provider->slotinfo[k11->slotidx];

	attr.type = type;
	attr.pValue = &flag;
	attr.ulValueLen = sizeof(flag);

	rv = f->C_GetAttributeValue(si->session, obj, &attr, 1);
	if (rv != CKR_OK) {
		error("C_GetAttributeValue failed: %lu", rv);
		return (-1);
	}
	*val = flag != 0;
	debug_f("provider \"%s\" slot %lu object %lu: attrib %lu = %d",
	    k11->provider->name, k11->slotidx, obj, type, *val);
	return (0);
}

static int
pkcs11_get_key(struct pkcs11_key *k11, CK_MECHANISM_TYPE mech_type)
{
	struct pkcs11_slotinfo	*si;
	CK_FUNCTION_LIST	*f;
	CK_OBJECT_HANDLE	 obj;
	CK_RV			 rv;
	CK_OBJECT_CLASS		 private_key_class;
	CK_BBOOL		 true_val;
	CK_MECHANISM		 mech;
	CK_ATTRIBUTE		 key_filter[3];
	int			 always_auth = 0;
	int			 did_login = 0;

	if (!k11->provider || !k11->provider->valid) {
		error("no pkcs11 (valid) provider found");
		return (-1);
	}

	f = k11->provider->function_list;
	si = &k11->provider->slotinfo[k11->slotidx];

	if ((si->token.flags & CKF_LOGIN_REQUIRED) && !si->logged_in) {
		if (pkcs11_login(k11, CKU_USER) < 0) {
			error("login failed");
			return (-1);
		}
		did_login = 1;
	}

	memset(&key_filter, 0, sizeof(key_filter));
	private_key_class = CKO_PRIVATE_KEY;
	key_filter[0].type = CKA_CLASS;
	key_filter[0].pValue = &private_key_class;
	key_filter[0].ulValueLen = sizeof(private_key_class);

	key_filter[1].type = CKA_ID;
	key_filter[1].pValue = k11->keyid;
	key_filter[1].ulValueLen = k11->keyid_len;

	true_val = CK_TRUE;
	key_filter[2].type = CKA_SIGN;
	key_filter[2].pValue = &true_val;
	key_filter[2].ulValueLen = sizeof(true_val);

	/* try to find object w/CKA_SIGN first, retry w/o */
	if (pkcs11_find(k11->provider, k11->slotidx, key_filter, 3, &obj) < 0 &&
	    pkcs11_find(k11->provider, k11->slotidx, key_filter, 2, &obj) < 0) {
		error("cannot find private key");
		return (-1);
	}

	memset(&mech, 0, sizeof(mech));
	mech.mechanism = mech_type;
	mech.pParameter = NULL_PTR;
	mech.ulParameterLen = 0;

	if ((rv = f->C_SignInit(si->session, &mech, obj)) != CKR_OK) {
		error("C_SignInit failed: %lu", rv);
		return (-1);
	}

	pkcs11_check_obj_bool_attrib(k11, obj, CKA_ALWAYS_AUTHENTICATE,
	    &always_auth); /* ignore errors here */
	if (always_auth && !did_login) {
		debug_f("always-auth key");
		if (pkcs11_login(k11, CKU_CONTEXT_SPECIFIC) < 0) {
			error("login failed for always-auth key");
			return (-1);
		}
	}

	return (0);
}

/* record the key information later use lookup by keyblob */
static int
pkcs11_record_key(struct pkcs11_provider *provider, CK_ULONG slotidx,
    CK_ATTRIBUTE *keyid_attrib, struct sshkey *key)
{
	struct sshbuf *keyblob;
	struct pkcs11_key *k11;
	int r;
	char *hex;

	hex = tohex(keyid_attrib->pValue, keyid_attrib->ulValueLen);
	debug_f("%s key: provider %s slot %lu keyid %s",
	    sshkey_type(key), provider->name, (u_long)slotidx, hex);
	free(hex);

	if ((keyblob = sshbuf_new()) == NULL)
		fatal_f("sshbuf_new failed");
	if ((r = sshkey_putb(key, keyblob)) != 0)
		fatal_fr(r, "sshkey_putb");

	/* Check if we've already recorded this key in a different slot */
	TAILQ_FOREACH(k11, &pkcs11_keys, next) {
		if (sshbuf_equals(k11->keyblob, keyblob) == 0) {
			hex = tohex(k11->keyid, k11->keyid_len);
			debug_f("Already seen this key at "
			    "provider %s slot %lu keyid %s",
			    k11->provider->name, k11->slotidx, hex);
			free(hex);
			sshbuf_free(keyblob);
			return -1;
		}
	}

	k11 = xcalloc(1, sizeof(*k11));
	k11->provider = provider;
	k11->keyblob = keyblob;
	provider->refcount++;	/* provider referenced by RSA key */
	k11->slotidx = slotidx;
	/* identify key object on smartcard */
	k11->keyid_len = keyid_attrib->ulValueLen;
	if (k11->keyid_len > 0) {
		k11->keyid = xmalloc(k11->keyid_len);
		memcpy(k11->keyid, keyid_attrib->pValue, k11->keyid_len);
	}
	TAILQ_INSERT_TAIL(&pkcs11_keys, k11, next);

	return 0;
}

/* retrieve the key information by keyblob */
static struct pkcs11_key *
pkcs11_lookup_key(struct sshkey *key)
{
	struct pkcs11_key *k11, *found = NULL;
	struct sshbuf *keyblob;
	int r;

	if ((keyblob = sshbuf_new()) == NULL)
		fatal_f("sshbuf_new failed");
	if ((r = sshkey_putb(key, keyblob)) != 0)
		fatal_fr(r, "sshkey_putb");
	TAILQ_FOREACH(k11, &pkcs11_keys, next) {
		if (sshbuf_equals(k11->keyblob, keyblob) == 0) {
			found = k11;
			break;
		}
	}
	sshbuf_free(keyblob);
	return found;
}

#ifdef WITH_OPENSSL
/*
 * See:
 * https://datatracker.ietf.org/doc/html/rfc8017#section-9.2
 */

/*
 * id-sha1 OBJECT IDENTIFIER ::= { iso(1) identified-organization(3)
 *	oiw(14) secsig(3) algorithms(2) 26 }
 */
static const u_char id_sha1[] = {
	0x30, 0x21, /* type Sequence, length 0x21 (33) */
	0x30, 0x09, /* type Sequence, length 0x09 */
	0x06, 0x05, /* type OID, length 0x05 */
	0x2b, 0x0e, 0x03, 0x02, 0x1a, /* id-sha1 OID */
	0x05, 0x00, /* NULL */
	0x04, 0x14  /* Octet string, length 0x14 (20), followed by sha1 hash */
};

/*
 * id-sha256 OBJECT IDENTIFIER ::= { joint-iso-itu-t(2) country(16) us(840)
 *      organization(1) gov(101) csor(3) nistAlgorithm(4) hashAlgs(2)
 *      id-sha256(1) }
 */
static const u_char id_sha256[] = {
	0x30, 0x31, /* type Sequence, length 0x31 (49) */
	0x30, 0x0d, /* type Sequence, length 0x0d (13) */
	0x06, 0x09, /* type OID, length 0x09 */
	0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01, /* id-sha256 */
	0x05, 0x00, /* NULL */
	0x04, 0x20  /* Octet string, length 0x20 (32), followed by sha256 hash */
};

/*
 * id-sha512 OBJECT IDENTIFIER ::= { joint-iso-itu-t(2) country(16) us(840)
 *      organization(1) gov(101) csor(3) nistAlgorithm(4) hashAlgs(2)
 *      id-sha256(3) }
 */
static const u_char id_sha512[] = {
	0x30, 0x51, /* type Sequence, length 0x51 (81) */
	0x30, 0x0d, /* type Sequence, length 0x0d (13) */
	0x06, 0x09, /* type OID, length 0x09 */
	0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x03, /* id-sha512 */
	0x05, 0x00, /* NULL */
	0x04, 0x40  /* Octet string, length 0x40 (64), followed by sha512 hash */
};

static int
rsa_hash_alg_oid(int hash_alg, const u_char **oidp, size_t *oidlenp)
{
	switch (hash_alg) {
	case SSH_DIGEST_SHA1:
		*oidp = id_sha1;
		*oidlenp = sizeof(id_sha1);
		break;
	case SSH_DIGEST_SHA256:
		*oidp = id_sha256;
		*oidlenp = sizeof(id_sha256);
		break;
	case SSH_DIGEST_SHA512:
		*oidp = id_sha512;
		*oidlenp = sizeof(id_sha512);
		break;
	default:
		return SSH_ERR_INVALID_ARGUMENT;
	}
	return 0;
}

static int
pkcs11_sign_rsa(struct sshkey *key,
    u_char **sigp, size_t *lenp,
    const u_char *data, size_t datalen,
    const char *alg, const char *sk_provider,
    const char *sk_pin, u_int compat)
{
	struct pkcs11_key	*k11;
	struct pkcs11_slotinfo	*si;
	CK_FUNCTION_LIST	*f;
	CK_ULONG		slen = 0;
	CK_RV			rv;
	int			hashalg, r, diff, siglen, ret = -1;
	u_char			*oid_dgst = NULL, *sig = NULL;
	size_t			dgst_len, oid_len, oid_dgst_len = 0;
	const u_char		*oid;

	if (sigp != NULL)
		*sigp = NULL;
	if (lenp != NULL)
		*lenp = 0;

	if ((k11 = pkcs11_lookup_key(key)) == NULL) {
		error_f("no key found");
		return SSH_ERR_KEY_NOT_FOUND;
	}

	debug3_f("sign with alg \"%s\" using provider %s slotidx %lu",
	    alg == NULL ? "" : alg, k11->provider->name, (u_long)k11->slotidx);

	if (pkcs11_get_key(k11, CKM_RSA_PKCS) == -1) {
		error("pkcs11_get_key failed");
		return SSH_ERR_AGENT_FAILURE;
	}

	f = k11->provider->function_list;
	si = &k11->provider->slotinfo[k11->slotidx];

	if ((siglen = EVP_PKEY_size(key->pkey)) <= 0)
		return SSH_ERR_INVALID_ARGUMENT;
	sig = xmalloc(siglen);
	slen = (CK_ULONG)siglen;

	/* Determine hash algorithm and OID for signature */
	if (alg == NULL || *alg == '\0')
		hashalg = SSH_DIGEST_SHA1;
	else if ((hashalg = ssh_rsa_hash_id_from_keyname(alg)) == -1)
		fatal_f("couldn't determine RSA hash alg \"%s\"", alg);
	if ((r = rsa_hash_alg_oid(hashalg, &oid, &oid_len)) != 0)
		fatal_fr(r, "rsa_hash_alg_oid failed");
	if ((dgst_len = ssh_digest_bytes(hashalg)) == 0)
		fatal_f("bad hash alg %d", hashalg);

	/* Prepare { oid || digest } */
	oid_dgst_len = oid_len + dgst_len;
	oid_dgst = xcalloc(1, oid_dgst_len);
	memcpy(oid_dgst, oid, oid_len);
	if ((r = ssh_digest_memory(hashalg, data, datalen,
	    oid_dgst + oid_len, dgst_len)) == -1)
		fatal_fr(r, "hash failed");

	/* XXX handle CKR_BUFFER_TOO_SMALL */
	if ((rv = f->C_Sign(si->session, (CK_BYTE *)oid_dgst,
	    oid_dgst_len, sig, &slen)) != CKR_OK) {
		error("C_Sign failed: %lu", rv);
		goto done;
	}

	if (slen < (CK_ULONG)siglen) {
		diff = siglen - slen;
		debug3_f("repack %lu < %d (diff %d)",
		    (u_long)slen, siglen, diff);
		memmove(sig + diff, sig, slen);
		explicit_bzero(sig, diff);
	} else if (slen > (size_t)siglen)
		fatal_f("bad C_Sign length");

	if ((ret = ssh_rsa_encode_store_sig(hashalg, sig, siglen,
	    sigp, lenp)) != 0)
		fatal_fr(ret, "couldn't store signature");

	/* success */
	ret = 0;
 done:
	freezero(oid_dgst, oid_dgst_len);
	free(sig);
	return ret;
}

#ifdef OPENSSL_HAS_ECC
static int
pkcs11_sign_ecdsa(struct sshkey *key,
    u_char **sigp, size_t *lenp,
    const u_char *data, size_t datalen,
    const char *alg, const char *sk_provider,
    const char *sk_pin, u_int compat)
{
	struct pkcs11_key	*k11;
	struct pkcs11_slotinfo	*si;
	CK_FUNCTION_LIST	*f;
	CK_ULONG		slen = 0, bnlen;
	CK_RV			rv;
	BIGNUM			*sig_r = NULL, *sig_s = NULL;
	u_char			*sig = NULL, *dgst = NULL;
	size_t			dgst_len = 0;
	int			hashalg, ret = -1, r, siglen;

	if (sigp != NULL)
		*sigp = NULL;
	if (lenp != NULL)
		*lenp = 0;

	if ((k11 = pkcs11_lookup_key(key)) == NULL) {
		error_f("no key found");
		return SSH_ERR_KEY_NOT_FOUND;
	}

	if (pkcs11_get_key(k11, CKM_ECDSA) == -1) {
		error("pkcs11_get_key failed");
		return SSH_ERR_AGENT_FAILURE;
	}

	debug3_f("sign using provider %s slotidx %lu",
	    k11->provider->name, (u_long)k11->slotidx);

	f = k11->provider->function_list;
	si = &k11->provider->slotinfo[k11->slotidx];

	/* Prepare digest to be signed */
	if ((hashalg = sshkey_ec_nid_to_hash_alg(key->ecdsa_nid)) == -1)
		fatal_f("couldn't determine ECDSA hash alg");
	if ((dgst_len = ssh_digest_bytes(hashalg)) == 0)
		fatal_f("bad hash alg %d", hashalg);
	dgst = xcalloc(1, dgst_len);
	if ((r = ssh_digest_memory(hashalg, data, datalen,
	    dgst, dgst_len)) == -1)
		fatal_fr(r, "hash failed");

	if ((siglen = EVP_PKEY_size(key->pkey)) <= 0)
		return SSH_ERR_INVALID_ARGUMENT;
	sig = xmalloc(siglen);
	slen = (CK_ULONG)siglen;

	/* XXX handle CKR_BUFFER_TOO_SMALL */
	rv = f->C_Sign(si->session, (CK_BYTE *)dgst, dgst_len, sig, &slen);
	if (rv != CKR_OK) {
		error("C_Sign failed: %lu", rv);
		goto done;
	}
	if (slen < 64 || slen > 132 || slen % 2) {
		error_f("bad signature length: %lu", (u_long)slen);
		goto done;
	}
	bnlen = slen/2;
	if ((sig_r = BN_bin2bn(sig, bnlen, NULL)) == NULL ||
	    (sig_s = BN_bin2bn(sig+bnlen, bnlen, NULL)) == NULL) {
		ossl_error("BN_bin2bn failed");
		goto done;
	}

	if ((ret = ssh_ecdsa_encode_store_sig(key, sig_r, sig_s,
	    sigp, lenp)) != 0)
		fatal_fr(ret, "couldn't store signature");

	/* success */
	ret = 0;
 done:
	freezero(dgst, dgst_len);
	BN_free(sig_r);
	BN_free(sig_s);
	free(sig);
	return ret;
}
#endif /* OPENSSL_HAS_ECC */
#endif /* WITH_OPENSSL */

static int
pkcs11_sign_ed25519(struct sshkey *key,
    u_char **sigp, size_t *lenp,
    const u_char *data, size_t datalen,
    const char *alg, const char *sk_provider,
    const char *sk_pin, u_int compat)
{
	struct pkcs11_key	*k11;
	struct pkcs11_slotinfo	*si;
	CK_FUNCTION_LIST	*f;
	CK_ULONG		slen = 0;
	CK_RV			rv;
	u_char			*sig = NULL;
	CK_BYTE			*xdata = NULL;
	int			ret = -1;

	if (sigp != NULL)
		*sigp = NULL;
	if (lenp != NULL)
		*lenp = 0;

	if ((k11 = pkcs11_lookup_key(key)) == NULL) {
		error_f("no key found");
		return SSH_ERR_KEY_NOT_FOUND;
	}

	if (pkcs11_get_key(k11, CKM_EDDSA) == -1) {
		error("pkcs11_get_key failed");
		return SSH_ERR_AGENT_FAILURE;
	}

	debug3_f("sign using provider %s slotidx %lu",
	    k11->provider->name, (u_long)k11->slotidx);

	f = k11->provider->function_list;
	si = &k11->provider->slotinfo[k11->slotidx];

	xdata = xmalloc(datalen);
	memcpy(xdata, data, datalen);
	sig = xmalloc(crypto_sign_ed25519_BYTES);
	slen = crypto_sign_ed25519_BYTES;

	rv = f->C_Sign(si->session, xdata, datalen, sig, &slen);
	if (rv != CKR_OK) {
		error("C_Sign failed: %lu", rv);
		goto done;
	}
	if (slen != crypto_sign_ed25519_BYTES) {
		error_f("bad signature length: %lu", (u_long)slen);
		goto done;
	}
	if ((ret = ssh_ed25519_encode_store_sig(sig, slen, sigp, lenp)) != 0)
		fatal_fr(ret, "couldn't store signature");

	/* success */
	ret = 0;
 done:
	if (xdata != NULL)
		freezero(xdata, datalen);
	free(sig);
	return ret;
}

/* remove trailing spaces */
static char *
rmspace(u_char *buf, size_t len)
{
	size_t i;

	if (len == 0)
		return buf;
	for (i = len - 1; i > 0; i--)
		if (buf[i] == ' ')
			buf[i] = '\0';
		else
			break;
	return buf;
}
/* Used to printf fixed-width, space-padded, unterminated strings using %.*s */
#define RMSPACE(s) (int)sizeof(s), rmspace(s, sizeof(s))

/*
 * open a pkcs11 session and login if required.
 * if pin == NULL we delay login until key use
 */
static int
pkcs11_open_session(struct pkcs11_provider *p, CK_ULONG slotidx, char *pin,
    CK_ULONG user)
{
	struct pkcs11_slotinfo	*si;
	CK_FUNCTION_LIST	*f;
	CK_RV			rv;
	CK_SESSION_HANDLE	session;
	int			login_required, ret;

	f = p->function_list;
	si = &p->slotinfo[slotidx];

	login_required = si->token.flags & CKF_LOGIN_REQUIRED;

	/* fail early before opening session */
	if (login_required && !pkcs11_interactive &&
	    (pin == NULL || strlen(pin) == 0)) {
		error("pin required");
		return (-SSH_PKCS11_ERR_PIN_REQUIRED);
	}
	if ((rv = f->C_OpenSession(p->slotlist[slotidx], CKF_RW_SESSION|
	    CKF_SERIAL_SESSION, NULL, NULL, &session)) != CKR_OK) {
		error("C_OpenSession failed: %lu", rv);
		return (-1);
	}
	if (login_required && pin != NULL && strlen(pin) != 0) {
		rv = f->C_Login(session, user, (u_char *)pin, strlen(pin));
		if (rv != CKR_OK && rv != CKR_USER_ALREADY_LOGGED_IN) {
			error("C_Login failed: %lu", rv);
			ret = (rv == CKR_PIN_LOCKED) ?
			    -SSH_PKCS11_ERR_PIN_LOCKED :
			    -SSH_PKCS11_ERR_LOGIN_FAIL;
			if ((rv = f->C_CloseSession(session)) != CKR_OK)
				error("C_CloseSession failed: %lu", rv);
			return (ret);
		}
		si->logged_in = 1;
	}
	si->session = session;
	return (0);
}

static int
pkcs11_key_included(struct sshkey ***keysp, int *nkeys, struct sshkey *key)
{
	int i;

	for (i = 0; i < *nkeys; i++)
		if (sshkey_equal(key, (*keysp)[i]))
			return (1);
	return (0);
}

#ifdef WITH_OPENSSL
#ifdef OPENSSL_HAS_ECC
static struct sshkey *
pkcs11_fetch_ecdsa_pubkey(struct pkcs11_provider *p, CK_ULONG slotidx,
    CK_OBJECT_HANDLE *obj)
{
	CK_ATTRIBUTE		 key_attr[3];
	CK_SESSION_HANDLE	 session;
	CK_FUNCTION_LIST	*f = NULL;
	CK_RV			 rv;
	ASN1_OCTET_STRING	*octet = NULL;
	EC_KEY			*ec = NULL;
	EC_GROUP		*group = NULL;
	struct sshkey		*key = NULL;
	const unsigned char	*attrp = NULL;
	int			 success = -1, r, i, nid;

	memset(&key_attr, 0, sizeof(key_attr));
	key_attr[0].type = CKA_ID;
	key_attr[1].type = CKA_EC_POINT;
	key_attr[2].type = CKA_EC_PARAMS;

	session = p->slotinfo[slotidx].session;
	f = p->function_list;

	/* figure out size of the attributes */
	rv = f->C_GetAttributeValue(session, *obj, key_attr, 3);
	if (rv != CKR_OK) {
		error("C_GetAttributeValue failed: %lu", rv);
		return (NULL);
	}

	/*
	 * Allow CKA_ID (always first attribute) to be empty, but
	 * ensure that none of the others are zero length.
	 * XXX assumes CKA_ID is always first.
	 */
	if (key_attr[1].ulValueLen == 0 ||
	    key_attr[2].ulValueLen == 0) {
		error("invalid attribute length");
		return (NULL);
	}

	/* allocate buffers for attributes */
	for (i = 0; i < 3; i++)
		if (key_attr[i].ulValueLen > 0)
			key_attr[i].pValue = xcalloc(1, key_attr[i].ulValueLen);

	/* retrieve ID, public point and curve parameters of EC key */
	rv = f->C_GetAttributeValue(session, *obj, key_attr, 3);
	if (rv != CKR_OK) {
		error("C_GetAttributeValue failed: %lu", rv);
		goto fail;
	}

	ec = EC_KEY_new();
	if (ec == NULL) {
		error("EC_KEY_new failed");
		goto fail;
	}

	attrp = key_attr[2].pValue;
	group = d2i_ECPKParameters(NULL, &attrp, key_attr[2].ulValueLen);
	if (group == NULL) {
		ossl_error("d2i_ECPKParameters failed");
		goto fail;
	}

	if (EC_KEY_set_group(ec, group) == 0) {
		ossl_error("EC_KEY_set_group failed");
		goto fail;
	}

	if (key_attr[1].ulValueLen <= 2) {
		error("CKA_EC_POINT too small");
		goto fail;
	}

	attrp = key_attr[1].pValue;
	octet = d2i_ASN1_OCTET_STRING(NULL, &attrp, key_attr[1].ulValueLen);
	if (octet == NULL) {
		ossl_error("d2i_ASN1_OCTET_STRING failed");
		goto fail;
	}
	attrp = octet->data;
	if (o2i_ECPublicKey(&ec, &attrp, octet->length) == NULL) {
		ossl_error("o2i_ECPublicKey failed");
		goto fail;
	}
	if ((r = sshkey_ec_validate_public(EC_KEY_get0_group(ec),
	    EC_KEY_get0_public_key(ec))) != 0) {
		error_fr(r, "invalid EC key");
		goto fail;
	}

	nid = sshkey_ecdsa_key_to_nid(ec);
	if (nid < 0) {
		error("couldn't get curve nid");
		goto fail;
	}

	key = sshkey_new(KEY_UNSPEC);
	if (key == NULL) {
		error("sshkey_new failed");
		goto fail;
	}

	EVP_PKEY_free(key->pkey);
	if ((key->pkey = EVP_PKEY_new()) == NULL)
		fatal("EVP_PKEY_new failed");
	if (EVP_PKEY_set1_EC_KEY(key->pkey, ec) != 1)
		fatal("EVP_PKEY_set1_EC_KEY failed");
	key->ecdsa_nid = nid;
	key->type = KEY_ECDSA;
	key->flags |= SSHKEY_FLAG_EXT;
	if (pkcs11_record_key(p, slotidx, &key_attr[0], key))
		goto fail;
	/* success */
	success = 0;
fail:
	if (success != 0) {
		sshkey_free(key);
		key = NULL;
	}
	for (i = 0; i < 3; i++)
		free(key_attr[i].pValue);
	if (ec)
		EC_KEY_free(ec);
	if (group)
		EC_GROUP_free(group);
	if (octet)
		ASN1_OCTET_STRING_free(octet);

	return (key);
}
#endif /* OPENSSL_HAS_ECC */

static struct sshkey *
pkcs11_fetch_rsa_pubkey(struct pkcs11_provider *p, CK_ULONG slotidx,
    CK_OBJECT_HANDLE *obj)
{
	CK_ATTRIBUTE		 key_attr[3];
	CK_SESSION_HANDLE	 session;
	CK_FUNCTION_LIST	*f = NULL;
	CK_RV			 rv;
	RSA			*rsa = NULL;
	BIGNUM			*rsa_n, *rsa_e;
	struct sshkey		*key = NULL;
	int			 i, success = -1;

	memset(&key_attr, 0, sizeof(key_attr));
	key_attr[0].type = CKA_ID;
	key_attr[1].type = CKA_MODULUS;
	key_attr[2].type = CKA_PUBLIC_EXPONENT;

	session = p->slotinfo[slotidx].session;
	f = p->function_list;

	/* figure out size of the attributes */
	rv = f->C_GetAttributeValue(session, *obj, key_attr, 3);
	if (rv != CKR_OK) {
		error("C_GetAttributeValue failed: %lu", rv);
		return (NULL);
	}

	/*
	 * Allow CKA_ID (always first attribute) to be empty, but
	 * ensure that none of the others are zero length.
	 * XXX assumes CKA_ID is always first.
	 */
	if (key_attr[1].ulValueLen == 0 ||
	    key_attr[2].ulValueLen == 0) {
		error("invalid attribute length");
		return (NULL);
	}

	/* allocate buffers for attributes */
	for (i = 0; i < 3; i++)
		if (key_attr[i].ulValueLen > 0)
			key_attr[i].pValue = xcalloc(1, key_attr[i].ulValueLen);

	/* retrieve ID, modulus and public exponent of RSA key */
	rv = f->C_GetAttributeValue(session, *obj, key_attr, 3);
	if (rv != CKR_OK) {
		error("C_GetAttributeValue failed: %lu", rv);
		goto fail;
	}

	rsa = RSA_new();
	if (rsa == NULL) {
		error("RSA_new failed");
		goto fail;
	}

	rsa_n = BN_bin2bn(key_attr[1].pValue, key_attr[1].ulValueLen, NULL);
	rsa_e = BN_bin2bn(key_attr[2].pValue, key_attr[2].ulValueLen, NULL);
	if (rsa_n == NULL || rsa_e == NULL) {
		error("BN_bin2bn failed");
		goto fail;
	}
	if (!RSA_set0_key(rsa, rsa_n, rsa_e, NULL))
		fatal_f("set key");
	rsa_n = rsa_e = NULL; /* transferred */

	key = sshkey_new(KEY_UNSPEC);
	if (key == NULL) {
		error("sshkey_new failed");
		goto fail;
	}

	EVP_PKEY_free(key->pkey);
	if ((key->pkey = EVP_PKEY_new()) == NULL)
		fatal("EVP_PKEY_new failed");
	if (EVP_PKEY_set1_RSA(key->pkey, rsa) != 1)
		fatal("EVP_PKEY_set1_RSA failed");
	key->type = KEY_RSA;
	key->flags |= SSHKEY_FLAG_EXT;
	if (EVP_PKEY_bits(key->pkey) < SSH_RSA_MINIMUM_MODULUS_SIZE) {
		error_f("RSA key too small %d < minimum %d",
		    EVP_PKEY_bits(key->pkey), SSH_RSA_MINIMUM_MODULUS_SIZE);
		goto fail;
	}
	if (pkcs11_record_key(p, slotidx, &key_attr[0], key))
		goto fail;
	/* success */
	success = 0;
fail:
	for (i = 0; i < 3; i++)
		free(key_attr[i].pValue);
	RSA_free(rsa);
	if (success != 0) {
		sshkey_free(key);
		key = NULL;
	}
	return key;
}
#endif /* WITH_OPENSSL */

static struct sshkey *
pkcs11_fetch_ed25519_pubkey(struct pkcs11_provider *p, CK_ULONG slotidx,
    CK_OBJECT_HANDLE *obj)
{
	CK_ATTRIBUTE		 key_attr[3];
	CK_SESSION_HANDLE	 session;
	CK_FUNCTION_LIST	*f = NULL;
	CK_RV			 rv;
	struct sshkey		*key = NULL;
	const unsigned char	*d = NULL;
	size_t			len;
	char			*hex = NULL;
	int			 success = -1, i;
	/* https://docs.oasis-open.org/pkcs11/pkcs11-curr/v3.0/os/pkcs11-curr-v3.0-os.html#_Toc30061180 */
	const u_char		 id1[14] = {
		0x13, 0x0c, 0x65, 0x64, 0x77, 0x61, 0x72, 0x64,
		0x73, 0x32, 0x35, 0x35, 0x31, 0x39,
	}; /* PrintableString { "edwards25519" } */
	const u_char		 id2[5] = {
		0x06, 0x03, 0x2b, 0x65, 0x70,
	}; /* OBJECT_IDENTIFIER { 1.3.101.112 } */

	memset(&key_attr, 0, sizeof(key_attr));
	key_attr[0].type = CKA_ID;
	key_attr[1].type = CKA_EC_POINT; /* XXX or CKA_VALUE ? */
	key_attr[2].type = CKA_EC_PARAMS;

	session = p->slotinfo[slotidx].session;
	f = p->function_list;

	/* figure out size of the attributes */
	rv = f->C_GetAttributeValue(session, *obj, key_attr, 3);
	if (rv != CKR_OK) {
		error("C_GetAttributeValue failed: %lu", rv);
		return (NULL);
	}

	/*
	 * Allow CKA_ID (always first attribute) to be empty, but
	 * ensure that none of the others are zero length.
	 * XXX assumes CKA_ID is always first.
	 */
	if (key_attr[1].ulValueLen == 0 ||
	    key_attr[2].ulValueLen == 0) {
		error("invalid attribute length");
		return (NULL);
	}

	/* allocate buffers for attributes */
	for (i = 0; i < 3; i++) {
		if (key_attr[i].ulValueLen > 0)
			key_attr[i].pValue = xcalloc(1, key_attr[i].ulValueLen);
	}

	/* retrieve ID, public point and curve parameters of EC key */
	rv = f->C_GetAttributeValue(session, *obj, key_attr, 3);
	if (rv != CKR_OK) {
		error("C_GetAttributeValue failed: %lu", rv);
		goto fail;
	}

	/* Expect one of the supported identifiers in CKA_EC_PARAMS */
	d = (u_char *)key_attr[2].pValue;
	len = key_attr[2].ulValueLen;
	if ((len != sizeof(id1) || memcmp(d, id1, sizeof(id1)) != 0) &&
	    (len != sizeof(id2) || memcmp(d, id2, sizeof(id2)) != 0)) {
		hex = tohex(d, len);
		logit_f("unsupported CKA_EC_PARAMS: %s (len %zu)", hex, len);
		goto fail;
	}

	/*
	 * Expect either a raw 32 byte pubkey or an OCTET STRING with
	 * a 32 byte pubkey in CKA_VALUE
	 */
	d = (u_char *)key_attr[1].pValue;
	len = key_attr[1].ulValueLen;
	if (len == ED25519_PK_SZ + 2 && d[0] == 0x04 && d[1] == ED25519_PK_SZ) {
		d += 2;
		len -= 2;
	}
	if (len != ED25519_PK_SZ) {
		hex = tohex(key_attr[1].pValue, key_attr[1].ulValueLen);
		logit_f("CKA_EC_POINT invalid octet str: %s (len %lu)",
		    hex, (u_long)key_attr[1].ulValueLen);
		goto fail;
	}

	if ((key = sshkey_new(KEY_UNSPEC)) == NULL)
		fatal_f("sshkey_new failed");
	key->ed25519_pk = xmalloc(ED25519_PK_SZ);
	memcpy(key->ed25519_pk, d, ED25519_PK_SZ);
	key->type = KEY_ED25519;
	key->flags |= SSHKEY_FLAG_EXT;
	if (pkcs11_record_key(p, slotidx, &key_attr[0], key))
		goto fail;
	/* success */
	success = 0;
 fail:
	if (success != 0) {
		sshkey_free(key);
		key = NULL;
	}
	free(hex);
	for (i = 0; i < 3; i++)
		free(key_attr[i].pValue);
	return key;
}

#ifdef WITH_OPENSSL
static int
pkcs11_fetch_x509_pubkey(struct pkcs11_provider *p, CK_ULONG slotidx,
    CK_OBJECT_HANDLE *obj, struct sshkey **keyp, char **labelp)
{
	CK_ATTRIBUTE		 cert_attr[3];
	CK_SESSION_HANDLE	 session;
	CK_FUNCTION_LIST	*f = NULL;
	CK_RV			 rv;
	X509			*x509 = NULL;
	X509_NAME		*x509_name = NULL;
	EVP_PKEY		*evp;
	RSA			*rsa = NULL;

	struct sshkey		*key = NULL;
	int			 i, success = -1;
	const u_char		*cp;
	char			*subject = NULL;
#ifdef OPENSSL_HAS_ED25519
	size_t			len;
#endif /* OPENSSL_HAS_ED25519 */
#ifdef OPENSSL_HAS_ECC
	EC_KEY			*ec = NULL;
	int			r, nid;
#endif

	*keyp = NULL;
	*labelp = NULL;

	memset(&cert_attr, 0, sizeof(cert_attr));
	cert_attr[0].type = CKA_ID;
	cert_attr[1].type = CKA_SUBJECT;
	cert_attr[2].type = CKA_VALUE;

	session = p->slotinfo[slotidx].session;
	f = p->function_list;

	/* figure out size of the attributes */
	rv = f->C_GetAttributeValue(session, *obj, cert_attr, 3);
	if (rv != CKR_OK) {
		error("C_GetAttributeValue failed: %lu", rv);
		return -1;
	}

	/*
	 * Allow CKA_ID (always first attribute) to be empty, but
	 * ensure that none of the others are zero length.
	 * XXX assumes CKA_ID is always first.
	 */
	if (cert_attr[1].ulValueLen == 0 ||
	    cert_attr[2].ulValueLen == 0) {
		error("invalid attribute length");
		return -1;
	}

	/* allocate buffers for attributes */
	for (i = 0; i < 3; i++)
		if (cert_attr[i].ulValueLen > 0)
			cert_attr[i].pValue = xcalloc(1, cert_attr[i].ulValueLen);

	/* retrieve ID, subject and value of certificate */
	rv = f->C_GetAttributeValue(session, *obj, cert_attr, 3);
	if (rv != CKR_OK) {
		error("C_GetAttributeValue failed: %lu", rv);
		goto out;
	}

	/* Decode DER-encoded cert subject */
	cp = cert_attr[1].pValue;
	if ((x509_name = d2i_X509_NAME(NULL, &cp,
	    cert_attr[1].ulValueLen)) == NULL ||
	    (subject = X509_NAME_oneline(x509_name, NULL, 0)) == NULL)
		subject = xstrdup("invalid subject");
	X509_NAME_free(x509_name);

	cp = cert_attr[2].pValue;
	if ((x509 = d2i_X509(NULL, &cp, cert_attr[2].ulValueLen)) == NULL) {
		error("d2i_x509 failed");
		goto out;
	}

	if ((evp = X509_get_pubkey(x509)) == NULL) {
		error("X509_get_pubkey failed");
		goto out;
	}

	if (EVP_PKEY_base_id(evp) == EVP_PKEY_RSA) {
		if (EVP_PKEY_get0_RSA(evp) == NULL) {
			error("invalid x509; no rsa key");
			goto out;
		}
		if ((rsa = RSAPublicKey_dup(EVP_PKEY_get0_RSA(evp))) == NULL) {
			error("RSAPublicKey_dup failed");
			goto out;
		}

		key = sshkey_new(KEY_UNSPEC);
		if (key == NULL) {
			error("sshkey_new failed");
			goto out;
		}

		EVP_PKEY_free(key->pkey);
		if ((key->pkey = EVP_PKEY_new()) == NULL)
			fatal("EVP_PKEY_new failed");
		if (EVP_PKEY_set1_RSA(key->pkey, rsa) != 1)
			fatal("EVP_PKEY_set1_RSA failed");
		key->type = KEY_RSA;
		key->flags |= SSHKEY_FLAG_EXT;
		if (EVP_PKEY_bits(key->pkey) < SSH_RSA_MINIMUM_MODULUS_SIZE) {
			error_f("RSA key too small %d < minimum %d",
			    EVP_PKEY_bits(key->pkey),
			    SSH_RSA_MINIMUM_MODULUS_SIZE);
			goto out;
		}
		if (pkcs11_record_key(p, slotidx, &cert_attr[0], key))
			goto out;
		/* success */
		success = 0;
#ifdef OPENSSL_HAS_ECC
	} else if (EVP_PKEY_base_id(evp) == EVP_PKEY_EC) {
		if (EVP_PKEY_get0_EC_KEY(evp) == NULL) {
			error("invalid x509; no ec key");
			goto out;
		}
		if ((ec = EC_KEY_dup(EVP_PKEY_get0_EC_KEY(evp))) == NULL) {
			error("EC_KEY_dup failed");
			goto out;
		}
		if ((r = sshkey_ec_validate_public(EC_KEY_get0_group(ec),
		    EC_KEY_get0_public_key(ec))) != 0) {
			error_fr(r, "invalid EC key");
			goto out;
		}
		nid = sshkey_ecdsa_key_to_nid(ec);
		if (nid < 0) {
			error("couldn't get curve nid");
			goto out;
		}

		key = sshkey_new(KEY_UNSPEC);
		if (key == NULL) {
			error("sshkey_new failed");
			goto out;
		}

		EVP_PKEY_free(key->pkey);
		if ((key->pkey = EVP_PKEY_new()) == NULL)
			fatal("EVP_PKEY_new failed");
		if (EVP_PKEY_set1_EC_KEY(key->pkey, ec) != 1)
			fatal("EVP_PKEY_set1_EC_KEY failed");
		key->ecdsa_nid = nid;
		key->type = KEY_ECDSA;
		key->flags |= SSHKEY_FLAG_EXT;
		if (pkcs11_record_key(p, slotidx, &cert_attr[0], key))
			goto out;
		/* success */
		success = 0;
#endif /* OPENSSL_HAS_ECC */
#ifdef OPENSSL_HAS_ED25519
	} else if (EVP_PKEY_base_id(evp) == EVP_PKEY_ED25519) {
		if ((key = sshkey_new(KEY_UNSPEC)) == NULL ||
		    (key->ed25519_pk = calloc(1, ED25519_PK_SZ)) == NULL)
			fatal_f("allocation failed");
		len = ED25519_PK_SZ;
		if (!EVP_PKEY_get_raw_public_key(evp, key->ed25519_pk, &len)) {
			ossl_error("EVP_PKEY_get_raw_public_key failed");
			goto out;
		}
		if (len != ED25519_PK_SZ) {
			error_f("incorrect returned public key "
			    "length for ed25519");
			goto out;
		}
		key->type = KEY_ED25519;
		key->flags |= SSHKEY_FLAG_EXT;
		if (pkcs11_record_key(p, slotidx, &cert_attr[0], key))
			goto out;
		/* success */
		success = 0;
#endif /* OPENSSL_HAS_ED25519 */
	} else {
		error("unknown certificate key type");
		goto out;
	}
 out:
	for (i = 0; i < 3; i++)
		free(cert_attr[i].pValue);
	X509_free(x509);
	RSA_free(rsa);
#ifdef OPENSSL_HAS_ECC
	EC_KEY_free(ec);
#endif /* OPENSSL_HAS_ECC */
	if (success != 0 || key == NULL) {
		sshkey_free(key);
		free(subject);
		return -1;
	}
	/* success */
	*keyp = key;
	*labelp = subject;
	return 0;
}
#endif /* WITH_OPENSSL */

static void
note_key(struct pkcs11_provider *p, CK_ULONG slotidx, const char *context,
    struct sshkey *key)
{
	char *fp;

	if ((fp = sshkey_fingerprint(key, SSH_FP_HASH_DEFAULT,
	    SSH_FP_DEFAULT)) == NULL) {
		error_f("sshkey_fingerprint failed");
		return;
	}
	debug2("%s: provider %s slot %lu: %s %s", context, p->name,
	    (u_long)slotidx, sshkey_type(key), fp);
	free(fp);
}

#ifdef WITH_OPENSSL /* libcrypto needed for certificate parsing */
/*
 * lookup certificates for token in slot identified by slotidx,
 * add 'wrapped' public keys to the 'keysp' array and increment nkeys.
 * keysp points to an (possibly empty) array with *nkeys keys.
 */
static int
pkcs11_fetch_certs(struct pkcs11_provider *p, CK_ULONG slotidx,
    struct sshkey ***keysp, char ***labelsp, int *nkeys)
{
	struct sshkey		*key = NULL;
	CK_OBJECT_CLASS		 key_class;
	CK_ATTRIBUTE		 key_attr[1];
	CK_SESSION_HANDLE	 session;
	CK_FUNCTION_LIST	*f = NULL;
	CK_RV			 rv;
	CK_OBJECT_HANDLE	 obj;
	CK_ULONG		 n = 0;
	int			 ret = -1;
	char			*label;

	memset(&key_attr, 0, sizeof(key_attr));
	memset(&obj, 0, sizeof(obj));

	key_class = CKO_CERTIFICATE;
	key_attr[0].type = CKA_CLASS;
	key_attr[0].pValue = &key_class;
	key_attr[0].ulValueLen = sizeof(key_class);

	session = p->slotinfo[slotidx].session;
	f = p->function_list;

	rv = f->C_FindObjectsInit(session, key_attr, 1);
	if (rv != CKR_OK) {
		error("C_FindObjectsInit failed: %lu", rv);
		goto fail;
	}

	while (1) {
		CK_CERTIFICATE_TYPE	ck_cert_type;

		rv = f->C_FindObjects(session, &obj, 1, &n);
		if (rv != CKR_OK) {
			error("C_FindObjects failed: %lu", rv);
			goto fail;
		}
		if (n == 0)
			break;

		memset(&ck_cert_type, 0, sizeof(ck_cert_type));
		memset(&key_attr, 0, sizeof(key_attr));
		key_attr[0].type = CKA_CERTIFICATE_TYPE;
		key_attr[0].pValue = &ck_cert_type;
		key_attr[0].ulValueLen = sizeof(ck_cert_type);

		rv = f->C_GetAttributeValue(session, obj, key_attr, 1);
		if (rv != CKR_OK) {
			error("C_GetAttributeValue failed: %lu", rv);
			goto fail;
		}

		key = NULL;
		label = NULL;
		switch (ck_cert_type) {
		case CKC_X_509:
			if (pkcs11_fetch_x509_pubkey(p, slotidx, &obj,
			    &key, &label) != 0) {
				error("failed to fetch key");
				continue;
			}
			break;
		default:
			error("skipping unsupported certificate type %lu",
			    ck_cert_type);
			continue;
		}
		note_key(p, slotidx, __func__, key);
		if (pkcs11_key_included(keysp, nkeys, key)) {
			debug2_f("key already included");
			sshkey_free(key);
		} else {
			/* expand key array and add key */
			*keysp = xrecallocarray(*keysp, *nkeys,
			    *nkeys + 1, sizeof(struct sshkey *));
			(*keysp)[*nkeys] = key;
			if (labelsp != NULL) {
				*labelsp = xrecallocarray(*labelsp, *nkeys,
				    *nkeys + 1, sizeof(char *));
				(*labelsp)[*nkeys] = xstrdup((char *)label);
			}
			*nkeys = *nkeys + 1;
			debug("have %d keys", *nkeys);
		}
	}

	ret = 0;
fail:
	rv = f->C_FindObjectsFinal(session);
	if (rv != CKR_OK) {
		error("C_FindObjectsFinal failed: %lu", rv);
		ret = -1;
	}

	return (ret);
}
#endif /* WITH_OPENSSL */

/*
 * lookup public keys for token in slot identified by slotidx,
 * add 'wrapped' public keys to the 'keysp' array and increment nkeys.
 * keysp points to an (possibly empty) array with *nkeys keys.
 */
static int
pkcs11_fetch_keys(struct pkcs11_provider *p, CK_ULONG slotidx,
    struct sshkey ***keysp, char ***labelsp, int *nkeys)
{
	struct sshkey		*key = NULL;
	CK_OBJECT_CLASS		 key_class;
	CK_ATTRIBUTE		 key_attr[2];
	CK_SESSION_HANDLE	 session;
	CK_FUNCTION_LIST	*f = NULL;
	CK_RV			 rv;
	CK_OBJECT_HANDLE	 obj;
	CK_ULONG		 n = 0;
	int			 ret = -1;

	memset(&key_attr, 0, sizeof(key_attr));
	memset(&obj, 0, sizeof(obj));

	key_class = CKO_PUBLIC_KEY;
	key_attr[0].type = CKA_CLASS;
	key_attr[0].pValue = &key_class;
	key_attr[0].ulValueLen = sizeof(key_class);

	session = p->slotinfo[slotidx].session;
	f = p->function_list;

	rv = f->C_FindObjectsInit(session, key_attr, 1);
	if (rv != CKR_OK) {
		error("C_FindObjectsInit failed: %lu", rv);
		goto fail;
	}

	while (1) {
		CK_KEY_TYPE	ck_key_type;
		CK_UTF8CHAR	label[256];

		rv = f->C_FindObjects(session, &obj, 1, &n);
		if (rv != CKR_OK) {
			error("C_FindObjects failed: %lu", rv);
			goto fail;
		}
		if (n == 0)
			break;

		memset(&ck_key_type, 0, sizeof(ck_key_type));
		memset(&key_attr, 0, sizeof(key_attr));
		key_attr[0].type = CKA_KEY_TYPE;
		key_attr[0].pValue = &ck_key_type;
		key_attr[0].ulValueLen = sizeof(ck_key_type);
		key_attr[1].type = CKA_LABEL;
		key_attr[1].pValue = &label;
		key_attr[1].ulValueLen = sizeof(label) - 1;

		rv = f->C_GetAttributeValue(session, obj, key_attr, 2);
		if (rv != CKR_OK) {
			error("C_GetAttributeValue failed: %lu", rv);
			goto fail;
		}

		label[key_attr[1].ulValueLen] = '\0';

		switch (ck_key_type) {
#ifdef WITH_OPENSSL
		case CKK_RSA:
			key = pkcs11_fetch_rsa_pubkey(p, slotidx, &obj);
			break;
#ifdef OPENSSL_HAS_ECC
		case CKK_ECDSA:
			key = pkcs11_fetch_ecdsa_pubkey(p, slotidx, &obj);
			break;
#endif /* OPENSSL_HAS_ECC */
#endif /* WITH_OPENSSL */
		case CKK_EC_EDWARDS:
			key = pkcs11_fetch_ed25519_pubkey(p, slotidx, &obj);
			break;
		default:
			/* XXX print key type? */
			key = NULL;
			error("skipping unsupported key type 0x%lx",
			    (u_long)ck_key_type);
		}

		if (key == NULL) {
			error("failed to fetch key");
			continue;
		}
		note_key(p, slotidx, __func__, key);
		if (pkcs11_key_included(keysp, nkeys, key)) {
			debug2_f("key already included");
			sshkey_free(key);
		} else {
			/* expand key array and add key */
			*keysp = xrecallocarray(*keysp, *nkeys,
			    *nkeys + 1, sizeof(struct sshkey *));
			(*keysp)[*nkeys] = key;
			if (labelsp != NULL) {
				*labelsp = xrecallocarray(*labelsp, *nkeys,
				    *nkeys + 1, sizeof(char *));
				(*labelsp)[*nkeys] = xstrdup((char *)label);
			}
			*nkeys = *nkeys + 1;
			debug("have %d keys", *nkeys);
		}
	}

	ret = 0;
fail:
	rv = f->C_FindObjectsFinal(session);
	if (rv != CKR_OK) {
		error("C_FindObjectsFinal failed: %lu", rv);
		ret = -1;
	}

	return (ret);
}

#ifdef WITH_PKCS11_KEYGEN
#define FILL_ATTR(attr, idx, typ, val, len) \
	{ (attr[idx]).type=(typ); (attr[idx]).pValue=(val); (attr[idx]).ulValueLen=len; idx++; }

static struct sshkey *
pkcs11_rsa_generate_private_key(struct pkcs11_provider *p, CK_ULONG slotidx,
    char *label, CK_ULONG bits, CK_BYTE keyid, u_int32_t *err)
{
	struct pkcs11_slotinfo	*si;
	char			*plabel = label ? label : "";
	int			 npub = 0, npriv = 0;
	CK_RV			 rv;
	CK_FUNCTION_LIST	*f;
	CK_SESSION_HANDLE	 session;
	CK_BBOOL		 true_val = CK_TRUE, false_val = CK_FALSE;
	CK_OBJECT_HANDLE	 pubKey, privKey;
	CK_ATTRIBUTE		 tpub[16], tpriv[16];
	CK_MECHANISM		 mech = {
	    CKM_RSA_PKCS_KEY_PAIR_GEN, NULL_PTR, 0
	};
	CK_BYTE			 pubExponent[] = {
	    0x01, 0x00, 0x01 /* RSA_F4 in bytes */
	};
	pubkey_filter[0].pValue = &pubkey_class;
	cert_filter[0].pValue = &cert_class;

	*err = 0;

	FILL_ATTR(tpub, npub, CKA_TOKEN, &true_val, sizeof(true_val));
	FILL_ATTR(tpub, npub, CKA_LABEL, plabel, strlen(plabel));
	FILL_ATTR(tpub, npub, CKA_ENCRYPT, &false_val, sizeof(false_val));
	FILL_ATTR(tpub, npub, CKA_VERIFY, &true_val, sizeof(true_val));
	FILL_ATTR(tpub, npub, CKA_VERIFY_RECOVER, &false_val,
	    sizeof(false_val));
	FILL_ATTR(tpub, npub, CKA_WRAP, &false_val, sizeof(false_val));
	FILL_ATTR(tpub, npub, CKA_DERIVE, &false_val, sizeof(false_val));
	FILL_ATTR(tpub, npub, CKA_MODULUS_BITS, &bits, sizeof(bits));
	FILL_ATTR(tpub, npub, CKA_PUBLIC_EXPONENT, pubExponent,
	    sizeof(pubExponent));
	FILL_ATTR(tpub, npub, CKA_ID, &keyid, sizeof(keyid));

	FILL_ATTR(tpriv, npriv, CKA_TOKEN,  &true_val, sizeof(true_val));
	FILL_ATTR(tpriv, npriv, CKA_LABEL,  plabel, strlen(plabel));
	FILL_ATTR(tpriv, npriv, CKA_PRIVATE,  &true_val, sizeof(true_val));
	FILL_ATTR(tpriv, npriv, CKA_SENSITIVE,  &true_val, sizeof(true_val));
	FILL_ATTR(tpriv, npriv, CKA_DECRYPT,  &false_val, sizeof(false_val));
	FILL_ATTR(tpriv, npriv, CKA_SIGN,  &true_val, sizeof(true_val));
	FILL_ATTR(tpriv, npriv, CKA_SIGN_RECOVER,  &false_val,
	    sizeof(false_val));
	FILL_ATTR(tpriv, npriv, CKA_UNWRAP,  &false_val, sizeof(false_val));
	FILL_ATTR(tpriv, npriv, CKA_DERIVE,  &false_val, sizeof(false_val));
	FILL_ATTR(tpriv, npriv, CKA_ID, &keyid, sizeof(keyid));

	f = p->function_list;
	si = &p->slotinfo[slotidx];
	session = si->session;

	if ((rv = f->C_GenerateKeyPair(session, &mech, tpub, npub, tpriv, npriv,
	    &pubKey, &privKey)) != CKR_OK) {
		error_f("key generation failed: error 0x%lx", rv);
		*err = rv;
		return NULL;
	}

	return pkcs11_fetch_rsa_pubkey(p, slotidx, &pubKey);
}

static int
h2i(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	else if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	else if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	else
		return -1;
}

static int
pkcs11_decode_hex(const char *hex, unsigned char **dest, size_t *rlen)
{
	size_t	i, len;

	if (dest)
		*dest = NULL;
	if (rlen)
		*rlen = 0;

	if ((len = strlen(hex)) % 2)
		return -1;
	len /= 2;

	*dest = xmalloc(len);

	for (i = 0; i < len; i++) {
		int hi, lo;

		hi = h2i(hex[2 * i]);
		lo = h2i(hex[(2 * i) + 1]);
		if (hi == -1 || lo == -1)
			return -1;
		(*dest)[i] = (hi << 4) | lo;
	}

	if (rlen)
		*rlen = len;

	return 0;
}

static struct ec_curve_info {
	const char	*name;
	const char	*oid;
	const char	*oid_encoded;
	size_t		 size;
} ec_curve_infos[] = {
	{"prime256v1",	"1.2.840.10045.3.1.7",	"06082A8648CE3D030107", 256},
	{"secp384r1",	"1.3.132.0.34",		"06052B81040022",	384},
	{"secp521r1",	"1.3.132.0.35",		"06052B81040023",	521},
	{NULL,		NULL,			NULL,			0},
};

static struct sshkey *
pkcs11_ecdsa_generate_private_key(struct pkcs11_provider *p, CK_ULONG slotidx,
    char *label, CK_ULONG bits, CK_BYTE keyid, u_int32_t *err)
{
	struct pkcs11_slotinfo	*si;
	char			*plabel = label ? label : "";
	int			 i;
	size_t			 ecparams_size;
	unsigned char		*ecparams = NULL;
	int			 npub = 0, npriv = 0;
	CK_RV			 rv;
	CK_FUNCTION_LIST	*f;
	CK_SESSION_HANDLE	 session;
	CK_BBOOL		 true_val = CK_TRUE, false_val = CK_FALSE;
	CK_OBJECT_HANDLE	 pubKey, privKey;
	CK_MECHANISM		 mech = {
	    CKM_EC_KEY_PAIR_GEN, NULL_PTR, 0
	};
	CK_ATTRIBUTE		 tpub[16], tpriv[16];

	*err = 0;

	for (i = 0; ec_curve_infos[i].name; i++) {
		if (ec_curve_infos[i].size == bits)
			break;
	}
	if (!ec_curve_infos[i].name) {
		error_f("invalid key size %lu", bits);
		return NULL;
	}
	if (pkcs11_decode_hex(ec_curve_infos[i].oid_encoded, &ecparams,
	    &ecparams_size) == -1) {
		error_f("invalid oid");
		return NULL;
	}

	FILL_ATTR(tpub, npub, CKA_TOKEN, &true_val, sizeof(true_val));
	FILL_ATTR(tpub, npub, CKA_LABEL, plabel, strlen(plabel));
	FILL_ATTR(tpub, npub, CKA_ENCRYPT, &false_val, sizeof(false_val));
	FILL_ATTR(tpub, npub, CKA_VERIFY, &true_val, sizeof(true_val));
	FILL_ATTR(tpub, npub, CKA_VERIFY_RECOVER, &false_val,
	    sizeof(false_val));
	FILL_ATTR(tpub, npub, CKA_WRAP, &false_val, sizeof(false_val));
	FILL_ATTR(tpub, npub, CKA_DERIVE, &false_val, sizeof(false_val));
	FILL_ATTR(tpub, npub, CKA_EC_PARAMS, ecparams, ecparams_size);
	FILL_ATTR(tpub, npub, CKA_ID, &keyid, sizeof(keyid));

	FILL_ATTR(tpriv, npriv, CKA_TOKEN, &true_val, sizeof(true_val));
	FILL_ATTR(tpriv, npriv, CKA_LABEL, plabel, strlen(plabel));
	FILL_ATTR(tpriv, npriv, CKA_PRIVATE, &true_val, sizeof(true_val));
	FILL_ATTR(tpriv, npriv, CKA_SENSITIVE, &true_val, sizeof(true_val));
	FILL_ATTR(tpriv, npriv, CKA_DECRYPT, &false_val, sizeof(false_val));
	FILL_ATTR(tpriv, npriv, CKA_SIGN, &true_val, sizeof(true_val));
	FILL_ATTR(tpriv, npriv, CKA_SIGN_RECOVER, &false_val,
	    sizeof(false_val));
	FILL_ATTR(tpriv, npriv, CKA_UNWRAP, &false_val, sizeof(false_val));
	FILL_ATTR(tpriv, npriv, CKA_DERIVE, &false_val, sizeof(false_val));
	FILL_ATTR(tpriv, npriv, CKA_ID, &keyid, sizeof(keyid));

	f = p->function_list;
	si = &p->slotinfo[slotidx];
	session = si->session;

	if ((rv = f->C_GenerateKeyPair(session, &mech, tpub, npub, tpriv, npriv,
	    &pubKey, &privKey)) != CKR_OK) {
		error_f("key generation failed: error 0x%lx", rv);
		*err = rv;
		return NULL;
	}

	return pkcs11_fetch_ecdsa_pubkey(p, slotidx, &pubKey);
}
#endif /* WITH_PKCS11_KEYGEN */

/*
 * register a new provider, fails if provider already exists. if
 * keyp is provided, fetch keys.
 */
static int
pkcs11_register_provider(char *provider_id, char *pin,
    struct sshkey ***keyp, char ***labelsp,
    struct pkcs11_provider **providerp, CK_ULONG user)
{
	int nkeys, need_finalize = 0;
	int ret = -1;
	struct pkcs11_provider *p = NULL;
	void *handle = NULL;
	CK_RV (*getfunctionlist)(CK_FUNCTION_LIST **);
	CK_RV rv;
	CK_FUNCTION_LIST *f = NULL;
	CK_TOKEN_INFO *token;
	CK_ULONG i;

	if (providerp == NULL)
		goto fail;
	*providerp = NULL;

	if (keyp != NULL)
		*keyp = NULL;
	if (labelsp != NULL)
		*labelsp = NULL;

	if (pkcs11_provider_lookup(provider_id) != NULL) {
		debug_f("provider already registered: %s", provider_id);
		goto fail;
	}
	if (lib_contains_symbol(provider_id, "C_GetFunctionList") != 0) {
		error("provider %s is not a PKCS11 library", provider_id);
		goto fail;
	}
	/* open shared pkcs11-library */
	if ((handle = dlopen(provider_id, RTLD_NOW)) == NULL) {
		error("dlopen %s failed: %s", provider_id, dlerror());
		goto fail;
	}
	if ((getfunctionlist = dlsym(handle, "C_GetFunctionList")) == NULL)
		fatal("dlsym(C_GetFunctionList) failed: %s", dlerror());
	p = xcalloc(1, sizeof(*p));
	p->name = xstrdup(provider_id);
	p->handle = handle;
	/* setup the pkcs11 callbacks */
	if ((rv = (*getfunctionlist)(&f)) != CKR_OK) {
		error("C_GetFunctionList for provider %s failed: %lu",
		    provider_id, rv);
		goto fail;
	}
	p->function_list = f;
	if ((rv = f->C_Initialize(NULL)) != CKR_OK) {
		error("C_Initialize for provider %s failed: %lu",
		    provider_id, rv);
		goto fail;
	}
	need_finalize = 1;
	if ((rv = f->C_GetInfo(&p->info)) != CKR_OK) {
		error("C_GetInfo for provider %s failed: %lu",
		    provider_id, rv);
		goto fail;
	}
	debug("provider %s: manufacturerID <%.*s> cryptokiVersion %d.%d"
	    " libraryDescription <%.*s> libraryVersion %d.%d",
	    provider_id,
	    RMSPACE(p->info.manufacturerID),
	    p->info.cryptokiVersion.major,
	    p->info.cryptokiVersion.minor,
	    RMSPACE(p->info.libraryDescription),
	    p->info.libraryVersion.major,
	    p->info.libraryVersion.minor);
	if ((rv = f->C_GetSlotList(CK_TRUE, NULL, &p->nslots)) != CKR_OK) {
		error("C_GetSlotList failed: %lu", rv);
		goto fail;
	}
	if (p->nslots == 0) {
		debug_f("provider %s returned no slots", provider_id);
		ret = -SSH_PKCS11_ERR_NO_SLOTS;
		goto fail;
	}
	p->slotlist = xcalloc(p->nslots, sizeof(CK_SLOT_ID));
	if ((rv = f->C_GetSlotList(CK_TRUE, p->slotlist, &p->nslots))
	    != CKR_OK) {
		error("C_GetSlotList for provider %s failed: %lu",
		    provider_id, rv);
		goto fail;
	}
	p->slotinfo = xcalloc(p->nslots, sizeof(struct pkcs11_slotinfo));
	p->valid = 1;
	nkeys = 0;
	for (i = 0; i < p->nslots; i++) {
		token = &p->slotinfo[i].token;
		if ((rv = f->C_GetTokenInfo(p->slotlist[i], token))
		    != CKR_OK) {
			error("C_GetTokenInfo for provider %s slot %lu "
			    "failed: %lu", provider_id, (u_long)i, rv);
			continue;
		}
		if ((token->flags & CKF_TOKEN_INITIALIZED) == 0) {
			debug2_f("ignoring uninitialised token in "
			    "provider %s slot %lu", provider_id, (u_long)i);
			continue;
		}
		debug("provider %s slot %lu: label <%.*s> "
		    "manufacturerID <%.*s> model <%.*s> serial <%.*s> "
		    "flags 0x%lx",
		    provider_id, (unsigned long)i,
		    RMSPACE(token->label), RMSPACE(token->manufacturerID),
		    RMSPACE(token->model), RMSPACE(token->serialNumber),
		    token->flags);
		/*
		 * open session, login with pin and retrieve public
		 * keys (if keyp is provided)
		 */
		if ((ret = pkcs11_open_session(p, i, pin, user)) != 0 ||
		    keyp == NULL)
			continue;
		pkcs11_fetch_keys(p, i, keyp, labelsp, &nkeys);
#ifdef WITH_OPENSSL
		pkcs11_fetch_certs(p, i, keyp, labelsp, &nkeys);
#endif
		if (nkeys == 0 && !p->slotinfo[i].logged_in &&
		    pkcs11_interactive) {
			/*
			 * Some tokens require login before they will
			 * expose keys.
			 */
			if (pkcs11_login_slot(p, &p->slotinfo[i],
			    CKU_USER) < 0) {
				error("login failed");
				continue;
			}
			pkcs11_fetch_keys(p, i, keyp, labelsp, &nkeys);
#ifdef WITH_OPENSSL
			pkcs11_fetch_certs(p, i, keyp, labelsp, &nkeys);
#endif
		}
	}

	/* now owned by caller */
	*providerp = p;

	TAILQ_INSERT_TAIL(&pkcs11_providers, p, next);
	p->refcount++;	/* add to provider list */

	return (nkeys);
fail:
	if (need_finalize && (rv = f->C_Finalize(NULL)) != CKR_OK)
		error("C_Finalize for provider %s failed: %lu",
		    provider_id, rv);
	if (p) {
		free(p->name);
		free(p->slotlist);
		free(p->slotinfo);
		free(p);
	}
	if (handle)
		dlclose(handle);
	if (ret > 0)
		ret = -1;
	return (ret);
}

int
pkcs11_init(int interactive)
{
	debug3_f("called, interactive = %d", interactive);

	pkcs11_interactive = interactive;
	TAILQ_INIT(&pkcs11_providers);
	TAILQ_INIT(&pkcs11_keys);
	return (0);
}

/* unregister all providers, keys might still point to the providers */
void
pkcs11_terminate(void)
{
	struct pkcs11_provider *p;
	struct pkcs11_key *k11;

	debug3_f("called");

	while ((k11 = TAILQ_FIRST(&pkcs11_keys)) != NULL) {
		TAILQ_REMOVE(&pkcs11_keys, k11, next);
		pkcs11_k11_free(k11);
	}
	while ((p = TAILQ_FIRST(&pkcs11_providers)) != NULL) {
		TAILQ_REMOVE(&pkcs11_providers, p, next);
		pkcs11_provider_finalize(p);
		pkcs11_provider_unref(p);
	}
}

/*
 * register a new provider and get number of keys hold by the token,
 * fails if provider already exists
 */
int
pkcs11_add_provider(char *provider_id, char *pin, struct sshkey ***keyp,
    char ***labelsp)
{
	struct pkcs11_provider *p = NULL;
	int nkeys;

	nkeys = pkcs11_register_provider(provider_id, pin, keyp, labelsp,
	    &p, CKU_USER);

	/* no keys found or some other error, de-register provider */
	if (nkeys <= 0 && p != NULL) {
		TAILQ_REMOVE(&pkcs11_providers, p, next);
		pkcs11_provider_finalize(p);
		pkcs11_provider_unref(p);
	}
	if (nkeys == 0)
		debug_f("provider %s returned no keys", provider_id);

	return (nkeys);
}

int
pkcs11_sign(struct sshkey *key,
    u_char **sigp, size_t *lenp,
    const u_char *data, size_t datalen,
    const char *alg, const char *sk_provider,
    const char *sk_pin, u_int compat)
{
	switch (key->type) {
	case KEY_RSA:
	case KEY_RSA_CERT:
#ifdef WITH_OPENSSL
		return pkcs11_sign_rsa(key, sigp, lenp, data, datalen,
		    alg, sk_provider, sk_pin, compat);
#ifdef OPENSSL_HAS_ECC
	case KEY_ECDSA:
	case KEY_ECDSA_CERT:
		return pkcs11_sign_ecdsa(key, sigp, lenp, data, datalen,
		    alg, sk_provider, sk_pin, compat);
#endif /* OPENSSL_HAS_ECC */
#endif /* WITH_OPENSSL */
	case KEY_ED25519:
	case KEY_ED25519_CERT:
		return pkcs11_sign_ed25519(key, sigp, lenp, data, datalen,
		    alg, sk_provider, sk_pin, compat);
	default:
		return SSH_ERR_KEY_TYPE_UNKNOWN;
	}
}

void
pkcs11_key_free(struct sshkey *key)
{
	/* never called */
}

#ifdef WITH_PKCS11_KEYGEN
struct sshkey *
pkcs11_gakp(char *provider_id, char *pin, unsigned int slotidx, char *label,
    unsigned int type, unsigned int bits, unsigned char keyid, u_int32_t *err)
{
	struct pkcs11_provider	*p = NULL;
	struct pkcs11_slotinfo	*si;
	CK_FUNCTION_LIST	*f;
	CK_SESSION_HANDLE	 session;
	struct sshkey		*k = NULL;
	int			 ret = -1, reset_pin = 0, reset_provider = 0;
	CK_RV			 rv;

	*err = 0;

	if ((p = pkcs11_provider_lookup(provider_id)) != NULL)
		debug_f("provider \"%s\" available", provider_id);
	else if ((ret = pkcs11_register_provider(provider_id, pin, NULL, NULL,
	    &p, CKU_SO)) < 0) {
		debug_f("could not register provider %s", provider_id);
		goto out;
	} else
		reset_provider = 1;

	f = p->function_list;
	si = &p->slotinfo[slotidx];
	session = si->session;

	if ((rv = f->C_SetOperationState(session , pin, strlen(pin),
	    CK_INVALID_HANDLE, CK_INVALID_HANDLE)) != CKR_OK) {
		debug_f("could not supply SO pin: %lu", rv);
		reset_pin = 0;
	} else
		reset_pin = 1;

	switch (type) {
	case KEY_RSA:
		if ((k = pkcs11_rsa_generate_private_key(p, slotidx, label,
		    bits, keyid, err)) == NULL) {
			debug_f("failed to generate RSA key");
			goto out;
		}
		break;
	case KEY_ECDSA:
		if ((k = pkcs11_ecdsa_generate_private_key(p, slotidx, label,
		    bits, keyid, err)) == NULL) {
			debug_f("failed to generate ECDSA key");
			goto out;
		}
		break;
	default:
		*err = SSH_PKCS11_ERR_GENERIC;
		debug_f("unknown type %d", type);
		goto out;
	}

out:
	if (reset_pin)
		f->C_SetOperationState(session , NULL, 0, CK_INVALID_HANDLE,
		    CK_INVALID_HANDLE);

	if (reset_provider)
		pkcs11_del_provider(provider_id);

	return (k);
}

struct sshkey *
pkcs11_destroy_keypair(char *provider_id, char *pin, unsigned long slotidx,
    unsigned char keyid, u_int32_t *err)
{
	struct pkcs11_provider	*p = NULL;
	struct pkcs11_slotinfo	*si;
	struct sshkey		*k = NULL;
	int			 reset_pin = 0, reset_provider = 0;
	CK_ULONG		 nattrs;
	CK_FUNCTION_LIST	*f;
	CK_SESSION_HANDLE	 session;
	CK_ATTRIBUTE		 attrs[16];
	CK_OBJECT_CLASS		 key_class;
	CK_KEY_TYPE		 key_type;
	CK_OBJECT_HANDLE	 obj = CK_INVALID_HANDLE;
	CK_RV			 rv;

	*err = 0;

	if ((p = pkcs11_provider_lookup(provider_id)) != NULL) {
		debug_f("using provider \"%s\"", provider_id);
	} else if (pkcs11_register_provider(provider_id, pin, NULL, NULL, &p,
	    CKU_SO) < 0) {
		debug_f("could not register provider %s",
		    provider_id);
		goto out;
	} else
		reset_provider = 1;

	f = p->function_list;
	si = &p->slotinfo[slotidx];
	session = si->session;

	if ((rv = f->C_SetOperationState(session , pin, strlen(pin),
	    CK_INVALID_HANDLE, CK_INVALID_HANDLE)) != CKR_OK) {
		debug_f("could not supply SO pin: %lu", rv);
		reset_pin = 0;
	} else
		reset_pin = 1;

	/* private key */
	nattrs = 0;
	key_class = CKO_PRIVATE_KEY;
	FILL_ATTR(attrs, nattrs, CKA_CLASS, &key_class, sizeof(key_class));
	FILL_ATTR(attrs, nattrs, CKA_ID, &keyid, sizeof(keyid));

	if (pkcs11_find(p, slotidx, attrs, nattrs, &obj) == 0 &&
	    obj != CK_INVALID_HANDLE) {
		if ((rv = f->C_DestroyObject(session, obj)) != CKR_OK) {
			debug_f("could not destroy private key 0x%hhx",
			    keyid);
			*err = rv;
			goto out;
		}
	}

	/* public key */
	nattrs = 0;
	key_class = CKO_PUBLIC_KEY;
	FILL_ATTR(attrs, nattrs, CKA_CLASS, &key_class, sizeof(key_class));
	FILL_ATTR(attrs, nattrs, CKA_ID, &keyid, sizeof(keyid));

	if (pkcs11_find(p, slotidx, attrs, nattrs, &obj) == 0 &&
	    obj != CK_INVALID_HANDLE) {

		/* get key type */
		nattrs = 0;
		FILL_ATTR(attrs, nattrs, CKA_KEY_TYPE, &key_type,
		    sizeof(key_type));
		rv = f->C_GetAttributeValue(session, obj, attrs, nattrs);
		if (rv != CKR_OK) {
			debug_f("could not get key type of public key 0x%hhx",
			    keyid);
			*err = rv;
			key_type = -1;
		}
		switch (key_type) {
#ifdef WITH_OPENSSL
		case CKK_RSA:
			k = pkcs11_fetch_rsa_pubkey(p, slotidx, &obj);
			break;
#ifdef OPENSSL_HAS_ECC
		case CKK_ECDSA:
			k = pkcs11_fetch_ecdsa_pubkey(p, slotidx, &obj);
			break;
#endif /* OPENSSL_HAS_ECC */
#endif /* WITH_OPENSSL */
		case CKK_EC_EDWARDS:
			k = pkcs11_fetch_ed25519_pubkey(p, slotidx, &obj);
			break;
		default:
			debug_f("unsupported key type %lu", (u_long)key_type);
			break;
		}

		if ((rv = f->C_DestroyObject(session, obj)) != CKR_OK) {
			debug_f("could not destroy public key 0x%hhx", keyid);
			*err = rv;
			goto out;
		}
	}

out:
	if (reset_pin)
		f->C_SetOperationState(session , NULL, 0, CK_INVALID_HANDLE,
		    CK_INVALID_HANDLE);

	if (reset_provider)
		pkcs11_del_provider(provider_id);

	return (k);
}
#endif /* WITH_PKCS11_KEYGEN */
#else /* ENABLE_PKCS11 */

#include <sys/types.h>
#include <stdarg.h>
#include <stdio.h>

#include "log.h"
#include "sshkey.h"
#include "ssherr.h"
#include "ssh-pkcs11.h"

int
pkcs11_init(int interactive)
{
	error_f("PKCS#11 not supported");
	return (-1);
}

int
pkcs11_add_provider(char *provider_id, char *pin, struct sshkey ***keyp,
    char ***labelsp)
{
	error_f("PKCS#11 not supported");
	return (-1);
}

void
pkcs11_key_free(struct sshkey *key)
{
	error_f("PKCS#11 not supported");
}

int
pkcs11_sign(struct sshkey *key,
    u_char **sigp, size_t *lenp,
    const u_char *data, size_t datalen,
    const char *alg, const char *sk_provider,
    const char *sk_pin, u_int compat)
{
	error_f("PKCS#11 not supported");
	return SSH_ERR_FEATURE_UNSUPPORTED;
}

void
pkcs11_terminate(void)
{
	error_f("PKCS#11 not supported");
}
#endif /* ENABLE_PKCS11 */
