/*
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
 * $Id: ipfw.c,v 1.23 1996/04/03 13:49:10 phk Exp $
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
show_ipfw(chain)
	struct ip_fw *chain;
{
	char *comma;
	u_long adrt;
	struct hostent *he;
	int i,mb;


	printf("%05u ", chain->fw_number);

	if (do_acct) 
		printf("%10lu %10lu ",chain->fw_pcnt,chain->fw_bcnt);

	if (chain->fw_flg & IP_FW_F_ACCEPT)
		printf("allow");
	else if (chain->fw_flg & IP_FW_F_ICMPRPL)
		printf("reject");
	else if (chain->fw_flg & IP_FW_F_COUNT)
		printf("count");
	else
		printf("deny");
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
		printf("%s%d",comma,chain->fw_pts[i]);
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
		printf("%s%d",comma,chain->fw_pts[chain->fw_nsp+i]);
		if (i==0 && (chain->fw_flg & IP_FW_F_DRNG))
			comma = "-";
		else
		    comma = ",";
	    }

	if ((chain->fw_flg & IP_FW_F_IN) && (chain->fw_flg & IP_FW_F_OUT))
		; 
	else if (chain->fw_flg & IP_FW_F_IN)
		printf(" in ");
	else if (chain->fw_flg & IP_FW_F_OUT)
		printf(" out ");

	if (chain->fw_flg&IP_FW_F_IFNAME && chain->fw_via_name[0]) {
		char ifnb[FW_IFNLEN+1];
		printf(" via ");
		strncpy(ifnb,chain->fw_via_name,FW_IFNLEN);
		ifnb[FW_IFNLEN]='\0';
		printf("%s%d",ifnb,chain->fw_via_unit);
	} else if (chain->fw_via_ip.s_addr) {
		printf(" via ");
		printf(inet_ntoa(chain->fw_via_ip));
	}

	if (chain->fw_flg & IP_FW_F_FRAG)
		printf("frag ");

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
	printf("\n");
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
	printf("FireWall chain entries: %d %d\n",l,i);
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
"\t\taction: {allow|deny|reject|count} [log]\n"
"\t\tproto: {ip|tcp|udp|icmp}}\n"
"\t\tsrc: from {any|ip[{/bits|:mask}]} [{port|port-port},...]\n"
"\t\tdst: to {any|ip[{/bits|:mask}]} [{port|port-port},...]\n"
"\textras:\n"
"\t\tfragment\n"
"\t\t{in|out|inout}\n"
"\t\tvia {ifname|ip}\n"
"\t\t{established|setup}\n"
"\t\ttcpflags [!]{syn|fin|rst|ack|psh},...\n"
"\t\tipoptions [!]{ssrr|lsrr|rr|ts},...\n"
, progname
);

		
	fprintf(stderr,"See man %s(8) for proper usage.\n",progname);
	exit (1);
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

		if (!inet_aton(*av,ipno))
			show_usage("ip number\n");
		if (md == ':' && !inet_aton(p,mask))
			show_usage("ip number\n");
		else if (md == '/') 
			mask->s_addr = htonl(0xffffffff << (32 - atoi(p)));
		else 
			mask->s_addr = htonl(0xffffffff);
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
	char *s, *comma;
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
		if (*p == '!') {
			p++;
			d = reset;
		} else {
			d = set;
		}
		q = strchr(p, ',');
		if (q) 
			*q++ = '\0';
		if (!strncmp(p,"syn",strlen(p))) *d |= IP_FW_TCPF_SYN;
		if (!strncmp(p,"fin",strlen(p))) *d |= IP_FW_TCPF_FIN;
		if (!strncmp(p,"ack",strlen(p))) *d |= IP_FW_TCPF_ACK;
		if (!strncmp(p,"psh",strlen(p))) *d |= IP_FW_TCPF_PSH;
		if (!strncmp(p,"rst",strlen(p))) *d |= IP_FW_TCPF_RST;
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
	if (ac && !strncmp(*av,"accept",strlen(*av))) {
		rule.fw_flg |= IP_FW_F_ACCEPT; av++; ac--;
	} else if (ac && !strncmp(*av,"allow",strlen(*av))) {
		rule.fw_flg |= IP_FW_F_ACCEPT; av++; ac--;
	} else if (ac && !strncmp(*av,"pass",strlen(*av))) {
		rule.fw_flg |= IP_FW_F_ACCEPT; av++; ac--;
	} else if (ac && !strncmp(*av,"count",strlen(*av))) {
		rule.fw_flg |= IP_FW_F_COUNT; av++; ac--;
	} else if (ac && !strncmp(*av,"deny",strlen(*av))) {
		av++; ac--;
	} else if (ac && !strncmp(*av,"reject",strlen(*av))) {
		rule.fw_flg |= IP_FW_F_ICMPRPL; av++; ac--;
	} else {
		show_usage("missing action\n");
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

	if (ac && !strncmp(*av,"via",strlen(*av))) { 
		av++; ac--; 
		if (!isdigit(**av)) {
			char *q;

			strcpy(rule.fw_via_name, *av);
			for (q = rule.fw_via_name; *q && !isdigit(*q); q++)
				continue;
			rule.fw_via_unit = atoi(q);
			*q = '\0';
			rule.fw_flg |= IP_FW_F_IFNAME;
		} else if (inet_aton(*av,&rule.fw_via_ip) == INADDR_NONE) {
			show_usage("bad IP# after via\n");
		}
		av++; ac--; 
	}

	while (ac) {
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
		printf("%d %s\n",ac,*av);
		show_usage("Unknown argument\n");
	}

	show_ipfw(&rule);
	i = setsockopt(s, IPPROTO_IP, IP_FW_ADD, &rule, sizeof rule);
	if (i)
		err(1,"setsockopt(IP_FW_ADD)");
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

	while ((ch = getopt(ac, av ,"aN")) != EOF)
	switch(ch) {
		case 'a':
			do_acct=1;
			break;
		case 'N':
	 		do_resolv=1;
        		break;
        	case '?':
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
		if (setsockopt(s,IPPROTO_IP,IP_FW_ZERO,NULL,0)<0) {
			fprintf(stderr,"%s: setsockopt failed.\n",progname);
			exit(1);
		} 
		printf("Accounting cleared.\n");
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

	s = socket( AF_INET, SOCK_RAW, IPPROTO_RAW );
	if ( s < 0 ) {
		fprintf(stderr,"%s: Can't open raw socket.\n"
			"Must be root to use this programm. \n",progname);
		exit(1);
	}

	setbuf(stdout,0);

	strcpy(progname,*av);

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
