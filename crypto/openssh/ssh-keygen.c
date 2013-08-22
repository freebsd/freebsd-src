/* $OpenBSD: ssh-keygen.c,v 1.225 2013/02/10 23:32:10 djm Exp $ */
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
#include <sys/param.h>

#include <openssl/evp.h>
#include <openssl/pem.h>
#include "openbsd-compat/openssl-compat.h"

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

#include "xmalloc.h"
#include "key.h"
#include "rsa.h"
#include "authfile.h"
#include "uuencode.h"
#include "buffer.h"
#include "pathnames.h"
#include "log.h"
#include "misc.h"
#include "match.h"
#include "hostfile.h"
#include "dns.h"
#include "ssh.h"
#include "ssh2.h"
#include "ssh-pkcs11.h"
#include "atomicio.h"
#include "krl.h"

/* Number of bits in the RSA/DSA key.  This value can be set on the command line. */
#define DEFAULT_BITS		2048
#define DEFAULT_BITS_DSA	1024
#define DEFAULT_BITS_ECDSA	256
u_int32_t bits = 0;

/*
 * Flag indicating that we just want to change the passphrase.  This can be
 * set on the command line.
 */
int change_passphrase = 0;

/*
 * Flag indicating that we just want to change the comment.  This can be set
 * on the command line.
 */
int change_comment = 0;

int quiet = 0;

int log_level = SYSLOG_LEVEL_INFO;

/* Flag indicating that we want to hash a known_hosts file */
int hash_hosts = 0;
/* Flag indicating that we want lookup a host in known_hosts file */
int find_host = 0;
/* Flag indicating that we want to delete a host from a known_hosts file */
int delete_host = 0;

/* Flag indicating that we want to show the contents of a certificate */
int show_cert = 0;

/* Flag indicating that we just want to see the key fingerprint */
int print_fingerprint = 0;
int print_bubblebabble = 0;

/* The identity file name, given on the command line or entered by the user. */
char identity_file[1024];
int have_identity = 0;

/* This is set to the passphrase if given on the command line. */
char *identity_passphrase = NULL;

/* This is set to the new passphrase if given on the command line. */
char *identity_new_passphrase = NULL;

/* This is set to the new comment if given on the command line. */
char *identity_comment = NULL;

/* Path to CA key when certifying keys. */
char *ca_key_path = NULL;

/* Certificate serial number */
unsigned long long cert_serial = 0;

/* Key type when certifying */
u_int cert_key_type = SSH2_CERT_TYPE_USER;

/* "key ID" of signed key */
char *cert_key_id = NULL;

/* Comma-separated list of principal names for certifying keys */
char *cert_principals = NULL;

/* Validity period for certificates */
u_int64_t cert_valid_from = 0;
u_int64_t cert_valid_to = ~0ULL;

/* Certificate options */
#define CERTOPT_X_FWD	(1)
#define CERTOPT_AGENT_FWD	(1<<1)
#define CERTOPT_PORT_FWD	(1<<2)
#define CERTOPT_PTY		(1<<3)
#define CERTOPT_USER_RC	(1<<4)
#define CERTOPT_DEFAULT	(CERTOPT_X_FWD|CERTOPT_AGENT_FWD| \
			 CERTOPT_PORT_FWD|CERTOPT_PTY|CERTOPT_USER_RC)
u_int32_t certflags_flags = CERTOPT_DEFAULT;
char *certflags_command = NULL;
char *certflags_src_addr = NULL;

/* Conversion to/from various formats */
int convert_to = 0;
int convert_from = 0;
enum {
	FMT_RFC4716,
	FMT_PKCS8,
	FMT_PEM
} convert_format = FMT_RFC4716;
int print_public = 0;
int print_generic = 0;

char *key_type_name = NULL;

/* Load key from this PKCS#11 provider */
char *pkcs11provider = NULL;

/* argv0 */
extern char *__progname;

char hostname[MAXHOSTNAMELEN];

/* moduli.c */
int gen_candidates(FILE *, u_int32_t, u_int32_t, BIGNUM *);
int prime_test(FILE *, FILE *, u_int32_t, u_int32_t, char *, unsigned long,
    unsigned long);

static void
type_bits_valid(int type, u_int32_t *bitsp)
{
	u_int maxbits;

	if (type == KEY_UNSPEC) {
		fprintf(stderr, "unknown key type %s\n", key_type_name);
		exit(1);
	}
	if (*bitsp == 0) {
		if (type == KEY_DSA)
			*bitsp = DEFAULT_BITS_DSA;
		else if (type == KEY_ECDSA)
			*bitsp = DEFAULT_BITS_ECDSA;
		else
			*bitsp = DEFAULT_BITS;
	}
	maxbits = (type == KEY_DSA) ?
	    OPENSSL_DSA_MAX_MODULUS_BITS : OPENSSL_RSA_MAX_MODULUS_BITS;
	if (*bitsp > maxbits) {
		fprintf(stderr, "key bits exceeds maximum %d\n", maxbits);
		exit(1);
	}
	if (type == KEY_DSA && *bitsp != 1024)
		fatal("DSA keys must be 1024 bits");
	else if (type != KEY_ECDSA && *bitsp < 768)
		fatal("Key must at least be 768 bits");
	else if (type == KEY_ECDSA && key_ecdsa_bits_to_nid(*bitsp) == -1)
		fatal("Invalid ECDSA key length - valid lengths are "
		    "256, 384 or 521 bits");
}

static void
ask_filename(struct passwd *pw, const char *prompt)
{
	char buf[1024];
	char *name = NULL;

	if (key_type_name == NULL)
		name = _PATH_SSH_CLIENT_ID_RSA;
	else {
		switch (key_type_from_name(key_type_name)) {
		case KEY_RSA1:
			name = _PATH_SSH_CLIENT_IDENTITY;
			break;
		case KEY_DSA_CERT:
		case KEY_DSA_CERT_V00:
		case KEY_DSA:
			name = _PATH_SSH_CLIENT_ID_DSA;
			break;
#ifdef OPENSSL_HAS_ECC
		case KEY_ECDSA_CERT:
		case KEY_ECDSA:
			name = _PATH_SSH_CLIENT_ID_ECDSA;
			break;
#endif
		case KEY_RSA_CERT:
		case KEY_RSA_CERT_V00:
		case KEY_RSA:
			name = _PATH_SSH_CLIENT_ID_RSA;
			break;
		default:
			fprintf(stderr, "bad key type\n");
			exit(1);
			break;
		}
	}
	snprintf(identity_file, sizeof(identity_file), "%s/%s", pw->pw_dir, name);
	fprintf(stderr, "%s (%s): ", prompt, identity_file);
	if (fgets(buf, sizeof(buf), stdin) == NULL)
		exit(1);
	buf[strcspn(buf, "\n")] = '\0';
	if (strcmp(buf, "") != 0)
		strlcpy(identity_file, buf, sizeof(identity_file));
	have_identity = 1;
}

static Key *
load_identity(char *filename)
{
	char *pass;
	Key *prv;

	prv = key_load_private(filename, "", NULL);
	if (prv == NULL) {
		if (identity_passphrase)
			pass = xstrdup(identity_passphrase);
		else
			pass = read_passphrase("Enter passphrase: ",
			    RP_ALLOW_STDIN);
		prv = key_load_private(filename, pass, NULL);
		memset(pass, 0, strlen(pass));
		xfree(pass);
	}
	return prv;
}

#define SSH_COM_PUBLIC_BEGIN		"---- BEGIN SSH2 PUBLIC KEY ----"
#define SSH_COM_PUBLIC_END		"---- END SSH2 PUBLIC KEY ----"
#define SSH_COM_PRIVATE_BEGIN		"---- BEGIN SSH2 ENCRYPTED PRIVATE KEY ----"
#define	SSH_COM_PRIVATE_KEY_MAGIC	0x3f6ff9eb

static void
do_convert_to_ssh2(struct passwd *pw, Key *k)
{
	u_int len;
	u_char *blob;
	char comment[61];

	if (k->type == KEY_RSA1) {
		fprintf(stderr, "version 1 keys are not supported\n");
		exit(1);
	}
	if (key_to_blob(k, &blob, &len) <= 0) {
		fprintf(stderr, "key_to_blob failed\n");
		exit(1);
	}
	/* Comment + surrounds must fit into 72 chars (RFC 4716 sec 3.3) */
	snprintf(comment, sizeof(comment),
	    "%u-bit %s, converted by %s@%s from OpenSSH",
	    key_size(k), key_type(k),
	    pw->pw_name, hostname);

	fprintf(stdout, "%s\n", SSH_COM_PUBLIC_BEGIN);
	fprintf(stdout, "Comment: \"%s\"\n", comment);
	dump_base64(stdout, blob, len);
	fprintf(stdout, "%s\n", SSH_COM_PUBLIC_END);
	key_free(k);
	xfree(blob);
	exit(0);
}

static void
do_convert_to_pkcs8(Key *k)
{
	switch (key_type_plain(k->type)) {
	case KEY_RSA1:
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
		fatal("%s: unsupported key type %s", __func__, key_type(k));
	}
	exit(0);
}

