/*
 * Copyright (C) 1993-1997 by Darren Reed.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 */
#include <stdio.h>
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
#include "ip_compat.h"
#include "ip_fil.h"
#include "ipf.h"

#if !defined(lint)
static const char sccsid[] ="@(#)parse.c	1.44 6/5/96 (C) 1993-1996 Darren Reed";
static const char rcsid[] = "@(#)$Id: parse.c,v 2.0.2.18 1997/10/19 15:39:29 darrenr Exp $";
#endif

extern	struct	ipopt_names	ionames[], secclass[];
extern	int	opts;

u_short	portnum __P((char *));
u_char	tcp_flags __P((char *, u_char *));
int	addicmp __P((char ***, struct frentry *));
int	extras __P((char ***, struct frentry *));
char    ***seg;
u_long  *sa, *msk;
u_short *pp, *tp;
u_char  *cp;

int	hostmask __P((char ***, u_32_t *, u_32_t *, u_short *, u_char *,
		      u_short *));
int	ports __P((char ***, u_short *, u_char *, u_short *));
int	icmpcode __P((char *)), addkeep __P((char ***, struct frentry *));
int	to_interface __P((frdest_t *, char *));
void	print_toif __P((char *, frdest_t *));
void	optprint __P((u_short, u_short, u_long, u_long));
int	countbits __P((u_long));
char	*portname __P((int, int));


char	*proto = NULL;
char	flagset[] = "FSRPAU";
u_char	flags[] = { TH_FIN, TH_SYN, TH_RST, TH_PUSH, TH_ACK, TH_URG };

static	char	thishost[64];


void initparse()
{
	gethostname(thishost, sizeof(thishost));
	thishost[sizeof(thishost) - 1] = '\0';
}


/* parse()
 *
 * parse a line read from the input filter rule file
 */
