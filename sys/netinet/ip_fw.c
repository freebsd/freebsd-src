/*
 * Copyright (c) 1993 Daniel Boulet
 * Copyright (c) 1994 Ugen J.S.Antsilevich
 * Copyright (c) 1996 Alex Nash
 * Copyright (c) 2000-2001 Luigi Rizzo
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
 * $FreeBSD$
 */

#define        DEB(x)
#define        DDB(x) x

/*
 * Implement IP packet firewall
 */

#if !defined(KLD_MODULE)
#include "opt_ipfw.h"
#include "opt_ipdn.h"
#include "opt_ipdivert.h"
#include "opt_inet.h"
#ifndef INET
#error IPFIREWALL requires INET.
#endif /* INET */
#endif

#if !(IPFW2)

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/ucred.h>
#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/in_pcb.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip_fw.h>
#include <netinet/ip_dummynet.h>
#include <netinet/tcp.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/tcpip.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>

#include <netinet/if_ether.h> /* XXX ethertype_ip */

static int fw_debug = 1;
#ifdef IPFIREWALL_VERBOSE
static int fw_verbose = 1;
#else
static int fw_verbose = 0;
#endif
#ifdef IPFIREWALL_VERBOSE_LIMIT
static int fw_verbose_limit = IPFIREWALL_VERBOSE_LIMIT;
#else
static int fw_verbose_limit = 0;
#endif

/*
 * Right now, two fields in the IP header are changed to host format
 * by the IP layer before calling the firewall. Ideally, we would like
 * to have them in network format so that the packet can be
 * used as it comes from the device driver (and is thus readonly).
 */

static u_int64_t counter;	/* counter for ipfw_report(NULL...) */

#define	IPFW_DEFAULT_RULE	((u_int)(u_short)~0)

LIST_HEAD (ip_fw_head, ip_fw) ip_fw_chain_head;

MALLOC_DEFINE(M_IPFW, "IpFw/IpAcct", "IpFw/IpAcct chain's");

#ifdef SYSCTL_NODE
SYSCTL_NODE(_net_inet_ip, OID_AUTO, fw, CTLFLAG_RW, 0, "Firewall");
SYSCTL_INT(_net_inet_ip_fw, OID_AUTO, enable, CTLFLAG_RW,
    &fw_enable, 0, "Enable ipfw");
SYSCTL_INT(_net_inet_ip_fw, OID_AUTO,one_pass,CTLFLAG_RW, 
    &fw_one_pass, 0, 
    "Only do a single pass through ipfw when using dummynet(4)");
SYSCTL_INT(_net_inet_ip_fw, OID_AUTO, debug, CTLFLAG_RW, 
    &fw_debug, 0, "Enable printing of debug ip_fw statements");
SYSCTL_INT(_net_inet_ip_fw, OID_AUTO, verbose, CTLFLAG_RW, 
    &fw_verbose, 0, "Log matches to ipfw rules");
SYSCTL_INT(_net_inet_ip_fw, OID_AUTO, verbose_limit, CTLFLAG_RW, 
    &fw_verbose_limit, 0, "Set upper limit of matches of ipfw rules logged");

/*
 * Extension for stateful ipfw.
 *
 * Dynamic rules are stored in lists accessed through a hash table
 * (ipfw_dyn_v) whose size is curr_dyn_buckets. This value can
 * be modified through the sysctl variable dyn_buckets which is
 * updated when the table becomes empty.
 *
 * XXX currently there is only one list, ipfw_dyn.
 *
 * When a packet is received, it is first hashed, then matched
 * against the entries in the corresponding list.
 * Matching occurs according to the rule type. The default is to
 * match the four fields and the protocol, and rules are bidirectional.
 *
 * For a busy proxy/web server we will have lots of connections to
 * the server. We could decide for a rule type where we ignore
 * ports (different hashing) and avoid special SYN/RST/FIN handling.
 *
 * XXX when we decide to support more than one rule type, we should
 * repeat the hashing multiple times uing only the useful fields.
 * Or, we could run the various tests in parallel, because the
 * 'move to front' technique should shorten the average search.
 *
 * The lifetime of dynamic rules is regulated by dyn_*_lifetime,
 * measured in seconds and depending on the flags.
 *
 * The total number of dynamic rules is stored in dyn_count.
 * The max number of dynamic rules is dyn_max. When we reach
 * the maximum number of rules we do not create anymore. This is
 * done to avoid consuming too much memory, but also too much
 * time when searching on each packet (ideally, we should try instead
 * to put a limit on the length of the list on each bucket...).
 *
 * Each dynamic rules holds a pointer to the parent ipfw rule so
 * we know what action to perform. Dynamic rules are removed when
 * the parent rule is deleted.
 * There are some limitations with dynamic rules -- we do not
 * obey the 'randomized match', and we do not do multiple
 * passes through the firewall.
 * XXX check the latter!!!
 */
static struct ipfw_dyn_rule **ipfw_dyn_v = NULL ;
static u_int32_t dyn_buckets = 256 ; /* must be power of 2 */
static u_int32_t curr_dyn_buckets = 256 ; /* must be power of 2 */

/*
 * timeouts for various events in handing dynamic rules.
 */
static u_int32_t dyn_ack_lifetime = 300 ;
static u_int32_t dyn_syn_lifetime = 20 ;
static u_int32_t dyn_fin_lifetime = 1 ;
static u_int32_t dyn_rst_lifetime = 1 ;
static u_int32_t dyn_udp_lifetime = 10 ;
static u_int32_t dyn_short_lifetime = 5 ;

/*
 * after reaching 0, dynamic rules are considered still valid for
 * an additional grace time, unless there is lack of resources.
 */
static u_int32_t dyn_grace_time = 10 ;

static u_int32_t static_count = 0 ;	/* # of static rules */
static u_int32_t dyn_count = 0 ;	/* # of dynamic rules */
static u_int32_t dyn_max = 1000 ;	/* max # of dynamic rules */

SYSCTL_INT(_net_inet_ip_fw, OID_AUTO, dyn_buckets, CTLFLAG_RW,
    &dyn_buckets, 0, "Number of dyn. buckets");
SYSCTL_INT(_net_inet_ip_fw, OID_AUTO, curr_dyn_buckets, CTLFLAG_RD,
    &curr_dyn_buckets, 0, "Current Number of dyn. buckets");
SYSCTL_INT(_net_inet_ip_fw, OID_AUTO, dyn_count, CTLFLAG_RD,
    &dyn_count, 0, "Number of dyn. rules");
SYSCTL_INT(_net_inet_ip_fw, OID_AUTO, dyn_max, CTLFLAG_RW,
    &dyn_max, 0, "Max number of dyn. rules");
SYSCTL_INT(_net_inet_ip_fw, OID_AUTO, static_count, CTLFLAG_RD,
    &static_count, 0, "Number of static rules");
SYSCTL_INT(_net_inet_ip_fw, OID_AUTO, dyn_ack_lifetime, CTLFLAG_RW,
    &dyn_ack_lifetime, 0, "Lifetime of dyn. rules for acks");
SYSCTL_INT(_net_inet_ip_fw, OID_AUTO, dyn_syn_lifetime, CTLFLAG_RW,
    &dyn_syn_lifetime, 0, "Lifetime of dyn. rules for syn");
SYSCTL_INT(_net_inet_ip_fw, OID_AUTO, dyn_fin_lifetime, CTLFLAG_RW,
    &dyn_fin_lifetime, 0, "Lifetime of dyn. rules for fin");
SYSCTL_INT(_net_inet_ip_fw, OID_AUTO, dyn_rst_lifetime, CTLFLAG_RW,
    &dyn_rst_lifetime, 0, "Lifetime of dyn. rules for rst");
SYSCTL_INT(_net_inet_ip_fw, OID_AUTO, dyn_udp_lifetime, CTLFLAG_RW,
    &dyn_udp_lifetime, 0, "Lifetime of dyn. rules for UDP");
SYSCTL_INT(_net_inet_ip_fw, OID_AUTO, dyn_short_lifetime, CTLFLAG_RW,
    &dyn_short_lifetime, 0, "Lifetime of dyn. rules for other situations");
SYSCTL_INT(_net_inet_ip_fw, OID_AUTO, dyn_grace_time, CTLFLAG_RD,
    &dyn_grace_time, 0, "Grace time for dyn. rules");

#endif /* SYSCTL_NODE */

#define dprintf(a)	do {						\
				if (fw_debug)				\
					printf a;			\
			} while (0)
#define SNPARGS(buf, len) buf + len, sizeof(buf) > len ? sizeof(buf) - len : 0

static int	add_entry (struct ip_fw_head *chainptr, struct ip_fw *frwl);
static int	del_entry (struct ip_fw_head *chainptr, u_short number);
static int	zero_entry (struct ip_fw *, int);
static int	check_ipfw_struct (struct ip_fw *m);
static int	iface_match (struct ifnet *ifp, union ip_fw_if *ifu,
				 int byname);
static int	ipopts_match (struct ip *ip, struct ip_fw *f);
static __inline int
		port_match (u_short *portptr, int nports, u_short port,
				int range_flag, int mask);
static int	tcpflg_match (struct tcphdr *tcp, struct ip_fw *f);
static int	icmptype_match (struct icmp *  icmp, struct ip_fw * f);
static void	ipfw_report (struct ip_fw *f, struct ip *ip, int ip_off,
				int ip_len, struct ifnet *rif,
				struct ifnet *oif);

static void	flush_rule_ptrs(void);

static ip_fw_chk_t ip_fw_chk;
static int	ip_fw_ctl (struct sockopt *sopt);

ip_dn_ruledel_t *ip_dn_ruledel_ptr = NULL;

