/*-
 * Copyright (c) 2025 Dag-Erling Smørgrav <des@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <atf-c.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

/*-
 * Measures qsort(3) runtime with pathological input and verify that it
 * stays close to N * log2(N).
 *
 * Thanks to Vivian Hussey for the proof of concept.
 *
 * The input we construct is similar to a sweep from 0 to N where each
 * half, except for the first element, has been reversed; for instance,
 * with N = 8, we get { 0, 3, 2, 1, 4, 8, 7, 6 }.  This triggers a bug in
 * the BSD qsort(3) where it will switch to insertion sort if the pivots
 * are sorted.
 *
 * This article goes into more detail about the bug and its origin:
 *
 * https://www.raygard.net/2022/02/26/Re-engineering-a-qsort-part-3
 *
 * With this optimization (the `if (swap_cnt == 0)` block), qsort(3) needs
 * roughly N * N / 4 comparisons to sort our pathological input.  Without
 * it, it needs only a little more than N * log2(N) comparisons.
 */

/* we stop testing once a single takes longer than this */
#define MAXRUNSECS 10

static bool debugging;

static uintmax_t ncmp;

static int
intcmp(const void *a, const void *b)
{
	ncmp++;
	return ((*(int *)a > *(int *)b) - (*(int *)a < *(int *)b));
}

static void
qsort_bench(int log2n)
{
	uintmax_t n = 1LLU << log2n;
	int *buf;

	/* fill an array with a pathological pattern */
	ATF_REQUIRE(buf = malloc(n * sizeof(*buf)));
	buf[0] = 0;
	buf[n / 2] = n / 2;
	for (unsigned int i = 1; i < n / 2; i++) {
		buf[i] = n / 2 - i;
		buf[n / 2 + i] = n - i;
	}

	ncmp = 0;
	qsort(buf, n, sizeof(*buf), intcmp);

	/* check result and free array */
	if (debugging) {
		for (unsigned int i = 1; i < n; i++) {
			ATF_REQUIRE_MSG(buf[i] > buf[i - 1],
			    "array is not sorted");
		}
	}
	free(buf);

	/* check that runtime does not exceed N² */
	ATF_CHECK_MSG(ncmp / n < n,
	    "runtime %ju exceeds N² for N = %ju", ncmp, n);

	/* check that runtime does not exceed N log N by much */
	ATF_CHECK_MSG(ncmp / n <= log2n + 1,
	    "runtime %ju exceeds N log N for N = %ju", ncmp, n);
}

ATF_TC_WITHOUT_HEAD(qsort_bench);
ATF_TC_BODY(qsort_bench, tc)
{
	struct timespec t0, t1;
	uintmax_t tus;

	for (int i = 10; i <= 30; i++) {
		clock_gettime(CLOCK_UPTIME, &t0);
		qsort_bench(i);
		clock_gettime(CLOCK_UPTIME, &t1);
		tus = t1.tv_sec * 1000000 + t1.tv_nsec / 1000;
		tus -= t0.tv_sec * 1000000 + t0.tv_nsec / 1000;
		if (debugging) {
			fprintf(stderr, "N = 2^%d in %ju.%06jus\n",
			    i, tus / 1000000, tus % 1000000);
		}
		/* stop once an individual run exceeds our limit */
		if (tus / 1000000 >= MAXRUNSECS)
			break;
	}
}

ATF_TP_ADD_TCS(tp)
{
	debugging = !getenv("__RUNNING_INSIDE_ATF_RUN") &&
	    isatty(STDERR_FILENO);
	ATF_TP_ADD_TC(tp, qsort_bench);
	return (atf_no_error());
}
