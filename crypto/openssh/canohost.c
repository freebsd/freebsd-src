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
RCSID("$OpenBSD: canohost.c,v 1.15 2000/09/07 21:13:37 markus Exp $");

#include "packet.h"
#include "xmalloc.h"
#include "ssh.h"

/*
 * Return the canonical name of the host at the other end of the socket. The
 * caller should free the returned string with xfree.
 */

char *
get_remote_hostname(int socket)
{
	struct sockaddr_storage from;
	int i;
	socklen_t fromlen;
	struct addrinfo hints, *ai, *aitop;
	char name[MAXHOSTNAMELEN];
	char ntop[NI_MAXHOST], ntop2[NI_MAXHOST];

	/* Get IP address of client. */
	fromlen = sizeof(from);
	memset(&from, 0, sizeof(from));
	if (getpeername(socket, (struct sockaddr *) & from, &fromlen) < 0) {
		debug("getpeername failed: %.100s", strerror(errno));
		fatal_cleanup();
	}
	if (getnameinfo((struct sockaddr *)&from, fromlen, ntop, sizeof(ntop),
	     NULL, 0, NI_NUMERICHOST) != 0)
		fatal("get_remote_hostname: getnameinfo NI_NUMERICHOST failed");

	/* Map the IP address to a host name. */
	if (getnameinfo((struct sockaddr *)&from, fromlen, name, sizeof(name),
	     NULL, 0, NI_NAMEREQD) == 0) {
		/* Got host name. */
		name[sizeof(name) - 1] = '\0';
		/*
		 * Convert it to all lowercase (which is expected by the rest
		 * of this software).
		 */
		for (i = 0; name[i]; i++)
			if (isupper(name[i]))
				name[i] = tolower(name[i]);

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
			log("reverse mapping checking getaddrinfo for %.700s failed - POSSIBLE BREAKIN ATTEMPT!", name);
			strlcpy(name, ntop, sizeof name);
			goto check_ip_options;
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
			log("Address %.100s maps to %.600s, but this does not map back to the address - POSSIBLE BREAKIN ATTEMPT!",
			    ntop, name);
			strlcpy(name, ntop, sizeof name);
			goto check_ip_options;
		}
		/* Address was found for the host name.  We accept the host name. */
	} else {
		/* Host name not found.  Use ascii representation of the address. */
		strlcpy(name, ntop, sizeof name);
		log("Could not reverse map address %.100s.", name);
	}

check_ip_options:

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
	/* IP options -- IPv4 only */
	if (from.ss_family == AF_INET) {
		unsigned char options[200], *ucp;
		char text[1024], *cp;
		socklen_t option_size;
		int ipproto;
		struct protoent *ip;

		if ((ip = getprotobyname("ip")) != NULL)
			ipproto = ip->p_proto;
		else
			ipproto = IPPROTO_IP;
		option_size = sizeof(options);
		if (getsockopt(0, ipproto, IP_OPTIONS, (char *) options,
		    &option_size) >= 0 && option_size != 0) {
			cp = text;
			/* Note: "text" buffer must be at least 3x as big as options. */
			for (ucp = options; option_size > 0; ucp++, option_size--, cp += 3)
				sprintf(cp, " %2.2x", *ucp);
			log("Connection from %.100s with IP options:%.800s",
			    ntop, text);
			packet_disconnect("Connection from %.100s with IP options:%.800s",
					  ntop, text);
		}
	}

	return xstrdup(name);
}

/*
 * Return the canonical name of the host in the other side of the current
 * connection.  The host name is cached, so it is efficient to call this
 * several times.
 */

const char *
get_canonical_hostname()
{
	static char *canonical_host_name = NULL;

	/* Check if we have previously retrieved this same name. */
	if (canonical_host_name != NULL)
		return canonical_host_name;

	/* Get the real hostname if socket; otherwise return UNKNOWN. */
	if (packet_connection_is_on_socket())
		canonical_host_name = get_remote_hostname(packet_get_connection_in());
	else
		canonical_host_name = xstrdup("UNKNOWN");

	return canonical_host_name;
}

/*
 * Returns the IP-address of the remote host as a string.  The returned
 * string must not be freed.
 */

const char *
get_remote_ipaddr()
{
	static char *canonical_host_ip = NULL;
	struct sockaddr_storage from;
	socklen_t fromlen;
	int socket;
	char ntop[NI_MAXHOST];

	/* Check whether we have chached the name. */
	if (canonical_host_ip != NULL)
		return canonical_host_ip;

	/* If not a socket, return UNKNOWN. */
	if (!packet_connection_is_on_socket()) {
		canonical_host_ip = xstrdup("UNKNOWN");
		return canonical_host_ip;
	}
	/* Get client socket. */
	socket = packet_get_connection_in();

	/* Get IP address of client. */
	fromlen = sizeof(from);
	memset(&from, 0, sizeof(from));
	if (getpeername(socket, (struct sockaddr *) & from, &fromlen) < 0) {
		debug("getpeername failed: %.100s", strerror(errno));
		fatal_cleanup();
	}
	/* Get the IP address in ascii. */
	if (getnameinfo((struct sockaddr *)&from, fromlen, ntop, sizeof(ntop),
	     NULL, 0, NI_NUMERICHOST) != 0)
		fatal("get_remote_hostname: getnameinfo NI_NUMERICHOST failed");

	canonical_host_ip = xstrdup(ntop);

	/* Return ip address string. */
	return canonical_host_ip;
}

/* Returns the local/remote port for the socket. */

int
get_sock_port(int sock, int local)
{
	struct sockaddr_storage from;
	socklen_t fromlen;
	char strport[NI_MAXSERV];

	/* Get IP address of client. */
	fromlen = sizeof(from);
	memset(&from, 0, sizeof(from));
	if (local) {
		if (getsockname(sock, (struct sockaddr *)&from, &fromlen) < 0) {
			error("getsockname failed: %.100s", strerror(errno));
			return 0;
		}
	} else {
		if (getpeername(sock, (struct sockaddr *) & from, &fromlen) < 0) {
			debug("getpeername failed: %.100s", strerror(errno));
			fatal_cleanup();
		}
	}
	/* Return port number. */
	if (getnameinfo((struct sockaddr *)&from, fromlen, NULL, 0,
	     strport, sizeof(strport), NI_NUMERICSERV) != 0)
		fatal("get_sock_port: getnameinfo NI_NUMERICSERV failed");
	return atoi(strport);
}

/* Returns remote/local port number for the current connection. */

int
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
get_remote_port()
{
	return get_port(0);
}

int
get_local_port()
{
	return get_port(1);
}
