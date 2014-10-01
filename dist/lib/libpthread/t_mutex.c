/* $NetBSD: t_mutex.c,v 1.6 2014/02/09 21:26:07 jmmv Exp $ */

/*
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__COPYRIGHT("@(#) Copyright (c) 2008\
 The NetBSD Foundation, inc. All rights reserved.");
__RCSID("$NetBSD: t_mutex.c,v 1.6 2014/02/09 21:26:07 jmmv Exp $");

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <atf-c.h>
#include <atf-c/config.h>

#include "h_common.h"

static pthread_mutex_t mutex;
static pthread_mutex_t static_mutex = PTHREAD_MUTEX_INITIALIZER;
static int global_x;

static void *
mutex1_threadfunc(void *arg)
{
	int *param;

	printf("2: Second thread.\n");

	param = arg;
	printf("2: Locking mutex\n");
	pthread_mutex_lock(&mutex);
	printf("2: Got mutex. *param = %d\n", *param);
	ATF_REQUIRE_EQ(*param, 20);
	(*param)++;

	pthread_mutex_unlock(&mutex);

	return param;
}

ATF_TC(mutex1);
ATF_TC_HEAD(mutex1, tc)
{
	atf_tc_set_md_var(tc, "descr", "Checks mutexes");
}
ATF_TC_BODY(mutex1, tc)
{
	int x;
	pthread_t new;
	void *joinval;

	printf("1: Mutex-test 1\n");

	PTHREAD_REQUIRE(pthread_mutex_init(&mutex, NULL));
	x = 1;
	PTHREAD_REQUIRE(pthread_mutex_lock(&mutex));
	PTHREAD_REQUIRE(pthread_create(&new, NULL, mutex1_threadfunc, &x));
	printf("1: Before changing the value.\n");
	sleep(2);
	x = 20;
	printf("1: Before releasing the mutex.\n");
	sleep(2);
	PTHREAD_REQUIRE(pthread_mutex_unlock(&mutex));
	printf("1: After releasing the mutex.\n");
	PTHREAD_REQUIRE(pthread_join(new, &joinval));

	PTHREAD_REQUIRE(pthread_mutex_lock(&mutex));
	printf("1: Thread joined. X was %d. Return value (int) was %d\n",
		x, *(int *)joinval);
	ATF_REQUIRE_EQ(x, 21);
	ATF_REQUIRE_EQ(*(int *)joinval, 21);
	PTHREAD_REQUIRE(pthread_mutex_unlock(&mutex));
}

static void *
mutex2_threadfunc(void *arg)
{
	long count = *(int *)arg;

	printf("2: Second thread (%p). Count is %ld\n", pthread_self(), count);

	while (count--) {
		PTHREAD_REQUIRE(pthread_mutex_lock(&mutex));
		global_x++;
		PTHREAD_REQUIRE(pthread_mutex_unlock(&mutex));
	}

	return (void *)count;
}

ATF_TC(mutex2);
ATF_TC_HEAD(mutex2, tc)
{
	atf_tc_set_md_var(tc, "descr", "Checks mutexes");
#if defined(__powerpc__)
	atf_tc_set_md_var(tc, "timeout", "40");
#endif
}
ATF_TC_BODY(mutex2, tc)
{
	int count, count2;
	pthread_t new;
	void *joinval;

	printf("1: Mutex-test 2\n");

#if defined(__powerpc__)
	atf_tc_expect_timeout("PR port-powerpc/44387");
#endif

	PTHREAD_REQUIRE(pthread_mutex_init(&mutex, NULL));
	
	global_x = 0;
	count = count2 = 10000000;

	PTHREAD_REQUIRE(pthread_mutex_lock(&mutex));
	PTHREAD_REQUIRE(pthread_create(&new, NULL, mutex2_threadfunc, &count2));

	printf("1: Thread %p\n", pthread_self());

	PTHREAD_REQUIRE(pthread_mutex_unlock(&mutex));

	while (count--) {
		PTHREAD_REQUIRE(pthread_mutex_lock(&mutex));
		global_x++;
		PTHREAD_REQUIRE(pthread_mutex_unlock(&mutex));
	}

	PTHREAD_REQUIRE(pthread_join(new, &joinval));

	PTHREAD_REQUIRE(pthread_mutex_lock(&mutex));
	printf("1: Thread joined. X was %d. Return value (long) was %ld\n",
		global_x, (long)joinval);
	ATF_REQUIRE_EQ(global_x, 20000000);

#if defined(__powerpc__)
	/* XXX force a timeout in ppc case since an un-triggered race
	   otherwise looks like a "failure" */
	/* We sleep for longer than the timeout to make ATF not
	   complain about unexpected success */
	sleep(41);
#endif
}

