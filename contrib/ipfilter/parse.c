/*
 * Copyright (C) 1993-2001 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 */
#ifdef __sgi
# include <sys/ptimers.h>
#endif
#include <sys/types.h>
#if !defined(__SVR4) && !defined(__svr4__)
#include <strings.h>
#else
#include <sys/byteorder.h>
#endif
#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <net/if.h>
#if __FreeBSD_version >= 300000
# include <net/if_var.h>
#endif
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <stddef.h>
#include <netdb.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>
#include <resolv.h>
#include <ctype.h>
#include <syslog.h>
#include "ip_compat.h"
#include "ip_fil.h"
#include "ipf.h"
#include "facpri.h"

#if !defined(lint)
static const char sccsid[] = "@(#)parse.c	1.44 6/5/96 (C) 1993-2000 Darren Reed";
static const char rcsid[] = "@(#)$IPFilter: parse.c,v 2.8 1999/12/28 10:49:46 darrenr Exp $";
#endif

extern	struct	ipopt_names	ionames[], secclass[];
extern	int	opts;
extern	int	use_inet6;

int	addicmp __P((char ***, struct frentry *, int));
int	extras __P((char ***, struct frentry *, int));

int	icmpcode __P((char *)), addkeep __P((char ***, struct frentry *, int));
int	to_interface __P((frdest_t *, char *, int));
void	print_toif __P((char *, frdest_t *));
void	optprint __P((u_short *, u_long, u_long));
int	loglevel __P((char **, u_int *, int));
void	printlog __P((frentry_t *));
void	printifname __P((char *, char *, void *));

extern	char	*proto;
extern	char	flagset[];
extern	u_char	flags[];


/* parse()
 *
 * parse a line read from the input filter rule file
 */
