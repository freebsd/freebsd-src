/* $OpenBSD: ssh-ecdsa.c,v 1.5 2012/01/08 13:17:11 miod Exp $ */
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

int
ssh_ecdsa_sign(const Key *key, u_char **sigp, u_int *lenp,
    const u_char *data, u_int datalen)
{
	ECDSA_SIG *sig;
	const EVP_MD *evp_md;
	EVP_MD_CTX md;
	u_char digest[EVP_MAX_MD_SIZE];
	u_int len, dlen;
	Buffer b, bb;

	if (key == NULL || key->ecdsa == NULL ||
	    (key->type != KEY_ECDSA && key->type != KEY_ECDSA_CERT)) {
		error("%s: no ECDSA key", __func__);
		return -1;
	}
	evp_md = key_ec_nid_to_evpmd(key->ecdsa_nid);
	EVP_DigestInit(&md, evp_md);
	EVP_DigestUpdate(&md, data, datalen);
	EVP_DigestFinal(&md, digest, &dlen);

	sig = ECDSA_do_sign(digest, dlen, key->ecdsa);
	memset(digest, 'd', sizeof(digest));

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
	const EVP_MD *evp_md;
	EVP_MD_CTX md;
	u_char digest[EVP_MAX_MD_SIZE], *sigblob;
	u_int len, dlen;
	int rlen, ret;
	Buffer b, bb;
	char *ktype;

	if (key == NULL || key->ecdsa == NULL ||
	    (key->type != KEY_ECDSA && key->type != KEY_ECDSA_CERT)) {
		error("%s: no ECDSA key", __func__);
		return -1;
	}
	evp_md = key_ec_nid_to_evpmd(key->ecdsa_nid);

	/* fetch signature */
	buffer_init(&b);
	buffer_append(&b, signature, signaturelen);
	ktype = buffer_get_string(&b, NULL);
	if (strcmp(key_ssh_name_plain(key), ktype) != 0) {
		error("%s: cannot handle type %s", __func__, ktype);
		buffer_free(&b);
		xfree(ktype);
		return -1;
	}
	xfree(ktype);
	sigblob = buffer_get_string(&b, &len);
	rlen = buffer_len(&b);
	buffer_free(&b);
	if (rlen != 0) {
		error("%s: remaining bytes in signature %d", __func__, rlen);
		xfree(sigblob);
		return -1;
	}

	/* parse signature */
	if ((sig = ECDSA_SIG_new()) == NULL)
		fatal("%s: ECDSA_SIG_new failed", __func__);
	if ((sig->r = BN_new()) == NULL ||
	    (sig->s = BN_new()) == NULL)
		fatal("%s: BN_new failed", __func__);

	buffer_init(&bb);
	buffer_append(&bb, sigblob, len);
	buffer_get_bignum2(&bb, sig->r);
	buffer_get_bignum2(&bb, sig->s);
	if (buffer_len(&bb) != 0)
		fatal("%s: remaining bytes in inner sigblob", __func__);
	buffer_free(&bb);

	/* clean up */
	memset(sigblob, 0, len);
	xfree(sigblob);

	/* hash the data */
	EVP_DigestInit(&md, evp_md);
	EVP_DigestUpdate(&md, data, datalen);
	EVP_DigestFinal(&md, digest, &dlen);

	ret = ECDSA_do_verify(digest, dlen, sig, key->ecdsa);
	memset(digest, 'd', sizeof(digest));

	ECDSA_SIG_free(sig);

	debug("%s: signature %s", __func__,
	    ret == 1 ? "correct" : ret == 0 ? "incorrect" : "error");
	return ret;
}

#endif /* OPENSSL_HAS_ECC */