struct	frentry	*parse(line)
char	*line;
{
	static	struct	frentry	fil;
	struct	protoent	*p = NULL;
	char	*cps[31], **cpp;
	u_char	ch;
	int	i, cnt = 1;

	while (*line && isspace(*line))
		line++;
	if (!*line)
		return NULL;

	bzero((char *)&fil, sizeof(fil));
	fil.fr_mip.fi_v = 0xf;
	fil.fr_ip.fi_v = 4;
	/*
	 * break line up into max of 20 segments
	 */
	if (opts & OPT_DEBUG)
		fprintf(stderr, "parse [%s]\n", line);
	for (i = 0, *cps = strtok(line, " \b\t\r\n"); cps[i] && i < 30; cnt++)
		cps[++i] = strtok(NULL, " \b\t\r\n");
	cps[i] = NULL;

	if (cnt < 3) {
		(void)fprintf(stderr,"not enough segments in line\n");
		return NULL;
	}

	cpp = cps;
	if (**cpp == '@')
		fil.fr_hits = (U_QUAD_T)atoi(*cpp++ + 1) + 1;


	if (!strcasecmp("block", *cpp)) {
		fil.fr_flags |= FR_BLOCK;
		if (!strncasecmp(*(cpp+1), "return-icmp", 11)) {
			fil.fr_flags |= FR_RETICMP;
			cpp++;
			if (*(*cpp + 11) == '(') {
				i = icmpcode(*cpp + 12);
				if (i == -1) {
					fprintf(stderr,
						"uncrecognised icmp code %s\n",
						*cpp + 12);
					return NULL;
				}
				fil.fr_icode = i;
			}
		} else if (!strncasecmp(*(cpp+1), "return-rst", 10)) {
			fil.fr_flags |= FR_RETRST;
			cpp++;
		}
	} else if (!strcasecmp("count", *cpp)) {
		fil.fr_flags |= FR_ACCOUNT;
	} else if (!strcasecmp("pass", *cpp)) {
		fil.fr_flags |= FR_PASS;
	} else if (!strcasecmp("auth", *cpp)) {
		 fil.fr_flags |= FR_AUTH;
	} else if (!strcasecmp("preauth", *cpp)) {
		 fil.fr_flags |= FR_PREAUTH;
	} else if (!strcasecmp("skip", *cpp)) {
		cpp++;
		if (!isdigit(**cpp)) {
			(void)fprintf(stderr, "integer must follow skip\n");
			return NULL;
		}
		fil.fr_skip = atoi(*cpp);
	} else if (!strcasecmp("log", *cpp)) {
		fil.fr_flags |= FR_LOG;
		if (!strcasecmp(*(cpp+1), "body")) {
			fil.fr_flags |= FR_LOGBODY;
			cpp++;
		}
		if (!strcasecmp(*(cpp+1), "first")) {
			fil.fr_flags |= FR_LOGFIRST;
			cpp++;
		}
	} else {
		/*
		 * Doesn't start with one of the action words
		 */
		(void)fprintf(stderr, "unknown keyword (%s)\n", *cpp);
		return NULL;
	}
	cpp++;

	if (!strcasecmp("in", *cpp))
		fil.fr_flags |= FR_INQUE;
	else if (!strcasecmp("out", *cpp)) {
		fil.fr_flags |= FR_OUTQUE;
		if (fil.fr_flags & FR_RETICMP) {
			(void)fprintf(stderr,
				"Can only use return-icmp with 'in'\n");
			return NULL;
		} else if (fil.fr_flags & FR_RETRST) {
			(void)fprintf(stderr,
				"Can only use return-rst with 'in'\n");
			return NULL;
		}
	} else {
		(void)fprintf(stderr,
			"missing 'in'/'out' keyword (%s)\n", *cpp);
		return NULL;
	}
	if (!*++cpp)
		return NULL;

	if (!strcasecmp("log", *cpp)) {
		cpp++;
		if (fil.fr_flags & FR_PASS)
			fil.fr_flags |= FR_LOGP;
		else if (fil.fr_flags & FR_BLOCK)
			fil.fr_flags |= FR_LOGB;
		if (!strcasecmp(*cpp, "body")) {
			fil.fr_flags |= FR_LOGBODY;
			cpp++;
		}
		if (!strcasecmp(*cpp, "first")) {
			fil.fr_flags |= FR_LOGFIRST;
			cpp++;
		}
		if (!strcasecmp(*cpp, "or-block")) {
			if (!(fil.fr_flags & FR_PASS)) {
				(void)fprintf(stderr,
					"or-block must be used with pass\n");
				return NULL;
			}
			fil.fr_flags |= FR_LOGORBLOCK;
			cpp++;
		}
	}

	if (!strcasecmp("quick", *cpp)) {
		cpp++;
		fil.fr_flags |= FR_QUICK;
	}

	*fil.fr_ifname = '\0';
	if (*cpp && !strcasecmp(*cpp, "on")) {
		if (!*++cpp) {
			(void)fprintf(stderr, "interface name missing\n");
			return NULL;
		}
		(void)strncpy(fil.fr_ifname, *cpp, IFNAMSIZ-1);
		fil.fr_ifname[IFNAMSIZ-1] = '\0';
		cpp++;
		if (!*cpp) {
			if (fil.fr_flags & FR_RETRST) {
				(void)fprintf(stderr,
					"%s can only be used with TCP\n",
					"return-rst");
				return NULL;
			}
			return &fil;
		}

		if (*cpp) {
			if (!strcasecmp(*cpp, "dup-to") && *(cpp + 1)) {
				cpp++;
				if (to_interface(&fil.fr_dif, *cpp))
					return NULL;
				cpp++;
			}
			if (!strcasecmp(*cpp, "to") && *(cpp + 1)) {
				cpp++;
				if (to_interface(&fil.fr_tif, *cpp))
					return NULL;
				cpp++;
			} else if (!strcasecmp(*cpp, "fastroute")) {
				fil.fr_flags |= FR_FASTROUTE;
				cpp++;
			}
		}
	}
	if (*cpp && !strcasecmp(*cpp, "tos")) {
		if (!*++cpp) {
			(void)fprintf(stderr, "tos missing value\n");
			return NULL;
		}
		fil.fr_tos = strtol(*cpp, NULL, 0);
		fil.fr_mip.fi_tos = 0xff;
		cpp++;
	}

	if (*cpp && !strcasecmp(*cpp, "ttl")) {
		if (!*++cpp) {
			(void)fprintf(stderr, "ttl missing hopcount value\n");
			return NULL;
		}
		fil.fr_ttl = atoi(*cpp);
		fil.fr_mip.fi_ttl = 0xff;
		cpp++;
	}

	/*
	 * check for "proto <protoname>" only decode udp/tcp/icmp as protoname
	 */
	proto = NULL;
	if (*cpp && !strcasecmp(*cpp, "proto")) {
		if (!*++cpp) {
			(void)fprintf(stderr, "protocol name missing\n");
			return NULL;
		}
		if (!strcasecmp(*cpp, "tcp/udp")) {
			fil.fr_ip.fi_fl |= FI_TCPUDP;
			fil.fr_mip.fi_fl |= FI_TCPUDP;
		} else {
			if (!(p = getprotobyname(*cpp)) && !isdigit(**cpp)) {
				(void)fprintf(stderr,
					"unknown protocol (%s)\n", *cpp);
				return NULL;
			}
			if (p)
				fil.fr_proto = p->p_proto;
			else if (isdigit(**cpp))
				fil.fr_proto = atoi(*cpp);
			fil.fr_mip.fi_p = 0xff;
		}
		proto = *cpp;
		if (fil.fr_proto != IPPROTO_TCP && fil.fr_flags & FR_RETRST) {
			(void)fprintf(stderr,
				"%s can only be used with TCP\n",
				"return-rst");
			return NULL;
		}
		if (!*++cpp)
			return &fil;
	}
	if (fil.fr_proto != IPPROTO_TCP && fil.fr_flags & FR_RETRST) {
		(void)fprintf(stderr, "%s can only be used with TCP\n",
			"return-rst");
		return NULL;
	}

	/*
	 * get the from host and bit mask to use against packets
	 */

	if (!*cpp) {
		fprintf(stderr, "missing source specification\n");
		return NULL;
	}
	if (!strcasecmp(*cpp, "all")) {
		cpp++;
		if (!*cpp)
			return &fil;
	} else {
		if (strcasecmp(*cpp, "from")) {
			(void)fprintf(stderr,
				"unexpected keyword (%s) - from\n", *cpp);
			return NULL;
		}
		if (!*++cpp) {
			(void)fprintf(stderr, "missing host after from\n");
			return NULL;
		}
		ch = 0;
		if (**cpp == '!') {
			fil.fr_flags |= FR_NOTSRCIP;
			(*cpp)++;
		}
		if (hostmask(&cpp, (u_32_t *)&fil.fr_src,
			     (u_32_t *)&fil.fr_smsk, &fil.fr_sport, &ch,
			     &fil.fr_stop)) {
			(void)fprintf(stderr, "bad host (%s)\n", *cpp);
			return NULL;
		}
		fil.fr_scmp = ch;
		if (!*cpp) {
			(void)fprintf(stderr, "missing to fields\n");
			return NULL;
		}

		/*
		 * do the same for the to field (destination host)
		 */
		if (strcasecmp(*cpp, "to")) {
			(void)fprintf(stderr,
				"unexpected keyword (%s) - to\n", *cpp);
			return NULL;
		}
		if (!*++cpp) {
			(void)fprintf(stderr, "missing host after to\n");
			return NULL;
		}
		ch = 0;
		if (**cpp == '!') {
			fil.fr_flags |= FR_NOTDSTIP;
			(*cpp)++;
		}
		if (hostmask(&cpp, (u_32_t *)&fil.fr_dst,
			     (u_32_t *)&fil.fr_dmsk, &fil.fr_dport, &ch,
			     &fil.fr_dtop)) {
			(void)fprintf(stderr, "bad host (%s)\n", *cpp);
			return NULL;
		}
		fil.fr_dcmp = ch;
	}

	/*
	 * check some sanity, make sure we don't have icmp checks with tcp
	 * or udp or visa versa.
	 */
	if (fil.fr_proto && (fil.fr_dcmp || fil.fr_scmp) &&
	    fil.fr_proto != IPPROTO_TCP && fil.fr_proto != IPPROTO_UDP) {
		(void)fprintf(stderr, "port operation on non tcp/udp\n");
		return NULL;
	}
	if (fil.fr_icmp && fil.fr_proto != IPPROTO_ICMP) {
		(void)fprintf(stderr, "icmp comparisons on wrong protocol\n");
		return NULL;
	}

	if (!*cpp)
		return &fil;

	if (*cpp && !strcasecmp(*cpp, "flags")) {
		if (!*++cpp) {
			(void)fprintf(stderr, "no flags present\n");
			return NULL;
		}
		fil.fr_tcpf = tcp_flags(*cpp, &fil.fr_tcpfm);
		cpp++;
	}

	/*
	 * extras...
	 */
	if (*cpp && (!strcasecmp(*cpp, "with") || !strcasecmp(*cpp, "and")))
		if (extras(&cpp, &fil))
			return NULL;

	/*
	 * icmp types for use with the icmp protocol
	 */
	if (*cpp && !strcasecmp(*cpp, "icmp-type")) {
		if (fil.fr_proto != IPPROTO_ICMP) {
			(void)fprintf(stderr,
				"icmp with wrong protocol (%d)\n",
				fil.fr_proto);
			return NULL;
		}
		if (addicmp(&cpp, &fil))
			return NULL;
		fil.fr_icmp = htons(fil.fr_icmp);
		fil.fr_icmpm = htons(fil.fr_icmpm);
	}

	/*
	 * Keep something...
	 */
	while (*cpp && !strcasecmp(*cpp, "keep"))
		if (addkeep(&cpp, &fil))
			return NULL;

	/*
	 * head of a new group ?
	 */
	if (*cpp && !strcasecmp(*cpp, "head")) {
		if (!*++cpp) {
			(void)fprintf(stderr, "head without group #\n");
			return NULL;
		}
		fil.fr_grhead = atoi(*cpp);
		cpp++;
	}

	/*
	 * head of a new group ?
	 */
	if (*cpp && !strcasecmp(*cpp, "group")) {
		if (!*++cpp) {
			(void)fprintf(stderr, "group without group #\n");
			return NULL;
		}
		fil.fr_group = atoi(*cpp);
		cpp++;
	}

	/*
	 * leftovers...yuck
	 */
	if (*cpp && **cpp) {
		fprintf(stderr, "unknown words at end: [");
		for (; *cpp; cpp++)
			(void)fprintf(stderr, "%s ", *cpp);
		(void)fprintf(stderr, "]\n");
		return NULL;
	}

	/*
	 * lazy users...
	 */
	if (!fil.fr_proto && (fil.fr_dcmp || fil.fr_scmp || fil.fr_tcpf)) {
		(void)fprintf(stderr,
			"no protocol given for TCP/UDP comparisons\n");
		return NULL;
	}
/*
	if ((fil.fr_flags & FR_KEEPFRAG) &&
	    (!(fil.fr_ip.fi_fl & FI_FRAG) || !(fil.fr_ip.fi_fl & FI_FRAG))) {
		(void)fprintf(stderr,
			"must use 'with frags' with 'keep frags'\n");
		return NULL;
	}
*/
	return &fil;
}


