/*
 * Copyright (C) 1993-1998 by Darren Reed.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 */
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#if !defined(__SVR4) && !defined(__svr4__)
#include <strings.h>
#else
#include <sys/byteorder.h>
#endif
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
#if __FreeBSD_version >= 300000
# include <net/if_var.h>
#endif
#include <netdb.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>
#include <resolv.h>
#include <ctype.h>
#include "netinet/ip_compat.h"
#include "netinet/ip_fil.h"
#include "netinet/ip_proxy.h"
#include "netinet/ip_nat.h"

#if	defined(sun) && !SOLARIS2
# define	STRERROR(x)	sys_errlist[x]
extern	char	*sys_errlist[];
#else
# define	STRERROR(x)	strerror(x)
#endif

#if !defined(lint)
static const char sccsid[] ="@(#)ipnat.c	1.9 6/5/96 (C) 1993 Darren Reed";
static const char rcsid[] = "@(#)$Id: natparse.c,v 1.2 1999/08/01 11:17:18 darrenr Exp $";
#endif


#if	SOLARIS
#define	bzero(a,b)	memset(a,0,b)
#endif

extern	int	countbits __P((u_32_t));
extern	u_32_t	hostnum __P((char *, int *, int));

ipnat_t	*natparse __P((char *, int));
void	printnat __P((ipnat_t *, int, void *));
void	natparsefile __P((int, char *, int));
u_32_t	n_hostmask __P((char *));
u_short	n_portnum __P((char *, char *, int));
void	nat_setgroupmap __P((struct ipnat *));

#define	OPT_REM		1
#define	OPT_NODO	2
#define	OPT_STAT	4
#define	OPT_LIST	8
#define	OPT_VERBOSE	16
#define	OPT_FLUSH	32
#define	OPT_CLEAR	64


