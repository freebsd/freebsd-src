/*
 * SHA256-based PRF (IEEE 802.11r)
 * Copyright (c) 2003-2007, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"

#include "common.h"
#include "sha256.h"
#include "crypto.h"


/**
 * sha256_prf - SHA256-based Pseudo-Random Function (IEEE 802.11r, 8.5.1.5.2)
 * @key: Key for PRF
 * @key_len: Length of the key in bytes
 * @label: A unique label for each purpose of the PRF
 * @data: Extra data to bind into the key
 * @data_len: Length of the data
 * @buf: Buffer for the generated pseudo-random key
 * @buf_len: Number of bytes of key to generate
 *
 * This function is used to derive new, cryptographically separate keys from a
 * given key.
 */
void sha256_prf(const u8 *key, size_t key_len, const char *label,
		const u8 *data, size_t data_len, u8 *buf, size_t buf_len)
{
	u16 counter = 1;
	size_t pos, plen;
	u8 hash[SHA256_MAC_LEN];
	const u8 *addr[4];
	size_t len[4];
	u8 counter_le[2], length_le[2];

	addr[0] = counter_le;
	len[0] = 2;
	addr[1] = (u8 *) label;
	len[1] = os_strlen(label);
	addr[2] = data;
	len[2] = data_len;
	addr[3] = length_le;
	len[3] = sizeof(length_le);

	WPA_PUT_LE16(length_le, buf_len * 8);
	pos = 0;
	while (pos < buf_len) {
		plen = buf_len - pos;
		WPA_PUT_LE16(counter_le, counter);
		if (plen >= SHA256_MAC_LEN) {
			hmac_sha256_vector(key, key_len, 4, addr, len,
					   &buf[pos]);
			pos += SHA256_MAC_LEN;
		} else {
			hmac_sha256_vector(key, key_len, 4, addr, len, hash);
			os_memcpy(&buf[pos], hash, plen);
			break;
		}
		counter++;
	}
}
