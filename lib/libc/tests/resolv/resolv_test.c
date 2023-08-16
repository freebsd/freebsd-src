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
#include <sys/cdefs.h>
__RCSID("$NetBSD: resolv.c,v 1.6 2004/05/23 16:59:11 christos Exp $");

#include <sys/types.h>
#include <sys/socket.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdatomic.h>
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
static _Atomic(int) *ask = NULL;
static _Atomic(int) *got = NULL;
static bool debug_output = 0;

static void load(const char *);
static void resolvone(long, int, enum method);
static void *resolvloop(void *);
static pthread_t run(int, enum method, long);

#define	DBG(...) do {					\
	if (debug_output)				\
		dprintf(STDOUT_FILENO, __VA_ARGS__);	\
	} while (0)

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
resolv_getaddrinfo(long threadnum, char *host, int port, const char **errstr)
{
	char portstr[6], hbuf[NI_MAXHOST], pbuf[NI_MAXSERV];
	struct addrinfo hints, *res;
	int error;

	snprintf(portstr, sizeof(portstr), "%d", port);
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_flags = AI_PASSIVE;
	hints.ai_socktype = SOCK_STREAM;
	error = getaddrinfo(host, portstr, &hints, &res);
	if (error == 0) {
		DBG("T%ld: host %s ok\n", threadnum, host);
		memset(hbuf, 0, sizeof(hbuf));
		memset(pbuf, 0, sizeof(pbuf));
		getnameinfo(res->ai_addr, res->ai_addrlen, hbuf, sizeof(hbuf),
		    pbuf, sizeof(pbuf), 0);
		DBG("T%ld: reverse %s %s\n", threadnum, hbuf, pbuf);
		freeaddrinfo(res);
	} else {
		*errstr = gai_strerror(error);
		DBG("T%ld: host %s not found: %s\n", threadnum, host, *errstr);
	}
	return error;
}

static int
resolv_gethostby(long threadnum, char *host, const char **errstr)
{
	char buf[1024];
	struct hostent *hp, *hp2;

	hp = gethostbyname(host);
	if (hp) {
		DBG("T%ld: host %s ok\n", threadnum, host);
		memcpy(buf, hp->h_addr, hp->h_length);
		hp2 = gethostbyaddr(buf, hp->h_length, hp->h_addrtype);
		if (hp2) {
			DBG("T%ld: reverse %s\n", threadnum, hp2->h_name);
		}
	} else {
		*errstr = hstrerror(h_errno);
		DBG("T%ld: host %s not found: %s\n", threadnum, host, *errstr);
	}
	return hp ? 0 : h_errno;
}

static int
resolv_getipnodeby(long threadnum, char *host, const char **errstr)
{
	char buf[1024];
	struct hostent *hp, *hp2;
	int error = 0;

	hp = getipnodebyname(host, AF_INET, 0, &error);
	if (hp) {
		DBG("T%ld: host %s ok\n", threadnum, host);
		memcpy(buf, hp->h_addr, hp->h_length);
		hp2 = getipnodebyaddr(buf, hp->h_length, hp->h_addrtype,
		    &error);
		if (hp2) {
			DBG("T%ld: reverse %s\n", threadnum, hp2->h_name);
			freehostent(hp2);
		}
		freehostent(hp);
	} else {
		*errstr = hstrerror(error);
		DBG("T%ld: host %s not found: %s\n", threadnum, host, *errstr);
	}
	return hp ? 0 : error;
}

static void
resolvone(long threadnum, int n, enum method method)
{
	const char* errstr = NULL;
	size_t i = (random() & 0x0fffffff) % hosts->sl_cur;
	char *host = hosts->sl_str[i];
	int error;

	DBG("T%ld: %d resolving %s %zd\n", threadnum, n, host, i);
	switch (method) {
	case METHOD_GETADDRINFO:
		error = resolv_getaddrinfo(threadnum, host, i, &errstr);
		break;
	case METHOD_GETHOSTBY:
		error = resolv_gethostby(threadnum, host, &errstr);
		break;
	case METHOD_GETIPNODEBY:
		error = resolv_getipnodeby(threadnum, host, &errstr);
		break;
	default:
		/* UNREACHABLE */
		/* XXX Needs an __assert_unreachable() for userland. */
		assert(0 && "Unreachable segment reached");
		abort();
		break;
	}
	atomic_fetch_add_explicit(&ask[i], 1, memory_order_relaxed);
	if (error == 0)
		atomic_fetch_add_explicit(&got[i], 1, memory_order_relaxed);
	else if (got[i] != 0)
		fprintf(stderr,
		    "T%ld ERROR after previous success for %s: %d (%s)\n",
		    threadnum, hosts->sl_str[i], error, errstr);
}

struct resolvloop_args {
	int nhosts;
	enum method method;
	long threadnum;
};

static void *
resolvloop(void *p)
{
	struct resolvloop_args *args = p;
	int nhosts = args->nhosts;

	if (nhosts == 0) {
		free(args);
		return NULL;
	}

	do {
		resolvone(args->threadnum, nhosts, args->method);
	} while (--nhosts);
	free(args);
	return (void *)(uintptr_t)nhosts;
}

static pthread_t
run(int nhosts, enum method method, long i)
{
	pthread_t t;
	int rc;
	struct resolvloop_args *args;

	/* Created thread is responsible for free(). */
	args = malloc(sizeof(*args));
	ATF_REQUIRE(args != NULL);

	args->nhosts = nhosts;
	args->method = method;
	args->threadnum = i + 1;
	rc = pthread_create(&t, NULL, resolvloop, args);
	ATF_REQUIRE_MSG(rc == 0, "pthread_create failed: %s", strerror(rc));
	return t;
}

static int
run_tests(const char *hostlist_file, enum method method)
{
	size_t nthreads = NTHREADS;
	pthread_t *threads;
	size_t nhosts = NHOSTS;
	size_t i;
	int c;
	hosts = sl_init();

	srandom(1234);
	debug_output = getenv("DEBUG_OUTPUT") != NULL;

	load(hostlist_file);

	ATF_REQUIRE_MSG(0 < hosts->sl_cur, "0 hosts in %s", hostlist_file);

	ask = calloc(hosts->sl_cur, sizeof(int));
	ATF_REQUIRE(ask != NULL);

	got = calloc(hosts->sl_cur, sizeof(int));
	ATF_REQUIRE(got != NULL);

	threads = calloc(nthreads, sizeof(pthread_t));
	ATF_REQUIRE(threads != NULL);

	for (i = 0; i < nthreads; i++) {
		threads[i] = run(nhosts, method, i);
	}
	/* Wait for all threads to join and check that they checked all hosts */
	for (i = 0; i < nthreads; i++) {
		size_t remaining;

		remaining = (uintptr_t)pthread_join(threads[i], NULL);
		ATF_CHECK_EQ_MSG(0, remaining,
		    "Thread %zd still had %zd hosts to check!", i, remaining);
	}

	c = 0;
	for (i = 0; i < hosts->sl_cur; i++) {
		if (got[i] != 0) {
			ATF_CHECK_EQ_MSG(ask[i], got[i],
			    "Error: host %s ask %d got %d", hosts->sl_str[i],
			    ask[i], got[i]);
			c += ask[i] != got[i];
		}
	}
	free(threads);
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
