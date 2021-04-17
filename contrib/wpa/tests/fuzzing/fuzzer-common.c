/*
 * Common helper functions for fuzzing tools
 * Copyright (c) 2019, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"


void wpa_fuzzer_set_debug_level(void)
{
	static int first = 1;

	if (first) {
		char *env;

		first = 0;
		env = getenv("WPADEBUG");
		if (env)
			wpa_debug_level = atoi(env);
		else
			wpa_debug_level = MSG_ERROR + 1;

		wpa_debug_show_keys = 1;
	}
}


#ifndef TEST_LIBFUZZER
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

int main(int argc, char *argv[])
{
	char *data;
	size_t len;

	if (argc < 2) {
		printf("usage: %s <file>\n", argv[0]);
		return -1;
	}

	data = os_readfile(argv[1], &len);
	if (!data) {
		printf("Could not read '%s'\n", argv[1]);
		return -1;
	}

	LLVMFuzzerTestOneInput((const uint8_t *) data, len);
	os_free(data);
	return 0;
}
#endif /* !TEST_LIBFUZZER */
