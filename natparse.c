/*
 * Copyright (C) 1993-2002 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 */
#if defined(__sgi) && (IRIX > 602)
# include <sys/ptimers.h>
#endif
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#if !defined(__SVR4) && !defined(__svr4__)
# include <strings.h>
#else
# include <sys/byteorder.h>
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
#include "netinet/ip_nat.h"
#include "netinet/ip_state.h"
#include "netinet/ip_proxy.h"
#include "ipf.h"

#if	defined(sun) && !SOLARIS2
# define	STRERROR(x)	sys_errlist[x]
extern	char	*sys_errlist[];
#else
# define	STRERROR(x)	strerror(x)
#endif

#if !defined(lint)
static const char sccsid[] ="@(#)ipnat.c	1.9 6/5/96 (C) 1993 Darren Reed";
static const char rcsid[] = "@(#)$Id: natparse.c,v 1.17.2.29 2003/05/15 17:45:34 darrenr Exp $";
#endif


#if	SOLARIS
#define	bzero(a,b)	memset(a,0,b)
#endif

extern	void	printnat __P((ipnat_t *, int));
extern	int	countbits __P((u_32_t));
extern	char	*proto;

ipnat_t	*natparse __P((char *, int, int *));
void	natparsefile __P((int, char *, int));
void	nat_setgroupmap __P((struct ipnat *));


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


/*
 * Parse a line of input from the ipnat configuration file
 *
 * status:
 *	< 0	error
 *	= 0	OK
 *	> 0	programmer error
 */
