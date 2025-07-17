/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright 2021 Jan Kokemüller
 * Copyright 2021 Alex Richardson <arichardson@FreeBSD.org>
 *
 * This work was supported by Innovate UK project 105694, "Digital Security by
 * Design (DSbD) Technology Platform Prototype".
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
#include <sys/cdefs.h>
#include <sys/event.h>

#include <atf-c.h>
#include <errno.h>
#include <signal.h>

ATF_TC_WITHOUT_HEAD(main);

ATF_TC_BODY(main, tc)
{
	int rv;

	sigset_t set;
	rv = sigemptyset(&set);
	ATF_REQUIRE_EQ(0, rv);
	rv = sigaddset(&set, SIGUSR1);
	ATF_REQUIRE_EQ(0, rv);
	rv = sigprocmask(SIG_BLOCK, &set, NULL);
	ATF_REQUIRE_EQ(0, rv);

	int skq = kqueue();
	ATF_REQUIRE(skq >= 0);

	struct kevent kev;
	EV_SET(&kev, SIGUSR1, EVFILT_SIGNAL, EV_ADD, 0, 0, 0);
	rv = kevent(skq, &kev, 1, NULL, 0, NULL);
	ATF_REQUIRE_EQ(0, rv);

	int kq = kqueue();
	ATF_REQUIRE(kq >= 0);

	EV_SET(&kev, skq, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, 0);
	rv = kevent(kq, &kev, 1, NULL, 0, NULL);
	ATF_REQUIRE_EQ(0, rv);

	/*
	 * It was previously not guaranteed that sending a signal to self would
	 * be immediately visible in the nested kqueue activation with a zero
	 * timeout. As of https://reviews.freebsd.org/D31858, the kqueue task
	 * queue will be processed in this case, so we are guaranteed to see the
	 * SIGUSR1 here even with a zero timeout. We run the code below in a
	 * loop to make it more likely that older kernels without the fix fail
	 * this test.
	 */
	for (int i = 0; i < 100; i++) {
		rv = kill(getpid(), SIGUSR1);
		ATF_REQUIRE_EQ(0, rv);

		rv = kevent(kq, NULL, 0, &kev, 1, &(struct timespec) { 0, 0 });
		ATF_REQUIRE_EQ_MSG(1, rv,
		    "Unexpected result %d from kevent() after %d iterations",
		    rv, i);
		rv = kevent(kq, NULL, 0, &kev, 1, &(struct timespec) { 0, 0 });
		ATF_REQUIRE_EQ(0, rv);

		rv = kevent(skq, NULL, 0, &kev, 1, &(struct timespec) { 0, 0 });
		ATF_REQUIRE_EQ(1, rv);
		rv = kevent(skq, NULL, 0, &kev, 1, &(struct timespec) { 0, 0 });
		ATF_REQUIRE_EQ(0, rv);

		siginfo_t siginfo;
		rv = sigtimedwait(&set, &siginfo, &(struct timespec) { 0, 0 });
		ATF_REQUIRE_EQ(SIGUSR1, rv);

		rv = sigtimedwait(&set, &siginfo, &(struct timespec) { 0, 0 });
		ATF_REQUIRE_ERRNO(EAGAIN, rv < 0);
	}
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, main);

	return (atf_no_error());
}