static void
do_convert_to_pem(Key *k)
{
	switch (key_type_plain(k->type)) {
	case KEY_RSA1:
	case KEY_RSA:
		if (!PEM_write_RSAPublicKey(stdout, k->rsa))
			fatal("PEM_write_RSAPublicKey failed");
		break;
#if notyet /* OpenSSH 0.9.8 lacks this function */
	case KEY_DSA:
		if (!PEM_write_DSAPublicKey(stdout, k->dsa))
			fatal("PEM_write_DSAPublicKey failed");
		break;
#endif
	/* XXX ECDSA? */
	default:
		fatal("%s: unsupported key type %s", __func__, key_type(k));
	}
	exit(0);
}

static void
do_convert_to(struct passwd *pw)
{
	Key *k;
	struct stat st;

	if (!have_identity)
		ask_filename(pw, "Enter file in which the key is");
	if (stat(identity_file, &st) < 0)
		fatal("%s: %s: %s", __progname, identity_file, strerror(errno));
	if ((k = key_load_public(identity_file, NULL)) == NULL) {
		if ((k = load_identity(identity_file)) == NULL) {
			fprintf(stderr, "load failed\n");
			exit(1);
		}
	}

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
		fatal("%s: unknown key format %d", __func__, convert_format);
	}
	exit(0);
}

static void
buffer_get_bignum_bits(Buffer *b, BIGNUM *value)
{
	u_int bignum_bits = buffer_get_int(b);
	u_int bytes = (bignum_bits + 7) / 8;

	if (buffer_len(b) < bytes)
		fatal("buffer_get_bignum_bits: input buffer too small: "
		    "need %d have %d", bytes, buffer_len(b));
	if (BN_bin2bn(buffer_ptr(b), bytes, value) == NULL)
		fatal("buffer_get_bignum_bits: BN_bin2bn failed");
	buffer_consume(b, bytes);
}

static Key *
do_convert_private_ssh2_from_blob(u_char *blob, u_int blen)
{
	Buffer b;
	Key *key = NULL;
	char *type, *cipher;
	u_char *sig, data[] = "abcde12345";
	int magic, rlen, ktype, i1, i2, i3, i4;
	u_int slen;
	u_long e;

	buffer_init(&b);
	buffer_append(&b, blob, blen);

	magic = buffer_get_int(&b);
	if (magic != SSH_COM_PRIVATE_KEY_MAGIC) {
		error("bad magic 0x%x != 0x%x", magic, SSH_COM_PRIVATE_KEY_MAGIC);
		buffer_free(&b);
		return NULL;
	}
	i1 = buffer_get_int(&b);
	type   = buffer_get_string(&b, NULL);
	cipher = buffer_get_string(&b, NULL);
	i2 = buffer_get_int(&b);
	i3 = buffer_get_int(&b);
	i4 = buffer_get_int(&b);
	debug("ignore (%d %d %d %d)", i1, i2, i3, i4);
	if (strcmp(cipher, "none") != 0) {
		error("unsupported cipher %s", cipher);
		xfree(cipher);
		buffer_free(&b);
		xfree(type);
		return NULL;
	}
	xfree(cipher);

	if (strstr(type, "dsa")) {
		ktype = KEY_DSA;
	} else if (strstr(type, "rsa")) {
		ktype = KEY_RSA;
	} else {
		buffer_free(&b);
		xfree(type);
		return NULL;
	}
	key = key_new_private(ktype);
	xfree(type);

	switch (key->type) {
	case KEY_DSA:
		buffer_get_bignum_bits(&b, key->dsa->p);
		buffer_get_bignum_bits(&b, key->dsa->g);
		buffer_get_bignum_bits(&b, key->dsa->q);
		buffer_get_bignum_bits(&b, key->dsa->pub_key);
		buffer_get_bignum_bits(&b, key->dsa->priv_key);
		break;
	case KEY_RSA:
		e = buffer_get_char(&b);
		debug("e %lx", e);
		if (e < 30) {
			e <<= 8;
			e += buffer_get_char(&b);
			debug("e %lx", e);
			e <<= 8;
			e += buffer_get_char(&b);
			debug("e %lx", e);
		}
		if (!BN_set_word(key->rsa->e, e)) {
			buffer_free(&b);
			key_free(key);
			return NULL;
		}
		buffer_get_bignum_bits(&b, key->rsa->d);
		buffer_get_bignum_bits(&b, key->rsa->n);
		buffer_get_bignum_bits(&b, key->rsa->iqmp);
		buffer_get_bignum_bits(&b, key->rsa->q);
		buffer_get_bignum_bits(&b, key->rsa->p);
		rsa_generate_additional_parameters(key->rsa);
		break;
	}
	rlen = buffer_len(&b);
	if (rlen != 0)
		error("do_convert_private_ssh2_from_blob: "
		    "remaining bytes in key blob %d", rlen);
	buffer_free(&b);

	/* try the key */
	key_sign(key, &sig, &slen, data, sizeof(data));
	key_verify(key, sig, slen, data, sizeof(data));
	xfree(sig);
	return key;
}

