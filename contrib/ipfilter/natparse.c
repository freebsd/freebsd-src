/*
 * Copyright (C) 1993-2001 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
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
#include "ipf.h"

#if	defined(sun) && !SOLARIS2
# define	STRERROR(x)	sys_errlist[x]
extern	char	*sys_errlist[];
#else
# define	STRERROR(x)	strerror(x)
#endif

#if !defined(lint)
static const char sccsid[] ="@(#)ipnat.c	1.9 6/5/96 (C) 1993 Darren Reed";
static const char rcsid[] = "@(#)$Id: natparse.c,v 1.17.2.11 2001/07/17 14:33:09 darrenr Exp $";
#endif


#if	SOLARIS
#define	bzero(a,b)	memset(a,0,b)
#endif

extern	int	countbits __P((u_32_t));
extern	char	*proto;

ipnat_t	*natparse __P((char *, int));
void	printnat __P((ipnat_t *, int, void *));
void	natparsefile __P((int, char *, int));
void	nat_setgroupmap __P((struct ipnat *));


void printnat(np, opts, ptr)
ipnat_t *np;
int opts;
void *ptr;
{
	struct	protoent	*pr;
	struct	servent	*sv;
	int	bits;

	switch (np->in_redir)
	{
	case NAT_REDIRECT :
		printf("rdr");
		break;
	case NAT_MAP :
		printf("map");
		break;
	case NAT_MAPBLK :
		printf("map-block");
		break;
	case NAT_BIMAP :
		printf("bimap");
		break;
	default :
		fprintf(stderr, "unknown value for in_redir: %#x\n",
			np->in_redir);
		break;
	}

	printf(" %s ", np->in_ifname);

	if (np->in_flags & IPN_FILTER) {
		if (np->in_flags & IPN_NOTSRC)
			printf("! ");
		printf("from ");
		if (np->in_redir == NAT_REDIRECT) {
			printhostmask(4, (u_32_t *)&np->in_srcip,
				      (u_32_t *)&np->in_srcmsk);
			if (np->in_scmp)
				printportcmp(np->in_p, &np->in_tuc.ftu_src);
		} else {
			printhostmask(4, (u_32_t *)&np->in_inip,
				      (u_32_t *)&np->in_inmsk);
			if (np->in_dcmp)
				printportcmp(np->in_p, &np->in_tuc.ftu_dst);
		}

		if (np->in_flags & IPN_NOTDST)
			printf(" !");
		printf(" to ");
		if (np->in_redir == NAT_REDIRECT) {
			printhostmask(4, (u_32_t *)&np->in_outip,
				      (u_32_t *)&np->in_outmsk);
			if (np->in_dcmp)
				printportcmp(np->in_p, &np->in_tuc.ftu_dst);
		} else {
			printhostmask(4, (u_32_t *)&np->in_srcip,
				      (u_32_t *)&np->in_srcmsk);
			if (np->in_scmp)
				printportcmp(np->in_p, &np->in_tuc.ftu_src);
		}
	}

	if (np->in_redir == NAT_REDIRECT) {
		if (!(np->in_flags & IPN_FILTER)) {
			printf("%s", inet_ntoa(np->in_out[0]));
			bits = countbits(np->in_out[1].s_addr);
			if (bits != -1)
				printf("/%d ", bits);
			else
				printf("/%s ", inet_ntoa(np->in_out[1]));
			if (np->in_pmin)
				printf("port %d", ntohs(np->in_pmin));
			if (np->in_pmax != np->in_pmin)
				printf("- %d", ntohs(np->in_pmax));
		}
		printf(" -> %s", inet_ntoa(np->in_in[0]));
		if (np->in_flags & IPN_SPLIT)
			printf(",%s", inet_ntoa(np->in_in[1]));
		if (np->in_pnext)
			printf(" port %d", ntohs(np->in_pnext));
		if ((np->in_flags & IPN_TCPUDP) == IPN_TCPUDP)
			printf(" tcp/udp");
		else if ((np->in_flags & IPN_TCP) == IPN_TCP)
			printf(" tcp");
		else if ((np->in_flags & IPN_UDP) == IPN_UDP)
			printf(" udp");
		if (np->in_flags & IPN_ROUNDR)
			printf(" round-robin");
		if (np->in_flags & IPN_FRAG)
			printf(" frag");
		printf("\n");
		if (opts & OPT_DEBUG)
			printf("\t%p %lu %#x %u %p %d\n", np->in_ifp,
			       np->in_space, np->in_flags, np->in_pmax, np,
			       np->in_use);
	} else {
		np->in_nextip.s_addr = htonl(np->in_nextip.s_addr);
		if (!(np->in_flags & IPN_FILTER)) {
			printf("%s/", inet_ntoa(np->in_in[0]));
			bits = countbits(np->in_in[1].s_addr);
			if (bits != -1)
				printf("%d ", bits);
			else
				printf("%s", inet_ntoa(np->in_in[1]));
		}
		printf(" -> ");
		if (np->in_flags & IPN_IPRANGE) {
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
			if (opts & OPT_VERBOSE)
				printf("\n\tip modulous %d", np->in_pmax);
		} else if (np->in_pmin || np->in_pmax) {
			printf(" portmap");
			if (np->in_flags & IPN_AUTOPORTMAP) {
				printf(" auto");
				if (opts & OPT_DEBUG)
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
		if (np->in_flags & IPN_FRAG)
			printf(" frag");
		printf("\n");
		if (opts & OPT_DEBUG) {
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
	static ipnat_t ipn;
	struct protoent *pr;
	char *dnetm = NULL, *dport = NULL;
	char *s, *t, *cps[31], **cpp;
	int i, cnt;

	proto = NULL;

	if ((s = strchr(line, '\n')))
		*s = '\0';
	if ((s = strchr(line, '#')))
		*s = '\0';
	while (*line && isspace(*line))
		line++;
	if (!*line)
		return NULL;

	bzero((char *)&ipn, sizeof(ipn));
	cnt = 0;

	for (i = 0, *cps = strtok(line, " \b\t\r\n"); cps[i] && i < 30; cnt++)
		cps[++i] = strtok(NULL, " \b\t\r\n");

	cps[i] = NULL;

	if (cnt < 3) {
		fprintf(stderr, "%d: not enough segments in line\n", linenum);
		return NULL;
	}

	cpp = cps;

	if (!strcasecmp(*cpp, "map"))
		ipn.in_redir = NAT_MAP;
	else if (!strcasecmp(*cpp, "map-block"))
		ipn.in_redir = NAT_MAPBLK;
	else if (!strcasecmp(*cpp, "rdr"))
		ipn.in_redir = NAT_REDIRECT;
	else if (!strcasecmp(*cpp, "bimap"))
		ipn.in_redir = NAT_BIMAP;
	else {
		fprintf(stderr, "%d: unknown mapping: \"%s\"\n",
			linenum, *cpp);
		return NULL;
	}

	cpp++;

	strncpy(ipn.in_ifname, *cpp, sizeof(ipn.in_ifname) - 1);
	ipn.in_ifname[sizeof(ipn.in_ifname) - 1] = '\0';
	cpp++;

	if (!strcasecmp(*cpp, "from") || (**cpp == '!')) {
		if (!strcmp(*cpp, "!")) {
			cpp++;
			if (strcasecmp(*cpp, "from")) {
				fprintf(stderr, "Missing from after !\n");
				return NULL;
			}
			ipn.in_flags |= IPN_NOTSRC;
		} else if (**cpp == '!') {
			if (strcasecmp(*cpp + 1, "from")) {
				fprintf(stderr, "Missing from after !\n");
				return NULL;
			}
			ipn.in_flags |= IPN_NOTSRC;
		}
		if ((ipn.in_flags & IPN_NOTSRC) &&
		    (ipn.in_redir & (NAT_MAP|NAT_MAPBLK))) {
			fprintf(stderr, "Cannot use '! from' with map\n");
			return NULL;
		}

		ipn.in_flags |= IPN_FILTER;
		cpp++;
		if (ipn.in_redir == NAT_REDIRECT) {
				if (hostmask(&cpp, (u_32_t *)&ipn.in_srcip,
					     (u_32_t *)&ipn.in_srcmsk,
					     &ipn.in_sport, &ipn.in_scmp,
					     &ipn.in_stop, linenum)) {
					return NULL;
				}
		} else {
				if (hostmask(&cpp, (u_32_t *)&ipn.in_inip,
					     (u_32_t *)&ipn.in_inmsk,
					     &ipn.in_sport, &ipn.in_scmp,
					     &ipn.in_stop, linenum)) {
					return NULL;
				}
		}

		if (!strcmp(*cpp, "!")) {
			cpp++;
			ipn.in_flags |= IPN_NOTDST;
		} else if (**cpp == '!') {
			(*cpp)++;
			ipn.in_flags |= IPN_NOTDST;
		}

		if (strcasecmp(*cpp, "to")) {
			fprintf(stderr, "%d: unexpected keyword (%s) - to\n",
				linenum, *cpp);
			return NULL;
		}
		if ((ipn.in_flags & IPN_NOTDST) &&
		    (ipn.in_redir & (NAT_REDIRECT))) {
			fprintf(stderr, "Cannot use '! to' with rdr\n");
			return NULL;
		}

		if (!*++cpp) {
			fprintf(stderr, "%d: missing host after to\n", linenum);
			return NULL;
		}
		if (ipn.in_redir == NAT_REDIRECT) {
				if (hostmask(&cpp, (u_32_t *)&ipn.in_outip,
					     (u_32_t *)&ipn.in_outmsk,
					     &ipn.in_dport, &ipn.in_dcmp,
					     &ipn.in_dtop, linenum)) {
					return NULL;
				}
				ipn.in_pmin = htons(ipn.in_dport);
		} else {
				if (hostmask(&cpp, (u_32_t *)&ipn.in_srcip,
					     (u_32_t *)&ipn.in_srcmsk,
					     &ipn.in_dport, &ipn.in_dcmp,
					     &ipn.in_dtop, linenum)) {
					return NULL;
				}
		}
	} else {
		s = *cpp;
		if (!s)
			return NULL;
		t = strchr(s, '/');
		if (!t)
			return NULL;
		*t++ = '\0';
		if (ipn.in_redir == NAT_REDIRECT) {
			if (hostnum((u_32_t *)&ipn.in_outip, s, linenum) == -1)
				return NULL;
			if (genmask(t, (u_32_t *)&ipn.in_outmsk) == -1) {
				return NULL;
			}
		} else {
			if (hostnum((u_32_t *)&ipn.in_inip, s, linenum) == -1)
				return NULL;
			if (genmask(t, (u_32_t *)&ipn.in_inmsk) == -1) {
				return NULL;
			}
		}
		cpp++;
		if (!*cpp)
			return NULL;
	}

	if ((ipn.in_redir == NAT_REDIRECT) && !(ipn.in_flags & IPN_FILTER)) {
		if (strcasecmp(*cpp, "port")) {
			fprintf(stderr, "%d: missing fields - 1st port\n",
				linenum);
			return NULL;
		}

		cpp++;

		if (!*cpp) {
			fprintf(stderr,
				"%d: missing fields (destination port)\n",
				linenum);
			return NULL;
		}

		if (isdigit(**cpp) && (s = strchr(*cpp, '-')))
			*s++ = '\0';
		else
			s = NULL;

		if (!portnum(*cpp, &ipn.in_pmin, linenum))
			return NULL;
		ipn.in_pmin = htons(ipn.in_pmin);
		cpp++;

		if (!strcmp(*cpp, "-")) {
			cpp++;
			s = *cpp++;
		}

		if (s) {
			if (!portnum(s, &ipn.in_pmax, linenum))
				return NULL;
			ipn.in_pmax = htons(ipn.in_pmax);
		} else
			ipn.in_pmax = ipn.in_pmin;
	}

	if (!*cpp) {
		fprintf(stderr, "%d: missing fields (->)\n", linenum);
		return NULL;
	}
	if (strcmp(*cpp, "->")) {
		fprintf(stderr, "%d: missing ->\n", linenum);
		return NULL;
	}
	cpp++;

	if (!*cpp) {
		fprintf(stderr, "%d: missing fields (%s)\n",
			linenum, ipn.in_redir ? "destination" : "target");
		return NULL;
	}

	if (ipn.in_redir == NAT_MAP) {
		if (!strcasecmp(*cpp, "range")) {
			cpp++;
			ipn.in_flags |= IPN_IPRANGE;
			if (!*cpp) {
				fprintf(stderr, "%d: missing fields (%s)\n",
					linenum,
					ipn.in_redir ? "destination":"target");
				return NULL;
			}
		}
	}

	if (ipn.in_flags & IPN_IPRANGE) {
		dnetm = strrchr(*cpp, '-');
		if (dnetm == NULL) {
			cpp++;
			if (*cpp && !strcmp(*cpp, "-") && *(cpp + 1))
					dnetm = *(cpp + 1);
		} else
			*dnetm++ = '\0';
		if (dnetm == NULL || *dnetm == '\0') {
			fprintf(stderr,
				"%d: desination range not specified\n",
				linenum);
			return NULL;
		}
	} else if (ipn.in_redir != NAT_REDIRECT) {
		dnetm = strrchr(*cpp, '/');
		if (dnetm == NULL) {
			cpp++;
			if (*cpp && !strcasecmp(*cpp, "netmask"))
				dnetm = *++cpp;
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

	if (ipn.in_redir == NAT_REDIRECT) {
		dnetm = strchr(*cpp, ',');
		if (dnetm != NULL) {
			ipn.in_flags |= IPN_SPLIT;
			*dnetm++ = '\0';
		}
		if (hostnum((u_32_t *)&ipn.in_inip, *cpp, linenum) == -1)
			return NULL;
	} else {
		if (hostnum((u_32_t *)&ipn.in_outip, *cpp, linenum) == -1)
			return NULL;
	}
	cpp++;

	if (ipn.in_redir & NAT_MAPBLK) {
		if (*cpp && strcasecmp(*cpp, "ports")) {
			fprintf(stderr,
				"%d: expected \"ports\" - got \"%s\"\n",
				linenum, *cpp);
			return NULL;
		}
		cpp++;
		if (*cpp) {
			ipn.in_pmin = atoi(*cpp);
			cpp++;
		} else
			ipn.in_pmin = 0;
	} else if ((ipn.in_redir & NAT_BIMAP) == NAT_REDIRECT) {
		if (!*cpp || strrchr(*cpp, '/') != NULL) {
			fprintf(stderr, "%d: No netmask supported in %s\n",
				linenum, "destination host for redirect");
			return NULL;
		}
		/* If it's a in_redir, expect target port */

		if (strcasecmp(*cpp, "port")) {
			fprintf(stderr, "%d: missing fields - 2nd port (%s)\n",
				linenum, *cpp);
			return NULL;
		}
		cpp++;
		if (!*cpp) {
			fprintf(stderr,
				"%d: missing fields (destination port)\n",
				linenum);
			return NULL;
		}
		if (!portnum(*cpp, &ipn.in_pnext, linenum))
			return NULL;
		ipn.in_pnext = htons(ipn.in_pnext);
		cpp++;
	} 
	if (dnetm && *dnetm == '/')
		*dnetm++ = '\0';

	if (ipn.in_redir & (NAT_MAP|NAT_MAPBLK)) {
		if (ipn.in_flags & IPN_IPRANGE) {
			if (hostnum((u_32_t *)&ipn.in_outmsk, dnetm,
				    linenum) == -1)
				return NULL;
		} else if (genmask(dnetm, (u_32_t *)&ipn.in_outmsk))
			return NULL;
	} else {
		if (ipn.in_flags & IPN_SPLIT) {
			if (hostnum((u_32_t *)&ipn.in_inmsk, dnetm,
				    linenum) == -1)
				return NULL;
		} else if (genmask("255.255.255.255", (u_32_t *)&ipn.in_inmsk))
			return NULL;
		if (!*cpp) {
			ipn.in_flags |= IPN_TCP; /* XXX- TCP only by default */
			proto = "tcp";
		} else {
			if (!strcasecmp(*cpp, "tcp"))
				ipn.in_flags |= IPN_TCP;
			else if (!strcasecmp(*cpp, "udp"))
				ipn.in_flags |= IPN_UDP;
			else if (!strcasecmp(*cpp, "tcp/udp"))
				ipn.in_flags |= IPN_TCPUDP;
			else if (!strcasecmp(*cpp, "tcpudp"))
				ipn.in_flags |= IPN_TCPUDP;
			else if (!strcasecmp(*cpp, "ip"))
				ipn.in_flags |= IPN_ANY;
			else {
				ipn.in_flags |= IPN_ANY;
				if ((pr = getprotobyname(*cpp)))
					ipn.in_p = pr->p_proto;
				else
					ipn.in_p = atoi(*cpp);
			}
			proto = *cpp;
			cpp++;

			if (*cpp && !strcasecmp(*cpp, "round-robin")) {
				cpp++;
				ipn.in_flags |= IPN_ROUNDR;
			}

			if (*cpp && !strcasecmp(*cpp, "frag")) {
				cpp++;
				ipn.in_flags |= IPN_FRAG;
			}

			if (*cpp) {
				fprintf(stderr,
				"%d: extra junk at the end of rdr: %s\n",
					linenum, *cpp);
				return NULL;
			}
		}
	}

	if (!(ipn.in_flags & IPN_SPLIT))
		ipn.in_inip &= ipn.in_inmsk;
	if ((ipn.in_flags & IPN_IPRANGE) == 0)
		ipn.in_outip &= ipn.in_outmsk;
	ipn.in_srcip &= ipn.in_srcmsk;

	if ((ipn.in_redir & NAT_MAPBLK) != 0)
		nat_setgroupmap(&ipn);

	if (*cpp && !strcasecmp(*cpp, "frag")) {
		cpp++;
		ipn.in_flags |= IPN_FRAG;
	}

	if (!*cpp)
		return &ipn;

	if (ipn.in_redir == NAT_BIMAP) {
		fprintf(stderr,
			"%d: extra words at the end of bimap line: %s\n",
			linenum, *cpp);
		return NULL;
	}

	if (!strcasecmp(*cpp, "proxy")) {
		cpp++;
		if (!*cpp) {
			fprintf(stderr,
				"%d: missing parameter for \"proxy\"\n",
				linenum);
			return NULL;
		}
		dport = NULL;

		if (!strcasecmp(*cpp, "port")) {
			cpp++;
			if (!*cpp) {
				fprintf(stderr,
					"%d: missing parameter for \"port\"\n",
					linenum);
				return NULL;
			}

			dport = *cpp;
			cpp++;

			if (!*cpp) {
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

		if ((proto = index(*cpp, '/'))) {
			*proto++ = '\0';
			if ((pr = getprotobyname(proto)))
				ipn.in_p = pr->p_proto;
			else
				ipn.in_p = atoi(proto);
		} else
			ipn.in_p = 0;

		if (dport && !portnum(dport, &ipn.in_dport, linenum))
			return NULL;
		ipn.in_dport = htons(ipn.in_dport);

		(void) strncpy(ipn.in_plabel, *cpp, sizeof(ipn.in_plabel));
		cpp++;

		if (*cpp) {
			fprintf(stderr,
				"%d: too many parameters for \"proxy\"\n",
				linenum);
			return NULL;
		}
		return &ipn;
	}

	if (strcasecmp(*cpp, "portmap")) {
		fprintf(stderr,
			"%d: expected \"portmap\" - got \"%s\"\n", linenum,
			*cpp);
		return NULL;
	}
	cpp++;
	if (!*cpp) {
		fprintf(stderr, "%d: missing expression following portmap\n",
			linenum);
		return NULL;
	}

	if (!strcasecmp(*cpp, "tcp"))
		ipn.in_flags |= IPN_TCP;
	else if (!strcasecmp(*cpp, "udp"))
		ipn.in_flags |= IPN_UDP;
	else if (!strcasecmp(*cpp, "tcpudp"))
		ipn.in_flags |= IPN_TCPUDP;
	else if (!strcasecmp(*cpp, "tcp/udp"))
		ipn.in_flags |= IPN_TCPUDP;
	else {
		fprintf(stderr,
			"%d: expected protocol name - got \"%s\"\n",
			linenum, *cpp);
		return NULL;
	}
	proto = *cpp;
	cpp++;

	if (!*cpp) {
		fprintf(stderr, "%d: no port range found\n", linenum);
		return NULL;
	}

	if (!strcasecmp(*cpp, "auto")) {
		ipn.in_flags |= IPN_AUTOPORTMAP;
		ipn.in_pmin = htons(1024);
		ipn.in_pmax = htons(65535);
		nat_setgroupmap(&ipn);
		return &ipn;
	}

	if (!(t = strchr(*cpp, ':'))) {
		fprintf(stderr, "%d: no port range in \"%s\"\n",
			linenum, *cpp);
		return NULL;
	}
	*t++ = '\0';
	if (!portnum(*cpp, &ipn.in_pmin, linenum) ||
	    !portnum(t, &ipn.in_pmax, linenum))
		return NULL;
	ipn.in_pmin = htons(ipn.in_pmin);
	ipn.in_pmax = htons(ipn.in_pmax);
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
				printnat(np, opts, NULL);
			if (!(opts & OPT_NODO)) {
				if (!(opts & OPT_REMOVE)) {
					if (ioctl(fd, SIOCADNAT, &np) == -1) {
						fprintf(stderr, "%d:",
							linenum);
						perror("ioctl(SIOCADNAT)");
					}
				} else if (ioctl(fd, SIOCRMNAT, &np) == -1) {
					fprintf(stderr, "%d:", linenum);
					perror("ioctl(SIOCRMNAT)");
				}
			}
		}
	}
	if (fp != stdin)
		fclose(fp);
}
