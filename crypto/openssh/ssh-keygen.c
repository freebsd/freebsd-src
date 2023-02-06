/* $OpenBSD: ssh-keygen.c,v 1.461 2022/12/04 23:50:49 cheloha Exp $ */
/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1994 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * Identity and host key generation and maintenance.
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 */

#include "includes.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>

#ifdef WITH_OPENSSL
#include <openssl/evp.h>
#include <openssl/pem.h>
#include "openbsd-compat/openssl-compat.h"
#endif

#ifdef HAVE_STDINT_H
# include <stdint.h>
#endif
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#ifdef HAVE_PATHS_H
# include <paths.h>
#endif
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <locale.h>
#include <time.h>

#include "xmalloc.h"
#include "sshkey.h"
#include "authfile.h"
#include "sshbuf.h"
#include "pathnames.h"
#include "log.h"
#include "misc.h"
#include "match.h"
#include "hostfile.h"
#include "dns.h"
#include "ssh.h"
#include "ssh2.h"
#include "ssherr.h"
#include "ssh-pkcs11.h"
#include "atomicio.h"
#include "krl.h"
#include "digest.h"
#include "utf8.h"
#include "authfd.h"
#include "sshsig.h"
#include "ssh-sk.h"
#include "sk-api.h" /* XXX for SSH_SK_USER_PRESENCE_REQD; remove */
#include "cipher.h"

#ifdef WITH_OPENSSL
# define DEFAULT_KEY_TYPE_NAME "rsa"
#else
# define DEFAULT_KEY_TYPE_NAME "ed25519"
#endif

/*
 * Default number of bits in the RSA, DSA and ECDSA keys.  These value can be
 * overridden on the command line.
 *
 * These values, with the exception of DSA, provide security equivalent to at
 * least 128 bits of security according to NIST Special Publication 800-57:
 * Recommendation for Key Management Part 1 rev 4 section 5.6.1.
 * For DSA it (and FIPS-186-4 section 4.2) specifies that the only size for
 * which a 160bit hash is acceptable is 1kbit, and since ssh-dss specifies only
 * SHA1 we limit the DSA key size 1k bits.
 */
#define DEFAULT_BITS		3072
#define DEFAULT_BITS_DSA	1024
#define DEFAULT_BITS_ECDSA	256

static int quiet = 0;

/* Flag indicating that we just want to see the key fingerprint */
static int print_fingerprint = 0;
static int print_bubblebabble = 0;

/* Hash algorithm to use for fingerprints. */
static int fingerprint_hash = SSH_FP_HASH_DEFAULT;

/* The identity file name, given on the command line or entered by the user. */
static char identity_file[PATH_MAX];
static int have_identity = 0;

/* This is set to the passphrase if given on the command line. */
static char *identity_passphrase = NULL;

/* This is set to the new passphrase if given on the command line. */
static char *identity_new_passphrase = NULL;

/* Key type when certifying */
static u_int cert_key_type = SSH2_CERT_TYPE_USER;

/* "key ID" of signed key */
static char *cert_key_id = NULL;

/* Comma-separated list of principal names for certifying keys */
static char *cert_principals = NULL;

/* Validity period for certificates */
static u_int64_t cert_valid_from = 0;
static u_int64_t cert_valid_to = ~0ULL;

/* Certificate options */
#define CERTOPT_X_FWD				(1)
#define CERTOPT_AGENT_FWD			(1<<1)
#define CERTOPT_PORT_FWD			(1<<2)
#define CERTOPT_PTY				(1<<3)
#define CERTOPT_USER_RC				(1<<4)
#define CERTOPT_NO_REQUIRE_USER_PRESENCE	(1<<5)
#define CERTOPT_REQUIRE_VERIFY			(1<<6)
#define CERTOPT_DEFAULT	(CERTOPT_X_FWD|CERTOPT_AGENT_FWD| \
			 CERTOPT_PORT_FWD|CERTOPT_PTY|CERTOPT_USER_RC)
static u_int32_t certflags_flags = CERTOPT_DEFAULT;
static char *certflags_command = NULL;
static char *certflags_src_addr = NULL;

/* Arbitrary extensions specified by user */
struct cert_ext {
	char *key;
	char *val;
	int crit;
};
static struct cert_ext *cert_ext;
static size_t ncert_ext;

/* Conversion to/from various formats */
enum {
	FMT_RFC4716,
	FMT_PKCS8,
	FMT_PEM
} convert_format = FMT_RFC4716;

static char *key_type_name = NULL;

/* Load key from this PKCS#11 provider */
static char *pkcs11provider = NULL;

/* FIDO/U2F provider to use */
static char *sk_provider = NULL;

/* Format for writing private keys */
static int private_key_format = SSHKEY_PRIVATE_OPENSSH;

/* Cipher for new-format private keys */
static char *openssh_format_cipher = NULL;

/* Number of KDF rounds to derive new format keys. */
static int rounds = 0;

/* argv0 */
extern char *__progname;

static char hostname[NI_MAXHOST];

#ifdef WITH_OPENSSL
/* moduli.c */
int gen_candidates(FILE *, u_int32_t, u_int32_t, BIGNUM *);
int prime_test(FILE *, FILE *, u_int32_t, u_int32_t, char *, unsigned long,
    unsigned long);
#endif

static void
type_bits_valid(int type, const char *name, u_int32_t *bitsp)
{
	if (type == KEY_UNSPEC)
		fatal("unknown key type %s", key_type_name);
	if (*bitsp == 0) {
#ifdef WITH_OPENSSL
		int nid;

		switch(type) {
		case KEY_DSA:
			*bitsp = DEFAULT_BITS_DSA;
			break;
		case KEY_ECDSA:
			if (name != NULL &&
			    (nid = sshkey_ecdsa_nid_from_name(name)) > 0)
				*bitsp = sshkey_curve_nid_to_bits(nid);
			if (*bitsp == 0)
				*bitsp = DEFAULT_BITS_ECDSA;
			break;
		case KEY_RSA:
			*bitsp = DEFAULT_BITS;
			break;
		}
#endif
	}
#ifdef WITH_OPENSSL
	switch (type) {
	case KEY_DSA:
		if (*bitsp != 1024)
			fatal("Invalid DSA key length: must be 1024 bits");
		break;
	case KEY_RSA:
		if (*bitsp < SSH_RSA_MINIMUM_MODULUS_SIZE)
			fatal("Invalid RSA key length: minimum is %d bits",
			    SSH_RSA_MINIMUM_MODULUS_SIZE);
		else if (*bitsp > OPENSSL_RSA_MAX_MODULUS_BITS)
			fatal("Invalid RSA key length: maximum is %d bits",
			    OPENSSL_RSA_MAX_MODULUS_BITS);
		break;
	case KEY_ECDSA:
		if (sshkey_ecdsa_bits_to_nid(*bitsp) == -1)
#ifdef OPENSSL_HAS_NISTP521
			fatal("Invalid ECDSA key length: valid lengths are "
			    "256, 384 or 521 bits");
#else
			fatal("Invalid ECDSA key length: valid lengths are "
			    "256 or 384 bits");
#endif
	}
#endif
}

/*
 * Checks whether a file exists and, if so, asks the user whether they wish
 * to overwrite it.
 * Returns nonzero if the file does not already exist or if the user agrees to
 * overwrite, or zero otherwise.
 */
static int
confirm_overwrite(const char *filename)
{
	char yesno[3];
	struct stat st;

	if (stat(filename, &st) != 0)
		return 1;
	printf("%s already exists.\n", filename);
	printf("Overwrite (y/n)? ");
	fflush(stdout);
	if (fgets(yesno, sizeof(yesno), stdin) == NULL)
		return 0;
	if (yesno[0] != 'y' && yesno[0] != 'Y')
		return 0;
	return 1;
}

static void
ask_filename(struct passwd *pw, const char *prompt)
{
	char buf[1024];
	char *name = NULL;

	if (key_type_name == NULL)
		name = _PATH_SSH_CLIENT_ID_RSA;
	else {
		switch (sshkey_type_from_name(key_type_name)) {
		case KEY_DSA_CERT:
		case KEY_DSA:
			name = _PATH_SSH_CLIENT_ID_DSA;
			break;
#ifdef OPENSSL_HAS_ECC
		case KEY_ECDSA_CERT:
		case KEY_ECDSA:
			name = _PATH_SSH_CLIENT_ID_ECDSA;
			break;
		case KEY_ECDSA_SK_CERT:
		case KEY_ECDSA_SK:
			name = _PATH_SSH_CLIENT_ID_ECDSA_SK;
			break;
#endif
		case KEY_RSA_CERT:
		case KEY_RSA:
			name = _PATH_SSH_CLIENT_ID_RSA;
			break;
		case KEY_ED25519:
		case KEY_ED25519_CERT:
			name = _PATH_SSH_CLIENT_ID_ED25519;
			break;
		case KEY_ED25519_SK:
		case KEY_ED25519_SK_CERT:
			name = _PATH_SSH_CLIENT_ID_ED25519_SK;
			break;
		case KEY_XMSS:
		case KEY_XMSS_CERT:
			name = _PATH_SSH_CLIENT_ID_XMSS;
			break;
		default:
			fatal("bad key type");
		}
	}
	snprintf(identity_file, sizeof(identity_file),
	    "%s/%s", pw->pw_dir, name);
	printf("%s (%s): ", prompt, identity_file);
	fflush(stdout);
	if (fgets(buf, sizeof(buf), stdin) == NULL)
		exit(1);
	buf[strcspn(buf, "\n")] = '\0';
	if (strcmp(buf, "") != 0)
		strlcpy(identity_file, buf, sizeof(identity_file));
	have_identity = 1;
}

static struct sshkey *
load_identity(const char *filename, char **commentp)
{
	char *pass;
	struct sshkey *prv;
	int r;

	if (commentp != NULL)
		*commentp = NULL;
	if ((r = sshkey_load_private(filename, "", &prv, commentp)) == 0)
		return prv;
	if (r != SSH_ERR_KEY_WRONG_PASSPHRASE)
		fatal_r(r, "Load key \"%s\"", filename);
	if (identity_passphrase)
		pass = xstrdup(identity_passphrase);
	else
		pass = read_passphrase("Enter passphrase: ", RP_ALLOW_STDIN);
	r = sshkey_load_private(filename, pass, &prv, commentp);
	freezero(pass, strlen(pass));
	if (r != 0)
		fatal_r(r, "Load key \"%s\"", filename);
	return prv;
}

#define SSH_COM_PUBLIC_BEGIN		"---- BEGIN SSH2 PUBLIC KEY ----"
#define SSH_COM_PUBLIC_END		"---- END SSH2 PUBLIC KEY ----"
#define SSH_COM_PRIVATE_BEGIN		"---- BEGIN SSH2 ENCRYPTED PRIVATE KEY ----"
#define	SSH_COM_PRIVATE_KEY_MAGIC	0x3f6ff9eb

#ifdef WITH_OPENSSL
static void
do_convert_to_ssh2(struct passwd *pw, struct sshkey *k)
{
	struct sshbuf *b;
	char comment[61], *b64;
	int r;

	if ((b = sshbuf_new()) == NULL)
		fatal_f("sshbuf_new failed");
	if ((r = sshkey_putb(k, b)) != 0)
		fatal_fr(r, "put key");
	if ((b64 = sshbuf_dtob64_string(b, 1)) == NULL)
		fatal_f("sshbuf_dtob64_string failed");

	/* Comment + surrounds must fit into 72 chars (RFC 4716 sec 3.3) */
	snprintf(comment, sizeof(comment),
	    "%u-bit %s, converted by %s@%s from OpenSSH",
	    sshkey_size(k), sshkey_type(k),
	    pw->pw_name, hostname);

	sshkey_free(k);
	sshbuf_free(b);

	fprintf(stdout, "%s\n", SSH_COM_PUBLIC_BEGIN);
	fprintf(stdout, "Comment: \"%s\"\n%s", comment, b64);
	fprintf(stdout, "%s\n", SSH_COM_PUBLIC_END);
	free(b64);
	exit(0);
}

static void
do_convert_to_pkcs8(struct sshkey *k)
{
	switch (sshkey_type_plain(k->type)) {
	case KEY_RSA:
		if (!PEM_write_RSA_PUBKEY(stdout, k->rsa))
			fatal("PEM_write_RSA_PUBKEY failed");
		break;
	case KEY_DSA:
		if (!PEM_write_DSA_PUBKEY(stdout, k->dsa))
			fatal("PEM_write_DSA_PUBKEY failed");
		break;
#ifdef OPENSSL_HAS_ECC
	case KEY_ECDSA:
		if (!PEM_write_EC_PUBKEY(stdout, k->ecdsa))
			fatal("PEM_write_EC_PUBKEY failed");
		break;
#endif
	default:
		fatal_f("unsupported key type %s", sshkey_type(k));
	}
	exit(0);
}

static void
do_convert_to_pem(struct sshkey *k)
{
	switch (sshkey_type_plain(k->type)) {
	case KEY_RSA:
		if (!PEM_write_RSAPublicKey(stdout, k->rsa))
			fatal("PEM_write_RSAPublicKey failed");
		break;
	case KEY_DSA:
		if (!PEM_write_DSA_PUBKEY(stdout, k->dsa))
			fatal("PEM_write_DSA_PUBKEY failed");
		break;
#ifdef OPENSSL_HAS_ECC
	case KEY_ECDSA:
		if (!PEM_write_EC_PUBKEY(stdout, k->ecdsa))
			fatal("PEM_write_EC_PUBKEY failed");
		break;
#endif
	default:
		fatal_f("unsupported key type %s", sshkey_type(k));
	}
	exit(0);
}

static void
do_convert_to(struct passwd *pw)
{
	struct sshkey *k;
	struct stat st;
	int r;

	if (!have_identity)
		ask_filename(pw, "Enter file in which the key is");
	if (stat(identity_file, &st) == -1)
		fatal("%s: %s: %s", __progname, identity_file, strerror(errno));
	if ((r = sshkey_load_public(identity_file, &k, NULL)) != 0)
		k = load_identity(identity_file, NULL);
	switch (convert_format) {
	case FMT_RFC4716:
		do_convert_to_ssh2(pw, k);
		break;
	case FMT_PKCS8:
		do_convert_to_pkcs8(k);
		break;
	case FMT_PEM:
		do_convert_to_pem(k);
		break;
	default:
		fatal_f("unknown key format %d", convert_format);
	}
	exit(0);
}

/*
 * This is almost exactly the bignum1 encoding, but with 32 bit for length
 * instead of 16.
 */
static void
buffer_get_bignum_bits(struct sshbuf *b, BIGNUM *value)
{
	u_int bytes, bignum_bits;
	int r;

	if ((r = sshbuf_get_u32(b, &bignum_bits)) != 0)
		fatal_fr(r, "parse");
	bytes = (bignum_bits + 7) / 8;
	if (sshbuf_len(b) < bytes)
		fatal_f("input buffer too small: need %d have %zu",
		    bytes, sshbuf_len(b));
	if (BN_bin2bn(sshbuf_ptr(b), bytes, value) == NULL)
		fatal_f("BN_bin2bn failed");
	if ((r = sshbuf_consume(b, bytes)) != 0)
		fatal_fr(r, "consume");
}

