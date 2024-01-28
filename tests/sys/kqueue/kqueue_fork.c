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
#include <sys/wait.h>

#include <err.h>
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

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, shared_table_filt_sig);

	return (atf_no_error());
}
