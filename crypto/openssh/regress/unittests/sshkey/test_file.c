/* 	$OpenBSD: test_file.c,v 1.10 2021/12/14 21:25:27 deraadt Exp $ */
/*
 * Regress test for sshkey.h key management API
 *
 * Placed in the public domain
 */

#include "includes.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef WITH_OPENSSL
#include <openssl/bn.h>
#include <openssl/rsa.h>
#include <openssl/dsa.h>
#include <openssl/objects.h>
#ifdef OPENSSL_HAS_NISTP256
# include <openssl/ec.h>
#endif /* OPENSSL_HAS_NISTP256 */
#endif /* WITH_OPENSSL */

#include "../test_helper/test_helper.h"

#include "ssherr.h"
#include "authfile.h"
#include "sshkey.h"
#include "sshbuf.h"
#include "digest.h"

#include "common.h"

void sshkey_file_tests(void);

void
sshkey_file_tests(void)
{
	struct sshkey *k1, *k2;
	struct sshbuf *buf, *pw;
#ifdef WITH_OPENSSL
	BIGNUM *a, *b, *c;
#endif
	char *cp;

	TEST_START("load passphrase");
	pw = load_text_file("pw");
	TEST_DONE();


#ifdef WITH_OPENSSL
	TEST_START("parse RSA from private");
	buf = load_file("rsa_1");
	ASSERT_INT_EQ(sshkey_parse_private_fileblob(buf, "", &k1, NULL), 0);
	sshbuf_free(buf);
	ASSERT_PTR_NE(k1, NULL);
	a = load_bignum("rsa_1.param.n");
	b = load_bignum("rsa_1.param.p");
	c = load_bignum("rsa_1.param.q");
	ASSERT_BIGNUM_EQ(rsa_n(k1), a);
	ASSERT_BIGNUM_EQ(rsa_p(k1), b);
	ASSERT_BIGNUM_EQ(rsa_q(k1), c);
	BN_free(a);
	BN_free(b);
	BN_free(c);
	TEST_DONE();

	TEST_START("parse RSA from private w/ passphrase");
	buf = load_file("rsa_1_pw");
	ASSERT_INT_EQ(sshkey_parse_private_fileblob(buf,
	    (const char *)sshbuf_ptr(pw), &k2, NULL), 0);
	sshbuf_free(buf);
	ASSERT_PTR_NE(k2, NULL);
	ASSERT_INT_EQ(sshkey_equal(k1, k2), 1);
	sshkey_free(k2);
	TEST_DONE();

	TEST_START("parse RSA from new-format");
	buf = load_file("rsa_n");
	ASSERT_INT_EQ(sshkey_parse_private_fileblob(buf, "", &k2, NULL), 0);
	sshbuf_free(buf);
	ASSERT_PTR_NE(k2, NULL);
	ASSERT_INT_EQ(sshkey_equal(k1, k2), 1);
	sshkey_free(k2);
	TEST_DONE();

	TEST_START("parse RSA from new-format w/ passphrase");
	buf = load_file("rsa_n_pw");
	ASSERT_INT_EQ(sshkey_parse_private_fileblob(buf,
	    (const char *)sshbuf_ptr(pw), &k2, NULL), 0);
	sshbuf_free(buf);
	ASSERT_PTR_NE(k2, NULL);
	ASSERT_INT_EQ(sshkey_equal(k1, k2), 1);
	sshkey_free(k2);
	TEST_DONE();

	TEST_START("load RSA from public");
	ASSERT_INT_EQ(sshkey_load_public(test_data_file("rsa_1.pub"), &k2,
	    NULL), 0);
	ASSERT_PTR_NE(k2, NULL);
	ASSERT_INT_EQ(sshkey_equal(k1, k2), 1);
	sshkey_free(k2);
	TEST_DONE();

	TEST_START("load RSA cert with SHA1 signature");
	ASSERT_INT_EQ(sshkey_load_cert(test_data_file("rsa_1_sha1"), &k2), 0);
	ASSERT_PTR_NE(k2, NULL);
	ASSERT_INT_EQ(k2->type, KEY_RSA_CERT);
	ASSERT_INT_EQ(sshkey_equal_public(k1, k2), 1);
	ASSERT_STRING_EQ(k2->cert->signature_type, "ssh-rsa");
	sshkey_free(k2);
	TEST_DONE();

	TEST_START("load RSA cert with SHA512 signature");
	ASSERT_INT_EQ(sshkey_load_cert(test_data_file("rsa_1_sha512"), &k2), 0);
	ASSERT_PTR_NE(k2, NULL);
	ASSERT_INT_EQ(k2->type, KEY_RSA_CERT);
	ASSERT_INT_EQ(sshkey_equal_public(k1, k2), 1);
	ASSERT_STRING_EQ(k2->cert->signature_type, "rsa-sha2-512");
	sshkey_free(k2);
	TEST_DONE();

	TEST_START("load RSA cert");
	ASSERT_INT_EQ(sshkey_load_cert(test_data_file("rsa_1"), &k2), 0);
	ASSERT_PTR_NE(k2, NULL);
	ASSERT_INT_EQ(k2->type, KEY_RSA_CERT);
	ASSERT_INT_EQ(sshkey_equal(k1, k2), 0);
	ASSERT_INT_EQ(sshkey_equal_public(k1, k2), 1);
	TEST_DONE();

	TEST_START("RSA key hex fingerprint");
	buf = load_text_file("rsa_1.fp");
	cp = sshkey_fingerprint(k1, SSH_DIGEST_SHA256, SSH_FP_BASE64);
	ASSERT_PTR_NE(cp, NULL);
	ASSERT_STRING_EQ(cp, (const char *)sshbuf_ptr(buf));
	sshbuf_free(buf);
	free(cp);
	TEST_DONE();

	TEST_START("RSA cert hex fingerprint");
	buf = load_text_file("rsa_1-cert.fp");
	cp = sshkey_fingerprint(k2, SSH_DIGEST_SHA256, SSH_FP_BASE64);
	ASSERT_PTR_NE(cp, NULL);
	ASSERT_STRING_EQ(cp, (const char *)sshbuf_ptr(buf));
	sshbuf_free(buf);
	free(cp);
	sshkey_free(k2);
	TEST_DONE();

	TEST_START("RSA key bubblebabble fingerprint");
	buf = load_text_file("rsa_1.fp.bb");
	cp = sshkey_fingerprint(k1, SSH_DIGEST_SHA1, SSH_FP_BUBBLEBABBLE);
	ASSERT_PTR_NE(cp, NULL);
	ASSERT_STRING_EQ(cp, (const char *)sshbuf_ptr(buf));
	sshbuf_free(buf);
	free(cp);
	TEST_DONE();

	sshkey_free(k1);

	TEST_START("parse DSA from private");
	buf = load_file("dsa_1");
	ASSERT_INT_EQ(sshkey_parse_private_fileblob(buf, "", &k1, NULL), 0);
	sshbuf_free(buf);
	ASSERT_PTR_NE(k1, NULL);
	a = load_bignum("dsa_1.param.g");
	b = load_bignum("dsa_1.param.priv");
	c = load_bignum("dsa_1.param.pub");
	ASSERT_BIGNUM_EQ(dsa_g(k1), a);
	ASSERT_BIGNUM_EQ(dsa_priv_key(k1), b);
	ASSERT_BIGNUM_EQ(dsa_pub_key(k1), c);
	BN_free(a);
	BN_free(b);
	BN_free(c);
	TEST_DONE();

	TEST_START("parse DSA from private w/ passphrase");
	buf = load_file("dsa_1_pw");
	ASSERT_INT_EQ(sshkey_parse_private_fileblob(buf,
	    (const char *)sshbuf_ptr(pw), &k2, NULL), 0);
	sshbuf_free(buf);
	ASSERT_PTR_NE(k2, NULL);
	ASSERT_INT_EQ(sshkey_equal(k1, k2), 1);
	sshkey_free(k2);
	TEST_DONE();

	TEST_START("parse DSA from new-format");
	buf = load_file("dsa_n");
	ASSERT_INT_EQ(sshkey_parse_private_fileblob(buf, "", &k2, NULL), 0);
	sshbuf_free(buf);
	ASSERT_PTR_NE(k2, NULL);
	ASSERT_INT_EQ(sshkey_equal(k1, k2), 1);
	sshkey_free(k2);
	TEST_DONE();

	TEST_START("parse DSA from new-format w/ passphrase");
	buf = load_file("dsa_n_pw");
	ASSERT_INT_EQ(sshkey_parse_private_fileblob(buf,
	    (const char *)sshbuf_ptr(pw), &k2, NULL), 0);
	sshbuf_free(buf);
	ASSERT_PTR_NE(k2, NULL);
	ASSERT_INT_EQ(sshkey_equal(k1, k2), 1);
	sshkey_free(k2);
	TEST_DONE();

	TEST_START("load DSA from public");
	ASSERT_INT_EQ(sshkey_load_public(test_data_file("dsa_1.pub"), &k2,
	    NULL), 0);
	ASSERT_PTR_NE(k2, NULL);
	ASSERT_INT_EQ(sshkey_equal(k1, k2), 1);
	sshkey_free(k2);
	TEST_DONE();

	TEST_START("load DSA cert");
	ASSERT_INT_EQ(sshkey_load_cert(test_data_file("dsa_1"), &k2), 0);
	ASSERT_PTR_NE(k2, NULL);
	ASSERT_INT_EQ(k2->type, KEY_DSA_CERT);
	ASSERT_INT_EQ(sshkey_equal(k1, k2), 0);
	ASSERT_INT_EQ(sshkey_equal_public(k1, k2), 1);
	TEST_DONE();

	TEST_START("DSA key hex fingerprint");
	buf = load_text_file("dsa_1.fp");
	cp = sshkey_fingerprint(k1, SSH_DIGEST_SHA256, SSH_FP_BASE64);
	ASSERT_PTR_NE(cp, NULL);
	ASSERT_STRING_EQ(cp, (const char *)sshbuf_ptr(buf));
	sshbuf_free(buf);
	free(cp);
	TEST_DONE();

	TEST_START("DSA cert hex fingerprint");
	buf = load_text_file("dsa_1-cert.fp");
	cp = sshkey_fingerprint(k2, SSH_DIGEST_SHA256, SSH_FP_BASE64);
	ASSERT_PTR_NE(cp, NULL);
	ASSERT_STRING_EQ(cp, (const char *)sshbuf_ptr(buf));
	sshbuf_free(buf);
	free(cp);
	sshkey_free(k2);
	TEST_DONE();

	TEST_START("DSA key bubblebabble fingerprint");
	buf = load_text_file("dsa_1.fp.bb");
	cp = sshkey_fingerprint(k1, SSH_DIGEST_SHA1, SSH_FP_BUBBLEBABBLE);
	ASSERT_PTR_NE(cp, NULL);
	ASSERT_STRING_EQ(cp, (const char *)sshbuf_ptr(buf));
	sshbuf_free(buf);
	free(cp);
	TEST_DONE();

	sshkey_free(k1);

#ifdef OPENSSL_HAS_ECC
	TEST_START("parse ECDSA from private");
	buf = load_file("ecdsa_1");
	ASSERT_INT_EQ(sshkey_parse_private_fileblob(buf, "", &k1, NULL), 0);
	sshbuf_free(buf);
	ASSERT_PTR_NE(k1, NULL);
	buf = load_text_file("ecdsa_1.param.curve");
	ASSERT_STRING_EQ((const char *)sshbuf_ptr(buf),
	    OBJ_nid2sn(k1->ecdsa_nid));
	sshbuf_free(buf);
#ifndef OPENSSL_IS_BORINGSSL /* lacks EC_POINT_point2bn() */
	a = load_bignum("ecdsa_1.param.priv");
	b = load_bignum("ecdsa_1.param.pub");
	c = EC_POINT_point2bn(EC_KEY_get0_group(k1->ecdsa),
	    EC_KEY_get0_public_key(k1->ecdsa), POINT_CONVERSION_UNCOMPRESSED,
	    NULL, NULL);
	ASSERT_PTR_NE(c, NULL);
	ASSERT_BIGNUM_EQ(EC_KEY_get0_private_key(k1->ecdsa), a);
	ASSERT_BIGNUM_EQ(b, c);
	BN_free(a);
	BN_free(b);
	BN_free(c);
#endif /* OPENSSL_IS_BORINGSSL */
	TEST_DONE();

	TEST_START("parse ECDSA from private w/ passphrase");
	buf = load_file("ecdsa_1_pw");
	ASSERT_INT_EQ(sshkey_parse_private_fileblob(buf,
	    (const char *)sshbuf_ptr(pw), &k2, NULL), 0);
	sshbuf_free(buf);
	ASSERT_PTR_NE(k2, NULL);
	ASSERT_INT_EQ(sshkey_equal(k1, k2), 1);
	sshkey_free(k2);
	TEST_DONE();

	TEST_START("parse ECDSA from new-format");
	buf = load_file("ecdsa_n");
	ASSERT_INT_EQ(sshkey_parse_private_fileblob(buf, "", &k2, NULL), 0);
	sshbuf_free(buf);
	ASSERT_PTR_NE(k2, NULL);
	ASSERT_INT_EQ(sshkey_equal(k1, k2), 1);
	sshkey_free(k2);
	TEST_DONE();

	TEST_START("parse ECDSA from new-format w/ passphrase");
	buf = load_file("ecdsa_n_pw");
	ASSERT_INT_EQ(sshkey_parse_private_fileblob(buf,
	    (const char *)sshbuf_ptr(pw), &k2, NULL), 0);
	sshbuf_free(buf);
	ASSERT_PTR_NE(k2, NULL);
	ASSERT_INT_EQ(sshkey_equal(k1, k2), 1);
	sshkey_free(k2);
	TEST_DONE();

	TEST_START("load ECDSA from public");
	ASSERT_INT_EQ(sshkey_load_public(test_data_file("ecdsa_1.pub"), &k2,
	    NULL), 0);
	ASSERT_PTR_NE(k2, NULL);
	ASSERT_INT_EQ(sshkey_equal(k1, k2), 1);
	sshkey_free(k2);
	TEST_DONE();

	TEST_START("load ECDSA cert");
	ASSERT_INT_EQ(sshkey_load_cert(test_data_file("ecdsa_1"), &k2), 0);
	ASSERT_PTR_NE(k2, NULL);
	ASSERT_INT_EQ(k2->type, KEY_ECDSA_CERT);
	ASSERT_INT_EQ(sshkey_equal(k1, k2), 0);
	ASSERT_INT_EQ(sshkey_equal_public(k1, k2), 1);
	TEST_DONE();

	TEST_START("ECDSA key hex fingerprint");
	buf = load_text_file("ecdsa_1.fp");
	cp = sshkey_fingerprint(k1, SSH_DIGEST_SHA256, SSH_FP_BASE64);
	ASSERT_PTR_NE(cp, NULL);
	ASSERT_STRING_EQ(cp, (const char *)sshbuf_ptr(buf));
	sshbuf_free(buf);
	free(cp);
	TEST_DONE();

	TEST_START("ECDSA cert hex fingerprint");
	buf = load_text_file("ecdsa_1-cert.fp");
	cp = sshkey_fingerprint(k2, SSH_DIGEST_SHA256, SSH_FP_BASE64);
	ASSERT_PTR_NE(cp, NULL);
	ASSERT_STRING_EQ(cp, (const char *)sshbuf_ptr(buf));
	sshbuf_free(buf);
	free(cp);
	sshkey_free(k2);
	TEST_DONE();

	TEST_START("ECDSA key bubblebabble fingerprint");
	buf = load_text_file("ecdsa_1.fp.bb");
	cp = sshkey_fingerprint(k1, SSH_DIGEST_SHA1, SSH_FP_BUBBLEBABBLE);
	ASSERT_PTR_NE(cp, NULL);
	ASSERT_STRING_EQ(cp, (const char *)sshbuf_ptr(buf));
	sshbuf_free(buf);
	free(cp);
	TEST_DONE();

	sshkey_free(k1);
#endif /* OPENSSL_HAS_ECC */
#endif /* WITH_OPENSSL */

	TEST_START("parse Ed25519 from private");
	buf = load_file("ed25519_1");
	ASSERT_INT_EQ(sshkey_parse_private_fileblob(buf, "", &k1, NULL), 0);
	sshbuf_free(buf);
	ASSERT_PTR_NE(k1, NULL);
	ASSERT_INT_EQ(k1->type, KEY_ED25519);
	/* XXX check key contents */
	TEST_DONE();

	TEST_START("parse Ed25519 from private w/ passphrase");
	buf = load_file("ed25519_1_pw");
	ASSERT_INT_EQ(sshkey_parse_private_fileblob(buf,
	    (const char *)sshbuf_ptr(pw), &k2, NULL), 0);
	sshbuf_free(buf);
	ASSERT_PTR_NE(k2, NULL);
	ASSERT_INT_EQ(sshkey_equal(k1, k2), 1);
	sshkey_free(k2);
	TEST_DONE();

	TEST_START("load Ed25519 from public");
	ASSERT_INT_EQ(sshkey_load_public(test_data_file("ed25519_1.pub"), &k2,
	    NULL), 0);
	ASSERT_PTR_NE(k2, NULL);
	ASSERT_INT_EQ(sshkey_equal(k1, k2), 1);
	sshkey_free(k2);
	TEST_DONE();

	TEST_START("load Ed25519 cert");
	ASSERT_INT_EQ(sshkey_load_cert(test_data_file("ed25519_1"), &k2), 0);
	ASSERT_PTR_NE(k2, NULL);
	ASSERT_INT_EQ(k2->type, KEY_ED25519_CERT);
	ASSERT_INT_EQ(sshkey_equal(k1, k2), 0);
	ASSERT_INT_EQ(sshkey_equal_public(k1, k2), 1);
	TEST_DONE();

	TEST_START("Ed25519 key hex fingerprint");
	buf = load_text_file("ed25519_1.fp");
	cp = sshkey_fingerprint(k1, SSH_DIGEST_SHA256, SSH_FP_BASE64);
	ASSERT_PTR_NE(cp, NULL);
	ASSERT_STRING_EQ(cp, (const char *)sshbuf_ptr(buf));
	sshbuf_free(buf);
	free(cp);
	TEST_DONE();

	TEST_START("Ed25519 cert hex fingerprint");
	buf = load_text_file("ed25519_1-cert.fp");
	cp = sshkey_fingerprint(k2, SSH_DIGEST_SHA256, SSH_FP_BASE64);
	ASSERT_PTR_NE(cp, NULL);
	ASSERT_STRING_EQ(cp, (const char *)sshbuf_ptr(buf));
	sshbuf_free(buf);
	free(cp);
	sshkey_free(k2);
	TEST_DONE();

	TEST_START("Ed25519 key bubblebabble fingerprint");
	buf = load_text_file("ed25519_1.fp.bb");
	cp = sshkey_fingerprint(k1, SSH_DIGEST_SHA1, SSH_FP_BUBBLEBABBLE);
	ASSERT_PTR_NE(cp, NULL);
	ASSERT_STRING_EQ(cp, (const char *)sshbuf_ptr(buf));
	sshbuf_free(buf);
	free(cp);
	TEST_DONE();

	sshkey_free(k1);

#ifdef ENABLE_SK
#if defined(WITH_OPENSSL) && defined(OPENSSL_HAS_ECC)
	TEST_START("parse ECDSA-SK from private");
	buf = load_file("ecdsa_sk1");
	ASSERT_INT_EQ(sshkey_parse_private_fileblob(buf, "", &k1, NULL), 0);
	sshbuf_free(buf);
	ASSERT_PTR_NE(k1, NULL);
	ASSERT_INT_EQ(k1->type, KEY_ECDSA_SK);
	TEST_DONE();

	TEST_START("parse ECDSA-SK from private w/ passphrase");
	buf = load_file("ecdsa_sk1_pw");
	ASSERT_INT_EQ(sshkey_parse_private_fileblob(buf,
	    (const char *)sshbuf_ptr(pw), &k2, NULL), 0);
	sshbuf_free(buf);
	ASSERT_PTR_NE(k2, NULL);
	ASSERT_INT_EQ(sshkey_equal(k1, k2), 1);
	sshkey_free(k2);
	TEST_DONE();

	TEST_START("load ECDSA-SK from public");
	ASSERT_INT_EQ(sshkey_load_public(test_data_file("ecdsa_sk1.pub"), &k2,
	    NULL), 0);
	ASSERT_PTR_NE(k2, NULL);
	ASSERT_INT_EQ(sshkey_equal(k1, k2), 1);
	sshkey_free(k2);
	TEST_DONE();

	TEST_START("load ECDSA-SK cert");
	ASSERT_INT_EQ(sshkey_load_cert(test_data_file("ecdsa_sk1"), &k2), 0);
	ASSERT_PTR_NE(k2, NULL);
	ASSERT_INT_EQ(k2->type, KEY_ECDSA_SK_CERT);
	ASSERT_INT_EQ(sshkey_equal(k1, k2), 0);
	ASSERT_INT_EQ(sshkey_equal_public(k1, k2), 1);
	TEST_DONE();

	TEST_START("ECDSA-SK key hex fingerprint");
	buf = load_text_file("ecdsa_sk1.fp");
	cp = sshkey_fingerprint(k1, SSH_DIGEST_SHA256, SSH_FP_BASE64);
	ASSERT_PTR_NE(cp, NULL);
	ASSERT_STRING_EQ(cp, (const char *)sshbuf_ptr(buf));
	sshbuf_free(buf);
	free(cp);
	TEST_DONE();

	TEST_START("ECDSA-SK cert hex fingerprint");
	buf = load_text_file("ecdsa_sk1-cert.fp");
	cp = sshkey_fingerprint(k2, SSH_DIGEST_SHA256, SSH_FP_BASE64);
	ASSERT_PTR_NE(cp, NULL);
	ASSERT_STRING_EQ(cp, (const char *)sshbuf_ptr(buf));
	sshbuf_free(buf);
	free(cp);
	sshkey_free(k2);
	TEST_DONE();

	TEST_START("ECDSA-SK key bubblebabble fingerprint");
	buf = load_text_file("ecdsa_sk1.fp.bb");
	cp = sshkey_fingerprint(k1, SSH_DIGEST_SHA1, SSH_FP_BUBBLEBABBLE);
	ASSERT_PTR_NE(cp, NULL);
	ASSERT_STRING_EQ(cp, (const char *)sshbuf_ptr(buf));
	sshbuf_free(buf);
	free(cp);
	TEST_DONE();

	sshkey_free(k1);
#endif

	TEST_START("parse Ed25519-SK from private");
	buf = load_file("ed25519_sk1");
	ASSERT_INT_EQ(sshkey_parse_private_fileblob(buf, "", &k1, NULL), 0);
	sshbuf_free(buf);
	ASSERT_PTR_NE(k1, NULL);
	ASSERT_INT_EQ(k1->type, KEY_ED25519_SK);
	/* XXX check key contents */
	TEST_DONE();

	TEST_START("parse Ed25519-SK from private w/ passphrase");
	buf = load_file("ed25519_sk1_pw");
	ASSERT_INT_EQ(sshkey_parse_private_fileblob(buf,
	    (const char *)sshbuf_ptr(pw), &k2, NULL), 0);
	sshbuf_free(buf);
	ASSERT_PTR_NE(k2, NULL);
	ASSERT_INT_EQ(sshkey_equal(k1, k2), 1);
	sshkey_free(k2);
	TEST_DONE();

	TEST_START("load Ed25519-SK from public");
	ASSERT_INT_EQ(sshkey_load_public(test_data_file("ed25519_sk1.pub"),
	    &k2, NULL), 0);
	ASSERT_PTR_NE(k2, NULL);
	ASSERT_INT_EQ(sshkey_equal(k1, k2), 1);
	sshkey_free(k2);
	TEST_DONE();

	TEST_START("load Ed25519-SK cert");
	ASSERT_INT_EQ(sshkey_load_cert(test_data_file("ed25519_sk1"), &k2), 0);
	ASSERT_PTR_NE(k2, NULL);
	ASSERT_INT_EQ(k2->type, KEY_ED25519_SK_CERT);
	ASSERT_INT_EQ(sshkey_equal(k1, k2), 0);
	ASSERT_INT_EQ(sshkey_equal_public(k1, k2), 1);
	TEST_DONE();

	TEST_START("Ed25519-SK key hex fingerprint");
	buf = load_text_file("ed25519_sk1.fp");
	cp = sshkey_fingerprint(k1, SSH_DIGEST_SHA256, SSH_FP_BASE64);
	ASSERT_PTR_NE(cp, NULL);
	ASSERT_STRING_EQ(cp, (const char *)sshbuf_ptr(buf));
	sshbuf_free(buf);
	free(cp);
	TEST_DONE();

	TEST_START("Ed25519-SK cert hex fingerprint");
	buf = load_text_file("ed25519_sk1-cert.fp");
	cp = sshkey_fingerprint(k2, SSH_DIGEST_SHA256, SSH_FP_BASE64);
	ASSERT_PTR_NE(cp, NULL);
	ASSERT_STRING_EQ(cp, (const char *)sshbuf_ptr(buf));
	sshbuf_free(buf);
	free(cp);
	sshkey_free(k2);
	TEST_DONE();

	TEST_START("Ed25519-SK key bubblebabble fingerprint");
	buf = load_text_file("ed25519_sk1.fp.bb");
	cp = sshkey_fingerprint(k1, SSH_DIGEST_SHA1, SSH_FP_BUBBLEBABBLE);
	ASSERT_PTR_NE(cp, NULL);
	ASSERT_STRING_EQ(cp, (const char *)sshbuf_ptr(buf));
	sshbuf_free(buf);
	free(cp);
	TEST_DONE();

	sshkey_free(k1);
#endif /* ENABLE_SK */

	sshbuf_free(pw);

}
