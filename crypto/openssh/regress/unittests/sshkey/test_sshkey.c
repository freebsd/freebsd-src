/* 	$OpenBSD: test_sshkey.c,v 1.1 2014/06/24 01:14:18 djm Exp $ */
/*
 * Regress test for sshkey.h key management API
 *
 * Placed in the public domain
 */

#include "includes.h"

#include <sys/types.h>
#include <sys/param.h>
#include <stdio.h>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#include <stdlib.h>
#include <string.h>

#include <openssl/bn.h>
#include <openssl/rsa.h>
#include <openssl/dsa.h>
#ifdef OPENSSL_HAS_NISTP256
# include <openssl/ec.h>
#endif

#include "../test_helper/test_helper.h"

#include "ssherr.h"
#include "sshbuf.h"
#define SSHBUF_INTERNAL 1	/* access internals for testing */
#include "sshkey.h"

#include "authfile.h"
#include "common.h"
#include "ssh2.h"

void sshkey_tests(void);

static void
build_cert(struct sshbuf *b, const struct sshkey *k, const char *type,
    const struct sshkey *sign_key, const struct sshkey *ca_key)
{
	struct sshbuf *ca_buf, *pk, *principals, *critopts, *exts;
	u_char *sigblob;
	size_t siglen;

	ca_buf = sshbuf_new();
	ASSERT_INT_EQ(sshkey_to_blob_buf(ca_key, ca_buf), 0);

	/*
	 * Get the public key serialisation by rendering the key and skipping
	 * the type string. This is a bit of a hack :/
	 */
	pk = sshbuf_new();
	ASSERT_INT_EQ(sshkey_plain_to_blob_buf(k, pk), 0);
	ASSERT_INT_EQ(sshbuf_skip_string(pk), 0);

	principals = sshbuf_new();
	ASSERT_INT_EQ(sshbuf_put_cstring(principals, "gsamsa"), 0);
	ASSERT_INT_EQ(sshbuf_put_cstring(principals, "gregor"), 0);

	critopts = sshbuf_new();
	/* XXX fill this in */

	exts = sshbuf_new();
	/* XXX fill this in */

	ASSERT_INT_EQ(sshbuf_put_cstring(b, type), 0);
	ASSERT_INT_EQ(sshbuf_put_cstring(b, "noncenoncenonce!"), 0); /* nonce */
	ASSERT_INT_EQ(sshbuf_putb(b, pk), 0); /* public key serialisation */
	ASSERT_INT_EQ(sshbuf_put_u64(b, 1234), 0); /* serial */
	ASSERT_INT_EQ(sshbuf_put_u32(b, SSH2_CERT_TYPE_USER), 0); /* type */
	ASSERT_INT_EQ(sshbuf_put_cstring(b, "gregor"), 0); /* key ID */
	ASSERT_INT_EQ(sshbuf_put_stringb(b, principals), 0); /* principals */
	ASSERT_INT_EQ(sshbuf_put_u64(b, 0), 0); /* start */
	ASSERT_INT_EQ(sshbuf_put_u64(b, 0xffffffffffffffffULL), 0); /* end */
	ASSERT_INT_EQ(sshbuf_put_stringb(b, critopts), 0); /* options */
	ASSERT_INT_EQ(sshbuf_put_stringb(b, exts), 0); /* extensions */
	ASSERT_INT_EQ(sshbuf_put_string(b, NULL, 0), 0); /* reserved */
	ASSERT_INT_EQ(sshbuf_put_stringb(b, ca_buf), 0); /* signature key */
	ASSERT_INT_EQ(sshkey_sign(sign_key, &sigblob, &siglen,
	    sshbuf_ptr(b), sshbuf_len(b), 0), 0);
	ASSERT_INT_EQ(sshbuf_put_string(b, sigblob, siglen), 0); /* signature */

	free(sigblob);
	sshbuf_free(ca_buf);
	sshbuf_free(exts);
	sshbuf_free(critopts);
	sshbuf_free(principals);
	sshbuf_free(pk);
}

