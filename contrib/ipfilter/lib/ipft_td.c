/*	$FreeBSD$	*/

/*
 * Copyright (C) 1993-2001 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * Id: ipft_td.c,v 1.15 2004/01/08 13:34:31 darrenr Exp
 */

/*
tcpdump -n

00:05:47.816843 128.231.76.76.3291 > 224.2.252.231.36573: udp 36 (encap)

tcpdump -nq

00:33:48.410771 192.73.213.11.1463 > 224.2.248.153.59360: udp 31 (encap)

tcpdump -nqt

128.250.133.13.23 > 128.250.20.20.2419: tcp 27

tcpdump -nqtt

123456789.1234567 128.250.133.13.23 > 128.250.20.20.2419: tcp 27

tcpdump -nqte

8:0:20:f:65:f7 0:0:c:1:8a:c5 81: 128.250.133.13.23 > 128.250.20.20.2419: tcp 27

*/

#include "ipf.h"
#include "ipt.h"

#ifndef linux
#include <netinet/ip_var.h>
#endif
#include <netinet/tcpip.h>


#if !defined(lint)
static const char sccsid[] = "@(#)ipft_td.c	1.8 2/4/96 (C)1995 Darren Reed";
static const char rcsid[] = "@(#)Id: ipft_td.c,v 1.15 2004/01/08 13:34:31 darrenr Exp";
#endif

static	int	tcpd_open __P((char *));
static	int	tcpd_close __P((void));
static	int	tcpd_readip __P((char *, int, char **, int *));
static	int	count_dots __P((char *));

struct	ipread	tcpd = { tcpd_open, tcpd_close, tcpd_readip, 0 };

static	FILE	*tfp = NULL;
static	int	tfd = -1;


static	int	tcpd_open(fname)
char	*fname;
{
	if (tfd != -1)
		return tfd;

	if (!strcmp(fname, "-")) {
		tfd = 0;
		tfp = stdin;
	} else {
		tfd = open(fname, O_RDONLY);
		tfp = fdopen(tfd, "r");
	}
	return tfd;
}


static	int	tcpd_close()
{
	(void) fclose(tfp);
	return close(tfd);
}


static	int	count_dots(str)
char	*str;
{
	int	i = 0;

	while (*str)
		if (*str++ == '.')
			i++;
	return i;
}


static	int	tcpd_readip(buf, cnt, ifn, dir)
char	*buf, **ifn;
int	cnt, *dir;
{
	struct	tcpiphdr pkt;
	ip_t	*ip = (ip_t *)&pkt;
	char	src[32], dst[32], misc[256], time[32], link1[32], link2[32];
	char	lbuf[160], *s;
	int	n, slen, extra = 0;

	if (!fgets(lbuf, sizeof(lbuf) - 1, tfp))
		return 0;

	if ((s = strchr(lbuf, '\n')))
		*s = '\0';
	lbuf[sizeof(lbuf)-1] = '\0';

	bzero(&pkt, sizeof(pkt));

	if ((n = sscanf(lbuf, "%31s > %31s: %255s", src, dst, misc)) != 3)
		if ((n = sscanf(lbuf, "%31s %31s > %31s: %255s",
				time, src, dst, misc)) != 4)
			if ((n = sscanf(lbuf, "%31s %31s: %31s > %31s: %255s",
					link1, link2, src, dst, misc)) != 5) {
				n = sscanf(lbuf,
					   "%31s %31s %31s: %31s > %31s: %255s",
					   time, link1, link2, src, dst, misc);
				if (n != 6)
					return -1;
			}

	if (count_dots(dst) == 4) {
		s = strrchr(src, '.');
		*s++ = '\0';
		(void) inet_aton(src, &ip->ip_src);
		pkt.ti_sport = htons(atoi(s));
		*--s = '.';
		s = strrchr(dst, '.');
	
		*s++ = '\0';
		(void) inet_aton(src, &ip->ip_dst);
		pkt.ti_dport = htons(atoi(s));
		*--s = '.';
	
	} else {
		(void) inet_aton(src, &ip->ip_src);
		(void) inet_aton(src, &ip->ip_dst);
	}
	ip->ip_len = sizeof(ip_t);
	IP_HL_A(ip, sizeof(ip_t));

	s = strtok(misc, " :");
	ip->ip_p = getproto(s);

	switch (ip->ip_p)
	{
	case IPPROTO_TCP :
	case IPPROTO_UDP :
		s = strtok(NULL, " :");
		ip->ip_len += atoi(s);
		if (ip->ip_p == IPPROTO_TCP)
			extra = sizeof(struct tcphdr);
		else if (ip->ip_p == IPPROTO_UDP)
			extra = sizeof(struct udphdr);
		break;
#ifdef	IGMP
	case IPPROTO_IGMP :
		extra = sizeof(struct igmp);
		break;
#endif
	case IPPROTO_ICMP :
		extra = sizeof(struct icmp);
		break;
	default :
		break;
	}

	slen = IP_HL(ip) + extra + ip->ip_len;
	return slen;
}
