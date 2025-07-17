/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Kyle Evans <kevans@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/umtx.h>

#include <pthread.h>

#include <atf-c.h>

/*
 * This is an implementation detail of _umtx_op(2), pulled from
 * sys/kern/kern_umtx.c.  The relevant bug observed that requests above the
 * batch size would not function as intended, so it's important that this
 * reflects the BATCH_SIZE configured there.
 */
#define	UMTX_OP_BATCH_SIZE	128
#define THREAD_COUNT		((UMTX_OP_BATCH_SIZE * 3) / 2)

static pthread_mutex_t static_mutex = PTHREAD_MUTEX_INITIALIZER;

static int batched_waiting;

static void *
batching_threadfunc(void *arg)
{

	pthread_mutex_lock(&static_mutex);
	++batched_waiting;
	pthread_mutex_unlock(&static_mutex);
	_umtx_op(arg, UMTX_OP_WAIT_UINT_PRIVATE, 0, NULL, NULL);

	return (NULL);
}

ATF_TC(batching);
ATF_TC_HEAD(batching, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Checks batching of UMTX_OP_NWAKE_PRIVATE");
}
ATF_TC_BODY(batching, tc)
{
	uintptr_t addrs[THREAD_COUNT];
	uint32_t vals[THREAD_COUNT];
	pthread_t threads[THREAD_COUNT];

	for (int i = 0; i < THREAD_COUNT; i++) {
		addrs[i] = (uintptr_t)&vals[i];
		vals[i] = 0;
		pthread_create(&threads[i], NULL, batching_threadfunc,
		    &vals[i]);
	}

	pthread_mutex_lock(&static_mutex);
	while (batched_waiting != THREAD_COUNT) {
		pthread_mutex_unlock(&static_mutex);
		pthread_yield();
		pthread_mutex_lock(&static_mutex);
	}

	/*
	 * Spin for another .50 seconds to make sure they're all safely in the
	 * kernel.
	 */
	usleep(500000);

	pthread_mutex_unlock(&static_mutex);
	_umtx_op(addrs, UMTX_OP_NWAKE_PRIVATE, THREAD_COUNT, NULL, NULL);

	for (int i = 0; i < THREAD_COUNT; i++) {
		ATF_REQUIRE_EQ(0, pthread_join(threads[i], NULL));
	}
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, batching);
	return (atf_no_error());
}
