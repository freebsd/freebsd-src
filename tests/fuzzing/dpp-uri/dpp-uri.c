/*
 * DPP URI fuzzer
 * Copyright (c) 2020, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "common/dpp.h"
#include "../fuzzer-common.h"


int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	struct dpp_global *dpp;
	struct dpp_global_config config;
	struct dpp_bootstrap_info *bi;
	char *uri;
	char buf[1000];
	int ret = -1;

	wpa_fuzzer_set_debug_level();

	if (os_program_init())
		return 0;

	uri = os_malloc(size + 1);
	if (!uri)
		goto out;
	os_memcpy(uri, data, size);
	uri[size] = '\0';
	os_memset(&config, 0, sizeof(config));
	dpp = dpp_global_init(&config);
	if (!dpp)
		goto out;

	bi = dpp_add_qr_code(dpp, uri);
	if (bi && dpp_bootstrap_info(dpp, bi->id, buf, sizeof(buf)) > 0)
		wpa_printf(MSG_DEBUG, "DPP: %s", buf);
	dpp_global_deinit(dpp);

	ret = 0;
out:
	os_free(uri);
	os_program_deinit();

	return ret;
}
