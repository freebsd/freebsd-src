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
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stringlist.h>

#include <atf-c.h>

#define NTHREADS	10
#define NHOSTS		100
#define WS		" \t\n\r"

enum method {
	METHOD_GETADDRINFO,
	METHOD_GETHOSTBY,
	METHOD_GETIPNODEBY
};

static StringList *hosts = NULL;
static int *ask = NULL;
static int *got = NULL;

static void load(const char *);
static void resolvone(int, enum method);
static void *resolvloop(void *);
static void run(int *, enum method);

static pthread_mutex_t stats = PTHREAD_MUTEX_INITIALIZER;

static void
load(const char *fname)
{
	FILE *fp;
	size_t linecap;
	char *line;

	fp = fopen(fname, "r");
	ATF_REQUIRE(fp != NULL);
	line = NULL;
	linecap = 0;
	while (getline(&line, &linecap, fp) >= 0) {
		char *ptr;

		for (ptr = strtok(line, WS); ptr; ptr = strtok(NULL, WS)) {
			if (ptr[0] == '#')
				break;
			sl_add(hosts, strdup(ptr));
		}
	}
	free(line);

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
	len = snprintf(buf, sizeof(buf), "%p: host %s %s\n",
	    self, host, error ? "not found" : "ok");
	(void)write(STDOUT_FILENO, buf, len);
	if (error == 0) {
		memset(hbuf, 0, sizeof(hbuf));
		memset(pbuf, 0, sizeof(pbuf));
		getnameinfo(res->ai_addr, res->ai_addrlen, hbuf, sizeof(hbuf),
			    pbuf, sizeof(pbuf), 0);
		len = snprintf(buf, sizeof(buf),
		    "%p: reverse %s %s\n", self, hbuf, pbuf);
		(void)write(STDOUT_FILENO, buf, len);
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
	len = snprintf(buf, sizeof(buf), "%p: host %s %s\n",
	    self, host, (hp == NULL) ? "not found" : "ok");
	(void)write(STDOUT_FILENO, buf, len);
	if (hp) {
		memcpy(buf, hp->h_addr, hp->h_length);
		hp2 = gethostbyaddr(buf, hp->h_length, hp->h_addrtype);
		if (hp2) {
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
	len = snprintf(buf, sizeof(buf), "%p: host %s %s\n",
	    self, host, (hp == NULL) ? "not found" : "ok");
	(void)write(STDOUT_FILENO, buf, len);
	if (hp) {
		memcpy(buf, hp->h_addr, hp->h_length);
		hp2 = getipnodebyaddr(buf, hp->h_length, hp->h_addrtype,
		    &h_error);
		if (hp2) {
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
resolvone(int n, enum method method)
{
	char buf[1024];
	pthread_t self = pthread_self();
	size_t i = (random() & 0x0fffffff) % hosts->sl_cur;
	char *host = hosts->sl_str[i];
	int error, len;

	len = snprintf(buf, sizeof(buf), "%p: %d resolving %s %d\n",
	    self, n, host, (int)i);
	(void)write(STDOUT_FILENO, buf, len);
	error = 0;
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
		/* UNREACHABLE */
		/* XXX Needs an __assert_unreachable() for userland. */
		assert(0 && "Unreachable segment reached");
		abort();
		break;
	}
	pthread_mutex_lock(&stats);
	ask[i]++;
	got[i] += error == 0;
	pthread_mutex_unlock(&stats);
}

struct resolvloop_args {
	int *nhosts;
	enum method method;
};

static void *
resolvloop(void *p)
{
	struct resolvloop_args *args = p;

	if (*args->nhosts == 0) {
		free(args);
		return NULL;
	}

	do
		resolvone(*args->nhosts, args->method);
	while (--(*args->nhosts));
	free(args);
	return NULL;
}

static void
run(int *nhosts, enum method method)
{
	pthread_t self;
	int rc;
	struct resolvloop_args *args;

	/* Created thread is responsible for free(). */
	args = malloc(sizeof(*args));
	ATF_REQUIRE(args != NULL);

	args->nhosts = nhosts;
	args->method = method;
	self = pthread_self();
	rc = pthread_create(&self, NULL, resolvloop, args);
	ATF_REQUIRE_MSG(rc == 0, "pthread_create failed: %s", strerror(rc));
}

static int
run_tests(const char *hostlist_file, enum method method)
{
	size_t nthreads = NTHREADS;
	size_t nhosts = NHOSTS;
	size_t i;
	int c, done, *nleft;
	hosts = sl_init();

	srandom(1234);

	load(hostlist_file);

	ATF_REQUIRE_MSG(0 < hosts->sl_cur, "0 hosts in %s", hostlist_file);

	nleft = malloc(nthreads * sizeof(int));
	ATF_REQUIRE(nleft != NULL);

	ask = calloc(hosts->sl_cur, sizeof(int));
	ATF_REQUIRE(ask != NULL);

	got = calloc(hosts->sl_cur, sizeof(int));
	ATF_REQUIRE(got != NULL);

	for (i = 0; i < nthreads; i++) {
		nleft[i] = nhosts;
		run(&nleft[i], method);
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
			printf("Error: host %s ask %d got %d\n",
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

#define	HOSTLIST_FILE	"mach"

#define	RUN_TESTS(tc, method) \
do {									\
	char *_hostlist_file;						\
	ATF_REQUIRE(0 < asprintf(&_hostlist_file, "%s/%s",		\
	    atf_tc_get_config_var(tc, "srcdir"), HOSTLIST_FILE));	\
	ATF_REQUIRE(run_tests(_hostlist_file, method) == 0);		\
} while(0)

ATF_TC(getaddrinfo_test);
ATF_TC_HEAD(getaddrinfo_test, tc) {
	atf_tc_set_md_var(tc, "timeout", "1200");
}
ATF_TC_BODY(getaddrinfo_test, tc)
{

	RUN_TESTS(tc, METHOD_GETADDRINFO);
}

ATF_TC(gethostby_test);
ATF_TC_HEAD(gethostby_test, tc) {
	atf_tc_set_md_var(tc, "timeout", "1200");
}
ATF_TC_BODY(gethostby_test, tc)
{

	RUN_TESTS(tc, METHOD_GETHOSTBY);
}

ATF_TC(getipnodeby_test);
ATF_TC_HEAD(getipnodeby_test, tc) {

	atf_tc_set_md_var(tc, "timeout", "1200");
}
ATF_TC_BODY(getipnodeby_test, tc)
{

	RUN_TESTS(tc, METHOD_GETIPNODEBY);
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, getaddrinfo_test);
	ATF_TP_ADD_TC(tp, gethostby_test);
	ATF_TP_ADD_TC(tp, getipnodeby_test);

	return (atf_no_error());
}