static struct sshkey *
do_convert_private_ssh2(struct sshbuf *b)
{
	struct sshkey *key = NULL;
	char *type, *cipher;
	u_char e1, e2, e3, *sig = NULL, data[] = "abcde12345";
	int r, rlen, ktype;
	u_int magic, i1, i2, i3, i4;
	size_t slen;
	u_long e;
	BIGNUM *dsa_p = NULL, *dsa_q = NULL, *dsa_g = NULL;
	BIGNUM *dsa_pub_key = NULL, *dsa_priv_key = NULL;
	BIGNUM *rsa_n = NULL, *rsa_e = NULL, *rsa_d = NULL;
	BIGNUM *rsa_p = NULL, *rsa_q = NULL, *rsa_iqmp = NULL;

	if ((r = sshbuf_get_u32(b, &magic)) != 0)
		fatal_fr(r, "parse magic");

	if (magic != SSH_COM_PRIVATE_KEY_MAGIC) {
		error("bad magic 0x%x != 0x%x", magic,
		    SSH_COM_PRIVATE_KEY_MAGIC);
		return NULL;
	}
	if ((r = sshbuf_get_u32(b, &i1)) != 0 ||
	    (r = sshbuf_get_cstring(b, &type, NULL)) != 0 ||
	    (r = sshbuf_get_cstring(b, &cipher, NULL)) != 0 ||
	    (r = sshbuf_get_u32(b, &i2)) != 0 ||
	    (r = sshbuf_get_u32(b, &i3)) != 0 ||
	    (r = sshbuf_get_u32(b, &i4)) != 0)
		fatal_fr(r, "parse");
	debug("ignore (%d %d %d %d)", i1, i2, i3, i4);
	if (strcmp(cipher, "none") != 0) {
		error("unsupported cipher %s", cipher);
		free(cipher);
		free(type);
		return NULL;
	}
	free(cipher);

	if (strstr(type, "dsa")) {
		ktype = KEY_DSA;
	} else if (strstr(type, "rsa")) {
		ktype = KEY_RSA;
	} else {
		free(type);
		return NULL;
	}
	if ((key = sshkey_new(ktype)) == NULL)
		fatal("sshkey_new failed");
	free(type);

	switch (key->type) {
	case KEY_DSA:
		if ((dsa_p = BN_new()) == NULL ||
		    (dsa_q = BN_new()) == NULL ||
		    (dsa_g = BN_new()) == NULL ||
		    (dsa_pub_key = BN_new()) == NULL ||
		    (dsa_priv_key = BN_new()) == NULL)
			fatal_f("BN_new");
		buffer_get_bignum_bits(b, dsa_p);
		buffer_get_bignum_bits(b, dsa_g);
		buffer_get_bignum_bits(b, dsa_q);
		buffer_get_bignum_bits(b, dsa_pub_key);
		buffer_get_bignum_bits(b, dsa_priv_key);
		if (!DSA_set0_pqg(key->dsa, dsa_p, dsa_q, dsa_g))
			fatal_f("DSA_set0_pqg failed");
		dsa_p = dsa_q = dsa_g = NULL; /* transferred */
		if (!DSA_set0_key(key->dsa, dsa_pub_key, dsa_priv_key))
			fatal_f("DSA_set0_key failed");
		dsa_pub_key = dsa_priv_key = NULL; /* transferred */
		break;
	case KEY_RSA:
		if ((r = sshbuf_get_u8(b, &e1)) != 0 ||
		    (e1 < 30 && (r = sshbuf_get_u8(b, &e2)) != 0) ||
		    (e1 < 30 && (r = sshbuf_get_u8(b, &e3)) != 0))
			fatal_fr(r, "parse RSA");
		e = e1;
		debug("e %lx", e);
		if (e < 30) {
			e <<= 8;
			e += e2;
			debug("e %lx", e);
			e <<= 8;
			e += e3;
			debug("e %lx", e);
		}
		if ((rsa_e = BN_new()) == NULL)
			fatal_f("BN_new");
		if (!BN_set_word(rsa_e, e)) {
			BN_clear_free(rsa_e);
			sshkey_free(key);
			return NULL;
		}
		if ((rsa_n = BN_new()) == NULL ||
		    (rsa_d = BN_new()) == NULL ||
		    (rsa_p = BN_new()) == NULL ||
		    (rsa_q = BN_new()) == NULL ||
		    (rsa_iqmp = BN_new()) == NULL)
			fatal_f("BN_new");
		buffer_get_bignum_bits(b, rsa_d);
		buffer_get_bignum_bits(b, rsa_n);
		buffer_get_bignum_bits(b, rsa_iqmp);
		buffer_get_bignum_bits(b, rsa_q);
		buffer_get_bignum_bits(b, rsa_p);
		if (!RSA_set0_key(key->rsa, rsa_n, rsa_e, rsa_d))
			fatal_f("RSA_set0_key failed");
		rsa_n = rsa_e = rsa_d = NULL; /* transferred */
		if (!RSA_set0_factors(key->rsa, rsa_p, rsa_q))
			fatal_f("RSA_set0_factors failed");
		rsa_p = rsa_q = NULL; /* transferred */
		if ((r = ssh_rsa_complete_crt_parameters(key, rsa_iqmp)) != 0)
			fatal_fr(r, "generate RSA parameters");
		BN_clear_free(rsa_iqmp);
		break;
	}
	rlen = sshbuf_len(b);
	if (rlen != 0)
		error_f("remaining bytes in key blob %d", rlen);

	/* try the key */
	if ((r = sshkey_sign(key, &sig, &slen, data, sizeof(data),
	    NULL, NULL, NULL, 0)) != 0)
		error_fr(r, "signing with converted key failed");
	else if ((r = sshkey_verify(key, sig, slen, data, sizeof(data),
	    NULL, 0, NULL)) != 0)
		error_fr(r, "verification with converted key failed");
	if (r != 0) {
		sshkey_free(key);
		free(sig);
		return NULL;
	}
	free(sig);
	return key;
}

static int
get_line(FILE *fp, char *line, size_t len)
{
	int c;
	size_t pos = 0;

	line[0] = '\0';
	while ((c = fgetc(fp)) != EOF) {
		if (pos >= len - 1)
			fatal("input line too long.");
		switch (c) {
		case '\r':
			c = fgetc(fp);
			if (c != EOF && c != '\n' && ungetc(c, fp) == EOF)
				fatal("unget: %s", strerror(errno));
			return pos;
		case '\n':
			return pos;
		}
		line[pos++] = c;
		line[pos] = '\0';
	}
	/* We reached EOF */
	return -1;
}

static void
do_convert_from_ssh2(struct passwd *pw, struct sshkey **k, int *private)
{
	int r, blen, escaped = 0;
	u_int len;
	char line[1024];
	struct sshbuf *buf;
	char encoded[8096];
	FILE *fp;

	if ((buf = sshbuf_new()) == NULL)
		fatal("sshbuf_new failed");
	if ((fp = fopen(identity_file, "r")) == NULL)
		fatal("%s: %s: %s", __progname, identity_file, strerror(errno));
	encoded[0] = '\0';
	while ((blen = get_line(fp, line, sizeof(line))) != -1) {
		if (blen > 0 && line[blen - 1] == '\\')
			escaped++;
		if (strncmp(line, "----", 4) == 0 ||
		    strstr(line, ": ") != NULL) {
			if (strstr(line, SSH_COM_PRIVATE_BEGIN) != NULL)
				*private = 1;
			if (strstr(line, " END ") != NULL) {
				break;
			}
			/* fprintf(stderr, "ignore: %s", line); */
			continue;
		}
		if (escaped) {
			escaped--;
			/* fprintf(stderr, "escaped: %s", line); */
			continue;
		}
		strlcat(encoded, line, sizeof(encoded));
	}
	len = strlen(encoded);
	if (((len % 4) == 3) &&
	    (encoded[len-1] == '=') &&
	    (encoded[len-2] == '=') &&
	    (encoded[len-3] == '='))
		encoded[len-3] = '\0';
	if ((r = sshbuf_b64tod(buf, encoded)) != 0)
		fatal_fr(r, "base64 decode");
	if (*private) {
		if ((*k = do_convert_private_ssh2(buf)) == NULL)
			fatal_f("private key conversion failed");
	} else if ((r = sshkey_fromb(buf, k)) != 0)
		fatal_fr(r, "parse key");
	sshbuf_free(buf);
	fclose(fp);
}

static void
do_convert_from_pkcs8(struct sshkey **k, int *private)
{
	EVP_PKEY *pubkey;
	FILE *fp;

	if ((fp = fopen(identity_file, "r")) == NULL)
		fatal("%s: %s: %s", __progname, identity_file, strerror(errno));
	if ((pubkey = PEM_read_PUBKEY(fp, NULL, NULL, NULL)) == NULL) {
		fatal_f("%s is not a recognised public key format",
		    identity_file);
	}
	fclose(fp);
	switch (EVP_PKEY_base_id(pubkey)) {
	case EVP_PKEY_RSA:
		if ((*k = sshkey_new(KEY_UNSPEC)) == NULL)
			fatal("sshkey_new failed");
		(*k)->type = KEY_RSA;
		(*k)->rsa = EVP_PKEY_get1_RSA(pubkey);
		break;
	case EVP_PKEY_DSA:
		if ((*k = sshkey_new(KEY_UNSPEC)) == NULL)
			fatal("sshkey_new failed");
		(*k)->type = KEY_DSA;
		(*k)->dsa = EVP_PKEY_get1_DSA(pubkey);
		break;
#ifdef OPENSSL_HAS_ECC
	case EVP_PKEY_EC:
		if ((*k = sshkey_new(KEY_UNSPEC)) == NULL)
			fatal("sshkey_new failed");
		(*k)->type = KEY_ECDSA;
		(*k)->ecdsa = EVP_PKEY_get1_EC_KEY(pubkey);
		(*k)->ecdsa_nid = sshkey_ecdsa_key_to_nid((*k)->ecdsa);
		break;
#endif
	default:
		fatal_f("unsupported pubkey type %d",
		    EVP_PKEY_base_id(pubkey));
	}
	EVP_PKEY_free(pubkey);
	return;
}

static void
do_convert_from_pem(struct sshkey **k, int *private)
{
	FILE *fp;
	RSA *rsa;

	if ((fp = fopen(identity_file, "r")) == NULL)
		fatal("%s: %s: %s", __progname, identity_file, strerror(errno));
	if ((rsa = PEM_read_RSAPublicKey(fp, NULL, NULL, NULL)) != NULL) {
		if ((*k = sshkey_new(KEY_UNSPEC)) == NULL)
			fatal("sshkey_new failed");
		(*k)->type = KEY_RSA;
		(*k)->rsa = rsa;
		fclose(fp);
		return;
	}
	fatal_f("unrecognised raw private key format");
}

static void
do_convert_from(struct passwd *pw)
{
	struct sshkey *k = NULL;
	int r, private = 0, ok = 0;
	struct stat st;

	if (!have_identity)
		ask_filename(pw, "Enter file in which the key is");
	if (stat(identity_file, &st) == -1)
		fatal("%s: %s: %s", __progname, identity_file, strerror(errno));

	switch (convert_format) {
	case FMT_RFC4716:
		do_convert_from_ssh2(pw, &k, &private);
		break;
	case FMT_PKCS8:
		do_convert_from_pkcs8(&k, &private);
		break;
	case FMT_PEM:
		do_convert_from_pem(&k, &private);
		break;
	default:
		fatal_f("unknown key format %d", convert_format);
	}

	if (!private) {
		if ((r = sshkey_write(k, stdout)) == 0)
			ok = 1;
		if (ok)
			fprintf(stdout, "\n");
	} else {
		switch (k->type) {
		case KEY_DSA:
			ok = PEM_write_DSAPrivateKey(stdout, k->dsa, NULL,
			    NULL, 0, NULL, NULL);
			break;
#ifdef OPENSSL_HAS_ECC
		case KEY_ECDSA:
			ok = PEM_write_ECPrivateKey(stdout, k->ecdsa, NULL,
			    NULL, 0, NULL, NULL);
			break;
#endif
		case KEY_RSA:
			ok = PEM_write_RSAPrivateKey(stdout, k->rsa, NULL,
			    NULL, 0, NULL, NULL);
			break;
		default:
			fatal_f("unsupported key type %s", sshkey_type(k));
		}
	}

	if (!ok)
		fatal("key write failed");
	sshkey_free(k);
	exit(0);
}
#endif

static void
do_print_public(struct passwd *pw)
{
	struct sshkey *prv;
	struct stat st;
	int r;
	char *comment = NULL;

	if (!have_identity)
		ask_filename(pw, "Enter file in which the key is");
	if (stat(identity_file, &st) == -1)
		fatal("%s: %s", identity_file, strerror(errno));
	prv = load_identity(identity_file, &comment);
	if ((r = sshkey_write(prv, stdout)) != 0)
		fatal_fr(r, "write key");
	if (comment != NULL && *comment != '\0')
		fprintf(stdout, " %s", comment);
	fprintf(stdout, "\n");
	if (sshkey_is_sk(prv)) {
		debug("sk_application: \"%s\", sk_flags 0x%02x",
			prv->sk_application, prv->sk_flags);
	}
	sshkey_free(prv);
	free(comment);
	exit(0);
}

static void
do_download(struct passwd *pw)
{
#ifdef ENABLE_PKCS11
	struct sshkey **keys = NULL;
	int i, nkeys;
	enum sshkey_fp_rep rep;
	int fptype;
	char *fp, *ra, **comments = NULL;

	fptype = print_bubblebabble ? SSH_DIGEST_SHA1 : fingerprint_hash;
	rep =    print_bubblebabble ? SSH_FP_BUBBLEBABBLE : SSH_FP_DEFAULT;

	pkcs11_init(1);
	nkeys = pkcs11_add_provider(pkcs11provider, NULL, &keys, &comments);
	if (nkeys <= 0)
		fatal("cannot read public key from pkcs11");
	for (i = 0; i < nkeys; i++) {
		if (print_fingerprint) {
			fp = sshkey_fingerprint(keys[i], fptype, rep);
			ra = sshkey_fingerprint(keys[i], fingerprint_hash,
			    SSH_FP_RANDOMART);
			if (fp == NULL || ra == NULL)
				fatal_f("sshkey_fingerprint fail");
			printf("%u %s %s (PKCS11 key)\n", sshkey_size(keys[i]),
			    fp, sshkey_type(keys[i]));
			if (log_level_get() >= SYSLOG_LEVEL_VERBOSE)
				printf("%s\n", ra);
			free(ra);
			free(fp);
		} else {
			(void) sshkey_write(keys[i], stdout); /* XXX check */
			fprintf(stdout, "%s%s\n",
			    *(comments[i]) == '\0' ? "" : " ", comments[i]);
		}
		free(comments[i]);
		sshkey_free(keys[i]);
	}
	free(comments);
	free(keys);
	pkcs11_terminate();
	exit(0);
#else
	fatal("no pkcs11 support");
#endif /* ENABLE_PKCS11 */
}

static struct sshkey *
try_read_key(char **cpp)
{
	struct sshkey *ret;
	int r;

	if ((ret = sshkey_new(KEY_UNSPEC)) == NULL)
		fatal("sshkey_new failed");
	if ((r = sshkey_read(ret, cpp)) == 0)
		return ret;
	/* Not a key */
	sshkey_free(ret);
	return NULL;
}

static void
fingerprint_one_key(const struct sshkey *public, const char *comment)
{
	char *fp = NULL, *ra = NULL;
	enum sshkey_fp_rep rep;
	int fptype;

	fptype = print_bubblebabble ? SSH_DIGEST_SHA1 : fingerprint_hash;
	rep =    print_bubblebabble ? SSH_FP_BUBBLEBABBLE : SSH_FP_DEFAULT;
	fp = sshkey_fingerprint(public, fptype, rep);
	ra = sshkey_fingerprint(public, fingerprint_hash, SSH_FP_RANDOMART);
	if (fp == NULL || ra == NULL)
		fatal_f("sshkey_fingerprint failed");
	mprintf("%u %s %s (%s)\n", sshkey_size(public), fp,
	    comment ? comment : "no comment", sshkey_type(public));
	if (log_level_get() >= SYSLOG_LEVEL_VERBOSE)
		printf("%s\n", ra);
	free(ra);
	free(fp);
}

static void
fingerprint_private(const char *path)
{
	struct stat st;
	char *comment = NULL;
	struct sshkey *privkey = NULL, *pubkey = NULL;
	int r;

	if (stat(identity_file, &st) == -1)
		fatal("%s: %s", path, strerror(errno));
	if ((r = sshkey_load_public(path, &pubkey, &comment)) != 0)
		debug_r(r, "load public \"%s\"", path);
	if (pubkey == NULL || comment == NULL || *comment == '\0') {
		free(comment);
		if ((r = sshkey_load_private(path, NULL,
		    &privkey, &comment)) != 0)
			debug_r(r, "load private \"%s\"", path);
	}
	if (pubkey == NULL && privkey == NULL)
		fatal("%s is not a key file.", path);

	fingerprint_one_key(pubkey == NULL ? privkey : pubkey, comment);
	sshkey_free(pubkey);
	sshkey_free(privkey);
	free(comment);
}

static void
do_fingerprint(struct passwd *pw)
{
	FILE *f;
	struct sshkey *public = NULL;
	char *comment = NULL, *cp, *ep, *line = NULL;
	size_t linesize = 0;
	int i, invalid = 1;
	const char *path;
	u_long lnum = 0;

	if (!have_identity)
		ask_filename(pw, "Enter file in which the key is");
	path = identity_file;

	if (strcmp(identity_file, "-") == 0) {
		f = stdin;
		path = "(stdin)";
	} else if ((f = fopen(path, "r")) == NULL)
		fatal("%s: %s: %s", __progname, path, strerror(errno));

	while (getline(&line, &linesize, f) != -1) {
		lnum++;
		cp = line;
		cp[strcspn(cp, "\n")] = '\0';
		/* Trim leading space and comments */
		cp = line + strspn(line, " \t");
		if (*cp == '#' || *cp == '\0')
			continue;

		/*
		 * Input may be plain keys, private keys, authorized_keys
		 * or known_hosts.
		 */

		/*
		 * Try private keys first. Assume a key is private if
		 * "SSH PRIVATE KEY" appears on the first line and we're
		 * not reading from stdin (XXX support private keys on stdin).
		 */
		if (lnum == 1 && strcmp(identity_file, "-") != 0 &&
		    strstr(cp, "PRIVATE KEY") != NULL) {
			free(line);
			fclose(f);
			fingerprint_private(path);
			exit(0);
		}

		/*
		 * If it's not a private key, then this must be prepared to
		 * accept a public key prefixed with a hostname or options.
		 * Try a bare key first, otherwise skip the leading stuff.
		 */
		if ((public = try_read_key(&cp)) == NULL) {
			i = strtol(cp, &ep, 10);
			if (i == 0 || ep == NULL ||
			    (*ep != ' ' && *ep != '\t')) {
				int quoted = 0;

				comment = cp;
				for (; *cp && (quoted || (*cp != ' ' &&
				    *cp != '\t')); cp++) {
					if (*cp == '\\' && cp[1] == '"')
						cp++;	/* Skip both */
					else if (*cp == '"')
						quoted = !quoted;
				}
				if (!*cp)
					continue;
				*cp++ = '\0';
			}
		}
		/* Retry after parsing leading hostname/key options */
		if (public == NULL && (public = try_read_key(&cp)) == NULL) {
			debug("%s:%lu: not a public key", path, lnum);
			continue;
		}

		/* Find trailing comment, if any */
		for (; *cp == ' ' || *cp == '\t'; cp++)
			;
		if (*cp != '\0' && *cp != '#')
			comment = cp;

		fingerprint_one_key(public, comment);
		sshkey_free(public);
		invalid = 0; /* One good key in the file is sufficient */
	}
	fclose(f);
	free(line);

	if (invalid)
		fatal("%s is not a public key file.", path);
	exit(0);
}

