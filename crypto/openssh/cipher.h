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

/* RCSID("$OpenBSD: cipher.h,v 1.25 2000/12/19 23:17:56 markus Exp $"); */
/* RCSID("$FreeBSD$"); */

#ifndef CIPHER_H
#define CIPHER_H

#include <openssl/des.h>
#include <openssl/blowfish.h>
#include <openssl/rc4.h>
#include <openssl/cast.h>
#include "rijndael.h"
/*
 * Cipher types for SSH-1.  New types can be added, but old types should not
 * be removed for compatibility.  The maximum allowed value is 31.
 */
#define SSH_CIPHER_SSH2		-3
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
#define SSH_CIPHER_MAX		31

typedef struct Cipher Cipher;
typedef struct CipherContext CipherContext;

struct CipherContext {
	union {
		struct {
			des_key_schedule key;
			des_cblock iv;
		}	des;
		struct {
			des_key_schedule key1;
			des_key_schedule key2;
			des_cblock iv2;
			des_key_schedule key3;
			des_cblock iv3;
		}       des3;
		struct {
			struct bf_key_st key;
			u_char iv[8];
		}       bf;
		struct {
			CAST_KEY key;
			u_char iv[8];
		} cast;
		struct {
			u4byte iv[4];
			rijndael_ctx enc;
			rijndael_ctx dec;
		} rijndael;
		RC4_KEY rc4;
	}       u;
	Cipher *cipher;
};
struct Cipher {
	char	*name;
	int	number;		/* for ssh1 only */
	u_int	block_size;
	u_int	key_len;
	void	(*setkey)(CipherContext *, const u_char *, u_int);
	void	(*setiv)(CipherContext *, const u_char *, u_int);
	void	(*encrypt)(CipherContext *, u_char *, const u_char *, u_int);
	void	(*decrypt)(CipherContext *, u_char *, const u_char *, u_int);
};

u_int cipher_mask_ssh1(int client);
Cipher *cipher_by_name(const char *name);
Cipher *cipher_by_number(int id);
int cipher_number(const char *name);
char *cipher_name(int id);
int ciphers_valid(const char *names);
void cipher_init(CipherContext *, Cipher *, const u_char *, u_int, const u_char *, u_int);
void cipher_encrypt(CipherContext *context, u_char *dest, const u_char *src, u_int len);
void cipher_decrypt(CipherContext *context, u_char *dest, const u_char *src, u_int len);
void cipher_set_key_string(CipherContext *context, Cipher *cipher, const char *passphrase);

#endif				/* CIPHER_H */
