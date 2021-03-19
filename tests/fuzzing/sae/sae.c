/*
 * SAE fuzzer
 * Copyright (c) 2020, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "common/sae.h"
#include "../fuzzer-common.h"


int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	struct sae_data sae;
	u16 res;
	const u8 *token = NULL;
	size_t token_len = 0;
	int groups[] = { 19, 0 };

	wpa_fuzzer_set_debug_level();

	if (os_program_init())
		return 0;

	os_memset(&sae, 0, sizeof(sae));
	res = sae_parse_commit(&sae, data, size, &token, &token_len, groups, 0);
	wpa_printf(MSG_DEBUG, "sae_parse_commit(0): %u", res);
	sae_clear_data(&sae);
	res = sae_parse_commit(&sae, data, size, &token, &token_len, groups, 1);
	wpa_printf(MSG_DEBUG, "sae_parse_commit(1): %u", res);
	sae_clear_data(&sae);
	os_program_deinit();

	return 0;
}
