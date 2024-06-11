/*-
 * Copyright (c) 2024 Klara, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/types.h>
#include <sys/mman.h>

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <arraylist.h>
#include <diff_main.h>

#include <atf-c.h>

ATF_TC_WITH_CLEANUP(diff_atomize_truncated);
ATF_TC_HEAD(diff_atomize_truncated, tc)
{
	atf_tc_set_md_var(tc, "descr", "Verify that the atomizer "
	    "does not crash when an input file is truncated");
}
ATF_TC_BODY(diff_atomize_truncated, tc)
{
	char line[128];
	struct diff_config cfg = { .atomize_func = diff_atomize_text_by_line };
	struct diff_data d = { };
	const char *fn = atf_tc_get_ident(tc);
	FILE *f;
	unsigned char *p;
	size_t size = 65536;

	ATF_REQUIRE((f = fopen(fn, "w+")) != NULL);
	line[sizeof(line) - 1] = '\n';
	for (unsigned int i = 0; i <= size / sizeof(line); i++) {
		memset(line, 'a' + i % 26, sizeof(line) - 1);
		ATF_REQUIRE(fwrite(line, sizeof(line), 1, f) == 1);
	}
	ATF_REQUIRE(fsync(fileno(f)) == 0);
	rewind(f);
	ATF_REQUIRE(truncate(fn, size / 2) == 0);
	ATF_REQUIRE((p = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fileno(f), 0)) != MAP_FAILED);
	ATF_REQUIRE(diff_atomize_file(&d, &cfg, f, p, size, 0) == 0);
	ATF_REQUIRE((size_t)d.len <= size / 2);
	ATF_REQUIRE((size_t)d.len >= size / 2 - sizeof(line));
	ATF_REQUIRE(d.atomizer_flags & DIFF_ATOMIZER_FILE_TRUNCATED);
}
ATF_TC_CLEANUP(diff_atomize_truncated, tc)
{
	unlink(atf_tc_get_ident(tc));
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, diff_atomize_truncated);
	return atf_no_error();
}
