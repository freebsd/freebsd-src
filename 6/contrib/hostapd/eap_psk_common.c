/*
 * WPA Supplicant / EAP-PSK shared routines
 * Copyright (c) 2004-2005, Jouni Malinen <jkmaline@cc.hut.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 * See README and COPYING for more details.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "aes_wrap.h"
#include "eap_psk_common.h"

#define aes_block_size 16


void eap_psk_key_setup(const u8 *psk, u8 *ak, u8 *kdk)
{
	memset(ak, 0, aes_block_size);
	aes_128_encrypt_block(psk, ak, ak);
	memcpy(kdk, ak, aes_block_size);
	ak[aes_block_size - 1] ^= 0x01;
	kdk[aes_block_size - 1] ^= 0x02;
	aes_128_encrypt_block(psk, ak, ak);
	aes_128_encrypt_block(psk, kdk, kdk);
}


void eap_psk_derive_keys(const u8 *kdk, const u8 *rand_p, u8 *tek, u8 *msk)
{
	u8 hash[aes_block_size];
	u8 counter = 1;
	int i;

	aes_128_encrypt_block(kdk, rand_p, hash);

	hash[aes_block_size - 1] ^= counter;
	aes_128_encrypt_block(kdk, hash, tek);
	hash[aes_block_size - 1] ^= counter;
	counter++;

	for (i = 0; i < EAP_PSK_MSK_LEN / aes_block_size; i++) {
		hash[aes_block_size - 1] ^= counter;
		aes_128_encrypt_block(kdk, hash, &msk[i * aes_block_size]);
		hash[aes_block_size - 1] ^= counter;
		counter++;
	}
}
