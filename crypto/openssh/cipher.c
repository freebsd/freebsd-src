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
 * Copyright (c) 1999,2000 Markus Friedl.  All rights reserved.
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
RCSID("$OpenBSD: cipher.c,v 1.30 2000/09/07 20:27:50 deraadt Exp $");

#include "ssh.h"
#include "cipher.h"
#include "xmalloc.h"

#include <openssl/md5.h>

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
void
SSH_3CBC_ENCRYPT(des_key_schedule ks1,
		 des_key_schedule ks2, des_cblock * iv2,
		 des_key_schedule ks3, des_cblock * iv3,
		 unsigned char *dest, unsigned char *src,
		 unsigned int len)
{
	des_cblock iv1;

	memcpy(&iv1, iv2, 8);

	des_cbc_encrypt(src, dest, len, ks1, &iv1, DES_ENCRYPT);
	memcpy(&iv1, dest + len - 8, 8);

	des_cbc_encrypt(dest, dest, len, ks2, iv2, DES_DECRYPT);
	memcpy(iv2, &iv1, 8);	/* Note how iv1 == iv2 on entry and exit. */

	des_cbc_encrypt(dest, dest, len, ks3, iv3, DES_ENCRYPT);
	memcpy(iv3, dest + len - 8, 8);
}

void
SSH_3CBC_DECRYPT(des_key_schedule ks1,
		 des_key_schedule ks2, des_cblock * iv2,
		 des_key_schedule ks3, des_cblock * iv3,
		 unsigned char *dest, unsigned char *src,
		 unsigned int len)
{
	des_cblock iv1;

	memcpy(&iv1, iv2, 8);

	des_cbc_encrypt(src, dest, len, ks3, iv3, DES_DECRYPT);
	memcpy(iv3, src + len - 8, 8);

	des_cbc_encrypt(dest, dest, len, ks2, iv2, DES_ENCRYPT);
	memcpy(iv2, dest + len - 8, 8);

	des_cbc_encrypt(dest, dest, len, ks1, &iv1, DES_DECRYPT);
	/* memcpy(&iv1, iv2, 8); */
	/* Note how iv1 == iv2 on entry and exit. */
}

/*
 * SSH1 uses a variation on Blowfish, all bytes must be swapped before
 * and after encryption/decryption. Thus the swap_bytes stuff (yuk).
 */
static void
swap_bytes(const unsigned char *src, unsigned char *dst_, int n)
{
	/* dst must be properly aligned. */
	u_int32_t *dst = (u_int32_t *) dst_;
	union {
		u_int32_t i;
		char c[4];
	} t;

	/* Process 8 bytes every lap. */
	for (n = n / 8; n > 0; n--) {
		t.c[3] = *src++;
		t.c[2] = *src++;
		t.c[1] = *src++;
		t.c[0] = *src++;
		*dst++ = t.i;

		t.c[3] = *src++;
		t.c[2] = *src++;
		t.c[1] = *src++;
		t.c[0] = *src++;
		*dst++ = t.i;
	}
}

/*
 * Names of all encryption algorithms.
 * These must match the numbers defined in cipher.h.
 */
static char *cipher_names[] =
{
	"none",
	"idea",
	"des",
	"3des",
	"tss",
	"rc4",
	"blowfish",
	"reserved",
	"blowfish-cbc",
	"3des-cbc",
	"arcfour",
	"cast128-cbc"
};

/*
 * Returns a bit mask indicating which ciphers are supported by this
 * implementation.  The bit mask has the corresponding bit set of each
 * supported cipher.
 */

unsigned int
cipher_mask1()
{
	unsigned int mask = 0;
	mask |= 1 << SSH_CIPHER_3DES;		/* Mandatory */
	mask |= 1 << SSH_CIPHER_BLOWFISH;
	return mask;
}
unsigned int
cipher_mask2()
{
	unsigned int mask = 0;
	mask |= 1 << SSH_CIPHER_BLOWFISH_CBC;
	mask |= 1 << SSH_CIPHER_3DES_CBC;
	mask |= 1 << SSH_CIPHER_ARCFOUR;
	mask |= 1 << SSH_CIPHER_CAST128_CBC;
	return mask;
}
unsigned int
cipher_mask()
{
	return cipher_mask1() | cipher_mask2();
}

/* Returns the name of the cipher. */

const char *
cipher_name(int cipher)
{
	if (cipher < 0 || cipher >= sizeof(cipher_names) / sizeof(cipher_names[0]) ||
	    cipher_names[cipher] == NULL)
		fatal("cipher_name: bad cipher name: %d", cipher);
	return cipher_names[cipher];
}

/* Returns 1 if the name of the ciphers are valid. */

