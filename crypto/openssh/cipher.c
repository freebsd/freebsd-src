/* $OpenBSD: cipher.c,v 1.97 2014/02/07 06:55:54 djm Exp $ */
/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 *
 *
 * Copyright (c) 1999 Niels Provos.  All rights reserved.
 * Copyright (c) 1999, 2000 Markus Friedl.  All rights reserved.
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

#include <sys/types.h>

#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#include "xmalloc.h"
#include "log.h"
#include "misc.h"
#include "cipher.h"
#include "buffer.h"
#include "digest.h"

/* compatibility with old or broken OpenSSL versions */
#include "openbsd-compat/openssl-compat.h"

extern const EVP_CIPHER *evp_ssh1_bf(void);
extern const EVP_CIPHER *evp_ssh1_3des(void);
extern void ssh1_3des_iv(EVP_CIPHER_CTX *, int, u_char *, int);

struct Cipher {
	char	*name;
	int	number;		/* for ssh1 only */
	u_int	block_size;
	u_int	key_len;
	u_int	iv_len;		/* defaults to block_size */
	u_int	auth_len;
	u_int	discard_len;
	u_int	flags;
#define CFLAG_CBC		(1<<0)
#define CFLAG_CHACHAPOLY	(1<<1)
	const EVP_CIPHER	*(*evptype)(void);
};

static const struct Cipher ciphers[] = {
	{ "none",	SSH_CIPHER_NONE, 8, 0, 0, 0, 0, 0, EVP_enc_null },
	{ "des",	SSH_CIPHER_DES, 8, 8, 0, 0, 0, 1, EVP_des_cbc },
	{ "3des",	SSH_CIPHER_3DES, 8, 16, 0, 0, 0, 1, evp_ssh1_3des },
	{ "blowfish",	SSH_CIPHER_BLOWFISH, 8, 32, 0, 0, 0, 1, evp_ssh1_bf },

	{ "3des-cbc",	SSH_CIPHER_SSH2, 8, 24, 0, 0, 0, 1, EVP_des_ede3_cbc },
	{ "blowfish-cbc",
			SSH_CIPHER_SSH2, 8, 16, 0, 0, 0, 1, EVP_bf_cbc },
	{ "cast128-cbc",
			SSH_CIPHER_SSH2, 8, 16, 0, 0, 0, 1, EVP_cast5_cbc },
	{ "arcfour",	SSH_CIPHER_SSH2, 8, 16, 0, 0, 0, 0, EVP_rc4 },
	{ "arcfour128",	SSH_CIPHER_SSH2, 8, 16, 0, 0, 1536, 0, EVP_rc4 },
	{ "arcfour256",	SSH_CIPHER_SSH2, 8, 32, 0, 0, 1536, 0, EVP_rc4 },
	{ "aes128-cbc",	SSH_CIPHER_SSH2, 16, 16, 0, 0, 0, 1, EVP_aes_128_cbc },
	{ "aes192-cbc",	SSH_CIPHER_SSH2, 16, 24, 0, 0, 0, 1, EVP_aes_192_cbc },
	{ "aes256-cbc",	SSH_CIPHER_SSH2, 16, 32, 0, 0, 0, 1, EVP_aes_256_cbc },
	{ "rijndael-cbc@lysator.liu.se",
			SSH_CIPHER_SSH2, 16, 32, 0, 0, 0, 1, EVP_aes_256_cbc },
	{ "aes128-ctr",	SSH_CIPHER_SSH2, 16, 16, 0, 0, 0, 0, EVP_aes_128_ctr },
	{ "aes192-ctr",	SSH_CIPHER_SSH2, 16, 24, 0, 0, 0, 0, EVP_aes_192_ctr },
	{ "aes256-ctr",	SSH_CIPHER_SSH2, 16, 32, 0, 0, 0, 0, EVP_aes_256_ctr },
#ifdef OPENSSL_HAVE_EVPGCM
	{ "aes128-gcm@openssh.com",
			SSH_CIPHER_SSH2, 16, 16, 12, 16, 0, 0, EVP_aes_128_gcm },
	{ "aes256-gcm@openssh.com",
			SSH_CIPHER_SSH2, 16, 32, 12, 16, 0, 0, EVP_aes_256_gcm },
#endif
	{ "chacha20-poly1305@openssh.com",
			SSH_CIPHER_SSH2, 8, 64, 0, 16, 0, CFLAG_CHACHAPOLY, NULL },
	{ NULL,		SSH_CIPHER_INVALID, 0, 0, 0, 0, 0, 0, NULL }
};

