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
#include <timeconv.h>
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
		do_compact,		/* show rules in compact mode */
		show_sets,		/* display rule sets */
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
	TOK_STARTBRACE,
	TOK_ENDBRACE,

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
	TOK_MAC,
	TOK_MACTYPE,

	TOK_PLR,
	TOK_NOERROR,
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
	{ "noerror",		TOK_NOERROR },
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
	{ "dst-ip",		TOK_DSTIP },
	{ "src-ip",		TOK_SRCIP },
	{ "dst-port",		TOK_DSTPORT },
	{ "src-port",		TOK_SRCPORT },
	{ "proto",		TOK_PROTO },
	{ "MAC",		TOK_MAC },
	{ "mac",		TOK_MAC },
	{ "mac-type",		TOK_MACTYPE },

	{ "not",		TOK_NOT },		/* pseudo option */
	{ "!", /* escape ? */	TOK_NOT },		/* pseudo option */
	{ "or",			TOK_OR },		/* pseudo option */
	{ "|", /* escape */	TOK_OR },		/* pseudo option */
	{ "{",			TOK_STARTBRACE },	/* pseudo option */
	{ "(",			TOK_STARTBRACE },	/* pseudo option */
	{ "}",			TOK_ENDBRACE },		/* pseudo option */
	{ ")",			TOK_ENDBRACE },		/* pseudo option */
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
print_newports(ipfw_insn_u16 *cmd, int proto, int opcode)
{
	u_int16_t *p = cmd->ports;
	int i;
	char *sep= " ";

	if (cmd->o.len & F_NOT)
		printf(" not");
	if (opcode != 0)
		printf ("%s", opcode == O_MAC_TYPE ? " mac-type" :
		    (opcode == O_IP_DSTPORT ? " dst-port" : " src-port"));
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
 * Returns *end == s in case the parameter is not found.
 */
static int
strtoport(char *s, char **end, int base, int proto)
{
	char *p, *buf;
	char *s1;
	int i;

	*end = s;		/* default - not found */
	if ( *s == '\0')
		return 0;	/* not found */

	if (isdigit(*s))
		return strtol(s, end, base);

	/*
	 * find separator. '\\' escapes the next char.
	 */
	for (s1 = s; *s1 && (isalnum(*s1) || *s1 == '\\') ; s1++)
		if (*s1 == '\\' && s1[1] != '\0')
			s1++;

	buf = malloc(s1 - s + 1);
	if (buf == NULL)
		return 0;

	/*
	 * copy into a buffer skipping backslashes
	 */
	for (p = s, i = 0; p != s1 ; p++)
		if ( *p != '\\')
			buf[i++] = *p;
	buf[i++] = '\0';

	if (proto == IPPROTO_ETHERTYPE) {
		i = match_token(ether_types, buf);
		free(buf);
		if (i != -1) {	/* found */
			*end = s1;
			return i;
		}
	} else {
		struct protoent *pe = NULL;
		struct servent *se;

		if (proto != 0)
			pe = getprotobynumber(proto);
		setservent(1);
		se = getservbyname(buf, pe ? pe->p_name : NULL);
		free(buf);
		if (se != NULL) {
			*end = s1;
			return ntohs(se->s_port);
		}
	}
	return 0;	/* not found */
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
	char *s = av;

	while (*s) {
		u_int16_t a, b;

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
		} else {	/* invalid separator */
			errx(EX_DATAERR, "invalid separator <%c> in <%s>\n",
				*s, av);
		}
		i++;
		p += 2;
		av = s+1;
	}
	if (i > 0) {
		if (i+1 > F_LEN_MASK)
			errx(EX_DATAERR, "too many ports/ranges\n");
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
	if (val < 0)
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
print_ip(ipfw_insn_ip *cmd, char *s)
{
	struct hostent *he = NULL;
	int mb;

	printf("%s%s ", cmd->o.len & F_NOT ? " not": "", s);

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
 * The first argument is the list of fields we have, the second is
 * the list of fields we want to be printed.
 *
 * Special cases if we have provided a MAC header:
 *   + if the rule does not contain IP addresses/ports, do not print them;
 *   + if the rule does not contain an IP proto, print "all" instead of "ip";
 *
 * Once we have 'have_options', IP header fields are printed as options.
 */
#define	HAVE_PROTO	0x0001
#define	HAVE_SRCIP	0x0002
#define	HAVE_DSTIP	0x0004
#define	HAVE_MAC	0x0008
#define	HAVE_MACTYPE	0x0010
#define	HAVE_OPTIONS	0x8000

#define	HAVE_IP		(HAVE_PROTO | HAVE_SRCIP | HAVE_DSTIP)
static void
show_prerequisites(int *flags, int want, int cmd)
{
	if ( (*flags & HAVE_IP) == HAVE_IP)
		*flags |= HAVE_OPTIONS;

	if ( (*flags & (HAVE_MAC|HAVE_MACTYPE|HAVE_OPTIONS)) == HAVE_MAC &&
	     cmd != O_MAC_TYPE) {
		/*
		 * mac-type was optimized out by the compiler,
		 * restore it
		 */
		printf(" any");
		*flags |= HAVE_MACTYPE | HAVE_OPTIONS;
		return;
	}
	if ( !(*flags & HAVE_OPTIONS)) {
		if ( !(*flags & HAVE_PROTO) && (want & HAVE_PROTO))
			printf(" ip");
		if ( !(*flags & HAVE_SRCIP) && (want & HAVE_SRCIP))
			printf(" from any");
		if ( !(*flags & HAVE_DSTIP) && (want & HAVE_DSTIP))
			printf(" to any");
	}
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

	u_int32_t set_disable = rule->set_disable;

	if (set_disable & (1 << rule->set)) { /* disabled */
		if (!show_sets)
			return;
		else
			printf("# DISABLED ");
	}
	printf("%05u ", rule->rulenum);

	if (do_acct)
		printf("%10qu %10qu ", rule->pcnt, rule->bcnt);

	if (do_time) {
		if (rule->timestamp) {
			char timestr[30];
			time_t t = _long_to_time(rule->timestamp);

			strcpy(timestr, ctime(&t));
			*strchr(timestr, '\n') = '\0';
			printf("%s ", timestr);
		} else {
			printf("			 ");
		}
	}

	if (show_sets)
		printf("set %d ", rule->set);

	/*
	 * print the optional "match probability"
	 */
	if (rule->cmd_len > 0) {
		cmd = rule->cmd ;
		if (cmd->opcode == O_PROB) {
			ipfw_insn_u32 *p = (ipfw_insn_u32 *)cmd;
			double d = 1.0 * p->d[0];

			d = (d / 0x7fffffff);
			printf("prob %f ", d);
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
			flags = HAVE_IP; /* avoid printing anything else */
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
				printf(",%d", s->sa.sin_port);
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
	 * then print the body.
	 */
	if (rule->_pad & 1) {	/* empty rules before options */
		if (!do_compact)
			printf(" ip from any to any");
		flags |= HAVE_IP | HAVE_OPTIONS;
	}

        for (l = rule->act_ofs, cmd = rule->cmd ;
			l > 0 ; l -= F_LEN(cmd) , cmd += F_LEN(cmd)) {
		/* useful alias */
		ipfw_insn_u32 *cmd32 = (ipfw_insn_u32 *)cmd;

		show_prerequisites(&flags, 0, cmd->opcode);

		switch(cmd->opcode) {
		case O_PROB:	
			break;	/* done already */

		case O_PROBE_STATE:
			break; /* no need to print anything here */

		case O_MACADDR2: {
			ipfw_insn_mac *m = (ipfw_insn_mac *)cmd;

			if ((cmd->len & F_OR) && !or_block)
				printf(" {");
			if (cmd->len & F_NOT)
				printf(" not");
			printf(" MAC");
			flags |= HAVE_MAC;
			print_mac( m->addr, m->mask);
			print_mac( m->addr + 6, m->mask + 6);
			}
			break;

		case O_MAC_TYPE:
			if ((cmd->len & F_OR) && !or_block)
				printf(" {");
			print_newports((ipfw_insn_u16 *)cmd, IPPROTO_ETHERTYPE,
				(flags & HAVE_OPTIONS) ? cmd->opcode : 0);
			flags |= HAVE_MAC | HAVE_MACTYPE | HAVE_OPTIONS;
			break;

		case O_IP_SRC:
		case O_IP_SRC_MASK:
		case O_IP_SRC_ME:
		case O_IP_SRC_SET:
			show_prerequisites(&flags, HAVE_PROTO, 0);
			if (!(flags & HAVE_SRCIP))
				printf(" from");
			if ((cmd->len & F_OR) && !or_block)
				printf(" {");
			print_ip((ipfw_insn_ip *)cmd,
				(flags & HAVE_OPTIONS) ? " src-ip" : "");
			flags |= HAVE_SRCIP;
			break;

		case O_IP_DST:
		case O_IP_DST_MASK:
		case O_IP_DST_ME:
		case O_IP_DST_SET:
			show_prerequisites(&flags, HAVE_PROTO|HAVE_SRCIP, 0);
			if (!(flags & HAVE_DSTIP))
				printf(" to");
			if ((cmd->len & F_OR) && !or_block)
				printf(" {");
			print_ip((ipfw_insn_ip *)cmd,
				(flags & HAVE_OPTIONS) ? " dst-ip" : "");
			flags |= HAVE_DSTIP;
			break;

		case O_IP_DSTPORT:
			show_prerequisites(&flags, HAVE_IP, 0);
		case O_IP_SRCPORT:
			show_prerequisites(&flags, HAVE_PROTO|HAVE_SRCIP, 0);
			if ((cmd->len & F_OR) && !or_block)
				printf(" {");
			print_newports((ipfw_insn_u16 *)cmd, proto,
				(flags & HAVE_OPTIONS) ? cmd->opcode : 0);
			break;

		case O_PROTO: {
			struct protoent *pe;

			if ((cmd->len & F_OR) && !or_block)
				printf(" {");
			if (cmd->len & F_NOT)
				printf(" not");
			proto = cmd->arg1;
			pe = getprotobynumber(cmd->arg1);
			if (flags & HAVE_OPTIONS)
				printf(" proto");
			if (pe)
				printf(" %s", pe->p_name);
			else
				printf(" %u", cmd->arg1);
			}
			flags |= HAVE_PROTO;
			break;

		default: /*options ... */
			show_prerequisites(&flags, HAVE_IP | HAVE_OPTIONS, 0);
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
	show_prerequisites(&flags, HAVE_IP, 0);

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
	    d->rulenum, d->pcnt, d->bcnt, d->expire);
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

/*
 * This one handles all set-related commands
 * 	ipfw set { show | enable | disable }
 * 	ipfw set swap X Y
 * 	ipfw set move X to Y
 * 	ipfw set move rule X to Y
 */
static void
sets_handler(int ac, char *av[])
{
	u_int32_t set_disable, masks[2];
	int i, nbytes;
	u_int16_t rulenum;
	u_int8_t cmd, new_set;

	ac--;
	av++;

	if (!ac)
		errx(EX_USAGE, "set needs command");
	if (!strncmp(*av, "show", strlen(*av)) ) {
		void *data;
		char *msg;

		nbytes = sizeof(struct ip_fw);
		if ((data = malloc(nbytes)) == NULL)
			err(EX_OSERR, "malloc");
		if (getsockopt(s, IPPROTO_IP, IP_FW_GET, data, &nbytes) < 0)
			err(EX_OSERR, "getsockopt(IP_FW_GET)");
		set_disable = ((struct ip_fw *)data)->set_disable;

		for (i = 0, msg = "disable" ; i < 31; i++)
			if (  (set_disable & (1<<i))) {
				printf("%s %d", msg, i);
				msg = "";
			}
		msg = (set_disable) ? " enable" : "enable";
		for (i = 0; i < 31; i++)
			if ( !(set_disable & (1<<i))) {
				printf("%s %d", msg, i);
				msg = "";
			}
		printf("\n");
	} else if (!strncmp(*av, "swap", strlen(*av))) {
		ac--; av++;
		if (ac != 2)
			errx(EX_USAGE, "set swap needs 2 set numbers\n");
		rulenum = atoi(av[0]);
		new_set = atoi(av[1]);
		if (!isdigit(*(av[0])) || rulenum > 30)
			errx(EX_DATAERR, "invalid set number %s\n", av[0]);
		if (!isdigit(*(av[1])) || new_set > 30)
			errx(EX_DATAERR, "invalid set number %s\n", av[1]);
		masks[0] = (4 << 24) | (new_set << 16) | (rulenum);
		i = setsockopt(s, IPPROTO_IP, IP_FW_DEL,
			masks, sizeof(u_int32_t));
	} else if (!strncmp(*av, "move", strlen(*av))) {
		ac--; av++;
		if (ac && !strncmp(*av, "rule", strlen(*av))) {
			cmd = 2;
			ac--; av++;
		} else
			cmd = 3;
		if (ac != 3 || strncmp(av[1], "to", strlen(*av)))
			errx(EX_USAGE, "syntax: set move [rule] X to Y\n");
		rulenum = atoi(av[0]);
		new_set = atoi(av[2]);
		if (!isdigit(*(av[0])) || (cmd == 3 && rulenum > 30) ||
			(cmd == 2 && rulenum == 65535) )
			errx(EX_DATAERR, "invalid source number %s\n", av[0]);
		if (!isdigit(*(av[2])) || new_set > 30)
			errx(EX_DATAERR, "invalid dest. set %s\n", av[1]);
		masks[0] = (cmd << 24) | (new_set << 16) | (rulenum);
		i = setsockopt(s, IPPROTO_IP, IP_FW_DEL,
			masks, sizeof(u_int32_t));
	} else if (!strncmp(*av, "disable", strlen(*av)) ||
		   !strncmp(*av, "enable",  strlen(*av)) ) {
		int which = !strncmp(*av, "enable",  strlen(*av)) ? 1 : 0;

		ac--; av++;
		masks[0] = masks[1] = 0;

		while (ac) {
			if (isdigit(**av)) {
				i = atoi(*av);
				if (i < 0 || i > 30)
					errx(EX_DATAERR,
					    "invalid set number %d\n", i);
				masks[which] |= (1<<i);
			} else if (!strncmp(*av, "disable", strlen(*av)))
				which = 0;
			else if (!strncmp(*av, "enable", strlen(*av)))
				which = 1;
			else
				errx(EX_DATAERR,
					"invalid set command %s\n", *av);
			av++; ac--;
		}
		if ( (masks[0] & masks[1]) != 0 )
			errx(EX_DATAERR,
			    "cannot enable and disable the same set\n");

		i = setsockopt(s, IPPROTO_IP, IP_FW_DEL, masks, sizeof(masks));
		if (i)
			warn("set enable/disable: setsockopt(IP_FW_DEL)");
	} else
		errx(EX_USAGE, "invalid set command %s\n", *av);
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
				if (d->rulenum > rnum)
					break;
				if (d->rulenum == rnum)
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
	u_int32_t rulenum;
	struct dn_pipe pipe;
	int i;
	int exitval = EX_OK;
	int do_set = 0;

	memset(&pipe, 0, sizeof pipe);

	av++; ac--;
	if (ac > 0 && !strncmp(*av, "set", strlen(*av))) {
		do_set = 1;	/* delete set */
		ac--; av++;
	}

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
			rulenum =  (i & 0xffff) | (do_set << 24);
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
		case TOK_NOERROR:
			pipe.fs.flags_fs |= DN_NOERROR;
			break;

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
	ipfw_insn_mac *mac;

	if (ac < 2)
		errx(EX_DATAERR, "MAC dst src");

	cmd->opcode = O_MACADDR2;
	cmd->len = (cmd->len & (F_NOT | F_OR)) | F_INSN_SIZE(ipfw_insn_mac);

	mac = (ipfw_insn_mac *)cmd;
	get_mac_addr_mask(av[0], mac->addr, mac->mask);	/* dst */
	get_mac_addr_mask(av[1], &(mac->addr[6]), &(mac->mask[6])); /* src */
	return cmd;
}

static ipfw_insn *
add_mactype(ipfw_insn *cmd, int ac, char *av)
{
	if (ac < 1)
		errx(EX_DATAERR, "missing MAC type");
	if (strcmp(av, "any") != 0) { /* we have a non-null type */
		fill_newports((ipfw_insn_u16 *)cmd, av, IPPROTO_ETHERTYPE);
		cmd->opcode = O_MAC_TYPE;
		return cmd;
	} else
		return NULL;
}

static ipfw_insn *
add_proto(ipfw_insn *cmd, char *av)
{
	struct protoent *pe;
	u_char proto = 0;

	if (!strncmp(av, "all", strlen(av)))
		; /* same as "ip" */
	else if ((proto = atoi(av)) > 0)
		; /* all done! */
	else if ((pe = getprotobyname(av)) != NULL)
		proto = pe->p_proto;
	else
		return NULL;
	if (proto != IPPROTO_IP)
		fill_cmd(cmd, O_PROTO, 0, proto);
	return cmd;
}

static ipfw_insn *
add_srcip(ipfw_insn *cmd, char *av)
{
	fill_ip((ipfw_insn_ip *)cmd, av);
	if (cmd->opcode == O_IP_DST_SET)			/* set */
		cmd->opcode = O_IP_SRC_SET;
	else if (F_LEN(cmd) == F_INSN_SIZE(ipfw_insn))		/* me */
		cmd->opcode = O_IP_SRC_ME;
	else if (F_LEN(cmd) == F_INSN_SIZE(ipfw_insn_u32))	/* one IP */
		cmd->opcode = O_IP_SRC;
	else if (F_LEN(cmd) == F_INSN_SIZE(ipfw_insn_ip))	/* addr/mask */
		cmd->opcode = O_IP_SRC_MASK;
	return cmd;
}

static ipfw_insn *
add_dstip(ipfw_insn *cmd, char *av)
{
	fill_ip((ipfw_insn_ip *)cmd, av);
	if (cmd->opcode == O_IP_DST_SET)			/* set */
		;
	else if (F_LEN(cmd) == F_INSN_SIZE(ipfw_insn))		/* me */
		cmd->opcode = O_IP_DST_ME;
	else if (F_LEN(cmd) == F_INSN_SIZE(ipfw_insn_u32))	/* one IP */
		cmd->opcode = O_IP_DST;
	else if (F_LEN(cmd) == F_INSN_SIZE(ipfw_insn_ip))	/* addr/mask */
		cmd->opcode = O_IP_DST_MASK;
	return cmd;
}

static ipfw_insn *
add_ports(ipfw_insn *cmd, char *av, u_char proto, int opcode)
{
	if (!strncmp(av, "any", strlen(av))) {
		return NULL;
	} else if (fill_newports((ipfw_insn_u16 *)cmd, av, proto)) {
		/* XXX todo: check that we have a protocol with ports */
		cmd->opcode = opcode;
		return cmd;
	}
	return NULL;
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
	ipfw_insn *first_cmd;	/* first match pattern */

	struct ip_fw *rule;

	/*
	 * various flags used to record that we entered some fields.
	 */
	ipfw_insn *have_state = NULL;	/* check-state or keep-state */

	int i;

	int open_par = 0;	/* open parenthesis ( */

	/* proto is here because it is used to fetch ports */
	u_char proto = IPPROTO_IP;	/* default protocol */

	double match_prob = 1; /* match probability, default is always match */

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

	/* [set N]	-- set number (0..30), optional */
	if (ac > 1 && !strncmp(*av, "set", strlen(*av))) {
		int set = strtoul(av[1], NULL, 10);
		if (set < 0 || set > 30)
			errx(EX_DATAERR, "illegal set %s", av[1]);
		rule->set = set;
		av += 2; ac -= 2;
	}

	/* [prob D]	-- match probability, optional */
	if (ac > 1 && !strncmp(*av, "prob", strlen(*av))) {
		match_prob = strtod(av[1], NULL);

		if (match_prob <= 0 || match_prob > 1)
			errx(EX_DATAERR, "illegal match prob. %s", av[1]);
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
			p->sa.sin_port = (u_short)i;
		}
		lookup_host(*av, &(p->sa.sin_addr));
		}
		ac--; av++;
		break;

	default:
		errx(EX_DATAERR, "invalid action %s\n", av[-1]);
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
		prev = NULL;					\
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
			prev = NULL;				\
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

	first_cmd = cmd;

#if 0
	/*
	 * MAC addresses, optional.
	 * If we have this, we skip the part "proto from src to dst"
	 * and jump straight to the option parsing.
	 */
	NOT_BLOCK;
	NEED1("missing protocol");
	if (!strncmp(*av, "MAC", strlen(*av)) ||
	    !strncmp(*av, "mac", strlen(*av))) {
		ac--; av++;	/* the "MAC" keyword */
		add_mac(cmd, ac, av); /* exits in case of errors */
		cmd = next_cmd(cmd);
		ac -= 2; av += 2;	/* dst-mac and src-mac */
		NOT_BLOCK;
		NEED1("missing mac type");
		if (add_mactype(cmd, ac, av[0]))
			cmd = next_cmd(cmd);
		ac--; av++;	/* any or mac-type */
		goto read_options;
	}
#endif

	/*
	 * protocol, mandatory
	 */
    OR_START(get_proto);
	NOT_BLOCK;
	NEED1("missing protocol");
	if (add_proto(cmd, *av)) {
		av++; ac--;
		if (F_LEN(cmd) == 0)	/* plain IP */
			proto = 0;
		else {
			proto = cmd->arg1;
			prev = cmd;
			cmd = next_cmd(cmd);
		}
	} else if (first_cmd != cmd) {
		errx(EX_DATAERR, "invalid protocol ``%s''", av);
	} else
		goto read_options;
    OR_BLOCK(get_proto);

	/*
	 * "from", mandatory
	 */
	if (!ac || strncmp(*av, "from", strlen(*av)))
		errx(EX_USAGE, "missing ``from''");
	ac--; av++;

	/*
	 * source IP, mandatory
	 */
    OR_START(source_ip);
	NOT_BLOCK;	/* optional "not" */
	NEED1("missing source address");
	if (add_srcip(cmd, *av)) {
		ac--; av++;
		if (F_LEN(cmd) != 0) {	/* ! any */
			prev = cmd;
			cmd = next_cmd(cmd);
		}
	}
    OR_BLOCK(source_ip);

	/*
	 * source ports, optional
	 */
	NOT_BLOCK;	/* optional "not" */
	if (ac) {
		if (!strncmp(*av, "any", strlen(*av)) ||
		    add_ports(cmd, *av, proto, O_IP_SRCPORT)) {
			ac--; av++;
			if (F_LEN(cmd) != 0)
				cmd = next_cmd(cmd);
		}
	}

	/*
	 * "to", mandatory
	 */
	if (!ac || strncmp(*av, "to", strlen(*av)))
		errx(EX_USAGE, "missing ``to''");
	av++; ac--;

	/*
	 * destination, mandatory
	 */
    OR_START(dest_ip);
	NOT_BLOCK;	/* optional "not" */
	NEED1("missing dst address");
	if (add_dstip(cmd, *av)) {
		ac--; av++;
		if (F_LEN(cmd) != 0) {	/* ! any */
			prev = cmd;
			cmd = next_cmd(cmd);
		}
	}
    OR_BLOCK(dest_ip);

	/*
	 * dest. ports, optional
	 */
	NOT_BLOCK;	/* optional "not" */
	if (ac) {
		if (!strncmp(*av, "any", strlen(*av)) ||
		    add_ports(cmd, *av, proto, O_IP_DSTPORT)) {
			ac--; av++;
			if (F_LEN(cmd) != 0)
				cmd = next_cmd(cmd);
		}
	}

read_options:
	if (ac && first_cmd == cmd) {
		/*
		 * nothing specified so far, store in the rule to ease
		 * printout later.
		 */
		 rule->_pad = 1;
	}
	prev = NULL;
	while (ac) {
		char *s;
		ipfw_insn_u32 *cmd32;	/* alias for cmd */

		s = *av;
		cmd32 = (ipfw_insn_u32 *)cmd;

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
			if (open_par == 0 || prev == NULL)
				errx(EX_USAGE, "invalid \"or\" block\n");
			prev->len |= F_OR;
			break;

		case TOK_STARTBRACE:
			if (open_par)
				errx(EX_USAGE, "+nested \"(\" not allowed\n");
			open_par = 1;
			break;

		case TOK_ENDBRACE:
			if (!open_par)
				errx(EX_USAGE, "+missing \")\"\n");
			open_par = 0;
			prev = NULL;
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
			cmd32->d[0] = pwd->pw_uid;
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
			cmd32->d[0] = grp->gr_gid;
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
			if (open_par)
				errx(EX_USAGE, "keep-state cannot be part "
				    "of an or block");
			if (have_state)
				errx(EX_USAGE, "only one of keep-state "
					"and limit is allowed");
			have_state = cmd;
			fill_cmd(cmd, O_KEEP_STATE, 0, 0);
			break;

		case TOK_LIMIT:
			if (open_par)
				errx(EX_USAGE, "limit cannot be part "
				    "of an or block");
			if (have_state)
				errx(EX_USAGE, "only one of keep-state "
					"and limit is allowed");
			NEED1("limit needs mask and # of connections");
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

		case TOK_PROTO:
			NEED1("missing protocol");
			if (add_proto(cmd, *av)) {
				proto = cmd->arg1;
				ac--; av++;
			} else
				errx(EX_DATAERR, "invalid protocol ``%s''", av);
			break;

		case TOK_SRCIP:
			NEED1("missing source IP");
			if (add_srcip(cmd, *av)) {
				ac--; av++;
			}
			break;

		case TOK_DSTIP:
			NEED1("missing destination IP");
			if (add_dstip(cmd, *av)) {
				ac--; av++;
			}
			break;

		case TOK_SRCPORT:
			NEED1("missing source port");
			if (!strncmp(*av, "any", strlen(*av)) ||
			    add_ports(cmd, *av, proto, O_IP_SRCPORT)) {
				ac--; av++;
			} else
				errx(EX_DATAERR, "invalid source port %s", *av);
			break;

		case TOK_DSTPORT:
			NEED1("missing destination port");
			if (!strncmp(*av, "any", strlen(*av)) ||
			    add_ports(cmd, *av, proto, O_IP_DSTPORT)) {
				ac--; av++;
			} else
				errx(EX_DATAERR, "invalid destination port %s",
				    *av);
			break;

		case TOK_MAC:
			if (ac < 2)
				errx(EX_USAGE, "MAC dst-mac src-mac");
			if (add_mac(cmd, ac, av)) {
				ac -= 2; av += 2;
			}
			break;

		case TOK_MACTYPE:
			NEED1("missing mac type");
			if (!add_mactype(cmd, ac, *av))
				errx(EX_DATAERR, "invalid mac type %s", av);
			ac--; av++;
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
	 * First thing to write into the command stream is the match probability.
	 */
	if (match_prob != 1) { /* 1 means always match */
		dst->opcode = O_PROB;
		dst->len = 2;
		*((int32_t *)(dst+1)) = (int32_t)(match_prob * 0x7fffffff);
		dst += dst->len;
	}

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
	if (have_state && have_state->opcode != O_CHECK_STATE) {
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
zero(int ac, char *av[])
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
resetlog(int ac, char *av[])
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
	while ((ch = getopt(ac, av, "hs:acdefNqStv")) != -1)
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
		case 'c':
			do_compact = 1;
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
		case 'S':
			show_sets = 1;
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
	else if (!strncmp(*av, "set", strlen(*av)))
		sets_handler(ac, av);
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
