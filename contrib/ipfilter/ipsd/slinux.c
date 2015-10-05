/*	$FreeBSD$	*/

/*
 * (C)opyright 1992-1998 Darren Reed. (from tcplog)
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 */

#include <stdio.h>
#include <netdb.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/timeb.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/dir.h>
#include <linux/netdevice.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include "ip_compat.h"
#include "tcpip.h"

#ifndef	lint
static const char sccsid[] = "@(#)slinux.c	1.1 12/3/95 (C) 1995 Darren Reed";
#endif

#define BUFSPACE	32768

/*
 * Be careful to only include those defined in the flags option for the
 * interface are included in the header size.
 */

static	int	timeout;
static	char	*eth_dev = NULL;


int	ack_recv(bp)
	char	*bp;
{
	struct	tcpip	tip;
	tcphdr_t	*tcp;
	ip_t	*ip;

	ip = (struct ip *)&tip;
	tcp = (tcphdr_t *)(ip + 1);

	bcopy(bp, (char *)&tip, sizeof(tip));
	bcopy(bp + (ip.ip_hl << 2), (char *)tcp, sizeof(*tcp));
	if (0 == detect(ip, tcp))
		return 1;
	return 0;
}


void	readloop(fd, port, dst)
	int 	fd, port;
	struct	in_addr dst;
{
	static	u_char	buf[BUFSPACE];
	struct	sockaddr dest;
	register u_char	*bp = buf;
	register int	cc;
	int	dlen, done = 0;
	time_t	now = time(NULL);

	do {
		fflush(stdout);
		dlen = sizeof(dest);
		bzero((char *)&dest, dlen);
		cc = recvfrom(fd, buf, BUFSPACE, 0, &dest, &dlen);
		if (!cc)
			if ((time(NULL) - now) > timeout)
				return done;
			else
				continue;

		if (bp[12] != 0x8 || bp[13] != 0)
			continue;	/* not ip */

		/*
		 * get rid of non-tcp or fragmented packets here.
		 */
		if (cc >= sizeof(struct tcpiphdr))
		    {
			if (((bp[14+9] != IPPROTO_TCP) &&
			     (bp[14+9] != IPPROTO_UDP)) ||
			    (bp[14+6] & 0x1f) || (bp[14+6] & 0xff))
				continue;
			done += ack_recv(bp + 14);
		    }
	} while (cc >= 0);
	perror("read");
	exit(-1);
}

int	initdevice(dev, tout)
	char	*dev;
	int	tout;
{
	int fd;

	eth_dev = strdup(dev);
	if ((fd = socket(AF_INET, SOCK_PACKET, htons(ETHERTYPE_IP))) == -1)
	    {
		perror("socket(SOCK_PACKET)");
		exit(-1);
	    }

	return fd;
}
