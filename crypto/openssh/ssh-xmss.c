/* $OpenBSD: ssh-xmss.c,v 1.14 2022/10/28 00:44:44 djm Exp $*/
/*
 * Copyright (c) 2017 Stefan-Lukas Gazdag.
 * Copyright (c) 2017 Markus Friedl.
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
#ifdef WITH_XMSS

#define SSHKEY_INTERNAL
#include <sys/types.h>
#include <limits.h>

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#ifdef HAVE_STDINT_H
# include <stdint.h>
#endif
#include <unistd.h>

#include "log.h"
#include "sshbuf.h"
#include "sshkey.h"
#include "sshkey-xmss.h"
#include "ssherr.h"
#include "ssh.h"

#include "xmss_fast.h"

static void
ssh_xmss_cleanup(struct sshkey *k)
{
	freezero(k->xmss_pk, sshkey_xmss_pklen(k));
	freezero(k->xmss_sk, sshkey_xmss_sklen(k));
	sshkey_xmss_free_state(k);
	free(k->xmss_name);
	free(k->xmss_filename);
	k->xmss_pk = NULL;
	k->xmss_sk = NULL;
	k->xmss_name = NULL;
	k->xmss_filename = NULL;
}

static int
ssh_xmss_equal(const struct sshkey *a, const struct sshkey *b)
{
	if (a->xmss_pk == NULL || b->xmss_pk == NULL)
		return 0;
	if (sshkey_xmss_pklen(a) != sshkey_xmss_pklen(b))
		return 0;
	if (memcmp(a->xmss_pk, b->xmss_pk, sshkey_xmss_pklen(a)) != 0)
		return 0;
	return 1;
}

static int
ssh_xmss_serialize_public(const struct sshkey *key, struct sshbuf *b,
    enum sshkey_serialize_rep opts)
{
	int r;

	if (key->xmss_name == NULL || key->xmss_pk == NULL ||
	    sshkey_xmss_pklen(key) == 0)
		return SSH_ERR_INVALID_ARGUMENT;
	if ((r = sshbuf_put_cstring(b, key->xmss_name)) != 0 ||
	    (r = sshbuf_put_string(b, key->xmss_pk,
	    sshkey_xmss_pklen(key))) != 0 ||
	    (r = sshkey_xmss_serialize_pk_info(key, b, opts)) != 0)
		return r;

	return 0;
}

static int
ssh_xmss_serialize_private(const struct sshkey *key, struct sshbuf *b,
    enum sshkey_serialize_rep opts)
{
	int r;

	if (key->xmss_name == NULL)
		return SSH_ERR_INVALID_ARGUMENT;
	/* Note: can't reuse ssh_xmss_serialize_public because of sk order */
	if ((r = sshbuf_put_cstring(b, key->xmss_name)) != 0 ||
	    (r = sshbuf_put_string(b, key->xmss_pk,
	    sshkey_xmss_pklen(key))) != 0 ||
	    (r = sshbuf_put_string(b, key->xmss_sk,
	    sshkey_xmss_sklen(key))) != 0 ||
	    (r = sshkey_xmss_serialize_state_opt(key, b, opts)) != 0)
		return r;

	return 0;
}

static int
ssh_xmss_copy_public(const struct sshkey *from, struct sshkey *to)
{
	int r = SSH_ERR_INTERNAL_ERROR;
	u_int32_t left;
	size_t pklen;

	if ((r = sshkey_xmss_init(to, from->xmss_name)) != 0)
		return r;
	if (from->xmss_pk == NULL)
		return 0; /* XXX SSH_ERR_INTERNAL_ERROR ? */

	if ((pklen = sshkey_xmss_pklen(from)) == 0 ||
	    sshkey_xmss_pklen(to) != pklen)
		return SSH_ERR_INTERNAL_ERROR;
	if ((to->xmss_pk = malloc(pklen)) == NULL)
		return SSH_ERR_ALLOC_FAIL;
	memcpy(to->xmss_pk, from->xmss_pk, pklen);
	/* simulate number of signatures left on pubkey */
	left = sshkey_xmss_signatures_left(from);
	if (left)
		sshkey_xmss_enable_maxsign(to, left);
	return 0;
}

static int
ssh_xmss_deserialize_public(const char *ktype, struct sshbuf *b,
    struct sshkey *key)
{
	size_t len = 0;
	char *xmss_name = NULL;
	u_char *pk = NULL;
	int ret = SSH_ERR_INTERNAL_ERROR;

