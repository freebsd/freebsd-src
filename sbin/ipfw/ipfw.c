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

static void show_usage(void);

static int
mask_bits(struct in_addr m_ad)
{
	int h_fnd = 0, h_num = 0, i;
	u_long mask;

	mask = ntohl(m_ad.s_addr);
	for (i = 0; i < sizeof(u_long)*CHAR_BIT; i++) {
		if (mask & 1L) {
			h_fnd = 1;
			h_num++;
		} else {
			if (h_fnd)
				return -1;
		}
		mask = mask >> 1;
	}
	return h_num;
}

static void
print_port(u_char prot, u_short port, const char comma)
{
	struct servent *se = NULL;
	struct protoent *pe;

	if (comma == ':') {
		printf("%c0x%04x", comma, port);
		return;
	}
	if (do_resolv) {
		pe = getprotobynumber(prot);
		se = getservbyport(htons(port), pe ? pe->p_name : NULL);
	}
	if (se)
		printf("%c%s", comma, se->s_name);
	else
		printf("%c%d", comma, port);
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

static void
show_ipfw(struct ip_fw *chain, int pcwidth, int bcwidth)
{
	char comma;
	u_long adrt;
	struct hostent *he;
	struct protoent *pe;
	int i, mb;
	int nsp = IP_FW_GETNSRCP(chain);
	int ndp = IP_FW_GETNDSTP(chain);

	if (do_resolv)
		setservent(1/*stay open*/);

	printf("%05u ", chain->fw_number);

	if (do_acct)
		printf("%*qu %*qu ", pcwidth, chain->fw_pcnt, bcwidth, chain->fw_bcnt);

	if (do_time) {
		if (chain->timestamp) {
			char timestr[30];

			strcpy(timestr, ctime((time_t *)&chain->timestamp));
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
		double d = 1.0 * (int)(chain->pipe_ptr);
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
			if(chain->fw_fwd_ip.sin_port)
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

	pe = getprotobynumber(chain->fw_prot);
	if (pe)
		printf(" %s", pe->p_name);
	else
		printf(" %u", chain->fw_prot);

	if (chain->fw_flg & IP_FW_F_SME) {
		printf(" from me");
	} else {
		printf(" from %s",
		    chain->fw_flg & IP_FW_F_INVSRC ? "not " : "");

		adrt = ntohl(chain->fw_smsk.s_addr);
		if (adrt == ULONG_MAX && do_resolv) {
			adrt = (chain->fw_src.s_addr);
			he = gethostbyaddr((char *)&adrt,
			    sizeof(u_long), AF_INET);
			if (he == NULL)
				printf("%s", inet_ntoa(chain->fw_src));
			else
				printf("%s", he->h_name);
		} else if (adrt != ULONG_MAX) {
			mb = mask_bits(chain->fw_smsk);
			if (mb == 0) {
				printf("any");
			} else if (mb > 0) {
				printf("%s", inet_ntoa(chain->fw_src));
				printf("/%d", mb);
			} else {
				printf("%s", inet_ntoa(chain->fw_src));
				printf(":");
				printf("%s", inet_ntoa(chain->fw_smsk));
			}
		} else {
			printf("%s", inet_ntoa(chain->fw_src));
		}
	}

	if (chain->fw_prot == IPPROTO_TCP || chain->fw_prot == IPPROTO_UDP) {
		comma = ' ';
		for (i = 0; i < nsp; i++) {
			print_port(chain->fw_prot,
			    chain->fw_uar.fw_pts[i], comma);
			if (i == 0 && (chain->fw_flg & IP_FW_F_SRNG))
				comma = '-';
			else if (i == 0 && (chain->fw_flg & IP_FW_F_SMSK))
				comma = ':';
			else
				comma = ',';
		}
	}

	if (chain->fw_flg & IP_FW_F_DME) {
		printf(" to me");
	} else {
		printf(" to %s", chain->fw_flg & IP_FW_F_INVDST ? "not " : "");

		adrt = ntohl(chain->fw_dmsk.s_addr);
		if (adrt == ULONG_MAX && do_resolv) {
			adrt = (chain->fw_dst.s_addr);
			he = gethostbyaddr((char *)&adrt,
			    sizeof(u_long), AF_INET);
			if (he == NULL)
				printf("%s", inet_ntoa(chain->fw_dst));
			else
				printf("%s", he->h_name);
		} else if (adrt != ULONG_MAX) {
			mb = mask_bits(chain->fw_dmsk);
			if (mb == 0) {
				printf("any");
			} else if (mb > 0) {
				printf("%s", inet_ntoa(chain->fw_dst));
				printf("/%d", mb);
			} else {
				printf("%s", inet_ntoa(chain->fw_dst));
				printf(":");
				printf("%s", inet_ntoa(chain->fw_dmsk));
			}
		} else {
			printf("%s", inet_ntoa(chain->fw_dst));
		}
	}

	if (chain->fw_prot == IPPROTO_TCP || chain->fw_prot == IPPROTO_UDP) {
		comma = ' ';
		for (i = 0; i < ndp; i++) {
			print_port(chain->fw_prot,
			    chain->fw_uar.fw_pts[nsp+i], comma);
			if (i == 0 && (chain->fw_flg & IP_FW_F_DRNG))
				comma = '-';
			else if (i == 0 && (chain->fw_flg & IP_FW_F_DMSK))
				comma = ':';
			else
				comma = ',';
		}
	}

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
		if (chain->next_rule_ptr)
			printf(" keep-state %d", (int)chain->next_rule_ptr);
		else
			printf(" keep-state");
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
		int	_opt_printed = 0;
#define PRINTOPT(x)	{if (_opt_printed) printf(",");\
			printf(x); _opt_printed = 1;}

		printf(" ipopt ");
		if (chain->fw_ipopt & IP_FW_IPOPT_SSRR)
			PRINTOPT("ssrr");
		if (chain->fw_ipnopt & IP_FW_IPOPT_SSRR)
			PRINTOPT("!ssrr");
		if (chain->fw_ipopt & IP_FW_IPOPT_LSRR)
			PRINTOPT("lsrr");
		if (chain->fw_ipnopt & IP_FW_IPOPT_LSRR)
			PRINTOPT("!lsrr");
		if (chain->fw_ipopt & IP_FW_IPOPT_RR)
			PRINTOPT("rr");
		if (chain->fw_ipnopt & IP_FW_IPOPT_RR)
			PRINTOPT("!rr");
		if (chain->fw_ipopt & IP_FW_IPOPT_TS)
			PRINTOPT("ts");
		if (chain->fw_ipnopt & IP_FW_IPOPT_TS)
			PRINTOPT("!ts");
	}

	if (chain->fw_ipflg & IP_FW_IF_TCPEST)
		printf(" established");
	else if (chain->fw_tcpf == IP_FW_TCPF_SYN &&
	    chain->fw_tcpnf == IP_FW_TCPF_ACK)
		printf(" setup");
	else if (chain->fw_tcpf || chain->fw_tcpnf) {
		int	_flg_printed = 0;
#define PRINTFLG(x)	{if (_flg_printed) printf(",");\
			printf(x); _flg_printed = 1;}

		printf(" tcpflags ");
		if (chain->fw_tcpf & IP_FW_TCPF_FIN)
			PRINTFLG("fin");
		if (chain->fw_tcpnf & IP_FW_TCPF_FIN)
			PRINTFLG("!fin");
		if (chain->fw_tcpf & IP_FW_TCPF_SYN)
			PRINTFLG("syn");
		if (chain->fw_tcpnf & IP_FW_TCPF_SYN)
			PRINTFLG("!syn");
		if (chain->fw_tcpf & IP_FW_TCPF_RST)
			PRINTFLG("rst");
		if (chain->fw_tcpnf & IP_FW_TCPF_RST)
			PRINTFLG("!rst");
		if (chain->fw_tcpf & IP_FW_TCPF_PSH)
			PRINTFLG("psh");
		if (chain->fw_tcpnf & IP_FW_TCPF_PSH)
			PRINTFLG("!psh");
		if (chain->fw_tcpf & IP_FW_TCPF_ACK)
			PRINTFLG("ack");
		if (chain->fw_tcpnf & IP_FW_TCPF_ACK)
			PRINTFLG("!ack");
		if (chain->fw_tcpf  & IP_FW_TCPF_URG)
			PRINTFLG("urg");
		if (chain->fw_tcpnf & IP_FW_TCPF_URG)
			PRINTFLG("!urg");
	}
	if (chain->fw_tcpopt || chain->fw_tcpnopt) {
		int	_opt_printed = 0;
#define PRINTTOPT(x)	{if (_opt_printed) printf(",");\
			printf(x); _opt_printed = 1;}

		printf(" tcpoptions ");
		if (chain->fw_tcpopt & IP_FW_TCPOPT_MSS)
			PRINTTOPT("mss");
		if (chain->fw_tcpnopt & IP_FW_TCPOPT_MSS)
			PRINTTOPT("!mss");
		if (chain->fw_tcpopt & IP_FW_TCPOPT_WINDOW)
			PRINTTOPT("window");
		if (chain->fw_tcpnopt & IP_FW_TCPOPT_WINDOW)
			PRINTTOPT("!window");
		if (chain->fw_tcpopt & IP_FW_TCPOPT_SACK)
			PRINTTOPT("sack");
		if (chain->fw_tcpnopt & IP_FW_TCPOPT_SACK)
			PRINTTOPT("!sack");
		if (chain->fw_tcpopt & IP_FW_TCPOPT_TS)
			PRINTTOPT("ts");
		if (chain->fw_tcpnopt & IP_FW_TCPOPT_TS)
			PRINTTOPT("!ts");
		if (chain->fw_tcpopt & IP_FW_TCPOPT_CC)
			PRINTTOPT("cc");
		if (chain->fw_tcpnopt & IP_FW_TCPOPT_CC)
			PRINTTOPT("!cc");
	}

	if (chain->fw_flg & IP_FW_F_ICMPBIT) {
		int type_index;
		int first = 1;

		printf(" icmptype");

		for (type_index = 0; type_index < IP_FW_ICMPTYPES_DIM * sizeof(unsigned) * 8; ++type_index)
			if (chain->fw_uar.fw_icmptypes[type_index / (sizeof(unsigned) * 8)] &
				(1U << (type_index % (sizeof(unsigned) * 8)))) {
				printf("%c%d", first == 1 ? ' ' : ',', type_index);
				first = 0;
			}
	}
	printf("\n");
