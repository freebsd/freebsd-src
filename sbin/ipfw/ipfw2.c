/*
 * Copyright (c) 2002 Luigi Rizzo
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
 * $FreeBSD$
 */

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

#define	IP_MASK_ALL	0xffffffff

/*
 * structure to hold flag names and associated values to be
 * set in the appropriate masks.
 * A NULL string terminates the array.
 * Often, an element with 0 value contains an error string.
 *
 */
struct _s_x {
	char *s;
	int x;
};

static struct _s_x f_tcpflags[] = {
	{ "syn", TH_SYN },
	{ "fin", TH_FIN },
	{ "ack", TH_ACK },
	{ "psh", TH_PUSH },
	{ "rst", TH_RST },
	{ "urg", TH_URG },
	{ "tcp flag", 0 },
	{ NULL,	0 }
};

static struct _s_x f_tcpopts[] = {
	{ "mss",	IP_FW_TCPOPT_MSS },
	{ "maxseg",	IP_FW_TCPOPT_MSS },
	{ "window",	IP_FW_TCPOPT_WINDOW },
	{ "sack",	IP_FW_TCPOPT_SACK },
	{ "ts",		IP_FW_TCPOPT_TS },
	{ "timestamp",	IP_FW_TCPOPT_TS },
	{ "cc",		IP_FW_TCPOPT_CC },
	{ "tcp option",	0 },
	{ NULL,	0 }
};

/*
 * IP options span the range 0 to 255 so we need to remap them
 * (though in fact only the low 5 bits are significant).
 */
static struct _s_x f_ipopts[] = {
	{ "ssrr",	IP_FW_IPOPT_SSRR},
	{ "lsrr",	IP_FW_IPOPT_LSRR},
	{ "rr",		IP_FW_IPOPT_RR},
	{ "ts",		IP_FW_IPOPT_TS},
	{ "ip option",	0 },
	{ NULL,	0 }
};

static struct _s_x f_iptos[] = {
	{ "lowdelay",	IPTOS_LOWDELAY},
	{ "throughput",	IPTOS_THROUGHPUT},
	{ "reliability", IPTOS_RELIABILITY},
	{ "mincost",	IPTOS_MINCOST},
	{ "congestion",	IPTOS_CE},
	{ "ecntransport", IPTOS_ECT},
	{ "ip tos option", 0},
	{ NULL,	0 }
};

static struct _s_x limit_masks[] = {
	{"all",		DYN_SRC_ADDR|DYN_SRC_PORT|DYN_DST_ADDR|DYN_DST_PORT},
	{"src-addr",	DYN_SRC_ADDR},
	{"src-port",	DYN_SRC_PORT},
	{"dst-addr",	DYN_DST_ADDR},
	{"dst-port",	DYN_DST_PORT},
	{NULL,		0}
};

/*
 * we use IPPROTO_ETHERTYPE as a fake protocol id to call the print routines
 * This is only used in this code.
 */
#define IPPROTO_ETHERTYPE	0x1000
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

enum tokens {
	TOK_NULL=0,

	TOK_OR,
	TOK_NOT,

	TOK_ACCEPT,
	TOK_COUNT,
	TOK_PIPE,
	TOK_QUEUE,
	TOK_DIVERT,
	TOK_TEE,
	TOK_FORWARD,
	TOK_SKIPTO,
	TOK_DENY,
	TOK_REJECT,
	TOK_RESET,
	TOK_UNREACH,
	TOK_CHECKSTATE,

	TOK_UID,
	TOK_GID,
	TOK_IN,
	TOK_LIMIT,
	TOK_KEEPSTATE,
	TOK_LAYER2,
	TOK_OUT,
	TOK_XMIT,
	TOK_RECV,
	TOK_VIA,
	TOK_FRAG,
	TOK_IPOPTS,
	TOK_IPLEN,
	TOK_IPID,
	TOK_IPPRECEDENCE,
	TOK_IPTOS,
	TOK_IPTTL,
	TOK_IPVER,
	TOK_ESTAB,
	TOK_SETUP,
	TOK_TCPFLAGS,
	TOK_TCPOPTS,
	TOK_TCPSEQ,
	TOK_TCPACK,
	TOK_TCPWIN,
	TOK_ICMPTYPES,

	TOK_PLR,
	TOK_BUCKETS,
	TOK_DSTIP,
	TOK_SRCIP,
	TOK_DSTPORT,
	TOK_SRCPORT,
	TOK_ALL,
	TOK_MASK,
	TOK_BW,
	TOK_DELAY,
	TOK_RED,
	TOK_GRED,
	TOK_DROPTAIL,
	TOK_PROTO,
	TOK_WEIGHT,
};

struct _s_x dummynet_params[] = {
	{ "plr",		TOK_PLR },
	{ "buckets",		TOK_BUCKETS },
	{ "dst-ip",		TOK_DSTIP },
	{ "src-ip",		TOK_SRCIP },
	{ "dst-port",		TOK_DSTPORT },
	{ "src-port",		TOK_SRCPORT },
	{ "proto",		TOK_PROTO },
	{ "weight",		TOK_WEIGHT },
	{ "all",		TOK_ALL },
	{ "mask",		TOK_MASK },
	{ "droptail",		TOK_DROPTAIL },
	{ "red",		TOK_RED },
	{ "gred",		TOK_GRED },
	{ "bw",			TOK_BW },
	{ "bandwidth",		TOK_BW },
	{ "delay",		TOK_DELAY },
	{ "pipe",		TOK_PIPE },
	{ "queue",		TOK_QUEUE },
	{ "dummynet-params",	TOK_NULL },
	{ NULL, 0 }
};

struct _s_x rule_actions[] = {
	{ "accept",		TOK_ACCEPT },
	{ "pass",		TOK_ACCEPT },
	{ "allow",		TOK_ACCEPT },
	{ "permit",		TOK_ACCEPT },
	{ "count",		TOK_COUNT },
	{ "pipe",		TOK_PIPE },
	{ "queue",		TOK_QUEUE },
	{ "divert",		TOK_DIVERT },
	{ "tee",		TOK_TEE },
	{ "fwd",		TOK_FORWARD },
	{ "forward",		TOK_FORWARD },
	{ "skipto",		TOK_SKIPTO },
	{ "deny",		TOK_DENY },
	{ "drop",		TOK_DENY },
	{ "reject",		TOK_REJECT },
	{ "reset",		TOK_RESET },
	{ "unreach",		TOK_UNREACH },
	{ "check-state",	TOK_CHECKSTATE },
	{ NULL,			TOK_NULL },
	{ NULL, 0 }
};

struct _s_x rule_options[] = {
	{ "uid",		TOK_UID },
	{ "gid",		TOK_GID },
	{ "in",			TOK_IN },
	{ "limit",		TOK_LIMIT },
	{ "keep-state",		TOK_KEEPSTATE },
	{ "bridged",		TOK_LAYER2 },
	{ "layer2",		TOK_LAYER2 },
	{ "out",		TOK_OUT },
	{ "xmit",		TOK_XMIT },
	{ "recv",		TOK_RECV },
	{ "via",		TOK_VIA },
	{ "fragment",		TOK_FRAG },
	{ "frag",		TOK_FRAG },
	{ "ipoptions",		TOK_IPOPTS },
	{ "ipopts",		TOK_IPOPTS },
	{ "iplen",		TOK_IPLEN },
	{ "ipid",		TOK_IPID },
	{ "ipprecedence",	TOK_IPPRECEDENCE },
	{ "iptos",		TOK_IPTOS },
	{ "ipttl",		TOK_IPTTL },
	{ "ipversion",		TOK_IPVER },
	{ "ipver",		TOK_IPVER },
	{ "estab",		TOK_ESTAB },
	{ "established",	TOK_ESTAB },
	{ "setup",		TOK_SETUP },
	{ "tcpflags",		TOK_TCPFLAGS },
	{ "tcpflgs",		TOK_TCPFLAGS },
	{ "tcpoptions",		TOK_TCPOPTS },
	{ "tcpopts",		TOK_TCPOPTS },
	{ "tcpseq",		TOK_TCPSEQ },
	{ "tcpack",		TOK_TCPACK },
	{ "tcpwin",		TOK_TCPWIN },
	{ "icmptype",		TOK_ICMPTYPES },
	{ "icmptypes",		TOK_ICMPTYPES },

	{ "not",		TOK_NOT },		/* pseudo option */
	{ "!", /* escape ? */	TOK_NOT },		/* pseudo option */
	{ "or",			TOK_OR },		/* pseudo option */
	{ "|", /* escape */	TOK_OR },		/* pseudo option */
	{ NULL,			TOK_NULL },
	{ NULL, 0 }
};

/**
 * match_token takes a table and a string, returns the value associated
 * with the string (0 meaning an error in most cases)
 */
static int
match_token(struct _s_x *table, char *string)
{
	struct _s_x *pt;
	int i = strlen(string);

	for (pt = table ; i && pt->s != NULL ; pt++)
		if (strlen(pt->s) == i && !bcmp(string, pt->s, i))
			return pt->x;
	return -1;
};

static char *
match_value(struct _s_x *p, u_int32_t value)
{
	for (; p->s != NULL; p++)
		if (p->x == value)
			return p->s;
	return NULL;
}

/*
 * prints one port, symbolic or numeric
 */