static int
get_line(FILE *fp, char *line, size_t len)
{
	int c;
	size_t pos = 0;

	line[0] = '\0';
	while ((c = fgetc(fp)) != EOF) {
		if (pos >= len - 1) {
			fprintf(stderr, "input line too long.\n");
			exit(1);
		}
		switch (c) {
		case '\r':
			c = fgetc(fp);
			if (c != EOF && c != '\n' && ungetc(c, fp) == EOF) {
				fprintf(stderr, "unget: %s\n", strerror(errno));
				exit(1);
			}
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
do_convert_from_ssh2(struct passwd *pw, Key **k, int *private)
{
	int blen;
	u_int len;
	char line[1024];
	u_char blob[8096];
	char encoded[8096];
	int escaped = 0;
	FILE *fp;

	if ((fp = fopen(identity_file, "r")) == NULL)
		fatal("%s: %s: %s", __progname, identity_file, strerror(errno));
	encoded[0] = '\0';
	while ((blen = get_line(fp, line, sizeof(line))) != -1) {
		if (line[blen - 1] == '\\')
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
	blen = uudecode(encoded, blob, sizeof(blob));
	if (blen < 0) {
		fprintf(stderr, "uudecode failed.\n");
		exit(1);
	}
	*k = *private ?
	    do_convert_private_ssh2_from_blob(blob, blen) :
	    key_from_blob(blob, blen);
	if (*k == NULL) {
		fprintf(stderr, "decode blob failed.\n");
		exit(1);
	}
	fclose(fp);
}

static void
do_convert_from_pkcs8(Key **k, int *private)
{
	EVP_PKEY *pubkey;
	FILE *fp;

	if ((fp = fopen(identity_file, "r")) == NULL)
		fatal("%s: %s: %s", __progname, identity_file, strerror(errno));
	if ((pubkey = PEM_read_PUBKEY(fp, NULL, NULL, NULL)) == NULL) {
		fatal("%s: %s is not a recognised public key format", __func__,
		    identity_file);
	}
	fclose(fp);
	switch (EVP_PKEY_type(pubkey->type)) {
	case EVP_PKEY_RSA:
		*k = key_new(KEY_UNSPEC);
		(*k)->type = KEY_RSA;
		(*k)->rsa = EVP_PKEY_get1_RSA(pubkey);
		break;
	case EVP_PKEY_DSA:
		*k = key_new(KEY_UNSPEC);
		(*k)->type = KEY_DSA;
		(*k)->dsa = EVP_PKEY_get1_DSA(pubkey);
		break;
#ifdef OPENSSL_HAS_ECC
	case EVP_PKEY_EC:
		*k = key_new(KEY_UNSPEC);
		(*k)->type = KEY_ECDSA;
		(*k)->ecdsa = EVP_PKEY_get1_EC_KEY(pubkey);
		(*k)->ecdsa_nid = key_ecdsa_key_to_nid((*k)->ecdsa);
		break;
#endif
	default:
		fatal("%s: unsupported pubkey type %d", __func__,
		    EVP_PKEY_type(pubkey->type));
	}
	EVP_PKEY_free(pubkey);
	return;
}

static void
do_convert_from_pem(Key **k, int *private)
{
	FILE *fp;
	RSA *rsa;
#ifdef notyet
	DSA *dsa;
#endif

	if ((fp = fopen(identity_file, "r")) == NULL)
		fatal("%s: %s: %s", __progname, identity_file, strerror(errno));
	if ((rsa = PEM_read_RSAPublicKey(fp, NULL, NULL, NULL)) != NULL) {
		*k = key_new(KEY_UNSPEC);
		(*k)->type = KEY_RSA;
		(*k)->rsa = rsa;
		fclose(fp);
		return;
	}
#if notyet /* OpenSSH 0.9.8 lacks this function */
	rewind(fp);
	if ((dsa = PEM_read_DSAPublicKey(fp, NULL, NULL, NULL)) != NULL) {
		*k = key_new(KEY_UNSPEC);
		(*k)->type = KEY_DSA;
		(*k)->dsa = dsa;
		fclose(fp);
		return;
	}
	/* XXX ECDSA */
#endif
	fatal("%s: unrecognised raw private key format", __func__);
}

static void
do_convert_from(struct passwd *pw)
{
	Key *k = NULL;
	int private = 0, ok = 0;
	struct stat st;

	if (!have_identity)
		ask_filename(pw, "Enter file in which the key is");
	if (stat(identity_file, &st) < 0)
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
		fatal("%s: unknown key format %d", __func__, convert_format);
	}

	if (!private)
		ok = key_write(k, stdout);
		if (ok)
			fprintf(stdout, "\n");
	else {
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
			fatal("%s: unsupported key type %s", __func__,
			    key_type(k));
		}
	}

	if (!ok) {
		fprintf(stderr, "key write failed\n");
		exit(1);
	}
	key_free(k);
	exit(0);
}

static void
do_print_public(struct passwd *pw)
{
	Key *prv;
	struct stat st;

	if (!have_identity)
		ask_filename(pw, "Enter file in which the key is");
	if (stat(identity_file, &st) < 0) {
		perror(identity_file);
		exit(1);
	}
	prv = load_identity(identity_file);
	if (prv == NULL) {
		fprintf(stderr, "load failed\n");
		exit(1);
	}
	if (!key_write(prv, stdout))
		fprintf(stderr, "key_write failed");
	key_free(prv);
	fprintf(stdout, "\n");
	exit(0);
}

static void
do_download(struct passwd *pw)
{
#ifdef ENABLE_PKCS11
	Key **keys = NULL;
	int i, nkeys;
	enum fp_rep rep;
	enum fp_type fptype;
	char *fp, *ra;

	fptype = print_bubblebabble ? SSH_FP_SHA1 : SSH_FP_MD5;
	rep =    print_bubblebabble ? SSH_FP_BUBBLEBABBLE : SSH_FP_HEX;

	pkcs11_init(0);
	nkeys = pkcs11_add_provider(pkcs11provider, NULL, &keys);
	if (nkeys <= 0)
		fatal("cannot read public key from pkcs11");
	for (i = 0; i < nkeys; i++) {
		if (print_fingerprint) {
			fp = key_fingerprint(keys[i], fptype, rep);
			ra = key_fingerprint(keys[i], SSH_FP_MD5,
			    SSH_FP_RANDOMART);
			printf("%u %s %s (PKCS11 key)\n", key_size(keys[i]),
			    fp, key_type(keys[i]));
			if (log_level >= SYSLOG_LEVEL_VERBOSE)
				printf("%s\n", ra);
			xfree(ra);
			xfree(fp);
		} else {
			key_write(keys[i], stdout);
			fprintf(stdout, "\n");
		}
		key_free(keys[i]);
	}
	xfree(keys);
	pkcs11_terminate();
	exit(0);
#else
	fatal("no pkcs11 support");
#endif /* ENABLE_PKCS11 */
}

static void
do_fingerprint(struct passwd *pw)
{
	FILE *f;
	Key *public;
	char *comment = NULL, *cp, *ep, line[16*1024], *fp, *ra;
	int i, skip = 0, num = 0, invalid = 1;
	enum fp_rep rep;
	enum fp_type fptype;
	struct stat st;

	fptype = print_bubblebabble ? SSH_FP_SHA1 : SSH_FP_MD5;
	rep =    print_bubblebabble ? SSH_FP_BUBBLEBABBLE : SSH_FP_HEX;

	if (!have_identity)
		ask_filename(pw, "Enter file in which the key is");
	if (stat(identity_file, &st) < 0) {
		perror(identity_file);
		exit(1);
	}
	public = key_load_public(identity_file, &comment);
	if (public != NULL) {
		fp = key_fingerprint(public, fptype, rep);
		ra = key_fingerprint(public, SSH_FP_MD5, SSH_FP_RANDOMART);
		printf("%u %s %s (%s)\n", key_size(public), fp, comment,
		    key_type(public));
		if (log_level >= SYSLOG_LEVEL_VERBOSE)
			printf("%s\n", ra);
		key_free(public);
		xfree(comment);
		xfree(ra);
		xfree(fp);
		exit(0);
	}
	if (comment) {
		xfree(comment);
		comment = NULL;
	}

	if ((f = fopen(identity_file, "r")) == NULL)
		fatal("%s: %s: %s", __progname, identity_file, strerror(errno));

	while (fgets(line, sizeof(line), f)) {
		if ((cp = strchr(line, '\n')) == NULL) {
			error("line %d too long: %.40s...",
			    num + 1, line);
			skip = 1;
			continue;
		}
		num++;
		if (skip) {
			skip = 0;
			continue;
		}
		*cp = '\0';

		/* Skip leading whitespace, empty and comment lines. */
		for (cp = line; *cp == ' ' || *cp == '\t'; cp++)
			;
		if (!*cp || *cp == '\n' || *cp == '#')
			continue;
		i = strtol(cp, &ep, 10);
		if (i == 0 || ep == NULL || (*ep != ' ' && *ep != '\t')) {
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
		ep = cp;
		public = key_new(KEY_RSA1);
		if (key_read(public, &cp) != 1) {
			cp = ep;
			key_free(public);
			public = key_new(KEY_UNSPEC);
			if (key_read(public, &cp) != 1) {
				key_free(public);
				continue;
			}
		}
		comment = *cp ? cp : comment;
		fp = key_fingerprint(public, fptype, rep);
		ra = key_fingerprint(public, SSH_FP_MD5, SSH_FP_RANDOMART);
		printf("%u %s %s (%s)\n", key_size(public), fp,
		    comment ? comment : "no comment", key_type(public));
		if (log_level >= SYSLOG_LEVEL_VERBOSE)
			printf("%s\n", ra);
		xfree(ra);
		xfree(fp);
		key_free(public);
		invalid = 0;
	}
	fclose(f);

	if (invalid) {
		printf("%s is not a public key file.\n", identity_file);
		exit(1);
	}
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
		{ "rsa1", "RSA1", _PATH_HOST_KEY_FILE },
		{ "rsa", "RSA" ,_PATH_HOST_RSA_KEY_FILE },
		{ "dsa", "DSA", _PATH_HOST_DSA_KEY_FILE },
#ifdef OPENSSL_HAS_ECC
		{ "ecdsa", "ECDSA",_PATH_HOST_ECDSA_KEY_FILE },
#endif
		{ NULL, NULL, NULL }
	};

	int first = 0;
	struct stat st;
	Key *private, *public;
	char comment[1024];
	int i, type, fd;
	FILE *f;

	for (i = 0; key_types[i].key_type; i++) {
		if (stat(key_types[i].path, &st) == 0)
			continue;
		if (errno != ENOENT) {
			printf("Could not stat %s: %s", key_types[i].path,
			    strerror(errno));
			first = 0;
			continue;
		}

		if (first == 0) {
			first = 1;
			printf("%s: generating new host keys: ", __progname);
		}
		printf("%s ", key_types[i].key_type_display);
		fflush(stdout);
		arc4random_stir();
		type = key_type_from_name(key_types[i].key_type);
		strlcpy(identity_file, key_types[i].path, sizeof(identity_file));
		bits = 0;
		type_bits_valid(type, &bits);
		private = key_generate(type, bits);
		if (private == NULL) {
			fprintf(stderr, "key_generate failed\n");
			first = 0;
			continue;
		}
		public  = key_from_private(private);
		snprintf(comment, sizeof comment, "%s@%s", pw->pw_name,
		    hostname);
		if (!key_save_private(private, identity_file, "", comment)) {
			printf("Saving the key failed: %s.\n", identity_file);
			key_free(private);
			key_free(public);
			first = 0;
			continue;
		}
		key_free(private);
		arc4random_stir();
		strlcat(identity_file, ".pub", sizeof(identity_file));
		fd = open(identity_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
		if (fd == -1) {
			printf("Could not save your public key in %s\n",
			    identity_file);
			key_free(public);
			first = 0;
			continue;
		}
		f = fdopen(fd, "w");
		if (f == NULL) {
			printf("fdopen %s failed\n", identity_file);
			key_free(public);
			first = 0;
			continue;
		}
		if (!key_write(public, f)) {
			fprintf(stderr, "write key failed\n");
			key_free(public);
			first = 0;
			continue;
		}
		fprintf(f, " %s\n", comment);
		fclose(f);
		key_free(public);

	}
	if (first != 0)
		printf("\n");
}

static void
printhost(FILE *f, const char *name, Key *public, int ca, int hash)
{
	if (print_fingerprint) {
		enum fp_rep rep;
		enum fp_type fptype;
		char *fp, *ra;

		fptype = print_bubblebabble ? SSH_FP_SHA1 : SSH_FP_MD5;
		rep =    print_bubblebabble ? SSH_FP_BUBBLEBABBLE : SSH_FP_HEX;
		fp = key_fingerprint(public, fptype, rep);
		ra = key_fingerprint(public, SSH_FP_MD5, SSH_FP_RANDOMART);
		printf("%u %s %s (%s)\n", key_size(public), fp, name,
		    key_type(public));
		if (log_level >= SYSLOG_LEVEL_VERBOSE)
			printf("%s\n", ra);
		xfree(ra);
		xfree(fp);
	} else {
		if (hash && (name = host_hash(name, NULL, 0)) == NULL)
			fatal("hash_host failed");
		fprintf(f, "%s%s%s ", ca ? CA_MARKER : "", ca ? " " : "", name);
		if (!key_write(public, f))
			fatal("key_write failed");
		fprintf(f, "\n");
	}
}

static void
do_known_hosts(struct passwd *pw, const char *name)
{
	FILE *in, *out = stdout;
	Key *pub;
	char *cp, *cp2, *kp, *kp2;
	char line[16*1024], tmp[MAXPATHLEN], old[MAXPATHLEN];
	int c, skip = 0, inplace = 0, num = 0, invalid = 0, has_unhashed = 0;
	int ca;

	if (!have_identity) {
		cp = tilde_expand_filename(_PATH_SSH_USER_HOSTFILE, pw->pw_uid);
		if (strlcpy(identity_file, cp, sizeof(identity_file)) >=
		    sizeof(identity_file))
			fatal("Specified known hosts path too long");
		xfree(cp);
		have_identity = 1;
	}
	if ((in = fopen(identity_file, "r")) == NULL)
		fatal("%s: %s: %s", __progname, identity_file, strerror(errno));

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
		if ((c = mkstemp(tmp)) == -1)
			fatal("mkstemp: %s", strerror(errno));
		if ((out = fdopen(c, "w")) == NULL) {
			c = errno;
			unlink(tmp);
			fatal("fdopen: %s", strerror(c));
		}
		inplace = 1;
	}

	while (fgets(line, sizeof(line), in)) {
		if ((cp = strchr(line, '\n')) == NULL) {
			error("line %d too long: %.40s...", num + 1, line);
			skip = 1;
			invalid = 1;
			continue;
		}
		num++;
		if (skip) {
			skip = 0;
			continue;
		}
		*cp = '\0';

		/* Skip leading whitespace, empty and comment lines. */
		for (cp = line; *cp == ' ' || *cp == '\t'; cp++)
			;
		if (!*cp || *cp == '\n' || *cp == '#') {
			if (inplace)
				fprintf(out, "%s\n", cp);
			continue;
		}
		/* Check whether this is a CA key */
		if (strncasecmp(cp, CA_MARKER, sizeof(CA_MARKER) - 1) == 0 &&
		    (cp[sizeof(CA_MARKER) - 1] == ' ' ||
		    cp[sizeof(CA_MARKER) - 1] == '\t')) {
			ca = 1;
			cp += sizeof(CA_MARKER);
		} else
			ca = 0;

		/* Find the end of the host name portion. */
		for (kp = cp; *kp && *kp != ' ' && *kp != '\t'; kp++)
			;

		if (*kp == '\0' || *(kp + 1) == '\0') {
			error("line %d missing key: %.40s...",
			    num, line);
			invalid = 1;
			continue;
		}
		*kp++ = '\0';
		kp2 = kp;

		pub = key_new(KEY_RSA1);
		if (key_read(pub, &kp) != 1) {
			kp = kp2;
			key_free(pub);
			pub = key_new(KEY_UNSPEC);
			if (key_read(pub, &kp) != 1) {
				error("line %d invalid key: %.40s...",
				    num, line);
				key_free(pub);
				invalid = 1;
				continue;
			}
		}

		if (*cp == HASH_DELIM) {
			if (find_host || delete_host) {
				cp2 = host_hash(name, cp, strlen(cp));
				if (cp2 == NULL) {
					error("line %d: invalid hashed "
					    "name: %.64s...", num, line);
					invalid = 1;
					continue;
				}
				c = (strcmp(cp2, cp) == 0);
				if (find_host && c) {
					printf("# Host %s found: "
					    "line %d type %s%s\n", name,
					    num, key_type(pub),
					    ca ? " (CA key)" : "");
					printhost(out, cp, pub, ca, 0);
				}
				if (delete_host) {
					if (!c && !ca)
						printhost(out, cp, pub, ca, 0);
					else
						printf("# Host %s found: "
						    "line %d type %s\n", name,
						    num, key_type(pub));
				}
			} else if (hash_hosts)
				printhost(out, cp, pub, ca, 0);
		} else {
			if (find_host || delete_host) {
				c = (match_hostname(name, cp,
				    strlen(cp)) == 1);
				if (find_host && c) {
					printf("# Host %s found: "
					    "line %d type %s%s\n", name,
					    num, key_type(pub),
					    ca ? " (CA key)" : "");
					printhost(out, name, pub,
					    ca, hash_hosts && !ca);
				}
				if (delete_host) {
					if (!c && !ca)
						printhost(out, cp, pub, ca, 0);
					else
						printf("# Host %s found: "
						    "line %d type %s\n", name,
						    num, key_type(pub));
				}
			} else if (hash_hosts) {
				for (cp2 = strsep(&cp, ",");
				    cp2 != NULL && *cp2 != '\0';
				    cp2 = strsep(&cp, ",")) {
					if (ca) {
						fprintf(stderr, "Warning: "
						    "ignoring CA key for host: "
						    "%.64s\n", cp2);
						printhost(out, cp2, pub, ca, 0);
					} else if (strcspn(cp2, "*?!") !=
					    strlen(cp2)) {
						fprintf(stderr, "Warning: "
						    "ignoring host name with "
						    "metacharacters: %.64s\n",
						    cp2);
						printhost(out, cp2, pub, ca, 0);
					} else
						printhost(out, cp2, pub, ca, 1);
				}
				has_unhashed = 1;
			}
		}
		key_free(pub);
	}
	fclose(in);

	if (invalid) {
		fprintf(stderr, "%s is not a valid known_hosts file.\n",
		    identity_file);
		if (inplace) {
			fprintf(stderr, "Not replacing existing known_hosts "
			    "file because of errors\n");
			fclose(out);
			unlink(tmp);
		}
		exit(1);
	}

	if (inplace) {
		fclose(out);

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

		fprintf(stderr, "%s updated.\n", identity_file);
		fprintf(stderr, "Original contents retained as %s\n", old);
		if (has_unhashed) {
			fprintf(stderr, "WARNING: %s contains unhashed "
			    "entries\n", old);
			fprintf(stderr, "Delete this file to ensure privacy "
			    "of hostnames\n");
		}
	}

	exit(0);
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
	Key *private;

	if (!have_identity)
		ask_filename(pw, "Enter file in which the key is");
	if (stat(identity_file, &st) < 0) {
		perror(identity_file);
		exit(1);
	}
	/* Try to load the file with empty passphrase. */
	private = key_load_private(identity_file, "", &comment);
	if (private == NULL) {
		if (identity_passphrase)
			old_passphrase = xstrdup(identity_passphrase);
		else
			old_passphrase =
			    read_passphrase("Enter old passphrase: ",
			    RP_ALLOW_STDIN);
		private = key_load_private(identity_file, old_passphrase,
		    &comment);
		memset(old_passphrase, 0, strlen(old_passphrase));
		xfree(old_passphrase);
		if (private == NULL) {
			printf("Bad passphrase.\n");
			exit(1);
		}
	}
	printf("Key has comment '%s'\n", comment);

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
			memset(passphrase1, 0, strlen(passphrase1));
			memset(passphrase2, 0, strlen(passphrase2));
			xfree(passphrase1);
			xfree(passphrase2);
			printf("Pass phrases do not match.  Try again.\n");
			exit(1);
		}
		/* Destroy the other copy. */
		memset(passphrase2, 0, strlen(passphrase2));
		xfree(passphrase2);
	}

	/* Save the file using the new passphrase. */
	if (!key_save_private(private, identity_file, passphrase1, comment)) {
		printf("Saving the key failed: %s.\n", identity_file);
		memset(passphrase1, 0, strlen(passphrase1));
		xfree(passphrase1);
		key_free(private);
		xfree(comment);
		exit(1);
	}
	/* Destroy the passphrase and the copy of the key in memory. */
	memset(passphrase1, 0, strlen(passphrase1));
	xfree(passphrase1);
	key_free(private);		 /* Destroys contents */
	xfree(comment);

	printf("Your identification has been saved with the new passphrase.\n");
	exit(0);
}