	if ((ret = sshbuf_get_cstring(b, &xmss_name, NULL)) != 0)
		goto out;
	if ((ret = sshkey_xmss_init(key, xmss_name)) != 0)
		goto out;
	if ((ret = sshbuf_get_string(b, &pk, &len)) != 0)
		goto out;
	if (len == 0 || len != sshkey_xmss_pklen(key)) {
		ret = SSH_ERR_INVALID_FORMAT;
		goto out;
	}
	key->xmss_pk = pk;
	pk = NULL;
	if (!sshkey_is_cert(key) &&
	    (ret = sshkey_xmss_deserialize_pk_info(key, b)) != 0)
		goto out;
	/* success */
	ret = 0;
 out:
	free(xmss_name);
	freezero(pk, len);
	return ret;
}

static int
ssh_xmss_deserialize_private(const char *ktype, struct sshbuf *b,
    struct sshkey *key)
{
	int r;
	char *xmss_name = NULL;
	size_t pklen = 0, sklen = 0;
	u_char *xmss_pk = NULL, *xmss_sk = NULL;

	/* Note: can't reuse ssh_xmss_deserialize_public because of sk order */
	if ((r = sshbuf_get_cstring(b, &xmss_name, NULL)) != 0 ||
	    (r = sshbuf_get_string(b, &xmss_pk, &pklen)) != 0 ||
	    (r = sshbuf_get_string(b, &xmss_sk, &sklen)) != 0)
		goto out;
	if (!sshkey_is_cert(key) &&
	    (r = sshkey_xmss_init(key, xmss_name)) != 0)
		goto out;
	if (pklen != sshkey_xmss_pklen(key) ||
	    sklen != sshkey_xmss_sklen(key)) {
		r = SSH_ERR_INVALID_FORMAT;
		goto out;
	}
	key->xmss_pk = xmss_pk;
	key->xmss_sk = xmss_sk;
	xmss_pk = xmss_sk = NULL;
	/* optional internal state */
	if ((r = sshkey_xmss_deserialize_state_opt(key, b)) != 0)
		goto out;
	/* success */
	r = 0;
 out:
	free(xmss_name);
	freezero(xmss_pk, pklen);
	freezero(xmss_sk, sklen);
	return r;
}

