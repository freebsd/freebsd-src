/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Gleb Smirnoff <glebius@FreeBSD.org>
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
 */

#include <dlfcn.h>
#include <errno.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <resolv.h>

#include <atf-c.h>

static const char goodname[] = "www.freebsd.org";
static const char goodname_dot[] = "www.freebsd.org.";
static const char badname[] = "does-not-exist.freebsd.org";
static const char badname_dot[] = "does-not-exist.freebsd.org.";
static const char ipv6onlyname[] = "beefy15.nyi.freebsd.org";
static const char ipv6onlyname_dot[] = "beefy15.nyi.freebsd.org.";
static const char ipv4onlyname[] = "ipv4only.arpa";
static const char ipv4onlyname_dot[] = "ipv4only.arpa.";
/*
 * We need an IP address that doesn't exist, but not reported with ICMP
 * unreachable by the nearest router.  Let's try TEST-NET-3.
 */
static char badresolvconf[] =	"nameserver 203.0.113.1";
static char badresolvconf2[] =	"nameserver 203.0.113.1\n"
				"nameserver 203.0.113.2";
static char *resconf = NULL;
FILE *
fopen(const char * restrict path, const char * restrict mode)
{
	static FILE *(*orig)(const char *, const char *);

	if (orig == NULL && (orig = dlsym(RTLD_NEXT, "fopen")) == NULL)
		atf_libc_error(ENOENT, "dlsym(fopen): %s", dlerror());
	if (resconf != NULL && strcmp(path, _PATH_RESCONF) == 0)
		return (fmemopen(resconf, strlen(resconf), mode));
	else
		return (orig(path, mode));
}

static int send_error = 0;
ssize_t
send(int s, const void *msg, size_t len, int flags)
{
	static ssize_t (*orig)(int, const void *, size_t, int);

	if (orig == NULL && (orig = dlsym(RTLD_NEXT, "send")) == NULL)
		atf_libc_error(ENOENT, "dlsym(send): %s", dlerror());
	if (send_error != 0) {
		errno = send_error;
		return (-1);
	} else {
		return (orig(s, msg, len, flags));
	}
}

ATF_TC(basic);
ATF_TC_HEAD(basic, tc)
{
	atf_tc_set_md_var(tc, "require.config", "allow_network_access");
}

ATF_TC_BODY(basic, tc)
{
	static const struct addrinfo hints = {
		.ai_family = AF_UNSPEC,
		.ai_flags = AI_CANONNAME,
	};
	struct addrinfo *res;
	int rv;

	rv = getaddrinfo(goodname, NULL, &hints, &res);
	ATF_REQUIRE_MSG(rv == 0,
	    "Expected 0, got %d (%s)", rv, gai_strerror(rv));
	freeaddrinfo(res);

	rv = getaddrinfo(goodname_dot, NULL, &hints, &res);
	ATF_REQUIRE_MSG(rv == 0,
	    "Expected 0, got %d (%s)", rv, gai_strerror(rv));
	freeaddrinfo(res);

	rv = getaddrinfo(badname, NULL, &hints, &res);
	ATF_REQUIRE_MSG(rv == EAI_NONAME,
	    "Expected %d (EAI_NONAME), got %d (%s)",
	    EAI_NONAME, rv, gai_strerror(rv));

	rv = getaddrinfo(badname_dot, NULL, &hints, &res);
	ATF_REQUIRE_MSG(rv == EAI_NONAME,
	    "Expected %d (EAI_NONAME), got %d (%s)",
	    EAI_NONAME, rv, gai_strerror(rv));
}

ATF_TC_WITHOUT_HEAD(timeout);
ATF_TC_BODY(timeout, tc)
{
	static const struct addrinfo hints = {
		.ai_family = AF_UNSPEC,
		.ai_flags = AI_CANONNAME,
	};
	struct addrinfo *res;
	int rv;

	resconf = badresolvconf;
	rv = getaddrinfo(goodname, NULL, &hints, &res);
	ATF_REQUIRE_MSG(rv == EAI_AGAIN,
	    "Expected %d (EAI_AGAIN), got %d (%s)",
	    EAI_AGAIN, rv, gai_strerror(rv));

	rv = getaddrinfo(goodname_dot, NULL, &hints, &res);
	ATF_REQUIRE_MSG(rv == EAI_AGAIN,
	    "Expected %d (EAI_AGAIN), got %d (%s)",
	    EAI_AGAIN, rv, gai_strerror(rv));
}

ATF_TC_WITHOUT_HEAD(timeout_specific);
ATF_TC_BODY(timeout_specific, tc)
{
	static const struct addrinfo hints = {
		.ai_family = AF_INET,
		.ai_socktype = SOCK_STREAM,
		.ai_flags = AI_CANONNAME,
	};
	struct addrinfo *res;
	int rv;

	resconf = badresolvconf;
	rv = getaddrinfo(goodname, "666", &hints, &res);
	ATF_REQUIRE_MSG(rv == EAI_AGAIN,
	    "Expected %d (EAI_AGAIN), got %d (%s)",
	    EAI_AGAIN, rv, gai_strerror(rv));

	rv = getaddrinfo(goodname_dot, "666", &hints, &res);
	ATF_REQUIRE_MSG(rv == EAI_AGAIN,
	    "Expected %d (EAI_AGAIN), got %d (%s)",
	    EAI_AGAIN, rv, gai_strerror(rv));
}

