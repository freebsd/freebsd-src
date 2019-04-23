/*
 * common module tests
 * Copyright (c) 2014-2019, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "utils/module_tests.h"
#include "crypto/crypto.h"
#include "ieee802_11_common.h"
#include "ieee802_11_defs.h"
#include "gas.h"
#include "wpa_common.h"
#include "sae.h"


struct ieee802_11_parse_test_data {
	u8 *data;
	size_t len;
	ParseRes result;
	int count;
};

static const struct ieee802_11_parse_test_data parse_tests[] = {
	{ (u8 *) "", 0, ParseOK, 0 },
	{ (u8 *) " ", 1, ParseFailed, 0 },
	{ (u8 *) "\xff\x00", 2, ParseUnknown, 1 },
	{ (u8 *) "\xff\x01", 2, ParseFailed, 0 },
	{ (u8 *) "\xdd\x03\x01\x02\x03", 5, ParseUnknown, 1 },
	{ (u8 *) "\xdd\x04\x01\x02\x03\x04", 6, ParseUnknown, 1 },
	{ (u8 *) "\xdd\x04\x00\x50\xf2\x02", 6, ParseUnknown, 1 },
	{ (u8 *) "\xdd\x05\x00\x50\xf2\x02\x02", 7, ParseOK, 1 },
	{ (u8 *) "\xdd\x05\x00\x50\xf2\x02\xff", 7, ParseUnknown, 1 },
	{ (u8 *) "\xdd\x04\x00\x50\xf2\xff", 6, ParseUnknown, 1 },
	{ (u8 *) "\xdd\x04\x50\x6f\x9a\xff", 6, ParseUnknown, 1 },
	{ (u8 *) "\xdd\x04\x00\x90\x4c\x33", 6, ParseOK, 1 },
	{ (u8 *) "\xdd\x04\x00\x90\x4c\xff\xdd\x04\x00\x90\x4c\x33", 12,
	  ParseUnknown, 2 },
	{ (u8 *) "\x10\x01\x00\x21\x00", 5, ParseOK, 2 },
	{ (u8 *) "\x24\x00", 2, ParseOK, 1 },
	{ (u8 *) "\x38\x00", 2, ParseOK, 1 },
	{ (u8 *) "\x54\x00", 2, ParseOK, 1 },
	{ (u8 *) "\x5a\x00", 2, ParseOK, 1 },
	{ (u8 *) "\x65\x00", 2, ParseOK, 1 },
	{ (u8 *) "\x65\x12\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f\x10\x11",
	  20, ParseOK, 1 },
	{ (u8 *) "\x6e\x00", 2, ParseOK, 1 },
	{ (u8 *) "\xc7\x00", 2, ParseOK, 1 },
	{ (u8 *) "\xc7\x01\x00", 3, ParseOK, 1 },
	{ (u8 *) "\x03\x00\x2a\x00\x36\x00\x37\x00\x38\x00\x2d\x00\x3d\x00\xbf\x00\xc0\x00",
	  18, ParseOK, 9 },
	{ (u8 *) "\x8b\x00", 2, ParseOK, 1 },
	{ (u8 *) "\xdd\x04\x00\x90\x4c\x04", 6, ParseUnknown, 1 },
	{ (u8 *) "\xed\x00", 2, ParseOK, 1 },
	{ (u8 *) "\xef\x00", 2, ParseOK, 1 },
	{ (u8 *) "\xef\x01\x11", 3, ParseOK, 1 },
	{ (u8 *) "\xf0\x00", 2, ParseOK, 1 },
	{ (u8 *) "\xf1\x00", 2, ParseOK, 1 },
	{ (u8 *) "\xf1\x02\x11\x22", 4, ParseOK, 1 },
	{ (u8 *) "\xf2\x00", 2, ParseOK, 1 },
	{ (u8 *) "\xff\x00", 2, ParseUnknown, 1 },
	{ (u8 *) "\xff\x01\x00", 3, ParseUnknown, 1 },
	{ (u8 *) "\xff\x01\x01", 3, ParseOK, 1 },
	{ (u8 *) "\xff\x02\x01\x00", 4, ParseOK, 1 },
	{ (u8 *) "\xff\x01\x02", 3, ParseOK, 1 },
	{ (u8 *) "\xff\x04\x02\x11\x22\x33", 6, ParseOK, 1 },
	{ (u8 *) "\xff\x01\x04", 3, ParseOK, 1 },
	{ (u8 *) "\xff\x01\x05", 3, ParseOK, 1 },
	{ (u8 *) "\xff\x0d\x05\x11\x22\x33\x44\x55\x55\x11\x22\x33\x44\x55\x55",
	  15, ParseOK, 1 },
	{ (u8 *) "\xff\x01\x06", 3, ParseOK, 1 },
	{ (u8 *) "\xff\x02\x06\x00", 4, ParseOK, 1 },
	{ (u8 *) "\xff\x01\x07", 3, ParseOK, 1 },
	{ (u8 *) "\xff\x09\x07\x11\x22\x33\x44\x55\x66\x77\x88", 11,
	  ParseOK, 1 },
	{ (u8 *) "\xff\x01\x0c", 3, ParseOK, 1 },
	{ (u8 *) "\xff\x02\x0c\x00", 4, ParseOK, 1 },
	{ (u8 *) "\xff\x01\x0d", 3, ParseOK, 1 },
	{ NULL, 0, ParseOK, 0 }
};

static int ieee802_11_parse_tests(void)
{
	int i, ret = 0;
	struct wpabuf *buf;

	wpa_printf(MSG_INFO, "ieee802_11_parse tests");

	for (i = 0; parse_tests[i].data; i++) {
		const struct ieee802_11_parse_test_data *test;
		struct ieee802_11_elems elems;
		ParseRes res;

		test = &parse_tests[i];
		res = ieee802_11_parse_elems(test->data, test->len, &elems, 1);
		if (res != test->result ||
		    ieee802_11_ie_count(test->data, test->len) != test->count) {
			wpa_printf(MSG_ERROR, "ieee802_11_parse test %d failed",
				   i);
			ret = -1;
		}
	}

	if (ieee802_11_vendor_ie_concat((const u8 *) "\x00\x01", 2, 0) != NULL)
	{
		wpa_printf(MSG_ERROR,
			   "ieee802_11_vendor_ie_concat test failed");
		ret = -1;
	}

	buf = ieee802_11_vendor_ie_concat((const u8 *) "\xdd\x05\x11\x22\x33\x44\x01\xdd\x05\x11\x22\x33\x44\x02\x00\x01",
					  16, 0x11223344);
	do {
		const u8 *pos;

		if (!buf) {
			wpa_printf(MSG_ERROR,
				   "ieee802_11_vendor_ie_concat test 2 failed");
			ret = -1;
			break;
		}

		if (wpabuf_len(buf) != 2) {
			wpa_printf(MSG_ERROR,
				   "ieee802_11_vendor_ie_concat test 3 failed");
			ret = -1;
			break;
		}

		pos = wpabuf_head(buf);
		if (pos[0] != 0x01 || pos[1] != 0x02) {
			wpa_printf(MSG_ERROR,
				   "ieee802_11_vendor_ie_concat test 3 failed");
			ret = -1;
			break;
		}
	} while (0);
	wpabuf_free(buf);

	return ret;
}


struct rsn_ie_parse_test_data {
	u8 *data;
	size_t len;
	int result;
};

static const struct rsn_ie_parse_test_data rsn_parse_tests[] = {
	{ (u8 *) "", 0, -1 },
	{ (u8 *) "\x30\x00", 2, -1 },
	{ (u8 *) "\x30\x02\x01\x00", 4, 0 },
	{ (u8 *) "\x30\x02\x00\x00", 4, -2 },
	{ (u8 *) "\x30\x02\x02\x00", 4, -2 },
	{ (u8 *) "\x30\x02\x00\x01", 4, -2 },
	{ (u8 *) "\x30\x02\x00\x00\x00", 5, -2 },
	{ (u8 *) "\x30\x03\x01\x00\x00", 5, -3 },
	{ (u8 *) "\x30\x06\x01\x00\x00\x00\x00\x00", 8, -1 },
	{ (u8 *) "\x30\x06\x01\x00\x00\x0f\xac\x04", 8, 0 },
	{ (u8 *) "\x30\x07\x01\x00\x00\x0f\xac\x04\x00", 9, -5 },
	{ (u8 *) "\x30\x08\x01\x00\x00\x0f\xac\x04\x00\x00", 10, -4 },
	{ (u8 *) "\x30\x08\x01\x00\x00\x0f\xac\x04\x00\x01", 10, -4 },
	{ (u8 *) "\x30\x0c\x01\x00\x00\x0f\xac\x04\x01\x00\x00\x0f\xac\x04",
	  14, 0 },
	{ (u8 *) "\x30\x0c\x01\x00\x00\x0f\xac\x04\x00\x01\x00\x0f\xac\x04",
	  14, -4 },
	{ (u8 *) "\x30\x0c\x01\x00\x00\x0f\xac\x04\x01\x00\x00\x0f\xac\x06",
	  14, -1 },
	{ (u8 *) "\x30\x10\x01\x00\x00\x0f\xac\x04\x02\x00\x00\x0f\xac\x04\x00\x0f\xac\x08",
	  18, 0 },
	{ (u8 *) "\x30\x0d\x01\x00\x00\x0f\xac\x04\x01\x00\x00\x0f\xac\x04\x00",
	  15, -7 },
	{ (u8 *) "\x30\x0e\x01\x00\x00\x0f\xac\x04\x01\x00\x00\x0f\xac\x04\x00\x00",
	  16, -6 },
	{ (u8 *) "\x30\x0e\x01\x00\x00\x0f\xac\x04\x01\x00\x00\x0f\xac\x04\x00\x01",
	  16, -6 },
	{ (u8 *) "\x30\x12\x01\x00\x00\x0f\xac\x04\x01\x00\x00\x0f\xac\x04\x01\x00\x00\x0f\xac\x01",
	  20, 0 },
	{ (u8 *) "\x30\x16\x01\x00\x00\x0f\xac\x04\x01\x00\x00\x0f\xac\x04\x02\x00\x00\x0f\xac\x01\x00\x0f\xac\x02",
	  24, 0 },
	{ (u8 *) "\x30\x13\x01\x00\x00\x0f\xac\x04\x01\x00\x00\x0f\xac\x04\x01\x00\x00\x0f\xac\x01\x00",
	  21, 0 },
	{ (u8 *) "\x30\x14\x01\x00\x00\x0f\xac\x04\x01\x00\x00\x0f\xac\x04\x01\x00\x00\x0f\xac\x01\x00\x00",
	  22, 0 },
	{ (u8 *) "\x30\x16\x01\x00\x00\x0f\xac\x04\x01\x00\x00\x0f\xac\x04\x01\x00\x00\x0f\xac\x01\x00\x00\x00\x00",
	  24, 0 },
	{ (u8 *) "\x30\x16\x01\x00\x00\x0f\xac\x04\x01\x00\x00\x0f\xac\x04\x01\x00\x00\x0f\xac\x01\x00\x00\x00\x01",
	  24, -9 },
	{ (u8 *) "\x30\x1a\x01\x00\x00\x0f\xac\x04\x01\x00\x00\x0f\xac\x04\x01\x00\x00\x0f\xac\x01\x00\x00\x00\x00\x00\x00\x00\x00",
	  28, -10 },
	{ (u8 *) "\x30\x1a\x01\x00\x00\x0f\xac\x04\x01\x00\x00\x0f\xac\x04\x01\x00\x00\x0f\xac\x01\x00\x00\x00\x00\x00\x0f\xac\x06",
	  28, 0 },
	{ (u8 *) "\x30\x1c\x01\x00\x00\x0f\xac\x04\x01\x00\x00\x0f\xac\x04\x01\x00\x00\x0f\xac\x01\x00\x00\x00\x00\x00\x0f\xac\x06\x01\x02",
	  30, 0 },
	{ NULL, 0, 0 }
};

static int rsn_ie_parse_tests(void)
{
	int i, ret = 0;

	wpa_printf(MSG_INFO, "rsn_ie_parse tests");

	for (i = 0; rsn_parse_tests[i].data; i++) {
		const struct rsn_ie_parse_test_data *test;
		struct wpa_ie_data data;

		test = &rsn_parse_tests[i];
		if (wpa_parse_wpa_ie_rsn(test->data, test->len, &data) !=
		    test->result) {
			wpa_printf(MSG_ERROR, "rsn_ie_parse test %d failed", i);
			ret = -1;
		}
	}

	return ret;
}


static int gas_tests(void)
{
	struct wpabuf *buf;

	wpa_printf(MSG_INFO, "gas tests");
	gas_anqp_set_len(NULL);

	buf = wpabuf_alloc(1);
	if (buf == NULL)
		return -1;
	gas_anqp_set_len(buf);
	wpabuf_free(buf);

	buf = wpabuf_alloc(20);
	if (buf == NULL)
		return -1;
	wpabuf_put_u8(buf, WLAN_ACTION_PUBLIC);
	wpabuf_put_u8(buf, WLAN_PA_GAS_INITIAL_REQ);
	wpabuf_put_u8(buf, 0);
	wpabuf_put_be32(buf, 0);
	wpabuf_put_u8(buf, 0);
	gas_anqp_set_len(buf);
	wpabuf_free(buf);

	return 0;
}


static int sae_tests(void)
{
#ifdef CONFIG_SAE
	struct sae_data sae;
	int ret = -1;
	/* IEEE P802.11-REVmd/D2.1, Annex J.10 */
	const u8 addr1[ETH_ALEN] = { 0x82, 0x7b, 0x91, 0x9d, 0xd4, 0xb9 };
	const u8 addr2[ETH_ALEN] = { 0x1e, 0xec, 0x49, 0xea, 0x64, 0x88 };
	const char *pw = "mekmitasdigoat";
	const char *pwid = "psk4internet";
	const u8 local_rand[] = {
		0xa9, 0x06, 0xf6, 0x1e, 0x4d, 0x3a, 0x5d, 0x4e,
		0xb2, 0x96, 0x5f, 0xf3, 0x4c, 0xf9, 0x17, 0xdd,
		0x04, 0x44, 0x45, 0xc8, 0x78, 0xc1, 0x7c, 0xa5,
		0xd5, 0xb9, 0x37, 0x86, 0xda, 0x9f, 0x83, 0xcf
	};
	const u8 local_mask[] = {
		0x42, 0x34, 0xb4, 0xfb, 0x17, 0xaa, 0x43, 0x5c,
		0x52, 0xfb, 0xfd, 0xeb, 0xe6, 0x40, 0x39, 0xb4,
		0x34, 0x78, 0x20, 0x0e, 0x54, 0xff, 0x7b, 0x6e,
		0x07, 0xb6, 0x9c, 0xad, 0x74, 0x15, 0x3c, 0x15
	};
	const u8 local_commit[] = {
		0x13, 0x00, 0xeb, 0x3b, 0xab, 0x19, 0x64, 0xe4,
		0xa0, 0xab, 0x05, 0x92, 0x5d, 0xdf, 0x33, 0x39,
		0x51, 0x91, 0x38, 0xbc, 0x65, 0xd6, 0xcd, 0xc0,
		0xf8, 0x13, 0xdd, 0x6f, 0xd4, 0x34, 0x4e, 0xb4,
		0xbf, 0xe4, 0x4b, 0x5c, 0x21, 0x59, 0x76, 0x58,
		0xf4, 0xe3, 0xed, 0xdf, 0xb4, 0xb9, 0x9f, 0x25,
		0xb4, 0xd6, 0x54, 0x0f, 0x32, 0xff, 0x1f, 0xd5,
		0xc5, 0x30, 0xc6, 0x0a, 0x79, 0x44, 0x48, 0x61,
		0x0b, 0xc6, 0xde, 0x3d, 0x92, 0xbd, 0xbb, 0xd4,
		0x7d, 0x93, 0x59, 0x80, 0xca, 0x6c, 0xf8, 0x98,
		0x8a, 0xb6, 0x63, 0x0b, 0xe6, 0x76, 0x4c, 0x88,
		0x5c, 0xeb, 0x97, 0x93, 0x97, 0x0f, 0x69, 0x52,
		0x17, 0xee, 0xff, 0x0d, 0x21, 0x70, 0x73, 0x6b,
		0x34, 0x69, 0x6e, 0x74, 0x65, 0x72, 0x6e, 0x65,
		0x74
	};
	const u8 peer_commit[] = {
		0x13, 0x00, 0x55, 0x64, 0xf0, 0x45, 0xb2, 0xea,
		0x1e, 0x56, 0x6c, 0xf1, 0xdd, 0x74, 0x1f, 0x70,
		0xd9, 0xbe, 0x35, 0xd2, 0xdf, 0x5b, 0x9a, 0x55,
		0x02, 0x94, 0x6e, 0xe0, 0x3c, 0xf8, 0xda, 0xe2,
		0x7e, 0x1e, 0x05, 0xb8, 0x43, 0x0e, 0xb7, 0xa9,
		0x9e, 0x24, 0x87, 0x7c, 0xe6, 0x9b, 0xaf, 0x3d,
		0xc5, 0x80, 0xe3, 0x09, 0x63, 0x3d, 0x6b, 0x38,
		0x5f, 0x83, 0xee, 0x1c, 0x3e, 0xc3, 0x59, 0x1f,
		0x1a, 0x53, 0x93, 0xc0, 0x6e, 0x80, 0x5d, 0xdc,
		0xeb, 0x2f, 0xde, 0x50, 0x93, 0x0d, 0xd7, 0xcf,
		0xeb, 0xb9, 0x87, 0xc6, 0xff, 0x96, 0x66, 0xaf,
		0x16, 0x4e, 0xb5, 0x18, 0x4d, 0x8e, 0x66, 0x62,
		0xed, 0x6a, 0xff, 0x0d, 0x21, 0x70, 0x73, 0x6b,
		0x34, 0x69, 0x6e, 0x74, 0x65, 0x72, 0x6e, 0x65,
		0x74
	};
	const u8 kck[] = {
		0x59, 0x9d, 0x6f, 0x1e, 0x27, 0x54, 0x8b, 0xe8,
		0x49, 0x9d, 0xce, 0xed, 0x2f, 0xec, 0xcf, 0x94,
		0x81, 0x8c, 0xe1, 0xc7, 0x9f, 0x1b, 0x4e, 0xb3,
		0xd6, 0xa5, 0x32, 0x28, 0xa0, 0x9b, 0xf3, 0xed
	};
	const u8 pmk[] = {
		0x7a, 0xea, 0xd8, 0x6f, 0xba, 0x4c, 0x32, 0x21,
		0xfc, 0x43, 0x7f, 0x5f, 0x14, 0xd7, 0x0d, 0x85,
		0x4e, 0xa5, 0xd5, 0xaa, 0xc1, 0x69, 0x01, 0x16,
		0x79, 0x30, 0x81, 0xed, 0xa4, 0xd5, 0x57, 0xc5
	};
	const u8 pmkid[] = {
		0x40, 0xa0, 0x9b, 0x60, 0x17, 0xce, 0xbf, 0x00,
		0x72, 0x84, 0x3b, 0x53, 0x52, 0xaa, 0x2b, 0x4f
	};
	const u8 local_confirm[] = {
		0x01, 0x00, 0x12, 0xd9, 0xd5, 0xc7, 0x8c, 0x50,
		0x05, 0x26, 0xd3, 0x6c, 0x41, 0xdb, 0xc5, 0x6a,
		0xed, 0xf2, 0x91, 0x4c, 0xed, 0xdd, 0xd7, 0xca,
		0xd4, 0xa5, 0x8c, 0x48, 0xf8, 0x3d, 0xbd, 0xe9,
		0xfc, 0x77
	};
	const u8 peer_confirm[] = {
		0x01, 0x00, 0x02, 0x87, 0x1c, 0xf9, 0x06, 0x89,
		0x8b, 0x80, 0x60, 0xec, 0x18, 0x41, 0x43, 0xbe,
		0x77, 0xb8, 0xc0, 0x8a, 0x80, 0x19, 0xb1, 0x3e,
		0xb6, 0xd0, 0xae, 0xf0, 0xd8, 0x38, 0x3d, 0xfa,
		0xc2, 0xfd
	};
	struct wpabuf *buf = NULL;
	struct crypto_bignum *mask = NULL;

	os_memset(&sae, 0, sizeof(sae));
	buf = wpabuf_alloc(1000);
	if (!buf ||
	    sae_set_group(&sae, 19) < 0 ||
	    sae_prepare_commit(addr1, addr2, (const u8 *) pw, os_strlen(pw),
			       pwid, &sae) < 0)
		goto fail;

	/* Override local values based on SAE test vector */
	crypto_bignum_deinit(sae.tmp->sae_rand, 1);
	sae.tmp->sae_rand = crypto_bignum_init_set(local_rand,
						   sizeof(local_rand));
	mask = crypto_bignum_init_set(local_mask, sizeof(local_mask));
	if (!sae.tmp->sae_rand || !mask)
		goto fail;

	if (crypto_bignum_add(sae.tmp->sae_rand, mask,
			      sae.tmp->own_commit_scalar) < 0 ||
	    crypto_bignum_mod(sae.tmp->own_commit_scalar, sae.tmp->order,
			      sae.tmp->own_commit_scalar) < 0 ||
	    crypto_ec_point_mul(sae.tmp->ec, sae.tmp->pwe_ecc, mask,
				sae.tmp->own_commit_element_ecc) < 0 ||
	    crypto_ec_point_invert(sae.tmp->ec,
				   sae.tmp->own_commit_element_ecc) < 0)
		goto fail;

	/* Check that output matches the test vector */
	sae_write_commit(&sae, buf, NULL, pwid);
	wpa_hexdump_buf(MSG_DEBUG, "SAE: Commit message", buf);

	if (wpabuf_len(buf) != sizeof(local_commit) ||
	    os_memcmp(wpabuf_head(buf), local_commit,
		      sizeof(local_commit)) != 0) {
		wpa_printf(MSG_ERROR, "SAE: Mismatch in local commit");
		goto fail;
	}

	if (sae_parse_commit(&sae, peer_commit, sizeof(peer_commit), NULL, NULL,
		    NULL) != 0 ||
	    sae_process_commit(&sae) < 0)
		goto fail;

	if (os_memcmp(kck, sae.tmp->kck, SAE_KCK_LEN) != 0) {
		wpa_printf(MSG_ERROR, "SAE: Mismatch in KCK");
		goto fail;
	}

	if (os_memcmp(pmk, sae.pmk, SAE_PMK_LEN) != 0) {
		wpa_printf(MSG_ERROR, "SAE: Mismatch in PMK");
		goto fail;
	}

	if (os_memcmp(pmkid, sae.pmkid, SAE_PMKID_LEN) != 0) {
		wpa_printf(MSG_ERROR, "SAE: Mismatch in PMKID");
		goto fail;
	}

	buf->used = 0;
	sae.send_confirm = 1;
	sae_write_confirm(&sae, buf);
	wpa_hexdump_buf(MSG_DEBUG, "SAE: Confirm message", buf);

	if (wpabuf_len(buf) != sizeof(local_confirm) ||
	    os_memcmp(wpabuf_head(buf), local_confirm,
		      sizeof(local_confirm)) != 0) {
		wpa_printf(MSG_ERROR, "SAE: Mismatch in local confirm");
		goto fail;
	}

	if (sae_check_confirm(&sae, peer_confirm, sizeof(peer_confirm)) < 0)
		goto fail;

	ret = 0;
fail:
	sae_clear_data(&sae);
	wpabuf_free(buf);
	crypto_bignum_deinit(mask, 1);
	return ret;
#else /* CONFIG_SAE */
	return 0;
#endif /* CONFIG_SAE */
}


int common_module_tests(void)
{
	int ret = 0;

	wpa_printf(MSG_INFO, "common module tests");

	if (ieee802_11_parse_tests() < 0 ||
	    gas_tests() < 0 ||
	    sae_tests() < 0 ||
	    rsn_ie_parse_tests() < 0)
		ret = -1;

	return ret;
}
