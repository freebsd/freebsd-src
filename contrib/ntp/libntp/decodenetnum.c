/*
 * decodenetnum - return a net number (this is crude, but careful)
 */
#include <config.h>
#include <sys/types.h>
#include <ctype.h>
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#include "ntp.h"
#include "ntp_stdlib.h"


/* If the given string position points to a decimal digit, parse the
 * number. If this is not possible, or the parsing did not consume the
 * whole string, or if the result exceeds the maximum value, return the
 * default value.
 */
static unsigned long
_num_or_dflt(
	char *		sval,
	unsigned long	maxval,
	unsigned long	defval
	)
{
	char *		ep;
	unsigned long	num;
	
	if (!(sval && isdigit(*(unsigned char*)sval)))
		return defval;
	
	num = strtoul(sval, &ep, 10);
	if (!*ep && num <= maxval)
		return num;
	
	return defval;
}

/* If the given string position is not NULL and does not point to the
 * terminator, replace the character with NUL and advance the pointer.
 * Return the resulting position.
 */
static inline char*
_chop(
	char * sp)
{
	if (sp && *sp)
		*sp++ = '\0';
	return sp;
}

/* If the given string position points to the given char, advance the
 * pointer and return the result. Otherwise, return NULL.
 */
static inline char*
_skip(
	char * sp,
	int    ch)
{
	if (sp && *(unsigned char*)sp == ch)
		return (sp + 1);
	return NULL;
}

/*
 * decodenetnum		convert text IP address and port to sockaddr_u
 *
 * Returns FALSE (->0) for failure, TRUE (->1) for success.
 */
int
decodenetnum(
	const char *num,
	sockaddr_u *net
	)
{
	/* Building a parser is more fun in Haskell, but here we go...
	 *
	 * This works through 'inet_pton()' taking the brunt of the
	 * work, after some string manipulations to split off URI
	 * brackets, ports and scope identifiers. The heuristics are
	 * simple but must hold for all _VALID_ addresses. inet_pton()
	 * will croak on bad ones later, but replicating the whole
	 * parser logic to detect errors is wasteful.
	 */
	
	sockaddr_u	netnum;
	char		buf[64];	/* working copy of input */
	char		*haddr=buf;
	unsigned int	port=NTP_PORT, scope=0;
	unsigned short	afam=AF_UNSPEC;
	
	/* copy input to working buffer with length check */
	if (strlcpy(buf, num, sizeof(buf)) >= sizeof(buf))
		return FALSE;

	/* Identify address family and possibly the port, if given.  If
	 * this results in AF_UNSPEC, we will fail in the next step.
	 */
	if (*haddr == '[') {
		char * endp = strchr(++haddr, ']');
		if (endp) {
			port = _num_or_dflt(_skip(_chop(endp), ':'),
					      0xFFFFu, port);
			afam = strchr(haddr, ':') ? AF_INET6 : AF_INET;
		}
	} else {
		char *col = strchr(haddr, ':');
		char *dot = strchr(haddr, '.');
		if (col == dot) {
			/* no dot, no colon: bad! */
			afam = AF_UNSPEC;
		} else if (!col) {
			/* no colon, only dot: IPv4! */
			afam = AF_INET;
		} else if (!dot || col < dot) {
			/* no dot or 1st colon before 1st dot: IPv6! */
			afam = AF_INET6;
		} else {
			/* 1st dot before 1st colon: must be IPv4 with port */
			afam = AF_INET;
			port = _num_or_dflt(_chop(col), 0xFFFFu, port);
		}
	}

	/* Since we don't know about additional members in the address
	 * structures, we wipe the result buffer thoroughly:
	 */	 
	memset(&netnum, 0, sizeof(netnum));

	/* For AF_INET6, evaluate and remove any scope suffix. Have
	 * inet_pton() do the real work for AF_INET and AF_INET6, bail
	 * out otherwise:
	 */
	switch (afam) {
	case AF_INET:
		if (inet_pton(afam, haddr, &netnum.sa4.sin_addr) <= 0)
			return FALSE;
		netnum.sa4.sin_port = htons((unsigned short)port);
		break;

	case AF_INET6:
		scope = _num_or_dflt(_chop(strchr(haddr, '%')), 0xFFFFFFFFu, scope);
		if (inet_pton(afam, haddr, &netnum.sa6.sin6_addr) <= 0)
			return FALSE;
		netnum.sa6.sin6_port = htons((unsigned short)port);
		netnum.sa6.sin6_scope_id = scope;
		break;

	case AF_UNSPEC:
	default:
		return FALSE;
	}

	/* Collect the remaining pieces and feed the output, which was
	 * not touched so far:
	 */
	netnum.sa.sa_family = afam;
	memcpy(net, &netnum, sizeof(netnum));
	return TRUE;
}
