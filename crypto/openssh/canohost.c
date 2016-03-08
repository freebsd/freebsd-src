/* $OpenBSD: canohost.c,v 1.72 2015/03/01 15:44:40 millert Exp $ */
/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * Functions for returning the canonical host name of the remote site.
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 */

#include "includes.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>

#include "xmalloc.h"
#include "packet.h"
#include "log.h"
#include "canohost.h"
#include "misc.h"

static void check_ip_options(int, char *);
static char *canonical_host_ip = NULL;
static int cached_port = -1;

/*
 * Return the canonical name of the host at the other end of the socket. The
 * caller should free the returned string.
 */

static char *
get_remote_hostname(int sock, int use_dns)
{
	struct sockaddr_storage from;
	socklen_t fromlen;
	struct addrinfo hints, *ai, *aitop;
	char name[NI_MAXHOST], ntop[NI_MAXHOST], ntop2[NI_MAXHOST];

	/* Get IP address of client. */
	fromlen = sizeof(from);
	memset(&from, 0, sizeof(from));
	if (getpeername(sock, (struct sockaddr *)&from, &fromlen) < 0) {
		debug("getpeername failed: %.100s", strerror(errno));
		cleanup_exit(255);
	}

	if (from.ss_family == AF_INET)
		check_ip_options(sock, ntop);

	ipv64_normalise_mapped(&from, &fromlen);

	if (from.ss_family == AF_INET6)
		fromlen = sizeof(struct sockaddr_in6);

	if (getnameinfo((struct sockaddr *)&from, fromlen, ntop, sizeof(ntop),
	    NULL, 0, NI_NUMERICHOST) != 0)
		fatal("get_remote_hostname: getnameinfo NI_NUMERICHOST failed");

	if (!use_dns)
		return xstrdup(ntop);

	debug3("Trying to reverse map address %.100s.", ntop);
	/* Map the IP address to a host name. */
	if (getnameinfo((struct sockaddr *)&from, fromlen, name, sizeof(name),
	    NULL, 0, NI_NAMEREQD) != 0) {
		/* Host name not found.  Use ip address. */
		return xstrdup(ntop);
	}

	/*
	 * if reverse lookup result looks like a numeric hostname,
	 * someone is trying to trick us by PTR record like following:
	 *	1.1.1.10.in-addr.arpa.	IN PTR	2.3.4.5
	 */
	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = SOCK_DGRAM;	/*dummy*/
	hints.ai_flags = AI_NUMERICHOST;
	if (getaddrinfo(name, NULL, &hints, &ai) == 0) {
		logit("Nasty PTR record \"%s\" is set up for %s, ignoring",
		    name, ntop);
		freeaddrinfo(ai);
		return xstrdup(ntop);
	}

	/* Names are stores in lowercase. */
	lowercase(name);

	/*
	 * Map it back to an IP address and check that the given
	 * address actually is an address of this host.  This is
	 * necessary because anyone with access to a name server can
	 * define arbitrary names for an IP address. Mapping from
	 * name to IP address can be trusted better (but can still be
	 * fooled if the intruder has access to the name server of
	 * the domain).
	 */
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = from.ss_family;
	hints.ai_socktype = SOCK_STREAM;
	if (getaddrinfo(name, NULL, &hints, &aitop) != 0) {
		logit("reverse mapping checking getaddrinfo for %.700s "
		    "[%s] failed - POSSIBLE BREAK-IN ATTEMPT!", name, ntop);
		return xstrdup(ntop);
	}
	/* Look for the address from the list of addresses. */
	for (ai = aitop; ai; ai = ai->ai_next) {
		if (getnameinfo(ai->ai_addr, ai->ai_addrlen, ntop2,
		    sizeof(ntop2), NULL, 0, NI_NUMERICHOST) == 0 &&
		    (strcmp(ntop, ntop2) == 0))
				break;
	}
	freeaddrinfo(aitop);
	/* If we reached the end of the list, the address was not there. */
	if (!ai) {
		/* Address not found for the host name. */
		logit("Address %.100s maps to %.600s, but this does not "
		    "map back to the address - POSSIBLE BREAK-IN ATTEMPT!",
		    ntop, name);
		return xstrdup(ntop);
	}
	return xstrdup(name);
}

