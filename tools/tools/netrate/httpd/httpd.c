/*-
 * Copyright (c) 2005 Robert N. M. Watson
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
#include <sys/uio.h>

#include <netinet/in.h>

#include <arpa/inet.h>

#include <err.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * Simple, multi-threaded HTTP server.  Very dumb.
 */
#define	THREADS	128
#define	BUFFER	(48*1024)
#define	HTTP	8000

#define	HTTP_OK	"HTTP/1.1 200 OK\n"
#define	HTTP_SERVER "Server rwatson_httpd/1.0 (FreeBSD)\n"
#define	HTTP_CONNECTION "Connection: close\n"
#define	HTTP_CONTENT "Content-Type: text/html\n"

static const char	*path;
static int		 listen_sock;
static int		 data_file;

/*
 * Given an open client socket, process its request.  No notion of timeout.
 */
static int
http_serve(int sock)
{
	struct iovec header_iovec[4];
	struct sf_hdtr sf_hdtr;
	ssize_t len;
	int ncount;
	char ch;

	/* Read until \n\n.  Not very smart. */
	ncount = 0;
	while (1) {
		len = recv(sock, &ch, sizeof(ch), 0);
		if (len < 0) {
			warn("recv");
			return (-1);
		}
		if (len == 0)
			return (-1);
		if (ch == '\n')
			ncount++;
		if (ncount == 2)
			break;
	}

	bzero(&sf_hdtr, sizeof(sf_hdtr));
	bzero(&header_iovec, sizeof(header_iovec));
	header_iovec[0].iov_base = HTTP_OK;
	header_iovec[0].iov_len = strlen(HTTP_OK);
	header_iovec[1].iov_base = HTTP_SERVER;
	header_iovec[1].iov_len = strlen(HTTP_SERVER);
	header_iovec[2].iov_base = HTTP_CONNECTION;
	header_iovec[2].iov_len = strlen(HTTP_CONNECTION);
	header_iovec[3].iov_base = HTTP_CONTENT;
	header_iovec[3].iov_len = strlen(HTTP_CONTENT);
	sf_hdtr.headers = header_iovec;
	sf_hdtr.hdr_cnt = 4;
	sf_hdtr.trailers = NULL;
	sf_hdtr.trl_cnt = 0;

	if (sendfile(data_file, sock, 0, 0, &sf_hdtr, NULL, 0) < 0)
		warn("sendfile");

	return (0);
}

static void *
httpd_worker(void *arg)
{
	int sock;

	while (1) {
		sock = accept(listen_sock, NULL, NULL);
		if (sock < 0)
			continue;
		(void)http_serve(sock);
		close(sock);
	}
}

int
main(int argc, char *argv[])
{
	pthread_t thread_array[THREADS];
	struct sockaddr_in sin;
	int i;

	if (argc != 2)
		errx(-1, "usage: http [PATH]");

	listen_sock = socket(PF_INET, SOCK_STREAM, 0);
	if (listen_sock < 0)
		err(-1, "socket(PF_INET, SOCK_STREAM)");

	bzero(&sin, sizeof(sin));
	sin.sin_len = sizeof(sin);
	sin.sin_family = AF_INET;
	sin.sin_port = htons(HTTP);

	path = argv[1];
	data_file = open(path, O_RDONLY);
	if (data_file < 0)
		err(-1, "open: %s", path);

	if (bind(listen_sock, (struct sockaddr *)&sin, sizeof(sin)) < 0)
		err(-1, "bind");

	if (listen(listen_sock, -1) < 0)
		err(-1, "listen");

	for (i = 0; i < THREADS; i++) {
		if (pthread_create(&thread_array[i], NULL, httpd_worker,
		    NULL) < 0)
			err(-1, "pthread_create");
	}

	for (i = 0; i < THREADS; i++) {
		if (pthread_join(thread_array[i], NULL) < 0)
			err(-1, "pthread_join");
	}
	return (0);
}