/*
 * Print the SSHFP RR.
 */
static int
do_print_resource_record(struct passwd *pw, char *fname, char *hname)
{
	Key *public;
	char *comment = NULL;
	struct stat st;

	if (fname == NULL)
		ask_filename(pw, "Enter file in which the key is");
	if (stat(fname, &st) < 0) {
		if (errno == ENOENT)
			return 0;
		perror(fname);
		exit(1);
	}
	public = key_load_public(fname, &comment);
	if (public != NULL) {
		export_dns_rr(hname, public, stdout, print_generic);
		key_free(public);
		xfree(comment);
		return 1;
	}
	if (comment)
		xfree(comment);

	printf("failed to read v2 public key from %s.\n", fname);
	exit(1);
}

/*
 * Change the comment of a private key file.
 */
static void
do_change_comment(struct passwd *pw)
{
	char new_comment[1024], *comment, *passphrase;
	Key *private;
	Key *public;
	struct stat st;
	FILE *f;
	int fd;

	if (!have_identity)
		ask_filename(pw, "Enter file in which the key is");
	if (stat(identity_file, &st) < 0) {
		perror(identity_file);
		exit(1);
	}
	private = key_load_private(identity_file, "", &comment);
	if (private == NULL) {
		if (identity_passphrase)
			passphrase = xstrdup(identity_passphrase);
		else if (identity_new_passphrase)
			passphrase = xstrdup(identity_new_passphrase);
		else
			passphrase = read_passphrase("Enter passphrase: ",
			    RP_ALLOW_STDIN);
		/* Try to load using the passphrase. */
		private = key_load_private(identity_file, passphrase, &comment);
		if (private == NULL) {
			memset(passphrase, 0, strlen(passphrase));
			xfree(passphrase);
			printf("Bad passphrase.\n");
			exit(1);
		}
	} else {
		passphrase = xstrdup("");
	}
	if (private->type != KEY_RSA1) {
		fprintf(stderr, "Comments are only supported for RSA1 keys.\n");
		key_free(private);
		exit(1);
	}
	printf("Key now has comment '%s'\n", comment);

	if (identity_comment) {
		strlcpy(new_comment, identity_comment, sizeof(new_comment));
	} else {
		printf("Enter new comment: ");
		fflush(stdout);
		if (!fgets(new_comment, sizeof(new_comment), stdin)) {
			memset(passphrase, 0, strlen(passphrase));
			key_free(private);
			exit(1);
		}
		new_comment[strcspn(new_comment, "\n")] = '\0';
	}

	/* Save the file using the new passphrase. */
	if (!key_save_private(private, identity_file, passphrase, new_comment)) {
		printf("Saving the key failed: %s.\n", identity_file);
		memset(passphrase, 0, strlen(passphrase));
		xfree(passphrase);
		key_free(private);
		xfree(comment);
		exit(1);
	}
	memset(passphrase, 0, strlen(passphrase));
	xfree(passphrase);
	public = key_from_private(private);
	key_free(private);

	strlcat(identity_file, ".pub", sizeof(identity_file));
	fd = open(identity_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd == -1) {
		printf("Could not save your public key in %s\n", identity_file);
		exit(1);
	}
	f = fdopen(fd, "w");
	if (f == NULL) {
		printf("fdopen %s failed\n", identity_file);
		exit(1);
	}
	if (!key_write(public, f))
		fprintf(stderr, "write key failed\n");
	key_free(public);
	fprintf(f, " %s\n", new_comment);
	fclose(f);

	xfree(comment);

	printf("The comment in your key file has been changed.\n");
	exit(0);
}

