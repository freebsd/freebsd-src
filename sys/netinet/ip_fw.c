/*
 * Copyright (c) 1993 Daniel Boulet
 * Copyright (c) 1994 Ugen J.S.Antsilevich
 * Copyright (c) 1996 Alex Nash
 * Copyright (c) 2000 Luigi Rizzo
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

#define STATEFUL       1
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
#include <netinet/in_pcb.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip_fw.h>
#ifdef DUMMYNET
#include <netinet/ip_dummynet.h>
#endif
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
int fw_one_pass = 1 ;
#ifdef IPFIREWALL_VERBOSE_LIMIT
static int fw_verbose_limit = IPFIREWALL_VERBOSE_LIMIT;
#else
static int fw_verbose_limit = 0;
#endif

static u_int64_t counter;	/* counter for ipfw_report(NULL...) */
struct ipfw_flow_id last_pkt ;

#define	IPFW_DEFAULT_RULE	((u_int)(u_short)~0)

LIST_HEAD (ip_fw_head, ip_fw_chain) ip_fw_chain;

MALLOC_DEFINE(M_IPFW, "IpFw/IpAcct", "IpFw/IpAcct chain's");

#ifdef SYSCTL_NODE
SYSCTL_DECL(_net_inet_ip);
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

#if STATEFUL
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
static u_int32_t dyn_ack_lifetime = 300 ;
static u_int32_t dyn_syn_lifetime = 20 ;
static u_int32_t dyn_fin_lifetime = 20 ;
static u_int32_t dyn_rst_lifetime = 5 ;
static u_int32_t dyn_short_lifetime = 30 ;
static u_int32_t dyn_count = 0 ;
static u_int32_t dyn_max = 1000 ;
SYSCTL_INT(_net_inet_ip_fw, OID_AUTO, dyn_buckets, CTLFLAG_RW,
    &dyn_buckets, 0, "Number of dyn. buckets");
SYSCTL_INT(_net_inet_ip_fw, OID_AUTO, curr_dyn_buckets, CTLFLAG_RD,
    &curr_dyn_buckets, 0, "Current Number of dyn. buckets");
SYSCTL_INT(_net_inet_ip_fw, OID_AUTO, dyn_count, CTLFLAG_RD,
    &dyn_count, 0, "Number of dyn. rules");
SYSCTL_INT(_net_inet_ip_fw, OID_AUTO, dyn_max, CTLFLAG_RW,
    &dyn_max, 0, "Max number of dyn. rules");
SYSCTL_INT(_net_inet_ip_fw, OID_AUTO, dyn_ack_lifetime, CTLFLAG_RW,
    &dyn_ack_lifetime, 0, "Lifetime of dyn. rules for acks");
SYSCTL_INT(_net_inet_ip_fw, OID_AUTO, dyn_syn_lifetime, CTLFLAG_RW,
    &dyn_syn_lifetime, 0, "Lifetime of dyn. rules for syn");
SYSCTL_INT(_net_inet_ip_fw, OID_AUTO, dyn_fin_lifetime, CTLFLAG_RW,
    &dyn_fin_lifetime, 0, "Lifetime of dyn. rules for fin");
SYSCTL_INT(_net_inet_ip_fw, OID_AUTO, dyn_rst_lifetime, CTLFLAG_RW,
    &dyn_rst_lifetime, 0, "Lifetime of dyn. rules for rst");
SYSCTL_INT(_net_inet_ip_fw, OID_AUTO, dyn_short_lifetime, CTLFLAG_RW,
    &dyn_short_lifetime, 0, "Lifetime of dyn. rules for other situations");
#endif /* STATEFUL */

#endif

#define dprintf(a)	do {						\
				if (fw_debug)				\
					printf a;			\
			} while (0)
#define SNPARGS(buf, len) buf + len, sizeof(buf) > len ? sizeof(buf) - len : 0

static int	add_entry __P((struct ip_fw_head *chainptr, struct ip_fw *frwl));
static int	del_entry __P((struct ip_fw_head *chainptr, u_short number));
static int	zero_entry __P((struct ip_fw *));
static int	resetlog_entry __P((struct ip_fw *));
static int	check_ipfw_struct __P((struct ip_fw *m));
static __inline int
		iface_match __P((struct ifnet *ifp, union ip_fw_if *ifu,
				 int byname));
static int	ipopts_match __P((struct ip *ip, struct ip_fw *f));
static __inline int
		port_match __P((u_short *portptr, int nports, u_short port,
				int range_flag, int mask));
static int	tcpflg_match __P((struct tcphdr *tcp, struct ip_fw *f));
static int	icmptype_match __P((struct icmp *  icmp, struct ip_fw * f));
static void	ipfw_report __P((struct ip_fw *f, struct ip *ip,
				struct ifnet *rif, struct ifnet *oif));

static void flush_rule_ptrs(void);

static int	ip_fw_chk __P((struct ip **pip, int hlen,
			struct ifnet *oif, u_int16_t *cookie, struct mbuf **m,
			struct ip_fw_chain **flow_id,
			struct sockaddr_in **next_hop));
