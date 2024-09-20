/* $OpenBSD: ssh-sk.c,v 1.41 2024/08/15 00:51:51 djm Exp $ */
/*
 * Copyright (c) 2019 Google LLC
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

/* #define DEBUG_SK 1 */

#include "includes.h"

#ifdef ENABLE_SK

#include <dlfcn.h>
#include <stddef.h>
#ifdef HAVE_STDINT_H
# include <stdint.h>
#endif
#include <string.h>
#include <stdio.h>

#if defined(WITH_OPENSSL) && defined(OPENSSL_HAS_ECC)
#include <openssl/objects.h>
#include <openssl/ec.h>
#include <openssl/evp.h>
#endif /* WITH_OPENSSL && OPENSSL_HAS_ECC */

#include "log.h"
#include "misc.h"
#include "sshbuf.h"
#include "sshkey.h"
#include "ssherr.h"
#include "digest.h"

#include "ssh-sk.h"
#include "sk-api.h"
#include "crypto_api.h"

/*
 * Almost every use of OpenSSL in this file is for ECDSA-NISTP256.
 * This is strictly a larger hammer than necessary, but it reduces changes
 * with upstream.
 */
#ifndef OPENSSL_HAS_ECC
# undef WITH_OPENSSL
#endif

struct sshsk_provider {
	char *path;
	void *dlhandle;

	/* Return the version of the middleware API */
	uint32_t (*sk_api_version)(void);

	/* Enroll a U2F key (private key generation) */
	int (*sk_enroll)(int alg, const uint8_t *challenge,
	    size_t challenge_len, const char *application, uint8_t flags,
	    const char *pin, struct sk_option **opts,
	    struct sk_enroll_response **enroll_response);

	/* Sign a challenge */
	int (*sk_sign)(int alg, const uint8_t *message, size_t message_len,
	    const char *application,
	    const uint8_t *key_handle, size_t key_handle_len,
	    uint8_t flags, const char *pin, struct sk_option **opts,
	    struct sk_sign_response **sign_response);

	/* Enumerate resident keys */
	int (*sk_load_resident_keys)(const char *pin, struct sk_option **opts,
	    struct sk_resident_key ***rks, size_t *nrks);
};

/* Built-in version */
int ssh_sk_enroll(int alg, const uint8_t *challenge,
    size_t challenge_len, const char *application, uint8_t flags,
    const char *pin, struct sk_option **opts,
    struct sk_enroll_response **enroll_response);
int ssh_sk_sign(int alg, const uint8_t *message, size_t message_len,
    const char *application,
    const uint8_t *key_handle, size_t key_handle_len,
    uint8_t flags, const char *pin, struct sk_option **opts,
    struct sk_sign_response **sign_response);
int ssh_sk_load_resident_keys(const char *pin, struct sk_option **opts,
    struct sk_resident_key ***rks, size_t *nrks);

static void
sshsk_free(struct sshsk_provider *p)
{
	if (p == NULL)
		return;
	free(p->path);
	if (p->dlhandle != NULL)
		dlclose(p->dlhandle);
	free(p);
}

