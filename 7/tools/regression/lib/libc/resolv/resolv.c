/*	$NetBSD: resolv.c,v 1.6 2004/05/23 16:59:11 christos Exp $	*/

/*-
 * Copyright (c) 2004 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/* $FreeBSD$ */
#include <sys/cdefs.h>
__RCSID("$NetBSD: resolv.c,v 1.6 2004/05/23 16:59:11 christos Exp $");

#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <stdio.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>
#include <string.h>
#include <stringlist.h>

#define NTHREADS	10
#define NHOSTS		100
#define WS		" \t\n\r"

enum method {
	METHOD_GETADDRINFO,
	METHOD_GETHOSTBY,
	METHOD_GETIPNODEBY
};

static StringList *hosts = NULL;
static int debug = 0;
static enum method method = METHOD_GETADDRINFO;
static int reverse = 0;
static int *ask = NULL;
static int *got = NULL;

static void usage(void)  __attribute__((__noreturn__));
static void load(const char *);
static void resolvone(int);
static void *resolvloop(void *);
static void run(int *);

static pthread_mutex_t stats = PTHREAD_MUTEX_INITIALIZER;

static void
usage(void)
{
	(void)fprintf(stderr,
	    "Usage: %s [-AdHIr] [-h <nhosts>] [-n <nthreads>] <file> ...\n",
	    getprogname());
	exit(1);
}

static void
load(const char *fname)
{
	FILE *fp;
	size_t len;
	char *line;

	if ((fp = fopen(fname, "r")) == NULL)
		err(1, "Cannot open `%s'", fname);
	while ((line = fgetln(fp, &len)) != NULL) {
		char c = line[len];
		char *ptr;
		line[len] = '\0';
		for (ptr = strtok(line, WS); ptr; ptr = strtok(NULL, WS))
			sl_add(hosts, strdup(ptr));
		line[len] = c;
	}

	(void)fclose(fp);
}

static int
resolv_getaddrinfo(pthread_t self, char *host, int port)
{
	char portstr[6], buf[1024], hbuf[NI_MAXHOST], pbuf[NI_MAXSERV];
	struct addrinfo hints, *res;
	int error, len;

	snprintf(portstr, sizeof(portstr), "%d", port);
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_flags = AI_PASSIVE;
	hints.ai_socktype = SOCK_STREAM;
	error = getaddrinfo(host, portstr, &hints, &res);
	if (debug) {
		len = snprintf(buf, sizeof(buf), "%p: host %s %s\n",
		    self, host, error ? "not found" : "ok");
		(void)write(STDOUT_FILENO, buf, len);
	}
	if (error == 0 && reverse) {
		memset(hbuf, 0, sizeof(hbuf));
		memset(pbuf, 0, sizeof(pbuf));
		getnameinfo(res->ai_addr, res->ai_addrlen, hbuf, sizeof(hbuf),
			    pbuf, sizeof(pbuf), 0);
		if (debug) {
			len = snprintf(buf, sizeof(buf),
			    "%p: reverse %s %s\n", self, hbuf, pbuf);
			(void)write(STDOUT_FILENO, buf, len);
		}
	}
	if (error == 0)
		freeaddrinfo(res);
	return error;
}

static int
resolv_gethostby(pthread_t self, char *host)
{
	char buf[1024];
	struct hostent *hp, *hp2;
	int len;

	hp = gethostbyname(host);
	if (debug) {
		len = snprintf(buf, sizeof(buf), "%p: host %s %s\n",
		    self, host, (hp == NULL) ? "not found" : "ok");
		(void)write(STDOUT_FILENO, buf, len);
	}
	if (hp && reverse) {
		memcpy(buf, hp->h_addr, hp->h_length);
		hp2 = gethostbyaddr(buf, hp->h_length, hp->h_addrtype);
		if (hp2 && debug) {
			len = snprintf(buf, sizeof(buf),
			    "%p: reverse %s\n", self, hp2->h_name);
			(void)write(STDOUT_FILENO, buf, len);
		}
	}
	return hp ? 0 : -1;
}