struct	frentry	*parse(line, linenum)
char	*line;
int     linenum;
{
	static	struct	frentry	fil;
	char	*cps[31], **cpp, *endptr, *s;
	struct	protoent	*p = NULL;
	int	i, cnt = 1, j, ch;
	u_int	k;

	while (*line && isspace(*line))
		line++;
	if (!*line)
		return NULL;

	bzero((char *)&fil, sizeof(fil));
	fil.fr_mip.fi_v = 0xf;
	fil.fr_ip.fi_v = use_inet6 ? 6 : 4;
	fil.fr_loglevel = 0xffff;

	/*
	 * break line up into max of 20 segments
	 */
	if (opts & OPT_DEBUG)
		fprintf(stderr, "parse [%s]\n", line);
	for (i = 0, *cps = strtok(line, " \b\t\r\n"); cps[i] && i < 30; cnt++)
		cps[++i] = strtok(NULL, " \b\t\r\n");
	cps[i] = NULL;

	if (cnt < 3) {
		fprintf(stderr, "%d: not enough segments in line\n", linenum);
		return NULL;
	}

	cpp = cps;
	/*
	 * The presence of an '@' followed by a number gives the position in
	 * the current rule list to insert this one.
	 */
	if (**cpp == '@')
		fil.fr_hits = (U_QUAD_T)atoi(*cpp++ + 1) + 1;


	/*
	 * Check the first keyword in the rule and any options that are
	 * expected to follow it.
	 */
	if (!strcasecmp("block", *cpp)) {
		fil.fr_flags |= FR_BLOCK;
		if (!strncasecmp(*(cpp+1), "return-icmp-as-dest", 19) &&
		    (i = 19))
			fil.fr_flags |= FR_FAKEICMP;
		else if (!strncasecmp(*(cpp+1), "return-icmp", 11) && (i = 11))
			fil.fr_flags |= FR_RETICMP;
		if (fil.fr_flags & FR_RETICMP) {
			cpp++;
			if (strlen(*cpp) == i) {
				if (*(cpp + 1) && **(cpp +1) == '(') {
					cpp++;
					i = 0;
				} else
					i = -1;
			}

			/*
			 * The ICMP code is not required to follow in ()'s
			 */
			if ((i >= 0) && (*(*cpp + i) == '(')) {
				i++;
				j = icmpcode(*cpp + i);
				if (j == -1) {
					fprintf(stderr,
					"%d: unrecognised icmp code %s\n",
						linenum, *cpp + 20);
					return NULL;
				}
				fil.fr_icode = j;
			}
		} else if (!strncasecmp(*(cpp+1), "return-rst", 10)) {
			fil.fr_flags |= FR_RETRST;
			cpp++;
		}
	} else if (!strcasecmp("count", *cpp)) {
		fil.fr_flags |= FR_ACCOUNT;
	} else if (!strcasecmp("pass", *cpp)) {
		fil.fr_flags |= FR_PASS;
	} else if (!strcasecmp("nomatch", *cpp)) {
		fil.fr_flags |= FR_NOMATCH;
	} else if (!strcasecmp("auth", *cpp)) {
		 fil.fr_flags |= FR_AUTH;
	} else if (!strcasecmp("preauth", *cpp)) {
		 fil.fr_flags |= FR_PREAUTH;
	} else if (!strcasecmp("skip", *cpp)) {
		cpp++;
		if (ratoui(*cpp, &k, 0, UINT_MAX))
			fil.fr_skip = k;
		else {
			fprintf(stderr, "%d: integer must follow skip\n",
				linenum);
			return NULL;
		}
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
		if (*cpp && !strcasecmp(*(cpp+1), "or-block")) {
			fil.fr_flags |= FR_LOGORBLOCK;
			cpp++;
		}
		if (!strcasecmp(*(cpp+1), "level")) {
			cpp++;
			if (loglevel(cpp, &fil.fr_loglevel, linenum) == -1)
				return NULL;
			cpp++;
		}
	} else {
		/*
		 * Doesn't start with one of the action words
		 */
		fprintf(stderr, "%d: unknown keyword (%s)\n", linenum, *cpp);
		return NULL;
	}
	if (!*++cpp) {
		fprintf(stderr, "%d: missing 'in'/'out' keyword\n", linenum);
		return NULL;
	}

	/*
	 * Get the direction for filtering.  Impose restrictions on direction
	 * if blocking with returning ICMP or an RST has been requested.
	 */
	if (!strcasecmp("in", *cpp))
		fil.fr_flags |= FR_INQUE;
	else if (!strcasecmp("out", *cpp)) {
		fil.fr_flags |= FR_OUTQUE;
		if (fil.fr_flags & FR_RETICMP) {
			fprintf(stderr,
				"%d: Can only use return-icmp with 'in'\n",
				linenum);
			return NULL;
		} else if (fil.fr_flags & FR_RETRST) {
			fprintf(stderr,
				"%d: Can only use return-rst with 'in'\n", 
				linenum);
			return NULL;
		}
	}
	if (!*++cpp) {
		fprintf(stderr, "%d: missing source specification\n", linenum);
		return NULL;
	}

	if (!strcasecmp("log", *cpp)) {
		if (!*++cpp) {
			fprintf(stderr, "%d: missing source specification\n",
				linenum);
			return NULL;
		}
		if (fil.fr_flags & FR_PASS)
			fil.fr_flags |= FR_LOGP;
		else if (fil.fr_flags & FR_BLOCK)
			fil.fr_flags |= FR_LOGB;
		if (*cpp && !strcasecmp(*cpp, "body")) {
			fil.fr_flags |= FR_LOGBODY;
			cpp++;
		}
		if (*cpp && !strcasecmp(*cpp, "first")) {
			fil.fr_flags |= FR_LOGFIRST;
			cpp++;
		}
		if (*cpp && !strcasecmp(*cpp, "or-block")) {
			if (!(fil.fr_flags & FR_PASS)) {
				fprintf(stderr,
					"%d: or-block must be used with pass\n",
					linenum);
				return NULL;
			}
			fil.fr_flags |= FR_LOGORBLOCK;
			cpp++;
		}
		if (*cpp && !strcasecmp(*cpp, "level")) {
			if (loglevel(cpp, &fil.fr_loglevel, linenum) == -1)
				return NULL;
			cpp++;
			cpp++;
		}
	}

	if (*cpp && !strcasecmp("quick", *cpp)) {
		if (fil.fr_skip != 0) {
			fprintf(stderr, "%d: cannot use skip with quick\n",
				linenum);
			return NULL;
		}
		cpp++;
		fil.fr_flags |= FR_QUICK;
	}

	/*
	 * Parse rule options that are available if a rule is tied to an
	 * interface.
	 */
	*fil.fr_ifname = '\0';
	*fil.fr_oifname = '\0';
	if (*cpp && !strcasecmp(*cpp, "on")) {
		if (!*++cpp) {
			fprintf(stderr, "%d: interface name missing\n",
				linenum);
			return NULL;
		}

		s = index(*cpp, ',');
		if (s != NULL) {
			*s++ = '\0';
			(void)strncpy(fil.fr_ifnames[1], s, IFNAMSIZ - 1);
			fil.fr_ifnames[1][IFNAMSIZ - 1] = '\0';
		} else
			strcpy(fil.fr_ifnames[1], "*");

		(void)strncpy(fil.fr_ifnames[0], *cpp, IFNAMSIZ - 1);
		fil.fr_ifnames[0][IFNAMSIZ - 1] = '\0';

		cpp++;
		if (!*cpp) {
			if ((fil.fr_flags & FR_RETMASK) == FR_RETRST) {
				fprintf(stderr,
					"%d: %s can only be used with TCP\n",
					linenum, "return-rst");
				return NULL;
			}
			return &fil;
		}

		if (*cpp) {
			if (!strcasecmp(*cpp, "dup-to") && *(cpp + 1)) {
				cpp++;
				if (to_interface(&fil.fr_dif, *cpp, linenum))
					return NULL;
				cpp++;
			}
			if (*cpp && !strcasecmp(*cpp, "to") && *(cpp + 1)) {
				cpp++;
				if (to_interface(&fil.fr_tif, *cpp, linenum))
					return NULL;
				cpp++;
			} else if (*cpp && !strcasecmp(*cpp, "fastroute")) {
				if (!(fil.fr_flags & FR_INQUE)) {
					fprintf(stderr,
						"can only use %s with 'in'\n",
						"fastroute");
					return NULL;
				}
				fil.fr_flags |= FR_FASTROUTE;
				cpp++;
			}
		}

		/*
		 * Set the "other" interface name.  Lets you specify both
		 * inbound and outbound interfaces for state rules.  Do not
		 * prevent both interfaces from being the same.
		 */
		strcpy(fil.fr_ifnames[3], "*");
		if ((*cpp != NULL) && (*(cpp + 1) != NULL) &&
		    ((((fil.fr_flags & FR_INQUE) != 0) &&
		      (strcasecmp(*cpp, "out-via") == 0)) ||
		     (((fil.fr_flags & FR_OUTQUE) != 0) &&
		      (strcasecmp(*cpp, "in-via") == 0)))) {
			cpp++;

			s = index(*cpp, ',');
			if (s != NULL) {
				*s++ = '\0';
				(void)strncpy(fil.fr_ifnames[3], s,
					      IFNAMSIZ - 1);
				fil.fr_ifnames[3][IFNAMSIZ - 1] = '\0';
			}

			(void)strncpy(fil.fr_ifnames[2], *cpp, IFNAMSIZ - 1);
			fil.fr_ifnames[2][IFNAMSIZ - 1] = '\0';
			cpp++;
		} else
			strcpy(fil.fr_ifnames[2], "*");
	}
	if (*cpp && !strcasecmp(*cpp, "tos")) {
		if (!*++cpp) {
			fprintf(stderr, "%d: tos missing value\n", linenum);
			return NULL;
		}
		fil.fr_tos = strtol(*cpp, NULL, 0);
		fil.fr_mip.fi_tos = 0xff;
		cpp++;
	}

	if (*cpp && !strcasecmp(*cpp, "ttl")) {
		if (!*++cpp) {
			fprintf(stderr, "%d: ttl missing hopcount value\n",
				linenum);
			return NULL;
		}
		if (ratoi(*cpp, &i, 0, 255))
			fil.fr_ttl = i;
		else {
			fprintf(stderr, "%d: invalid ttl (%s)\n",
				linenum, *cpp);
			return NULL;
		}
		fil.fr_mip.fi_ttl = 0xff;
		cpp++;
	}

	/*
	 * check for "proto <protoname>" only decode udp/tcp/icmp as protoname
	 */
	proto = NULL;
	if (*cpp && !strcasecmp(*cpp, "proto")) {
		if (!*++cpp) {
			fprintf(stderr, "%d: protocol name missing\n", linenum);
			return NULL;
		}
		proto = *cpp++;
		if (!strcasecmp(proto, "tcp/udp")) {
			fil.fr_ip.fi_fl |= FI_TCPUDP;
			fil.fr_mip.fi_fl |= FI_TCPUDP;
		} else if (use_inet6 && !strcasecmp(proto, "icmp")) {
			fprintf(stderr,
"%d: use proto ipv6-icmp with IPv6 (or use proto 1 if you really mean icmp)\n",
				linenum);
		} else {
			if (!(p = getprotobyname(proto)) && !isdigit(*proto)) {
				fprintf(stderr,
					"%d: unknown protocol (%s)\n",
					linenum, proto);
				return NULL;
			}
			if (p)
				fil.fr_proto = p->p_proto;
			else if (isdigit(*proto)) {
				i = (int)strtol(proto, &endptr, 0);
				if (*endptr != '\0' || i < 0 || i > 255) {
					fprintf(stderr,
						"%d: unknown protocol (%s)\n",
						linenum, proto);
					return NULL;		
				}
				fil.fr_proto = i;
			}
			fil.fr_mip.fi_p = 0xff;
		}
	}
	if ((fil.fr_proto != IPPROTO_TCP) &&
	    ((fil.fr_flags & FR_RETMASK) == FR_RETRST)) {
		fprintf(stderr, "%d: %s can only be used with TCP\n",
			linenum, "return-rst");
		return NULL;
	}

	/*
	 * get the from host and bit mask to use against packets
	 */

	if (!*cpp) {
		fprintf(stderr, "%d: missing source specification\n", linenum);
		return NULL;
	}
	if (!strcasecmp(*cpp, "all")) {
		cpp++;
		if (!*cpp)
			return &fil;
	} else {
		if (strcasecmp(*cpp, "from")) {
			fprintf(stderr, "%d: unexpected keyword (%s) - from\n",
				linenum, *cpp);
			return NULL;
		}
		if (!*++cpp) {
			fprintf(stderr, "%d: missing host after from\n",
				linenum);
			return NULL;
		}
		if (!strcmp(*cpp, "!")) {
			fil.fr_flags |= FR_NOTSRCIP;
			if (!*++cpp) {
				fprintf(stderr,
					"%d: missing host after from\n",
					linenum);
				return NULL;
			}
		} else if (**cpp == '!') {
			fil.fr_flags |= FR_NOTSRCIP;
			(*cpp)++;
		}
		ch = 0;
		if (hostmask(&cpp, (u_32_t *)&fil.fr_src,
			     (u_32_t *)&fil.fr_smsk, &fil.fr_sport, &ch,
			     &fil.fr_stop, linenum)) {
			return NULL;
		}

		if ((ch != 0) && (fil.fr_proto != IPPROTO_TCP) &&
		    (fil.fr_proto != IPPROTO_UDP) &&
		    !(fil.fr_ip.fi_fl & FI_TCPUDP)) {
			fprintf(stderr,
				"%d: cannot use port and neither tcp or udp\n",
				linenum);
			return NULL;
		}

		fil.fr_scmp = ch;
		if (!*cpp) {
			fprintf(stderr, "%d: missing to fields\n", linenum);
			return NULL;
		}

		/*
		 * do the same for the to field (destination host)
		 */
		if (strcasecmp(*cpp, "to")) {
			fprintf(stderr, "%d: unexpected keyword (%s) - to\n",
				linenum, *cpp);
			return NULL;
		}
		if (!*++cpp) {
			fprintf(stderr, "%d: missing host after to\n", linenum);
			return NULL;
		}
		ch = 0;
		if (!strcmp(*cpp, "!")) {
			fil.fr_flags |= FR_NOTDSTIP;
			if (!*++cpp) {
				fprintf(stderr,
					"%d: missing host after from\n",
					linenum);
				return NULL;
			}
		} else if (**cpp == '!') {
			fil.fr_flags |= FR_NOTDSTIP;
			(*cpp)++;
		}
		if (hostmask(&cpp, (u_32_t *)&fil.fr_dst,
			     (u_32_t *)&fil.fr_dmsk, &fil.fr_dport, &ch,
			     &fil.fr_dtop, linenum)) {
			return NULL;
		}
		if ((ch != 0) && (fil.fr_proto != IPPROTO_TCP) &&
		    (fil.fr_proto != IPPROTO_UDP) &&
		    !(fil.fr_ip.fi_fl & FI_TCPUDP)) {
			fprintf(stderr,
				"%d: cannot use port and neither tcp or udp\n",
				linenum);
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
		fprintf(stderr, "%d: port operation on non tcp/udp\n", linenum);
		return NULL;
	}
	if (fil.fr_icmp && fil.fr_proto != IPPROTO_ICMP) {
		fprintf(stderr, "%d: icmp comparisons on wrong protocol\n",
			linenum);
		return NULL;
	}

	if (!*cpp)
		return &fil;

	if (*cpp && !strcasecmp(*cpp, "flags")) {
		if (!*++cpp) {
			fprintf(stderr, "%d: no flags present\n", linenum);
			return NULL;
		}
		fil.fr_tcpf = tcp_flags(*cpp, &fil.fr_tcpfm, linenum);
		cpp++;
	}

	/*
	 * extras...
	 */
	if ((fil.fr_v == 4) && *cpp && (!strcasecmp(*cpp, "with") ||
	     !strcasecmp(*cpp, "and")))
		if (extras(&cpp, &fil, linenum))
			return NULL;

	/*
	 * icmp types for use with the icmp protocol
	 */
	if (*cpp && !strcasecmp(*cpp, "icmp-type")) {
		if (fil.fr_proto != IPPROTO_ICMP &&
		    fil.fr_proto != IPPROTO_ICMPV6) {
			fprintf(stderr,
				"%d: icmp with wrong protocol (%d)\n",
				linenum, fil.fr_proto);
			return NULL;
		}
		if (addicmp(&cpp, &fil, linenum))
			return NULL;
		fil.fr_icmp = htons(fil.fr_icmp);
		fil.fr_icmpm = htons(fil.fr_icmpm);
	}

	/*
	 * Keep something...
	 */
	while (*cpp && !strcasecmp(*cpp, "keep"))
		if (addkeep(&cpp, &fil, linenum))
			return NULL;

	/*
	 * This is here to enforce the old interface binding behaviour.
	 * That is, "on X" is equivalent to "<dir> on X <!dir>-via -,X"
	 */
	if (fil.fr_flags & FR_KEEPSTATE) {
		if (*fil.fr_ifnames[0] && !*fil.fr_ifnames[3]) {
			bcopy(fil.fr_ifnames[0], fil.fr_ifnames[3],
			      sizeof(fil.fr_ifnames[3]));
			strncpy(fil.fr_ifnames[2], "*",
				sizeof(fil.fr_ifnames[3]));
		}
	}

	/*
	 * head of a new group ?
	 */
	if (*cpp && !strcasecmp(*cpp, "head")) {
		if (fil.fr_skip != 0) {
			fprintf(stderr, "%d: cannot use skip with head\n",
				linenum);
			return NULL;
		}
		if (!*++cpp) {
			fprintf(stderr, "%d: head without group #\n", linenum);
			return NULL;
		}
		if (ratoui(*cpp, &k, 0, UINT_MAX))
			fil.fr_grhead = (u_32_t)k;
		else {
			fprintf(stderr, "%d: invalid group (%s)\n",
				linenum, *cpp);
			return NULL;
		}
		cpp++;
	}

	/*
	 * head of a new group ?
	 */
	if (*cpp && !strcasecmp(*cpp, "group")) {
		if (!*++cpp) {
			fprintf(stderr, "%d: group without group #\n",
				linenum);
			return NULL;
		}
		if (ratoui(*cpp, &k, 0, UINT_MAX))
			fil.fr_group = k;
		else {
			fprintf(stderr, "%d: invalid group (%s)\n",
				linenum, *cpp);
			return NULL;
		}
		cpp++;
	}

	/*
	 * leftovers...yuck
	 */
	if (*cpp && **cpp) {
		fprintf(stderr, "%d: unknown words at end: [", linenum);
		for (; *cpp; cpp++)
			fprintf(stderr, "%s ", *cpp);
		fprintf(stderr, "]\n");
		return NULL;
	}

	/*
	 * lazy users...
	 */
	if ((fil.fr_tcpf || fil.fr_tcpfm) && fil.fr_proto != IPPROTO_TCP) {
		fprintf(stderr, "%d: TCP protocol not specified\n", linenum);
		return NULL;
	}
	if (!(fil.fr_ip.fi_fl & FI_TCPUDP) && (fil.fr_proto != IPPROTO_TCP) &&
	    (fil.fr_proto != IPPROTO_UDP) && (fil.fr_dcmp || fil.fr_scmp)) {
		if (!fil.fr_proto) {
			fil.fr_ip.fi_fl |= FI_TCPUDP;
			fil.fr_mip.fi_fl |= FI_TCPUDP;
		} else {
			fprintf(stderr,
				"%d: port comparisons for non-TCP/UDP\n",
				linenum);
			return NULL;
		}
	}
/*
	if ((fil.fr_flags & FR_KEEPFRAG) &&
	    (!(fil.fr_ip.fi_fl & FI_FRAG) || !(fil.fr_ip.fi_fl & FI_FRAG))) {
		fprintf(stderr,
			"%d: must use 'with frags' with 'keep frags'\n",
			linenum);
		return NULL;
	}
*/
	return &fil;
}


int loglevel(cpp, facpri, linenum)
char **cpp;
u_int *facpri;
int linenum;
{
	int fac, pri;
	char *s;

	fac = 0;
	pri = 0;
	if (!*++cpp) {
		fprintf(stderr, "%d: %s\n", linenum,
			"missing identifier after level");
		return -1;
	}

	s = index(*cpp, '.');
	if (s) {
		*s++ = '\0';
		fac = fac_findname(*cpp);
		if (fac == -1) {
			fprintf(stderr, "%d: %s %s\n", linenum,
				"Unknown facility", *cpp);
			return -1;
		}
		pri = pri_findname(s);
		if (pri == -1) {
			fprintf(stderr, "%d: %s %s\n", linenum,
				"Unknown priority", s);
			return -1;
		}
	} else {
		pri = pri_findname(*cpp);
		if (pri == -1) {
			fprintf(stderr, "%d: %s %s\n", linenum,
				"Unknown priority", *cpp);
			return -1;
		}
	}
	*facpri = fac|pri;
	return 0;
}


int to_interface(fdp, to, linenum)
frdest_t *fdp;
char *to;
int linenum;
{
	char *s;

	s = index(to, ':');
	fdp->fd_ifp = NULL;
	if (s) {
		*s++ = '\0';
		if (hostnum((u_32_t *)&fdp->fd_ip, s, linenum) == -1)
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
	printf("%s %s%s", tag, fdp->fd_ifname,
		     (fdp->fd_ifp || (long)fdp->fd_ifp == -1) ? "" : "(!)");
#ifdef	USE_INET6
	if (use_inet6 && IP6_NOTZERO(&fdp->fd_ip6.in6)) {
		char ipv6addr[80];

		inet_ntop(AF_INET6, &fdp->fd_ip6, ipv6addr,
			  sizeof(fdp->fd_ip6));
		printf(":%s", ipv6addr);
	} else
#endif
	if (fdp->fd_ip.s_addr)
		printf(":%s", inet_ntoa(fdp->fd_ip));
	putchar(' ');
}


/*
 * deal with extra bits on end of the line
 */
int	extras(cp, fr, linenum)
char	***cp;
struct	frentry	*fr;
int     linenum;
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
	       !strcasecmp(**cp, "not") || !strncasecmp(**cp, "opt", 3) ||
	       !strncasecmp(**cp, "frag", 4) || !strcasecmp(**cp, "no") ||
	       !strcasecmp(**cp, "short"))) {
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
				fprintf(stderr,
					"%d: opt missing arguements\n",
					linenum);
				return -1;
			}
			(*cp)++;
			if (!(opts = optname(cp, &secmsk, linenum)))
				return -1;
			oflags = FI_OPTIONS;
		} else if (***cp == 's' || ***cp == 'S') {
			if (fr->fr_tcpf) {
				fprintf(stderr,
				"%d: short cannot be used with TCP flags\n",
					linenum);
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
		if (notopt) {
		  if (!secmsk) {
				fr->fr_mip.fi_optmsk |= opts;
		  } else {
				fr->fr_mip.fi_optmsk |= (opts & ~0x0100);
		  }
		} else {
				fr->fr_mip.fi_optmsk |= opts;
		}
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


u_32_t optname(cp, sp, linenum)
char ***cp;
u_short *sp;
int linenum;
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
			fprintf(stderr, "%d: unknown IP option name %s\n",
				linenum, s);
			return 0;
		}
		if (!strcasecmp(s, "sec-class"))
			sec = 1;
	}

	if (sec && !*(*cp + 1)) {
		fprintf(stderr, "%d: missing security level after sec-class\n",
			linenum);
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
				fprintf(stderr,
					"%d: no such security level: %s\n",
					linenum, s);
				return 0;
			}
		}
		if (smsk)
			*sp = smsk;
	}
	return msk;
}