ipnat_t *natparse(line, linenum, status)
char *line;
int linenum;
int *status;
{
	static ipnat_t ipn;
	struct protoent *pr;
	char *dnetm = NULL, *dport = NULL;
	char *s, *t, *cps[31], **cpp;
	int i, cnt;
	char *port1a = NULL, *port1b = NULL, *port2a = NULL;

	*status = 100;		/* default to error */
	proto = NULL;

	/*
	 * Search for end of line and comment marker, advance of leading spaces
	 */
	if ((s = strchr(line, '\n')))
		*s = '\0';
	if ((s = strchr(line, '#')))
		*s = '\0';
	while (*line && isspace(*line))
		line++;
	if (!*line) {
		*status = 0;
		return NULL;
	}

	bzero((char *)&ipn, sizeof(ipn));
	cnt = 0;

	/*
	 * split line upto into segments.
	 */
	for (i = 0, *cps = strtok(line, " \b\t\r\n"); cps[i] && i < 30; cnt++)
		cps[++i] = strtok(NULL, " \b\t\r\n");

	cps[i] = NULL;

	if (cnt < 3) {
		fprintf(stderr, "%d: not enough segments in line\n", linenum);
		*status = -1;
		return NULL;
	}

	cpp = cps;

	/*
	 * Check first word is a recognised keyword and then is the interface
	 */
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
		*status = -1;
		return NULL;
	}

	cpp++;

	strncpy(ipn.in_ifname, *cpp, sizeof(ipn.in_ifname) - 1);
	ipn.in_ifname[sizeof(ipn.in_ifname) - 1] = '\0';
	cpp++;

	/*
	 * If the first word after the interface is "from" or is a ! then
	 * the expanded syntax is being used so parse it differently.
	 */
	if (!strcasecmp(*cpp, "from") || (**cpp == '!')) {
		if (!strcmp(*cpp, "!")) {
			cpp++;
			if (strcasecmp(*cpp, "from")) {
				fprintf(stderr, "Missing from after !\n");
				*status = -1;
				return NULL;
			}
			ipn.in_flags |= IPN_NOTSRC;
		} else if (**cpp == '!') {
			if (strcasecmp(*cpp + 1, "from")) {
				fprintf(stderr, "Missing from after !\n");
				*status = -1;
				return NULL;
			}
			ipn.in_flags |= IPN_NOTSRC;
		}
		if ((ipn.in_flags & IPN_NOTSRC) &&
		    (ipn.in_redir & (NAT_MAP|NAT_MAPBLK))) {
			fprintf(stderr, "Cannot use '! from' with map\n");
			*status = -1;
			return NULL;
		}

		ipn.in_flags |= IPN_FILTER;
		cpp++;
		if (ipn.in_redir == NAT_REDIRECT) {
			if (hostmask(&cpp, (u_32_t *)&ipn.in_srcip,
				     (u_32_t *)&ipn.in_srcmsk, &ipn.in_sport,
				     &ipn.in_scmp, &ipn.in_stop, linenum)) {
				*status = -1;
				return NULL;
			}
		} else {
			if (hostmask(&cpp, (u_32_t *)&ipn.in_inip,
				     (u_32_t *)&ipn.in_inmsk, &ipn.in_sport,
				     &ipn.in_scmp, &ipn.in_stop, linenum)) {
				*status = -1;
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
			*status = -1;
			return NULL;
		}
		if ((ipn.in_flags & IPN_NOTDST) &&
		    (ipn.in_redir & (NAT_REDIRECT))) {
			fprintf(stderr, "Cannot use '! to' with rdr\n");
			*status = -1;
			return NULL;
		}

		if (!*++cpp) {
			fprintf(stderr, "%d: missing host after to\n", linenum);
			*status = -1;
			return NULL;
		}
		if (ipn.in_redir == NAT_REDIRECT) {
			if (hostmask(&cpp, (u_32_t *)&ipn.in_outip,
				     (u_32_t *)&ipn.in_outmsk, &ipn.in_dport,
				     &ipn.in_dcmp, &ipn.in_dtop, linenum)) {
				*status = -1;
				return NULL;
			}
			ipn.in_pmin = htons(ipn.in_dport);
		} else {
			if (hostmask(&cpp, (u_32_t *)&ipn.in_srcip,
				     (u_32_t *)&ipn.in_srcmsk, &ipn.in_dport,
				     &ipn.in_dcmp, &ipn.in_dtop, linenum)) {
				*status = -1;
				return NULL;
			}
		}
	} else {
		s = *cpp;
		if (!s) {
			fprintf(stderr, "%d: short line\n", linenum);
			*status = -1;
			return NULL;
		}
		t = strchr(s, '/');
		if (!t) {
			fprintf(stderr, "%d: no netmask on LHS\n", linenum);
			*status = -1;
			return NULL;
		}
		*t++ = '\0';
		if (ipn.in_redir == NAT_REDIRECT) {
			if (hostnum((u_32_t *)&ipn.in_outip, s, linenum) == -1){
				*status = -1;
				return NULL;
			}
			if (genmask(t, (u_32_t *)&ipn.in_outmsk) == -1) {
				*status = -1;
				return NULL;
			}
		} else {
			if (hostnum((u_32_t *)&ipn.in_inip, s, linenum) == -1) {
				*status = -1;
				return NULL;
			}
			if (genmask(t, (u_32_t *)&ipn.in_inmsk) == -1) {
				*status = -1;
				return NULL;
			}
		}
		cpp++;
		if (!*cpp) {
			fprintf(stderr, "%d: short line\n", linenum);
			*status = -1;
			return NULL;
		}
	}

	/*
	 * If it is a standard redirect then we expect it to have a port
	 * match after the hostmask.
	 */
	if ((ipn.in_redir == NAT_REDIRECT) && !(ipn.in_flags & IPN_FILTER)) {
		if (strcasecmp(*cpp, "port")) {
			fprintf(stderr, "%d: missing fields - 1st port\n",
				linenum);
			*status = -1;
			return NULL;
		}

		cpp++;

		if (!*cpp) {
			fprintf(stderr,
				"%d: missing fields (destination port)\n",
				linenum);
			*status = -1;
			return NULL;
		}

		if (isdigit(**cpp) && (s = strchr(*cpp, '-')))
			*s++ = '\0';
		else
			s = NULL;

		port1a = *cpp++;

		if (!strcmp(*cpp, "-")) {
			cpp++;
			s = *cpp++;
		}

		if (s)
			port1b = s;
		else
			ipn.in_pmax = ipn.in_pmin;
	}

	/*
	 * In the middle of the NAT rule syntax is -> to indicate the
	 * direction of translation.
	 */
	if (!*cpp) {
		fprintf(stderr, "%d: missing fields (->)\n", linenum);
		*status = -1;
		return NULL;
	}
	if (strcmp(*cpp, "->")) {
		fprintf(stderr, "%d: missing ->\n", linenum);
		*status = -1;
		return NULL;
	}
	cpp++;

	if (!*cpp) {
		fprintf(stderr, "%d: missing fields (%s)\n",
			linenum, ipn.in_redir ? "destination" : "target");
		*status = -1;
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
				*status = -1;
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
			*status = -1;
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
			*status = -1;
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
		if (hostnum((u_32_t *)&ipn.in_inip, *cpp, linenum) == -1) {
			*status = -1;
			return NULL;
		}
#if SOLARIS
		if (ntohl(ipn.in_inip) == INADDR_LOOPBACK) {
			fprintf(stderr,
				"localhost as destination not supported\n");
			*status = -1;
			return NULL;
		}
#endif
	} else {
		if (!strcmp(*cpp, ipn.in_ifname))
			*cpp = "0";
		if (hostnum((u_32_t *)&ipn.in_outip, *cpp, linenum) == -1) {
			*status = -1;
			return NULL;
		}
	}
	cpp++;

	if (ipn.in_redir & NAT_MAPBLK) {
		if (*cpp) {
			if (strcasecmp(*cpp, "ports")) {
				fprintf(stderr,
					"%d: expected \"ports\" - got \"%s\"\n",
					linenum, *cpp);
				*status = -1;
				return NULL;
			}
			cpp++;
			if (*cpp == NULL) {
				fprintf(stderr,
					"%d: missing argument to \"ports\"\n",
					linenum);
				*status = -1;
				return NULL;
			}
			if (!strcasecmp(*cpp, "auto"))
				ipn.in_flags |= IPN_AUTOPORTMAP;
			else
				ipn.in_pmin = atoi(*cpp);
			cpp++;
		} else
			ipn.in_pmin = 0;
	} else if ((ipn.in_redir & NAT_BIMAP) == NAT_REDIRECT) {
		if (*cpp && (strrchr(*cpp, '/') != NULL)) {
			fprintf(stderr, "%d: No netmask supported in %s\n",
				linenum, "destination host for redirect");
			*status = -1;
			return NULL;
		}

		if (!*cpp) {
			fprintf(stderr, "%d: Missing destination port %s\n",
				linenum, "in redirect");
			*status = -1;
			return NULL;
		}

		/* If it's a in_redir, expect target port */

		if (strcasecmp(*cpp, "port")) {
			fprintf(stderr, "%d: missing fields - 2nd port (%s)\n",
				linenum, *cpp);
			*status = -1;
			return NULL;
		}
		cpp++;
		if (!*cpp) {
			fprintf(stderr,
				"%d: missing fields (destination port)\n",
				linenum);
			*status = -1;
			return NULL;
		}

		port2a = *cpp++;
	} 
	if (dnetm && *dnetm == '/')
		*dnetm++ = '\0';

	if (ipn.in_redir & (NAT_MAP|NAT_MAPBLK)) {
		if (ipn.in_flags & IPN_IPRANGE) {
			if (hostnum((u_32_t *)&ipn.in_outmsk, dnetm,
				    linenum) == -1) {
				*status = -1;
				return NULL;
			}
		} else if (genmask(dnetm, (u_32_t *)&ipn.in_outmsk)) {
			*status = -1;
			return NULL;
		}
	} else {
		if (ipn.in_flags & IPN_SPLIT) {
			if (hostnum((u_32_t *)&ipn.in_inmsk, dnetm,
				    linenum) == -1) {
				*status = -1;
				return NULL;
			}
		} else if (genmask("255.255.255.255", (u_32_t *)&ipn.in_inmsk)){
			*status = -1;
			return NULL;
		}
		if (!*cpp) {
			ipn.in_flags |= IPN_TCP; /* XXX- TCP only by default */
			proto = "tcp";
		} else {
			proto = *cpp++;
			if (!strcasecmp(proto, "tcp"))
				ipn.in_flags |= IPN_TCP;
			else if (!strcasecmp(proto, "udp"))
				ipn.in_flags |= IPN_UDP;
			else if (!strcasecmp(proto, "tcp/udp"))
				ipn.in_flags |= IPN_TCPUDP;
			else if (!strcasecmp(proto, "tcpudp")) {
				ipn.in_flags |= IPN_TCPUDP;
				proto = "tcp/udp";
			} else if (!strcasecmp(proto, "ip"))
				ipn.in_flags |= IPN_ANY;
			else {
				ipn.in_flags |= IPN_ANY;
				if ((pr = getprotobyname(proto)))
					ipn.in_p = pr->p_proto;
				else {
					if (!isdigit(*proto)) {
						fprintf(stderr,
						"%d: Unknown protocol %s\n",
							linenum, proto);
						*status = -1;
						return NULL;
					} else
						ipn.in_p = atoi(proto);
				}
			}
			if ((ipn.in_flags & IPN_TCPUDP) == 0) {
				port1a = "0";
				port2a = "0";
			}

			if (*cpp && !strcasecmp(*cpp, "round-robin")) {
				cpp++;
				ipn.in_flags |= IPN_ROUNDR;
			}

			if (*cpp && !strcasecmp(*cpp, "frag")) {
				cpp++;
				ipn.in_flags |= IPN_FRAG;
			}

			if (*cpp && !strcasecmp(*cpp, "age")) {
				cpp++;
				if (!*cpp) {
					fprintf(stderr,
						"%d: age with no parameters\n",
						linenum);
					*status = -1;
					return NULL;
				}

				ipn.in_age[0] = atoi(*cpp);
				s = index(*cpp, '/');
				if (s != NULL)
					ipn.in_age[1] = atoi(s + 1);
				else
					ipn.in_age[1] = ipn.in_age[0];
				cpp++;
			}

			if (*cpp && !strcasecmp(*cpp, "mssclamp")) {
				cpp++;
				if (*cpp) {
					ipn.in_mssclamp = atoi(*cpp);
					cpp++;
				} else {
					fprintf(stderr,
					   "%d: mssclamp with no parameters\n",
						linenum);
					*status = -1;
					return NULL;
				}
			}

			if (*cpp) {
				fprintf(stderr,
				"%d: extra junk at the end of the line: %s\n",
					linenum, *cpp);
				*status = -1;
				return NULL;
			}
		}
	}

	if ((ipn.in_redir == NAT_REDIRECT) && !(ipn.in_flags & IPN_FILTER)) {
		if (!portnum(port1a, &ipn.in_pmin, linenum)) {
			*status = -1;
			return NULL;
		}
		ipn.in_pmin = htons(ipn.in_pmin);
		if (port1b != NULL) {
			if (!portnum(port1b, &ipn.in_pmax, linenum)) {
				*status = -1;
				return NULL;
			}
			ipn.in_pmax = htons(ipn.in_pmax);
		} else
			ipn.in_pmax = ipn.in_pmin;
	}

	if ((ipn.in_redir & NAT_BIMAP) == NAT_REDIRECT) {
		if (!portnum(port2a, &ipn.in_pnext, linenum)) {
			*status = -1;
			return NULL;
		}
		ipn.in_pnext = htons(ipn.in_pnext);
	}

	if (!(ipn.in_flags & IPN_SPLIT))
		ipn.in_inip &= ipn.in_inmsk;
	if ((ipn.in_flags & IPN_IPRANGE) == 0)
		ipn.in_outip &= ipn.in_outmsk;
	ipn.in_srcip &= ipn.in_srcmsk;

	if ((ipn.in_redir & NAT_MAPBLK) != 0)
		nat_setgroupmap(&ipn);

	if (*cpp && !*(cpp+1) && !strcasecmp(*cpp, "frag")) {
		cpp++;
		ipn.in_flags |= IPN_FRAG;
	}

	if (!*cpp) {
		*status = 0;
		return &ipn;
	}

	if (ipn.in_redir != NAT_BIMAP && !strcasecmp(*cpp, "proxy")) {
		u_short pport;

		if (ipn.in_redir == NAT_BIMAP) {
			fprintf(stderr, "%d: cannot use proxy with bimap\n",
				linenum);
			*status = -1;
			return NULL;
		}
		cpp++;
		if (!*cpp) {
			fprintf(stderr,
				"%d: missing parameter for \"proxy\"\n",
				linenum);
			*status = -1;
			return NULL;
		}
		dport = NULL;

		if (!strcasecmp(*cpp, "port")) {
			cpp++;
			if (!*cpp) {
				fprintf(stderr,
					"%d: missing parameter for \"port\"\n",
					linenum);
				*status = -1;
				return NULL;
			}

			dport = *cpp;
			cpp++;

			if (!*cpp) {
				fprintf(stderr,
					"%d: missing parameter for \"proxy\"\n",
					linenum);
				*status = -1;
				return NULL;
			}
		} else {
			fprintf(stderr,
				"%d: missing keyword \"port\"\n", linenum);
			*status = -1;
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

		if (dport && !portnum(dport, &pport, linenum))
			return NULL;
		if (ipn.in_dcmp != 0) {
			if (pport != ipn.in_dport) {
				fprintf(stderr,
					"%d: mismatch in port numbers\n",
					linenum);
				return NULL;
			}
		} else
			ipn.in_dport = htons(pport);

		(void) strncpy(ipn.in_plabel, *cpp, sizeof(ipn.in_plabel));
		cpp++;

	} else if (ipn.in_redir != NAT_BIMAP && !strcasecmp(*cpp, "portmap")) {
		if (ipn.in_redir == NAT_BIMAP) {
			fprintf(stderr, "%d: cannot use portmap with bimap\n",
				linenum);
			*status = -1;
			return NULL;
		}
		cpp++;
		if (!*cpp) {
			fprintf(stderr,
				"%d: missing expression following portmap\n",
				linenum);
			*status = -1;
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
			*status = -1;
			return NULL;
		}
		proto = *cpp;
		cpp++;

		if (!*cpp) {
			fprintf(stderr, "%d: no port range found\n", linenum);
			*status = -1;
			return NULL;
		}

		if (!strcasecmp(*cpp, "auto")) {
			ipn.in_flags |= IPN_AUTOPORTMAP;
			ipn.in_pmin = htons(1024);
			ipn.in_pmax = htons(65535);
			nat_setgroupmap(&ipn);
			cpp++;
		} else {
			if (!(t = strchr(*cpp, ':'))) {
				fprintf(stderr,
					"%d: no port range in \"%s\"\n",
					linenum, *cpp);
				*status = -1;
				return NULL;
			}
			*t++ = '\0';
			if (!portnum(*cpp, &ipn.in_pmin, linenum) ||
			    !portnum(t, &ipn.in_pmax, linenum)) {
				*status = -1;
				return NULL;
			}
			ipn.in_pmin = htons(ipn.in_pmin);
			ipn.in_pmax = htons(ipn.in_pmax);
			cpp++;
		}
	}

	if (*cpp && !strcasecmp(*cpp, "frag")) {
		cpp++;
		ipn.in_flags |= IPN_FRAG;
	}

	if (*cpp && !strcasecmp(*cpp, "age")) {
		cpp++;
		if (!*cpp) {
			fprintf(stderr, "%d: age with no parameters\n",
				linenum);
			*status = -1;
			return NULL;
		}
		ipn.in_age[0] = atoi(*cpp);
		s = index(*cpp, '/');
		if (s != NULL)
			ipn.in_age[1] = atoi(s + 1);
		else
			ipn.in_age[1] = ipn.in_age[0];
		cpp++;
	}

	if (*cpp && !strcasecmp(*cpp, "mssclamp")) {
		cpp++;
		if (*cpp) {
			ipn.in_mssclamp = atoi(*cpp);
			cpp++;
		} else {
			fprintf(stderr, "%d: mssclamp with no parameters\n",
				linenum);
			*status = -1;
			return NULL;
		}
	}

	if (*cpp) {
		fprintf(stderr, "%d: extra junk at the end of the line: %s\n",
			linenum, *cpp);
		*status = -1;
		return NULL;
	}

	*status = 0;
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
	int	parsestatus;

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

		parsestatus = 1;
		np = natparse(line, linenum, &parsestatus);
		if (parsestatus != 0) {
			if (*line) {
				fprintf(stderr, "%d: syntax error in \"%s\"\n",
					linenum, line);
			}
			fprintf(stderr, "%s: %s error (%d), quitting\n",
			    file,
			    ((parsestatus < 0)? "parse": "internal"),
			    parsestatus);
			exit(1);
		}
		if (np) {
			if ((opts & OPT_VERBOSE) && np)
				printnat(np, opts);
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