done:
	if (do_resolv)
		endservent();
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
	struct ip_fw *rules;
	struct dn_pipe *pipes;
	void *data = NULL;
	int pcwidth = 0;
	int bcwidth = 0;
	int n, num = 0;
	int nbytes;

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
	/* determine num more accurately */
	num = 0;
	while (rules[num].fw_number < 65535)
		num++;
	num++; /* counting starts from 0 ... */
	/* if showing stats, figure out column widths ahead of time */
	if (do_acct) {
		for (n = 0; n < num; n++) {
			struct ip_fw *const r = &rules[n];
			char temp[32];
			int width;

			/* packet counter */
			width = sprintf(temp, "%qu", r->fw_pcnt);
			if (width > pcwidth)
				pcwidth = width;

			/* byte counter */
			width = sprintf(temp, "%qu", r->fw_bcnt);
			if (width > bcwidth)
				bcwidth = width;
		}
	}
	if (ac == 0) {
		/* display all rules */
		for (n = 0; n < num; n++) {
			struct ip_fw *const r = &rules[n];

			show_ipfw(r, pcwidth, bcwidth);
		}
	} else {
		/* display specific rules requested on command line */
		int exitval = EX_OK;

		while (ac--) {
			u_long rnum;
			char *endptr;
			int seen;

			/* convert command line rule # */
			rnum = strtoul(*av++, &endptr, 10);
			if (*endptr) {
				exitval = EX_USAGE;
				warnx("invalid rule number: %s", *(av - 1));
				continue;
			}
			do_dynamic = 0;
			for (seen = n = 0; n < num; n++) {
				struct ip_fw *const r = &rules[n];

				if (r->fw_number > rnum)
					break;
				if (r->fw_number == rnum) {
					show_ipfw(r, pcwidth, bcwidth);
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
		if (exitval != EX_OK)
			exit(exitval);
	}
	/*
	 * show dynamic rules
	*/
	if (do_dynamic && num * sizeof (rules[0]) != nbytes) {
		struct ipfw_dyn_rule *d =
		    (struct ipfw_dyn_rule *)&rules[num];
		struct in_addr a;
		struct protoent *pe;

            printf("## Dynamic rules:\n");
            for (;; d++) {
		if (d->expire == 0 && !do_expired) {
			if (d->next == NULL)
				break;
			continue;
		}

                printf("%05d %qu %qu (T %d, # %d) ty %d",
                    (int)(d->chain),
                    d->pcnt, d->bcnt,
                    d->expire,
                    d->bucket,
                    d->type);
		pe = getprotobynumber(d->id.proto);
		if (pe)
			printf(" %s,", pe->p_name);
		else
			printf(" %u,", d->id.proto);
                a.s_addr = htonl(d->id.src_ip);
                printf(" %s", inet_ntoa(a));
                printf(" %d", d->id.src_port);
                switch (d->type) {
                default: /* bidir, no mask */
                    printf(" <->");
                    break;
                }
                a.s_addr = htonl(d->id.dst_ip);
                printf(" %s", inet_ntoa(a));
                printf(" %d", d->id.dst_port);
                printf("\n");
                if (d->next == NULL)
                    break;
            }
        }

	free(data);
}

static void
show_usage(void)
{
	fprintf(stderr, "usage: ipfw [options]\n"
"    [pipe] flush\n"
"    add [number] rule\n"
"    [pipe] delete number ...\n"
"    [pipe] list [number ...]\n"
"    [pipe] show [number ...]\n"
"    zero [number ...]\n"
"    resetlog [number ...]\n"
"    pipe number config [pipeconfig]\n"
"  rule: [prob <match_probability>] action proto src dst extras...\n"
"    action:\n"
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
"    tcpoptions [!]{mss|window|sack|ts|cc}, ...\n"
"    icmptypes {type[, type]}...\n"
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
				if (atoi(p) == 0) {
					mask->s_addr = 0;
				} else if (atoi(p) > 32) {
					errx(EX_DATAERR, "bad width ``%s''", p);
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
		if ((s = getservbyname(buf, protocol))) {
			val = htons(s->s_port);
		} else {
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
	} else
	if (*s == '-') {
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

static void
fill_tcpflag(u_char *set, u_char *reset, char **vp)
{
	char *p = *vp, *q;
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
			errx(EX_DATAERR, "invalid tcp flag ``%s''", p);
		p = q;
	}
}

static void
fill_tcpopts(u_char *set, u_char *reset, char **vp)
{
	char *p = *vp, *q;
	u_char *d;

	while (p && *p) {
		struct tpcopts {
			char * name;
			u_char value;
		} opts[] = {
			{ "mss", IP_FW_TCPOPT_MSS },
			{ "window", IP_FW_TCPOPT_WINDOW },
			{ "sack", IP_FW_TCPOPT_SACK },
			{ "ts", IP_FW_TCPOPT_TS },
			{ "cc", IP_FW_TCPOPT_CC },
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
		for (i = 0; i < sizeof(opts) / sizeof(opts[0]); ++i)
			if (!strncmp(p, opts[i].name, strlen(p))) {
				*d |= opts[i].value;
				break;
			}
		if (i == sizeof(opts) / sizeof(opts[0]))
			errx(EX_DATAERR, "invalid tcp option ``%s''", p);
		p = q;
	}
}

static void
fill_ipopt(u_char *set, u_char *reset, char **vp)
{
	char *p = *vp, *q;
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
		if (!strncmp(p, "ssrr", strlen(p))) *d |= IP_FW_IPOPT_SSRR;
		if (!strncmp(p, "lsrr", strlen(p))) *d |= IP_FW_IPOPT_LSRR;
		if (!strncmp(p, "rr", strlen(p)))   *d |= IP_FW_IPOPT_RR;
		if (!strncmp(p, "ts", strlen(p)))   *d |= IP_FW_IPOPT_TS;
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
					fprintf(stderr, " mask is 0x%08x\n", a);
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
		if (pipe.fs.min_th >= pipe.fs.max_th)
			errx(EX_DATAERR, "min_th %d must be < than max_th %d",
			    pipe.fs.min_th, pipe.fs.max_th);
		if (pipe.fs.max_th == 0)
			errx(EX_DATAERR, "max_th must be > 0");
		if (pipe.bandwidth) {
			size_t len;
			int lookup_depth, avg_pkt_size;
			double s, idle, weight, w_q;
			struct clockinfo clock;
			int t;

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
				errx(EX_DATAERR, "net.inet.ip.dummynet.red_avg_pkt_size must"
				    " be greater than zero");

			len = sizeof(struct clockinfo);
			if (sysctlbyname("kern.clockrate",
			    &clock, &len, NULL, 0) == -1)
				errx(1, "sysctlbyname(\"%s\")",
				    "kern.clockrate");

			/* ticks needed for sending a medium-sized packet */
			s = clock.hz * avg_pkt_size * 8 / pipe.bandwidth;

			/*
			 * max idle time (in ticks) before avg queue size
			 * becomes 0.
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
			pipe.fs.lookup_weight =
			    (int)(weight * (1 << SCALE_RED));
		}
	}
#if 0
	printf("configuring pipe %d bw %d delay %d size %d\n",
	    pipe.pipe_nr, pipe.bandwidth, pipe.delay, pipe.queue_size);
#endif
	i = setsockopt(s, IPPROTO_IP, IP_DUMMYNET_CONFIGURE, &pipe,
	    sizeof pipe);
	if (i)
		err(1, "setsockopt(%s)", "IP_DUMMYNET_CONFIGURE");

}

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

	/* Rule number */
	if (ac && isdigit(**av)) {
		rule.fw_number = atoi(*av); av++; ac--;
	}

	/* Action */
	if (ac > 1 && !strncmp(*av, "prob", strlen(*av))) {
		double d = strtod(av[1], NULL);
		if (d <= 0 || d > 1)
			errx(EX_DATAERR, "illegal match prob. %s", av[1]);
		if (d != 1) { /* 1 means always match */
			rule.fw_flg |= IP_FW_F_RND_MATCH;
			/* we really store dont_match probability */
			(long)rule.pipe_ptr = (long)((1 - d) * 0x7fffffff);
		}
		av += 2; ac -= 2;
	}

	if (ac == 0)
		errx(EX_USAGE, "missing action");
	if (!strncmp(*av, "accept", strlen(*av))
		    || !strncmp(*av, "pass", strlen(*av))
		    || !strncmp(*av, "allow", strlen(*av))
		    || !strncmp(*av, "permit", strlen(*av))) {
		rule.fw_flg |= IP_FW_F_ACCEPT; av++; ac--;
	} else if (!strncmp(*av, "count", strlen(*av))) {
		rule.fw_flg |= IP_FW_F_COUNT; av++; ac--;
	} else if (!strncmp(*av, "pipe", strlen(*av))) {
		rule.fw_flg |= IP_FW_F_PIPE; av++; ac--;
		if (!ac)
			errx(EX_USAGE, "missing pipe number");
		rule.fw_divert_port = strtoul(*av, NULL, 0); av++; ac--;
	} else if (!strncmp(*av, "queue", strlen(*av))) {
		rule.fw_flg |= IP_FW_F_QUEUE; av++; ac--;
		if (!ac)
			errx(EX_USAGE, "missing queue number");
		rule.fw_divert_port = strtoul(*av, NULL, 0); av++; ac--;
	} else if (!strncmp(*av, "divert", strlen(*av))) {
		rule.fw_flg |= IP_FW_F_DIVERT; av++; ac--;
		if (!ac)
			errx(EX_USAGE, "missing %s port", "divert");
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
		if (!ac)
			errx(EX_USAGE, "missing %s port", "tee divert");
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
		if (!ac)
			errx(EX_USAGE, "missing forwarding IP address");
		rule.fw_fwd_ip.sin_len = sizeof(struct sockaddr_in);
		rule.fw_fwd_ip.sin_family = AF_INET;
		rule.fw_fwd_ip.sin_port = 0;
		pp = strchr(*av, ':');
		if(pp == NULL)
			pp = strchr(*av, ',');
		if(pp != NULL)
		{
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
		if (!ac)
			errx(EX_USAGE, "missing skipto rule number");
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

	/* [log] */
	if (ac && !strncmp(*av, "log", strlen(*av))) {
		rule.fw_flg |= IP_FW_F_PRN; av++; ac--;
	}
	if (ac && !strncmp(*av, "logamount", strlen(*av))) {
		if (!(rule.fw_flg & IP_FW_F_PRN))
			errx(EX_USAGE, "``logamount'' not valid without"
			     " ``log''");
		ac--; av++;
		if (!ac)
			errx(EX_USAGE, "``logamount'' requires argument");
		rule.fw_logamount = atoi(*av);
		if (rule.fw_logamount < 0)
			errx(EX_DATAERR, "``logamount'' argument must be"
			     " positive");
		if (rule.fw_logamount == 0)
			rule.fw_logamount = -1;
		ac--; av++;
	}

	/* protocol */
	if (ac == 0)
		errx(EX_USAGE, "missing protocol");
	if ((proto = atoi(*av)) > 0) {
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

	/* from */
	if (ac && !strncmp(*av, "from", strlen(*av))) { av++; ac--; }
	else
		errx(EX_USAGE, "missing ``from''");

	if (ac && !strncmp(*av, "not", strlen(*av))) {
		rule.fw_flg |= IP_FW_F_INVSRC;
		av++; ac--;
	}
	if (!ac)
		errx(EX_USAGE, "missing arguments");

	if (ac && !strncmp(*av, "me", strlen(*av))) {
		rule.fw_flg |= IP_FW_F_SME;
		av++; ac--;
	} else {
		fill_ip(&rule.fw_src, &rule.fw_smsk, &ac, &av);
	}

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

	/* to */
	if (ac && !strncmp(*av, "to", strlen(*av))) { av++; ac--; }
	else
		errx(EX_USAGE, "missing ``to''");

	if (ac && !strncmp(*av, "not", strlen(*av))) {
		rule.fw_flg |= IP_FW_F_INVDST;
		av++; ac--;
	}
	if (!ac)
		errx(EX_USAGE, "missing arguments");


	if (ac && !strncmp(*av, "me", strlen(*av))) {
		rule.fw_flg |= IP_FW_F_DME;
		av++; ac--;
	} else {
		fill_ip(&rule.fw_dst, &rule.fw_dmsk, &ac, &av);
	}

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

	while (ac) {
		if (!strncmp(*av, "uid", strlen(*av))) {
			struct passwd *pwd;
			char *end;
			uid_t uid;

			rule.fw_flg |= IP_FW_F_UID;
			ac--; av++;
			if (!ac)
				errx(EX_USAGE, "``uid'' requires argument");

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
			if (!ac)
				errx(EX_USAGE, "``gid'' requires argument");

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
		} else if (!strncmp(*av, "keep-state", strlen(*av))) {
			u_long type;
			rule.fw_flg |= IP_FW_F_KEEP_S;

			av++; ac--;
			if (ac > 0 && (type = atoi(*av)) != 0) {
				(int)rule.next_rule_ptr = type;
				av++; ac--;
			}
		} else if (!strncmp(*av, "bridged", strlen(*av))) {
			rule.fw_flg |= IP_FW_BRIDGED;
			av++; ac--;
		} else if (!strncmp(*av, "out", strlen(*av))) {
			rule.fw_flg |= IP_FW_F_OUT;
			av++; ac--;
		} else if (ac && !strncmp(*av, "xmit", strlen(*av))) {
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
		} else if (ac && !strncmp(*av, "recv", strlen(*av))) {
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
		} else if (ac && !strncmp(*av, "via", strlen(*av))) {
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
		} else if (!strncmp(*av, "ipoptions", strlen(*av))) {
			av++; ac--;
			if (!ac)
				errx(EX_USAGE, "missing argument"
				    " for ``ipoptions''");
			fill_ipopt(&rule.fw_ipopt, &rule.fw_ipnopt, av);
			av++; ac--;
		} else if (rule.fw_prot == IPPROTO_TCP) {
			if (!strncmp(*av, "established", strlen(*av))) {
				rule.fw_ipflg |= IP_FW_IF_TCPEST;
				av++; ac--;
			} else if (!strncmp(*av, "setup", strlen(*av))) {
				rule.fw_tcpf  |= IP_FW_TCPF_SYN;
				rule.fw_tcpnf  |= IP_FW_TCPF_ACK;
				av++; ac--;
			} else if (!strncmp(*av, "tcpflags", strlen(*av))
			    || !strncmp(*av, "tcpflgs", strlen(*av))) {
				av++; ac--;
				if (!ac)
					errx(EX_USAGE, "missing argument"
					    " for ``tcpflags''");
				fill_tcpflag(&rule.fw_tcpf,
				    &rule.fw_tcpnf, av);
				av++; ac--;
			} else if (!strncmp(*av, "tcpoptions", strlen(*av))
			    || !strncmp(*av, "tcpopts", strlen(*av))) {
				av++; ac--;
				if (!ac)
					errx(EX_USAGE, "missing argument"
					    " for ``tcpoptions''");
				fill_tcpopts(&rule.fw_tcpopt,
				    &rule.fw_tcpnopt, av);
				av++; ac--;
			}
		} else if (rule.fw_prot == IPPROTO_ICMP) {
			if (!strncmp(*av, "icmptypes", strlen(*av))) {
				av++; ac--;
				if (!ac)
					errx(EX_USAGE, "missing argument"
					    " for ``icmptypes''");
				fill_icmptypes(rule.fw_uar.fw_icmptypes,
				    av, &rule.fw_flg);
				av++; ac--;
			}
		} else {
			errx(EX_USAGE, "unknown argument ``%s''", *av);
		}
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
		show_ipfw(&rule, 10, 10);
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

static int
ipfw_main(int ac, char **av)
{
	int ch;

	if (ac == 1)
		show_usage();

	/* Initialize globals. */
	do_resolv = do_acct = do_time = do_quiet =
	do_pipe = do_sort = verbose = 0;

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
	if (*(av += optind) == NULL)
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

	/* allow argument swapping */
	if (ac > 1 && *av[0] >= '0' && *av[0] <= '9') {
		char *p = av[0];
		av[0] = av[1];
		av[1] = p;
	}
	if (!strncmp(*av, "add", strlen(*av))) {
		add(ac, av);
	} else if (do_pipe && !strncmp(*av, "config", strlen(*av))) {
		config_pipe(ac, av);
	} else if (!strncmp(*av, "delete", strlen(*av))) {
		delete(ac, av);
	} else if (!strncmp(*av, "flush", strlen(*av))) {
		int do_flush = 0;

		if (do_force || do_quiet)
			do_flush = 1;
		else {
			int c;

			/* Ask the user */
			printf("Are you sure? [yn] ");
			fflush(stdout);
			do {
				c = toupper(getc(stdin));
				while (c != '\n' && getc(stdin) != '\n')
					if (feof(stdin))
						return (0);
			} while (c != 'Y' && c != 'N');
			printf("\n");
			if (c == 'Y')
				do_flush = 1;
		}
		if (do_flush) {
			if (setsockopt(s, IPPROTO_IP,
			    do_pipe ? IP_DUMMYNET_FLUSH : IP_FW_FLUSH,
			    NULL, 0) < 0)
				err(EX_UNAVAILABLE, "setsockopt(IP_%s_FLUSH)",
				    do_pipe ? "DUMMYNET" : "FW");
			if (!do_quiet)
				printf("Flushed all %s.\n",
				    do_pipe ? "pipes" : "rules");
		}
	} else if (!strncmp(*av, "zero", strlen(*av))) {
		zero(ac, av);
	} else if (!strncmp(*av, "resetlog", strlen(*av))) {
		resetlog(ac, av);
	} else if (!strncmp(*av, "print", strlen(*av))) {
		list(--ac, ++av);
	} else if (!strncmp(*av, "list", strlen(*av))) {
		list(--ac, ++av);
	} else if (!strncmp(*av, "show", strlen(*av))) {
		do_acct++;
		list(--ac, ++av);
	} else {
		errx(EX_USAGE, "bad arguments, for usage summary ``ipfw''");
	}
	return 0;
}

int
main(int ac, char *av[])
{
#define MAX_ARGS	32
#define WHITESP		" \t\f\v\n\r"
	char	buf[BUFSIZ];
	char	*a, *p, *args[MAX_ARGS], *cmd = NULL;
	char	linename[10];
	int	i, c, lineno, qflag, pflag, status;
	FILE	*f = NULL;
	pid_t	preproc = 0;

	s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
	if (s < 0)
		err(EX_UNAVAILABLE, "socket");

	setbuf(stdout, 0);

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
	} else {
		ipfw_main(ac, av);
	}
	return EX_OK;
}
