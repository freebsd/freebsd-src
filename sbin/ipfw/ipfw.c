/*
 * Copyright (c) 1996 Alex Nash
 * Copyright (c) 1996 Poul-Henning Kamp
 * Copyright (c) 1994 Ugen J.S.Antsilevich
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
 * $Id: ipfw.c,v 1.28 1996/06/29 01:28:19 alex Exp $
 *
 */

#include <stdio.h>
#include <ctype.h>
#include <err.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <limits.h>
#include <time.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip_fw.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

int 	lineno = -1;
char 		progname[BUFSIZ];		/* Program name for errors */

int 		s;				/* main RAW socket 	   */
int 		do_resolv=0;			/* Would try to resolv all */
int		do_acct=0;			/* Show packet/byte count  */
int		do_time=0;			/* Show time stamps        */

int
mask_bits(m_ad)
	struct in_addr m_ad;
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
print_port(port, comma, flg)
	u_short port,flg;
	const char *comma;
{
	int printed = 0;

	if (do_resolv) {
		struct servent *se;
		const char *protocol;

		switch (flg & IP_FW_F_KIND) {
			case IP_FW_F_TCP:
				protocol = "tcp";
				break;
			case IP_FW_F_UDP:
				protocol = "udp";
				break;
			default:
				protocol = NULL;
				break;
		}

		se = getservbyport(htons(port), protocol);

		if (se) {
			printf("%s%s", comma, se->s_name);
			printed = 1;
		}
	} 
	if (!printed)
		printf("%s%d",comma,port);
}

void
show_ipfw(chain)
	struct ip_fw *chain;
{
	char *comma;
	u_long adrt;
	struct hostent *he;
	int i,mb;

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
		case IP_FW_F_DIVERT:
			printf("divert %u", chain->fw_divert_port);
			break;
		case IP_FW_F_COUNT:
			printf("count");
			break;
		case IP_FW_F_DENY:
			if (chain->fw_flg & IP_FW_F_ICMPRPL)
				printf("reject");
			else
				printf("deny");
			break;
		default:
			errx(1, "impossible");
	}
   
	if (chain->fw_flg & IP_FW_F_PRN)
		printf(" log");

	switch (chain->fw_flg & IP_FW_F_KIND) {
		case IP_FW_F_ICMP:
			printf(" icmp ");
			break;
		case IP_FW_F_TCP:
			printf(" tcp ");
			break;
		case IP_FW_F_UDP:
			printf(" udp ");
			break;
		case IP_FW_F_ALL:
			printf(" all ");
			break;
		default:
			break;
	}

	printf("from ");

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

	comma = " ";
	for (i=0;i<chain->fw_nsp; i++ ) {
		print_port(chain->fw_pts[i], comma, chain->fw_flg);
		if (i==0 && (chain->fw_flg & IP_FW_F_SRNG))
			comma = "-";
		else
			comma = ",";
	}

	printf(" to ");

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

	comma = " ";
	for (i=0;i<chain->fw_ndp;i++) {
		print_port(chain->fw_pts[chain->fw_nsp+i], comma, chain->fw_flg);
		if (i==0 && (chain->fw_flg & IP_FW_F_DRNG))
			comma = "-";
		else
		    comma = ",";
	    }

	if ((chain->fw_flg & IP_FW_F_IN) && (chain->fw_flg & IP_FW_F_OUT))
		; 
	else if (chain->fw_flg & IP_FW_F_IN)
		printf(" in");
	else if (chain->fw_flg & IP_FW_F_OUT)
		printf(" out");

	if (chain->fw_flg&IP_FW_F_IFNAME && chain->fw_via_name[0]) {
		char ifnb[FW_IFNLEN+1];
		printf(" via ");
		strncpy(ifnb,chain->fw_via_name,FW_IFNLEN);
		ifnb[FW_IFNLEN]='\0';
		if (chain->fw_flg & IP_FW_F_IFUWILD)
			printf("%s*",ifnb);
		else 
			printf("%s%d",ifnb,chain->fw_via_unit);
	} else if (chain->fw_via_ip.s_addr) {
		printf(" via ");
		printf(inet_ntoa(chain->fw_via_ip));
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

void
show_usage(str)
	char	*str;
{
	if (str)
		fprintf(stderr,"%s: ERROR - %s\n",progname,str);
	fprintf(stderr,
"Usage:\n"
"\t%s [options]\n"
"\t\tflush\n"
"\t\tadd [number] rule\n"
"\t\tdelete number\n"
"\t\tlist [number]\n"
"\t\tzero [number]\n"
"\trule:\taction proto src dst extras...\n"
"\t\taction: {allow|deny|reject|count|divert port} [log]\n"
"\t\tproto: {ip|tcp|udp|icmp}}\n"
"\t\tsrc: from {any|ip[{/bits|:mask}]} [{port|port-port},[port],...]\n"
"\t\tdst: to {any|ip[{/bits|:mask}]} [{port|port-port},[port],...]\n"
"\textras:\n"
"\t\tfragment\n"
"\t\t{in|out|inout}\n"
"\t\tvia {ifname|ip}\n"
"\t\t{established|setup}\n"
"\t\ttcpflags [!]{syn|fin|rst|ack|psh|urg},...\n"
"\t\tipoptions [!]{ssrr|lsrr|rr|ts},...\n"
"\t\ticmptypes {type},...\n"
, progname
);

		
	fprintf(stderr,"See man %s(8) for proper usage.\n",progname);
	exit (1);
}

int
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

		if (lookup_host(*av,ipno) != 0)
			show_usage("ip number\n");
		switch (md) {
			case ':':
				if (!inet_aton(p,mask))
					show_usage("ip number\n");
				break;
			case '/':
				if (atoi(p) == 0) {
					mask->s_addr = 0;
				} else {
					mask->s_addr = htonl(0xffffffff << (32 - atoi(p)));
				}
				break;
			default:
				mask->s_addr = htonl(0xffffffff);
				break;
		}
		ipno->s_addr &= mask->s_addr;
		av++;
		ac--;
	}
	*acp = ac;
	*avp = av;
}

