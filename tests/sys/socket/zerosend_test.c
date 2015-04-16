/*-
 * Copyright (c) 2007 Robert N. M. Watson
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
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <netinet/in.h>

#include <arpa/inet.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <atf-c.h>

static void
try_0send(int fd)
{
	ssize_t len;
	char ch;

	ch = 0;
	len = send(fd, &ch, 0, 0);
	ATF_REQUIRE_MSG(len != -1, "send failed: %s", strerror(errno));
	ATF_REQUIRE_MSG(len == 0, "send returned %zd (not 0): %s",
	    len, strerror(errno));
}

static void
try_0write(int fd)
{
	ssize_t len;
	char ch;

	ch = 0;
	len = write(fd, &ch, 0);
	ATF_REQUIRE_MSG(len != -1, "write failed: %s", strerror(errno));
	ATF_REQUIRE_MSG(len == 0, "write returned: %zd (not 0): %s",
	    len, strerror(errno));
}

static void
setup_udp(int *fdp)
{
	struct sockaddr_in sin;
	int port_base, sock1, sock2;

	bzero(&sin, sizeof(sin));
	sin.sin_len = sizeof(sin);
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	port_base = MAX((int)random() % 65535, 1025);

	sin.sin_port = htons(port_base);
	sock1 = socket(PF_INET, SOCK_DGRAM, 0);
	ATF_REQUIRE_MSG(sock1 != -1, "socket # 1 failed: %s", strerror(errno));
	ATF_REQUIRE_MSG(bind(sock1, (struct sockaddr *)&sin, sizeof(sin)) == 0,
	    "bind # 1 failed: %s", strerror(errno));
	sin.sin_port = htons(port_base + 1);
	ATF_REQUIRE_MSG(connect(sock1, (struct sockaddr *)&sin, sizeof(sin))
	    == 0, "connect # 1 failed: %s", strerror(errno));

	sock2 = socket(PF_INET, SOCK_DGRAM, 0);
	ATF_REQUIRE_MSG(sock2 != -1, "socket # 2 failed: %s", strerror(errno));
	ATF_REQUIRE_MSG(bind(sock2, (struct sockaddr *)&sin, sizeof(sin)) == 0,
	    "bind # 2 failed: %s", strerror(errno));
	sin.sin_port = htons(port_base);
	ATF_REQUIRE_MSG(connect(sock2, (struct sockaddr *)&sin, sizeof(sin))
	    == 0, "connect # 2 failed: %s", strerror(errno));

	fdp[0] = sock1;
	fdp[1] = sock2;
	fdp[2] = -1;
}

static void
setup_tcp(int *fdp)
{
	fd_set writefds, exceptfds;
	struct sockaddr_in sin;
	int port_base, ret, sock1, sock2, sock3;
	struct timeval tv;

	bzero(&sin, sizeof(sin));
	sin.sin_len = sizeof(sin);
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	port_base = MAX((int)random() % 65535, 1025);

	/*
	 * First set up the listen socket.
	 */
	sin.sin_port = htons(port_base);
	sock1 = socket(PF_INET, SOCK_STREAM, 0);
	ATF_REQUIRE_MSG(sock1 != -1, "socket # 1 failed: %s", strerror(errno));
	ATF_REQUIRE_MSG(bind(sock1, (struct sockaddr *)&sin, sizeof(sin)) == 0,
	    "bind # 1 failed: %s", strerror(errno));
	ATF_REQUIRE_MSG(listen(sock1, -1) == 0,
	    "listen # 1 failed: %s", strerror(errno));

	/*
	 * Now connect to it, non-blocking so that we don't deadlock against
	 * ourselves.
	 */
	sock2 = socket(PF_INET, SOCK_STREAM, 0);
	ATF_REQUIRE_MSG(sock2 != -1, "socket # 2 failed: %s", strerror(errno));
	ATF_REQUIRE_MSG(fcntl(sock2, F_SETFL, O_NONBLOCK) == 0,
	    "setting socket as nonblocking failed: %s", strerror(errno));
	ATF_REQUIRE_MSG(
	    (connect(sock2, (struct sockaddr *)&sin, sizeof(sin)) == 0 ||
	     errno == EINPROGRESS),
	    "connect # 2 failed: %s", strerror(errno));

	/*
	 * Now pick up the connection after sleeping a moment to make sure
	 * there's been time for some packets to go back and forth.
	 */
	ATF_REQUIRE_MSG(sleep(1) == 0, "sleep(1) <= 0");
	sock3 = accept(sock1, NULL, NULL);
	ATF_REQUIRE_MSG(sock3 != -1, "accept failed: %s", strerror(errno));
	ATF_REQUIRE_MSG(sleep(1) == 0, "sleep(1) <= 0");

	FD_ZERO(&writefds);
	FD_SET(sock2, &writefds);
	FD_ZERO(&exceptfds);
	FD_SET(sock2, &exceptfds);
	tv.tv_sec = 1;
	tv.tv_usec = 0;
	ret = select(sock2 + 1, NULL, &writefds, &exceptfds, &tv);
	ATF_REQUIRE_MSG(ret != -1, "select failed: %s", strerror(errno));
	ATF_REQUIRE_MSG(!FD_ISSET(sock2, &exceptfds),
	    "select: exception occurred with sock2");
	ATF_REQUIRE_MSG(FD_ISSET(sock2, &writefds),
	    "not writable");

	close(sock1);
	fdp[0] = sock2;
	fdp[1] = sock3;
	fdp[2] = sock1;
}