static char err_prefix[] = "ip_fw_ctl:";

/*
 * Returns 1 if the port is matched by the vector, 0 otherwise
 */
static __inline int 
port_match(u_short *portptr, int nports, u_short port, int range_flag, int mask)
{
	if (!nports)
		return 1;
	if (mask) {
		if ( 0 == ((portptr[0] ^ port) & portptr[1]) )
			return 1;
		nports -= 2;
		portptr += 2;
	}
	if (range_flag) {
		if (portptr[0] <= port && port <= portptr[1])
			return 1;
		nports -= 2;
		portptr += 2;
	}
	while (nports-- > 0)
		if (*portptr++ == port)
			return 1;
	return 0;
}

static int
tcpflg_match(struct tcphdr *tcp, struct ip_fw *f)
{
	u_char		flg_set, flg_clr;

	/*
	 * If an established connection is required, reject packets that
	 * have only SYN of RST|ACK|SYN set.  Otherwise, fall through to
	 * other flag requirements.
	 */
	if ((f->fw_ipflg & IP_FW_IF_TCPEST) &&
	    ((tcp->th_flags & (IP_FW_TCPF_RST | IP_FW_TCPF_ACK |
	    IP_FW_TCPF_SYN)) == IP_FW_TCPF_SYN))
		return 0;

	flg_set = tcp->th_flags & f->fw_tcpf;
	flg_clr = tcp->th_flags & f->fw_tcpnf;

	if (flg_set != f->fw_tcpf)
		return 0;
	if (flg_clr)
		return 0;

	return 1;
}

static int
icmptype_match(struct icmp *icmp, struct ip_fw *f)
{
	int type;

	if (!(f->fw_flg & IP_FW_F_ICMPBIT))
		return(1);

	type = icmp->icmp_type;

	/* check for matching type in the bitmap */
	if (type < IP_FW_ICMPTYPES_MAX &&
	    (f->fw_uar.fw_icmptypes[type / (sizeof(unsigned) * NBBY)] & 
	    (1U << (type % (sizeof(unsigned) * NBBY)))))
		return(1);

	return(0); /* no match */
}

static int
is_icmp_query(struct ip *ip)
{
	const struct icmp *icmp;
	int icmp_type;

	icmp = (struct icmp *)((u_int32_t *)ip + ip->ip_hl);
	icmp_type = icmp->icmp_type;

	if (icmp_type == ICMP_ECHO || icmp_type == ICMP_ROUTERSOLICIT ||
	    icmp_type == ICMP_TSTAMP || icmp_type == ICMP_IREQ ||
	    icmp_type == ICMP_MASKREQ)
		return(1);

	return(0);
}

static int
ipopts_match(struct ip *ip, struct ip_fw *f)
{
	register u_char *cp;
	int opt, optlen, cnt;
	u_char	opts, nopts, nopts_sve;

	cp = (u_char *)(ip + 1);
	cnt = (ip->ip_hl << 2) - sizeof (struct ip);
	opts = f->fw_ipopt;
	nopts = nopts_sve = f->fw_ipnopt;

	for (; cnt > 0; cnt -= optlen, cp += optlen) {
		opt = cp[IPOPT_OPTVAL];
		if (opt == IPOPT_EOL)
			break;
		if (opt == IPOPT_NOP)
			optlen = 1;
		else {
			optlen = cp[IPOPT_OLEN];
			if (optlen <= 0 || optlen > cnt) {
				return 0; /*XXX*/
			}
		}
		switch (opt) {

		default:
			break;

		case IPOPT_LSRR:
			opts &= ~IP_FW_IPOPT_LSRR;
			nopts &= ~IP_FW_IPOPT_LSRR;
			break;

		case IPOPT_SSRR:
			opts &= ~IP_FW_IPOPT_SSRR;
			nopts &= ~IP_FW_IPOPT_SSRR;
			break;

		case IPOPT_RR:
			opts &= ~IP_FW_IPOPT_RR;
			nopts &= ~IP_FW_IPOPT_RR;
			break;
		case IPOPT_TS:
			opts &= ~IP_FW_IPOPT_TS;
			nopts &= ~IP_FW_IPOPT_TS;
			break;
		}
		if (opts == nopts)
			break;
	}
	if (opts == 0 && nopts == nopts_sve)
		return 1;
	else
		return 0;
}

static int
tcpopts_match(struct tcphdr *tcp, struct ip_fw *f)
{
	register u_char *cp;
	int opt, optlen, cnt;
	u_char	opts, nopts, nopts_sve;

	cp = (u_char *)(tcp + 1);
	cnt = (tcp->th_off << 2) - sizeof (struct tcphdr);
	opts = f->fw_tcpopt;
	nopts = nopts_sve = f->fw_tcpnopt;

	for (; cnt > 0; cnt -= optlen, cp += optlen) {
		opt = cp[0];
		if (opt == TCPOPT_EOL)
			break;
		if (opt == TCPOPT_NOP)
			optlen = 1;
		else {
			optlen = cp[1];
			if (optlen <= 0)
				break;
		}


		switch (opt) {

		default:
			break;

		case TCPOPT_MAXSEG:
			opts &= ~IP_FW_TCPOPT_MSS;
			nopts &= ~IP_FW_TCPOPT_MSS;
			break;

		case TCPOPT_WINDOW:
			opts &= ~IP_FW_TCPOPT_WINDOW;
			nopts &= ~IP_FW_TCPOPT_WINDOW;
			break;

		case TCPOPT_SACK_PERMITTED:
		case TCPOPT_SACK:
			opts &= ~IP_FW_TCPOPT_SACK;
			nopts &= ~IP_FW_TCPOPT_SACK;
			break;

		case TCPOPT_TIMESTAMP:
			opts &= ~IP_FW_TCPOPT_TS;
			nopts &= ~IP_FW_TCPOPT_TS;
			break;

		case TCPOPT_CC:
		case TCPOPT_CCNEW:
		case TCPOPT_CCECHO:
			opts &= ~IP_FW_TCPOPT_CC;
			nopts &= ~IP_FW_TCPOPT_CC;
			break;
		}
		if (opts == nopts)
			break;
	}
	if (opts == 0 && nopts == nopts_sve)
		return 1;
	else
		return 0;
}

static int
iface_match(struct ifnet *ifp, union ip_fw_if *ifu, int byname)
{
	/* Check by name or by IP address */
	if (byname) {
		/* Check unit number (-1 is wildcard) */
		if (ifu->fu_via_if.unit != -1
		    && ifp->if_unit != ifu->fu_via_if.unit)
			return(0);
		/* Check name */
		if (strncmp(ifp->if_name, ifu->fu_via_if.name, FW_IFNLEN))
			return(0);
		return(1);
	} else if (ifu->fu_via_ip.s_addr != 0) {	/* Zero == wildcard */
		struct ifaddr *ia;

		TAILQ_FOREACH(ia, &ifp->if_addrhead, ifa_link) {
			if (ia->ifa_addr == NULL)
				continue;
			if (ia->ifa_addr->sa_family != AF_INET)
				continue;
			if (ifu->fu_via_ip.s_addr != ((struct sockaddr_in *)
			    (ia->ifa_addr))->sin_addr.s_addr)
				continue;
			return(1);
		}
		return(0);
	}
	return(1);
}

static void
ipfw_report(struct ip_fw *f, struct ip *ip, int ip_off, int ip_len,
	struct ifnet *rif, struct ifnet *oif)
{
    struct tcphdr *const tcp = (struct tcphdr *) ((u_int32_t *) ip+ ip->ip_hl);
    struct udphdr *const udp = (struct udphdr *) ((u_int32_t *) ip+ ip->ip_hl);
    struct icmp *const icmp = (struct icmp *) ((u_int32_t *) ip + ip->ip_hl);
    u_int64_t count;
    char *action;
    char action2[32], proto[47], name[18], fragment[27];
    int len;
    int offset = ip_off & IP_OFFMASK;

    count = f ? f->fw_pcnt : ++counter;
    if ((f == NULL && fw_verbose_limit != 0 && count > fw_verbose_limit) ||
	(f && f->fw_logamount != 0 && count > f->fw_loghighest))
	    return;

    /* Print command name */
    snprintf(SNPARGS(name, 0), "ipfw: %d", f ? f->fw_number : -1);

    action = action2;
    if (!f)
	    action = "Refuse";
    else {
	    switch (f->fw_flg & IP_FW_F_COMMAND) {
	    case IP_FW_F_DENY:
		    action = "Deny";
		    break;
	    case IP_FW_F_REJECT:
		    if (f->fw_reject_code == IP_FW_REJECT_RST)
			    action = "Reset";
		    else
			    action = "Unreach";
		    break;
	    case IP_FW_F_ACCEPT:
		    action = "Accept";
		    break;
	    case IP_FW_F_COUNT:
		    action = "Count";
		    break;
#ifdef IPDIVERT
	    case IP_FW_F_DIVERT:
		    snprintf(SNPARGS(action2, 0), "Divert %d",
			f->fw_divert_port);
		    break;
	    case IP_FW_F_TEE:
		    snprintf(SNPARGS(action2, 0), "Tee %d",
			f->fw_divert_port);
		    break;
#endif
	    case IP_FW_F_SKIPTO:
		    snprintf(SNPARGS(action2, 0), "SkipTo %d",
			f->fw_skipto_rule);
		    break;
	    case IP_FW_F_PIPE:
		    snprintf(SNPARGS(action2, 0), "Pipe %d",
			f->fw_skipto_rule);
		    break;
	    case IP_FW_F_QUEUE:
		    snprintf(SNPARGS(action2, 0), "Queue %d",
			f->fw_skipto_rule);
		    break;

	    case IP_FW_F_FWD:
		    if (f->fw_fwd_ip.sin_port)
			    snprintf(SNPARGS(action2, 0),
				"Forward to %s:%d",
				inet_ntoa(f->fw_fwd_ip.sin_addr),
				f->fw_fwd_ip.sin_port);
		    else
			    snprintf(SNPARGS(action2, 0), "Forward to %s",
				inet_ntoa(f->fw_fwd_ip.sin_addr));
		    break;

	    default:	
		    action = "UNKNOWN";
		    break;
	    }
    }