static int
ssh_xmss_sign(struct sshkey *key,
    u_char **sigp, size_t *lenp,
    const u_char *data, size_t datalen,
    const char *alg, const char *sk_provider, const char *sk_pin, u_int compat)
{
	u_char *sig = NULL;
	size_t slen = 0, len = 0, required_siglen;
	unsigned long long smlen;
	int r, ret;
	struct sshbuf *b = NULL;

	if (lenp != NULL)
		*lenp = 0;
	if (sigp != NULL)
		*sigp = NULL;

	if (key == NULL ||
	    sshkey_type_plain(key->type) != KEY_XMSS ||
	    key->xmss_sk == NULL ||
	    sshkey_xmss_params(key) == NULL)
		return SSH_ERR_INVALID_ARGUMENT;
	if ((r = sshkey_xmss_siglen(key, &required_siglen)) != 0)
		return r;
	if (datalen >= INT_MAX - required_siglen)
		return SSH_ERR_INVALID_ARGUMENT;
	smlen = slen = datalen + required_siglen;
	if ((sig = malloc(slen)) == NULL)
		return SSH_ERR_ALLOC_FAIL;
	if ((r = sshkey_xmss_get_state(key, 1)) != 0)
		goto out;
	if ((ret = xmss_sign(key->xmss_sk, sshkey_xmss_bds_state(key), sig, &smlen,
	    data, datalen, sshkey_xmss_params(key))) != 0 || smlen <= datalen) {
		r = SSH_ERR_INVALID_ARGUMENT; /* XXX better error? */
		goto out;
	}
	/* encode signature */
	if ((b = sshbuf_new()) == NULL) {
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	if ((r = sshbuf_put_cstring(b, "ssh-xmss@openssh.com")) != 0 ||
	    (r = sshbuf_put_string(b, sig, smlen - datalen)) != 0)
		goto out;
	len = sshbuf_len(b);
	if (sigp != NULL) {
		if ((*sigp = malloc(len)) == NULL) {
			r = SSH_ERR_ALLOC_FAIL;
			goto out;
		}
		memcpy(*sigp, sshbuf_ptr(b), len);
	}
	if (lenp != NULL)
		*lenp = len;
	/* success */
	r = 0;
 out:
	if ((ret = sshkey_xmss_update_state(key, 1)) != 0) {
		/* discard signature since we cannot update the state */
		if (r == 0 && sigp != NULL && *sigp != NULL) {
			explicit_bzero(*sigp, len);
			free(*sigp);
		}
		if (sigp != NULL)
			*sigp = NULL;
		if (lenp != NULL)
			*lenp = 0;
		r = ret;
	}
	sshbuf_free(b);
	if (sig != NULL)
		freezero(sig, slen);

	return r;
}

static int
ssh_xmss_verify(const struct sshkey *key,
    const u_char *sig, size_t siglen,
    const u_char *data, size_t dlen, const char *alg, u_int compat,
    struct sshkey_sig_details **detailsp)
{
	struct sshbuf *b = NULL;
	char *ktype = NULL;
	const u_char *sigblob;
	u_char *sm = NULL, *m = NULL;
	size_t len, required_siglen;
	unsigned long long smlen = 0, mlen = 0;
	int r, ret;

	if (key == NULL ||
	    sshkey_type_plain(key->type) != KEY_XMSS ||
	    key->xmss_pk == NULL ||
	    sshkey_xmss_params(key) == NULL ||
	    sig == NULL || siglen == 0)
		return SSH_ERR_INVALID_ARGUMENT;
	if ((r = sshkey_xmss_siglen(key, &required_siglen)) != 0)
		return r;
	if (dlen >= INT_MAX - required_siglen)
		return SSH_ERR_INVALID_ARGUMENT;

	if ((b = sshbuf_from(sig, siglen)) == NULL)
		return SSH_ERR_ALLOC_FAIL;
	if ((r = sshbuf_get_cstring(b, &ktype, NULL)) != 0 ||
	    (r = sshbuf_get_string_direct(b, &sigblob, &len)) != 0)
		goto out;
	if (strcmp("ssh-xmss@openssh.com", ktype) != 0) {
		r = SSH_ERR_KEY_TYPE_MISMATCH;
		goto out;
	}
	if (sshbuf_len(b) != 0) {
		r = SSH_ERR_UNEXPECTED_TRAILING_DATA;
		goto out;
	}
	if (len != required_siglen) {
		r = SSH_ERR_INVALID_FORMAT;
		goto out;
	}
	if (dlen >= SIZE_MAX - len) {
		r = SSH_ERR_INVALID_ARGUMENT;
		goto out;
	}
	smlen = len + dlen;
	mlen = smlen;
	if ((sm = malloc(smlen)) == NULL || (m = malloc(mlen)) == NULL) {
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	memcpy(sm, sigblob, len);
	memcpy(sm+len, data, dlen);
	if ((ret = xmss_sign_open(m, &mlen, sm, smlen,
	    key->xmss_pk, sshkey_xmss_params(key))) != 0) {
		debug2_f("xmss_sign_open failed: %d", ret);
	}
	if (ret != 0 || mlen != dlen) {
		r = SSH_ERR_SIGNATURE_INVALID;
		goto out;
	}
	/* XXX compare 'm' and 'data' ? */
	/* success */
	r = 0;
 out:
	if (sm != NULL)
		freezero(sm, smlen);
	if (m != NULL)
		freezero(m, smlen);
	sshbuf_free(b);
	free(ktype);
	return r;
}

static const struct sshkey_impl_funcs sshkey_xmss_funcs = {
	/* .size = */		NULL,
	/* .alloc = */		NULL,
	/* .cleanup = */	ssh_xmss_cleanup,
	/* .equal = */		ssh_xmss_equal,
	/* .ssh_serialize_public = */ ssh_xmss_serialize_public,
	/* .ssh_deserialize_public = */ ssh_xmss_deserialize_public,
	/* .ssh_serialize_private = */ ssh_xmss_serialize_private,
	/* .ssh_deserialize_private = */ ssh_xmss_deserialize_private,
	/* .generate = */	sshkey_xmss_generate_private_key,
	/* .copy_public = */	ssh_xmss_copy_public,
	/* .sign = */		ssh_xmss_sign,
	/* .verify = */		ssh_xmss_verify,
};

const struct sshkey_impl sshkey_xmss_impl = {
	/* .name = */		"ssh-xmss@openssh.com",
	/* .shortname = */	"XMSS",
	/* .sigalg = */		NULL,
	/* .type = */		KEY_XMSS,
	/* .nid = */		0,
	/* .cert = */		0,
	/* .sigonly = */	0,
	/* .keybits = */	256,
	/* .funcs = */		&sshkey_xmss_funcs,
};

const struct sshkey_impl sshkey_xmss_cert_impl = {
	/* .name = */		"ssh-xmss-cert-v01@openssh.com",
	/* .shortname = */	"XMSS-CERT",
	/* .sigalg = */		NULL,
	/* .type = */		KEY_XMSS_CERT,
	/* .nid = */		0,
	/* .cert = */		1,
	/* .sigonly = */	0,
	/* .keybits = */	256,
	/* .funcs = */		&sshkey_xmss_funcs,
};
#endif /* WITH_XMSS */
