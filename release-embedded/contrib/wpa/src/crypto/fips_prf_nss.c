/*
 * FIPS 186-2 PRF for NSS
 * Copyright (c) 2009, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"
#include <openssl/sha.h>

#include "common.h"
#include "crypto.h"


int fips186_2_prf(const u8 *seed, size_t seed_len, u8 *x, size_t xlen)
{
	return -1;
}
