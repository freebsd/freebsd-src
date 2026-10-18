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
#include <fcntl.h>
#include <kvm.h>
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
		usleep(1000);
	return (arg);
}

static int
thread_count(kvm_t *kd, pid_t pid)
{
	struct kinfo_proc *kip;
	int count;

	kip = kvm_getprocs(kd, KERN_PROC_PID | KERN_PROC_INC_THREAD, pid,
	    &count);
	ATF_REQUIRE(kip != NULL);
	return (count);
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
	kvm_t *kd;
	void *retval;
	int error, nthr, x;

	kd = kvm_openfiles(NULL, "/dev/null", NULL, O_RDONLY, NULL);
	ATF_REQUIRE(kd != NULL);
	nthr = thread_count(kd, getpid());

	error = pthread_create(&thr, NULL, thr_fun, &x);
	ATF_REQUIRE_EQ(error, 0);

	error = pthread_tryjoin_np(thr, &retval);
	ATF_REQUIRE_EQ(error, EBUSY);

	atomic_store_explicit(&finish, 1, memory_order_relaxed);
	while (thread_count(kd, getpid()) != nthr)
		usleep(1000);
	kvm_close(kd);

	error = pthread_tryjoin_np(thr, &retval);
	ATF_REQUIRE_EQ(error, 0);
	ATF_REQUIRE_EQ(retval, &x);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, pthread_tryjoin);
	return (atf_no_error());
}