static const char *
fmt_validity(u_int64_t valid_from, u_int64_t valid_to)
{
	char from[32], to[32];
	static char ret[64];
	time_t tt;
	struct tm *tm;

	*from = *to = '\0';
	if (valid_from == 0 && valid_to == 0xffffffffffffffffULL)
		return "forever";

	if (valid_from != 0) {
		/* XXX revisit INT_MAX in 2038 :) */
		tt = valid_from > INT_MAX ? INT_MAX : valid_from;
		tm = localtime(&tt);
		strftime(from, sizeof(from), "%Y-%m-%dT%H:%M:%S", tm);
	}
	if (valid_to != 0xffffffffffffffffULL) {
		/* XXX revisit INT_MAX in 2038 :) */
		tt = valid_to > INT_MAX ? INT_MAX : valid_to;
		tm = localtime(&tt);
		strftime(to, sizeof(to), "%Y-%m-%dT%H:%M:%S", tm);
	}

	if (valid_from == 0) {
		snprintf(ret, sizeof(ret), "before %s", to);
		return ret;
	}
	if (valid_to == 0xffffffffffffffffULL) {
		snprintf(ret, sizeof(ret), "after %s", from);
		return ret;
	}

	snprintf(ret, sizeof(ret), "from %s to %s", from, to);
	return ret;
}

static void
add_flag_option(Buffer *c, const char *name)
{
	debug3("%s: %s", __func__, name);
	buffer_put_cstring(c, name);
	buffer_put_string(c, NULL, 0);
}

static void
add_string_option(Buffer *c, const char *name, const char *value)
{
	Buffer b;

	debug3("%s: %s=%s", __func__, name, value);
	buffer_init(&b);
	buffer_put_cstring(&b, value);

	buffer_put_cstring(c, name);
	buffer_put_string(c, buffer_ptr(&b), buffer_len(&b));

	buffer_free(&b);
}

#define OPTIONS_CRITICAL	1
#define OPTIONS_EXTENSIONS	2
static void
prepare_options_buf(Buffer *c, int which)
{
	buffer_clear(c);
	if ((which & OPTIONS_CRITICAL) != 0 &&
	    certflags_command != NULL)
		add_string_option(c, "force-command", certflags_command);
	if ((which & OPTIONS_EXTENSIONS) != 0 &&
	    (certflags_flags & CERTOPT_X_FWD) != 0)
		add_flag_option(c, "permit-X11-forwarding");
	if ((which & OPTIONS_EXTENSIONS) != 0 &&
	    (certflags_flags & CERTOPT_AGENT_FWD) != 0)
		add_flag_option(c, "permit-agent-forwarding");
	if ((which & OPTIONS_EXTENSIONS) != 0 &&
	    (certflags_flags & CERTOPT_PORT_FWD) != 0)
		add_flag_option(c, "permit-port-forwarding");
	if ((which & OPTIONS_EXTENSIONS) != 0 &&
	    (certflags_flags & CERTOPT_PTY) != 0)
		add_flag_option(c, "permit-pty");
	if ((which & OPTIONS_EXTENSIONS) != 0 &&
	    (certflags_flags & CERTOPT_USER_RC) != 0)
		add_flag_option(c, "permit-user-rc");
	if ((which & OPTIONS_CRITICAL) != 0 &&
	    certflags_src_addr != NULL)
		add_string_option(c, "source-address", certflags_src_addr);
}

static Key *
load_pkcs11_key(char *path)
{
#ifdef ENABLE_PKCS11
	Key **keys = NULL, *public, *private = NULL;
	int i, nkeys;

	if ((public = key_load_public(path, NULL)) == NULL)
		fatal("Couldn't load CA public key \"%s\"", path);

	nkeys = pkcs11_add_provider(pkcs11provider, identity_passphrase, &keys);
	debug3("%s: %d keys", __func__, nkeys);
	if (nkeys <= 0)
		fatal("cannot read public key from pkcs11");
	for (i = 0; i < nkeys; i++) {
		if (key_equal_public(public, keys[i])) {
			private = keys[i];
			continue;
		}
		key_free(keys[i]);
	}
	xfree(keys);
	key_free(public);
	return private;
#else
	fatal("no pkcs11 support");
#endif /* ENABLE_PKCS11 */
}

static void
do_ca_sign(struct passwd *pw, int argc, char **argv)
{
	int i, fd;
	u_int n;
	Key *ca, *public;
	char *otmp, *tmp, *cp, *out, *comment, **plist = NULL;
	FILE *f;
	int v00 = 0; /* legacy keys */

	if (key_type_name != NULL) {
		switch (key_type_from_name(key_type_name)) {
		case KEY_RSA_CERT_V00:
		case KEY_DSA_CERT_V00:
			v00 = 1;
			break;
		case KEY_UNSPEC:
			if (strcasecmp(key_type_name, "v00") == 0) {
				v00 = 1;
				break;
			} else if (strcasecmp(key_type_name, "v01") == 0)
				break;
			/* FALLTHROUGH */
		default:
			fprintf(stderr, "unknown key type %s\n", key_type_name);
			exit(1);
		}
	}

	pkcs11_init(1);
	tmp = tilde_expand_filename(ca_key_path, pw->pw_uid);
	if (pkcs11provider != NULL) {
		if ((ca = load_pkcs11_key(tmp)) == NULL)
			fatal("No PKCS#11 key matching %s found", ca_key_path);
	} else if ((ca = load_identity(tmp)) == NULL)
		fatal("Couldn't load CA key \"%s\"", tmp);
	xfree(tmp);

	for (i = 0; i < argc; i++) {
		/* Split list of principals */
		n = 0;
		if (cert_principals != NULL) {
			otmp = tmp = xstrdup(cert_principals);
			plist = NULL;
			for (; (cp = strsep(&tmp, ",")) != NULL; n++) {
				plist = xrealloc(plist, n + 1, sizeof(*plist));
				if (*(plist[n] = xstrdup(cp)) == '\0')
					fatal("Empty principal name");
			}
			xfree(otmp);
		}
	
		tmp = tilde_expand_filename(argv[i], pw->pw_uid);
		if ((public = key_load_public(tmp, &comment)) == NULL)
			fatal("%s: unable to open \"%s\"", __func__, tmp);
		if (public->type != KEY_RSA && public->type != KEY_DSA &&
		    public->type != KEY_ECDSA)
			fatal("%s: key \"%s\" type %s cannot be certified",
			    __func__, tmp, key_type(public));

		/* Prepare certificate to sign */
		if (key_to_certified(public, v00) != 0)
			fatal("Could not upgrade key %s to certificate", tmp);
		public->cert->type = cert_key_type;
		public->cert->serial = (u_int64_t)cert_serial;
		public->cert->key_id = xstrdup(cert_key_id);
		public->cert->nprincipals = n;
		public->cert->principals = plist;
		public->cert->valid_after = cert_valid_from;
		public->cert->valid_before = cert_valid_to;
		if (v00) {
			prepare_options_buf(&public->cert->critical,
			    OPTIONS_CRITICAL|OPTIONS_EXTENSIONS);
		} else {
			prepare_options_buf(&public->cert->critical,
			    OPTIONS_CRITICAL);
			prepare_options_buf(&public->cert->extensions,
			    OPTIONS_EXTENSIONS);
		}
		public->cert->signature_key = key_from_private(ca);

		if (key_certify(public, ca) != 0)
			fatal("Couldn't not certify key %s", tmp);

		if ((cp = strrchr(tmp, '.')) != NULL && strcmp(cp, ".pub") == 0)
			*cp = '\0';
		xasprintf(&out, "%s-cert.pub", tmp);
		xfree(tmp);

		if ((fd = open(out, O_WRONLY|O_CREAT|O_TRUNC, 0644)) == -1)
			fatal("Could not open \"%s\" for writing: %s", out,
			    strerror(errno));
		if ((f = fdopen(fd, "w")) == NULL)
			fatal("%s: fdopen: %s", __func__, strerror(errno));
		if (!key_write(public, f))
			fatal("Could not write certified key to %s", out);
		fprintf(f, " %s\n", comment);
		fclose(f);

		if (!quiet) {
			logit("Signed %s key %s: id \"%s\" serial %llu%s%s "
			    "valid %s", key_cert_type(public), 
			    out, public->cert->key_id,
			    (unsigned long long)public->cert->serial,
			    cert_principals != NULL ? " for " : "",
			    cert_principals != NULL ? cert_principals : "",
			    fmt_validity(cert_valid_from, cert_valid_to));
		}

		key_free(public);
		xfree(out);
	}
	pkcs11_terminate();
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

static u_int64_t
parse_absolute_time(const char *s)
{
	struct tm tm;
	time_t tt;
	char buf[32], *fmt;

	/*
	 * POSIX strptime says "The application shall ensure that there 
	 * is white-space or other non-alphanumeric characters between
	 * any two conversion specifications" so arrange things this way.
	 */
	switch (strlen(s)) {
	case 8:
		fmt = "%Y-%m-%d";
		snprintf(buf, sizeof(buf), "%.4s-%.2s-%.2s", s, s + 4, s + 6);
		break;
	case 14:
		fmt = "%Y-%m-%dT%H:%M:%S";
		snprintf(buf, sizeof(buf), "%.4s-%.2s-%.2sT%.2s:%.2s:%.2s",
		    s, s + 4, s + 6, s + 8, s + 10, s + 12);
		break;
	default:
		fatal("Invalid certificate time format %s", s);
	}

	bzero(&tm, sizeof(tm));
	if (strptime(buf, fmt, &tm) == NULL)
		fatal("Invalid certificate time %s", s);
	if ((tt = mktime(&tm)) < 0)
		fatal("Certificate time %s cannot be represented", s);
	return (u_int64_t)tt;
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
	 * from := [+-]timespec | YYYYMMDD | YYYYMMDDHHMMSS
	 *   to := [+-]timespec | YYYYMMDD | YYYYMMDDHHMMSS
	 */
	from = xstrdup(timespec);
	to = strchr(from, ':');
	if (to == NULL || from == to || *(to + 1) == '\0')
		fatal("Invalid certificate life specification %s", timespec);
	*to++ = '\0';

	if (*from == '-' || *from == '+')
		cert_valid_from = parse_relative_time(from, now);
	else
		cert_valid_from = parse_absolute_time(from);

	if (*to == '-' || *to == '+')
		cert_valid_to = parse_relative_time(to, cert_valid_from);
	else
		cert_valid_to = parse_absolute_time(to);

	if (cert_valid_to <= cert_valid_from)
		fatal("Empty certificate validity interval");
	xfree(from);
}

static void
add_cert_option(char *opt)
{
	char *val;

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
	} else
		fatal("Unsupported certificate option \"%s\"", opt);
}

