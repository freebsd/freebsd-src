/*	$OpenBSD: key.h,v 1.23 2003/11/10 16:23:41 jakob Exp $	*/

/*
 * Copyright (c) 2000, 2001 Markus Friedl.  All rights reserved.
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

/* key is stored in external hardware */
#define KEY_FLAG_EXT		0x0001

struct Key {
	int	 type;
	int	 flags;
	RSA	*rsa;
	DSA	*dsa;
};

Key		*key_new(int);
Key		*key_new_private(int);
void		 key_free(Key *);
Key		*key_demote(const Key *);
int		 key_equal(const Key *, const Key *);
char		*key_fingerprint(const Key *, enum fp_type, enum fp_rep);
u_char		*key_fingerprint_raw(const Key *, enum fp_type, u_int *);
const char	*key_type(const Key *);
int		 key_write(const Key *, FILE *);
int		 key_read(Key *, char **);
u_int		 key_size(const Key *);

Key	*key_generate(int, u_int);
Key	*key_from_private(const Key *);
int	 key_type_from_name(char *);

Key		*key_from_blob(const u_char *, u_int);
int		 key_to_blob(const Key *, u_char **, u_int *);
const char	*key_ssh_name(const Key *);
int		 key_names_valid2(const char *);

int	 key_sign(const Key *, u_char **, u_int *, const u_char *, u_int);
int	 key_verify(const Key *, const u_char *, u_int, const u_char *, u_int);

int	 ssh_dss_sign(const Key *, u_char **, u_int *, const u_char *, u_int);
int	 ssh_dss_verify(const Key *, const u_char *, u_int, const u_char *, u_int);
int	 ssh_rsa_sign(const Key *, u_char **, u_int *, const u_char *, u_int);
int	 ssh_rsa_verify(const Key *, const u_char *, u_int, const u_char *, u_int);

#endif