int to_interface(fdp, to)
frdest_t *fdp;
char *to;
{
	int	r = 0;
	char	*s;

	s = index(to, ':');
	fdp->fd_ifp = NULL;
	if (s) {
		*s++ = '\0';
		fdp->fd_ip.s_addr = hostnum(s, &r);
		if (r == -1)
			return -1;
	}
	(void) strncpy(fdp->fd_ifname, to, sizeof(fdp->fd_ifname) - 1);
	fdp->fd_ifname[sizeof(fdp->fd_ifname) - 1] = '\0';
	return 0;
}


void print_toif(tag, fdp)
char *tag;
frdest_t *fdp;
{
	(void)printf("%s %s%s", tag, fdp->fd_ifname,
		     (fdp->fd_ifp || (long)fdp->fd_ifp == -1) ? "" : "(!)");
	if (fdp->fd_ip.s_addr)
		(void)printf(":%s", inet_ntoa(fdp->fd_ip));
	putchar(' ');
}


/*
 * returns false if neither "hostmask/num" or "hostmask mask addr" are
 * found in the line segments
 */
int	hostmask(seg, sa, msk, pp, cp, tp)
char	***seg;
u_32_t	*sa, *msk;
u_short	*pp, *tp;
u_char	*cp;
{
	char	*s;
	int	bits = -1, resolved;

	/*
	 * is it possibly hostname/num ?
	 */
	if ((s = index(**seg, '/'))) {
		*s++ = '\0';
		if (!isdigit(*s))
			return -1;
		if (index(s, '.'))
			*msk = inet_addr(s);
		if (!index(s, '.') && !index(s, 'x')) {
			/*
			 * set x most significant bits
			 */
			for (bits = atoi(s); bits; bits--) {
				*msk /= 2;
				*msk |= ntohl(inet_addr("128.0.0.0"));
			}
			*msk = htonl(*msk);
		} else {
			if (inet_aton(s, (struct in_addr *)msk) == -1)
				return -1;
		}
		*sa = hostnum(**seg, &resolved) & *msk;
		if (resolved == -1)
			return -1;
		(*seg)++;
		return ports(seg, pp, cp, tp);
	}

	/*
	 * look for extra segments if "mask" found in right spot
	 */
	if (*(*seg+1) && *(*seg+2) && !strcasecmp(*(*seg+1), "mask")) {
		*sa = hostnum(**seg, &resolved);
		if (resolved == -1)
			return -1;
		(*seg)++;
		(*seg)++;
		if (inet_aton(**seg, (struct in_addr *)msk) == -1)
			return -1;
		(*seg)++;
		*sa &= *msk;
		return ports(seg, pp, cp, tp);
	}

	if (**seg) {
		*sa = hostnum(**seg, &resolved);
		if (resolved == -1)
			return -1;
		(*seg)++;
		*msk = (*sa ? inet_addr("255.255.255.255") : 0L);
		*sa &= *msk;
		return ports(seg, pp, cp, tp);
	}
	return -1;
}

