/*	$NetBSD$	*/

/*
 * Copyright (C) 1993-2001 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 */
#if !defined(lint)
static const char sccsid[] ="@(#)ipnat.c	1.9 6/5/96 (C) 1993 Darren Reed";
static const char rcsid[] = "@(#)Id: natparse.c,v 1.8.2.1 2004/12/09 19:41:21 darrenr Exp";
#endif

#include <sys/ioctl.h>
#include <errno.h>
#include <ctype.h>

#include "ipf.h"
#include "opts.h"


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
	char *dnetm = NULL, *dport = NULL, *proto = NULL;
	char *s, *t, *cps[31], **cpp;
	int i, cnt;


	if ((s = strchr(line, '\n')))
		*s = '\0';
	if ((s = strchr(line, '#')))
		*s = '\0';
	while (*line && ISSPACE(*line))
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

	strncpy(ipn.in_ifnames[0], *cpp, sizeof(ipn.in_ifnames[0]) - 1);
	ipn.in_ifnames[0][sizeof(ipn.in_ifnames[0]) - 1] = '\0';
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
			if (hostmask(&cpp, proto, NULL,
				     (u_32_t *)&ipn.in_srcip,
				     (u_32_t *)&ipn.in_srcmsk, linenum) == -1)
				return NULL;

			if (ports(&cpp, proto, &ipn.in_sport,
				  &ipn.in_scmp, &ipn.in_stop, linenum))
				return NULL;
		} else {
			if (hostmask(&cpp, proto, NULL,
				     (u_32_t *)&ipn.in_inip,
				     (u_32_t *)&ipn.in_inmsk, linenum) == -1)
				return NULL;

			if (ports(&cpp, proto, &ipn.in_dport,
				  &ipn.in_dcmp, &ipn.in_dtop, linenum))
				return NULL;
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
			if (hostmask(&cpp, proto, NULL,
				     (u_32_t *)&ipn.in_outip,
				     (u_32_t *)&ipn.in_outmsk, linenum))
					return NULL;

			if (ports(&cpp, proto, &ipn.in_dport,
				  &ipn.in_dcmp, &ipn.in_dtop, linenum))
				return NULL;
			ipn.in_pmin = htons(ipn.in_dport);
		} else {
			if (hostmask(&cpp, proto, NULL,
				     (u_32_t *)&ipn.in_srcip,
				     (u_32_t *)&ipn.in_srcmsk, linenum))
				return NULL;

			if (ports(&cpp, proto, &ipn.in_sport,
				  &ipn.in_scmp, &ipn.in_stop, linenum))
				return NULL;
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
			if (hostnum((u_32_t *)&ipn.in_outip, s, linenum, NULL))
				return NULL;
			if (genmask(t, (u_32_t *)&ipn.in_outmsk) == -1) {
				return NULL;
			}
		} else {
			if (hostnum((u_32_t *)&ipn.in_inip, s, linenum, NULL))
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

		if (ISDIGIT(**cpp) && (s = strchr(*cpp, '-')))
			*s++ = '\0';
		else
			s = NULL;

		if (!portnum(*cpp, proto, &ipn.in_pmin, linenum))
			return NULL;
		ipn.in_pmin = htons(ipn.in_pmin);
		cpp++;

		if (!strcmp(*cpp, "-")) {
			cpp++;
			s = *cpp++;
		}

		if (s) {
			if (!portnum(s, proto, &ipn.in_pmax, linenum))
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
		if (hostnum((u_32_t *)&ipn.in_inip, *cpp, linenum, NULL))
			return NULL;
	} else {
		if (hostnum((u_32_t *)&ipn.in_outip, *cpp, linenum, NULL))
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
		if (*cpp && strrchr(*cpp, '/') != NULL) {
			fprintf(stderr, "%d: No netmask supported in %s\n",
				linenum, "destination host for redirect");
			return NULL;
		}
		/* If it's a in_redir, expect target port */

		if (!*cpp || strcasecmp(*cpp, "port")) {
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
		if (!portnum(*cpp, proto, &ipn.in_pnext, linenum))
			return NULL;
		ipn.in_pnext = htons(ipn.in_pnext);
		cpp++;
	}
	if (dnetm && *dnetm == '/')
		*dnetm++ = '\0';

	if (ipn.in_redir & (NAT_MAP|NAT_MAPBLK)) {
		if (ipn.in_flags & IPN_IPRANGE) {
			if (hostnum((u_32_t *)&ipn.in_outmsk, dnetm,
				    linenum, NULL) == -1)
				return NULL;
		} else if (genmask(dnetm, (u_32_t *)&ipn.in_outmsk))
			return NULL;
	} else {
		if (ipn.in_flags & IPN_SPLIT) {
			if (hostnum((u_32_t *)&ipn.in_inmsk, dnetm,
				    linenum, NULL) == -1)
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
				ipn.in_p = getproto(*cpp);
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

			if (*cpp && !strcasecmp(*cpp, "age")) {
				cpp++;
				if (!*cpp) {
					fprintf(stderr,
						"%d: age with no parameters\n",
						linenum);
					return NULL;
				}

				ipn.in_age[0] = atoi(*cpp);
				s = strchr(*cpp, '/');
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
					return NULL;
				}
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
		ipn.in_flags |= IPN_ROUNDR;
	}

	if (!*cpp)
		return &ipn;

	if (ipn.in_redir != NAT_BIMAP && !strcasecmp(*cpp, "proxy")) {
		if (ipn.in_redir == NAT_BIMAP) {
			fprintf(stderr, "%d: cannot use proxy with bimap\n",
				linenum);
			return NULL;
		}

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

		if ((proto = strchr(*cpp, '/'))) {
			*proto++ = '\0';
			ipn.in_p = getproto(proto);
		} else
			ipn.in_p = 0;

		if (dport && !portnum(dport, proto, &ipn.in_dport, linenum))
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


	if (!strcasecmp(*cpp, "icmpidmap")) {

		cpp++;
		if (!*cpp) {
			fprintf(stderr,
				"%d: icmpidmap misses protocol and range\n",
				linenum);
			return NULL;
		};

		if (!strcasecmp(*cpp, "icmp"))
			ipn.in_flags = IPN_ICMPQUERY;
		else {
			fprintf(stderr, "%d: icmpidmap only valid for icmp\n",
				linenum);
			return NULL;
		}
		cpp++;

		if (!*cpp) {
			fprintf(stderr, "%d: no icmp id argument found\n",
				linenum);
			return NULL;
		}

		if (!(t = strchr(*cpp, ':'))) {
			fprintf(stderr,
				"%d: no icmp id range detected in \"%s\"\n",
				linenum, *cpp);
			return NULL;
		}
		*t++ = '\0';
		
		if (!icmpidnum(*cpp, &ipn.in_pmin, linenum) ||
		    !icmpidnum(t, &ipn.in_pmax, linenum))
			return NULL;
	} else if (!strcasecmp(*cpp, "portmap")) {
		if (ipn.in_redir == NAT_BIMAP) {
			fprintf(stderr, "%d: cannot use proxy with bimap\n",
				linenum);
			return NULL;
		}
		cpp++;
		if (!*cpp) {
			fprintf(stderr,
				"%d: missing expression following portmap\n",
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
		} else {
			 if (!(t = strchr(*cpp, ':'))) {
				fprintf(stderr,
					"%d: no port range in \"%s\"\n",
					linenum, *cpp);
				return NULL;
			}
			*t++ = '\0';
			if (!portnum(*cpp, proto, &ipn.in_pmin, linenum) ||
			    !portnum(t, proto, &ipn.in_pmax, linenum))
				return NULL;
		}
		cpp++;
	}

	if (*cpp && !strcasecmp(*cpp, "round-robin")) {
		cpp++;
		ipn.in_flags |= IPN_ROUNDR;
	}

	if (*cpp && !strcasecmp(*cpp, "age")) {
		cpp++;
		if (!*cpp) {
			fprintf(stderr, "%d: age with no parameters\n",
				linenum);
			return NULL;
		}
		s = strchr(*cpp, '/');
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
			return NULL;
		}
	}

	if (*cpp) {
		fprintf(stderr, "%d: extra junk at the end of the line: %s\n",
			linenum, *cpp);
		return NULL;
	}

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

	while (getline(line, sizeof(line) - 1, fp, &linenum)) {
		line[sizeof(line) - 1] = '\0';
		if ((s = strchr(line, '\n')))
			*s = '\0';

		if (!(np = natparse(line, linenum))) {
			if (*line)
				fprintf(stderr, "%d: syntax error in \"%s\"\n",
					linenum, line);
		} else {
			if ((opts & OPT_VERBOSE) && np)
				printnat(np, opts);
			if (!(opts & OPT_DONOTHING)) {
				if (!(opts & OPT_REMOVE)) {
					if (ioctl(fd, SIOCADNAT, &np) == -1)
						perror("ioctl(SIOCADNAT)");
				} else if (ioctl(fd, SIOCRMNAT, &np) == -1)
					perror("ioctl(SIOCRMNAT)");
			}
		}
	}
	if (fp != stdin)
		fclose(fp);
}


int	icmpidnum(str, id, linenum)
char	*str;
u_short *id;
int     linenum;
{
	int i;

	
	i = atoi(str);

	if ((i<0) || (i>65535)) {
		fprintf(stderr, "%d: invalid icmp id\"%s\".\n", linenum, str);
		return 0;
	}
	
	*id = (u_short)i;

	return 1;
}
