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
 * $Id: ipfw.c,v 1.34.2.3 1997/03/05 12:30:08 bde Exp $
 *
 */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/time.h>

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

#include <net/if.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip_var.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip_fw.h>
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

static struct icmpcode icmpcodes[] = {
      { ICMP_UNREACH_NET,		"net" },
      { ICMP_UNREACH_HOST,		"host" },
      { ICMP_UNREACH_PROTOCOL,		"protocol" },
      { ICMP_UNREACH_PORT,		"port" },
      { ICMP_UNREACH_NEEDFRAG,		"needfrag" },
      { ICMP_UNREACH_SRCFAIL,		"srcfail" },
      { ICMP_UNREACH_NET_UNKNOWN,	"net-unknown" },
      { ICMP_UNREACH_HOST_UNKNOWN,	"host-unknown" },
      { ICMP_UNREACH_ISOLATED,		"isolated" },
      { ICMP_UNREACH_NET_PROHIB,	"net-prohib" },
      { ICMP_UNREACH_HOST_PROHIB,	"host-prohib" },
      { ICMP_UNREACH_TOSNET,		"tosnet" },
      { ICMP_UNREACH_TOSHOST,		"toshost" },
      { ICMP_UNREACH_FILTER_PROHIB,	"filter-prohib" },
      { ICMP_UNREACH_HOST_PRECEDENCE,	"host-precedence" },
      { ICMP_UNREACH_PRECEDENCE_CUTOFF,	"precedence-cutoff" },
      { 0, NULL }
};

