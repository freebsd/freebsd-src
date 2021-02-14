/* $OpenBSD: ssh-ed25519-sk.c,v 1.5 2020/02/26 13:40:09 jsg Exp $ */
/*
 * Copyright (c) 2019 Markus Friedl.  All rights reserved.
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

#define SSHKEY_INTERNAL
#include <sys/types.h>
#include <limits.h>

#include "crypto_api.h"

#include <string.h>
#include <stdarg.h>

#include "log.h"
#include "sshbuf.h"
#include "sshkey.h"
#include "ssherr.h"
#include "ssh.h"
#include "digest.h"

int
ssh_ed25519_sk_verify(const struct sshkey *key,
    const u_char *signature, size_t signaturelen,
    const u_char *data, size_t datalen, u_int compat,
    struct sshkey_sig_details **detailsp)
{
	struct sshbuf *b = NULL;
	struct sshbuf *encoded = NULL;
	char *ktype = NULL;
	const u_char *sigblob;
	const u_char *sm;
	u_char *m = NULL;
	u_char apphash[32];
	u_char msghash[32];
	u_char sig_flags;
	u_int sig_counter;
	size_t len;
	unsigned long long smlen = 0, mlen = 0;
	int r = SSH_ERR_INTERNAL_ERROR;
	int ret;
	struct sshkey_sig_details *details = NULL;

	if (detailsp != NULL)
		*detailsp = NULL;

	if (key == NULL ||
	    sshkey_type_plain(key->type) != KEY_ED25519_SK ||
	    key->ed25519_pk == NULL ||
	    signature == NULL || signaturelen == 0)
		return SSH_ERR_INVALID_ARGUMENT;

	if ((b = sshbuf_from(signature, signaturelen)) == NULL)
		return SSH_ERR_ALLOC_FAIL;
	if (sshbuf_get_cstring(b, &ktype, NULL) != 0 ||
	    sshbuf_get_string_direct(b, &sigblob, &len) != 0 ||
	    sshbuf_get_u8(b, &sig_flags) != 0 ||
	    sshbuf_get_u32(b, &sig_counter) != 0) {
		r = SSH_ERR_INVALID_FORMAT;
		goto out;
	}
#ifdef DEBUG_SK
	fprintf(stderr, "%s: data:\n", __func__);
	/* sshbuf_dump_data(data, datalen, stderr); */
	fprintf(stderr, "%s: sigblob:\n", __func__);
	sshbuf_dump_data(sigblob, len, stderr);
	fprintf(stderr, "%s: sig_flags = 0x%02x, sig_counter = %u\n",
	    __func__, sig_flags, sig_counter);
#endif
	if (strcmp(sshkey_ssh_name_plain(key), ktype) != 0) {
		r = SSH_ERR_KEY_TYPE_MISMATCH;
		goto out;
	}
	if (sshbuf_len(b) != 0) {
		r = SSH_ERR_UNEXPECTED_TRAILING_DATA;
		goto out;
	}
	if (len > crypto_sign_ed25519_BYTES) {
		r = SSH_ERR_INVALID_FORMAT;
		goto out;
	}
	if (ssh_digest_memory(SSH_DIGEST_SHA256, key->sk_application,
	    strlen(key->sk_application), apphash, sizeof(apphash)) != 0 ||
	    ssh_digest_memory(SSH_DIGEST_SHA256, data, datalen,
	    msghash, sizeof(msghash)) != 0) {
		r = SSH_ERR_INVALID_ARGUMENT;
		goto out;
	}
#ifdef DEBUG_SK
	fprintf(stderr, "%s: hashed application:\n", __func__);
	sshbuf_dump_data(apphash, sizeof(apphash), stderr);
	fprintf(stderr, "%s: hashed message:\n", __func__);
	sshbuf_dump_data(msghash, sizeof(msghash), stderr);
#endif
	if ((details = calloc(1, sizeof(*details))) == NULL) {
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	details->sk_counter = sig_counter;
	details->sk_flags = sig_flags;
	if ((encoded = sshbuf_new()) == NULL) {
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	if (sshbuf_put(encoded, sigblob, len) != 0 ||
	    sshbuf_put(encoded, apphash, sizeof(apphash)) != 0 ||
	    sshbuf_put_u8(encoded, sig_flags) != 0 ||
	    sshbuf_put_u32(encoded, sig_counter) != 0 ||
	    sshbuf_put(encoded, msghash, sizeof(msghash)) != 0) {
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
#ifdef DEBUG_SK
	fprintf(stderr, "%s: signed buf:\n", __func__);
	sshbuf_dump(encoded, stderr);
#endif
	sm = sshbuf_ptr(encoded);
	smlen = sshbuf_len(encoded);
	mlen = smlen;
	if ((m = malloc(smlen)) == NULL) {
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	if ((ret = crypto_sign_ed25519_open(m, &mlen, sm, smlen,
	    key->ed25519_pk)) != 0) {
		debug2("%s: crypto_sign_ed25519_open failed: %d",
		    __func__, ret);
	}
	if (ret != 0 || mlen != smlen - len) {
		r = SSH_ERR_SIGNATURE_INVALID;
		goto out;
	}
	/* XXX compare 'm' and 'sm + len' ? */
	/* success */
	r = 0;
	if (detailsp != NULL) {
		*detailsp = details;
		details = NULL;
	}
 out:
	if (m != NULL)
		freezero(m, smlen); /* NB mlen may be invalid if r != 0 */
	sshkey_sig_details_free(details);
	sshbuf_free(b);
	sshbuf_free(encoded);
	free(ktype);
	return r;
}