void printnat(np, verbose, ptr)
ipnat_t *np;
int verbose;
void *ptr;
{
	struct	protoent	*pr;
	struct	servent	*sv;
	int	bits;

	switch (np->in_redir)
	{
	case NAT_REDIRECT :
		printf("rdr ");
		break;
	case NAT_MAP :
		printf("map ");
		break;
	case NAT_MAPBLK :
		printf("map-block ");
		break;
	case NAT_BIMAP :
		printf("bimap ");
		break;
	default :
		fprintf(stderr, "unknown value for in_redir: %#x\n",
			np->in_redir);
		break;
	}

	if (np->in_redir == NAT_REDIRECT) {
		printf("%s ", np->in_ifname);
		if (np->in_src[0].s_addr || np->in_src[1].s_addr) {
			printf("from %s",inet_ntoa(np->in_src[0]));
			bits = countbits(np->in_src[1].s_addr);
			if (bits != -1)
				printf("/%d ", bits);
			else
				printf("/%s ", inet_ntoa(np->in_src[1]));
		}
		printf("%s",inet_ntoa(np->in_out[0]));
		bits = countbits(np->in_out[1].s_addr);
		if (bits != -1)
			printf("/%d ", bits);
		else
			printf("/%s ", inet_ntoa(np->in_out[1]));
		if (np->in_pmin)
			printf("port %d ", ntohs(np->in_pmin));
		printf("-> %s", inet_ntoa(np->in_in[0]));
		if (np->in_pnext)
			printf(" port %d", ntohs(np->in_pnext));
		if ((np->in_flags & IPN_TCPUDP) == IPN_TCPUDP)
			printf(" tcp/udp");
		else if ((np->in_flags & IPN_TCP) == IPN_TCP)
			printf(" tcp");
		else if ((np->in_flags & IPN_UDP) == IPN_UDP)
			printf(" udp");
		printf("\n");
		if (verbose)
			printf("\t%p %lu %x %u %p %d\n", np->in_ifp,
			       np->in_space, np->in_flags, np->in_pnext, np,
			       np->in_use);
	} else {
		np->in_nextip.s_addr = htonl(np->in_nextip.s_addr);
		printf("%s %s/", np->in_ifname, inet_ntoa(np->in_in[0]));
		bits = countbits(np->in_in[1].s_addr);
		if (bits != -1)
			printf("%d ", bits);
		else
			printf("%s", inet_ntoa(np->in_in[1]));
		printf(" -> ");
		if (np->in_flags & IPN_RANGE) {
			printf("range %s-", inet_ntoa(np->in_out[0]));
			printf("%s", inet_ntoa(np->in_out[1]));
		} else {
			printf("%s/", inet_ntoa(np->in_out[0]));
			bits = countbits(np->in_out[1].s_addr);
			if (bits != -1)
				printf("%d ", bits);
			else
				printf("%s", inet_ntoa(np->in_out[1]));
		}
		if (*np->in_plabel) {
			pr = getprotobynumber(np->in_p);
			printf(" proxy port");
			if (np->in_dport != 0) {
				if (pr != NULL)
					sv = getservbyport(np->in_dport,
							   pr->p_name);
				else
					sv = getservbyport(np->in_dport, NULL);
				if (sv != NULL)
					printf(" %s", sv->s_name);
				else
					printf(" %hu", ntohs(np->in_dport));
			}
			printf(" %.*s/", (int)sizeof(np->in_plabel),
				np->in_plabel);
			if (pr != NULL)
				fputs(pr->p_name, stdout);
			else
				printf("%d", np->in_p);
		} else if (np->in_redir == NAT_MAPBLK) {
			printf(" ports %d", np->in_pmin);
			if (verbose)
				printf("\n\tip modulous %d", np->in_pmax);
		} else if (np->in_pmin || np->in_pmax) {
			printf(" portmap");
			if (np->in_flags & IPN_AUTOPORTMAP) {
				printf(" auto");
				if (verbose)
					printf(" [%d:%d %d %d]",
					       ntohs(np->in_pmin),
					       ntohs(np->in_pmax),
					       np->in_ippip, np->in_ppip);
			} else {
				if ((np->in_flags & IPN_TCPUDP) == IPN_TCPUDP)
					printf(" tcp/udp");
				else if (np->in_flags & IPN_TCP)
					printf(" tcp");
				else if (np->in_flags & IPN_UDP)
					printf(" udp");
				printf(" %d:%d", ntohs(np->in_pmin),
				       ntohs(np->in_pmax));
			}
		}
		printf("\n");
		if (verbose) {
			printf("\tifp %p space %lu nextip %s pnext %d",
			       np->in_ifp, np->in_space,
			       inet_ntoa(np->in_nextip), np->in_pnext);
			printf(" flags %x use %u\n",
			       np->in_flags, np->in_use);
		}
	}
}


void nat_setgroupmap(n)
ipnat_t *n;
{
	if (n->in_outmsk == n->in_inmsk)
		n->in_ippip = 1;
	else if (n->in_flags & IPN_AUTOPORTMAP) {
		n->in_ippip = ~ntohl(n->in_inmsk);
		if (n->in_outmsk != 0xffffffff)
			n->in_ippip /= (~ntohl(n->in_outmsk) + 1);
		n->in_ippip++;
		if (n->in_ippip == 0)
			n->in_ippip = 1;
		n->in_ppip = USABLE_PORTS / n->in_ippip;
	} else {
		n->in_space = USABLE_PORTS * ~ntohl(n->in_outmsk);
		n->in_nip = 0;
		if (!(n->in_ppip = n->in_pmin))
			n->in_ppip = 1;
		n->in_ippip = USABLE_PORTS / n->in_ppip;
	}
}