static struct sshsk_provider *
sshsk_open(const char *path)
{
	struct sshsk_provider *ret = NULL;
	uint32_t version;

	if (path == NULL || *path == '\0') {
		error("No FIDO SecurityKeyProvider specified");
		return NULL;
	}
	if ((ret = calloc(1, sizeof(*ret))) == NULL) {
		error_f("calloc failed");
		return NULL;
	}
	if ((ret->path = strdup(path)) == NULL) {
		error_f("strdup failed");
		goto fail;
	}
	/* Skip the rest if we're using the linked in middleware */
	if (strcasecmp(ret->path, "internal") == 0) {
#ifdef ENABLE_SK_INTERNAL
		ret->sk_enroll = ssh_sk_enroll;
		ret->sk_sign = ssh_sk_sign;
		ret->sk_load_resident_keys = ssh_sk_load_resident_keys;
		return ret;
#else
		error("internal security key support not enabled");
		goto fail;
#endif
	}
	if (lib_contains_symbol(path, "sk_api_version") != 0) {
		error("provider %s is not an OpenSSH FIDO library", path);
		goto fail;
	}
	if ((ret->dlhandle = dlopen(path, RTLD_NOW)) == NULL)
		fatal("Provider \"%s\" dlopen failed: %s", path, dlerror());
	if ((ret->sk_api_version = dlsym(ret->dlhandle,
	    "sk_api_version")) == NULL) {
		error("Provider \"%s\" dlsym(sk_api_version) failed: %s",
		    path, dlerror());
		goto fail;
	}
	version = ret->sk_api_version();
	debug_f("provider %s implements version 0x%08lx", ret->path,
	    (u_long)version);
	if ((version & SSH_SK_VERSION_MAJOR_MASK) != SSH_SK_VERSION_MAJOR) {
		error("Provider \"%s\" implements unsupported "
		    "version 0x%08lx (supported: 0x%08lx)",
		    path, (u_long)version, (u_long)SSH_SK_VERSION_MAJOR);
		goto fail;
	}
	if ((ret->sk_enroll = dlsym(ret->dlhandle, "sk_enroll")) == NULL) {
		error("Provider %s dlsym(sk_enroll) failed: %s",
		    path, dlerror());
		goto fail;
	}
	if ((ret->sk_sign = dlsym(ret->dlhandle, "sk_sign")) == NULL) {
		error("Provider \"%s\" dlsym(sk_sign) failed: %s",
		    path, dlerror());
		goto fail;
	}
	if ((ret->sk_load_resident_keys = dlsym(ret->dlhandle,
	    "sk_load_resident_keys")) == NULL) {
		error("Provider \"%s\" dlsym(sk_load_resident_keys) "
		    "failed: %s", path, dlerror());
		goto fail;
	}
	/* success */
	return ret;
fail:
	sshsk_free(ret);
	return NULL;
}

static void
sshsk_free_enroll_response(struct sk_enroll_response *r)
{
	if (r == NULL)
		return;
	freezero(r->key_handle, r->key_handle_len);
	freezero(r->public_key, r->public_key_len);
	freezero(r->signature, r->signature_len);
	freezero(r->attestation_cert, r->attestation_cert_len);
	freezero(r->authdata, r->authdata_len);
	freezero(r, sizeof(*r));
}

static void
sshsk_free_sign_response(struct sk_sign_response *r)
{
	if (r == NULL)
		return;
	freezero(r->sig_r, r->sig_r_len);
	freezero(r->sig_s, r->sig_s_len);
	freezero(r, sizeof(*r));
}

