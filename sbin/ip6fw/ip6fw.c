/*	$KAME: ip6fw.c,v 1.14 2003/10/02 19:36:25 itojun Exp $	*/

/*
 * Copyright (C) 1998, 1999, 2000 and 2001 WIDE Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Copyright (c) 1996 Alex Nash, Paul Traina, Poul-Henning Kamp
 * Copyright (c) 1994 Ugen J.S.Antsilevich
 *
 * Idea and grammar partially left from:
 * Copyright (c) 1993 Daniel Boulet
 *
 * Redistribution and use in source forms, with and without modification,
 * are permitted provided that this entire comment appears intact.
 *
 * Redistribution in binary form may occur without any restrictions.
 * Obviously, it would be nice if you gave credit where credit is due
 * but requiring it would be too onerous.
 *
 * This software is provided ``AS IS'' without any warranties of any kind.
 *
 * NEW command line interface for IP firewall facility
 *
 * $Id: ip6fw.c,v 1.1.2.2.2.2 1999/05/14 05:13:50 shin Exp $
 * $FreeBSD$
 */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/wait.h>

#include <ctype.h>
#include <err.h>
#include <limits.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sysexits.h>

#include <net/if.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip6.h>
#include <netinet/icmp6.h>
#include <netinet6/ip6_fw.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

int 		lineno = -1;

int 		s;				/* main RAW socket 	   */
int 		do_resolv=0;			/* Would try to resolv all */
int		do_acct=0;			/* Show packet/byte count  */
int		do_time=0;			/* Show time stamps        */
int		do_quiet=0;			/* Be quiet in add and flush  */
int		do_force=0;			/* Don't ask for confirmation */

struct icmpcode {
	int	code;
	char	*str;
};

static struct icmpcode icmp6codes[] = {
      { ICMP6_DST_UNREACH_NOROUTE,	"noroute" },
      { ICMP6_DST_UNREACH_ADMIN,	"admin" },
      { ICMP6_DST_UNREACH_NOTNEIGHBOR,	"notneighbor" },
      { ICMP6_DST_UNREACH_ADDR,		"addr" },
      { ICMP6_DST_UNREACH_NOPORT,	"noport" },
      { 0, NULL }
};

static char ntop_buf[INET6_ADDRSTRLEN];

static void show_usage(const char *fmt, ...);

static int
mask_bits(u_char *m_ad, int m_len)
{
	int h_num = 0,i;

	for (i = 0; i < m_len; i++, m_ad++) {
		if (*m_ad != 0xff)
			break;
		h_num += 8;
	}
	if (i < m_len) {
		switch (*m_ad) {
#define	MASKLEN(m, l)	case m: h_num += l; break
		MASKLEN(0xfe, 7);
		MASKLEN(0xfc, 6);
		MASKLEN(0xf8, 5);
		MASKLEN(0xf0, 4);
		MASKLEN(0xe0, 3);
		MASKLEN(0xc0, 2);
		MASKLEN(0x80, 1);
#undef	MASKLEN
		}
	}
	return h_num;
}

static int pl2m[9] = { 0x00, 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe, 0xff };

struct in6_addr *plen2mask(int n)
{
	static struct in6_addr ia;
	u_char	*p;
	int	i;

	memset(&ia, 0, sizeof(struct in6_addr));
	p = (u_char *)&ia;
	for (i = 0; i < 16; i++, p++, n -= 8) {
		if (n >= 8) {
			*p = 0xff;
			continue;
		}
		*p = pl2m[n];
		break;
	}
	return &ia;
}

void
print_port(prot, port, comma)
	u_char  prot;
	u_short port;
	const char *comma;
{
	struct servent *se;
	struct protoent *pe;
	const char *protocol;
	int printed = 0;

	if (do_resolv) {
		pe = getprotobynumber(prot);
		if (pe)
			protocol = pe->p_name;
		else
			protocol = NULL;

		se = getservbyport(htons(port), protocol);
		if (se) {
			printf("%s%s", comma, se->s_name);
			printed = 1;
		}
	}
	if (!printed)
		printf("%s%d",comma,port);
}

static void
print_iface(char *key, union ip6_fw_if *un, int byname)
{
	char ifnb[IP6FW_IFNLEN+1];

	if (byname) {
		strncpy(ifnb, un->fu_via_if.name, IP6FW_IFNLEN);
		ifnb[IP6FW_IFNLEN]='\0';
		if (un->fu_via_if.unit == -1)
			printf(" %s %s*", key, ifnb);
		else
			printf(" %s %s%d", key, ifnb, un->fu_via_if.unit);
	} else if (!IN6_IS_ADDR_UNSPECIFIED(&un->fu_via_ip6)) {
		printf(" %s %s", key, inet_ntop(AF_INET6,&un->fu_via_ip6,ntop_buf,sizeof(ntop_buf)));
	} else
		printf(" %s any", key);
}

static void
print_reject_code(int code)
{
	struct icmpcode *ic;

	for (ic = icmp6codes; ic->str; ic++)
		if (ic->code == code) {
			printf("%s", ic->str);
			return;
		}
	printf("%u", code);
}