/*
 * If IP options are supported, make sure there are none (log and
 * disconnect them if any are found).  Basically we are worried about
 * source routing; it can be used to pretend you are somebody
 * (ip-address) you are not. That itself may be "almost acceptable"
 * under certain circumstances, but rhosts autentication is useless
 * if source routing is accepted. Notice also that if we just dropped
 * source routing here, the other side could use IP spoofing to do
 * rest of the interaction and could still bypass security.  So we
 * exit here if we detect any IP options.
 */
/* IPv4 only */
static void
check_ip_options(int sock, char *ipaddr)
{
#ifdef IP_OPTIONS
	u_char options[200];
	char text[sizeof(options) * 3 + 1];
	socklen_t option_size, i;
	int ipproto;
	struct protoent *ip;

	if ((ip = getprotobyname("ip")) != NULL)
		ipproto = ip->p_proto;
	else
		ipproto = IPPROTO_IP;
	option_size = sizeof(options);
	if (getsockopt(sock, ipproto, IP_OPTIONS, options,
	    &option_size) >= 0 && option_size != 0) {
		text[0] = '\0';
		for (i = 0; i < option_size; i++)
			snprintf(text + i*3, sizeof(text) - i*3,
			    " %2.2x", options[i]);
		fatal("Connection from %.100s with IP options:%.800s",
		    ipaddr, text);
	}
#endif /* IP_OPTIONS */
}

void
ipv64_normalise_mapped(struct sockaddr_storage *addr, socklen_t *len)
{
	struct sockaddr_in6 *a6 = (struct sockaddr_in6 *)addr;
	struct sockaddr_in *a4 = (struct sockaddr_in *)addr;
	struct in_addr inaddr;
	u_int16_t port;

	if (addr->ss_family != AF_INET6 ||
	    !IN6_IS_ADDR_V4MAPPED(&a6->sin6_addr))
		return;

	debug3("Normalising mapped IPv4 in IPv6 address");

	memcpy(&inaddr, ((char *)&a6->sin6_addr) + 12, sizeof(inaddr));
	port = a6->sin6_port;

	memset(a4, 0, sizeof(*a4));

	a4->sin_family = AF_INET;
	*len = sizeof(*a4);
	memcpy(&a4->sin_addr, &inaddr, sizeof(inaddr));
	a4->sin_port = port;
}

/*
 * Return the canonical name of the host in the other side of the current
 * connection.  The host name is cached, so it is efficient to call this
 * several times.
 */

const char *
get_canonical_hostname(int use_dns)
{
	char *host;
	static char *canonical_host_name = NULL;
	static char *remote_ip = NULL;

	/* Check if we have previously retrieved name with same option. */
	if (use_dns && canonical_host_name != NULL)
		return canonical_host_name;
	if (!use_dns && remote_ip != NULL)
		return remote_ip;

	/* Get the real hostname if socket; otherwise return UNKNOWN. */
	if (packet_connection_is_on_socket())
		host = get_remote_hostname(packet_get_connection_in(), use_dns);
	else
		host = "UNKNOWN";

	if (use_dns)
		canonical_host_name = host;
	else
		remote_ip = host;
	return host;
}

/*
 * Returns the local/remote IP-address/hostname of socket as a string.
 * The returned string must be freed.
 */
static char *
get_socket_address(int sock, int remote, int flags)
{
	struct sockaddr_storage addr;
	socklen_t addrlen;
	char ntop[NI_MAXHOST];
	int r;

	/* Get IP address of client. */
	addrlen = sizeof(addr);
	memset(&addr, 0, sizeof(addr));

	if (remote) {
		if (getpeername(sock, (struct sockaddr *)&addr, &addrlen)
		    < 0)
			return NULL;
	} else {
		if (getsockname(sock, (struct sockaddr *)&addr, &addrlen)
		    < 0)
			return NULL;
	}

	/* Work around Linux IPv6 weirdness */
	if (addr.ss_family == AF_INET6) {
		addrlen = sizeof(struct sockaddr_in6);
		ipv64_normalise_mapped(&addr, &addrlen);
	}

	switch (addr.ss_family) {
	case AF_INET:
	case AF_INET6:
		/* Get the address in ascii. */
		if ((r = getnameinfo((struct sockaddr *)&addr, addrlen, ntop,
		    sizeof(ntop), NULL, 0, flags)) != 0) {
			error("get_socket_address: getnameinfo %d failed: %s",
			    flags, ssh_gai_strerror(r));
			return NULL;
		}
		return xstrdup(ntop);
	case AF_UNIX:
		/* Get the Unix domain socket path. */
		return xstrdup(((struct sockaddr_un *)&addr)->sun_path);
	default:
		/* We can't look up remote Unix domain sockets. */
		return NULL;
	}
}

