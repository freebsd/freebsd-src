/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
#include "qat_utils.h"

#define AES_128_KEY_LEN_BYTES 16
#define AES_192_KEY_LEN_BYTES 24
#define AES_256_KEY_LEN_BYTES 32

CpaStatus
qatUtilsHashMD5(uint8_t *in, uint8_t *out)
{
	MD5_CTX ctx;

	MD5Init(&ctx);
	MD5Update(&ctx, in, MD5_BLOCK_LENGTH);
	bcopy(&ctx, out, MD5_DIGEST_LENGTH);

	return CPA_STATUS_SUCCESS;
}

CpaStatus
qatUtilsHashSHA1(uint8_t *in, uint8_t *out)
{
	SHA1_CTX ctx;

	SHA1Init(&ctx);
	SHA1Update(&ctx, in, SHA1_BLOCK_LEN);
	bcopy(&ctx, out, SHA1_HASH_LEN);

	return CPA_STATUS_SUCCESS;
}

CpaStatus
qatUtilsHashSHA224(uint8_t *in, uint8_t *out)
{
	SHA224_CTX ctx;

	SHA224_Init(&ctx);
	SHA224_Update(&ctx, in, SHA224_BLOCK_LENGTH);
	bcopy(&ctx, out, SHA256_DIGEST_LENGTH);

	return CPA_STATUS_SUCCESS;
}

CpaStatus
qatUtilsHashSHA256(uint8_t *in, uint8_t *out)
{
	SHA256_CTX ctx;

	SHA256_Init(&ctx);
	SHA256_Update(&ctx, in, SHA256_BLOCK_LENGTH);
	bcopy(&ctx, out, SHA256_DIGEST_LENGTH);

	return CPA_STATUS_SUCCESS;
}

CpaStatus
qatUtilsHashSHA384(uint8_t *in, uint8_t *out)
{
	SHA384_CTX ctx;

	SHA384_Init(&ctx);
	SHA384_Update(&ctx, in, SHA384_BLOCK_LENGTH);
	bcopy(&ctx, out, SHA512_DIGEST_LENGTH);

	return CPA_STATUS_SUCCESS;
}

CpaStatus
qatUtilsHashSHA512(uint8_t *in, uint8_t *out)
{
	SHA512_CTX ctx;

	SHA512_Init(&ctx);
	SHA512_Update(&ctx, in, SHA512_BLOCK_LENGTH);
	bcopy(&ctx, out, SHA512_DIGEST_LENGTH);

	return CPA_STATUS_SUCCESS;
}

CpaStatus
qatUtilsHashMD5Full(uint8_t *in, uint8_t *out, uint32_t len)
{
	MD5_CTX ctx;

	MD5Init(&ctx);
	MD5Update(&ctx, in, len);
	MD5Final(out, &ctx);

	return CPA_STATUS_SUCCESS;
}

CpaStatus
qatUtilsHashSHA1Full(uint8_t *in, uint8_t *out, uint32_t len)
{
	SHA1_CTX ctx;

	SHA1Init(&ctx);
	SHA1Update(&ctx, in, len);
	SHA1Final((caddr_t)out, &ctx);

	return CPA_STATUS_SUCCESS;
}

CpaStatus
qatUtilsHashSHA256Full(uint8_t *in, uint8_t *out, uint32_t len)
{
	SHA256_CTX ctx;

	SHA256_Init(&ctx);
	SHA256_Update(&ctx, in, len);
	SHA256_Final(out, &ctx);

	return CPA_STATUS_SUCCESS;
}

CpaStatus
qatUtilsHashSHA384Full(uint8_t *in, uint8_t *out, uint32_t len)
{
	SHA384_CTX ctx;

	SHA384_Init(&ctx);
	SHA384_Update(&ctx, in, len);
	SHA384_Final(out, &ctx);

	return CPA_STATUS_SUCCESS;
}

CpaStatus
qatUtilsHashSHA512Full(uint8_t *in, uint8_t *out, uint32_t len)
{
	SHA512_CTX ctx;

	SHA512_Init(&ctx);
	SHA512_Update(&ctx, in, len);
	SHA512_Final(out, &ctx);

	return CPA_STATUS_SUCCESS;
}

#define BYTE_TO_BITS_SHIFT 3

CpaStatus
qatUtilsAESEncrypt(uint8_t *key,
		   uint32_t keyLenInBytes,
		   uint8_t *in,
		   uint8_t *out)
{
	rijndael_ctx ctx;

	rijndael_set_key(&ctx, key, keyLenInBytes << BYTE_TO_BITS_SHIFT);
	rijndael_encrypt(&ctx, in, out);

	return CPA_STATUS_SUCCESS;
}

CpaStatus
qatUtilsAESKeyExpansionForward(uint8_t *key,
			       uint32_t keyLenInBytes,
			       uint32_t *out)
{
	rijndael_ctx ctx;
	uint32_t i = 0, j = 0;
	uint32_t lw_per_round = 4;
	int32_t lw_left_to_copy = keyLenInBytes / lw_per_round;
	uint32_t *key_pointer = NULL;

	/* Error check for wrong input key len */
	if (AES_128_KEY_LEN_BYTES != keyLenInBytes &&
	    AES_192_KEY_LEN_BYTES != keyLenInBytes &&
	    AES_256_KEY_LEN_BYTES != keyLenInBytes) {
		return CPA_STATUS_INVALID_PARAM;
	}

	rijndael_set_key(&ctx, key, keyLenInBytes << BYTE_TO_BITS_SHIFT);

	/* Pointer to the last round of expanded key. */
	key_pointer = &ctx.ek[lw_per_round * ctx.Nr];

	while (lw_left_to_copy > 0) {
		for (i = 0; i < MIN(lw_left_to_copy, lw_per_round); i++, j++) {
			out[j] = __builtin_bswap32(key_pointer[i]);
		}

		lw_left_to_copy -= lw_per_round;
		key_pointer -= lw_left_to_copy;
	}

	return CPA_STATUS_SUCCESS;
}