void
sshkey_tests(void)
{
	struct sshkey *k1, *k2, *k3, *k4, *kr, *kd, *ke, *kf;
	struct sshbuf *b;

	TEST_START("new invalid");
	k1 = sshkey_new(-42);
	ASSERT_PTR_EQ(k1, NULL);
	TEST_DONE();

	TEST_START("new/free KEY_UNSPEC");
	k1 = sshkey_new(KEY_UNSPEC);
	ASSERT_PTR_NE(k1, NULL);
	sshkey_free(k1);
	TEST_DONE();

	TEST_START("new/free KEY_RSA1");
	k1 = sshkey_new(KEY_RSA1);
	ASSERT_PTR_NE(k1, NULL);
	ASSERT_PTR_NE(k1->rsa, NULL);
	ASSERT_PTR_NE(k1->rsa->n, NULL);
	ASSERT_PTR_NE(k1->rsa->e, NULL);
	ASSERT_PTR_EQ(k1->rsa->p, NULL);
	sshkey_free(k1);
	TEST_DONE();

	TEST_START("new/free KEY_RSA");
	k1 = sshkey_new(KEY_RSA);
	ASSERT_PTR_NE(k1, NULL);
	ASSERT_PTR_NE(k1->rsa, NULL);
	ASSERT_PTR_NE(k1->rsa->n, NULL);
	ASSERT_PTR_NE(k1->rsa->e, NULL);
	ASSERT_PTR_EQ(k1->rsa->p, NULL);
	sshkey_free(k1);
	TEST_DONE();

	TEST_START("new/free KEY_DSA");
	k1 = sshkey_new(KEY_DSA);
	ASSERT_PTR_NE(k1, NULL);
	ASSERT_PTR_NE(k1->dsa, NULL);
	ASSERT_PTR_NE(k1->dsa->g, NULL);
	ASSERT_PTR_EQ(k1->dsa->priv_key, NULL);
	sshkey_free(k1);
	TEST_DONE();

	TEST_START("new/free KEY_ECDSA");
	k1 = sshkey_new(KEY_ECDSA);
	ASSERT_PTR_NE(k1, NULL);
	ASSERT_PTR_EQ(k1->ecdsa, NULL);  /* Can't allocate without NID */
	sshkey_free(k1);
	TEST_DONE();

	TEST_START("new/free KEY_ED25519");
	k1 = sshkey_new(KEY_ED25519);
	ASSERT_PTR_NE(k1, NULL);
	/* These should be blank until key loaded or generated */
	ASSERT_PTR_EQ(k1->ed25519_sk, NULL);
	ASSERT_PTR_EQ(k1->ed25519_pk, NULL);
	sshkey_free(k1);
	TEST_DONE();

	TEST_START("new_private KEY_RSA");
	k1 = sshkey_new_private(KEY_RSA);
	ASSERT_PTR_NE(k1, NULL);
	ASSERT_PTR_NE(k1->rsa, NULL);
	ASSERT_PTR_NE(k1->rsa->n, NULL);
	ASSERT_PTR_NE(k1->rsa->e, NULL);
	ASSERT_PTR_NE(k1->rsa->p, NULL);
	ASSERT_INT_EQ(sshkey_add_private(k1), 0);
	sshkey_free(k1);
	TEST_DONE();

	TEST_START("new_private KEY_DSA");
	k1 = sshkey_new_private(KEY_DSA);
	ASSERT_PTR_NE(k1, NULL);
	ASSERT_PTR_NE(k1->dsa, NULL);
	ASSERT_PTR_NE(k1->dsa->g, NULL);
	ASSERT_PTR_NE(k1->dsa->priv_key, NULL);
	ASSERT_INT_EQ(sshkey_add_private(k1), 0);
	sshkey_free(k1);
	TEST_DONE();

	TEST_START("generate KEY_RSA too small modulus");
	ASSERT_INT_EQ(sshkey_generate(KEY_RSA, 128, &k1),
	    SSH_ERR_INVALID_ARGUMENT);
	ASSERT_PTR_EQ(k1, NULL);
	TEST_DONE();

	TEST_START("generate KEY_RSA too large modulus");
	ASSERT_INT_EQ(sshkey_generate(KEY_RSA, 1 << 20, &k1),
	    SSH_ERR_INVALID_ARGUMENT);
	ASSERT_PTR_EQ(k1, NULL);
	TEST_DONE();

	TEST_START("generate KEY_DSA wrong bits");
	ASSERT_INT_EQ(sshkey_generate(KEY_DSA, 2048, &k1),
	    SSH_ERR_INVALID_ARGUMENT);
	ASSERT_PTR_EQ(k1, NULL);
	sshkey_free(k1);
	TEST_DONE();

	TEST_START("generate KEY_ECDSA wrong bits");
	ASSERT_INT_EQ(sshkey_generate(KEY_ECDSA, 42, &k1),
	    SSH_ERR_INVALID_ARGUMENT);
	ASSERT_PTR_EQ(k1, NULL);
	sshkey_free(k1);
	TEST_DONE();

	TEST_START("generate KEY_RSA");
	ASSERT_INT_EQ(sshkey_generate(KEY_RSA, 768, &kr), 0);
	ASSERT_PTR_NE(kr, NULL);
	ASSERT_PTR_NE(kr->rsa, NULL);
	ASSERT_PTR_NE(kr->rsa->n, NULL);
	ASSERT_PTR_NE(kr->rsa->e, NULL);
	ASSERT_PTR_NE(kr->rsa->p, NULL);
	ASSERT_INT_EQ(BN_num_bits(kr->rsa->n), 768);
	TEST_DONE();

	TEST_START("generate KEY_DSA");
	ASSERT_INT_EQ(sshkey_generate(KEY_DSA, 1024, &kd), 0);
	ASSERT_PTR_NE(kd, NULL);
	ASSERT_PTR_NE(kd->dsa, NULL);
	ASSERT_PTR_NE(kd->dsa->g, NULL);
	ASSERT_PTR_NE(kd->dsa->priv_key, NULL);
	TEST_DONE();

#ifdef OPENSSL_HAS_ECC
	TEST_START("generate KEY_ECDSA");
	ASSERT_INT_EQ(sshkey_generate(KEY_ECDSA, 256, &ke), 0);
	ASSERT_PTR_NE(ke, NULL);
	ASSERT_PTR_NE(ke->ecdsa, NULL);
	ASSERT_PTR_NE(EC_KEY_get0_public_key(ke->ecdsa), NULL);
	ASSERT_PTR_NE(EC_KEY_get0_private_key(ke->ecdsa), NULL);
	TEST_DONE();
#endif

	TEST_START("generate KEY_ED25519");
	ASSERT_INT_EQ(sshkey_generate(KEY_ED25519, 256, &kf), 0);
	ASSERT_PTR_NE(kf, NULL);
	ASSERT_INT_EQ(kf->type, KEY_ED25519);
	ASSERT_PTR_NE(kf->ed25519_pk, NULL);
	ASSERT_PTR_NE(kf->ed25519_sk, NULL);
	TEST_DONE();

	TEST_START("demote KEY_RSA");
	ASSERT_INT_EQ(sshkey_demote(kr, &k1), 0);
	ASSERT_PTR_NE(k1, NULL);
	ASSERT_PTR_NE(kr, k1);
	ASSERT_INT_EQ(k1->type, KEY_RSA);
	ASSERT_PTR_NE(k1->rsa, NULL);
	ASSERT_PTR_NE(k1->rsa->n, NULL);
	ASSERT_PTR_NE(k1->rsa->e, NULL);
	ASSERT_PTR_EQ(k1->rsa->p, NULL);
	TEST_DONE();

	TEST_START("equal KEY_RSA/demoted KEY_RSA");
	ASSERT_INT_EQ(sshkey_equal(kr, k1), 1);
	sshkey_free(k1);
	TEST_DONE();

	TEST_START("demote KEY_DSA");
	ASSERT_INT_EQ(sshkey_demote(kd, &k1), 0);
	ASSERT_PTR_NE(k1, NULL);
	ASSERT_PTR_NE(kd, k1);
	ASSERT_INT_EQ(k1->type, KEY_DSA);
	ASSERT_PTR_NE(k1->dsa, NULL);
	ASSERT_PTR_NE(k1->dsa->g, NULL);
	ASSERT_PTR_EQ(k1->dsa->priv_key, NULL);
	TEST_DONE();

	TEST_START("equal KEY_DSA/demoted KEY_DSA");
	ASSERT_INT_EQ(sshkey_equal(kd, k1), 1);
	sshkey_free(k1);
	TEST_DONE();

#ifdef OPENSSL_HAS_ECC
	TEST_START("demote KEY_ECDSA");
	ASSERT_INT_EQ(sshkey_demote(ke, &k1), 0);
	ASSERT_PTR_NE(k1, NULL);
	ASSERT_PTR_NE(ke, k1);
	ASSERT_INT_EQ(k1->type, KEY_ECDSA);
	ASSERT_PTR_NE(k1->ecdsa, NULL);
	ASSERT_INT_EQ(k1->ecdsa_nid, ke->ecdsa_nid);
	ASSERT_PTR_NE(EC_KEY_get0_public_key(ke->ecdsa), NULL);
	ASSERT_PTR_EQ(EC_KEY_get0_private_key(k1->ecdsa), NULL);
	TEST_DONE();

	TEST_START("equal KEY_ECDSA/demoted KEY_ECDSA");
	ASSERT_INT_EQ(sshkey_equal(ke, k1), 1);
	sshkey_free(k1);
	TEST_DONE();
#endif

	TEST_START("demote KEY_ED25519");
	ASSERT_INT_EQ(sshkey_demote(kf, &k1), 0);
	ASSERT_PTR_NE(k1, NULL);
	ASSERT_PTR_NE(kf, k1);
	ASSERT_INT_EQ(k1->type, KEY_ED25519);
	ASSERT_PTR_NE(k1->ed25519_pk, NULL);
	ASSERT_PTR_EQ(k1->ed25519_sk, NULL);
	TEST_DONE();

	TEST_START("equal KEY_ED25519/demoted KEY_ED25519");
	ASSERT_INT_EQ(sshkey_equal(kf, k1), 1);
	sshkey_free(k1);
	TEST_DONE();

	TEST_START("equal mismatched key types");
	ASSERT_INT_EQ(sshkey_equal(kd, kr), 0);
#ifdef OPENSSL_HAS_ECC
	ASSERT_INT_EQ(sshkey_equal(kd, ke), 0);
	ASSERT_INT_EQ(sshkey_equal(kr, ke), 0);
	ASSERT_INT_EQ(sshkey_equal(ke, kf), 0);
#endif
	ASSERT_INT_EQ(sshkey_equal(kd, kf), 0);
	TEST_DONE();

	TEST_START("equal different keys");
	ASSERT_INT_EQ(sshkey_generate(KEY_RSA, 768, &k1), 0);
	ASSERT_INT_EQ(sshkey_equal(kr, k1), 0);
	sshkey_free(k1);
	ASSERT_INT_EQ(sshkey_generate(KEY_DSA, 1024, &k1), 0);
	ASSERT_INT_EQ(sshkey_equal(kd, k1), 0);
	sshkey_free(k1);
#ifdef OPENSSL_HAS_ECC
	ASSERT_INT_EQ(sshkey_generate(KEY_ECDSA, 256, &k1), 0);
	ASSERT_INT_EQ(sshkey_equal(ke, k1), 0);
	sshkey_free(k1);
#endif
	ASSERT_INT_EQ(sshkey_generate(KEY_ED25519, 256, &k1), 0);
	ASSERT_INT_EQ(sshkey_equal(kf, k1), 0);
	sshkey_free(k1);
	TEST_DONE();

	sshkey_free(kr);
	sshkey_free(kd);
#ifdef OPENSSL_HAS_ECC
	sshkey_free(ke);
#endif
	sshkey_free(kf);

/* XXX certify test */
/* XXX sign test */
/* XXX verify test */

	TEST_START("nested certificate");
	ASSERT_INT_EQ(sshkey_load_cert(test_data_file("rsa_1"), &k1), 0);
	ASSERT_INT_EQ(sshkey_load_public(test_data_file("rsa_1.pub"), &k2,
	    NULL), 0);
	b = load_file("rsa_2");
	ASSERT_INT_EQ(sshkey_parse_private_fileblob(b, "", "rsa_1",
	    &k3, NULL), 0);
	sshbuf_reset(b);
	build_cert(b, k2, "ssh-rsa-cert-v01@openssh.com", k3, k1);
	ASSERT_INT_EQ(sshkey_from_blob(sshbuf_ptr(b), sshbuf_len(b), &k4),
	    SSH_ERR_KEY_CERT_INVALID_SIGN_KEY);
	ASSERT_PTR_EQ(k4, NULL);
	sshbuf_free(b);
	sshkey_free(k1);
	sshkey_free(k2);
	sshkey_free(k3);
	TEST_DONE();

}