static int
mask_bits(struct in_addr m_ad)
{
	int h_fnd=0,h_num=0,i;
	u_long mask;

	mask=ntohl(m_ad.s_addr);
	for (i=0;i<sizeof(u_long)*CHAR_BIT;i++) {
		if (mask & 1L) {
			h_fnd=1;
			h_num++;
		} else {
			if (h_fnd)
				return -1;
		}
		mask=mask>>1;
	}
	return h_num;
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
print_iface(char *key, union ip_fw_if *un, int byname)
{
	char ifnb[FW_IFNLEN+1];

	if (byname) {
		strncpy(ifnb, un->fu_via_if.name, FW_IFNLEN);
		ifnb[FW_IFNLEN]='\0';
		if (un->fu_via_if.unit == -1)
			printf(" %s %s*", key, ifnb);
		else 
			printf(" %s %s%d", key, ifnb, un->fu_via_if.unit);
	} else if (un->fu_via_ip.s_addr != 0) {
		printf(" %s %s", key, inet_ntoa(un->fu_via_ip));
	} else
		printf(" %s any", key);
}

static void
print_reject_code(int code)
{
	struct icmpcode *ic;

	for (ic = icmpcodes; ic->str; ic++)
		if (ic->code == code) {
			printf("%s", ic->str);
			return;
		}
	printf("%u", code);
}

static void
show_ipfw(struct ip_fw *chain)
{
	char *comma;
	u_long adrt;
	struct hostent *he;
	struct protoent *pe;
	int i, mb;
	int nsp = IP_FW_GETNSRCP(chain);
	int ndp = IP_FW_GETNDSTP(chain);

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

			strcpy(timestr, ctime((time_t *)&chain->timestamp));
			*strchr(timestr, '\n') = '\0';
			printf("%s ", timestr);
		}
		else
			printf("                         ");
	}

	switch (chain->fw_flg & IP_FW_F_COMMAND)
	{
		case IP_FW_F_ACCEPT:
			printf("allow");
			break;
		case IP_FW_F_DENY:
			printf("deny");
			break;
		case IP_FW_F_COUNT:
			printf("count");
			break;
		case IP_FW_F_DIVERT:
			printf("divert %u", chain->fw_divert_port);
			break;
		case IP_FW_F_TEE:
			printf("tee %u", chain->fw_divert_port);
			break;
		case IP_FW_F_SKIPTO:
			printf("skipto %u", chain->fw_skipto_rule);
			break;
		case IP_FW_F_REJECT:
			if (chain->fw_reject_code == IP_FW_REJECT_RST)
				printf("reset");
			else {
				printf("unreach ");
				print_reject_code(chain->fw_reject_code);
			}
			break;
		default:
			errx(1, "impossible");
	}
   
	if (chain->fw_flg & IP_FW_F_PRN)
		printf(" log");

	pe = getprotobynumber(chain->fw_prot);
	if (pe)
		printf(" %s", pe->p_name);
	else
		printf(" %u", chain->fw_prot);

	printf(" from %s", chain->fw_flg & IP_FW_F_INVSRC ? "not " : "");

	adrt=ntohl(chain->fw_smsk.s_addr);
	if (adrt==ULONG_MAX && do_resolv) {
		adrt=(chain->fw_src.s_addr);
		he=gethostbyaddr((char *)&adrt,sizeof(u_long),AF_INET);
		if (he==NULL) {
			printf(inet_ntoa(chain->fw_src));
		} else
			printf("%s",he->h_name);
	} else {
		if (adrt!=ULONG_MAX) {
			mb=mask_bits(chain->fw_smsk);
			if (mb == 0) {
				printf("any");
			} else {
				if (mb > 0) {
					printf(inet_ntoa(chain->fw_src));
					printf("/%d",mb);
				} else {
					printf(inet_ntoa(chain->fw_src));
					printf(":");
					printf(inet_ntoa(chain->fw_smsk));
				}
			}
		} else
			printf(inet_ntoa(chain->fw_src));
	}

	if (chain->fw_prot == IPPROTO_TCP || chain->fw_prot == IPPROTO_UDP) {
		comma = " ";
		for (i = 0; i < nsp; i++) {
			print_port(chain->fw_prot, chain->fw_pts[i], comma);
			if (i==0 && (chain->fw_flg & IP_FW_F_SRNG))
				comma = "-";
			else
				comma = ",";
		}
	}

	printf(" to %s", chain->fw_flg & IP_FW_F_INVDST ? "not " : "");

	adrt=ntohl(chain->fw_dmsk.s_addr);
	if (adrt==ULONG_MAX && do_resolv) {
		adrt=(chain->fw_dst.s_addr);
		he=gethostbyaddr((char *)&adrt,sizeof(u_long),AF_INET);
		if (he==NULL) {
			printf(inet_ntoa(chain->fw_dst));
		} else
			printf("%s",he->h_name);
	} else {
		if (adrt!=ULONG_MAX) {
			mb=mask_bits(chain->fw_dmsk);
			if (mb == 0) {
				printf("any");
			} else {
				if (mb > 0) {
					printf(inet_ntoa(chain->fw_dst));
					printf("/%d",mb);
				} else {
					printf(inet_ntoa(chain->fw_dst));
					printf(":");
					printf(inet_ntoa(chain->fw_dmsk));
				}
			}
		} else
			printf(inet_ntoa(chain->fw_dst));
	}

	if (chain->fw_prot == IPPROTO_TCP || chain->fw_prot == IPPROTO_UDP) {
		comma = " ";
		for (i = 0; i < ndp; i++) {
			print_port(chain->fw_prot, chain->fw_pts[nsp+i], comma);
			if (i==0 && (chain->fw_flg & IP_FW_F_DRNG))
				comma = "-";
			else
				comma = ",";
		}
	}

	/* Direction */
	if ((chain->fw_flg & IP_FW_F_IN) && !(chain->fw_flg & IP_FW_F_OUT))
		printf(" in");
	if (!(chain->fw_flg & IP_FW_F_IN) && (chain->fw_flg & IP_FW_F_OUT))
		printf(" out");

	/* Handle hack for "via" backwards compatibility */
	if ((chain->fw_flg & IF_FW_F_VIAHACK) == IF_FW_F_VIAHACK) {
		print_iface("via",
		    &chain->fw_in_if, chain->fw_flg & IP_FW_F_IIFNAME);
	} else {
		/* Receive interface specified */
		if (chain->fw_flg & IP_FW_F_IIFACE)
			print_iface("recv", &chain->fw_in_if,
			    chain->fw_flg & IP_FW_F_IIFNAME);
		/* Transmit interface specified */
		if (chain->fw_flg & IP_FW_F_OIFACE)
			print_iface("xmit", &chain->fw_out_if,
			    chain->fw_flg & IP_FW_F_OIFNAME);
	}

	if (chain->fw_flg & IP_FW_F_FRAG)
		printf(" frag");

	if (chain->fw_ipopt || chain->fw_ipnopt) {
		int 	_opt_printed = 0;
#define PRINTOPT(x)	{if (_opt_printed) printf(",");\
			printf(x); _opt_printed = 1;}

		printf(" ipopt ");
		if (chain->fw_ipopt  & IP_FW_IPOPT_SSRR) PRINTOPT("ssrr");
		if (chain->fw_ipnopt & IP_FW_IPOPT_SSRR) PRINTOPT("!ssrr");
		if (chain->fw_ipopt  & IP_FW_IPOPT_LSRR) PRINTOPT("lsrr");
		if (chain->fw_ipnopt & IP_FW_IPOPT_LSRR) PRINTOPT("!lsrr");
		if (chain->fw_ipopt  & IP_FW_IPOPT_RR)   PRINTOPT("rr");
		if (chain->fw_ipnopt & IP_FW_IPOPT_RR)   PRINTOPT("!rr");
		if (chain->fw_ipopt  & IP_FW_IPOPT_TS)   PRINTOPT("ts");
		if (chain->fw_ipnopt & IP_FW_IPOPT_TS)   PRINTOPT("!ts");
	} 

	if (chain->fw_tcpf & IP_FW_TCPF_ESTAB) 
		printf(" established");
	else if (chain->fw_tcpf == IP_FW_TCPF_SYN &&
	    chain->fw_tcpnf == IP_FW_TCPF_ACK)
		printf(" setup");
	else if (chain->fw_tcpf || chain->fw_tcpnf) {
		int 	_flg_printed = 0;
#define PRINTFLG(x)	{if (_flg_printed) printf(",");\
			printf(x); _flg_printed = 1;}

		printf(" tcpflg ");
		if (chain->fw_tcpf  & IP_FW_TCPF_FIN)  PRINTFLG("fin");
		if (chain->fw_tcpnf & IP_FW_TCPF_FIN)  PRINTFLG("!fin");
		if (chain->fw_tcpf  & IP_FW_TCPF_SYN)  PRINTFLG("syn");
		if (chain->fw_tcpnf & IP_FW_TCPF_SYN)  PRINTFLG("!syn");
		if (chain->fw_tcpf  & IP_FW_TCPF_RST)  PRINTFLG("rst");
		if (chain->fw_tcpnf & IP_FW_TCPF_RST)  PRINTFLG("!rst");
		if (chain->fw_tcpf  & IP_FW_TCPF_PSH)  PRINTFLG("psh");
		if (chain->fw_tcpnf & IP_FW_TCPF_PSH)  PRINTFLG("!psh");
		if (chain->fw_tcpf  & IP_FW_TCPF_ACK)  PRINTFLG("ack");
		if (chain->fw_tcpnf & IP_FW_TCPF_ACK)  PRINTFLG("!ack");
		if (chain->fw_tcpf  & IP_FW_TCPF_URG)  PRINTFLG("urg");
		if (chain->fw_tcpnf & IP_FW_TCPF_URG)  PRINTFLG("!urg");
	} 
	if (chain->fw_flg & IP_FW_F_ICMPBIT) {
		int type_index;
		int first = 1;

		printf(" icmptype");

		for (type_index = 0; type_index < 256; ++type_index)
			if (chain->fw_icmptypes[type_index / (sizeof(unsigned) * 8)] & 
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
	struct ip_fw *r;
	struct ip_fw rules[1024];
	int l,i;

	memset(rules,0,sizeof rules);
	l = sizeof rules;
	i = getsockopt(s, IPPROTO_IP, IP_FW_GET, rules, &l);
	if (i < 0)
		err(2,"getsockopt(IP_FW_GET)");
	for (r=rules; l >= sizeof rules[0]; r++, l-=sizeof rules[0])
		show_ipfw(r);
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
	fprintf(stderr, "usage: ipfw [options]\n"
"    flush\n"
"    add [number] rule\n"
"    delete number ...\n"
"    list [number]\n"
"    show [number]\n"
"    zero [number ...]\n"
"  rule:  action proto src dst extras...\n"
"    action:\n"
"      {allow|permit|accept|pass|deny|drop|reject|unreach code|\n"
"       reset|count|skipto num|divert port|tee port} [log]\n"
"    proto: {ip|tcp|udp|icmp|<number>}\n"
"    src: from [not] {any|ip[{/bits|:mask}]} [{port|port-port},[port],...]\n"
"    dst: to [not] {any|ip[{/bits|:mask}]} [{port|port-port},[port],...]\n"
"  extras:\n"
"    fragment\n"
"    in\n"
"    out\n"
"    {xmit|recv|via} {iface|ip|any}\n"
"    {established|setup}\n"
"    tcpflags [!]{syn|fin|rst|ack|psh|urg},...\n"
"    ipoptions [!]{ssrr|lsrr|rr|ts},...\n"
"    icmptypes {type[,type]}...\n");

	exit(1);
}

static int
lookup_host (host, ipaddr)
	char *host;
	struct in_addr *ipaddr;
{
	struct hostent *he = gethostbyname(host);

	if (!he)
		return(-1);

	*ipaddr = *(struct in_addr *)he->h_addr_list[0];

	return(0);
}

void
fill_ip(ipno, mask, acp, avp)
	struct in_addr *ipno, *mask;
	int *acp;
	char ***avp;
{
	int ac = *acp;
	char **av = *avp;
	char *p = 0, md = 0;

	if (ac && !strncmp(*av,"any",strlen(*av))) {
		ipno->s_addr = mask->s_addr = 0; av++; ac--;
	} else {
		p = strchr(*av, '/');
		if (!p) 
			p = strchr(*av, ':');
		if (p) {
			md = *p;
			*p++ = '\0'; 
		}

		if (lookup_host(*av, ipno) != 0)
			show_usage("hostname ``%s'' unknown", *av);
		switch (md) {
			case ':':
				if (!inet_aton(p,mask))
					show_usage("bad netmask ``%s''", p);
				break;
			case '/':
				if (atoi(p) == 0) {
					mask->s_addr = 0;
				} else if (atoi(p) > 32) {
					show_usage("bad width ``%s''", p);
				} else {
					mask->s_addr =
					    htonl(~0 << (32 - atoi(p)));
				}
				break;
			default:
				mask->s_addr = htonl(~0);
				break;
		}
		ipno->s_addr &= mask->s_addr;
		av++;
		ac--;
	}
	*acp = ac;
	*avp = av;
}

static void
fill_reject_code(u_short *codep, char *str)
{
	struct icmpcode *ic;
	u_long val;
	char *s;

	val = strtoul(str, &s, 0);
	if (s != str && *s == '\0' && val < 0x100) {
		*codep = val;
		return;
	}
	for (ic = icmpcodes; ic->str; ic++)
		if (!strcasecmp(str, ic->str)) {
			*codep = ic->code;
			return;
		}
	show_usage("unknown ICMP unreachable code ``%s''", str);
}

static void
add_port(cnt, ptr, off, port)
	u_short *cnt, *ptr, off, port;
{
	if (off + *cnt >= IP_FW_MAX_PORTS)
		errx(1, "too many ports (max is %d)", IP_FW_MAX_PORTS);
	ptr[off+*cnt] = port;
	(*cnt)++;
}

int
fill_port(cnt, ptr, off, arg)
	u_short *cnt, *ptr, off;
	char *arg;
{
	char *s;
	int initial_range = 0;

	s = strchr(arg,'-');
	if (s) {
		*s++ = '\0';
		if (strchr(arg, ','))
			errx(1, "port range must be first in list");
		add_port(cnt, ptr, off, *arg ? atoi(arg) : 0x0000);
		arg = s;
		s = strchr(arg,',');
		if (s)
			*s++ = '\0';
		add_port(cnt, ptr, off, *arg ? atoi(arg) : 0xffff);
		arg = s;
		initial_range = 1;
	}
	while (arg != NULL) {
		s = strchr(arg,',');
		if (s)
			*s++ = '\0';
		add_port(cnt, ptr, off, atoi(arg));
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
			{ "syn", IP_FW_TCPF_SYN },
			{ "fin", IP_FW_TCPF_FIN },
			{ "ack", IP_FW_TCPF_ACK },
			{ "psh", IP_FW_TCPF_PSH },
			{ "rst", IP_FW_TCPF_RST },
			{ "urg", IP_FW_TCPF_URG }
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
fill_ipopt(u_char *set, u_char *reset, char **vp)
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
		if (!strncmp(p,"ssrr",strlen(p))) *d |= IP_FW_IPOPT_SSRR;
		if (!strncmp(p,"lsrr",strlen(p))) *d |= IP_FW_IPOPT_LSRR;
		if (!strncmp(p,"rr",strlen(p)))   *d |= IP_FW_IPOPT_RR;
		if (!strncmp(p,"ts",strlen(p)))   *d |= IP_FW_IPOPT_TS;
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
			show_usage("invalid ICMP type");

		if (icmptype > 255)
			show_usage("ICMP types are between 0 and 255 inclusive");

		types[icmptype / (sizeof(unsigned) * 8)] |= 
			1 << (icmptype % (sizeof(unsigned) * 8));
		*fw_flg |= IP_FW_F_ICMPBIT;
	}
}

void
delete(ac,av)
	int ac;
	char **av;
{
	struct ip_fw rule;
	int i;
	
	memset(&rule, 0, sizeof rule);

	av++; ac--;

	/* Rule number */
	while (ac && isdigit(**av)) {
		rule.fw_number = atoi(*av); av++; ac--;
		i = setsockopt(s, IPPROTO_IP, IP_FW_DEL, &rule, sizeof rule);
		if (i)
			warn("setsockopt(%s)", "IP_FW_DEL");
	}
}

static void
verify_interface(union ip_fw_if *ifu)
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
fill_iface(char *which, union ip_fw_if *ifu, int *byname, int ac, char *arg)
{
	if (!ac)
	    show_usage("missing argument for ``%s''", which);

	/* Parse the interface or address */
	if (!strcmp(arg, "any")) {
		ifu->fu_via_ip.s_addr = 0;
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
	} else if (!inet_aton(arg, &ifu->fu_via_ip)) {
		show_usage("bad ip address ``%s''", arg);
	} else
		*byname = 0;
}

static void
add(ac,av)
	int ac;
	char **av;
{
	struct ip_fw rule;
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
		rule.fw_flg |= IP_FW_F_ACCEPT; av++; ac--;
	} else if (!strncmp(*av,"count",strlen(*av))) {
		rule.fw_flg |= IP_FW_F_COUNT; av++; ac--;
	} else if (!strncmp(*av,"divert",strlen(*av))) {
		rule.fw_flg |= IP_FW_F_DIVERT; av++; ac--;
		if (!ac)
			show_usage("missing divert port");
		rule.fw_divert_port = strtoul(*av, NULL, 0); av++; ac--;
		if (rule.fw_divert_port == 0)
			show_usage("illegal divert port");
	} else if (!strncmp(*av,"tee",strlen(*av))) {
		rule.fw_flg |= IP_FW_F_TEE; av++; ac--;
		if (!ac)
			show_usage("missing divert port");
		rule.fw_divert_port = strtoul(*av, NULL, 0); av++; ac--;
		if (rule.fw_divert_port == 0)
			show_usage("illegal divert port");
	} else if (!strncmp(*av,"skipto",strlen(*av))) {
		rule.fw_flg |= IP_FW_F_SKIPTO; av++; ac--;
		if (!ac)
			show_usage("missing skipto rule number");
		rule.fw_skipto_rule = strtoul(*av, NULL, 0); av++; ac--;
	} else if ((!strncmp(*av,"deny",strlen(*av))
		    || !strncmp(*av,"drop",strlen(*av)))) {
		rule.fw_flg |= IP_FW_F_DENY; av++; ac--;
	} else if (!strncmp(*av,"reject",strlen(*av))) {
		rule.fw_flg |= IP_FW_F_REJECT; av++; ac--;
		rule.fw_reject_code = ICMP_UNREACH_HOST;
	} else if (!strncmp(*av,"reset",strlen(*av))) {
		rule.fw_flg |= IP_FW_F_REJECT; av++; ac--;
		rule.fw_reject_code = IP_FW_REJECT_RST;	/* check TCP later */
	} else if (!strncmp(*av,"unreach",strlen(*av))) {
		rule.fw_flg |= IP_FW_F_REJECT; av++; ac--;
		fill_reject_code(&rule.fw_reject_code, *av); av++; ac--;
	} else {
		show_usage("invalid action ``%s''", *av);
	}

	/* [log] */
	if (ac && !strncmp(*av,"log",strlen(*av))) {
		rule.fw_flg |= IP_FW_F_PRN; av++; ac--;
	}

	/* protocol */
	if (ac == 0)
		show_usage("missing protocol");
	if ((proto = atoi(*av)) > 0) {
		rule.fw_prot = proto; av++; ac--;
	} else if (!strncmp(*av,"all",strlen(*av))) {
		rule.fw_prot = IPPROTO_IP; av++; ac--;
	} else if ((pe = getprotobyname(*av)) != NULL) {
		rule.fw_prot = pe->p_proto; av++; ac--;
	} else {
		show_usage("invalid protocol ``%s''", *av);
	}

	if (rule.fw_prot != IPPROTO_TCP
	    && (rule.fw_flg & IP_FW_F_COMMAND) == IP_FW_F_REJECT
	    && rule.fw_reject_code == IP_FW_REJECT_RST)
		show_usage("``reset'' is only valid for tcp packets");

	/* from */
	if (ac && !strncmp(*av,"from",strlen(*av))) { av++; ac--; }
	else
		show_usage("missing ``from''");

	if (ac && !strncmp(*av,"not",strlen(*av))) {
		rule.fw_flg |= IP_FW_F_INVSRC;
		av++; ac--;
	}
	if (!ac)
		show_usage("missing arguments");

	fill_ip(&rule.fw_src, &rule.fw_smsk, &ac, &av);

	if (ac && isdigit(**av)) {
		u_short nports = 0;

		if (fill_port(&nports, rule.fw_pts, 0, *av))
			rule.fw_flg |= IP_FW_F_SRNG;
		IP_FW_SETNSRCP(&rule, nports);
		av++; ac--;
	}

	/* to */
	if (ac && !strncmp(*av,"to",strlen(*av))) { av++; ac--; }
	else
		show_usage("missing ``to''");

	if (ac && !strncmp(*av,"not",strlen(*av))) {
		rule.fw_flg |= IP_FW_F_INVDST;
		av++; ac--;
	}
	if (!ac)
		show_usage("missing arguments");

	fill_ip(&rule.fw_dst, &rule.fw_dmsk, &ac, &av);

	if (ac && isdigit(**av)) {
		u_short	nports = 0;

		if (fill_port(&nports,
		    rule.fw_pts, IP_FW_GETNSRCP(&rule), *av))
			rule.fw_flg |= IP_FW_F_DRNG;
		IP_FW_SETNDSTP(&rule, nports);
		av++; ac--;
	}

	if ((rule.fw_prot != IPPROTO_TCP) && (rule.fw_prot != IPPROTO_UDP)
	    && (IP_FW_GETNSRCP(&rule) || IP_FW_GETNDSTP(&rule))) {
		show_usage("only TCP and UDP protocols are valid"
		    " with port specifications");
	}

	while (ac) {
		if (!strncmp(*av,"in",strlen(*av))) { 
			rule.fw_flg |= IP_FW_F_IN;
			av++; ac--; continue;
		}
		if (!strncmp(*av,"out",strlen(*av))) { 
			rule.fw_flg |= IP_FW_F_OUT;
			av++; ac--; continue;
		}
		if (ac && !strncmp(*av,"xmit",strlen(*av))) {
			union ip_fw_if ifu;
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
			rule.fw_flg |= IP_FW_F_OIFACE;
			if (byname)
				rule.fw_flg |= IP_FW_F_OIFNAME;
			av++; ac--; continue;
		}
		if (ac && !strncmp(*av,"recv",strlen(*av))) {
			union ip_fw_if ifu;
			int byname;

			if (saw_via)
				goto badviacombo;
			saw_xmrc = 1;
			av++; ac--; 
			fill_iface("recv", &ifu, &byname, ac, *av);
			rule.fw_in_if = ifu;
			rule.fw_flg |= IP_FW_F_IIFACE;
			if (byname)
				rule.fw_flg |= IP_FW_F_IIFNAME;
			av++; ac--; continue;
		}
		if (ac && !strncmp(*av,"via",strlen(*av))) {
			union ip_fw_if ifu;
			int byname = 0;

			if (saw_xmrc)
				goto badviacombo;
			saw_via = 1;
			av++; ac--; 
			fill_iface("via", &ifu, &byname, ac, *av);
			rule.fw_out_if = rule.fw_in_if = ifu;
			if (byname)
				rule.fw_flg |=
				    (IP_FW_F_IIFNAME | IP_FW_F_OIFNAME);
			av++; ac--; continue;
		}
		if (!strncmp(*av,"fragment",strlen(*av))) {
			rule.fw_flg |= IP_FW_F_FRAG;
			av++; ac--; continue;
		}
		if (!strncmp(*av,"ipoptions",strlen(*av))) { 
			av++; ac--; 
			if (!ac)
				show_usage("missing argument"
				    " for ``ipoptions''");
			fill_ipopt(&rule.fw_ipopt, &rule.fw_ipnopt, av);
			av++; ac--; continue;
		}
		if (rule.fw_prot == IPPROTO_TCP) {
			if (!strncmp(*av,"established",strlen(*av))) { 
				rule.fw_tcpf  |= IP_FW_TCPF_ESTAB;
				av++; ac--; continue;
			}
			if (!strncmp(*av,"setup",strlen(*av))) { 
				rule.fw_tcpf  |= IP_FW_TCPF_SYN;
				rule.fw_tcpnf  |= IP_FW_TCPF_ACK;
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
		if (rule.fw_prot == IPPROTO_ICMP) {
			if (!strncmp(*av,"icmptypes",strlen(*av))) {
				av++; ac--;
				if (!ac)
					show_usage("missing argument"
					    " for ``icmptypes''");
				fill_icmptypes(rule.fw_icmptypes,
				    av, &rule.fw_flg);
				av++; ac--; continue;
			}
		}
		show_usage("unknown argument ``%s''", *av);
	}

	/* No direction specified -> do both directions */
	if (!(rule.fw_flg & (IP_FW_F_OUT|IP_FW_F_IN)))
		rule.fw_flg |= (IP_FW_F_OUT|IP_FW_F_IN);

	/* Sanity check interface check, but handle "via" case separately */
	if (saw_via) {
		if (rule.fw_flg & IP_FW_F_IN)
			rule.fw_flg |= IP_FW_F_IIFACE;
		if (rule.fw_flg & IP_FW_F_OUT)
			rule.fw_flg |= IP_FW_F_OIFACE;
	} else if ((rule.fw_flg & IP_FW_F_OIFACE) && (rule.fw_flg & IP_FW_F_IN))
		show_usage("can't check xmit interface of incoming packets");

	if (!do_quiet)
		show_ipfw(&rule);
	i = setsockopt(s, IPPROTO_IP, IP_FW_ADD, &rule, sizeof rule);
	if (i)
		err(1, "setsockopt(%s)", "IP_FW_ADD");
}

static void
zero (ac, av)
	int ac;
	char **av;
{
	av++; ac--;

	if (!ac) {
		/* clear all entries */
		if (setsockopt(s,IPPROTO_IP,IP_FW_ZERO,NULL,0)<0)
			err(1, "setsockopt(%s)", "IP_FW_ZERO");
		if (!do_quiet)
			printf("Accounting cleared.\n");
	} else {
		struct ip_fw rule;

		memset(&rule, 0, sizeof rule);
		while (ac) {
			/* Rule number */
			if (isdigit(**av)) {
				rule.fw_number = atoi(*av); av++; ac--;
				if (setsockopt(s, IPPROTO_IP,
				    IP_FW_ZERO, &rule, sizeof rule))
					warn("setsockopt(%s)", "IP_FW_ZERO");
				else
					printf("Entry %d cleared\n",
					    rule.fw_number);
			} else
				show_usage("invalid rule number ``%s''", *av);
		}
	}
}

int
ipfw_main(ac,av)
	int 	ac;
	char 	**av;
{

	char 		ch;
	extern int 	optind;


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
			if (setsockopt(s,IPPROTO_IP,IP_FW_FLUSH,NULL,0) < 0)
				err(1, "setsockopt(%s)", "IP_FW_FLUSH");
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
#define MAX_ARGS	32
#define WHITESP		" \t\f\v\n\r"
	char	buf[BUFSIZ];
	char	*a, *args[MAX_ARGS];
	char	linename[10];
	int 	i;
	FILE	*f;

	s = socket( AF_INET, SOCK_RAW, IPPROTO_RAW );
	if ( s < 0 )
		err(1, "socket");

	setbuf(stdout,0);

	if (av[1] && !access(av[1], R_OK)) {
		lineno = 0;
		if ((f = fopen(av[1], "r")) == NULL)
			err(1, "fopen: %s", av[1]);
		while (fgets(buf, BUFSIZ, f)) {

			lineno++;
			sprintf(linename, "Line %d", lineno);
			args[0] = linename;

			for (i = 1, a = strtok(buf, WHITESP);
			    a && i < MAX_ARGS; a = strtok(NULL, WHITESP), i++)
				args[i] = a;
			if (i == MAX_ARGS)
				errx(1, "%s: too many arguments", linename);
			args[i] = NULL;

			ipfw_main(i, args); 
		}
		fclose(f);
	} else
		ipfw_main(ac,av);
	return 0;
}
