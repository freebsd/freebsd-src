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

/*
 * TCP regression test that opens a loopback TCP session, then closes one end
 * while shutting down the other.  This triggers an unusual TCP stack case in
 * which an open file descriptor / socket is associated with a closed TCP
 * connection.
 */

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>

#include <err.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define	TCP_PORT	9001

static void
tcp_server(pid_t partner)
{
	int error, listen_fd, accept_fd;
	struct sockaddr_in sin;

	listen_fd = socket(PF_INET, SOCK_STREAM, 0);
	if (listen_fd < 0) {
		error = errno;
		(void)kill(partner, SIGTERM);
		errno = error;
		err(-1, "tcp_server: socket");
	}

	bzero(&sin, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_len = sizeof(sin);
	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	sin.sin_port = htons(TCP_PORT);

	if (bind(listen_fd, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
		error = errno;
		(void)kill(partner, SIGTERM);
		errno = error;
		err(-1, "tcp_server: bind");
	}

	if (listen(listen_fd, -1) < 0) {
		error = errno;
		(void)kill(partner, SIGTERM);
		errno = error;
		err(-1, "tcp_server: listen");
	}

	accept_fd = accept(listen_fd, NULL, NULL);
	if (accept_fd < 0) {
		error = errno;
		(void)kill(partner, SIGTERM);
		errno = error;
		err(-1, "tcp_server: accept");
	}
	close(accept_fd);
	close(listen_fd);
}

static void
tcp_client(pid_t partner, int secs)
{
	struct sockaddr_in sin;
	int error, sock;

	sleep(1);

	sock = socket(PF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		error = errno;
		(void)kill(partner, SIGTERM);
		errno = error;
		err(-1, "socket");
	}

	bzero(&sin, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_len = sizeof(sin);
	sin.sin_addr.s_addr = ntohl(INADDR_LOOPBACK);
	sin.sin_port = htons(TCP_PORT);

	if (connect(sock, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
		error = errno;
		(void)kill(partner, SIGTERM);
		errno = error;
		err(-1, "connect");
	}

	if (shutdown(sock, SHUT_RDWR) < 0) {
		error = errno;
		(void)kill(partner, SIGTERM);
		errno = error;
		err(-1, "shutdown");
	}

	sleep(secs);
	close(sock);
}

int
main(int argc, char *argv[])
{
	pid_t child_pid, parent_pid;

	if (signal(SIGCHLD, SIG_IGN) == SIG_ERR)
		err(-1, "signal");

	/*
	 * Run the whole thing twice: once, with a short sleep in the client,
	 * so that we close before time wait runs out, and once with a long
	 * sleep so that the time wait terminates while the socket is open.
	 */
	parent_pid = getpid();
	child_pid = fork();
	if (child_pid < 0)
		err(-1, "fork");
	if (child_pid == 0) {
		child_pid = getpid();
		tcp_server(child_pid);
		exit(0);
	} else
		tcp_client(parent_pid, 1);
	(void)kill(child_pid, SIGTERM);
	sleep(5);

	parent_pid = getpid();
	child_pid = fork();
	if (child_pid < 0)
		err(-1, "fork");
	if (child_pid == 0) {
		child_pid = getpid();
		tcp_server(parent_pid);
	} else
		tcp_client(child_pid, 800);

	return (0);
}
