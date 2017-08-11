/*-
 * Copyright (c) 2017 Spectra Logic Corp
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
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/event.h>

#include <aio.h>
#include <semaphore.h>

#include <atf-c.h>

#include "freebsd_test_suite/macros.h"

static sem_t completions; 


static void
handler(int sig __unused)
{
	ATF_REQUIRE_EQ(0, sem_post(&completions));
}

static void
thr_handler(union sigval sv __unused)
{
	ATF_REQUIRE_EQ(0, sem_post(&completions));
}

/* With LIO_WAIT, an empty lio_listio should return immediately */
ATF_TC_WITHOUT_HEAD(lio_listio_empty_wait);
ATF_TC_BODY(lio_listio_empty_wait, tc)
{
	struct aiocb *list = NULL;

	ATF_REQUIRE_EQ(0, lio_listio(LIO_WAIT, &list, 0, NULL));
}

/*
 * With LIO_NOWAIT, an empty lio_listio should send completion notification
 * immediately
 */
ATF_TC_WITHOUT_HEAD(lio_listio_empty_nowait_kevent);
ATF_TC_BODY(lio_listio_empty_nowait_kevent, tc)
{
	struct aiocb *list = NULL;
	struct sigevent sev;
	struct kevent kq_returned;
	int kq, result;
	void *udata = (void*)0xdeadbeefdeadbeef;

	atf_tc_expect_timeout("Bug 220398 - lio_listio(2) never sends"
			" asynchronous notification if nent==0");
	kq = kqueue();
	ATF_REQUIRE(kq > 0);
	sev.sigev_notify = SIGEV_KEVENT;
	sev.sigev_notify_kqueue = kq;
	sev.sigev_value.sival_ptr = udata;
	ATF_REQUIRE_EQ(0, lio_listio(LIO_NOWAIT, &list, 0, &sev));
	result = kevent(kq, NULL, 0, &kq_returned, 1, NULL);
	ATF_REQUIRE_MSG(result == 1, "Never got completion notification");
	ATF_REQUIRE_EQ((uintptr_t)list, kq_returned.ident);
	ATF_REQUIRE_EQ(EVFILT_LIO, kq_returned.filter);
	ATF_REQUIRE_EQ(udata, kq_returned.udata);
}

/*
 * With LIO_NOWAIT, an empty lio_listio should send completion notification
 * immediately
 */
ATF_TC_WITHOUT_HEAD(lio_listio_empty_nowait_signal);
ATF_TC_BODY(lio_listio_empty_nowait_signal, tc)
{
	struct aiocb *list = NULL;
	struct sigevent sev;

	atf_tc_expect_timeout("Bug 220398 - lio_listio(2) never sends"
	    " asynchronous notification if nent==0");
	ATF_REQUIRE_EQ(0, sem_init(&completions, false, 0));
	sev.sigev_notify = SIGEV_SIGNAL;
	sev.sigev_signo = SIGUSR1;
	ATF_REQUIRE(SIG_ERR != signal(SIGUSR1, handler));
	ATF_REQUIRE_EQ(0, lio_listio(LIO_NOWAIT, &list, 0, &sev));
	ATF_REQUIRE_EQ(0, sem_wait(&completions));
	ATF_REQUIRE_EQ(0, sem_destroy(&completions));
}

/*
 * With LIO_NOWAIT, an empty lio_listio should send completion notification
 * immediately
 */
ATF_TC_WITHOUT_HEAD(lio_listio_empty_nowait_thread);
ATF_TC_BODY(lio_listio_empty_nowait_thread, tc)
{
	struct aiocb *list = NULL;
	struct sigevent sev;

	atf_tc_expect_timeout("Bug 220398 - lio_listio(2) never sends"
	    " asynchronous notification if nent==0");
	ATF_REQUIRE_EQ(0, sem_init(&completions, false, 0));
	bzero(&sev, sizeof(sev));
	sev.sigev_notify = SIGEV_THREAD;
	sev.sigev_notify_function = thr_handler;
	sev.sigev_notify_attributes = NULL;
	ATF_REQUIRE_MSG(0 == lio_listio(LIO_NOWAIT, &list, 0, &sev),
	    "lio_listio: %s", strerror(errno));
	ATF_REQUIRE_EQ(0, sem_wait(&completions));
	ATF_REQUIRE_EQ(0, sem_destroy(&completions));
}


ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, lio_listio_empty_nowait_kevent);
	ATF_TP_ADD_TC(tp, lio_listio_empty_nowait_signal);
	ATF_TP_ADD_TC(tp, lio_listio_empty_nowait_thread);
	ATF_TP_ADD_TC(tp, lio_listio_empty_wait);

	return (atf_no_error());
}
