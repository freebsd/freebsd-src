/*-
 * Copyright (c) 1997 Berkeley Software Design, Inc. All rights reserved.
 * The Berkeley Software Design Inc. software License Agreement specifies
 * the terms and conditions for redistribution.
 *
 *	BSDI $Id: getaddrinfo.c,v 8.3 1999/06/11 01:25:58 vixie Exp $
 */

#include <port_before.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <arpa/inet.h>
#include <port_after.h>

#define SA(addr)	((struct sockaddr *)(addr))
#define SIN(addr)	((struct sockaddr_in *)(addr))
#define SIN6(addr)	((struct sockaddr_in6 *)(addr))
#define SUN(addr)	((struct sockaddr_un *)(addr))

static struct addrinfo
	*ai_reverse(struct addrinfo *oai),
	*ai_clone(struct addrinfo *oai, int family),
	*ai_alloc(int family, int addrlen);
#ifdef AF_LOCAL
static int get_local(const char *name, int socktype, struct addrinfo **res);
#endif

static int add_ipv4(const char *hostname, int flags, struct addrinfo **aip,
    int socktype, int port);
static int add_ipv6(const char *hostname, int flags, struct addrinfo **aip,
    int socktype, int port);
static void set_order(int, int (**)());

#define FOUND_IPV4	0x1
#define FOUND_IPV6	0x2
#define FOUND_MAX	2