#define	CIPHER_SEP	","
int
ciphers_valid(const char *names)
{
	char *ciphers, *cp;
	char *p;
	int i;

	if (names == NULL || strcmp(names, "") == 0)
		return 0;
	ciphers = cp = xstrdup(names);
	for ((p = strsep(&cp, CIPHER_SEP)); p && *p != '\0'; 
	     (p = strsep(&cp, CIPHER_SEP))) {
		i = cipher_number(p);
		if (i == -1 || !(cipher_mask2() & (1 << i))) {
			xfree(ciphers);
			return 0;
		}
	}
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
	int i;
	if (name == NULL)
		return -1;
	for (i = 0; i < sizeof(cipher_names) / sizeof(cipher_names[0]); i++)
		if (strcmp(cipher_names[i], name) == 0 &&
		    (cipher_mask() & (1 << i)))
			return i;
	return -1;
}

/*
 * Selects the cipher, and keys if by computing the MD5 checksum of the
 * passphrase and using the resulting 16 bytes as the key.
 */

void
cipher_set_key_string(CipherContext *context, int cipher, const char *passphrase)
{
	MD5_CTX md;
	unsigned char digest[16];

	MD5_Init(&md);
	MD5_Update(&md, (const unsigned char *) passphrase, strlen(passphrase));
	MD5_Final(digest, &md);

	cipher_set_key(context, cipher, digest, 16);

	memset(digest, 0, sizeof(digest));
	memset(&md, 0, sizeof(md));
}

/* Selects the cipher to use and sets the key. */

void
cipher_set_key(CipherContext *context, int cipher, const unsigned char *key,
    int keylen)
{
	unsigned char padded[32];

	/* Set cipher type. */
	context->type = cipher;

	/* Get 32 bytes of key data.  Pad if necessary.  (So that code
	   below does not need to worry about key size). */
	memset(padded, 0, sizeof(padded));
	memcpy(padded, key, keylen < sizeof(padded) ? keylen : sizeof(padded));

	/* Initialize the initialization vector. */
	switch (cipher) {
	case SSH_CIPHER_NONE:
		/*
		 * Has to stay for authfile saving of private key with no
		 * passphrase
		 */
		break;

	case SSH_CIPHER_3DES:
		/*
		 * Note: the least significant bit of each byte of key is
		 * parity, and must be ignored by the implementation.  16
		 * bytes of key are used (first and last keys are the same).
		 */
		if (keylen < 16)
			error("Key length %d is insufficient for 3DES.", keylen);
		des_set_key((void *) padded, context->u.des3.key1);
		des_set_key((void *) (padded + 8), context->u.des3.key2);
		if (keylen <= 16)
			des_set_key((void *) padded, context->u.des3.key3);
		else
			des_set_key((void *) (padded + 16), context->u.des3.key3);
		memset(context->u.des3.iv2, 0, sizeof(context->u.des3.iv2));
		memset(context->u.des3.iv3, 0, sizeof(context->u.des3.iv3));
		break;

	case SSH_CIPHER_BLOWFISH:
		if (keylen < 16)
			error("Key length %d is insufficient for blowfish.", keylen);
		BF_set_key(&context->u.bf.key, keylen, padded);
		memset(context->u.bf.iv, 0, 8);
		break;

	case SSH_CIPHER_3DES_CBC:
	case SSH_CIPHER_BLOWFISH_CBC:
	case SSH_CIPHER_ARCFOUR:
	case SSH_CIPHER_CAST128_CBC:
		fatal("cipher_set_key: illegal cipher: %s", cipher_name(cipher));
		break;

	default:
		fatal("cipher_set_key: unknown cipher: %s", cipher_name(cipher));
	}
	memset(padded, 0, sizeof(padded));
}

void
cipher_set_key_iv(CipherContext * context, int cipher,
    const unsigned char *key, int keylen,
    const unsigned char *iv, int ivlen)
{
	/* Set cipher type. */
	context->type = cipher;

	/* Initialize the initialization vector. */
	switch (cipher) {
	case SSH_CIPHER_NONE:
		break;

	case SSH_CIPHER_3DES:
	case SSH_CIPHER_BLOWFISH:
		fatal("cipher_set_key_iv: illegal cipher: %s", cipher_name(cipher));
		break;

	case SSH_CIPHER_3DES_CBC:
		if (keylen < 24)
			error("Key length %d is insufficient for 3des-cbc.", keylen);
		des_set_key((void *) key, context->u.des3.key1);
		des_set_key((void *) (key+8), context->u.des3.key2);
		des_set_key((void *) (key+16), context->u.des3.key3);
		if (ivlen < 8)
			error("IV length %d is insufficient for 3des-cbc.", ivlen);
		memcpy(context->u.des3.iv3, (char *)iv, 8);
		break;

	case SSH_CIPHER_BLOWFISH_CBC:
		if (keylen < 16)
			error("Key length %d is insufficient for blowfish.", keylen);
		if (ivlen < 8)
			error("IV length %d is insufficient for blowfish.", ivlen);
		BF_set_key(&context->u.bf.key, keylen, (unsigned char *)key);
		memcpy(context->u.bf.iv, (char *)iv, 8);
		break;

	case SSH_CIPHER_ARCFOUR:
		if (keylen < 16)
			error("Key length %d is insufficient for arcfour.", keylen);
		RC4_set_key(&context->u.rc4, keylen, (unsigned char *)key);
		break;

	case SSH_CIPHER_CAST128_CBC:
		if (keylen < 16)
			error("Key length %d is insufficient for cast128.", keylen);
		if (ivlen < 8)
			error("IV length %d is insufficient for cast128.", ivlen);
		CAST_set_key(&context->u.cast.key, keylen, (unsigned char *) key);
		memcpy(context->u.cast.iv, (char *)iv, 8);
		break;

	default:
		fatal("cipher_set_key: unknown cipher: %s", cipher_name(cipher));
	}
}

