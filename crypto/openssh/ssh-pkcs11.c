/* $OpenBSD: ssh-pkcs11.c,v 1.56 2023/03/08 05:33:53 tb Exp $ */
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

#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif

#include <sys/types.h>
#include <stdarg.h>
#include <stdio.h>

#include <ctype.h>
#include <string.h>
#include <dlfcn.h>

#include "openbsd-compat/sys-queue.h"
#include "openbsd-compat/openssl-compat.h"

#include <openssl/ecdsa.h>
#include <openssl/x509.h>
#include <openssl/err.h>

#define CRYPTOKI_COMPAT
#include "pkcs11.h"

#include "log.h"
#include "misc.h"
#include "sshkey.h"
#include "ssh-pkcs11.h"
#include "digest.h"
#include "xmalloc.h"

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
	struct pkcs11_provider	*provider;
	CK_ULONG		slotidx;
	char			*keyid;
	int			keyid_len;
};

int pkcs11_interactive = 0;

#if defined(OPENSSL_HAS_ECC) && defined(HAVE_EC_KEY_METHOD_NEW)
static void
ossl_error(const char *msg)
{
	unsigned long    e;

	error_f("%s", msg);
	while ((e = ERR_get_error()) != 0)
		error_f("libcrypto error: %s", ERR_error_string(e, NULL));
}
#endif /* OPENSSL_HAS_ECC && HAVE_EC_KEY_METHOD_NEW */

int
pkcs11_init(int interactive)
{
	pkcs11_interactive = interactive;
	TAILQ_INIT(&pkcs11_providers);
	return (0);
}

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