static void
do_gen_all_hostkeys(struct passwd *pw)
{
	struct {
		char *key_type;
		char *key_type_display;
		char *path;
	} key_types[] = {
#ifdef WITH_OPENSSL
		{ "rsa", "RSA" ,_PATH_HOST_RSA_KEY_FILE },
#ifdef OPENSSL_HAS_ECC
		{ "ecdsa", "ECDSA",_PATH_HOST_ECDSA_KEY_FILE },
#endif /* OPENSSL_HAS_ECC */
#endif /* WITH_OPENSSL */
		{ "ed25519", "ED25519",_PATH_HOST_ED25519_KEY_FILE },
#ifdef WITH_XMSS
		{ "xmss", "XMSS",_PATH_HOST_XMSS_KEY_FILE },
#endif /* WITH_XMSS */
		{ NULL, NULL, NULL }
	};

	u_int32_t bits = 0;
	int first = 0;
	struct stat st;
	struct sshkey *private, *public;
	char comment[1024], *prv_tmp, *pub_tmp, *prv_file, *pub_file;
	int i, type, fd, r;

	for (i = 0; key_types[i].key_type; i++) {
		public = private = NULL;
		prv_tmp = pub_tmp = prv_file = pub_file = NULL;

		xasprintf(&prv_file, "%s%s",
		    identity_file, key_types[i].path);

		/* Check whether private key exists and is not zero-length */
		if (stat(prv_file, &st) == 0) {
			if (st.st_size != 0)
				goto next;
		} else if (errno != ENOENT) {
			error("Could not stat %s: %s", key_types[i].path,
			    strerror(errno));
			goto failnext;
		}

		/*
		 * Private key doesn't exist or is invalid; proceed with
		 * key generation.
		 */
		xasprintf(&prv_tmp, "%s%s.XXXXXXXXXX",
		    identity_file, key_types[i].path);
		xasprintf(&pub_tmp, "%s%s.pub.XXXXXXXXXX",
		    identity_file, key_types[i].path);
		xasprintf(&pub_file, "%s%s.pub",
		    identity_file, key_types[i].path);

		if (first == 0) {
			first = 1;
			printf("%s: generating new host keys: ", __progname);
		}
		printf("%s ", key_types[i].key_type_display);
		fflush(stdout);
		type = sshkey_type_from_name(key_types[i].key_type);
		if ((fd = mkstemp(prv_tmp)) == -1) {
			error("Could not save your private key in %s: %s",
			    prv_tmp, strerror(errno));
			goto failnext;
		}
		(void)close(fd); /* just using mkstemp() to reserve a name */
		bits = 0;
		type_bits_valid(type, NULL, &bits);
		if ((r = sshkey_generate(type, bits, &private)) != 0) {
			error_r(r, "sshkey_generate failed");
			goto failnext;
		}
		if ((r = sshkey_from_private(private, &public)) != 0)
			fatal_fr(r, "sshkey_from_private");
		snprintf(comment, sizeof comment, "%s@%s", pw->pw_name,
		    hostname);
		if ((r = sshkey_save_private(private, prv_tmp, "",
		    comment, private_key_format, openssh_format_cipher,
		    rounds)) != 0) {
			error_r(r, "Saving key \"%s\" failed", prv_tmp);
			goto failnext;
		}
		if ((fd = mkstemp(pub_tmp)) == -1) {
			error("Could not save your public key in %s: %s",
			    pub_tmp, strerror(errno));
			goto failnext;
		}
		(void)fchmod(fd, 0644);
		(void)close(fd);
		if ((r = sshkey_save_public(public, pub_tmp, comment)) != 0) {
			error_r(r, "Unable to save public key to %s",
			    identity_file);
			goto failnext;
		}

		/* Rename temporary files to their permanent locations. */
		if (rename(pub_tmp, pub_file) != 0) {
			error("Unable to move %s into position: %s",
			    pub_file, strerror(errno));
			goto failnext;
		}
		if (rename(prv_tmp, prv_file) != 0) {
			error("Unable to move %s into position: %s",
			    key_types[i].path, strerror(errno));
 failnext:
			first = 0;
			goto next;
		}
 next:
		sshkey_free(private);
		sshkey_free(public);
		free(prv_tmp);
		free(pub_tmp);
		free(prv_file);
		free(pub_file);
	}
	if (first != 0)
		printf("\n");
}

struct known_hosts_ctx {
	const char *host;	/* Hostname searched for in find/delete case */
	FILE *out;		/* Output file, stdout for find_hosts case */
	int has_unhashed;	/* When hashing, original had unhashed hosts */
	int found_key;		/* For find/delete, host was found */
	int invalid;		/* File contained invalid items; don't delete */
	int hash_hosts;		/* Hash hostnames as we go */
	int find_host;		/* Search for specific hostname */
	int delete_host;	/* Delete host from known_hosts */
};

static int
known_hosts_hash(struct hostkey_foreach_line *l, void *_ctx)
{
	struct known_hosts_ctx *ctx = (struct known_hosts_ctx *)_ctx;
	char *hashed, *cp, *hosts, *ohosts;
	int has_wild = l->hosts && strcspn(l->hosts, "*?!") != strlen(l->hosts);
	int was_hashed = l->hosts && l->hosts[0] == HASH_DELIM;

	switch (l->status) {
	case HKF_STATUS_OK:
	case HKF_STATUS_MATCHED:
		/*
		 * Don't hash hosts already already hashed, with wildcard
		 * characters or a CA/revocation marker.
		 */
		if (was_hashed || has_wild || l->marker != MRK_NONE) {
			fprintf(ctx->out, "%s\n", l->line);
			if (has_wild && !ctx->find_host) {
				logit("%s:%lu: ignoring host name "
				    "with wildcard: %.64s", l->path,
				    l->linenum, l->hosts);
			}
			return 0;
		}
		/*
		 * Split any comma-separated hostnames from the host list,
		 * hash and store separately.
		 */
		ohosts = hosts = xstrdup(l->hosts);
		while ((cp = strsep(&hosts, ",")) != NULL && *cp != '\0') {
			lowercase(cp);
			if ((hashed = host_hash(cp, NULL, 0)) == NULL)
				fatal("hash_host failed");
			fprintf(ctx->out, "%s %s\n", hashed, l->rawkey);
			free(hashed);
			ctx->has_unhashed = 1;
		}
		free(ohosts);
		return 0;
	case HKF_STATUS_INVALID:
		/* Retain invalid lines, but mark file as invalid. */
		ctx->invalid = 1;
		logit("%s:%lu: invalid line", l->path, l->linenum);
		/* FALLTHROUGH */
	default:
		fprintf(ctx->out, "%s\n", l->line);
		return 0;
	}
	/* NOTREACHED */
	return -1;
}

static int
known_hosts_find_delete(struct hostkey_foreach_line *l, void *_ctx)
{
	struct known_hosts_ctx *ctx = (struct known_hosts_ctx *)_ctx;
	enum sshkey_fp_rep rep;
	int fptype;
	char *fp = NULL, *ra = NULL;

	fptype = print_bubblebabble ? SSH_DIGEST_SHA1 : fingerprint_hash;
	rep =    print_bubblebabble ? SSH_FP_BUBBLEBABBLE : SSH_FP_DEFAULT;

	if (l->status == HKF_STATUS_MATCHED) {
		if (ctx->delete_host) {
			if (l->marker != MRK_NONE) {
				/* Don't remove CA and revocation lines */
				fprintf(ctx->out, "%s\n", l->line);
			} else {
				/*
				 * Hostname matches and has no CA/revoke
				 * marker, delete it by *not* writing the
				 * line to ctx->out.
				 */
				ctx->found_key = 1;
				if (!quiet)
					printf("# Host %s found: line %lu\n",
					    ctx->host, l->linenum);
			}
			return 0;
		} else if (ctx->find_host) {
			ctx->found_key = 1;
			if (!quiet) {
				printf("# Host %s found: line %lu %s\n",
				    ctx->host,
				    l->linenum, l->marker == MRK_CA ? "CA" :
				    (l->marker == MRK_REVOKE ? "REVOKED" : ""));
			}
			if (ctx->hash_hosts)
				known_hosts_hash(l, ctx);
			else if (print_fingerprint) {
				fp = sshkey_fingerprint(l->key, fptype, rep);
				ra = sshkey_fingerprint(l->key,
				    fingerprint_hash, SSH_FP_RANDOMART);
				if (fp == NULL || ra == NULL)
					fatal_f("sshkey_fingerprint failed");
				mprintf("%s %s %s%s%s\n", ctx->host,
				    sshkey_type(l->key), fp,
				    l->comment[0] ? " " : "",
				    l->comment);
				if (log_level_get() >= SYSLOG_LEVEL_VERBOSE)
					printf("%s\n", ra);
				free(ra);
				free(fp);
			} else
				fprintf(ctx->out, "%s\n", l->line);
			return 0;
		}
	} else if (ctx->delete_host) {
		/* Retain non-matching hosts when deleting */
		if (l->status == HKF_STATUS_INVALID) {
			ctx->invalid = 1;
			logit("%s:%lu: invalid line", l->path, l->linenum);
		}
		fprintf(ctx->out, "%s\n", l->line);
	}
	return 0;
}

static void
do_known_hosts(struct passwd *pw, const char *name, int find_host,
    int delete_host, int hash_hosts)
{
	char *cp, tmp[PATH_MAX], old[PATH_MAX];
	int r, fd, oerrno, inplace = 0;
	struct known_hosts_ctx ctx;
	u_int foreach_options;
	struct stat sb;

	if (!have_identity) {
		cp = tilde_expand_filename(_PATH_SSH_USER_HOSTFILE, pw->pw_uid);
		if (strlcpy(identity_file, cp, sizeof(identity_file)) >=
		    sizeof(identity_file))
			fatal("Specified known hosts path too long");
		free(cp);
		have_identity = 1;
	}
	if (stat(identity_file, &sb) != 0)
		fatal("Cannot stat %s: %s", identity_file, strerror(errno));

	memset(&ctx, 0, sizeof(ctx));
	ctx.out = stdout;
	ctx.host = name;
	ctx.hash_hosts = hash_hosts;
	ctx.find_host = find_host;
	ctx.delete_host = delete_host;

	/*
	 * Find hosts goes to stdout, hash and deletions happen in-place
	 * A corner case is ssh-keygen -HF foo, which should go to stdout
	 */
	if (!find_host && (hash_hosts || delete_host)) {
		if (strlcpy(tmp, identity_file, sizeof(tmp)) >= sizeof(tmp) ||
		    strlcat(tmp, ".XXXXXXXXXX", sizeof(tmp)) >= sizeof(tmp) ||
		    strlcpy(old, identity_file, sizeof(old)) >= sizeof(old) ||
		    strlcat(old, ".old", sizeof(old)) >= sizeof(old))
			fatal("known_hosts path too long");
		umask(077);
		if ((fd = mkstemp(tmp)) == -1)
			fatal("mkstemp: %s", strerror(errno));
		if ((ctx.out = fdopen(fd, "w")) == NULL) {
			oerrno = errno;
			unlink(tmp);
			fatal("fdopen: %s", strerror(oerrno));
		}
		fchmod(fd, sb.st_mode & 0644);
		inplace = 1;
	}
	/* XXX support identity_file == "-" for stdin */
	foreach_options = find_host ? HKF_WANT_MATCH : 0;
	foreach_options |= print_fingerprint ? HKF_WANT_PARSE_KEY : 0;
	if ((r = hostkeys_foreach(identity_file, (find_host || !hash_hosts) ?
	    known_hosts_find_delete : known_hosts_hash, &ctx, name, NULL,
	    foreach_options, 0)) != 0) {
		if (inplace)
			unlink(tmp);
		fatal_fr(r, "hostkeys_foreach");
	}

	if (inplace)
		fclose(ctx.out);

	if (ctx.invalid) {
		error("%s is not a valid known_hosts file.", identity_file);
		if (inplace) {
			error("Not replacing existing known_hosts "
			    "file because of errors");
			unlink(tmp);
		}
		exit(1);
	} else if (delete_host && !ctx.found_key) {
		logit("Host %s not found in %s", name, identity_file);
		if (inplace)
			unlink(tmp);
	} else if (inplace) {
		/* Backup existing file */
		if (unlink(old) == -1 && errno != ENOENT)
			fatal("unlink %.100s: %s", old, strerror(errno));
		if (link(identity_file, old) == -1)
			fatal("link %.100s to %.100s: %s", identity_file, old,
			    strerror(errno));
		/* Move new one into place */
		if (rename(tmp, identity_file) == -1) {
			error("rename\"%s\" to \"%s\": %s", tmp, identity_file,
			    strerror(errno));
			unlink(tmp);
			unlink(old);
			exit(1);
		}

		printf("%s updated.\n", identity_file);
		printf("Original contents retained as %s\n", old);
		if (ctx.has_unhashed) {
			logit("WARNING: %s contains unhashed entries", old);
			logit("Delete this file to ensure privacy "
			    "of hostnames");
		}
	}

	exit (find_host && !ctx.found_key);
}

/*
 * Perform changing a passphrase.  The argument is the passwd structure
 * for the current user.
 */
static void
do_change_passphrase(struct passwd *pw)
{
	char *comment;
	char *old_passphrase, *passphrase1, *passphrase2;
	struct stat st;
	struct sshkey *private;
	int r;

	if (!have_identity)
		ask_filename(pw, "Enter file in which the key is");
	if (stat(identity_file, &st) == -1)
		fatal("%s: %s", identity_file, strerror(errno));
	/* Try to load the file with empty passphrase. */
	r = sshkey_load_private(identity_file, "", &private, &comment);
	if (r == SSH_ERR_KEY_WRONG_PASSPHRASE) {
		if (identity_passphrase)
			old_passphrase = xstrdup(identity_passphrase);
		else
			old_passphrase =
			    read_passphrase("Enter old passphrase: ",
			    RP_ALLOW_STDIN);
		r = sshkey_load_private(identity_file, old_passphrase,
		    &private, &comment);
		freezero(old_passphrase, strlen(old_passphrase));
		if (r != 0)
			goto badkey;
	} else if (r != 0) {
 badkey:
		fatal_r(r, "Failed to load key %s", identity_file);
	}
	if (comment)
		mprintf("Key has comment '%s'\n", comment);

	/* Ask the new passphrase (twice). */
	if (identity_new_passphrase) {
		passphrase1 = xstrdup(identity_new_passphrase);
		passphrase2 = NULL;
	} else {
		passphrase1 =
			read_passphrase("Enter new passphrase (empty for no "
			    "passphrase): ", RP_ALLOW_STDIN);
		passphrase2 = read_passphrase("Enter same passphrase again: ",
		    RP_ALLOW_STDIN);

		/* Verify that they are the same. */
		if (strcmp(passphrase1, passphrase2) != 0) {
			explicit_bzero(passphrase1, strlen(passphrase1));
			explicit_bzero(passphrase2, strlen(passphrase2));
			free(passphrase1);
			free(passphrase2);
			printf("Pass phrases do not match.  Try again.\n");
			exit(1);
		}
		/* Destroy the other copy. */
		freezero(passphrase2, strlen(passphrase2));
	}

	/* Save the file using the new passphrase. */
	if ((r = sshkey_save_private(private, identity_file, passphrase1,
	    comment, private_key_format, openssh_format_cipher, rounds)) != 0) {
		error_r(r, "Saving key \"%s\" failed", identity_file);
		freezero(passphrase1, strlen(passphrase1));
		sshkey_free(private);
		free(comment);
		exit(1);
	}
	/* Destroy the passphrase and the copy of the key in memory. */
	freezero(passphrase1, strlen(passphrase1));
	sshkey_free(private);		 /* Destroys contents */
	free(comment);

	printf("Your identification has been saved with the new passphrase.\n");
	exit(0);
}

/*
 * Print the SSHFP RR.
 */
static int
do_print_resource_record(struct passwd *pw, char *fname, char *hname,
    int print_generic)
{
	struct sshkey *public;
	char *comment = NULL;
	struct stat st;
	int r;

	if (fname == NULL)
		fatal_f("no filename");
	if (stat(fname, &st) == -1) {
		if (errno == ENOENT)
			return 0;
		fatal("%s: %s", fname, strerror(errno));
	}
	if ((r = sshkey_load_public(fname, &public, &comment)) != 0)
		fatal_r(r, "Failed to read v2 public key from \"%s\"", fname);
	export_dns_rr(hname, public, stdout, print_generic);
	sshkey_free(public);
	free(comment);
	return 1;
}