/*--*/

/* Returns a list of supported ciphers separated by the specified char. */
char *
cipher_alg_list(char sep, int auth_only)
{
	char *ret = NULL;
	size_t nlen, rlen = 0;
	const Cipher *c;

	for (c = ciphers; c->name != NULL; c++) {
		if (c->number != SSH_CIPHER_SSH2)
			continue;
		if (auth_only && c->auth_len == 0)
			continue;
		if (ret != NULL)
			ret[rlen++] = sep;
		nlen = strlen(c->name);
		ret = xrealloc(ret, 1, rlen + nlen + 2);
		memcpy(ret + rlen, c->name, nlen + 1);
		rlen += nlen;
	}
	return ret;
}

u_int
cipher_blocksize(const Cipher *c)
{
	return (c->block_size);
}

u_int
cipher_keylen(const Cipher *c)
{
	return (c->key_len);
}

u_int
cipher_seclen(const Cipher *c)
{
	if (strcmp("3des-cbc", c->name) == 0)
		return 14;
	return cipher_keylen(c);
}

u_int
cipher_authlen(const Cipher *c)
{
	return (c->auth_len);
}

u_int
cipher_ivlen(const Cipher *c)
{
	/*
	 * Default is cipher block size, except for chacha20+poly1305 that
	 * needs no IV. XXX make iv_len == -1 default?
	 */
	return (c->iv_len != 0 || (c->flags & CFLAG_CHACHAPOLY) != 0) ?
	    c->iv_len : c->block_size;
}

u_int
cipher_get_number(const Cipher *c)
{
	return (c->number);
}

u_int
cipher_is_cbc(const Cipher *c)
{
	return (c->flags & CFLAG_CBC) != 0;
}

u_int
cipher_mask_ssh1(int client)
{
	u_int mask = 0;
	mask |= 1 << SSH_CIPHER_3DES;		/* Mandatory */
	mask |= 1 << SSH_CIPHER_BLOWFISH;
	if (client) {
		mask |= 1 << SSH_CIPHER_DES;
	}
	return mask;
}

const Cipher *
cipher_by_name(const char *name)
{
	const Cipher *c;
	for (c = ciphers; c->name != NULL; c++)
		if (strcmp(c->name, name) == 0)
			return c;
	return NULL;
}

const Cipher *
cipher_by_number(int id)
{
	const Cipher *c;
	for (c = ciphers; c->name != NULL; c++)
		if (c->number == id)
			return c;
	return NULL;
}

#define	CIPHER_SEP	","
int
ciphers_valid(const char *names)
{
	const Cipher *c;
	char *cipher_list, *cp;
	char *p;

	if (names == NULL || strcmp(names, "") == 0)
		return 0;
	cipher_list = cp = xstrdup(names);
	for ((p = strsep(&cp, CIPHER_SEP)); p && *p != '\0';
	    (p = strsep(&cp, CIPHER_SEP))) {
		c = cipher_by_name(p);
		if (c == NULL || c->number != SSH_CIPHER_SSH2) {
			debug("bad cipher %s [%s]", p, names);
			free(cipher_list);
			return 0;
		}
	}
	debug3("ciphers ok: [%s]", names);
	free(cipher_list);
	return 1;
}

/*
 * Parses the name of the cipher.  Returns the number of the corresponding
 * cipher, or -1 on error.
 */

int
cipher_number(const char *name)
{
	const Cipher *c;
	if (name == NULL)
		return -1;
	for (c = ciphers; c->name != NULL; c++)
		if (strcasecmp(c->name, name) == 0)
			return c->number;
	return -1;
}