static void
show_options(const Buffer *optbuf, int v00, int in_critical)
{
	u_char *name, *data;
	u_int dlen;
	Buffer options, option;

	buffer_init(&options);
	buffer_append(&options, buffer_ptr(optbuf), buffer_len(optbuf));

	buffer_init(&option);
	while (buffer_len(&options) != 0) {
		name = buffer_get_string(&options, NULL);
		data = buffer_get_string_ptr(&options, &dlen);
		buffer_append(&option, data, dlen);
		printf("                %s", name);
		if ((v00 || !in_critical) && 
		    (strcmp(name, "permit-X11-forwarding") == 0 ||
		    strcmp(name, "permit-agent-forwarding") == 0 ||
		    strcmp(name, "permit-port-forwarding") == 0 ||
		    strcmp(name, "permit-pty") == 0 ||
		    strcmp(name, "permit-user-rc") == 0))
			printf("\n");
		else if ((v00 || in_critical) &&
		    (strcmp(name, "force-command") == 0 ||
		    strcmp(name, "source-address") == 0)) {
			data = buffer_get_string(&option, NULL);
			printf(" %s\n", data);
			xfree(data);
		} else {
			printf(" UNKNOWN OPTION (len %u)\n",
			    buffer_len(&option));
			buffer_clear(&option);
		}
		xfree(name);
		if (buffer_len(&option) != 0)
			fatal("Option corrupt: extra data at end");
	}
	buffer_free(&option);
	buffer_free(&options);
}

static void
do_show_cert(struct passwd *pw)
{
	Key *key;
	struct stat st;
	char *key_fp, *ca_fp;
	u_int i, v00;

	if (!have_identity)
		ask_filename(pw, "Enter file in which the key is");
	if (stat(identity_file, &st) < 0)
		fatal("%s: %s: %s", __progname, identity_file, strerror(errno));
	if ((key = key_load_public(identity_file, NULL)) == NULL)
		fatal("%s is not a public key", identity_file);
	if (!key_is_cert(key))
		fatal("%s is not a certificate", identity_file);
	v00 = key->type == KEY_RSA_CERT_V00 || key->type == KEY_DSA_CERT_V00;

	key_fp = key_fingerprint(key, SSH_FP_MD5, SSH_FP_HEX);
	ca_fp = key_fingerprint(key->cert->signature_key,
	    SSH_FP_MD5, SSH_FP_HEX);

	printf("%s:\n", identity_file);
	printf("        Type: %s %s certificate\n", key_ssh_name(key),
	    key_cert_type(key));
	printf("        Public key: %s %s\n", key_type(key), key_fp);
	printf("        Signing CA: %s %s\n",
	    key_type(key->cert->signature_key), ca_fp);
	printf("        Key ID: \"%s\"\n", key->cert->key_id);
	if (!v00) {
		printf("        Serial: %llu\n",
		    (unsigned long long)key->cert->serial);
	}
	printf("        Valid: %s\n",
	    fmt_validity(key->cert->valid_after, key->cert->valid_before));
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
	if (buffer_len(&key->cert->critical) == 0)
		printf("(none)\n");
	else {
		printf("\n");
		show_options(&key->cert->critical, v00, 1);
	}
	if (!v00) {
		printf("        Extensions: ");
		if (buffer_len(&key->cert->extensions) == 0)
			printf("(none)\n");
		else {
			printf("\n");
			show_options(&key->cert->extensions, v00, 0);
		}
	}
	exit(0);
}

static void
load_krl(const char *path, struct ssh_krl **krlp)
{
	Buffer krlbuf;
	int fd;

	buffer_init(&krlbuf);
	if ((fd = open(path, O_RDONLY)) == -1)
		fatal("open %s: %s", path, strerror(errno));
	if (!key_load_file(fd, path, &krlbuf))
		fatal("Unable to load KRL");
	close(fd);
	/* XXX check sigs */
	if (ssh_krl_from_blob(&krlbuf, krlp, NULL, 0) != 0 ||
	    *krlp == NULL)
		fatal("Invalid KRL file");
	buffer_free(&krlbuf);
}

static void
update_krl_from_file(struct passwd *pw, const char *file, const Key *ca,
    struct ssh_krl *krl)
{
	Key *key = NULL;
	u_long lnum = 0;
	char *path, *cp, *ep, line[SSH_MAX_PUBKEY_BYTES];
	unsigned long long serial, serial2;
	int i, was_explicit_key, was_sha1, r;
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
	while (read_keyfile_line(krl_spec, path, line, sizeof(line),
	    &lnum) == 0) {
		was_explicit_key = was_sha1 = 0;
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
			if (ca == NULL) {
				fatal("revoking certificated by serial number "
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
				fatal("%s: revoke serial failed",
				    __func__);
			}
		} else if (strncasecmp(cp, "id:", 3) == 0) {
			if (ca == NULL) {
				fatal("revoking certificated by key ID "
				    "requires specification of a CA key");
			}
			cp += 3;
			cp = cp + strspn(cp, " \t");
			if (ssh_krl_revoke_cert_by_key_id(krl, ca, cp) != 0)
				fatal("%s: revoke key ID failed", __func__);
		} else {
			if (strncasecmp(cp, "key:", 4) == 0) {
				cp += 4;
				cp = cp + strspn(cp, " \t");
				was_explicit_key = 1;
			} else if (strncasecmp(cp, "sha1:", 5) == 0) {
				cp += 5;
				cp = cp + strspn(cp, " \t");
				was_sha1 = 1;
			} else {
				/*
				 * Just try to process the line as a key.
				 * Parsing will fail if it isn't.
				 */
			}
			if ((key = key_new(KEY_UNSPEC)) == NULL)
				fatal("key_new");
			if (key_read(key, &cp) != 1)
				fatal("%s:%lu: invalid key", path, lnum);
			if (was_explicit_key)
				r = ssh_krl_revoke_key_explicit(krl, key);
			else if (was_sha1)
				r = ssh_krl_revoke_key_sha1(krl, key);
			else
				r = ssh_krl_revoke_key(krl, key);
			if (r != 0)
				fatal("%s: revoke key failed", __func__);
			key_free(key);
		}
	}
	if (strcmp(path, "-") != 0)
		fclose(krl_spec);
}

