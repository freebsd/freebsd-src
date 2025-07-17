/* 
 * netof - return the net address part of an ip address in a sockaddr_u structure
 *         (zero out host part)
 */
#include <config.h>
#include <stdio.h>
#include <syslog.h>

#include "ntp_fp.h"
#include "ntp_net.h"
#include "ntp_stdlib.h"
#include "ntp.h"

/* 
 * Return the network portion of a host address.  Used by ntp_io.c
 * findbcastinter() to find a multicast/broadcast interface for
 * a given remote address.  Note static storage is used, with room
 * for only two addresses, which is all that is needed at present.
 * 
 */
sockaddr_u *
netof(
	sockaddr_u *hostaddr
	)
{
	static sockaddr_u	netofbuf[2];
	static int		next_netofbuf;
	u_int32			netnum;
	sockaddr_u *		netaddr;

	netaddr = &netofbuf[next_netofbuf];
	next_netofbuf = (next_netofbuf + 1) % COUNTOF(netofbuf);

	memcpy(netaddr, hostaddr, sizeof(*netaddr));

	if (IS_IPV4(netaddr)) {
		/*
		 * We live in a modern classless IPv4 world.  Assume /24.
		 */
		netnum = SRCADR(netaddr) & IN_CLASSC_NET;
		SET_ADDR4(netaddr, netnum);
	} else if (IS_IPV6(netaddr))
		/* assume the typical /64 subnet size */
		zero_mem(&NSRCADR6(netaddr)[8], 8);
#ifdef DEBUG
	else {
		msyslog(LOG_ERR, "netof unknown AF %d", AF(netaddr));
		exit(1);
	}
#endif

	return netaddr;
}
