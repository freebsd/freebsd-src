/*
 * SAE-PK password/modifier generator
 * Copyright (c) 2020, The Linux Foundation
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "utils/base64.h"
#include "crypto/crypto.h"
#include "common/sae.h"


int main(int argc, char *argv[])
{
	char *der = NULL;
	size_t der_len;
	struct crypto_ec_key *key = NULL;
	struct wpabuf *pub = NULL;
	u8 *data = NULL, *m;
	size_t data_len;
	char *b64 = NULL, *pw = NULL, *pos, *src;
	int sec, j;
	int ret = -1;
	u8 hash[SAE_MAX_HASH_LEN];
	char hash_hex[2 * SAE_MAX_HASH_LEN + 1];
	u8 pw_base_bin[SAE_MAX_HASH_LEN];
	u8 *dst;
	int group;
	size_t hash_len;
	unsigned long long i, expected;
	char m_hex[2 * SAE_PK_M_LEN + 1];
	u32 sec_1b, val20;

	wpa_debug_level = MSG_INFO;
	if (os_program_init() < 0)
		goto fail;

	if (argc != 4) {
		fprintf(stderr,
			"usage: sae_pk_gen <DER ECPrivateKey file> <Sec:3|5> <SSID>\n");
		goto fail;
	}

	sec = atoi(argv[2]);
	if (sec != 3 && sec != 5) {
		fprintf(stderr,
			"Invalid Sec value (allowed values: 3 and 5)\n");
		goto fail;
	}
	sec_1b = sec == 3;
	expected = 1;
	for (j = 0; j < sec; j++)
		expected *= 256;

	der = os_readfile(argv[1], &der_len);
	if (!der) {
		fprintf(stderr, "Could not read %s: %s\n",
			argv[1], strerror(errno));
		goto fail;
	}

	key = crypto_ec_key_parse_priv((u8 *) der, der_len);
	if (!key) {
		fprintf(stderr, "Could not parse ECPrivateKey\n");
		goto fail;
	}

	pub = crypto_ec_key_get_subject_public_key(key);
	if (!pub) {
		fprintf(stderr, "Failed to build SubjectPublicKey\n");
		goto fail;
	}

	group = crypto_ec_key_group(key);
	switch (group) {
	case 19:
		hash_len = 32;
		break;
	case 20:
		hash_len = 48;
		break;
	case 21:
		hash_len = 64;
		break;
	default:
		fprintf(stderr, "Unsupported private key group\n");
		goto fail;
	}

	data_len = os_strlen(argv[3]) + SAE_PK_M_LEN + wpabuf_len(pub);
	data = os_malloc(data_len);
	if (!data) {
		fprintf(stderr, "No memory for data buffer\n");
		goto fail;
	}
	os_memcpy(data, argv[3], os_strlen(argv[3]));
	m = data + os_strlen(argv[3]);
	if (os_get_random(m, SAE_PK_M_LEN) < 0) {
		fprintf(stderr, "Could not generate random Modifier M\n");
		goto fail;
	}
	os_memcpy(m + SAE_PK_M_LEN, wpabuf_head(pub), wpabuf_len(pub));

	fprintf(stderr, "Searching for a suitable Modifier M value\n");
	for (i = 0;; i++) {
		if (sae_hash(hash_len, data, data_len, hash) < 0) {
			fprintf(stderr, "Hash failed\n");
			goto fail;
		}
		if (hash[0] == 0 && hash[1] == 0) {
			if ((hash[2] & 0xf0) == 0)
				fprintf(stderr, "\r%3.2f%%",
					100.0 * (double) i / (double) expected);
			for (j = 2; j < sec; j++) {
				if (hash[j])
					break;
			}
			if (j == sec)
				break;
		}
		inc_byte_array(m, SAE_PK_M_LEN);
	}

	if (wpa_snprintf_hex(m_hex, sizeof(m_hex), m, SAE_PK_M_LEN) < 0 ||
	    wpa_snprintf_hex(hash_hex, sizeof(hash_hex), hash, hash_len) < 0)
		goto fail;
	fprintf(stderr, "\nFound a valid hash in %llu iterations: %s\n",
		i + 1, hash_hex);

	b64 = base64_encode(der, der_len, NULL);
	if (!b64)
		goto fail;
	src = pos = b64;
	while (*src) {
		if (*src != '\n')
			*pos++ = *src;
		src++;
	}
	*pos = '\0';

	/* Skip 8*Sec bits and add Sec_1b as the every 20th bit starting with
	 * one. */
	os_memset(pw_base_bin, 0, sizeof(pw_base_bin));
	dst = pw_base_bin;
	for (j = 0; j < 8 * (int) hash_len / 20; j++) {
		val20 = sae_pk_get_be19(hash + sec);
		val20 |= sec_1b << 19;
		sae_pk_buf_shift_left_19(hash + sec, hash_len - sec);

		if (j & 1) {
			*dst |= (val20 >> 16) & 0x0f;
			dst++;
			*dst++ = (val20 >> 8) & 0xff;
			*dst++ = val20 & 0xff;
		} else {
			*dst++ = (val20 >> 12) & 0xff;
			*dst++ = (val20 >> 4) & 0xff;
			*dst = (val20 << 4) & 0xf0;
		}
	}
	if (wpa_snprintf_hex(hash_hex, sizeof(hash_hex),
			     pw_base_bin, hash_len - sec) >= 0)
		fprintf(stderr, "PasswordBase binary data for base32: %s",
			hash_hex);

	pw = sae_pk_base32_encode(pw_base_bin, 20 * 3 - 5);
	if (!pw)
		goto fail;

	printf("# SAE-PK password/M/private key for Sec=%d.\n", sec);
	printf("sae_password=%s|pk=%s:%s\n", pw, m_hex, b64);
	printf("# Longer passwords can be used for improved security at the cost of usability:\n");
	for (j = 4; j <= ((int) hash_len * 8 + 5 - 8 * sec) / 19; j++) {
		os_free(pw);
		pw = sae_pk_base32_encode(pw_base_bin, 20 * j - 5);
		if (pw)
			printf("# %s\n", pw);
	}

	ret = 0;
fail:
	os_free(der);
	wpabuf_free(pub);
	crypto_ec_key_deinit(key);
	os_free(data);
	os_free(b64);
	os_free(pw);

	os_program_deinit();

	return ret;
}
