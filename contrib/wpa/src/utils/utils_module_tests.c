/*
 * utils module tests
 * Copyright (c) 2014-2015, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "utils/bitfield.h"
#include "utils/ext_password.h"
#include "utils/trace.h"
#include "utils/base64.h"


struct printf_test_data {
	u8 *data;
	size_t len;
	char *encoded;
};

static const struct printf_test_data printf_tests[] = {
	{ (u8 *) "abcde", 5, "abcde" },
	{ (u8 *) "a\0b\nc\ed\re\tf\"\\", 13, "a\\0b\\nc\\ed\\re\\tf\\\"\\\\" },
	{ (u8 *) "\x00\x31\x00\x32\x00\x39", 6, "\\x001\\0002\\09" },
	{ (u8 *) "\n\n\n", 3, "\n\12\x0a" },
	{ (u8 *) "\303\245\303\244\303\266\303\205\303\204\303\226", 12,
	  "\\xc3\\xa5\xc3\\xa4\\xc3\\xb6\\xc3\\x85\\xc3\\x84\\xc3\\x96" },
	{ (u8 *) "\303\245\303\244\303\266\303\205\303\204\303\226", 12,
	  "\\303\\245\\303\\244\\303\\266\\303\\205\\303\\204\\303\\226" },
	{ (u8 *) "\xe5\xe4\xf6\xc5\xc4\xd6", 6,
	  "\\xe5\\xe4\\xf6\\xc5\\xc4\\xd6" },
	{ NULL, 0, NULL }
};


static int printf_encode_decode_tests(void)
{
	int i;
	size_t binlen;
	char buf[100];
	u8 bin[100];
	int errors = 0;

	wpa_printf(MSG_INFO, "printf encode/decode tests");

	for (i = 0; printf_tests[i].data; i++) {
		const struct printf_test_data *test = &printf_tests[i];
		printf_encode(buf, sizeof(buf), test->data, test->len);
		wpa_printf(MSG_INFO, "%d: -> \"%s\"", i, buf);

		binlen = printf_decode(bin, sizeof(bin), buf);
		if (binlen != test->len ||
		    os_memcmp(bin, test->data, binlen) != 0) {
			wpa_hexdump(MSG_ERROR, "Error in decoding#1",
				    bin, binlen);
			errors++;
		}

		binlen = printf_decode(bin, sizeof(bin), test->encoded);
		if (binlen != test->len ||
		    os_memcmp(bin, test->data, binlen) != 0) {
			wpa_hexdump(MSG_ERROR, "Error in decoding#2",
				    bin, binlen);
			errors++;
		}
	}

	buf[5] = 'A';
	printf_encode(buf, 5, (const u8 *) "abcde", 5);
	if (buf[5] != 'A') {
		wpa_printf(MSG_ERROR, "Error in bounds checking#1");
		errors++;
	}

	for (i = 5; i < 10; i++) {
		buf[i] = 'A';
		printf_encode(buf, i, (const u8 *) "\xdd\xdd\xdd\xdd\xdd", 5);
		if (buf[i] != 'A') {
			wpa_printf(MSG_ERROR, "Error in bounds checking#2(%d)",
				   i);
			errors++;
		}
	}

	if (printf_decode(bin, 3, "abcde") != 2)
		errors++;

	if (printf_decode(bin, 3, "\\xa") != 1 || bin[0] != 10)
		errors++;

	if (printf_decode(bin, 3, "\\a") != 1 || bin[0] != 'a')
		errors++;

	if (errors) {
		wpa_printf(MSG_ERROR, "%d printf test(s) failed", errors);
		return -1;
	}

	return 0;
}


static int bitfield_tests(void)
{
	struct bitfield *bf;
	int i;
	int errors = 0;

	wpa_printf(MSG_INFO, "bitfield tests");

	bf = bitfield_alloc(123);
	if (bf == NULL)
		return -1;

	for (i = 0; i < 123; i++) {
		if (bitfield_is_set(bf, i) || bitfield_is_set(bf, i + 1))
			errors++;
		if (i > 0 && bitfield_is_set(bf, i - 1))
			errors++;
		bitfield_set(bf, i);
		if (!bitfield_is_set(bf, i))
			errors++;
		bitfield_clear(bf, i);
		if (bitfield_is_set(bf, i))
			errors++;
	}

	for (i = 123; i < 200; i++) {
		if (bitfield_is_set(bf, i) || bitfield_is_set(bf, i + 1))
			errors++;
		if (i > 0 && bitfield_is_set(bf, i - 1))
			errors++;
		bitfield_set(bf, i);
		if (bitfield_is_set(bf, i))
			errors++;
		bitfield_clear(bf, i);
		if (bitfield_is_set(bf, i))
			errors++;
	}

	for (i = 0; i < 123; i++) {
		if (bitfield_is_set(bf, i) || bitfield_is_set(bf, i + 1))
			errors++;
		bitfield_set(bf, i);
		if (!bitfield_is_set(bf, i))
			errors++;
	}

	for (i = 0; i < 123; i++) {
		if (!bitfield_is_set(bf, i))
			errors++;
		bitfield_clear(bf, i);
		if (bitfield_is_set(bf, i))
			errors++;
	}

	for (i = 0; i < 123; i++) {
		if (bitfield_get_first_zero(bf) != i)
			errors++;
		bitfield_set(bf, i);
	}
	if (bitfield_get_first_zero(bf) != -1)
		errors++;
	for (i = 0; i < 123; i++) {
		if (!bitfield_is_set(bf, i))
			errors++;
		bitfield_clear(bf, i);
		if (bitfield_get_first_zero(bf) != i)
			errors++;
		bitfield_set(bf, i);
	}
	if (bitfield_get_first_zero(bf) != -1)
		errors++;

	bitfield_free(bf);

	bf = bitfield_alloc(8);
	if (bf == NULL)
		return -1;
	if (bitfield_get_first_zero(bf) != 0)
		errors++;
	for (i = 0; i < 8; i++)
		bitfield_set(bf, i);
	if (bitfield_get_first_zero(bf) != -1)
		errors++;
	bitfield_free(bf);

	if (errors) {
		wpa_printf(MSG_ERROR, "%d bitfield test(s) failed", errors);
		return -1;
	}

	return 0;
}


static int int_array_tests(void)
{
	int test1[] = { 1, 2, 3, 4, 5, 6, 0 };
	int test2[] = { 1, -1, 0 };
	int test3[] = { 1, 1, 1, -1, 2, 3, 4, 1, 2, 0 };
	int test3_res[] = { -1, 1, 2, 3, 4, 0 };
	int errors = 0;
	int len;

	wpa_printf(MSG_INFO, "int_array tests");

	if (int_array_len(test1) != 6 ||
	    int_array_len(test2) != 2)
		errors++;

	int_array_sort_unique(test3);
	len = int_array_len(test3_res);
	if (int_array_len(test3) != len)
		errors++;
	else if (os_memcmp(test3, test3_res, len * sizeof(int)) != 0)
		errors++;

	if (errors) {
		wpa_printf(MSG_ERROR, "%d int_array test(s) failed", errors);
		return -1;
	}

	return 0;
}


static int ext_password_tests(void)
{
	struct ext_password_data *data;
	int ret = 0;
	struct wpabuf *pw;

	wpa_printf(MSG_INFO, "ext_password tests");

	data = ext_password_init("unknown", "foo");
	if (data != NULL)
		return -1;

	data = ext_password_init("test", NULL);
	if (data == NULL)
		return -1;
	pw = ext_password_get(data, "foo");
	if (pw != NULL)
		ret = -1;
	ext_password_free(pw);

	ext_password_deinit(data);

	pw = ext_password_get(NULL, "foo");
	if (pw != NULL)
		ret = -1;
	ext_password_free(pw);

	return ret;
}


static int trace_tests(void)
{
	wpa_printf(MSG_INFO, "trace tests");

	wpa_trace_show("test backtrace");
	wpa_trace_dump_funcname("test funcname", trace_tests);

	return 0;
}


static int base64_tests(void)
{
	int errors = 0;
	unsigned char *res;
	size_t res_len;

	wpa_printf(MSG_INFO, "base64 tests");

	res = base64_encode((const unsigned char *) "", ~0, &res_len);
	if (res) {
		errors++;
		os_free(res);
	}

	res = base64_encode((const unsigned char *) "=", 1, &res_len);
	if (!res || res_len != 5 || res[0] != 'P' || res[1] != 'Q' ||
	    res[2] != '=' || res[3] != '=' || res[4] != '\n')
		errors++;
	os_free(res);

	res = base64_encode((const unsigned char *) "=", 1, NULL);
	if (!res || res[0] != 'P' || res[1] != 'Q' ||
	    res[2] != '=' || res[3] != '=' || res[4] != '\n')
		errors++;
	os_free(res);

	res = base64_decode((const unsigned char *) "", 0, &res_len);
	if (res) {
		errors++;
		os_free(res);
	}

	res = base64_decode((const unsigned char *) "a", 1, &res_len);
	if (res) {
		errors++;
		os_free(res);
	}

	res = base64_decode((const unsigned char *) "====", 4, &res_len);
	if (res) {
		errors++;
		os_free(res);
	}

	res = base64_decode((const unsigned char *) "PQ==", 4, &res_len);
	if (!res || res_len != 1 || res[0] != '=')
		errors++;
	os_free(res);

	res = base64_decode((const unsigned char *) "P.Q-=!=*", 8, &res_len);
	if (!res || res_len != 1 || res[0] != '=')
		errors++;
	os_free(res);

	if (errors) {
		wpa_printf(MSG_ERROR, "%d base64 test(s) failed", errors);
		return -1;
	}

	return 0;
}


static int common_tests(void)
{
	char buf[3];
	u8 addr[ETH_ALEN] = { 1, 2, 3, 4, 5, 6 };
	u8 bin[3];
	int errors = 0;
	struct wpa_freq_range_list ranges;

	wpa_printf(MSG_INFO, "common tests");

	if (hwaddr_mask_txt(buf, 3, addr, addr) != -1)
		errors++;

	if (wpa_scnprintf(buf, 0, "hello") != 0 ||
	    wpa_scnprintf(buf, 3, "hello") != 2)
		errors++;

	if (wpa_snprintf_hex(buf, 0, addr, ETH_ALEN) != 0 ||
	    wpa_snprintf_hex(buf, 3, addr, ETH_ALEN) != 2)
		errors++;

	if (merge_byte_arrays(bin, 3, addr, ETH_ALEN, NULL, 0) != 3 ||
	    merge_byte_arrays(bin, 3, NULL, 0, addr, ETH_ALEN) != 3)
		errors++;

	if (dup_binstr(NULL, 0) != NULL)
		errors++;

	if (freq_range_list_includes(NULL, 0) != 0)
		errors++;

	os_memset(&ranges, 0, sizeof(ranges));
	if (freq_range_list_parse(&ranges, "") != 0 ||
	    freq_range_list_includes(&ranges, 0) != 0 ||
	    freq_range_list_str(&ranges) != NULL)
		errors++;

	if (utf8_unescape(NULL, 0, buf, sizeof(buf)) != 0 ||
	    utf8_unescape("a", 1, NULL, 0) != 0 ||
	    utf8_unescape("a\\", 2, buf, sizeof(buf)) != 0 ||
	    utf8_unescape("abcde", 5, buf, sizeof(buf)) != 0 ||
	    utf8_unescape("abc", 3, buf, 3) != 3)
		errors++;

	if (utf8_unescape("a", 0, buf, sizeof(buf)) != 1 || buf[0] != 'a')
		errors++;

	if (utf8_unescape("\\b", 2, buf, sizeof(buf)) != 1 || buf[0] != 'b')
		errors++;

	if (utf8_escape(NULL, 0, buf, sizeof(buf)) != 0 ||
	    utf8_escape("a", 1, NULL, 0) != 0 ||
	    utf8_escape("abcde", 5, buf, sizeof(buf)) != 0 ||
	    utf8_escape("a\\bcde", 6, buf, sizeof(buf)) != 0 ||
	    utf8_escape("ab\\cde", 6, buf, sizeof(buf)) != 0 ||
	    utf8_escape("abc\\de", 6, buf, sizeof(buf)) != 0 ||
	    utf8_escape("abc", 3, buf, 3) != 3)
		errors++;

	if (utf8_escape("a", 0, buf, sizeof(buf)) != 1 || buf[0] != 'a')
		errors++;

	if (errors) {
		wpa_printf(MSG_ERROR, "%d common test(s) failed", errors);
		return -1;
	}

	return 0;
}


int utils_module_tests(void)
{
	int ret = 0;

	wpa_printf(MSG_INFO, "utils module tests");

	if (printf_encode_decode_tests() < 0 ||
	    ext_password_tests() < 0 ||
	    trace_tests() < 0 ||
	    bitfield_tests() < 0 ||
	    base64_tests() < 0 ||
	    common_tests() < 0 ||
	    int_array_tests() < 0)
		ret = -1;

	return ret;
}
