/*-
 * Copyright (c) 1999 Brian Somers <brian@Awfulhak.org>
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
 * $FreeBSD: src/lib/libutil/realhostname.c,v 1.6.2.3 2000/07/18 10:25:37 ume Exp $
 */

#include <sys/param.h>

#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <stdio.h>
#include <string.h>

#include "libutil.h"

/* wrapper for KAME-special getnameinfo() */
#ifndef NI_WITHSCOPEID
#define	NI_WITHSCOPEID	0
#endif

struct sockinet {
	u_char	si_len;
	u_char	si_family;
	u_short	si_port;
};

int
realhostname(char *host, size_t hsize, const struct in_addr *ip)
{
	int result;
	struct hostent *hp;

	result = HOSTNAME_INVALIDADDR;
	hp = gethostbyaddr((char *)ip, sizeof(*ip), AF_INET);

	if (hp != NULL && strlen(hp->h_name) <= hsize) {
		char lookup[MAXHOSTNAMELEN];

		strncpy(lookup, hp->h_name, sizeof(lookup) - 1);
		lookup[sizeof(lookup) - 1] = '\0';
		hp = gethostbyname(lookup);
		if (hp == NULL)
			result = HOSTNAME_INVALIDNAME;
		else for (; ; hp->h_addr_list++) {
			if (hp->h_addr_list[0] == NULL) {
				result = HOSTNAME_INCORRECTNAME;
				break;
			}
			if (!memcmp(*hp->h_addr_list, ip, sizeof(*ip))) {
				strncpy(host, lookup, hsize);
				return HOSTNAME_FOUND;
			}
		}
	}

	strncpy(host, inet_ntoa(*ip), hsize);

	return result;
}

int
realhostname_sa(char *host, size_t hsize, struct sockaddr *addr, int addrlen)
{
	int result, error;
	char buf[NI_MAXHOST];

	result = HOSTNAME_INVALIDADDR;

	error = getnameinfo(addr, addrlen, buf, sizeof(buf), NULL, 0, 0);
	if (error == 0) {
		struct addrinfo hints, *res, *ores;
		struct sockaddr *sa;

		memset(&hints, 0, sizeof(struct addrinfo));
		hints.ai_family =
			(addr->sa_family == AF_INET) ? AF_INET : AF_UNSPEC;
		hints.ai_flags = AI_CANONNAME;

		error = getaddrinfo(buf, NULL, &hints, &res);
		if (error) {
			result = HOSTNAME_INVALIDNAME;
			goto numeric;
		} else for (ores = res; ; res = res->ai_next) {
			if (res == NULL) {
				freeaddrinfo(ores);
				result = HOSTNAME_INCORRECTNAME;
				goto numeric;
			}
			sa = res->ai_addr;
			if (sa == NULL) {
				freeaddrinfo(ores);
				result = HOSTNAME_INCORRECTNAME;
				goto numeric;
			}
			if (sa->sa_len == addrlen &&
			    sa->sa_family == addr->sa_family) {
				u_int16_t port;

				port = ((struct sockinet *)addr)->si_port;
				((struct sockinet *)addr)->si_port = 0;
				if (!memcmp(sa, addr, sa->sa_len)) {
					result = HOSTNAME_FOUND;
					((struct sockinet *)addr)->si_port =
						port;
					if (ores->ai_canonname == 0) {
						freeaddrinfo(ores);
						goto numeric;
					}
					if (strlen(ores->ai_canonname) > hsize) {
						if (addr->sa_family == AF_INET) {
							freeaddrinfo(ores);
							goto numeric;
						}
						strncpy(buf,
							ores->ai_canonname,
							sizeof(buf));
						trimdomain(buf, hsize);
						strncpy(host, buf, hsize);
					} else
						strncpy(host,
							ores->ai_canonname,
							hsize);
					break;
				}
				((struct sockinet *)addr)->si_port = port;
			}
#ifdef INET6
			/*
			 * XXX IPv4 mapped IPv6 addr consideraton,
			 * specified in rfc2373.
			 */
			if (sa->sa_family == AF_INET &&
			    addr->sa_family == AF_INET6) {
				struct in_addr *in;
				struct in6_addr *in6;

				in = &((struct sockaddr_in *)sa)->sin_addr;
				in6 = &((struct sockaddr_in6 *)addr)->sin6_addr;
				if (IN6_IS_ADDR_V4MAPPED(in6) &&
				    !memcmp(&in6->s6_addr[12], in,
					    sizeof(*in))) {
					result = HOSTNAME_FOUND;
					if (ores->ai_canonname == 0 ||
					    strlen(ores->ai_canonname) > hsize) {
						freeaddrinfo(ores);
						goto numeric;
					}
					strncpy(host, ores->ai_canonname,
						hsize);
					break;
				}
			}
#endif
		}
		freeaddrinfo(ores);
	} else {
		struct sockaddr_in sin;
    numeric:
#ifdef INET6
		if (addr->sa_family == AF_INET6) {
			struct sockaddr_in6 *sin6;

			sin6 = (struct sockaddr_in6 *)addr;
			if (IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr)) {
				memset(&sin, 0, sizeof(sin));
				sin.sin_len = sizeof(struct sockaddr_in);
				sin.sin_family = AF_INET;
				sin.sin_port = sin6->sin6_port;
				memcpy(&sin.sin_addr,
				       &sin6->sin6_addr.s6_addr[12],
				       sizeof(struct in_addr));
				addr = (struct sockaddr *)&sin;
				addrlen = sin.sin_len;
			}
		}
#endif
		if (getnameinfo(addr, addrlen, buf, sizeof(buf), NULL, 0,
				NI_NUMERICHOST|NI_WITHSCOPEID) == 0)
			strncpy(host, buf, hsize);
	}

	return result;
}


