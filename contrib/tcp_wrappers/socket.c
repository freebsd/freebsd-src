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
    static struct sockaddr_in client;
    static struct sockaddr_in server;
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
    request->client->sin = &client;

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
    request->server->sin = &server;
}

/* sock_hostaddr - map endpoint address to printable form */

void    sock_hostaddr(host)
struct host_info *host;
{
    struct sockaddr_in *sin = host->sin;

    if (sin != 0)
	STRN_CPY(host->addr, inet_ntoa(sin->sin_addr), sizeof(host->addr));
}

/* sock_hostname - map endpoint address to host name */

void    sock_hostname(host)
struct host_info *host;
{
    struct sockaddr_in *sin = host->sin;
    struct hostent *hp;
    int     i;

    /*
     * On some systems, for example Solaris 2.3, gethostbyaddr(0.0.0.0) does
     * not fail. Instead it returns "INADDR_ANY". Unfortunately, this does
     * not work the other way around: gethostbyname("INADDR_ANY") fails. We
     * have to special-case 0.0.0.0, in order to avoid false alerts from the
     * host name/address checking code below.
     */
    if (sin != 0 && sin->sin_addr.s_addr != 0
	&& (hp = gethostbyaddr((char *) &(sin->sin_addr),
			       sizeof(sin->sin_addr), AF_INET)) != 0) {

	STRN_CPY(host->name, hp->h_name, sizeof(host->name));

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

	if ((hp = gethostbyname(host->name)) == 0) {

	    /*
	     * Unable to verify that the host name matches the address. This
	     * may be a transient problem or a botched name server setup.
	     */

	    tcpd_warn("can't verify hostname: gethostbyname(%s) failed",
		      host->name);

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
		if (memcmp(hp->h_addr_list[i],
			   (char *) &sin->sin_addr,
			   sizeof(sin->sin_addr)) == 0)
		    return;			/* name is good, keep it */
	    }

	    /*
	     * The host name does not map to the initial address. Perhaps
	     * someone has messed up. Perhaps someone compromised a name
	     * server.
	     */

	    tcpd_warn("host name/address mismatch: %s != %.*s",
		      inet_ntoa(sin->sin_addr), STRING_LENGTH, hp->h_name);
	}
	strcpy(host->name, paranoid);		/* name is bad, clobber it */
    }
}

/* sock_sink - absorb unreceived IP datagram */

static void sock_sink(fd)
int     fd;
{
    char    buf[BUFSIZ];
    struct sockaddr_in sin;
    int     size = sizeof(sin);

    /*
     * Eat up the not-yet received datagram. Some systems insist on a
     * non-zero source address argument in the recvfrom() call below.
     */

    (void) recvfrom(fd, buf, sizeof(buf), 0, (struct sockaddr *) & sin, &size);
}
