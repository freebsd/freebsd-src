/* $OpenBSD: ssh-rsa.c,v 1.44 2010/07/16 14:07:35 djm Exp $ */
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

static int openssh_RSA_verify(int, u_char *, u_int, u_char *, u_int, RSA *);

/* RSASSA-PKCS1-v1_5 (PKCS #1 v2.0 signature) with SHA1 */
int
ssh_rsa_sign(const Key *key, u_char **sigp, u_int *lenp,
    const u_char *data, u_int datalen)
{
	const EVP_MD *evp_md;
	EVP_MD_CTX md;
	u_char digest[EVP_MAX_MD_SIZE], *sig;
	u_int slen, dlen, len;
	int ok, nid;
	Buffer b;

	if (key == NULL || key->rsa == NULL || (key->type != KEY_RSA &&
	    key->type != KEY_RSA_CERT && key->type != KEY_RSA_CERT_V00)) {
		error("ssh_rsa_sign: no RSA key");
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

		error("ssh_rsa_sign: RSA_sign failed: %s",
		    ERR_error_string(ecode, NULL));
		xfree(sig);
		return -1;
	}
	if (len < slen) {
		u_int diff = slen - len;
		debug("slen %u > len %u", slen, len);
		memmove(sig + diff, sig, len);
		memset(sig, 0, diff);
	} else if (len > slen) {
		error("ssh_rsa_sign: slen %u slen2 %u", slen, len);
		xfree(sig);
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
	xfree(sig);

	return 0;
}

int
ssh_rsa_verify(const Key *key, const u_char *signature, u_int signaturelen,
    const u_char *data, u_int datalen)
{
	Buffer b;
	const EVP_MD *evp_md;
	EVP_MD_CTX md;
	char *ktype;
	u_char digest[EVP_MAX_MD_SIZE], *sigblob;
	u_int len, dlen, modlen;
	int rlen, ret, nid;

	if (key == NULL || key->rsa == NULL || (key->type != KEY_RSA &&
	    key->type != KEY_RSA_CERT && key->type != KEY_RSA_CERT_V00)) {
		error("ssh_rsa_verify: no RSA key");
		return -1;
	}
	if (BN_num_bits(key->rsa->n) < SSH_RSA_MINIMUM_MODULUS_SIZE) {
		error("ssh_rsa_verify: RSA modulus too small: %d < minimum %d bits",
		    BN_num_bits(key->rsa->n), SSH_RSA_MINIMUM_MODULUS_SIZE);
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
	/* RSA_verify expects a signature of RSA_size */
	modlen = RSA_size(key->rsa);
	if (len > modlen) {
		error("ssh_rsa_verify: len %u > modlen %u", len, modlen);
		xfree(sigblob);
		return -1;
	} else if (len < modlen) {
		u_int diff = modlen - len;
		debug("ssh_rsa_verify: add padding: modlen %u > len %u",
		    modlen, len);
		sigblob = xrealloc(sigblob, 1, modlen);
		memmove(sigblob + diff, sigblob, len);
		memset(sigblob, 0, diff);
		len = modlen;
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

	ret = openssh_RSA_verify(nid, digest, dlen, sigblob, len, key->rsa);
	memset(digest, 'd', sizeof(digest));
	memset(sigblob, 's', len);
	xfree(sigblob);
	debug("ssh_rsa_verify: signature %scorrect", (ret==0) ? "in" : "");
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
/*
 * id-md5 OBJECT IDENTIFIER ::= { iso(1) member-body(2) us(840)
 *	rsadsi(113549) digestAlgorithm(2) 5 }
 */
static const u_char id_md5[] = {
	0x30, 0x20, /* type Sequence, length 0x20 (32) */
	0x30, 0x0c, /* type Sequence, length 0x09 */
	0x06, 0x08, /* type OID, length 0x05 */
	0x2a, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x02, 0x05, /* id-md5 */
	0x05, 0x00, /* NULL */
	0x04, 0x10  /* Octet string, length 0x10 (16), followed by md5 hash */
};

static int
openssh_RSA_verify(int type, u_char *hash, u_int hashlen,
    u_char *sigbuf, u_int siglen, RSA *rsa)
{
	u_int ret, rsasize, oidlen = 0, hlen = 0;
	int len, oidmatch, hashmatch;
	const u_char *oid = NULL;
	u_char *decrypted = NULL;

	ret = 0;
	switch (type) {
	case NID_sha1:
		oid = id_sha1;
		oidlen = sizeof(id_sha1);
		hlen = 20;
		break;
	case NID_md5:
		oid = id_md5;
		oidlen = sizeof(id_md5);
		hlen = 16;
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
	if (decrypted)
		xfree(decrypted);
	return ret;
}