/*
 * Change the comment of a private key file.
 */
static void
do_change_comment(struct passwd *pw, const char *identity_comment)
{
	char new_comment[1024], *comment, *passphrase;
	struct sshkey *private;
	struct sshkey *public;
	struct stat st;
	int r;

	if (!have_identity)
		ask_filename(pw, "Enter file in which the key is");
	if (stat(identity_file, &st) == -1)
		fatal("%s: %s", identity_file, strerror(errno));
	if ((r = sshkey_load_private(identity_file, "",
	    &private, &comment)) == 0)
		passphrase = xstrdup("");
	else if (r != SSH_ERR_KEY_WRONG_PASSPHRASE)
		fatal_r(r, "Cannot load private key \"%s\"", identity_file);
	else {
		if (identity_passphrase)
			passphrase = xstrdup(identity_passphrase);
		else if (identity_new_passphrase)
			passphrase = xstrdup(identity_new_passphrase);
		else
			passphrase = read_passphrase("Enter passphrase: ",
			    RP_ALLOW_STDIN);
		/* Try to load using the passphrase. */
		if ((r = sshkey_load_private(identity_file, passphrase,
		    &private, &comment)) != 0) {
			freezero(passphrase, strlen(passphrase));
			fatal_r(r, "Cannot load private key \"%s\"",
			    identity_file);
		}
	}

	if (private->type != KEY_ED25519 && private->type != KEY_XMSS &&
	    private_key_format != SSHKEY_PRIVATE_OPENSSH) {
		error("Comments are only supported for keys stored in "
		    "the new format (-o).");
		explicit_bzero(passphrase, strlen(passphrase));
		sshkey_free(private);
		exit(1);
	}
	if (comment)
		printf("Old comment: %s\n", comment);
	else
		printf("No existing comment\n");

	if (identity_comment) {
		strlcpy(new_comment, identity_comment, sizeof(new_comment));
	} else {
		printf("New comment: ");
		fflush(stdout);
		if (!fgets(new_comment, sizeof(new_comment), stdin)) {
			explicit_bzero(passphrase, strlen(passphrase));
			sshkey_free(private);
			exit(1);
		}
		new_comment[strcspn(new_comment, "\n")] = '\0';
	}
	if (comment != NULL && strcmp(comment, new_comment) == 0) {
		printf("No change to comment\n");
		free(passphrase);
		sshkey_free(private);
		free(comment);
		exit(0);
	}

	/* Save the file using the new passphrase. */
	if ((r = sshkey_save_private(private, identity_file, passphrase,
	    new_comment, private_key_format, openssh_format_cipher,
	    rounds)) != 0) {
		error_r(r, "Saving key \"%s\" failed", identity_file);
		freezero(passphrase, strlen(passphrase));
		sshkey_free(private);
		free(comment);
		exit(1);
	}
	freezero(passphrase, strlen(passphrase));
	if ((r = sshkey_from_private(private, &public)) != 0)
		fatal_fr(r, "sshkey_from_private");
	sshkey_free(private);

	strlcat(identity_file, ".pub", sizeof(identity_file));
	if ((r = sshkey_save_public(public, identity_file, new_comment)) != 0)
		fatal_r(r, "Unable to save public key to %s", identity_file);
	sshkey_free(public);
	free(comment);

	if (strlen(new_comment) > 0)
		printf("Comment '%s' applied\n", new_comment);
	else
		printf("Comment removed\n");

	exit(0);
}

static void
cert_ext_add(const char *key, const char *value, int iscrit)
{
	cert_ext = xreallocarray(cert_ext, ncert_ext + 1, sizeof(*cert_ext));
	cert_ext[ncert_ext].key = xstrdup(key);
	cert_ext[ncert_ext].val = value == NULL ? NULL : xstrdup(value);
	cert_ext[ncert_ext].crit = iscrit;
	ncert_ext++;
}

/* qsort(3) comparison function for certificate extensions */
static int
cert_ext_cmp(const void *_a, const void *_b)
{
	const struct cert_ext *a = (const struct cert_ext *)_a;
	const struct cert_ext *b = (const struct cert_ext *)_b;
	int r;

	if (a->crit != b->crit)
		return (a->crit < b->crit) ? -1 : 1;
	if ((r = strcmp(a->key, b->key)) != 0)
		return r;
	if ((a->val == NULL) != (b->val == NULL))
		return (a->val == NULL) ? -1 : 1;
	if (a->val != NULL && (r = strcmp(a->val, b->val)) != 0)
		return r;
	return 0;
}

#define OPTIONS_CRITICAL	1
#define OPTIONS_EXTENSIONS	2
static void
prepare_options_buf(struct sshbuf *c, int which)
{
	struct sshbuf *b;
	size_t i;
	int r;
	const struct cert_ext *ext;

	if ((b = sshbuf_new()) == NULL)
		fatal_f("sshbuf_new failed");
	sshbuf_reset(c);
	for (i = 0; i < ncert_ext; i++) {
		ext = &cert_ext[i];
		if ((ext->crit && (which & OPTIONS_EXTENSIONS)) ||
		    (!ext->crit && (which & OPTIONS_CRITICAL)))
			continue;
		if (ext->val == NULL) {
			/* flag option */
			debug3_f("%s", ext->key);
			if ((r = sshbuf_put_cstring(c, ext->key)) != 0 ||
			    (r = sshbuf_put_string(c, NULL, 0)) != 0)
				fatal_fr(r, "prepare flag");
		} else {
			/* key/value option */
			debug3_f("%s=%s", ext->key, ext->val);
			sshbuf_reset(b);
			if ((r = sshbuf_put_cstring(c, ext->key)) != 0 ||
			    (r = sshbuf_put_cstring(b, ext->val)) != 0 ||
			    (r = sshbuf_put_stringb(c, b)) != 0)
				fatal_fr(r, "prepare k/v");
		}
	}
	sshbuf_free(b);
}

static void
finalise_cert_exts(void)
{
	/* critical options */
	if (certflags_command != NULL)
		cert_ext_add("force-command", certflags_command, 1);
	if (certflags_src_addr != NULL)
		cert_ext_add("source-address", certflags_src_addr, 1);
	if ((certflags_flags & CERTOPT_REQUIRE_VERIFY) != 0)
		cert_ext_add("verify-required", NULL, 1);
	/* extensions */
	if ((certflags_flags & CERTOPT_X_FWD) != 0)
		cert_ext_add("permit-X11-forwarding", NULL, 0);
	if ((certflags_flags & CERTOPT_AGENT_FWD) != 0)
		cert_ext_add("permit-agent-forwarding", NULL, 0);
	if ((certflags_flags & CERTOPT_PORT_FWD) != 0)
		cert_ext_add("permit-port-forwarding", NULL, 0);
	if ((certflags_flags & CERTOPT_PTY) != 0)
		cert_ext_add("permit-pty", NULL, 0);
	if ((certflags_flags & CERTOPT_USER_RC) != 0)
		cert_ext_add("permit-user-rc", NULL, 0);
	if ((certflags_flags & CERTOPT_NO_REQUIRE_USER_PRESENCE) != 0)
		cert_ext_add("no-touch-required", NULL, 0);
	/* order lexically by key */
	if (ncert_ext > 0)
		qsort(cert_ext, ncert_ext, sizeof(*cert_ext), cert_ext_cmp);
}

static struct sshkey *
load_pkcs11_key(char *path)
{
#ifdef ENABLE_PKCS11
	struct sshkey **keys = NULL, *public, *private = NULL;
	int r, i, nkeys;

	if ((r = sshkey_load_public(path, &public, NULL)) != 0)
		fatal_r(r, "Couldn't load CA public key \"%s\"", path);

	nkeys = pkcs11_add_provider(pkcs11provider, identity_passphrase,
	    &keys, NULL);
	debug3_f("%d keys", nkeys);
	if (nkeys <= 0)
		fatal("cannot read public key from pkcs11");
	for (i = 0; i < nkeys; i++) {
		if (sshkey_equal_public(public, keys[i])) {
			private = keys[i];
			continue;
		}
		sshkey_free(keys[i]);
	}
	free(keys);
	sshkey_free(public);
	return private;
#else
	fatal("no pkcs11 support");
#endif /* ENABLE_PKCS11 */
}

/* Signer for sshkey_certify_custom that uses the agent */
static int
agent_signer(struct sshkey *key, u_char **sigp, size_t *lenp,
    const u_char *data, size_t datalen,
    const char *alg, const char *provider, const char *pin,
    u_int compat, void *ctx)
{
	int *agent_fdp = (int *)ctx;

	return ssh_agent_sign(*agent_fdp, key, sigp, lenp,
	    data, datalen, alg, compat);
}

static void
do_ca_sign(struct passwd *pw, const char *ca_key_path, int prefer_agent,
    unsigned long long cert_serial, int cert_serial_autoinc,
    int argc, char **argv)
{
	int r, i, found, agent_fd = -1;
	u_int n;
	struct sshkey *ca, *public;
	char valid[64], *otmp, *tmp, *cp, *out, *comment;
	char *ca_fp = NULL, **plist = NULL, *pin = NULL;
	struct ssh_identitylist *agent_ids;
	size_t j;
	struct notifier_ctx *notifier = NULL;

#ifdef ENABLE_PKCS11
	pkcs11_init(1);
#endif
	tmp = tilde_expand_filename(ca_key_path, pw->pw_uid);
	if (pkcs11provider != NULL) {
		/* If a PKCS#11 token was specified then try to use it */
		if ((ca = load_pkcs11_key(tmp)) == NULL)
			fatal("No PKCS#11 key matching %s found", ca_key_path);
	} else if (prefer_agent) {
		/*
		 * Agent signature requested. Try to use agent after making
		 * sure the public key specified is actually present in the
		 * agent.
		 */
		if ((r = sshkey_load_public(tmp, &ca, NULL)) != 0)
			fatal_r(r, "Cannot load CA public key %s", tmp);
		if ((r = ssh_get_authentication_socket(&agent_fd)) != 0)
			fatal_r(r, "Cannot use public key for CA signature");
		if ((r = ssh_fetch_identitylist(agent_fd, &agent_ids)) != 0)
			fatal_r(r, "Retrieve agent key list");
		found = 0;
		for (j = 0; j < agent_ids->nkeys; j++) {
			if (sshkey_equal(ca, agent_ids->keys[j])) {
				found = 1;
				break;
			}
		}
		if (!found)
			fatal("CA key %s not found in agent", tmp);
		ssh_free_identitylist(agent_ids);
		ca->flags |= SSHKEY_FLAG_EXT;
	} else {
		/* CA key is assumed to be a private key on the filesystem */
		ca = load_identity(tmp, NULL);
		if (sshkey_is_sk(ca) &&
		    (ca->sk_flags & SSH_SK_USER_VERIFICATION_REQD)) {
			if ((pin = read_passphrase("Enter PIN for CA key: ",
			    RP_ALLOW_STDIN)) == NULL)
				fatal_f("couldn't read PIN");
		}
	}
	free(tmp);

	if (key_type_name != NULL) {
		if (sshkey_type_from_name(key_type_name) != ca->type) {
			fatal("CA key type %s doesn't match specified %s",
			    sshkey_ssh_name(ca), key_type_name);
		}
	} else if (ca->type == KEY_RSA) {
		/* Default to a good signature algorithm */
		key_type_name = "rsa-sha2-512";
	}
	ca_fp = sshkey_fingerprint(ca, fingerprint_hash, SSH_FP_DEFAULT);

	finalise_cert_exts();
	for (i = 0; i < argc; i++) {
		/* Split list of principals */
		n = 0;
		if (cert_principals != NULL) {
			otmp = tmp = xstrdup(cert_principals);
			plist = NULL;
			for (; (cp = strsep(&tmp, ",")) != NULL; n++) {
				plist = xreallocarray(plist, n + 1, sizeof(*plist));
				if (*(plist[n] = xstrdup(cp)) == '\0')
					fatal("Empty principal name");
			}
			free(otmp);
		}
		if (n > SSHKEY_CERT_MAX_PRINCIPALS)
			fatal("Too many certificate principals specified");

		tmp = tilde_expand_filename(argv[i], pw->pw_uid);
		if ((r = sshkey_load_public(tmp, &public, &comment)) != 0)
			fatal_r(r, "load pubkey \"%s\"", tmp);
		if (sshkey_is_cert(public))
			fatal_f("key \"%s\" type %s cannot be certified",
			    tmp, sshkey_type(public));

		/* Prepare certificate to sign */
		if ((r = sshkey_to_certified(public)) != 0)
			fatal_r(r, "Could not upgrade key %s to certificate", tmp);
		public->cert->type = cert_key_type;
		public->cert->serial = (u_int64_t)cert_serial;
		public->cert->key_id = xstrdup(cert_key_id);
		public->cert->nprincipals = n;
		public->cert->principals = plist;
		public->cert->valid_after = cert_valid_from;
		public->cert->valid_before = cert_valid_to;
		prepare_options_buf(public->cert->critical, OPTIONS_CRITICAL);
		prepare_options_buf(public->cert->extensions,
		    OPTIONS_EXTENSIONS);
		if ((r = sshkey_from_private(ca,
		    &public->cert->signature_key)) != 0)
			fatal_r(r, "sshkey_from_private (ca key)");

		if (agent_fd != -1 && (ca->flags & SSHKEY_FLAG_EXT) != 0) {
			if ((r = sshkey_certify_custom(public, ca,
			    key_type_name, sk_provider, NULL, agent_signer,
			    &agent_fd)) != 0)
				fatal_r(r, "Couldn't certify %s via agent", tmp);
		} else {
			if (sshkey_is_sk(ca) &&
			    (ca->sk_flags & SSH_SK_USER_PRESENCE_REQD)) {
				notifier = notify_start(0,
				    "Confirm user presence for key %s %s",
				    sshkey_type(ca), ca_fp);
			}
			r = sshkey_certify(public, ca, key_type_name,
			    sk_provider, pin);
			notify_complete(notifier, "User presence confirmed");
			if (r != 0)
				fatal_r(r, "Couldn't certify key %s", tmp);
		}

		if ((cp = strrchr(tmp, '.')) != NULL && strcmp(cp, ".pub") == 0)
			*cp = '\0';
		xasprintf(&out, "%s-cert.pub", tmp);
		free(tmp);

		if ((r = sshkey_save_public(public, out, comment)) != 0) {
			fatal_r(r, "Unable to save public key to %s",
			    identity_file);
		}

		if (!quiet) {
			sshkey_format_cert_validity(public->cert,
			    valid, sizeof(valid));
			logit("Signed %s key %s: id \"%s\" serial %llu%s%s "
			    "valid %s", sshkey_cert_type(public),
			    out, public->cert->key_id,
			    (unsigned long long)public->cert->serial,
			    cert_principals != NULL ? " for " : "",
			    cert_principals != NULL ? cert_principals : "",
			    valid);
		}

		sshkey_free(public);
		free(out);
		if (cert_serial_autoinc)
			cert_serial++;
	}
	if (pin != NULL)
		freezero(pin, strlen(pin));
	free(ca_fp);
#ifdef ENABLE_PKCS11
	pkcs11_terminate();
#endif
	exit(0);
}

static u_int64_t
parse_relative_time(const char *s, time_t now)
{
	int64_t mul, secs;

	mul = *s == '-' ? -1 : 1;

	if ((secs = convtime(s + 1)) == -1)
		fatal("Invalid relative certificate time %s", s);
	if (mul == -1 && secs > now)
		fatal("Certificate time %s cannot be represented", s);
	return now + (u_int64_t)(secs * mul);
}

static void
parse_hex_u64(const char *s, uint64_t *up)
{
	char *ep;
	unsigned long long ull;

	errno = 0;
	ull = strtoull(s, &ep, 16);
	if (*s == '\0' || *ep != '\0')
		fatal("Invalid certificate time: not a number");
	if (errno == ERANGE && ull == ULONG_MAX)
		fatal_fr(SSH_ERR_SYSTEM_ERROR, "Invalid certificate time");
	*up = (uint64_t)ull;
}

static void
parse_cert_times(char *timespec)
{
	char *from, *to;
	time_t now = time(NULL);
	int64_t secs;

	/* +timespec relative to now */
	if (*timespec == '+' && strchr(timespec, ':') == NULL) {
		if ((secs = convtime(timespec + 1)) == -1)
			fatal("Invalid relative certificate life %s", timespec);
		cert_valid_to = now + secs;
		/*
		 * Backdate certificate one minute to avoid problems on hosts
		 * with poorly-synchronised clocks.
		 */
		cert_valid_from = ((now - 59)/ 60) * 60;
		return;
	}

	/*
	 * from:to, where
	 * from := [+-]timespec | YYYYMMDD | YYYYMMDDHHMMSS | 0x... | "always"
	 *   to := [+-]timespec | YYYYMMDD | YYYYMMDDHHMMSS | 0x... | "forever"
	 */
	from = xstrdup(timespec);
	to = strchr(from, ':');
	if (to == NULL || from == to || *(to + 1) == '\0')
		fatal("Invalid certificate life specification %s", timespec);
	*to++ = '\0';

	if (*from == '-' || *from == '+')
		cert_valid_from = parse_relative_time(from, now);
	else if (strcmp(from, "always") == 0)
		cert_valid_from = 0;
	else if (strncmp(from, "0x", 2) == 0)
		parse_hex_u64(from, &cert_valid_from);
	else if (parse_absolute_time(from, &cert_valid_from) != 0)
		fatal("Invalid from time \"%s\"", from);

	if (*to == '-' || *to == '+')
		cert_valid_to = parse_relative_time(to, now);
	else if (strcmp(to, "forever") == 0)
		cert_valid_to = ~(u_int64_t)0;
	else if (strncmp(to, "0x", 2) == 0)
		parse_hex_u64(to, &cert_valid_to);
	else if (parse_absolute_time(to, &cert_valid_to) != 0)
		fatal("Invalid to time \"%s\"", to);

	if (cert_valid_to <= cert_valid_from)
		fatal("Empty certificate validity interval");
	free(from);
}

