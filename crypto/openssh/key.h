/*	$OpenBSD: key.h,v 1.12 2001/04/17 10:53:24 markus Exp $	*/

/*
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
#ifndef KEY_H
#define KEY_H

#include <openssl/rsa.h>
#include <openssl/dsa.h>

typedef struct Key Key;
enum types {
	KEY_RSA1,
	KEY_RSA,
	KEY_DSA,
	KEY_UNSPEC
};
enum fp_type {
	SSH_FP_SHA1,
	SSH_FP_MD5
};
enum fp_rep {
	SSH_FP_HEX,
	SSH_FP_BUBBLEBABBLE
};
struct Key {
	int	type;
	RSA	*rsa;
	DSA	*dsa;
};

Key	*key_new(int type);
Key	*key_new_private(int type);
void	key_free(Key *k);
int	key_equal(Key *a, Key *b);
char	*key_fingerprint(Key *k, enum fp_type dgst_type, enum fp_rep dgst_rep);
char	*key_type(Key *k);
int	key_write(Key *key, FILE *f);
int	key_read(Key *key, char **cpp);
u_int	key_size(Key *k);

Key	*key_generate(int type, u_int bits);
Key	*key_from_private(Key *k);
int	key_type_from_name(char *name);

Key	*key_from_blob(char *blob, int blen);
int	key_to_blob(Key *key, u_char **blobp, u_int *lenp);
char	*key_ssh_name(Key *k);
int	key_names_valid2(const char *names);

int
key_sign(
    Key *key,
    u_char **sigp, int *lenp,
    u_char *data, int datalen);

int
key_verify(
    Key *key,
    u_char *signature, int signaturelen,
    u_char *data, int datalen);

#endif
