 /*
  * This module determines the type of socket (datagram, stream), the client
  * socket address and port, the server socket address and port. In addition,
  * it provides methods to map a transport address to a printable host name
  * or address. Socket address information results are in static memory.
  * 
  * The result from the hostname lookup method is STRING_PARANOID when a host
  * pretends to have someone elses name, or when a host name is available but
  * could not be verified.
  * 
  * When lookup or conversion fails the result is set to STRING_UNKNOWN.
  * 
  * Diagnostics are reported through syslog(3).
  * 
  * Author: Wietse Venema, Eindhoven University of Technology, The Netherlands.
  *
  * $FreeBSD$
  */

#ifndef lint
static char sccsid[] = "@(#) socket.c 1.15 97/03/21 19:27:24";
#endif

/* System libraries. */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <syslog.h>
#include <string.h>

#ifdef INET6
#ifndef USE_GETIPNODEBY
#include <resolv.h>
#endif
#endif

extern char *inet_ntoa();

/* Local stuff. */

#include "tcpd.h"

/* Forward declarations. */

static void sock_sink();

#ifdef APPEND_DOT

 /*
  * Speed up DNS lookups by terminating the host name with a dot. Should be
  * done with care. The speedup can give problems with lookups from sources
  * that lack DNS-style trailing dot magic, such as local files or NIS maps.
  */

static struct hostent *gethostbyname_dot(name)
char   *name;
{
    char    dot_name[MAXHOSTNAMELEN + 1];

    /*
     * Don't append dots to unqualified names. Such names are likely to come
     * from local hosts files or from NIS.
     */

    if (strchr(name, '.') == 0 || strlen(name) >= MAXHOSTNAMELEN - 1) {
	return (gethostbyname(name));
    } else {
	sprintf(dot_name, "%s.", name);
	return (gethostbyname(dot_name));
    }
}

#define gethostbyname gethostbyname_dot
#endif

/* sock_host - look up endpoint addresses and install conversion methods */

void    sock_host(request)
struct request_info *request;
{
#ifdef INET6
    static struct sockaddr_storage client;
    static struct sockaddr_storage server;
#else
    static struct sockaddr_in client;
    static struct sockaddr_in server;
#endif
    int     len;
    char    buf[BUFSIZ];
    int     fd = request->fd;

    sock_methods(request);

    /*
     * Look up the client host address. Hal R. Brand <BRAND@addvax.llnl.gov>
     * suggested how to get the client host info in case of UDP connections:
     * peek at the first message without actually looking at its contents. We
     * really should verify that client.sin_family gets the value AF_INET,
     * but this program has already caused too much grief on systems with
     * broken library code.
     */

    len = sizeof(client);
    if (getpeername(fd, (struct sockaddr *) & client, &len) < 0) {
	request->sink = sock_sink;
	len = sizeof(client);
	if (recvfrom(fd, buf, sizeof(buf), MSG_PEEK,
		     (struct sockaddr *) & client, &len) < 0) {
	    tcpd_warn("can't get client address: %m");
	    return;				/* give up */
	}
#ifdef really_paranoid
	memset(buf, 0 sizeof(buf));
#endif
    }
#ifdef INET6
    request->client->sin = (struct sockaddr *)&client;
#else
    request->client->sin = &client;
#endif

    /*
     * Determine the server binding. This is used for client username
     * lookups, and for access control rules that trigger on the server
     * address or name.
     */

    len = sizeof(server);
    if (getsockname(fd, (struct sockaddr *) & server, &len) < 0) {
	tcpd_warn("getsockname: %m");
	return;
    }
#ifdef INET6
    request->server->sin = (struct sockaddr *)&server;
#else
    request->server->sin = &server;
#endif
}

/* sock_hostaddr - map endpoint address to printable form */

void    sock_hostaddr(host)
struct host_info *host;
{
#ifdef INET6
    struct sockaddr *sin = host->sin;
    char *ap;
    int alen;

