/*
 * Copyright (C) 1993-2001 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 */
#ifdef	__FreeBSD__
# ifndef __FreeBSD_cc_version
#  include <osreldate.h>
# else
#  if __FreeBSD_cc_version < 430000
#   include <osreldate.h>
#  endif
# endif
#endif
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <sys/types.h>
#if !defined(__SVR4) && !defined(__svr4__) && !defined(__sgi)
#include <strings.h>
#else
#if !defined(__sgi)
#include <sys/byteorder.h>
#endif
#include <sys/file.h>
#endif
#include <sys/param.h>
#include <sys/time.h>
#include <stdlib.h>
#include <unistd.h>
#include <stddef.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#ifndef	linux
#include <netinet/ip_var.h>
#endif
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>
#include <netinet/ip_icmp.h>
#include <net/if.h>
#if __FreeBSD_version >= 300000
# include <net/if_var.h>
#endif
#include <netdb.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>
#include <resolv.h>
#include <ctype.h>
#include "ip_compat.h"
#include <netinet/tcpip.h>
#include "ip_fil.h"
#include "ip_nat.h"
#include "ip_state.h"
#include "ipf.h"
#include "ipt.h"

#if !defined(lint)
static const char sccsid[] = "@(#)ipt.c	1.19 6/3/96 (C) 1993-2000 Darren Reed";
static const char rcsid[] = "@(#)$Id: ipt.c,v 2.6.2.2 2001/06/26 10:43:19 darrenr Exp $";
#endif

extern	char	*optarg;
extern	struct frentry	*ipfilter[2][2];
extern	struct ipread	snoop, etherf, tcpd, pcap, iptext, iphex;
extern	struct ifnet	*get_unit __P((char *, int));
extern	void	init_ifp __P((void));
extern	ipnat_t	*natparse __P((char *, int));
extern	int	fr_running;

int	opts = 0;
#ifdef	USE_INET6
int	use_inet6 = 0;
#endif
int	main __P((int, char *[]));

int main(argc,argv)
int argc;
char *argv[];
{
	struct	ipread	*r = &iptext;
	u_long	buf[2048];
	struct	ifnet	*ifp;
	char	*rules = NULL, *datain = NULL, *iface = NULL;
	ip_t	*ip;
	int	fd, i, dir = 0, c;

	while ((c = getopt(argc, argv, "6bdEHi:I:NoPr:STvX")) != -1)
		switch (c)
		{
#ifdef	USE_INET6
		case '6' :
			use_inet6 = 1;
			break;
#endif
		case 'b' :
			opts |= OPT_BRIEF;
			break;
		case 'd' :
			opts |= OPT_DEBUG;
			break;
		case 'i' :
			datain = optarg;
			break;
		case 'I' :
			iface = optarg;
			break;
		case 'o' :
			opts |= OPT_SAVEOUT;
			break;
		case 'r' :
			rules = optarg;
			break;
		case 'v' :
			opts |= OPT_VERBOSE;
			break;
		case 'E' :
			r = &etherf;
			break;
		case 'H' :
			r = &iphex;
			break;
		case 'N' :
			opts |= OPT_NAT;
			break;
		case 'P' :
			r = &pcap;
			break;
		case 'S' :
			r = &snoop;
			break;
		case 'T' :
			r = &tcpd;
			break;
		case 'X' :
			r = &iptext;
			break;
		}

	if (!rules) {
		(void)fprintf(stderr,"no rule file present\n");
		exit(-1);
	}

	nat_init();
	fr_stateinit();
	initparse();
	fr_running = 1;

	if (rules) {
		char	line[513], *s;
		void	*fr;
		FILE	*fp;
		int     linenum = 0;

		if (!strcmp(rules, "-"))
			fp = stdin;
		else if (!(fp = fopen(rules, "r"))) {
			(void)fprintf(stderr, "couldn't open %s\n", rules);
			exit(-1);
		}
		if (!(opts & OPT_BRIEF))
			(void)printf("opening rule file \"%s\"\n", rules);
		while (fgets(line, sizeof(line)-1, fp)) {
		        linenum++;
			/*
			 * treat both CR and LF as EOL
			 */
			if ((s = index(line, '\n')))
				*s = '\0';
			if ((s = index(line, '\r')))
				*s = '\0';
			/*
			 * # is comment marker, everything after is a ignored
			 */
			if ((s = index(line, '#')))
				*s = '\0';

			if (!*line)
				continue;

			/* fake an `ioctl' call :) */

			if ((opts & OPT_NAT) != 0) {
				if (!(fr = natparse(line, linenum)))
					continue;
				i = IPL_EXTERN(ioctl)(IPL_LOGNAT, SIOCADNAT,
						      (caddr_t)&fr,
						      FWRITE|FREAD);
				if (opts & OPT_DEBUG)
					fprintf(stderr,
						"iplioctl(ADNAT,%p,1) = %d\n",
						fr, i);
			} else {
				if (!(fr = parse(line, linenum)))
					continue;
				i = IPL_EXTERN(ioctl)(0, SIOCADAFR,
						      (caddr_t)&fr,
						      FWRITE|FREAD);
				if (opts & OPT_DEBUG)
					fprintf(stderr,
						"iplioctl(ADAFR,%p,1) = %d\n",
						fr, i);
			}
		}
		(void)fclose(fp);
	}

	if (opts & OPT_SAVEOUT)
		init_ifp();

	if (datain)
		fd = (*r->r_open)(datain);
	else
		fd = (*r->r_open)("-");

	if (fd < 0)
		exit(-1);

	ip = (ip_t *)buf;
	while ((i = (*r->r_readip)((char *)buf, sizeof(buf),
				    &iface, &dir)) > 0) {
		ifp = iface ? get_unit(iface, ip->ip_v) : NULL;
		ip->ip_off = ntohs(ip->ip_off);
		ip->ip_len = ntohs(ip->ip_len);
		i = fr_check(ip, ip->ip_hl << 2, ifp, dir, (mb_t **)&buf);
		if ((opts & OPT_NAT) == 0)
			switch (i)
			{
			case -2 :
				(void)printf("auth");
				break;
			case -1 :
				(void)printf("block");
				break;
			case 0 :
				(void)printf("pass");
				break;
			case 1 :
				(void)printf("nomatch");
				break;
			}

		if (!(opts & OPT_BRIEF)) {
			putchar(' ');
			printpacket((ip_t *)buf);
			printf("--------------");
		} else if ((opts & (OPT_BRIEF|OPT_NAT)) == (OPT_NAT|OPT_BRIEF))
			printpacket((ip_t *)buf);
#ifndef	linux
		if (dir && ifp && ip->ip_v)
# ifdef __sgi
			(*ifp->if_output)(ifp, (void *)buf, NULL);
# else
			(*ifp->if_output)(ifp, (void *)buf, NULL, 0);
# endif
#endif
		if ((opts & (OPT_BRIEF|OPT_NAT)) != (OPT_NAT|OPT_BRIEF))
			putchar('\n');
		dir = 0;
	}
	(*r->r_close)();
	return 0;
}