static void *
mutex3_threadfunc(void *arg)
{
	long count = *(int *)arg;

	printf("2: Second thread (%p). Count is %ld\n", pthread_self(), count);

	while (count--) {
		PTHREAD_REQUIRE(pthread_mutex_lock(&static_mutex));
		global_x++;
		PTHREAD_REQUIRE(pthread_mutex_unlock(&static_mutex));
	}

	return (void *)count;
}

ATF_TC(mutex3);
ATF_TC_HEAD(mutex3, tc)
{
	atf_tc_set_md_var(tc, "descr", "Checks mutexes using a static "
	    "initializer");
#if defined(__powerpc__)
	atf_tc_set_md_var(tc, "timeout", "40");
#endif
}
ATF_TC_BODY(mutex3, tc)
{
	int count, count2;
	pthread_t new;
	void *joinval;

	printf("1: Mutex-test 3\n");

#if defined(__powerpc__)
	atf_tc_expect_timeout("PR port-powerpc/44387");
#endif

	global_x = 0;
	count = count2 = 10000000;

	PTHREAD_REQUIRE(pthread_mutex_lock(&static_mutex));
	PTHREAD_REQUIRE(pthread_create(&new, NULL, mutex3_threadfunc, &count2));

	printf("1: Thread %p\n", pthread_self());

	PTHREAD_REQUIRE(pthread_mutex_unlock(&static_mutex));

	while (count--) {
		PTHREAD_REQUIRE(pthread_mutex_lock(&static_mutex));
		global_x++;
		PTHREAD_REQUIRE(pthread_mutex_unlock(&static_mutex));
	}

	PTHREAD_REQUIRE(pthread_join(new, &joinval));

	PTHREAD_REQUIRE(pthread_mutex_lock(&static_mutex));
	printf("1: Thread joined. X was %d. Return value (long) was %ld\n",
		global_x, (long)joinval);
	ATF_REQUIRE_EQ(global_x, 20000000);

#if defined(__powerpc__)
	/* XXX force a timeout in ppc case since an un-triggered race
	   otherwise looks like a "failure" */
	/* We sleep for longer than the timeout to make ATF not
	   complain about unexpected success */
	sleep(41);
#endif
}

static void *
mutex4_threadfunc(void *arg)
{
	int *param;

	printf("2: Second thread.\n");

	param = arg;
	printf("2: Locking mutex\n");
	PTHREAD_REQUIRE(pthread_mutex_lock(&mutex));
	printf("2: Got mutex. *param = %d\n", *param);
	(*param)++;

	PTHREAD_REQUIRE(pthread_mutex_unlock(&mutex));

	return param;
}

ATF_TC(mutex4);
ATF_TC_HEAD(mutex4, tc)
{
	atf_tc_set_md_var(tc, "descr", "Checks mutexes");
}
ATF_TC_BODY(mutex4, tc)
{
	int x;
	pthread_t new;
	pthread_mutexattr_t mattr;
	void *joinval;

	printf("1: Mutex-test 4\n");

	PTHREAD_REQUIRE(pthread_mutexattr_init(&mattr));
	PTHREAD_REQUIRE(pthread_mutexattr_settype(&mattr, PTHREAD_MUTEX_RECURSIVE));

	PTHREAD_REQUIRE(pthread_mutex_init(&mutex, &mattr));

	PTHREAD_REQUIRE(pthread_mutexattr_destroy(&mattr));

	x = 1;
	PTHREAD_REQUIRE(pthread_mutex_lock(&mutex));
	PTHREAD_REQUIRE(pthread_create(&new, NULL, mutex4_threadfunc, &x));

	printf("1: Before recursively acquiring the mutex.\n");
	PTHREAD_REQUIRE(pthread_mutex_lock(&mutex));

	printf("1: Before releasing the mutex once.\n");
	sleep(2);
	PTHREAD_REQUIRE(pthread_mutex_unlock(&mutex));
	printf("1: After releasing the mutex once.\n");

	x = 20;

	printf("1: Before releasing the mutex twice.\n");
	sleep(2);
	PTHREAD_REQUIRE(pthread_mutex_unlock(&mutex));
	printf("1: After releasing the mutex twice.\n");

	PTHREAD_REQUIRE(pthread_join(new, &joinval));

	PTHREAD_REQUIRE(pthread_mutex_lock(&mutex));
	printf("1: Thread joined. X was %d. Return value (int) was %d\n",
		x, *(int *)joinval);
	ATF_REQUIRE_EQ(x, 21);
	ATF_REQUIRE_EQ(*(int *)joinval, 21);
	PTHREAD_REQUIRE(pthread_mutex_unlock(&mutex));
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, mutex1);
	ATF_TP_ADD_TC(tp, mutex2);
	ATF_TP_ADD_TC(tp, mutex3);
	ATF_TP_ADD_TC(tp, mutex4);

	return atf_no_error();
}