static int	ip_fw_ctl __P((struct sockopt *sopt));

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
		if (portptr[0] <= port && port <= portptr[1]) {
			return 1;
		}
		nports -= 2;
		portptr += 2;
	}
	while (nports-- > 0) {
		if (*portptr++ == port) {
			return 1;
		}
	}
	return 0;
}

static int
tcpflg_match(struct tcphdr *tcp, struct ip_fw *f)
{
	u_char		flg_set, flg_clr;
	
	if ((f->fw_tcpf & IP_FW_TCPF_ESTAB) &&
	    (tcp->th_flags & (IP_FW_TCPF_RST | IP_FW_TCPF_ACK)))
		return 1;

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

static __inline int
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

		for (ia = ifp->if_addrhead.tqh_first;
		    ia != NULL; ia = ia->ifa_link.tqe_next) {
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
ipfw_report(struct ip_fw *f, struct ip *ip,
	struct ifnet *rif, struct ifnet *oif)
{
    struct tcphdr *const tcp = (struct tcphdr *) ((u_int32_t *) ip+ ip->ip_hl);
    struct udphdr *const udp = (struct udphdr *) ((u_int32_t *) ip+ ip->ip_hl);
    struct icmp *const icmp = (struct icmp *) ((u_int32_t *) ip + ip->ip_hl);
    u_int64_t count;
    char *action;
    char action2[32], proto[47], name[18], fragment[17];
    int len;

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
#ifdef DUMMYNET
	    case IP_FW_F_PIPE:
		    snprintf(SNPARGS(action2, 0), "Pipe %d",
			f->fw_skipto_rule);
		    break;
	    case IP_FW_F_QUEUE:
		    snprintf(SNPARGS(action2, 0), "Queue %d",
			f->fw_skipto_rule);
		    break;
#endif
#ifdef IPFIREWALL_FORWARD
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
#endif
	    default:	
		    action = "UNKNOWN";
		    break;
	    }
    }

    switch (ip->ip_p) {
    case IPPROTO_TCP:
	    len = snprintf(SNPARGS(proto, 0), "TCP %s",
		inet_ntoa(ip->ip_src));
	    if ((ip->ip_off & IP_OFFMASK) == 0)
		    len += snprintf(SNPARGS(proto, len), ":%d ",
			ntohs(tcp->th_sport));
	    else
		    len += snprintf(SNPARGS(proto, len), " ");
	    len += snprintf(SNPARGS(proto, len), "%s",
		inet_ntoa(ip->ip_dst));
	    if ((ip->ip_off & IP_OFFMASK) == 0)
		    snprintf(SNPARGS(proto, len), ":%d",
			ntohs(tcp->th_dport));
	    break;
    case IPPROTO_UDP:
	    len = snprintf(SNPARGS(proto, 0), "UDP %s",
		inet_ntoa(ip->ip_src));
	    if ((ip->ip_off & IP_OFFMASK) == 0)
		    len += snprintf(SNPARGS(proto, len), ":%d ",
			ntohs(udp->uh_sport));
	    else
		    len += snprintf(SNPARGS(proto, len), " ");
	    len += snprintf(SNPARGS(proto, len), "%s",
		inet_ntoa(ip->ip_dst));
	    if ((ip->ip_off & IP_OFFMASK) == 0)
		    snprintf(SNPARGS(proto, len), ":%d",
			ntohs(udp->uh_dport));
	    break;
    case IPPROTO_ICMP:
	    if ((ip->ip_off & IP_OFFMASK) == 0)
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

    if ((ip->ip_off & IP_OFFMASK))
	    snprintf(SNPARGS(fragment, 0), " Fragment = %d",
		ip->ip_off & IP_OFFMASK);
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

#if STATEFUL
static __inline int
hash_packet(struct ipfw_flow_id *id)
{
    u_int32_t i ;

    i = (id->dst_ip) ^ (id->src_ip) ^ (id->dst_port) ^ (id->src_port);
    i &= (curr_dyn_buckets - 1) ;
    return i ;
}

#define TIME_LEQ(a,b)       ((int)((a)-(b)) <= 0)
/*
 * Remove all dynamic rules pointing to a given chain, or all
 * rules if chain == NULL. Second parameter is 1 if we want to
 * delete unconditionally, otherwise only expired rules are removed.
 */
static void
remove_dyn_rule(struct ip_fw_chain *chain, int force)
{
    struct ipfw_dyn_rule *prev, *q, *old_q ;
    int i ;
    static u_int32_t last_remove = 0 ;

    if (ipfw_dyn_v == NULL || dyn_count == 0)
	return ;
    /* do not expire more than once per second, it is useless */
    if (force == 0 && last_remove == time_second)
	return ;
    last_remove = time_second ;

    for (i = 0 ; i < curr_dyn_buckets ; i++) {
	for (prev=NULL, q = ipfw_dyn_v[i] ; q ; ) {
	    if ( (chain == NULL || chain == q->chain) &&
		 (force || TIME_LEQ( q->expire , time_second ) ) ) {
		DEB(printf("-- remove entry 0x%08x %d -> 0x%08x %d, %d left\n",
		    (q->id.src_ip), (q->id.src_port),
		    (q->id.dst_ip), (q->id.dst_port), dyn_count-1 ); )
		old_q = q ;
		if (prev != NULL)
		    prev->next = q = q->next ;
		else
		    ipfw_dyn_v[i] = q = q->next ;
		dyn_count-- ;
		free(old_q, M_IPFW);
		continue ;
	    } else {
		prev = q ;
		q = q->next ;
	    }
	}
    }
}

static struct ipfw_dyn_rule *
lookup_dyn_rule(struct ipfw_flow_id *pkt, int *match_direction)
{
    /*
     * stateful ipfw extensions.
     * Lookup into dynamic session queue
     */
    struct ipfw_dyn_rule *prev, *q, *old_q ;
    int i, dir = 0;
#define MATCH_FORWARD 1

    if (ipfw_dyn_v == NULL)
	return NULL ;
    i = hash_packet( pkt );
    for (prev=NULL, q = ipfw_dyn_v[i] ; q != NULL ; ) {
       if (TIME_LEQ( q->expire , time_second ) ) { /* expire entry */
           old_q = q ;
           if (prev != NULL)
               prev->next = q = q->next ;
           else
               ipfw_dyn_v[i] = q = q->next ;
           dyn_count-- ;
           free(old_q, M_IPFW);
           continue ;
	}
	if ( pkt->proto == q->id.proto) {
	    switch (q->type) {
	    default:        /* bidirectional rule, no masks */
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
		break ;
	    }
	}
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
	    q->expire = time_second + dyn_fin_lifetime ;
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
    } else {
	/* should do something for UDP and others... */
	q->expire = time_second + dyn_short_lifetime ;
    }
    if (match_direction)
	*match_direction = dir ;
    return q ;
}

/*
 * Install state for a dynamic session.
 */

static void
add_dyn_rule(struct ipfw_flow_id *id, struct ipfw_flow_id *mask,
       struct ip_fw_chain *chain)
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
           if (ipfw_dyn_v != NULL)
		free(ipfw_dyn_v, M_IPFW);
           ipfw_dyn_v = malloc(curr_dyn_buckets * sizeof r,
                   M_IPFW, M_DONTWAIT);
	   if (ipfw_dyn_v == NULL)
		return ; /* failed ! */
           bzero(ipfw_dyn_v, curr_dyn_buckets * sizeof r);
       }
    }
    i = hash_packet(id);

    r = malloc(sizeof *r, M_IPFW, M_DONTWAIT);
    if (r == NULL) {
       printf ("sorry cannot allocate state\n");
       return ;
    }
    bzero (r, sizeof (*r) );

    if (mask)
	r->mask = *mask ;
    r->id = *id ;
    r->expire = time_second + dyn_syn_lifetime ;
    r->chain = chain ;
    r->type = ((struct ip_fw_ext *)chain->rule)->dyn_type ;

    r->bucket = i ;
    r->next = ipfw_dyn_v[i] ;
    ipfw_dyn_v[i] = r ;
    dyn_count++ ;
    DEB(printf("-- add entry 0x%08x %d -> 0x%08x %d, %d left\n",
       (r->id.src_ip), (r->id.src_port),
       (r->id.dst_ip), (r->id.dst_port),
       dyn_count ); )
}

