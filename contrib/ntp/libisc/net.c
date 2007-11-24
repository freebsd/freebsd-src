/*
 * Copyright (C) 1999-2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: net.c,v 1.25 2001/11/30 01:59:45 gson Exp $ */

#include <config.h>

#include <errno.h>
#include <unistd.h>

#include <isc/net.h>
#include <isc/strerror.h>
#include <isc/string.h>
#include <isc/util.h>

#if defined(ISC_PLATFORM_HAVEIPV6) && defined(ISC_PLATFORM_NEEDIN6ADDRANY)
const struct in6_addr isc_net_in6addrany = IN6ADDR_ANY_INIT;
#endif

static isc_boolean_t 	once = ISC_FALSE;
static isc_result_t	ipv4_result = ISC_R_NOTFOUND;
static isc_result_t	ipv6_result = ISC_R_NOTFOUND;

static isc_result_t
try_proto(int domain) {
	int s;
	isc_result_t result = ISC_R_SUCCESS;
	char strbuf[ISC_STRERRORSIZE];

	s = socket(domain, SOCK_STREAM, 0);
	if (s == -1) {
		switch (errno) {
#ifdef EAFNOSUPPORT
		case EAFNOSUPPORT:
#endif
#ifdef EPROTONOSUPPORT
		case EPROTONOSUPPORT:
#endif
#ifdef EINVAL
		case EINVAL:
#endif
			return (ISC_R_NOTFOUND);
		default:
			isc__strerror(errno, strbuf, sizeof(strbuf));
			UNEXPECTED_ERROR(__FILE__, __LINE__,
					 "socket() %s failed",
					 strbuf);
			return (ISC_R_UNEXPECTED);
		}
	}

#ifdef ISC_PLATFORM_HAVEIPV6
#ifdef ISC_PLATFORM_HAVEIN6PKTINFO
	if (domain == PF_INET6) {
		struct sockaddr_in6 sin6;
		unsigned int len;

		/*
		 * Check to see if IPv6 is broken, as is common on Linux.
		 */
		len = sizeof(sin6);
		if (getsockname(s, (struct sockaddr *)&sin6, (void *)&len) < 0)
		{
			result = ISC_R_NOTFOUND;
		} else {
			if (len == sizeof(struct sockaddr_in6))
				result = ISC_R_SUCCESS;
			else {
				result = ISC_R_NOTFOUND;
			}
		}
	}
#endif
#endif

	(void)close(s);

	return (result);
}

static void
initialize_action(void) {
	ipv4_result = try_proto(PF_INET);
#ifdef ISC_PLATFORM_HAVEIPV6
#ifdef ISC_PLATFORM_HAVEIN6PKTINFO
	ipv6_result = try_proto(PF_INET6);
#endif
#endif
}

static void
initialize(void) {
	if(once == ISC_FALSE) {
		initialize_action();
		once = ISC_TRUE;
	}
}

isc_result_t
isc_net_probeipv4(void) {
	initialize();
	return (ipv4_result);
}

isc_result_t
isc_net_probeipv6(void) {
	initialize();
	return (ipv6_result);
}