static void
add_cert_option(char *opt)
{
	char *val, *cp;
	int iscrit = 0;

	if (strcasecmp(opt, "clear") == 0)
		certflags_flags = 0;
	else if (strcasecmp(opt, "no-x11-forwarding") == 0)
		certflags_flags &= ~CERTOPT_X_FWD;
	else if (strcasecmp(opt, "permit-x11-forwarding") == 0)
		certflags_flags |= CERTOPT_X_FWD;
	else if (strcasecmp(opt, "no-agent-forwarding") == 0)
		certflags_flags &= ~CERTOPT_AGENT_FWD;
	else if (strcasecmp(opt, "permit-agent-forwarding") == 0)
		certflags_flags |= CERTOPT_AGENT_FWD;
	else if (strcasecmp(opt, "no-port-forwarding") == 0)
		certflags_flags &= ~CERTOPT_PORT_FWD;
	else if (strcasecmp(opt, "permit-port-forwarding") == 0)
		certflags_flags |= CERTOPT_PORT_FWD;
	else if (strcasecmp(opt, "no-pty") == 0)
		certflags_flags &= ~CERTOPT_PTY;
	else if (strcasecmp(opt, "permit-pty") == 0)
		certflags_flags |= CERTOPT_PTY;
	else if (strcasecmp(opt, "no-user-rc") == 0)
		certflags_flags &= ~CERTOPT_USER_RC;
	else if (strcasecmp(opt, "permit-user-rc") == 0)
		certflags_flags |= CERTOPT_USER_RC;
	else if (strcasecmp(opt, "touch-required") == 0)
		certflags_flags &= ~CERTOPT_NO_REQUIRE_USER_PRESENCE;
	else if (strcasecmp(opt, "no-touch-required") == 0)
		certflags_flags |= CERTOPT_NO_REQUIRE_USER_PRESENCE;
	else if (strcasecmp(opt, "no-verify-required") == 0)
		certflags_flags &= ~CERTOPT_REQUIRE_VERIFY;
	else if (strcasecmp(opt, "verify-required") == 0)
		certflags_flags |= CERTOPT_REQUIRE_VERIFY;
	else if (strncasecmp(opt, "force-command=", 14) == 0) {
		val = opt + 14;
		if (*val == '\0')
			fatal("Empty force-command option");
		if (certflags_command != NULL)
			fatal("force-command already specified");
		certflags_command = xstrdup(val);
	} else if (strncasecmp(opt, "source-address=", 15) == 0) {
		val = opt + 15;
		if (*val == '\0')
			fatal("Empty source-address option");
		if (certflags_src_addr != NULL)
			fatal("source-address already specified");
		if (addr_match_cidr_list(NULL, val) != 0)
			fatal("Invalid source-address list");
		certflags_src_addr = xstrdup(val);
	} else if (strncasecmp(opt, "extension:", 10) == 0 ||
		    (iscrit = (strncasecmp(opt, "critical:", 9) == 0))) {
		val = xstrdup(strchr(opt, ':') + 1);
		if ((cp = strchr(val, '=')) != NULL)
			*cp++ = '\0';
		cert_ext_add(val, cp, iscrit);
		free(val);
	} else
		fatal("Unsupported certificate option \"%s\"", opt);
}

static void
show_options(struct sshbuf *optbuf, int in_critical)
{
	char *name, *arg, *hex;
	struct sshbuf *options, *option = NULL;
	int r;

	if ((options = sshbuf_fromb(optbuf)) == NULL)
		fatal_f("sshbuf_fromb failed");
	while (sshbuf_len(options) != 0) {
		sshbuf_free(option);
		option = NULL;
		if ((r = sshbuf_get_cstring(options, &name, NULL)) != 0 ||
		    (r = sshbuf_froms(options, &option)) != 0)
			fatal_fr(r, "parse option");
		printf("                %s", name);
		if (!in_critical &&
		    (strcmp(name, "permit-X11-forwarding") == 0 ||
		    strcmp(name, "permit-agent-forwarding") == 0 ||
		    strcmp(name, "permit-port-forwarding") == 0 ||
		    strcmp(name, "permit-pty") == 0 ||
		    strcmp(name, "permit-user-rc") == 0 ||
		    strcmp(name, "no-touch-required") == 0)) {
			printf("\n");
		} else if (in_critical &&
		    (strcmp(name, "force-command") == 0 ||
		    strcmp(name, "source-address") == 0)) {
			if ((r = sshbuf_get_cstring(option, &arg, NULL)) != 0)
				fatal_fr(r, "parse critical");
			printf(" %s\n", arg);
			free(arg);
		} else if (in_critical &&
		    strcmp(name, "verify-required") == 0) {
			printf("\n");
		} else if (sshbuf_len(option) > 0) {
			hex = sshbuf_dtob16(option);
			printf(" UNKNOWN OPTION: %s (len %zu)\n",
			    hex, sshbuf_len(option));
			sshbuf_reset(option);
			free(hex);
		} else
			printf(" UNKNOWN FLAG OPTION\n");
		free(name);
		if (sshbuf_len(option) != 0)
			fatal("Option corrupt: extra data at end");
	}
	sshbuf_free(option);
	sshbuf_free(options);
}

static void
print_cert(struct sshkey *key)
{
	char valid[64], *key_fp, *ca_fp;
	u_int i;

	key_fp = sshkey_fingerprint(key, fingerprint_hash, SSH_FP_DEFAULT);
	ca_fp = sshkey_fingerprint(key->cert->signature_key,
	    fingerprint_hash, SSH_FP_DEFAULT);
	if (key_fp == NULL || ca_fp == NULL)
		fatal_f("sshkey_fingerprint fail");
	sshkey_format_cert_validity(key->cert, valid, sizeof(valid));

	printf("        Type: %s %s certificate\n", sshkey_ssh_name(key),
	    sshkey_cert_type(key));
	printf("        Public key: %s %s\n", sshkey_type(key), key_fp);
	printf("        Signing CA: %s %s (using %s)\n",
	    sshkey_type(key->cert->signature_key), ca_fp,
	    key->cert->signature_type);
	printf("        Key ID: \"%s\"\n", key->cert->key_id);
	printf("        Serial: %llu\n", (unsigned long long)key->cert->serial);
	printf("        Valid: %s\n", valid);
	printf("        Principals: ");
	if (key->cert->nprincipals == 0)
		printf("(none)\n");
	else {
		for (i = 0; i < key->cert->nprincipals; i++)
			printf("\n                %s",
			    key->cert->principals[i]);
		printf("\n");
	}
	printf("        Critical Options: ");
	if (sshbuf_len(key->cert->critical) == 0)
		printf("(none)\n");
	else {
		printf("\n");
		show_options(key->cert->critical, 1);
	}
	printf("        Extensions: ");
	if (sshbuf_len(key->cert->extensions) == 0)
		printf("(none)\n");
	else {
		printf("\n");
		show_options(key->cert->extensions, 0);
	}
}

static void
do_show_cert(struct passwd *pw)
{
	struct sshkey *key = NULL;
	struct stat st;
	int r, is_stdin = 0, ok = 0;
	FILE *f;
	char *cp, *line = NULL;
	const char *path;
	size_t linesize = 0;
	u_long lnum = 0;

	if (!have_identity)
		ask_filename(pw, "Enter file in which the key is");
	if (strcmp(identity_file, "-") != 0 && stat(identity_file, &st) == -1)
		fatal("%s: %s: %s", __progname, identity_file, strerror(errno));

	path = identity_file;
	if (strcmp(path, "-") == 0) {
		f = stdin;
		path = "(stdin)";
		is_stdin = 1;
	} else if ((f = fopen(identity_file, "r")) == NULL)
		fatal("fopen %s: %s", identity_file, strerror(errno));

	while (getline(&line, &linesize, f) != -1) {
		lnum++;
		sshkey_free(key);
		key = NULL;
		/* Trim leading space and comments */
		cp = line + strspn(line, " \t");
		if (*cp == '#' || *cp == '\0')
			continue;
		if ((key = sshkey_new(KEY_UNSPEC)) == NULL)
			fatal("sshkey_new");
		if ((r = sshkey_read(key, &cp)) != 0) {
			error_r(r, "%s:%lu: invalid key", path, lnum);
			continue;
		}
		if (!sshkey_is_cert(key)) {
			error("%s:%lu is not a certificate", path, lnum);
			continue;
		}
		ok = 1;
		if (!is_stdin && lnum == 1)
			printf("%s:\n", path);
		else
			printf("%s:%lu:\n", path, lnum);
		print_cert(key);
	}
	free(line);
	sshkey_free(key);
	fclose(f);
	exit(ok ? 0 : 1);
}

static void
load_krl(const char *path, struct ssh_krl **krlp)
{
	struct sshbuf *krlbuf;
	int r;

	if ((r = sshbuf_load_file(path, &krlbuf)) != 0)
		fatal_r(r, "Unable to load KRL %s", path);
	/* XXX check sigs */
	if ((r = ssh_krl_from_blob(krlbuf, krlp, NULL, 0)) != 0 ||
	    *krlp == NULL)
		fatal_r(r, "Invalid KRL file %s", path);
	sshbuf_free(krlbuf);
}

static void
hash_to_blob(const char *cp, u_char **blobp, size_t *lenp,
    const char *file, u_long lnum)
{
	char *tmp;
	size_t tlen;
	struct sshbuf *b;
	int r;

	if (strncmp(cp, "SHA256:", 7) != 0)
		fatal("%s:%lu: unsupported hash algorithm", file, lnum);
	cp += 7;

	/*
	 * OpenSSH base64 hashes omit trailing '='
	 * characters; put them back for decode.
	 */
	tlen = strlen(cp);
	tmp = xmalloc(tlen + 4 + 1);
	strlcpy(tmp, cp, tlen + 1);
	while ((tlen % 4) != 0) {
		tmp[tlen++] = '=';
		tmp[tlen] = '\0';
	}
	if ((b = sshbuf_new()) == NULL)
		fatal_f("sshbuf_new failed");
	if ((r = sshbuf_b64tod(b, tmp)) != 0)
		fatal_r(r, "%s:%lu: decode hash failed", file, lnum);
	free(tmp);
	*lenp = sshbuf_len(b);
	*blobp = xmalloc(*lenp);
	memcpy(*blobp, sshbuf_ptr(b), *lenp);
	sshbuf_free(b);
}

static void
update_krl_from_file(struct passwd *pw, const char *file, int wild_ca,
    const struct sshkey *ca, struct ssh_krl *krl)
{
	struct sshkey *key = NULL;
	u_long lnum = 0;
	char *path, *cp, *ep, *line = NULL;
	u_char *blob = NULL;
	size_t blen = 0, linesize = 0;
	unsigned long long serial, serial2;
	int i, was_explicit_key, was_sha1, was_sha256, was_hash, r;
	FILE *krl_spec;

	path = tilde_expand_filename(file, pw->pw_uid);
	if (strcmp(path, "-") == 0) {
		krl_spec = stdin;
		free(path);
		path = xstrdup("(standard input)");
	} else if ((krl_spec = fopen(path, "r")) == NULL)
		fatal("fopen %s: %s", path, strerror(errno));

	if (!quiet)
		printf("Revoking from %s\n", path);
	while (getline(&line, &linesize, krl_spec) != -1) {
		lnum++;
		was_explicit_key = was_sha1 = was_sha256 = was_hash = 0;
		cp = line + strspn(line, " \t");
		/* Trim trailing space, comments and strip \n */
		for (i = 0, r = -1; cp[i] != '\0'; i++) {
			if (cp[i] == '#' || cp[i] == '\n') {
				cp[i] = '\0';
				break;
			}
			if (cp[i] == ' ' || cp[i] == '\t') {
				/* Remember the start of a span of whitespace */
				if (r == -1)
					r = i;
			} else
				r = -1;
		}
		if (r != -1)
			cp[r] = '\0';
		if (*cp == '\0')
			continue;
		if (strncasecmp(cp, "serial:", 7) == 0) {
			if (ca == NULL && !wild_ca) {
				fatal("revoking certificates by serial number "
				    "requires specification of a CA key");
			}
			cp += 7;
			cp = cp + strspn(cp, " \t");
			errno = 0;
			serial = strtoull(cp, &ep, 0);
			if (*cp == '\0' || (*ep != '\0' && *ep != '-'))
				fatal("%s:%lu: invalid serial \"%s\"",
				    path, lnum, cp);
			if (errno == ERANGE && serial == ULLONG_MAX)
				fatal("%s:%lu: serial out of range",
				    path, lnum);
			serial2 = serial;
			if (*ep == '-') {
				cp = ep + 1;
				errno = 0;
				serial2 = strtoull(cp, &ep, 0);
				if (*cp == '\0' || *ep != '\0')
					fatal("%s:%lu: invalid serial \"%s\"",
					    path, lnum, cp);
				if (errno == ERANGE && serial2 == ULLONG_MAX)
					fatal("%s:%lu: serial out of range",
					    path, lnum);
				if (serial2 <= serial)
					fatal("%s:%lu: invalid serial range "
					    "%llu:%llu", path, lnum,
					    (unsigned long long)serial,
					    (unsigned long long)serial2);
			}
			if (ssh_krl_revoke_cert_by_serial_range(krl,
			    ca, serial, serial2) != 0) {
				fatal_f("revoke serial failed");
			}
		} else if (strncasecmp(cp, "id:", 3) == 0) {
			if (ca == NULL && !wild_ca) {
				fatal("revoking certificates by key ID "
				    "requires specification of a CA key");
			}
			cp += 3;
			cp = cp + strspn(cp, " \t");
			if (ssh_krl_revoke_cert_by_key_id(krl, ca, cp) != 0)
				fatal_f("revoke key ID failed");
		} else if (strncasecmp(cp, "hash:", 5) == 0) {
			cp += 5;
			cp = cp + strspn(cp, " \t");
			hash_to_blob(cp, &blob, &blen, file, lnum);
			r = ssh_krl_revoke_key_sha256(krl, blob, blen);
			if (r != 0)
				fatal_fr(r, "revoke key failed");
		} else {
			if (strncasecmp(cp, "key:", 4) == 0) {
				cp += 4;
				cp = cp + strspn(cp, " \t");
				was_explicit_key = 1;
			} else if (strncasecmp(cp, "sha1:", 5) == 0) {
				cp += 5;
				cp = cp + strspn(cp, " \t");
				was_sha1 = 1;
			} else if (strncasecmp(cp, "sha256:", 7) == 0) {
				cp += 7;
				cp = cp + strspn(cp, " \t");
				was_sha256 = 1;
				/*
				 * Just try to process the line as a key.
				 * Parsing will fail if it isn't.
				 */
			}
			if ((key = sshkey_new(KEY_UNSPEC)) == NULL)
				fatal("sshkey_new");
			if ((r = sshkey_read(key, &cp)) != 0)
				fatal_r(r, "%s:%lu: invalid key", path, lnum);
			if (was_explicit_key)
				r = ssh_krl_revoke_key_explicit(krl, key);
			else if (was_sha1) {
				if (sshkey_fingerprint_raw(key,
				    SSH_DIGEST_SHA1, &blob, &blen) != 0) {
					fatal("%s:%lu: fingerprint failed",
					    file, lnum);
				}
				r = ssh_krl_revoke_key_sha1(krl, blob, blen);
			} else if (was_sha256) {
				if (sshkey_fingerprint_raw(key,
				    SSH_DIGEST_SHA256, &blob, &blen) != 0) {
					fatal("%s:%lu: fingerprint failed",
					    file, lnum);
				}
				r = ssh_krl_revoke_key_sha256(krl, blob, blen);
			} else
				r = ssh_krl_revoke_key(krl, key);
			if (r != 0)
				fatal_fr(r, "revoke key failed");
			freezero(blob, blen);
			blob = NULL;
			blen = 0;
			sshkey_free(key);
		}
	}
	if (strcmp(path, "-") != 0)
		fclose(krl_spec);
	free(line);
	free(path);
}