static void
do_gen_krl(struct passwd *pw, int updating, int argc, char **argv)
{
	struct ssh_krl *krl;
	struct stat sb;
	Key *ca = NULL;
	int fd, i;
	char *tmp;
	Buffer kbuf;

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
		tmp = tilde_expand_filename(ca_key_path, pw->pw_uid);
		if ((ca = key_load_public(tmp, NULL)) == NULL)
			fatal("Cannot load CA public key %s", tmp);
		xfree(tmp);
	}

	if (updating)
		load_krl(identity_file, &krl);
	else if ((krl = ssh_krl_init()) == NULL)
		fatal("couldn't create KRL");

	if (cert_serial != 0)
		ssh_krl_set_version(krl, cert_serial);
	if (identity_comment != NULL)
		ssh_krl_set_comment(krl, identity_comment);

	for (i = 0; i < argc; i++)
		update_krl_from_file(pw, argv[i], ca, krl);

	buffer_init(&kbuf);
	if (ssh_krl_to_blob(krl, &kbuf, NULL, 0) != 0)
		fatal("Couldn't generate KRL");
	if ((fd = open(identity_file, O_WRONLY|O_CREAT|O_TRUNC, 0644)) == -1)
		fatal("open %s: %s", identity_file, strerror(errno));
	if (atomicio(vwrite, fd, buffer_ptr(&kbuf), buffer_len(&kbuf)) !=
	    buffer_len(&kbuf))
		fatal("write %s: %s", identity_file, strerror(errno));
	close(fd);
	buffer_free(&kbuf);
	ssh_krl_free(krl);
}

static void
do_check_krl(struct passwd *pw, int argc, char **argv)
{
	int i, r, ret = 0;
	char *comment;
	struct ssh_krl *krl;
	Key *k;

	if (*identity_file == '\0')
		fatal("KRL checking requires an input file");
	load_krl(identity_file, &krl);
	for (i = 0; i < argc; i++) {
		if ((k = key_load_public(argv[i], &comment)) == NULL)
			fatal("Cannot load public key %s", argv[i]);
		r = ssh_krl_check_key(krl, k);
		printf("%s%s%s%s: %s\n", argv[i],
		    *comment ? " (" : "", comment, *comment ? ")" : "",
		    r == 0 ? "ok" : "REVOKED");
		if (r != 0)
			ret = 1;
		key_free(k);
		free(comment);
	}
	ssh_krl_free(krl);
	exit(ret);
}

static void
usage(void)
{
	fprintf(stderr, "usage: %s [options]\n", __progname);
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "  -A          Generate non-existent host keys for all key types.\n");
	fprintf(stderr, "  -a trials   Number of trials for screening DH-GEX moduli.\n");
	fprintf(stderr, "  -B          Show bubblebabble digest of key file.\n");
	fprintf(stderr, "  -b bits     Number of bits in the key to create.\n");
	fprintf(stderr, "  -C comment  Provide new comment.\n");
	fprintf(stderr, "  -c          Change comment in private and public key files.\n");
#ifdef ENABLE_PKCS11
	fprintf(stderr, "  -D pkcs11   Download public key from pkcs11 token.\n");
#endif
	fprintf(stderr, "  -e          Export OpenSSH to foreign format key file.\n");
	fprintf(stderr, "  -F hostname Find hostname in known hosts file.\n");
	fprintf(stderr, "  -f filename Filename of the key file.\n");
	fprintf(stderr, "  -G file     Generate candidates for DH-GEX moduli.\n");
	fprintf(stderr, "  -g          Use generic DNS resource record format.\n");
	fprintf(stderr, "  -H          Hash names in known_hosts file.\n");
	fprintf(stderr, "  -h          Generate host certificate instead of a user certificate.\n");
	fprintf(stderr, "  -I key_id   Key identifier to include in certificate.\n");
	fprintf(stderr, "  -i          Import foreign format to OpenSSH key file.\n");
	fprintf(stderr, "  -J number   Screen this number of moduli lines.\n");
	fprintf(stderr, "  -j number   Start screening moduli at specified line.\n");
	fprintf(stderr, "  -K checkpt  Write checkpoints to this file.\n");
	fprintf(stderr, "  -k          Generate a KRL file.\n");
	fprintf(stderr, "  -L          Print the contents of a certificate.\n");
	fprintf(stderr, "  -l          Show fingerprint of key file.\n");
	fprintf(stderr, "  -M memory   Amount of memory (MB) to use for generating DH-GEX moduli.\n");
	fprintf(stderr, "  -m key_fmt  Conversion format for -e/-i (PEM|PKCS8|RFC4716).\n");
	fprintf(stderr, "  -N phrase   Provide new passphrase.\n");
	fprintf(stderr, "  -n name,... User/host principal names to include in certificate\n");
	fprintf(stderr, "  -O option   Specify a certificate option.\n");
	fprintf(stderr, "  -P phrase   Provide old passphrase.\n");
	fprintf(stderr, "  -p          Change passphrase of private key file.\n");
	fprintf(stderr, "  -Q          Test whether key(s) are revoked in KRL.\n");
	fprintf(stderr, "  -q          Quiet.\n");
	fprintf(stderr, "  -R hostname Remove host from known_hosts file.\n");
	fprintf(stderr, "  -r hostname Print DNS resource record.\n");
	fprintf(stderr, "  -S start    Start point (hex) for generating DH-GEX moduli.\n");
	fprintf(stderr, "  -s ca_key   Certify keys with CA key.\n");
	fprintf(stderr, "  -T file     Screen candidates for DH-GEX moduli.\n");
	fprintf(stderr, "  -t type     Specify type of key to create.\n");
	fprintf(stderr, "  -u          Update KRL rather than creating a new one.\n");
	fprintf(stderr, "  -V from:to  Specify certificate validity interval.\n");
	fprintf(stderr, "  -v          Verbose.\n");
	fprintf(stderr, "  -W gen      Generator to use for generating DH-GEX moduli.\n");
	fprintf(stderr, "  -y          Read private key file and print public key.\n");
	fprintf(stderr, "  -z serial   Specify a serial number.\n");

	exit(1);
}

/*
 * Main program for key management.
 */