/*
 * returns an ip address as a long var as a result of either a DNS lookup or
 * straight inet_addr() call
 */
u_32_t	hostnum(host, resolved)
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
	if (!strcasecmp("<thishost>", host))
		host = thishost;

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
 * check for possible presence of the port fields in the line
 */
int	ports(seg, pp, cp, tp)
char	***seg;
u_short	*pp, *tp;
u_char	*cp;
{
	int	comp = -1;

	if (!*seg || !**seg || !***seg)
		return 0;
	if (!strcasecmp(**seg, "port") && *(*seg + 1) && *(*seg + 2)) {
		(*seg)++;
		if (isdigit(***seg) && *(*seg + 2)) {
			*pp = portnum(**seg);
			(*seg)++;
			if (!strcmp(**seg, "<>"))
				comp = FR_OUTRANGE;
			else if (!strcmp(**seg, "><"))
				comp = FR_INRANGE;
			(*seg)++;
			*tp = portnum(**seg);
		} else if (!strcmp(**seg, "=") || !strcasecmp(**seg, "eq"))
			comp = FR_EQUAL;
		else if (!strcmp(**seg, "!=") || !strcasecmp(**seg, "ne"))
			comp = FR_NEQUAL;
		else if (!strcmp(**seg, "<") || !strcasecmp(**seg, "lt"))
			comp = FR_LESST;
		else if (!strcmp(**seg, ">") || !strcasecmp(**seg, "gt"))
			comp = FR_GREATERT;
		else if (!strcmp(**seg, "<=") || !strcasecmp(**seg, "le"))
			comp = FR_LESSTE;
		else if (!strcmp(**seg, ">=") || !strcasecmp(**seg, "ge"))
			comp = FR_GREATERTE;
		else {
			(void)fprintf(stderr,"unknown comparator (%s)\n",
					**seg);
			return -1;
		}
		if (comp != FR_OUTRANGE && comp != FR_INRANGE) {
			(*seg)++;
			*pp = portnum(**seg);
		}
		*cp = comp;
		(*seg)++;
	}
	return 0;
}

/*
 * find the port number given by the name, either from getservbyname() or
 * straight atoi()
 */
