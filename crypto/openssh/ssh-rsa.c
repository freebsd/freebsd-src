/* $OpenBSD: ssh-rsa.c,v 1.50 2014/01/09 23:20:00 djm Exp $ */
/*
 * Copyright (c) 2000, 2003 Markus Friedl <markus@openbsd.org>
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

#include <openssl/evp.h>
#include <openssl/err.h>

#include <stdarg.h>
#include <string.h>

#include "xmalloc.h"
#include "log.h"
#include "buffer.h"
#include "key.h"
#include "compat.h"
#include "misc.h"
#include "ssh.h"
#include "digest.h"

static int openssh_RSA_verify(int, u_char *, u_int, u_char *, u_int, RSA *);

/* RSASSA-PKCS1-v1_5 (PKCS #1 v2.0 signature) with SHA1 */
int
ssh_rsa_sign(const Key *key, u_char **sigp, u_int *lenp,
    const u_char *data, u_int datalen)
{
	int hash_alg;
	u_char digest[SSH_DIGEST_MAX_LENGTH], *sig;
	u_int slen, dlen, len;
	int ok, nid;
	Buffer b;

	if (key == NULL || key_type_plain(key->type) != KEY_RSA ||
	    key->rsa == NULL) {
		error("%s: no RSA key", __func__);
		return -1;
	}

	/* hash the data */
	hash_alg = SSH_DIGEST_SHA1;
	nid = NID_sha1;
	if ((dlen = ssh_digest_bytes(hash_alg)) == 0) {
		error("%s: bad hash algorithm %d", __func__, hash_alg);
		return -1;
	}
	if (ssh_digest_memory(hash_alg, data, datalen,
	    digest, sizeof(digest)) != 0) {
		error("%s: ssh_digest_memory failed", __func__);
		return -1;
	}

	slen = RSA_size(key->rsa);
	sig = xmalloc(slen);

	ok = RSA_sign(nid, digest, dlen, sig, &len, key->rsa);
	memset(digest, 'd', sizeof(digest));

	if (ok != 1) {
		int ecode = ERR_get_error();

		error("%s: RSA_sign failed: %s", __func__,
		    ERR_error_string(ecode, NULL));
		free(sig);
		return -1;
	}
	if (len < slen) {
		u_int diff = slen - len;
		debug("slen %u > len %u", slen, len);
		memmove(sig + diff, sig, len);
		memset(sig, 0, diff);
	} else if (len > slen) {
		error("%s: slen %u slen2 %u", __func__, slen, len);
		free(sig);
		return -1;
	}
	/* encode signature */
	buffer_init(&b);
	buffer_put_cstring(&b, "ssh-rsa");
	buffer_put_string(&b, sig, slen);
	len = buffer_len(&b);
	if (lenp != NULL)
		*lenp = len;
	if (sigp != NULL) {
		*sigp = xmalloc(len);
		memcpy(*sigp, buffer_ptr(&b), len);
	}
	buffer_free(&b);
	memset(sig, 's', slen);
	free(sig);

	return 0;
}

int
ssh_rsa_verify(const Key *key, const u_char *signature, u_int signaturelen,
    const u_char *data, u_int datalen)
{
	Buffer b;
	int hash_alg;
	char *ktype;
	u_char digest[SSH_DIGEST_MAX_LENGTH], *sigblob;
	u_int len, dlen, modlen;
	int rlen, ret;

	if (key == NULL || key_type_plain(key->type) != KEY_RSA ||
	    key->rsa == NULL) {
		error("%s: no RSA key", __func__);
		return -1;
	}

	if (BN_num_bits(key->rsa->n) < SSH_RSA_MINIMUM_MODULUS_SIZE) {
		error("%s: RSA modulus too small: %d < minimum %d bits",
		    __func__, BN_num_bits(key->rsa->n),
		    SSH_RSA_MINIMUM_MODULUS_SIZE);
		return -1;
	}
	buffer_init(&b);
	buffer_append(&b, signature, signaturelen);
	ktype = buffer_get_cstring(&b, NULL);
	if (strcmp("ssh-rsa", ktype) != 0) {
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
	/* RSA_verify expects a signature of RSA_size */
	modlen = RSA_size(key->rsa);
	if (len > modlen) {
		error("%s: len %u > modlen %u", __func__, len, modlen);
		free(sigblob);
		return -1;
	} else if (len < modlen) {
		u_int diff = modlen - len;
		debug("%s: add padding: modlen %u > len %u", __func__,
		    modlen, len);
		sigblob = xrealloc(sigblob, 1, modlen);
		memmove(sigblob + diff, sigblob, len);
		memset(sigblob, 0, diff);
		len = modlen;
	}
	/* hash the data */
	hash_alg = SSH_DIGEST_SHA1;
	if ((dlen = ssh_digest_bytes(hash_alg)) == 0) {
		error("%s: bad hash algorithm %d", __func__, hash_alg);
		return -1;
	}
	if (ssh_digest_memory(hash_alg, data, datalen,
	    digest, sizeof(digest)) != 0) {
		error("%s: ssh_digest_memory failed", __func__);
		return -1;
	}

	ret = openssh_RSA_verify(hash_alg, digest, dlen, sigblob, len,
	    key->rsa);
	memset(digest, 'd', sizeof(digest));
	memset(sigblob, 's', len);
	free(sigblob);
	debug("%s: signature %scorrect", __func__, (ret == 0) ? "in" : "");
	return ret;
}

/*
 * See:
 * http://www.rsasecurity.com/rsalabs/pkcs/pkcs-1/
 * ftp://ftp.rsasecurity.com/pub/pkcs/pkcs-1/pkcs-1v2-1.asn
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

static int
openssh_RSA_verify(int hash_alg, u_char *hash, u_int hashlen,
    u_char *sigbuf, u_int siglen, RSA *rsa)
{
	u_int ret, rsasize, oidlen = 0, hlen = 0;
	int len, oidmatch, hashmatch;
	const u_char *oid = NULL;
	u_char *decrypted = NULL;

	ret = 0;
	switch (hash_alg) {
	case SSH_DIGEST_SHA1:
		oid = id_sha1;
		oidlen = sizeof(id_sha1);
		hlen = 20;
		break;
	default:
		goto done;
	}
	if (hashlen != hlen) {
		error("bad hashlen");
		goto done;
	}
	rsasize = RSA_size(rsa);
	if (siglen == 0 || siglen > rsasize) {
		error("bad siglen");
		goto done;
	}
	decrypted = xmalloc(rsasize);
	if ((len = RSA_public_decrypt(siglen, sigbuf, decrypted, rsa,
	    RSA_PKCS1_PADDING)) < 0) {
		error("RSA_public_decrypt failed: %s",
		    ERR_error_string(ERR_get_error(), NULL));
		goto done;
	}
	if (len < 0 || (u_int)len != hlen + oidlen) {
		error("bad decrypted len: %d != %d + %d", len, hlen, oidlen);
		goto done;
	}
	oidmatch = timingsafe_bcmp(decrypted, oid, oidlen) == 0;
	hashmatch = timingsafe_bcmp(decrypted + oidlen, hash, hlen) == 0;
	if (!oidmatch) {
		error("oid mismatch");
		goto done;
	}
	if (!hashmatch) {
		error("hash mismatch");
		goto done;
	}
	ret = 1;
done:
	free(decrypted);
	return ret;
}
