/* $OpenBSD: kex.h,v 1.119 2023/08/28 03:28:43 djm Exp $ */

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
#ifndef KEX_H
#define KEX_H

#include "mac.h"
#include "crypto_api.h"

#ifdef WITH_OPENSSL
# include <openssl/bn.h>
# include <openssl/dh.h>
# include <openssl/ecdsa.h>
# ifdef OPENSSL_HAS_ECC
#  include <openssl/ec.h>
# else /* OPENSSL_HAS_ECC */
#  define EC_KEY	void
#  define EC_GROUP	void
#  define EC_POINT	void
# endif /* OPENSSL_HAS_ECC */
#else /* WITH_OPENSSL */
# define DH		void
# define BIGNUM		void
# define EC_KEY		void
# define EC_GROUP	void
# define EC_POINT	void
#endif /* WITH_OPENSSL */

#define KEX_COOKIE_LEN	16

#define	KEX_DH1				"diffie-hellman-group1-sha1"
#define	KEX_DH14_SHA1			"diffie-hellman-group14-sha1"
#define	KEX_DH14_SHA256			"diffie-hellman-group14-sha256"
#define	KEX_DH16_SHA512			"diffie-hellman-group16-sha512"
#define	KEX_DH18_SHA512			"diffie-hellman-group18-sha512"
#define	KEX_DHGEX_SHA1			"diffie-hellman-group-exchange-sha1"
#define	KEX_DHGEX_SHA256		"diffie-hellman-group-exchange-sha256"
#define	KEX_ECDH_SHA2_NISTP256		"ecdh-sha2-nistp256"
#define	KEX_ECDH_SHA2_NISTP384		"ecdh-sha2-nistp384"
#define	KEX_ECDH_SHA2_NISTP521		"ecdh-sha2-nistp521"
#define	KEX_CURVE25519_SHA256		"curve25519-sha256"
#define	KEX_CURVE25519_SHA256_OLD	"curve25519-sha256@libssh.org"
#define	KEX_SNTRUP761X25519_SHA512	"sntrup761x25519-sha512@openssh.com"

#define COMP_NONE	0
/* pre-auth compression (COMP_ZLIB) is only supported in the client */
#define COMP_ZLIB	1
#define COMP_DELAYED	2

#define CURVE25519_SIZE 32

enum kex_init_proposals {
	PROPOSAL_KEX_ALGS,
	PROPOSAL_SERVER_HOST_KEY_ALGS,
	PROPOSAL_ENC_ALGS_CTOS,
	PROPOSAL_ENC_ALGS_STOC,
	PROPOSAL_MAC_ALGS_CTOS,
	PROPOSAL_MAC_ALGS_STOC,
	PROPOSAL_COMP_ALGS_CTOS,
	PROPOSAL_COMP_ALGS_STOC,
	PROPOSAL_LANG_CTOS,
	PROPOSAL_LANG_STOC,
	PROPOSAL_MAX
};

enum kex_modes {
	MODE_IN,
	MODE_OUT,
	MODE_MAX
};

enum kex_exchange {
	KEX_DH_GRP1_SHA1,
	KEX_DH_GRP14_SHA1,
	KEX_DH_GRP14_SHA256,
	KEX_DH_GRP16_SHA512,
	KEX_DH_GRP18_SHA512,
	KEX_DH_GEX_SHA1,
	KEX_DH_GEX_SHA256,
	KEX_ECDH_SHA2,
	KEX_C25519_SHA256,
	KEX_KEM_SNTRUP761X25519_SHA512,
	KEX_MAX
};

/* kex->flags */
#define KEX_INIT_SENT			0x0001
#define KEX_INITIAL			0x0002
#define KEX_HAS_PUBKEY_HOSTBOUND	0x0004
#define KEX_RSA_SHA2_256_SUPPORTED 	0x0008 /* only set in server for now */
#define KEX_RSA_SHA2_512_SUPPORTED 	0x0010 /* only set in server for now */
#define KEX_HAS_PING		 	0x0020

struct sshenc {
	char	*name;
	const struct sshcipher *cipher;
	int	enabled;
	u_int	key_len;
	u_int	iv_len;
	u_int	block_size;
	u_char	*key;
	u_char	*iv;
};
struct sshcomp {
	u_int	type;
	int	enabled;
	char	*name;
};
struct newkeys {
	struct sshenc	enc;
	struct sshmac	mac;
	struct sshcomp  comp;
};

struct ssh;
struct sshbuf;

struct kex {
	struct newkeys	*newkeys[MODE_MAX];
	u_int	we_need;
	u_int	dh_need;
	int	server;
	char	*name;
	char	*hostkey_alg;
	int	hostkey_type;
	int	hostkey_nid;
	u_int	kex_type;
	char	*server_sig_algs;
	int	ext_info_c;
	struct sshbuf *my;
	struct sshbuf *peer;
	struct sshbuf *client_version;
	struct sshbuf *server_version;
	struct sshbuf *session_id;
	struct sshbuf *initial_sig;
	struct sshkey *initial_hostkey;
	sig_atomic_t done;
	u_int	flags;
	int	hash_alg;
	int	ec_nid;
	char	*failed_choice;
	int	(*verify_host_key)(struct sshkey *, struct ssh *);
	struct sshkey *(*load_host_public_key)(int, int, struct ssh *);
	struct sshkey *(*load_host_private_key)(int, int, struct ssh *);
	int	(*host_key_index)(struct sshkey *, int, struct ssh *);
	int	(*sign)(struct ssh *, struct sshkey *, struct sshkey *,
	    u_char **, size_t *, const u_char *, size_t, const char *);
	int	(*kex[KEX_MAX])(struct ssh *);
	/* kex specific state */
	DH	*dh;			/* DH */
	u_int	min, max, nbits;	/* GEX */
	EC_KEY	*ec_client_key;		/* ECDH */
	const EC_GROUP *ec_group;	/* ECDH */
	u_char c25519_client_key[CURVE25519_SIZE]; /* 25519 + KEM */
	u_char c25519_client_pubkey[CURVE25519_SIZE]; /* 25519 */
	u_char sntrup761_client_key[crypto_kem_sntrup761_SECRETKEYBYTES]; /* KEM */
	struct sshbuf *client_pub;
};