static void
do_gen_krl(struct passwd *pw, int updating, const char *ca_key_path,
    unsigned long long krl_version, const char *krl_comment,
    int argc, char **argv)
{
	struct ssh_krl *krl;
	struct stat sb;
	struct sshkey *ca = NULL;
	int i, r, wild_ca = 0;
	char *tmp;
	struct sshbuf *kbuf;

	if (*identity_file == '\0')
		fatal("KRL generation requires an output file");
	if (stat(identity_file, &sb) == -1) {
		if (errno != ENOENT)
			fatal("Cannot access KRL \"%s\": %s",
			    identity_file, strerror(errno));
		if (updating)
			fatal("KRL \"%s\" does not exist", identity_file);
	}
	if (ca_key_path != NULL) {
		if (strcasecmp(ca_key_path, "none") == 0)
			wild_ca = 1;
		else {
			tmp = tilde_expand_filename(ca_key_path, pw->pw_uid);
			if ((r = sshkey_load_public(tmp, &ca, NULL)) != 0)
				fatal_r(r, "Cannot load CA public key %s", tmp);
			free(tmp);
		}
	}

	if (updating)
		load_krl(identity_file, &krl);
	else if ((krl = ssh_krl_init()) == NULL)
		fatal("couldn't create KRL");

	if (krl_version != 0)
		ssh_krl_set_version(krl, krl_version);
	if (krl_comment != NULL)
		ssh_krl_set_comment(krl, krl_comment);

	for (i = 0; i < argc; i++)
		update_krl_from_file(pw, argv[i], wild_ca, ca, krl);

	if ((kbuf = sshbuf_new()) == NULL)
		fatal("sshbuf_new failed");
	if (ssh_krl_to_blob(krl, kbuf, NULL, 0) != 0)
		fatal("Couldn't generate KRL");
	if ((r = sshbuf_write_file(identity_file, kbuf)) != 0)
		fatal("write %s: %s", identity_file, strerror(errno));
	sshbuf_free(kbuf);
	ssh_krl_free(krl);
	sshkey_free(ca);
}

static void
do_check_krl(struct passwd *pw, int print_krl, int argc, char **argv)
{
	int i, r, ret = 0;
	char *comment;
	struct ssh_krl *krl;
	struct sshkey *k;

	if (*identity_file == '\0')
		fatal("KRL checking requires an input file");
	load_krl(identity_file, &krl);
	if (print_krl)
		krl_dump(krl, stdout);
	for (i = 0; i < argc; i++) {
		if ((r = sshkey_load_public(argv[i], &k, &comment)) != 0)
			fatal_r(r, "Cannot load public key %s", argv[i]);
		r = ssh_krl_check_key(krl, k);
		printf("%s%s%s%s: %s\n", argv[i],
		    *comment ? " (" : "", comment, *comment ? ")" : "",
		    r == 0 ? "ok" : "REVOKED");
		if (r != 0)
			ret = 1;
		sshkey_free(k);
		free(comment);
	}
	ssh_krl_free(krl);
	exit(ret);
}

static struct sshkey *
load_sign_key(const char *keypath, const struct sshkey *pubkey)
{
	size_t i, slen, plen = strlen(keypath);
	char *privpath = xstrdup(keypath);
	static const char * const suffixes[] = { "-cert.pub", ".pub", NULL };
	struct sshkey *ret = NULL, *privkey = NULL;
	int r, waspub = 0;
	struct stat st;

	/*
	 * If passed a public key filename, then try to locate the corresponding
	 * private key. This lets us specify certificates on the command-line
	 * and have ssh-keygen find the appropriate private key.
	 */
	for (i = 0; suffixes[i]; i++) {
		slen = strlen(suffixes[i]);
		if (plen <= slen ||
		    strcmp(privpath + plen - slen, suffixes[i]) != 0)
			continue;
		privpath[plen - slen] = '\0';
		debug_f("%s looks like a public key, using private key "
		    "path %s instead", keypath, privpath);
		waspub = 1;
	}
	if (waspub && stat(privpath, &st) != 0 && errno == ENOENT)
		fatal("No private key found for public key \"%s\"", keypath);
	if ((r = sshkey_load_private(privpath, "", &privkey, NULL)) != 0 &&
	    (r != SSH_ERR_KEY_WRONG_PASSPHRASE)) {
		debug_fr(r, "load private key \"%s\"", privpath);
		fatal("No private key found for \"%s\"", privpath);
	} else if (privkey == NULL)
		privkey = load_identity(privpath, NULL);

	if (!sshkey_equal_public(pubkey, privkey)) {
		error("Public key %s doesn't match private %s",
		    keypath, privpath);
		goto done;
	}
	if (sshkey_is_cert(pubkey) && !sshkey_is_cert(privkey)) {
		/*
		 * Graft the certificate onto the private key to make
		 * it capable of signing.
		 */
		if ((r = sshkey_to_certified(privkey)) != 0) {
			error_fr(r, "sshkey_to_certified");
			goto done;
		}
		if ((r = sshkey_cert_copy(pubkey, privkey)) != 0) {
			error_fr(r, "sshkey_cert_copy");
			goto done;
		}
	}
	/* success */
	ret = privkey;
	privkey = NULL;
 done:
	sshkey_free(privkey);
	free(privpath);
	return ret;
}

static int
sign_one(struct sshkey *signkey, const char *filename, int fd,
    const char *sig_namespace, const char *hashalg, sshsig_signer *signer,
    void *signer_ctx)
{
	struct sshbuf *sigbuf = NULL, *abuf = NULL;
	int r = SSH_ERR_INTERNAL_ERROR, wfd = -1, oerrno;
	char *wfile = NULL, *asig = NULL, *fp = NULL;
	char *pin = NULL, *prompt = NULL;

	if (!quiet) {
		if (fd == STDIN_FILENO)
			fprintf(stderr, "Signing data on standard input\n");
		else
			fprintf(stderr, "Signing file %s\n", filename);
	}
	if (signer == NULL && sshkey_is_sk(signkey)) {
		if ((signkey->sk_flags & SSH_SK_USER_VERIFICATION_REQD)) {
			xasprintf(&prompt, "Enter PIN for %s key: ",
			    sshkey_type(signkey));
			if ((pin = read_passphrase(prompt,
			    RP_ALLOW_STDIN)) == NULL)
				fatal_f("couldn't read PIN");
		}
		if ((signkey->sk_flags & SSH_SK_USER_PRESENCE_REQD)) {
			if ((fp = sshkey_fingerprint(signkey, fingerprint_hash,
			    SSH_FP_DEFAULT)) == NULL)
				fatal_f("fingerprint failed");
			fprintf(stderr, "Confirm user presence for key %s %s\n",
			    sshkey_type(signkey), fp);
			free(fp);
		}
	}
	if ((r = sshsig_sign_fd(signkey, hashalg, sk_provider, pin,
	    fd, sig_namespace, &sigbuf, signer, signer_ctx)) != 0) {
		error_r(r, "Signing %s failed", filename);
		goto out;
	}
	if ((r = sshsig_armor(sigbuf, &abuf)) != 0) {
		error_fr(r, "sshsig_armor");
		goto out;
	}
	if ((asig = sshbuf_dup_string(abuf)) == NULL) {
		error_f("buffer error");
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}

	if (fd == STDIN_FILENO) {
		fputs(asig, stdout);
		fflush(stdout);
	} else {
		xasprintf(&wfile, "%s.sig", filename);
		if (confirm_overwrite(wfile)) {
			if ((wfd = open(wfile, O_WRONLY|O_CREAT|O_TRUNC,
			    0666)) == -1) {
				oerrno = errno;
				error("Cannot open %s: %s",
				    wfile, strerror(errno));
				errno = oerrno;
				r = SSH_ERR_SYSTEM_ERROR;
				goto out;
			}
			if (atomicio(vwrite, wfd, asig,
			    strlen(asig)) != strlen(asig)) {
				oerrno = errno;
				error("Cannot write to %s: %s",
				    wfile, strerror(errno));
				errno = oerrno;
				r = SSH_ERR_SYSTEM_ERROR;
				goto out;
			}
			if (!quiet) {
				fprintf(stderr, "Write signature to %s\n",
				    wfile);
			}
		}
	}
	/* success */
	r = 0;
 out:
	free(wfile);
	free(prompt);
	free(asig);
	if (pin != NULL)
		freezero(pin, strlen(pin));
	sshbuf_free(abuf);
	sshbuf_free(sigbuf);
	if (wfd != -1)
		close(wfd);
	return r;
}

static int
sig_process_opts(char * const *opts, size_t nopts, char **hashalgp,
    uint64_t *verify_timep, int *print_pubkey)
{
	size_t i;
	time_t now;

	if (verify_timep != NULL)
		*verify_timep = 0;
	if (print_pubkey != NULL)
		*print_pubkey = 0;
	if (hashalgp != NULL)
		*hashalgp = NULL;
	for (i = 0; i < nopts; i++) {
		if (hashalgp != NULL &&
		    strncasecmp(opts[i], "hashalg=", 8) == 0) {
			*hashalgp = xstrdup(opts[i] + 8);
		} else if (verify_timep &&
		    strncasecmp(opts[i], "verify-time=", 12) == 0) {
			if (parse_absolute_time(opts[i] + 12,
			    verify_timep) != 0 || *verify_timep == 0) {
				error("Invalid \"verify-time\" option");
				return SSH_ERR_INVALID_ARGUMENT;
			}
		} else if (print_pubkey &&
		    strcasecmp(opts[i], "print-pubkey") == 0) {
			*print_pubkey = 1;
		} else {
			error("Invalid option \"%s\"", opts[i]);
			return SSH_ERR_INVALID_ARGUMENT;
		}
	}
	if (verify_timep && *verify_timep == 0) {
		if ((now = time(NULL)) < 0) {
			error("Time is before epoch");
			return SSH_ERR_INVALID_ARGUMENT;
		}
		*verify_timep = (uint64_t)now;
	}
	return 0;
}


static int
sig_sign(const char *keypath, const char *sig_namespace, int require_agent,
    int argc, char **argv, char * const *opts, size_t nopts)
{
	int i, fd = -1, r, ret = -1;
	int agent_fd = -1;
	struct sshkey *pubkey = NULL, *privkey = NULL, *signkey = NULL;
	sshsig_signer *signer = NULL;
	char *hashalg = NULL;

	/* Check file arguments. */
	for (i = 0; i < argc; i++) {
		if (strcmp(argv[i], "-") != 0)
			continue;
		if (i > 0 || argc > 1)
			fatal("Cannot sign mix of paths and standard input");
	}

	if (sig_process_opts(opts, nopts, &hashalg, NULL, NULL) != 0)
		goto done; /* error already logged */

	if ((r = sshkey_load_public(keypath, &pubkey, NULL)) != 0) {
		error_r(r, "Couldn't load public key %s", keypath);
		goto done;
	}

	if ((r = ssh_get_authentication_socket(&agent_fd)) != 0) {
		if (require_agent)
			fatal("Couldn't get agent socket");
		debug_r(r, "Couldn't get agent socket");
	} else {
		if ((r = ssh_agent_has_key(agent_fd, pubkey)) == 0)
			signer = agent_signer;
		else {
			if (require_agent)
				fatal("Couldn't find key in agent");
			debug_r(r, "Couldn't find key in agent");
		}
	}

	if (signer == NULL) {
		/* Not using agent - try to load private key */
		if ((privkey = load_sign_key(keypath, pubkey)) == NULL)
			goto done;
		signkey = privkey;
	} else {
		/* Will use key in agent */
		signkey = pubkey;
	}

	if (argc == 0) {
		if ((r = sign_one(signkey, "(stdin)", STDIN_FILENO,
		    sig_namespace, hashalg, signer, &agent_fd)) != 0)
			goto done;
	} else {
		for (i = 0; i < argc; i++) {
			if (strcmp(argv[i], "-") == 0)
				fd = STDIN_FILENO;
			else if ((fd = open(argv[i], O_RDONLY)) == -1) {
				error("Cannot open %s for signing: %s",
				    argv[i], strerror(errno));
				goto done;
			}
			if ((r = sign_one(signkey, argv[i], fd, sig_namespace,
			    hashalg, signer, &agent_fd)) != 0)
				goto done;
			if (fd != STDIN_FILENO)
				close(fd);
			fd = -1;
		}
	}

	ret = 0;
done:
	if (fd != -1 && fd != STDIN_FILENO)
		close(fd);
	sshkey_free(pubkey);
	sshkey_free(privkey);
	free(hashalg);
	return ret;
}

static int
sig_verify(const char *signature, const char *sig_namespace,
    const char *principal, const char *allowed_keys, const char *revoked_keys,
    char * const *opts, size_t nopts)
{
	int r, ret = -1;
	int print_pubkey = 0;
	struct sshbuf *sigbuf = NULL, *abuf = NULL;
	struct sshkey *sign_key = NULL;
	char *fp = NULL;
	struct sshkey_sig_details *sig_details = NULL;
	uint64_t verify_time = 0;

	if (sig_process_opts(opts, nopts, NULL, &verify_time,
	    &print_pubkey) != 0)
		goto done; /* error already logged */

	memset(&sig_details, 0, sizeof(sig_details));
	if ((r = sshbuf_load_file(signature, &abuf)) != 0) {
		error_r(r, "Couldn't read signature file");
		goto done;
	}

	if ((r = sshsig_dearmor(abuf, &sigbuf)) != 0) {
		error_fr(r, "sshsig_armor");
		goto done;
	}
	if ((r = sshsig_verify_fd(sigbuf, STDIN_FILENO, sig_namespace,
	    &sign_key, &sig_details)) != 0)
		goto done; /* sshsig_verify() prints error */

	if ((fp = sshkey_fingerprint(sign_key, fingerprint_hash,
	    SSH_FP_DEFAULT)) == NULL)
		fatal_f("sshkey_fingerprint failed");
	debug("Valid (unverified) signature from key %s", fp);
	if (sig_details != NULL) {
		debug2_f("signature details: counter = %u, flags = 0x%02x",
		    sig_details->sk_counter, sig_details->sk_flags);
	}
	free(fp);
	fp = NULL;

	if (revoked_keys != NULL) {
		if ((r = sshkey_check_revoked(sign_key, revoked_keys)) != 0) {
			debug3_fr(r, "sshkey_check_revoked");
			goto done;
		}
	}

	if (allowed_keys != NULL && (r = sshsig_check_allowed_keys(allowed_keys,
	    sign_key, principal, sig_namespace, verify_time)) != 0) {
		debug3_fr(r, "sshsig_check_allowed_keys");
		goto done;
	}
	/* success */
	ret = 0;
done:
	if (!quiet) {
		if (ret == 0) {
			if ((fp = sshkey_fingerprint(sign_key, fingerprint_hash,
			    SSH_FP_DEFAULT)) == NULL)
				fatal_f("sshkey_fingerprint failed");
			if (principal == NULL) {
				printf("Good \"%s\" signature with %s key %s\n",
				    sig_namespace, sshkey_type(sign_key), fp);

			} else {
				printf("Good \"%s\" signature for %s with %s key %s\n",
				    sig_namespace, principal,
				    sshkey_type(sign_key), fp);
			}
		} else {
			printf("Could not verify signature.\n");
		}
	}
	/* Print the signature key if requested */
	if (ret == 0 && print_pubkey && sign_key != NULL) {
		if ((r = sshkey_write(sign_key, stdout)) == 0)
			fputc('\n', stdout);
		else {
			error_r(r, "Could not print public key.\n");
			ret = -1;
		}
	}
	sshbuf_free(sigbuf);
	sshbuf_free(abuf);
	sshkey_free(sign_key);
	sshkey_sig_details_free(sig_details);
	free(fp);
	return ret;
}

static int
sig_find_principals(const char *signature, const char *allowed_keys,
    char * const *opts, size_t nopts)
{
	int r, ret = -1;
	struct sshbuf *sigbuf = NULL, *abuf = NULL;
	struct sshkey *sign_key = NULL;
	char *principals = NULL, *cp, *tmp;
	uint64_t verify_time = 0;

	if (sig_process_opts(opts, nopts, NULL, &verify_time, NULL) != 0)
		goto done; /* error already logged */

	if ((r = sshbuf_load_file(signature, &abuf)) != 0) {
		error_r(r, "Couldn't read signature file");
		goto done;
	}
	if ((r = sshsig_dearmor(abuf, &sigbuf)) != 0) {
		error_fr(r, "sshsig_armor");
		goto done;
	}
	if ((r = sshsig_get_pubkey(sigbuf, &sign_key)) != 0) {
		error_fr(r, "sshsig_get_pubkey");
		goto done;
	}
	if ((r = sshsig_find_principals(allowed_keys, sign_key,
	    verify_time, &principals)) != 0) {
		if (r != SSH_ERR_KEY_NOT_FOUND)
			error_fr(r, "sshsig_find_principal");
		goto done;
	}
	ret = 0;
done:
	if (ret == 0 ) {
		/* Emit matching principals one per line */
		tmp = principals;
		while ((cp = strsep(&tmp, ",")) != NULL && *cp != '\0')
			puts(cp);
	} else {
		fprintf(stderr, "No principal matched.\n");
	}
	sshbuf_free(sigbuf);
	sshbuf_free(abuf);
	sshkey_free(sign_key);
	free(principals);
	return ret;
}

static int
sig_match_principals(const char *allowed_keys, char *principal,
	char * const *opts, size_t nopts)
{
	int r;
	char **principals = NULL;
	size_t i, nprincipals = 0;

	if ((r = sig_process_opts(opts, nopts, NULL, NULL, NULL)) != 0)
		return r; /* error already logged */

	if ((r = sshsig_match_principals(allowed_keys, principal,
	    &principals, &nprincipals)) != 0) {
		debug_f("match: %s", ssh_err(r));
		fprintf(stderr, "No principal matched.\n");
		return r;
	}
	for (i = 0; i < nprincipals; i++) {
		printf("%s\n", principals[i]);
		free(principals[i]);
	}
	free(principals);

	return 0;
}

