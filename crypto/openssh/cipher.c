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
RCSID("$OpenBSD: cipher.c,v 1.52 2002/02/18 13:05:32 markus Exp $");
RCSID("$FreeBSD$");

#include "xmalloc.h"
#include "log.h"
#include "cipher.h"

#include <openssl/md5.h>
#include "rijndael.h"

static EVP_CIPHER *evp_ssh1_3des(void);
static EVP_CIPHER *evp_ssh1_bf(void);
static EVP_CIPHER *evp_rijndael(void);

struct Cipher {
	char	*name;
	int	number;		/* for ssh1 only */
	u_int	block_size;
	u_int	key_len;
	EVP_CIPHER	*(*evptype)(void);
} ciphers[] = {
	{ "none", 		SSH_CIPHER_NONE, 8, 0, EVP_enc_null },
	{ "des", 		SSH_CIPHER_DES, 8, 8, EVP_des_cbc },
	{ "3des", 		SSH_CIPHER_3DES, 8, 16, evp_ssh1_3des },
	{ "blowfish", 		SSH_CIPHER_BLOWFISH, 8, 32, evp_ssh1_bf },

	{ "3des-cbc", 		SSH_CIPHER_SSH2, 8, 24, EVP_des_ede3_cbc },
	{ "blowfish-cbc", 	SSH_CIPHER_SSH2, 8, 16, EVP_bf_cbc },
	{ "cast128-cbc", 	SSH_CIPHER_SSH2, 8, 16, EVP_cast5_cbc },
	{ "arcfour", 		SSH_CIPHER_SSH2, 8, 16, EVP_rc4 },
	{ "aes128-cbc", 	SSH_CIPHER_SSH2, 16, 16, evp_rijndael },
	{ "aes192-cbc", 	SSH_CIPHER_SSH2, 16, 24, evp_rijndael },
	{ "aes256-cbc", 	SSH_CIPHER_SSH2, 16, 32, evp_rijndael },

	{ NULL,			SSH_CIPHER_ILLEGAL, 0, 0, NULL }
};

/*--*/

u_int	
cipher_blocksize(Cipher *c)
{
	return (c->block_size);
}
u_int	
cipher_keylen(Cipher *c)
{
	return (c->key_len);
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
		if (strcasecmp(c->name, name) == 0)
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
	char *ciphers, *cp;
	char *p;

	if (names == NULL || strcmp(names, "") == 0)
		return 0;
	ciphers = cp = xstrdup(names);
	for ((p = strsep(&cp, CIPHER_SEP)); p && *p != '\0';
	    (p = strsep(&cp, CIPHER_SEP))) {
		c = cipher_by_name(p);
		if (c == NULL || c->number != SSH_CIPHER_SSH2) {
			debug("bad cipher %s [%s]", p, names);
			xfree(ciphers);
			return 0;
		} else {
			debug3("cipher ok: %s [%s]", p, names);
		}
	}
	debug3("ciphers ok: [%s]", names);
	xfree(ciphers);
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
	c = cipher_by_name(name);
	return (c==NULL) ? -1 : c->number;
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
    int encrypt)
{
	static int dowarn = 1;
	const EVP_CIPHER *type;
	int klen;

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
	if (EVP_CipherInit(&cc->evp, type, NULL, (u_char *)iv,
	    (encrypt == CIPHER_ENCRYPT)) == 0)
		fatal("cipher_init: EVP_CipherInit failed for %s",
		    cipher->name);
	klen = EVP_CIPHER_CTX_key_length(&cc->evp);
	if (klen > 0 && keylen != klen) {
		debug("cipher_init: set keylen (%d -> %d)", klen, keylen);
		if (EVP_CIPHER_CTX_set_key_length(&cc->evp, keylen) == 0)
			fatal("cipher_init: set keylen failed (%d -> %d)",
			    klen, keylen);
	}
	if (EVP_CipherInit(&cc->evp, NULL, (u_char *)key, NULL, -1) == 0)
		fatal("cipher_init: EVP_CipherInit: set key failed for %s",
		    cipher->name);
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
    const char *passphrase, int encrypt)
{
	MD5_CTX md;
	u_char digest[16];

	MD5_Init(&md);
	MD5_Update(&md, (const u_char *)passphrase, strlen(passphrase));
	MD5_Final(digest, &md);

	cipher_init(cc, cipher, digest, 16, NULL, 0, encrypt);

	memset(digest, 0, sizeof(digest));
	memset(&md, 0, sizeof(md));
}

/* Implementations for other non-EVP ciphers */

/*
 * This is used by SSH1:
 *
 * What kind of triple DES are these 2 routines?
 *
 * Why is there a redundant initialization vector?
 *
 * If only iv3 was used, then, this would till effect have been
 * outer-cbc. However, there is also a private iv1 == iv2 which
 * perhaps makes differential analysis easier. On the other hand, the
 * private iv1 probably makes the CRC-32 attack ineffective. This is a
 * result of that there is no longer any known iv1 to use when
 * choosing the X block.
 */