static void
show_ip6fw(struct ip6_fw *chain)
{
	char *comma;
	struct hostent *he;
	struct protoent *pe;
	int i, mb;
	int nsp = IPV6_FW_GETNSRCP(chain);
	int ndp = IPV6_FW_GETNDSTP(chain);

	if (do_resolv)
		setservent(1/*stayopen*/);

	printf("%05u ", chain->fw_number);

	if (do_acct)
		printf("%10lu %10lu ",chain->fw_pcnt,chain->fw_bcnt);

	if (do_time)
	{
		if (chain->timestamp)
		{
			char timestr[30];
			time_t t = _long_to_time(chain->timestamp);

			strcpy(timestr, ctime(&t));
			*strchr(timestr, '\n') = '\0';
			printf("%s ", timestr);
		}
		else
			printf("                         ");
	}

	switch (chain->fw_flg & IPV6_FW_F_COMMAND)
	{
		case IPV6_FW_F_ACCEPT:
			printf("allow");
			break;
		case IPV6_FW_F_DENY:
			printf("deny");
			break;
		case IPV6_FW_F_COUNT:
			printf("count");
			break;
		case IPV6_FW_F_DIVERT:
			printf("divert %u", chain->fw_divert_port);
			break;
		case IPV6_FW_F_TEE:
			printf("tee %u", chain->fw_divert_port);
			break;
		case IPV6_FW_F_SKIPTO:
			printf("skipto %u", chain->fw_skipto_rule);
			break;
		case IPV6_FW_F_REJECT:
			if (chain->fw_reject_code == IPV6_FW_REJECT_RST)
				printf("reset");
			else {
				printf("unreach ");
				print_reject_code(chain->fw_reject_code);
			}
			break;
		default:
			errx(EX_OSERR, "impossible");
	}

	if (chain->fw_flg & IPV6_FW_F_PRN)
		printf(" log");

	pe = getprotobynumber(chain->fw_prot);
	if (pe)
		printf(" %s", pe->p_name);
	else
		printf(" %u", chain->fw_prot);

	printf(" from %s", chain->fw_flg & IPV6_FW_F_INVSRC ? "not " : "");

	mb=mask_bits((u_char *)&chain->fw_smsk,sizeof(chain->fw_smsk));
	if (mb==128 && do_resolv) {
		he=gethostbyaddr((char *)&(chain->fw_src),sizeof(chain->fw_src),AF_INET6);
		if (he==NULL) {
			printf(inet_ntop(AF_INET6,&chain->fw_src,ntop_buf,sizeof(ntop_buf)));
		} else
			printf("%s",he->h_name);
	} else {
		if (mb!=128) {
			if (mb == 0) {
				printf("any");
			} else {
				printf(inet_ntop(AF_INET6,&chain->fw_src,ntop_buf,sizeof(ntop_buf)));
				printf("/%d",mb);
			}
		} else
			printf(inet_ntop(AF_INET6,&chain->fw_src,ntop_buf,sizeof(ntop_buf)));
	}

	if (chain->fw_prot == IPPROTO_TCP || chain->fw_prot == IPPROTO_UDP) {
		comma = " ";
		for (i = 0; i < nsp; i++) {
			print_port(chain->fw_prot, chain->fw_pts[i], comma);
			if (i==0 && (chain->fw_flg & IPV6_FW_F_SRNG))
				comma = "-";
			else
				comma = ",";
		}
	}

	printf(" to %s", chain->fw_flg & IPV6_FW_F_INVDST ? "not " : "");

	mb=mask_bits((u_char *)&chain->fw_dmsk,sizeof(chain->fw_dmsk));
	if (mb==128 && do_resolv) {
		he=gethostbyaddr((char *)&(chain->fw_dst),sizeof(chain->fw_dst),AF_INET6);
		if (he==NULL) {
			printf(inet_ntop(AF_INET6,&chain->fw_dst,ntop_buf,sizeof(ntop_buf)));
		} else
			printf("%s",he->h_name);
	} else {
		if (mb!=128) {
			if (mb == 0) {
				printf("any");
			} else {
				printf(inet_ntop(AF_INET6,&chain->fw_dst,ntop_buf,sizeof(ntop_buf)));
				printf("/%d",mb);
			}
		} else
			printf(inet_ntop(AF_INET6,&chain->fw_dst,ntop_buf,sizeof(ntop_buf)));
	}

	if (chain->fw_prot == IPPROTO_TCP || chain->fw_prot == IPPROTO_UDP) {
		comma = " ";
		for (i = 0; i < ndp; i++) {
			print_port(chain->fw_prot, chain->fw_pts[nsp+i], comma);
			if (i==0 && (chain->fw_flg & IPV6_FW_F_DRNG))
				comma = "-";
			else
				comma = ",";
		}
	}

	/* Direction */
	if ((chain->fw_flg & IPV6_FW_F_IN) && !(chain->fw_flg & IPV6_FW_F_OUT))
		printf(" in");
	if (!(chain->fw_flg & IPV6_FW_F_IN) && (chain->fw_flg & IPV6_FW_F_OUT))
		printf(" out");

	/* Handle hack for "via" backwards compatibility */
	if ((chain->fw_flg & IF6_FW_F_VIAHACK) == IF6_FW_F_VIAHACK) {
		print_iface("via",
		    &chain->fw_in_if, chain->fw_flg & IPV6_FW_F_IIFNAME);
	} else {
		/* Receive interface specified */
		if (chain->fw_flg & IPV6_FW_F_IIFACE)
			print_iface("recv", &chain->fw_in_if,
			    chain->fw_flg & IPV6_FW_F_IIFNAME);
		/* Transmit interface specified */
		if (chain->fw_flg & IPV6_FW_F_OIFACE)
			print_iface("xmit", &chain->fw_out_if,
			    chain->fw_flg & IPV6_FW_F_OIFNAME);
	}

	if (chain->fw_flg & IPV6_FW_F_FRAG)
		printf(" frag");

	if (chain->fw_ip6opt || chain->fw_ip6nopt) {
		int 	_opt_printed = 0;
#define	PRINTOPT(x)	{if (_opt_printed) printf(",");\
			printf(x); _opt_printed = 1;}

		printf(" ip6opt ");
		if (chain->fw_ip6opt  & IPV6_FW_IP6OPT_HOPOPT) PRINTOPT("hopopt");
		if (chain->fw_ip6nopt & IPV6_FW_IP6OPT_HOPOPT) PRINTOPT("!hopopt");
		if (chain->fw_ip6opt  & IPV6_FW_IP6OPT_ROUTE)  PRINTOPT("route");
		if (chain->fw_ip6nopt & IPV6_FW_IP6OPT_ROUTE)  PRINTOPT("!route");
		if (chain->fw_ip6opt  & IPV6_FW_IP6OPT_FRAG)  PRINTOPT("frag");
		if (chain->fw_ip6nopt & IPV6_FW_IP6OPT_FRAG)  PRINTOPT("!frag");
		if (chain->fw_ip6opt  & IPV6_FW_IP6OPT_ESP)    PRINTOPT("esp");
		if (chain->fw_ip6nopt & IPV6_FW_IP6OPT_ESP)    PRINTOPT("!esp");
		if (chain->fw_ip6opt  & IPV6_FW_IP6OPT_AH)     PRINTOPT("ah");
		if (chain->fw_ip6nopt & IPV6_FW_IP6OPT_AH)     PRINTOPT("!ah");
		if (chain->fw_ip6opt  & IPV6_FW_IP6OPT_NONXT)  PRINTOPT("nonxt");
		if (chain->fw_ip6nopt & IPV6_FW_IP6OPT_NONXT)  PRINTOPT("!nonxt");
		if (chain->fw_ip6opt  & IPV6_FW_IP6OPT_OPTS)   PRINTOPT("opts");
		if (chain->fw_ip6nopt & IPV6_FW_IP6OPT_OPTS)   PRINTOPT("!opts");
	}

	if (chain->fw_ipflg & IPV6_FW_IF_TCPEST)
		printf(" established");
	else if (chain->fw_tcpf == IPV6_FW_TCPF_SYN &&
	    chain->fw_tcpnf == IPV6_FW_TCPF_ACK)
		printf(" setup");
	else if (chain->fw_tcpf || chain->fw_tcpnf) {
		int 	_flg_printed = 0;
#define	PRINTFLG(x)	{if (_flg_printed) printf(",");\
			printf(x); _flg_printed = 1;}

		printf(" tcpflg ");
		if (chain->fw_tcpf  & IPV6_FW_TCPF_FIN)  PRINTFLG("fin");
		if (chain->fw_tcpnf & IPV6_FW_TCPF_FIN)  PRINTFLG("!fin");
		if (chain->fw_tcpf  & IPV6_FW_TCPF_SYN)  PRINTFLG("syn");
		if (chain->fw_tcpnf & IPV6_FW_TCPF_SYN)  PRINTFLG("!syn");
		if (chain->fw_tcpf  & IPV6_FW_TCPF_RST)  PRINTFLG("rst");
		if (chain->fw_tcpnf & IPV6_FW_TCPF_RST)  PRINTFLG("!rst");
		if (chain->fw_tcpf  & IPV6_FW_TCPF_PSH)  PRINTFLG("psh");
		if (chain->fw_tcpnf & IPV6_FW_TCPF_PSH)  PRINTFLG("!psh");
		if (chain->fw_tcpf  & IPV6_FW_TCPF_ACK)  PRINTFLG("ack");
		if (chain->fw_tcpnf & IPV6_FW_TCPF_ACK)  PRINTFLG("!ack");
		if (chain->fw_tcpf  & IPV6_FW_TCPF_URG)  PRINTFLG("urg");
		if (chain->fw_tcpnf & IPV6_FW_TCPF_URG)  PRINTFLG("!urg");
	}
	if (chain->fw_flg & IPV6_FW_F_ICMPBIT) {
		int type_index;
		int first = 1;

		printf(" icmptype");

		for (type_index = 0; type_index < IPV6_FW_ICMPTYPES_DIM * sizeof(unsigned) * 8; ++type_index)
			if (chain->fw_icmp6types[type_index / (sizeof(unsigned) * 8)] &
				(1U << (type_index % (sizeof(unsigned) * 8)))) {
				printf("%c%d", first == 1 ? ' ' : ',', type_index);
				first = 0;
			}
	}
	printf("\n");
	if (do_resolv)
		endservent();
}

