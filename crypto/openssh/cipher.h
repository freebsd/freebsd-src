/*	$OpenBSD: cipher.h,v 1.35 2004/07/28 09:40:29 markus Exp $	*/

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

#ifndef CIPHER_H
#define CIPHER_H

#include <openssl/evp.h>
/*
 * Cipher types for SSH-1.  New types can be added, but old types should not
 * be removed for compatibility.  The maximum allowed value is 31.
 */
#define SSH_CIPHER_SSH2		-3
#define SSH_CIPHER_INVALID	-2	/* No valid cipher selected. */
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

#define CIPHER_ENCRYPT		1
#define CIPHER_DECRYPT		0

typedef struct Cipher Cipher;
typedef struct CipherContext CipherContext;

struct Cipher;
struct CipherContext {
	int	plaintext;
	EVP_CIPHER_CTX evp;
	Cipher *cipher;
};

u_int	 cipher_mask_ssh1(int);
Cipher	*cipher_by_name(const char *);
Cipher	*cipher_by_number(int);
int	 cipher_number(const char *);
char	*cipher_name(int);
int	 ciphers_valid(const char *);
void	 cipher_init(CipherContext *, Cipher *, const u_char *, u_int,
    const u_char *, u_int, int);
void	 cipher_crypt(CipherContext *, u_char *, const u_char *, u_int);
void	 cipher_cleanup(CipherContext *);
void	 cipher_set_key_string(CipherContext *, Cipher *, const char *, int);
u_int	 cipher_blocksize(const Cipher *);
u_int	 cipher_keylen(const Cipher *);

u_int	 cipher_get_number(const Cipher *);
void	 cipher_get_keyiv(CipherContext *, u_char *, u_int);
void	 cipher_set_keyiv(CipherContext *, u_char *);
int	 cipher_get_keyiv_len(const CipherContext *);
int	 cipher_get_keycontext(const CipherContext *, u_char *);
void	 cipher_set_keycontext(CipherContext *, u_char *);
#endif				/* CIPHER_H */
