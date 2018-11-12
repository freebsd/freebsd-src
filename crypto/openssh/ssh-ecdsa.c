/* $OpenBSD: ssh-ecdsa.c,v 1.10 2014/02/03 23:28:00 djm Exp $ */
/*
 * Copyright (c) 2000 Markus Friedl.  All rights reserved.
 * Copyright (c) 2010 Damien Miller.  All rights reserved.
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

#ifdef OPENSSL_HAS_ECC

#include <sys/types.h>

#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/evp.h>

#include <string.h>

#include "xmalloc.h"
#include "buffer.h"
#include "compat.h"
#include "log.h"
#include "key.h"
#include "digest.h"

int
ssh_ecdsa_sign(const Key *key, u_char **sigp, u_int *lenp,
    const u_char *data, u_int datalen)
{
	ECDSA_SIG *sig;
	int hash_alg;
	u_char digest[SSH_DIGEST_MAX_LENGTH];
	u_int len, dlen;
	Buffer b, bb;

	if (key == NULL || key_type_plain(key->type) != KEY_ECDSA ||
	    key->ecdsa == NULL) {
		error("%s: no ECDSA key", __func__);
		return -1;
	}

	hash_alg = key_ec_nid_to_hash_alg(key->ecdsa_nid);
	if ((dlen = ssh_digest_bytes(hash_alg)) == 0) {
		error("%s: bad hash algorithm %d", __func__, hash_alg);
		return -1;
	}
	if (ssh_digest_memory(hash_alg, data, datalen,
	    digest, sizeof(digest)) != 0) {
		error("%s: digest_memory failed", __func__);
		return -1;
	}

	sig = ECDSA_do_sign(digest, dlen, key->ecdsa);
	explicit_bzero(digest, sizeof(digest));

	if (sig == NULL) {
		error("%s: sign failed", __func__);
		return -1;
	}

	buffer_init(&bb);
	buffer_put_bignum2(&bb, sig->r);
	buffer_put_bignum2(&bb, sig->s);
	ECDSA_SIG_free(sig);

	buffer_init(&b);
	buffer_put_cstring(&b, key_ssh_name_plain(key));
	buffer_put_string(&b, buffer_ptr(&bb), buffer_len(&bb));
	buffer_free(&bb);
	len = buffer_len(&b);
	if (lenp != NULL)
		*lenp = len;
	if (sigp != NULL) {
		*sigp = xmalloc(len);
		memcpy(*sigp, buffer_ptr(&b), len);
	}
	buffer_free(&b);

	return 0;
}
int
ssh_ecdsa_verify(const Key *key, const u_char *signature, u_int signaturelen,
    const u_char *data, u_int datalen)
{
	ECDSA_SIG *sig;
	int hash_alg;
	u_char digest[SSH_DIGEST_MAX_LENGTH], *sigblob;
	u_int len, dlen;
	int rlen, ret;
	Buffer b, bb;
	char *ktype;

	if (key == NULL || key_type_plain(key->type) != KEY_ECDSA ||
	    key->ecdsa == NULL) {
		error("%s: no ECDSA key", __func__);
		return -1;
	}

	/* fetch signature */
	buffer_init(&b);
	buffer_append(&b, signature, signaturelen);
	ktype = buffer_get_string(&b, NULL);
	if (strcmp(key_ssh_name_plain(key), ktype) != 0) {
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

	/* parse signature */
	if ((sig = ECDSA_SIG_new()) == NULL)
		fatal("%s: ECDSA_SIG_new failed", __func__);

	buffer_init(&bb);
	buffer_append(&bb, sigblob, len);
	buffer_get_bignum2(&bb, sig->r);
	buffer_get_bignum2(&bb, sig->s);
	if (buffer_len(&bb) != 0)
		fatal("%s: remaining bytes in inner sigblob", __func__);
	buffer_free(&bb);

	/* clean up */
	explicit_bzero(sigblob, len);
	free(sigblob);

	/* hash the data */
	hash_alg = key_ec_nid_to_hash_alg(key->ecdsa_nid);
	if ((dlen = ssh_digest_bytes(hash_alg)) == 0) {
		error("%s: bad hash algorithm %d", __func__, hash_alg);
		return -1;
	}
	if (ssh_digest_memory(hash_alg, data, datalen,
	    digest, sizeof(digest)) != 0) {
		error("%s: digest_memory failed", __func__);
		return -1;
	}

	ret = ECDSA_do_verify(digest, dlen, sig, key->ecdsa);
	explicit_bzero(digest, sizeof(digest));

	ECDSA_SIG_free(sig);

	debug("%s: signature %s", __func__,
	    ret == 1 ? "correct" : ret == 0 ? "incorrect" : "error");
	return ret;
}

#endif /* OPENSSL_HAS_ECC */