void
list(ac, av)
	int	ac;
	char 	**av;
{
	struct ip6_fw *r, *rules, *n;
	int l,i;
	unsigned long rulenum;
	int nalloc, bytes, maxbytes;

	/* extract rules from kernel, resizing array as necessary */
	rules = NULL;
	nalloc = sizeof *rules;
	bytes = nalloc;
	maxbytes = 65536 * sizeof *rules;
	while (bytes >= nalloc) {
		if ((n = realloc(rules, nalloc * 2 + 200)) == NULL)
			err(EX_OSERR, "realloc");
		bytes = nalloc = nalloc * 2 + 200;
		rules = n;
		i = getsockopt(s, IPPROTO_IPV6, IPV6_FW_GET, rules, &bytes);
		if ((i < 0 && errno != EINVAL) || nalloc > maxbytes)
			err(EX_OSERR, "getsockopt(IPV6_FW_GET)");
	}
	if (!ac) {
		/* display all rules */
		for (r = rules, l = bytes; l >= sizeof rules[0];
			 r++, l-=sizeof rules[0])
			show_ip6fw(r);
	}
	else {
		/* display specific rules requested on command line */
		int exitval = 0;

		while (ac--) {
			char *endptr;
			int seen;

			/* convert command line rule # */
			rulenum = strtoul(*av++, &endptr, 10);
			if (*endptr) {
				exitval = 1;
				warn("invalid rule number: %s", av[-1]);
				continue;
			}
			seen = 0;
			for (r = rules, l = bytes;
				 l >= sizeof rules[0] && r->fw_number <= rulenum;
				 r++, l-=sizeof rules[0])
				if (rulenum == r->fw_number) {
					show_ip6fw(r);
					seen = 1;
				}
			if (!seen) {
				exitval = 1;
				warnx("rule %lu does not exist", rulenum);
			}
		}
		if (exitval != 0)
			exit(exitval);
	}
}

