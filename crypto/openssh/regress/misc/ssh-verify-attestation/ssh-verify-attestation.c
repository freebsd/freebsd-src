/* $OpenBSD: ssh-verify-attestation.c,v 1.2 2024/12/06 10:37:42 djm Exp $ */
/*
 * Copyright (c) 2022-2024 Damien Miller
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * This is a small program to verify FIDO attestation objects that
 * ssh-keygen(1) can record when enrolling a FIDO key. It requires that
 * the attestation object and challenge used when creating the key be
 * recorded.
 *
 * Example usage:
 *
 * $ # Generate a random challenge.
 * $ dd if=/dev/urandom of=key_ecdsa_sk.challenge bs=32 count=1
 * $ # Generate a key, record the attestation blob.
 * $ ssh-keygen -f key_ecdsa_sk -t ecdsa-sk \
 *       -Ochallenge=key_ecdsa_sk.challenge \
 *       -Owrite-attestation=key_ecdsa_sk.attest -N ''
 * $ # Validate the challenge (-A = print attestation CA cert)
 * $ ./obj/ssh-verify-attestation -A key_ecdsa_sk key_ecdsa_sk.challenge \
 *       key_ecdsa_sk.attest
 *
 * Limitations/TODO:
 *
 * 1) It doesn't automatically detect the attestation statement format. It
 *    assumes the "packed" format used by FIDO2 keys. If that doesn't work,
 *    then try using the -U option to select the "fido-u2f" format.
 * 2) It makes assumptions about RK, UV, etc status of the key/cred.
 * 3) Probably bugs.
 *
 * Thanks to Markus Friedl and Pedro Martelletto for help getting this
 * working.
 */

#include "includes.h"

#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>

#include "xmalloc.h"
#include "log.h"
#include "sshbuf.h"
#include "sshkey.h"
#include "authfile.h"
#include "ssherr.h"
#include "misc.h"
#include "digest.h"
#include "crypto_api.h"

#include <fido.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/pem.h>

extern char *__progname;

#define ATTEST_MAGIC	"ssh-sk-attest-v01"

static int
prepare_fido_cred(fido_cred_t *cred, int credtype, const char *attfmt,
    const char *rp_id, struct sshbuf *b, const struct sshbuf *challenge,
    struct sshbuf **attestation_certp)
{
	struct sshbuf *attestation_cert = NULL, *sig = NULL, *authdata = NULL;
	char *magic = NULL;
	int r = SSH_ERR_INTERNAL_ERROR;

	*attestation_certp = NULL;

	/* Make sure it's the format we're expecting */
	if ((r = sshbuf_get_cstring(b, &magic, NULL)) != 0) {
		error_fr(r, "parse header");
		goto out;
	}
	if (strcmp(magic, ATTEST_MAGIC) != 0) {
		error_f("unsupported format");
		r = SSH_ERR_INVALID_FORMAT;
		goto out;
	}
	/* Parse the remaining fields */
	if ((r = sshbuf_froms(b, &attestation_cert)) != 0 ||
	    (r = sshbuf_froms(b, &sig)) != 0 ||
	    (r = sshbuf_froms(b, &authdata)) != 0 ||
	    (r = sshbuf_get_u32(b, NULL)) != 0 || /* reserved flags */
	    (r = sshbuf_get_string_direct(b, NULL, NULL)) != 0) { /* reserved */
		error_fr(r, "parse body");
		goto out;
	}
	debug3_f("attestation cert len=%zu, sig len=%zu, "
	    "authdata len=%zu challenge len=%zu", sshbuf_len(attestation_cert),
	    sshbuf_len(sig), sshbuf_len(authdata), sshbuf_len(challenge));

	fido_cred_set_type(cred, credtype);
	fido_cred_set_fmt(cred, attfmt);
	fido_cred_set_clientdata(cred, sshbuf_ptr(challenge),
	    sshbuf_len(challenge));
	fido_cred_set_rp(cred, rp_id, NULL);
	fido_cred_set_authdata(cred, sshbuf_ptr(authdata),
	    sshbuf_len(authdata));
	/* XXX set_extensions, set_rk, set_uv */
	fido_cred_set_x509(cred, sshbuf_ptr(attestation_cert),
	    sshbuf_len(attestation_cert));
	fido_cred_set_sig(cred, sshbuf_ptr(sig), sshbuf_len(sig));

	/* success */
	*attestation_certp = attestation_cert;
	attestation_cert = NULL;
	r = 0;
 out:
	free(magic);
	sshbuf_free(attestation_cert);
	sshbuf_free(sig);
	sshbuf_free(authdata);
	return r;
}

