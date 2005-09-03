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
RCSID("$OpenBSD: cipher.c,v 1.77 2005/07/16 01:35:24 djm Exp $");

#include "xmalloc.h"
#include "log.h"
#include "cipher.h"

#include <openssl/md5.h>

/* compatibility with old or broken OpenSSL versions */
#include "openbsd-compat/openssl-compat.h"

extern const EVP_CIPHER *evp_ssh1_bf(void);
extern const EVP_CIPHER *evp_ssh1_3des(void);
extern void ssh1_3des_iv(EVP_CIPHER_CTX *, int, u_char *, int);
extern const EVP_CIPHER *evp_aes_128_ctr(void);
extern void ssh_aes_ctr_iv(EVP_CIPHER_CTX *, int, u_char *, u_int);

struct Cipher {
	char	*name;
	int	number;		/* for ssh1 only */
	u_int	block_size;
	u_int	key_len;
	u_int	discard_len;
	const EVP_CIPHER	*(*evptype)(void);
} ciphers[] = {
	{ "none",		SSH_CIPHER_NONE, 8, 0, 0, EVP_enc_null },
	{ "des",		SSH_CIPHER_DES, 8, 8, 0, EVP_des_cbc },
	{ "3des",		SSH_CIPHER_3DES, 8, 16, 0, evp_ssh1_3des },
	{ "blowfish",		SSH_CIPHER_BLOWFISH, 8, 32, 0, evp_ssh1_bf },

	{ "3des-cbc",		SSH_CIPHER_SSH2, 8, 24, 0, EVP_des_ede3_cbc },
	{ "blowfish-cbc",	SSH_CIPHER_SSH2, 8, 16, 0, EVP_bf_cbc },
	{ "cast128-cbc",	SSH_CIPHER_SSH2, 8, 16, 0, EVP_cast5_cbc },
	{ "arcfour",		SSH_CIPHER_SSH2, 8, 16, 0, EVP_rc4 },
	{ "arcfour128",		SSH_CIPHER_SSH2, 8, 16, 1536, EVP_rc4 },
	{ "arcfour256",		SSH_CIPHER_SSH2, 8, 32, 1536, EVP_rc4 },
	{ "aes128-cbc",		SSH_CIPHER_SSH2, 16, 16, 0, EVP_aes_128_cbc },
	{ "aes192-cbc",		SSH_CIPHER_SSH2, 16, 24, 0, EVP_aes_192_cbc },
	{ "aes256-cbc",		SSH_CIPHER_SSH2, 16, 32, 0, EVP_aes_256_cbc },
	{ "rijndael-cbc@lysator.liu.se",
				SSH_CIPHER_SSH2, 16, 32, 0, EVP_aes_256_cbc },
	{ "aes128-ctr",		SSH_CIPHER_SSH2, 16, 16, 0, evp_aes_128_ctr },
	{ "aes192-ctr",		SSH_CIPHER_SSH2, 16, 24, 0, evp_aes_128_ctr },
	{ "aes256-ctr",		SSH_CIPHER_SSH2, 16, 32, 0, evp_aes_128_ctr },
#ifdef USE_CIPHER_ACSS
	{ "acss@openssh.org",	SSH_CIPHER_SSH2, 16, 5, 0, EVP_acss },
#endif
	{ NULL,			SSH_CIPHER_INVALID, 0, 0, 0, NULL }
};

/*--*/

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
cipher_get_number(const Cipher *c)
{
	return (c->number);
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

Cipher *
cipher_by_name(const char *name)
{
	Cipher *c;
	for (c = ciphers; c->name != NULL; c++)
		if (strcmp(c->name, name) == 0)
			return c;
	return NULL;
}

Cipher *
cipher_by_number(int id)
{
	Cipher *c;
	for (c = ciphers; c->name != NULL; c++)
		if (c->number == id)
			return c;
	return NULL;
}

#define	CIPHER_SEP	","
int
ciphers_valid(const char *names)
{
	Cipher *c;
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
			xfree(cipher_list);
			return 0;
		} else {
			debug3("cipher ok: %s [%s]", p, names);
		}
	}
	debug3("ciphers ok: [%s]", names);
	xfree(cipher_list);
	return 1;
}

/*
 * Parses the name of the cipher.  Returns the number of the corresponding
 * cipher, or -1 on error.
 */

int
cipher_number(const char *name)
{
	Cipher *c;
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
	Cipher *c = cipher_by_number(id);
	return (c==NULL) ? "<unknown>" : c->name;
}