ipnat_t *natparse(line, linenum)
char *line;
int linenum;
{
	struct protoent *pr;
	static ipnat_t ipn;
	char *s, *t;
	char *shost, *snetm, *dhost, *proto, *srchost, *srcnetm;
	char *dnetm = NULL, *dport = NULL, *tport = NULL;
	int resolved;

	srchost = NULL;
	srcnetm = NULL;

	bzero((char *)&ipn, sizeof(ipn));
	if ((s = strchr(line, '\n')))
		*s = '\0';
	if ((s = strchr(line, '#')))
		*s = '\0';
	if (!*line)
		return NULL;
	if (!(s = strtok(line, " \t")))
		return NULL;
	if (!strcasecmp(s, "map"))
		ipn.in_redir = NAT_MAP;
	else if (!strcasecmp(s, "map-block"))
		ipn.in_redir = NAT_MAPBLK;
	else if (!strcasecmp(s, "rdr"))
		ipn.in_redir = NAT_REDIRECT;
	else if (!strcasecmp(s, "bimap"))
		ipn.in_redir = NAT_BIMAP;
	else {
		fprintf(stderr, "%d: unknown mapping: \"%s\"\n",
			linenum, s);
		return NULL;
	}

	if (!(s = strtok(NULL, " \t"))) {
		fprintf(stderr, "%d: missing fields (interface)\n",
			linenum);
		return NULL;
	}

	strncpy(ipn.in_ifname, s, sizeof(ipn.in_ifname) - 1);
	ipn.in_ifname[sizeof(ipn.in_ifname) - 1] = '\0';
	if (!(s = strtok(NULL, " \t"))) {
		fprintf(stderr, "%d: missing fields (%s)\n", linenum, 
			ipn.in_redir ? "from source | destination" : "source");
		return NULL;
	}

	if ((ipn.in_redir == NAT_REDIRECT) && !strcasecmp(s, "from")) {
		if (!(s = strtok(NULL, " \t"))) {
			fprintf(stderr,
				"%d: missing fields (source address)\n",
				linenum);
			return NULL;
		}

		srchost = s;
		srcnetm = strrchr(srchost, '/');

		if (srcnetm == NULL) {
			if (!(s = strtok(NULL, " \t"))) {
				fprintf(stderr,
				"%d: missing fields (source netmask)\n",
					linenum);
				return NULL;
			}

			if (strcasecmp(s, "netmask")) {
				fprintf(stderr,
					"%d: missing fields (netmask)\n",
					linenum);
				return NULL;
			}
			if (!(s = strtok(NULL, " \t"))) {
				fprintf(stderr,
					"%d: missing fields (source netmask)\n",
					linenum);
				return NULL;
			}
			srcnetm = s;
		}
		if (*srcnetm == '/')
			*srcnetm++ = '\0';

		/* re read the  next word  -- destination */
		if (!(s = strtok(NULL, " \t"))) {
			fprintf(stderr,
				"%d: missing fields (destination)\n", linenum);
			return NULL;
		}

	}

	shost = s;

	if (ipn.in_redir == NAT_REDIRECT) {
		if (!(s = strtok(NULL, " \t"))) {
			fprintf(stderr,
				"%d: missing fields (destination port)\n",
				linenum);
			return NULL;
		}

		if (strcasecmp(s, "port")) {
			fprintf(stderr, "%d: missing fields (port)\n", linenum);
			return NULL;
		}

		if (!(s = strtok(NULL, " \t"))) {
			fprintf(stderr,
				"%d: missing fields (destination port)\n",
				linenum);
			return NULL;
		}

		dport = s;
	}


	if (!(s = strtok(NULL, " \t"))) {
		fprintf(stderr, "%d: missing fields (->)\n", linenum);
		return NULL;
	}
	if (!strcmp(s, "->")) {
		snetm = strrchr(shost, '/');
		if (!snetm) {
			fprintf(stderr,
				"%d: missing fields (%s netmask)\n", linenum,
				ipn.in_redir ? "destination" : "source");
			return NULL;
		}
	} else {
		if (strcasecmp(s, "netmask")) {
			fprintf(stderr, "%d: missing fields (netmask)\n",
				linenum);
			return NULL;
		}
		if (!(s = strtok(NULL, " \t"))) {
			fprintf(stderr,
				"%d: missing fields (%s netmask)\n", linenum,
				ipn.in_redir ? "destination" : "source");
			return NULL;
		}
		snetm = s;
	}

	if (!(s = strtok(NULL, " \t"))) {
		fprintf(stderr, "%d: missing fields (%s)\n",
			linenum, ipn.in_redir ? "destination":"target");
		return NULL;
	}

	if (ipn.in_redir == NAT_MAP) {
		if (!strcasecmp(s, "range")) {
			ipn.in_flags |= IPN_RANGE;
			if (!(s = strtok(NULL, " \t"))) {
				fprintf(stderr, "%d: missing fields (%s)\n",
					linenum,
					ipn.in_redir ? "destination":"target");
				return NULL;
			}
		}
	}
	dhost = s;

	if (ipn.in_redir & (NAT_MAP|NAT_MAPBLK)) {
		if (ipn.in_flags & IPN_RANGE) {
			dnetm = strrchr(dhost, '-');
			if (dnetm == NULL) {
				if (!(s = strtok(NULL, " \t")))
					dnetm = NULL;
				else {
					if (strcmp(s, "-"))
						s = NULL;
					else if ((s = strtok(NULL, " \t"))) {
						dnetm = s;
					}
				}
			} else
				*dnetm++ = '\0';
			if (dnetm == NULL || *dnetm == '\0') {
				fprintf(stderr,
					"%d: desination range not specified\n",
					linenum);
				return NULL;
			}
		} else {
			dnetm = strrchr(dhost, '/');
			if (dnetm == NULL) {
				if (!(s = strtok(NULL, " \t")))
					dnetm = NULL;
				else if (!strcasecmp(s, "netmask"))
					if ((s = strtok(NULL, " \t")) != NULL)
						dnetm = s;
			}
			if (dnetm == NULL) {
				fprintf(stderr,
					"%d: missing fields (dest netmask)\n",
					linenum);
				return NULL;
			}
			if (*dnetm == '/')
				*dnetm++ = '\0';
		}
		s = strtok(NULL, " \t");
	}

	if (ipn.in_redir & NAT_MAPBLK) {
		if (s && strcasecmp(s, "ports")) {
			fprintf(stderr,
				"%d: expected \"ports\" - got \"%s\"\n",
				linenum, s);
			return NULL;
		}
		if (s != NULL) {
			if ((s = strtok(NULL, " \t")) == NULL)
				return NULL;
			ipn.in_pmin = atoi(s);
			s = strtok(NULL, " \t");
		} else
			ipn.in_pmin = 0;
	} else if ((ipn.in_redir & NAT_BIMAP) == NAT_REDIRECT) {
		if (strrchr(dhost, '/') != NULL) {
			fprintf(stderr, "%d: No netmask supported in %s\n",
				linenum, "destination host for redirect");
			return NULL;
		}
		/* If it's a in_redir, expect target port */
		if (!(s = strtok(NULL, " \t"))) {
			fprintf(stderr,
				"%d: missing fields (destination port)\n",
				linenum);
			return NULL;
		}

		if (strcasecmp(s, "port")) {
			fprintf(stderr, "%d: missing fields (port)\n",
				linenum);
			return NULL;
		}
	  
		if (!(s = strtok(NULL, " \t"))) {
			fprintf(stderr,
				"%d: missing fields (destination port)\n",
				linenum);
			return NULL;
		}
		tport = s;
	} 
	if (dnetm && *dnetm == '/')
		*dnetm++ = '\0';
	if (snetm && *snetm == '/')
		*snetm++ = '\0';

	if (ipn.in_redir & (NAT_MAP|NAT_MAPBLK)) {
		ipn.in_inip = hostnum(shost, &resolved, linenum);
		if (resolved == -1)
			return NULL;
		ipn.in_inmsk = n_hostmask(snetm);
		ipn.in_outip = hostnum(dhost, &resolved, linenum);
		if (resolved == -1)
			return NULL;
		if (ipn.in_flags & IPN_RANGE) {
			ipn.in_outmsk = hostnum(dnetm, &resolved, linenum);
			if (resolved == -1)
				return NULL;
		} else
			ipn.in_outmsk = n_hostmask(dnetm);
		if (srchost) {
			ipn.in_srcip = hostnum(srchost, &resolved, linenum);
			if (resolved == -1)
				return NULL;
		}
		if (srcnetm)
			ipn.in_srcmsk = n_hostmask(srcnetm);
	} else {
		if (srchost) {
			ipn.in_srcip = hostnum(srchost, &resolved, linenum);
			if (resolved == -1)
				return NULL;
		}
		if (srcnetm)
			ipn.in_srcmsk = n_hostmask(srcnetm);
		ipn.in_inip = hostnum(dhost, &resolved, linenum);
		if (resolved == -1)
			return NULL;
		ipn.in_inmsk = n_hostmask("255.255.255.255");
		ipn.in_outip = hostnum(shost, &resolved, linenum);
		if (resolved == -1)
			return NULL;
		ipn.in_outmsk = n_hostmask(snetm);
		if (!(s = strtok(NULL, " \t"))) {
			ipn.in_flags = IPN_TCP; /* XXX- TCP only by default */
			proto = "tcp";
		} else {
			if (!strcasecmp(s, "tcp"))
				ipn.in_flags = IPN_TCP;
			else if (!strcasecmp(s, "udp"))
				ipn.in_flags = IPN_UDP;
			else if (!strcasecmp(s, "tcp/udp"))
				ipn.in_flags = IPN_TCPUDP;
			else if (!strcasecmp(s, "tcpudp"))
				ipn.in_flags = IPN_TCPUDP;
			else if (!strcasecmp(s, "ip")) 
				ipn.in_flags = IPN_ANY;
			else {
				fprintf(stderr,
					"%d: expected protocol - got \"%s\"\n",
					linenum, s);
				return NULL;
			}
			proto = s;
			if ((s = strtok(NULL, " \t"))) {
				fprintf(stderr,
				"%d: extra junk at the end of rdr: %s\n",
					linenum, s);
				return NULL;
			}
		}
		ipn.in_pmin = n_portnum(dport, proto, linenum);
		ipn.in_pmax = ipn.in_pmin;
		ipn.in_pnext = n_portnum(tport, proto, linenum);
		s = NULL;
	}
	ipn.in_inip &= ipn.in_inmsk;
	if ((ipn.in_flags & IPN_RANGE) == 0)
		ipn.in_outip &= ipn.in_outmsk;
	ipn.in_srcip &= ipn.in_srcmsk;

	if ((ipn.in_redir & NAT_MAPBLK) != 0)
		nat_setgroupmap(&ipn);

	if (!s)
		return &ipn;

	if (ipn.in_redir == NAT_BIMAP) {
		fprintf(stderr,
			"%d: extra words at the end of bimap line: %s\n",
			linenum, s);
		return NULL;
	}
	if (!strcasecmp(s, "proxy")) {
		if (!(s = strtok(NULL, " \t"))) {
			fprintf(stderr,
				"%d: missing parameter for \"proxy\"\n",
				linenum);
			return NULL;
		}
		dport = NULL;

		if (!strcasecmp(s, "port")) {
			if (!(s = strtok(NULL, " \t"))) {
				fprintf(stderr,
					"%d: missing parameter for \"port\"\n",
					linenum);
				return NULL;
			}

			dport = s;

			if (!(s = strtok(NULL, " \t"))) {
				fprintf(stderr,
					"%d: missing parameter for \"proxy\"\n",
					linenum);
				return NULL;
			}
		} else {
			fprintf(stderr,
				"%d: missing keyword \"port\"\n", linenum);
			return NULL;
		}
		if ((proto = index(s, '/'))) {
			*proto++ = '\0';
			if ((pr = getprotobyname(proto)))
				ipn.in_p = pr->p_proto;
			else
				ipn.in_p = atoi(proto);
			if (dport)
				ipn.in_dport = n_portnum(dport, proto, linenum);
		} else {
			ipn.in_p = 0;
			if (dport)
				ipn.in_dport = n_portnum(dport, NULL, linenum);
		}

		(void) strncpy(ipn.in_plabel, s, sizeof(ipn.in_plabel));
		if ((s = strtok(NULL, " \t"))) {
			fprintf(stderr,
				"%d: too many parameters for \"proxy\"\n",
				linenum);
			return NULL;
		}
		return &ipn;
		
	}

	if (strcasecmp(s, "portmap")) {
		fprintf(stderr,
			"%d: expected \"portmap\" - got \"%s\"\n", linenum, s);
		return NULL;
	}
	if (!(s = strtok(NULL, " \t")))
		return NULL;
	if (!strcasecmp(s, "tcp"))
		ipn.in_flags = IPN_TCP;
	else if (!strcasecmp(s, "udp"))
		ipn.in_flags = IPN_UDP;
	else if (!strcasecmp(s, "tcpudp"))
		ipn.in_flags = IPN_TCPUDP;
	else if (!strcasecmp(s, "tcp/udp"))
		ipn.in_flags = IPN_TCPUDP;
	else {
		fprintf(stderr,
			"%d: expected protocol name - got \"%s\"\n",
			linenum, s);
		return NULL;
	}

	if (!(s = strtok(NULL, " \t"))) {
		fprintf(stderr, "%d: no port range found\n", linenum);
		return NULL;
	}

	if (!strcasecmp(s, "auto")) {
		ipn.in_flags |= IPN_AUTOPORTMAP;
		ipn.in_pmin = htons(1024);
		ipn.in_pmax = htons(65535);
		nat_setgroupmap(&ipn);
		return &ipn;
	}
	proto = s;
	if (!(t = strchr(s, ':'))) {
		fprintf(stderr, "%d: no port range in \"%s\"\n", linenum, s);
		return NULL;
	}
	*t++ = '\0';
	ipn.in_pmin = n_portnum(s, proto, linenum);
	ipn.in_pmax = n_portnum(t, proto, linenum);
	return &ipn;
}