void
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
			show_usage("invalid tcp flag\n");
		p = q;
	}
}

void
fill_ipopt(set, reset, vp)
	u_char *set, *reset;
	char **vp;
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
	if (ac && isdigit(**av)) {
		rule.fw_number = atoi(*av); av++; ac--;
	}

	i = setsockopt(s, IPPROTO_IP, IP_FW_DEL, &rule, sizeof rule);
	if (i)
		err(1,"setsockopt(IP_FW_DEL)");
}

void
add(ac,av)
	int ac;
	char **av;
{
	struct ip_fw rule;
	int i;
	
	memset(&rule, 0, sizeof rule);

	av++; ac--;

	/* Rule number */
	if (ac && isdigit(**av)) {
		rule.fw_number = atoi(*av); av++; ac--;
	}

	/* Action */
	if (ac && (!strncmp(*av,"accept",strlen(*av))
		    || !strncmp(*av,"pass",strlen(*av))
		    || !strncmp(*av,"allow",strlen(*av))
		    || !strncmp(*av,"permit",strlen(*av)))) {
		rule.fw_flg |= IP_FW_F_ACCEPT; av++; ac--;
	} else if (ac && !strncmp(*av,"count",strlen(*av))) {
		rule.fw_flg |= IP_FW_F_COUNT; av++; ac--;
	} else if (ac && !strncmp(*av,"divert",strlen(*av))) {
		rule.fw_flg |= IP_FW_F_DIVERT; av++; ac--;
		if (!ac)
			show_usage("missing divert port");
		rule.fw_divert_port = strtoul(*av, NULL, 0); av++; ac--;
		if (rule.fw_divert_port == 0)
			show_usage("illegal divert port");
	} else if (ac && (!strncmp(*av,"deny",strlen(*av)))) {
		rule.fw_flg |= IP_FW_F_DENY; av++; ac--;
	} else if (ac && !strncmp(*av,"reject",strlen(*av))) {
		rule.fw_flg |= IP_FW_F_DENY|IP_FW_F_ICMPRPL; av++; ac--;
	} else {
		show_usage("missing/unrecognized action\n");
	}

	/* [log] */
	if (ac && !strncmp(*av,"log",strlen(*av))) {
		rule.fw_flg |= IP_FW_F_PRN; av++; ac--;
	}

	/* protocol */
	if (ac && !strncmp(*av,"ip",strlen(*av))) {
		rule.fw_flg |= IP_FW_F_ALL; av++; ac--;
	} else if (ac && !strncmp(*av,"all",strlen(*av))) {
		rule.fw_flg |= IP_FW_F_ALL; av++; ac--;
	} else if (ac && !strncmp(*av,"tcp",strlen(*av))) {
		rule.fw_flg |= IP_FW_F_TCP; av++; ac--;
	} else if (ac && !strncmp(*av,"udp",strlen(*av))) {
		rule.fw_flg |= IP_FW_F_UDP; av++; ac--;
	} else if (ac && !strncmp(*av,"icmp",strlen(*av))) {
		rule.fw_flg |= IP_FW_F_ICMP; av++; ac--;
	} else {
		show_usage("missing protocol\n");
	}

	/* from */
	if (ac && !strncmp(*av,"from",strlen(*av))) { av++; ac--; }
	else show_usage("missing ``from''\n");

	fill_ip(&rule.fw_src, &rule.fw_smsk, &ac, &av);

	if (ac && isdigit(**av)) {
		if (fill_port(&rule.fw_nsp, &rule.fw_pts, 0, *av))
			rule.fw_flg |= IP_FW_F_SRNG;
		av++; ac--;
	}

	/* to */
	if (ac && !strncmp(*av,"to",strlen(*av))) { av++; ac--; }
	else show_usage("missing ``to''\n");

	if (!ac) show_usage("Missing arguments\n");

	fill_ip(&rule.fw_dst, &rule.fw_dmsk, &ac, &av);

	if (ac && isdigit(**av)) {
		if (fill_port(&rule.fw_ndp, &rule.fw_pts, rule.fw_nsp, *av))
			rule.fw_flg |= IP_FW_F_DRNG;
		av++; ac--;
	}

	if ((rule.fw_flg & IP_FW_F_KIND) != IP_FW_F_TCP &&
		(rule.fw_flg & IP_FW_F_KIND) != IP_FW_F_UDP &&
		(rule.fw_nsp || rule.fw_ndp)) {
		show_usage("only TCP and UDP protocols are valid with port specifications");
	}

	while (ac) {
		if (ac && !strncmp(*av,"via",strlen(*av))) {
			if (rule.fw_via_ip.s_addr || (rule.fw_flg & IP_FW_F_IFNAME)) {
				show_usage("multiple 'via' options specified");
			}

			av++; ac--; 
			if (!isdigit(**av)) {
				char *q;

				strcpy(rule.fw_via_name, *av);
				for (q = rule.fw_via_name; *q && !isdigit(*q) && *q != '*'; q++)
					continue;
				if (*q == '*')
					rule.fw_flg = IP_FW_F_IFUWILD;
				else
					rule.fw_via_unit = atoi(q);
				*q = '\0';
				rule.fw_flg |= IP_FW_F_IFNAME;
			} else if (inet_aton(*av,&rule.fw_via_ip) == INADDR_NONE) {
				show_usage("bad IP# after via\n");
			}
			av++; ac--; 
			continue;
		}
		if (!strncmp(*av,"fragment",strlen(*av))) { 
			rule.fw_flg |= IP_FW_F_FRAG; av++; ac--; continue;
		}
		if (!strncmp(*av,"in",strlen(*av))) { 
			rule.fw_flg |= IP_FW_F_IN; av++; ac--; continue;
		}
		if (!strncmp(*av,"out",strlen(*av))) { 
			rule.fw_flg |= IP_FW_F_OUT; av++; ac--; continue;
		}
		if (ac > 1 && !strncmp(*av,"ipoptions",strlen(*av))) { 
			av++; ac--; 
			fill_ipopt(&rule.fw_ipopt, &rule.fw_ipnopt, av);
			av++; ac--; continue;
		}
		if ((rule.fw_flg & IP_FW_F_KIND) == IP_FW_F_TCP) {
			if (!strncmp(*av,"established",strlen(*av))) { 
				rule.fw_tcpf  |= IP_FW_TCPF_ESTAB;
				av++; ac--; continue;
			}
			if (!strncmp(*av,"setup",strlen(*av))) { 
				rule.fw_tcpf  |= IP_FW_TCPF_SYN;
				rule.fw_tcpnf  |= IP_FW_TCPF_ACK;
				av++; ac--; continue;
			}
			if (ac > 1 && !strncmp(*av,"tcpflags",strlen(*av))) { 
				av++; ac--; 
				fill_tcpflag(&rule.fw_tcpf, &rule.fw_tcpnf, av);
				av++; ac--; continue;
			}
		}
		if ((rule.fw_flg & IP_FW_F_KIND) == IP_FW_F_ICMP) {
			if (ac > 1 && !strncmp(*av,"icmptypes",strlen(*av))) {
				av++; ac--;
				fill_icmptypes(rule.fw_icmptypes, av, &rule.fw_flg);
				av++; ac--; continue;
			}
		}
		printf("%d %s\n",ac,*av);
		show_usage("Unknown argument\n");
	}

	show_ipfw(&rule);
	i = setsockopt(s, IPPROTO_IP, IP_FW_ADD, &rule, sizeof rule);
	if (i)
		err(1,"setsockopt(IP_FW_ADD)");
}