char *
get_peer_ipaddr(int sock)
{
	char *p;

	if ((p = get_socket_address(sock, 1, NI_NUMERICHOST)) != NULL)
		return p;
	return xstrdup("UNKNOWN");
}

char *
get_local_ipaddr(int sock)
{
	char *p;

	if ((p = get_socket_address(sock, 0, NI_NUMERICHOST)) != NULL)
		return p;
	return xstrdup("UNKNOWN");
}

char *
get_local_name(int fd)
{
	char *host, myname[NI_MAXHOST];

	/* Assume we were passed a socket */
	if ((host = get_socket_address(fd, 0, NI_NAMEREQD)) != NULL)
		return host;

	/* Handle the case where we were passed a pipe */
	if (gethostname(myname, sizeof(myname)) == -1) {
		verbose("get_local_name: gethostname: %s", strerror(errno));
	} else {
		host = xstrdup(myname);
	}

	return host;
}

void
clear_cached_addr(void)
{
	free(canonical_host_ip);
	canonical_host_ip = NULL;
	cached_port = -1;
}

/*
 * Returns the IP-address of the remote host as a string.  The returned
 * string must not be freed.
 */

const char *
get_remote_ipaddr(void)
{
	/* Check whether we have cached the ipaddr. */
	if (canonical_host_ip == NULL) {
		if (packet_connection_is_on_socket()) {
			canonical_host_ip =
			    get_peer_ipaddr(packet_get_connection_in());
			if (canonical_host_ip == NULL)
				cleanup_exit(255);
		} else {
			/* If not on socket, return UNKNOWN. */
			canonical_host_ip = xstrdup("UNKNOWN");
		}
	}
	return canonical_host_ip;
}

const char *
get_remote_name_or_ip(u_int utmp_len, int use_dns)
{
	static const char *remote = "";
	if (utmp_len > 0)
		remote = get_canonical_hostname(use_dns);
	if (utmp_len == 0 || strlen(remote) > utmp_len)
		remote = get_remote_ipaddr();
	return remote;
}

/* Returns the local/remote port for the socket. */

int
get_sock_port(int sock, int local)
{
	struct sockaddr_storage from;
	socklen_t fromlen;
	char strport[NI_MAXSERV];
	int r;

	/* Get IP address of client. */
	fromlen = sizeof(from);
	memset(&from, 0, sizeof(from));
	if (local) {
		if (getsockname(sock, (struct sockaddr *)&from, &fromlen) < 0) {
			error("getsockname failed: %.100s", strerror(errno));
			return 0;
		}
	} else {
		if (getpeername(sock, (struct sockaddr *)&from, &fromlen) < 0) {
			debug("getpeername failed: %.100s", strerror(errno));
			return -1;
		}
	}

	/* Work around Linux IPv6 weirdness */
	if (from.ss_family == AF_INET6)
		fromlen = sizeof(struct sockaddr_in6);

	/* Non-inet sockets don't have a port number. */
	if (from.ss_family != AF_INET && from.ss_family != AF_INET6)
		return 0;

	/* Return port number. */
	if ((r = getnameinfo((struct sockaddr *)&from, fromlen, NULL, 0,
	    strport, sizeof(strport), NI_NUMERICSERV)) != 0)
		fatal("get_sock_port: getnameinfo NI_NUMERICSERV failed: %s",
		    ssh_gai_strerror(r));
	return atoi(strport);
}

/* Returns remote/local port number for the current connection. */

static int
get_port(int local)
{
	/*
	 * If the connection is not a socket, return 65535.  This is
	 * intentionally chosen to be an unprivileged port number.
	 */
	if (!packet_connection_is_on_socket())
		return 65535;

	/* Get socket and return the port number. */
	return get_sock_port(packet_get_connection_in(), local);
}

int
get_peer_port(int sock)
{
	return get_sock_port(sock, 0);
}

int
get_remote_port(void)
{
	/* Cache to avoid getpeername() on a dead connection */
	if (cached_port == -1)
		cached_port = get_port(0);

	return cached_port;
}

int
get_local_port(void)
{
	return get_port(1);
}
