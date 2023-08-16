/*-
 * Copyright (c) 2013 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Pawel Jakub Dawidek under sponsorship from
 * the FreeBSD Foundation.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/wait.h>
#include <sys/nv.h>

#include <stdlib.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <atf-c.h>

#define	ALPHABET	"abcdefghijklmnopqrstuvwxyz"
#define	fd_is_valid(fd)	(fcntl((fd), F_GETFL) != -1 || errno != EBADF)

static void
send_nvlist_child(int sock)
{
	nvlist_t *nvl;
	nvlist_t *empty;
	int pfd[2];

	nvl = nvlist_create(0);
	empty = nvlist_create(0);

	nvlist_add_bool(nvl, "nvlist/bool/true", true);
	nvlist_add_bool(nvl, "nvlist/bool/false", false);
	nvlist_add_number(nvl, "nvlist/number/0", 0);
	nvlist_add_number(nvl, "nvlist/number/1", 1);
	nvlist_add_number(nvl, "nvlist/number/-1", -1);
	nvlist_add_number(nvl, "nvlist/number/UINT64_MAX", UINT64_MAX);
	nvlist_add_number(nvl, "nvlist/number/INT64_MIN", INT64_MIN);
	nvlist_add_number(nvl, "nvlist/number/INT64_MAX", INT64_MAX);
	nvlist_add_string(nvl, "nvlist/string/", "");
	nvlist_add_string(nvl, "nvlist/string/x", "x");
	nvlist_add_string(nvl, "nvlist/string/" ALPHABET, ALPHABET);

	nvlist_add_descriptor(nvl, "nvlist/descriptor/STDERR_FILENO",
	    STDERR_FILENO);
	if (pipe(pfd) == -1)
		err(EXIT_FAILURE, "pipe");
	if (write(pfd[1], "test", 4) != 4)
		err(EXIT_FAILURE, "write");
	close(pfd[1]);
	nvlist_add_descriptor(nvl, "nvlist/descriptor/pipe_rd", pfd[0]);
	close(pfd[0]);

	nvlist_add_binary(nvl, "nvlist/binary/x", "x", 1);
	nvlist_add_binary(nvl, "nvlist/binary/" ALPHABET, ALPHABET,
	    sizeof(ALPHABET));
	nvlist_move_nvlist(nvl, "nvlist/nvlist/empty", empty);
	nvlist_add_nvlist(nvl, "nvlist/nvlist", nvl);

	nvlist_send(sock, nvl);

	nvlist_destroy(nvl);
}

static void
send_nvlist_parent(int sock)
{
	nvlist_t *nvl;
	const nvlist_t *cnvl, *empty;
	const char *name, *cname;
	void *cookie, *ccookie;
	int type, ctype, fd;
	size_t size;
	char buf[4];

	nvl = nvlist_recv(sock, 0);
	ATF_REQUIRE(nvlist_error(nvl) == 0);
	if (nvlist_error(nvl) != 0)
		err(1, "nvlist_recv() failed");

	cookie = NULL;

	name = nvlist_next(nvl, &type, &cookie);
	ATF_REQUIRE(name != NULL);
	ATF_REQUIRE(type == NV_TYPE_BOOL);
	ATF_REQUIRE(strcmp(name, "nvlist/bool/true") == 0);
	ATF_REQUIRE(nvlist_get_bool(nvl, name) == true);

	name = nvlist_next(nvl, &type, &cookie);
	ATF_REQUIRE(name != NULL);
	ATF_REQUIRE(type == NV_TYPE_BOOL);
	ATF_REQUIRE(strcmp(name, "nvlist/bool/false") == 0);
	ATF_REQUIRE(nvlist_get_bool(nvl, name) == false);

	name = nvlist_next(nvl, &type, &cookie);
	ATF_REQUIRE(name != NULL);
	ATF_REQUIRE(type == NV_TYPE_NUMBER);
	ATF_REQUIRE(strcmp(name, "nvlist/number/0") == 0);
	ATF_REQUIRE(nvlist_get_number(nvl, name) == 0);

	name = nvlist_next(nvl, &type, &cookie);
	ATF_REQUIRE(name != NULL);
	ATF_REQUIRE(type == NV_TYPE_NUMBER);
	ATF_REQUIRE(strcmp(name, "nvlist/number/1") == 0);
	ATF_REQUIRE(nvlist_get_number(nvl, name) == 1);

	name = nvlist_next(nvl, &type, &cookie);
	ATF_REQUIRE(name != NULL);
	ATF_REQUIRE(type == NV_TYPE_NUMBER);
	ATF_REQUIRE(strcmp(name, "nvlist/number/-1") == 0);
	ATF_REQUIRE((int)nvlist_get_number(nvl, name) == -1);

	name = nvlist_next(nvl, &type, &cookie);
	ATF_REQUIRE(name != NULL);
	ATF_REQUIRE(type == NV_TYPE_NUMBER);
	ATF_REQUIRE(strcmp(name, "nvlist/number/UINT64_MAX") == 0);
	ATF_REQUIRE(nvlist_get_number(nvl, name) == UINT64_MAX);

	name = nvlist_next(nvl, &type, &cookie);
	ATF_REQUIRE(name != NULL);
	ATF_REQUIRE(type == NV_TYPE_NUMBER);
	ATF_REQUIRE(strcmp(name, "nvlist/number/INT64_MIN") == 0);
	ATF_REQUIRE((int64_t)nvlist_get_number(nvl, name) == INT64_MIN);

	name = nvlist_next(nvl, &type, &cookie);
	ATF_REQUIRE(name != NULL);
	ATF_REQUIRE(type == NV_TYPE_NUMBER);
	ATF_REQUIRE(strcmp(name, "nvlist/number/INT64_MAX") == 0);
	ATF_REQUIRE((int64_t)nvlist_get_number(nvl, name) == INT64_MAX);

	name = nvlist_next(nvl, &type, &cookie);
	ATF_REQUIRE(name != NULL);
	ATF_REQUIRE(type == NV_TYPE_STRING);
	ATF_REQUIRE(strcmp(name, "nvlist/string/") == 0);
	ATF_REQUIRE(strcmp(nvlist_get_string(nvl, name), "") == 0);

	name = nvlist_next(nvl, &type, &cookie);
	ATF_REQUIRE(name != NULL);
	ATF_REQUIRE(type == NV_TYPE_STRING);
	ATF_REQUIRE(strcmp(name, "nvlist/string/x") == 0);
	ATF_REQUIRE(strcmp(nvlist_get_string(nvl, name), "x") == 0);

	name = nvlist_next(nvl, &type, &cookie);
	ATF_REQUIRE(name != NULL);
	ATF_REQUIRE(type == NV_TYPE_STRING);
	ATF_REQUIRE(strcmp(name, "nvlist/string/" ALPHABET) == 0);
	ATF_REQUIRE(strcmp(nvlist_get_string(nvl, name), ALPHABET) == 0);

	name = nvlist_next(nvl, &type, &cookie);
	ATF_REQUIRE(name != NULL);
	ATF_REQUIRE(type == NV_TYPE_DESCRIPTOR);
	ATF_REQUIRE(strcmp(name, "nvlist/descriptor/STDERR_FILENO") == 0);
	ATF_REQUIRE(fd_is_valid(nvlist_get_descriptor(nvl, name)));

	name = nvlist_next(nvl, &type, &cookie);
	ATF_REQUIRE(name != NULL);
	ATF_REQUIRE(type == NV_TYPE_DESCRIPTOR);
	ATF_REQUIRE(strcmp(name, "nvlist/descriptor/pipe_rd") == 0);
	fd = nvlist_get_descriptor(nvl, name);
	ATF_REQUIRE(fd_is_valid(fd));
	ATF_REQUIRE(read(fd, buf, sizeof(buf)) == 4);
	ATF_REQUIRE(strncmp(buf, "test", sizeof(buf)) == 0);

	name = nvlist_next(nvl, &type, &cookie);
	ATF_REQUIRE(name != NULL);
	ATF_REQUIRE(type == NV_TYPE_BINARY);
	ATF_REQUIRE(strcmp(name, "nvlist/binary/x") == 0);
	ATF_REQUIRE(memcmp(nvlist_get_binary(nvl, name, NULL), "x", 1) == 0);
	ATF_REQUIRE(memcmp(nvlist_get_binary(nvl, name, &size), "x", 1) == 0);
	ATF_REQUIRE(size == 1);

	name = nvlist_next(nvl, &type, &cookie);
	ATF_REQUIRE(name != NULL);
	ATF_REQUIRE(type == NV_TYPE_BINARY);
	ATF_REQUIRE(strcmp(name, "nvlist/binary/" ALPHABET) == 0);
	ATF_REQUIRE(memcmp(nvlist_get_binary(nvl, name, NULL), ALPHABET,
	    sizeof(ALPHABET)) == 0);
	ATF_REQUIRE(memcmp(nvlist_get_binary(nvl, name, &size), ALPHABET,
	    sizeof(ALPHABET)) == 0);
	ATF_REQUIRE(size == sizeof(ALPHABET));

	name = nvlist_next(nvl, &type, &cookie);
	ATF_REQUIRE(name != NULL);
	ATF_REQUIRE(type == NV_TYPE_NVLIST);
	ATF_REQUIRE(strcmp(name, "nvlist/nvlist/empty") == 0);
	cnvl = nvlist_get_nvlist(nvl, name);
	ATF_REQUIRE(nvlist_empty(cnvl));

	name = nvlist_next(nvl, &type, &cookie);
	ATF_REQUIRE(name != NULL);
	ATF_REQUIRE(type == NV_TYPE_NVLIST);
	ATF_REQUIRE(strcmp(name, "nvlist/nvlist") == 0);
	cnvl = nvlist_get_nvlist(nvl, name);

	ccookie = NULL;

	cname = nvlist_next(cnvl, &ctype, &ccookie);
	ATF_REQUIRE(cname != NULL);
	ATF_REQUIRE(ctype == NV_TYPE_BOOL);
	ATF_REQUIRE(strcmp(cname, "nvlist/bool/true") == 0);
	ATF_REQUIRE(nvlist_get_bool(cnvl, cname) == true);

	cname = nvlist_next(cnvl, &ctype, &ccookie);
	ATF_REQUIRE(cname != NULL);
	ATF_REQUIRE(ctype == NV_TYPE_BOOL);
	ATF_REQUIRE(strcmp(cname, "nvlist/bool/false") == 0);
	ATF_REQUIRE(nvlist_get_bool(cnvl, cname) == false);

	cname = nvlist_next(cnvl, &ctype, &ccookie);
	ATF_REQUIRE(cname != NULL);
	ATF_REQUIRE(ctype == NV_TYPE_NUMBER);
	ATF_REQUIRE(strcmp(cname, "nvlist/number/0") == 0);
	ATF_REQUIRE(nvlist_get_number(cnvl, cname) == 0);

	cname = nvlist_next(cnvl, &ctype, &ccookie);
	ATF_REQUIRE(cname != NULL);
	ATF_REQUIRE(ctype == NV_TYPE_NUMBER);
	ATF_REQUIRE(strcmp(cname, "nvlist/number/1") == 0);
	ATF_REQUIRE(nvlist_get_number(cnvl, cname) == 1);

	cname = nvlist_next(cnvl, &ctype, &ccookie);
	ATF_REQUIRE(cname != NULL);
	ATF_REQUIRE(ctype == NV_TYPE_NUMBER);
	ATF_REQUIRE(strcmp(cname, "nvlist/number/-1") == 0);
	ATF_REQUIRE((int)nvlist_get_number(cnvl, cname) == -1);

	cname = nvlist_next(cnvl, &ctype, &ccookie);
	ATF_REQUIRE(cname != NULL);
	ATF_REQUIRE(ctype == NV_TYPE_NUMBER);
	ATF_REQUIRE(strcmp(cname, "nvlist/number/UINT64_MAX") == 0);
	ATF_REQUIRE(nvlist_get_number(cnvl, cname) == UINT64_MAX);

	cname = nvlist_next(cnvl, &ctype, &ccookie);
	ATF_REQUIRE(cname != NULL);
	ATF_REQUIRE(ctype == NV_TYPE_NUMBER);
	ATF_REQUIRE(strcmp(cname, "nvlist/number/INT64_MIN") == 0);
	ATF_REQUIRE((int64_t)nvlist_get_number(cnvl, cname) == INT64_MIN);

	cname = nvlist_next(cnvl, &ctype, &ccookie);
	ATF_REQUIRE(cname != NULL);
	ATF_REQUIRE(ctype == NV_TYPE_NUMBER);
	ATF_REQUIRE(strcmp(cname, "nvlist/number/INT64_MAX") == 0);
	ATF_REQUIRE((int64_t)nvlist_get_number(cnvl, cname) == INT64_MAX);

	cname = nvlist_next(cnvl, &ctype, &ccookie);
	ATF_REQUIRE(cname != NULL);
	ATF_REQUIRE(ctype == NV_TYPE_STRING);
	ATF_REQUIRE(strcmp(cname, "nvlist/string/") == 0);
	ATF_REQUIRE(strcmp(nvlist_get_string(cnvl, cname), "") == 0);

	cname = nvlist_next(cnvl, &ctype, &ccookie);
	ATF_REQUIRE(cname != NULL);
	ATF_REQUIRE(ctype == NV_TYPE_STRING);
	ATF_REQUIRE(strcmp(cname, "nvlist/string/x") == 0);
	ATF_REQUIRE(strcmp(nvlist_get_string(cnvl, cname), "x") == 0);

	cname = nvlist_next(cnvl, &ctype, &ccookie);
	ATF_REQUIRE(cname != NULL);
	ATF_REQUIRE(ctype == NV_TYPE_STRING);
	ATF_REQUIRE(strcmp(cname, "nvlist/string/" ALPHABET) == 0);
	ATF_REQUIRE(strcmp(nvlist_get_string(cnvl, cname), ALPHABET) == 0);

	cname = nvlist_next(cnvl, &ctype, &ccookie);
	ATF_REQUIRE(cname != NULL);
	ATF_REQUIRE(ctype == NV_TYPE_DESCRIPTOR);
	ATF_REQUIRE(strcmp(cname, "nvlist/descriptor/STDERR_FILENO") == 0);
	ATF_REQUIRE(fd_is_valid(nvlist_get_descriptor(cnvl, cname)));

	cname = nvlist_next(cnvl, &ctype, &ccookie);
	ATF_REQUIRE(cname != NULL);
	ATF_REQUIRE(ctype == NV_TYPE_DESCRIPTOR);
	ATF_REQUIRE(strcmp(cname, "nvlist/descriptor/pipe_rd") == 0);
	ATF_REQUIRE(fd_is_valid(nvlist_get_descriptor(cnvl, cname)));

	cname = nvlist_next(cnvl, &ctype, &ccookie);
	ATF_REQUIRE(cname != NULL);
	ATF_REQUIRE(ctype == NV_TYPE_BINARY);
	ATF_REQUIRE(strcmp(cname, "nvlist/binary/x") == 0);
	ATF_REQUIRE(memcmp(nvlist_get_binary(cnvl, cname, NULL), "x", 1) == 0);
	ATF_REQUIRE(memcmp(nvlist_get_binary(cnvl, cname, &size), "x", 1) == 0);
	ATF_REQUIRE(size == 1);

	cname = nvlist_next(cnvl, &ctype, &ccookie);
	ATF_REQUIRE(cname != NULL);
	ATF_REQUIRE(ctype == NV_TYPE_BINARY);
	ATF_REQUIRE(strcmp(cname, "nvlist/binary/" ALPHABET) == 0);
	ATF_REQUIRE(memcmp(nvlist_get_binary(cnvl, cname, NULL), ALPHABET,
	    sizeof(ALPHABET)) == 0);
	ATF_REQUIRE(memcmp(nvlist_get_binary(cnvl, cname, &size), ALPHABET,
	    sizeof(ALPHABET)) == 0);
	ATF_REQUIRE(size == sizeof(ALPHABET));

	cname = nvlist_next(cnvl, &ctype, &ccookie);
	ATF_REQUIRE(cname != NULL);
	ATF_REQUIRE(ctype == NV_TYPE_NVLIST);
	ATF_REQUIRE(strcmp(cname, "nvlist/nvlist/empty") == 0);
	empty = nvlist_get_nvlist(cnvl, cname);
	ATF_REQUIRE(nvlist_empty(empty));

	cname = nvlist_next(cnvl, &ctype, &ccookie);
	ATF_REQUIRE(cname == NULL);

	name = nvlist_next(nvl, &type, &cookie);
	ATF_REQUIRE(name == NULL);

	nvlist_destroy(nvl);
}

static void
nvlist_send_recv__send_nvlist(short sotype)
{
	int socks[2], status;
	pid_t pid;

	ATF_REQUIRE(socketpair(PF_UNIX, sotype, 0, socks) == 0);

	pid = fork();
	ATF_REQUIRE(pid >= 0);
	if (pid == 0) {
		/* Child. */
		(void)close(socks[0]);
		send_nvlist_child(socks[1]);
		_exit(0);
	}

	(void)close(socks[1]);
	send_nvlist_parent(socks[0]);

	ATF_REQUIRE(waitpid(pid, &status, 0) == pid);
	ATF_REQUIRE(status == 0);
}

