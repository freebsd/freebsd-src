/*
 * 
 * cipher.h
 * 
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * 
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * 
 * Created: Wed Apr 19 16:50:42 1995 ylo
 * 
 */

/* RCSID("$Id: cipher.h,v 1.10 1999/11/24 19:53:46 markus Exp $"); */

#ifndef CIPHER_H
#define CIPHER_H

#include <ssl/des.h>
#include <ssl/blowfish.h>

/* Cipher types.  New types can be added, but old types should not be removed
   for compatibility.  The maximum allowed value is 31. */
#define SSH_CIPHER_NOT_SET	-1	/* None selected (invalid number). */
#define SSH_CIPHER_NONE		0	/* no encryption */
#define SSH_CIPHER_IDEA		1	/* IDEA CFB */
#define SSH_CIPHER_DES		2	/* DES CBC */
#define SSH_CIPHER_3DES		3	/* 3DES CBC */
#define SSH_CIPHER_BROKEN_TSS	4	/* TRI's Simple Stream encryption CBC */
#define SSH_CIPHER_BROKEN_RC4	5	/* Alleged RC4 */
#define SSH_CIPHER_BLOWFISH	6

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
	}       u;
}       CipherContext;
/*
 * Returns a bit mask indicating which ciphers are supported by this
 * implementation.  The bit mask has the corresponding bit set of each
 * supported cipher.
 */
unsigned int cipher_mask();

/* Returns the name of the cipher. */
const char *cipher_name(int cipher);

/*
 * Parses the name of the cipher.  Returns the number of the corresponding
 * cipher, or -1 on error.
 */
int     cipher_number(const char *name);

/*
 * Selects the cipher to use and sets the key.  If for_encryption is true,
 * the key is setup for encryption; otherwise it is setup for decryption.
 */
void 
cipher_set_key(CipherContext * context, int cipher,
    const unsigned char *key, int keylen, int for_encryption);

/*
 * Sets key for the cipher by computing the MD5 checksum of the passphrase,
 * and using the resulting 16 bytes as the key.
 */
void 
cipher_set_key_string(CipherContext * context, int cipher,
    const char *passphrase, int for_encryption);

/* Encrypts data using the cipher. */
void 
cipher_encrypt(CipherContext * context, unsigned char *dest,
    const unsigned char *src, unsigned int len);

/* Decrypts data using the cipher. */
void 
cipher_decrypt(CipherContext * context, unsigned char *dest,
    const unsigned char *src, unsigned int len);

/*
 * If and CRC-32 attack is detected this function is called. Defaults to
 * fatal, changed to packet_disconnect in sshd and ssh.
 */
extern void (*cipher_attack_detected) (const char *fmt, ...);

#endif				/* CIPHER_H */