void
cipher_init(CipherContext *cc, Cipher *cipher,
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

	if (keylen < cipher->key_len)
		fatal("cipher_init: key length %d is insufficient for %s.",
		    keylen, cipher->name);
	if (iv != NULL && ivlen < cipher->block_size)
		fatal("cipher_init: iv length %d is insufficient for %s.",
		    ivlen, cipher->name);
	cc->cipher = cipher;

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
		memset(discard, 0, cipher->discard_len);
		xfree(junk);
		xfree(discard);
	}
}

void
cipher_crypt(CipherContext *cc, u_char *dest, const u_char *src, u_int len)
{
	if (len % cc->cipher->block_size)
		fatal("cipher_encrypt: bad plaintext length %d", len);
	if (EVP_Cipher(&cc->evp, dest, (u_char *)src, len) == 0)
		fatal("evp_crypt: EVP_Cipher failed");
}

void
cipher_cleanup(CipherContext *cc)
{
	if (EVP_CIPHER_CTX_cleanup(&cc->evp) == 0)
		error("cipher_cleanup: EVP_CIPHER_CTX_cleanup failed");
}

/*
 * Selects the cipher, and keys if by computing the MD5 checksum of the
 * passphrase and using the resulting 16 bytes as the key.
 */

void
cipher_set_key_string(CipherContext *cc, Cipher *cipher,
    const char *passphrase, int do_encrypt)
{
	MD5_CTX md;
	u_char digest[16];

	MD5_Init(&md);
	MD5_Update(&md, (const u_char *)passphrase, strlen(passphrase));
	MD5_Final(digest, &md);

	cipher_init(cc, cipher, digest, 16, NULL, 0, do_encrypt);

	memset(digest, 0, sizeof(digest));
	memset(&md, 0, sizeof(md));
}

/*
 * Exports an IV from the CipherContext required to export the key
 * state back from the unprivileged child to the privileged parent
 * process.
 */

int
cipher_get_keyiv_len(const CipherContext *cc)
{
	Cipher *c = cc->cipher;
	int ivlen;

	if (c->number == SSH_CIPHER_3DES)
		ivlen = 24;
	else
		ivlen = EVP_CIPHER_CTX_iv_length(&cc->evp);
	return (ivlen);
}

void
cipher_get_keyiv(CipherContext *cc, u_char *iv, u_int len)
{
	Cipher *c = cc->cipher;
	int evplen;

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
#if OPENSSL_VERSION_NUMBER < 0x00907000L
		if (c->evptype == evp_rijndael)
			ssh_rijndael_iv(&cc->evp, 0, iv, len);
		else
#endif
		if (c->evptype == evp_aes_128_ctr)
			ssh_aes_ctr_iv(&cc->evp, 0, iv, len);
		else
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
	Cipher *c = cc->cipher;
	int evplen = 0;

	switch (c->number) {
	case SSH_CIPHER_SSH2:
	case SSH_CIPHER_DES:
	case SSH_CIPHER_BLOWFISH:
		evplen = EVP_CIPHER_CTX_iv_length(&cc->evp);
		if (evplen == 0)
			return;
#if OPENSSL_VERSION_NUMBER < 0x00907000L
		if (c->evptype == evp_rijndael)
			ssh_rijndael_iv(&cc->evp, 1, iv, evplen);
		else
#endif
		if (c->evptype == evp_aes_128_ctr)
			ssh_aes_ctr_iv(&cc->evp, 1, iv, evplen);
		else
			memcpy(cc->evp.iv, iv, evplen);
		break;
	case SSH_CIPHER_3DES:
		ssh1_3des_iv(&cc->evp, 1, iv, 24);
		break;
	default:
		fatal("%s: bad cipher %d", __func__, c->number);
	}
}

#if OPENSSL_VERSION_NUMBER < 0x00907000L
#define EVP_X_STATE(evp)	&(evp).c
#define EVP_X_STATE_LEN(evp)	sizeof((evp).c)
#else
#define EVP_X_STATE(evp)	(evp).cipher_data
#define EVP_X_STATE_LEN(evp)	(evp).cipher->ctx_size
#endif

int
cipher_get_keycontext(const CipherContext *cc, u_char *dat)
{
	Cipher *c = cc->cipher;
	int plen = 0;

	if (c->evptype == EVP_rc4 || c->evptype == EVP_acss) {
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
	Cipher *c = cc->cipher;
	int plen;

	if (c->evptype == EVP_rc4 || c->evptype == EVP_acss) {
		plen = EVP_X_STATE_LEN(cc->evp);
		memcpy(EVP_X_STATE(cc->evp), dat, plen);
	}
}
