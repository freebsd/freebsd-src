/* 	$OpenBSD: test_mldsa.c,v 1.4 2026/06/22 12:28:48 dtucker Exp $ */
/*
 * Regress test for ML-DSA
 *
 * Placed in the public domain
 */

#include "includes.h"

#ifdef USE_MLDSA

#include <sys/types.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../test_helper/test_helper.h"
#include "crypto_api.h"
#include "xmalloc.h"
#include "sshbuf.h"

/* in tests.c */
struct sshbuf *load_text_file(const char *name);
char *get_json_string(struct sshbuf *content, const char *key, int consume);

void mldsa_tests(void);

struct kat {
	char *seed;
	char *pk_hash;
	char *sk_hash;
	char *msg;
	char *rand;
	char *sig_hash;
};

static void
load_kats(const char *file, struct kat ***katsp, size_t *nkatsp)
{
	struct sshbuf *json_buf;
	struct kat *kat = NULL, **kats = NULL;
	size_t nkats = 0;

	json_buf = load_text_file(file);
	while (sshbuf_find(json_buf, 0, "key_generation_seed",
	    strlen("key_generation_seed"), NULL) == 0) {
		kat = xcalloc(1, sizeof(*kat));
		kat->seed = get_json_string(json_buf, "key_generation_seed", 1);
		kat->pk_hash = get_json_string(json_buf,
		    "sha3_256_hash_of_verification_key", 1);
		kat->sk_hash = get_json_string(json_buf,
		    "sha3_256_hash_of_signing_key", 1);
		kat->msg = get_json_string(json_buf, "message", 1);
		kat->rand = get_json_string(json_buf, "signing_randomness", 1);
		kat->sig_hash = get_json_string(json_buf,
		    "sha3_256_hash_of_signature", 1);
		kats = xrecallocarray(kats, nkats, nkats + 1, sizeof(*kats));
		kats[nkats++] = kat;
	}
	*katsp = kats;
	*nkatsp = nkats;
	sshbuf_free(json_buf);
}

static void
free_kats(struct kat **kats, size_t nkats)
{
	size_t i;
	struct kat *kat;

	for (i = 0; i < nkats; i++) {
		kat = kats[i];
		free(kat->seed);
		free(kat->pk_hash);
		free(kat->sk_hash);
		free(kat->msg);
		free(kat->rand);
		free(kat->sig_hash);
		free(kat);
	}
	free(kats);
}

void
mldsa_tests(void)
{
	uint8_t pk[MLDSA44_PUBLICKEYBYTES];
	uint8_t sk[MLDSA44_SECRETKEYBYTES];
	uint8_t sig[MLDSA44_SIGBYTES];
	uint8_t pk_hash[32], sk_hash[32], sig_hash[32];
	uint8_t expected_pk_hash[32], expected_sk_hash[32];
	uint8_t expected_sig_hash[32];
	uint8_t seed[32], rand[32];
	uint8_t *msg;
	size_t nkats, i, msglen;
	struct kat **kats, *kat;

	TEST_START("ML-DSA 44 KATs");
	load_kats("nistkats-44.json", &kats, &nkats);
	for (i = 0; i < nkats; i++) {
		kat = kats[i];
		test_subtest_info("vector %zu", i);

		hex2bin(seed, kat->seed, 32);
		hex2bin(expected_pk_hash, kat->pk_hash, 32);
		hex2bin(expected_sk_hash, kat->sk_hash, 32);

		/* Keypair generation */
		ASSERT_INT_EQ(crypto_sign_mldsa44_keypair_seeded(pk,
		    sk, seed), 0);

		sha3_256(pk_hash, pk, sizeof(pk));
		sha3_256(sk_hash, sk, sizeof(sk));

		ASSERT_MEM_EQ(pk_hash, expected_pk_hash, 32);
		ASSERT_MEM_EQ(sk_hash, expected_sk_hash, 32);

		msglen = strlen(kat->msg) / 2;
		ASSERT_PTR_NE(msg = malloc(msglen), NULL);
		hex2bin(msg, kat->msg, msglen);
		hex2bin(rand, kat->rand, 32);
		hex2bin(expected_sig_hash, kat->sig_hash, 32);

		/* Signing */
		ASSERT_INT_EQ(crypto_sign_mldsa44_seeded(sig, msg,
		    msglen, NULL, 0, sk, rand), 0);

		sha3_256(sig_hash, sig, sizeof(sig));
		ASSERT_MEM_EQ(sig_hash, expected_sig_hash, 32);

		/* Verification */
		ASSERT_INT_EQ(crypto_sign_mldsa44_verify(sig, msg,
		    msglen, NULL, 0, pk), 0);
		free(msg);
	}
	free_kats(kats, nkats);
	TEST_DONE();
}
#endif /* USE_MLDSA */