    switch (ip->ip_p) {
    case IPPROTO_TCP:
	    len = snprintf(SNPARGS(proto, 0), "TCP %s",
		inet_ntoa(ip->ip_src));
	    if (offset == 0)
		    len += snprintf(SNPARGS(proto, len), ":%d ",
			ntohs(tcp->th_sport));
	    else
		    len += snprintf(SNPARGS(proto, len), " ");
	    len += snprintf(SNPARGS(proto, len), "%s",
		inet_ntoa(ip->ip_dst));
	    if (offset == 0)
		    snprintf(SNPARGS(proto, len), ":%d",
			ntohs(tcp->th_dport));
	    break;
    case IPPROTO_UDP:
	    len = snprintf(SNPARGS(proto, 0), "UDP %s",
		inet_ntoa(ip->ip_src));
	    if (offset == 0)
		    len += snprintf(SNPARGS(proto, len), ":%d ",
			ntohs(udp->uh_sport));
	    else
		    len += snprintf(SNPARGS(proto, len), " ");
	    len += snprintf(SNPARGS(proto, len), "%s",
		inet_ntoa(ip->ip_dst));
	    if (offset == 0)
		    snprintf(SNPARGS(proto, len), ":%d",
			ntohs(udp->uh_dport));
	    break;
    case IPPROTO_ICMP:
	    if (offset == 0)
		    len = snprintf(SNPARGS(proto, 0), "ICMP:%u.%u ",
			icmp->icmp_type, icmp->icmp_code);
	    else
		    len = snprintf(SNPARGS(proto, 0), "ICMP ");
	    len += snprintf(SNPARGS(proto, len), "%s",
		inet_ntoa(ip->ip_src));
	    snprintf(SNPARGS(proto, len), " %s", inet_ntoa(ip->ip_dst));
	    break;
    default:
	    len = snprintf(SNPARGS(proto, 0), "P:%d %s", ip->ip_p,
		inet_ntoa(ip->ip_src));
	    snprintf(SNPARGS(proto, len), " %s", inet_ntoa(ip->ip_dst));
	    break;
    }

    if (ip_off & (IP_MF | IP_OFFMASK))
	    snprintf(SNPARGS(fragment, 0), " (frag %d:%d@%d%s)",
		     ntohs(ip->ip_id), ip_len - (ip->ip_hl << 2),
		     offset << 3,
		     (ip_off & IP_MF) ? "+" : "");
    else
	    fragment[0] = '\0';
    if (oif)
	    log(LOG_SECURITY | LOG_INFO, "%s %s %s out via %s%d%s\n",
		name, action, proto, oif->if_name, oif->if_unit, fragment);
    else if (rif)
	    log(LOG_SECURITY | LOG_INFO, "%s %s %s in via %s%d%s\n", name,
		action, proto, rif->if_name, rif->if_unit, fragment);
    else
	    log(LOG_SECURITY | LOG_INFO, "%s %s %s%s\n", name, action,
		proto, fragment);
    if ((f ? f->fw_logamount != 0 : 1) &&
	count == (f ? f->fw_loghighest : fw_verbose_limit))
	    log(LOG_SECURITY | LOG_NOTICE,
		"ipfw: limit %d reached on entry %d\n",
		f ? f->fw_logamount : fw_verbose_limit,
		f ? f->fw_number : -1);
}

static __inline int
hash_packet(struct ipfw_flow_id *id)
{
    u_int32_t i ;

    i = (id->dst_ip) ^ (id->src_ip) ^ (id->dst_port) ^ (id->src_port);
    i &= (curr_dyn_buckets - 1) ;
    return i ;
}

/**
 * unlink a dynamic rule from a chain. prev is a pointer to
 * the previous one, q is a pointer to the rule to delete,
 * head is a pointer to the head of the queue.
 * Modifies q and potentially also head.
 */
#define UNLINK_DYN_RULE(prev, head, q) {				\
	struct ipfw_dyn_rule *old_q = q;				\
									\
	/* remove a refcount to the parent */				\
	if (q->dyn_type == DYN_LIMIT)					\
		q->parent->count--;					\
	DEB(printf("-- unlink entry 0x%08x %d -> 0x%08x %d, %d left\n", \
		(q->id.src_ip), (q->id.src_port),			\
		(q->id.dst_ip), (q->id.dst_port), dyn_count-1 ); )	\
	if (prev != NULL)						\
		prev->next = q = q->next ;				\
	else								\
		ipfw_dyn_v[i] = q = q->next ;				\
	dyn_count-- ;							\
	free(old_q, M_IPFW); }

#define TIME_LEQ(a,b)       ((int)((a)-(b)) <= 0)
/**
 * Remove all dynamic rules pointing to a given rule, or all
 * rules if rule == NULL. Second parameter is 1 if we want to
 * delete unconditionally, otherwise only expired rules are removed.
 */
static void
remove_dyn_rule(struct ip_fw *rule, int force)
{
    struct ipfw_dyn_rule *prev, *q;
    int i, pass, max_pass ;
    static u_int32_t last_remove = 0 ;

    if (ipfw_dyn_v == NULL || dyn_count == 0)
	return ;
    /* do not expire more than once per second, it is useless */
    if (force == 0 && last_remove == time_second)
	return ;
    last_remove = time_second ;

    /*
     * because DYN_LIMIT refer to parent rules, during the first pass only
     * remove child and mark any pending LIMIT_PARENT, and remove
     * them in a second pass.
     */
  for (pass = max_pass = 0; pass <= max_pass ; pass++ ) {
    for (i = 0 ; i < curr_dyn_buckets ; i++) {
	for (prev=NULL, q = ipfw_dyn_v[i] ; q ; ) {
	    /*
	     * logic can become complex here, so we split tests.
	     * First, test if we match any rule,
	     * then make sure the rule is expired or we want to kill it,
	     * and possibly more in the future.
	     */
	    int zap = ( rule == NULL || rule == q->rule);
	    if (zap)
		zap = force || TIME_LEQ( q->expire , time_second );
	    /* do not zap parent in first pass, record we need a second pass */
	    if (zap && q->dyn_type == DYN_LIMIT_PARENT) {
		max_pass = 1; /* we need a second pass */
		if (pass == 0 || q->count != 0) {
		    zap = 0 ;
		    if (pass == 1 && force) /* should not happen */
			printf("OUCH! cannot remove rule, count %d\n",
				q->count);
		}
	    }
	    if (zap) {
		UNLINK_DYN_RULE(prev, ipfw_dyn_v[i], q);
	    } else {
		prev = q ;
		q = q->next ;
	    }
	}
    }
  }
}

#define EXPIRE_DYN_CHAIN(rule) remove_dyn_rule(rule, 0 /* expired ones */)
#define EXPIRE_DYN_CHAINS() remove_dyn_rule(NULL, 0 /* expired ones */)
#define DELETE_DYN_CHAIN(rule) remove_dyn_rule(rule, 1 /* force removal */)
#define DELETE_DYN_CHAINS() remove_dyn_rule(NULL, 1 /* force removal */)

/**
 * lookup a dynamic rule.
 */
static struct ipfw_dyn_rule *
lookup_dyn_rule(struct ipfw_flow_id *pkt, int *match_direction)
{
    /*
     * stateful ipfw extensions.
     * Lookup into dynamic session queue
     */
    struct ipfw_dyn_rule *prev, *q ;
    int i, dir = 0;
#define MATCH_FORWARD 1

    if (ipfw_dyn_v == NULL)
	return NULL ;
    i = hash_packet( pkt );
    for (prev=NULL, q = ipfw_dyn_v[i] ; q != NULL ; ) {
	if (q->dyn_type == DYN_LIMIT_PARENT)
	    goto next;
	if (TIME_LEQ( q->expire , time_second ) ) { /* expire entry */
	    UNLINK_DYN_RULE(prev, ipfw_dyn_v[i], q);
	    continue;
	}
	if ( pkt->proto == q->id.proto) {
	    if (pkt->src_ip == q->id.src_ip &&
		    pkt->dst_ip == q->id.dst_ip &&
		    pkt->src_port == q->id.src_port &&
		    pkt->dst_port == q->id.dst_port ) {
		dir = MATCH_FORWARD ;
		goto found ;
	    }
	    if (pkt->src_ip == q->id.dst_ip &&
		    pkt->dst_ip == q->id.src_ip &&
		    pkt->src_port == q->id.dst_port &&
		    pkt->dst_port == q->id.src_port ) {
		dir = 0 ; /* reverse match */
		goto found ;
	    }
	}
next:
	prev = q ;
	q = q->next ;
    }
    return NULL ; /* clearly not found */
found:
    if ( prev != NULL) { /* found and not in front */
	prev->next = q->next ;
	q->next = ipfw_dyn_v[i] ;
	ipfw_dyn_v[i] = q ;
    }
    if (pkt->proto == IPPROTO_TCP) {
	/* update state according to flags */
	u_char flags = pkt->flags & (TH_FIN|TH_SYN|TH_RST);
	q->state |= (dir == MATCH_FORWARD ) ? flags : (flags << 8);
	switch (q->state) {
	case TH_SYN :
	    /* opening */
	    q->expire = time_second + dyn_syn_lifetime ;
	    break ;
	case TH_SYN | (TH_SYN << 8) :
	    /* move to established */
	    q->expire = time_second + dyn_ack_lifetime ;
	    break ;
	case TH_SYN | (TH_SYN << 8) | TH_FIN :
	case TH_SYN | (TH_SYN << 8) | (TH_FIN << 8) :
	    /* one side tries to close */
	    q->expire = time_second + dyn_ack_lifetime ;
	    break ;
	case TH_SYN | (TH_SYN << 8) | TH_FIN | (TH_FIN << 8) :
	    /* both sides closed */
	    q->expire = time_second + dyn_fin_lifetime ;
	    break ;
	default:
#if 0
	    /*
	     * reset or some invalid combination, but can also
	     * occur if we use keep-state the wrong way.
	     */
	    if ( (q->state & ((TH_RST << 8)|TH_RST)) == 0)
		printf("invalid state: 0x%x\n", q->state);
#endif
	    q->expire = time_second + dyn_rst_lifetime ;
	    break ;
	}
    } else if (pkt->proto == IPPROTO_UDP) {
	q->expire = time_second + dyn_udp_lifetime ;
    } else {
	/* other protocols */
	q->expire = time_second + dyn_short_lifetime ;
    }
    if (match_direction)
	*match_direction = dir ;
    return q ;
}