/*
 * Install dynamic state.
 * There are different types of dynamic rules which can be installed.
 * The type is in chain->dyn_type.
 * Type 0 (default) is a bidirectional rule
 */
static void
install_state(struct ip_fw_chain *chain)
{
    struct ipfw_dyn_rule *q ;
    static int last_log ;

    u_long type = ((struct ip_fw_ext *)chain->rule)->dyn_type ;

    DEB(printf("-- install state type %d 0x%08lx %u -> 0x%08lx %u\n",
       type,
       (last_pkt.src_ip), (last_pkt.src_port),
       (last_pkt.dst_ip), (last_pkt.dst_port) );)

    q = lookup_dyn_rule(&last_pkt, NULL) ;
    if (q != NULL) {
	if (last_log == time_second)
	    return ;
	last_log = time_second ;
       printf(" entry already present, done\n");
       return ;
    }
    if (dyn_count >= dyn_max) /* try remove old ones... */
	remove_dyn_rule(NULL, 0 /* expire */);
    if (dyn_count >= dyn_max) {
	if (last_log == time_second)
	    return ;
	last_log = time_second ;
       printf(" Too many dynamic rules, sorry\n");
       return ;
    }
    switch (type) {
    default: /* bidir rule */
       add_dyn_rule(&last_pkt, NULL, chain);
       break ;
    }
    q = lookup_dyn_rule(&last_pkt, NULL) ; /* XXX this just sets the lifetime ... */
}
#endif /* STATEFUL */

/*
 * given an ip_fw_chain *, lookup_next_rule will return a pointer
 * of the same type to the next one. This can be either the jump
 * target (for skipto instructions) or the next one in the chain (in
 * all other cases including a missing jump target).
 * Backward jumps are not allowed, so start looking from the next
 * rule...
 */ 
static struct ip_fw_chain * lookup_next_rule(struct ip_fw_chain *me);

