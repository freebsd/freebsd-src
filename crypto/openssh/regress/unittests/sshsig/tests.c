/* 	$OpenBSD: tests.c,v 1.4 2024/01/11 01:45:59 djm Exp $ */
/*
 * Regress test for sshbuf.h buffer API
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
#include <openssl/evp.h>
#include <openssl/crypto.h>
#endif

#include "ssherr.h"
#include "authfile.h"
#include "sshkey.h"
#include "sshbuf.h"
#include "sshsig.h"
#include "log.h"

#include "../test_helper/test_helper.h"

static struct sshbuf *
load_file(const char *name)
{
	struct sshbuf *ret = NULL;

	ASSERT_INT_EQ(sshbuf_load_file(test_data_file(name), &ret), 0);
	ASSERT_PTR_NE(ret, NULL);
	return ret;
}

static struct sshkey *
load_key(const char *name)
{
	struct sshkey *ret = NULL;
	ASSERT_INT_EQ(sshkey_load_public(test_data_file(name), &ret, NULL), 0);
	ASSERT_PTR_NE(ret, NULL);
	return ret;
}

static void
check_sig(const char *keyname, const char *signame, const struct sshbuf *msg,
    const char *namespace)
{
	struct sshkey *k, *sign_key;
	struct sshbuf *sig, *rawsig;
	struct sshkey_sig_details *sig_details;

	k = load_key(keyname);
	sig = load_file(signame);
	sign_key = NULL;
	sig_details = NULL;
	rawsig = NULL;
	ASSERT_INT_EQ(sshsig_dearmor(sig, &rawsig), 0);
	ASSERT_INT_EQ(sshsig_verifyb(rawsig, msg, namespace,
	    &sign_key, &sig_details), 0);
	ASSERT_INT_EQ(sshkey_equal(k, sign_key), 1);
	sshkey_free(k);
	sshkey_free(sign_key);
	sshkey_sig_details_free(sig_details);
	sshbuf_free(sig);
	sshbuf_free(rawsig);
}

void
tests(void)
{
	struct sshbuf *msg;
	char *namespace;

#if 0
        log_init("test_sshsig", SYSLOG_LEVEL_DEBUG3, SYSLOG_FACILITY_AUTH, 1);
#endif

#ifdef WITH_OPENSSL
	OpenSSL_add_all_algorithms();
	ERR_load_crypto_strings();
#endif

	TEST_START("load data");
	msg = load_file("namespace");
	namespace = sshbuf_dup_string(msg);
	ASSERT_PTR_NE(namespace, NULL);
	sshbuf_free(msg);
	msg = load_file("signed-data");
	TEST_DONE();

#ifdef WITH_OPENSSL
	TEST_START("check RSA signature");
	check_sig("rsa.pub", "rsa.sig", msg, namespace);
	TEST_DONE();

#ifdef WITH_DSA
	TEST_START("check DSA signature");
	check_sig("dsa.pub", "dsa.sig", msg, namespace);
	TEST_DONE();
#endif

#ifdef OPENSSL_HAS_ECC
	TEST_START("check ECDSA signature");
	check_sig("ecdsa.pub", "ecdsa.sig", msg, namespace);
	TEST_DONE();
#endif
#endif

	TEST_START("check ED25519 signature");
	check_sig("ed25519.pub", "ed25519.sig", msg, namespace);
	TEST_DONE();

#ifdef ENABLE_SK
#if defined(WITH_OPENSSL) && defined(OPENSSL_HAS_ECC)
	TEST_START("check ECDSA-SK signature");
	check_sig("ecdsa_sk.pub", "ecdsa_sk.sig", msg, namespace);
	TEST_DONE();
#endif

	TEST_START("check ED25519-SK signature");
	check_sig("ed25519_sk.pub", "ed25519_sk.sig", msg, namespace);
	TEST_DONE();

#if defined(WITH_OPENSSL) && defined(OPENSSL_HAS_ECC)
	TEST_START("check ECDSA-SK webauthn signature");
	check_sig("ecdsa_sk_webauthn.pub", "ecdsa_sk_webauthn.sig",
	    msg, namespace);
 	TEST_DONE();
#endif
#endif /* ENABLE_SK */

	sshbuf_free(msg);
	free(namespace);
}