/**
 * Install state of type 'type' for a dynamic session.
 * The hash table contains two type of rules:
 * - regular rules (DYN_KEEP_STATE)
 * - rules for sessions with limited number of sess per user
 *   (DYN_LIMIT). When they are created, the parent is
 *   increased by 1, and decreased on delete. In this case,
 *   the third parameter is the parent rule and not the chain.
 * - "parent" rules for the above (DYN_LIMIT_PARENT).
 */

static struct ipfw_dyn_rule *
add_dyn_rule(struct ipfw_flow_id *id, u_int8_t dyn_type, struct ip_fw *rule)
{
    struct ipfw_dyn_rule *r ;

    int i ;
    if (ipfw_dyn_v == NULL ||
		(dyn_count == 0 && dyn_buckets != curr_dyn_buckets)) {
	/* try reallocation, make sure we have a power of 2 */
	u_int32_t i = dyn_buckets ;
	while ( i > 0 && (i & 1) == 0 )
	    i >>= 1 ;
	if (i != 1) /* not a power of 2 */
	    dyn_buckets = curr_dyn_buckets ; /* reset */
	else {
	    curr_dyn_buckets = dyn_buckets ;
	    if (ipfw_dyn_v != NULL)
		free(ipfw_dyn_v, M_IPFW);
	    ipfw_dyn_v = malloc(curr_dyn_buckets * sizeof r,
                   M_IPFW, M_DONTWAIT | M_ZERO);
	    if (ipfw_dyn_v == NULL)
		return NULL; /* failed ! */
	}
    }
    i = hash_packet(id);

    r = malloc(sizeof *r, M_IPFW, M_DONTWAIT | M_ZERO);
    if (r == NULL) {
	printf ("sorry cannot allocate state\n");
	return NULL ;
    }

    /* increase refcount on parent, and set pointer */
    if (dyn_type == DYN_LIMIT) {
	struct ipfw_dyn_rule *parent = (struct ipfw_dyn_rule *)rule;
	if ( parent->dyn_type != DYN_LIMIT_PARENT)
	    panic("invalid parent");
	parent->count++ ;
	r->parent = parent ;
	rule = parent->rule;
    }

    r->id = *id ;
    r->expire = time_second + dyn_syn_lifetime ;
    r->rule = rule ;
    r->dyn_type = dyn_type ;
    r->pcnt = r->bcnt = 0 ;
    r->count = 0 ;

    r->bucket = i ;
    r->next = ipfw_dyn_v[i] ;
    ipfw_dyn_v[i] = r ;
    dyn_count++ ;
    DEB(printf("-- add entry 0x%08x %d -> 0x%08x %d, total %d\n",
       (r->id.src_ip), (r->id.src_port),
       (r->id.dst_ip), (r->id.dst_port),
       dyn_count ); )
    return r;
}

/**
 * lookup dynamic parent rule using pkt and rule as search keys.
 * If the lookup fails, then install one.
 */
static struct ipfw_dyn_rule *
lookup_dyn_parent(struct ipfw_flow_id *pkt, struct ip_fw *rule)
{
    struct ipfw_dyn_rule *q;
    int i;

    if (ipfw_dyn_v) {
	i = hash_packet( pkt );
	for (q = ipfw_dyn_v[i] ; q != NULL ; q=q->next)
	    if (q->dyn_type == DYN_LIMIT_PARENT && rule == q->rule &&
		    pkt->proto == q->id.proto &&
		    pkt->src_ip == q->id.src_ip &&
		    pkt->dst_ip == q->id.dst_ip &&
		    pkt->src_port == q->id.src_port &&
		    pkt->dst_port == q->id.dst_port) {
		q->expire = time_second + dyn_short_lifetime ;
		DEB(printf("lookup_dyn_parent found 0x%p\n", q);)
		return q;
	    }
    }
    return add_dyn_rule(pkt, DYN_LIMIT_PARENT, rule);
}

/*
 * Install dynamic state.
 * There are different types of dynamic rules which can be installed.
 * The type is in rule->dyn_type.
 * Type 0 (default) is a bidirectional rule
 *
 * Returns 1 (failure) if state is not installed because of errors or because
 * session limitations are enforced.
 */
static int
install_state(struct ip_fw *rule, struct ip_fw_args *args)
{
    struct ipfw_dyn_rule *q ;
    static int last_log ;

    u_int8_t type = rule->dyn_type ;

    DEB(printf("-- install state type %d 0x%08x %u -> 0x%08x %u\n",
       type,
       (args->f_id.src_ip), (args->f_id.src_port),
       (args->f_id.dst_ip), (args->f_id.dst_port) );)

    q = lookup_dyn_rule(&args->f_id, NULL) ;
    if (q != NULL) { /* should never occur */
	if (last_log != time_second) {
	    last_log = time_second ;
	    printf(" entry already present, done\n");
	}
	return 0 ;
    }
    if (dyn_count >= dyn_max) /* try remove old ones... */
	EXPIRE_DYN_CHAINS();
    if (dyn_count >= dyn_max) {
	if (last_log != time_second) {
	    last_log = time_second ;
	    printf(" Too many dynamic rules, sorry\n");
	}
	return 1; /* cannot install, notify caller */
    }

    switch (type) {
    case DYN_KEEP_STATE: /* bidir rule */
	add_dyn_rule(&args->f_id, DYN_KEEP_STATE, rule);
	break ;
    case DYN_LIMIT: /* limit number of sessions */
	{
	u_int16_t limit_mask = rule->limit_mask ;
	u_int16_t conn_limit = rule->conn_limit ;
	struct ipfw_flow_id id;
	struct ipfw_dyn_rule *parent;

	DEB(printf("installing dyn-limit rule %d\n", conn_limit);)

	id.dst_ip = id.src_ip = 0;
	id.dst_port = id.src_port = 0 ;
	id.proto = args->f_id.proto ;

	if (limit_mask & DYN_SRC_ADDR)
	    id.src_ip = args->f_id.src_ip;
	if (limit_mask & DYN_DST_ADDR)
	    id.dst_ip = args->f_id.dst_ip;
	if (limit_mask & DYN_SRC_PORT)
	    id.src_port = args->f_id.src_port;
	if (limit_mask & DYN_DST_PORT)
	    id.dst_port = args->f_id.dst_port;
	parent = lookup_dyn_parent(&id, rule);
	if (parent == NULL) {
	    printf("add parent failed\n");
	    return 1;
	}
	if (parent->count >= conn_limit) {
	    EXPIRE_DYN_CHAIN(rule); /* try to expire some */
	    /*
	     * The expiry might have removed the parent too.
	     * We lookup again, which will re-create if necessary.
	     */
	    parent = lookup_dyn_parent(&id, rule);
	    if (parent == NULL) {
		printf("add parent failed\n");
		return 1;
	    }
	    if (parent->count >= conn_limit) {
		if (fw_verbose && last_log != time_second) {
			last_log = time_second;
			log(LOG_SECURITY | LOG_DEBUG,
			    "drop session, too many entries\n");
		}
		return 1;
	    }
	}
	add_dyn_rule(&args->f_id, DYN_LIMIT, (struct ip_fw *)parent);
	}
	break ;
    default:
	printf("unknown dynamic rule type %u\n", type);
	return 1 ;
    }
    lookup_dyn_rule(&args->f_id, NULL) ; /* XXX just set the lifetime */
    return 0;
}

/*
 * given an ip_fw *, lookup_next_rule will return a pointer
 * of the same type to the next one. This can be either the jump
 * target (for skipto instructions) or the next one in the list (in
 * all other cases including a missing jump target).
 * Backward jumps are not allowed, so start looking from the next
 * rule...
 */ 
static struct ip_fw * lookup_next_rule(struct ip_fw *me);

static struct ip_fw *
lookup_next_rule(struct ip_fw *me)
{
    struct ip_fw *rule ;
    int rulenum = me->fw_skipto_rule ; /* guess... */

    if ( (me->fw_flg & IP_FW_F_COMMAND) == IP_FW_F_SKIPTO )
	for (rule = LIST_NEXT(me,next); rule ; rule = LIST_NEXT(rule,next))
	    if (rule->fw_number >= rulenum)
		return rule ;
    return LIST_NEXT(me,next) ; /* failure or not a skipto */
}

