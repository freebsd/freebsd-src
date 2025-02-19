// Basic fuzz test for encapsulate operation.

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

int LLVMFuzzerTestOneInput(const uint8_t* input, size_t len)
{
	unsigned char pk[crypto_kem_sntrup761_PUBLICKEYBYTES];
	unsigned char ciphertext[crypto_kem_sntrup761_CIPHERTEXTBYTES];
	unsigned char secret[crypto_kem_sntrup761_BYTES];

	memset(&pk, 0, sizeof(pk));
	if (len > sizeof(pk)) {
		len = sizeof(pk);
	}
	memcpy(pk, input, len);

	real_random = 0;
	(void)crypto_kem_sntrup761_enc(ciphertext, secret, pk);
	real_random = 1;
	(void)crypto_kem_sntrup761_enc(ciphertext, secret, pk);
	return 0;
}

} // extern