    if (!sin)
	return;
    switch (sin->sa_family) {
    case AF_INET:
	ap = (char *)&((struct sockaddr_in *)sin)->sin_addr;
	alen = sizeof(struct in_addr);
	break;
    case AF_INET6:
	ap = (char *)&((struct sockaddr_in6 *)sin)->sin6_addr;
	alen = sizeof(struct in6_addr);
	break;
    default:
	return;
    }
    host->addr[0] = '\0';
    inet_ntop(sin->sa_family, ap, host->addr, sizeof(host->addr));
#else
    struct sockaddr_in *sin = host->sin;

    if (sin != 0)
	STRN_CPY(host->addr, inet_ntoa(sin->sin_addr), sizeof(host->addr));
#endif
}

/* sock_hostname - map endpoint address to host name */

void    sock_hostname(host)
struct host_info *host;
{
#ifdef INET6
    struct sockaddr *sin = host->sin;
    char addr[128];
#ifdef USE_GETIPNODEBY
    int h_error;
#else
    u_long res_options;
#endif
    struct hostent *hp = NULL;
    char *ap;
    int alen;
#else
    struct sockaddr_in *sin = host->sin;
    struct hostent *hp;
#endif
    int     i;

    /*
     * On some systems, for example Solaris 2.3, gethostbyaddr(0.0.0.0) does
     * not fail. Instead it returns "INADDR_ANY". Unfortunately, this does
     * not work the other way around: gethostbyname("INADDR_ANY") fails. We
     * have to special-case 0.0.0.0, in order to avoid false alerts from the
     * host name/address checking code below.
     */
#ifdef INET6
    if (sin != NULL) {
	switch (sin->sa_family) {
	case AF_INET:
	    if (((struct sockaddr_in *)sin)->sin_addr.s_addr == 0) {
		strcpy(host->name, paranoid);	/* name is bad, clobber it */
		return;
	    }
	    ap = (char *) &((struct sockaddr_in *)sin)->sin_addr;
	    alen = sizeof(struct in_addr);
	    break;
	case AF_INET6:
	    ap = (char *) &((struct sockaddr_in6 *)sin)->sin6_addr;
	    alen = sizeof(struct in6_addr);
	    break;
	defalut:
	    strcpy(host->name, paranoid);	/* name is bad, clobber it */
	    return;
	}
#ifdef USE_GETIPNODEBY
	hp = getipnodebyaddr(ap, alen, sin->sa_family, &h_error);
#else
	hp = gethostbyaddr(ap, alen, sin->sa_family);
#endif
    }
    if (hp) {
#else
    if (sin != 0 && sin->sin_addr.s_addr != 0
	&& (hp = gethostbyaddr((char *) &(sin->sin_addr),
			       sizeof(sin->sin_addr), AF_INET)) != 0) {
#endif

	STRN_CPY(host->name, hp->h_name, sizeof(host->name));
#if defined(INET6) && defined(USE_GETIPNODEBY)
	freehostent(hp);
#endif

	/*
	 * Verify that the address is a member of the address list returned
	 * by gethostbyname(hostname).
	 * 
	 * Verify also that gethostbyaddr() and gethostbyname() return the same
	 * hostname, or rshd and rlogind may still end up being spoofed.
	 * 
	 * On some sites, gethostbyname("localhost") returns "localhost.domain".
	 * This is a DNS artefact. We treat it as a special case. When we
	 * can't believe the address list from gethostbyname("localhost")
	 * we're in big trouble anyway.
	 */

#ifdef INET6
#ifdef USE_GETIPNODEBY
	hp = getipnodebyname(host->name, sin->sa_family,
			     AI_V4MAPPED | AI_ADDRCONFIG | AI_ALL, &h_error);
#else
	if ((_res.options & RES_INIT) == 0) {
	    if (res_init() < 0) {
		inet_ntop(sin->sa_family, ap, addr, sizeof(addr));
		tcpd_warn("can't verify hostname: res_init() for %s failed",
			  addr);
		strcpy(host->name, paranoid);	/* name is bad, clobber it */
		return;
	    }
	}
	res_options = _res.options;
	if (sin->sa_family == AF_INET6)
	    _res.options |= RES_USE_INET6;
	else
	    _res.options &= ~RES_USE_INET6;
	hp = gethostbyname2(host->name,
			    (sin->sa_family == AF_INET6 &&
			     IN6_IS_ADDR_V4MAPPED(&((struct sockaddr_in6 *)sin)->sin6_addr)) ?
				AF_INET : sin->sa_family);
	_res.options = res_options;
#endif
	if (!hp) {
#else
	if ((hp = gethostbyname(host->name)) == 0) {
#endif

	    /*
	     * Unable to verify that the host name matches the address. This
	     * may be a transient problem or a botched name server setup.
	     */

#ifdef INET6
#ifdef USE_GETIPNODEBY
	    tcpd_warn("can't verify hostname: getipnodebyname(%s, %s) failed",
#else
	    tcpd_warn("can't verify hostname: gethostbyname2(%s, %s) failed",
#endif
		      host->name,
		      (sin->sa_family == AF_INET) ? "AF_INET" : "AF_INET6");
#else
	    tcpd_warn("can't verify hostname: gethostbyname(%s) failed",
		      host->name);
#endif

	} else if (STR_NE(host->name, hp->h_name)
		   && STR_NE(host->name, "localhost")) {

	    /*
	     * The gethostbyaddr() and gethostbyname() calls did not return
	     * the same hostname. This could be a nameserver configuration
	     * problem. It could also be that someone is trying to spoof us.
	     */

	    tcpd_warn("host name/name mismatch: %s != %.*s",
		      host->name, STRING_LENGTH, hp->h_name);

	} else {

	    /*
	     * The address should be a member of the address list returned by
	     * gethostbyname(). We should first verify that the h_addrtype
	     * field is AF_INET, but this program has already caused too much
	     * grief on systems with broken library code.
	     */

	    for (i = 0; hp->h_addr_list[i]; i++) {
#ifdef INET6
		if (memcmp(hp->h_addr_list[i], ap, alen) == 0) {
#ifdef USE_GETIPNODEBY
		    freehostent(hp);
#endif
		    return;			/* name is good, keep it */
		}
#else
		if (memcmp(hp->h_addr_list[i],
			   (char *) &sin->sin_addr,
			   sizeof(sin->sin_addr)) == 0)
		    return;			/* name is good, keep it */
#endif
	    }

	    /*
	     * The host name does not map to the initial address. Perhaps
	     * someone has messed up. Perhaps someone compromised a name
	     * server.
	     */

#ifdef INET6
	    inet_ntop(sin->sa_family, ap, addr, sizeof(addr));
	    tcpd_warn("host name/address mismatch: %s != %.*s",
		      addr, STRING_LENGTH, hp->h_name);
#else
	    tcpd_warn("host name/address mismatch: %s != %.*s",
		      inet_ntoa(sin->sin_addr), STRING_LENGTH, hp->h_name);
#endif
	}
	strcpy(host->name, paranoid);		/* name is bad, clobber it */
#if defined(INET6) && defined(USE_GETIPNODEBY)
	if (hp)
	    freehostent(hp);
#endif
    }
}

/* sock_sink - absorb unreceived IP datagram */

static void sock_sink(fd)
int     fd;
{
    char    buf[BUFSIZ];
#ifdef INET6
    struct sockaddr_storage sin;
#else
    struct sockaddr_in sin;
#endif
    int     size = sizeof(sin);

    /*
     * Eat up the not-yet received datagram. Some systems insist on a
     * non-zero source address argument in the recvfrom() call below.
     */

    (void) recvfrom(fd, buf, sizeof(buf), 0, (struct sockaddr *) & sin, &size);
}
