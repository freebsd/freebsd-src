/*
 * Sample transparent proxy program.
 *
 * Sample implementation of a program which intercepts a TCP connectiona and
 * just echos all data back to the origin.  Written to work via inetd as a
 * "nonwait" program running as root; ie.
 * tcpmux          stream  tcp     nowait root /usr/local/bin/proxy proxy
 * with a NAT rue like this:
 * rdr smc0 0/0 port 80 -> 127.0.0.1/32 port 1
 */
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <syslog.h>
#if !defined(__SVR4) && !defined(__svr4__)
#include <strings.h>
#else
#include <sys/byteorder.h>
#endif
#include <sys/types.h>
#include <sys/time.h>
#include <sys/param.h>
#include <stdlib.h>
#include <unistd.h>
#include <stddef.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#if defined(sun) && (defined(__svr4__) || defined(__SVR4))
# include <sys/ioccom.h>
# include <sys/sysmacros.h>
#endif
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <net/if.h>
#include <netdb.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>
#include <resolv.h>
#include <ctype.h>
#include "netinet/ip_compat.h"
#include "netinet/ip_fil.h"
#include "netinet/ip_proxy.h"
#include "netinet/ip_nat.h"


main(argc, argv)
int argc;
char *argv[];
{
	struct	sockaddr_in	sin, sloc, sout;
	natlookup_t	natlook;
	natlookup_t	*natlookp = &natlook;
	char	buffer[512];
	int	namelen, fd, n;

	/*
	 * get IP# and port # of the remote end of the connection (at the
	 * origin).
	 */
	namelen = sizeof(sin);
	if (getpeername(0, (struct sockaddr *)&sin, &namelen) == -1) {
		perror("getpeername");
		exit(-1);
	}

	/*
	 * get IP# and port # of the local end of the connection (at the
	 * man-in-the-middle).
	 */
	namelen = sizeof(sin);
	if (getsockname(0, (struct sockaddr *)&sloc, &namelen) == -1) {
		perror("getsockname");
		exit(-1);
	}

	/*
	 * Build up the NAT natlookup structure.
	 */
	bzero((char *)&natlook, sizeof(natlook));
	natlook.nl_outip = sin.sin_addr;
	natlook.nl_inip = sloc.sin_addr;
	natlook.nl_flags = IPN_TCP;
	natlook.nl_outport = sin.sin_port;
	natlook.nl_inport = sloc.sin_port;

	/*
	 * Open the NAT device and lookup the mapping pair.
	 */
	fd = open(IPL_NAT, O_RDONLY);
	if (ioctl(fd, SIOCGNATL, &natlookp) == -1) {
		perror("ioctl");
		exit(-1);
	}
	close(fd);
	/*
	 * Log it
	 */
	syslog(LOG_DAEMON|LOG_INFO, "connect to %s,%d",
		inet_ntoa(natlook.nl_realip), ntohs(natlook.nl_realport));
	printf("connect to %s,%d\n",
		inet_ntoa(natlook.nl_realip), ntohs(natlook.nl_realport));

	/*
	 * Just echo data read in from stdin to stdout
	 */
	while ((n = read(0, buffer, sizeof(buffer))) > 0)
		if (write(1, buffer, n) != n)
			break;
	close(0);
}