struct ssh1_3des_ctx
{
	EVP_CIPHER_CTX	k1, k2, k3;
};
static int
ssh1_3des_init(EVP_CIPHER_CTX *ctx, const u_char *key, const u_char *iv,
    int enc)
{
	struct ssh1_3des_ctx *c;
	u_char *k1, *k2, *k3;

	if ((c = EVP_CIPHER_CTX_get_app_data(ctx)) == NULL) {
		c = xmalloc(sizeof(*c));
		EVP_CIPHER_CTX_set_app_data(ctx, c);
	}
	if (key == NULL)
		return (1);
	if (enc == -1)
		enc = ctx->encrypt;
	k1 = k2 = k3 = (u_char *) key;
	k2 += 8;
	if (EVP_CIPHER_CTX_key_length(ctx) >= 16+8) {
		if (enc)
			k3 += 16;
		else
			k1 += 16;
	}
	EVP_CIPHER_CTX_init(&c->k1);
	EVP_CIPHER_CTX_init(&c->k2);
	EVP_CIPHER_CTX_init(&c->k3);
	if (EVP_CipherInit(&c->k1, EVP_des_cbc(), k1, NULL, enc) == 0 ||
	    EVP_CipherInit(&c->k2, EVP_des_cbc(), k2, NULL, !enc) == 0 ||
	    EVP_CipherInit(&c->k3, EVP_des_cbc(), k3, NULL, enc) == 0) {
		memset(c, 0, sizeof(*c));
		xfree(c);
		EVP_CIPHER_CTX_set_app_data(ctx, NULL);
		return (0);
	}
	return (1);
}
static int
ssh1_3des_cbc(EVP_CIPHER_CTX *ctx, u_char *dest, const u_char *src, u_int len)
{
	struct ssh1_3des_ctx *c;

	if ((c = EVP_CIPHER_CTX_get_app_data(ctx)) == NULL) {
		error("ssh1_3des_cbc: no context");
		return (0);
	}
	if (EVP_Cipher(&c->k1, dest, (u_char *)src, len) == 0 ||
	    EVP_Cipher(&c->k2, dest, dest, len) == 0 ||
	    EVP_Cipher(&c->k3, dest, dest, len) == 0)
		return (0);
	return (1);
}
static int
ssh1_3des_cleanup(EVP_CIPHER_CTX *ctx)
{
	struct ssh1_3des_ctx *c;

	if ((c = EVP_CIPHER_CTX_get_app_data(ctx)) != NULL) {
		memset(c, 0, sizeof(*c));
		xfree(c);
		EVP_CIPHER_CTX_set_app_data(ctx, NULL);
	}
	return (1);
}
static EVP_CIPHER *
evp_ssh1_3des(void)
{
	static EVP_CIPHER ssh1_3des;

	memset(&ssh1_3des, 0, sizeof(EVP_CIPHER));
	ssh1_3des.nid = NID_undef;
	ssh1_3des.block_size = 8;
	ssh1_3des.iv_len = 0;
	ssh1_3des.key_len = 16;
	ssh1_3des.init = ssh1_3des_init;
	ssh1_3des.cleanup = ssh1_3des_cleanup;
	ssh1_3des.do_cipher = ssh1_3des_cbc;
	ssh1_3des.flags = EVP_CIPH_CBC_MODE | EVP_CIPH_VARIABLE_LENGTH;
	return (&ssh1_3des);
}

/*
 * SSH1 uses a variation on Blowfish, all bytes must be swapped before
 * and after encryption/decryption. Thus the swap_bytes stuff (yuk).
 */
static void
swap_bytes(const u_char *src, u_char *dst, int n)
{
	u_char c[4];

	/* Process 4 bytes every lap. */
	for (n = n / 4; n > 0; n--) {
		c[3] = *src++;
		c[2] = *src++;
		c[1] = *src++;
		c[0] = *src++;

		*dst++ = c[0];
		*dst++ = c[1];
		*dst++ = c[2];
		*dst++ = c[3];
	}
}
static int (*orig_bf)(EVP_CIPHER_CTX *, u_char *, const u_char *, u_int) = NULL;
static int
bf_ssh1_cipher(EVP_CIPHER_CTX *ctx, u_char *out, const u_char *in, u_int len)
{
	int ret;

	swap_bytes(in, out, len);
	ret = (*orig_bf)(ctx, out, out, len);
	swap_bytes(out, out, len);
	return (ret);
}
static EVP_CIPHER *
evp_ssh1_bf(void)
{
	static EVP_CIPHER ssh1_bf;

	memcpy(&ssh1_bf, EVP_bf_cbc(), sizeof(EVP_CIPHER));
	orig_bf = ssh1_bf.do_cipher;
	ssh1_bf.nid = NID_undef;
	ssh1_bf.do_cipher = bf_ssh1_cipher;
	ssh1_bf.key_len = 32;
	return (&ssh1_bf);
}

