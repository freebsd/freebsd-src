/*
 * The mrouted program is covered by the license in the accompanying file
 * named "LICENSE".  Use of the mrouted program represents acceptance of
 * the terms and conditions listed in that file.
 *
 * The mrouted program is COPYRIGHT 1989 by The Board of Trustees of
 * Leland Stanford Junior University.
 *
 *
 * $Id: inet.c,v 1.4 1993/05/30 01:36:38 deering Exp $
 */


#include "defs.h"


/*
 * Exported variables.
 */
char s1[16];		/* buffers to hold the string representations  */
char s2[16];		/* of IP addresses, to be passed to inet_fmt() */
char s3[16];		/* or inet_fmts().                             */


/*
 * Verify that a given IP address is credible as a host address.
 * (Without a mask, cannot detect addresses of the form {subnet,0} or
 * {subnet,-1}.)
 */
int inet_valid_host(naddr)
    u_long naddr;
{
    register u_long addr;

    addr = ntohl(naddr);

    return (!(IN_MULTICAST(addr) ||
	      IN_BADCLASS (addr) ||
	      (addr & 0xff000000) == 0));
}


/*
 * Verify that a given subnet number and mask pair are credible.
 */
int inet_valid_subnet(nsubnet, nmask)
    u_long nsubnet, nmask;
{
    register u_long subnet, mask;

    subnet = ntohl(nsubnet);
    mask   = ntohl(nmask);

    if ((subnet & mask) != subnet) return (FALSE);

    if (IN_CLASSA(subnet)) {
	if (mask < 0xff000000 ||
	   (subnet & 0xff000000) == 0 ||
	   (subnet & 0xff000000) == 0x7f000000) return (FALSE);
    }
    else if (IN_CLASSB(subnet)) {
	if (mask < 0xffff0000) return (FALSE);
    }
    else if (IN_CLASSC(subnet)) {
	if (mask < 0xffffff00) return (FALSE);
    }
    else return (FALSE);

    return (TRUE);
}


/*
 * Convert an IP address in u_long (network) format into a printable string.
 */
char *inet_fmt(addr, s)
    u_long addr;
    char *s;
{
    register u_char *a;

    a = (u_char *)&addr;
    sprintf(s, "%u.%u.%u.%u", a[0], a[1], a[2], a[3]);
    return (s);
}


/*
 * Convert an IP subnet number in u_long (network) format into a printable
 * string.
 */
char *inet_fmts(addr, mask, s)
    u_long addr, mask;
    char *s;
{
    register u_char *a, *m;

    a = (u_char *)&addr;
    m = (u_char *)&mask;

    if      (m[3] != 0) sprintf(s, "%u.%u.%u.%u", a[0], a[1], a[2], a[3]);
    else if (m[2] != 0) sprintf(s, "%u.%u.%u",    a[0], a[1], a[2]);
    else if (m[1] != 0) sprintf(s, "%u.%u",       a[0], a[1]);
    else                sprintf(s, "%u",          a[0]);

    return (s);
}


/*
 * Convert the printable string representation of an IP address into the
 * u_long (network) format.  Return 0xffffffff on error.  (To detect the
 * legal address with that value, you must explicitly compare the string
 * with "255.255.255.255".)
 */
u_long inet_parse(s)
    char *s;
{
    u_long a;
    u_int a0, a1, a2, a3;
    char c;

    if (sscanf(s, "%u.%u.%u.%u%c", &a0, &a1, &a2, &a3, &c) != 4 ||
	a0 > 255 || a1 > 255 || a2 > 255 || a3 > 255)
	return (0xffffffff);

    ((u_char *)&a)[0] = a0;
    ((u_char *)&a)[1] = a1;
    ((u_char *)&a)[2] = a2;
    ((u_char *)&a)[3] = a3;

    return (a);
}


/*
 * inet_cksum extracted from:
 *			P I N G . C
 *
 * Author -
 *	Mike Muuss
 *	U. S. Army Ballistic Research Laboratory
 *	December, 1983
 * Modified at Uc Berkeley
 *
 * (ping.c) Status -
 *	Public Domain.  Distribution Unlimited.
 *
 *			I N _ C K S U M
 *
 * Checksum routine for Internet Protocol family headers (C Version)
 *
 */
int inet_cksum(addr, len)
	u_short *addr;
	u_int len;
{
	register int nleft = (int)len;
	register u_short *w = addr;
	u_short answer = 0;
	register int sum = 0;

	/*
	 *  Our algorithm is simple, using a 32 bit accumulator (sum),
	 *  we add sequential 16 bit words to it, and at the end, fold
	 *  back all the carry bits from the top 16 bits into the lower
	 *  16 bits.
	 */
	while( nleft > 1 )  {
		sum += *w++;
		nleft -= 2;
	}

	/* mop up an odd byte, if necessary */
	if( nleft == 1 ) {
		*(u_char *) (&answer) = *(u_char *)w ;
		sum += answer;
	}

	/*
	 * add back carry outs from top 16 bits to low 16 bits
	 */
	sum = (sum >> 16) + (sum & 0xffff);	/* add hi 16 to low 16 */
	sum += (sum >> 16);			/* add carry */
	answer = ~sum;				/* truncate to 16 bits */
	return (answer);
}
