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
 */

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */


#include <sys/param.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <grp.h>
#include <limits.h>
#include <netdb.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <sysexits.h>

#include <net/if.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip_fw.h>
#include <net/route.h> /* def. of struct route */
#include <netinet/ip_dummynet.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

int		s,			/* main RAW socket */
		do_resolv,		/* Would try to resolve all */
		do_acct,		/* Show packet/byte count */
		do_time,		/* Show time stamps */
		do_quiet,		/* Be quiet in add and flush */
		do_force,		/* Don't ask for confirmation */
		do_pipe,		/* this cmd refers to a pipe */
		do_sort,		/* field to sort results (0 = no) */
		do_dynamic,		/* display dynamic rules */
		do_expired,		/* display expired dynamic rules */
		verbose;

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

/*
 * structure to hold flag names and associated values to be
 * set in the appropriate masks.
 * The last element has value=0 and the string is the error message.
 */
struct _flaglist {
	char * name;
	u_char value;
};

static struct _flaglist f_tcpflags[] = {
	{ "syn", TH_SYN },
	{ "fin", TH_FIN },
	{ "ack", TH_ACK },
	{ "psh", TH_PUSH },
	{ "rst", TH_RST },
	{ "urg", TH_URG },
	{ "tcp flag", 0 }
};

static struct _flaglist f_tcpopts[] = {
	{ "mss", IP_FW_TCPOPT_MSS },
	{ "window", IP_FW_TCPOPT_WINDOW },
	{ "sack", IP_FW_TCPOPT_SACK },
	{ "ts", IP_FW_TCPOPT_TS },
	{ "cc", IP_FW_TCPOPT_CC },
	{ "tcp option", 0 }
};

static struct _flaglist f_ipopts[] = {
	{ "ssrr", IP_FW_IPOPT_SSRR},
	{ "lsrr", IP_FW_IPOPT_LSRR},
	{ "rr", IP_FW_IPOPT_RR},
	{ "ts", IP_FW_IPOPT_TS},
	{ "ip option", 0 }
};

static struct _flaglist f_iptos[] = {
	{ "lowdelay", IPTOS_LOWDELAY},
	{ "throughput", IPTOS_THROUGHPUT},
	{ "reliability", IPTOS_RELIABILITY},
	{ "mincost", IPTOS_MINCOST},
	{ "congestion", IPTOS_CE},
#if 0 /* conflicting */
	{ "ecntransport", IPTOS_ECT},
#endif
	{ "ip tos option", 0},
};

/**
 * _s_x holds a string-int pair for various lookups. Same as _flaglist.
 */
struct _s_x {
	char *s;
	int x;
};

static struct _s_x limit_masks[] = {
	{"src-addr",	DYN_SRC_ADDR},
	{"src-port",	DYN_SRC_PORT},
	{"dst-addr",	DYN_DST_ADDR},
	{"dst-port",	DYN_DST_PORT},
	{NULL,		0} };

static struct _s_x ether_types[] = {
    /*
     * Note, we cannot use "-:&/" in the names because they are field
     * separators in the type specifications. Also, we use s = NULL as
     * end-delimiter, because a type of 0 can be legal.
     */
	{ "ip",		0x0800 },
	{ "ipv4",	0x0800 },
	{ "ipv6",	0x86dd },
	{ "arp",	0x0806 },
	{ "rarp",	0x8035 },
	{ "vlan",	0x8100 },
	{ "loop",	0x9000 },
	{ "trail",	0x1000 },
	{ "at",		0x809b },
	{ "atalk",	0x809b },
	{ "aarp",	0x80f3 },
	{ "pppoe_disc",	0x8863 },
	{ "pppoe_sess",	0x8864 },
	{ "ipx_8022",	0x00E0 },
	{ "ipx_8023",	0x0000 },
	{ "ipx_ii",	0x8137 },
	{ "ipx_snap",	0x8137 },
	{ "ipx",	0x8137 },
	{ "ns",		0x0600 },
	{ NULL,		0 }
};

	static void show_usage(void);

/*
 * print the arrays of ports. The first two entries can be
 * a range (a-b) or a port:mask pair, and they are processed
 * accordingly if one of the two arguments range,mask is set.
 * (they are not supposed to be both set).
 */
static void
print_ports(u_char prot, int n, u_short *ports, int range, int mask)
{
	int i=0;
	char comma = ' ';

	if (mask) {
		printf(" %04x:%04x", ports[0], ports[1]);
		i=2;
		comma = ',';
	}
	for (; i < n; i++) {
		struct servent *se = NULL;

		if (do_resolv) {
			struct protoent *pe = getprotobynumber(prot);
			se = getservbyport(htons(ports[i]),
				pe ? pe->p_name : NULL);
		}
		if (se)
			printf("%c%s", comma, se->s_name);
		else
			printf("%c%d", comma, ports[i]);
		if (i == 0 && range)
			comma = '-';
		else
			comma = ',';
	}
}