int
getaddrinfo(const char *hostname, const char *servname,
	const struct addrinfo *hints, struct addrinfo **res)
{
	struct servent *sp;
	char *proto;
	int family, socktype, flags, protocol;
	struct addrinfo *ai, *ai_list;
	int port, err, i;
	int (*net_order[FOUND_MAX+1])();

	if (hostname == NULL && servname == NULL)
		return (EAI_NONAME);

	proto = NULL;
	if (hints != NULL) {
		if (hints->ai_flags & ~(AI_MASK))
			return (EAI_BADFLAGS);
		if (hints->ai_addrlen || hints->ai_canonname ||
		    hints->ai_addr || hints->ai_next) {
			errno = EINVAL;
			return (EAI_SYSTEM);
		}
		family = hints->ai_family;
		socktype = hints->ai_socktype;
		protocol = hints->ai_protocol;
		flags = hints->ai_flags;
		switch (family) {
		case AF_UNSPEC:
			switch (hints->ai_socktype) {
			case SOCK_STREAM:	proto = "tcp"; break;
			case SOCK_DGRAM:	proto = "udp"; break;
			}
			break;
		case AF_INET:
		case AF_INET6:
			switch (hints->ai_socktype) {
			case 0:			break;
			case SOCK_STREAM:	proto = "tcp"; break;
			case SOCK_DGRAM:	proto = "udp"; break;
			case SOCK_RAW:		break;
			default:		return (EAI_SOCKTYPE);
			}
			break;
#ifdef	AF_LOCAL
		case AF_LOCAL:
			switch (hints->ai_socktype) {
			case 0:			break;
			case SOCK_STREAM:	break;
			case SOCK_DGRAM:	break;
			default:		return (EAI_SOCKTYPE);
			}
			break;
#endif
		default:
			return (EAI_FAMILY);
		}
	} else {
		protocol = 0;
		family = 0;
		socktype = 0;
		flags = 0;
	}

#ifdef	AF_LOCAL
	/*
	 * First, deal with AF_LOCAL.  If the family was not set,
	 * then assume AF_LOCAL if the first character of the
	 * hostname/servname is '/'.
	 */

	if (hostname &&
	    (family == AF_LOCAL || (family == 0 && *hostname == '/')))
		return (get_local(hostname, socktype, res));

	if (servname &&
	    (family == AF_LOCAL || (family == 0 && *servname == '/')))
		return (get_local(servname, socktype, res));
#endif

	/*
	 * Ok, only AF_INET and AF_INET6 left.
	 */
	ai_list = NULL;

	/*
	 * First, look up the service name (port) if it was
	 * requested.  If the socket type wasn't specified, then
	 * try and figure it out.
	 */
	if (servname) {
		char *e;

		port = strtol(servname, &e, 10);
		if (*e == '\0') {
			if (socktype == 0)
				return (EAI_SOCKTYPE);
			if (port < 0 || port > 65535)
				return (EAI_SERVICE);
			port = htons(port);
		} else {
			sp = getservbyname(servname, proto);
			if (sp == NULL)
				return (EAI_SERVICE);
			port = sp->s_port;
			if (socktype == 0) {
				if (strcmp(sp->s_proto, "tcp"))
					socktype = SOCK_STREAM;
				else if (strcmp(sp->s_proto, "udp"))
					socktype = SOCK_DGRAM;
			}
		}
	} else
		port = 0;

	/*
	 * Next, deal with just a service name, and no hostname.
	 * (we verified that one of them was non-null up above).
	 */
	if (hostname == NULL && (flags & AI_PASSIVE) != 0) {
		if (family == AF_INET || family == 0) {
			ai = ai_alloc(AF_INET, sizeof(struct sockaddr_in));
			if (ai == NULL)
				return (EAI_MEMORY);
			ai->ai_socktype = socktype;
			ai->ai_protocol = protocol;
			SIN(ai->ai_addr)->sin_port = port;
			ai->ai_next = ai_list;
			ai_list = ai;
		}

		if (family == AF_INET6 || family == 0) {
			ai = ai_alloc(AF_INET6, sizeof(struct sockaddr_in6));
			if (ai == NULL) {
				freeaddrinfo(ai_list);
				return (EAI_MEMORY);
			}
			ai->ai_socktype = socktype;
			ai->ai_protocol = protocol;
			SIN6(ai->ai_addr)->sin6_port = port;
			ai->ai_next = ai_list;
			ai_list = ai;
		}

		*res = ai_list;
		return (0);
	}

	/*
	 * If the family isn't specified or AI_NUMERICHOST specified,
	 * check first to see if it * is a numeric address.
	 * Though the gethostbyname2() routine
	 * will recognize numeric addresses, it will only recognize
	 * the format that it is being called for.  Thus, a numeric
	 * AF_INET address will be treated by the AF_INET6 call as
	 * a domain name, and vice versa.  Checking for both numerics
	 * here avoids that.
	 */
	if (hostname != NULL &&
	    (family == 0 || (flags & AI_NUMERICHOST) != 0)) {
		char abuf[sizeof(struct in6_addr)];
		char nbuf[sizeof("xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:xxxx00")];
		int addrsize, addroff;

		if (inet_aton(hostname, (struct in_addr *)abuf)) {
			if (family == AF_INET6) {
				/* Convert to a V4 mapped address */
				struct in6_addr *a6 = (struct in6_addr *)abuf;
				memcpy(&a6->s6_addr[12], &a6->s6_addr[0], 4);
				memset(&a6->s6_addr[10], 0xff, 2);
				memset(&a6->s6_addr[0], 0, 10);
				goto inet6_addr;
			}
			addrsize = sizeof(struct in_addr);
			addroff = (char *)(&SIN(0)->sin_addr) - (char *)0;
			family = AF_INET;
			goto common;

		} else if (inet_pton(AF_INET6, hostname, abuf)) {
			if (family && family != AF_INET6)
				return (EAI_NONAME);
		inet6_addr:
			addrsize = sizeof(struct in6_addr);
			addroff = (char *)(&SIN6(0)->sin6_addr) - (char *)0;
			family = AF_INET6;

		common:
			if ((ai = ai_clone(ai_list, family)) == NULL)
				return (EAI_MEMORY);
			ai_list = ai;
			ai->ai_socktype = socktype;
			SIN(ai->ai_addr)->sin_port = port;
			memcpy((char *)ai->ai_addr + addroff, abuf, addrsize);
			if (flags & AI_CANONNAME) {
				inet_ntop(family, abuf, nbuf, sizeof(nbuf));
				ai->ai_canonname = strdup(nbuf);
			}
			goto done;
		} else if ((flags & AI_NUMERICHOST) != 0){
			return (EAI_NONAME);
		}
	}

	set_order(family, net_order);
	for (i = 0; i < FOUND_MAX; i++) {
		if (net_order[i] == NULL)
			break;
		if ((err = (net_order[i])(hostname, flags, &ai_list,
		    socktype, port)) != 0)
			return(err);
	}

	if (ai_list == NULL)
		return (EAI_NODATA);

done:
	ai_list = ai_reverse(ai_list);

	*res = ai_list;
	return (0);
}