/* Encrypts data using the cipher. */

void
cipher_encrypt(CipherContext *context, unsigned char *dest,
	       const unsigned char *src, unsigned int len)
{
	if ((len & 7) != 0)
		fatal("cipher_encrypt: bad plaintext length %d", len);

	switch (context->type) {
	case SSH_CIPHER_NONE:
		memcpy(dest, src, len);
		break;

	case SSH_CIPHER_3DES:
		SSH_3CBC_ENCRYPT(context->u.des3.key1,
				 context->u.des3.key2, &context->u.des3.iv2,
				 context->u.des3.key3, &context->u.des3.iv3,
				 dest, (unsigned char *) src, len);
		break;

	case SSH_CIPHER_BLOWFISH:
		swap_bytes(src, dest, len);
		BF_cbc_encrypt(dest, dest, len,
			       &context->u.bf.key, context->u.bf.iv,
			       BF_ENCRYPT);
		swap_bytes(dest, dest, len);
		break;

	case SSH_CIPHER_BLOWFISH_CBC:
		BF_cbc_encrypt((void *)src, dest, len,
			       &context->u.bf.key, context->u.bf.iv,
			       BF_ENCRYPT);
		break;

	case SSH_CIPHER_3DES_CBC:
		des_ede3_cbc_encrypt(src, dest, len,
		    context->u.des3.key1, context->u.des3.key2,
		    context->u.des3.key3, &context->u.des3.iv3, DES_ENCRYPT);
		break;

	case SSH_CIPHER_ARCFOUR:
		RC4(&context->u.rc4, len, (unsigned char *)src, dest);
		break;

	case SSH_CIPHER_CAST128_CBC:
		CAST_cbc_encrypt(src, dest, len,
		    &context->u.cast.key, context->u.cast.iv, CAST_ENCRYPT);
		break;

	default:
		fatal("cipher_encrypt: unknown cipher: %s", cipher_name(context->type));
	}
}

/* Decrypts data using the cipher. */

void
cipher_decrypt(CipherContext *context, unsigned char *dest,
	       const unsigned char *src, unsigned int len)
{
	if ((len & 7) != 0)
		fatal("cipher_decrypt: bad ciphertext length %d", len);

	switch (context->type) {
	case SSH_CIPHER_NONE:
		memcpy(dest, src, len);
		break;

	case SSH_CIPHER_3DES:
		SSH_3CBC_DECRYPT(context->u.des3.key1,
				 context->u.des3.key2, &context->u.des3.iv2,
				 context->u.des3.key3, &context->u.des3.iv3,
				 dest, (unsigned char *) src, len);
		break;

	case SSH_CIPHER_BLOWFISH:
		swap_bytes(src, dest, len);
		BF_cbc_encrypt((void *) dest, dest, len,
			       &context->u.bf.key, context->u.bf.iv,
			       BF_DECRYPT);
		swap_bytes(dest, dest, len);
		break;

	case SSH_CIPHER_BLOWFISH_CBC:
		BF_cbc_encrypt((void *) src, dest, len,
			       &context->u.bf.key, context->u.bf.iv,
			       BF_DECRYPT);
		break;

	case SSH_CIPHER_3DES_CBC:
		des_ede3_cbc_encrypt(src, dest, len,
		    context->u.des3.key1, context->u.des3.key2,
		    context->u.des3.key3, &context->u.des3.iv3, DES_DECRYPT);
		break;

	case SSH_CIPHER_ARCFOUR:
		RC4(&context->u.rc4, len, (unsigned char *)src, dest);
		break;

	case SSH_CIPHER_CAST128_CBC:
		CAST_cbc_encrypt(src, dest, len,
		    &context->u.cast.key, context->u.cast.iv, CAST_DECRYPT);
		break;

	default:
		fatal("cipher_decrypt: unknown cipher: %s", cipher_name(context->type));
	}
}
