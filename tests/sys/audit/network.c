/*-
 * Copyright (c) 2018 Aniket Pandey
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
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/types.h>
#include <sys/socket.h>

#include <atf-c.h>
#include <stdarg.h>
#include <unistd.h>

#include "utils.h"

static int sockfd;
static struct pollfd fds[1];
static char extregex[80];
static const char *auclass = "nt";
static const char *failregex = "return,failure : Address family "
			       "not supported by protocol family";

/*
 * Variadic function to close socket descriptors
 */
static void
close_sockets(int count, ...)
{
	int sockd;
	va_list socklist;
	va_start(socklist, count);
	for (sockd = 0; sockd < count; sockd++) {
		close(va_arg(socklist, int));
	}
	va_end(socklist);
}


ATF_TC_WITH_CLEANUP(socket_success);
ATF_TC_HEAD(socket_success, tc)
{
	atf_tc_set_md_var(tc, "descr", "Tests the audit of a successful "
					"socket(2) call");
}

ATF_TC_BODY(socket_success, tc)
{
	FILE *pipefd = setup(fds, auclass);
	ATF_REQUIRE((sockfd = socket(PF_UNIX, SOCK_STREAM, 0)) != -1);
	/* Check the presence of sockfd in audit record */
	snprintf(extregex, sizeof(extregex), "socket.*ret.*success,%d", sockfd);
	check_audit(fds, extregex, pipefd);
	close(sockfd);
}

ATF_TC_CLEANUP(socket_success, tc)
{
	cleanup();
}


ATF_TC_WITH_CLEANUP(socket_failure);
ATF_TC_HEAD(socket_failure, tc)
{
	atf_tc_set_md_var(tc, "descr", "Tests the audit of an unsuccessful "
					"socket(2) call");
}

ATF_TC_BODY(socket_failure, tc)
{
	snprintf(extregex, sizeof(extregex), "socket.*%s", failregex);
	FILE *pipefd = setup(fds, auclass);
	/* Failure reason: Unsupported value of 'domain' argument: 0 */
	ATF_REQUIRE_EQ(-1, socket(0, SOCK_STREAM, 0));
	check_audit(fds, extregex, pipefd);
}

ATF_TC_CLEANUP(socket_failure, tc)
{
	cleanup();
}


ATF_TC_WITH_CLEANUP(socketpair_success);
ATF_TC_HEAD(socketpair_success, tc)
{
	atf_tc_set_md_var(tc, "descr", "Tests the audit of a successful "
					"socketpair(2) call");
}

ATF_TC_BODY(socketpair_success, tc)
{
	int sv[2];
	FILE *pipefd = setup(fds, auclass);
	ATF_REQUIRE_EQ(0, socketpair(PF_UNIX, SOCK_STREAM, 0, sv));

	/* Check for 0x0 (argument 3: default protocol) in the audit record */
	snprintf(extregex, sizeof(extregex), "socketpair.*0x0.*return,success");
	check_audit(fds, extregex, pipefd);
	close_sockets(2, sv[0], sv[1]);
}

ATF_TC_CLEANUP(socketpair_success, tc)
{
	cleanup();
}


ATF_TC_WITH_CLEANUP(socketpair_failure);
ATF_TC_HEAD(socketpair_failure, tc)
{
	atf_tc_set_md_var(tc, "descr", "Tests the audit of an unsuccessful "
					"socketpair(2) call");
}

ATF_TC_BODY(socketpair_failure, tc)
{
	snprintf(extregex, sizeof(extregex), "socketpair.*%s", failregex);
	FILE *pipefd = setup(fds, auclass);
	/* Failure reason: Unsupported value of 'domain' argument: 0 */
	ATF_REQUIRE_EQ(-1, socketpair(0, SOCK_STREAM, 0, NULL));
	check_audit(fds, extregex, pipefd);
}

ATF_TC_CLEANUP(socketpair_failure, tc)
{
	cleanup();
}


ATF_TC_WITH_CLEANUP(setsockopt_success);
ATF_TC_HEAD(setsockopt_success, tc)
{
	atf_tc_set_md_var(tc, "descr", "Tests the audit of a successful "
					"setsockopt(2) call");
}

ATF_TC_BODY(setsockopt_success, tc)
{
	int tr = 1;
	ATF_REQUIRE((sockfd = socket(PF_UNIX, SOCK_STREAM, 0)) != -1);
	/* Check the presence of sockfd in audit record */
	snprintf(extregex, sizeof(extregex),
			"setsockopt.*0x%x.*return,success", sockfd);

	FILE *pipefd = setup(fds, auclass);
	ATF_REQUIRE_EQ(0, setsockopt(sockfd, SOL_SOCKET,
		SO_REUSEADDR, &tr, sizeof(int)));
	check_audit(fds, extregex, pipefd);
	close(sockfd);
}

ATF_TC_CLEANUP(setsockopt_success, tc)
{
	cleanup();
}


ATF_TC_WITH_CLEANUP(setsockopt_failure);
ATF_TC_HEAD(setsockopt_failure, tc)
{
	atf_tc_set_md_var(tc, "descr", "Tests the audit of an unsuccessful "
					"setsockopt(2) call");
}

ATF_TC_BODY(setsockopt_failure, tc)
{
	int tr = 1;
	const char *regex = "setsockopt.*fail.*Socket operation on non-socket";
	FILE *pipefd = setup(fds, auclass);
	/* Failure reason: No socket descriptor with the value of 0 exists */
	ATF_REQUIRE_EQ(-1, setsockopt(0, SOL_SOCKET,
			SO_REUSEADDR, &tr, sizeof(int)));
	check_audit(fds, regex, pipefd);
}

ATF_TC_CLEANUP(setsockopt_failure, tc)
{
	cleanup();
}


ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, socket_success);
	ATF_TP_ADD_TC(tp, socket_failure);
	ATF_TP_ADD_TC(tp, socketpair_success);
	ATF_TP_ADD_TC(tp, socketpair_failure);
	ATF_TP_ADD_TC(tp, setsockopt_success);
	ATF_TP_ADD_TC(tp, setsockopt_failure);

	return (atf_no_error());
}