static void
show_usage(const char *fmt, ...)
{
	if (fmt) {
		char buf[100];
		va_list args;

		va_start(args, fmt);
		vsnprintf(buf, sizeof(buf), fmt, args);
		va_end(args);
		warnx("error: %s", buf);
	}
	fprintf(stderr, "usage: ip6fw [options]\n"
"    flush\n"
"    add [number] rule\n"
"    delete number ...\n"
"    list [number ...]\n"
"    show [number ...]\n"
"    zero [number ...]\n"
"  rule:  action proto src dst extras...\n"
"    action:\n"
"      {allow|permit|accept|pass|deny|drop|reject|unreach code|\n"
"       reset|count|skipto num} [log]\n"
"    proto: {ipv6|tcp|udp|ipv6-icmp|<number>}\n"
"    src: from [not] {any|ipv6[/prefixlen]} [{port|port-port},[port],...]\n"
"    dst: to [not] {any|ipv6[/prefixlen]} [{port|port-port},[port],...]\n"
"  extras:\n"
"    fragment     (may not be used with ports or tcpflags)\n"
"    in\n"
"    out\n"
"    {xmit|recv|via} {iface|ipv6|any}\n"
"    {established|setup}\n"
"    tcpflags [!]{syn|fin|rst|ack|psh|urg},...\n"
"    ipv6options [!]{hopopt|route|frag|esp|ah|nonxt|opts},...\n"
"    icmptypes {type[,type]}...\n");

	exit(1);
}

static int
lookup_host (host, addr, family)
	char *host;
	u_char *addr;
{
	struct hostent *he;

	if (inet_pton(family, host, addr) != 1) {
		if ((he = gethostbyname2(host, family)) == NULL)
			return(-1);
		memcpy(addr, he->h_addr_list[0], he->h_length);
	}	
	return(0);
}

void
fill_ip6(ipno, mask, acp, avp)
	struct in6_addr *ipno, *mask;
	int *acp;
	char ***avp;
{
	int ac = *acp;
	char **av = *avp;
	char *p = 0, md = 0;
	int i;

	if (ac && !strncmp(*av,"any",strlen(*av))) {
		*ipno = *mask = in6addr_any; av++; ac--;
	} else {
		p = strchr(*av, '/');
		if (p) {
			md = *p;
			*p++ = '\0';
		}

		if (lookup_host(*av, ipno, AF_INET6) != 0)
			show_usage("hostname ``%s'' unknown", *av);
		switch (md) {
			case '/':
				if (atoi(p) == 0) {
					*mask = in6addr_any;
				} else if (atoi(p) > 128) {
					show_usage("bad width ``%s''", p);
				} else {
					*mask = *(plen2mask(atoi(p)));
				}
				break;
			default:
				*mask = *(plen2mask(128));
				break;
		}
		for (i = 0; i < sizeof(*ipno); i++)
			ipno->s6_addr[i] &= mask->s6_addr[i];
		av++;
		ac--;
	}
	*acp = ac;
	*avp = av;
}

static void
fill_reject_code6(u_short *codep, char *str)
{
	struct icmpcode *ic;
	u_long val;
	char *s;

	val = strtoul(str, &s, 0);
	if (s != str && *s == '\0' && val < 0x100) {
		*codep = val;
		return;
	}
	for (ic = icmp6codes; ic->str; ic++)
		if (!strcasecmp(str, ic->str)) {
			*codep = ic->code;
			return;
		}
	show_usage("unknown ICMP6 unreachable code ``%s''", str);
}

static void
add_port(cnt, ptr, off, port)
	u_short *cnt, *ptr, off, port;
{
	if (off + *cnt >= IPV6_FW_MAX_PORTS)
		errx(1, "too many ports (max is %d)", IPV6_FW_MAX_PORTS);
	ptr[off+*cnt] = port;
	(*cnt)++;
}

static int
lookup_port(const char *arg, int test, int nodash)
{
	int		val;
	char		*earg, buf[32];
	struct servent	*s;

	snprintf(buf, sizeof(buf), "%s", arg);
	buf[strcspn(arg, nodash ? "-," : ",")] = 0;
	val = (int) strtoul(buf, &earg, 0);
	if (!*buf || *earg) {
		setservent(1);
		if ((s = getservbyname(buf, NULL))) {
			val = htons(s->s_port);
		} else {
			if (!test) {
				errx(1, "unknown port ``%s''", arg);
			}
			val = -1;
		}
	} else {
		if (val < 0 || val > 0xffff) {
			if (!test) {
				errx(1, "port ``%s'' out of range", arg);
			}
			val = -1;
		}
	}
	return(val);
}

int
fill_port(cnt, ptr, off, arg)
	u_short *cnt, *ptr, off;
	char *arg;
{
	char *s;
	int initial_range = 0;

	s = arg + strcspn(arg, "-,");	/* first port name can't have a dash */
	if (*s == '-') {
		*s++ = '\0';
		if (strchr(arg, ','))
			errx(1, "port range must be first in list");
		add_port(cnt, ptr, off, *arg ? lookup_port(arg, 0, 0) : 0x0000);
		arg = s;
		s = strchr(arg,',');
		if (s)
			*s++ = '\0';
		add_port(cnt, ptr, off, *arg ? lookup_port(arg, 0, 0) : 0xffff);
		arg = s;
		initial_range = 1;
	}
	while (arg != NULL) {
		s = strchr(arg,',');
		if (s)
			*s++ = '\0';
		add_port(cnt, ptr, off, lookup_port(arg, 0, 0));
		arg = s;
	}
	return initial_range;
}

void
fill_tcpflag(set, reset, vp)
	u_char *set, *reset;
	char **vp;
{
	char *p = *vp,*q;
	u_char *d;

	while (p && *p) {
		struct tpcflags {
			char * name;
			u_char value;
		} flags[] = {
			{ "syn", IPV6_FW_TCPF_SYN },
			{ "fin", IPV6_FW_TCPF_FIN },
			{ "ack", IPV6_FW_TCPF_ACK },
			{ "psh", IPV6_FW_TCPF_PSH },
			{ "rst", IPV6_FW_TCPF_RST },
			{ "urg", IPV6_FW_TCPF_URG }
		};
		int i;

		if (*p == '!') {
			p++;
			d = reset;
		} else {
			d = set;
		}
		q = strchr(p, ',');
		if (q)
			*q++ = '\0';
		for (i = 0; i < sizeof(flags) / sizeof(flags[0]); ++i)
			if (!strncmp(p, flags[i].name, strlen(p))) {
				*d |= flags[i].value;
				break;
			}
		if (i == sizeof(flags) / sizeof(flags[0]))
			show_usage("invalid tcp flag ``%s''", p);
		p = q;
	}
}

