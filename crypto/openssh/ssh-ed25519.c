/* $OpenBSD: ssh-ed25519.c,v 1.3 2014/02/23 20:03:42 djm Exp $ */
/*
 * Copyright (c) 2013 Markus Friedl <markus@openbsd.org>
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

#include <sys/types.h>

#include "crypto_api.h"

#include <limits.h>
#include <string.h>
#include <stdarg.h>

#include "xmalloc.h"
#include "log.h"
#include "buffer.h"
#include "key.h"
#include "ssh.h"

int
ssh_ed25519_sign(const Key *key, u_char **sigp, u_int *lenp,
    const u_char *data, u_int datalen)
{
	u_char *sig;
	u_int slen, len;
	unsigned long long smlen;
	int ret;
	Buffer b;

	if (key == NULL || key_type_plain(key->type) != KEY_ED25519 ||
	    key->ed25519_sk == NULL) {
		error("%s: no ED25519 key", __func__);
		return -1;
	}

	if (datalen >= UINT_MAX - crypto_sign_ed25519_BYTES) {
		error("%s: datalen %u too long", __func__, datalen);
		return -1;
	}
	smlen = slen = datalen + crypto_sign_ed25519_BYTES;
	sig = xmalloc(slen);

	if ((ret = crypto_sign_ed25519(sig, &smlen, data, datalen,
	    key->ed25519_sk)) != 0 || smlen <= datalen) {
		error("%s: crypto_sign_ed25519 failed: %d", __func__, ret);
		free(sig);
		return -1;
	}
	/* encode signature */
	buffer_init(&b);
	buffer_put_cstring(&b, "ssh-ed25519");
	buffer_put_string(&b, sig, smlen - datalen);
	len = buffer_len(&b);
	if (lenp != NULL)
		*lenp = len;
	if (sigp != NULL) {
		*sigp = xmalloc(len);
		memcpy(*sigp, buffer_ptr(&b), len);
	}
	buffer_free(&b);
	explicit_bzero(sig, slen);
	free(sig);

	return 0;
}

int
ssh_ed25519_verify(const Key *key, const u_char *signature, u_int signaturelen,
    const u_char *data, u_int datalen)
{
	Buffer b;
	char *ktype;
	u_char *sigblob, *sm, *m;
	u_int len;
	unsigned long long smlen, mlen;
	int rlen, ret;

	if (key == NULL || key_type_plain(key->type) != KEY_ED25519 ||
	    key->ed25519_pk == NULL) {
		error("%s: no ED25519 key", __func__);
		return -1;
	}
	buffer_init(&b);
	buffer_append(&b, signature, signaturelen);
	ktype = buffer_get_cstring(&b, NULL);
	if (strcmp("ssh-ed25519", ktype) != 0) {
		error("%s: cannot handle type %s", __func__, ktype);
		buffer_free(&b);
		free(ktype);
		return -1;
	}
	free(ktype);
	sigblob = buffer_get_string(&b, &len);
	rlen = buffer_len(&b);
	buffer_free(&b);
	if (rlen != 0) {
		error("%s: remaining bytes in signature %d", __func__, rlen);
		free(sigblob);
		return -1;
	}
	if (len > crypto_sign_ed25519_BYTES) {
		error("%s: len %u > crypto_sign_ed25519_BYTES %u", __func__,
		    len, crypto_sign_ed25519_BYTES);
		free(sigblob);
		return -1;
	}
	smlen = len + datalen;
	sm = xmalloc(smlen);
	memcpy(sm, sigblob, len);
	memcpy(sm+len, data, datalen);
	mlen = smlen;
	m = xmalloc(mlen);
	if ((ret = crypto_sign_ed25519_open(m, &mlen, sm, smlen,
	    key->ed25519_pk)) != 0) {
		debug2("%s: crypto_sign_ed25519_open failed: %d",
		    __func__, ret);
	}
	if (ret == 0 && mlen != datalen) {
		debug2("%s: crypto_sign_ed25519_open "
		    "mlen != datalen (%llu != %u)", __func__, mlen, datalen);
		ret = -1;
	}
	/* XXX compare 'm' and 'data' ? */

	explicit_bzero(sigblob, len);
	explicit_bzero(sm, smlen);
	explicit_bzero(m, smlen); /* NB. mlen may be invalid if ret != 0 */
	free(sigblob);
	free(sm);
	free(m);
	debug("%s: signature %scorrect", __func__, (ret != 0) ? "in" : "");

	/* translate return code carefully */
	return (ret == 0) ? 1 : -1;
}
