/*
 * Issues to be discussed:
 * - Thread safe-ness must be checked
 * - Return values.  There seems to be no standard for return value (RFC2133)
 *   but INRIA implementation returns EAI_xxx defined for getaddrinfo().
 */

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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by WIDE Project and
 *    its contributors.
 * 4. Neither the name of the project nor the names of its contributors
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

#include <port_before.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>

#include <netdb.h>
#include <resolv.h>
#include <string.h>

#include <port_after.h>

#define SUCCESS 0
#define ANY 0
#define YES 1
#define NO  0

/*
 * Note that a_off will be dynamically adjusted so that to be consistent
 * with the definition of sockaddr_in{,6}.
 * The value presented below is just a guess.
 */
static struct afd {
	int a_af;
	int a_addrlen;
	int a_socklen;
	int a_off;
} afdl [] = {
	/* first entry is linked last... */
	{PF_INET, sizeof(struct in_addr), sizeof(struct sockaddr_in),
		4 /*XXX*/},
	{PF_INET6, sizeof(struct in6_addr), sizeof(struct sockaddr_in6),
		8 /*XXX*/},
	{0, 0, 0},
};

struct sockinet {
	u_char	si_len;
	u_char	si_family;
	u_short	si_port;
};

#define ENI_NOSOCKET 	0
#define ENI_NOSERVNAME	1
#define ENI_NOHOSTNAME	2
#define ENI_MEMORY	3
#define ENI_SYSTEM	4
#define ENI_FAMILY	5
#define ENI_SALEN	6

int
getnameinfo(sa, salen, host, hostlen, serv, servlen, flags)
	const struct sockaddr *sa;
	size_t salen;
	char *host;
	size_t hostlen;
	char *serv;
	size_t servlen;
	int flags;
{
	struct afd *afd;
	struct servent *sp;
	struct hostent *hp;
	u_short port;
#ifdef HAVE_SA_LEN
	int len;
#endif
	int family, i;
	char *addr, *p;
	u_char pfx;
	static int firsttime = 1;
	static char numserv[512];
	static char numaddr[512];


	/* dynamically adjust a_off */
	if (firsttime) {
		struct afd *p;
		u_char *q;
		struct sockaddr_in sin;
		struct sockaddr_in6 sin6;

		for (p = &afdl[0]; p->a_af; p++) {
			switch (p->a_af) {
			case PF_INET:
				q = (u_char *)&sin.sin_addr.s_addr;
				p->a_off = q - (u_char *)&sin;
				break;
			case PF_INET6:
				q = (u_char *)&sin6.sin6_addr.s6_addr;
				p->a_off = q - (u_char *)&sin6;
				break;
			default:
				break;
			}
		}
		firsttime = 0;
	}

	if (sa == NULL)
		return ENI_NOSOCKET;

#ifdef HAVE_SA_LEN
	len = sa->sa_len;
	if (len != salen) return ENI_SALEN;
#endif
	
	family = sa->sa_family;
	for (i = 0; afdl[i].a_af; i++)
		if (afdl[i].a_af == family) {
			afd = &afdl[i];
			goto found;
		}
	return ENI_FAMILY;
	
 found:
	if (salen != afd->a_socklen) return ENI_SALEN;
	
	port = ((struct sockinet *)sa)->si_port; /* network byte order */
	addr = (char *)sa + afd->a_off;

	if (serv == NULL || servlen == 0) {
		/* what we should do? */
	} else if (flags & NI_NUMERICSERV) {
		snprintf(numserv, sizeof(numserv), "%d", ntohs(port));
		if (strlen(numserv) > servlen)
			return ENI_MEMORY;
		strcpy(serv, numserv);
	} else {
		sp = getservbyport(port, (flags & NI_DGRAM) ? "udp" : "tcp");
		if (sp) {
			if (strlen(sp->s_name) + 1 > servlen)
				return ENI_MEMORY;
			strcpy(serv, sp->s_name);
		} else
			return ENI_NOSERVNAME;
	}

	switch (sa->sa_family) {
	case AF_INET:
		if (ntohl(*(u_long *)addr) >> IN_CLASSA_NSHIFT == 0)
			flags |= NI_NUMERICHOST;			
		break;
	case AF_INET6:
		pfx = *addr;
		if (pfx == 0 || pfx == 0xfe || pfx == 0xff)
			flags |= NI_NUMERICHOST;
		break;
	}
	if (host == NULL || hostlen == 0) {
		/* what should we do? */
	} else if (flags & NI_NUMERICHOST) {
		if (inet_ntop(afd->a_af, addr, numaddr, sizeof(numaddr))
		    == NULL)
			return ENI_SYSTEM;
		if (strlen(numaddr) + 1 > hostlen)
			return ENI_MEMORY;
		strcpy(host, numaddr);
	} else {
		hp = gethostbyaddr(addr, afd->a_addrlen, afd->a_af);

		if (hp) {
			if (flags & NI_NOFQDN) {
				p = strchr(hp->h_name, '.');
				if (p) *p = '\0';
			}
			if (strlen(hp->h_name) + 1 > hostlen)
				return ENI_MEMORY;
			strcpy(host, hp->h_name);
		} else {
			if (flags & NI_NAMEREQD)
				return ENI_NOHOSTNAME;
			if (inet_ntop(afd->a_af, addr, numaddr, sizeof(numaddr))
			    == NULL)
				return ENI_NOHOSTNAME;
			if (strlen(numaddr) + 1 > hostlen)
				return ENI_MEMORY;
			strcpy(host, numaddr);
		}
	}
	return SUCCESS;
}