static void
fill_ip6opt(u_char *set, u_char *reset, char **vp)
{
	char *p = *vp,*q;
	u_char *d;

	while (p && *p) {
		if (*p == '!') {
			p++;
			d = reset;
		} else {
			d = set;
		}
		q = strchr(p, ',');
		if (q)
			*q++ = '\0';
		if (!strncmp(p,"hopopt",strlen(p))) *d |= IPV6_FW_IP6OPT_HOPOPT;
		if (!strncmp(p,"route",strlen(p)))  *d |= IPV6_FW_IP6OPT_ROUTE;
		if (!strncmp(p,"frag",strlen(p)))   *d |= IPV6_FW_IP6OPT_FRAG;
		if (!strncmp(p,"esp",strlen(p)))    *d |= IPV6_FW_IP6OPT_ESP;
		if (!strncmp(p,"ah",strlen(p)))     *d |= IPV6_FW_IP6OPT_AH;
		if (!strncmp(p,"nonxt",strlen(p)))  *d |= IPV6_FW_IP6OPT_NONXT;
		if (!strncmp(p,"opts",strlen(p)))   *d |= IPV6_FW_IP6OPT_OPTS;
		p = q;
	}
}

void
fill_icmptypes(types, vp, fw_flg)
	u_long *types;
	char **vp;
	u_short *fw_flg;
{
	char *c = *vp;

	while (*c)
	{
		unsigned long icmptype;

		if ( *c == ',' )
			++c;

		icmptype = strtoul(c, &c, 0);

		if ( *c != ',' && *c != '\0' )
			show_usage("invalid ICMP6 type");

		if (icmptype >= IPV6_FW_ICMPTYPES_DIM * sizeof(unsigned) * 8)
			show_usage("ICMP6 type out of range");

		types[icmptype / (sizeof(unsigned) * 8)] |=
			1 << (icmptype % (sizeof(unsigned) * 8));
		*fw_flg |= IPV6_FW_F_ICMPBIT;
	}
}

void
delete(ac,av)
	int ac;
	char **av;
{
	struct ip6_fw rule;
	int i;
	int exitval = 0;

	memset(&rule, 0, sizeof rule);

	av++; ac--;

	/* Rule number */
	while (ac && isdigit(**av)) {
		rule.fw_number = atoi(*av); av++; ac--;
		i = setsockopt(s, IPPROTO_IPV6, IPV6_FW_DEL, &rule, sizeof rule);
		if (i) {
			exitval = 1;
			warn("rule %u: setsockopt(%s)", rule.fw_number, "IPV6_FW_DEL");
		}
	}
	if (exitval != 0)
		exit(exitval);
}

static void
verify_interface(union ip6_fw_if *ifu)
{
	struct ifreq ifr;

	/*
	 *	If a unit was specified, check for that exact interface.
	 *	If a wildcard was specified, check for unit 0.
	 */
	snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s%d",
			 ifu->fu_via_if.name,
			 ifu->fu_via_if.unit == -1 ? 0 : ifu->fu_via_if.unit);

	if (ioctl(s, SIOCGIFFLAGS, &ifr) < 0)
		warnx("warning: interface ``%s'' does not exist", ifr.ifr_name);
}

static void
fill_iface(char *which, union ip6_fw_if *ifu, int *byname, int ac, char *arg)
{
	if (!ac)
	    show_usage("missing argument for ``%s''", which);

	/* Parse the interface or address */
	if (!strcmp(arg, "any")) {
		ifu->fu_via_ip6 = in6addr_any;
		*byname = 0;
	} else if (!isdigit(*arg)) {
		char *q;

		*byname = 1;
		strncpy(ifu->fu_via_if.name, arg, sizeof(ifu->fu_via_if.name));
		ifu->fu_via_if.name[sizeof(ifu->fu_via_if.name) - 1] = '\0';
		for (q = ifu->fu_via_if.name;
		    *q && !isdigit(*q) && *q != '*'; q++)
			continue;
		ifu->fu_via_if.unit = (*q == '*') ? -1 : atoi(q);
		*q = '\0';
		verify_interface(ifu);
	} else if (inet_pton(AF_INET6, arg, &ifu->fu_via_ip6) != 1) {
		show_usage("bad ip6 address ``%s''", arg);
	} else
		*byname = 0;
}

