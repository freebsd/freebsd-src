/*
 * Copyright (c) 2015-2018, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *  * Neither the name of Intel Corporation nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PTUNIT_THREADS_H
#define PTUNIT_THREADS_H

#include "ptunit.h"

#if defined(FEATURE_THREADS)
#  include <threads.h>
#endif /* defined(FEATURE_THREADS) */


/* The maximal number of threads. */
enum {
	ptu_thrd_max	= 16
};

/* A test fixture component providing threading support. */
struct ptunit_thrd_fixture {
#if defined(FEATURE_THREADS)

	/* An array of threads created by ptunit_thrd_create(). */
	thrd_t threads[ptu_thrd_max];

	/* A lock protecting the outer fixture.  We don't need it. */
	mtx_t lock;

#endif /* defined(FEATURE_THREADS) */

	/* The actual number of created threads. */
	uint8_t nthreads;

	/* The result of joined threads. */
	int result[ptu_thrd_max];
};


static inline struct ptunit_result
ptunit_thrd_init(struct ptunit_thrd_fixture *tfix)
{
	ptu_ptr(tfix);

	memset(tfix, 0, sizeof(*tfix));

#if defined(FEATURE_THREADS)
	{
		int errcode;

		errcode = mtx_init(&tfix->lock, mtx_plain);
		ptu_int_eq(errcode, thrd_success);
	}
#endif /* defined(FEATURE_THREADS) */

	return ptu_passed();
}

static inline struct ptunit_result
ptunit_thrd_fini(struct ptunit_thrd_fixture *tfix)
{
	ptu_ptr(tfix);

#if defined(FEATURE_THREADS)
	{
		int thrd, errcode[ptu_thrd_max];

		for (thrd = 0; thrd < tfix->nthreads; ++thrd)
			errcode[thrd] = thrd_join(&tfix->threads[thrd],
						  &tfix->result[thrd]);

		mtx_destroy(&tfix->lock);

		for (thrd = 0; thrd < tfix->nthreads; ++thrd)
			ptu_int_eq(errcode[thrd], thrd_success);
	}
#endif /* defined(FEATURE_THREADS) */

	return ptu_passed();
}

#if defined(FEATURE_THREADS)

static inline struct ptunit_result
ptunit_thrd_create(struct ptunit_thrd_fixture *tfix, int (*worker)(void *),
		   void *arg)
{
	int errcode;

	ptu_ptr(tfix);

	errcode = thrd_create(&tfix->threads[tfix->nthreads++], worker, arg);
	ptu_int_eq(errcode, thrd_success);

	return ptu_passed();
}

#endif /* defined(FEATURE_THREADS) */

static inline struct ptunit_result
ptunit_thrd_lock(struct ptunit_thrd_fixture *tfix)
{
	ptu_ptr(tfix);

#if defined(FEATURE_THREADS)
	{
		int errcode;

		errcode = mtx_lock(&tfix->lock);
		ptu_int_eq(errcode, thrd_success);
	}
#endif /* defined(FEATURE_THREADS) */

	return ptu_passed();
}

static inline struct ptunit_result
ptunit_thrd_unlock(struct ptunit_thrd_fixture *tfix)
{
	ptu_ptr(tfix);

#if defined(FEATURE_THREADS)
	{
		int errcode;

		errcode = mtx_unlock(&tfix->lock);
		ptu_int_eq(errcode, thrd_success);
	}
#endif /* defined(FEATURE_THREADS) */

	return ptu_passed();
}

#endif /* PTUNIT_THREADS_H */
