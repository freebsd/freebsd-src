/* 	$OpenBSD: test_mlkem.c,v 1.2 2026/06/16 22:27:10 dtucker Exp $ */
/*
 * Regress test for ML-KEM
 *
 * Placed in the public domain
 */

#include "includes.h"

#ifdef USE_MLKEM768X25519

#include <sys/types.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../test_helper/test_helper.h"
#include "crypto_api.h"

struct mlkem768_kat {
	const char *seed;
	const char *pk_hash;
	const char *sk_hash;
	const char *enc_seed;
	const char *ct_hash;
	const char *shared_secret;
};

static const struct mlkem768_kat mlkem768_kats[] = {
	{
		"7c9935a0b07694aa0c6d10e4db6b1add2fd81a25"
		"ccb148032dcd739936737f2d"
		"8626ed79d451140800e03b59b956f8210e556067"
		"407d13dc90fa9e8b872bfb8f",
		"f57262661358cde8d3ebf990e5fd1d5b896c992c"
		"cfaadb5256b68bbf5943b132",
		"7deef44965b03d76de543ad6ef9e74a2772fa5a9"
		"fa0e761120dac767cf0152ef",
		"147c03f7a5bebba406c8fae1874d7f13c80efe79"
		"a3a9a874cc09fe76f6997615",
		"6e777e2cf8054659136a971d9e70252f30122693"
		"0c19c470ee0688163a63c15b",
		"e7184a0975ee3470878d2d159ec83129c8aec253"
		"d4ee17b4810311d198cd0368"
	},
	{
		"d60b93492a1d8c1c7ba6fc0b733137f3406cee81"
		"10a93f170e7a78658af326d9"
		"003271531cf27285b8721ed5cb46853043b346a6"
		"6cba6cf765f1b0eaa40bf672",
		"7b00751eb9b1253231213f8a14f06f0fe1b7a4fd"
		"b7d1cfe44c161e577e5e8f0a",
		"3a8c009e8e648ac572d5592e4a92907fae0c1767"
		"be41c544b59dc3ffe61f7ded",
		"cde797df8ce67231f6c5d15811843e01eb2ab84c"
		"7490931240822adbddd72046",
		"bce1bf3450f574130b9561ee11565fa41d599d05"
		"d2136f10ad2c013eb5d13ca9",
		"5f0c5d9f39d3e724b5a2bd54e69e360f72ffab5d"
		"4d6cc5e572fecba80acd4796"
	},
	{
		"4b622de1350119c45a9f2e2ef3dc5df50a759d13"
		"8cdfbd64c81cc7cc2f513345"
		"e82fcc97ca60ccb27bf6938c975658aeb8b4d37c"
		"ffbde25d97e561f36c219ade",
		"9bda55b63cffa9bf953993918b18cd6595ea6433"
		"b479e89b5cd3c9339e4468cb",
		"d5fc96564df6e53622b2db8295a80a44e3bad714"
		"7696e2ad1f728639c98791b1",
		"f43f68fbd694f0a6d307297110ecd4739876489f"
		"df07eb9b03364e2ed0ff96e9",
		"2a8b5d9bdcac1b7ffb6d655368e15148308eee98"
		"ae34f4105eaae87f24a008a2",
		"7f3bcc03a35a0030255264914e5d88a0c93611c7"
		"ca21f0609678a88ca42ce1c9"
	},
	{
		"050d58f9f757edc1e8180e3808b806f5bbb3586d"
		"b3470b069826d1bb9a4efc2c"
		"de950541fd53a8a47aaa8cdfe80d928262a5ef7f"
		"8129ec3ef92f78d7cc32ef60",
		"647a81f0f1b3e3dacb6e73e900f7c078cdfaa711"
		"9a5ede48c7685fdb7e0fe2f5",
		"1cf686cb8732c73a38b35d73b0b28fb120bc89cd"
		"a1554d9f12adedc057862081",
		"ea74fbc3c546500ed684bed6fe3c496d3b86d2d6"
		"dfaf223969b942e9a8c95e85",
		"1c51c85ce66d80c1f9bb138e5bce84dd75cee426"
		"0c8817e06c6f2bd920601530",
		"c630736985fdb7830d7446e18b6b81fa4a707a60"
		"58964b99190120de85e7559c"
	},
	{
		"66b79b844e0c2adad694e0478661ac46fe6b6001"
		"f6a71ff8e2f034b1fd8833d3"
		"be2d3c64d38269a1ee8660b9a2beaeb9f5ac022e"
		"8f0a357feebfd13b06813854",
		"811aea11a24a4b09e428415f82ee836e930c3b77"
		"867aafc5e6728149e3f2bd1b",
		"6a1ff1351c538a5661fc3576c29408c19f42da36"
		"88fa16f9ec5ead6a84420db5",
		"64efa87a12cb96f98b9b81a7e5128a959c74e533"
		"2aaab0444fca7b4a5e5e0216",
		"db4dedc1e4d383acaf974fb50ffbf881bd3938ad"
		"196fb9aebeeb1bf1ddc94e10",
		"41e078d0d0c4fe5df5c6683171d5c1c3f1ef152c"
		"4945f9cb299f74278ce4cc4f"
	}
};