#ifdef WITH_OPENSSL
/* Assemble key from response */
static int
sshsk_ecdsa_assemble(struct sk_enroll_response *resp, struct sshkey **keyp)
{
	struct sshkey *key = NULL;
	struct sshbuf *b = NULL;
	EC_KEY *ecdsa = NULL;
	EC_POINT *q = NULL;
	const EC_GROUP *g = NULL;
	int r;

	*keyp = NULL;
	if ((key = sshkey_new(KEY_ECDSA_SK)) == NULL) {
		error_f("sshkey_new failed");
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	key->ecdsa_nid = NID_X9_62_prime256v1;
	if ((ecdsa = EC_KEY_new_by_curve_name(key->ecdsa_nid)) == NULL ||
	    (g = EC_KEY_get0_group(ecdsa)) == NULL ||
	    (q = EC_POINT_new(g)) == NULL ||
	    (b = sshbuf_new()) == NULL) {
		error_f("allocation failed");
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	if ((r = sshbuf_put_string(b,
	    resp->public_key, resp->public_key_len)) != 0) {
		error_fr(r, "sshbuf_put_string");
		goto out;
	}
	if ((r = sshbuf_get_ec(b, q, g)) != 0) {
		error_fr(r, "parse");
		r = SSH_ERR_INVALID_FORMAT;
		goto out;
	}
	if (sshkey_ec_validate_public(g, q) != 0) {
		error("Authenticator returned invalid ECDSA key");
		r = SSH_ERR_KEY_INVALID_EC_VALUE;
		goto out;
	}
	if (EC_KEY_set_public_key(ecdsa, q) != 1) {
		/* XXX assume it is a allocation error */
		error_f("allocation failed");
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	if ((key->pkey = EVP_PKEY_new()) == NULL) {
		error_f("allocation failed");
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	if (EVP_PKEY_set1_EC_KEY(key->pkey, ecdsa) != 1) {
		error_f("Assigning EC_KEY failed");
		r = SSH_ERR_LIBCRYPTO_ERROR;
		goto out;
	}
	/* success */
	*keyp = key;
	key = NULL; /* transferred */
	r = 0;
 out:
	sshkey_free(key);
	sshbuf_free(b);
	EC_KEY_free(ecdsa);
	EC_POINT_free(q);
	return r;
}
#endif /* WITH_OPENSSL */

static int
sshsk_ed25519_assemble(struct sk_enroll_response *resp, struct sshkey **keyp)
{
	struct sshkey *key = NULL;
	int r;

	*keyp = NULL;
	if (resp->public_key_len != ED25519_PK_SZ) {
		error_f("invalid size: %zu", resp->public_key_len);
		r = SSH_ERR_INVALID_FORMAT;
		goto out;
	}
	if ((key = sshkey_new(KEY_ED25519_SK)) == NULL) {
		error_f("sshkey_new failed");
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	if ((key->ed25519_pk = malloc(ED25519_PK_SZ)) == NULL) {
		error_f("malloc failed");
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	memcpy(key->ed25519_pk, resp->public_key, ED25519_PK_SZ);
	/* success */
	*keyp = key;
	key = NULL; /* transferred */
	r = 0;
 out:
	sshkey_free(key);
	return r;
}

static int
sshsk_key_from_response(int alg, const char *application, uint8_t flags,
    struct sk_enroll_response *resp, struct sshkey **keyp)
{
	struct sshkey *key = NULL;
	int r = SSH_ERR_INTERNAL_ERROR;

	*keyp = NULL;

	/* Check response validity */
	if (resp->public_key == NULL || resp->key_handle == NULL) {
		error_f("sk_enroll response invalid");
		r = SSH_ERR_INVALID_FORMAT;
		goto out;
	}
	switch (alg) {
#ifdef WITH_OPENSSL
	case SSH_SK_ECDSA:
		if ((r = sshsk_ecdsa_assemble(resp, &key)) != 0)
			goto out;
		break;
#endif /* WITH_OPENSSL */
	case SSH_SK_ED25519:
		if ((r = sshsk_ed25519_assemble(resp, &key)) != 0)
			goto out;
		break;
	default:
		error_f("unsupported algorithm %d", alg);
		r = SSH_ERR_INVALID_ARGUMENT;
		goto out;
	}
	key->sk_flags = flags;
	if ((key->sk_key_handle = sshbuf_new()) == NULL ||
	    (key->sk_reserved = sshbuf_new()) == NULL) {
		error_f("allocation failed");
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	if ((key->sk_application = strdup(application)) == NULL) {
		error_f("strdup application failed");
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	if ((r = sshbuf_put(key->sk_key_handle, resp->key_handle,
	    resp->key_handle_len)) != 0) {
		error_fr(r, "put key handle");
		goto out;
	}
	/* success */
	r = 0;
	*keyp = key;
	key = NULL;
 out:
	sshkey_free(key);
	return r;
}

static int
skerr_to_ssherr(int skerr)
{
	switch (skerr) {
	case SSH_SK_ERR_UNSUPPORTED:
		return SSH_ERR_FEATURE_UNSUPPORTED;
	case SSH_SK_ERR_PIN_REQUIRED:
		return SSH_ERR_KEY_WRONG_PASSPHRASE;
	case SSH_SK_ERR_DEVICE_NOT_FOUND:
		return SSH_ERR_DEVICE_NOT_FOUND;
	case SSH_SK_ERR_CREDENTIAL_EXISTS:
		return SSH_ERR_KEY_BAD_PERMISSIONS;
	case SSH_SK_ERR_GENERAL:
	default:
		return SSH_ERR_INVALID_FORMAT;
	}
}

static void
sshsk_free_options(struct sk_option **opts)
{
	size_t i;

	if (opts == NULL)
		return;
	for (i = 0; opts[i] != NULL; i++) {
		free(opts[i]->name);
		free(opts[i]->value);
		free(opts[i]);
	}
	free(opts);
}

static int
sshsk_add_option(struct sk_option ***optsp, size_t *noptsp,
    const char *name, const char *value, uint8_t required)
{
	struct sk_option **opts = *optsp;
	size_t nopts = *noptsp;

	if ((opts = recallocarray(opts, nopts, nopts + 2, /* extra for NULL */
	    sizeof(*opts))) == NULL) {
		error_f("array alloc failed");
		return SSH_ERR_ALLOC_FAIL;
	}
	*optsp = opts;
	*noptsp = nopts + 1;
	if ((opts[nopts] = calloc(1, sizeof(**opts))) == NULL) {
		error_f("alloc failed");
		return SSH_ERR_ALLOC_FAIL;
	}
	if ((opts[nopts]->name = strdup(name)) == NULL ||
	    (opts[nopts]->value = strdup(value)) == NULL) {
		error_f("alloc failed");
		return SSH_ERR_ALLOC_FAIL;
	}
	opts[nopts]->required = required;
	return 0;
}

static int
make_options(const char *device, const char *user_id,
    struct sk_option ***optsp)
{
	struct sk_option **opts = NULL;
	size_t nopts = 0;
	int r, ret = SSH_ERR_INTERNAL_ERROR;

	if (device != NULL &&
	    (r = sshsk_add_option(&opts, &nopts, "device", device, 0)) != 0) {
		ret = r;
		goto out;
	}
	if (user_id != NULL &&
	    (r = sshsk_add_option(&opts, &nopts, "user", user_id, 0)) != 0) {
		ret = r;
		goto out;
	}
	/* success */
	*optsp = opts;
	opts = NULL;
	nopts = 0;
	ret = 0;
 out:
	sshsk_free_options(opts);
	return ret;
}


static int
fill_attestation_blob(const struct sk_enroll_response *resp,
    struct sshbuf *attest)
{
	int r;

	if (attest == NULL)
		return 0; /* nothing to do */
	if ((r = sshbuf_put_cstring(attest, "ssh-sk-attest-v01")) != 0 ||
	    (r = sshbuf_put_string(attest,
	    resp->attestation_cert, resp->attestation_cert_len)) != 0 ||
	    (r = sshbuf_put_string(attest,
	    resp->signature, resp->signature_len)) != 0 ||
	    (r = sshbuf_put_string(attest,
	    resp->authdata, resp->authdata_len)) != 0 ||
	    (r = sshbuf_put_u32(attest, 0)) != 0 || /* resvd flags */
	    (r = sshbuf_put_string(attest, NULL, 0)) != 0 /* resvd */) {
		error_fr(r, "compose");
		return r;
	}
	/* success */
	return 0;
}

int
sshsk_enroll(int type, const char *provider_path, const char *device,
    const char *application, const char *userid, uint8_t flags,
    const char *pin, struct sshbuf *challenge_buf,
    struct sshkey **keyp, struct sshbuf *attest)
{
	struct sshsk_provider *skp = NULL;
	struct sshkey *key = NULL;
	u_char randchall[32];
	const u_char *challenge;
	size_t challenge_len;
	struct sk_enroll_response *resp = NULL;
	struct sk_option **opts = NULL;
	int r = SSH_ERR_INTERNAL_ERROR;
	int alg;

	debug_f("provider \"%s\", device \"%s\", application \"%s\", "
	    "userid \"%s\", flags 0x%02x, challenge len %zu%s",
	    provider_path, device, application, userid, flags,
	    challenge_buf == NULL ? 0 : sshbuf_len(challenge_buf),
	    (pin != NULL && *pin != '\0') ? " with-pin" : "");

	*keyp = NULL;
	if (attest)
		sshbuf_reset(attest);

	if ((r = make_options(device, userid, &opts)) != 0)
		goto out;

	switch (type) {
#ifdef WITH_OPENSSL
	case KEY_ECDSA_SK:
		alg = SSH_SK_ECDSA;
		break;
#endif /* WITH_OPENSSL */
	case KEY_ED25519_SK:
		alg = SSH_SK_ED25519;
		break;
	default:
		error_f("unsupported key type");
		r = SSH_ERR_INVALID_ARGUMENT;
		goto out;
	}
	if (provider_path == NULL) {
		error_f("missing provider");
		r = SSH_ERR_INVALID_ARGUMENT;
		goto out;
	}
	if (application == NULL || *application == '\0') {
		error_f("missing application");
		r = SSH_ERR_INVALID_ARGUMENT;
		goto out;
	}
	if (challenge_buf == NULL) {
		debug_f("using random challenge");
		arc4random_buf(randchall, sizeof(randchall));
		challenge = randchall;
		challenge_len = sizeof(randchall);
	} else if (sshbuf_len(challenge_buf) == 0) {
		error("Missing enrollment challenge");
		r = SSH_ERR_INVALID_ARGUMENT;
		goto out;
	} else {
		challenge = sshbuf_ptr(challenge_buf);
		challenge_len = sshbuf_len(challenge_buf);
		debug3_f("using explicit challenge len=%zd", challenge_len);
	}
	if ((skp = sshsk_open(provider_path)) == NULL) {
		r = SSH_ERR_INVALID_FORMAT; /* XXX sshsk_open return code? */
		goto out;
	}
	/* XXX validate flags? */
	/* enroll key */
	if ((r = skp->sk_enroll(alg, challenge, challenge_len, application,
	    flags, pin, opts, &resp)) != 0) {
		debug_f("provider \"%s\" failure %d", provider_path, r);
		r = skerr_to_ssherr(r);
		goto out;
	}

	if ((r = sshsk_key_from_response(alg, application, resp->flags,
	    resp, &key)) != 0)
		goto out;

	/* Optionally fill in the attestation information */
	if ((r = fill_attestation_blob(resp, attest)) != 0)
		goto out;

	/* success */
	*keyp = key;
	key = NULL; /* transferred */
	r = 0;
 out:
	sshsk_free_options(opts);
	sshsk_free(skp);
	sshkey_free(key);
	sshsk_free_enroll_response(resp);
	explicit_bzero(randchall, sizeof(randchall));
	return r;
}

#ifdef WITH_OPENSSL
static int
sshsk_ecdsa_sig(struct sk_sign_response *resp, struct sshbuf *sig)
{
	struct sshbuf *inner_sig = NULL;
	int r = SSH_ERR_INTERNAL_ERROR;

	/* Check response validity */
	if (resp->sig_r == NULL || resp->sig_s == NULL) {
		error_f("sk_sign response invalid");
		r = SSH_ERR_INVALID_FORMAT;
		goto out;
	}
	if ((inner_sig = sshbuf_new()) == NULL) {
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	/* Prepare and append inner signature object */
	if ((r = sshbuf_put_bignum2_bytes(inner_sig,
	    resp->sig_r, resp->sig_r_len)) != 0 ||
	    (r = sshbuf_put_bignum2_bytes(inner_sig,
	    resp->sig_s, resp->sig_s_len)) != 0) {
		error_fr(r, "compose inner");
		goto out;
	}
	if ((r = sshbuf_put_stringb(sig, inner_sig)) != 0 ||
	    (r = sshbuf_put_u8(sig, resp->flags)) != 0 ||
	    (r = sshbuf_put_u32(sig, resp->counter)) != 0) {
		error_fr(r, "compose");
		goto out;
	}
#ifdef DEBUG_SK
	fprintf(stderr, "%s: sig_r:\n", __func__);
	sshbuf_dump_data(resp->sig_r, resp->sig_r_len, stderr);
	fprintf(stderr, "%s: sig_s:\n", __func__);
	sshbuf_dump_data(resp->sig_s, resp->sig_s_len, stderr);
	fprintf(stderr, "%s: inner:\n", __func__);
	sshbuf_dump(inner_sig, stderr);
#endif
	r = 0;
 out:
	sshbuf_free(inner_sig);
	return r;
}
#endif /* WITH_OPENSSL */

static int
sshsk_ed25519_sig(struct sk_sign_response *resp, struct sshbuf *sig)
{
	int r = SSH_ERR_INTERNAL_ERROR;

	/* Check response validity */
	if (resp->sig_r == NULL) {
		error_f("sk_sign response invalid");
		r = SSH_ERR_INVALID_FORMAT;
		goto out;
	}
	if ((r = sshbuf_put_string(sig,
	    resp->sig_r, resp->sig_r_len)) != 0 ||
	    (r = sshbuf_put_u8(sig, resp->flags)) != 0 ||
	    (r = sshbuf_put_u32(sig, resp->counter)) != 0) {
		error_fr(r, "compose");
		goto out;
	}
#ifdef DEBUG_SK
	fprintf(stderr, "%s: sig_r:\n", __func__);
	sshbuf_dump_data(resp->sig_r, resp->sig_r_len, stderr);
#endif
	r = 0;
 out:
	return r;
}

int
sshsk_sign(const char *provider_path, struct sshkey *key,
    u_char **sigp, size_t *lenp, const u_char *data, size_t datalen,
    u_int compat, const char *pin)
{
	struct sshsk_provider *skp = NULL;
	int r = SSH_ERR_INTERNAL_ERROR;
	int type, alg;
	struct sk_sign_response *resp = NULL;
	struct sshbuf *inner_sig = NULL, *sig = NULL;
	struct sk_option **opts = NULL;

	debug_f("provider \"%s\", key %s, flags 0x%02x%s",
	    provider_path, sshkey_type(key), key->sk_flags,
	    (pin != NULL && *pin != '\0') ? " with-pin" : "");

	if (sigp != NULL)
		*sigp = NULL;
	if (lenp != NULL)
		*lenp = 0;
	type = sshkey_type_plain(key->type);
	switch (type) {
#ifdef WITH_OPENSSL
	case KEY_ECDSA_SK:
		alg = SSH_SK_ECDSA;
		break;
#endif /* WITH_OPENSSL */
	case KEY_ED25519_SK:
		alg = SSH_SK_ED25519;
		break;
	default:
		return SSH_ERR_INVALID_ARGUMENT;
	}
	if (provider_path == NULL ||
	    key->sk_key_handle == NULL ||
	    key->sk_application == NULL || *key->sk_application == '\0') {
		r = SSH_ERR_INVALID_ARGUMENT;
		goto out;
	}
	if ((skp = sshsk_open(provider_path)) == NULL) {
		r = SSH_ERR_INVALID_FORMAT; /* XXX sshsk_open return code? */
		goto out;
	}
#ifdef DEBUG_SK
	fprintf(stderr, "%s: sk_flags = 0x%02x, sk_application = \"%s\"\n",
	    __func__, key->sk_flags, key->sk_application);
	fprintf(stderr, "%s: sk_key_handle:\n", __func__);
	sshbuf_dump(key->sk_key_handle, stderr);
#endif
	if ((r = skp->sk_sign(alg, data, datalen, key->sk_application,
	    sshbuf_ptr(key->sk_key_handle), sshbuf_len(key->sk_key_handle),
	    key->sk_flags, pin, opts, &resp)) != 0) {
		debug_f("sk_sign failed with code %d", r);
		r = skerr_to_ssherr(r);
		goto out;
	}
	/* Assemble signature */
	if ((sig = sshbuf_new()) == NULL) {
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	if ((r = sshbuf_put_cstring(sig, sshkey_ssh_name_plain(key))) != 0) {
		error_fr(r, "compose outer");
		goto out;
	}
	switch (type) {
#ifdef WITH_OPENSSL
	case KEY_ECDSA_SK:
		if ((r = sshsk_ecdsa_sig(resp, sig)) != 0)
			goto out;
		break;
#endif /* WITH_OPENSSL */
	case KEY_ED25519_SK:
		if ((r = sshsk_ed25519_sig(resp, sig)) != 0)
			goto out;
		break;
	}
#ifdef DEBUG_SK
	fprintf(stderr, "%s: sig_flags = 0x%02x, sig_counter = %u\n",
	    __func__, resp->flags, resp->counter);
	fprintf(stderr, "%s: data to sign:\n", __func__);
	sshbuf_dump_data(data, datalen, stderr);
	fprintf(stderr, "%s: sigbuf:\n", __func__);
	sshbuf_dump(sig, stderr);
#endif
	if (sigp != NULL) {
		if ((*sigp = malloc(sshbuf_len(sig))) == NULL) {
			r = SSH_ERR_ALLOC_FAIL;
			goto out;
		}
		memcpy(*sigp, sshbuf_ptr(sig), sshbuf_len(sig));
	}
	if (lenp != NULL)
		*lenp = sshbuf_len(sig);
	/* success */
	r = 0;
 out:
	sshsk_free_options(opts);
	sshsk_free(skp);
	sshsk_free_sign_response(resp);
	sshbuf_free(sig);
	sshbuf_free(inner_sig);
	return r;
}

static void
sshsk_free_sk_resident_keys(struct sk_resident_key **rks, size_t nrks)
{
	size_t i;

	if (nrks == 0 || rks == NULL)
		return;
	for (i = 0; i < nrks; i++) {
		free(rks[i]->application);
		freezero(rks[i]->user_id, rks[i]->user_id_len);
		freezero(rks[i]->key.key_handle, rks[i]->key.key_handle_len);
		freezero(rks[i]->key.public_key, rks[i]->key.public_key_len);
		freezero(rks[i]->key.signature, rks[i]->key.signature_len);
		freezero(rks[i]->key.attestation_cert,
		    rks[i]->key.attestation_cert_len);
		freezero(rks[i], sizeof(**rks));
	}
	free(rks);
}

static void
sshsk_free_resident_key(struct sshsk_resident_key *srk)
{
	if (srk == NULL)
		return;
	sshkey_free(srk->key);
	freezero(srk->user_id, srk->user_id_len);
	free(srk);
}


void
sshsk_free_resident_keys(struct sshsk_resident_key **srks, size_t nsrks)
{
	size_t i;

	if (srks == NULL || nsrks == 0)
		return;

	for (i = 0; i < nsrks; i++)
		sshsk_free_resident_key(srks[i]);
	free(srks);
}

int
sshsk_load_resident(const char *provider_path, const char *device,
    const char *pin, u_int flags, struct sshsk_resident_key ***srksp,
    size_t *nsrksp)
{
	struct sshsk_provider *skp = NULL;
	int r = SSH_ERR_INTERNAL_ERROR;
	struct sk_resident_key **rks = NULL;
	size_t i, nrks = 0, nsrks = 0;
	struct sshkey *key = NULL;
	struct sshsk_resident_key *srk = NULL, **srks = NULL, **tmp;
	uint8_t sk_flags;
	struct sk_option **opts = NULL;

	debug_f("provider \"%s\"%s", provider_path,
	    (pin != NULL && *pin != '\0') ? ", have-pin": "");

	if (srksp == NULL || nsrksp == NULL)
		return SSH_ERR_INVALID_ARGUMENT;
	*srksp = NULL;
	*nsrksp = 0;

	if ((r = make_options(device, NULL, &opts)) != 0)
		goto out;
	if ((skp = sshsk_open(provider_path)) == NULL) {
		r = SSH_ERR_INVALID_FORMAT; /* XXX sshsk_open return code? */
		goto out;
	}
	if ((r = skp->sk_load_resident_keys(pin, opts, &rks, &nrks)) != 0) {
		error("Provider \"%s\" returned failure %d", provider_path, r);
		r = skerr_to_ssherr(r);
		goto out;
	}
	for (i = 0; i < nrks; i++) {
		debug3_f("rk %zu: slot %zu, alg %d, app \"%s\", uidlen %zu",
		    i, rks[i]->slot, rks[i]->alg, rks[i]->application,
		    rks[i]->user_id_len);
		/* XXX need better filter here */
		if (strncmp(rks[i]->application, "ssh:", 4) != 0)
			continue;
		switch (rks[i]->alg) {
		case SSH_SK_ECDSA:
		case SSH_SK_ED25519:
			break;
		default:
			continue;
		}
		sk_flags = SSH_SK_USER_PRESENCE_REQD|SSH_SK_RESIDENT_KEY;
		if ((rks[i]->flags & SSH_SK_USER_VERIFICATION_REQD))
			sk_flags |= SSH_SK_USER_VERIFICATION_REQD;
		if ((r = sshsk_key_from_response(rks[i]->alg,
		    rks[i]->application, sk_flags, &rks[i]->key, &key)) != 0)
			goto out;
		if ((srk = calloc(1, sizeof(*srk))) == NULL) {
			error_f("calloc failed");
			r = SSH_ERR_ALLOC_FAIL;
			goto out;
		}
		srk->key = key;
		key = NULL; /* transferred */
		if ((srk->user_id = calloc(1, rks[i]->user_id_len)) == NULL) {
			error_f("calloc failed");
			r = SSH_ERR_ALLOC_FAIL;
			goto out;
		}
		memcpy(srk->user_id, rks[i]->user_id, rks[i]->user_id_len);
		srk->user_id_len = rks[i]->user_id_len;
		if ((tmp = recallocarray(srks, nsrks, nsrks + 1,
		    sizeof(*tmp))) == NULL) {
			error_f("recallocarray failed");
			r = SSH_ERR_ALLOC_FAIL;
			goto out;
		}
		srks = tmp;
		srks[nsrks++] = srk;
		srk = NULL;
		/* XXX synthesise comment */
	}
	/* success */
	*srksp = srks;
	*nsrksp = nsrks;
	srks = NULL;
	nsrks = 0;
	r = 0;
 out:
	sshsk_free_options(opts);
	sshsk_free(skp);
	sshsk_free_sk_resident_keys(rks, nrks);
	sshkey_free(key);
	sshsk_free_resident_key(srk);
	sshsk_free_resident_keys(srks, nsrks);
	return r;
}

#endif /* ENABLE_SK */
