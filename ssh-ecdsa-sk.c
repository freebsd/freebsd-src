/* $OpenBSD: ssh-ecdsa-sk.c,v 1.5 2019/11/26 03:04:27 djm Exp $ */
/*
 * Copyright (c) 2000 Markus Friedl.  All rights reserved.
 * Copyright (c) 2010 Damien Miller.  All rights reserved.
 * Copyright (c) 2019 Google Inc.  All rights reserved.
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

/* #define DEBUG_SK 1 */

#include "includes.h"

#include <sys/types.h>

#ifdef WITH_OPENSSL
#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/evp.h>
#endif

#include <string.h>
#include <stdio.h> /* needed for DEBUG_SK only */

#include "openbsd-compat/openssl-compat.h"

#include "sshbuf.h"
#include "ssherr.h"
#include "digest.h"
#define SSHKEY_INTERNAL
#include "sshkey.h"

/* ARGSUSED */
int
ssh_ecdsa_sk_verify(const struct sshkey *key,
    const u_char *signature, size_t signaturelen,
    const u_char *data, size_t datalen, u_int compat,
    struct sshkey_sig_details **detailsp)
{
#ifdef OPENSSL_HAS_ECC
	ECDSA_SIG *sig = NULL;
	BIGNUM *sig_r = NULL, *sig_s = NULL;
	u_char sig_flags;
	u_char msghash[32], apphash[32], sighash[32];
	u_int sig_counter;
	int ret = SSH_ERR_INTERNAL_ERROR;
	struct sshbuf *b = NULL, *sigbuf = NULL, *original_signed = NULL;
	char *ktype = NULL;
	struct sshkey_sig_details *details = NULL;
#ifdef DEBUG_SK
	char *tmp = NULL;
#endif

	if (detailsp != NULL)
		*detailsp = NULL;
	if (key == NULL || key->ecdsa == NULL ||
	    sshkey_type_plain(key->type) != KEY_ECDSA_SK ||
	    signature == NULL || signaturelen == 0)
		return SSH_ERR_INVALID_ARGUMENT;

	if (key->ecdsa_nid != NID_X9_62_prime256v1)
		return SSH_ERR_INTERNAL_ERROR;

	/* fetch signature */
	if ((b = sshbuf_from(signature, signaturelen)) == NULL)
		return SSH_ERR_ALLOC_FAIL;
	if (sshbuf_get_cstring(b, &ktype, NULL) != 0 ||
	    sshbuf_froms(b, &sigbuf) != 0 ||
	    sshbuf_get_u8(b, &sig_flags) != 0 ||
	    sshbuf_get_u32(b, &sig_counter) != 0) {
		ret = SSH_ERR_INVALID_FORMAT;
		goto out;
	}
	if (strcmp(sshkey_ssh_name_plain(key), ktype) != 0) {
		ret = SSH_ERR_KEY_TYPE_MISMATCH;
		goto out;
	}
	if (sshbuf_len(b) != 0) {
		ret = SSH_ERR_UNEXPECTED_TRAILING_DATA;
		goto out;
	}

	/* parse signature */
	if (sshbuf_get_bignum2(sigbuf, &sig_r) != 0 ||
	    sshbuf_get_bignum2(sigbuf, &sig_s) != 0) {
		ret = SSH_ERR_INVALID_FORMAT;
		goto out;
	}
	if ((sig = ECDSA_SIG_new()) == NULL) {
		ret = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	if (!ECDSA_SIG_set0(sig, sig_r, sig_s)) {
		ret = SSH_ERR_LIBCRYPTO_ERROR;
		goto out;
	}
#ifdef DEBUG_SK
	fprintf(stderr, "%s: data: (len %zu)\n", __func__, datalen);
	/* sshbuf_dump_data(data, datalen, stderr); */
	fprintf(stderr, "%s: sig_r: %s\n", __func__, (tmp = BN_bn2hex(sig_r)));
	free(tmp);
	fprintf(stderr, "%s: sig_s: %s\n", __func__, (tmp = BN_bn2hex(sig_s)));
	free(tmp);
	fprintf(stderr, "%s: sig_flags = 0x%02x, sig_counter = %u\n",
	    __func__, sig_flags, sig_counter);
#endif
	sig_r = sig_s = NULL; /* transferred */

	if (sshbuf_len(sigbuf) != 0) {
		ret = SSH_ERR_UNEXPECTED_TRAILING_DATA;
		goto out;
	}

	/* Reconstruct data that was supposedly signed */
	if ((original_signed = sshbuf_new()) == NULL) {
		ret = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	if ((ret = ssh_digest_memory(SSH_DIGEST_SHA256, data, datalen,
	    msghash, sizeof(msghash))) != 0)
		goto out;
	/* Application value is hashed before signature */
	if ((ret = ssh_digest_memory(SSH_DIGEST_SHA256, key->sk_application,
	    strlen(key->sk_application), apphash, sizeof(apphash))) != 0)
		goto out;
#ifdef DEBUG_SK
	fprintf(stderr, "%s: hashed application:\n", __func__);
	sshbuf_dump_data(apphash, sizeof(apphash), stderr);
	fprintf(stderr, "%s: hashed message:\n", __func__);
	sshbuf_dump_data(msghash, sizeof(msghash), stderr);
#endif
	if ((ret = sshbuf_put(original_signed,
	    apphash, sizeof(apphash))) != 0 ||
	    (ret = sshbuf_put_u8(original_signed, sig_flags)) != 0 ||
	    (ret = sshbuf_put_u32(original_signed, sig_counter)) != 0 ||
	    (ret = sshbuf_put(original_signed, msghash, sizeof(msghash))) != 0)
		goto out;
	/* Signature is over H(original_signed) */
	if ((ret = ssh_digest_buffer(SSH_DIGEST_SHA256, original_signed,
	    sighash, sizeof(sighash))) != 0)
		goto out;
	if ((details = calloc(1, sizeof(*details))) == NULL) {
		ret = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	details->sk_counter = sig_counter;
	details->sk_flags = sig_flags;
#ifdef DEBUG_SK
	fprintf(stderr, "%s: signed buf:\n", __func__);
	sshbuf_dump(original_signed, stderr);
	fprintf(stderr, "%s: signed hash:\n", __func__);
	sshbuf_dump_data(sighash, sizeof(sighash), stderr);
#endif

	/* Verify it */
	switch (ECDSA_do_verify(sighash, sizeof(sighash), sig, key->ecdsa)) {
	case 1:
		ret = 0;
		break;
	case 0:
		ret = SSH_ERR_SIGNATURE_INVALID;
		goto out;
	default:
		ret = SSH_ERR_LIBCRYPTO_ERROR;
		goto out;
	}
	/* success */
	if (detailsp != NULL) {
		*detailsp = details;
		details = NULL;
	}
 out:
	explicit_bzero(&sig_flags, sizeof(sig_flags));
	explicit_bzero(&sig_counter, sizeof(sig_counter));
	explicit_bzero(msghash, sizeof(msghash));
	explicit_bzero(sighash, sizeof(msghash));
	explicit_bzero(apphash, sizeof(apphash));
	sshkey_sig_details_free(details);
	sshbuf_free(original_signed);
	sshbuf_free(sigbuf);
	sshbuf_free(b);
	ECDSA_SIG_free(sig);
	BN_clear_free(sig_r);
	BN_clear_free(sig_s);
	free(ktype);
	return ret;
#else
	return SSH_ERR_INTERNAL_ERROR;
#endif
}