static struct ip_fw_chain *
lookup_next_rule(struct ip_fw_chain *me)
{
    struct ip_fw_chain *chain ;
    int rule = me->rule->fw_skipto_rule ; /* guess... */

    if ( (me->rule->fw_flg & IP_FW_F_COMMAND) == IP_FW_F_SKIPTO )
	for (chain = me->chain.le_next; chain ; chain = chain->chain.le_next )
	    if (chain->rule->fw_number >= rule)
                return chain ;
    return me->chain.le_next ; /* failure or not a skipto */
}

/*
 * Parameters:
 *
 *	pip	Pointer to packet header (struct ip **)
 *	hlen	Packet header length
 *	oif	Outgoing interface, or NULL if packet is incoming
 *	*cookie Skip up to the first rule past this rule number;
 *		upon return, non-zero port number for divert or tee.
 *		Special case: cookie == NULL on input for bridging.
 *	*m	The packet; we set to NULL when/if we nuke it.
 *	*flow_id pointer to the last matching rule (in/out)
 *	*next_hop socket we are forwarding to (in/out).
 *
 * Return value:
 *
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
ip_fw_chk(struct ip **pip, int hlen,
	struct ifnet *oif, u_int16_t *cookie, struct mbuf **m,
	struct ip_fw_chain **flow_id,
        struct sockaddr_in **next_hop)
{
	struct ip_fw_chain *chain;
	struct ip_fw *f = NULL, *rule = NULL;
	struct ip *ip = *pip;
	struct ifnet *const rif = (*m)->m_pkthdr.rcvif;
	u_short offset = 0 ;
	u_short src_port = 0, dst_port = 0;
	struct in_addr src_ip, dst_ip; /* XXX */
	u_int8_t proto= 0, flags = 0 ; /* XXX */
	u_int16_t skipto, bridgeCookie;

#if STATEFUL
	int dyn_checked = 0 ; /* set after dyn.rules have been checked. */
	int direction = MATCH_FORWARD ; /* dirty trick... */
	struct ipfw_dyn_rule *q = NULL ;
#endif

	/* Special hack for bridging (as usual) */
	if (cookie == NULL) {
		bridgeCookie = 0;
		cookie = &bridgeCookie;
	}

	/* Grab and reset cookie */
	skipto = *cookie;
	*cookie = 0;