/*
 * Parameters:
 *
 *	*m	The packet; we set to NULL when/if we nuke it.
 *	oif	Outgoing interface, or NULL if packet is incoming
 *	*cookie Skip up to the first rule past this rule number;
 *		upon return, non-zero port number for divert or tee.
 *		Special case: cookie == NULL on input for bridging.
 *	*flow_id pointer to the last matching rule (in/out)
 *	*next_hop socket we are forwarding to (in/out).
 *
 * Return value:
 *
 *	IP_FW_PORT_DENY_FLAG	the packet must be dropped.
 *	0	The packet is to be accepted and routed normally OR
 *      	the packet was denied/rejected and has been dropped;
 *		in the latter case, *m is equal to NULL upon return.
 *	port	Divert the packet to port, with these caveats:
 *
 *		- If IP_FW_PORT_TEE_FLAG is set, tee the packet instead
 *		  of diverting it (ie, 'ipfw tee').
 *
 *		- If IP_FW_PORT_DYNT_FLAG is set, interpret the lower
 *		  16 bits as a dummynet pipe number instead of diverting
 */

static int 
ip_fw_chk(struct ip_fw_args *args)
{
	/*
	 * grab things into variables to minimize diffs.
	 * XXX this has to be cleaned up later.
	 */
	struct mbuf **m = &(args->m);
	struct ifnet *oif = args->oif;
	u_int16_t *cookie = &(args->divert_rule);
	struct ip_fw **flow_id = &(args->rule);
	struct sockaddr_in **next_hop = &(args->next_hop);

	struct ip_fw *f = NULL;		/* matching rule */
	struct ip *ip = mtod(*m, struct ip *);
	struct ifnet *const rif = (*m)->m_pkthdr.rcvif;
	struct ifnet *tif;
	u_int hlen = 0;

	u_short ip_off=0, offset = 0;
	/* local copy of addresses for faster matching */
	u_short src_port = 0, dst_port = 0;
	struct in_addr src_ip, dst_ip;
	u_int8_t proto= 0, flags = 0;
	u_int16_t skipto;
	u_int16_t ip_len=0;

	int dyn_checked = 0 ; /* set after dyn.rules have been checked. */
	int direction = MATCH_FORWARD ; /* dirty trick... */
	struct ipfw_dyn_rule *q = NULL ;

	/* Special hack for bridging (as usual) */
#define BRIDGED		(args->eh != NULL)
	if (BRIDGED) {	/* this is a bridged packet */
		if ( (*m)->m_pkthdr.len >= sizeof(struct ip) &&
			    ntohs(args->eh->ether_type) == ETHERTYPE_IP)
			hlen = ip->ip_hl << 2;
		else
			return 0; /* XXX ipfw1 always accepts non-ip pkts */
	} else
		hlen = ip->ip_hl << 2;

	/* Grab and reset cookie */
	skipto = *cookie;
	*cookie = 0;

#define PULLUP_TO(len)	do {						\
			    if ((*m)->m_len < (len)) {			\
				if ((*m = m_pullup(*m, (len))) == 0)	\
				    goto bogusfrag;			\
				ip = mtod(*m, struct ip *);		\
			    }						\
			} while (0)

    if (hlen > 0) { /* this is an IP packet */
	/*
	 * Collect parameters into local variables for faster matching.
	 */
	proto = ip->ip_p;
	src_ip = ip->ip_src;
	dst_ip = ip->ip_dst;
	if (BRIDGED) { /* bridged packets are as on the wire */
	    ip_off = ntohs(ip->ip_off);
	    ip_len = ntohs(ip->ip_len);
	} else {
	    ip_off = ip->ip_off;
	    ip_len = ip->ip_len;
	}
	offset = ip_off & IP_OFFMASK;
	if (offset == 0) {
	    switch (proto) {
	    case IPPROTO_TCP : {
		struct tcphdr *tcp;

		PULLUP_TO(hlen + sizeof(struct tcphdr));
		tcp =(struct tcphdr *)((u_int32_t *)ip + ip->ip_hl);
		dst_port = tcp->th_dport ;
		src_port = tcp->th_sport ;
		flags = tcp->th_flags ;
		}
		break ;

	    case IPPROTO_UDP : {
		struct udphdr *udp;

		PULLUP_TO(hlen + sizeof(struct udphdr));
		udp =(struct udphdr *)((u_int32_t *)ip + ip->ip_hl);
		dst_port = udp->uh_dport ;
		src_port = udp->uh_sport ;
		}
		break;

	    case IPPROTO_ICMP:
		PULLUP_TO(hlen + 4);	/* type, code and checksum. */
		flags = ((struct icmp *)
			((u_int32_t *)ip + ip->ip_hl))->icmp_type ;
		break ;

	    default :
		break;
	    }
	}
    }