static void
print_port(int proto, u_int16_t port)
{

	if (proto == IPPROTO_ETHERTYPE) {
		char *s;

		if (do_resolv && (s = match_value(ether_types, port)) )
			printf("%s", s);
		else
			printf("0x%04x", port);
	} else {
		struct servent *se = NULL;
		if (do_resolv) {
			struct protoent *pe = getprotobynumber(proto);

			se = getservbyport(htons(port), pe ? pe->p_name : NULL);
		}
		if (se)
			printf("%s", se->s_name);
		else
			printf("%d", port);
	}
}

/*
 * print the values in a list of ports
 * XXX todo: add support for mask.
 */
static void
print_newports(ipfw_insn_u16 *cmd, int proto)
{
	u_int16_t *p = cmd->ports;
	int i;
	char *sep= " ";

	if (cmd->o.len & F_NOT)
		printf(" not");
	for (i = F_LEN((ipfw_insn *)cmd) - 1; i > 0; i--, p += 2) {
		printf(sep);
		print_port(proto, p[0]);
		if (p[0] != p[1]) {
			printf("-");
			print_port(proto, p[1]);
		}
		sep = ",";
	}
}

/*
 * Like strtol, but also translates service names into port numbers
 * for some protocols.
 * In particular:
 *	proto == -1 disables the protocol check;
 *	proto == IPPROTO_ETHERTYPE looks up an internal table
 *	proto == <some value in /etc/protocols> matches the values there.
 */
static int
strtoport(char *s, char **end, int base, int proto)
{
	char *s1, sep;
	int i;

	if ( *s == '\0')
		goto none;
		
	if (isdigit(*s))
		return strtol(s, end, base);

	/*
	 * find separator and replace with a '\0'
	 */
	for (s1 = s; *s1 && isalnum(*s1) ; s1++)
		;
	sep = *s1;
	*s1 = '\0';

	if (proto == IPPROTO_ETHERTYPE) {
		i = match_token(ether_types, s);
		*s1 = sep;
		if (i == -1) {	/* not found */
			*end = s;
			return 0;
		} else {
			*end = s1;
			return i;
		}
	} else {
		struct protoent *pe = NULL;
		struct servent *se;

		if (proto != 0)
			pe = getprotobynumber(proto);
		setservent(1);
		se = getservbyname(s, pe ? pe->p_name : NULL);
		*s1 = sep;
		if (se != NULL) {
			*end = s1;
			return ntohs(se->s_port);
		}
	}
none:
	*end = s;
	return 0;
}

/*
 * fill the body of the command with the list of port ranges.
 * At the moment it only understands numeric ranges.
 */
static int
fill_newports(ipfw_insn_u16 *cmd, char *av, int proto)
{
	u_int16_t *p = cmd->ports;
	int i = 0;

	for (; *av ; i++, p +=2 ) {
		u_int16_t a, b;
		char *s;

		a = strtoport(av, &s, 0, proto);
		if (s == av) /* no parameter */
			break;
		if (*s == '-') { /* a range */
			av = s+1;
			b = strtoport(av, &s, 0, proto);
			if (s == av) /* no parameter */
				break;
			p[0] = a;
			p[1] = b;
		} else if (*s == ',' || *s == '\0' ) {
			p[0] = p[1] = a;
		} else	/* invalid separator */
			break;
		av = s+1;
	}
	if (i > 0) {
		if (i+1 > F_LEN_MASK)
			errx(EX_DATAERR, "too many port range\n");
		cmd->o.len |= i+1; /* leave F_NOT and F_OR untouched */
	}
	return i;
}

static struct _s_x icmpcodes[] = {
      { "net",			ICMP_UNREACH_NET },
      { "host",			ICMP_UNREACH_HOST },
      { "protocol",		ICMP_UNREACH_PROTOCOL },
      { "port",			ICMP_UNREACH_PORT },
      { "needfrag",		ICMP_UNREACH_NEEDFRAG },
      { "srcfail",		ICMP_UNREACH_SRCFAIL },
      { "net-unknown",		ICMP_UNREACH_NET_UNKNOWN },
      { "host-unknown",		ICMP_UNREACH_HOST_UNKNOWN },
      { "isolated",		ICMP_UNREACH_ISOLATED },
      { "net-prohib",		ICMP_UNREACH_NET_PROHIB },
      { "host-prohib",		ICMP_UNREACH_HOST_PROHIB },
      { "tosnet",		ICMP_UNREACH_TOSNET },
      { "toshost",		ICMP_UNREACH_TOSHOST },
      { "filter-prohib",	ICMP_UNREACH_FILTER_PROHIB },
      { "host-precedence",	ICMP_UNREACH_HOST_PRECEDENCE },
      { "precedence-cutoff",	ICMP_UNREACH_PRECEDENCE_CUTOFF },
      { NULL, 0 }
};

static void
fill_reject_code(u_short *codep, char *str)
{
	int val;
	char *s;

	val = strtoul(str, &s, 0);
	if (s == str || *s != '\0' || val >= 0x100)
		val = match_token(icmpcodes, str);
	if (val <= 0)
		errx(EX_DATAERR, "unknown ICMP unreachable code ``%s''", str);
	*codep = val;
	return;
}

static void
print_reject_code(u_int16_t code)
{
	char *s = match_value(icmpcodes, code);

	if (s != NULL)
		printf("unreach %s", s);
	else
		printf("unreach %u", code);
}

/*
 * Returns the number of bits set (from left) in a contiguous bitmask,
 * or -1 if the mask is not contiguous.
 * XXX this needs a proper fix.
 * This effectively works on masks in big-endian (network) format.
 * when compiled on little endian architectures.
 *
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
		if ( (p[n/8] & (1 << (7 - (n%8)))) != 0)
			return -1; /* mask not contiguous */
	return i;
}

/*
 * print flags set/clear in the two bitmasks passed as parameters.
 * There is a specialized check for f_tcpflags.
 */
static void
print_flags(char *name, ipfw_insn *cmd, struct _s_x *list)
{
	char *comma="";
	int i;
	u_char set = cmd->arg1 & 0xff;
	u_char clear = (cmd->arg1 >> 8) & 0xff;

	if (list == f_tcpflags && set == TH_SYN && clear == TH_ACK) {
		printf(" setup");
		return;
	}

	printf(" %s ", name);
	for (i=0; list[i].x != 0; i++) {
		if (set & list[i].x) {
			set &= ~list[i].x;
			printf("%s%s", comma, list[i].s);
			comma = ",";
		}
		if (clear & list[i].x) {
			clear &= ~list[i].x;
			printf("%s!%s", comma, list[i].s);
			comma = ",";
		}
	}
}

/*
 * Print the ip address contained in a command.
 */