#define PULLUP_TO(len)	do {						\
			    if ((*m)->m_len < (len)) {			\
				if ((*m = m_pullup(*m, (len))) == 0)	\
				    goto bogusfrag;			\
				ip = mtod(*m, struct ip *);		\
				*pip = ip;				\
			    }						\
			} while (0)

	/*
	 * Collect parameters into local variables for faster matching.
	 */
	proto = ip->ip_p;
	src_ip = ip->ip_src;
	dst_ip = ip->ip_dst;
	offset = (ip->ip_off & IP_OFFMASK);
	if (offset == 0) {
	    struct tcphdr *tcp;
	    struct udphdr *udp;

	    switch (proto) {
	    case IPPROTO_TCP :
		PULLUP_TO(hlen + sizeof(struct tcphdr));
		tcp =(struct tcphdr *)((u_int32_t *)ip + ip->ip_hl);
		dst_port = tcp->th_dport ;
		src_port = tcp->th_sport ;
		flags = tcp->th_flags ;
		break ;

	    case IPPROTO_UDP :
		PULLUP_TO(hlen + sizeof(struct udphdr));
		udp =(struct udphdr *)((u_int32_t *)ip + ip->ip_hl);
		dst_port = udp->uh_dport ;
		src_port = udp->uh_sport ;
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
#undef PULLUP_TO
	last_pkt.src_ip = ntohl(src_ip.s_addr);
	last_pkt.dst_ip = ntohl(dst_ip.s_addr);
	last_pkt.proto = proto;
	last_pkt.src_port = ntohs(src_port);
	last_pkt.dst_port = ntohs(dst_port);
	last_pkt.flags = flags;

	if (*flow_id) {
		/* Accept if passed first test */
		if (fw_one_pass)
			return 0;
		/*
		 * Packet has already been tagged. Look for the next rule
		 * to restart processing.
		 */
		chain = LIST_NEXT(*flow_id, chain);

		if ((chain = (*flow_id)->rule->next_rule_ptr) == NULL)
			chain = (*flow_id)->rule->next_rule_ptr =
			    lookup_next_rule(*flow_id);
		if (chain == NULL)
			goto dropit;
	} else {
		/*
		 * Go down the chain, looking for enlightment.
		 * If we've been asked to start at a given rule, do so.
		 */
		chain = LIST_FIRST(&ip_fw_chain);
		if (skipto != 0) {
			if (skipto >= IPFW_DEFAULT_RULE)
				goto dropit;
			while (chain && chain->rule->fw_number <= skipto)
				chain = LIST_NEXT(chain, chain);
			if (chain == NULL)
				goto dropit;
		}
	}


	for (; chain; chain = LIST_NEXT(chain, chain)) {
again:
		f = chain->rule;
		if (f->fw_number == IPFW_DEFAULT_RULE)
		    goto got_match ;

#if STATEFUL
		/*
		 * dynamic rules are checked at the first keep-state or
		 * check-state occurrence.
		 */
		if (f->fw_flg & (IP_FW_F_KEEP_S|IP_FW_F_CHECK_S) &&
			 dyn_checked == 0 ) {
		    dyn_checked = 1 ;
		    q = lookup_dyn_rule(&last_pkt, &direction);
		    if (q != NULL) {
			DEB(printf("-- dynamic match 0x%08x %d %s 0x%08x %d\n",
			    (q->id.src_ip), (q->id.src_port),
			    (direction == MATCH_FORWARD ? "-->" : "<--"),
			    (q->id.dst_ip), (q->id.dst_port) ); )
			chain = q->chain ;
			f = chain->rule ;
			q->pcnt++ ;
			q->bcnt += ip->ip_len;
			goto got_match ; /* random not allowed here */
		    }
		    /* if this was a check-only rule, continue with next */
		    if (f->fw_flg & IP_FW_F_CHECK_S)
			continue ;
		}
#endif /* stateful ipfw */

		/* Check if rule only valid for bridged packets */
		if ((f->fw_flg & IP_FW_BRIDGED) != 0 && cookie != &bridgeCookie)
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
				if (f->fw_nports != 0 ||
				    f->fw_tcpf != f->fw_tcpnf)
					continue;

				break;
			}
			tcp = (struct tcphdr *) ((u_int32_t *)ip + ip->ip_hl);

			if (f->fw_tcpopt != f->fw_tcpnopt && !tcpopts_match(tcp, f))
				continue;
			if (f->fw_tcpf != f->fw_tcpnf && !tcpflg_match(tcp, f))
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
				if (f->fw_nports != 0)
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
		if (fw_verbose)
			ipfw_report(NULL, ip, rif, oif);
		goto dropit;

		}

rnd_then_got_match:
		if ( ((struct ip_fw_ext *)f)->dont_match_prob &&
		    random() < ((struct ip_fw_ext *)f)->dont_match_prob )
			continue ;
got_match:
#if STATEFUL   /* stateful ipfw */
		/*
		 * If not a dynamic match (q == NULL) and keep-state, install
		 * a new dynamic entry.
		 */
		if (q == NULL && f->fw_flg & IP_FW_F_KEEP_S)
		    install_state(chain);
#endif
		*flow_id = chain ; /* XXX set flow id */
		/* Update statistics */
		f->fw_pcnt += 1;
		f->fw_bcnt += ip->ip_len;
		f->timestamp = time_second;

		/* Log to console if desired */
		if ((f->fw_flg & IP_FW_F_PRN) && fw_verbose)
			ipfw_report(f, ip, rif, oif);

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
			if ( f->next_rule_ptr )
			    chain = f->next_rule_ptr ;
			else
			    chain = lookup_next_rule(chain) ;
			if (! chain) goto dropit;
			goto again ;
#ifdef DUMMYNET
		case IP_FW_F_PIPE:
		case IP_FW_F_QUEUE:
			return(f->fw_pipe_nr | IP_FW_PORT_DYNT_FLAG);
#endif
#ifdef IPFIREWALL_FORWARD
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
			 */
			if (next_hop != NULL /* Make sure, first... */
			    && (q == NULL || direction == MATCH_FORWARD) )
				*next_hop = &(f->fw_fwd_ip);
			return(0); /* Allow the packet */
#endif
		}

		/* Deny/reject this packet using this rule */
		rule = f;
		break;

	}

	/* Rule IPFW_DEFAULT_RULE should always be there and match */
	KASSERT(chain != NULL, ("ip_fw: no chain"));

	/*
	 * At this point, we're going to drop the packet.
	 * Send a reject notice if all of the following are true:
	 *
	 * - The packet matched a reject rule
	 * - The packet is not an ICMP packet, or is an ICMP query packet
	 * - The packet is not a multicast or broadcast packet
	 */
	if ((rule->fw_flg & IP_FW_F_COMMAND) == IP_FW_F_REJECT
	    && (ip->ip_p != IPPROTO_ICMP || is_icmp_query(ip))
	    && !((*m)->m_flags & (M_BCAST|M_MCAST))
	    && !IN_MULTICAST(ntohl(ip->ip_dst.s_addr))) {
		switch (rule->fw_reject_code) {
		case IP_FW_REJECT_RST:
		  {
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
			tip->ti_len = ip->ip_len - hlen - (tip->ti_off << 2);
			if (tcp->th_flags & TH_ACK) {
				tcp_respond(NULL, (void *)ip, tcp, *m,
				    (tcp_seq)0, ntohl(tcp->th_ack), TH_RST);
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
			    rule->fw_reject_code, 0L, 0);
			*m = NULL;
			break;
		}
	}

dropit:
	/*
	 * Finally, drop the packet.
	 */
	if (*m) {
		m_freem(*m);
		*m = NULL;
	}
	return(0);
}

/*
 * when a rule is added/deleted, zero the direct pointers within
 * all firewall rules. These will be reconstructed on the fly
 * as packets are matched.
 * Must be called at splnet().
 */
