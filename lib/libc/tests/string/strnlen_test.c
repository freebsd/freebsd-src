/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Strahinja Stanisic <strajabot@FreeBSD.org>
 */

#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdalign.h>
#include <stdint.h>

#include <atf-c.h>

ATF_TC(strnlen_alignments);
ATF_TC_HEAD(strnlen_alignments, tc)
{
    atf_tc_set_md_var(tc, "descr", "Test strnlen(3) with different alignments");
}

ATF_TC_BODY(strnlen_alignments, tc)
{
	size_t (*strnlen_fn)(const char*, size_t) = strnlen;
	char alignas(16) buffer[1 + 16 + 64 + 1 + 1];

	memset(buffer, '/', sizeof(buffer));

	for (int align = 1; align < 1 + 16; align++) {
		char *s = buffer + align;

		for (size_t maxlen = 0; maxlen <= 64; maxlen++) {
			for (size_t len = 0; len <= maxlen; len++) {
				/* returns length */

				/* without sentinels */
				s[len] = '\0';
				size_t val = strnlen_fn(s, maxlen);
				if (val != len) {
					fprintf(stderr, "align =  %d, maxlen = %zu, len = %zu",
					    align, maxlen, len);
					atf_tc_fail("returned incorrect len");
				}

				/* with sentinels */
				s[-1] = '\0';
				s[maxlen + 1] = '\0';
				val = strnlen_fn(s, maxlen);
				if (val != len) {
					fprintf(stderr, "align =  %d, maxlen = %zu, len = %zu",
					    align, maxlen, len);
					atf_tc_fail("returned incorrect len (sentinels)");
				}

				/* cleanup */
				s[-1] = '/';
				s[len] = '/';
				s[maxlen + 1] = '/';

			}

			/* returns maxlen */

			/* without sentinels */
			size_t val = strnlen_fn(s, maxlen);
			if (val != maxlen) {
				fprintf(stderr, "align =  %d, maxlen = %zu",
				     align, maxlen);
				atf_tc_fail("should return maxlen");
			}

			/* with sentinels */
			s[-1] = '\0';
			s[maxlen + 1] = '\0';
			val = strnlen_fn(s, maxlen);
			if (val != maxlen) {
				fprintf(stderr, "align =  %d, maxlen = %zu",
				    align, maxlen);
				atf_tc_fail("should return maxlen (sentinels)");
			}

			/* cleanup */
			s[-1] = '/';
			s[maxlen + 1] = '/';
		}
	}
}

ATF_TC(strnlen_size_max);
ATF_TC_HEAD(strnlen_size_max, tc)
{
    atf_tc_set_md_var(tc, "descr", "Test strnlen(3) with maxlen=SIZE_MAX");
}

ATF_TC_BODY(strnlen_size_max, tc)
{
	size_t (*strnlen_fn)(const char*, size_t) = strnlen;
	char alignas(16) buffer[1 + 16 + 64 + 1 + 1];

	memset(buffer, '/', sizeof(buffer));

	for (int align = 1; align < 1 + 16; align++) {
		char* s = buffer + align;

		for (size_t len = 0; len <= 64; len++) {
			/* returns length */

			/* without sentinels */
			s[len] = '\0';
			size_t val = strnlen_fn(s, SIZE_MAX);
			if (val != len) {
				fprintf(stderr, "align =  %d, maxlen = %zu, len = %zu",
				    align, SIZE_MAX, len);
				atf_tc_fail("returned incorrect len (SIZE_MAX)");
			}

			/* with sentinels */
			s[-1] = '\0';
			val = strnlen_fn(s, SIZE_MAX);
			if (val != len) {
				fprintf(stderr, "align =  %d, maxlen = %zu, len = %zu",
				    align, SIZE_MAX, len);
				atf_tc_fail("returned incorrect len (sentinels) (SIZE_MAX)");
			}

			/* cleanup */
			s[-1] = '/';
			s[len] = '/';
		}
	}
}



ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, strnlen_alignments);
	ATF_TP_ADD_TC(tp, strnlen_size_max);

	return atf_no_error();
}
