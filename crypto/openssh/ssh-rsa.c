/*
 * Copyright (c) 2000 Markus Friedl.  All rights reserved.
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
RCSID("$OpenBSD: ssh-rsa.c,v 1.16 2002/02/24 19:14:59 markus Exp $");

#include <openssl/evp.h>
#include <openssl/err.h>

#include "xmalloc.h"
#include "log.h"
#include "buffer.h"
#include "bufaux.h"
#include "key.h"
#include "ssh-rsa.h"
#include "compat.h"

/* RSASSA-PKCS1-v1_5 (PKCS #1 v2.0 signature) with SHA1 */
int
ssh_rsa_sign(
    Key *key,
    u_char **sigp, u_int *lenp,
    u_char *data, u_int datalen)
{
	const EVP_MD *evp_md;
	EVP_MD_CTX md;
	u_char digest[EVP_MAX_MD_SIZE], *sig, *ret;
	u_int slen, dlen, len;
	int ok, nid;
	Buffer b;

	if (key == NULL || key->type != KEY_RSA || key->rsa == NULL) {
		error("ssh_rsa_sign: no RSA key");
		return -1;
	}
	if (datafellows & SSH_BUG_SIGBLOB) {
		error("ssh_rsa_sign: SSH_BUG_SIGBLOB not supported");
		return -1;
	}
	nid = (datafellows & SSH_BUG_RSASIGMD5) ? NID_md5 : NID_sha1;
	if ((evp_md = EVP_get_digestbynid(nid)) == NULL) {
		error("ssh_rsa_sign: EVP_get_digestbynid %d failed", nid);
		return -1;
	}
	EVP_DigestInit(&md, evp_md);
	EVP_DigestUpdate(&md, data, datalen);
	EVP_DigestFinal(&md, digest, &dlen);

	slen = RSA_size(key->rsa);
	sig = xmalloc(slen);

	ok = RSA_sign(nid, digest, dlen, sig, &len, key->rsa);
	memset(digest, 'd', sizeof(digest));

	if (ok != 1) {
		int ecode = ERR_get_error();
		error("ssh_rsa_sign: RSA_sign failed: %s", ERR_error_string(ecode, NULL));
		xfree(sig);
		return -1;
	}
	if (len < slen) {
		int diff = slen - len;
		debug("slen %d > len %d", slen, len);
		memmove(sig + diff, sig, len);
		memset(sig, 0, diff);
	} else if (len > slen) {
		error("ssh_rsa_sign: slen %d slen2 %d", slen, len);
		xfree(sig);
		return -1;
	}
	/* encode signature */
	buffer_init(&b);
	buffer_put_cstring(&b, "ssh-rsa");
	buffer_put_string(&b, sig, slen);
	len = buffer_len(&b);
	ret = xmalloc(len);
	memcpy(ret, buffer_ptr(&b), len);
	buffer_free(&b);
	memset(sig, 's', slen);
	xfree(sig);

	if (lenp != NULL)
		*lenp = len;
	if (sigp != NULL)
		*sigp = ret;
	return 0;
}

int
ssh_rsa_verify(
    Key *key,
    u_char *signature, u_int signaturelen,
    u_char *data, u_int datalen)
{
	Buffer b;
	const EVP_MD *evp_md;
	EVP_MD_CTX md;
	char *ktype;
	u_char digest[EVP_MAX_MD_SIZE], *sigblob;
	u_int len, dlen;
	int rlen, ret, nid;

	if (key == NULL || key->type != KEY_RSA || key->rsa == NULL) {
		error("ssh_rsa_verify: no RSA key");
		return -1;
	}
	if (datafellows & SSH_BUG_SIGBLOB) {
		error("ssh_rsa_verify: SSH_BUG_SIGBLOB not supported");
		return -1;
	}
	if (BN_num_bits(key->rsa->n) < 768) {
		error("ssh_rsa_verify: n too small: %d bits",
		    BN_num_bits(key->rsa->n));
		return -1;
	}
	buffer_init(&b);
	buffer_append(&b, signature, signaturelen);
	ktype = buffer_get_string(&b, NULL);
	if (strcmp("ssh-rsa", ktype) != 0) {
		error("ssh_rsa_verify: cannot handle type %s", ktype);
		buffer_free(&b);
		xfree(ktype);
		return -1;
	}
	xfree(ktype);
	sigblob = buffer_get_string(&b, &len);
	rlen = buffer_len(&b);
	buffer_free(&b);
	if (rlen != 0) {
		error("ssh_rsa_verify: remaining bytes in signature %d", rlen);
		xfree(sigblob);
		return -1;
	}
	nid = (datafellows & SSH_BUG_RSASIGMD5) ? NID_md5 : NID_sha1;
	if ((evp_md = EVP_get_digestbynid(nid)) == NULL) {
		error("ssh_rsa_verify: EVP_get_digestbynid %d failed", nid);
		xfree(sigblob);
		return -1;
	}
	EVP_DigestInit(&md, evp_md);
	EVP_DigestUpdate(&md, data, datalen);
	EVP_DigestFinal(&md, digest, &dlen);

	ret = RSA_verify(nid, digest, dlen, sigblob, len, key->rsa);
	memset(digest, 'd', sizeof(digest));
	memset(sigblob, 's', len);
	xfree(sigblob);
	if (ret == 0) {
		int ecode = ERR_get_error();
		error("ssh_rsa_verify: RSA_verify failed: %s", ERR_error_string(ecode, NULL));
	}
	debug("ssh_rsa_verify: signature %scorrect", (ret==0) ? "in" : "");
	return ret;
}