static void
nvlist_send_recv__send_closed_fd(short sotype)
{
	nvlist_t *nvl;
	int socks[2];

	ATF_REQUIRE(socketpair(PF_UNIX, sotype, 0, socks) == 0);

	nvl = nvlist_create(0);
	ATF_REQUIRE(nvl != NULL);
	nvlist_add_descriptor(nvl, "fd", 12345);
	ATF_REQUIRE(nvlist_error(nvl) == EBADF);

	ATF_REQUIRE_ERRNO(EBADF, nvlist_send(socks[1], nvl) != 0);
}

static int
nopenfds(void)
{
	size_t len;
	int error, mib[4], n;

	mib[0] = CTL_KERN;
	mib[1] = KERN_PROC;
	mib[2] = KERN_PROC_NFDS;
	mib[3] = 0;

	len = sizeof(n);
	error = sysctl(mib, nitems(mib), &n, &len, NULL, 0);
	if (error != 0)
		return (-1);
	return (n);
}

#define	NFDS	512

static void
send_many_fds_child(int sock)
{
	char name[16];
	nvlist_t *nvl;
	int anfds, bnfds, fd, i, j;

	fd = open(_PATH_DEVNULL, O_RDONLY);
	ATF_REQUIRE(fd >= 0);

	for (i = 1; i < NFDS; i++) {
		nvl = nvlist_create(0);
		bnfds = nopenfds();
		if (bnfds == -1)
			err(EXIT_FAILURE, "sysctl");

		for (j = 0; j < i; j++) {
			snprintf(name, sizeof(name), "fd%d", j);
			nvlist_add_descriptor(nvl, name, fd);
		}
		nvlist_send(sock, nvl);
		nvlist_destroy(nvl);

		anfds = nopenfds();
		if (anfds == -1)
			err(EXIT_FAILURE, "sysctl");
		if (anfds != bnfds)
			errx(EXIT_FAILURE, "fd count mismatch");
	}
}