#undef PULLUP_TO
	args->f_id.src_ip = ntohl(src_ip.s_addr);
	args->f_id.dst_ip = ntohl(dst_ip.s_addr);
	args->f_id.proto = proto;
	args->f_id.src_port = ntohs(src_port);
	args->f_id.dst_port = ntohs(dst_port);
	args->f_id.flags = flags;

	if (*flow_id) {
	    /*
	     * Packet has already been tagged. Look for the next rule
	     * to restart processing.
	     */
	    if (fw_one_pass) /* just accept if fw_one_pass is set */
		return 0;

	    f = (*flow_id)->next_rule_ptr ;
	    if (f == NULL)
		f = (*flow_id)->next_rule_ptr = lookup_next_rule(*flow_id);
	    if (f == NULL)
		goto dropit;
	} else {
	    /*
	     * Go down the list, looking for enlightment.
	     * If we've been asked to start at a given rule, do so.
	     */
	    f = LIST_FIRST(&ip_fw_chain_head);
	    if (skipto != 0) {
		if (skipto >= IPFW_DEFAULT_RULE)
		    goto dropit;
		while (f && f->fw_number <= skipto)
		    f = LIST_NEXT(f, next);
		if (f == NULL)
		    goto dropit;
	    }
	}

	for (; f; f = LIST_NEXT(f, next)) {
again:
		if (f->fw_number == IPFW_DEFAULT_RULE)
		    goto got_match ;

		/*
		 * dynamic rules are checked at the first keep-state or
		 * check-state occurrence.
		 */
		if (f->fw_flg & (IP_FW_F_KEEP_S|IP_FW_F_CHECK_S) &&
			 dyn_checked == 0 ) {
		    dyn_checked = 1 ;
		    q = lookup_dyn_rule(&args->f_id, &direction);
		    if (q != NULL) {
			DEB(printf("-- dynamic match 0x%08x %d %s 0x%08x %d\n",
			    (q->id.src_ip), (q->id.src_port),
			    (direction == MATCH_FORWARD ? "-->" : "<--"),
			    (q->id.dst_ip), (q->id.dst_port) ); )
			f = q->rule ;
			q->pcnt++ ;
			q->bcnt += ip_len;
			goto got_match ; /* random not allowed here */
		    }
		    /* if this was a check-only rule, continue with next */
		    if (f->fw_flg & IP_FW_F_CHECK_S)
			continue ;
		}

		/* Check if rule only valid for bridged packets */
		if ((f->fw_flg & IP_FW_BRIDGED) != 0 && !(BRIDGED))
			continue;

		if (oif) {
			/* Check direction outbound */
			if (!(f->fw_flg & IP_FW_F_OUT))
				continue;
		} else {
			/* Check direction inbound */
			if (!(f->fw_flg & IP_FW_F_IN))
				continue;
		}

		/* Fragments */
		if ((f->fw_flg & IP_FW_F_FRAG) && offset == 0 )
			continue;

		if (f->fw_flg & IP_FW_F_SME) {
			INADDR_TO_IFP(src_ip, tif);
			if (tif == NULL)
				continue;
		}
		if (f->fw_flg & IP_FW_F_DME) {
			INADDR_TO_IFP(dst_ip, tif);
			if (tif == NULL)
				continue;
		}
		/* If src-addr doesn't match, not this rule. */
		if (((f->fw_flg & IP_FW_F_INVSRC) != 0) ^ ((src_ip.s_addr
		    & f->fw_smsk.s_addr) != f->fw_src.s_addr))
			continue;

		/* If dest-addr doesn't match, not this rule. */
		if (((f->fw_flg & IP_FW_F_INVDST) != 0) ^ ((dst_ip.s_addr
		    & f->fw_dmsk.s_addr) != f->fw_dst.s_addr))
			continue;

		/* Interface check */
		if ((f->fw_flg & IF_FW_F_VIAHACK) == IF_FW_F_VIAHACK) {
			struct ifnet *const iface = oif ? oif : rif;

			/* Backwards compatibility hack for "via" */
			if (!iface || !iface_match(iface,
			    &f->fw_in_if, f->fw_flg & IP_FW_F_OIFNAME))
				continue;
		} else {
			/* Check receive interface */
			if ((f->fw_flg & IP_FW_F_IIFACE)
			    && (!rif || !iface_match(rif,
			      &f->fw_in_if, f->fw_flg & IP_FW_F_IIFNAME)))
				continue;
			/* Check outgoing interface */
			if ((f->fw_flg & IP_FW_F_OIFACE)
			    && (!oif || !iface_match(oif,
			      &f->fw_out_if, f->fw_flg & IP_FW_F_OIFNAME)))
				continue;
		}

		/* Check IP options */
		if (f->fw_ipopt != f->fw_ipnopt && !ipopts_match(ip, f))
			continue;

		/* Check protocol; if wildcard, and no [ug]id, match */
		if (f->fw_prot == IPPROTO_IP) {
			if (!(f->fw_flg & (IP_FW_F_UID|IP_FW_F_GID)))
				goto rnd_then_got_match;
		} else
		    /* If different, don't match */
		    if (proto != f->fw_prot) 
			    continue;

		/* Protocol specific checks for uid only */
		if (f->fw_flg & (IP_FW_F_UID|IP_FW_F_GID)) {
		    switch (proto) {
		    case IPPROTO_TCP:
			{
			    struct inpcb *P;

			    if (offset == 1)	/* cf. RFC 1858 */
				    goto bogusfrag;
			    if (offset != 0)
				    continue;

			    if (oif)
				P = in_pcblookup_hash(&tcbinfo, dst_ip,
				   dst_port, src_ip, src_port, 0,
				   oif);
			    else
				P = in_pcblookup_hash(&tcbinfo, src_ip,
				   src_port, dst_ip, dst_port, 0,
				   NULL);

			    if (P && P->inp_socket) {
				if (f->fw_flg & IP_FW_F_UID) {
					if (P->inp_socket->so_cred->cr_uid !=
					    f->fw_uid)
						continue;
				} else if (!groupmember(f->fw_gid,
					    P->inp_socket->so_cred))
						continue;
			    } else
				continue;
			    break;
			}

		    case IPPROTO_UDP:
			{
			    struct inpcb *P;

			    if (offset != 0)
				continue;

			    if (oif)
				P = in_pcblookup_hash(&udbinfo, dst_ip,
				   dst_port, src_ip, src_port, 1,
				   oif);
			    else
				P = in_pcblookup_hash(&udbinfo, src_ip,
				   src_port, dst_ip, dst_port, 1,
				   NULL);

			    if (P && P->inp_socket) {
				if (f->fw_flg & IP_FW_F_UID) {
					if (P->inp_socket->so_cred->cr_uid !=
					    f->fw_uid)
						continue;
				} else if (!groupmember(f->fw_gid,
					    P->inp_socket->so_cred))
						continue;
			    } else
				continue;
			    break;
			}

		    default:
			    continue;
		    }
		}
		    
		/* Protocol specific checks */
		switch (proto) {
		case IPPROTO_TCP:
		    {
			struct tcphdr *tcp;

			if (offset == 1)	/* cf. RFC 1858 */
				goto bogusfrag;
			if (offset != 0) {
				/*
				 * TCP flags and ports aren't available in this
				 * packet -- if this rule specified either one,
				 * we consider the rule a non-match.
				 */
				if (IP_FW_HAVEPORTS(f) != 0 ||
				    f->fw_tcpopt != f->fw_tcpnopt ||
				    f->fw_tcpf != f->fw_tcpnf)
					continue;

				break;
			}
			tcp = (struct tcphdr *) ((u_int32_t *)ip + ip->ip_hl);

			if (f->fw_tcpopt != f->fw_tcpnopt && !tcpopts_match(tcp, f))
				continue;
			if (((f->fw_tcpf != f->fw_tcpnf) ||
			    (f->fw_ipflg & IP_FW_IF_TCPEST)) &&
			    !tcpflg_match(tcp, f))
				continue;
			goto check_ports;
		    }

		case IPPROTO_UDP:
			if (offset != 0) {
				/*
				 * Port specification is unavailable -- if this
				 * rule specifies a port, we consider the rule
				 * a non-match.
				 */
				if (IP_FW_HAVEPORTS(f) )
					continue;

				break;
			}
check_ports:
			if (!port_match(&f->fw_uar.fw_pts[0],
			    IP_FW_GETNSRCP(f), ntohs(src_port),
			    f->fw_flg & IP_FW_F_SRNG,
			    f->fw_flg & IP_FW_F_SMSK))
				continue;
			if (!port_match(&f->fw_uar.fw_pts[IP_FW_GETNSRCP(f)],
			    IP_FW_GETNDSTP(f), ntohs(dst_port),
			    f->fw_flg & IP_FW_F_DRNG,
			    f->fw_flg & IP_FW_F_DMSK)) 
				continue;
			break;

		case IPPROTO_ICMP:
		    {
			struct icmp *icmp;

			if (offset != 0)	/* Type isn't valid */
				break;
			icmp = (struct icmp *) ((u_int32_t *)ip + ip->ip_hl);
			if (!icmptype_match(icmp, f))
				continue;
			break;
		    }

		default:
			break;

bogusfrag:
		if (fw_verbose) {
			if (m != NULL)
				ipfw_report(NULL, ip, ip_off, ip_len, rif, oif);
			else
				printf("pullup failed\n");
		}
		goto dropit;

		}

rnd_then_got_match:
		if ( f->dont_match_prob && random() < f->dont_match_prob )
			continue ;
got_match:
		/*
		 * If not a dynamic match (q == NULL) and keep-state, install
		 * a new dynamic entry.
		 */
		if (q == NULL && f->fw_flg & IP_FW_F_KEEP_S) {
		    if (install_state(f, args)) /* error or limit violation */
			goto dropit;
		}
		/* Update statistics */
		f->fw_pcnt += 1;
		f->fw_bcnt += ip_len;
		f->timestamp = time_second;

		/* Log to console if desired */
		if ((f->fw_flg & IP_FW_F_PRN) && fw_verbose && hlen > 0)
			ipfw_report(f, ip, offset, ip_len, rif, oif);

		/* Take appropriate action */
		switch (f->fw_flg & IP_FW_F_COMMAND) {
		case IP_FW_F_ACCEPT:
			return(0);
		case IP_FW_F_COUNT:
			continue;
#ifdef IPDIVERT
		case IP_FW_F_DIVERT:
			*cookie = f->fw_number;
			return(f->fw_divert_port);
		case IP_FW_F_TEE:
			*cookie = f->fw_number;
			return(f->fw_divert_port | IP_FW_PORT_TEE_FLAG);
#endif
		case IP_FW_F_SKIPTO: /* XXX check */
			if (f->next_rule_ptr == NULL)
			    f->next_rule_ptr = lookup_next_rule(f) ;
			f = f->next_rule_ptr;
			if (!f)
			    goto dropit;
			goto again ;

		case IP_FW_F_PIPE:
		case IP_FW_F_QUEUE:
			*flow_id = f ; /* XXX set flow id */
			return(f->fw_pipe_nr | IP_FW_PORT_DYNT_FLAG);

		case IP_FW_F_FWD:
			/* Change the next-hop address for this packet.
			 * Initially we'll only worry about directly
			 * reachable next-hop's, but ultimately
			 * we will work out for next-hops that aren't
			 * direct the route we would take for it. We
			 * [cs]ould leave this latter problem to
			 * ip_output.c. We hope to high [name the abode of
			 * your favourite deity] that ip_output doesn't modify
			 * the new value of next_hop (which is dst there)
			 * XXX warning-- there is a dangerous reference here
			 * from next_hop to a field within the rule. If the
			 * rule is deleted, weird things might occur.
			 */
			if (next_hop != NULL /* Make sure, first... */
			    && (q == NULL || direction == MATCH_FORWARD) )
				*next_hop = &(f->fw_fwd_ip);
			return(0); /* Allow the packet */

		}

		/* Deny/reject this packet using this rule */
		break;
	}

	/* Rule IPFW_DEFAULT_RULE should always be there and match */
	KASSERT(f != NULL, ("ip_fw: no chain"));

	/*
	 * At this point, we're going to drop the packet.
	 * Send a reject notice if all of the following are true:
	 *
	 * - The packet matched a reject rule
	 * - The packet is not an ICMP packet, or is an ICMP query packet
	 * - The packet is not a multicast or broadcast packet
	 */
	if ((f->fw_flg & IP_FW_F_COMMAND) == IP_FW_F_REJECT
	    && (proto != IPPROTO_ICMP || is_icmp_query(ip))
	    && !((*m)->m_flags & (M_BCAST|M_MCAST))
	    && !IN_MULTICAST(ntohl(ip->ip_dst.s_addr))) {
		/* Must convert to host order for icmp_error() etc. */
		if (BRIDGED) {
			ip->ip_len = ntohs(ip->ip_len);
			ip->ip_off = ntohs(ip->ip_off);
		}
		switch (f->fw_reject_code) {
		case IP_FW_REJECT_RST:
		  {
			/* XXX warning, this code writes into the mbuf */
			struct tcphdr *const tcp =
				(struct tcphdr *) ((u_int32_t *)ip + ip->ip_hl);
			struct tcpiphdr ti, *const tip = (struct tcpiphdr *) ip;

			if (offset != 0 || (tcp->th_flags & TH_RST))
				break;
			ti.ti_i = *((struct ipovly *) ip);
			ti.ti_t = *tcp;
			bcopy(&ti, ip, sizeof(ti));
			NTOHL(tip->ti_seq);
			NTOHL(tip->ti_ack);
			tip->ti_len = ip_len - hlen - (tip->ti_off << 2);
			if (tcp->th_flags & TH_ACK) {
				tcp_respond(NULL, (void *)ip, tcp, *m,
				    (tcp_seq)0, tcp->th_ack, TH_RST);
			} else {
				if (tcp->th_flags & TH_SYN)
					tip->ti_len++;
				tcp_respond(NULL, (void *)ip, tcp, *m, 
				    tip->ti_seq + tip->ti_len,
				    (tcp_seq)0, TH_RST|TH_ACK);
			}
			*m = NULL;
			break;
		  }
		default:	/* Send an ICMP unreachable using code */
			icmp_error(*m, ICMP_UNREACH,
			    f->fw_reject_code, 0L, 0);
			*m = NULL;
			break;
		}
	}

dropit:
	/*
	 * Finally, drop the packet.
	 */
	return(IP_FW_PORT_DENY_FLAG);
#undef BRIDGED
}

/*
 * when a rule is added/deleted, zero the direct pointers within
 * all firewall rules. These will be reconstructed on the fly
 * as packets are matched.
 * Must be called at splimp().
 */
