/*
 * (C)opyright 1993-1996 by Darren Reed.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 */
#include <stdio.h>
#include <assert.h>
#include <string.h>
#if !defined(__SVR4) && !defined(__svr4__)
#include <strings.h>
#else
#include <sys/byteorder.h>
#endif
#include <sys/types.h>
#include <sys/param.h>
#include <stdlib.h>
#include <unistd.h>
#include <stddef.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip_var.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>
#include <netinet/ip_icmp.h>
#include <netinet/tcpip.h>
#include <net/if.h>
#include "ip_fil.h"
#include <netdb.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>
#include <resolv.h>
#include "ipf.h"
#include "ipt.h"
#include <ctype.h>

#if !defined(lint) && defined(LIBC_SCCS)
static	char	sccsid[] = "@(#)ipt.c	1.19 6/3/96 (C) 1993-1996 Darren Reed";
static	char	rcsid[] = "$Id: ipt.c,v 2.0.1.1 1997/01/09 15:14:44 darrenr Exp $";
#endif

extern	int	fr_check();
extern	char	*optarg;
extern	struct	frentry	*ipfilter[2][2];
extern	struct	ipread	snoop, etherf, tcpd, pcap, iptext, iphex;
extern	void	debug(), verbose();

struct frentry	*ft_in  = NULL, *ft_out = NULL;

int	opts = 0;

int main(argc,argv)
int argc;
char *argv[];
{
	struct	ipread	*r = &iptext;
	struct	frentry	*f;
	struct	ip	*ip;
	u_long	buf[64];
	char	c;
	char	*rules = NULL, *datain = NULL, *iface = NULL;
	int	fd, i, dir = 0;

	while ((c = getopt(argc, argv, "bdEHi:I:Pr:STvX")) != -1)
		switch (c)
		{
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

	initparse();

	if (rules) {
		struct	frentry *fr;
		char	line[513], *s;
		FILE	*fp;

		if (!strcmp(rules, "-"))
			fp = stdin;
		else if (!(fp = fopen(rules, "r"))) {
			(void)fprintf(stderr, "couldn't open %s\n", rules);
			exit(-1);
		}
		if (!(opts & OPT_BRIEF))
			(void)printf("opening rule file \"%s\"\n", rules);
		while (fgets(line, sizeof(line)-1, fp)) {
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

			if (!(fr = parse(line)))
				continue;
			f = (struct frentry *)malloc(sizeof(*f));
			if (fr->fr_flags & FR_INQUE) {
				if (!ft_in)
					ft_in = ipfilter[0][0] = f;
				else
					ft_in->fr_next = f, ft_in = f;
			} else if (fr->fr_flags & FR_OUTQUE) {
				if (!ft_out)
					ft_out = ipfilter[1][0] = f;
				else
					ft_out->fr_next = f, ft_out = f;
			}
			bcopy((char *)fr, (char *)f, sizeof(*fr));
		}
		(void)fclose(fp);
	}

	if (datain)
		fd = (*r->r_open)(datain);
	else
		fd = (*r->r_open)("-");

	if (fd < 0)
		exit(-1);

	ip = (struct ip *)buf;
	while ((i = (*r->r_readip)(buf, sizeof(buf), &iface, &dir)) > 0) {
		ip->ip_off = ntohs(ip->ip_off);
		ip->ip_len = ntohs(ip->ip_len);
		switch (fr_check(ip, ip->ip_hl << 2, iface, dir))
		{
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
			printpacket(buf);
			printf("--------------");
		}
		putchar('\n');
		dir = 0;
	}
	(*r->r_close)();
	return 0;
}
