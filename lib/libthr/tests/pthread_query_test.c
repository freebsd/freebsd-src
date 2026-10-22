/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright 2026 The FreeBSD Foundation
 *
 * This software was developed by Konstantin Belousov <kib@FreeBSD.org>
 * under sponsorship from the FreeBSD Foundation.
 */

#include <atf-c.h>
#include <err.h>
#include <errno.h>
#include <pthread.h>
#include <pthread_np.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static atomic_int started;

#define	MY_ERRNO_VAL	0x12345678

static void *
thr_fun(void *arg)
{
	errno = MY_ERRNO_VAL;
	/* release so the errno update is visible */
	atomic_store_explicit(&started, 1, memory_order_release);
	for (;;)
		sleep(1);
	return (arg);
}

ATF_TC(pthread_query_all);
ATF_TC_HEAD(pthread_query_all, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Checks pthread_query_all_np(3)");
}

ATF_TC_BODY(pthread_query_all, tc)
{
	pthread_t thr;
	struct pthread_info_np *pi;
	size_t bufsz, n;
	void *p;
	int error;
	bool found;

	error = pthread_create(&thr, NULL, thr_fun, NULL);
	ATF_REQUIRE_EQ(error, 0);

	while (atomic_load_explicit(&started, memory_order_acquire) != 1)
		usleep(1000);

	bufsz = 0;
	error = pthread_query_all_np(NULL, &bufsz);
	ATF_REQUIRE_EQ(error, ENOMEM);
	ATF_REQUIRE(bufsz >= sizeof(*pi) * 2);
	p = malloc(bufsz);
	ATF_REQUIRE(p != NULL);

	error = pthread_query_all_np((struct pthread_info_np *)p, &bufsz);
	ATF_REQUIRE_EQ(error, 0);

	found = false;
	for (n = 0; n < bufsz;) {
		pi = p;
		if (pi->thread == thr) {
			found = true;
			break;
		}

		n += pi->pthread_info_size;
		p = (char *)p + pi->pthread_info_size;
	}

	ATF_REQUIRE(found);
	ATF_REQUIRE((pi->flags & PTHREAD_INFO_DETACHED_NP) == 0);
	ATF_REQUIRE((pi->flags & PTHREAD_INFO_INITIAL_NP) == 0);
	ATF_REQUIRE_EQ(*(pi->errno_location), MY_ERRNO_VAL);
}

ATF_TC(pthread_query_main);
ATF_TC_HEAD(pthread_query_main, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Checks pthread_query_np(3) for single initial thread");
}

ATF_TC_BODY(pthread_query_main, tc)
{
	pthread_mutex_t mtx;
	pthread_t main_thr;
	struct pthread_info_np pi;
	int error;
	static const char main_name[] = "main_name";

	error = pthread_mutex_init(&mtx, NULL);	/* init the threading lib */
	ATF_REQUIRE_EQ(error, 0);

	main_thr = pthread_self();
	pthread_set_name_np(main_thr, main_name);
	error = pthread_query_np(main_thr, &pi, sizeof(pi));
	ATF_REQUIRE_EQ(error, 0);
	ATF_REQUIRE_EQ(pi.pthread_info_size, sizeof(pi));
	ATF_REQUIRE_EQ(pi.state, PTHREAD_STATE_RUNNING_NP);
	ATF_REQUIRE((pi.flags & PTHREAD_INFO_INITIAL_NP) != 0);
	ATF_REQUIRE_EQ(pi.thread, main_thr);
	ATF_REQUIRE_EQ(pi.lwpid, pthread_getthreadid_np());
	ATF_REQUIRE_EQ(strcmp(pi.name, main_name), 0);
	ATF_REQUIRE_EQ(pi.errno_location, &errno);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, pthread_query_main);
	ATF_TP_ADD_TC(tp, pthread_query_all);
	return (atf_no_error());
}