static void
set_order(family, net_order)
	int family;
	int (**net_order)();
{
	char *order, *tok;
	int found;

	if (family) {
		switch (family) {
		case AF_INET:
			*net_order++ = add_ipv4;
			break;
		case AF_INET6:
			*net_order++ = add_ipv6;
			break;
		}
	} else {
		order = getenv("NET_ORDER");
		found = 0;
		while (order != NULL) {
			/* We ignore any unknown names.  */
			tok = strsep(&order, ":");
			if (strcasecmp(tok, "inet6") == 0) {
				if ((found & FOUND_IPV6) == 0)
					*net_order++ = add_ipv6;
				found |= FOUND_IPV6;
			} else if (strcasecmp(tok, "inet") == 0 ||
			    strcasecmp(tok, "inet4") == 0) {
				if ((found & FOUND_IPV4) == 0)
					*net_order++ = add_ipv4;
				found |= FOUND_IPV4;
			}
		}

		/* Add in anything that we didn't find */
		if ((found & FOUND_IPV4) == 0)
			*net_order++ = add_ipv4;
		if ((found & FOUND_IPV6) == 0)
			*net_order++ = add_ipv6;
	}
	*net_order = NULL;
	return;
}

static char v4_loop[4] = { 127, 0, 0, 1 };

static int
add_ipv4(const char *hostname, int flags, struct addrinfo **aip,
    int socktype, int port)
{
	struct addrinfo *ai;
	struct hostent *hp;
	char **addr;

	if (hostname == NULL && (flags & AI_PASSIVE) == 0) {
		if ((ai = ai_clone(*aip, AF_INET)) == NULL) {
			freeaddrinfo(*aip);
			return(EAI_MEMORY);
		}

		*aip = ai;
		ai->ai_socktype = socktype;
		SIN(ai->ai_addr)->sin_port = port;
		memcpy(&SIN(ai->ai_addr)->sin_addr, v4_loop, 4);
	} else if ((hp = gethostbyname2(hostname, AF_INET)) != NULL) {
		for (addr = hp->h_addr_list; *addr; addr++) {
			if ((ai = ai_clone(*aip, hp->h_addrtype)) == NULL) {
				freeaddrinfo(*aip);
				return(EAI_MEMORY);
			}
			*aip = ai;
			ai->ai_socktype = socktype;

			/* We get IPv6 addresses if RES_USE_INET6 is set */
			if (hp->h_addrtype == AF_INET6) {
				SIN6(ai->ai_addr)->sin6_port = port;
				memcpy(&SIN6(ai->ai_addr)->sin6_addr, *addr,
				    hp->h_length);
			} else {
				SIN(ai->ai_addr)->sin_port = port;
				memcpy(&SIN(ai->ai_addr)->sin_addr, *addr,
				    hp->h_length);
			}
			if (flags & AI_CANONNAME)
				ai->ai_canonname = strdup(hp->h_name);
		}
	}
	return(0);
}

static char v6_loop[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 };