static void
add(ac,av)
	int ac;
	char **av;
{
	struct ip6_fw rule;
	int i;
	u_char proto;
	struct protoent *pe;
	int saw_xmrc = 0, saw_via = 0;

	memset(&rule, 0, sizeof rule);

	av++; ac--;

	/* Rule number */
	if (ac && isdigit(**av)) {
		rule.fw_number = atoi(*av); av++; ac--;
	}

	/* Action */
	if (ac == 0)
		show_usage("missing action");
	if (!strncmp(*av,"accept",strlen(*av))
		    || !strncmp(*av,"pass",strlen(*av))
		    || !strncmp(*av,"allow",strlen(*av))
		    || !strncmp(*av,"permit",strlen(*av))) {
		rule.fw_flg |= IPV6_FW_F_ACCEPT; av++; ac--;
	} else if (!strncmp(*av,"count",strlen(*av))) {
		rule.fw_flg |= IPV6_FW_F_COUNT; av++; ac--;
	}
#if 0
	else if (!strncmp(*av,"divert",strlen(*av))) {
		rule.fw_flg |= IPV6_FW_F_DIVERT; av++; ac--;
		if (!ac)
			show_usage("missing divert port");
		rule.fw_divert_port = strtoul(*av, NULL, 0); av++; ac--;
		if (rule.fw_divert_port == 0) {
			struct servent *s;
			setservent(1);
			s = getservbyname(av[-1], "divert");
			if (s != NULL)
				rule.fw_divert_port = ntohs(s->s_port);
			else
				show_usage("illegal divert port");
		}
	} else if (!strncmp(*av,"tee",strlen(*av))) {
		rule.fw_flg |= IPV6_FW_F_TEE; av++; ac--;
		if (!ac)
			show_usage("missing divert port");
		rule.fw_divert_port = strtoul(*av, NULL, 0); av++; ac--;
		if (rule.fw_divert_port == 0) {
			struct servent *s;
			setservent(1);
			s = getservbyname(av[-1], "divert");
			if (s != NULL)
				rule.fw_divert_port = ntohs(s->s_port);
			else
				show_usage("illegal divert port");
		}
	}
#endif
	else if (!strncmp(*av,"skipto",strlen(*av))) {
		rule.fw_flg |= IPV6_FW_F_SKIPTO; av++; ac--;
		if (!ac)
			show_usage("missing skipto rule number");
		rule.fw_skipto_rule = strtoul(*av, NULL, 0); av++; ac--;
	} else if ((!strncmp(*av,"deny",strlen(*av))
		    || !strncmp(*av,"drop",strlen(*av)))) {
		rule.fw_flg |= IPV6_FW_F_DENY; av++; ac--;
	} else if (!strncmp(*av,"reject",strlen(*av))) {
		rule.fw_flg |= IPV6_FW_F_REJECT; av++; ac--;
		rule.fw_reject_code = ICMP6_DST_UNREACH_NOROUTE;
	} else if (!strncmp(*av,"reset",strlen(*av))) {
		rule.fw_flg |= IPV6_FW_F_REJECT; av++; ac--;
		rule.fw_reject_code = IPV6_FW_REJECT_RST;	/* check TCP later */
	} else if (!strncmp(*av,"unreach",strlen(*av))) {
		rule.fw_flg |= IPV6_FW_F_REJECT; av++; ac--;
		fill_reject_code6(&rule.fw_reject_code, *av); av++; ac--;
	} else {
		show_usage("invalid action ``%s''", *av);
	}

	/* [log] */
	if (ac && !strncmp(*av,"log",strlen(*av))) {
		rule.fw_flg |= IPV6_FW_F_PRN; av++; ac--;
	}

	/* protocol */
	if (ac == 0)
		show_usage("missing protocol");
	if ((proto = atoi(*av)) > 0) {
		rule.fw_prot = proto; av++; ac--;
	} else if (!strncmp(*av,"all",strlen(*av))) {
		rule.fw_prot = IPPROTO_IPV6; av++; ac--;
	} else if ((pe = getprotobyname(*av)) != NULL) {
		rule.fw_prot = pe->p_proto; av++; ac--;
	} else {
		show_usage("invalid protocol ``%s''", *av);
	}

	if (rule.fw_prot != IPPROTO_TCP
	    && (rule.fw_flg & IPV6_FW_F_COMMAND) == IPV6_FW_F_REJECT
	    && rule.fw_reject_code == IPV6_FW_REJECT_RST)
		show_usage("``reset'' is only valid for tcp packets");

	/* from */
	if (ac && !strncmp(*av,"from",strlen(*av))) { av++; ac--; }
	else
		show_usage("missing ``from''");

	if (ac && !strncmp(*av,"not",strlen(*av))) {
		rule.fw_flg |= IPV6_FW_F_INVSRC;
		av++; ac--;
	}
	if (!ac)
		show_usage("missing arguments");

	fill_ip6(&rule.fw_src, &rule.fw_smsk, &ac, &av);

	if (ac && (isdigit(**av) || lookup_port(*av, 1, 1) >= 0)) {
		u_short nports = 0;

		if (fill_port(&nports, rule.fw_pts, 0, *av))
			rule.fw_flg |= IPV6_FW_F_SRNG;
		IPV6_FW_SETNSRCP(&rule, nports);
		av++; ac--;
	}

	/* to */
	if (ac && !strncmp(*av,"to",strlen(*av))) { av++; ac--; }
	else
		show_usage("missing ``to''");

	if (ac && !strncmp(*av,"not",strlen(*av))) {
		rule.fw_flg |= IPV6_FW_F_INVDST;
		av++; ac--;
	}
	if (!ac)
		show_usage("missing arguments");

	fill_ip6(&rule.fw_dst, &rule.fw_dmsk, &ac, &av);

	if (ac && (isdigit(**av) || lookup_port(*av, 1, 1) >= 0)) {
		u_short	nports = 0;

		if (fill_port(&nports,
		    rule.fw_pts, IPV6_FW_GETNSRCP(&rule), *av))
			rule.fw_flg |= IPV6_FW_F_DRNG;
		IPV6_FW_SETNDSTP(&rule, nports);
		av++; ac--;
	}

	if ((rule.fw_prot != IPPROTO_TCP) && (rule.fw_prot != IPPROTO_UDP)
	    && (IPV6_FW_GETNSRCP(&rule) || IPV6_FW_GETNDSTP(&rule))) {
		show_usage("only TCP and UDP protocols are valid"
		    " with port specifications");
	}

	while (ac) {
		if (!strncmp(*av,"in",strlen(*av))) {
			rule.fw_flg |= IPV6_FW_F_IN;
			av++; ac--; continue;
		}
		if (!strncmp(*av,"out",strlen(*av))) {
			rule.fw_flg |= IPV6_FW_F_OUT;
			av++; ac--; continue;
		}
		if (ac && !strncmp(*av,"xmit",strlen(*av))) {
			union ip6_fw_if ifu;
			int byname;

			if (saw_via) {
badviacombo:
				show_usage("``via'' is incompatible"
				    " with ``xmit'' and ``recv''");
			}
			saw_xmrc = 1;
			av++; ac--;
			fill_iface("xmit", &ifu, &byname, ac, *av);
			rule.fw_out_if = ifu;
			rule.fw_flg |= IPV6_FW_F_OIFACE;
			if (byname)
				rule.fw_flg |= IPV6_FW_F_OIFNAME;
			av++; ac--; continue;
		}
		if (ac && !strncmp(*av,"recv",strlen(*av))) {
			union ip6_fw_if ifu;
			int byname;

			if (saw_via)
				goto badviacombo;
			saw_xmrc = 1;
			av++; ac--;
			fill_iface("recv", &ifu, &byname, ac, *av);
			rule.fw_in_if = ifu;
			rule.fw_flg |= IPV6_FW_F_IIFACE;
			if (byname)
				rule.fw_flg |= IPV6_FW_F_IIFNAME;
			av++; ac--; continue;
		}
		if (ac && !strncmp(*av,"via",strlen(*av))) {
			union ip6_fw_if ifu;
			int byname = 0;

			if (saw_xmrc)
				goto badviacombo;
			saw_via = 1;
			av++; ac--;
			fill_iface("via", &ifu, &byname, ac, *av);
			rule.fw_out_if = rule.fw_in_if = ifu;
			if (byname)
				rule.fw_flg |=
				    (IPV6_FW_F_IIFNAME | IPV6_FW_F_OIFNAME);
			av++; ac--; continue;
		}
		if (!strncmp(*av,"fragment",strlen(*av))) {
			rule.fw_flg |= IPV6_FW_F_FRAG;
			av++; ac--; continue;
		}
		if (!strncmp(*av,"ipv6options",strlen(*av))) {
			av++; ac--;
			if (!ac)
				show_usage("missing argument"
				    " for ``ipv6options''");
			fill_ip6opt(&rule.fw_ip6opt, &rule.fw_ip6nopt, av);
			av++; ac--; continue;
		}
		if (rule.fw_prot == IPPROTO_TCP) {
			if (!strncmp(*av,"established",strlen(*av))) {
				rule.fw_ipflg |= IPV6_FW_IF_TCPEST;
				av++; ac--; continue;
			}
			if (!strncmp(*av,"setup",strlen(*av))) {
				rule.fw_tcpf  |= IPV6_FW_TCPF_SYN;
				rule.fw_tcpnf  |= IPV6_FW_TCPF_ACK;
				av++; ac--; continue;
			}
			if (!strncmp(*av,"tcpflags",strlen(*av))) {
				av++; ac--;
				if (!ac)
					show_usage("missing argument"
					    " for ``tcpflags''");
				fill_tcpflag(&rule.fw_tcpf, &rule.fw_tcpnf, av);
				av++; ac--; continue;
			}
		}
		if (rule.fw_prot == IPPROTO_ICMPV6) {
			if (!strncmp(*av,"icmptypes",strlen(*av))) {
				av++; ac--;
				if (!ac)
					show_usage("missing argument"
					    " for ``icmptypes''");
				fill_icmptypes(rule.fw_icmp6types,
				    av, &rule.fw_flg);
				av++; ac--; continue;
			}
		}
		show_usage("unknown argument ``%s''", *av);
	}

	/* No direction specified -> do both directions */
	if (!(rule.fw_flg & (IPV6_FW_F_OUT|IPV6_FW_F_IN)))
		rule.fw_flg |= (IPV6_FW_F_OUT|IPV6_FW_F_IN);

	/* Sanity check interface check, but handle "via" case separately */
	if (saw_via) {
		if (rule.fw_flg & IPV6_FW_F_IN)
			rule.fw_flg |= IPV6_FW_F_IIFACE;
		if (rule.fw_flg & IPV6_FW_F_OUT)
			rule.fw_flg |= IPV6_FW_F_OIFACE;
	} else if ((rule.fw_flg & IPV6_FW_F_OIFACE) && (rule.fw_flg & IPV6_FW_F_IN))
		show_usage("can't check xmit interface of incoming packets");

	/* frag may not be used in conjunction with ports or TCP flags */
	if (rule.fw_flg & IPV6_FW_F_FRAG) {
		if (rule.fw_tcpf || rule.fw_tcpnf)
			show_usage("can't mix 'frag' and tcpflags");

		if (rule.fw_nports)
			show_usage("can't mix 'frag' and port specifications");
	}

	if (!do_quiet)
		show_ip6fw(&rule);
	i = setsockopt(s, IPPROTO_IPV6, IPV6_FW_ADD, &rule, sizeof rule);
	if (i)
		err(EX_UNAVAILABLE, "setsockopt(%s)", "IPV6_FW_ADD");
}

