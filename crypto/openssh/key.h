/* $OpenBSD: key.h,v 1.37 2013/05/19 02:42:42 djm Exp $ */

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

#include "buffer.h"
#include <openssl/rsa.h>
#include <openssl/dsa.h>
#ifdef OPENSSL_HAS_ECC
#include <openssl/ec.h>
#endif

typedef struct Key Key;
enum types {
	KEY_RSA1,
	KEY_RSA,
	KEY_DSA,
	KEY_ECDSA,
	KEY_RSA_CERT,
	KEY_DSA_CERT,
	KEY_ECDSA_CERT,
	KEY_RSA_CERT_V00,
	KEY_DSA_CERT_V00,
	KEY_UNSPEC
};
enum fp_type {
	SSH_FP_SHA1,
	SSH_FP_MD5,
	SSH_FP_SHA256
};
enum fp_rep {
	SSH_FP_HEX,
	SSH_FP_BUBBLEBABBLE,
	SSH_FP_RANDOMART
};

/* key is stored in external hardware */
#define KEY_FLAG_EXT		0x0001

#define CERT_MAX_PRINCIPALS	256
struct KeyCert {
	Buffer		 certblob; /* Kept around for use on wire */
	u_int		 type; /* SSH2_CERT_TYPE_USER or SSH2_CERT_TYPE_HOST */
	u_int64_t	 serial;
	char		*key_id;
	u_int		 nprincipals;
	char		**principals;
	u_int64_t	 valid_after, valid_before;
	Buffer		 critical;
	Buffer		 extensions;
	Key		*signature_key;
};

struct Key {
	int	 type;
	int	 flags;
	RSA	*rsa;
	DSA	*dsa;
	int	 ecdsa_nid;	/* NID of curve */
#ifdef OPENSSL_HAS_ECC
	EC_KEY	*ecdsa;
#else
	void	*ecdsa;
#endif
	struct KeyCert *cert;
};

Key		*key_new(int);
void		 key_add_private(Key *);
Key		*key_new_private(int);
void		 key_free(Key *);
Key		*key_demote(const Key *);
int		 key_equal_public(const Key *, const Key *);
int		 key_equal(const Key *, const Key *);
char		*key_fingerprint(const Key *, enum fp_type, enum fp_rep);
u_char		*key_fingerprint_raw(const Key *, enum fp_type, u_int *);
const char	*key_type(const Key *);
const char	*key_cert_type(const Key *);
int		 key_write(const Key *, FILE *);
int		 key_read(Key *, char **);
u_int		 key_size(const Key *);

Key	*key_generate(int, u_int);
Key	*key_from_private(const Key *);
int	 key_type_from_name(char *);
int	 key_is_cert(const Key *);
int	 key_type_plain(int);
int	 key_to_certified(Key *, int);
int	 key_drop_cert(Key *);
int	 key_certify(Key *, Key *);
void	 key_cert_copy(const Key *, struct Key *);
int	 key_cert_check_authority(const Key *, int, int, const char *,
	    const char **);
int	 key_cert_is_legacy(const Key *);

int		 key_ecdsa_nid_from_name(const char *);
int		 key_curve_name_to_nid(const char *);
const char	*key_curve_nid_to_name(int);
u_int		 key_curve_nid_to_bits(int);
int		 key_ecdsa_bits_to_nid(int);
#ifdef OPENSSL_HAS_ECC
int		 key_ecdsa_key_to_nid(EC_KEY *);
const EVP_MD	*key_ec_nid_to_evpmd(int nid);
int		 key_ec_validate_public(const EC_GROUP *, const EC_POINT *);
int		 key_ec_validate_private(const EC_KEY *);
#endif
char		*key_alg_list(void);

Key		*key_from_blob(const u_char *, u_int);
int		 key_to_blob(const Key *, u_char **, u_int *);
const char	*key_ssh_name(const Key *);
const char	*key_ssh_name_plain(const Key *);
int		 key_names_valid2(const char *);

int	 key_sign(const Key *, u_char **, u_int *, const u_char *, u_int);
int	 key_verify(const Key *, const u_char *, u_int, const u_char *, u_int);

int	 ssh_dss_sign(const Key *, u_char **, u_int *, const u_char *, u_int);
int	 ssh_dss_verify(const Key *, const u_char *, u_int, const u_char *, u_int);
int	 ssh_ecdsa_sign(const Key *, u_char **, u_int *, const u_char *, u_int);
int	 ssh_ecdsa_verify(const Key *, const u_char *, u_int, const u_char *, u_int);
int	 ssh_rsa_sign(const Key *, u_char **, u_int *, const u_char *, u_int);
int	 ssh_rsa_verify(const Key *, const u_char *, u_int, const u_char *, u_int);

#if defined(OPENSSL_HAS_ECC) && (defined(DEBUG_KEXECDH) || defined(DEBUG_PK))
void	key_dump_ec_point(const EC_GROUP *, const EC_POINT *);
void	key_dump_ec_key(const EC_KEY *);
#endif

#endif