static void
print_iface(char *key, union ip_fw_if *un, int byname)
{
	char ifnb[FW_IFNLEN+1];

	if (byname) {
		strncpy(ifnb, un->fu_via_if.name, FW_IFNLEN);
		ifnb[FW_IFNLEN] = '\0';
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

/*
 * Returns the number of bits set (from left) in a contiguous bitmask,
 * or -1 if the mask is not contiguous.
 * This effectively works on masks in big-endian (network) format.
 * First bit is bit 7 of the first byte -- note, for MAC addresses,
 * the first bit on the wire is bit 0 of the first byte.
 * len is the max length in bits.
 */
static int
contigmask(u_char *p, int len)
{
    int i, n;
    for (i=0; i<len ; i++)
	if ( (p[i/8] & (1 << (7 - (i%8)))) == 0) /* first bit unset */
	    break;
    for (n=i+1; n < len; n++)
	if ( (p[n/8] & (1 << (7 - (n%8)))) != 0) /* mask is not contiguous */
	    return -1;
    return i;
}

/*
 * print options set/clear in the two bitmasks passed as parameters.
 */
static void
printopts(char *name, u_char set, u_char clear, struct _flaglist *list)
{
	char *comma="";
	int i;

	printf(" %s ", name);
	for (i=0; list[i].value != 0; i++) {
		if (set & list[i].value) {
			printf("%s%s", comma, list[i].name);
			comma = ",";
		}
		if (clear & list[i].value) {
			printf("%s!%s", comma, list[i].name);
			comma = ",";
		}
	}
}

static void
print_ip(struct in_addr addr, struct in_addr mask)
{
	struct hostent *he = NULL;
	int mb = contigmask((u_char *)&mask, 32);

	if (mb == 32 && do_resolv)
		he = gethostbyaddr((char *)&(addr.s_addr),
		    sizeof(u_long), AF_INET);
	if (he != NULL)		/* resolved to name */
		printf("%s", he->h_name);
	else if (mb == 0)	/* any */
		printf("any");
	else {		/* numeric IP followed by some kind of mask */
		printf("%s", inet_ntoa(addr));
		if (mb < 0)
			printf(":%s", inet_ntoa(mask));
		else if (mb < 32)
			printf("/%d", mb);
	}
}

/*
 * prints a MAC address/mask pair
 */
static void
print_mac(u_char *addr, u_char *mask)
{
	int l = contigmask(mask, 48);

	if (l == 0)
		printf(" any");
	else {
		printf(" %02x:%02x:%02x:%02x:%02x:%02x",
		    addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
		if (l == -1)
			printf("&%02x:%02x:%02x:%02x:%02x:%02x",
			    mask[0], mask[1], mask[2],
			    mask[3], mask[4], mask[5]);
		else if (l < 48)
			printf("/%d", l);
	}
}

static void
show_ipfw(struct ip_fw *chain)
{
	struct protoent *pe;
	int nsp = IP_FW_GETNSRCP(chain);
	int ndp = IP_FW_GETNDSTP(chain);

	if (do_resolv)
		setservent(1/*stay open*/);

	printf("%05u ", chain->fw_number);

	if (do_acct)
		printf("%10qu %10qu ", chain->fw_pcnt, chain->fw_bcnt);

	if (do_time) {
		if (chain->timestamp) {
			char timestr[30];
			time_t t = _long_to_time(chain->timestamp);

			strcpy(timestr, ctime(&t));
			*strchr(timestr, '\n') = '\0';
			printf("%s ", timestr);
		} else {
			printf("			 ");
		}
	}

	if (chain->fw_flg == IP_FW_F_CHECK_S) {
		printf("check-state\n");
		goto done;
	}

	if (chain->fw_flg & IP_FW_F_RND_MATCH) {
		double d = 1.0 * chain->dont_match_prob;
		d = 1 - (d / 0x7fffffff);
		printf("prob %f ", d);
	}

	switch (chain->fw_flg & IP_FW_F_COMMAND) {
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
	case IP_FW_F_PIPE:
		printf("pipe %u", chain->fw_skipto_rule);
		break;
	case IP_FW_F_QUEUE:
		printf("queue %u", chain->fw_skipto_rule);
		break;
	case IP_FW_F_REJECT:
		if (chain->fw_reject_code == IP_FW_REJECT_RST)
			printf("reset");
		else {
			printf("unreach ");
			print_reject_code(chain->fw_reject_code);
		}
		break;
	case IP_FW_F_FWD:
		printf("fwd %s", inet_ntoa(chain->fw_fwd_ip.sin_addr));
		if (chain->fw_fwd_ip.sin_port)
			printf(",%d", chain->fw_fwd_ip.sin_port);
		break;
	default:
		errx(EX_OSERR, "impossible");
	}

	if (chain->fw_flg & IP_FW_F_PRN) {
		printf(" log");
		if (chain->fw_logamount)
			printf(" logamount %d", chain->fw_logamount);
	}

	if (chain->fw_flg & IP_FW_F_MAC) {
		u_char *addr, *mask;
		u_short *type, *typemask;
		int l;

		printf(" MAC");
		addr = (u_char *)&(chain->fw_mac_hdr);
		mask = (u_char *)&(chain->fw_mac_mask);
		print_mac(addr, mask);	/* destination */

		addr += 6;
		mask += 6;
		print_mac(addr, mask);	/* source */
		type = (u_short *)&(chain->fw_mac_type);
		typemask = (u_short *)&(chain->fw_mac_mask_type);

		/* type is in net format for all cases but range */
		if (chain->fw_flg & IP_FW_F_SRNG)
			printf(" %04x-%04x", *type, *typemask);
		else if (ntohs(*typemask) == 0)
			printf(" any");
		else if (ntohs(*typemask) != 0xffff)
			printf(" %04x&%04x", ntohs(*type), ntohs(*typemask));
		else {
			struct _s_x *p = NULL;
			u_int16_t i = ntohs(*type);
			if (do_resolv)
				for (p = ether_types ; p->s != NULL ; p++)
					if (p->x == i)
						break;
			if (p && p->s != NULL)
				printf(" %s", p->s);
			else
				printf(" %04x", i);
		}
		
		goto do_options;
	}
	pe = getprotobynumber(chain->fw_prot);
	if (pe)
		printf(" %s", pe->p_name);
	else
		printf(" %u", chain->fw_prot);

	printf(" from %s", chain->fw_flg & IP_FW_F_INVSRC ? "not " : "");

	if (chain->fw_flg & IP_FW_F_SME)
		printf("me");
	else
		print_ip(chain->fw_src, chain->fw_smsk);

	if (chain->fw_prot == IPPROTO_TCP || chain->fw_prot == IPPROTO_UDP)
		print_ports(chain->fw_prot, nsp, chain->fw_uar.fw_pts,
			chain->fw_flg & IP_FW_F_SRNG,
			chain->fw_flg & IP_FW_F_SMSK );

	printf(" to %s", chain->fw_flg & IP_FW_F_INVDST ? "not " : "");

	if (chain->fw_flg & IP_FW_F_DME)
		printf("me");
	else
		print_ip(chain->fw_dst, chain->fw_dmsk);

	if (chain->fw_prot == IPPROTO_TCP || chain->fw_prot == IPPROTO_UDP)
		print_ports(chain->fw_prot, ndp, chain->fw_uar.fw_pts+nsp,
			chain->fw_flg & IP_FW_F_DRNG,
			chain->fw_flg & IP_FW_F_DMSK );

do_options:
	if (chain->fw_flg & IP_FW_F_UID) {
		struct passwd *pwd = getpwuid(chain->fw_uid);

		if (pwd)
			printf(" uid %s", pwd->pw_name);
		else
			printf(" uid %u", chain->fw_uid);
	}

	if (chain->fw_flg & IP_FW_F_GID) {
		struct group *grp = getgrgid(chain->fw_gid);

		if (grp)
			printf(" gid %s", grp->gr_name);
		else
			printf(" gid %u", chain->fw_gid);
	}

	if (chain->fw_flg & IP_FW_F_KEEP_S) {
		struct _s_x *p = limit_masks;

		switch(chain->dyn_type) {
		default:
			printf(" *** unknown type ***");
			break ;
		case DYN_KEEP_STATE:
			printf(" keep-state");
			break;
		case DYN_LIMIT:
			printf(" limit");
			for ( ; p->x != 0 ; p++)
				if (chain->limit_mask & p->x)
					printf(" %s", p->s);
			printf(" %d", chain->conn_limit);
			break ;
		}
	}

	/* Direction */
	if (chain->fw_flg & IP_FW_BRIDGED)
		printf(" bridged");
	if ((chain->fw_flg & IP_FW_F_IN) && !(chain->fw_flg & IP_FW_F_OUT))
		printf(" in");
	if (!(chain->fw_flg & IP_FW_F_IN) && (chain->fw_flg & IP_FW_F_OUT))
		printf(" out");

	/* Handle hack for "via" backwards compatibility */
	if ((chain->fw_flg & IF_FW_F_VIAHACK) == IF_FW_F_VIAHACK) {
		print_iface("via", &chain->fw_in_if,
		    chain->fw_flg & IP_FW_F_IIFNAME);
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

	if (chain->fw_ipflg & IP_FW_IF_IPOPT)
		printopts("ipopt", chain->fw_ipopt, chain->fw_ipnopt,
			f_ipopts);

	if (chain->fw_ipflg & IP_FW_IF_IPLEN)
		printf(" iplen %u", chain->fw_iplen);

	if (chain->fw_ipflg & IP_FW_IF_IPID)
		printf(" ipid %#x", chain->fw_ipid);

	if (chain->fw_ipflg & IP_FW_IF_IPPRE)
		printf(" ipprecedence %u", (chain->fw_iptos & 0xe0) >> 5);

	if (chain->fw_ipflg & IP_FW_IF_IPTOS)
		printopts("iptos", chain->fw_iptos, chain->fw_ipntos,
			f_iptos);

	if (chain->fw_ipflg & IP_FW_IF_IPTTL)
		printf(" ipttl %u", chain->fw_ipttl);

	if (chain->fw_ipflg & IP_FW_IF_IPVER)
		printf(" ipversion %u", chain->fw_ipver);

	if (chain->fw_ipflg & IP_FW_IF_TCPEST)
		printf(" established");
	else if (chain->fw_tcpf == TH_SYN &&
		    chain->fw_tcpnf == TH_ACK)
		printf(" setup");
	else if (chain->fw_ipflg & IP_FW_IF_TCPFLG)
		printopts("tcpflags", chain->fw_tcpf, chain->fw_tcpnf,
			f_tcpflags);

	if (chain->fw_ipflg & IP_FW_IF_TCPOPT)
		printopts("tcpoptions", chain->fw_tcpopt, chain->fw_tcpnopt,
			f_tcpopts);

	if (chain->fw_ipflg & IP_FW_IF_TCPSEQ)
		printf(" tcpseq %lu", (u_long)ntohl(chain->fw_tcpseq));
	if (chain->fw_ipflg & IP_FW_IF_TCPACK)
		printf(" tcpack %lu", (u_long)ntohl(chain->fw_tcpack));
	if (chain->fw_ipflg & IP_FW_IF_TCPWIN)
		printf(" tcpwin %hu", ntohs(chain->fw_tcpwin));

	if (chain->fw_flg & IP_FW_F_ICMPBIT) {
		int i, first = 1;
		unsigned j;

		printf(" icmptype");

		for (i = 0; i < IP_FW_ICMPTYPES_DIM; ++i)
			for (j = 0; j < sizeof(unsigned) * 8; ++j)
				if (chain->fw_uar.fw_icmptypes[i] & (1 << j)) {
					printf("%c%d", first ? ' ' : ',',
					    i * sizeof(unsigned) * 8 + j);
					first = 0;
				}
	}
	printf("\n");
done:
	if (do_resolv)
		endservent();
}

static void
show_dyn_ipfw(struct ipfw_dyn_rule *d)
{
	struct protoent *pe;
	struct in_addr a;

	if (!d->expire && !do_expired)
		return;

	printf("%05d %qu %qu (T %ds, slot %d)",
	    (int)(d->rule), d->pcnt, d->bcnt, d->expire, d->bucket);
	switch (d->dyn_type) {
	case DYN_LIMIT_PARENT:
		printf(" PARENT %d", d->count);
		break;
	case DYN_LIMIT:
		printf(" LIMIT");
		break;
	case DYN_KEEP_STATE: /* bidir, no mask */
		printf(" <->");
		break;
	}

	if (do_resolv && (pe = getprotobynumber(d->id.proto)) != NULL)
		printf(" %s,", pe->p_name);
	else
		printf(" %u,", d->id.proto);

	a.s_addr = htonl(d->id.src_ip);
	printf(" %s %d", inet_ntoa(a), d->id.src_port);

	a.s_addr = htonl(d->id.dst_ip);
	printf("<-> %s %d", inet_ntoa(a), d->id.dst_port);
	printf("\n");
}

int
sort_q(const void *pa, const void *pb)
{
	int rev = (do_sort < 0);
	int field = rev ? -do_sort : do_sort;
	long long res = 0;
	const struct dn_flow_queue *a = pa;
	const struct dn_flow_queue *b = pb;

	switch (field) {
	case 1: /* pkts */
		res = a->len - b->len;
		break;
	case 2: /* bytes */
		res = a->len_bytes - b->len_bytes;
		break;

	case 3: /* tot pkts */
		res = a->tot_pkts - b->tot_pkts;
		break;

	case 4: /* tot bytes */
		res = a->tot_bytes - b->tot_bytes;
		break;
	}
	if (res < 0)
		res = -1;
	if (res > 0)
		res = 1;
	return (int)(rev ? res : -res);
}

static void
list_queues(struct dn_flow_set *fs, struct dn_flow_queue *q)
{
	int l;

	printf("    mask: 0x%02x 0x%08x/0x%04x -> 0x%08x/0x%04x\n",
	    fs->flow_mask.proto,
	    fs->flow_mask.src_ip, fs->flow_mask.src_port,
	    fs->flow_mask.dst_ip, fs->flow_mask.dst_port);
	if (fs->rq_elements == 0)
		return;

	printf("BKT Prot ___Source IP/port____ "
	    "____Dest. IP/port____ Tot_pkt/bytes Pkt/Byte Drp\n");
	if (do_sort != 0)
		heapsort(q, fs->rq_elements, sizeof *q, sort_q);
	for (l = 0; l < fs->rq_elements; l++) {
		struct in_addr ina;
		struct protoent *pe;

		ina.s_addr = htonl(q[l].id.src_ip);
		printf("%3d ", q[l].hash_slot);
		pe = getprotobynumber(q[l].id.proto);
		if (pe)
			printf("%-4s ", pe->p_name);
		else
			printf("%4u ", q[l].id.proto);
		printf("%15s/%-5d ",
		    inet_ntoa(ina), q[l].id.src_port);
		ina.s_addr = htonl(q[l].id.dst_ip);
		printf("%15s/%-5d ",
		    inet_ntoa(ina), q[l].id.dst_port);
		printf("%4qu %8qu %2u %4u %3u\n",
		    q[l].tot_pkts, q[l].tot_bytes,
		    q[l].len, q[l].len_bytes, q[l].drops);
		if (verbose)
			printf("   S %20qd  F %20qd\n",
			    q[l].S, q[l].F);
	}
}

static void
print_flowset_parms(struct dn_flow_set *fs, char *prefix)
{
	int l;
	char qs[30];
	char plr[30];
	char red[90];	/* Display RED parameters */

	l = fs->qsize;
	if (fs->flags_fs & DN_QSIZE_IS_BYTES) {
		if (l >= 8192)
			sprintf(qs, "%d KB", l / 1024);
		else
			sprintf(qs, "%d B", l);
	} else
		sprintf(qs, "%3d sl.", l);
	if (fs->plr)
		sprintf(plr, "plr %f", 1.0 * fs->plr / (double)(0x7fffffff));
	else
		plr[0] = '\0';
	if (fs->flags_fs & DN_IS_RED)	/* RED parameters */
		sprintf(red,
		    "\n\t  %cRED w_q %f min_th %d max_th %d max_p %f",
		    (fs->flags_fs & DN_IS_GENTLE_RED) ? 'G' : ' ',
		    1.0 * fs->w_q / (double)(1 << SCALE_RED),
		    SCALE_VAL(fs->min_th),
		    SCALE_VAL(fs->max_th),
		    1.0 * fs->max_p / (double)(1 << SCALE_RED));
	else
		sprintf(red, "droptail");

	printf("%s %s%s %d queues (%d buckets) %s\n",
	    prefix, qs, plr, fs->rq_elements, fs->rq_size, red);
}

static void
list(int ac, char *av[])
{
	struct ip_fw *rules, *r;
	struct ipfw_dyn_rule *dynrules, *d;
	struct dn_pipe *pipes;
	void *data = NULL;
	int n, nbytes, nstat, ndyn;
	int exitval = EX_OK;
	int lac;
	char **lav;
	u_long rnum;
	char *endptr;
	int seen = 0;

	/* get rules or pipes from kernel, resizing array as necessary */
	{
		const int unit = do_pipe ? sizeof(*pipes) : sizeof(*rules);
		const int ocmd = do_pipe ? IP_DUMMYNET_GET : IP_FW_GET;
		int nalloc = unit;
		nbytes = nalloc;

		while (nbytes >= nalloc) {
			nalloc = nalloc * 2 + 200;
			nbytes = nalloc;
			if ((data = realloc(data, nbytes)) == NULL)
				err(EX_OSERR, "realloc");
			if (getsockopt(s, IPPROTO_IP, ocmd, data, &nbytes) < 0)
				err(EX_OSERR, "getsockopt(IP_%s_GET)",
				    do_pipe ? "DUMMYNET" : "FW");
		}
	}

	/* display requested pipes */
	if (do_pipe) {
		u_long rulenum;
		void *next = data;
		struct dn_pipe *p = (struct dn_pipe *) data;
		struct dn_flow_set *fs;
		struct dn_flow_queue *q;
		int l;

		if (ac > 0)
			rulenum = strtoul(*av++, NULL, 10);
		else
			rulenum = 0;
		for (; nbytes >= sizeof *p; p = (struct dn_pipe *)next) {
			double b = p->bandwidth;
			char buf[30];
			char prefix[80];

			if (p->next != (struct dn_pipe *)DN_IS_PIPE)
				break;
			l = sizeof(*p) + p->fs.rq_elements * sizeof(*q);
			next = (void *)p + l;
			nbytes -= l;
			q = (struct dn_flow_queue *)(p+1);

			if (rulenum != 0 && rulenum != p->pipe_nr)
				continue;
			if (p->if_name[0] != '\0')
				sprintf(buf, "%s", p->if_name);
			else if (b == 0)
				sprintf(buf, "unlimited");
			else if (b >= 1000000)
				sprintf(buf, "%7.3f Mbit/s", b/1000000);
			else if (b >= 1000)
				sprintf(buf, "%7.3f Kbit/s", b/1000);
			else
				sprintf(buf, "%7.3f bit/s ", b);

			sprintf(prefix, "%05d: %s %4d ms ",
			    p->pipe_nr, buf, p->delay);
			print_flowset_parms(&(p->fs), prefix);
			if (verbose)
				printf("   V %20qd\n", p->V >> MY_M);
			list_queues(&(p->fs), q);
		}
		fs = (struct dn_flow_set *) next;
		for (; nbytes >= sizeof *fs; fs = (struct dn_flow_set *)next) {
			char prefix[80];

			if (fs->next != (struct dn_flow_set *)DN_IS_QUEUE)
				break;
			l = sizeof(*fs) + fs->rq_elements * sizeof(*q);
			next = (void *)fs + l;
			nbytes -= l;
			q = (struct dn_flow_queue *)(fs+1);
			sprintf(prefix, "q%05d: weight %d pipe %d ",
			    fs->fs_nr, fs->weight, fs->parent_nr);
			print_flowset_parms(fs, prefix);
			list_queues(fs, q);
		}
		free(data);
		return;
	}

	rules = (struct ip_fw *)data;
	for (nstat = 0; rules[nstat].fw_number < 65535; ++nstat)
		/* nothing */ ;
	nstat++; /* counting starts from 0 ... */
	dynrules = (struct ipfw_dyn_rule *)&rules[nstat];
	ndyn = (nbytes - (nstat * sizeof *rules)) / sizeof *dynrules;


	/* if no rule numbers were specified, list all rules */
	if (ac == 0) {
		for (n = 0; n < nstat; n++)
			show_ipfw(&rules[n]);

		if (do_dynamic && ndyn) {
			printf("## Dynamic rules:\n");
			for (n = 0, d = dynrules; n < ndyn; n++, d++)
				show_dyn_ipfw(d);
		}
		free(data);
		return;
	}

	/* display specific rules requested on command line */

	for (lac = ac, lav = av; lac != 0; lac--) {
		/* convert command line rule # */
		rnum = strtoul(*lav++, &endptr, 10);
		if (*endptr) {
			exitval = EX_USAGE;
			warnx("invalid rule number: %s", *(lav - 1));
			continue;
		}
		for (n = seen = 0, r = rules; n < nstat; n++, r++) {
			if (r->fw_number > rnum)
				break;
			if (r->fw_number == rnum) {
				show_ipfw(r);
				seen = 1;
			}
		}
		if (!seen) {
			/* give precedence to other error(s) */
			if (exitval == EX_OK)
				exitval = EX_UNAVAILABLE;
			warnx("rule %lu does not exist", rnum);
		}
	}

	printf("## Dynamic rules:\n");
	if (do_dynamic && ndyn) {
		for (lac = ac, lav = av; lac != 0; lac--) {
			rnum = strtoul(*lav++, &endptr, 10);
			if (*endptr)
				/* already warned */
				continue;
			for (n = 0, d = dynrules; n < ndyn; n++, d++) {
				if ((int)(d->rule) > rnum)
					break;
				if ((int)(d->rule) == rnum)
					show_dyn_ipfw(d);
			}
		}
	}

	ac = 0;

	free(data);

	if (exitval != EX_OK)
		exit(exitval);
}

static void
show_usage(void)
{
	fprintf(stderr, "usage: ipfw [options]\n"
"    add [number] rule\n"
"    pipe number config [pipeconfig]\n"
"    queue number config [queueconfig]\n"
"    [pipe] flush\n"
"    [pipe] delete number ...\n"
"    [pipe] {list|show} [number ...]\n"
"    {zero|resetlog} [number ...]\n"
"  rule: [prob <match_probability>] action (mac-hdr|ip-hdr) options...\n"
"  action:\n"
"      {allow|permit|accept|pass|deny|drop|reject|unreach code|\n"
"	reset|count|skipto num|divert port|tee port|fwd ip|\n"
"	pipe num} [log [logamount count]]\n"
"    proto: {ip|tcp|udp|icmp|<number>}\n"
"    src: from [not] {me|any|ip[{/bits|:mask}]} [{port[-port]}, [port], ...]\n"
"    dst: to [not] {me|any|ip[{/bits|:mask}]} [{port[-port]}, [port], ...]\n"
"  extras:\n"
"    uid {user id}\n"
"    gid {group id}\n"
"    fragment	  (may not be used with ports or tcpflags)\n"
"    in\n"
"    out\n"
"    {xmit|recv|via} {iface|ip|any}\n"
"    {established|setup}\n"
"    tcpflags [!]{syn|fin|rst|ack|psh|urg}, ...\n"
"    ipoptions [!]{ssrr|lsrr|rr|ts}, ...\n"
"    iplen {length}\n"
"    ipid {identification number}\n"
"    ipprecedence {precedence}\n"
"    iptos [!]{lowdelay|throughput|reliability|mincost|congestion}, ...\n"
"    ipttl {time to live}\n"
"    ipversion {version number}\n"
"    tcpoptions [!]{mss|window|sack|ts|cc}, ...\n"
"    tcpseq {sequence number}\n"
"    tcpack {acknowledgement number}\n"
"    tcpwin {window size}\n"
"    icmptypes {type[, type]}...\n"
"    keep-state [method]\n"
"  pipeconfig:\n"
"    {bw|bandwidth} <number>{bit/s|Kbit/s|Mbit/s|Bytes/s|KBytes/s|MBytes/s}\n"
"    {bw|bandwidth} interface_name\n"
"    delay <milliseconds>\n"
"    queue <size>{packets|Bytes|KBytes}\n"
"    plr <fraction>\n"
"    mask {all| [dst-ip|src-ip|dst-port|src-port|proto] <number>}\n"
"    buckets <number>}\n"
"    {red|gred} <fraction>/<number>/<number>/<fraction>\n"
"    droptail\n"
);

	exit(EX_USAGE);
}

static int
lookup_host (char *host, struct in_addr *ipaddr)
{
	struct hostent *he;

	if (!inet_aton(host, ipaddr)) {
		if ((he = gethostbyname(host)) == NULL)
			return(-1);
		*ipaddr = *(struct in_addr *)he->h_addr_list[0];
	}
	return(0);
}

static void
fill_ip(struct in_addr *ipno, struct in_addr *mask, int *acp, char ***avp)
{
	int ac = *acp;
	char **av = *avp;
	char *p = 0, md = 0;

	if (ac && !strncmp(*av, "any", strlen(*av))) {
		ipno->s_addr = mask->s_addr = 0; av++; ac--;
	} else {
		u_int32_t i;

		p = strchr(*av, '/');
		if (!p)
			p = strchr(*av, ':');
		if (p) {
			md = *p;
			*p++ = '\0';
		}

		if (lookup_host(*av, ipno) != 0)
			errx(EX_NOHOST, "hostname ``%s'' unknown", *av);
		switch (md) {
		case ':':
			if (!inet_aton(p, mask))
				errx(EX_DATAERR, "bad netmask ``%s''", p);
			break;
		case '/':
			i = atoi(p);
			if (i == 0)
				mask->s_addr = 0;
			else if (i > 32)
				errx(EX_DATAERR, "bad width ``%s''", p);
			else
				mask->s_addr = htonl(~0 << (32 - i));
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

	if (str == '\0')
		errx(EX_DATAERR, "missing unreachable code");
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
	errx(EX_DATAERR, "unknown ICMP unreachable code ``%s''", str);
}

static void
add_port(u_short *cnt, u_short *ptr, u_short off, u_short port)
{
	if (off + *cnt >= IP_FW_MAX_PORTS)
		errx(EX_USAGE, "too many ports (max is %d)", IP_FW_MAX_PORTS);
	ptr[off+*cnt] = port;
	(*cnt)++;
}

static int
lookup_port(const char *arg, int proto, int test, int nodash)
{
	int		val;
	char		*earg, buf[32];
	struct servent	*s;
	char		*p, *q;

	snprintf(buf, sizeof(buf), "%s", arg);

	for (p = q = buf; *p; *q++ = *p++) {
		if (*p == '\\') {
			if (*(p+1))
				p++;
		} else {
			if (*p == ',' || (nodash && *p == '-'))
				break;
		}
	}
	*q = '\0';

	val = (int) strtoul(buf, &earg, 0);
	if (!*buf || *earg) {
		char *protocol = NULL;

		if (proto != 0) {
			struct protoent *pe = getprotobynumber(proto);

			if (pe)
				protocol = pe->p_name;
		}

		setservent(1);
		s = getservbyname(buf, protocol);
		if (s != NULL)
			val = htons(s->s_port);
		else {
			if (!test)
				errx(EX_DATAERR, "unknown port ``%s''", buf);
			val = -1;
		}
	} else {
		if (val < 0 || val > 0xffff) {
			if (!test)
				errx(EX_DATAERR,
				    "port ``%s'' out of range", buf);
			val = -1;
		}
	}
	return(val);
}

/*
 * return: 0 normally, 1 if first pair is a range,
 * 2 if first pair is a port+mask
 */
static int
fill_port(u_short *cnt, u_short *ptr, u_short off, char *arg, int proto)
{
	char *s;
	int initial_range = 0;

	for (s = arg; *s && *s != ',' && *s != '-' && *s != ':'; s++) {
		if (*s == '\\' && *(s+1))
			s++;
	}
	if (*s == ':') {
		*s++ = '\0';
		if (strchr(arg, ','))
			errx(EX_USAGE, "port/mask must be first in list");
		add_port(cnt, ptr, off,
		    *arg ? lookup_port(arg, proto, 0, 0) : 0x0000);
		arg = s;
		s = strchr(arg, ',');
		if (s)
			*s++ = '\0';
		add_port(cnt, ptr, off,
		    *arg ? lookup_port(arg, proto, 0, 0) : 0xffff);
		arg = s;
		initial_range = 2;
	} else if (*s == '-') {
		*s++ = '\0';
		if (strchr(arg, ','))
			errx(EX_USAGE, "port range must be first in list");
		add_port(cnt, ptr, off,
		    *arg ? lookup_port(arg, proto, 0, 0) : 0x0000);
		arg = s;
		s = strchr(arg, ',');
		if (s)
			*s++ = '\0';
		add_port(cnt, ptr, off,
		    *arg ? lookup_port(arg, proto, 0, 0) : 0xffff);
		arg = s;
		initial_range = 1;
	}
	while (arg != NULL) {
		s = strchr(arg, ',');
		if (s)
			*s++ = '\0';
		add_port(cnt, ptr, off, lookup_port(arg, proto, 0, 0));
		arg = s;
	}
	return initial_range;
}

/*
 * helper function to process a set of flags and set bits in the
 * appropriate masks.
 */
static void
fill_flags(u_char *set, u_char *reset, struct _flaglist *flags, char **vp)
{
	char *p = *vp, *q;	/* parameter */
	u_char *d;	/* which mask we are working on */

	while (p && *p) {
		int i;

		if (*p == '!') {
			p++;
			d = reset;
		} else
			d = set;
		q = strchr(p, ',');
		if (q)
			*q++ = '\0';
		for (i = 0; flags[i].value != 0; ++i)
			if (!strncmp(p, flags[i].name, strlen(p))) {
				*d |= flags[i].value;
				break;
			}
		if (flags[i].value == 0)
			errx(EX_DATAERR, "invalid %s ``%s''", flags[i].name, p);
		p = q;
	}
}

static void
fill_icmptypes(unsigned *types, char **vp, u_int *fw_flg)
{
	unsigned long icmptype;
	char *c = *vp;

	while (*c) {
		if (*c == ',')
			++c;

		icmptype = strtoul(c, &c, 0);

		if (*c != ',' && *c != '\0')
			errx(EX_DATAERR, "invalid ICMP type");

		if (icmptype >= IP_FW_ICMPTYPES_DIM * sizeof(unsigned) * 8)
			errx(EX_DATAERR, "ICMP type out of range");

		types[icmptype / (sizeof(unsigned) * 8)] |=
			1 << (icmptype % (sizeof(unsigned) * 8));
		*fw_flg |= IP_FW_F_ICMPBIT;
	}
}

static void
delete(int ac, char *av[])
{
	struct ip_fw rule;
	struct dn_pipe pipe;
	int i;
	int exitval = EX_OK;

	memset(&rule, 0, sizeof rule);
	memset(&pipe, 0, sizeof pipe);

	av++; ac--;

	/* Rule number */
	while (ac && isdigit(**av)) {
		i = atoi(*av); av++; ac--;
		if (do_pipe) {
			if (do_pipe == 1)
				pipe.pipe_nr = i;
			else
				pipe.fs.fs_nr = i;
			i = setsockopt(s, IPPROTO_IP, IP_DUMMYNET_DEL,
			    &pipe, sizeof pipe);
			if (i) {
				exitval = 1;
				warn("rule %u: setsockopt(IP_DUMMYNET_DEL)",
				    do_pipe == 1 ? pipe.pipe_nr :
				    pipe.fs.fs_nr);
			}
		} else {
			rule.fw_number = i;
			i = setsockopt(s, IPPROTO_IP, IP_FW_DEL, &rule,
			    sizeof rule);
			if (i) {
				exitval = EX_UNAVAILABLE;
				warn("rule %u: setsockopt(IP_FW_DEL)",
				    rule.fw_number);
			}
		}
	}
	if (exitval != EX_OK)
		exit(exitval);
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
		warnx("warning: interface ``%s'' does not exist",
		    ifr.ifr_name);
}

static void
fill_iface(char *which, union ip_fw_if *ifu, int *byname, int ac, char *arg)
{
	if (!ac)
	    errx(EX_USAGE, "missing argument for ``%s''", which);

	/* Parse the interface or address */
	if (!strcmp(arg, "any")) {
		ifu->fu_via_ip.s_addr = 0;
		*byname = 0;
	} else if (!isdigit(*arg)) {
		char *q;

		*byname = 1;
		strncpy(ifu->fu_via_if.name, arg,
		    sizeof(ifu->fu_via_if.name));
		ifu->fu_via_if.name[sizeof(ifu->fu_via_if.name) - 1] = '\0';
		for (q = ifu->fu_via_if.name;
		    *q && !isdigit(*q) && *q != '*'; q++)
			continue;
		ifu->fu_via_if.unit = (*q == '*') ? -1 : atoi(q);
		*q = '\0';
		verify_interface(ifu);
	} else if (!inet_aton(arg, &ifu->fu_via_ip)) {
		errx(EX_DATAERR, "bad ip address ``%s''", arg);
	} else
		*byname = 0;
}

static void
config_pipe(int ac, char **av)
{
	struct dn_pipe pipe;
	int i;
	char *end;

	memset(&pipe, 0, sizeof pipe);

	av++; ac--;
	/* Pipe number */
	if (ac && isdigit(**av)) {
		i = atoi(*av); av++; ac--;
		if (do_pipe == 1)
			pipe.pipe_nr = i;
		else
			pipe.fs.fs_nr = i;
	}
	while (ac > 1) {
		if (!strncmp(*av, "plr", strlen(*av))) {

			double d = strtod(av[1], NULL);
			if (d > 1)
				d = 1;
			else if (d < 0)
				d = 0;
			pipe.fs.plr = (int)(d*0x7fffffff);
			av += 2;
			ac -= 2;
		} else if (!strncmp(*av, "queue", strlen(*av))) {
			end = NULL;
			pipe.fs.qsize = strtoul(av[1], &end, 0);
			if (*end == 'K' || *end == 'k') {
				pipe.fs.flags_fs |= DN_QSIZE_IS_BYTES;
				pipe.fs.qsize *= 1024;
			} else if (*end == 'B' || !strncmp(end, "by", 2)) {
				pipe.fs.flags_fs |= DN_QSIZE_IS_BYTES;
			}
			av += 2;
			ac -= 2;
		} else if (!strncmp(*av, "buckets", strlen(*av))) {
			pipe.fs.rq_size = strtoul(av[1], NULL, 0);
			av += 2;
			ac -= 2;
		} else if (!strncmp(*av, "mask", strlen(*av))) {
			/* per-flow queue, mask is dst_ip, dst_port,
			 * src_ip, src_port, proto measured in bits
			 */
			u_int32_t a;
			void *par = NULL;

			pipe.fs.flow_mask.dst_ip = 0;
			pipe.fs.flow_mask.src_ip = 0;
			pipe.fs.flow_mask.dst_port = 0;
			pipe.fs.flow_mask.src_port = 0;
			pipe.fs.flow_mask.proto = 0;
			end = NULL;
			av++; ac--;
			if (ac >= 1 && !strncmp(*av, "all", strlen(*av))) {
				/* special case -- all bits are significant */
				pipe.fs.flow_mask.dst_ip = ~0;
				pipe.fs.flow_mask.src_ip = ~0;
				pipe.fs.flow_mask.dst_port = ~0;
				pipe.fs.flow_mask.src_port = ~0;
				pipe.fs.flow_mask.proto = ~0;
				pipe.fs.flags_fs |= DN_HAVE_FLOW_MASK;
				av++;
				ac--;
				continue;
			}
			while (ac >= 1) {
				int len = strlen(*av);

				if (!strncmp(*av, "dst-ip", len))
					par = &pipe.fs.flow_mask.dst_ip;
				else if (!strncmp(*av, "src-ip", len))
					par = &pipe.fs.flow_mask.src_ip;
				else if (!strncmp(*av, "dst-port", len))
					par = &pipe.fs.flow_mask.dst_port;
				else if (!strncmp(*av, "src-port", len))
					par = &pipe.fs.flow_mask.src_port;
				else if (!strncmp(*av, "proto", len))
					par = &pipe.fs.flow_mask.proto;
				else
					break;
				if (ac < 2)
					errx(EX_USAGE, "mask: %s value"
					    " missing", *av);
				if (*av[1] == '/') {
					a = strtoul(av[1]+1, &end, 0);
					if (a == 32) /* special case... */
						a = ~0;
					else
						a = (1 << a) - 1;
				} else {
					a = strtoul(av[1], &end, 0);
				}
				if (par == &pipe.fs.flow_mask.src_port
				    || par == &pipe.fs.flow_mask.dst_port) {
					if (a >= (1 << 16))
						errx(EX_DATAERR, "mask: %s"
						    " must be 16 bit, not"
						    " 0x%08x", *av, a);
					*((u_int16_t *)par) = (u_int16_t)a;
				} else if (par == &pipe.fs.flow_mask.proto) {
					if (a >= (1 << 8))
						errx(EX_DATAERR, "mask: %s"
						    " must be"
						    " 8 bit, not 0x%08x",
						    *av, a);
					*((u_int8_t *)par) = (u_int8_t)a;
				} else
					*((u_int32_t *)par) = a;
				if (a != 0)
					pipe.fs.flags_fs |= DN_HAVE_FLOW_MASK;
				av += 2;
				ac -= 2;
			} /* end for */
		} else if (!strncmp(*av, "red", strlen(*av))
		    || !strncmp(*av, "gred", strlen(*av))) {
			/* RED enabled */
			pipe.fs.flags_fs |= DN_IS_RED;
			if (*av[0] == 'g')
				pipe.fs.flags_fs |= DN_IS_GENTLE_RED;
			if ((end = strsep(&av[1], "/"))) {
				double w_q = strtod(end, NULL);
				if (w_q > 1 || w_q <= 0)
					errx(EX_DATAERR, "w_q %f must be "
					    "0 < x <= 1", w_q);
				pipe.fs.w_q = (int) (w_q * (1 << SCALE_RED));
			}
			if ((end = strsep(&av[1], "/"))) {
				pipe.fs.min_th = strtoul(end, &end, 0);
				if (*end == 'K' || *end == 'k')
					pipe.fs.min_th *= 1024;
			}
			if ((end = strsep(&av[1], "/"))) {
				pipe.fs.max_th = strtoul(end, &end, 0);
				if (*end == 'K' || *end == 'k')
					pipe.fs.max_th *= 1024;
			}
			if ((end = strsep(&av[1], "/"))) {
				double max_p = strtod(end, NULL);
				if (max_p > 1 || max_p <= 0)
					errx(EX_DATAERR, "max_p %f must be "
					    "0 < x <= 1", max_p);
				pipe.fs.max_p =
				    (int)(max_p * (1 << SCALE_RED));
			}
			av += 2;
			ac -= 2;
		} else if (!strncmp(*av, "droptail", strlen(*av))) {
			/* DROPTAIL */
			pipe.fs.flags_fs &= ~(DN_IS_RED|DN_IS_GENTLE_RED);
			av += 1;
			ac -= 1;
		} else {
			int len = strlen(*av);
			if (do_pipe == 1) {
				/* some commands are only good for pipes. */
				if (!strncmp(*av, "bw", len)
				    || !strncmp(*av, "bandwidth", len)) {
					if (av[1][0] >= 'a'
					    && av[1][0] <= 'z') {
						int l = sizeof(pipe.if_name)-1;
						/* interface name */
						strncpy(pipe.if_name, av[1], l);
						pipe.if_name[l] = '\0';
						pipe.bandwidth = 0;
					} else {
						pipe.if_name[0] = '\0';
						pipe.bandwidth =
						    strtoul(av[1], &end, 0);
						if (*end == 'K'
						    || *end == 'k') {
							end++;
							pipe.bandwidth *=
							    1000;
						} else if (*end == 'M') {
							end++;
							pipe.bandwidth *=
							    1000000;
						}
						if (*end == 'B'
						    || !strncmp(end, "by", 2))
							pipe.bandwidth *= 8;
					}
					if (pipe.bandwidth < 0)
						errx(EX_DATAERR,
						    "bandwidth too large");
					av += 2;
					ac -= 2;
				} else if (!strncmp(*av, "delay", len)) {
					pipe.delay = strtoul(av[1], NULL, 0);
					av += 2;
					ac -= 2;
				} else {
					errx(EX_DATAERR, "unrecognised pipe"
					    " option ``%s''", *av);
				}
			} else { /* this refers to a queue */
				if (!strncmp(*av, "weight", len)) {
					pipe.fs.weight =
					    strtoul(av[1], &end, 0);
					av += 2;
					ac -= 2;
				} else if (!strncmp(*av, "pipe", len)) {
					pipe.fs.parent_nr =
					    strtoul(av[1], &end, 0);
					av += 2;
					ac -= 2;
				} else {
					errx(EX_DATAERR, "unrecognised option "
					    "``%s''", *av);
				}
			}
		}
	}
	if (do_pipe == 1) {
		if (pipe.pipe_nr == 0)
			errx(EX_DATAERR, "pipe_nr %d must be > 0",
			    pipe.pipe_nr);
		if (pipe.delay > 10000)
			errx(EX_DATAERR, "delay %d must be < 10000",
			    pipe.delay);
	} else { /* do_pipe == 2, queue */
		if (pipe.fs.parent_nr == 0)
			errx(EX_DATAERR, "pipe %d must be > 0",
			    pipe.fs.parent_nr);
		if (pipe.fs.weight >100)
			errx(EX_DATAERR, "weight %d must be <= 100",
			    pipe.fs.weight);
	}
	if (pipe.fs.flags_fs & DN_QSIZE_IS_BYTES) {
		if (pipe.fs.qsize > 1024*1024)
			errx(EX_DATAERR, "queue size %d, must be < 1MB",
			    pipe.fs.qsize);
	} else {
		if (pipe.fs.qsize > 100)
			errx(EX_DATAERR, "queue size %d, must be"
			    " 2 <= x <= 100", pipe.fs.qsize);
	}
	if (pipe.fs.flags_fs & DN_IS_RED) {
		size_t len;
		int lookup_depth, avg_pkt_size;
		double s, idle, weight, w_q;
		struct clockinfo clock;
		int t;

		if (pipe.fs.min_th >= pipe.fs.max_th)
			errx(EX_DATAERR, "min_th %d must be < than max_th %d",
			    pipe.fs.min_th, pipe.fs.max_th);
		if (pipe.fs.max_th == 0)
			errx(EX_DATAERR, "max_th must be > 0");

		len = sizeof(int);
		if (sysctlbyname("net.inet.ip.dummynet.red_lookup_depth",
			    &lookup_depth, &len, NULL, 0) == -1)

			errx(1, "sysctlbyname(\"%s\")",
			    "net.inet.ip.dummynet.red_lookup_depth");
		if (lookup_depth == 0)
			errx(EX_DATAERR, "net.inet.ip.dummynet.red_lookup_depth"
			    " must be greater than zero");

		len = sizeof(int);
		if (sysctlbyname("net.inet.ip.dummynet.red_avg_pkt_size",
			    &avg_pkt_size, &len, NULL, 0) == -1)

			errx(1, "sysctlbyname(\"%s\")",
			    "net.inet.ip.dummynet.red_avg_pkt_size");
		if (avg_pkt_size == 0)
			errx(EX_DATAERR,
			    "net.inet.ip.dummynet.red_avg_pkt_size must"
			    " be greater than zero");

		len = sizeof(struct clockinfo);
		if (sysctlbyname("kern.clockrate", &clock, &len, NULL, 0) == -1)
			errx(1, "sysctlbyname(\"%s\")", "kern.clockrate");

		/*
		 * Ticks needed for sending a medium-sized packet.
		 * Unfortunately, when we are configuring a WF2Q+ queue, we
		 * do not have bandwidth information, because that is stored
		 * in the parent pipe, and also we have multiple queues
		 * competing for it. So we set s=0, which is not very
		 * correct. But on the other hand, why do we want RED with
		 * WF2Q+ ?
		 */
		if (pipe.bandwidth==0) /* this is a WF2Q+ queue */
			s = 0;
		else
			s = clock.hz * avg_pkt_size * 8 / pipe.bandwidth;

		/*
		 * max idle time (in ticks) before avg queue size becomes 0.
		 * NOTA:  (3/w_q) is approx the value x so that
		 * (1-w_q)^x < 10^-3.
		 */
		w_q = ((double)pipe.fs.w_q) / (1 << SCALE_RED);
		idle = s * 3. / w_q;
		pipe.fs.lookup_step = (int)idle / lookup_depth;
		if (!pipe.fs.lookup_step)
			pipe.fs.lookup_step = 1;
		weight = 1 - w_q;
		for (t = pipe.fs.lookup_step; t > 0; --t)
			weight *= weight;
		pipe.fs.lookup_weight = (int)(weight * (1 << SCALE_RED));
	}
	i = setsockopt(s, IPPROTO_IP, IP_DUMMYNET_CONFIGURE, &pipe,
	    sizeof pipe);
	if (i)
		err(1, "setsockopt(%s)", "IP_DUMMYNET_CONFIGURE");
}

static void
get_mac_addr_mask(char *p, u_char *addr, u_char *mask)
{
	int i, l;

	for (i=0; i<6; i++)
		addr[i] = mask[i] = 0;
	if (!strcmp(p, "any"))
		return;

	for (i=0; *p && i<6;i++, p++) {
		addr[i] = strtol(p, &p, 16);
		if (*p != ':') /* we start with the mask */
			break;
	}
	if (*p == '/') { /* mask len */
		l = strtol(p+1, &p, 0);
		for (i=0; l>0; l -=8, i++)
			mask[i] = (l >=8) ? 0xff : (~0) << (8-l);
	} else if (*p == '&') { /* mask */
		for (i=0, p++; *p && i<6;i++, p++) {
			mask[i] = strtol(p, &p, 16);
			if (*p != ':')
				break;
		}
	} else if (*p == '\0') {
		for (i=0; i<6; i++)
			mask[i] = 0xff;
	}
	for (i=0; i<6; i++)
		addr[i] &= mask[i];
}

/*
 * fetch and add the MAC address and type, with masks
 */
static void
add_mac(struct ip_fw *rule, int ac, char *av[])
{
	u_char *addr, *mask;
	u_short *type, *typemask;
	int i;
	char *p;
	struct _s_x *pt;

	if (ac <3)
		errx(EX_DATAERR, "MAC dst src type");
	addr = (u_char *)&(rule->fw_mac_hdr);
	mask = (u_char *)&(rule->fw_mac_mask);

	get_mac_addr_mask(av[0], addr, mask);
	addr += 6;
	mask += 6;
	av++;

	get_mac_addr_mask(av[0], addr, mask);
	av++;

	type = (u_short *)&(rule->fw_mac_type);
	typemask = (u_short *)&(rule->fw_mac_mask_type);
	rule->fw_flg |= IP_FW_F_MAC;

	if (!strcmp(av[0], "any")) {
		*type = *typemask = htons(0);
		return;
	}

	/*
	 * the match length is the string up to the first separator
	 * we know, i.e. any of "\0:/&". Note, we use bcmp instead of
	 * strcmp as we want an exact match.
	 */
	p = strpbrk(av[0], "-:/&");
	if (p == NULL)
		i = strlen(av[0]);
	else
		i = p - av[0];
	for (pt = ether_types ; i && pt->s != NULL ; pt++)
		if (strlen(pt->s) == i && !bcmp(*av, pt->s, i))
			break;
	/* store type in network format for all cases but range */
	if (pt->s != NULL) {
		*type = htons(pt->x);
		p = av[0] + i;
	} else
		*type = htons( strtol(av[0], &p, 16) );
	*typemask = htons(0xffff); /* default */
	if (*p == '-') {
		rule->fw_flg |= IP_FW_F_SRNG;
		*type = ntohs(*type);	/* revert to host format */
		p++;
		i = strlen(p);
		for (pt = ether_types ; i && pt->s != NULL ; pt++)
			if (strlen(pt->s) == i && !bcmp(p, pt->s, i))
				break;
		if (pt->s != NULL) {
			*typemask = pt->x;
			p += i;
		} else
			*typemask = strtol(p, &p, 16);
	} else if (*p == '/') {
		i = strtol(p+1, &p, 10);
		if (i > 16)
			errx(EX_DATAERR, "MAC: bad type %s\n", av[0]);
		*typemask = htons( (~0) << (16 - i) );
		*type &= *typemask;
	} else if (*p == ':') {
		*typemask = htons( strtol(p+1, &p, 16) );
		*type &= *typemask;
	}
	if (*p != '\0')
		errx(EX_DATAERR, "MAC: bad end type %s\n", av[0]);
}

/*
 * the following macro returns an error message if we run out of
 * arguments.
 */
#define	NEED1(msg)	{if (!ac) errx(EX_USAGE, msg);}

static void
add(int ac, char *av[])
{
	struct ip_fw rule;
	int i;
	u_char proto;
	struct protoent *pe;
	int saw_xmrc = 0, saw_via = 0;

	memset(&rule, 0, sizeof rule);

	av++; ac--;

	/* [rule N]	-- Rule number optional */
	if (ac && isdigit(**av)) {
		rule.fw_number = atoi(*av); av++; ac--;
	}

	/* [prob D]	-- match probability, optional */
	if (ac > 1 && !strncmp(*av, "prob", strlen(*av))) {
		double d = strtod(av[1], NULL);
		if (d <= 0 || d > 1)
			errx(EX_DATAERR, "illegal match prob. %s", av[1]);
		if (d != 1) { /* 1 means always match */
			rule.fw_flg |= IP_FW_F_RND_MATCH;
			rule.dont_match_prob = (long)((1 - d) * 0x7fffffff);
		}
		av += 2; ac -= 2;
	}

	/* action	-- mandatory */
	NEED1("missing action");
	if (!strncmp(*av, "accept", strlen(*av))
		    || !strncmp(*av, "pass", strlen(*av))
		    || !strncmp(*av, "allow", strlen(*av))
		    || !strncmp(*av, "permit", strlen(*av))) {
		rule.fw_flg |= IP_FW_F_ACCEPT; av++; ac--;
	} else if (!strncmp(*av, "count", strlen(*av))) {
		rule.fw_flg |= IP_FW_F_COUNT; av++; ac--;
	} else if (!strncmp(*av, "pipe", strlen(*av))) {
		rule.fw_flg |= IP_FW_F_PIPE; av++; ac--;
		NEED1("missing pipe number");
		rule.fw_divert_port = strtoul(*av, NULL, 0); av++; ac--;
	} else if (!strncmp(*av, "queue", strlen(*av))) {
		rule.fw_flg |= IP_FW_F_QUEUE; av++; ac--;
		NEED1("missing queue number");
		rule.fw_divert_port = strtoul(*av, NULL, 0); av++; ac--;
	} else if (!strncmp(*av, "divert", strlen(*av))) {
		rule.fw_flg |= IP_FW_F_DIVERT; av++; ac--;
		NEED1("missing divert port");
		rule.fw_divert_port = strtoul(*av, NULL, 0); av++; ac--;
		if (rule.fw_divert_port == 0) {
			struct servent *s;
			setservent(1);
			s = getservbyname(av[-1], "divert");
			if (s != NULL)
				rule.fw_divert_port = ntohs(s->s_port);
			else
				errx(EX_DATAERR, "illegal %s port", "divert");
		}
	} else if (!strncmp(*av, "tee", strlen(*av))) {
		rule.fw_flg |= IP_FW_F_TEE; av++; ac--;
		NEED1("missing tee divert port");
		rule.fw_divert_port = strtoul(*av, NULL, 0); av++; ac--;
		if (rule.fw_divert_port == 0) {
			struct servent *s;
			setservent(1);
			s = getservbyname(av[-1], "divert");
			if (s != NULL)
				rule.fw_divert_port = ntohs(s->s_port);
			else
				errx(EX_DATAERR, "illegal %s port",
				    "tee divert");
		}
	} else if (!strncmp(*av, "fwd", strlen(*av))
	    || !strncmp(*av, "forward", strlen(*av))) {
		struct in_addr dummyip;
		char *pp;
		rule.fw_flg |= IP_FW_F_FWD; av++; ac--;
		NEED1("missing forwarding IP address");
		rule.fw_fwd_ip.sin_len = sizeof(struct sockaddr_in);
		rule.fw_fwd_ip.sin_family = AF_INET;
		rule.fw_fwd_ip.sin_port = 0;
		pp = strchr(*av, ':');
		if( pp == NULL)
			pp = strchr(*av, ',');
		if (pp != NULL) {
			*(pp++) = '\0';
			i = lookup_port(pp, 0, 1, 0);
			if (i == -1)
				errx(EX_DATAERR, "illegal forwarding"
				    " port ``%s''", pp);
			else
				rule.fw_fwd_ip.sin_port = (u_short)i;
		}
		fill_ip(&(rule.fw_fwd_ip.sin_addr), &dummyip, &ac, &av);
		if (rule.fw_fwd_ip.sin_addr.s_addr == 0)
			errx(EX_DATAERR, "illegal forwarding IP address");

	} else if (!strncmp(*av, "skipto", strlen(*av))) {
		rule.fw_flg |= IP_FW_F_SKIPTO; av++; ac--;
		NEED1("missing skipto rule number");
		rule.fw_skipto_rule = strtoul(*av, NULL, 10); av++; ac--;
	} else if ((!strncmp(*av, "deny", strlen(*av))
		    || !strncmp(*av, "drop", strlen(*av)))) {
		rule.fw_flg |= IP_FW_F_DENY; av++; ac--;
	} else if (!strncmp(*av, "reject", strlen(*av))) {
		rule.fw_flg |= IP_FW_F_REJECT; av++; ac--;
		rule.fw_reject_code = ICMP_UNREACH_HOST;
	} else if (!strncmp(*av, "reset", strlen(*av))) {
		rule.fw_flg |= IP_FW_F_REJECT; av++; ac--;
		rule.fw_reject_code = IP_FW_REJECT_RST;	/* check TCP later */
	} else if (!strncmp(*av, "unreach", strlen(*av))) {
		rule.fw_flg |= IP_FW_F_REJECT; av++; ac--;
		fill_reject_code(&rule.fw_reject_code, *av); av++; ac--;
	} else if (!strncmp(*av, "check-state", strlen(*av))) {
		rule.fw_flg |= IP_FW_F_CHECK_S; av++; ac--;
		goto done;
	} else {
		errx(EX_DATAERR, "invalid action ``%s''", *av);
	}

	/* [log [logamount N]]	-- log, optional */
	if (ac && !strncmp(*av, "log", strlen(*av))) {
	    rule.fw_flg |= IP_FW_F_PRN; av++; ac--;
	    if (ac && !strncmp(*av, "logamount", strlen(*av))) {
		ac--; av++;
		NEED1("``logamount'' requires argument");
		rule.fw_logamount = atoi(*av);
		if (rule.fw_logamount < 0)
		    errx(EX_DATAERR, "``logamount'' argument must be positive");
		if (rule.fw_logamount == 0)
		    rule.fw_logamount = -1;
		ac--; av++;
	    }
	}

	/* protocol	-- mandatory */
	NEED1("missing protocol");
	
	if (!strncmp(*av, "MAC", strlen(*av))) {
		ac--;
		av++;
		/* need exactly 3 fields */
		add_mac(&rule, ac, av);	/* exits in case of errors */
		ac -= 3;
		av += 3;
		goto do_options;
	} else if ((proto = atoi(*av)) > 0) {
		rule.fw_prot = proto; av++; ac--;
	} else if (!strncmp(*av, "all", strlen(*av))) {
		rule.fw_prot = IPPROTO_IP; av++; ac--;
	} else if ((pe = getprotobyname(*av)) != NULL) {
		rule.fw_prot = pe->p_proto; av++; ac--;
	} else {
		errx(EX_DATAERR, "invalid protocol ``%s''", *av);
	}

	if (rule.fw_prot != IPPROTO_TCP
	    && (rule.fw_flg & IP_FW_F_COMMAND) == IP_FW_F_REJECT
	    && rule.fw_reject_code == IP_FW_REJECT_RST)
		errx(EX_DATAERR, "``reset'' is only valid for tcp packets");

	/* from --	mandatory */
	if (!ac || strncmp(*av, "from", strlen(*av)))
		errx(EX_USAGE, "missing ``from''");
	av++; ac--;

	/* not	--	optional */
	if (ac && !strncmp(*av, "not", strlen(*av))) {
		rule.fw_flg |= IP_FW_F_INVSRC;
		av++; ac--;
	}
	NEED1("missing source address");

	/* source	-- mandatory */
	if (!strncmp(*av, "me", strlen(*av))) {
		rule.fw_flg |= IP_FW_F_SME;
		av++; ac--;
	} else {
		fill_ip(&rule.fw_src, &rule.fw_smsk, &ac, &av);
	}

	/* ports	-- optional */
	if (ac && (isdigit(**av)
	    || lookup_port(*av, rule.fw_prot, 1, 1) >= 0)) {
		u_short nports = 0;
		int retval;

		retval = fill_port(&nports, rule.fw_uar.fw_pts,
		    0, *av, rule.fw_prot);
		if (retval == 1)
			rule.fw_flg |= IP_FW_F_SRNG;
		else if (retval == 2)
			rule.fw_flg |= IP_FW_F_SMSK;
		IP_FW_SETNSRCP(&rule, nports);
		av++; ac--;
	}

	/* to --	mandatory */
	if (!ac || strncmp(*av, "to", strlen(*av)))
		errx(EX_USAGE, "missing ``to''");
	av++; ac--;

	/* not --	optional */
	if (ac && !strncmp(*av, "not", strlen(*av))) {
		rule.fw_flg |= IP_FW_F_INVDST;
		av++; ac--;
	}
	NEED1("missing dst address");

	/* destination	-- mandatory */
	if (!strncmp(*av, "me", strlen(*av))) {
		rule.fw_flg |= IP_FW_F_DME;
		av++; ac--;
	} else {
		fill_ip(&rule.fw_dst, &rule.fw_dmsk, &ac, &av);
	}

	/* dest.ports	-- optional */
	if (ac && (isdigit(**av)
	    || lookup_port(*av, rule.fw_prot, 1, 1) >= 0)) {
		u_short	nports = 0;
		int retval;

		retval = fill_port(&nports, rule.fw_uar.fw_pts,
		    IP_FW_GETNSRCP(&rule), *av, rule.fw_prot);
		if (retval == 1)
			rule.fw_flg |= IP_FW_F_DRNG;
		else if (retval == 2)
			rule.fw_flg |= IP_FW_F_DMSK;
		IP_FW_SETNDSTP(&rule, nports);
		av++; ac--;
	}

	if ((rule.fw_prot != IPPROTO_TCP) && (rule.fw_prot != IPPROTO_UDP)
	    && (IP_FW_GETNSRCP(&rule) || IP_FW_GETNDSTP(&rule))) {
		errx(EX_USAGE, "only TCP and UDP protocols are valid"
		    " with port specifications");
	}

do_options:
	while (ac) {
		if (!strncmp(*av, "uid", strlen(*av))) {
			struct passwd *pwd;
			char *end;
			uid_t uid;

			rule.fw_flg |= IP_FW_F_UID;
			ac--; av++;
			NEED1("``uid'' requires argument");

			uid = strtoul(*av, &end, 0);
			if (*end == '\0')
				pwd = getpwuid(uid);
			else
				pwd = getpwnam(*av);
			if (pwd == NULL)
				errx(EX_DATAERR, "uid \"%s\" is"
				     " nonexistent", *av);
			rule.fw_uid = pwd->pw_uid;
			ac--; av++;
		} else if (!strncmp(*av, "gid", strlen(*av))) {
			struct group *grp;
			char *end;
			gid_t gid;

			rule.fw_flg |= IP_FW_F_GID;
			ac--; av++;
			NEED1("``gid'' requires argument");

			gid = strtoul(*av, &end, 0);
			if (*end == '\0')
				grp = getgrgid(gid);
			else
				grp = getgrnam(*av);
			if (grp == NULL)
				errx(EX_DATAERR, "gid \"%s\" is"
				     " nonexistent", *av);
			rule.fw_gid = grp->gr_gid;
			ac--; av++;
		} else if (!strncmp(*av, "in", strlen(*av))) {
			rule.fw_flg |= IP_FW_F_IN;
			av++; ac--;
		} else if (!strncmp(*av,"limit",strlen(*av))) {
			/* dyn. rule used to limit number of connections. */
			rule.fw_flg |= IP_FW_F_KEEP_S;
			rule.dyn_type = DYN_LIMIT ;
			rule.limit_mask = 0 ;
			av++; ac--;
			for (; ac >1 ;) {
			    struct _s_x *p = limit_masks;
			    for ( ; p->x != 0 ; p++)
				if (!strncmp(*av, p->s, strlen(*av))) {
				    rule.limit_mask |= p->x ;
				    av++; ac-- ;
				    break ;
				}
			    if (p->s == NULL)
				break ;
			}
			NEED1("limit needs mask and # of connections");
			rule.conn_limit = atoi(*av);
			if (rule.conn_limit == 0)
			    errx(EX_USAGE, "limit: limit must be >0");
			if (rule.limit_mask == 0)
			    errx(EX_USAGE, "missing limit mask");
			av++; ac--;
		} else if (!strncmp(*av, "keep-state", strlen(*av))) {
			u_long type;
			rule.fw_flg |= IP_FW_F_KEEP_S;

			av++; ac--;
			if (ac > 0 && (type = atoi(*av)) != 0) {
				rule.dyn_type = type;
				av++; ac--;
			}
		} else if (!strncmp(*av, "bridged", strlen(*av))) {
			rule.fw_flg |= IP_FW_BRIDGED;
			av++; ac--;
		} else if (!strncmp(*av, "out", strlen(*av))) {
			rule.fw_flg |= IP_FW_F_OUT;
			av++; ac--;
		} else if (!strncmp(*av, "xmit", strlen(*av))) {
			union ip_fw_if ifu;
			int byname;

			if (saw_via) {
badviacombo:
				errx(EX_USAGE, "``via'' is incompatible"
				    " with ``xmit'' and ``recv''");
			}
			saw_xmrc = 1;
			av++; ac--;
			fill_iface("xmit", &ifu, &byname, ac, *av);
			rule.fw_out_if = ifu;
			rule.fw_flg |= IP_FW_F_OIFACE;
			if (byname)
				rule.fw_flg |= IP_FW_F_OIFNAME;
			av++; ac--;
		} else if (!strncmp(*av, "recv", strlen(*av))) {
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
			av++; ac--;
		} else if (!strncmp(*av, "via", strlen(*av))) {
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
			av++; ac--;
		} else if (!strncmp(*av, "fragment", strlen(*av))) {
			rule.fw_flg |= IP_FW_F_FRAG;
			av++; ac--;
		} else if (!strncmp(*av, "ipoptions", strlen(*av))
		    || !strncmp(*av, "ipopts", strlen(*av))) {
			av++; ac--;
			NEED1("missing argument for ``ipoptions''");
			rule.fw_ipflg |= IP_FW_IF_IPOPT;
			fill_flags(&rule.fw_ipopt, &rule.fw_ipnopt,
				f_ipopts, av);
			av++; ac--;
		} else if (!strncmp(*av, "iplen", strlen(*av))) {
			av++; ac--;
			NEED1("missing argument for ``iplen''");
			rule.fw_ipflg |= IP_FW_IF_IPLEN;
			rule.fw_iplen = (u_short)strtoul(*av, NULL, 0);
			av++; ac--;
		} else if (!strncmp(*av, "ipid", strlen(*av))) {
			unsigned long ipid;
			char *c;

			av++; ac--;
			NEED1("missing argument for ``ipid''");
			ipid = strtoul(*av, &c, 0);
			if (*c != '\0')
				errx(EX_DATAERR, "argument to ipid must"
				     " be numeric");
			if (ipid > 65535)
				errx(EX_DATAERR, "argument to ipid out"
				     " of range");
			rule.fw_ipflg |= IP_FW_IF_IPID;
			rule.fw_ipid = (u_short)ipid;
			av++; ac--;
		} else if (!strncmp(*av, "ipprecedence", strlen(*av))) {
			u_long ippre;
			char *c;

			av++; ac--;
			NEED1("missing argument for ``ipprecedence''");
			ippre = strtoul(*av, &c, 0);
			if (*c != '\0')
				errx(EX_DATAERR, "argument to ipprecedence"
					" must be numeric");
			if (ippre > 7)
				errx(EX_DATAERR, "argument to ipprecedence"
					" out of range");
			rule.fw_ipflg |= IP_FW_IF_IPPRE;
			rule.fw_iptos |= (u_short)(ippre << 5);
			av++; ac--;
		} else if (!strncmp(*av, "iptos", strlen(*av))) {
			av++; ac--;
			NEED1("missing argument for ``iptos''");
			rule.fw_ipflg |= IP_FW_IF_IPTOS;
			fill_flags(&rule.fw_iptos, &rule.fw_ipntos,
			    f_iptos, av);
			av++; ac--;
		} else if (!strncmp(*av, "ipttl", strlen(*av))) {
			av++; ac--;
			NEED1("missing argument for ``ipttl''");
			rule.fw_ipflg |= IP_FW_IF_IPTTL;
			rule.fw_ipttl = (u_short)strtoul(*av, NULL, 0);
			av++; ac--;
		} else if (!strncmp(*av, "ipversion", strlen(*av))
		    || !strncmp(*av, "ipver", strlen(*av))) {
			av++; ac--;
			NEED1("missing argument for ``ipversion''");
			rule.fw_ipflg |= IP_FW_IF_IPVER;
			rule.fw_ipver = (u_short)strtoul(*av, NULL, 0);
			av++; ac--;
		} else if (rule.fw_prot == IPPROTO_TCP) {
			if (!strncmp(*av, "established", strlen(*av))) {
				rule.fw_ipflg |= IP_FW_IF_TCPEST;
				av++; ac--;
			} else if (!strncmp(*av, "setup", strlen(*av))) {
				rule.fw_tcpf  |= TH_SYN;
				rule.fw_tcpnf  |= TH_ACK;
				rule.fw_ipflg |= IP_FW_IF_TCPFLG;
				av++; ac--;
			} else if (!strncmp(*av, "tcpflags", strlen(*av))
			    || !strncmp(*av, "tcpflgs", strlen(*av))) {
				av++; ac--;
				NEED1("missing argument for ``tcpflags''");
				rule.fw_ipflg |= IP_FW_IF_TCPFLG;
				fill_flags(&rule.fw_tcpf,
				    &rule.fw_tcpnf, f_tcpflags, av);
				av++; ac--;
			} else if (!strncmp(*av, "tcpoptions", strlen(*av))
			    || !strncmp(*av, "tcpopts", strlen(*av))) {
				av++; ac--;
				NEED1("missing argument for ``tcpoptions''");
				rule.fw_ipflg |= IP_FW_IF_TCPOPT;
				fill_flags(&rule.fw_tcpopt,
				    &rule.fw_tcpnopt, f_tcpopts, av);
				av++; ac--;
			} else if (!strncmp(*av, "tcpseq", strlen(*av))) {
				av++; ac--;
				NEED1("missing argument for ``tcpseq''");
				rule.fw_ipflg |= IP_FW_IF_TCPSEQ;
				rule.fw_tcpseq =
				    htonl(strtoul(*av, NULL, 0));
				av++; ac--;
			} else if (!strncmp(*av, "tcpack", strlen(*av))) {
				av++; ac--;
				NEED1("missing argument for ``tcpack''");
				rule.fw_ipflg |= IP_FW_IF_TCPACK;
				rule.fw_tcpack =
				    htonl(strtoul(*av, NULL, 0));
				av++; ac--;
			} else if (!strncmp(*av, "tcpwin", strlen(*av))) {
				av++; ac--;
				NEED1("missing argument for ``tcpwin''");
				rule.fw_ipflg |= IP_FW_IF_TCPWIN;
				rule.fw_tcpwin =
				    htons((u_short)strtoul(*av, NULL, 0));
				av++; ac--;
			} else
				errx(EX_USAGE, "unknown or out of order"
				     " argument ``%s''", *av);
		} else if (rule.fw_prot == IPPROTO_ICMP) {
			if (!strncmp(*av, "icmptypes", strlen(*av))) {
				av++; ac--;
				NEED1("missing argument for ``icmptypes''");
				fill_icmptypes(rule.fw_uar.fw_icmptypes,
				    av, &rule.fw_flg);
				av++; ac--;
			} else
				errx(EX_USAGE, "unknown or out of"
				     " order argument ``%s''", *av);
		} else
			errx(EX_USAGE, "unknown argument ``%s''", *av);
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
	} else if ((rule.fw_flg & IP_FW_F_OIFACE)
	    && (rule.fw_flg & IP_FW_F_IN)) {
		errx(EX_DATAERR, "can't check xmit interface of incoming"
		     " packets");
	}

	/* frag may not be used in conjunction with ports or TCP flags */
	if (rule.fw_flg & IP_FW_F_FRAG) {
		if (rule.fw_tcpf || rule.fw_tcpnf)
			errx(EX_DATAERR, "can't mix 'frag' and tcpflags");

		if (rule.fw_nports)
			errx(EX_DATAERR, "can't mix 'frag' and port"
			     " specifications");
	}
	if (rule.fw_flg & IP_FW_F_PRN) {
		if (!rule.fw_logamount) {
			size_t len = sizeof(int);

			if (sysctlbyname("net.inet.ip.fw.verbose_limit",
			    &rule.fw_logamount, &len, NULL, 0) == -1)
				errx(1, "sysctlbyname(\"%s\")",
				    "net.inet.ip.fw.verbose_limit");
		} else if (rule.fw_logamount == -1)
			rule.fw_logamount = 0;
		rule.fw_loghighest = rule.fw_logamount;
	}
done:
	i = sizeof(rule);
	if (getsockopt(s, IPPROTO_IP, IP_FW_ADD, &rule, &i) == -1)
		err(EX_UNAVAILABLE, "getsockopt(%s)", "IP_FW_ADD");
	if (!do_quiet)
		show_ipfw(&rule);
}

static void
zero (int ac, char *av[])
{
	struct ip_fw rule;
	int failed = EX_OK;

	av++; ac--;

	if (!ac) {
		/* clear all entries */
		if (setsockopt(s, IPPROTO_IP, IP_FW_ZERO, NULL, 0) < 0)
			err(EX_UNAVAILABLE, "setsockopt(%s)", "IP_FW_ZERO");
		if (!do_quiet)
			printf("Accounting cleared.\n");

		return;
	}

	memset(&rule, 0, sizeof rule);
	while (ac) {
		/* Rule number */
		if (isdigit(**av)) {
			rule.fw_number = atoi(*av); av++; ac--;
			if (setsockopt(s, IPPROTO_IP,
			    IP_FW_ZERO, &rule, sizeof rule)) {
				warn("rule %u: setsockopt(IP_FW_ZERO)",
				    rule.fw_number);
				failed = EX_UNAVAILABLE;
			} else if (!do_quiet)
				printf("Entry %d cleared\n",
				    rule.fw_number);
		} else {
			errx(EX_USAGE, "invalid rule number ``%s''", *av);
		}
	}
	if (failed != EX_OK)
		exit(failed);
}

static void
resetlog (int ac, char *av[])
{
	struct ip_fw rule;
	int failed = EX_OK;

	av++; ac--;

	if (!ac) {
		/* clear all entries */
		if (setsockopt(s, IPPROTO_IP, IP_FW_RESETLOG, NULL, 0) < 0)
			err(EX_UNAVAILABLE, "setsockopt(IP_FW_RESETLOG)");
		if (!do_quiet)
			printf("Logging counts reset.\n");

		return;
	}

	memset(&rule, 0, sizeof rule);
	while (ac) {
		/* Rule number */
		if (isdigit(**av)) {
			rule.fw_number = atoi(*av); av++; ac--;
			if (setsockopt(s, IPPROTO_IP,
			    IP_FW_RESETLOG, &rule, sizeof rule)) {
				warn("rule %u: setsockopt(IP_FW_RESETLOG)",
				    rule.fw_number);
				failed = EX_UNAVAILABLE;
			} else if (!do_quiet)
				printf("Entry %d logging count reset\n",
				    rule.fw_number);
		} else {
			errx(EX_DATAERR, "invalid rule number ``%s''", *av);
		}
	}
	if (failed != EX_OK)
		exit(failed);
}

static void
flush()
{
	if (!do_force && !do_quiet) { /* need to ask user */
		int c;

		printf("Are you sure? [yn] ");
		fflush(stdout);
		do {
			c = toupper(getc(stdin));
			while (c != '\n' && getc(stdin) != '\n')
				if (feof(stdin))
					return; /* and do not flush */
		} while (c != 'Y' && c != 'N');
		printf("\n");
		if (c == 'N')	/* user said no */
			return;
	}
	if (setsockopt(s, IPPROTO_IP,
	    do_pipe ? IP_DUMMYNET_FLUSH : IP_FW_FLUSH, NULL, 0) < 0)
		err(EX_UNAVAILABLE, "setsockopt(IP_%s_FLUSH)",
		    do_pipe ? "DUMMYNET" : "FW");
	if (!do_quiet)
		printf("Flushed all %s.\n", do_pipe ? "pipes" : "rules");
}

static int
ipfw_main(int ac, char **av)
{
	int ch;

	if (ac == 1)
		show_usage();

	/* Set the force flag for non-interactive processes */
	do_force = !isatty(STDIN_FILENO);

	optind = optreset = 1;
	while ((ch = getopt(ac, av, "s:adefNqtv")) != -1)
		switch (ch) {
		case 's': /* sort */
			do_sort = atoi(optarg);
			break;
		case 'a':
			do_acct = 1;
			break;
		case 'd':
			do_dynamic = 1;
			break;
		case 'e':
			do_expired = 1;
			break;
		case 'f':
			do_force = 1;
			break;
		case 'N':
			do_resolv = 1;
			break;
		case 'q':
			do_quiet = 1;
			break;
		case 't':
			do_time = 1;
			break;
		case 'v': /* verbose */
			verbose++;
			break;
		default:
			show_usage();
		}

	ac -= optind;
	av += optind;
	if (*av == NULL)
		 errx(EX_USAGE, "bad arguments, for usage summary ``ipfw''");

	if (!strncmp(*av, "pipe", strlen(*av))) {
		do_pipe = 1;
		ac--;
		av++;
	} else if (!strncmp(*av, "queue", strlen(*av))) {
		do_pipe = 2;
		ac--;
		av++;
	}
	if (!ac)
		errx(EX_USAGE, "pipe requires arguments");

	/* allow argument swapping -- pipe config xxx or pipe xxx config */
	if (ac > 1 && *av[0] >= '0' && *av[0] <= '9') {
		char *p = av[0];
		av[0] = av[1];
		av[1] = p;
	}
	if (!strncmp(*av, "add", strlen(*av)))
		add(ac, av);
	else if (do_pipe && !strncmp(*av, "config", strlen(*av)))
		config_pipe(ac, av);
	else if (!strncmp(*av, "delete", strlen(*av)))
		delete(ac, av);
	else if (!strncmp(*av, "flush", strlen(*av)))
		flush();
	else if (!strncmp(*av, "zero", strlen(*av)))
		zero(ac, av);
	else if (!strncmp(*av, "resetlog", strlen(*av)))
		resetlog(ac, av);
	else if (!strncmp(*av, "print", strlen(*av)))
		list(--ac, ++av);
	else if (!strncmp(*av, "list", strlen(*av)))
		list(--ac, ++av);
	else if (!strncmp(*av, "show", strlen(*av))) {
		do_acct++;
		list(--ac, ++av);
	} else
		errx(EX_USAGE, "bad command `%s'", *av);
	return 0;
}


static void
ipfw_readfile(int ac, char *av[]) 
{
#define MAX_ARGS	32
#define WHITESP		" \t\f\v\n\r"
	char	buf[BUFSIZ];
	char	*a, *p, *args[MAX_ARGS], *cmd = NULL;
	char	linename[10];
	int	i=0, lineno=0, qflag=0, pflag=0, status;
	FILE	*f = NULL;
	pid_t	preproc = 0;
	int	c;

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
			errx(EX_USAGE, "bad arguments, for usage"
			     " summary ``ipfw''");
		}

	av += optind;
	ac -= optind;
	if (ac != 1)
		errx(EX_USAGE, "extraneous filename arguments");

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
			if (dup2(fileno(f), 0) == -1
			    || dup2(pipedes[1], 1) == -1)
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
		i = 1;
		if (qflag)
			args[i++] = "-q";
		for (a = strtok(buf, WHITESP);
		    a && i < MAX_ARGS; a = strtok(NULL, WHITESP), i++)
			args[i] = a;
		if (i == (qflag? 2: 1))
			continue;
		if (i == MAX_ARGS)
			errx(EX_USAGE, "%s: too many arguments",
			    linename);
		args[i] = NULL;

		ipfw_main(i, args);
	}
	fclose(f);
	if (pflag) {
		if (waitpid(preproc, &status, 0) == -1)
			errx(EX_OSERR, "waitpid()");
		if (WIFEXITED(status) && WEXITSTATUS(status) != EX_OK)
			errx(EX_UNAVAILABLE,
			    "preprocessor exited with status %d",
			    WEXITSTATUS(status));
		else if (WIFSIGNALED(status))
			errx(EX_UNAVAILABLE,
			    "preprocessor exited with signal %d",
			    WTERMSIG(status));
	}
}

int
main(int ac, char *av[])
{
	s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
	if (s < 0)
		err(EX_UNAVAILABLE, "socket");

	setbuf(stdout, 0);

	/*
	 * Only interpret the last command line argument as a file to
	 * be preprocessed if it is specified as an absolute pathname.
	 */

	if (ac > 1 && av[ac - 1][0] == '/' && access(av[ac - 1], R_OK) == 0)
		ipfw_readfile(ac, av);
	else
		ipfw_main(ac, av);
	return EX_OK;
}