ATF_TC_WITHOUT_HEAD(timeout2);
ATF_TC_BODY(timeout2, tc)
{
	static const struct addrinfo hints = {
		.ai_family = AF_UNSPEC,
		.ai_flags = AI_CANONNAME,
	};
	struct addrinfo *res;
	int rv;

	resconf = badresolvconf2;
	rv = getaddrinfo(goodname, NULL, &hints, &res);
	ATF_REQUIRE_MSG(rv == EAI_AGAIN,
	    "Expected %d (EAI_AGAIN), got %d (%s)",
	    EAI_AGAIN, rv, gai_strerror(rv));

	rv = getaddrinfo(goodname_dot, NULL, &hints, &res);
	ATF_REQUIRE_MSG(rv == EAI_AGAIN,
	    "Expected %d (EAI_AGAIN), got %d (%s)",
	    EAI_AGAIN, rv, gai_strerror(rv));
}

/*
 * Emulate interface/network down.
 */
ATF_TC_WITHOUT_HEAD(netdown);
ATF_TC_BODY(netdown, tc)
{
	static const struct addrinfo hints = {
		.ai_family = AF_UNSPEC,
		.ai_flags = AI_CANONNAME,
	};
	struct addrinfo *res;
	int rv;

	send_error = ENETDOWN;
	rv = getaddrinfo(goodname, NULL, &hints, &res);
	ATF_REQUIRE_MSG(rv == EAI_AGAIN,
	    "Expected %d (EAI_AGAIN), got %d (%s)",
	    EAI_AGAIN, rv, gai_strerror(rv));

	rv = getaddrinfo(goodname_dot, NULL, &hints, &res);
	ATF_REQUIRE_MSG(rv == EAI_AGAIN,
	    "Expected %d (EAI_AGAIN), got %d (%s)",
	    EAI_AGAIN, rv, gai_strerror(rv));
}

/*
 * See https://reviews.freebsd.org/D37139.
 */
ATF_TC(nofamily);
ATF_TC_HEAD(nofamily, tc)
{
	atf_tc_set_md_var(tc, "require.config", "allow_network_access");
}
ATF_TC_BODY(nofamily, tc)
{
	static const struct addrinfo hints4 = {
		.ai_family = AF_INET,
		.ai_flags = AI_CANONNAME,
	}, hints6 = {
		.ai_family = AF_INET6,
		.ai_flags = AI_CANONNAME,
	};
	struct addrinfo *res;
	int rv;

	rv = getaddrinfo(ipv6onlyname, NULL, &hints4, &res);
	ATF_REQUIRE_MSG(rv == EAI_ADDRFAMILY,
	    "Expected %d (EAI_ADDRFAMILY), got %d (%s)",
	    EAI_ADDRFAMILY, rv, gai_strerror(rv));

	rv = getaddrinfo(ipv6onlyname_dot, NULL, &hints4, &res);
	ATF_REQUIRE_MSG(rv == EAI_ADDRFAMILY,
	    "Expected %d (EAI_ADDRFAMILY), got %d (%s)",
	    EAI_ADDRFAMILY, rv, gai_strerror(rv));

	rv = getaddrinfo(ipv4onlyname, NULL, &hints6, &res);
	ATF_REQUIRE_MSG(rv == EAI_ADDRFAMILY,
	    "Expected %d (EAI_ADDRFAMILY), got %d (%s)",
	    EAI_ADDRFAMILY, rv, gai_strerror(rv));

	rv = getaddrinfo(ipv4onlyname_dot, NULL, &hints6, &res);
	ATF_REQUIRE_MSG(rv == EAI_ADDRFAMILY,
	    "Expected %d (EAI_ADDRFAMILY), got %d (%s)",
	    EAI_ADDRFAMILY, rv, gai_strerror(rv));

	rv = getaddrinfo(badname, NULL, &hints4, &res);
	ATF_REQUIRE_MSG(rv == EAI_NONAME,
	    "Expected %d (EAI_NONAME), got %d (%s)",
	    EAI_NONAME, rv, gai_strerror(rv));

	rv = getaddrinfo(badname_dot, NULL, &hints6, &res);
	ATF_REQUIRE_MSG(rv == EAI_NONAME,
	    "Expected %d (EAI_NONAME), got %d (%s)",
	    EAI_NONAME, rv, gai_strerror(rv));
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, basic);
	ATF_TP_ADD_TC(tp, timeout);
	ATF_TP_ADD_TC(tp, timeout_specific);
	ATF_TP_ADD_TC(tp, timeout2);
	ATF_TP_ADD_TC(tp, netdown);
	ATF_TP_ADD_TC(tp, nofamily);

	return (atf_no_error());
}