char *
cipher_name(int id)
{
	const Cipher *c = cipher_by_number(id);
	return (c==NULL) ? "<unknown>" : c->name;
}

void
cipher_init(CipherContext *cc, const Cipher *cipher,
    const u_char *key, u_int keylen, const u_char *iv, u_int ivlen,
    int do_encrypt)
{
	static int dowarn = 1;
#ifdef SSH_OLD_EVP
	EVP_CIPHER *type;
#else
	const EVP_CIPHER *type;
	int klen;
#endif
	u_char *junk, *discard;

	if (cipher->number == SSH_CIPHER_DES) {
		if (dowarn) {
			error("Warning: use of DES is strongly discouraged "
			    "due to cryptographic weaknesses");
			dowarn = 0;
		}
		if (keylen > 8)
			keylen = 8;
	}
	cc->plaintext = (cipher->number == SSH_CIPHER_NONE);
	cc->encrypt = do_encrypt;

	if (keylen < cipher->key_len)
		fatal("cipher_init: key length %d is insufficient for %s.",
		    keylen, cipher->name);
	if (iv != NULL && ivlen < cipher_ivlen(cipher))
		fatal("cipher_init: iv length %d is insufficient for %s.",
		    ivlen, cipher->name);
	cc->cipher = cipher;

	if ((cc->cipher->flags & CFLAG_CHACHAPOLY) != 0) {
		chachapoly_init(&cc->cp_ctx, key, keylen);
		return;
	}
	type = (*cipher->evptype)();
	EVP_CIPHER_CTX_init(&cc->evp);
#ifdef SSH_OLD_EVP
	if (type->key_len > 0 && type->key_len != keylen) {
		debug("cipher_init: set keylen (%d -> %d)",
		    type->key_len, keylen);
		type->key_len = keylen;
	}
	EVP_CipherInit(&cc->evp, type, (u_char *)key, (u_char *)iv,
	    (do_encrypt == CIPHER_ENCRYPT));
#else
	if (EVP_CipherInit(&cc->evp, type, NULL, (u_char *)iv,
	    (do_encrypt == CIPHER_ENCRYPT)) == 0)
		fatal("cipher_init: EVP_CipherInit failed for %s",
		    cipher->name);
	if (cipher_authlen(cipher) &&
	    !EVP_CIPHER_CTX_ctrl(&cc->evp, EVP_CTRL_GCM_SET_IV_FIXED,
	    -1, (u_char *)iv))
		fatal("cipher_init: EVP_CTRL_GCM_SET_IV_FIXED failed for %s",
		    cipher->name);
	klen = EVP_CIPHER_CTX_key_length(&cc->evp);
	if (klen > 0 && keylen != (u_int)klen) {
		debug2("cipher_init: set keylen (%d -> %d)", klen, keylen);
		if (EVP_CIPHER_CTX_set_key_length(&cc->evp, keylen) == 0)
			fatal("cipher_init: set keylen failed (%d -> %d)",
			    klen, keylen);
	}
	if (EVP_CipherInit(&cc->evp, NULL, (u_char *)key, NULL, -1) == 0)
		fatal("cipher_init: EVP_CipherInit: set key failed for %s",
		    cipher->name);
#endif

	if (cipher->discard_len > 0) {
		junk = xmalloc(cipher->discard_len);
		discard = xmalloc(cipher->discard_len);
		if (EVP_Cipher(&cc->evp, discard, junk,
		    cipher->discard_len) == 0)
			fatal("evp_crypt: EVP_Cipher failed during discard");
		explicit_bzero(discard, cipher->discard_len);
		free(junk);
		free(discard);
	}
}

/*
 * cipher_crypt() operates as following:
 * Copy 'aadlen' bytes (without en/decryption) from 'src' to 'dest'.
 * Theses bytes are treated as additional authenticated data for
 * authenticated encryption modes.
 * En/Decrypt 'len' bytes at offset 'aadlen' from 'src' to 'dest'.
 * Use 'authlen' bytes at offset 'len'+'aadlen' as the authentication tag.
 * This tag is written on encryption and verified on decryption.
 * Both 'aadlen' and 'authlen' can be set to 0.
 * cipher_crypt() returns 0 on success and -1 if the decryption integrity
 * check fails.
 */