static uint8_t *
get_pubkey_from_cred_ecdsa(const fido_cred_t *cred, size_t *pubkey_len)
{
	const uint8_t *ptr;
	uint8_t *pubkey = NULL, *ret = NULL;
	BIGNUM *x = NULL, *y = NULL;
	EC_POINT *q = NULL;
	EC_GROUP *g = NULL;

	if ((x = BN_new()) == NULL ||
	    (y = BN_new()) == NULL ||
	    (g = EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1)) == NULL ||
	    (q = EC_POINT_new(g)) == NULL) {
		error_f("libcrypto setup failed");
		goto out;
	}
	if ((ptr = fido_cred_pubkey_ptr(cred)) == NULL) {
		error_f("fido_cred_pubkey_ptr failed");
		goto out;
	}
	if (fido_cred_pubkey_len(cred) != 64) {
		error_f("bad fido_cred_pubkey_len %zu",
		    fido_cred_pubkey_len(cred));
		goto out;
	}

	if (BN_bin2bn(ptr, 32, x) == NULL ||
	    BN_bin2bn(ptr + 32, 32, y) == NULL) {
		error_f("BN_bin2bn failed");
		goto out;
	}
	if (EC_POINT_set_affine_coordinates_GFp(g, q, x, y, NULL) != 1) {
		error_f("EC_POINT_set_affine_coordinates_GFp failed");
		goto out;
	}
	*pubkey_len = EC_POINT_point2oct(g, q,
	    POINT_CONVERSION_UNCOMPRESSED, NULL, 0, NULL);
	if (*pubkey_len == 0 || *pubkey_len > 2048) {
		error_f("bad pubkey length %zu", *pubkey_len);
		goto out;
	}
	if ((pubkey = malloc(*pubkey_len)) == NULL) {
		error_f("malloc pubkey failed");
		goto out;
	}
	if (EC_POINT_point2oct(g, q, POINT_CONVERSION_UNCOMPRESSED,
	    pubkey, *pubkey_len, NULL) == 0) {
		error_f("EC_POINT_point2oct failed");
		goto out;
	}
	/* success */
	ret = pubkey;
	pubkey = NULL;
 out:
	free(pubkey);
	EC_POINT_free(q);
	EC_GROUP_free(g);
	BN_clear_free(x);
	BN_clear_free(y);
	return ret;
}