static void
flush_rule_ptrs()
{
    struct ip_fw *fcp ;

    LIST_FOREACH(fcp, &ip_fw_chain_head, next) {
	fcp->next_rule_ptr = NULL ;
    }
}

void
flush_pipe_ptrs(struct dn_flow_set *match)
{
    struct ip_fw *fcp ;

    LIST_FOREACH(fcp, &ip_fw_chain_head, next) {
	if (match == NULL || fcp->pipe_ptr == match)
		fcp->pipe_ptr = NULL;
    }
}

static int
add_entry(struct ip_fw_head *head, struct ip_fw *rule)
{
	struct ip_fw *ftmp, *fcp, *fcpl;
	u_short nbr = 0;
	int s;

	ftmp = malloc(sizeof *ftmp, M_IPFW, M_DONTWAIT | M_ZERO);
	if (!ftmp)
		return (ENOSPC);
	bcopy(rule, ftmp, sizeof(*ftmp));

	ftmp->fw_in_if.fu_via_if.name[FW_IFNLEN - 1] = '\0';
	ftmp->fw_pcnt = 0L;
	ftmp->fw_bcnt = 0L;
	ftmp->next_rule_ptr = NULL ;
	ftmp->pipe_ptr = NULL ;
	
	s = splimp();

	if (LIST_FIRST(head) == 0) {
		LIST_INSERT_HEAD(head, ftmp, next);
		goto done;
        }

	/* If entry number is 0, find highest numbered rule and add 100 */
	if (ftmp->fw_number == 0) {
		LIST_FOREACH(fcp, head, next) {
			if (fcp->fw_number != IPFW_DEFAULT_RULE)
				nbr = fcp->fw_number;
			else
				break;
		}
		if (nbr < IPFW_DEFAULT_RULE - 100)
			nbr += 100;
		ftmp->fw_number = rule->fw_number = nbr;
	}

	/* Got a valid number; now insert it, keeping the list ordered */
	fcpl = NULL ;
	LIST_FOREACH(fcp, head, next) {
		if (fcp->fw_number > ftmp->fw_number) {
			if (fcpl) {
				LIST_INSERT_AFTER(fcpl, ftmp, next);
			} else {
				LIST_INSERT_HEAD(head, ftmp, next);
			}
			break;
		} else {
			fcpl = fcp;
		}
	}
	flush_rule_ptrs();
done:
	static_count++;
	splx(s);
	DEB(printf("++ installed rule %d, static count now %d\n",
		ftmp->fw_number, static_count);)
	return (0);
}

/**
 * free storage associated with a static rule entry (including
 * dependent dynamic rules), and zeroes rule pointers to avoid
 * dangling pointer dereferences.
 * @return a pointer to the next entry.
 * Must be called at splimp() and with a non-null argument.
 */
static struct ip_fw *
free_chain(struct ip_fw *fcp)
{
    struct ip_fw *n;

    n = LIST_NEXT(fcp, next);
    DELETE_DYN_CHAIN(fcp);
    LIST_REMOVE(fcp, next);
    static_count--;
    if (DUMMYNET_LOADED)
	ip_dn_ruledel_ptr(fcp) ;
    flush_rule_ptrs(); /* more efficient to do outside the loop */
    free(fcp, M_IPFW);
    return n;
}

/**
 * remove all rules with given number.
 */
static int
del_entry(struct ip_fw_head *chainptr, u_short number)
{
    struct ip_fw *rule;

    if (number != IPFW_DEFAULT_RULE) {
	LIST_FOREACH(rule, chainptr, next) {
	    if (rule->fw_number == number) {
		int s ;

		s = splimp(); /* prevent access to rules while removing */
		while (rule && rule->fw_number == number)
		    rule = free_chain(rule);
		/* XXX could move flush_rule_ptrs() here */
		splx(s);
		return 0 ;
	    }
	}
    }
    return (EINVAL);
}

/**
 * Reset some or all counters on firewall rules.
 * @arg frwl is null to clear all entries, or contains a specific
 * rule number.
 * @arg log_only is 1 if we only want to reset logs, zero otherwise.
 */

static int
zero_entry(struct ip_fw *frwl, int log_only)
{
    struct ip_fw *rule;
    int s;
    u_short number = 0 ;
    char *msg ;

    if (frwl == 0) {
	s = splimp();
	LIST_FOREACH(rule, &ip_fw_chain_head, next) {
	    if (log_only == 0) {
		rule->fw_bcnt = rule->fw_pcnt = 0;
		rule->timestamp = 0;
	    }
	    rule->fw_loghighest = rule->fw_pcnt+rule->fw_logamount;
	}
	splx(s);
	msg = log_only ? "ipfw: All logging counts cleared.\n" :
			"ipfw: Accounting cleared.\n";
    } else {
	int cleared = 0;
	number = frwl->fw_number ;
	/*
	 * It is possible to insert multiple chain entries with the
	 * same number, so we don't stop after finding the first
	 * match if zeroing a specific entry.
	 */
	LIST_FOREACH(rule, &ip_fw_chain_head, next)
	    if (number == rule->fw_number) {
		s = splimp();
		while (rule && number == rule->fw_number) {
		    if (log_only == 0) {
			rule->fw_bcnt = rule->fw_pcnt = 0;
			rule->timestamp = 0;
		    }
		    rule->fw_loghighest = rule->fw_pcnt+ rule->fw_logamount;
		    rule = LIST_NEXT(rule, next);
		}
		splx(s);
		cleared = 1;
		break;
	    }
	if (!cleared)	/* we did not find any matching rules */
	    return (EINVAL);
	msg = log_only ? "ipfw: Entry %d logging count reset.\n" :
			"ipfw: Entry %d cleared.\n";
    }
    if (fw_verbose)
	log(LOG_SECURITY | LOG_NOTICE, msg, number);
    return (0);
}

static int
check_ipfw_struct(struct ip_fw *frwl)
{
	/* Check for invalid flag bits */
	if ((frwl->fw_flg & ~IP_FW_F_MASK) != 0) {
		dprintf(("%s undefined flag bits set (flags=%x)\n",
		    err_prefix, frwl->fw_flg));
		return (EINVAL);
	}
	if (frwl->fw_flg == IP_FW_F_CHECK_S) {
		/* check-state */
		return 0 ;
	}
	/* Must apply to incoming or outgoing (or both) */
	if (!(frwl->fw_flg & (IP_FW_F_IN | IP_FW_F_OUT))) {
		dprintf(("%s neither in nor out\n", err_prefix));
		return (EINVAL);
	}
	/* Empty interface name is no good */
	if (((frwl->fw_flg & IP_FW_F_IIFNAME)
	      && !*frwl->fw_in_if.fu_via_if.name)
	    || ((frwl->fw_flg & IP_FW_F_OIFNAME)
	      && !*frwl->fw_out_if.fu_via_if.name)) {
		dprintf(("%s empty interface name\n", err_prefix));
		return (EINVAL);
	}
	/* Sanity check interface matching */
	if ((frwl->fw_flg & IF_FW_F_VIAHACK) == IF_FW_F_VIAHACK) {
		;		/* allow "via" backwards compatibility */
	} else if ((frwl->fw_flg & IP_FW_F_IN)
	    && (frwl->fw_flg & IP_FW_F_OIFACE)) {
		dprintf(("%s outgoing interface check on incoming\n",
		    err_prefix));
		return (EINVAL);
	}
	/* Sanity check port ranges */
	if ((frwl->fw_flg & IP_FW_F_SRNG) && IP_FW_GETNSRCP(frwl) < 2) {
		dprintf(("%s src range set but n_src_p=%d\n",
		    err_prefix, IP_FW_GETNSRCP(frwl)));
		return (EINVAL);
	}
	if ((frwl->fw_flg & IP_FW_F_DRNG) && IP_FW_GETNDSTP(frwl) < 2) {
		dprintf(("%s dst range set but n_dst_p=%d\n",
		    err_prefix, IP_FW_GETNDSTP(frwl)));
		return (EINVAL);
	}
	if (IP_FW_GETNSRCP(frwl) + IP_FW_GETNDSTP(frwl) > IP_FW_MAX_PORTS) {
		dprintf(("%s too many ports (%d+%d)\n",
		    err_prefix, IP_FW_GETNSRCP(frwl), IP_FW_GETNDSTP(frwl)));
		return (EINVAL);
	}
	/*
	 *	Protocols other than TCP/UDP don't use port range
	 */
	if ((frwl->fw_prot != IPPROTO_TCP) &&
	    (frwl->fw_prot != IPPROTO_UDP) &&
	    (IP_FW_GETNSRCP(frwl) || IP_FW_GETNDSTP(frwl))) {
		dprintf(("%s port(s) specified for non TCP/UDP rule\n",
		    err_prefix));
		return (EINVAL);
	}

	/*
	 *	Rather than modify the entry to make such entries work, 
	 *	we reject this rule and require user level utilities
	 *	to enforce whatever policy they deem appropriate.
	 */
	if ((frwl->fw_src.s_addr & (~frwl->fw_smsk.s_addr)) || 
		(frwl->fw_dst.s_addr & (~frwl->fw_dmsk.s_addr))) {
		dprintf(("%s rule never matches\n", err_prefix));
		return (EINVAL);
	}

	if ((frwl->fw_flg & IP_FW_F_FRAG) &&
		(frwl->fw_prot == IPPROTO_UDP || frwl->fw_prot == IPPROTO_TCP)) {
		if (IP_FW_HAVEPORTS(frwl)) {
			dprintf(("%s cannot mix 'frag' and ports\n", err_prefix));
			return (EINVAL);
		}
		if (frwl->fw_prot == IPPROTO_TCP &&
			frwl->fw_tcpf != frwl->fw_tcpnf) {
			dprintf(("%s cannot mix 'frag' and TCP flags\n", err_prefix));
			return (EINVAL);
		}
	}

	/* Check command specific stuff */
	switch (frwl->fw_flg & IP_FW_F_COMMAND) {
	case IP_FW_F_REJECT:
		if (frwl->fw_reject_code >= 0x100
		    && !(frwl->fw_prot == IPPROTO_TCP
		      && frwl->fw_reject_code == IP_FW_REJECT_RST)) {
			dprintf(("%s unknown reject code\n", err_prefix));
			return (EINVAL);
		}
		break;
#ifdef IPDIVERT
	case IP_FW_F_DIVERT:		/* Diverting to port zero is invalid */
	case IP_FW_F_TEE:
#endif
	case IP_FW_F_PIPE:              /* pipe 0 is invalid */
	case IP_FW_F_QUEUE:             /* queue 0 is invalid */
		if (frwl->fw_divert_port == 0) {
			dprintf(("%s 0 is an invalid argument\n", err_prefix));
			return (EINVAL);
		}
		break;
	case IP_FW_F_DENY:
	case IP_FW_F_ACCEPT:
	case IP_FW_F_COUNT:
	case IP_FW_F_SKIPTO:
	case IP_FW_F_FWD:
	case IP_FW_F_UID:
	case IP_FW_F_GID:
		break;
	default:
		dprintf(("%s invalid command\n", err_prefix));
		return (EINVAL);
	}

	return 0;
}