static int
add_ipv6(const char *hostname, int flags, struct addrinfo **aip,
	 int socktype, int port)
{
	struct addrinfo *ai;
	struct hostent *hp;
	char **addr;

	if (hostname == NULL && (flags & AI_PASSIVE) == 0) {
		if ((ai = ai_clone(*aip, AF_INET6)) == NULL) {
			freeaddrinfo(*aip);
			return(EAI_MEMORY);
		}

		*aip = ai;
		ai->ai_socktype = socktype;
		SIN6(ai->ai_addr)->sin6_port = port;
		memcpy(&SIN6(ai->ai_addr)->sin6_addr, v6_loop, 16);
	} else if ((hp = gethostbyname2(hostname, AF_INET6)) != NULL) {
		for (addr = hp->h_addr_list; *addr; addr++) {
			if ((ai = ai_clone(*aip, AF_INET6)) == NULL) {
				freeaddrinfo(*aip);
				return (EAI_MEMORY);
			}
			*aip = ai;
			ai->ai_socktype = socktype;
			SIN6(ai->ai_addr)->sin6_port = port;
			memcpy(&SIN6(ai->ai_addr)->sin6_addr, *addr,
			    hp->h_length);
			if (flags & AI_CANONNAME)
				ai->ai_canonname = strdup(hp->h_name);
		}
	}
	return (0);
}

void
freeaddrinfo(struct addrinfo *ai) {
	struct addrinfo *ai_next;

	while (ai != NULL) {
		ai_next = ai->ai_next;
		if (ai->ai_addr)
			free(ai->ai_addr);
		if (ai->ai_canonname)
			free(ai->ai_canonname);
		free(ai);
		ai = ai_next;
	}
}

#ifdef AF_LOCAL
static int
get_local(const char *name, int socktype, struct addrinfo **res) {
	struct addrinfo *ai;
	struct sockaddr_un *sun;

	if (socktype == 0)
		return (EAI_SOCKTYPE);

	if ((ai = ai_alloc(AF_LOCAL, sizeof(*sun))) == NULL)
		return (EAI_MEMORY);

	sun = SUN(ai->ai_addr);
	strncpy(sun->sun_path, name, sizeof(sun->sun_path));

	ai->ai_socktype = socktype;
	/*
	 * ai->ai_flags, ai->ai_protocol, ai->ai_canonname,
	 * and ai->ai_next were initialized to zero.
	 */

	*res = ai;
	return (0);
}
#endif

/*
 * Allocate an addrinfo structure, and a sockaddr structure
 * of the specificed length.  We initialize:
 *	ai_addrlen
 *	ai_family
 *	ai_addr
 *	ai_addr->sa_family
 *	ai_addr->sa_len	(HAVE_SA_LEN)
 * and everything else is initialized to zero.
 */
static struct addrinfo *
ai_alloc(int family, int addrlen) {
	struct addrinfo *ai;

	if ((ai = (struct addrinfo *)calloc(1, sizeof(*ai))) == NULL)
		return (NULL);

	if ((ai->ai_addr = SA(calloc(1, addrlen))) == NULL) {
		free(ai);
		return (NULL);
	}
	ai->ai_addrlen = addrlen;
	ai->ai_family = family;
	ai->ai_addr->sa_family = family;
#ifdef	HAVE_SA_LEN
	ai->ai_addr->sa_len = addrlen;
#endif
	return (ai);
}

static struct addrinfo *
ai_clone(struct addrinfo *oai, int family) {
	struct addrinfo *ai;

	ai = ai_alloc(family, ((family == AF_INET6) ?
	    sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in)));

	if (ai == NULL) {
		freeaddrinfo(oai);
		return (NULL);
	}
	if (oai == NULL)
		return (ai);

	ai->ai_flags = oai->ai_flags;
	ai->ai_socktype = oai->ai_socktype;
	ai->ai_protocol = oai->ai_protocol;
	ai->ai_canonname = NULL;
	ai->ai_next = oai;
	return (ai);
}

static struct addrinfo *
ai_reverse(struct addrinfo *oai) {
	struct addrinfo *nai, *tai;

	nai = NULL;

	while (oai) {
		/* grab one off the old list */
		tai = oai;
		oai = oai->ai_next;
		/* put it on the front of the new list */
		tai->ai_next = nai;
		nai = tai;
	}
	return (nai);
}
