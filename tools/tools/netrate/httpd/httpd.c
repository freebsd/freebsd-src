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
#include <sys/utsname.h>

#include <netinet/in.h>

#include <arpa/inet.h>

#include <err.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * Simple, multi-threaded HTTP server.  Very dumb.
 *
 * If a path is specified as an argument, only that file is served.  If no
 * path is specified, httpd will create one file to send per server thread.
 */
#define	THREADS		128
#define	BUFFER		1024
#define	FILESIZE	1024

#define	HTTP_OK		"HTTP/1.1 200 OK\n"
#define	HTTP_SERVER1	"Server rwatson_httpd/1.0 ("
#define	HTTP_SERVER2	")\n"
#define	HTTP_CONNECTION	"Connection: close\n"
#define	HTTP_CONTENT	"Content-Type: text/html\n\n"

struct httpd_thread_state {
	pthread_t	hts_thread;
	int		hts_fd;
} hts[THREADS];

static const char	*path;
static int		 data_file;
static int		 listen_sock;
static struct utsname	 utsname;

/*
 * Given an open client socket, process its request.  No notion of timeout.
 */
static int
http_serve(int sock, int fd)
{
	struct iovec header_iovec[6];
	struct sf_hdtr sf_hdtr;
	char buffer[BUFFER];
	ssize_t len;
	int i, ncount;

	/* Read until \n\n.  Not very smart. */
	ncount = 0;
	while (1) {
		len = recv(sock, buffer, BUFFER, 0);
		if (len < 0) {
			warn("recv");
			return (-1);
		}
		if (len == 0)
			return (-1);
		for (i = 0; i < len; i++) {
			switch (buffer[i]) {
			case '\n':
				ncount++;
				break;

			case '\r':
				break;

			default:
				ncount = 0;
			}
		}
		if (ncount == 2)
			break;
	}

	bzero(&sf_hdtr, sizeof(sf_hdtr));
	bzero(&header_iovec, sizeof(header_iovec));
	header_iovec[0].iov_base = HTTP_OK;
	header_iovec[0].iov_len = strlen(HTTP_OK);
	header_iovec[1].iov_base = HTTP_SERVER1;
	header_iovec[1].iov_len = strlen(HTTP_SERVER1);
	header_iovec[2].iov_base = utsname.sysname;
	header_iovec[2].iov_len = strlen(utsname.sysname);
	header_iovec[3].iov_base = HTTP_SERVER2;
	header_iovec[3].iov_len = strlen(HTTP_SERVER2);
	header_iovec[4].iov_base = HTTP_CONNECTION;
	header_iovec[4].iov_len = strlen(HTTP_CONNECTION);
	header_iovec[5].iov_base = HTTP_CONTENT;
	header_iovec[5].iov_len = strlen(HTTP_CONTENT);
	sf_hdtr.headers = header_iovec;
	sf_hdtr.hdr_cnt = 6;
	sf_hdtr.trailers = NULL;
	sf_hdtr.trl_cnt = 0;

	if (sendfile(fd, sock, 0, 0, &sf_hdtr, NULL, 0) < 0)
		warn("sendfile");

	return (0);
}

static void *
httpd_worker(void *arg)
{
	struct httpd_thread_state *htsp;
	int sock;

	htsp = arg;

	while (1) {
		sock = accept(listen_sock, NULL, NULL);
		if (sock < 0)
			continue;
		(void)http_serve(sock, htsp->hts_fd);
		close(sock);
	}
}

int
main(int argc, char *argv[])
{
	u_char filebuffer[FILESIZE];
	char temppath[PATH_MAX];
	struct sockaddr_in sin;
	ssize_t len;
	int i;

	if (argc != 2 && argc != 3)
		errx(-1, "usage: http port [path]");

	if (uname(&utsname) < 0)
		err(-1, "utsname");

	listen_sock = socket(PF_INET, SOCK_STREAM, 0);
	if (listen_sock < 0)
		err(-1, "socket(PF_INET, SOCK_STREAM)");

	bzero(&sin, sizeof(sin));
	sin.sin_len = sizeof(sin);
	sin.sin_family = AF_INET;
	sin.sin_port = htons(atoi(argv[1]));

	/*
	 * If a path is specified, use it.  Otherwise, create temporary files
	 * with some data for each thread.
	 */
	path = argv[2];
	if (path != NULL) {
		data_file = open(path, O_RDONLY);
		if (data_file < 0)
			err(-1, "open: %s", path);
		for (i = 0; i < THREADS; i++)
			hts[i].hts_fd = data_file;
	} else {
		memset(filebuffer, 'A', FILESIZE - 1);
		filebuffer[FILESIZE - 1] = '\n';
		for (i = 0; i < THREADS; i++) {
			snprintf(temppath, PATH_MAX, "/tmp/httpd.XXXXXXXXXXX");
			hts[i].hts_fd = mkstemp(temppath);
			if (hts[i].hts_fd < 0)
				err(-1, "mkstemp");
			(void)unlink(temppath);
			len = write(hts[i].hts_fd, filebuffer, FILESIZE);
			if (len < 0)
				err(-1, "write");
			if (len < FILESIZE)
				errx(-1, "write: short");
		}
	}

	if (bind(listen_sock, (struct sockaddr *)&sin, sizeof(sin)) < 0)
		err(-1, "bind");

	if (listen(listen_sock, -1) < 0)
		err(-1, "listen");

	for (i = 0; i < THREADS; i++) {
		if (pthread_create(&hts[i].hts_thread, NULL, httpd_worker,
		    &hts[i]) < 0)
			err(-1, "pthread_create");
	}

	for (i = 0; i < THREADS; i++) {
		if (pthread_join(hts[i].hts_thread, NULL) < 0)
			err(-1, "pthread_join");
	}
	return (0);
}