#ifdef __STDC__
void optprint(u_short *sec, u_long optmsk, u_long optbits)
#else
void optprint(sec, optmsk, optbits)
u_short *sec;
u_long optmsk, optbits;
#endif
{
	u_short secmsk = sec[0], secbits = sec[1];
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
int addicmp(cp, fp, linenum)
char ***cp;
struct frentry	*fp;
int linenum;
{
	char	**t;
	int	i;

	(*cp)++;
	if (!**cp)
		return -1;

	if (isdigit(***cp)) {
		if (!ratoi(**cp, &i, 0, 255)) {
			fprintf(stderr,
				"%d: Invalid icmp-type (%s) specified\n",
				linenum, **cp);
			return -1;
		}
	} else if (fp->fr_proto == IPPROTO_ICMPV6) {
		fprintf(stderr, "%d: Unknown ICMPv6 type (%s) specified, %s",
			linenum, **cp, "(use numeric value instead\n");
		return -1;
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
			fprintf(stderr,
				"%d: Invalid icmp-type (%s) specified\n",
				linenum, **cp);
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
		if (!ratoi(**cp, &i, 0, 255)) {
			fprintf(stderr, 
				"%d: Invalid icmp code (%s) specified\n",
				linenum, **cp);
			return -1;
		}
	} else {
		i = icmpcode(**cp);
		if (i == -1) {
			fprintf(stderr, 
				"%d: Invalid icmp code (%s) specified\n",
				linenum, **cp);
			return -1;
		}
	}
	i &= 0xff;
	fp->fr_icmp |= (u_short)i;
	fp->fr_icmpm = (u_short)0xffff;
	(*cp)++;
	return 0;
}


#define	MAX_ICMPCODE	15

char	*icmpcodes[] = {
	"net-unr", "host-unr", "proto-unr", "port-unr", "needfrag",
	"srcfail", "net-unk", "host-unk", "isolate", "net-prohib",
	"host-prohib", "net-tos", "host-tos", "filter-prohib", "host-preced",
	"preced-cutoff", NULL };
/*
 * Return the number for the associated ICMP unreachable code.
 */
int icmpcode(str)
char *str;
{
	char	*s;
	int	i, len;

	if ((s = strrchr(str, ')')))
		*s = '\0';
	if (isdigit(*str)) {
		if (!ratoi(str, &i, 0, 255))
			return -1;
		else
			return i;
	}
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
int addkeep(cp, fp, linenum)
char ***cp;
struct frentry	*fp;
int linenum; 
{
	char *s;

	(*cp)++;
	if (!**cp) {
		fprintf(stderr, "%d: Missing keyword after keep\n",
			linenum);
		return -1;
	}

	if (strcasecmp(**cp, "state") == 0)
		fp->fr_flags |= FR_KEEPSTATE;
	else if (strncasecmp(**cp, "frag", 4) == 0)
		fp->fr_flags |= FR_KEEPFRAG;
	else if (strcasecmp(**cp, "state-age") == 0) {
		if (fp->fr_ip.fi_p == IPPROTO_TCP) {
			fprintf(stderr, "%d: cannot use state-age with tcp\n",
				linenum);
			return -1;
		}
		if ((fp->fr_flags & FR_KEEPSTATE) == 0) {
			fprintf(stderr, "%d: state-age with no 'keep state'\n",
				linenum);
			return -1;
		}
		(*cp)++;
		if (!**cp) {
			fprintf(stderr, "%d: state-age with no arg\n",
				linenum);
			return -1;
		}
		fp->fr_age[0] = atoi(**cp);
		s = index(**cp, '/');
		if (s != NULL) {
			s++;
			fp->fr_age[1] = atoi(s);
		} else
			fp->fr_age[1] = fp->fr_age[0];
	} else {
		fprintf(stderr, "%d: Unrecognised state keyword \"%s\"\n",
			linenum, **cp);
		return -1;
	}
	(*cp)++;
	return 0;
}


void printifname(format, name, ifp)
char *format, *name;
void *ifp;
{
	printf("%s%s", format, name);
	if ((ifp == NULL) && strcmp(name, "-") && strcmp(name, "*"))
		printf("(!)");
}


/*
 * print the filter structure in a useful way
 */
void printfr(fp)
struct frentry	*fp;
{
	struct protoent	*p;
	u_short	sec[2];
	char *s;
	u_char *t;
	int pr;

	if (fp->fr_flags & FR_PASS)
		printf("pass");
	if (fp->fr_flags & FR_NOMATCH)
		printf("nomatch");
	else if (fp->fr_flags & FR_BLOCK) {
		printf("block");
		if (fp->fr_flags & FR_RETICMP) {
			if ((fp->fr_flags & FR_RETMASK) == FR_FAKEICMP)
				printf(" return-icmp-as-dest");
			else if ((fp->fr_flags & FR_RETMASK) == FR_RETICMP)
				printf(" return-icmp");
			if (fp->fr_icode) {
				if (fp->fr_icode <= MAX_ICMPCODE)
					printf("(%s)",
						icmpcodes[(int)fp->fr_icode]);
				else
					printf("(%d)", fp->fr_icode);
			}
		} else if ((fp->fr_flags & FR_RETMASK) == FR_RETRST)
			printf(" return-rst");
	} else if ((fp->fr_flags & FR_LOGMASK) == FR_LOG) {
		printlog(fp);
	} else if (fp->fr_flags & FR_ACCOUNT)
		printf("count");
	else if (fp->fr_flags & FR_AUTH)
		printf("auth");
	else if (fp->fr_flags & FR_PREAUTH)
		printf("preauth");
	else if (fp->fr_skip)
		printf("skip %hu", fp->fr_skip);

	if (fp->fr_flags & FR_OUTQUE)
		printf(" out ");
	else
		printf(" in ");

	if (((fp->fr_flags & FR_LOGB) == FR_LOGB) ||
	    ((fp->fr_flags & FR_LOGP) == FR_LOGP)) {
		printlog(fp);
		putchar(' ');
	}

	if (fp->fr_flags & FR_QUICK)
		printf("quick ");

	if (*fp->fr_ifname) {
		printifname("on ", fp->fr_ifname, fp->fr_ifa);
		if (*fp->fr_ifnames[1] && strcmp(fp->fr_ifnames[1], "*"))
			printifname(",", fp->fr_ifnames[1], fp->fr_ifas[1]);
		putchar(' ');

		if (*fp->fr_dif.fd_ifname)
			print_toif("dup-to", &fp->fr_dif);
		if (*fp->fr_tif.fd_ifname)
			print_toif("to", &fp->fr_tif);
		if (fp->fr_flags & FR_FASTROUTE)
			printf("fastroute ");

		if ((*fp->fr_ifnames[2] && strcmp(fp->fr_ifnames[2], "*")) ||
		    (*fp->fr_ifnames[3] && strcmp(fp->fr_ifnames[3], "*"))) {
			if (fp->fr_flags & FR_OUTQUE)
				printf("in-via ");
			else
				printf("out-via ");

			if (*fp->fr_ifnames[2]) {
				printifname("", fp->fr_ifnames[2],
					    fp->fr_ifas[2]);
				putchar(',');
			}

			if (*fp->fr_ifnames[3])
				printifname("", fp->fr_ifnames[3],
					    fp->fr_ifas[3]);
			putchar(' ');
		}
	}

	if (fp->fr_mip.fi_tos)
		printf("tos %#x ", fp->fr_tos);
	if (fp->fr_mip.fi_ttl)
		printf("ttl %d ", fp->fr_ttl);
	if (fp->fr_ip.fi_fl & FI_TCPUDP) {
			printf("proto tcp/udp ");
			pr = -1;
	} else if ((pr = fp->fr_mip.fi_p)) {
		if ((p = getprotobynumber(fp->fr_proto)))
			printf("proto %s ", p->p_name);
		else
			printf("proto %d ", fp->fr_proto);
	}

	printf("from %s", fp->fr_flags & FR_NOTSRCIP ? "!" : "");
	printhostmask(fp->fr_v, (u_32_t *)&fp->fr_src.s_addr,
		      (u_32_t *)&fp->fr_smsk.s_addr);
	if (fp->fr_scmp)
		printportcmp(pr, &fp->fr_tuc.ftu_src);

	printf(" to %s", fp->fr_flags & FR_NOTDSTIP ? "!" : "");
	printhostmask(fp->fr_v, (u_32_t *)&fp->fr_dst.s_addr,
		      (u_32_t *)&fp->fr_dmsk.s_addr);
	if (fp->fr_dcmp)
		printportcmp(pr, &fp->fr_tuc.ftu_dst);

	if ((fp->fr_ip.fi_fl & ~FI_TCPUDP) ||
	    (fp->fr_mip.fi_fl & ~FI_TCPUDP) ||
	    fp->fr_ip.fi_optmsk || fp->fr_mip.fi_optmsk ||
	    fp->fr_ip.fi_secmsk || fp->fr_mip.fi_secmsk) {
		printf(" with");
		if (fp->fr_ip.fi_optmsk || fp->fr_mip.fi_optmsk ||
		    fp->fr_ip.fi_secmsk || fp->fr_mip.fi_secmsk) {
			sec[0] = fp->fr_mip.fi_secmsk;
			sec[1] = fp->fr_ip.fi_secmsk;
			optprint(sec,
				 fp->fr_mip.fi_optmsk, fp->fr_ip.fi_optmsk);
		} else if (fp->fr_mip.fi_fl & FI_OPTIONS) {
			if (!(fp->fr_ip.fi_fl & FI_OPTIONS))
				printf(" not");
			printf(" ipopt");
		}
		if (fp->fr_mip.fi_fl & FI_SHORT) {
			if (!(fp->fr_ip.fi_fl & FI_SHORT))
				printf(" not");
			printf(" short");
		}
		if (fp->fr_mip.fi_fl & FI_FRAG) {
			if (!(fp->fr_ip.fi_fl & FI_FRAG))
				printf(" not");
			printf(" frag");
		}
	}
	if (fp->fr_proto == IPPROTO_ICMP && fp->fr_icmpm != 0) {
		int	type = fp->fr_icmp, code;

		type = ntohs(fp->fr_icmp);
		code = type & 0xff;
		type /= 256;
		if (type < (sizeof(icmptypes) / sizeof(char *) - 1) &&
		    icmptypes[type])
			printf(" icmp-type %s", icmptypes[type]);
		else
			printf(" icmp-type %d", type);
		if (ntohs(fp->fr_icmpm) & 0xff)
			printf(" code %d", code);
	}
	if (fp->fr_proto == IPPROTO_ICMPV6 && fp->fr_icmpm != 0) {
		int	type = fp->fr_icmp, code;

		type = ntohs(fp->fr_icmp);
		code = type & 0xff;
		type /= 256;
		printf(" icmp-type %d", type);
		if (ntohs(fp->fr_icmpm) & 0xff)
			printf(" code %d", code);
	}
	if (fp->fr_proto == IPPROTO_TCP && (fp->fr_tcpf || fp->fr_tcpfm)) {
		printf(" flags ");
		if (fp->fr_tcpf & ~TCPF_ALL)
			printf("0x%x", fp->fr_tcpf);
		else
			for (s = flagset, t = flags; *s; s++, t++)
				if (fp->fr_tcpf & *t)
					(void)putchar(*s);
		if (fp->fr_tcpfm) {
			(void)putchar('/');
			if (fp->fr_tcpfm & ~TCPF_ALL)
				printf("0x%x", fp->fr_tcpfm);
			else
				for (s = flagset, t = flags; *s; s++, t++)
					if (fp->fr_tcpfm & *t)
						(void)putchar(*s);
		}
	}

	if (fp->fr_flags & FR_KEEPSTATE)
		printf(" keep state");
	if (fp->fr_flags & FR_KEEPFRAG)
		printf(" keep frags");
	if (fp->fr_age[0] != 0 || fp->fr_age[1]!= 0)
		printf(" state-age %u/%u", fp->fr_age[0], fp->fr_age[1]);
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
		printf("%02x ", *s);
		if (j == 16) {
			printf("\n");
			j = 0;
		}
	}
	putchar('\n');
	(void)fflush(stdout);
}


void printlog(fp)
frentry_t *fp;
{
	char *s, *u;

	printf("log");
	if (fp->fr_flags & FR_LOGBODY)
		printf(" body");
	if (fp->fr_flags & FR_LOGFIRST)
		printf(" first");
	if (fp->fr_flags & FR_LOGORBLOCK)
		printf(" or-block");
	if (fp->fr_loglevel != 0xffff) {
		printf(" level ");
		if (fp->fr_loglevel & LOG_FACMASK) {
			s = fac_toname(fp->fr_loglevel);
			if (s == NULL)
				s = "!!!";
		} else
			s = "";
		u = pri_toname(fp->fr_loglevel);
		if (u == NULL)
			u = "!!!";
		if (*s)
			printf("%s.%s", s, u);
		else
			printf("%s", u);
	}
}