static void
zero (ac, av)
	int ac;
	char **av;
{
	av++; ac--;

	if (!ac) {
		/* clear all entries */
		if (setsockopt(s,IPPROTO_IPV6,IPV6_FW_ZERO,NULL,0)<0)
			err(EX_UNAVAILABLE, "setsockopt(%s)", "IPV6_FW_ZERO");
		if (!do_quiet)
			printf("Accounting cleared.\n");
	} else {
		struct ip6_fw rule;
		int failed = 0;

		memset(&rule, 0, sizeof rule);
		while (ac) {
			/* Rule number */
			if (isdigit(**av)) {
				rule.fw_number = atoi(*av); av++; ac--;
				if (setsockopt(s, IPPROTO_IPV6,
				    IPV6_FW_ZERO, &rule, sizeof rule)) {
					warn("rule %u: setsockopt(%s)", rule.fw_number,
						 "IPV6_FW_ZERO");
					failed = 1;
				}
				else if (!do_quiet)
					printf("Entry %d cleared\n",
					    rule.fw_number);
			} else
				show_usage("invalid rule number ``%s''", *av);
		}
		if (failed != 0)
			exit(failed);
	}
}

int
ip6fw_main(ac,av)
	int 	ac;
	char 	**av;
{
	int 		ch;

	/* init optind to 1 */
	optind = 1;

	if ( ac == 1 ) {
		show_usage(NULL);
	}

	/* Set the force flag for non-interactive processes */
	do_force = !isatty(STDIN_FILENO);

	while ((ch = getopt(ac, av ,"afqtN")) != -1)
	switch(ch) {
		case 'a':
			do_acct=1;
			break;
		case 'f':
			do_force=1;
			break;
		case 'q':
			do_quiet=1;
			break;
		case 't':
			do_time=1;
			break;
		case 'N':
	 		do_resolv=1;
			break;
		default:
			show_usage(NULL);
	}

	ac -= optind;
	if (*(av+=optind)==NULL) {
		 show_usage("Bad arguments");
	}

	if (!strncmp(*av, "add", strlen(*av))) {
		add(ac,av);
	} else if (!strncmp(*av, "delete", strlen(*av))) {
		delete(ac,av);
	} else if (!strncmp(*av, "flush", strlen(*av))) {
		int do_flush = 0;

		if ( do_force || do_quiet )
			do_flush = 1;
		else {
			int c;

			/* Ask the user */
			printf("Are you sure? [yn] ");
			do {
                    		fflush(stdout);
				c = toupper(getc(stdin));
				while (c != '\n' && getc(stdin) != '\n')
					if (feof(stdin))
						return (0);
			} while (c != 'Y' && c != 'N');
			printf("\n");
			if (c == 'Y')
				do_flush = 1;
		}
		if ( do_flush ) {
			if (setsockopt(s,IPPROTO_IPV6,IPV6_FW_FLUSH,NULL,0) < 0)
				err(EX_UNAVAILABLE, "setsockopt(%s)", "IPV6_FW_FLUSH");
			if (!do_quiet)
				printf("Flushed all rules.\n");
		}
	} else if (!strncmp(*av, "zero", strlen(*av))) {
		zero(ac,av);
	} else if (!strncmp(*av, "print", strlen(*av))) {
		list(--ac,++av);
	} else if (!strncmp(*av, "list", strlen(*av))) {
		list(--ac,++av);
	} else if (!strncmp(*av, "show", strlen(*av))) {
		do_acct++;
		list(--ac,++av);
	} else {
		show_usage("Bad arguments");
	}
	return 0;
}