static void
print_ip(ipfw_insn_ip *cmd)
{
	struct hostent *he = NULL;
	int mb;

	printf("%s ", cmd->o.len & F_NOT ? " not": "");

	if (cmd->o.opcode == O_IP_SRC_ME || cmd->o.opcode == O_IP_DST_ME) {
		printf("me");
		return;
	}
	if (cmd->o.opcode == O_IP_SRC_SET || cmd->o.opcode == O_IP_DST_SET) {
		u_int32_t x, *d;
		int i;
		char comma = '{';

		x = cmd->o.arg1 - 1;
		x = htonl( ~x );
		cmd->addr.s_addr = htonl(cmd->addr.s_addr);
		printf("%s/%d", inet_ntoa(cmd->addr),
			contigmask((u_char *)&x, 32));
		x = cmd->addr.s_addr = htonl(cmd->addr.s_addr);
		x &= 0xff; /* base */
		d = (u_int32_t *)&(cmd->mask);
		for (i=0; i < cmd->o.arg1; i++)
			if (d[ i/32] & (1<<(i & 31))) {
				printf("%c%d", comma, i+x);
				comma = ',';
			}
		printf("}");
		return;
	}
	if (cmd->o.opcode == O_IP_SRC || cmd->o.opcode == O_IP_DST)
		mb = 32;
	else
		mb = contigmask((u_char *)&(cmd->mask.s_addr), 32);
	if (mb == 32 && do_resolv)
		he = gethostbyaddr((char *)&(cmd->addr.s_addr),
		    sizeof(u_long), AF_INET);
	if (he != NULL)		/* resolved to name */
		printf("%s", he->h_name);
	else if (mb == 0)	/* any */
		printf("any");
	else {		/* numeric IP followed by some kind of mask */
		printf("%s", inet_ntoa(cmd->addr));
		if (mb < 0)
			printf(":%s", inet_ntoa(cmd->mask));
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
fill_icmptypes(ipfw_insn_u32 *cmd, char *av)
{
	u_int8_t type;

	cmd->d[0] = 0;
	while (*av) {
		if (*av == ',')
			av++;

		type = strtoul(av, &av, 0);

		if (*av != ',' && *av != '\0')
			errx(EX_DATAERR, "invalid ICMP type");

		if (type > 31)
			errx(EX_DATAERR, "ICMP type out of range");

		cmd->d[0] |= 1 << type;
	}
	cmd->o.opcode = O_ICMPTYPE;
	cmd->o.len |= F_INSN_SIZE(ipfw_insn_u32);
}

static void
print_icmptypes(ipfw_insn_u32 *cmd)
{
	int i;
	char sep= ' ';

	printf(" icmptypes");
	for (i = 0; i < 32; i++) {
		if ( (cmd->d[0] & (1 << (i))) == 0)
			continue;
		printf("%c%d", sep, i);
		sep = ',';
	}
}

/*
 * show_ipfw() prints the body of an ipfw rule.
 * Because the standard rule has at least proto src_ip dst_ip, we use
 * a helper function to produce these entries if not provided explicitly.
 */
#define	HAVE_PROTO	1
#define	HAVE_SRCIP	2
#define	HAVE_DSTIP	4
#define	HAVE_MAC	8

static void
show_prerequisites(int *flags, int want)
{
	if ( !(*flags & HAVE_PROTO) && (want & HAVE_PROTO))
		printf(" ip");
	if ( !(*flags & HAVE_SRCIP) && (want & HAVE_SRCIP))
		printf(" from any");
	if ( !(*flags & HAVE_DSTIP) && (want & HAVE_DSTIP))
		printf(" to any");
	*flags |= want;
}

static void
show_ipfw(struct ip_fw *rule)
{
	int l;
	ipfw_insn *cmd;
	int proto = 0;		/* default */
	int flags = 0;	/* prerequisites */
	ipfw_insn_log *logptr = NULL; /* set if we find an O_LOG */
	int or_block = 0;	/* we are in an or block */

	printf("%05u ", rule->rulenum);

	if (do_acct)
		printf("%10qu %10qu ", rule->pcnt, rule->bcnt);

	if (do_time) {
		if (rule->timestamp) {
			char timestr[30];
#if _FreeBSD_version < 500000 /* XXX check */
#define	_long_to_time(x)	(time_t)(x)
#endif
			time_t t = _long_to_time(rule->timestamp);

			strcpy(timestr, ctime(&t));
			*strchr(timestr, '\n') = '\0';
			printf("%s ", timestr);
		} else {
			printf("			 ");
		}
	}

	/*
	 * first print actions
	 */
        for (l = rule->cmd_len - rule->act_ofs, cmd = ACTION_PTR(rule);
			l > 0 ; l -= F_LEN(cmd), cmd += F_LEN(cmd)) {
		switch(cmd->opcode) {
		case O_CHECK_STATE:
			printf("check-state");
			/* avoid printing anything else */
			flags = HAVE_PROTO|HAVE_SRCIP|HAVE_DSTIP;
			break;

		case O_PROB:
		    {
			ipfw_insn_u32 *p = (ipfw_insn_u32 *)cmd;
			double d = 1.0 * p->d[0];

			d = 1 - (d / 0x7fffffff);
			printf("prob %f ", d);
		    }
			break;

		case O_ACCEPT:
			printf("allow");
			break;

		case O_COUNT:
			printf("count");
			break;

		case O_DENY:
			printf("deny");
			break;

		case O_REJECT:
			if (cmd->arg1 == ICMP_REJECT_RST)
				printf("reset");
			else if (cmd->arg1 == ICMP_UNREACH_HOST)
				printf("reject");
			else
				print_reject_code(cmd->arg1);
			break;

		case O_SKIPTO:
			printf("skipto %u", cmd->arg1);
			break;

		case O_PIPE:
			printf("pipe %u", cmd->arg1);
			break;

		case O_QUEUE:
			printf("queue %u", cmd->arg1);
			break;

		case O_DIVERT:
			printf("divert %u", cmd->arg1);
			break;

		case O_TEE:
			printf("tee %u", cmd->arg1);
			break;

		case O_FORWARD_IP:
		    {
			ipfw_insn_sa *s = (ipfw_insn_sa *)cmd;

			printf("fwd %s", inet_ntoa(s->sa.sin_addr));
			if (s->sa.sin_port)
				printf(",%d", ntohs(s->sa.sin_port));
		    }
			break;

		case O_LOG: /* O_LOG is printed last */
			logptr = (ipfw_insn_log *)cmd;
			break;

		default:
			printf("** unrecognized action %d len %d",
				cmd->opcode, cmd->len);
		}
	}
	if (logptr) {
		if (logptr->max_log > 0)
			printf(" log logamount %d", logptr->max_log);
		else
			printf(" log");
	}
	/*
	 * then print the body
	 */
        for (l = rule->act_ofs, cmd = rule->cmd ;
			l > 0 ; l -= F_LEN(cmd) , cmd += F_LEN(cmd)) {
		/* useful alias */
		ipfw_insn_u32 *cmd32 = (ipfw_insn_u32 *)cmd;

		switch(cmd->opcode) {
		case O_PROBE_STATE:
			break; /* no need to print anything here */

		case O_MACADDR2: {
			ipfw_insn_mac *m = (ipfw_insn_mac *)cmd;
			if ( (flags & HAVE_MAC) == 0)
				printf(" MAC");
			flags |= HAVE_MAC;
			if (cmd->len & F_NOT)
				printf(" not");
			print_mac( m->addr, m->mask);
			print_mac( m->addr + 6, m->mask + 6);
			}
			break;

		case O_MAC_TYPE:
			print_newports((ipfw_insn_u16 *)cmd, IPPROTO_ETHERTYPE);
			break;

		case O_IP_SRC:
		case O_IP_SRC_MASK:
		case O_IP_SRC_ME:
		case O_IP_SRC_SET:
			show_prerequisites(&flags, HAVE_PROTO);
			if (!(flags & HAVE_SRCIP))
				printf(" from");
			if ((cmd->len & F_OR) && !or_block)
				printf(" {");
			print_ip((ipfw_insn_ip *)cmd);
			flags |= HAVE_SRCIP;
			break;

		case O_IP_DST:
		case O_IP_DST_MASK:
		case O_IP_DST_ME:
		case O_IP_DST_SET:
			show_prerequisites(&flags, HAVE_PROTO|HAVE_SRCIP);
			if (!(flags & HAVE_DSTIP))
				printf(" to");
			if ((cmd->len & F_OR) && !or_block)
				printf(" {");
			print_ip((ipfw_insn_ip *)cmd);
			flags |= HAVE_DSTIP;
			break;

		case O_IP_DSTPORT:
			show_prerequisites(&flags,
				HAVE_PROTO|HAVE_SRCIP|HAVE_DSTIP);
		case O_IP_SRCPORT:
			show_prerequisites(&flags, HAVE_PROTO|HAVE_SRCIP);
			print_newports((ipfw_insn_u16 *)cmd, proto);
			break;

		case O_PROTO: {
			struct protoent *pe;

			if ((cmd->len & F_OR) && !or_block)
				printf(" {");
			if (cmd->len & F_NOT)
				printf(" not");
			proto = cmd->arg1;
			pe = getprotobynumber(cmd->arg1);
			if (pe)
				printf(" %s", pe->p_name);
			else
				printf(" %u", cmd->arg1);
			}
			flags |= HAVE_PROTO;
			break;
		
		default: /*options ... */
			show_prerequisites(&flags,
			    HAVE_PROTO|HAVE_SRCIP|HAVE_DSTIP);
			if ((cmd->len & F_OR) && !or_block)
				printf(" {");
			if (cmd->len & F_NOT && cmd->opcode != O_IN)
				printf(" not");
			switch(cmd->opcode) {
			case O_FRAG:
				printf(" frag");
				break;

			case O_IN:
				printf(cmd->len & F_NOT ? " out" : " in");
				break;

			case O_LAYER2:
				printf(" layer2");
				break;
			case O_XMIT:
			case O_RECV:
			case O_VIA: {
				char *s;
				ipfw_insn_if *cmdif = (ipfw_insn_if *)cmd;

				if (cmd->opcode == O_XMIT)
					s = "xmit";
				else if (cmd->opcode == O_RECV)
					s = "recv";
				else if (cmd->opcode == O_VIA)
					s = "via";
				if (cmdif->name[0] == '\0')
					printf(" %s %s", s,
					    inet_ntoa(cmdif->p.ip));
				else if (cmdif->p.unit == -1)
					printf(" %s %s*", s, cmdif->name);
				else
					printf(" %s %s%d", s, cmdif->name,
					    cmdif->p.unit);
				}
				break;

			case O_IPID:
				printf(" ipid %u", cmd->arg1 );
				break;

			case O_IPTTL:
				printf(" ipttl %u", cmd->arg1 );
				break;

			case O_IPVER:
				printf(" ipver %u", cmd->arg1 );
				break;

			case O_IPPRECEDENCE:
				printf(" ipprecedence %u", (cmd->arg1) >> 5 );
				break;

			case O_IPLEN:
				printf(" iplen %u", cmd->arg1 );
				break;

			case O_IPOPT:
				print_flags("ipoptions", cmd, f_ipopts);
				break;

			case O_IPTOS:
				print_flags("iptos", cmd, f_iptos);
				break;

			case O_ICMPTYPE:
				print_icmptypes((ipfw_insn_u32 *)cmd);
				break;

			case O_ESTAB:
				printf(" established");
				break;

			case O_TCPFLAGS:
				print_flags("tcpflags", cmd, f_tcpflags);
				break;

			case O_TCPOPTS:
				print_flags("tcpoptions", cmd, f_tcpopts);
				break;

			case O_TCPWIN:
				printf(" tcpwin %d", ntohs(cmd->arg1));
				break;

			case O_TCPACK:
				printf(" tcpack %d", ntohl(cmd32->d[0]));
				break;

			case O_TCPSEQ:
				printf(" tcpseq %d", ntohl(cmd32->d[0]));
				break;

			case O_UID:
			    {
				struct passwd *pwd = getpwuid(cmd32->d[0]);

				if (pwd)
					printf(" uid %s", pwd->pw_name);
				else
					printf(" uid %u", cmd32->d[0]);
			    }
				break;

			case O_GID:
			    {
				struct group *grp = getgrgid(cmd32->d[0]);

				if (grp)
					printf(" gid %s", grp->gr_name);
				else
					printf(" gid %u", cmd32->d[0]);
			    }
				break;

			case O_KEEP_STATE:
				printf(" keep-state");
				break;

			case O_LIMIT:
			    {
				struct _s_x *p = limit_masks;
				ipfw_insn_limit *c = (ipfw_insn_limit *)cmd;
				u_int8_t x = c->limit_mask;
				char *comma = " ";

				printf(" limit");
				for ( ; p->x != 0 ; p++) 
					if ((x & p->x) == p->x) {
						x &= ~p->x;
						printf("%s%s", comma, p->s);
						comma = ",";
					}
				printf(" %d", c->conn_limit);
			    }
				break;

			default:
				printf(" [opcode %d len %d]",
				    cmd->opcode, cmd->len);
			}
		}
		if (cmd->len & F_OR) {
			printf(" or");
			or_block = 1;
		} else if (or_block) {
			printf(" }");
			or_block = 0;
		}
	}
	show_prerequisites(&flags, HAVE_PROTO|HAVE_SRCIP|HAVE_DSTIP);

	printf("\n");
}

static void
show_dyn_ipfw(ipfw_dyn_rule *d)
{
	struct protoent *pe;
	struct in_addr a;

	if (!do_expired) {
		if (!d->expire && !(d->dyn_type == O_LIMIT_PARENT))
			return;
	}

	printf("%05d %10qu %10qu (%ds)",
	    (int)(d->rule), d->pcnt, d->bcnt, d->expire);
	switch (d->dyn_type) {
	case O_LIMIT_PARENT:
		printf(" PARENT %d", d->count);
		break;
	case O_LIMIT:
		printf(" LIMIT");
		break;
	case O_KEEP_STATE: /* bidir, no mask */
		printf(" STATE"); 
		break;
	}

	if ((pe = getprotobynumber(d->id.proto)) != NULL)
		printf(" %s", pe->p_name);
	else
		printf(" proto %u", d->id.proto);

	a.s_addr = htonl(d->id.src_ip);
	printf(" %s %d", inet_ntoa(a), d->id.src_port);

	a.s_addr = htonl(d->id.dst_ip);
	printf(" <-> %s %d", inet_ntoa(a), d->id.dst_port);
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
list_pipes(void *data, int nbytes, int ac, char *av[])
{
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
			break;	/* done with pipes, now queues */

		/*
		 * compute length, as pipe have variable size
		 */
		l = sizeof(*p) + p->fs.rq_elements * sizeof(*q);
		next = (void *)p + l;
		nbytes -= l;

		if (rulenum != 0 && rulenum != p->pipe_nr)
			continue;

		/*
		 * Print rate (or clocking interface)
		 */
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
		
		q = (struct dn_flow_queue *)(p+1);
		list_queues(&(p->fs), q);
	}
	for (fs = next; nbytes >= sizeof *fs; fs = next) {
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
}

static void
list(int ac, char *av[])
{
	struct ip_fw *r;
	ipfw_dyn_rule *dynrules, *d;

	void *lim, *data = NULL;
	int n, nbytes, nstat, ndyn;
	int exitval = EX_OK;
	int lac;
	char **lav;
	u_long rnum;
	char *endptr;
	int seen = 0;

	const int ocmd = do_pipe ? IP_DUMMYNET_GET : IP_FW_GET;
	int nalloc = 1024;	/* start somewhere... */

	ac--;
	av++;

	/* get rules or pipes from kernel, resizing array as necessary */
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

	if (do_pipe) {
		list_pipes(data, nbytes, ac, av);
		goto done;
	}

	/*
	 * Count static rules. They have variable size so we
	 * need to scan the list to count them.
	 */
	for (nstat = 1, r = data, lim = data + nbytes;
		    r->rulenum < 65535 && (void *)r < lim;
		    ++nstat, r = (void *)r + RULESIZE(r) )
		; /* nothing */

	/*
	 * Count dynamic rules. This is easier as they have
	 * fixed size.
	 */
	r = (void *)r + RULESIZE(r);
	dynrules = (ipfw_dyn_rule *)r ;
	n = (void *)r - data;
	ndyn = (nbytes - n) / sizeof *dynrules;

	/* if no rule numbers were specified, list all rules */
	if (ac == 0) {
		for (n = 0, r = data; n < nstat;
		    n++, r = (void *)r + RULESIZE(r) )
			show_ipfw(r);

		if (do_dynamic && ndyn) {
			printf("## Dynamic rules (%d):\n", ndyn);
			for (n = 0, d = dynrules; n < ndyn; n++, d++)
				show_dyn_ipfw(d);
		}
		goto done;
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
		for (n = seen = 0, r = data; n < nstat;
		    n++, r = (void *)r + RULESIZE(r) ) {
			if (r->rulenum > rnum)
				break;
			if (r->rulenum == rnum) {
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

	if (do_dynamic && ndyn) {
		printf("## Dynamic rules:\n");
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

done:
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
"do \"ipfw -h\" or see ipfw manpage for details\n"
);

	exit(EX_USAGE);
}

static void
help(void)
{
	
	fprintf(stderr, "ipfw syntax summary:\n"
"ipfw add [N] [prob {0..1}] ACTION [log [logamount N]] ADDR OPTIONS\n"
"ipfw {pipe|queue} N config BODY\n"
"ipfw [pipe] {zero|delete|show} [N{,N}]\n"
"\n"
"RULE:		[1..] [PROB] BODY\n"
"RULENUM:	INTEGER(1..65534)\n"
"PROB:		prob REAL(0..1)\n"
"BODY:		check-state [LOG] (no body) |\n"
"		ACTION [LOG] MATCH_ADDR [OPTION_LIST]\n"
"ACTION:	check-state | allow | count | deny | reject | skipto N |\n"
"		{divert|tee} PORT | forward ADDR | pipe N | queue N\n"
"ADDR:		[ MAC dst src ether_type ] \n"
"		[ from IPLIST [ PORT ] to IPLIST [ PORTLIST ] ]\n"
"IPLIST:	IPADDR | ( IPADDR or ... or IPADDR )\n"
"IPADDR:	[not] { any | me | ip | ip/bits | ip:mask | ip/bits{x,y,z} }\n"
"OPTION_LIST:	OPTION [,OPTION_LIST]\n"
);
exit(0);
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

/*
 * fills the addr and mask fields in the instruction as appropriate from av.
 * Update length as appropriate.
 * The following formats are allowed:
 *	any	matches any IP. Actually returns an empty instruction.
 *	me	returns O_IP_*_ME
 *	1.2.3.4		single IP address
 *	1.2.3.4:5.6.7.8	address:mask
 *	1.2.3.4/24	address/mask
 *	1.2.3.4/26{1,6,5,4,23}	set of addresses in a subnet
 */
static void
fill_ip(ipfw_insn_ip *cmd, char *av)
{
	char *p = 0, md = 0;
	u_int32_t i;

	cmd->o.len &= ~F_LEN_MASK;	/* zero len */

	if (!strncmp(av, "any", strlen(av)))
		return;

	if (!strncmp(av, "me", strlen(av))) {
		cmd->o.len |= F_INSN_SIZE(ipfw_insn);
		return;
	}

	p = strchr(av, '/');
	if (!p)
		p = strchr(av, ':');
	if (p) {
		md = *p;
		*p++ = '\0';
	}

	if (lookup_host(av, &cmd->addr) != 0)
		errx(EX_NOHOST, "hostname ``%s'' unknown", av);
	switch (md) {
	case ':':
		if (!inet_aton(p, &cmd->mask))
			errx(EX_DATAERR, "bad netmask ``%s''", p);
		break;
	case '/':
		i = atoi(p);
		if (i == 0)
			cmd->mask.s_addr = htonl(0);
		else if (i > 32)
			errx(EX_DATAERR, "bad width ``%s''", p);
		else
			cmd->mask.s_addr = htonl(~0 << (32 - i));
		break;
	default:
		cmd->mask.s_addr = htonl(~0);
		break;
	}
	cmd->addr.s_addr &= cmd->mask.s_addr;
	/*
	 * now look if we have a set of addresses. They are stored as follows:
	 *   arg1	is the set size (powers of 2, 2..256)
	 *   addr	is the base address IN HOST FORMAT
	 *   mask..	is an array of u_int32_t with bits set.
	 */
	if (p)
		p = strchr(p, '{');
	if (p) {	/* fetch addresses */
		u_int32_t *d;
		int low, high;
		int i = contigmask((u_char *)&(cmd->mask), 32);

		if (i < 24 || i > 31) {
			fprintf(stderr, "invalid set with mask %d\n",
				i);
			exit(0);
		}
		cmd->o.arg1 = 1<<(32-i);
		cmd->addr.s_addr = ntohl(cmd->addr.s_addr);
		d = (u_int32_t *)&cmd->mask;
		cmd->o.opcode = O_IP_DST_SET;	/* default */
		cmd->o.len |= F_INSN_SIZE(ipfw_insn_u32) + (cmd->o.arg1+31)/32;
		for (i = 0; i < (cmd->o.arg1+31)/32 ; i++)
			d[i] = 0;	/* clear masks */

		av = p+1;
		low = cmd->addr.s_addr & 0xff;
		high = low + cmd->o.arg1 - 1;
		while (isdigit(*av)) {
			char *s;
			u_int16_t a = strtol(av, &s, 0);

			if (s == av) /* no parameter */
				break;
			if (a < low || a > high) {
			    fprintf(stderr, "addr %d out of range [%d-%d]\n",
				a, low, high);
			    exit(0);
			}
			a -= low;
			d[ a/32] |= 1<<(a & 31);
			if (*s != ',')
				break;
			av = s+1;
		}
		return;
	}

	if (cmd->mask.s_addr == 0) { /* any */
		if (cmd->o.len & F_NOT)
			errx(EX_DATAERR, "not any never matches");
		else	/* useless, nuke it */
			return;
	} else if (cmd->mask.s_addr ==  IP_MASK_ALL)	/* one IP */
		cmd->o.len |= F_INSN_SIZE(ipfw_insn_u32);
	else						/* addr/mask */
		cmd->o.len |= F_INSN_SIZE(ipfw_insn_ip);
}


/*
 * helper function to process a set of flags and set bits in the
 * appropriate masks.
 */
static void
fill_flags(ipfw_insn *cmd, enum ipfw_opcodes opcode,
	struct _s_x *flags, char *p)
{
	u_int8_t set=0, clear=0;

	while (p && *p) {
		char *q;	/* points to the separator */
		int val;
		u_int8_t *which;	/* mask we are working on */

		if (*p == '!') {
			p++;
			which = &clear;
		} else
			which = &set;
		q = strchr(p, ',');
		if (q)
			*q++ = '\0';
		val = match_token(flags, p);
		if (val <= 0)
			errx(EX_DATAERR, "invalid flag %s", p);
		*which |= (u_int8_t)val;
		p = q;
	}
        cmd->opcode = opcode;
        cmd->len =  (cmd->len & (F_NOT | F_OR)) | 1;
        cmd->arg1 = (set & 0xff) | ( (clear & 0xff) << 8);
}


static void
delete(int ac, char *av[])
{
	int rulenum;
	struct dn_pipe pipe;
	int i;
	int exitval = EX_OK;

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
			rulenum = i;
			i = setsockopt(s, IPPROTO_IP, IP_FW_DEL, &rulenum,
			    sizeof rulenum);
			if (i) {
				exitval = EX_UNAVAILABLE;
				warn("rule %u: setsockopt(IP_FW_DEL)",
				    rulenum);
			}
		}
	}
	if (exitval != EX_OK)
		exit(exitval);
}


/*
 * fill the interface structure. We do not check the name as we can
 * create interfaces dynamically, so checking them at insert time
 * makes relatively little sense.
 * A '*' following the name means any unit.
 */
static void
fill_iface(ipfw_insn_if *cmd, char *arg)
{
	cmd->name[0] = '\0';
	cmd->o.len |= F_INSN_SIZE(ipfw_insn_if);

	/* Parse the interface or address */
	if (!strcmp(arg, "any"))
		cmd->o.len = 0;		/* effectively ignore this command */
	else if (!isdigit(*arg)) {
		char *q;

		strncpy(cmd->name, arg, sizeof(cmd->name));
		cmd->name[sizeof(cmd->name) - 1] = '\0';
		/* find first digit or wildcard */
		for (q = cmd->name; *q && !isdigit(*q) && *q != '*'; q++)
			continue;
		cmd->p.unit = (*q == '*') ? -1 : atoi(q);
		*q = '\0';
	} else if (!inet_aton(arg, &cmd->p.ip))
		errx(EX_DATAERR, "bad ip address ``%s''", arg);
}

/*
 * the following macro returns an error message if we run out of
 * arguments.
 */
#define	NEED1(msg)	{if (!ac) errx(EX_USAGE, msg);}

static void
config_pipe(int ac, char **av)
{
	struct dn_pipe pipe;
	int i;
	char *end;
	u_int32_t a;
	void *par = NULL;

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
	while (ac > 0) {
		double d;
		int tok = match_token(dummynet_params, *av);
		ac--; av++;

		switch(tok) {
		case TOK_PLR:
			NEED1("plr needs argument 0..1\n");
			d = strtod(av[0], NULL);
			if (d > 1)
				d = 1;
			else if (d < 0)
				d = 0;
			pipe.fs.plr = (int)(d*0x7fffffff);
			ac--; av++;
			break;

		case TOK_QUEUE:
			NEED1("queue needs queue size\n");
			end = NULL;
			pipe.fs.qsize = strtoul(av[0], &end, 0);
			if (*end == 'K' || *end == 'k') {
				pipe.fs.flags_fs |= DN_QSIZE_IS_BYTES;
				pipe.fs.qsize *= 1024;
			} else if (*end == 'B' || !strncmp(end, "by", 2)) {
				pipe.fs.flags_fs |= DN_QSIZE_IS_BYTES;
			}
			ac--; av++;
			break;

		case TOK_BUCKETS:
			NEED1("buckets needs argument\n");
			pipe.fs.rq_size = strtoul(av[0], NULL, 0);
			ac--; av++;
			break;

		case TOK_MASK:
			NEED1("mask needs mask specifier\n");
			/*
			 * per-flow queue, mask is dst_ip, dst_port,
			 * src_ip, src_port, proto measured in bits
			 */
			par = NULL;

			pipe.fs.flow_mask.dst_ip = 0;
			pipe.fs.flow_mask.src_ip = 0;
			pipe.fs.flow_mask.dst_port = 0;
			pipe.fs.flow_mask.src_port = 0;
			pipe.fs.flow_mask.proto = 0;
			end = NULL;

			while (ac >= 1) {
			    u_int32_t *p32 = NULL;
			    u_int16_t *p16 = NULL;

			    tok = match_token(dummynet_params, *av);
			    ac--; av++;
			    switch(tok) {
			    case TOK_ALL:
				    /*
				     * special case, all bits significant
				     */
				    pipe.fs.flow_mask.dst_ip = ~0;
				    pipe.fs.flow_mask.src_ip = ~0;
				    pipe.fs.flow_mask.dst_port = ~0;
				    pipe.fs.flow_mask.src_port = ~0;
				    pipe.fs.flow_mask.proto = ~0;
				    pipe.fs.flags_fs |= DN_HAVE_FLOW_MASK;
				    goto end_mask;

			    case TOK_DSTIP:
				    p32 = &pipe.fs.flow_mask.dst_ip;
				    break;

			    case TOK_SRCIP:
				    p32 = &pipe.fs.flow_mask.src_ip;
				    break;

			    case TOK_DSTPORT:
				    p16 = &pipe.fs.flow_mask.dst_port;
				    break;

			    case TOK_SRCPORT:
				    p16 = &pipe.fs.flow_mask.src_port;
				    break;

			    case TOK_PROTO:
				    break;

			    default:
				    ac++; av--; /* backtrack */
				    goto end_mask;
			    }
			    if (ac < 1)
				    errx(EX_USAGE, "mask: value missing");
			    if (*av[0] == '/') {
				    a = strtoul(av[0]+1, &end, 0);
				    a = (a == 32) ? ~0 : (1 << a) - 1;
			    } else 
				    a = strtoul(av[0], &end, 0);
			    if (p32 != NULL)
				    *p32 = a;
			    else if (p16 != NULL) {
				    if (a > 65535)
					    errx(EX_DATAERR,
						"mask: must be 16 bit");
				    *p16 = (u_int16_t)a;
			    } else {
				    if (a > 255)
					    errx(EX_DATAERR,
						"mask: must be 8 bit");
				    pipe.fs.flow_mask.proto = (u_int8_t)a;
			    }
			    if (a != 0)
				    pipe.fs.flags_fs |= DN_HAVE_FLOW_MASK;
			    ac--; av++;
			} /* end while, config masks */
end_mask:
			break;

		case TOK_RED:
		case TOK_GRED:
			NEED1("red/gred needs w_q/min_th/max_th/max_p\n");
			pipe.fs.flags_fs |= DN_IS_RED;
			if (tok == TOK_GRED)
				pipe.fs.flags_fs |= DN_IS_GENTLE_RED;
			/*
			 * the format for parameters is w_q/min_th/max_th/max_p
			 */
			if ((end = strsep(&av[0], "/"))) {
			    double w_q = strtod(end, NULL);
			    if (w_q > 1 || w_q <= 0)
				errx(EX_DATAERR, "0 < w_q <= 1");
			    pipe.fs.w_q = (int) (w_q * (1 << SCALE_RED));
			}
			if ((end = strsep(&av[0], "/"))) {
			    pipe.fs.min_th = strtoul(end, &end, 0);
			    if (*end == 'K' || *end == 'k')
				pipe.fs.min_th *= 1024;
			}
			if ((end = strsep(&av[0], "/"))) {
			    pipe.fs.max_th = strtoul(end, &end, 0);
			    if (*end == 'K' || *end == 'k')
				pipe.fs.max_th *= 1024;
			}
			if ((end = strsep(&av[0], "/"))) {
			    double max_p = strtod(end, NULL);
			    if (max_p > 1 || max_p <= 0)
				errx(EX_DATAERR, "0 < max_p <= 1");
			    pipe.fs.max_p = (int)(max_p * (1 << SCALE_RED));
			}
			ac--; av++;
			break;

		case TOK_DROPTAIL:
			pipe.fs.flags_fs &= ~(DN_IS_RED|DN_IS_GENTLE_RED);
			break;
		    
		case TOK_BW:
			NEED1("bw needs bandwidth or interface\n");
			if (do_pipe != 1)
			    errx(EX_DATAERR, "bandwidth only valid for pipes");
			/*
			 * set clocking interface or bandwidth value
			 */
			if (av[0][0] >= 'a' && av[0][0] <= 'z') {
			    int l = sizeof(pipe.if_name)-1;
			    /* interface name */
			    strncpy(pipe.if_name, av[0], l);
			    pipe.if_name[l] = '\0';
			    pipe.bandwidth = 0;
			} else {
			    pipe.if_name[0] = '\0';
			    pipe.bandwidth = strtoul(av[0], &end, 0);
			    if (*end == 'K' || *end == 'k') {
				end++;
				pipe.bandwidth *= 1000;
			    } else if (*end == 'M') {
				end++;
				pipe.bandwidth *= 1000000;
			    }
			    if (*end == 'B' || !strncmp(end, "by", 2))
				pipe.bandwidth *= 8;
			    if (pipe.bandwidth < 0)
				errx(EX_DATAERR, "bandwidth too large");
			}
			ac--; av++;
			break;

		case TOK_DELAY:
			if (do_pipe != 1)
				errx(EX_DATAERR, "delay only valid for pipes");
			NEED1("delay needs argument 0..10000ms\n");
			pipe.delay = strtoul(av[0], NULL, 0);
			ac--; av++;
			break;

		case TOK_WEIGHT:
			if (do_pipe == 1)
				errx(EX_DATAERR,"weight only valid for queues");
			NEED1("weight needs argument 0..100\n");
			pipe.fs.weight = strtoul(av[0], &end, 0);
			ac--; av++;
			break;

		case TOK_PIPE:
			if (do_pipe == 1)
				errx(EX_DATAERR,"pipe only valid for queues");
			NEED1("pipe needs pipe_number\n");
			pipe.fs.parent_nr = strtoul(av[0], &end, 0);
			ac--; av++;
			break;

		default:
			errx(EX_DATAERR, "unrecognised option ``%s''", *av);
		}
	}
	if (do_pipe == 1) {
		if (pipe.pipe_nr == 0)
			errx(EX_DATAERR, "pipe_nr must be > 0");
		if (pipe.delay > 10000)
			errx(EX_DATAERR, "delay must be < 10000");
	} else { /* do_pipe == 2, queue */
		if (pipe.fs.parent_nr == 0)
			errx(EX_DATAERR, "pipe must be > 0");
		if (pipe.fs.weight >100)
			errx(EX_DATAERR, "weight must be <= 100");
	}
	if (pipe.fs.flags_fs & DN_QSIZE_IS_BYTES) {
		if (pipe.fs.qsize > 1024*1024)
			errx(EX_DATAERR, "queue size must be < 1MB");
	} else {
		if (pipe.fs.qsize > 100)
			errx(EX_DATAERR, "2 <= queue size <= 100");
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
 * helper function, updates the pointer to cmd with the length
 * of the current command, and also cleans up the first word of
 * the new command in case it has been clobbered before.
 */
static ipfw_insn *
next_cmd(ipfw_insn *cmd)
{
	cmd += F_LEN(cmd);
	bzero(cmd, sizeof(*cmd));
	return cmd;
}

/*
 * A function to fill simple commands of size 1.
 * Existing flags are preserved.
 */
static void
fill_cmd(ipfw_insn *cmd, enum ipfw_opcodes opcode, int flags, u_int16_t arg)
{
	cmd->opcode = opcode;
	cmd->len =  ((cmd->len | flags) & (F_NOT | F_OR)) | 1;
	cmd->arg1 = arg;
}

/*
 * Fetch and add the MAC address and type, with masks. This generates one or
 * two microinstructions, and returns the pointer to the last one.
 */
static ipfw_insn *
add_mac(ipfw_insn *cmd, int ac, char *av[])
{
	ipfw_insn_mac *mac; /* also *src */

	if (ac <3)
		errx(EX_DATAERR, "MAC dst src type");

	cmd->opcode = O_MACADDR2;
	cmd->len = (cmd->len & (F_NOT | F_OR)) | F_INSN_SIZE(ipfw_insn_mac);

	mac = (ipfw_insn_mac *)cmd;
	get_mac_addr_mask(av[0], mac->addr, mac->mask);		/* dst */
	get_mac_addr_mask(av[1], &(mac->addr[6]), &(mac->mask[6])); /* src */
	av += 2;

	if (strcmp(av[0], "any") != 0) {	/* we have a non-null port */
		cmd += F_LEN(cmd);

		fill_newports((ipfw_insn_u16 *)cmd, av[0], IPPROTO_ETHERTYPE);
		cmd->opcode = O_MAC_TYPE;
	}

	return cmd;
}

/*
 * Parse arguments and assemble the microinstructions which make up a rule.
 * Rules are added into the 'rulebuf' and then copied in the correct order
 * into the actual rule.
 *
 * The syntax for a rule starts with the action, followed by an
 * optional log action, and the various match patterns.
 * In the assembled microcode, the first opcode must be a O_PROBE_STATE
 * (generated if the rule includes a keep-state option), then the
 * various match patterns, the "log" action, and the actual action.
 * 
 */
static void
add(int ac, char *av[])
{
	/*
	 * rules are added into the 'rulebuf' and then copied in
	 * the correct order into the actual rule.
	 * Some things that need to go out of order (prob, action etc.)
	 * go into actbuf[].
	 */
	static u_int32_t rulebuf[255], actbuf[255], cmdbuf[255];

	ipfw_insn *src, *dst, *cmd, *action, *prev;

	struct ip_fw *rule;

	/*
	 * various flags used to record that we entered some fields.
	 */
	int have_mac = 0;	/* set if we have a MAC address */
	ipfw_insn *have_state = NULL;	/* check-state or keep-state */

	int i;

	int open_par = 0;	/* open parenthesis ( */

	/* proto is here because it is used to fetch ports */
	u_char proto = IPPROTO_IP;	/* default protocol */

	bzero(actbuf, sizeof(actbuf));		/* actions go here */
	bzero(cmdbuf, sizeof(cmdbuf));
	bzero(rulebuf, sizeof(rulebuf));

	rule = (struct ip_fw *)rulebuf;
	cmd = (ipfw_insn *)cmdbuf;
	action = (ipfw_insn *)actbuf;

	av++; ac--;

	/* [rule N]	-- Rule number optional */
	if (ac && isdigit(**av)) {
		rule->rulenum = atoi(*av);
		av++;
		ac--;
	}

	/* [prob D]	-- match probability, optional */
	if (ac > 1 && !strncmp(*av, "prob", strlen(*av))) {
		double d = strtod(av[1], NULL);

		if (d <= 0 || d > 1)
			errx(EX_DATAERR, "illegal match prob. %s", av[1]);
		if (d != 1) { /* 1 means always match */
			action->opcode = O_PROB;
			action->len = 2;
			*((int32_t *)(action+1)) =
				(int32_t)((1 - d) * 0x7fffffff);
			action += action->len;
		}
		av += 2; ac -= 2;
	}

	/* action	-- mandatory */
	NEED1("missing action");
	i = match_token(rule_actions, *av);
	ac--; av++;
	action->len = 1;	/* default */
	switch(i) {
	case TOK_CHECKSTATE:
		have_state = action;
		action->opcode = O_CHECK_STATE;
		break;

	case TOK_ACCEPT:
		action->opcode = O_ACCEPT;
		break;

	case TOK_DENY:
		action->opcode = O_DENY;
		action->arg1 = 0;
		break;

	case TOK_REJECT:
		action->opcode = O_REJECT;
		action->arg1 = ICMP_UNREACH_HOST;
		break;

	case TOK_RESET:
		action->opcode = O_REJECT;
		action->arg1 = ICMP_REJECT_RST;
		break;

	case TOK_UNREACH:
		action->opcode = O_REJECT;
		NEED1("missing reject code");
		fill_reject_code(&action->arg1, *av);
		ac--; av++;
		break;

	case TOK_COUNT:
		action->opcode = O_COUNT;
		break;

	case TOK_QUEUE:
	case TOK_PIPE:
		action->len = F_INSN_SIZE(ipfw_insn_pipe);
	case TOK_SKIPTO:
		if (i == TOK_QUEUE)
			action->opcode = O_QUEUE;
		else if (i == TOK_PIPE)
			action->opcode = O_PIPE;
		else if (i == TOK_SKIPTO)
			action->opcode = O_SKIPTO;
		NEED1("missing skipto/pipe/queue number");
		action->arg1 = strtoul(*av, NULL, 10);
		av++; ac--;
		break;

	case TOK_DIVERT:
	case TOK_TEE:
		action->opcode = (i == TOK_DIVERT) ? O_DIVERT : O_TEE;
		NEED1("missing divert/tee port");
		action->arg1 = strtoul(*av, NULL, 0);
		if (action->arg1 == 0) {
			struct servent *s;
			setservent(1);
			s = getservbyname(av[0], "divert");
			if (s != NULL)
				action->arg1 = ntohs(s->s_port);
			else
				errx(EX_DATAERR, "illegal divert/tee port");
		}
		ac--; av++;
		break;

	case TOK_FORWARD: {
		ipfw_insn_sa *p = (ipfw_insn_sa *)action;
		char *s, *end;

		NEED1("missing forward address[:port]");

		action->opcode = O_FORWARD_IP;
		action->len = F_INSN_SIZE(ipfw_insn_sa);

		p->sa.sin_len = sizeof(struct sockaddr_in);
		p->sa.sin_family = AF_INET;
		p->sa.sin_port = 0;
		/*
		 * locate the address-port separator (':' or ',')
		 */
		s = strchr(*av, ':');
		if (s == NULL)
			s = strchr(*av, ',');
		if (s != NULL) {
			*(s++) = '\0';
			i = strtoport(s, &end, 0 /* base */, 0 /* proto */);
			if (s == end)
				errx(EX_DATAERR,
				    "illegal forwarding port ``%s''", s);
			p->sa.sin_port = htons( (u_short)i );
		}
		lookup_host(*av, &(p->sa.sin_addr));
		}
		ac--; av++;
		break;

	default:
		errx(EX_DATAERR, "invalid action %s\n", *av);
	}
	action = next_cmd(action);

	/*
	 * [log [logamount N]]	-- log, optional
	 *
	 * If exists, it goes first in the cmdbuf, but then it is
	 * skipped in the copy section to the end of the buffer.
	 */
	if (ac && !strncmp(*av, "log", strlen(*av))) {
		ipfw_insn_log *c = (ipfw_insn_log *)cmd;

		cmd->len = F_INSN_SIZE(ipfw_insn_log);
		cmd->opcode = O_LOG;
		av++; ac--;
		if (ac && !strncmp(*av, "logamount", strlen(*av))) {
			ac--; av++;
			NEED1("logamount requires argument");
			c->max_log = atoi(*av);
			if (c->max_log < 0)
				errx(EX_DATAERR, "logamount must be positive");
			ac--; av++;
		}
		cmd = next_cmd(cmd);
	}

	if (have_state)	/* must be a check-state, we are done */
		goto done;

#define OR_START(target)					\
	if (ac && (*av[0] == '(' || *av[0] == '{')) {		\
		if (open_par)					\
			errx(EX_USAGE, "nested \"(\" not allowed\n"); \
		open_par = 1;					\
		if ( (av[0])[1] == '\0') {			\
			ac--; av++;				\
		} else						\
			(*av)++;				\
	}							\
	target:							\


#define	CLOSE_PAR						\
	if (open_par) {						\
		if (ac && (					\
		    !strncmp(*av, ")", strlen(*av)) ||		\
		    !strncmp(*av, "}", strlen(*av)) )) {	\
			open_par = 0;				\
			ac--; av++;				\
		} else						\
			errx(EX_USAGE, "missing \")\"\n");	\
	}
		
#define NOT_BLOCK						\
	if (ac && !strncmp(*av, "not", strlen(*av))) {		\
		if (cmd->len & F_NOT)				\
			errx(EX_USAGE, "double \"not\" not allowed\n"); \
		cmd->len |= F_NOT;				\
		ac--; av++;					\
	}

#define OR_BLOCK(target)					\
	if (ac && !strncmp(*av, "or", strlen(*av))) {		\
		if (prev == NULL || open_par == 0)		\
			errx(EX_DATAERR, "invalid OR block");	\
		prev->len |= F_OR;				\
		ac--; av++;					\
		goto target;					\
	}							\
	CLOSE_PAR;

	/*
	 * protocol, mandatory
	 */
    OR_START(get_proto);
	NOT_BLOCK;
	NEED1("missing protocol");
	{
	struct protoent *pe;

	if (!strncmp(*av, "all", strlen(*av)))
		; /* same as "ip" */
	else if (!strncmp(*av, "MAC", strlen(*av))) {
		/* need exactly 3 fields */
		cmd = add_mac(cmd, ac-1, av+1);	/* exits in case of errors */
		ac -= 3;
		av += 3;
		have_mac = 1;
	} else if ((proto = atoi(*av)) > 0)
		; /* all done! */
	else if ((pe = getprotobyname(*av)) != NULL)
		proto = pe->p_proto;
	else
		errx(EX_DATAERR, "invalid protocol ``%s''", *av);
	av++; ac--;
	if (proto != IPPROTO_IP)
		fill_cmd(cmd, O_PROTO, 0, proto);
	}
	cmd = next_cmd(cmd);
    OR_BLOCK(get_proto);

	/*
	 * "from", mandatory (unless we have a MAC address)
	 */
	if (!ac || strncmp(*av, "from", strlen(*av))) {
		if (have_mac)	/* we do not need a "to" address */
			goto read_to;
		errx(EX_USAGE, "missing ``from''");
	}
	ac--; av++;

	/*
	 * source IP, mandatory
	 */
    OR_START(source_ip);
	NOT_BLOCK;	/* optional "not" */
	NEED1("missing source address");

	/* source	-- mandatory */
	fill_ip((ipfw_insn_ip *)cmd, *av);
	if (cmd->opcode == O_IP_DST_SET)			/* set */
		cmd->opcode = O_IP_SRC_SET;
	else if (F_LEN(cmd) == F_INSN_SIZE(ipfw_insn))		/* me */
		cmd->opcode = O_IP_SRC_ME;
	else if (F_LEN(cmd) == F_INSN_SIZE(ipfw_insn_u32))	/* one IP */
		cmd->opcode = O_IP_SRC;
	else if (F_LEN(cmd) == F_INSN_SIZE(ipfw_insn_ip))	/* addr/mask */
		cmd->opcode = O_IP_SRC_MASK;
	/* otherwise len will be zero and the command skipped */
	ac--; av++;
	prev = cmd; /* in case we need to backtrack */
	cmd = next_cmd(cmd);
    OR_BLOCK(source_ip);

	/*
	 * source ports, optional
	 */
	NOT_BLOCK;	/* optional "not" */
	if (ac && fill_newports((ipfw_insn_u16 *)cmd, *av, proto)) {
		/* XXX todo: check that we have a protocol with ports */
		cmd->opcode = O_IP_SRCPORT;
		ac--;
		av++;
		cmd = next_cmd(cmd);
	}

read_to:
	/*
	 * "to", mandatory (unless we have a MAC address
	 */
	if (!ac || strncmp(*av, "to", strlen(*av))) {
		if (have_mac)
			goto read_options;
		errx(EX_USAGE, "missing ``to''");
	}
	av++; ac--;

	/*
	 * destination, mandatory
	 */
    OR_START(dest_ip);
	NOT_BLOCK;	/* optional "not" */
	NEED1("missing dst address");
	fill_ip((ipfw_insn_ip *)cmd, *av);
	if (cmd->opcode == O_IP_DST_SET)			/* set */
		;
	else if (F_LEN(cmd) == F_INSN_SIZE(ipfw_insn))		/* me */
		cmd->opcode = O_IP_DST_ME;
	else if (F_LEN(cmd) == F_INSN_SIZE(ipfw_insn_u32))	/* one IP */
		cmd->opcode = O_IP_DST;
	else if (F_LEN(cmd) == F_INSN_SIZE(ipfw_insn_ip))	/* addr/mask */
		cmd->opcode = O_IP_DST_MASK;
	ac--;
	av++;
	prev = cmd;
	cmd = next_cmd(cmd);
    OR_BLOCK(dest_ip);

	/*
	 * dest. ports, optional
	 */
	NOT_BLOCK;	/* optional "not" */
	if (ac && fill_newports((ipfw_insn_u16 *)cmd, *av, proto)) {
		/* XXX todo: check that we have a protocol with ports */
		cmd->opcode = O_IP_DSTPORT;
		ac--;
		av++;
		cmd += F_LEN(cmd);
	}

read_options:
	prev = NULL;
	while (ac) {
		char *s = *av;
		ipfw_insn_u32 *cmd32 = (ipfw_insn_u32 *)cmd;	/* alias */

		if (*s == '!') {	/* alternate syntax for NOT */
			if (cmd->len & F_NOT)
				errx(EX_USAGE, "double \"not\" not allowed\n");
			cmd->len = F_NOT;
			s++;
		}
		i = match_token(rule_options, s);
		ac--; av++;
		switch(i) {
		case TOK_NOT:
			if (cmd->len & F_NOT)
				errx(EX_USAGE, "double \"not\" not allowed\n");
			cmd->len = F_NOT;
			break;

		case TOK_OR:
			if (prev == NULL)
				errx(EX_USAGE, "invalid \"or\" block\n");
			prev->len |= F_OR;
			break;
				
		case TOK_IN:
			fill_cmd(cmd, O_IN, 0, 0);
			break;

		case TOK_OUT:
			cmd->len ^= F_NOT; /* toggle F_NOT */
			fill_cmd(cmd, O_IN, 0, 0);
			break;

		case TOK_FRAG:
			fill_cmd(cmd, O_FRAG, 0, 0);
			break;

		case TOK_LAYER2:
			fill_cmd(cmd, O_LAYER2, 0, 0);
			break;

		case TOK_XMIT:
		case TOK_RECV:
		case TOK_VIA:
			NEED1("recv, xmit, via require interface name"
				" or address");
			fill_iface((ipfw_insn_if *)cmd, av[0]);
			ac--; av++;
			if (F_LEN(cmd) == 0)	/* not a valid address */
				break;
			if (i == TOK_XMIT)
				cmd->opcode = O_XMIT;
			else if (i == TOK_RECV)
				cmd->opcode = O_RECV;
			else if (i == TOK_VIA)
				cmd->opcode = O_VIA;
			break;

		case TOK_ICMPTYPES:
			NEED1("icmptypes requires list of types");
			fill_icmptypes((ipfw_insn_u32 *)cmd, *av);
			av++; ac--;
			break;

		case TOK_IPTTL:
			NEED1("ipttl requires TTL");
			fill_cmd(cmd, O_IPTTL, 0, strtoul(*av, NULL, 0));
			ac--; av++;
			break;

		case TOK_IPID:
			NEED1("ipid requires length");
			fill_cmd(cmd, O_IPID, 0, strtoul(*av, NULL, 0));
			ac--; av++;
			break;

		case TOK_IPLEN:
			NEED1("iplen requires length");
			fill_cmd(cmd, O_IPLEN, 0, strtoul(*av, NULL, 0));
			ac--; av++;
			break;

		case TOK_IPVER:
			NEED1("ipver requires version");
			fill_cmd(cmd, O_IPVER, 0, strtoul(*av, NULL, 0));
			ac--; av++;
			break;

		case TOK_IPPRECEDENCE:
			NEED1("ipprecedence requires value");
			fill_cmd(cmd, O_IPPRECEDENCE, 0,
			    (strtoul(*av, NULL, 0) & 7) << 5);
			ac--; av++;
			break;

		case TOK_IPOPTS:
			NEED1("missing argument for ipoptions");
			fill_flags(cmd, O_IPOPT, f_ipopts, *av);
			ac--; av++;
			break;

		case TOK_IPTOS:
			NEED1("missing argument for iptos");
			fill_flags(cmd, O_IPTOS, f_iptos, *av);
			ac--; av++;
			break;

		case TOK_UID:
			NEED1("uid requires argument");
		    {
			char *end;
			uid_t uid;
			struct passwd *pwd;

			cmd->opcode = O_UID;
			uid = strtoul(*av, &end, 0);
			pwd = (*end == '\0') ? getpwuid(uid) : getpwnam(*av);
			if (pwd == NULL)
				errx(EX_DATAERR, "uid \"%s\" nonexistent", *av);
			cmd32->d[0] = uid;
			cmd->len = F_INSN_SIZE(ipfw_insn_u32);
			ac--; av++;
		    }
			break;

		case TOK_GID:
			NEED1("gid requires argument");
		    {
			char *end;
			gid_t gid;
			struct group *grp;

			cmd->opcode = O_GID;
			gid = strtoul(*av, &end, 0);
			grp = (*end == '\0') ? getgrgid(gid) : getgrnam(*av);
			if (grp == NULL)
				errx(EX_DATAERR, "gid \"%s\" nonexistent", *av);
			
			cmd32->d[0] = gid;
			cmd->len = F_INSN_SIZE(ipfw_insn_u32);
			ac--; av++;
		    }
			break;

		case TOK_ESTAB:
			fill_cmd(cmd, O_ESTAB, 0, 0);
			break;

		case TOK_SETUP:
			fill_cmd(cmd, O_TCPFLAGS, 0,
				(TH_SYN) | ( (TH_ACK) & 0xff) <<8 );
			break;

		case TOK_TCPOPTS:
			NEED1("missing argument for tcpoptions");
			fill_flags(cmd, O_TCPOPTS, f_tcpopts, *av);
			ac--; av++;
			break;

		case TOK_TCPSEQ:
		case TOK_TCPACK:
			NEED1("tcpseq/tcpack requires argument");
			cmd->len = F_INSN_SIZE(ipfw_insn_u32);
			cmd->opcode = (i == TOK_TCPSEQ) ? O_TCPSEQ : O_TCPACK;
			cmd32->d[0] = htonl(strtoul(*av, NULL, 0));
			ac--; av++;
			break;

		case TOK_TCPWIN:
			NEED1("tcpwin requires length");
			fill_cmd(cmd, O_TCPWIN, 0,
			    htons(strtoul(*av, NULL, 0)));
			ac--; av++;
			break;

		case TOK_TCPFLAGS:
			NEED1("missing argument for tcpflags");
			cmd->opcode = O_TCPFLAGS;
			fill_flags(cmd, O_TCPFLAGS, f_tcpflags, *av);
			ac--; av++;
			break;

		case TOK_KEEPSTATE:
			if (have_state)
				errx(EX_USAGE, "only one of keep-state "
					"and limit is allowed");
			have_state = cmd;
			fill_cmd(cmd, O_KEEP_STATE, 0, 0);
			break;

		case TOK_LIMIT:
			NEED1("limit needs mask and # of connections");
			if (have_state)
				errx(EX_USAGE, "only one of keep-state "
					"and limit is allowed");
			have_state = cmd;
		    {
			ipfw_insn_limit *c = (ipfw_insn_limit *)cmd;

			cmd->len = F_INSN_SIZE(ipfw_insn_limit);
			cmd->opcode = O_LIMIT;
			c->limit_mask = 0;
			c->conn_limit = 0;
			for (; ac >1 ;) {
				int val;

				val = match_token(limit_masks, *av);
				if (val <= 0)
					break;
				c->limit_mask |= val;
				ac--; av++;
			}
			c->conn_limit = atoi(*av);
			if (c->conn_limit == 0)
				errx(EX_USAGE, "limit: limit must be >0");
			if (c->limit_mask == 0)
				errx(EX_USAGE, "missing limit mask");
			ac--; av++;
		    }
			break;

		default:
			errx(EX_USAGE, "unrecognised option [%d] %s\n", i, s);
		}
		if (F_LEN(cmd) > 0) {	/* prepare to advance */
			prev = cmd;
			cmd = next_cmd(cmd);
		}
	}

done:
	/*
	 * Now copy stuff into the rule.
	 * If we have a keep-state option, the first instruction
	 * must be a PROBE_STATE (which is generated here).
	 * If we have a LOG option, it was stored as the first command,
	 * and now must be moved to the top of the action part.
	 */
	dst = (ipfw_insn *)rule->cmd;

	/*
	 * generate O_PROBE_STATE if necessary
	 */
	if (have_state && have_state->opcode != O_CHECK_STATE) {
		fill_cmd(dst, O_PROBE_STATE, 0, 0);
		dst = next_cmd(dst);
	}
	/*
	 * copy all commands but O_LOG, O_KEEP_STATE, O_LIMIT
	 */
	for (src = (ipfw_insn *)cmdbuf; src != cmd; src += i) {
		i = F_LEN(src);

		switch (src->opcode) {
		case O_LOG:
		case O_KEEP_STATE:
		case O_LIMIT:
			break;
		default:
			bcopy(src, dst, i * sizeof(u_int32_t));
			dst += i;
		}
	}

	/*
	 * put back the have_state command as last opcode
	 */
	if (have_state) {
		i = F_LEN(have_state);
		bcopy(have_state, dst, i * sizeof(u_int32_t));
		dst += i;
	}
	/*
	 * start action section
	 */
	rule->act_ofs = dst - rule->cmd;

	/*
	 * put back O_LOG if necessary
	 */
	src = (ipfw_insn *)cmdbuf;
	if ( src->opcode == O_LOG ) {
		i = F_LEN(src);
		bcopy(src, dst, i * sizeof(u_int32_t));
		dst += i;
	}
	/*
	 * copy all other actions
	 */
	for (src = (ipfw_insn *)actbuf; src != action; src += i) {
		i = F_LEN(src);
		bcopy(src, dst, i * sizeof(u_int32_t));
		dst += i;
	}

	rule->cmd_len = (u_int32_t *)dst - (u_int32_t *)(rule->cmd);
	i = (void *)dst - (void *)rule;
	if (getsockopt(s, IPPROTO_IP, IP_FW_ADD, rule, &i) == -1)
		err(EX_UNAVAILABLE, "getsockopt(%s)", "IP_FW_ADD");
	if (!do_quiet)
		show_ipfw(rule);
}

static void
zero (int ac, char *av[])
{
	int rulenum;
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

	while (ac) {
		/* Rule number */
		if (isdigit(**av)) {
			rulenum = atoi(*av);
			av++;
			ac--;
			if (setsockopt(s, IPPROTO_IP,
			    IP_FW_ZERO, &rulenum, sizeof rulenum)) {
				warn("rule %u: setsockopt(IP_FW_ZERO)",
				    rulenum);
				failed = EX_UNAVAILABLE;
			} else if (!do_quiet)
				printf("Entry %d cleared\n", rulenum);
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
	int rulenum;
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

	while (ac) {
		/* Rule number */
		if (isdigit(**av)) {
			rulenum = atoi(*av);
			av++;
			ac--;
			if (setsockopt(s, IPPROTO_IP,
			    IP_FW_RESETLOG, &rulenum, sizeof rulenum)) {
				warn("rule %u: setsockopt(IP_FW_RESETLOG)",
				    rulenum);
				failed = EX_UNAVAILABLE;
			} else if (!do_quiet)
				printf("Entry %d logging count reset\n",
				    rulenum);
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
	int cmd = do_pipe ? IP_DUMMYNET_FLUSH : IP_FW_FLUSH;

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
	if (setsockopt(s, IPPROTO_IP, cmd, NULL, 0) < 0)
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
	while ((ch = getopt(ac, av, "hs:adefNqtv")) != -1)
		switch (ch) {
		case 'h': /* help */
			help();
			break;	/* NOTREACHED */

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
	NEED1("bad arguments, for usage summary ``ipfw''");

	/*
	 * optional: pipe or queue
	 */
	if (!strncmp(*av, "pipe", strlen(*av))) {
		do_pipe = 1;
		ac--;
		av++;
	} else if (!strncmp(*av, "queue", strlen(*av))) {
		do_pipe = 2;
		ac--;
		av++;
	}
	NEED1("missing command");

	/*
	 * for pipes and queues we normally say 'pipe NN config'
	 * but the code is easier to parse as 'pipe config NN'
	 * so we swap the two arguments.
	 */
	if (do_pipe > 0 && ac > 1 && *av[0] >= '0' && *av[0] <= '9') {
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
	else if (!strncmp(*av, "print", strlen(*av)) ||
	         !strncmp(*av, "list", strlen(*av)))
		list(ac, av);
	else if (!strncmp(*av, "show", strlen(*av))) {
		do_acct++;
		list(ac, av);
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

	/*
	 * If the last argument is an absolute pathname, interpret it
	 * as a file to be preprocessed.
	 */

	if (ac > 1 && av[ac - 1][0] == '/' && access(av[ac - 1], R_OK) == 0)
		ipfw_readfile(ac, av);
	else
		ipfw_main(ac, av);
	return EX_OK;
}
