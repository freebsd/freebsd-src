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

#include <netinet/in.h>

#include <arpa/inet.h>

#include <err.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * Simple, multi-threaded HTTP benchmark.  Fetches a single URL using the
 * specified parameters, and after a period of execution, reports on how it
 * worked out.
 */
#define	THREADS	128
#define	SECONDS	20
#define	BUFFER	(48*1024)
#define	QUIET	1

struct http_worker_description {
	pthread_t	hwd_thread;
	uintmax_t	hwd_count;
	uintmax_t	hwd_errorcount;
};

static struct sockaddr_in		 sin;
static char				*path;
static struct http_worker_description	 hwd[THREADS];
static int				 run_done;
static pthread_barrier_t		 start_barrier;

/*
 * Given a partially processed URL, fetch it from the specified host.
 */
static int
http_fetch(struct sockaddr_in *sin, char *path, int quiet)
{
	u_char buffer[BUFFER];
	ssize_t len;
	size_t sofar;
	int sock;

	sock = socket(PF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		if (!quiet)
			warn("socket(PF_INET, SOCK_STREAM)");
		return (-1);
	}

	if (connect(sock, (struct sockaddr *)sin, sizeof(*sin)) < 0) {
		if (!quiet)
			warn("connect");
		close(sock);
		return (-1);
	}

	/* Send a request. */
	snprintf(buffer, BUFFER, "GET %s HTTP/1.0\n\n", path);
	sofar = 0;
	while (sofar < strlen(buffer)) {
		len = send(sock, buffer, strlen(buffer), 0);
		if (len < 0) {
			if (!quiet)
				warn("send");
			close(sock);
			return (-1);
		}
		if (len == 0) {
			if (!quiet)
				warnx("send: len == 0");
		}
		sofar += len;
	}

	/* Read until done.  Not very smart. */
	while (1) {
		len = recv(sock, buffer, BUFFER, 0);
		if (len < 0) {
			if (!quiet)
				warn("recv");
			close(sock);
			return (-1);
		}
		if (len == 0)
			break;
	}

	close(sock);
	return (0);
}

static void *
http_worker(void *arg)
{
	struct http_worker_description *hwdp;
	int ret;

	ret = pthread_barrier_wait(&start_barrier);
	if (ret != 0 && ret != PTHREAD_BARRIER_SERIAL_THREAD)
		err(-1, "pthread_barrier_wait");

	hwdp = arg;
	while (!run_done) {
		if (http_fetch(&sin, path, QUIET) < 0) {
			hwdp->hwd_errorcount++;
			continue;
		}
		/* Don't count transfers that didn't finish in time. */
		if (!run_done)
			hwdp->hwd_count++;
	}

	return (NULL);
}

int
main(int argc, char *argv[])
{
	uintmax_t total;
	int i;

	if (argc != 4)
		errx(-1, "usage: http [ip] [port] [path]");

	bzero(&sin, sizeof(sin));
	sin.sin_len = sizeof(sin);
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = inet_addr(argv[1]);
	sin.sin_port = htons(atoi(argv[2]));
	path = argv[3];

	/*
	 * Do one test retrieve so we can report the error from it, if any.
	 */
	if (http_fetch(&sin, path, 0) < 0)
		exit(-1);

	if (pthread_barrier_init(&start_barrier, NULL, THREADS) < 0)
		err(-1, "pthread_mutex_init");

	for (i = 0; i < THREADS; i++) {
		hwd[i].hwd_count = 0;
		if (pthread_create(&hwd[i].hwd_thread, NULL, http_worker,
		    &hwd[i]) < 0)
			err(-1, "pthread_create");
	}
	sleep(SECONDS);
	run_done = 1;
	for (i = 0; i < THREADS; i++) {
		if (pthread_join(hwd[i].hwd_thread, NULL) < 0)
			err(-1, "pthread_join");
	}
	total = 0;
	for (i = 0; i < THREADS; i++)
		total += hwd[i].hwd_count;
	printf("%ju transfers/second\n", total / SECONDS);
	total = 0;
	for (i = 0; i < THREADS; i++)
		total += hwd[i].hwd_errorcount;
	printf("%ju errors/second\n", total / SECONDS);
	return (0);
}