static void
flush_rule_ptrs()
{
    struct ip_fw_chain *fcp ;

    for (fcp = ip_fw_chain.lh_first; fcp; fcp = fcp->chain.le_next) {
	fcp->rule->next_rule_ptr = NULL ;
    }
}

static int
add_entry(struct ip_fw_head *chainptr, struct ip_fw *frwl)
{
	struct ip_fw *ftmp = 0;
	struct ip_fw_ext *ftmp_ext = 0 ;
	struct ip_fw_chain *fwc = 0, *fcp, *fcpl = 0;
	u_short nbr = 0;
	int s;

	fwc = malloc(sizeof *fwc, M_IPFW, M_DONTWAIT);
	ftmp_ext = malloc(sizeof *ftmp_ext, M_IPFW, M_DONTWAIT);
	ftmp = &ftmp_ext->rule ;
	if (!fwc || !ftmp) {
		dprintf(("%s malloc said no\n", err_prefix));
		if (fwc)  free(fwc, M_IPFW);
		if (ftmp) free(ftmp, M_IPFW);
		return (ENOSPC);
	}

	bzero(ftmp_ext, sizeof(*ftmp_ext)); /* play safe! */
	bcopy(frwl, ftmp, sizeof(*ftmp));
	if (ftmp->fw_flg & IP_FW_F_RND_MATCH)
		ftmp_ext->dont_match_prob = (intptr_t)ftmp->pipe_ptr;
	if (ftmp->fw_flg & IP_FW_F_KEEP_S)
		ftmp_ext->dyn_type = (u_long)(ftmp->next_rule_ptr) ;

	ftmp->fw_in_if.fu_via_if.name[FW_IFNLEN - 1] = '\0';
	ftmp->fw_pcnt = 0L;
	ftmp->fw_bcnt = 0L;
	ftmp->next_rule_ptr = NULL ;
	ftmp->pipe_ptr = NULL ;
	fwc->rule = ftmp;
	
	s = splnet();

	if (chainptr->lh_first == 0) {
		LIST_INSERT_HEAD(chainptr, fwc, chain);
		splx(s);
		return(0);
        }

	/* If entry number is 0, find highest numbered rule and add 100 */
	if (ftmp->fw_number == 0) {
		for (fcp = LIST_FIRST(chainptr); fcp; fcp = LIST_NEXT(fcp, chain)) {
			if (fcp->rule->fw_number != (u_short)-1)
				nbr = fcp->rule->fw_number;
			else
				break;
		}
		if (nbr < IPFW_DEFAULT_RULE - 100)
			nbr += 100;
		ftmp->fw_number = frwl->fw_number = nbr;
	}

	/* Got a valid number; now insert it, keeping the list ordered */
	for (fcp = LIST_FIRST(chainptr); fcp; fcp = LIST_NEXT(fcp, chain)) {
		if (fcp->rule->fw_number > ftmp->fw_number) {
			if (fcpl) {
				LIST_INSERT_AFTER(fcpl, fwc, chain);
			} else {
				LIST_INSERT_HEAD(chainptr, fwc, chain);
			}
			break;
		} else {
			fcpl = fcp;
		}
	}
	flush_rule_ptrs();

	splx(s);
	return (0);
}

static int
del_entry(struct ip_fw_head *chainptr, u_short number)
{
	struct ip_fw_chain *fcp;

	fcp = LIST_FIRST(chainptr);
	if (number != (u_short)-1) {
		for (; fcp; fcp = LIST_NEXT(fcp, chain)) {
			if (fcp->rule->fw_number == number) {
				int s;

				/* prevent access to rules while removing them */
				s = splnet();
				while (fcp && fcp->rule->fw_number == number) {
					struct ip_fw_chain *next;

#if STATEFUL
					remove_dyn_rule(fcp, 1 /* force_delete */);
#endif
					next = LIST_NEXT(fcp, chain);
					LIST_REMOVE(fcp, chain);
#ifdef DUMMYNET
					dn_rule_delete(fcp) ;
#endif
					flush_rule_ptrs();
					free(fcp->rule, M_IPFW);
					free(fcp, M_IPFW);
					fcp = next;
				}
				splx(s);
				return 0;
			}
		}
	}

	return (EINVAL);
}

static int
zero_entry(struct ip_fw *frwl)
{
	struct ip_fw_chain *fcp;
	int s, cleared;

	if (frwl == 0) {
		s = splnet();
		for (fcp = LIST_FIRST(&ip_fw_chain); fcp; fcp = LIST_NEXT(fcp, chain)) {
			fcp->rule->fw_bcnt = fcp->rule->fw_pcnt = 0;
			fcp->rule->fw_loghighest = fcp->rule->fw_logamount;
			fcp->rule->timestamp = 0;
		}
		splx(s);
	}
	else {
		cleared = 0;

		/*
		 *	It's possible to insert multiple chain entries with the
		 *	same number, so we don't stop after finding the first
		 *	match if zeroing a specific entry.
		 */
		for (fcp = LIST_FIRST(&ip_fw_chain); fcp; fcp = LIST_NEXT(fcp, chain))
			if (frwl->fw_number == fcp->rule->fw_number) {
				s = splnet();
				while (fcp && frwl->fw_number == fcp->rule->fw_number) {
					fcp->rule->fw_bcnt = fcp->rule->fw_pcnt = 0;
					fcp->rule->fw_loghighest =
					    fcp->rule->fw_logamount;
					fcp->rule->timestamp = 0;
					fcp = LIST_NEXT(fcp, chain);
				}
				splx(s);
				cleared = 1;
				break;
			}
		if (!cleared)	/* we didn't find any matching rules */
			return (EINVAL);
	}

	if (fw_verbose) {
		if (frwl)
			log(LOG_SECURITY | LOG_NOTICE,
			    "ipfw: Entry %d cleared.\n", frwl->fw_number);
		else
			log(LOG_SECURITY | LOG_NOTICE,
			    "ipfw: Accounting cleared.\n");
	}

	return (0);
}