void natparsefile(fd, file, opts)
int fd;
char *file;
int opts;
{
	char	line[512], *s;
	ipnat_t	*np;
	FILE	*fp;
	int	linenum = 0;

	if (strcmp(file, "-")) {
		if (!(fp = fopen(file, "r"))) {
			fprintf(stderr, "%s: open: %s\n", file,
				STRERROR(errno));
			exit(1);
		}
	} else
		fp = stdin;

	while (fgets(line, sizeof(line) - 1, fp)) {
	        linenum++;
		line[sizeof(line) - 1] = '\0';
		if ((s = strchr(line, '\n')))
			*s = '\0';

		if (!(np = natparse(line, linenum))) {
			if (*line)
				fprintf(stderr, "%d: syntax error in \"%s\"\n",
					linenum, line);
		} else {
			if ((opts & OPT_VERBOSE) && np)
				printnat(np, opts &  OPT_VERBOSE, NULL);
			if (!(opts & OPT_NODO)) {
				if (!(opts & OPT_REM)) {
					if (ioctl(fd, SIOCADNAT, np) == -1)
						perror("ioctl(SIOCADNAT)");
				} else if (ioctl(fd, SIOCRMNAT, np) == -1)
					perror("ioctl(SIOCRMNAT)");
			}
		}
	}
	if (fp != stdin)
		fclose(fp);
}