int
main(ac, av)
	int	ac;
	char	**av;
{
#define	MAX_ARGS	32
#define	WHITESP		" \t\f\v\n\r"
	char	buf[BUFSIZ];
	char	*a, *p, *args[MAX_ARGS], *cmd = NULL;
	char	linename[10];
	int 	i, c, lineno, qflag, pflag, status;
	FILE	*f = NULL;
	pid_t	preproc = 0;

	s = socket( AF_INET6, SOCK_RAW, IPPROTO_RAW );
	if ( s < 0 )
		err(EX_UNAVAILABLE, "socket");

	setbuf(stdout,0);

	/*
	 * Only interpret the last command line argument as a file to
	 * be preprocessed if it is specified as an absolute pathname.
	 */

	if (ac > 1 && av[ac - 1][0] == '/' && access(av[ac - 1], R_OK) == 0) {
		qflag = pflag = i = 0;
		lineno = 0;

		while ((c = getopt(ac, av, "D:U:p:q")) != -1)
			switch(c) {
			case 'D':
				if (!pflag)
					errx(EX_USAGE, "-D requires -p");
				if (i > MAX_ARGS - 2)
					errx(EX_USAGE,
					     "too many -D or -U options");
				args[i++] = "-D";
				args[i++] = optarg;
				break;

			case 'U':
				if (!pflag)
					errx(EX_USAGE, "-U requires -p");
				if (i > MAX_ARGS - 2)
					errx(EX_USAGE,
					     "too many -D or -U options");
				args[i++] = "-U";
				args[i++] = optarg;
				break;

			case 'p':
				pflag = 1;
				cmd = optarg;
				args[0] = cmd;
				i = 1;
				break;

			case 'q':
				qflag = 1;
				break;

			default:
				show_usage(NULL);
			}

		av += optind;
		ac -= optind;
		if (ac != 1)
			show_usage("extraneous filename arguments");

		if ((f = fopen(av[0], "r")) == NULL)
			err(EX_UNAVAILABLE, "fopen: %s", av[0]);

		if (pflag) {
			/* pipe through preprocessor (cpp or m4) */
			int pipedes[2];

			args[i] = 0;

			if (pipe(pipedes) == -1)
				err(EX_OSERR, "cannot create pipe");

			switch((preproc = fork())) {
			case -1:
				err(EX_OSERR, "cannot fork");

			case 0:
				/* child */
				if (dup2(fileno(f), 0) == -1 ||
				    dup2(pipedes[1], 1) == -1)
					err(EX_OSERR, "dup2()");
				fclose(f);
				close(pipedes[1]);
				close(pipedes[0]);
				execvp(cmd, args);
				err(EX_OSERR, "execvp(%s) failed", cmd);

			default:
				/* parent */
				fclose(f);
				close(pipedes[1]);
				if ((f = fdopen(pipedes[0], "r")) == NULL) {
					int savederrno = errno;

					(void)kill(preproc, SIGTERM);
					errno = savederrno;
					err(EX_OSERR, "fdopen()");
				}
			}
		}

		while (fgets(buf, BUFSIZ, f)) {
			lineno++;
			sprintf(linename, "Line %d", lineno);
			args[0] = linename;

			if (*buf == '#')
				continue;
			if ((p = strchr(buf, '#')) != NULL)
				*p = '\0';
			i=1;
			if (qflag) args[i++]="-q";
			for (a = strtok(buf, WHITESP);
			    a && i < MAX_ARGS; a = strtok(NULL, WHITESP), i++)
				args[i] = a;
			if (i == (qflag? 2: 1))
				continue;
			if (i == MAX_ARGS)
				errx(EX_USAGE, "%s: too many arguments", linename);
			args[i] = NULL;

			ip6fw_main(i, args);
		}
		fclose(f);
		if (pflag) {
			if (waitpid(preproc, &status, 0) != -1) {
				if (WIFEXITED(status)) {
					if (WEXITSTATUS(status) != EX_OK)
						errx(EX_UNAVAILABLE,
						     "preprocessor exited with status %d",
						     WEXITSTATUS(status));
				} else if (WIFSIGNALED(status)) {
					errx(EX_UNAVAILABLE,
					     "preprocessor exited with signal %d",
					     WTERMSIG(status));
				}
			}
		}
	} else
		ip6fw_main(ac,av);
	return 0;
}
