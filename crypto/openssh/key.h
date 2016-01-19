/* $OpenBSD: key.h,v 1.47 2015/01/28 22:36:00 djm Exp $ */

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

#include "sshkey.h"

typedef struct sshkey Key;

#define types sshkey_types
#define fp_type sshkey_fp_type
#define fp_rep sshkey_fp_rep

#ifndef SSH_KEY_NO_DEFINE
#define key_new			sshkey_new
#define key_free		sshkey_free
#define key_equal_public	sshkey_equal_public
#define key_equal		sshkey_equal
#define key_type		sshkey_type
#define key_cert_type		sshkey_cert_type
#define key_ssh_name		sshkey_ssh_name
#define key_ssh_name_plain	sshkey_ssh_name_plain
#define key_type_from_name	sshkey_type_from_name
#define key_ecdsa_nid_from_name	sshkey_ecdsa_nid_from_name
#define key_type_is_cert	sshkey_type_is_cert
#define key_size		sshkey_size
#define key_ecdsa_bits_to_nid	sshkey_ecdsa_bits_to_nid
#define key_ecdsa_key_to_nid	sshkey_ecdsa_key_to_nid
#define key_is_cert		sshkey_is_cert
#define key_type_plain		sshkey_type_plain
#define key_cert_is_legacy	sshkey_cert_is_legacy
#define key_curve_name_to_nid	sshkey_curve_name_to_nid
#define key_curve_nid_to_bits	sshkey_curve_nid_to_bits
#define key_curve_nid_to_name	sshkey_curve_nid_to_name
#define key_ec_nid_to_hash_alg	sshkey_ec_nid_to_hash_alg
#define key_dump_ec_point	sshkey_dump_ec_point
#define key_dump_ec_key		sshkey_dump_ec_key
#endif

void	 key_add_private(Key *);
Key	*key_new_private(int);
void	 key_free(Key *);
Key	*key_demote(const Key *);
int	 key_write(const Key *, FILE *);
int	 key_read(Key *, char **);

Key	*key_generate(int, u_int);
Key	*key_from_private(const Key *);
int	 key_to_certified(Key *, int);
int	 key_drop_cert(Key *);
int	 key_certify(Key *, Key *);
void	 key_cert_copy(const Key *, Key *);
int	 key_cert_check_authority(const Key *, int, int, const char *,
	    const char **);
char	*key_alg_list(int, int);

#if defined(WITH_OPENSSL) && defined(OPENSSL_HAS_ECC)
int	 key_ec_validate_public(const EC_GROUP *, const EC_POINT *);
int	 key_ec_validate_private(const EC_KEY *);
#endif /* defined(WITH_OPENSSL) && defined(OPENSSL_HAS_ECC) */

Key	*key_from_blob(const u_char *, u_int);
int	 key_to_blob(const Key *, u_char **, u_int *);

int	 key_sign(const Key *, u_char **, u_int *, const u_char *, u_int);
int	 key_verify(const Key *, const u_char *, u_int, const u_char *, u_int);

void     key_private_serialize(const Key *, struct sshbuf *);
Key	*key_private_deserialize(struct sshbuf *);

/* authfile.c */
int	 key_save_private(Key *, const char *, const char *, const char *,
    int, const char *, int);
int	 key_load_file(int, const char *, struct sshbuf *);
Key	*key_load_cert(const char *);
Key	*key_load_public(const char *, char **);
Key	*key_load_private(const char *, const char *, char **);
Key	*key_load_private_cert(int, const char *, const char *, int *);
Key	*key_load_private_type(int, const char *, const char *, char **, int *);
int	 key_perm_ok(int, const char *);

#endif
