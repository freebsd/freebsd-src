/*
 * Copyright (C) 1995-1997 by Darren Reed.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 */
#include <stdio.h>
#include <ctype.h>
#include <assert.h>
#include <string.h>
#include <sys/types.h>
#if !defined(__SVR4) && !defined(__svr4__)
#include <strings.h>
#else
#include <sys/byteorder.h>
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
#include <arpa/inet.h>
#include <net/if.h>
#include <netdb.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include "ip_compat.h"
#include <netinet/tcpip.h>
#include "ipf.h"
#include "ipt.h"

#if !defined(lint)
static const char sccsid[] = "@(#)ipft_tx.c	1.7 6/5/96 (C) 1993 Darren Reed";
static const char rcsid[] = "@(#)$Id: ipft_tx.c,v 2.0.2.11.2.1 1997/11/12 10:56:11 darrenr Exp $";
#endif

extern	int	opts;

static	char	*tx_proto = "";

static	int	text_open __P((char *)), text_close __P((void));
static	int	text_readip __P((char *, int, char **, int *));
static	int	parseline __P((char *, ip_t *, char **, int *));

static	char	tcp_flagset[] = "FSRPAU";
static	u_char	tcp_flags[] = { TH_FIN, TH_SYN, TH_RST, TH_PUSH,
				TH_ACK, TH_URG };

struct	ipread	iptext = { text_open, text_close, text_readip };
static	FILE	*tfp = NULL;
static	int	tfd = -1;

static	u_long	tx_hostnum __P((char *, int *));
static	u_short	tx_portnum __P((char *));


/*
 * returns an ip address as a long var as a result of either a DNS lookup or
 * straight inet_addr() call
 */
static	u_long	tx_hostnum(host, resolved)
char	*host;
int	*resolved;
{
	struct	hostent	*hp;
	struct	netent	*np;

	*resolved = 0;
	if (!strcasecmp("any",host))
		return 0L;
	if (isdigit(*host))
		return inet_addr(host);

	if (!(hp = gethostbyname(host))) {
		if (!(np = getnetbyname(host))) {
			*resolved = -1;
			fprintf(stderr, "can't resolve hostname: %s\n", host);
			return 0;
		}
		return np->n_net;
	}
	return *(u_32_t *)hp->h_addr;
}


/*
 * find the port number given by the name, either from getservbyname() or
 * straight atoi()
 */
static	u_short	tx_portnum(name)
char	*name;
{
	struct	servent	*sp, *sp2;
	u_short	p1 = 0;

	if (isdigit(*name))
		return (u_short)atoi(name);
	if (!tx_proto)
		tx_proto = "tcp/udp";
	if (strcasecmp(tx_proto, "tcp/udp")) {
		sp = getservbyname(name, tx_proto);
		if (sp)
			return ntohs(sp->s_port);
		(void) fprintf(stderr, "unknown service \"%s\".\n", name);
		return 0;
	}
	sp = getservbyname(name, "tcp");
	if (sp)
		p1 = sp->s_port;
	sp2 = getservbyname(name, "udp");
	if (!sp || !sp2) {
		(void) fprintf(stderr, "unknown tcp/udp service \"%s\".\n",
			name);
		return 0;
	}
	if (p1 != sp2->s_port) {
		(void) fprintf(stderr, "%s %d/tcp is a different port to ",
			name, p1);
		(void) fprintf(stderr, "%s %d/udp\n", name, sp->s_port);
		return 0;
	}
	return ntohs(p1);
}


char	*tx_icmptypes[] = {
	"echorep", (char *)NULL, (char *)NULL, "unreach", "squench",
	"redir", (char *)NULL, (char *)NULL, "echo", "routerad",
	"routersol", "timex", "paramprob", "timest", "timestrep",
	"inforeq", "inforep", "maskreq", "maskrep", "END"
};

static	int	text_open(fname)
char	*fname;
{
	if (tfp && tfd != -1) {
		rewind(tfp);
		return tfd;
	}

	if (!strcmp(fname, "-")) {
		tfd = 0;
		tfp = stdin;
	} else {
		tfd = open(fname, O_RDONLY);
		if (tfd != -1)
			tfp = fdopen(tfd, "r");
	}
	return tfd;
}


static	int	text_close()
{
	int	cfd = tfd;

	tfd = -1;
	return close(cfd);
}


static	int	text_readip(buf, cnt, ifn, dir)
char	*buf, **ifn;
int	cnt, *dir;
{
	register char *s;
	ip_t *ip;
	char	line[513];

 	ip = (ip_t *)buf;
	*ifn = NULL;
	while (fgets(line, sizeof(line)-1, tfp)) {
		if ((s = index(line, '\n')))
			*s = '\0';
		if ((s = index(line, '\r')))
			*s = '\0';
		if ((s = index(line, '#')))
			*s = '\0';
		if (!*line)
			continue;
		if (!(opts & OPT_BRIEF))
			printf("input: %s\n", line);
		*ifn = NULL;
		*dir = 0;
		if (!parseline(line, (ip_t *)buf, ifn, dir))
#if 0
			return sizeof(struct tcpiphdr);
#else
			return sizeof(ip_t);
#endif
	}
	return -1;
}