int	 kex_names_valid(const char *);
char	*kex_alg_list(char);
char	*kex_names_cat(const char *, const char *);
int	 kex_assemble_names(char **, const char *, const char *);
void	 kex_proposal_populate_entries(struct ssh *, char *prop[PROPOSAL_MAX],
    const char *, const char *, const char *, const char *, const char *);
void	 kex_proposal_free_entries(char *prop[PROPOSAL_MAX]);

int	 kex_exchange_identification(struct ssh *, int, const char *);

struct kex *kex_new(void);
int	 kex_ready(struct ssh *, char *[PROPOSAL_MAX]);
int	 kex_setup(struct ssh *, char *[PROPOSAL_MAX]);
void	 kex_free_newkeys(struct newkeys *);
void	 kex_free(struct kex *);

int	 kex_buf2prop(struct sshbuf *, int *, char ***);
int	 kex_prop2buf(struct sshbuf *, char *proposal[PROPOSAL_MAX]);
void	 kex_prop_free(char **);
int	 kex_load_hostkey(struct ssh *, struct sshkey **, struct sshkey **);
int	 kex_verify_host_key(struct ssh *, struct sshkey *);

int	 kex_send_kexinit(struct ssh *);
int	 kex_input_kexinit(int, u_int32_t, struct ssh *);
int	 kex_input_ext_info(int, u_int32_t, struct ssh *);
int	 kex_protocol_error(int, u_int32_t, struct ssh *);
int	 kex_derive_keys(struct ssh *, u_char *, u_int, const struct sshbuf *);
int	 kex_send_newkeys(struct ssh *);
int	 kex_start_rekex(struct ssh *);

int	 kexgex_client(struct ssh *);
int	 kexgex_server(struct ssh *);
int	 kex_gen_client(struct ssh *);
int	 kex_gen_server(struct ssh *);

int	 kex_dh_keypair(struct kex *);
int	 kex_dh_enc(struct kex *, const struct sshbuf *, struct sshbuf **,
    struct sshbuf **);
int	 kex_dh_dec(struct kex *, const struct sshbuf *, struct sshbuf **);

int	 kex_ecdh_keypair(struct kex *);
int	 kex_ecdh_enc(struct kex *, const struct sshbuf *, struct sshbuf **,
    struct sshbuf **);
int	 kex_ecdh_dec(struct kex *, const struct sshbuf *, struct sshbuf **);

int	 kex_c25519_keypair(struct kex *);
int	 kex_c25519_enc(struct kex *, const struct sshbuf *, struct sshbuf **,
    struct sshbuf **);
int	 kex_c25519_dec(struct kex *, const struct sshbuf *, struct sshbuf **);

int	 kex_kem_sntrup761x25519_keypair(struct kex *);
int	 kex_kem_sntrup761x25519_enc(struct kex *, const struct sshbuf *,
    struct sshbuf **, struct sshbuf **);
int	 kex_kem_sntrup761x25519_dec(struct kex *, const struct sshbuf *,
    struct sshbuf **);

int	 kex_dh_keygen(struct kex *);
int	 kex_dh_compute_key(struct kex *, BIGNUM *, struct sshbuf *);

int	 kexgex_hash(int, const struct sshbuf *, const struct sshbuf *,
    const struct sshbuf *, const struct sshbuf *, const struct sshbuf *,
    int, int, int,
    const BIGNUM *, const BIGNUM *, const BIGNUM *,
    const BIGNUM *, const u_char *, size_t,
    u_char *, size_t *);

void	kexc25519_keygen(u_char key[CURVE25519_SIZE], u_char pub[CURVE25519_SIZE])
	__attribute__((__bounded__(__minbytes__, 1, CURVE25519_SIZE)))
	__attribute__((__bounded__(__minbytes__, 2, CURVE25519_SIZE)));
int	kexc25519_shared_key(const u_char key[CURVE25519_SIZE],
    const u_char pub[CURVE25519_SIZE], struct sshbuf *out)
	__attribute__((__bounded__(__minbytes__, 1, CURVE25519_SIZE)))
	__attribute__((__bounded__(__minbytes__, 2, CURVE25519_SIZE)));
int	kexc25519_shared_key_ext(const u_char key[CURVE25519_SIZE],
    const u_char pub[CURVE25519_SIZE], struct sshbuf *out, int)
	__attribute__((__bounded__(__minbytes__, 1, CURVE25519_SIZE)))
	__attribute__((__bounded__(__minbytes__, 2, CURVE25519_SIZE)));

#if defined(DEBUG_KEX) || defined(DEBUG_KEXDH) || defined(DEBUG_KEXECDH)
void	dump_digest(const char *, const u_char *, int);
#endif

#if !defined(WITH_OPENSSL) || !defined(OPENSSL_HAS_ECC)
# undef EC_KEY
# undef EC_GROUP
# undef EC_POINT
#endif

#endif
