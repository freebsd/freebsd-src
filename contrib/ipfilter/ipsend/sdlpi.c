/*
 * (C)opyright 1992-1997 Darren Reed. (from tcplog)
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 */

#include <stdio.h>
#include <netdb.h>
#include <ctype.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/timeb.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/stropts.h>

#ifdef sun
#include <sys/pfmod.h>
#include <sys/bufmod.h>
#endif
#include <sys/dlpi.h>

#include <net/if.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#include <netinet/ip_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet/tcp.h>

#include "ipsend.h"

#if !defined(lint)
static const char sccsid[] = "@(#)sdlpi.c	1.3 10/30/95 (C)1995 Darren Reed";
static const char rcsid[] = "@(#)$Id: sdlpi.c,v 2.0.2.6 1997/10/15 14:49:14 darrenr Exp $";
#endif

#define	CHUNKSIZE	8192
#define BUFSPACE	(4*CHUNKSIZE)


/*
 * Be careful to only include those defined in the flags option for the
 * interface are included in the header size.
 */
int	initdevice(device, sport, tout)
char	*device;
int	sport, tout;
{
	char	devname[16], *s, buf[256];
	int	i, fd;

	(void) sprintf(devname, "/dev/%s", device);

	s = devname + 5;
	while (*s && !isdigit(*s))
		s++;
	if (!*s)
	    {
		fprintf(stderr, "bad device name %s\n", devname);
		exit(-1);
	    }
	i = atoi(s);
	*s = '\0';
	/*
	 * For writing
	 */
	if ((fd = open(devname, O_RDWR)) < 0)
	    {
		fprintf(stderr, "O_RDWR(1) ");
		perror(devname);
		exit(-1);
	    }

	if (dlattachreq(fd, i) == -1 || dlokack(fd, buf) == -1)
	    {
		fprintf(stderr, "DLPI error\n");
		exit(-1);
	    }
	dlbindreq(fd, ETHERTYPE_IP, 0, DL_CLDLS, 0, 0);
	dlbindack(fd, buf);
	/*
	 * write full headers
	 */
#ifdef sun /* we require RAW DLPI mode, which is a Sun extension */
	if (strioctl(fd, DLIOCRAW, -1, 0, NULL) == -1)
	    {
		fprintf(stderr, "DLIOCRAW error\n");
		exit(-1);
	    }
#else
you lose
#endif
	return fd;
}


/*
 * output an IP packet onto a fd opened for /dev/nit
 */
int	sendip(fd, pkt, len)
int	fd, len;
char	*pkt;
{			
	struct	strbuf	dbuf, *dp = &dbuf;

	/*
	 * construct NIT STREAMS messages, first control then data.
	 */
	dp->buf = pkt;
	dp->len = len;
	dp->maxlen = dp->len;

	if (putmsg(fd, NULL, dp, 0) == -1)
	    {
		perror("putmsg");
		return -1;
	    }
	if (ioctl(fd, I_FLUSH, FLUSHW) == -1)
	    {
		perror("I_FLUSHW");
		return -1;
	    }
	return len;
}