static int
resetlog_entry(struct ip_fw *frwl)
{
	struct ip_fw_chain *fcp;
	int s, cleared;

	if (frwl == 0) {
		s = splnet();
		counter = 0;
		for (fcp = LIST_FIRST(&ip_fw_chain); fcp; fcp = LIST_NEXT(fcp, chain))
			fcp->rule->fw_loghighest = fcp->rule->fw_pcnt +
			    fcp->rule->fw_logamount;
		splx(s);
	}
	else {
		cleared = 0;

		/*
		 *	It's possible to insert multiple chain entries with the
		 *	same number, so we don't stop after finding the first
		 *	match if zeroing a specific entry.
		 */
		for (fcp = LIST_FIRST(&ip_fw_chain); fcp; fcp = LIST_NEXT(fcp, chain))
			if (frwl->fw_number == fcp->rule->fw_number) {
				s = splnet();
				while (fcp && frwl->fw_number == fcp->rule->fw_number) {
					fcp->rule->fw_loghighest =
					    fcp->rule->fw_pcnt +
					    fcp->rule->fw_logamount;
					fcp = LIST_NEXT(fcp, chain);
				}
				splx(s);
				cleared = 1;
				break;
			}
		if (!cleared)	/* we didn't find any matching rules */
			return (EINVAL);
	}

	if (fw_verbose) {
		if (frwl)
			log(LOG_SECURITY | LOG_NOTICE,
			    "ipfw: Entry %d logging count reset.\n",
			    frwl->fw_number);
		else
			log(LOG_SECURITY | LOG_NOTICE, "
			    ipfw: All logging counts cleared.\n");
	}

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
		if (frwl->fw_nports) {
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
	switch (frwl->fw_flg & IP_FW_F_COMMAND)
	{
	case IP_FW_F_REJECT:
		if (frwl->fw_reject_code >= 0x100
		    && !(frwl->fw_prot == IPPROTO_TCP
		      && frwl->fw_reject_code == IP_FW_REJECT_RST)) {
			dprintf(("%s unknown reject code\n", err_prefix));
			return (EINVAL);
		}
		break;
#if defined(IPDIVERT) || defined(DUMMYNET)
#ifdef IPDIVERT
	case IP_FW_F_DIVERT:		/* Diverting to port zero is invalid */
	case IP_FW_F_TEE:
#endif
#ifdef DUMMYNET
	case IP_FW_F_PIPE:              /* piping through 0 is invalid */
	case IP_FW_F_QUEUE:             /* piping through 0 is invalid */
#endif
		if (frwl->fw_divert_port == 0) {
			dprintf(("%s can't divert to port 0\n", err_prefix));
			return (EINVAL);
		}
		break;
#endif /* IPDIVERT || DUMMYNET */
	case IP_FW_F_DENY:
	case IP_FW_F_ACCEPT:
	case IP_FW_F_COUNT:
	case IP_FW_F_SKIPTO:
#ifdef IPFIREWALL_FORWARD
	case IP_FW_F_FWD:
#endif
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
	struct ip_fw_chain *fcp;
	struct ip_fw frwl, *bp , *buf;

	/*
	 * Disallow sets in really-really secure mode, but still allow
	 * the logging counters to be reset.
	 */
	if (sopt->sopt_dir == SOPT_SET && securelevel >= 3 &&
	    sopt->sopt_name != IP_FW_RESETLOG)
			return (EPERM);
	error = 0;

	switch (sopt->sopt_name) {
	case IP_FW_GET:
		for (fcp = LIST_FIRST(&ip_fw_chain), size = 0; fcp;
		     fcp = LIST_NEXT(fcp, chain))
			size += sizeof *fcp->rule;
#if STATEFUL
		if (ipfw_dyn_v) {
		    int i ;
		    struct ipfw_dyn_rule *p ;

		    for (i = 0 ; i < curr_dyn_buckets ; i++ )
			for ( p = ipfw_dyn_v[i] ; p != NULL ; p = p->next )
			    size += sizeof(*p) ;
		}
#endif
		buf = malloc(size, M_TEMP, M_WAITOK);
		if (buf == 0) {
			error = ENOBUFS;
			break;
		}

		for (fcp = LIST_FIRST(&ip_fw_chain), bp = buf; fcp;
		     fcp = LIST_NEXT(fcp, chain)) {
			bcopy(fcp->rule, bp, sizeof *fcp->rule);
			bp->pipe_ptr = (void *)(intptr_t)
			    ((struct ip_fw_ext *)fcp->rule)->dont_match_prob;
			bp->next_rule_ptr = (void *)(intptr_t)
			    ((struct ip_fw_ext *)fcp->rule)->dyn_type;
			bp++;
		}
#if STATEFUL
		if (ipfw_dyn_v) {
		    int i ;
		    struct ipfw_dyn_rule *p, *dst, *last = NULL ;

		    dst = (struct ipfw_dyn_rule *)bp ;
		    for (i = 0 ; i < curr_dyn_buckets ; i++ )
			for ( p = ipfw_dyn_v[i] ; p != NULL ; p = p->next, dst++ ) {
			    bcopy(p, dst, sizeof *p);
                            (int)dst->chain = p->chain->rule->fw_number ;
                            dst->next = dst ; /* fake non-null pointer... */
			    last = dst ;
			    if (TIME_LEQ(dst->expire, time_second) )
				dst->expire = 0 ;
			    else
				dst->expire -= time_second ;
			    }
		    if (last != NULL)
			last->next = NULL ;
		}
#endif
		error = sooptcopyout(sopt, buf, size);
		FREE(buf, M_TEMP);
		break;

	case IP_FW_FLUSH:
#if STATEFUL
               s = splnet();
               remove_dyn_rule(NULL, 1 /* force delete */);
               splx(s);
#endif
		for (fcp = ip_fw_chain.lh_first; 
		     fcp != 0 && fcp->rule->fw_number != IPFW_DEFAULT_RULE;
		     fcp = ip_fw_chain.lh_first) {
			s = splnet();
			LIST_REMOVE(fcp, chain);
#ifdef DUMMYNET
			dn_rule_delete(fcp);
#endif
			FREE(fcp->rule, M_IPFW);
			FREE(fcp, M_IPFW);
			splx(s);
		}
		break;

	case IP_FW_ZERO:
		if (sopt->sopt_val != 0) {
			error = sooptcopyin(sopt, &frwl, sizeof frwl,
					    sizeof frwl);
			if (error || (error = zero_entry(&frwl)))
				break;
		} else {
			error = zero_entry(0);
		}
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
			error = add_entry(&ip_fw_chain, &frwl);
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
			error = del_entry(&ip_fw_chain, frwl.fw_number);
		}
		break;

	case IP_FW_RESETLOG:
		if (sopt->sopt_val != 0) {
			error = sooptcopyin(sopt, &frwl, sizeof frwl,
					    sizeof frwl);
			if (error || (error = resetlog_entry(&frwl)))
				break;
		} else {
			error = resetlog_entry(0);
		}
		break;

	default:
		printf("ip_fw_ctl invalid option %d\n", sopt->sopt_name);
		error = EINVAL ;
	}

	return (error);
}

struct ip_fw_chain *ip_fw_default_rule ;

void
ip_fw_init(void)
{
	struct ip_fw default_rule;

	ip_fw_chk_ptr = ip_fw_chk;
	ip_fw_ctl_ptr = ip_fw_ctl;
	LIST_INIT(&ip_fw_chain);

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
	    add_entry(&ip_fw_chain, &default_rule))
		panic("ip_fw_init");

	ip_fw_default_rule = ip_fw_chain.lh_first ;
	printf("IP packet filtering initialized, "
#ifdef IPDIVERT
		"divert enabled, "
#else
		"divert disabled, "
#endif
#ifdef IPFIREWALL_FORWARD
		"rule-based forwarding enabled, "
#else
		"rule-based forwarding disabled, "
#endif
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

static ip_fw_chk_t *old_chk_ptr;
static ip_fw_ctl_t *old_ctl_ptr;

static int
ipfw_modevent(module_t mod, int type, void *unused)
{
	int s;
	
	switch (type) {
	case MOD_LOAD:
		s = splnet();

		old_chk_ptr = ip_fw_chk_ptr;
		old_ctl_ptr = ip_fw_ctl_ptr;

		ip_fw_init();
		splx(s);
		return 0;
	case MOD_UNLOAD:
		s = splnet();
		ip_fw_chk_ptr =  old_chk_ptr;
		ip_fw_ctl_ptr =  old_ctl_ptr;
#if STATEFUL
		remove_dyn_rule(NULL, 1 /* force delete */);
#endif
		while (LIST_FIRST(&ip_fw_chain) != NULL) {
			struct ip_fw_chain *fcp = LIST_FIRST(&ip_fw_chain);
			LIST_REMOVE(LIST_FIRST(&ip_fw_chain), chain);
#ifdef DUMMYNET
			dn_rule_delete(fcp);
#endif
			free(fcp->rule, M_IPFW);
			free(fcp, M_IPFW);
		}
	
		splx(s);
		printf("IP firewall unloaded\n");
		return 0;
	default:
		break;
	}
	return 0;
}

static moduledata_t ipfwmod = {
	"ipfw",
	ipfw_modevent,
	0
};
DECLARE_MODULE(ipfw, ipfwmod, SI_SUB_PSEUDO, SI_ORDER_ANY);