u_short	portnum(name)
char	*name;
{
	struct	servent	*sp, *sp2;
	u_short	p1 = 0;

	if (isdigit(*name))
		return (u_short)atoi(name);
	if (!proto)
		proto = "tcp/udp";
	if (strcasecmp(proto, "tcp/udp")) {
		sp = getservbyname(name, proto);
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


u_char tcp_flags(flgs, mask)
char *flgs;
u_char *mask;
{
	u_char tcpf = 0, tcpfm = 0, *fp = &tcpf;
	char *s, *t;

	for (s = flgs; *s; s++) {
		if (*s == '/' && fp == &tcpf) {
			fp = &tcpfm;
			continue;
		}
		if (!(t = index(flagset, *s))) {
			(void)fprintf(stderr, "unknown flag (%c)\n", *s);
			return 0;
		}
		*fp |= flags[t - flagset];
	}
	if (!tcpfm)
		tcpfm = 0xff;
	*mask = tcpfm;
	return tcpf;
}


/*
 * deal with extra bits on end of the line
 */
int	extras(cp, fr)
char	***cp;
struct	frentry	*fr;
{
	u_short	secmsk;
	u_long	opts;
	int	notopt;
	char	oflags;

	opts = 0;
	secmsk = 0;
	notopt = 0;
	(*cp)++;
	if (!**cp)
		return -1;

	while (**cp && (!strncasecmp(**cp, "ipopt", 5) ||
	       !strncasecmp(**cp, "not", 3) || !strncasecmp(**cp, "opt", 4) ||
	       !strncasecmp(**cp, "frag", 3) || !strncasecmp(**cp, "no", 2) ||
	       !strncasecmp(**cp, "short", 5))) {
		if (***cp == 'n' || ***cp == 'N') {
			notopt = 1;
			(*cp)++;
			continue;
		} else if (***cp == 'i' || ***cp == 'I') {
			if (!notopt)
				fr->fr_ip.fi_fl |= FI_OPTIONS;
			fr->fr_mip.fi_fl |= FI_OPTIONS;
			goto nextopt;
		} else if (***cp == 'f' || ***cp == 'F') {
			if (!notopt)
				fr->fr_ip.fi_fl |= FI_FRAG;
			fr->fr_mip.fi_fl |= FI_FRAG;
			goto nextopt;
		} else if (***cp == 'o' || ***cp == 'O') {
			if (!*(*cp + 1)) {
				(void)fprintf(stderr,
					"opt missing arguements\n");
				return -1;
			}
			(*cp)++;
			if (!(opts = optname(cp, &secmsk)))
				return -1;
			oflags = FI_OPTIONS;
		} else if (***cp == 's' || ***cp == 'S') {
			if (fr->fr_tcpf) {
				(void) fprintf(stderr,
				    "short cannot be used with TCP flags\n");
				return -1;
			}

			if (!notopt)
				fr->fr_ip.fi_fl |= FI_SHORT;
			fr->fr_mip.fi_fl |= FI_SHORT;
			goto nextopt;
		} else
			return -1;

		if (!notopt || !opts)
			fr->fr_mip.fi_fl |= oflags;
		if (notopt)
			if (!secmsk)
				fr->fr_mip.fi_optmsk |= opts;
			else
				fr->fr_mip.fi_optmsk |= (opts & ~0x0100);
		else
				fr->fr_mip.fi_optmsk |= opts;
		fr->fr_mip.fi_secmsk |= secmsk;

		if (notopt) {
			fr->fr_ip.fi_fl &= (~oflags & 0xf);
			fr->fr_ip.fi_optmsk &= ~opts;
			fr->fr_ip.fi_secmsk &= ~secmsk;
		} else {
			fr->fr_ip.fi_fl |= oflags;
			fr->fr_ip.fi_optmsk |= opts;
			fr->fr_ip.fi_secmsk |= secmsk;
		}
nextopt:
		notopt = 0;
		opts = 0;
		oflags = 0;
		secmsk = 0;
		(*cp)++;
	}
	return 0;
}


u_32_t optname(cp, sp)
char ***cp;
u_short *sp;
{
	struct ipopt_names *io, *so;
	u_long msk = 0;
	u_short smsk = 0;
	char *s;
	int sec = 0;

	for (s = strtok(**cp, ","); s; s = strtok(NULL, ",")) {
		for (io = ionames; io->on_name; io++)
			if (!strcasecmp(s, io->on_name)) {
				msk |= io->on_bit;
				break;
			}
		if (!io->on_name) {
			fprintf(stderr, "unknown IP option name %s\n", s);
			return 0;
		}
		if (!strcasecmp(s, "sec-class"))
			sec = 1;
	}

	if (sec && !*(*cp + 1)) {
		fprintf(stderr, "missing security level after sec-class\n");
		return 0;
	}

	if (sec) {
		(*cp)++;
		for (s = strtok(**cp, ","); s; s = strtok(NULL, ",")) {
			for (so = secclass; so->on_name; so++)
				if (!strcasecmp(s, so->on_name)) {
					smsk |= so->on_bit;
					break;
				}
			if (!so->on_name) {
				fprintf(stderr, "no such security level: %s\n",
					s);
				return 0;
			}
		}
		if (smsk)
			*sp = smsk;
	}
	return msk;
}


#ifdef __STDC__
void optprint(u_short secmsk, u_short secbits, u_long optmsk, u_long optbits)
#else
void optprint(secmsk, secbits, optmsk, optbits)
u_short secmsk, secbits;
u_long optmsk, optbits;
#endif
{
	struct ipopt_names *io, *so;
	char *s;
	int secflag = 0;

	s = " opt ";
	for (io = ionames; io->on_name; io++)
		if ((io->on_bit & optmsk) &&
		    ((io->on_bit & optmsk) == (io->on_bit & optbits))) {
			if ((io->on_value != IPOPT_SECURITY) ||
			    (!secmsk && !secbits)) {
				printf("%s%s", s, io->on_name);
				if (io->on_value == IPOPT_SECURITY)
					io++;
				s = ",";
			} else
				secflag = 1;
		}


	if (secmsk & secbits) {
		printf("%ssec-class", s);
		s = " ";
		for (so = secclass; so->on_name; so++)
			if ((secmsk & so->on_bit) &&
			    ((so->on_bit & secmsk) == (so->on_bit & secbits))) {
				printf("%s%s", s, so->on_name);
				s = ",";
			}
	}

	if ((optmsk && (optmsk != optbits)) ||
	    (secmsk && (secmsk != secbits))) {
		s = " ";
		printf(" not opt");
		if (optmsk != optbits) {
			for (io = ionames; io->on_name; io++)
				if ((io->on_bit & optmsk) &&
				    ((io->on_bit & optmsk) !=
				     (io->on_bit & optbits))) {
					if ((io->on_value != IPOPT_SECURITY) ||
					    (!secmsk && !secbits)) {
						printf("%s%s", s, io->on_name);
						s = ",";
						if (io->on_value ==
						    IPOPT_SECURITY)
							io++;
					} else
						io++;
				}
		}

		if (secmsk != secbits) {
			printf("%ssec-class", s);
			s = " ";
			for (so = secclass; so->on_name; so++)
				if ((so->on_bit & secmsk) &&
				    ((so->on_bit & secmsk) !=
				     (so->on_bit & secbits))) {
					printf("%s%s", s, so->on_name);
					s = ",";
				}
		}
	}
}

char	*icmptypes[] = {
	"echorep", (char *)NULL, (char *)NULL, "unreach", "squench",
	"redir", (char *)NULL, (char *)NULL, "echo", "routerad",
	"routersol", "timex", "paramprob", "timest", "timestrep",
	"inforeq", "inforep", "maskreq", "maskrep", "END"
};

/*
 * set the icmp field to the correct type if "icmp" word is found
 */
int	addicmp(cp, fp)
char	***cp;
struct	frentry	*fp;
{
	char	**t;
	int	i;

	(*cp)++;
	if (!**cp)
		return -1;
	if (!fp->fr_proto)	/* to catch lusers */
		fp->fr_proto = IPPROTO_ICMP;
	if (isdigit(***cp)) {
		i = atoi(**cp);
		(*cp)++;
	} else {
		for (t = icmptypes, i = 0; ; t++, i++) {
			if (!*t)
				continue;
			if (!strcasecmp("END", *t)) {
				i = -1;
				break;
			}
			if (!strcasecmp(*t, **cp))
				break;
		}
		if (i == -1) {
			(void)fprintf(stderr,
				"Invalid icmp-type (%s) specified\n", **cp);
			return -1;
		}
	}
	fp->fr_icmp = (u_short)(i << 8);
	fp->fr_icmpm = (u_short)0xff00;
	(*cp)++;
	if (!**cp)
		return 0;

	if (**cp && strcasecmp("code", **cp))
		return 0;
	(*cp)++;
	if (isdigit(***cp)) {
		i = atoi(**cp);
		fp->fr_icmp |= (u_short)i;
		fp->fr_icmpm = (u_short)0xffff;
		(*cp)++;
		return 0;
	}
	return -1;
}


#define	MAX_ICMPCODE	12

char	*icmpcodes[] = {
	"net-unr", "host-unr", "proto-unr", "port-unr", "needfrag", "srcfail",
	"net-unk", "host-unk", "isolate", "net-prohib", "host-prohib",
	"net-tos", "host-tos", NULL };
/*
 * Return the number for the associated ICMP unreachable code.
 */
int icmpcode(str)
char *str;
{
	char	*s;
	int	i, len;

	if (!(s = strrchr(str, ')')))
		return -1;
	*s = '\0';
	if (isdigit(*str))
		return atoi(str);
	len = strlen(str);
	for (i = 0; icmpcodes[i]; i++)
		if (!strncasecmp(str, icmpcodes[i], MIN(len,
				 strlen(icmpcodes[i])) ))
			return i;
	return -1;
}


/*
 * set the icmp field to the correct type if "icmp" word is found
 */
int	addkeep(cp, fp)
char	***cp;
struct	frentry	*fp;
{
	if (fp->fr_proto != IPPROTO_TCP && fp->fr_proto != IPPROTO_UDP &&
	    fp->fr_proto != IPPROTO_ICMP && !(fp->fr_ip.fi_fl & FI_TCPUDP)) {
		(void)fprintf(stderr, "Can only use keep with UDP/ICMP/TCP\n");
		return -1;
	}

	(*cp)++;
	if (**cp && strcasecmp(**cp, "state") && strcasecmp(**cp, "frags")) {
		(void)fprintf(stderr, "Unrecognised state keyword \"%s\"\n",
			**cp);
		return -1;
	}

	if (***cp == 's' || ***cp == 'S')
		fp->fr_flags |= FR_KEEPSTATE;
	else if (***cp == 'f' || ***cp == 'F')
		fp->fr_flags |= FR_KEEPFRAG;
	(*cp)++;
	return 0;
}


/*
 * count consecutive 1's in bit mask.  If the mask generated by counting
 * consecutive 1's is different to that passed, return -1, else return #
 * of bits.
 */
int	countbits(ip)
u_long	ip;
{
	u_long	ipn;
	int	cnt = 0, i, j;

	ip = ipn = ntohl(ip);
	for (i = 32; i; i--, ipn *= 2)
		if (ipn & 0x80000000)
			cnt++;
		else
			break;
	ipn = 0;
	for (i = 32, j = cnt; i; i--, j--) {
		ipn *= 2;
		if (j > 0)
			ipn++;
	}
	if (ipn == ip)
		return cnt;
	return -1;
}


char	*portname(pr, port)
int	pr, port;
{
	static	char	buf[32];
	struct	protoent	*p = NULL;
	struct	servent	*sv = NULL, *sv1 = NULL;

	if (pr == -1) {
		if ((sv = getservbyport(port, "tcp"))) {
			strncpy(buf, sv->s_name, sizeof(buf)-1);
			buf[sizeof(buf)-1] = '\0';
			sv1 = getservbyport(port, "udp");
			sv = strncasecmp(buf, sv->s_name, strlen(buf)) ?
			     NULL : sv1;
		}
		if (sv)
			return buf;
	} else if (pr && (p = getprotobynumber(pr))) {
		if ((sv = getservbyport(port, p->p_name))) {
			strncpy(buf, sv->s_name, sizeof(buf)-1);
			buf[sizeof(buf)-1] = '\0';
			return buf;
		}
	}

	(void) sprintf(buf, "%d", port);
	return buf;
}


/*
 * print the filter structure in a useful way
 */
void	printfr(fp)
struct	frentry	*fp;
{
	static	char	*pcmp1[] = { "*", "=", "!=", "<", ">", "<=", ">=",
				    "<>", "><"};
	struct	protoent	*p;
	int	ones = 0, pr;
	char	*s;
	u_char	*t;

	if (fp->fr_flags & FR_PASS)
		(void)printf("pass");
	else if (fp->fr_flags & FR_BLOCK) {
		(void)printf("block");
		if (fp->fr_flags & FR_RETICMP) {
			(void)printf(" return-icmp");
			if (fp->fr_icode)
				if (fp->fr_icode <= MAX_ICMPCODE)
					printf("(%s)",
						icmpcodes[(int)fp->fr_icode]);
				else
					printf("(%d)", fp->fr_icode);
		}
		if (fp->fr_flags & FR_RETRST)
			(void)printf(" return-rst");
	} else if ((fp->fr_flags & FR_LOGMASK) == FR_LOG) {
		(void)printf("log");
		if (fp->fr_flags & FR_LOGBODY)
			(void)printf(" body");
		if (fp->fr_flags & FR_LOGFIRST)
			(void)printf(" first");
	} else if (fp->fr_flags & FR_ACCOUNT)
		(void)printf("count");
	else if (fp->fr_flags & FR_AUTH)
		(void)printf("auth");
	else if (fp->fr_flags & FR_PREAUTH)
		(void)printf("preauth");
	else if (fp->fr_skip)
		(void)printf("skip %d", fp->fr_skip);

	if (fp->fr_flags & FR_OUTQUE)
		(void)printf(" out ");
	else
		(void)printf(" in ");

	if (((fp->fr_flags & FR_LOGB) == FR_LOGB) ||
	    ((fp->fr_flags & FR_LOGP) == FR_LOGP)) {
		(void)printf("log ");
		if (fp->fr_flags & FR_LOGBODY)
			(void)printf("body ");
		if (fp->fr_flags & FR_LOGFIRST)
			(void)printf("first ");
		if (fp->fr_flags & FR_LOGORBLOCK)
			(void)printf("or-block ");
	}
	if (fp->fr_flags & FR_QUICK)
		(void)printf("quick ");

	if (*fp->fr_ifname) {
		(void)printf("on %s%s ", fp->fr_ifname,
			(fp->fr_ifa || (long)fp->fr_ifa == -1) ? "" : "(!)");
		if (*fp->fr_dif.fd_ifname)
			print_toif("dup-to", &fp->fr_dif);
		if (*fp->fr_tif.fd_ifname)
			print_toif("to", &fp->fr_tif);
		if (fp->fr_flags & FR_FASTROUTE)
			(void)printf("fastroute ");

	}
	if (fp->fr_mip.fi_tos)
		(void)printf("tos %#x ", fp->fr_tos);
	if (fp->fr_mip.fi_ttl)
		(void)printf("ttl %d ", fp->fr_ttl);
	if (fp->fr_ip.fi_fl & FI_TCPUDP) {
			(void)printf("proto tcp/udp ");
			pr = -1;
	} else if ((pr = fp->fr_mip.fi_p)) {
		if ((p = getprotobynumber(fp->fr_proto)))
			(void)printf("proto %s ", p->p_name);
		else
			(void)printf("proto %d ", fp->fr_proto);
	}

	printf("from %s", fp->fr_flags & FR_NOTSRCIP ? "!" : "");
	if (!fp->fr_src.s_addr & !fp->fr_smsk.s_addr)
		(void)printf("any ");
	else {
		(void)printf("%s", inet_ntoa(fp->fr_src));
		if ((ones = countbits(fp->fr_smsk.s_addr)) == -1)
			(void)printf("/%s ", inet_ntoa(fp->fr_smsk));
		else
			(void)printf("/%d ", ones);
	}
	if (fp->fr_scmp)
		if (fp->fr_scmp == FR_INRANGE || fp->fr_scmp == FR_OUTRANGE)
			(void)printf("port %d %s %d ", fp->fr_sport,
				     pcmp1[fp->fr_scmp], fp->fr_stop);
		else
			(void)printf("port %s %s ", pcmp1[fp->fr_scmp],
				     portname(pr, fp->fr_sport));

	printf("to %s", fp->fr_flags & FR_NOTDSTIP ? "!" : "");
	if (!fp->fr_dst.s_addr & !fp->fr_dmsk.s_addr)
		(void)printf("any");
	else {
		(void)printf("%s", inet_ntoa(fp->fr_dst));
		if ((ones = countbits(fp->fr_dmsk.s_addr)) == -1)
			(void)printf("/%s", inet_ntoa(fp->fr_dmsk));
		else
			(void)printf("/%d", ones);
	}
	if (fp->fr_dcmp) {
		if (fp->fr_dcmp == FR_INRANGE || fp->fr_dcmp == FR_OUTRANGE)
			(void)printf(" port %d %s %d", fp->fr_dport,
				     pcmp1[fp->fr_dcmp], fp->fr_dtop);
		else
			(void)printf(" port %s %s", pcmp1[fp->fr_dcmp],
				     portname(pr, fp->fr_dport));
	}
	if ((fp->fr_ip.fi_fl & ~FI_TCPUDP) ||
	    (fp->fr_mip.fi_fl & ~FI_TCPUDP) ||
	    fp->fr_ip.fi_optmsk || fp->fr_mip.fi_optmsk ||
	    fp->fr_ip.fi_secmsk || fp->fr_mip.fi_secmsk) {
		(void)printf(" with");
		if (fp->fr_ip.fi_optmsk || fp->fr_mip.fi_optmsk ||
		    fp->fr_ip.fi_secmsk || fp->fr_mip.fi_secmsk)
			optprint(fp->fr_mip.fi_secmsk,
				 fp->fr_ip.fi_secmsk,
				 fp->fr_mip.fi_optmsk,
				 fp->fr_ip.fi_optmsk);
		else if (fp->fr_mip.fi_fl & FI_OPTIONS) {
			if (!(fp->fr_ip.fi_fl & FI_OPTIONS))
				(void)printf(" not");
			(void)printf(" ipopt");
		}
		if (fp->fr_mip.fi_fl & FI_SHORT) {
			if (!(fp->fr_ip.fi_fl & FI_SHORT))
				(void)printf(" not");
			(void)printf(" short");
		}
		if (fp->fr_mip.fi_fl & FI_FRAG) {
			if (!(fp->fr_ip.fi_fl & FI_FRAG))
				(void)printf(" not");
			(void)printf(" frag");
		}
	}
	if (fp->fr_proto == IPPROTO_ICMP && fp->fr_icmpm) {
		int	type = fp->fr_icmp, code;

		type = ntohs(fp->fr_icmp);
		code = type & 0xff;
		type /= 256;
		if (type < (sizeof(icmptypes) / sizeof(char *)) &&
		    icmptypes[type])
			(void)printf(" icmp-type %s", icmptypes[type]);
		else
			(void)printf(" icmp-type %d", type);
		if (code)
			(void)printf(" code %d", code);
	}
	if (fp->fr_proto == IPPROTO_TCP && (fp->fr_tcpf || fp->fr_tcpfm)) {
		(void)printf(" flags ");
		for (s = flagset, t = flags; *s; s++, t++)
			if (fp->fr_tcpf & *t)
				(void)putchar(*s);
		if (fp->fr_tcpfm) {
			(void)putchar('/');
			for (s = flagset, t = flags; *s; s++, t++)
				if (fp->fr_tcpfm & *t)
					(void)putchar(*s);
		}
	}

	if (fp->fr_flags & FR_KEEPSTATE)
		printf(" keep state");
	if (fp->fr_flags & FR_KEEPFRAG)
		printf(" keep frags");
	if (fp->fr_grhead)
		printf(" head %d", fp->fr_grhead);
	if (fp->fr_group)
		printf(" group %d", fp->fr_group);
	(void)putchar('\n');
}

void	binprint(fp)
struct frentry *fp;
{
	int i = sizeof(*fp), j = 0;
	u_char *s;

	for (s = (u_char *)fp; i; i--, s++) {
		j++;
		(void)printf("%02x ",*s);
		if (j == 16) {
			(void)printf("\n");
			j = 0;
		}
	}
	putchar('\n');
	(void)fflush(stdout);
}