void mlkem_tests(void);

void
mlkem_tests(void)
{
	uint8_t pk[MLKEM768_PUBLICKEYBYTES];
	uint8_t sk[MLKEM768_SECRETKEYBYTES];
	uint8_t ct[MLKEM768_CIPHERTEXTBYTES];
	uint8_t shared_secret[MLKEM768_BYTES], shared_secret2[MLKEM768_BYTES];
	uint8_t pk_hash[32], sk_hash[32], ct_hash[32];
	uint8_t expected_pk_hash[32], expected_sk_hash[32];
	uint8_t expected_ct_hash[32], expected_shared_secret[32];
	uint8_t seed[64], enc_seed[32];
	size_t i;

	TEST_START("ML-KEM 768 KATs");
	for (i = 0; i < sizeof(mlkem768_kats) / sizeof(mlkem768_kats[0]); i++) {
		test_subtest_info("vector %zu", i);

		hex2bin(seed, mlkem768_kats[i].seed, 64);
		hex2bin(expected_pk_hash, mlkem768_kats[i].pk_hash, 32);
		hex2bin(expected_sk_hash, mlkem768_kats[i].sk_hash, 32);

		/* Keypair generation */
		ASSERT_INT_EQ(crypto_kem_mlkem768_keypair_seeded(pk, sk, seed), 0);

		sha3_256(pk_hash, pk, sizeof(pk));
		sha3_256(sk_hash, sk, sizeof(sk));

		ASSERT_MEM_EQ(pk_hash, expected_pk_hash, 32);
		ASSERT_MEM_EQ(sk_hash, expected_sk_hash, 32);

		hex2bin(enc_seed, mlkem768_kats[i].enc_seed, 32);
		hex2bin(expected_ct_hash, mlkem768_kats[i].ct_hash, 32);
		hex2bin(expected_shared_secret,
		    mlkem768_kats[i].shared_secret, 32);

		/* Encapsulation */
		ASSERT_INT_EQ(crypto_kem_mlkem768_enc_seeded(ct, shared_secret,
		    pk, enc_seed), 0);

		sha3_256(ct_hash, ct, sizeof(ct));
		ASSERT_MEM_EQ(ct_hash, expected_ct_hash, 32);
		ASSERT_MEM_EQ(shared_secret, expected_shared_secret, 32);

		/* Decapsulation */
		ASSERT_INT_EQ(crypto_kem_mlkem768_dec(shared_secret2, ct, sk), 0);
		ASSERT_MEM_EQ(shared_secret, shared_secret2, 32);
	}
	TEST_DONE();
}
#endif /* USE_MLKEM768X25519 */
