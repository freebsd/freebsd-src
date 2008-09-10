/*-
 * Copyright (c) 2006 Robert N. M. Watson
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

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <netinet/in.h>

#include <err.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * Simple regression test for sendfile.  Creates a file sized at four pages
 * and then proceeds to send it over a series of sockets, exercising a number
 * of cases and performing limited validation.
 */

#define	TEST_PORT	5678
#define	TEST_MAGIC	0x4440f7bb
#define	TEST_PAGES	4
#define	TEST_SECONDS	30

struct test_header {
	u_int32_t	th_magic;
	u_int32_t	th_header_length;
	u_int32_t	th_offset;
	u_int32_t	th_length;
};

pid_t	child_pid, parent_pid;
int	listen_socket;
int	file_fd;

static int
test_th(struct test_header *th, u_int32_t *header_length, u_int32_t *offset,
    u_int32_t *length)
{

	if (th->th_magic != htonl(TEST_MAGIC))
		return (0);
	*header_length = ntohl(th->th_header_length);
	*offset = ntohl(th->th_offset);
	*length = ntohl(th->th_length);
	return (1);
}

static void
signal_alarm(int signum)
{

	(void)signum;
}

static void
setup_alarm(int seconds)
{

	signal(SIGALRM, signal_alarm);
	alarm(seconds);
}

static void
cancel_alarm(void)
{

	alarm(0);
	signal(SIGALRM, SIG_DFL);
}

static void
receive_test(int accept_socket)
{
	u_int32_t header_length, offset, length, counter;
	struct test_header th;
	ssize_t len;
	char ch;

	len = read(accept_socket, &th, sizeof(th));
	if (len < 0)
		err(1, "read");
	if ((size_t)len < sizeof(th))
		errx(1, "read: %zd", len);

	if (test_th(&th, &header_length, &offset, &length) == 0)
		errx(1, "test_th: bad");

	counter = 0;
	while (1) {
		len = read(accept_socket, &ch, sizeof(ch));
		if (len < 0)
			err(1, "read");
		if (len == 0)
			break;
		counter++;
		/* XXXRW: Validate byte here. */
	}
	if (counter != header_length + length)
		errx(1, "receive_test: expected (%d, %d) received %d",
		    header_length, length, counter);
}

static void
run_child(void)
{
	int accept_socket;

	while (1) {
		accept_socket = accept(listen_socket, NULL, NULL);	
		setup_alarm(TEST_SECONDS);
		receive_test(accept_socket);
		cancel_alarm();
		close(accept_socket);
	}
}

static int
new_test_socket(void)
{
	struct sockaddr_in sin;
	int connect_socket;

	connect_socket = socket(PF_INET, SOCK_STREAM, 0);
	if (connect_socket < 0)
		err(1, "socket");

	bzero(&sin, sizeof(sin));
	sin.sin_len = sizeof(sin);
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	sin.sin_port = htons(TEST_PORT);

	if (connect(connect_socket, (struct sockaddr *)&sin, sizeof(sin)) < 0)
		err(1, "connect");

	return (connect_socket);
}

static void
init_th(struct test_header *th, u_int32_t header_length, u_int32_t offset,
    u_int32_t length)
{

	bzero(th, sizeof(*th));
	th->th_magic = htonl(TEST_MAGIC);
	th->th_header_length = htonl(header_length);
	th->th_offset = htonl(offset);
	th->th_length = htonl(length);
}