static void
nvlist_send_recv__send_many_fds(short sotype)
{
	char name[16];
	nvlist_t *nvl;
	int anfds, bnfds, fd, i, j, socks[2], status;
	pid_t pid;

	ATF_REQUIRE(socketpair(PF_UNIX, sotype, 0, socks) == 0);

	pid = fork();
	ATF_REQUIRE(pid >= 0);
	if (pid == 0) {
		/* Child. */
		(void)close(socks[0]);
		send_many_fds_child(socks[1]);
		_exit(0);
	}

	(void)close(socks[1]);

	for (i = 1; i < NFDS; i++) {
		bnfds = nopenfds();
		ATF_REQUIRE(bnfds != -1);

		nvl = nvlist_recv(socks[0], 0);
		ATF_REQUIRE(nvl != NULL);
		for (j = 0; j < i; j++) {
			snprintf(name, sizeof(name), "fd%d", j);
			fd = nvlist_take_descriptor(nvl, name);
			ATF_REQUIRE(close(fd) == 0);
		}
		nvlist_destroy(nvl);

		anfds = nopenfds();
		ATF_REQUIRE(anfds != -1);
		ATF_REQUIRE(anfds == bnfds);
	}

	ATF_REQUIRE(waitpid(pid, &status, 0) == pid);
	ATF_REQUIRE(status == 0);
}