u_32_t	n_hostmask(msk)
char	*msk;
{
	int	bits = -1;
	u_32_t	mask;

	if (!isdigit(*msk))
		return (u_32_t)-1;
	if (strchr(msk, '.'))
		return inet_addr(msk);
	if (strchr(msk, 'x'))
		return (u_32_t)strtol(msk, NULL, 0);
	/*
	 * set x most significant bits
	 */
	for (mask = 0, bits = atoi(msk); bits; bits--) {
		mask /= 2;
		mask |= ntohl(inet_addr("128.0.0.0"));
	}
	mask = htonl(mask);
	return mask;
}


u_short	n_portnum(name, proto, linenum)
char	*name, *proto;
int     linenum;
{
	struct	servent *sp, *sp2;
	u_short	p1 = 0;

	if (isdigit(*name))
		return htons((u_short)atoi(name));
	if (!proto)
		proto = "tcp/udp";
	if (strcasecmp(proto, "tcp/udp")) {
		sp = getservbyname(name, proto);
		if (sp)
			return sp->s_port;
		fprintf(stderr, "%d: unknown service \"%s\".\n", linenum, name);
		return 0;
	}
	sp = getservbyname(name, "tcp");
	if (sp)
		p1 = sp->s_port;
	sp2 = getservbyname(name, "udp");
	if (!sp || !sp2) {
		fprintf(stderr, "%d: unknown tcp/udp service \"%s\".\n",
			linenum, name);
		return 0;
	}
	if (p1 != sp2->s_port) {
		fprintf(stderr, "%d: %s %d/tcp is a different port to ",
			linenum, name, p1);
		fprintf(stderr, "%d: %s %d/udp\n", linenum, name, sp->s_port);
		return 0;
	}
	return p1;
}