/* RIJNDAEL */
#define RIJNDAEL_BLOCKSIZE 16
struct ssh_rijndael_ctx
{
	rijndael_ctx	r_ctx;
	u_char		r_iv[RIJNDAEL_BLOCKSIZE];
};

static int
ssh_rijndael_init(EVP_CIPHER_CTX *ctx, const u_char *key, const u_char *iv,
    int enc)
{
	struct ssh_rijndael_ctx *c;

	if ((c = EVP_CIPHER_CTX_get_app_data(ctx)) == NULL) {
		c = xmalloc(sizeof(*c));
		EVP_CIPHER_CTX_set_app_data(ctx, c);
	}
	if (key != NULL) {
		if (enc == -1)
			enc = ctx->encrypt;
		rijndael_set_key(&c->r_ctx, (u_char *)key,
		    8*EVP_CIPHER_CTX_key_length(ctx), enc);
	}
	if (iv != NULL)
		memcpy(c->r_iv, iv, RIJNDAEL_BLOCKSIZE);
	return (1);
}
static int
ssh_rijndael_cbc(EVP_CIPHER_CTX *ctx, u_char *dest, const u_char *src,
    u_int len)
{
	struct ssh_rijndael_ctx *c;
	u_char buf[RIJNDAEL_BLOCKSIZE];
	u_char *cprev, *cnow, *plain, *ivp;
	int i, j, blocks = len / RIJNDAEL_BLOCKSIZE;

	if (len == 0)
		return (1);
	if (len % RIJNDAEL_BLOCKSIZE)
		fatal("ssh_rijndael_cbc: bad len %d", len);
	if ((c = EVP_CIPHER_CTX_get_app_data(ctx)) == NULL) {
		error("ssh_rijndael_cbc: no context");
		return (0);
	}
	if (ctx->encrypt) {
		cnow  = dest;
		plain = (u_char *)src;
		cprev = c->r_iv;
		for (i = 0; i < blocks; i++, plain+=RIJNDAEL_BLOCKSIZE,
		    cnow+=RIJNDAEL_BLOCKSIZE) {
			for (j = 0; j < RIJNDAEL_BLOCKSIZE; j++)
				buf[j] = plain[j] ^ cprev[j];
			rijndael_encrypt(&c->r_ctx, buf, cnow);
			cprev = cnow;
		}
		memcpy(c->r_iv, cprev, RIJNDAEL_BLOCKSIZE);
	} else {
		cnow  = (u_char *) (src+len-RIJNDAEL_BLOCKSIZE);
		plain = dest+len-RIJNDAEL_BLOCKSIZE;

		memcpy(buf, cnow, RIJNDAEL_BLOCKSIZE);
		for (i = blocks; i > 0; i--, cnow-=RIJNDAEL_BLOCKSIZE,
		    plain-=RIJNDAEL_BLOCKSIZE) {
			rijndael_decrypt(&c->r_ctx, cnow, plain);
			ivp = (i == 1) ? c->r_iv : cnow-RIJNDAEL_BLOCKSIZE;
			for (j = 0; j < RIJNDAEL_BLOCKSIZE; j++)
				plain[j] ^= ivp[j];
		}
		memcpy(c->r_iv, buf, RIJNDAEL_BLOCKSIZE);
	}
	return (1);
}
static int
ssh_rijndael_cleanup(EVP_CIPHER_CTX *ctx)
{
	struct ssh_rijndael_ctx *c;

	if ((c = EVP_CIPHER_CTX_get_app_data(ctx)) != NULL) {
		memset(c, 0, sizeof(*c));
		xfree(c);
		EVP_CIPHER_CTX_set_app_data(ctx, NULL);
	}
	return (1);
}
static EVP_CIPHER *
evp_rijndael(void)
{
	static EVP_CIPHER rijndal_cbc;

	memset(&rijndal_cbc, 0, sizeof(EVP_CIPHER));
	rijndal_cbc.nid = NID_undef;
	rijndal_cbc.block_size = RIJNDAEL_BLOCKSIZE;
	rijndal_cbc.iv_len = RIJNDAEL_BLOCKSIZE;
	rijndal_cbc.key_len = 16;
	rijndal_cbc.init = ssh_rijndael_init;
	rijndal_cbc.cleanup = ssh_rijndael_cleanup;
	rijndal_cbc.do_cipher = ssh_rijndael_cbc;
	rijndal_cbc.flags = EVP_CIPH_CBC_MODE | EVP_CIPH_VARIABLE_LENGTH |
	    EVP_CIPH_ALWAYS_CALL_INIT;
	return (&rijndal_cbc);
}
