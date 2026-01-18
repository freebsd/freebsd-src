/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright 2025 The FreeBSD Foundation
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
#include <stdio.h>
#include <unistd.h>

static atomic_int finish;

static void *
thr_fun(void *arg)
{
	while (atomic_load_explicit(&finish, memory_order_relaxed) != 1)
		sleep(1);
	atomic_store_explicit(&finish, 2, memory_order_relaxed);
	return (arg);
}

ATF_TC(pthread_tryjoin);
ATF_TC_HEAD(pthread_tryjoin, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Checks pthread_tryjoin(3)");
}

ATF_TC_BODY(pthread_tryjoin, tc)
{
	pthread_t thr;
	void *retval;
	int error, x;

	error = pthread_create(&thr, NULL, thr_fun, &x);
	ATF_REQUIRE_EQ(error, 0);

	error = pthread_tryjoin_np(thr, &retval);
	ATF_REQUIRE_EQ(error, EBUSY);

	atomic_store_explicit(&finish, 1, memory_order_relaxed);
	while (atomic_load_explicit(&finish, memory_order_relaxed) != 2)
		sleep(1);

	error = pthread_tryjoin_np(thr, &retval);
	ATF_REQUIRE_EQ(error, 0);
	ATF_REQUIRE_EQ(retval, &x);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, pthread_tryjoin);
	return (atf_no_error());
}