/*
 * This test needs to tune the following sysctl's:
 *      net.local.dgram.maxdgram
 *      net.local.dgram.recvspace
 */
ATF_TC_WITHOUT_HEAD(nvlist_send_recv__send_many_fds__dgram);
ATF_TC_BODY(nvlist_send_recv__send_many_fds__dgram, tc)
{
	u_long maxdgram, recvspace, temp_maxdgram, temp_recvspace;
	size_t len;
	int error;

	atf_tc_skip("https://bugs.freebsd.org/260891");

	/* size of the largest datagram to send */
	temp_maxdgram = 16772;
	len = sizeof(maxdgram);
	error = sysctlbyname("net.local.dgram.maxdgram", &maxdgram,
	    &len, &temp_maxdgram, sizeof(temp_maxdgram));
	if (error != 0)
		atf_tc_skip("cannot set net.local.dgram.maxdgram: %s", strerror(errno));

	/*
	 * The receive queue fills up quicker than it's being emptied,
	 * bump it to a sufficiently large enough value, 1M.
	 */
	temp_recvspace = 1048576;
	len = sizeof(recvspace);
	error = sysctlbyname("net.local.dgram.recvspace", &recvspace,
	    &len, &temp_recvspace, sizeof(temp_recvspace));
	if (error != 0)
		atf_tc_skip("cannot set net.local.dgram.recvspace: %s", strerror(errno));

	nvlist_send_recv__send_many_fds(SOCK_DGRAM);

	/* restore original values */
	error = sysctlbyname("net.local.dgram.maxdgram", NULL, NULL, &maxdgram, sizeof(maxdgram));
	if (error != 0)
		warn("failed to restore net.local.dgram.maxdgram");

	error = sysctlbyname("net.local.dgram.recvspace", NULL, NULL, &recvspace, sizeof(recvspace));
	if (error != 0)
		warn("failed to restore net.local.dgram.recvspace");
}