int
main(int argc, char **argv)
{
	char dotsshdir[MAXPATHLEN], comment[1024], *passphrase1, *passphrase2;
	char *checkpoint = NULL;
	char out_file[MAXPATHLEN], *ep, *rr_hostname = NULL;
	Key *private, *public;
	struct passwd *pw;
	struct stat st;
	int opt, type, fd;
	u_int32_t memory = 0, generator_wanted = 0, trials = 100;
	int do_gen_candidates = 0, do_screen_candidates = 0;
	int gen_all_hostkeys = 0, gen_krl = 0, update_krl = 0, check_krl = 0;
	unsigned long start_lineno = 0, lines_to_process = 0;
	BIGNUM *start = NULL;
	FILE *f;
	const char *errstr;

	extern int optind;
	extern char *optarg;

	/* Ensure that fds 0, 1 and 2 are open or directed to /dev/null */
	sanitise_stdfd();

	__progname = ssh_get_progname(argv[0]);

	OpenSSL_add_all_algorithms();
	log_init(argv[0], SYSLOG_LEVEL_INFO, SYSLOG_FACILITY_USER, 1);

	seed_rng();

	/* we need this for the home * directory.  */
	pw = getpwuid(getuid());
	if (!pw) {
		printf("You don't exist, go away!\n");
		exit(1);
	}
	if (gethostname(hostname, sizeof(hostname)) < 0) {
		perror("gethostname");
		exit(1);
	}

	while ((opt = getopt(argc, argv, "ABHLQXceghiklpquvxy"
	    "C:D:F:G:I:J:K:M:N:O:P:R:S:T:V:W:a:b:f:g:j:m:n:r:s:t:z:")) != -1) {
		switch (opt) {
		case 'A':
			gen_all_hostkeys = 1;
			break;
		case 'b':
			bits = (u_int32_t)strtonum(optarg, 256, 32768, &errstr);
			if (errstr)
				fatal("Bits has bad value %s (%s)",
					optarg, errstr);
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
		case 'J':
			lines_to_process = strtoul(optarg, NULL, 10);
                        break;
		case 'j':
			start_lineno = strtoul(optarg, NULL, 10);
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
				break;
			}
			if (strcasecmp(optarg, "PEM") == 0) {
				convert_format = FMT_PEM;
				break;
			}
			fatal("Unsupported conversion format \"%s\"", optarg);
		case 'n':
			cert_principals = optarg;
			break;
		case 'p':
			change_passphrase = 1;
			break;
		case 'c':
			change_comment = 1;
			break;
		case 'f':
			if (strlcpy(identity_file, optarg, sizeof(identity_file)) >=
			    sizeof(identity_file))
				fatal("Identity filename too long");
			have_identity = 1;
			break;
		case 'g':
			print_generic = 1;
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
			add_cert_option(optarg);
			break;
		case 'C':
			identity_comment = optarg;
			break;
		case 'q':
			quiet = 1;
			break;
		case 'e':
		case 'x':
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
		case 'W':
			generator_wanted = (u_int32_t)strtonum(optarg, 1,
			    UINT_MAX, &errstr);
			if (errstr)
				fatal("Desired generator has bad value: %s (%s)",
					optarg, errstr);
			break;
		case 'a':
			trials = (u_int32_t)strtonum(optarg, 1, UINT_MAX, &errstr);
			if (errstr)
				fatal("Invalid number of trials: %s (%s)",
					optarg, errstr);
			break;
		case 'M':
			memory = (u_int32_t)strtonum(optarg, 1, UINT_MAX, &errstr);
			if (errstr)
				fatal("Memory limit is %s: %s", errstr, optarg);
			break;
		case 'G':
			do_gen_candidates = 1;
			if (strlcpy(out_file, optarg, sizeof(out_file)) >=
			    sizeof(out_file))
				fatal("Output filename too long");
			break;
		case 'T':
			do_screen_candidates = 1;
			if (strlcpy(out_file, optarg, sizeof(out_file)) >=
			    sizeof(out_file))
				fatal("Output filename too long");
			break;
		case 'K':
			if (strlen(optarg) >= MAXPATHLEN)
				fatal("Checkpoint filename too long");
			checkpoint = xstrdup(optarg);
			break;
		case 'S':
			/* XXX - also compare length against bits */
			if (BN_hex2bn(&start, optarg) == 0)
				fatal("Invalid start point.");
			break;
		case 'V':
			parse_cert_times(optarg);
			break;
		case 'z':
			errno = 0;
			cert_serial = strtoull(optarg, &ep, 10);
			if (*optarg < '0' || *optarg > '9' || *ep != '\0' ||
			    (errno == ERANGE && cert_serial == ULLONG_MAX))
				fatal("Invalid serial number \"%s\"", optarg);
			break;
		case '?':
		default:
			usage();
		}
	}

	/* reinit */
	log_init(argv[0], log_level, SYSLOG_FACILITY_USER, 1);

	argv += optind;
	argc -= optind;

	if (ca_key_path != NULL) {
		if (argc < 1 && !gen_krl) {
			printf("Too few arguments.\n");
			usage();
		}
	} else if (argc > 0 && !gen_krl && !check_krl) {
		printf("Too many arguments.\n");
		usage();
	}
	if (change_passphrase && change_comment) {
		printf("Can only have one of -p and -c.\n");
		usage();
	}
	if (print_fingerprint && (delete_host || hash_hosts)) {
		printf("Cannot use -l with -H or -R.\n");
		usage();
	}
	if (gen_krl) {
		do_gen_krl(pw, update_krl, argc, argv);
		return (0);
	}
	if (check_krl) {
		do_check_krl(pw, argc, argv);
		return (0);
	}
	if (ca_key_path != NULL) {
		if (cert_key_id == NULL)
			fatal("Must specify key id (-I) when certifying");
		do_ca_sign(pw, argc, argv);
	}
	if (show_cert)
		do_show_cert(pw);
	if (delete_host || hash_hosts || find_host)
		do_known_hosts(pw, rr_hostname);
	if (pkcs11provider != NULL)
		do_download(pw);
	if (print_fingerprint || print_bubblebabble)
		do_fingerprint(pw);
	if (change_passphrase)
		do_change_passphrase(pw);
	if (change_comment)
		do_change_comment(pw);
	if (convert_to)
		do_convert_to(pw);
	if (convert_from)
		do_convert_from(pw);
	if (print_public)
		do_print_public(pw);
	if (rr_hostname != NULL) {
		unsigned int n = 0;

		if (have_identity) {
			n = do_print_resource_record(pw,
			    identity_file, rr_hostname);
			if (n == 0) {
				perror(identity_file);
				exit(1);
			}
			exit(0);
		} else {

			n += do_print_resource_record(pw,
			    _PATH_HOST_RSA_KEY_FILE, rr_hostname);
			n += do_print_resource_record(pw,
			    _PATH_HOST_DSA_KEY_FILE, rr_hostname);
			n += do_print_resource_record(pw,
			    _PATH_HOST_ECDSA_KEY_FILE, rr_hostname);

			if (n == 0)
				fatal("no keys found.");
			exit(0);
		}
	}

	if (do_gen_candidates) {
		FILE *out = fopen(out_file, "w");

		if (out == NULL) {
			error("Couldn't open modulus candidate file \"%s\": %s",
			    out_file, strerror(errno));
			return (1);
		}
		if (bits == 0)
			bits = DEFAULT_BITS;
		if (gen_candidates(out, memory, bits, start) != 0)
			fatal("modulus candidate generation failed");

		return (0);
	}

	if (do_screen_candidates) {
		FILE *in;
		FILE *out = fopen(out_file, "a");

		if (have_identity && strcmp(identity_file, "-") != 0) {
			if ((in = fopen(identity_file, "r")) == NULL) {
				fatal("Couldn't open modulus candidate "
				    "file \"%s\": %s", identity_file,
				    strerror(errno));
			}
		} else
			in = stdin;

		if (out == NULL) {
			fatal("Couldn't open moduli file \"%s\": %s",
			    out_file, strerror(errno));
		}
		if (prime_test(in, out, trials, generator_wanted, checkpoint,
		    start_lineno, lines_to_process) != 0)
			fatal("modulus screening failed");
		return (0);
	}

	if (gen_all_hostkeys) {
		do_gen_all_hostkeys(pw);
		return (0);
	}

	arc4random_stir();

	if (key_type_name == NULL)
		key_type_name = "rsa";

	type = key_type_from_name(key_type_name);
	type_bits_valid(type, &bits);

	if (!quiet)
		printf("Generating public/private %s key pair.\n", key_type_name);
	private = key_generate(type, bits);
	if (private == NULL) {
		fprintf(stderr, "key_generate failed\n");
		exit(1);
	}
	public  = key_from_private(private);

	if (!have_identity)
		ask_filename(pw, "Enter file in which to save the key");

	/* Create ~/.ssh directory if it doesn't already exist. */
	snprintf(dotsshdir, sizeof dotsshdir, "%s/%s",
	    pw->pw_dir, _PATH_SSH_USER_DIR);
	if (strstr(identity_file, dotsshdir) != NULL) {
		if (stat(dotsshdir, &st) < 0) {
			if (errno != ENOENT) {
				error("Could not stat %s: %s", dotsshdir,
				    strerror(errno));
			} else if (mkdir(dotsshdir, 0700) < 0) {
				error("Could not create directory '%s': %s",
				    dotsshdir, strerror(errno));
			} else if (!quiet)
				printf("Created directory '%s'.\n", dotsshdir);
		}
	}
	/* If the file already exists, ask the user to confirm. */
	if (stat(identity_file, &st) >= 0) {
		char yesno[3];
		printf("%s already exists.\n", identity_file);
		printf("Overwrite (y/n)? ");
		fflush(stdout);
		if (fgets(yesno, sizeof(yesno), stdin) == NULL)
			exit(1);
		if (yesno[0] != 'y' && yesno[0] != 'Y')
			exit(1);
	}
	/* Ask for a passphrase (twice). */
	if (identity_passphrase)
		passphrase1 = xstrdup(identity_passphrase);
	else if (identity_new_passphrase)
		passphrase1 = xstrdup(identity_new_passphrase);
	else {
passphrase_again:
		passphrase1 =
			read_passphrase("Enter passphrase (empty for no "
			    "passphrase): ", RP_ALLOW_STDIN);
		passphrase2 = read_passphrase("Enter same passphrase again: ",
		    RP_ALLOW_STDIN);
		if (strcmp(passphrase1, passphrase2) != 0) {
			/*
			 * The passphrases do not match.  Clear them and
			 * retry.
			 */
			memset(passphrase1, 0, strlen(passphrase1));
			memset(passphrase2, 0, strlen(passphrase2));
			xfree(passphrase1);
			xfree(passphrase2);
			printf("Passphrases do not match.  Try again.\n");
			goto passphrase_again;
		}
		/* Clear the other copy of the passphrase. */
		memset(passphrase2, 0, strlen(passphrase2));
		xfree(passphrase2);
	}

	if (identity_comment) {
		strlcpy(comment, identity_comment, sizeof(comment));
	} else {
		/* Create default comment field for the passphrase. */
		snprintf(comment, sizeof comment, "%s@%s", pw->pw_name, hostname);
	}

	/* Save the key with the given passphrase and comment. */
	if (!key_save_private(private, identity_file, passphrase1, comment)) {
		printf("Saving the key failed: %s.\n", identity_file);
		memset(passphrase1, 0, strlen(passphrase1));
		xfree(passphrase1);
		exit(1);
	}
	/* Clear the passphrase. */
	memset(passphrase1, 0, strlen(passphrase1));
	xfree(passphrase1);

	/* Clear the private key and the random number generator. */
	key_free(private);
	arc4random_stir();

	if (!quiet)
		printf("Your identification has been saved in %s.\n", identity_file);

	strlcat(identity_file, ".pub", sizeof(identity_file));
	fd = open(identity_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd == -1) {
		printf("Could not save your public key in %s\n", identity_file);
		exit(1);
	}
	f = fdopen(fd, "w");
	if (f == NULL) {
		printf("fdopen %s failed\n", identity_file);
		exit(1);
	}
	if (!key_write(public, f))
		fprintf(stderr, "write key failed\n");
	fprintf(f, " %s\n", comment);
	fclose(f);

	if (!quiet) {
		char *fp = key_fingerprint(public, SSH_FP_MD5, SSH_FP_HEX);
		char *ra = key_fingerprint(public, SSH_FP_MD5,
		    SSH_FP_RANDOMART);
		printf("Your public key has been saved in %s.\n",
		    identity_file);
		printf("The key fingerprint is:\n");
		printf("%s %s\n", fp, comment);
		printf("The key's randomart image is:\n");
		printf("%s\n", ra);
		xfree(ra);
		xfree(fp);
	}

	key_free(public);
	exit(0);
}