int
cipher_crypt(CipherContext *cc, u_int seqnr, u_char *dest, const u_char *src,
    u_int len, u_int aadlen, u_int authlen)
{
	if ((cc->cipher->flags & CFLAG_CHACHAPOLY) != 0)
		return chachapoly_crypt(&cc->cp_ctx, seqnr, dest, src, len,
		    aadlen, authlen, cc->encrypt);
	if (authlen) {
		u_char lastiv[1];

		if (authlen != cipher_authlen(cc->cipher))
			fatal("%s: authlen mismatch %d", __func__, authlen);
		/* increment IV */
		if (!EVP_CIPHER_CTX_ctrl(&cc->evp, EVP_CTRL_GCM_IV_GEN,
		    1, lastiv))
			fatal("%s: EVP_CTRL_GCM_IV_GEN", __func__);
		/* set tag on decyption */
		if (!cc->encrypt &&
		    !EVP_CIPHER_CTX_ctrl(&cc->evp, EVP_CTRL_GCM_SET_TAG,
		    authlen, (u_char *)src + aadlen + len))
			fatal("%s: EVP_CTRL_GCM_SET_TAG", __func__);
	}
	if (aadlen) {
		if (authlen &&
		    EVP_Cipher(&cc->evp, NULL, (u_char *)src, aadlen) < 0)
			fatal("%s: EVP_Cipher(aad) failed", __func__);
		memcpy(dest, src, aadlen);
	}
	if (len % cc->cipher->block_size)
		fatal("%s: bad plaintext length %d", __func__, len);
	if (EVP_Cipher(&cc->evp, dest + aadlen, (u_char *)src + aadlen,
	    len) < 0)
		fatal("%s: EVP_Cipher failed", __func__);
	if (authlen) {
		/* compute tag (on encrypt) or verify tag (on decrypt) */
		if (EVP_Cipher(&cc->evp, NULL, NULL, 0) < 0) {
			if (cc->encrypt)
				fatal("%s: EVP_Cipher(final) failed", __func__);
			else
				return -1;
		}
		if (cc->encrypt &&
		    !EVP_CIPHER_CTX_ctrl(&cc->evp, EVP_CTRL_GCM_GET_TAG,
		    authlen, dest + aadlen + len))
			fatal("%s: EVP_CTRL_GCM_GET_TAG", __func__);
	}
	return 0;
}

/* Extract the packet length, including any decryption necessary beforehand */
int
cipher_get_length(CipherContext *cc, u_int *plenp, u_int seqnr,
    const u_char *cp, u_int len)
{
	if ((cc->cipher->flags & CFLAG_CHACHAPOLY) != 0)
		return chachapoly_get_length(&cc->cp_ctx, plenp, seqnr,
		    cp, len);
	if (len < 4)
		return -1;
	*plenp = get_u32(cp);
	return 0;
}

void
cipher_cleanup(CipherContext *cc)
{
	if ((cc->cipher->flags & CFLAG_CHACHAPOLY) != 0)
		explicit_bzero(&cc->cp_ctx, sizeof(cc->cp_ctx));
	else if (EVP_CIPHER_CTX_cleanup(&cc->evp) == 0)
		error("cipher_cleanup: EVP_CIPHER_CTX_cleanup failed");
}

/*
 * Selects the cipher, and keys if by computing the MD5 checksum of the
 * passphrase and using the resulting 16 bytes as the key.
 */

void
cipher_set_key_string(CipherContext *cc, const Cipher *cipher,
    const char *passphrase, int do_encrypt)
{
	u_char digest[16];

	if (ssh_digest_memory(SSH_DIGEST_MD5, passphrase, strlen(passphrase),
	    digest, sizeof(digest)) < 0)
		fatal("%s: md5 failed", __func__);

	cipher_init(cc, cipher, digest, 16, NULL, 0, do_encrypt);

	explicit_bzero(digest, sizeof(digest));
}