ATF_TC_WITHOUT_HEAD(nvlist_send_recv__send_many_fds__stream);
ATF_TC_BODY(nvlist_send_recv__send_many_fds__stream, tc)
{
	nvlist_send_recv__send_many_fds(SOCK_STREAM);
}

ATF_TC_WITHOUT_HEAD(nvlist_send_recv__send_nvlist__dgram);
ATF_TC_BODY(nvlist_send_recv__send_nvlist__dgram, tc)
{
	nvlist_send_recv__send_nvlist(SOCK_DGRAM);
}

ATF_TC_WITHOUT_HEAD(nvlist_send_recv__send_nvlist__stream);
ATF_TC_BODY(nvlist_send_recv__send_nvlist__stream, tc)
{
	nvlist_send_recv__send_nvlist(SOCK_STREAM);
}

ATF_TC_WITHOUT_HEAD(nvlist_send_recv__send_closed_fd__dgram);
ATF_TC_BODY(nvlist_send_recv__send_closed_fd__dgram, tc)
{
	nvlist_send_recv__send_closed_fd(SOCK_DGRAM);
}

ATF_TC_WITHOUT_HEAD(nvlist_send_recv__send_closed_fd__stream);
ATF_TC_BODY(nvlist_send_recv__send_closed_fd__stream, tc)
{
	nvlist_send_recv__send_closed_fd(SOCK_STREAM);
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, nvlist_send_recv__send_nvlist__dgram);
	ATF_TP_ADD_TC(tp, nvlist_send_recv__send_nvlist__stream);
	ATF_TP_ADD_TC(tp, nvlist_send_recv__send_closed_fd__dgram);
	ATF_TP_ADD_TC(tp, nvlist_send_recv__send_closed_fd__stream);
	ATF_TP_ADD_TC(tp, nvlist_send_recv__send_many_fds__dgram);
	ATF_TP_ADD_TC(tp, nvlist_send_recv__send_many_fds__stream);

	return (atf_no_error());
}