void
zero (ac, av)
	int ac;
	char **av;
{
	av++; ac--;

	if (!ac) {
		/* clear all entries */
		if (setsockopt(s,IPPROTO_IP,IP_FW_ZERO,NULL,0)<0) {
			fprintf(stderr,"%s: setsockopt failed.\n",progname);
			exit(1);
		} 
		printf("Accounting cleared.\n");
	} else {
		/* clear a specific entry */
		struct ip_fw rule;

		memset(&rule, 0, sizeof rule);

		/* Rule number */
		if (isdigit(**av)) {
			rule.fw_number = atoi(*av); av++; ac--;

			if (setsockopt(s, IPPROTO_IP, IP_FW_ZERO, &rule, sizeof rule))
				err(1, "setsockopt(Zero)");
			printf("Entry %d cleared\n", rule.fw_number);
		}
		else {
			show_usage("expected number");
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

	while ((ch = getopt(ac, av ,"atN")) != EOF)
	switch(ch) {
		case 'a':
			do_acct=1;
			break;
		case 't':
			do_time=1;
			break;
		case 'N':
	 		do_resolv=1;
			break;
		default:
			show_usage("Unrecognised switch");
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
		if (setsockopt(s,IPPROTO_IP,IP_FW_FLUSH,NULL,0)<0) {
			fprintf(stderr,"%s: setsockopt failed.\n",progname);
			exit(1);
		} 
		printf("Flushed all rules.\n");
	} else if (!strncmp(*av, "zero", strlen(*av))) {
		zero(ac,av);
	} else if (!strncmp(*av, "print", strlen(*av))) {
		list(--ac,++av);
	} else if (!strncmp(*av, "list", strlen(*av))) {
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
	char	buf[BUFSIZ];
	char	*args[MAX_ARGS];
	char	linename[10];
	int 	i;
	FILE	*f;

	strcpy(progname,*av);

	s = socket( AF_INET, SOCK_RAW, IPPROTO_RAW );
	if ( s < 0 ) {
		fprintf(stderr,"%s: Can't open raw socket.\n"
			"Must be root to use this program.\n",progname);
		exit(1);
	}

	setbuf(stdout,0);

	if (av[1] && !access(av[1], R_OK)) {
		lineno = 0;
		f = fopen(av[1], "r");
		while (fgets(buf, BUFSIZ, f)) {
			if (buf[strlen(buf)-1]=='\n')
				buf[strlen(buf)-1] = 0;

			lineno++;
			sprintf(linename, "Line %d", lineno);
			args[0] = linename;

			args[1] = buf;
			while(*args[1] == ' ')
				args[1]++;
			i = 2;
			while((args[i] = strchr(args[i-1],' '))) {
				*(args[i]++) = 0;
				while(*args[i] == ' ')
					args[i]++;
				i++;
			}
			if (*args[i-1] == 0)
				i--;
			args[i] = NULL;

			ipfw_main(i, args); 
		}
		fclose(f);
	} else
		ipfw_main(ac,av);
	return 0;
}