/* unregister all providers, keys might still point to the providers */
void
pkcs11_terminate(void)
{
	struct pkcs11_provider *p;

	while ((p = TAILQ_FIRST(&pkcs11_providers)) != NULL) {
		TAILQ_REMOVE(&pkcs11_providers, p, next);
		pkcs11_provider_finalize(p);
		pkcs11_provider_unref(p);
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

static RSA_METHOD *rsa_method;
static int rsa_idx = 0;
#if defined(OPENSSL_HAS_ECC) && defined(HAVE_EC_KEY_METHOD_NEW)
static EC_KEY_METHOD *ec_key_method;
static int ec_key_idx = 0;
#endif /* OPENSSL_HAS_ECC && HAVE_EC_KEY_METHOD_NEW */

/* release a wrapped object */
static void
pkcs11_k11_free(void *parent, void *ptr, CRYPTO_EX_DATA *ad, int idx,
    long argl, void *argp)
{
	struct pkcs11_key	*k11 = ptr;

	debug_f("parent %p ptr %p idx %d", parent, ptr, idx);
	if (k11 == NULL)
		return;
	if (k11->provider)
		pkcs11_provider_unref(k11->provider);
	free(k11->keyid);
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

/* openssl callback doing the actual signing operation */
static int
pkcs11_rsa_private_encrypt(int flen, const u_char *from, u_char *to, RSA *rsa,
    int padding)
{
	struct pkcs11_key	*k11;
	struct pkcs11_slotinfo	*si;
	CK_FUNCTION_LIST	*f;
	CK_ULONG		tlen = 0;
	CK_RV			rv;
	int			rval = -1;

	if ((k11 = RSA_get_ex_data(rsa, rsa_idx)) == NULL) {
		error("RSA_get_ex_data failed");
		return (-1);
	}

	if (pkcs11_get_key(k11, CKM_RSA_PKCS) == -1) {
		error("pkcs11_get_key failed");
		return (-1);
	}

	f = k11->provider->function_list;
	si = &k11->provider->slotinfo[k11->slotidx];
	tlen = RSA_size(rsa);

	/* XXX handle CKR_BUFFER_TOO_SMALL */
	rv = f->C_Sign(si->session, (CK_BYTE *)from, flen, to, &tlen);
	if (rv == CKR_OK)
		rval = tlen;
	else
		error("C_Sign failed: %lu", rv);

	return (rval);
}

static int
pkcs11_rsa_private_decrypt(int flen, const u_char *from, u_char *to, RSA *rsa,
    int padding)
{
	return (-1);
}

static int
pkcs11_rsa_start_wrapper(void)
{
	if (rsa_method != NULL)
		return (0);
	rsa_method = RSA_meth_dup(RSA_get_default_method());
	if (rsa_method == NULL)
		return (-1);
	rsa_idx = RSA_get_ex_new_index(0, "ssh-pkcs11-rsa",
	    NULL, NULL, pkcs11_k11_free);
	if (rsa_idx == -1)
		return (-1);
	if (!RSA_meth_set1_name(rsa_method, "pkcs11") ||
	    !RSA_meth_set_priv_enc(rsa_method, pkcs11_rsa_private_encrypt) ||
	    !RSA_meth_set_priv_dec(rsa_method, pkcs11_rsa_private_decrypt)) {
		error_f("setup pkcs11 method failed");
		return (-1);
	}
	return (0);
}

/* redirect private key operations for rsa key to pkcs11 token */
static int
pkcs11_rsa_wrap(struct pkcs11_provider *provider, CK_ULONG slotidx,
    CK_ATTRIBUTE *keyid_attrib, RSA *rsa)
{
	struct pkcs11_key	*k11;

	if (pkcs11_rsa_start_wrapper() == -1)
		return (-1);

	k11 = xcalloc(1, sizeof(*k11));
	k11->provider = provider;
	provider->refcount++;	/* provider referenced by RSA key */
	k11->slotidx = slotidx;
	/* identify key object on smartcard */
	k11->keyid_len = keyid_attrib->ulValueLen;
	if (k11->keyid_len > 0) {
		k11->keyid = xmalloc(k11->keyid_len);
		memcpy(k11->keyid, keyid_attrib->pValue, k11->keyid_len);
	}

	RSA_set_method(rsa, rsa_method);
	RSA_set_ex_data(rsa, rsa_idx, k11);
	return (0);
}

#if defined(OPENSSL_HAS_ECC) && defined(HAVE_EC_KEY_METHOD_NEW)
/* openssl callback doing the actual signing operation */
static ECDSA_SIG *
ecdsa_do_sign(const unsigned char *dgst, int dgst_len, const BIGNUM *inv,
    const BIGNUM *rp, EC_KEY *ec)
{
	struct pkcs11_key	*k11;
	struct pkcs11_slotinfo	*si;
	CK_FUNCTION_LIST	*f;
	CK_ULONG		siglen = 0, bnlen;
	CK_RV			rv;
	ECDSA_SIG		*ret = NULL;
	u_char			*sig;
	BIGNUM			*r = NULL, *s = NULL;

	if ((k11 = EC_KEY_get_ex_data(ec, ec_key_idx)) == NULL) {
		ossl_error("EC_KEY_get_ex_data failed for ec");
		return (NULL);
	}

	if (pkcs11_get_key(k11, CKM_ECDSA) == -1) {
		error("pkcs11_get_key failed");
		return (NULL);
	}

	f = k11->provider->function_list;
	si = &k11->provider->slotinfo[k11->slotidx];

	siglen = ECDSA_size(ec);
	sig = xmalloc(siglen);

	/* XXX handle CKR_BUFFER_TOO_SMALL */
	rv = f->C_Sign(si->session, (CK_BYTE *)dgst, dgst_len, sig, &siglen);
	if (rv != CKR_OK) {
		error("C_Sign failed: %lu", rv);
		goto done;
	}
	if (siglen < 64 || siglen > 132 || siglen % 2) {
		error_f("bad signature length: %lu", (u_long)siglen);
		goto done;
	}
	bnlen = siglen/2;
	if ((ret = ECDSA_SIG_new()) == NULL) {
		error("ECDSA_SIG_new failed");
		goto done;
	}
	if ((r = BN_bin2bn(sig, bnlen, NULL)) == NULL ||
	    (s = BN_bin2bn(sig+bnlen, bnlen, NULL)) == NULL) {
		ossl_error("BN_bin2bn failed");
		ECDSA_SIG_free(ret);
		ret = NULL;
		goto done;
	}
	if (!ECDSA_SIG_set0(ret, r, s)) {
		error_f("ECDSA_SIG_set0 failed");
		ECDSA_SIG_free(ret);
		ret = NULL;
		goto done;
	}
	r = s = NULL; /* now owned by ret */
	/* success */
 done:
	BN_free(r);
	BN_free(s);
	free(sig);

	return (ret);
}

static int
pkcs11_ecdsa_start_wrapper(void)
{
	int (*orig_sign)(int, const unsigned char *, int, unsigned char *,
	    unsigned int *, const BIGNUM *, const BIGNUM *, EC_KEY *) = NULL;

	if (ec_key_method != NULL)
		return (0);
	ec_key_idx = EC_KEY_get_ex_new_index(0, "ssh-pkcs11-ecdsa",
	    NULL, NULL, pkcs11_k11_free);
	if (ec_key_idx == -1)
		return (-1);
	ec_key_method = EC_KEY_METHOD_new(EC_KEY_OpenSSL());
	if (ec_key_method == NULL)
		return (-1);
	EC_KEY_METHOD_get_sign(ec_key_method, &orig_sign, NULL, NULL);
	EC_KEY_METHOD_set_sign(ec_key_method, orig_sign, NULL, ecdsa_do_sign);
	return (0);
}

static int
pkcs11_ecdsa_wrap(struct pkcs11_provider *provider, CK_ULONG slotidx,
    CK_ATTRIBUTE *keyid_attrib, EC_KEY *ec)
{
	struct pkcs11_key	*k11;

	if (pkcs11_ecdsa_start_wrapper() == -1)
		return (-1);

	k11 = xcalloc(1, sizeof(*k11));
	k11->provider = provider;
	provider->refcount++;	/* provider referenced by ECDSA key */
	k11->slotidx = slotidx;
	/* identify key object on smartcard */
	k11->keyid_len = keyid_attrib->ulValueLen;
	if (k11->keyid_len > 0) {
		k11->keyid = xmalloc(k11->keyid_len);
		memcpy(k11->keyid, keyid_attrib->pValue, k11->keyid_len);
	}
	EC_KEY_set_method(ec, ec_key_method);
	EC_KEY_set_ex_data(ec, ec_key_idx, k11);

	return (0);
}
#endif /* OPENSSL_HAS_ECC && HAVE_EC_KEY_METHOD_NEW */

/* remove trailing spaces */
static void
rmspace(u_char *buf, size_t len)
{
	size_t i;

	if (!len)
		return;
	for (i = len - 1;  i > 0; i--)
		if (i == len - 1 || buf[i] == ' ')
			buf[i] = '\0';
		else
			break;
}

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

#if defined(OPENSSL_HAS_ECC) && defined(HAVE_EC_KEY_METHOD_NEW)
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
	int			 i;
	int			 nid;

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

	nid = sshkey_ecdsa_key_to_nid(ec);
	if (nid < 0) {
		error("couldn't get curve nid");
		goto fail;
	}

	if (pkcs11_ecdsa_wrap(p, slotidx, &key_attr[0], ec))
		goto fail;

	key = sshkey_new(KEY_UNSPEC);
	if (key == NULL) {
		error("sshkey_new failed");
		goto fail;
	}

	key->ecdsa = ec;
	key->ecdsa_nid = nid;
	key->type = KEY_ECDSA;
	key->flags |= SSHKEY_FLAG_EXT;
	ec = NULL;	/* now owned by key */

fail:
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
#endif /* OPENSSL_HAS_ECC && HAVE_EC_KEY_METHOD_NEW */

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
	int			 i;

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

	if (pkcs11_rsa_wrap(p, slotidx, &key_attr[0], rsa))
		goto fail;

	key = sshkey_new(KEY_UNSPEC);
	if (key == NULL) {
		error("sshkey_new failed");
		goto fail;
	}

	key->rsa = rsa;
	key->type = KEY_RSA;
	key->flags |= SSHKEY_FLAG_EXT;
	rsa = NULL;	/* now owned by key */

fail:
	for (i = 0; i < 3; i++)
		free(key_attr[i].pValue);
	RSA_free(rsa);

	return (key);
}

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
#ifdef OPENSSL_HAS_ECC
	EC_KEY			*ec = NULL;
#endif
	struct sshkey		*key = NULL;
	int			 i;
#if defined(OPENSSL_HAS_ECC) && defined(HAVE_EC_KEY_METHOD_NEW)
	int			 nid;
#endif
	const u_char		*cp;
	char			*subject = NULL;

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

		if (pkcs11_rsa_wrap(p, slotidx, &cert_attr[0], rsa))
			goto out;

		key = sshkey_new(KEY_UNSPEC);
		if (key == NULL) {
			error("sshkey_new failed");
			goto out;
		}

		key->rsa = rsa;
		key->type = KEY_RSA;
		key->flags |= SSHKEY_FLAG_EXT;
		rsa = NULL;	/* now owned by key */
#if defined(OPENSSL_HAS_ECC) && defined(HAVE_EC_KEY_METHOD_NEW)
	} else if (EVP_PKEY_base_id(evp) == EVP_PKEY_EC) {
		if (EVP_PKEY_get0_EC_KEY(evp) == NULL) {
			error("invalid x509; no ec key");
			goto out;
		}
		if ((ec = EC_KEY_dup(EVP_PKEY_get0_EC_KEY(evp))) == NULL) {
			error("EC_KEY_dup failed");
			goto out;
		}

		nid = sshkey_ecdsa_key_to_nid(ec);
		if (nid < 0) {
			error("couldn't get curve nid");
			goto out;
		}

		if (pkcs11_ecdsa_wrap(p, slotidx, &cert_attr[0], ec))
			goto out;

		key = sshkey_new(KEY_UNSPEC);
		if (key == NULL) {
			error("sshkey_new failed");
			goto out;
		}

		key->ecdsa = ec;
		key->ecdsa_nid = nid;
		key->type = KEY_ECDSA;
		key->flags |= SSHKEY_FLAG_EXT;
		ec = NULL;	/* now owned by key */
#endif /* OPENSSL_HAS_ECC && HAVE_EC_KEY_METHOD_NEW */
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
#endif
	if (key == NULL) {
		free(subject);
		return -1;
	}
	/* success */
	*keyp = key;
	*labelp = subject;
	return 0;
}

#if 0
static int
have_rsa_key(const RSA *rsa)
{
	const BIGNUM *rsa_n, *rsa_e;

	RSA_get0_key(rsa, &rsa_n, &rsa_e, NULL);
	return rsa_n != NULL && rsa_e != NULL;
}
#endif

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
			debug2_f("key already included");;
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
		case CKK_RSA:
			key = pkcs11_fetch_rsa_pubkey(p, slotidx, &obj);
			break;
#if defined(OPENSSL_HAS_ECC) && defined(HAVE_EC_KEY_METHOD_NEW)
		case CKK_ECDSA:
			key = pkcs11_fetch_ecdsa_pubkey(p, slotidx, &obj);
			break;
#endif /* OPENSSL_HAS_ECC && HAVE_EC_KEY_METHOD_NEW */
		default:
			/* XXX print key type? */
			key = NULL;
			error("skipping unsupported key type");
		}

		if (key == NULL) {
			error("failed to fetch key");
			continue;
		}
		note_key(p, slotidx, __func__, key);
		if (pkcs11_key_included(keysp, nkeys, key)) {
			debug2_f("key already included");;
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
pkcs11_decode_hex(const char *hex, unsigned char **dest, size_t *rlen)
{
	size_t	i, len;
	char	ptr[3];

	if (dest)
		*dest = NULL;
	if (rlen)
		*rlen = 0;

	if ((len = strlen(hex)) % 2)
		return -1;
	len /= 2;

	*dest = xmalloc(len);

	ptr[2] = '\0';
	for (i = 0; i < len; i++) {
		ptr[0] = hex[2 * i];
		ptr[1] = hex[(2 * i) + 1];
		if (!isxdigit(ptr[0]) || !isxdigit(ptr[1]))
			return -1;
		(*dest)[i] = (unsigned char)strtoul(ptr, NULL, 16);
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
	rmspace(p->info.manufacturerID, sizeof(p->info.manufacturerID));
	rmspace(p->info.libraryDescription, sizeof(p->info.libraryDescription));
	debug("provider %s: manufacturerID <%s> cryptokiVersion %d.%d"
	    " libraryDescription <%s> libraryVersion %d.%d",
	    provider_id,
	    p->info.manufacturerID,
	    p->info.cryptokiVersion.major,
	    p->info.cryptokiVersion.minor,
	    p->info.libraryDescription,
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
		rmspace(token->label, sizeof(token->label));
		rmspace(token->manufacturerID, sizeof(token->manufacturerID));
		rmspace(token->model, sizeof(token->model));
		rmspace(token->serialNumber, sizeof(token->serialNumber));
		debug("provider %s slot %lu: label <%s> manufacturerID <%s> "
		    "model <%s> serial <%s> flags 0x%lx",
		    provider_id, (unsigned long)i,
		    token->label, token->manufacturerID, token->model,
		    token->serialNumber, token->flags);
		/*
		 * open session, login with pin and retrieve public
		 * keys (if keyp is provided)
		 */
		if ((ret = pkcs11_open_session(p, i, pin, user)) != 0 ||
		    keyp == NULL)
			continue;
		pkcs11_fetch_keys(p, i, keyp, labelsp, &nkeys);
		pkcs11_fetch_certs(p, i, keyp, labelsp, &nkeys);
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
			pkcs11_fetch_certs(p, i, keyp, labelsp, &nkeys);
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
		if (key_type == CKK_RSA)
			k = pkcs11_fetch_rsa_pubkey(p, slotidx, &obj);
		else if (key_type == CKK_ECDSA)
			k = pkcs11_fetch_ecdsa_pubkey(p, slotidx, &obj);

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

int
pkcs11_init(int interactive)
{
	error("%s: dlopen() not supported", __func__);
	return (-1);
}

int
pkcs11_add_provider(char *provider_id, char *pin, struct sshkey ***keyp,
    char ***labelsp)
{
	error("%s: dlopen() not supported", __func__);
	return (-1);
}

void
pkcs11_terminate(void)
{
	error("%s: dlopen() not supported", __func__);
}
#endif /* ENABLE_PKCS11 */
