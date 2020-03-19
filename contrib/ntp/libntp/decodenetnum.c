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
#include "ntp_assert.h"

#define PORTSTR(x) _PORTSTR(x)
#define _PORTSTR(x) #x

static int
isnumstr(
	const char *s
	)
{
	while (*s >= '0' && *s <= '9')
		++s;
	return !*s;
}

/*
 * decodenetnum		convert text IP address and port to sockaddr_u
 *
 * Returns 0 for failure, 1 for success.
 */
int
decodenetnum(
	const char *num,
	sockaddr_u *netnum
	)
{
	static const char * const servicename = "ntp";
	static const char * const serviceport = PORTSTR(NTP_PORT);
	
	struct addrinfo hints, *ai = NULL;
	int err;
	const char *host_str;
	const char *port_str;
	char *pp;
	char *np;
	char nbuf[80];

	REQUIRE(num != NULL);

	if (strlen(num) >= sizeof(nbuf)) {
		printf("length error\n");
		return FALSE;
	}

	port_str = servicename;
	if ('[' != num[0]) {
		/*
		 * to distinguish IPv6 embedded colons from a port
		 * specification on an IPv4 address, assume all 
		 * legal IPv6 addresses have at least two colons.
		 */
		pp = strchr(num, ':');
		if (NULL == pp)
			host_str = num;	/* no colons */
		else if (NULL != strchr(pp + 1, ':'))
			host_str = num;	/* two or more colons */
		else {			/* one colon */
			strlcpy(nbuf, num, sizeof(nbuf));
			host_str = nbuf;
			pp = strchr(nbuf, ':');
			*pp = '\0';
			port_str = pp + 1;
		}
	} else {
		host_str = np = nbuf; 
		while (*++num && ']' != *num)
			*np++ = *num;
		*np = 0;
		if (']' == num[0] && ':' == num[1] && '\0' != num[2])
			port_str = &num[2];
	}
	if ( ! *host_str)
		return FALSE;
	if ( ! *port_str)
		port_str = servicename;
	
	ZERO(hints);
	hints.ai_flags |= Z_AI_NUMERICHOST;
	if (isnumstr(port_str))
		hints.ai_flags |= Z_AI_NUMERICSERV;
	err = getaddrinfo(host_str, port_str, &hints, &ai);
	/* retry with default service name if the service lookup failed */ 
	if (err == EAI_SERVICE && strcmp(port_str, servicename)) {
		hints.ai_flags &= ~Z_AI_NUMERICSERV;
		port_str = servicename;
		err = getaddrinfo(host_str, port_str, &hints, &ai);
	}
	/* retry another time with default service port if the service lookup failed */ 
	if (err == EAI_SERVICE && strcmp(port_str, serviceport)) {
		hints.ai_flags |= Z_AI_NUMERICSERV;
		port_str = serviceport;
		err = getaddrinfo(host_str, port_str, &hints, &ai);
	}
	if (err != 0)
		return FALSE;

	INSIST(ai->ai_addrlen <= sizeof(*netnum));
	ZERO(*netnum);
	memcpy(netnum, ai->ai_addr, ai->ai_addrlen);
	freeaddrinfo(ai);

	return TRUE;
}
