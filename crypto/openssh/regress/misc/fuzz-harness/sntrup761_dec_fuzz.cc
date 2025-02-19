// Basic fuzz test for depcapsulate operation,

#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <err.h>

extern "C" {

#include "crypto_api.h"
#include "hash.c"

#undef randombytes
#define USE_SNTRUP761X25519 1
#ifdef SNTRUP761_NO_ASM
# undef __GNUC__
#endif
void randombytes(unsigned char *ptr, size_t l);
volatile crypto_int16 crypto_int16_optblocker = 0;
volatile crypto_int32 crypto_int32_optblocker = 0;
volatile crypto_int64 crypto_int64_optblocker = 0;
#include "sntrup761.c"

static int real_random;

void
randombytes(unsigned char *ptr, size_t l)
{
	if (real_random)
		arc4random_buf(ptr, l);
	else
		memset(ptr, 0, l);
}

void privkeys(unsigned char *zero_sk, unsigned char *rnd_sk)
{
	unsigned char pk[crypto_kem_sntrup761_PUBLICKEYBYTES];

	real_random = 0;
	if (crypto_kem_sntrup761_keypair(pk, zero_sk) != 0)
		errx(1, "crypto_kem_sntrup761_keypair failed");
	real_random = 1;
	if (crypto_kem_sntrup761_keypair(pk, rnd_sk) != 0)
		errx(1, "crypto_kem_sntrup761_keypair failed");
}

int LLVMFuzzerTestOneInput(const uint8_t* input, size_t len)
{
	static bool once;
	static unsigned char zero_sk[crypto_kem_sntrup761_SECRETKEYBYTES];
	static unsigned char rnd_sk[crypto_kem_sntrup761_SECRETKEYBYTES];
	unsigned char ciphertext[crypto_kem_sntrup761_CIPHERTEXTBYTES];
	unsigned char secret[crypto_kem_sntrup761_BYTES];

	if (!once) {
		privkeys(zero_sk, rnd_sk);
		once = true;
	}

	memset(&ciphertext, 0, sizeof(ciphertext));
	if (len > sizeof(ciphertext)) {
		len = sizeof(ciphertext);
	}
	memcpy(ciphertext, input, len);

	(void)crypto_kem_sntrup761_dec(secret, ciphertext, zero_sk);
	(void)crypto_kem_sntrup761_dec(secret, ciphertext, rnd_sk);
	return 0;
}

} // extern