/*
 * Exports an IV from the CipherContext required to export the key
 * state back from the unprivileged child to the privileged parent
 * process.
 */

int
cipher_get_keyiv_len(const CipherContext *cc)
{
	const Cipher *c = cc->cipher;
	int ivlen;

	if (c->number == SSH_CIPHER_3DES)
		ivlen = 24;
	else if ((cc->cipher->flags & CFLAG_CHACHAPOLY) != 0)
		ivlen = 0;
	else
		ivlen = EVP_CIPHER_CTX_iv_length(&cc->evp);
	return (ivlen);
}

void
cipher_get_keyiv(CipherContext *cc, u_char *iv, u_int len)
{
	const Cipher *c = cc->cipher;
	int evplen;

	if ((cc->cipher->flags & CFLAG_CHACHAPOLY) != 0) {
		if (len != 0)
			fatal("%s: wrong iv length %d != %d", __func__, len, 0);
		return;
	}

	switch (c->number) {
	case SSH_CIPHER_SSH2:
	case SSH_CIPHER_DES:
	case SSH_CIPHER_BLOWFISH:
		evplen = EVP_CIPHER_CTX_iv_length(&cc->evp);
		if (evplen <= 0)
			return;
		if ((u_int)evplen != len)
			fatal("%s: wrong iv length %d != %d", __func__,
			    evplen, len);
#ifdef USE_BUILTIN_RIJNDAEL
		if (c->evptype == evp_rijndael)
			ssh_rijndael_iv(&cc->evp, 0, iv, len);
		else
#endif
#ifndef OPENSSL_HAVE_EVPCTR
		if (c->evptype == evp_aes_128_ctr)
			ssh_aes_ctr_iv(&cc->evp, 0, iv, len);
		else
#endif
		memcpy(iv, cc->evp.iv, len);
		break;
	case SSH_CIPHER_3DES:
		ssh1_3des_iv(&cc->evp, 0, iv, 24);
		break;
	default:
		fatal("%s: bad cipher %d", __func__, c->number);
	}
}

void
cipher_set_keyiv(CipherContext *cc, u_char *iv)
{
	const Cipher *c = cc->cipher;
	int evplen = 0;

	if ((cc->cipher->flags & CFLAG_CHACHAPOLY) != 0)
		return;

	switch (c->number) {
	case SSH_CIPHER_SSH2:
	case SSH_CIPHER_DES:
	case SSH_CIPHER_BLOWFISH:
		evplen = EVP_CIPHER_CTX_iv_length(&cc->evp);
		if (evplen == 0)
			return;
#ifdef USE_BUILTIN_RIJNDAEL
		if (c->evptype == evp_rijndael)
			ssh_rijndael_iv(&cc->evp, 1, iv, evplen);
		else
#endif
#ifndef OPENSSL_HAVE_EVPCTR
		if (c->evptype == evp_aes_128_ctr)
			ssh_aes_ctr_iv(&cc->evp, 1, iv, evplen);
		else
#endif
		memcpy(cc->evp.iv, iv, evplen);
		break;
	case SSH_CIPHER_3DES:
		ssh1_3des_iv(&cc->evp, 1, iv, 24);
		break;
	default:
		fatal("%s: bad cipher %d", __func__, c->number);
	}
}

int
cipher_get_keycontext(const CipherContext *cc, u_char *dat)
{
	const Cipher *c = cc->cipher;
	int plen = 0;

	if (c->evptype == EVP_rc4) {
		plen = EVP_X_STATE_LEN(cc->evp);
		if (dat == NULL)
			return (plen);
		memcpy(dat, EVP_X_STATE(cc->evp), plen);
	}
	return (plen);
}

void
cipher_set_keycontext(CipherContext *cc, u_char *dat)
{
	const Cipher *c = cc->cipher;
	int plen;

	if (c->evptype == EVP_rc4) {
		plen = EVP_X_STATE_LEN(cc->evp);
		memcpy(EVP_X_STATE(cc->evp), dat, plen);
	}
}
