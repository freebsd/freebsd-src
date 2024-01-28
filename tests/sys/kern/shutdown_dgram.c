/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Gleb Smirnoff <glebius@FreeBSD.org>
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

#include <sys/socket.h>
#include <sys/un.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <netinet/in.h>

#include <atf-c.h>

/*
 * shutdown(2) on SOCK_DGRAM shall return ENOTCONN per POSIX.  However, there
 * is historical behavior of the shutdown(2) also unblocking any ongoing
 * recv(2) syscall on the socket.  It is known that some programs rely on this
 * behavior, but exact list of programs isn't known.  Neither we know if the
 * "feature" is required on PF_UNIX sockets or on PF_INET/INET6 sockets or
 * on both kinds.  Feel free to improve this comment if you know any details.
 *
 * List of relevant commits, bug reports and reviews:
 * 63649db04205
 * https://reviews.freebsd.org/D10351
 * b114aa79596c (regresses)
 * https://reviews.freebsd.org/D3039 (regresses)
 * kern/84761 c5cff17017f9 aada5cccd878
 */


static void *
blocking_thread(void *arg)
{
	int *s = arg;
	char buf[1];
	int error, rv;

	rv = recv(*s, buf, sizeof(buf), 0);
	error = (rv == -1) ? errno : 0;

	return ((void *)(uintptr_t)error);
}

static void
shutdown_thread(int s)
{
	pthread_t t;
	int rv;

	ATF_REQUIRE(pthread_create(&t, NULL, blocking_thread, &s) == 0);
	usleep(1000);
	ATF_REQUIRE(shutdown(s, SHUT_RD) == -1);
	ATF_REQUIRE(errno == ENOTCONN);
	ATF_REQUIRE(pthread_join(t, (void *)&rv) == 0);
	ATF_REQUIRE(rv == 0);
	close(s);
}

ATF_TC_WITHOUT_HEAD(unblock);
ATF_TC_BODY(unblock, tc)
{
	static const struct sockaddr_un sun = {
		.sun_family = AF_LOCAL,
		.sun_len = sizeof(sun),
		.sun_path = "shutdown-dgram-test-sock",
	};
	int s;

	ATF_REQUIRE((s = socket(PF_UNIX, SOCK_DGRAM, 0)) >= 0);
	ATF_REQUIRE(bind(s, (struct sockaddr *)&sun, sizeof(sun)) == 0);
	shutdown_thread(s);

	static const struct sockaddr_in sin = {
		.sin_family = AF_INET,
		.sin_len = sizeof(sin),
	};
	ATF_REQUIRE((s = socket(PF_INET, SOCK_DGRAM, 0)) >= 0);
	ATF_REQUIRE(bind(s, (struct sockaddr *)&sin, sizeof(sin)) == 0);
	shutdown_thread(s);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, unblock);

	return (atf_no_error());
}
