/*
 * Issues to be discussed:
 * - Thread safe-ness must be checked
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
#include <stddef.h>

#include <port_after.h>

/*
 * Note that a_off will be dynamically adjusted so that to be consistent
 * with the definition of sockaddr_in{,6}.
 * The value presented below is just a guess.
 */
static struct afd {
	int a_af;
	int a_addrlen;
	size_t a_socklen;
	int a_off;
} afdl [] = {
	/* first entry is linked last... */
	{PF_INET, sizeof(struct in_addr), sizeof(struct sockaddr_in),
	 offsetof(struct sockaddr_in, sin_addr)},
	{PF_INET6, sizeof(struct in6_addr), sizeof(struct sockaddr_in6),
	 offsetof(struct sockaddr_in6, sin6_addr)},
	{0, 0, 0, 0},
};

struct sockinet {
#ifdef HAVE_SA_LEN
	u_char	si_len;
#endif
	u_char	si_family;
	u_short	si_port;
};

static int ip6_parsenumeric __P((const struct sockaddr *, const char *, char *,
				 size_t, int));
#ifdef HAVE_SIN6_SCOPE_ID
static int ip6_sa2str __P((const struct sockaddr_in6 *, char *, size_t, int));
#endif

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
	size_t len;
#endif
	int family, i;
	const char *addr;
	char *p;
	u_char pfx;
	char numserv[512];
	char numaddr[512];

	if (sa == NULL)
		return EAI_FAIL;

#ifdef HAVE_SA_LEN
	len = sa->sa_len;
	if (len != salen) return EAI_FAIL;
#endif

	family = sa->sa_family;
	for (i = 0; afdl[i].a_af; i++)
		if (afdl[i].a_af == family) {
			afd = &afdl[i];
			goto found;
		}
	return EAI_FAMILY;

 found:
	if (salen != afd->a_socklen) return EAI_FAIL;

	port = ((const struct sockinet *)sa)->si_port; /* network byte order */
	addr = (const char *)sa + afd->a_off;

	if (serv == NULL || servlen == 0) {
		/*
		 * rfc2553bis says that serv == NULL or servlen == 0 means that
		 * the caller does not want the result.
		 */
	} else if (flags & NI_NUMERICSERV) {
		sprintf(numserv, "%d", ntohs(port));
		if (strlen(numserv) > servlen)
			return EAI_MEMORY;
		strcpy(serv, numserv);
	} else {
		sp = getservbyport(port, (flags & NI_DGRAM) ? "udp" : "tcp");
		if (sp) {
			if (strlen(sp->s_name) + 1 > servlen)
				return EAI_MEMORY;
			strcpy(serv, sp->s_name);
		} else
			return EAI_NONAME;
	}

	switch (sa->sa_family) {
	case AF_INET:
		if (ntohl(*(const u_long *)addr) >> IN_CLASSA_NSHIFT == 0)
			flags |= NI_NUMERICHOST;			
		break;
	case AF_INET6:
		pfx = *addr;
		if (pfx == 0 || pfx == 0xfe || pfx == 0xff)
			flags |= NI_NUMERICHOST;
		break;
	}
	if (host == NULL || hostlen == 0) {
		/*
		 * rfc2553bis says that host == NULL or hostlen == 0 means that
		 * the caller does not want the result.
		 */
	} else if (flags & NI_NUMERICHOST) {
		goto numeric;
	} else {
		hp = gethostbyaddr(addr, afd->a_addrlen, afd->a_af);

		if (hp) {
			if (flags & NI_NOFQDN) {
				p = strchr(hp->h_name, '.');
				if (p) *p = '\0';
			}
			if (strlen(hp->h_name) + 1 > hostlen)
				return EAI_MEMORY;
			strcpy(host, hp->h_name);
		} else {
			if (flags & NI_NAMEREQD)
				return EAI_NONAME;
		  numeric:
			switch(afd->a_af) {
			case AF_INET6:
			{
				int error;

				if ((error = ip6_parsenumeric(sa, addr, host,
							      hostlen,
							      flags)) != 0)
					return(error);
				break;
			}

			default:
				if (inet_ntop(afd->a_af, addr, numaddr,
					      sizeof(numaddr)) == NULL)
					return EAI_NONAME;
				if (strlen(numaddr) + 1 > hostlen)
					return EAI_MEMORY;
				strcpy(host, numaddr);
			}
		}
	}
	return(0);
}

static int
ip6_parsenumeric(const struct sockaddr *sa, const char *addr, char *host,
		 size_t hostlen, int flags)
{
	size_t numaddrlen;
	char numaddr[512];

#ifndef HAVE_SIN6_SCOPE_ID
	UNUSED(sa);
	UNUSED(flags);
#endif

	if (inet_ntop(AF_INET6, addr, numaddr, sizeof(numaddr))
	    == NULL)
		return EAI_SYSTEM;

	numaddrlen = strlen(numaddr);
	if (numaddrlen + 1 > hostlen) /* don't forget terminator */
		return EAI_MEMORY;
	strcpy(host, numaddr);

#ifdef HAVE_SIN6_SCOPE_ID
	if (((const struct sockaddr_in6 *)sa)->sin6_scope_id) {
		char scopebuf[MAXHOSTNAMELEN]; /* XXX */
		int scopelen;

		/* ip6_sa2str never fails */
		scopelen = ip6_sa2str((const struct sockaddr_in6 *)sa,
				      scopebuf, sizeof(scopebuf), flags);

		if (scopelen + 1 + numaddrlen + 1 > hostlen)
			return EAI_MEMORY;

		/* construct <numeric-addr><delim><scopeid> */
		memcpy(host + numaddrlen + 1, scopebuf,
		       scopelen);
		host[numaddrlen] = SCOPE_DELIMITER;
		host[numaddrlen + 1 + scopelen] = '\0';
	}
#endif

	return 0;
}

#ifdef HAVE_SIN6_SCOPE_ID
/* ARGSUSED */
static int
ip6_sa2str(const struct sockaddr_in6 *sa6, char *buf,
	   size_t bufsiz, int flags)
{
#ifdef USE_IFNAMELINKID
	unsigned int ifindex = (unsigned int)sa6->sin6_scope_id;
	const struct in6_addr *a6 = &sa6->sin6_addr;
#endif
	char tmp[64];

#ifdef NI_NUMERICSCOPE
	if (flags & NI_NUMERICSCOPE) {
		sprintf(tmp, "%u", sa6->sin6_scope_id);
		if (bufsiz != 0) {
			strncpy(buf, tmp, bufsiz - 1);
			buf[bufsiz - 1] = '\0';
		}
		return(strlen(tmp));
	}
#endif

#ifdef USE_IFNAMELINKID
	/*
	 * For a link-local address, convert the index to an interface
	 * name, assuming a one-to-one mapping between links and interfaces.
	 * Note, however, that this assumption is stronger than the
	 * specification of the scoped address architecture;  the
	 * specficication says that more than one interfaces can belong to
	 * a single link.
	 */

	/* if_indextoname() does not take buffer size.  not a good api... */
	if ((IN6_IS_ADDR_LINKLOCAL(a6) || IN6_IS_ADDR_MC_LINKLOCAL(a6)) &&
	    bufsiz >= IF_NAMESIZE) {
		char *p = if_indextoname(ifindex, buf);
		if (p) {
			return(strlen(p));
		}
	}
#endif

	/* last resort */
	sprintf(tmp, "%u", sa6->sin6_scope_id);
	if (bufsiz != 0) {
		strncpy(buf, tmp, bufsiz - 1);
		buf[bufsiz - 1] = '\0';
	}
	return(strlen(tmp));
}
#endif