static void
setup_udsstream(int *fdp)
{

	ATF_REQUIRE_MSG(socketpair(PF_LOCAL, SOCK_STREAM, 0, fdp) == 0,
	    "socketpair failed: %s", strerror(errno));
}

static void
setup_udsdgram(int *fdp)
{

	ATF_REQUIRE_MSG(socketpair(PF_LOCAL, SOCK_DGRAM, 0, fdp) == 0,
	    "socketpair failed: %s", strerror(errno));
}

static void
setup_pipe(int *fdp)
{

	ATF_REQUIRE_MSG(pipe(fdp) == 0, "pipe failed: %s", strerror(errno));
}

static void
setup_fifo(int *fdp)
{
	char path[] = "0send_fifo.XXXXXXX";
	int fd1, fd2;

	ATF_REQUIRE_MSG(mkstemp(path) != -1,
	    "mkstemp failed: %s", strerror(errno));
	unlink(path);

	ATF_REQUIRE_MSG(mkfifo(path, 0600) == 0,
	    "mkfifo(\"%s\", 0600) failed: %s", path, strerror(errno));

	fd1 = open(path, O_RDONLY | O_NONBLOCK);
	ATF_REQUIRE_MSG(fd1 != -1, "open(\"%s\", O_RDONLY)", path);

	fd2 = open(path, O_WRONLY | O_NONBLOCK);
	ATF_REQUIRE_MSG(fd2 != -1, "open(\"%s\", O_WRONLY)", path);

	fdp[0] = fd2;
	fdp[1] = fd1;
	fdp[2] = -1;
}

static int fd[3];

static void
close_fds(int *fdp)
{
	unsigned int i;

	for (i = 0; i < nitems(fdp); i++)
		close(fdp[i]);
}

ATF_TC_WITHOUT_HEAD(udp_zero_send);
ATF_TC_BODY(udp_zero_send, tc)
{

	setup_udp(fd);
	try_0send(fd[0]);
	close_fds(fd);
}

ATF_TC_WITHOUT_HEAD(udp_zero_write);
ATF_TC_BODY(udp_zero_write, tc)
{

	setup_udp(fd);
	try_0write(fd[0]);
	close_fds(fd);
}

ATF_TC_WITHOUT_HEAD(tcp_zero_send);
ATF_TC_BODY(tcp_zero_send, tc)
{

	setup_tcp(fd);
	try_0send(fd[0]);
	close_fds(fd);
}

ATF_TC_WITHOUT_HEAD(tcp_zero_write);
ATF_TC_BODY(tcp_zero_write, tc)
{

	setup_tcp(fd);
	try_0write(fd[0]);
	close_fds(fd);
}

ATF_TC_WITHOUT_HEAD(udsstream_zero_send);
ATF_TC_BODY(udsstream_zero_send, tc)
{

	setup_udsstream(fd);
	try_0send(fd[0]);
	close_fds(fd);
}

ATF_TC_WITHOUT_HEAD(udsstream_zero_write);
ATF_TC_BODY(udsstream_zero_write, tc)
{

	setup_udsstream(fd);
	try_0write(fd[0]);
	close_fds(fd);
}

ATF_TC_WITHOUT_HEAD(udsdgram_zero_send);
ATF_TC_BODY(udsdgram_zero_send, tc)
{

	setup_udsdgram(fd);
	try_0send(fd[0]);
	close_fds(fd);
}

ATF_TC_WITHOUT_HEAD(udsdgram_zero_write);
ATF_TC_BODY(udsdgram_zero_write, tc)
{

	setup_udsdgram(fd);
	try_0write(fd[0]);
	close_fds(fd);
}

ATF_TC_WITHOUT_HEAD(pipe_zero_write);
ATF_TC_BODY(pipe_zero_write, tc)
{

	setup_pipe(fd);
	try_0write(fd[0]);
	close_fds(fd);
}

ATF_TC_WITHOUT_HEAD(fifo_zero_write);
ATF_TC_BODY(fifo_zero_write, tc)
{

	setup_fifo(fd);
	try_0write(fd[0]);
	close_fds(fd);
}

ATF_TP_ADD_TCS(tp)
{

	srandomdev();

	ATF_TP_ADD_TC(tp, udp_zero_send);
	ATF_TP_ADD_TC(tp, udp_zero_write);
	ATF_TP_ADD_TC(tp, tcp_zero_send);
	ATF_TP_ADD_TC(tp, tcp_zero_write);
	ATF_TP_ADD_TC(tp, udsstream_zero_write);
	ATF_TP_ADD_TC(tp, udsstream_zero_send);
	ATF_TP_ADD_TC(tp, udsdgram_zero_write);
	ATF_TP_ADD_TC(tp, udsdgram_zero_send);
	ATF_TP_ADD_TC(tp, pipe_zero_write);
	ATF_TP_ADD_TC(tp, fifo_zero_write);

	return (atf_no_error());
}