static void
send_test(int connect_socket, u_int32_t header_length, u_int32_t offset,
    u_int32_t length)
{
	struct test_header th;
	struct sf_hdtr hdtr, *hdtrp;
	struct iovec headers;
	char *header;
	ssize_t len;
	off_t off;

	len = lseek(file_fd, 0, SEEK_SET);
	if (len < 0)
		err(1, "lseek");
	if (len != 0)
		errx(1, "lseek: %zd", len);

	init_th(&th, header_length, offset, length);

	len = write(connect_socket, &th, sizeof(th));
	if (len < 0)
		err(1, "send");
	if (len != sizeof(th))
		err(1, "send: %zd", len);

	if (header_length != 0) {
		header = malloc(header_length);
		if (header == NULL)
			err(1, "malloc");
		hdtrp = &hdtr;
		bzero(&headers, sizeof(headers));
		headers.iov_base = header;
		headers.iov_len = header_length;
		bzero(&hdtr, sizeof(hdtr));
		hdtr.headers = &headers;
		hdtr.hdr_cnt = 1;
		hdtr.trailers = NULL;
		hdtr.trl_cnt = 0;
	} else {
		hdtrp = NULL;
		header = NULL;
	}

	if (sendfile(file_fd, connect_socket, offset, length, hdtrp, &off,
	    0) < 0)
		err(1, "sendfile");

	if (length == 0) {
		struct stat sb;

		if (fstat(file_fd, &sb) < 0)
			err(1, "fstat");
		length = sb.st_size - offset;
	}

	if (off != length) {
		errx(1, "sendfile: off(%ju) != length(%ju)",
		    (uintmax_t)off, (uintmax_t)length);
	}

	if (header != NULL)
		free(header);
}

static void
run_parent(void)
{
	int connect_socket;

	connect_socket = new_test_socket();
	send_test(connect_socket, 0, 0, 1);
	close(connect_socket);

	sleep(1);

	connect_socket = new_test_socket();
	send_test(connect_socket, 0, 0, getpagesize());
	close(connect_socket);

	sleep(1);

	connect_socket = new_test_socket();
	send_test(connect_socket, 0, 1, 1);
	close(connect_socket);

	sleep(1);

	connect_socket = new_test_socket();
	send_test(connect_socket, 0, 1, getpagesize());
	close(connect_socket);

	sleep(1);

	connect_socket = new_test_socket();
	send_test(connect_socket, 0, getpagesize(), getpagesize());
	close(connect_socket);

	sleep(1);

	connect_socket = new_test_socket();
	send_test(connect_socket, 0, 0, 2 * getpagesize());
	close(connect_socket);

	sleep(1);

	connect_socket = new_test_socket();
	send_test(connect_socket, 0, 0, 0);
	close(connect_socket);

	sleep(1);

	connect_socket = new_test_socket();
	send_test(connect_socket, 0, getpagesize(), 0);
	close(connect_socket);

	sleep(1);

	connect_socket = new_test_socket();
	send_test(connect_socket, 0, 2 * getpagesize(), 0);
	close(connect_socket);

	sleep(1);

	connect_socket = new_test_socket();
	send_test(connect_socket, 0, TEST_PAGES * getpagesize(), 0);
	close(connect_socket);

	sleep(1);

	(void)kill(child_pid, SIGKILL);
}

int
main(void)
{
	char path[PATH_MAX], *page_buffer;
	struct sockaddr_in sin;
	int pagesize;
	ssize_t len;

	pagesize = getpagesize();
	page_buffer = malloc(TEST_PAGES * pagesize);
	if (page_buffer == NULL)
		err(1, "malloc");
	bzero(page_buffer, TEST_PAGES * pagesize);

	listen_socket = socket(PF_INET, SOCK_STREAM, 0);
	if (listen_socket < 0)
		err(1, "socket");

	bzero(&sin, sizeof(sin));
	sin.sin_len = sizeof(sin);
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	sin.sin_port = htons(TEST_PORT);

	snprintf(path, PATH_MAX, "/tmp/sendfile.XXXXXXXXXXXX");
	file_fd = mkstemp(path);
	(void)unlink(path);

	len = write(file_fd, page_buffer, TEST_PAGES * pagesize);
	if (len < 0)
		err(1, "write");

	len = lseek(file_fd, 0, SEEK_SET);
	if (len < 0)
		err(1, "lseek");
	if (len != 0)
		errx(1, "lseek: %zd", len);

	if (bind(listen_socket, (struct sockaddr *)&sin, sizeof(sin)) < 0)
		err(1, "bind");

	if (listen(listen_socket, -1) < 0)
		err(1, "listen");

	parent_pid = getpid();
	child_pid = fork();
	if (child_pid < 0)
		err(1, "fork");
	if (child_pid == 0) {
		child_pid = getpid();
		run_child();
	} else
		run_parent();

	return (0);
}