static void
do_moduli_gen(const char *out_file, char **opts, size_t nopts)
{
#ifdef WITH_OPENSSL
	/* Moduli generation/screening */
	u_int32_t memory = 0;
	BIGNUM *start = NULL;
	int moduli_bits = 0;
	FILE *out;
	size_t i;
	const char *errstr;

	/* Parse options */
	for (i = 0; i < nopts; i++) {
		if (strncmp(opts[i], "memory=", 7) == 0) {
			memory = (u_int32_t)strtonum(opts[i]+7, 1,
			    UINT_MAX, &errstr);
			if (errstr) {
				fatal("Memory limit is %s: %s",
				    errstr, opts[i]+7);
			}
		} else if (strncmp(opts[i], "start=", 6) == 0) {
			/* XXX - also compare length against bits */
			if (BN_hex2bn(&start, opts[i]+6) == 0)
				fatal("Invalid start point.");
		} else if (strncmp(opts[i], "bits=", 5) == 0) {
			moduli_bits = (int)strtonum(opts[i]+5, 1,
			    INT_MAX, &errstr);
			if (errstr) {
				fatal("Invalid number: %s (%s)",
					opts[i]+12, errstr);
			}
		} else {
			fatal("Option \"%s\" is unsupported for moduli "
			    "generation", opts[i]);
		}
	}

	if ((out = fopen(out_file, "w")) == NULL) {
		fatal("Couldn't open modulus candidate file \"%s\": %s",
		    out_file, strerror(errno));
	}
	setvbuf(out, NULL, _IOLBF, 0);

	if (moduli_bits == 0)
		moduli_bits = DEFAULT_BITS;
	if (gen_candidates(out, memory, moduli_bits, start) != 0)
		fatal("modulus candidate generation failed");
#else /* WITH_OPENSSL */
	fatal("Moduli generation is not supported");
#endif /* WITH_OPENSSL */
}

static void
do_moduli_screen(const char *out_file, char **opts, size_t nopts)
{
#ifdef WITH_OPENSSL
	/* Moduli generation/screening */
	char *checkpoint = NULL;
	u_int32_t generator_wanted = 0;
	unsigned long start_lineno = 0, lines_to_process = 0;
	int prime_tests = 0;
	FILE *out, *in = stdin;
	size_t i;
	const char *errstr;

	/* Parse options */
	for (i = 0; i < nopts; i++) {
		if (strncmp(opts[i], "lines=", 6) == 0) {
			lines_to_process = strtoul(opts[i]+6, NULL, 10);
		} else if (strncmp(opts[i], "start-line=", 11) == 0) {
			start_lineno = strtoul(opts[i]+11, NULL, 10);
		} else if (strncmp(opts[i], "checkpoint=", 11) == 0) {
			checkpoint = xstrdup(opts[i]+11);
		} else if (strncmp(opts[i], "generator=", 10) == 0) {
			generator_wanted = (u_int32_t)strtonum(
			    opts[i]+10, 1, UINT_MAX, &errstr);
			if (errstr != NULL) {
				fatal("Generator invalid: %s (%s)",
				    opts[i]+10, errstr);
			}
		} else if (strncmp(opts[i], "prime-tests=", 12) == 0) {
			prime_tests = (int)strtonum(opts[i]+12, 1,
			    INT_MAX, &errstr);
			if (errstr) {
				fatal("Invalid number: %s (%s)",
					opts[i]+12, errstr);
			}
		} else {
			fatal("Option \"%s\" is unsupported for moduli "
			    "screening", opts[i]);
		}
	}

	if (have_identity && strcmp(identity_file, "-") != 0) {
		if ((in = fopen(identity_file, "r")) == NULL) {
			fatal("Couldn't open modulus candidate "
			    "file \"%s\": %s", identity_file,
			    strerror(errno));
		}
	}

	if ((out = fopen(out_file, "a")) == NULL) {
		fatal("Couldn't open moduli file \"%s\": %s",
		    out_file, strerror(errno));
	}
	setvbuf(out, NULL, _IOLBF, 0);
	if (prime_test(in, out, prime_tests == 0 ? 100 : prime_tests,
	    generator_wanted, checkpoint,
	    start_lineno, lines_to_process) != 0)
		fatal("modulus screening failed");
#else /* WITH_OPENSSL */
	fatal("Moduli screening is not supported");
#endif /* WITH_OPENSSL */
}

/* Read and confirm a passphrase */
static char *
read_check_passphrase(const char *prompt1, const char *prompt2,
    const char *retry_prompt)
{
	char *passphrase1, *passphrase2;

	for (;;) {
		passphrase1 = read_passphrase(prompt1, RP_ALLOW_STDIN);
		passphrase2 = read_passphrase(prompt2, RP_ALLOW_STDIN);
		if (strcmp(passphrase1, passphrase2) == 0) {
			freezero(passphrase2, strlen(passphrase2));
			return passphrase1;
		}
		/* The passphrases do not match. Clear them and retry. */
		freezero(passphrase1, strlen(passphrase1));
		freezero(passphrase2, strlen(passphrase2));
		fputs(retry_prompt, stdout);
		fputc('\n', stdout);
		fflush(stdout);
	}
	/* NOTREACHED */
	return NULL;
}

static char *
private_key_passphrase(void)
{
	if (identity_passphrase)
		return xstrdup(identity_passphrase);
	if (identity_new_passphrase)
		return xstrdup(identity_new_passphrase);

	return read_check_passphrase(
	    "Enter passphrase (empty for no passphrase): ",
	    "Enter same passphrase again: ",
	    "Passphrases do not match.  Try again.");
}

static char *
sk_suffix(const char *application, const uint8_t *user, size_t userlen)
{
	char *ret, *cp;
	size_t slen, i;

	/* Trim off URL-like preamble */
	if (strncmp(application, "ssh://", 6) == 0)
		ret =  xstrdup(application + 6);
	else if (strncmp(application, "ssh:", 4) == 0)
		ret =  xstrdup(application + 4);
	else
		ret = xstrdup(application);

	/* Count trailing zeros in user */
	for (i = 0; i < userlen; i++) {
		if (user[userlen - i - 1] != 0)
			break;
	}
	if (i >= userlen)
		return ret; /* user-id was default all-zeros */

	/* Append user-id, escaping non-UTF-8 characters */
	slen = userlen - i;
	if (asmprintf(&cp, INT_MAX, NULL, "%.*s", (int)slen, user) == -1)
		fatal_f("asmprintf failed");
	/* Don't emit a user-id that contains path or control characters */
	if (strchr(cp, '/') != NULL || strstr(cp, "..") != NULL ||
	    strchr(cp, '\\') != NULL) {
		free(cp);
		cp = tohex(user, slen);
	}
	xextendf(&ret, "_", "%s", cp);
	free(cp);
	return ret;
}

static int
do_download_sk(const char *skprovider, const char *device)
{
	struct sshsk_resident_key **srks;
	size_t nsrks, i;
	int r, ret = -1;
	char *fp, *pin = NULL, *pass = NULL, *path, *pubpath;
	const char *ext;
	struct sshkey *key;

	if (skprovider == NULL)
		fatal("Cannot download keys without provider");

	pin = read_passphrase("Enter PIN for authenticator: ", RP_ALLOW_STDIN);
	if (!quiet) {
		printf("You may need to touch your authenticator "
		    "to authorize key download.\n");
	}
	if ((r = sshsk_load_resident(skprovider, device, pin, 0,
	    &srks, &nsrks)) != 0) {
		if (pin != NULL)
			freezero(pin, strlen(pin));
		error_r(r, "Unable to load resident keys");
		return -1;
	}
	if (nsrks == 0)
		logit("No keys to download");
	if (pin != NULL)
		freezero(pin, strlen(pin));

	for (i = 0; i < nsrks; i++) {
		key = srks[i]->key;
		if (key->type != KEY_ECDSA_SK && key->type != KEY_ED25519_SK) {
			error("Unsupported key type %s (%d)",
			    sshkey_type(key), key->type);
			continue;
		}
		if ((fp = sshkey_fingerprint(key, fingerprint_hash,
		    SSH_FP_DEFAULT)) == NULL)
			fatal_f("sshkey_fingerprint failed");
		debug_f("key %zu: %s %s %s (flags 0x%02x)", i,
		    sshkey_type(key), fp, key->sk_application, key->sk_flags);
		ext = sk_suffix(key->sk_application,
		    srks[i]->user_id, srks[i]->user_id_len);
		xasprintf(&path, "id_%s_rk%s%s",
		    key->type == KEY_ECDSA_SK ? "ecdsa_sk" : "ed25519_sk",
		    *ext == '\0' ? "" : "_", ext);

		/* If the file already exists, ask the user to confirm. */
		if (!confirm_overwrite(path)) {
			free(path);
			break;
		}

		/* Save the key with the application string as the comment */
		if (pass == NULL)
			pass = private_key_passphrase();
		if ((r = sshkey_save_private(key, path, pass,
		    key->sk_application, private_key_format,
		    openssh_format_cipher, rounds)) != 0) {
			error_r(r, "Saving key \"%s\" failed", path);
			free(path);
			break;
		}
		if (!quiet) {
			printf("Saved %s key%s%s to %s\n", sshkey_type(key),
			    *ext != '\0' ? " " : "",
			    *ext != '\0' ? key->sk_application : "",
			    path);
		}

		/* Save public key too */
		xasprintf(&pubpath, "%s.pub", path);
		free(path);
		if ((r = sshkey_save_public(key, pubpath,
		    key->sk_application)) != 0) {
			error_r(r, "Saving public key \"%s\" failed", pubpath);
			free(pubpath);
			break;
		}
		free(pubpath);
	}

	if (i >= nsrks)
		ret = 0; /* success */
	if (pass != NULL)
		freezero(pass, strlen(pass));
	sshsk_free_resident_keys(srks, nsrks);
	return ret;
}

static void
save_attestation(struct sshbuf *attest, const char *path)
{
	mode_t omask;
	int r;

	if (path == NULL)
		return; /* nothing to do */
	if (attest == NULL || sshbuf_len(attest) == 0)
		fatal("Enrollment did not return attestation data");
	omask = umask(077);
	r = sshbuf_write_file(path, attest);
	umask(omask);
	if (r != 0)
		fatal_r(r, "Unable to write attestation data \"%s\"", path);
	if (!quiet)
		printf("Your FIDO attestation certificate has been saved in "
		    "%s\n", path);
}

static int
confirm_sk_overwrite(const char *application, const char *user)
{
	char yesno[3];

	printf("A resident key scoped to '%s' with user id '%s' already "
	    "exists.\n", application == NULL ? "ssh:" : application,
	    user == NULL ? "null" : user);
	printf("Overwrite key in token (y/n)? ");
	fflush(stdout);
	if (fgets(yesno, sizeof(yesno), stdin) == NULL)
		return 0;
	if (yesno[0] != 'y' && yesno[0] != 'Y')
		return 0;
	return 1;
}

static void
usage(void)
{
	fprintf(stderr,
	    "usage: ssh-keygen [-q] [-a rounds] [-b bits] [-C comment] [-f output_keyfile]\n"
	    "                  [-m format] [-N new_passphrase] [-O option]\n"
	    "                  [-t dsa | ecdsa | ecdsa-sk | ed25519 | ed25519-sk | rsa]\n"
	    "                  [-w provider] [-Z cipher]\n"
	    "       ssh-keygen -p [-a rounds] [-f keyfile] [-m format] [-N new_passphrase]\n"
	    "                   [-P old_passphrase] [-Z cipher]\n"
#ifdef WITH_OPENSSL
	    "       ssh-keygen -i [-f input_keyfile] [-m key_format]\n"
	    "       ssh-keygen -e [-f input_keyfile] [-m key_format]\n"
#endif
	    "       ssh-keygen -y [-f input_keyfile]\n"
	    "       ssh-keygen -c [-a rounds] [-C comment] [-f keyfile] [-P passphrase]\n"
	    "       ssh-keygen -l [-v] [-E fingerprint_hash] [-f input_keyfile]\n"
	    "       ssh-keygen -B [-f input_keyfile]\n");
#ifdef ENABLE_PKCS11
	fprintf(stderr,
	    "       ssh-keygen -D pkcs11\n");
#endif
	fprintf(stderr,
	    "       ssh-keygen -F hostname [-lv] [-f known_hosts_file]\n"
	    "       ssh-keygen -H [-f known_hosts_file]\n"
	    "       ssh-keygen -K [-a rounds] [-w provider]\n"
	    "       ssh-keygen -R hostname [-f known_hosts_file]\n"
	    "       ssh-keygen -r hostname [-g] [-f input_keyfile]\n"
#ifdef WITH_OPENSSL
	    "       ssh-keygen -M generate [-O option] output_file\n"
	    "       ssh-keygen -M screen [-f input_file] [-O option] output_file\n"
#endif
	    "       ssh-keygen -I certificate_identity -s ca_key [-hU] [-D pkcs11_provider]\n"
	    "                  [-n principals] [-O option] [-V validity_interval]\n"
	    "                  [-z serial_number] file ...\n"
	    "       ssh-keygen -L [-f input_keyfile]\n"
	    "       ssh-keygen -A [-a rounds] [-f prefix_path]\n"
	    "       ssh-keygen -k -f krl_file [-u] [-s ca_public] [-z version_number]\n"
	    "                  file ...\n"
	    "       ssh-keygen -Q [-l] -f krl_file [file ...]\n"
	    "       ssh-keygen -Y find-principals -s signature_file -f allowed_signers_file\n"
	    "       ssh-keygen -Y match-principals -I signer_identity -f allowed_signers_file\n"
	    "       ssh-keygen -Y check-novalidate -n namespace -s signature_file\n"
	    "       ssh-keygen -Y sign -f key_file -n namespace file [-O option] ...\n"
	    "       ssh-keygen -Y verify -f allowed_signers_file -I signer_identity\n"
	    "                  -n namespace -s signature_file [-r krl_file] [-O option]\n");
	exit(1);
}

/*
 * Main program for key management.
 */
