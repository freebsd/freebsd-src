/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Copyright (c) 1982, 1986, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/*
 * Compatability shims with the rfc2553 API to simplify ntp.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <ctype.h>
#include <sys/socket.h>
#include "ntp_rfc2553.h"
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#include <netdb.h>

#include "ntpd.h"
#include "ntp_malloc.h"
#include "ntp_stdlib.h"
#include "ntp_string.h"

#ifndef HAVE_IPV6

#if defined(SYS_WINNT)
/* XXX This is the preferred way, but for some reason the SunOS compiler
 * does not like it.
 */
const struct in6_addr in6addr_any = IN6ADDR_ANY_INIT;
#else
const struct in6_addr in6addr_any;
#endif

static char *ai_errlist[] = {
	"Success",
	"Address family for hostname not supported",	/* EAI_ADDRFAMILY */
	"Temporary failure in name resolution",		/* EAI_AGAIN      */
	"Invalid value for ai_flags",		       	/* EAI_BADFLAGS   */
	"Non-recoverable failure in name resolution", 	/* EAI_FAIL       */
	"ai_family not supported",			/* EAI_FAMILY     */
	"Memory allocation failure", 			/* EAI_MEMORY     */
	"No address associated with hostname", 		/* EAI_NODATA     */
	"hostname nor servname provided, or not known",	/* EAI_NONAME     */
	"servname not supported for ai_socktype",	/* EAI_SERVICE    */
	"ai_socktype not supported", 			/* EAI_SOCKTYPE   */
	"System error returned in errno", 		/* EAI_SYSTEM     */
	"Invalid value for hints",			/* EAI_BADHINTS	  */
	"Resolved protocol is unknown",			/* EAI_PROTOCOL   */
	"Unknown error", 				/* EAI_MAX        */
};

static	int ipv4_aton P((const char *, struct sockaddr_storage *));
static	int do_nodename P((const char *nodename, struct addrinfo *ai,
    const struct addrinfo *hints));

int
getaddrinfo (const char *nodename, const char *servname,
	const struct addrinfo *hints, struct addrinfo **res)
{
	int rval;
	struct addrinfo *ai;
	struct sockaddr_in *sockin;

	ai = calloc(sizeof(struct addrinfo), 1);
	if (ai == NULL)
		return (EAI_MEMORY);

	if (nodename != NULL) {
		rval = do_nodename(nodename, ai, hints);
		if (rval != 0) {
			freeaddrinfo(ai);
			return (rval);
		}
	}
	if (nodename == NULL && hints != NULL) {
		ai->ai_addr = calloc(sizeof(struct sockaddr_storage), 1);
		if (ai->ai_addr == NULL) {
			freeaddrinfo(ai);
			return (EAI_MEMORY);
		}
		ai->ai_family = AF_INET;
		ai->ai_addrlen = sizeof(struct sockaddr_storage);
		sockin = (struct sockaddr_in *)ai->ai_addr;
		sockin->sin_family = AF_INET;
		sockin->sin_addr.s_addr = htonl(INADDR_ANY);
#ifdef HAVE_SA_LEN_IN_STRUCT_SOCKADDR
		ai->ai_addr->sa_len = SOCKLEN(ai->ai_addr);
#endif
	}
	if (servname != NULL) {
		ai->ai_family = AF_INET;
		ai->ai_socktype = SOCK_DGRAM;
		if (strcmp(servname, "ntp") != 0) {
			freeaddrinfo(ai);
			return (EAI_SERVICE);
		}
		sockin = (struct sockaddr_in *)ai->ai_addr;
		sockin->sin_port = htons(NTP_PORT);
	}
	*res = ai;
	return (0);
}

void
freeaddrinfo(struct addrinfo *ai)
{
	if (ai->ai_canonname != NULL)
		free(ai->ai_canonname);
	if (ai->ai_addr != NULL)
		free(ai->ai_addr);
	free(ai);
}

int
getnameinfo (const struct sockaddr *sa, u_int salen, char *host,
	size_t hostlen, char *serv, size_t servlen, int flags)
{
	struct hostent *hp;

	if (sa->sa_family != AF_INET)
		return (EAI_FAMILY);
	hp = gethostbyaddr(
	    (const char *)&((const struct sockaddr_in *)sa)->sin_addr,
	    4, AF_INET);
	if (hp == NULL) {
		if (h_errno == TRY_AGAIN)
			return (EAI_AGAIN);
		else
			return (EAI_FAIL);
	}
	if (host != NULL) {
		strncpy(host, hp->h_name, hostlen);
		host[hostlen] = '\0';
	}
	return (0);
}