static	int	parseline(line, ip, ifn, out)
char	*line;
ip_t	*ip;
char	**ifn;
int	*out;
{
	tcphdr_t	th, *tcp = &th;
	struct	icmp	icmp, *ic = &icmp;
	char	*cps[20], **cpp, c, ipopts[68];
	int	i, r;

	if (*ifn)
		free(*ifn);
	bzero((char *)ip, MAX(sizeof(*tcp), sizeof(*ic)) + sizeof(*ip));
	bzero((char *)tcp, sizeof(*tcp));
	bzero((char *)ic, sizeof(*ic));
	bzero(ipopts, sizeof(ipopts));
	ip->ip_hl = sizeof(*ip) >> 2;
	ip->ip_v = IPVERSION;
	for (i = 0, cps[0] = strtok(line, " \b\t\r\n"); cps[i] && i < 19; )
		cps[++i] = strtok(NULL, " \b\t\r\n");
	if (i < 2)
		return 1;

	cpp = cps;

	c = **cpp;
	if (!isalpha(c) || (tolower(c) != 'o' && tolower(c) != 'i')) {
		fprintf(stderr, "bad direction \"%s\"\n", *cpp);
		return 1;
	}
	*out = (tolower(c) == 'o') ? 1 : 0;
	cpp++;

	if (!strcasecmp(*cpp, "on")) {
		cpp++;
		if (!*cpp)
			return 1;
		*ifn = strdup(*cpp++);
	}

	c = **cpp;
	ip->ip_len = sizeof(ip_t);
	if (!strcasecmp(*cpp, "tcp") || !strcasecmp(*cpp, "udp") ||
	    !strcasecmp(*cpp, "icmp")) {
		if (c == 't') {
			ip->ip_p = IPPROTO_TCP;
			ip->ip_len += sizeof(struct tcphdr);
			tx_proto = "tcp";
		} else if (c == 'u') {
			ip->ip_p = IPPROTO_UDP;
			ip->ip_len += sizeof(struct udphdr);
			tx_proto = "udp";
		} else {
			ip->ip_p = IPPROTO_ICMP;
			ip->ip_len += sizeof(struct icmp);
			tx_proto = "icmp";
		}
		cpp++;
	} else
		ip->ip_p = IPPROTO_IP;

	if (!*cpp)
		return 1;
	if (ip->ip_p == IPPROTO_TCP || ip->ip_p == IPPROTO_UDP) {
		char	*last;

		last = index(*cpp, ',');
		if (!last) {
			fprintf(stderr, "tcp/udp with no source port\n");
			return 1;
		}
		*last++ = '\0';
		tcp->th_sport = htons(tx_portnum(last));
	}
	ip->ip_src.s_addr = tx_hostnum(*cpp, &r);
	cpp++;
	if (!*cpp)
		return 1;

	if (ip->ip_p == IPPROTO_TCP || ip->ip_p == IPPROTO_UDP) {
		char	*last;

		last = index(*cpp, ',');
		if (!last) {
			fprintf(stderr, "tcp/udp with no destination port\n");
			return 1;
		}
		*last++ = '\0';
		tcp->th_dport = htons(tx_portnum(last));
	}
	ip->ip_dst.s_addr = tx_hostnum(*cpp, &r);
	cpp++;
	if (*cpp && ip->ip_p == IPPROTO_TCP) {
		extern	char	tcp_flagset[];
		extern	u_char	tcp_flags[];
		char	*s, *t;

		for (s = *cpp; *s; s++)
			if ((t  = index(tcp_flagset, *s)))
				tcp->th_flags |= tcp_flags[t - tcp_flagset];
		if (tcp->th_flags)
			cpp++;
		assert(tcp->th_flags != 0);
	} else if (*cpp && ip->ip_p == IPPROTO_ICMP) {
		extern	char	*tx_icmptypes[];
		char	**s, *t;
		int	i;

		for (s = tx_icmptypes, i = 0; !*s || strcmp(*s, "END");
		     s++, i++)
			if (*s && !strncasecmp(*cpp, *s, strlen(*s))) {
				ic->icmp_type = i;
				if ((t = index(*cpp, ',')))
					ic->icmp_code = atoi(t+1);
				cpp++;
				break;
			}
	}

	if (*cpp && !strcasecmp(*cpp, "opt")) {
		u_long	olen;

		cpp++;
		olen = buildopts(*cpp, ipopts, (ip->ip_hl - 5) << 2);
		if (olen) {
			bcopy(ipopts, (char *)(ip + 1), olen);
			ip->ip_hl += olen >> 2;
		}
	}
	if (ip->ip_p == IPPROTO_TCP || ip->ip_p == IPPROTO_UDP)
		bcopy((char *)tcp, ((char *)ip) + (ip->ip_hl << 2),
			sizeof(*tcp));
	else if (ip->ip_p == IPPROTO_ICMP)
		bcopy((char *)ic, ((char *)ip) + (ip->ip_hl << 2),
			sizeof(*ic));
	return 0;
}