int
main(int argc, char **argv)
{
	char comment[1024], *passphrase = NULL;
	char *rr_hostname = NULL, *ep, *fp, *ra;
	struct sshkey *private, *public;
	struct passwd *pw;
	int r, opt, type;
	int change_passphrase = 0, change_comment = 0, show_cert = 0;
	int find_host = 0, delete_host = 0, hash_hosts = 0;
	int gen_all_hostkeys = 0, gen_krl = 0, update_krl = 0, check_krl = 0;
	int prefer_agent = 0, convert_to = 0, convert_from = 0;
	int print_public = 0, print_generic = 0, cert_serial_autoinc = 0;
	int do_gen_candidates = 0, do_screen_candidates = 0, download_sk = 0;
	unsigned long long cert_serial = 0;
	char *identity_comment = NULL, *ca_key_path = NULL, **opts = NULL;
	char *sk_application = NULL, *sk_device = NULL, *sk_user = NULL;
	char *sk_attestation_path = NULL;
	struct sshbuf *challenge = NULL, *attest = NULL;
	size_t i, nopts = 0;
	u_int32_t bits = 0;
	uint8_t sk_flags = SSH_SK_USER_PRESENCE_REQD;
	const char *errstr;
	int log_level = SYSLOG_LEVEL_INFO;
	char *sign_op = NULL;

	extern int optind;
	extern char *optarg;

	/* Ensure that fds 0, 1 and 2 are open or directed to /dev/null */
	sanitise_stdfd();

	__progname = ssh_get_progname(argv[0]);

	seed_rng();

	log_init(argv[0], SYSLOG_LEVEL_INFO, SYSLOG_FACILITY_USER, 1);

	msetlocale();

	/* we need this for the home * directory.  */
	pw = getpwuid(getuid());
	if (!pw)
		fatal("No user exists for uid %lu", (u_long)getuid());
	pw = pwcopy(pw);
	if (gethostname(hostname, sizeof(hostname)) == -1)
		fatal("gethostname: %s", strerror(errno));

	sk_provider = getenv("SSH_SK_PROVIDER");

	/* Remaining characters: dGjJSTWx */
	while ((opt = getopt(argc, argv, "ABHKLQUXceghiklopquvy"
	    "C:D:E:F:I:M:N:O:P:R:V:Y:Z:"
	    "a:b:f:g:m:n:r:s:t:w:z:")) != -1) {
		switch (opt) {
		case 'A':
			gen_all_hostkeys = 1;
			break;
		case 'b':
			bits = (u_int32_t)strtonum(optarg, 1, UINT32_MAX,
			    &errstr);
			if (errstr)
				fatal("Bits has bad value %s (%s)",
					optarg, errstr);
			break;
		case 'E':
			fingerprint_hash = ssh_digest_alg_by_name(optarg);
			if (fingerprint_hash == -1)
				fatal("Invalid hash algorithm \"%s\"", optarg);
			break;
		case 'F':
			find_host = 1;
			rr_hostname = optarg;
			break;
		case 'H':
			hash_hosts = 1;
			break;
		case 'I':
			cert_key_id = optarg;
			break;
		case 'R':
			delete_host = 1;
			rr_hostname = optarg;
			break;
		case 'L':
			show_cert = 1;
			break;
		case 'l':
			print_fingerprint = 1;
			break;
		case 'B':
			print_bubblebabble = 1;
			break;
		case 'm':
			if (strcasecmp(optarg, "RFC4716") == 0 ||
			    strcasecmp(optarg, "ssh2") == 0) {
				convert_format = FMT_RFC4716;
				break;
			}
			if (strcasecmp(optarg, "PKCS8") == 0) {
				convert_format = FMT_PKCS8;
				private_key_format = SSHKEY_PRIVATE_PKCS8;
				break;
			}
			if (strcasecmp(optarg, "PEM") == 0) {
				convert_format = FMT_PEM;
				private_key_format = SSHKEY_PRIVATE_PEM;
				break;
			}
			fatal("Unsupported conversion format \"%s\"", optarg);
		case 'n':
			cert_principals = optarg;
			break;
		case 'o':
			/* no-op; new format is already the default */
			break;
		case 'p':
			change_passphrase = 1;
			break;
		case 'c':
			change_comment = 1;
			break;
		case 'f':
			if (strlcpy(identity_file, optarg,
			    sizeof(identity_file)) >= sizeof(identity_file))
				fatal("Identity filename too long");
			have_identity = 1;
			break;
		case 'g':
			print_generic = 1;
			break;
		case 'K':
			download_sk = 1;
			break;
		case 'P':
			identity_passphrase = optarg;
			break;
		case 'N':
			identity_new_passphrase = optarg;
			break;
		case 'Q':
			check_krl = 1;
			break;
		case 'O':
			opts = xrecallocarray(opts, nopts, nopts + 1,
			    sizeof(*opts));
			opts[nopts++] = xstrdup(optarg);
			break;
		case 'Z':
			openssh_format_cipher = optarg;
			if (cipher_by_name(openssh_format_cipher) == NULL)
				fatal("Invalid OpenSSH-format cipher '%s'",
				    openssh_format_cipher);
			break;
		case 'C':
			identity_comment = optarg;
			break;
		case 'q':
			quiet = 1;
			break;
		case 'e':
			/* export key */
			convert_to = 1;
			break;
		case 'h':
			cert_key_type = SSH2_CERT_TYPE_HOST;
			certflags_flags = 0;
			break;
		case 'k':
			gen_krl = 1;
			break;
		case 'i':
		case 'X':
			/* import key */
			convert_from = 1;
			break;
		case 'y':
			print_public = 1;
			break;
		case 's':
			ca_key_path = optarg;
			break;
		case 't':
			key_type_name = optarg;
			break;
		case 'D':
			pkcs11provider = optarg;
			break;
		case 'U':
			prefer_agent = 1;
			break;
		case 'u':
			update_krl = 1;
			break;
		case 'v':
			if (log_level == SYSLOG_LEVEL_INFO)
				log_level = SYSLOG_LEVEL_DEBUG1;
			else {
				if (log_level >= SYSLOG_LEVEL_DEBUG1 &&
				    log_level < SYSLOG_LEVEL_DEBUG3)
					log_level++;
			}
			break;
		case 'r':
			rr_hostname = optarg;
			break;
		case 'a':
			rounds = (int)strtonum(optarg, 1, INT_MAX, &errstr);
			if (errstr)
				fatal("Invalid number: %s (%s)",
					optarg, errstr);
			break;
		case 'V':
			parse_cert_times(optarg);
			break;
		case 'Y':
			sign_op = optarg;
			break;
		case 'w':
			sk_provider = optarg;
			break;
		case 'z':
			errno = 0;
			if (*optarg == '+') {
				cert_serial_autoinc = 1;
				optarg++;
			}
			cert_serial = strtoull(optarg, &ep, 10);
			if (*optarg < '0' || *optarg > '9' || *ep != '\0' ||
			    (errno == ERANGE && cert_serial == ULLONG_MAX))
				fatal("Invalid serial number \"%s\"", optarg);
			break;
		case 'M':
			if (strcmp(optarg, "generate") == 0)
				do_gen_candidates = 1;
			else if (strcmp(optarg, "screen") == 0)
				do_screen_candidates = 1;
			else
				fatal("Unsupported moduli option %s", optarg);
			break;
		default:
			usage();
		}
	}

#ifdef ENABLE_SK_INTERNAL
	if (sk_provider == NULL)
		sk_provider = "internal";
#endif

	/* reinit */
	log_init(argv[0], log_level, SYSLOG_FACILITY_USER, 1);

	argv += optind;
	argc -= optind;

	if (sign_op != NULL) {
		if (strncmp(sign_op, "find-principals", 15) == 0) {
			if (ca_key_path == NULL) {
				error("Too few arguments for find-principals:"
				    "missing signature file");
				exit(1);
			}
			if (!have_identity) {
				error("Too few arguments for find-principals:"
				    "missing allowed keys file");
				exit(1);
			}
			return sig_find_principals(ca_key_path, identity_file,
			    opts, nopts);
		} else if (strncmp(sign_op, "match-principals", 16) == 0) {
			if (!have_identity) {
				error("Too few arguments for match-principals:"
				    "missing allowed keys file");
				exit(1);
			}
			if (cert_key_id == NULL) {
				error("Too few arguments for match-principals: "
				    "missing principal ID");
				exit(1);
			}
			return sig_match_principals(identity_file, cert_key_id,
			    opts, nopts);
		} else if (strncmp(sign_op, "sign", 4) == 0) {
			/* NB. cert_principals is actually namespace, via -n */
			if (cert_principals == NULL ||
			    *cert_principals == '\0') {
				error("Too few arguments for sign: "
				    "missing namespace");
				exit(1);
			}
			if (!have_identity) {
				error("Too few arguments for sign: "
				    "missing key");
				exit(1);
			}
			return sig_sign(identity_file, cert_principals,
			    prefer_agent, argc, argv, opts, nopts);
		} else if (strncmp(sign_op, "check-novalidate", 16) == 0) {
			/* NB. cert_principals is actually namespace, via -n */
			if (cert_principals == NULL ||
			    *cert_principals == '\0') {
				error("Too few arguments for check-novalidate: "
				    "missing namespace");
				exit(1);
			}
			if (ca_key_path == NULL) {
				error("Too few arguments for check-novalidate: "
				    "missing signature file");
				exit(1);
			}
			return sig_verify(ca_key_path, cert_principals,
			    NULL, NULL, NULL, opts, nopts);
		} else if (strncmp(sign_op, "verify", 6) == 0) {
			/* NB. cert_principals is actually namespace, via -n */
			if (cert_principals == NULL ||
			    *cert_principals == '\0') {
				error("Too few arguments for verify: "
				    "missing namespace");
				exit(1);
			}
			if (ca_key_path == NULL) {
				error("Too few arguments for verify: "
				    "missing signature file");
				exit(1);
			}
			if (!have_identity) {
				error("Too few arguments for sign: "
				    "missing allowed keys file");
				exit(1);
			}
			if (cert_key_id == NULL) {
				error("Too few arguments for verify: "
				    "missing principal identity");
				exit(1);
			}
			return sig_verify(ca_key_path, cert_principals,
			    cert_key_id, identity_file, rr_hostname,
			    opts, nopts);
		}
		error("Unsupported operation for -Y: \"%s\"", sign_op);
		usage();
		/* NOTREACHED */
	}

	if (ca_key_path != NULL) {
		if (argc < 1 && !gen_krl) {
			error("Too few arguments.");
			usage();
		}
	} else if (argc > 0 && !gen_krl && !check_krl &&
	    !do_gen_candidates && !do_screen_candidates) {
		error("Too many arguments.");
		usage();
	}
	if (change_passphrase && change_comment) {
		error("Can only have one of -p and -c.");
		usage();
	}
	if (print_fingerprint && (delete_host || hash_hosts)) {
		error("Cannot use -l with -H or -R.");
		usage();
	}
	if (gen_krl) {
		do_gen_krl(pw, update_krl, ca_key_path,
		    cert_serial, identity_comment, argc, argv);
		return (0);
	}
	if (check_krl) {
		do_check_krl(pw, print_fingerprint, argc, argv);
		return (0);
	}
	if (ca_key_path != NULL) {
		if (cert_key_id == NULL)
			fatal("Must specify key id (-I) when certifying");
		for (i = 0; i < nopts; i++)
			add_cert_option(opts[i]);
		do_ca_sign(pw, ca_key_path, prefer_agent,
		    cert_serial, cert_serial_autoinc, argc, argv);
	}
	if (show_cert)
		do_show_cert(pw);
	if (delete_host || hash_hosts || find_host) {
		do_known_hosts(pw, rr_hostname, find_host,
		    delete_host, hash_hosts);
	}
	if (pkcs11provider != NULL)
		do_download(pw);
	if (download_sk) {
		for (i = 0; i < nopts; i++) {
			if (strncasecmp(opts[i], "device=", 7) == 0) {
				sk_device = xstrdup(opts[i] + 7);
			} else {
				fatal("Option \"%s\" is unsupported for "
				    "FIDO authenticator download", opts[i]);
			}
		}
		return do_download_sk(sk_provider, sk_device);
	}
	if (print_fingerprint || print_bubblebabble)
		do_fingerprint(pw);
	if (change_passphrase)
		do_change_passphrase(pw);
	if (change_comment)
		do_change_comment(pw, identity_comment);
#ifdef WITH_OPENSSL
	if (convert_to)
		do_convert_to(pw);
	if (convert_from)
		do_convert_from(pw);
#else /* WITH_OPENSSL */
	if (convert_to || convert_from)
		fatal("key conversion disabled at compile time");
#endif /* WITH_OPENSSL */
	if (print_public)
		do_print_public(pw);
	if (rr_hostname != NULL) {
		unsigned int n = 0;

		if (have_identity) {
			n = do_print_resource_record(pw, identity_file,
			    rr_hostname, print_generic);
			if (n == 0)
				fatal("%s: %s", identity_file, strerror(errno));
			exit(0);
		} else {

			n += do_print_resource_record(pw,
			    _PATH_HOST_RSA_KEY_FILE, rr_hostname,
			    print_generic);
			n += do_print_resource_record(pw,
			    _PATH_HOST_DSA_KEY_FILE, rr_hostname,
			    print_generic);
			n += do_print_resource_record(pw,
			    _PATH_HOST_ECDSA_KEY_FILE, rr_hostname,
			    print_generic);
			n += do_print_resource_record(pw,
			    _PATH_HOST_ED25519_KEY_FILE, rr_hostname,
			    print_generic);
			n += do_print_resource_record(pw,
			    _PATH_HOST_XMSS_KEY_FILE, rr_hostname,
			    print_generic);
			if (n == 0)
				fatal("no keys found.");
			exit(0);
		}
	}

	if (do_gen_candidates || do_screen_candidates) {
		if (argc <= 0)
			fatal("No output file specified");
		else if (argc > 1)
			fatal("Too many output files specified");
	}
	if (do_gen_candidates) {
		do_moduli_gen(argv[0], opts, nopts);
		return 0;
	}
	if (do_screen_candidates) {
		do_moduli_screen(argv[0], opts, nopts);
		return 0;
	}

	if (gen_all_hostkeys) {
		do_gen_all_hostkeys(pw);
		return (0);
	}

	if (key_type_name == NULL)
		key_type_name = DEFAULT_KEY_TYPE_NAME;

	type = sshkey_type_from_name(key_type_name);
	type_bits_valid(type, key_type_name, &bits);

	if (!quiet)
		printf("Generating public/private %s key pair.\n",
		    key_type_name);
	switch (type) {
	case KEY_ECDSA_SK:
	case KEY_ED25519_SK:
		for (i = 0; i < nopts; i++) {
			if (strcasecmp(opts[i], "no-touch-required") == 0) {
				sk_flags &= ~SSH_SK_USER_PRESENCE_REQD;
			} else if (strcasecmp(opts[i], "verify-required") == 0) {
				sk_flags |= SSH_SK_USER_VERIFICATION_REQD;
			} else if (strcasecmp(opts[i], "resident") == 0) {
				sk_flags |= SSH_SK_RESIDENT_KEY;
			} else if (strncasecmp(opts[i], "device=", 7) == 0) {
				sk_device = xstrdup(opts[i] + 7);
			} else if (strncasecmp(opts[i], "user=", 5) == 0) {
				sk_user = xstrdup(opts[i] + 5);
			} else if (strncasecmp(opts[i], "challenge=", 10) == 0) {
				if ((r = sshbuf_load_file(opts[i] + 10,
				    &challenge)) != 0) {
					fatal_r(r, "Unable to load FIDO "
					    "enrollment challenge \"%s\"",
					    opts[i] + 10);
				}
			} else if (strncasecmp(opts[i],
			    "write-attestation=", 18) == 0) {
				sk_attestation_path = opts[i] + 18;
			} else if (strncasecmp(opts[i],
			    "application=", 12) == 0) {
				sk_application = xstrdup(opts[i] + 12);
				if (strncmp(sk_application, "ssh:", 4) != 0) {
					fatal("FIDO application string must "
					    "begin with \"ssh:\"");
				}
			} else {
				fatal("Option \"%s\" is unsupported for "
				    "FIDO authenticator enrollment", opts[i]);
			}
		}
		if ((attest = sshbuf_new()) == NULL)
			fatal("sshbuf_new failed");
		r = 0;
		for (i = 0 ;;) {
			if (!quiet) {
				printf("You may need to touch your "
				    "authenticator%s to authorize key "
				    "generation.\n",
				    r == 0 ? "" : " again");
			}
			fflush(stdout);
			r = sshsk_enroll(type, sk_provider, sk_device,
			    sk_application == NULL ? "ssh:" : sk_application,
			    sk_user, sk_flags, passphrase, challenge,
			    &private, attest);
			if (r == 0)
				break;
			if (r == SSH_ERR_KEY_BAD_PERMISSIONS &&
			    (sk_flags & SSH_SK_RESIDENT_KEY) != 0 &&
			    (sk_flags & SSH_SK_FORCE_OPERATION) == 0 &&
			    confirm_sk_overwrite(sk_application, sk_user)) {
				sk_flags |= SSH_SK_FORCE_OPERATION;
				continue;
			}
			if (r != SSH_ERR_KEY_WRONG_PASSPHRASE)
				fatal_r(r, "Key enrollment failed");
			else if (passphrase != NULL) {
				error("PIN incorrect");
				freezero(passphrase, strlen(passphrase));
				passphrase = NULL;
			}
			if (++i >= 3)
				fatal("Too many incorrect PINs");
			passphrase = read_passphrase("Enter PIN for "
			    "authenticator: ", RP_ALLOW_STDIN);
		}
		if (passphrase != NULL) {
			freezero(passphrase, strlen(passphrase));
			passphrase = NULL;
		}
		break;
	default:
		if ((r = sshkey_generate(type, bits, &private)) != 0)
			fatal("sshkey_generate failed");
		break;
	}
	if ((r = sshkey_from_private(private, &public)) != 0)
		fatal_r(r, "sshkey_from_private");

	if (!have_identity)
		ask_filename(pw, "Enter file in which to save the key");

	/* Create ~/.ssh directory if it doesn't already exist. */
	hostfile_create_user_ssh_dir(identity_file, !quiet);

	/* If the file already exists, ask the user to confirm. */
	if (!confirm_overwrite(identity_file))
		exit(1);

	/* Determine the passphrase for the private key */
	passphrase = private_key_passphrase();
	if (identity_comment) {
		strlcpy(comment, identity_comment, sizeof(comment));
	} else {
		/* Create default comment field for the passphrase. */
		snprintf(comment, sizeof comment, "%s@%s", pw->pw_name, hostname);
	}

	/* Save the key with the given passphrase and comment. */
	if ((r = sshkey_save_private(private, identity_file, passphrase,
	    comment, private_key_format, openssh_format_cipher, rounds)) != 0) {
		error_r(r, "Saving key \"%s\" failed", identity_file);
		freezero(passphrase, strlen(passphrase));
		exit(1);
	}
	freezero(passphrase, strlen(passphrase));
	sshkey_free(private);

	if (!quiet) {
		printf("Your identification has been saved in %s\n",
		    identity_file);
	}

	strlcat(identity_file, ".pub", sizeof(identity_file));
	if ((r = sshkey_save_public(public, identity_file, comment)) != 0)
		fatal_r(r, "Unable to save public key to %s", identity_file);

	if (!quiet) {
		fp = sshkey_fingerprint(public, fingerprint_hash,
		    SSH_FP_DEFAULT);
		ra = sshkey_fingerprint(public, fingerprint_hash,
		    SSH_FP_RANDOMART);
		if (fp == NULL || ra == NULL)
			fatal("sshkey_fingerprint failed");
		printf("Your public key has been saved in %s\n",
		    identity_file);
		printf("The key fingerprint is:\n");
		printf("%s %s\n", fp, comment);
		printf("The key's randomart image is:\n");
		printf("%s\n", ra);
		free(ra);
		free(fp);
	}

	if (sk_attestation_path != NULL)
		save_attestation(attest, sk_attestation_path);

	sshbuf_free(attest);
	sshkey_free(public);

	exit(0);
}