char *
gai_strerror(int ecode)
{
	if (ecode < 0 || ecode > EAI_MAX)
		ecode = EAI_MAX;
	return ai_errlist[ecode];
}

static int
do_nodename(
	const char *nodename,
	struct addrinfo *ai,
	const struct addrinfo *hints)
{
	struct hostent *hp;
	struct sockaddr_in *sockin;

	ai->ai_addr = calloc(sizeof(struct sockaddr_storage), 1);
	if (ai->ai_addr == NULL)
		return (EAI_MEMORY);

	if (hints != NULL && hints->ai_flags & AI_NUMERICHOST) {
		if (ipv4_aton(nodename,
		    (struct sockaddr_storage *)ai->ai_addr) == 1) {
			ai->ai_family = AF_INET;
			ai->ai_addrlen = sizeof(struct sockaddr_in);
			return (0);
		}
		return (EAI_NONAME);
	}
	hp = gethostbyname(nodename);
	if (hp == NULL) {
		if (h_errno == TRY_AGAIN)
			return (EAI_AGAIN);
		else {
			if (ipv4_aton(nodename,
			    (struct sockaddr_storage *)ai->ai_addr) == 1) {
				ai->ai_family = AF_INET;
				ai->ai_addrlen = sizeof(struct sockaddr_in);
				return (0);
			}
			return (EAI_FAIL);
		}
	}
	ai->ai_family = hp->h_addrtype;
	ai->ai_addrlen = sizeof(struct sockaddr);
	sockin = (struct sockaddr_in *)ai->ai_addr;
	memcpy(&sockin->sin_addr, hp->h_addr, hp->h_length);
	ai->ai_addr->sa_family = hp->h_addrtype;
#ifdef HAVE_SA_LEN_IN_STRUCT_SOCKADDR
	ai->ai_addr->sa_len = sizeof(struct sockaddr);
#endif
	if (hints != NULL && hints->ai_flags & AI_CANONNAME) {
		ai->ai_canonname = malloc(strlen(hp->h_name) + 1);
		if (ai->ai_canonname == NULL)
			return (EAI_MEMORY);
		strcpy(ai->ai_canonname, hp->h_name);
	}
	return (0);
}

/*
 * ipv4_aton - return a net number (this is crude, but careful)
 */
static int
ipv4_aton(
	const char *num,
	struct sockaddr_storage *saddr
	)
{
	const char *cp;
	char *bp;
	int i;
	int temp;
	char buf[80];		/* will core dump on really stupid stuff */
	u_int32 netnum;
	struct sockaddr_in *addr;

	cp = num;
	netnum = 0;
	for (i = 0; i < 4; i++) {
		bp = buf;
		while (isdigit((int)*cp))
			*bp++ = *cp++;
		if (bp == buf)
			break;

		if (i < 3) {
			if (*cp++ != '.')
				break;
		} else if (*cp != '\0')
			break;

		*bp = '\0';
		temp = atoi(buf);
		if (temp > 255)
			break;
		netnum <<= 8;
		netnum += temp;
#ifdef DEBUG
		if (debug > 3)
			printf("ipv4_aton %s step %d buf %s temp %d netnum %lu\n",
			   num, i, buf, temp, (u_long)netnum);
#endif
	}

	if (i < 4) {
#ifdef DEBUG
		if (debug > 3)
			printf(
				"ipv4_aton: \"%s\" invalid host number, line ignored\n",
				num);
#endif
		return (0);
	}

	/*
	 * make up socket address.	Clear it out for neatness.
	 */
	memset((void *)saddr, 0, sizeof(struct sockaddr_storage));
	addr = (struct sockaddr_in *)saddr;
	addr->sin_family = AF_INET;
	addr->sin_port = htons(NTP_PORT);
	addr->sin_addr.s_addr = htonl(netnum);
#ifdef DEBUG
	if (debug > 1)
		printf("ipv4_aton given %s, got %s.\n", num, ntoa(saddr));
#endif
	return (1);
}
#endif /* !HAVE_IPV6 */
