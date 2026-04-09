/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Andreas Bock <andreas.bock@virtual-arts-software.de>
 * Copyright (c) 2023 Mark Johnston <markj@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the distribution.
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

#include <sys/event.h>
#include <sys/procdesc.h>
#include <sys/stat.h>
#include <sys/user.h>
#include <sys/wait.h>

#include <err.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

#include <atf-c.h>

/*
 * A regression test for bugzilla 275286.
 */
ATF_TC_WITHOUT_HEAD(shared_table_filt_sig);
ATF_TC_BODY(shared_table_filt_sig, tc)
{
	struct sigaction sa;
	pid_t pid;
	int error, status;

	sa.sa_handler = SIG_IGN;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	error = sigaction(SIGINT, &sa, NULL);
	ATF_REQUIRE(error == 0);

	pid = rfork(RFPROC);
	ATF_REQUIRE(pid != -1);
	if (pid == 0) {
		struct kevent ev;
		int kq;

		kq = kqueue();
		if (kq < 0)
			err(1, "kqueue");
		EV_SET(&ev, SIGINT, EVFILT_SIGNAL, EV_ADD | EV_ENABLE, 0, 0,
		    NULL);
		if (kevent(kq, &ev, 1, NULL, 0, NULL) < 0)
			err(2, "kevent");
		if (kevent(kq, NULL, 0, &ev, 1, NULL) < 0)
			err(3, "kevent");
		_exit(0);
	}

	/* Wait for the child to block in kevent(). */
	usleep(100000);

	error = kill(pid, SIGINT);
	ATF_REQUIRE(error == 0);

	error = waitpid(pid, &status, 0);
	ATF_REQUIRE(error != -1);
	ATF_REQUIRE(WIFEXITED(status));
	ATF_REQUIRE_EQ(WEXITSTATUS(status), 0);
}

#define	TIMER_FORKED	0
#define	TIMER_TIMEOUT	1

#define	RECV_TIMER	0x01
#define	RECV_VNODE	0x02
#define	RECV_CLOREAD	0x04
#define	RECV_ERROR	0x80
#define	RECV_ALL	(RECV_TIMER | RECV_VNODE)

static int
cponfork_notes_check(int kq, int clofd)
{
	struct kevent ev;
	int error, received = 0;

	EV_SET(&ev, TIMER_TIMEOUT, EVFILT_TIMER,
	    EV_ADD | EV_ENABLE | EV_ONESHOT, NOTE_SECONDS, 4, NULL);
	error = kevent(kq, &ev, 1, NULL, 0, NULL);
	if (error == -1)
		return (RECV_ERROR);

	while ((received & RECV_ALL) != RECV_ALL) {
		error = kevent(kq, NULL, 0, &ev, 1, NULL);
		if (error < 0)
			return (RECV_ERROR);
		else if (error == 0)
			break;

		switch (ev.filter) {
		case EVFILT_TIMER:
			if (ev.ident == TIMER_TIMEOUT)
				return (received | RECV_ERROR);

			received |= RECV_TIMER;
			break;
		case EVFILT_VNODE:
			received |= RECV_VNODE;
			break;
		case EVFILT_READ:
			if ((int)ev.ident != clofd)
				return (received | RECV_ERROR);
			received |= RECV_CLOREAD;
			break;
		}
	}

	return (received);
}

ATF_TC_WITHOUT_HEAD(cponfork_notes);
ATF_TC_BODY(cponfork_notes, tc)
{
	struct kevent ev[3];
	int clofd, dfd, error, kq, pdfd, pmask, status;
	pid_t pid;

	kq = kqueuex(KQUEUE_CPONFORK);
	ATF_REQUIRE(kq >= 0);

	dfd = open(".", O_DIRECTORY);
	ATF_REQUIRE(dfd >= 0);

	clofd = kqueue();
	ATF_REQUIRE(clofd >= 0);

	/*
	 * Setup an event on clofd that we can trigger to make it readable,
	 * as we'll want this ready to go when we fork to be sure that if we
	 * *were* going to receive an event from it, it would have occurred
	 * before the three-second timer that would normally close out the child
	 * fires.
	 */
	EV_SET(&ev[0], 0, EVFILT_USER, EV_ADD | EV_ENABLE, 0, 0, NULL);
	error = kevent(clofd, &ev[0], 1, NULL, 0, NULL);
	ATF_REQUIRE(error != -1);

	/*
	 * Every event we setup here we should expect to observe in both the
	 * child and the parent, with exception to the EVFILT_READ of clofd.  We
	 * except that one to be dropped in the child when the kqueue it's
	 * attached to goes away, thus its exclusion from the RECV_ALL mask.
	 */
	EV_SET(&ev[0], dfd, EVFILT_VNODE, EV_ADD | EV_ENABLE | EV_ONESHOT,
	    NOTE_WRITE, 0, NULL);
	EV_SET(&ev[1], TIMER_FORKED, EVFILT_TIMER, EV_ADD | EV_ENABLE | EV_ONESHOT,
	    NOTE_SECONDS, 3, NULL);
	EV_SET(&ev[2], clofd, EVFILT_READ, EV_ADD | EV_ENABLE | EV_ONESHOT, 0,
	    0, NULL);
	error = kevent(kq, &ev[0], 3, NULL, 0, NULL);
	ATF_REQUIRE(error != -1);

	/* Fire off an event to make clofd readable. */
	EV_SET(&ev[0], 0, EVFILT_USER, 0, NOTE_TRIGGER, 0, NULL);
	error = kevent(clofd, &ev[0], 1, NULL, 0, NULL);

	/*
	 * We're only using pdfork here for the kill-on-exit semantics, in case
	 * the parent fails to setup some context needed for one of our events
	 * to fire.
	 */
	pid = pdfork(&pdfd, 0);
	ATF_REQUIRE(pid != -1);
	if (pid == 0) {
		struct kinfo_file kf = { .kf_structsize = sizeof(kf) };

		if (fcntl(kq, F_KINFO, &kf) != 0)
			_exit(RECV_ERROR);
		else if (kf.kf_type != KF_TYPE_KQUEUE)
			_exit(RECV_ERROR);

		_exit(cponfork_notes_check(kq, clofd));
	}

	/* Setup anything we need to fire off any of our events above. */
	error = mkdir("canary", 0755);
	ATF_REQUIRE(error == 0);

	/*
	 * We'll simultaneously do the same exercise of polling the kqueue in
	 * the parent, to demonstrate that forking doesn't "steal" any of the
	 * knotes from us -- all of the events we've added are one-shot and
	 * still fire twice (once in parent, once in child).
	 */
	pmask = cponfork_notes_check(kq, clofd);
	ATF_REQUIRE_EQ(pmask, RECV_ALL | RECV_CLOREAD);

	/* Wait for the child to timeout or observe the timer. */
	_Static_assert(RECV_ALL <= UCHAR_MAX,
	    "Too many events to observe -- switch from waitpid -> waitid");
	error = waitpid(pid, &status, 0);
	ATF_REQUIRE(error != -1);
	ATF_REQUIRE(WIFEXITED(status));
	ATF_REQUIRE_EQ(WEXITSTATUS(status), RECV_ALL);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, shared_table_filt_sig);
	ATF_TP_ADD_TC(tp, cponfork_notes);

	return (atf_no_error());
}