/* copied from sshsk_ecdsa_assemble() */
static int
cred_matches_key_ecdsa(const fido_cred_t *cred, const struct sshkey *k)
{
	struct sshkey *key = NULL;
	struct sshbuf *b = NULL;
	EC_KEY *ec = NULL;
	uint8_t *pubkey = NULL;
	size_t pubkey_len;
	int r;

	if ((key = sshkey_new(KEY_ECDSA_SK)) == NULL) {
		error_f("sshkey_new failed");
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	key->ecdsa_nid = NID_X9_62_prime256v1;
	if ((key->pkey = EVP_PKEY_new()) == NULL ||
	    (ec = EC_KEY_new_by_curve_name(key->ecdsa_nid)) == NULL ||
	    (b = sshbuf_new()) == NULL) {
		error_f("allocation failed");
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	if ((pubkey = get_pubkey_from_cred_ecdsa(cred, &pubkey_len)) == NULL) {
		error_f("get_pubkey_from_cred_ecdsa failed");
		r = SSH_ERR_INVALID_FORMAT;
		goto out;
	}
	if ((r = sshbuf_put_string(b, pubkey, pubkey_len)) != 0) {
		error_fr(r, "sshbuf_put_string");
		goto out;
	}
	if ((r = sshbuf_get_eckey(b, ec)) != 0) {
		error_fr(r, "parse");
		r = SSH_ERR_INVALID_FORMAT;
		goto out;
	}
	if (sshkey_ec_validate_public(EC_KEY_get0_group(ec),
	    EC_KEY_get0_public_key(ec)) != 0) {
		error("Authenticator returned invalid ECDSA key");
		r = SSH_ERR_KEY_INVALID_EC_VALUE;
		goto out;
	}
	if (EVP_PKEY_set1_EC_KEY(key->pkey, ec) != 1) {
		/* XXX assume it is a allocation error */
		error_f("allocation failed");
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	key->sk_application = xstrdup(k->sk_application); /* XXX */
	if (!sshkey_equal_public(key, k)) {
		error("sshkey_equal_public failed");
		r = SSH_ERR_INVALID_ARGUMENT;
		goto out;
	}
	r = 0; /* success */
 out:
	EC_KEY_free(ec);
	free(pubkey);
	sshkey_free(key);
	sshbuf_free(b);
	return r;
}


/* copied from sshsk_ed25519_assemble() */
static int
cred_matches_key_ed25519(const fido_cred_t *cred, const struct sshkey *k)
{
	struct sshkey *key = NULL;
	const uint8_t *ptr;
	int r = -1;

	if ((ptr = fido_cred_pubkey_ptr(cred)) == NULL) {
		error_f("fido_cred_pubkey_ptr failed");
		goto out;
	}
	if (fido_cred_pubkey_len(cred) != ED25519_PK_SZ) {
		error_f("bad fido_cred_pubkey_len %zu",
		    fido_cred_pubkey_len(cred));
		goto out;
	}

	if ((key = sshkey_new(KEY_ED25519_SK)) == NULL) {
		error_f("sshkey_new failed");
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	if ((key->ed25519_pk = malloc(ED25519_PK_SZ)) == NULL) {
		error_f("malloc failed");
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	memcpy(key->ed25519_pk, ptr, ED25519_PK_SZ);
	key->sk_application = xstrdup(k->sk_application); /* XXX */
	if (!sshkey_equal_public(key, k)) {
		error("sshkey_equal_public failed");
		r = SSH_ERR_INVALID_ARGUMENT;
		goto out;
	}
	r = 0; /* success */
 out:
	sshkey_free(key);
	return r;
}

static int
cred_matches_key(const fido_cred_t *cred, const struct sshkey *k)
{
	switch (sshkey_type_plain(k->type)) {
	case KEY_ECDSA_SK:
		switch (k->ecdsa_nid) {
		case NID_X9_62_prime256v1:
			return cred_matches_key_ecdsa(cred, k);
			break;
		default:
			fatal("Unsupported ECDSA key size");
		}
		break;
	case KEY_ED25519_SK:
		return cred_matches_key_ed25519(cred, k);
	default:
		error_f("key type %s not supported", sshkey_type(k));
		return -1;
	}
}

int
main(int argc, char **argv)
{
	LogLevel log_level = SYSLOG_LEVEL_INFO;
	int r, ch, credtype = -1;
	struct sshkey *k = NULL;
	struct sshbuf *attestation = NULL, *challenge = NULL;
	struct sshbuf *attestation_cert = NULL;
	char *fp;
	const char *attfmt = "packed", *style = NULL;
	fido_cred_t *cred = NULL;
	int write_attestation_cert = 0;
	extern int optind;
	/* extern char *optarg; */

	ERR_load_crypto_strings();

	sanitise_stdfd();
	log_init(__progname, log_level, SYSLOG_FACILITY_AUTH, 1);

	while ((ch = getopt(argc, argv, "UAv")) != -1) {
		switch (ch) {
		case 'U':
			attfmt = "fido-u2f";
			break;
		case 'A':
			write_attestation_cert = 1;
			break;
		case 'v':
			if (log_level == SYSLOG_LEVEL_ERROR)
				log_level = SYSLOG_LEVEL_DEBUG1;
			else if (log_level < SYSLOG_LEVEL_DEBUG3)
				log_level++;
			break;
		default:
			goto usage;
		}
	}
	log_init(__progname, log_level, SYSLOG_FACILITY_AUTH, 1);
	argv += optind;
	argc -= optind;

	if (argc < 3) {
 usage:
		fprintf(stderr, "usage: %s [-vAU] "
		   "pubkey challenge attestation-blob\n", __progname);
		exit(1);
	}
	if ((r = sshkey_load_public(argv[0], &k, NULL)) != 0)
		fatal_r(r, "load key %s", argv[0]);
	if ((fp = sshkey_fingerprint(k, SSH_FP_HASH_DEFAULT,
	    SSH_FP_DEFAULT)) == NULL)
		fatal("sshkey_fingerprint failed");
	debug2("key %s: %s %s", argv[2], sshkey_type(k), fp);
	free(fp);
	if ((r = sshbuf_load_file(argv[1], &challenge)) != 0)
		fatal_r(r, "load challenge %s", argv[1]);
	if ((r = sshbuf_load_file(argv[2], &attestation)) != 0)
		fatal_r(r, "load attestation %s", argv[2]);
	if ((cred = fido_cred_new()) == NULL)
		fatal("fido_cred_new failed");

	switch (sshkey_type_plain(k->type)) {
	case KEY_ECDSA_SK:
		switch (k->ecdsa_nid) {
		case NID_X9_62_prime256v1:
			credtype = COSE_ES256;
			break;
		default:
			fatal("Unsupported ECDSA key size");
		}
		break;
	case KEY_ED25519_SK:
		credtype = COSE_EDDSA;
		break;
	default:
		fatal("unsupported key type %s", sshkey_type(k));
	}

	if ((r = prepare_fido_cred(cred, credtype, attfmt, k->sk_application,
	    attestation, challenge, &attestation_cert)) != 0)
		fatal_r(r, "prepare_fido_cred %s", argv[2]);
	if (fido_cred_x5c_ptr(cred) != NULL) {
		debug("basic attestation");
		if ((r = fido_cred_verify(cred)) != FIDO_OK)
			fatal("basic attestation failed");
		style = "basic";
	} else {
		debug("self attestation");
		if ((r = fido_cred_verify_self(cred)) != FIDO_OK)
			fatal("self attestation failed");
		style = "self";
	}
	if (cred_matches_key(cred, k) != 0)
		fatal("cred authdata does not match key");

	fido_cred_free(&cred);

	if (write_attestation_cert) {
		PEM_write(stdout, "CERTIFICATE", NULL,
		    sshbuf_ptr(attestation_cert), sshbuf_len(attestation_cert));
	}
	sshbuf_free(attestation_cert);

	logit("%s: verified %s attestation", argv[2], style);

	return (0);
}