static int
resolv_getipnodeby(pthread_t self, char *host)
{
	char buf[1024];
	struct hostent *hp, *hp2;
	int len, h_error;

	hp = getipnodebyname(host, AF_INET, 0, &h_error);
	if (debug) {
		len = snprintf(buf, sizeof(buf), "%p: host %s %s\n",
		    self, host, (hp == NULL) ? "not found" : "ok");
		(void)write(STDOUT_FILENO, buf, len);
	}
	if (hp && reverse) {
		memcpy(buf, hp->h_addr, hp->h_length);
		hp2 = getipnodebyaddr(buf, hp->h_length, hp->h_addrtype,
		    &h_error);
		if (hp2 && debug) {
			len = snprintf(buf, sizeof(buf),
			    "%p: reverse %s\n", self, hp2->h_name);
			(void)write(STDOUT_FILENO, buf, len);
		}
		if (hp2)
			freehostent(hp2);
	}
	if (hp)
		freehostent(hp);
	return hp ? 0 : -1;
}

static void
resolvone(int n)
{
	char buf[1024];
	pthread_t self = pthread_self();
	size_t i = (random() & 0x0fffffff) % hosts->sl_cur;
	char *host = hosts->sl_str[i];
	struct addrinfo hints, *res;
	int error, len;

	if (debug) {
		len = snprintf(buf, sizeof(buf), "%p: %d resolving %s %d\n",
		    self, n, host, (int)i);
		(void)write(STDOUT_FILENO, buf, len);
	}
	switch (method) {
	case METHOD_GETADDRINFO:
		error = resolv_getaddrinfo(self, host, i);
		break;
	case METHOD_GETHOSTBY:
		error = resolv_gethostby(self, host);
		break;
	case METHOD_GETIPNODEBY:
		error = resolv_getipnodeby(self, host);
		break;
	default:
		break;
	}
	pthread_mutex_lock(&stats);
	ask[i]++;
	got[i] += error == 0;
	pthread_mutex_unlock(&stats);
}

static void *
resolvloop(void *p)
{
	int *nhosts = (int *)p;
	if (*nhosts == 0)
		return;
	do
		resolvone(*nhosts);
	while (--(*nhosts));
	return NULL;
}

static void
run(int *nhosts)
{
	pthread_t self = pthread_self();
	if (pthread_create(&self, NULL, resolvloop, nhosts) != 0)
		err(1, "pthread_create");
}

int
main(int argc, char *argv[])
{
	int nthreads = NTHREADS;
	int nhosts = NHOSTS;
	int i, c, done, *nleft;
	hosts = sl_init();

	srandom(1234);

	while ((c = getopt(argc, argv, "Adh:HIn:r")) != -1)
		switch (c) {
		case 'A':
			method = METHOD_GETADDRINFO;
			break;
		case 'd':
			debug++;
			break;
		case 'h':
			nhosts = atoi(optarg);
			break;
		case 'H':
			method = METHOD_GETHOSTBY;
			break;
		case 'I':
			method = METHOD_GETIPNODEBY;
			break;
		case 'n':
			nthreads = atoi(optarg);
			break;
		case 'r':
			reverse++;
			break;
		default:
			usage();
		}

	for (i = optind; i < argc; i++)
		load(argv[i]);

	if (hosts->sl_cur == 0)
		usage();

	if ((nleft = malloc(nthreads * sizeof(int))) == NULL)
		err(1, "malloc");
	if ((ask = calloc(hosts->sl_cur, sizeof(int))) == NULL)
		err(1, "calloc");
	if ((got = calloc(hosts->sl_cur, sizeof(int))) == NULL)
		err(1, "calloc");


	for (i = 0; i < nthreads; i++) {
		nleft[i] = nhosts;
		run(&nleft[i]);
	}

	for (done = 0; !done;) {
		done = 1;
		for (i = 0; i < nthreads; i++) {
			if (nleft[i] != 0) {
				done = 0;
				break;
			}
		}
		sleep(1);
	}
	c = 0;
	for (i = 0; i < hosts->sl_cur; i++) {
		if (ask[i] != got[i] && got[i] != 0) {
			warnx("Error: host %s ask %d got %d\n",
			    hosts->sl_str[i], ask[i], got[i]);
			c++;
		}
	}
	free(nleft);
	free(ask);
	free(got);
	sl_free(hosts, 1);
	return c;
}