static int
ip_fw_ctl(struct sockopt *sopt)
{
	int error, s;
	size_t size;
	struct ip_fw *fcp;
	struct ip_fw frwl, *bp , *buf;

	/*
	 * Disallow modifications in really-really secure mode, but still allow
	 * the logging counters to be reset.
	 */
	if (securelevel >= 3 && (sopt->sopt_name == IP_FW_ADD ||
	    (sopt->sopt_dir == SOPT_SET && sopt->sopt_name != IP_FW_RESETLOG)))
			return (EPERM);
	error = 0;

	switch (sopt->sopt_name) {
	case IP_FW_GET:
		/*
		 * pass up a copy of the current rules. Static rules
		 * come first (the last of which has number 65535),
		 * followed by a possibly empty list of dynamic rule.
		 * The last dynamic rule has NULL in the "next" field.
		 */
		s = splimp();
		/* size of static rules */
		size = static_count * sizeof(struct ip_fw) ;
		if (ipfw_dyn_v)		/* add size of dyn.rules */
		    size += (dyn_count * sizeof(struct ipfw_dyn_rule));

		/*
		 * XXX todo: if the user passes a short length to know how
		 * much room is needed, do not
		 * bother filling up the buffer, just jump to the
		 * sooptcopyout.
		 */
		buf = malloc(size, M_TEMP, M_WAITOK);
		if (buf == 0) {
		    splx(s);
		    error = ENOBUFS;
		    break;
		}

		bp = buf ;
		LIST_FOREACH(fcp, &ip_fw_chain_head, next) {
		    bcopy(fcp, bp, sizeof *fcp);
		    bp++;
		}
		if (ipfw_dyn_v) {
		    int i ;
		    struct ipfw_dyn_rule *p, *dst, *last = NULL ;

		    dst = (struct ipfw_dyn_rule *)bp ;
		    for (i = 0 ; i < curr_dyn_buckets ; i++ )
			for ( p = ipfw_dyn_v[i] ; p != NULL ; p = p->next, dst++ ) {
			    bcopy(p, dst, sizeof *p);
                            (int)dst->rule = p->rule->fw_number ;
			    /*
			     * store a non-null value in "next". The userland
			     * code will interpret a NULL here as a marker
			     * for the last dynamic rule.
			     */
                            dst->next = dst ;
			    last = dst ;
			    if (TIME_LEQ(dst->expire, time_second) )
				dst->expire = 0 ;
			    else
				dst->expire -= time_second ;
			    }
		    if (last != NULL)
			last->next = NULL ; /* mark last dynamic rule */
		}
		splx(s);

		error = sooptcopyout(sopt, buf, size);
		free(buf, M_TEMP);
		break;

	case IP_FW_FLUSH:
		/*
		 * Normally we cannot release the lock on each iteration.
		 * We could do it here only because we start from the head all
		 * the times so there is no risk of missing some entries.
		 * On the other hand, the risk is that we end up with
		 * a very inconsistent ruleset, so better keep the lock
		 * around the whole cycle.
		 * 
		 * XXX this code can be improved by resetting the head of
		 * the list to point to the default rule, and then freeing
		 * the old list without the need for a lock.
		 */

		s = splimp();
		while ( (fcp = LIST_FIRST(&ip_fw_chain_head)) &&
			fcp->fw_number != IPFW_DEFAULT_RULE )
		    free_chain(fcp);
		splx(s);
		break;

	case IP_FW_ADD:
		error = sooptcopyin(sopt, &frwl, sizeof frwl, sizeof frwl);
		if (error || (error = check_ipfw_struct(&frwl)))
			break;

		if (frwl.fw_number == IPFW_DEFAULT_RULE) {
			dprintf(("%s can't add rule %u\n", err_prefix,
				 (unsigned)IPFW_DEFAULT_RULE));
			error = EINVAL;
		} else {
			error = add_entry(&ip_fw_chain_head, &frwl);
			if (!error && sopt->sopt_dir == SOPT_GET)
				error = sooptcopyout(sopt, &frwl, sizeof frwl);
		}
		break;

	case IP_FW_DEL:
		error = sooptcopyin(sopt, &frwl, sizeof frwl, sizeof frwl);
		if (error)
			break;

		if (frwl.fw_number == IPFW_DEFAULT_RULE) {
			dprintf(("%s can't delete rule %u\n", err_prefix,
				 (unsigned)IPFW_DEFAULT_RULE));
			error = EINVAL;
		} else {
			error = del_entry(&ip_fw_chain_head, frwl.fw_number);
		}
		break;

	case IP_FW_ZERO:
	case IP_FW_RESETLOG:
	    {
		int cmd = (sopt->sopt_name == IP_FW_RESETLOG );
		void *arg = NULL ;

		if (sopt->sopt_val != 0) {
		    error = sooptcopyin(sopt, &frwl, sizeof frwl, sizeof frwl);
		    if (error)
			break;
		    arg = &frwl ;
		}
		error = zero_entry(arg, cmd);
	    }
	    break;

	default:
		printf("ip_fw_ctl invalid option %d\n", sopt->sopt_name);
		error = EINVAL ;
	}

	return (error);
}

/**
 * dummynet needs a reference to the default rule, because rules can
 * be deleted while packets hold a reference to them (e.g. to resume
 * processing at the next rule). When this happens, dummynet changes
 * the reference to the default rule (probably it could well be a
 * NULL pointer, but this way we do not need to check for the special
 * case, plus here he have info on the default behaviour.
 */
struct ip_fw *ip_fw_default_rule ;

void
ip_fw_init(void)
{
	struct ip_fw default_rule;

	ip_fw_chk_ptr = ip_fw_chk;
	ip_fw_ctl_ptr = ip_fw_ctl;
	LIST_INIT(&ip_fw_chain_head);

	bzero(&default_rule, sizeof default_rule);
	default_rule.fw_prot = IPPROTO_IP;
	default_rule.fw_number = IPFW_DEFAULT_RULE;
#ifdef IPFIREWALL_DEFAULT_TO_ACCEPT
	default_rule.fw_flg |= IP_FW_F_ACCEPT;
#else
	default_rule.fw_flg |= IP_FW_F_DENY;
#endif
	default_rule.fw_flg |= IP_FW_F_IN | IP_FW_F_OUT;
	if (check_ipfw_struct(&default_rule) != 0 ||
	    add_entry(&ip_fw_chain_head, &default_rule))
		panic("ip_fw_init");

	ip_fw_default_rule = LIST_FIRST(&ip_fw_chain_head) ;
	printf("IP packet filtering initialized, "
#ifdef IPDIVERT
		"divert enabled, "
#else
		"divert disabled, "
#endif
		"rule-based forwarding enabled, "
#ifdef IPFIREWALL_DEFAULT_TO_ACCEPT
		"default to accept, ");
#else
		"default to deny, " );
#endif
#ifndef IPFIREWALL_VERBOSE
	printf("logging disabled\n");
#else
	if (fw_verbose_limit == 0)
		printf("unlimited logging\n");
	else
		printf("logging limited to %d packets/entry by default\n",
		    fw_verbose_limit);
#endif
}

static int
ipfw_modevent(module_t mod, int type, void *unused)
{
	int s;
	int err = 0 ;
#if defined(KLD_MODULE)
	struct ip_fw *fcp;
#endif
	
	switch (type) {
	case MOD_LOAD:
		s = splimp();
		if (IPFW_LOADED) {
			splx(s);
			printf("IP firewall already loaded\n");
			err = EEXIST ;
		} else {
			ip_fw_init();
			splx(s);
		}
		break ;
	case MOD_UNLOAD:
#if !defined(KLD_MODULE)
		printf("ipfw statically compiled, cannot unload\n");
		err = EBUSY;
#else
		s = splimp();
		ip_fw_chk_ptr = NULL ;
		ip_fw_ctl_ptr = NULL ;
		while ( (fcp = LIST_FIRST(&ip_fw_chain_head)) != NULL)
			free_chain(fcp);
		splx(s);
		printf("IP firewall unloaded\n");
#endif
		break ;

	default:
		break;
	}
	return err;
}

static moduledata_t ipfwmod = {
	"ipfw",
	ipfw_modevent,
	0
};
DECLARE_MODULE(ipfw, ipfwmod, SI_SUB_PSEUDO, SI_ORDER_ANY);
#endif /* !IPFW2 */
