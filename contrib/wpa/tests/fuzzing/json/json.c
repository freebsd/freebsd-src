/*
 * JSON parser - test program
 * Copyright (c) 2019, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"
#include "utils/common.h"
#include "utils/json.h"
#include "../fuzzer-common.h"


int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	struct json_token *root;
	char *txt;
	size_t buflen = 10000;

	wpa_fuzzer_set_debug_level();

	root = json_parse((const char *) data, size);
	if (!root) {
		wpa_printf(MSG_DEBUG, "JSON parsing failed");
		return 0;
	}

	txt = os_zalloc(buflen);
	if (txt) {
		json_print_tree(root, txt, buflen);
		wpa_printf(MSG_DEBUG, "%s", txt);
		os_free(txt);
	}
	json_free(root);

	return 0;
}
