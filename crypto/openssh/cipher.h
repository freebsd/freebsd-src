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
 */

/* RCSID("$OpenBSD: cipher.h,v 1.19 2000/09/07 20:27:50 deraadt Exp $"); */
/* $FreeBSD$ */

#ifndef CIPHER_H
#define CIPHER_H

#include <openssl/des.h>
#include <openssl/blowfish.h>
#include <openssl/rc4.h>
#include <openssl/cast.h>

/* Cipher types.  New types can be added, but old types should not be removed
   for compatibility.  The maximum allowed value is 31. */
#define SSH_CIPHER_ILLEGAL	-2	/* No valid cipher selected. */
#define SSH_CIPHER_NOT_SET	-1	/* None selected (invalid number). */
#define SSH_CIPHER_NONE		0	/* no encryption */
#define SSH_CIPHER_IDEA		1	/* IDEA CFB */
#define SSH_CIPHER_DES		2	/* DES CBC */
#define SSH_CIPHER_3DES		3	/* 3DES CBC */
#define SSH_CIPHER_BROKEN_TSS	4	/* TRI's Simple Stream encryption CBC */
#define SSH_CIPHER_BROKEN_RC4	5	/* Alleged RC4 */
#define SSH_CIPHER_BLOWFISH	6
#define SSH_CIPHER_RESERVED	7

/* these ciphers are used in SSH2: */
#define SSH_CIPHER_BLOWFISH_CBC	8
#define SSH_CIPHER_3DES_CBC	9
#define SSH_CIPHER_ARCFOUR	10	/* Alleged RC4 */
#define SSH_CIPHER_CAST128_CBC	11

typedef struct {
	unsigned int type;
	union {
		struct {
			des_key_schedule key1;
			des_key_schedule key2;
			des_cblock iv2;
			des_key_schedule key3;
			des_cblock iv3;
		}       des3;
		struct {
			struct bf_key_st key;
			unsigned char iv[8];
		}       bf;
		struct {
			CAST_KEY key;
			unsigned char iv[8];
		} cast;
		RC4_KEY rc4;
	}       u;
}       CipherContext;
/*
 * Returns a bit mask indicating which ciphers are supported by this
 * implementation.  The bit mask has the corresponding bit set of each
 * supported cipher.
 */
unsigned int cipher_mask();
unsigned int cipher_mask1();
unsigned int cipher_mask2();

/* Returns the name of the cipher. */
const char *cipher_name(int cipher);

/*
 * Parses the name of the cipher.  Returns the number of the corresponding
 * cipher, or -1 on error.
 */
int     cipher_number(const char *name);

/* returns 1 if all ciphers are supported (ssh2 only) */
int     ciphers_valid(const char *names);

/*
 * Selects the cipher to use and sets the key.  If for_encryption is true,
 * the key is setup for encryption; otherwise it is setup for decryption.
 */
void
cipher_set_key(CipherContext * context, int cipher,
    const unsigned char *key, int keylen);
void
cipher_set_key_iv(CipherContext * context, int cipher,
    const unsigned char *key, int keylen,
    const unsigned char *iv, int ivlen);

/*
 * Sets key for the cipher by computing the MD5 checksum of the passphrase,
 * and using the resulting 16 bytes as the key.
 */
void
cipher_set_key_string(CipherContext * context, int cipher,
    const char *passphrase);

/* Encrypts data using the cipher. */
void
cipher_encrypt(CipherContext * context, unsigned char *dest,
    const unsigned char *src, unsigned int len);

/* Decrypts data using the cipher. */
void
cipher_decrypt(CipherContext * context, unsigned char *dest,
    const unsigned char *src, unsigned int len);

#endif				/* CIPHER_H */
