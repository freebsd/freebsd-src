/*-
 * Copyright (c) 2002 Luigi Rizzo, Universita` di Pisa
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#define        DEB(x)
#define        DDB(x) x

/*
 * Dynamic rule support for ipfw
 */

#if !defined(KLD_MODULE)
#include "opt_ipfw.h"
#include "opt_ipdivert.h"
#include "opt_ipdn.h"
#include "opt_inet.h"
#ifndef INET
#error IPFIREWALL requires INET.
#endif /* INET */
#endif
#include "opt_inet6.h"
#include "opt_ipsec.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <net/ethernet.h> /* for ETHERTYPE_IP */
#include <net/if.h>
#include <net/vnet.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>	/* ip_defttl */
#include <netinet/ip_fw.h>
#include <netinet/ipfw/ip_fw_private.h>
#include <netinet/tcp_var.h>
#include <netinet/udp.h>

#include <netinet/ip6.h>	/* IN6_ARE_ADDR_EQUAL */
#ifdef INET6
#include <netinet6/in6_var.h>
#include <netinet6/ip6_var.h>
#endif

#include <machine/in_cksum.h>	/* XXX for in_cksum */

#ifdef MAC
#include <security/mac/mac_framework.h>
#endif

/*
 * Description of dynamic rules.
 *
 * Dynamic rules are stored in lists accessed through a hash table
 * (ipfw_dyn_v) whose size is curr_dyn_buckets. This value can
 * be modified through the sysctl variable dyn_buckets which is
 * updated when the table becomes empty.
 *
 * XXX currently there is only one list, ipfw_dyn.
 *
 * When a packet is received, its address fields are first masked
 * with the mask defined for the rule, then hashed, then matched
 * against the entries in the corresponding list.
 * Dynamic rules can be used for different purposes:
 *  + stateful rules;
 *  + enforcing limits on the number of sessions;
 *  + in-kernel NAT (not implemented yet)
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
 * Each dynamic rule holds a pointer to the parent ipfw rule so
 * we know what action to perform. Dynamic rules are removed when
 * the parent rule is deleted. XXX we should make them survive.
 *
 * There are some limitations with dynamic rules -- we do not
 * obey the 'randomized match', and we do not do multiple
 * passes through the firewall. XXX check the latter!!!
 */

/*
 * Static variables followed by global ones
 */
static VNET_DEFINE(ipfw_dyn_rule **, ipfw_dyn_v);
static VNET_DEFINE(u_int32_t, dyn_buckets);
static VNET_DEFINE(u_int32_t, curr_dyn_buckets);
static VNET_DEFINE(struct callout, ipfw_timeout);
#define	V_ipfw_dyn_v			VNET(ipfw_dyn_v)
#define	V_dyn_buckets			VNET(dyn_buckets)
#define	V_curr_dyn_buckets		VNET(curr_dyn_buckets)
#define V_ipfw_timeout                  VNET(ipfw_timeout)

static uma_zone_t ipfw_dyn_rule_zone;
#ifndef __FreeBSD__
DEFINE_SPINLOCK(ipfw_dyn_mtx);
#else
static struct mtx ipfw_dyn_mtx;		/* mutex guarding dynamic rules */
#endif

#define	IPFW_DYN_LOCK_INIT() \
	mtx_init(&ipfw_dyn_mtx, "IPFW dynamic rules", NULL, MTX_DEF)
#define	IPFW_DYN_LOCK_DESTROY()	mtx_destroy(&ipfw_dyn_mtx)
#define	IPFW_DYN_LOCK()		mtx_lock(&ipfw_dyn_mtx)
#define	IPFW_DYN_UNLOCK()	mtx_unlock(&ipfw_dyn_mtx)
#define	IPFW_DYN_LOCK_ASSERT()	mtx_assert(&ipfw_dyn_mtx, MA_OWNED)

void
ipfw_dyn_unlock(void)
{
	IPFW_DYN_UNLOCK();
}

/*
 * Timeouts for various events in handing dynamic rules.
 */
static VNET_DEFINE(u_int32_t, dyn_ack_lifetime);
static VNET_DEFINE(u_int32_t, dyn_syn_lifetime);
static VNET_DEFINE(u_int32_t, dyn_fin_lifetime);
static VNET_DEFINE(u_int32_t, dyn_rst_lifetime);
static VNET_DEFINE(u_int32_t, dyn_udp_lifetime);
static VNET_DEFINE(u_int32_t, dyn_short_lifetime);

#define	V_dyn_ack_lifetime		VNET(dyn_ack_lifetime)
#define	V_dyn_syn_lifetime		VNET(dyn_syn_lifetime)
#define	V_dyn_fin_lifetime		VNET(dyn_fin_lifetime)
#define	V_dyn_rst_lifetime		VNET(dyn_rst_lifetime)
#define	V_dyn_udp_lifetime		VNET(dyn_udp_lifetime)
#define	V_dyn_short_lifetime		VNET(dyn_short_lifetime)

/*
 * Keepalives are sent if dyn_keepalive is set. They are sent every
 * dyn_keepalive_period seconds, in the last dyn_keepalive_interval
 * seconds of lifetime of a rule.
 * dyn_rst_lifetime and dyn_fin_lifetime should be strictly lower
 * than dyn_keepalive_period.
 */

static VNET_DEFINE(u_int32_t, dyn_keepalive_interval);
static VNET_DEFINE(u_int32_t, dyn_keepalive_period);
static VNET_DEFINE(u_int32_t, dyn_keepalive);

#define	V_dyn_keepalive_interval	VNET(dyn_keepalive_interval)
#define	V_dyn_keepalive_period		VNET(dyn_keepalive_period)
#define	V_dyn_keepalive			VNET(dyn_keepalive)

static VNET_DEFINE(u_int32_t, dyn_count);	/* # of dynamic rules */
static VNET_DEFINE(u_int32_t, dyn_max);		/* max # of dynamic rules */

#define	V_dyn_count			VNET(dyn_count)
#define	V_dyn_max			VNET(dyn_max)

#ifdef SYSCTL_NODE

SYSBEGIN(f2)

SYSCTL_DECL(_net_inet_ip_fw);
SYSCTL_VNET_INT(_net_inet_ip_fw, OID_AUTO, dyn_buckets,
    CTLFLAG_RW, &VNET_NAME(dyn_buckets), 0,
    "Number of dyn. buckets");
SYSCTL_VNET_INT(_net_inet_ip_fw, OID_AUTO, curr_dyn_buckets,
    CTLFLAG_RD, &VNET_NAME(curr_dyn_buckets), 0,
    "Current Number of dyn. buckets");
SYSCTL_VNET_INT(_net_inet_ip_fw, OID_AUTO, dyn_count,
    CTLFLAG_RD, &VNET_NAME(dyn_count), 0,
    "Number of dyn. rules");
SYSCTL_VNET_INT(_net_inet_ip_fw, OID_AUTO, dyn_max,
    CTLFLAG_RW, &VNET_NAME(dyn_max), 0,
    "Max number of dyn. rules");
SYSCTL_VNET_INT(_net_inet_ip_fw, OID_AUTO, dyn_ack_lifetime,
    CTLFLAG_RW, &VNET_NAME(dyn_ack_lifetime), 0,
    "Lifetime of dyn. rules for acks");
SYSCTL_VNET_INT(_net_inet_ip_fw, OID_AUTO, dyn_syn_lifetime,
    CTLFLAG_RW, &VNET_NAME(dyn_syn_lifetime), 0,
    "Lifetime of dyn. rules for syn");
SYSCTL_VNET_INT(_net_inet_ip_fw, OID_AUTO, dyn_fin_lifetime,
    CTLFLAG_RW, &VNET_NAME(dyn_fin_lifetime), 0,
    "Lifetime of dyn. rules for fin");
SYSCTL_VNET_INT(_net_inet_ip_fw, OID_AUTO, dyn_rst_lifetime,
    CTLFLAG_RW, &VNET_NAME(dyn_rst_lifetime), 0,
    "Lifetime of dyn. rules for rst");
SYSCTL_VNET_INT(_net_inet_ip_fw, OID_AUTO, dyn_udp_lifetime,
    CTLFLAG_RW, &VNET_NAME(dyn_udp_lifetime), 0,
    "Lifetime of dyn. rules for UDP");
SYSCTL_VNET_INT(_net_inet_ip_fw, OID_AUTO, dyn_short_lifetime,
    CTLFLAG_RW, &VNET_NAME(dyn_short_lifetime), 0,
    "Lifetime of dyn. rules for other situations");
SYSCTL_VNET_INT(_net_inet_ip_fw, OID_AUTO, dyn_keepalive,
    CTLFLAG_RW, &VNET_NAME(dyn_keepalive), 0,
    "Enable keepalives for dyn. rules");

SYSEND

#endif /* SYSCTL_NODE */


static __inline int
hash_packet6(struct ipfw_flow_id *id)
{
	u_int32_t i;
	i = (id->dst_ip6.__u6_addr.__u6_addr32[2]) ^
	    (id->dst_ip6.__u6_addr.__u6_addr32[3]) ^
	    (id->src_ip6.__u6_addr.__u6_addr32[2]) ^
	    (id->src_ip6.__u6_addr.__u6_addr32[3]) ^
	    (id->dst_port) ^ (id->src_port);
	return i;
}

/*
 * IMPORTANT: the hash function for dynamic rules must be commutative
 * in source and destination (ip,port), because rules are bidirectional
 * and we want to find both in the same bucket.
 */
static __inline int
hash_packet(struct ipfw_flow_id *id)
{
	u_int32_t i;

#ifdef INET6
	if (IS_IP6_FLOW_ID(id)) 
		i = hash_packet6(id);
	else
#endif /* INET6 */
	i = (id->dst_ip) ^ (id->src_ip) ^ (id->dst_port) ^ (id->src_port);
	i &= (V_curr_dyn_buckets - 1);
	return i;
}

static __inline void
unlink_dyn_rule_print(struct ipfw_flow_id *id)
{
	struct in_addr da;
#ifdef INET6
	char src[INET6_ADDRSTRLEN], dst[INET6_ADDRSTRLEN];
#else
	char src[INET_ADDRSTRLEN], dst[INET_ADDRSTRLEN];
#endif

#ifdef INET6
	if (IS_IP6_FLOW_ID(id)) {
		ip6_sprintf(src, &id->src_ip6);
		ip6_sprintf(dst, &id->dst_ip6);
	} else
#endif
	{
		da.s_addr = htonl(id->src_ip);
		inet_ntoa_r(da, src);
		da.s_addr = htonl(id->dst_ip);
		inet_ntoa_r(da, dst);
	}
	printf("ipfw: unlink entry %s %d -> %s %d, %d left\n",
	    src, id->src_port, dst, id->dst_port, V_dyn_count - 1);
}

/**
 * unlink a dynamic rule from a chain. prev is a pointer to
 * the previous one, q is a pointer to the rule to delete,
 * head is a pointer to the head of the queue.
 * Modifies q and potentially also head.
 */
#define UNLINK_DYN_RULE(prev, head, q) {				\
	ipfw_dyn_rule *old_q = q;					\
									\
	/* remove a refcount to the parent */				\
	if (q->dyn_type == O_LIMIT)					\
		q->parent->count--;					\
	DEB(unlink_dyn_rule_print(&q->id);)				\
	if (prev != NULL)						\
		prev->next = q = q->next;				\
	else								\
		head = q = q->next;					\
	V_dyn_count--;							\
	uma_zfree(ipfw_dyn_rule_zone, old_q); }

#define TIME_LEQ(a,b)       ((int)((a)-(b)) <= 0)

/**
 * Remove dynamic rules pointing to "rule", or all of them if rule == NULL.
 *
 * If keep_me == NULL, rules are deleted even if not expired,
 * otherwise only expired rules are removed.
 *
 * The value of the second parameter is also used to point to identify
 * a rule we absolutely do not want to remove (e.g. because we are
 * holding a reference to it -- this is the case with O_LIMIT_PARENT
 * rules). The pointer is only used for comparison, so any non-null
 * value will do.
 */
static void
remove_dyn_rule(struct ip_fw *rule, ipfw_dyn_rule *keep_me)
{
	static u_int32_t last_remove = 0;

#define FORCE (keep_me == NULL)

	ipfw_dyn_rule *prev, *q;
	int i, pass = 0, max_pass = 0;

	IPFW_DYN_LOCK_ASSERT();

	if (V_ipfw_dyn_v == NULL || V_dyn_count == 0)
		return;
	/* do not expire more than once per second, it is useless */
	if (!FORCE && last_remove == time_uptime)
		return;
	last_remove = time_uptime;

	/*
	 * because O_LIMIT refer to parent rules, during the first pass only
	 * remove child and mark any pending LIMIT_PARENT, and remove
	 * them in a second pass.
	 */
next_pass:
	for (i = 0 ; i < V_curr_dyn_buckets ; i++) {
		for (prev=NULL, q = V_ipfw_dyn_v[i] ; q ; ) {
			/*
			 * Logic can become complex here, so we split tests.
			 */
			if (q == keep_me)
				goto next;
			if (rule != NULL && rule != q->rule)
				goto next; /* not the one we are looking for */
			if (q->dyn_type == O_LIMIT_PARENT) {
				/*
				 * handle parent in the second pass,
				 * record we need one.
				 */
				max_pass = 1;
				if (pass == 0)
					goto next;
				if (FORCE && q->count != 0 ) {
					/* XXX should not happen! */
					printf("ipfw: OUCH! cannot remove rule,"
					     " count %d\n", q->count);
				}
			} else {
				if (!FORCE &&
				    !TIME_LEQ( q->expire, time_uptime ))
					goto next;
			}
             if (q->dyn_type != O_LIMIT_PARENT || !q->count) {
                     UNLINK_DYN_RULE(prev, V_ipfw_dyn_v[i], q);
                     continue;
             }
next:
			prev=q;
			q=q->next;
		}
	}
	if (pass++ < max_pass)
		goto next_pass;
}

void
ipfw_remove_dyn_children(struct ip_fw *rule)
{
	IPFW_DYN_LOCK();
	remove_dyn_rule(rule, NULL /* force removal */);
	IPFW_DYN_UNLOCK();
}

/**
 * lookup a dynamic rule, locked version
 */
static ipfw_dyn_rule *
lookup_dyn_rule_locked(struct ipfw_flow_id *pkt, int *match_direction,
    struct tcphdr *tcp)
{
	/*
	 * stateful ipfw extensions.
	 * Lookup into dynamic session queue
	 */
#define MATCH_REVERSE	0
#define MATCH_FORWARD	1
#define MATCH_NONE	2
#define MATCH_UNKNOWN	3
	int i, dir = MATCH_NONE;
	ipfw_dyn_rule *prev, *q=NULL;

	IPFW_DYN_LOCK_ASSERT();

	if (V_ipfw_dyn_v == NULL)
		goto done;	/* not found */
	i = hash_packet( pkt );
	for (prev=NULL, q = V_ipfw_dyn_v[i] ; q != NULL ; ) {
		if (q->dyn_type == O_LIMIT_PARENT && q->count)
			goto next;
		if (TIME_LEQ( q->expire, time_uptime)) { /* expire entry */
			UNLINK_DYN_RULE(prev, V_ipfw_dyn_v[i], q);
			continue;
		}
		if (pkt->proto == q->id.proto &&
		    q->dyn_type != O_LIMIT_PARENT) {
			if (IS_IP6_FLOW_ID(pkt)) {
			    if (IN6_ARE_ADDR_EQUAL(&(pkt->src_ip6),
				&(q->id.src_ip6)) &&
			    IN6_ARE_ADDR_EQUAL(&(pkt->dst_ip6),
				&(q->id.dst_ip6)) &&
			    pkt->src_port == q->id.src_port &&
			    pkt->dst_port == q->id.dst_port ) {
				dir = MATCH_FORWARD;
				break;
			    }
			    if (IN6_ARE_ADDR_EQUAL(&(pkt->src_ip6),
				    &(q->id.dst_ip6)) &&
				IN6_ARE_ADDR_EQUAL(&(pkt->dst_ip6),
				    &(q->id.src_ip6)) &&
				pkt->src_port == q->id.dst_port &&
				pkt->dst_port == q->id.src_port ) {
				    dir = MATCH_REVERSE;
				    break;
			    }
			} else {
			    if (pkt->src_ip == q->id.src_ip &&
				pkt->dst_ip == q->id.dst_ip &&
				pkt->src_port == q->id.src_port &&
				pkt->dst_port == q->id.dst_port ) {
				    dir = MATCH_FORWARD;
				    break;
			    }
			    if (pkt->src_ip == q->id.dst_ip &&
				pkt->dst_ip == q->id.src_ip &&
				pkt->src_port == q->id.dst_port &&
				pkt->dst_port == q->id.src_port ) {
				    dir = MATCH_REVERSE;
				    break;
			    }
			}
		}
next:
		prev = q;
		q = q->next;
	}
	if (q == NULL)
		goto done; /* q = NULL, not found */

	if ( prev != NULL) { /* found and not in front */
		prev->next = q->next;
		q->next = V_ipfw_dyn_v[i];
		V_ipfw_dyn_v[i] = q;
	}
	if (pkt->proto == IPPROTO_TCP) { /* update state according to flags */
		u_char flags = pkt->_flags & (TH_FIN|TH_SYN|TH_RST);

#define BOTH_SYN	(TH_SYN | (TH_SYN << 8))
#define BOTH_FIN	(TH_FIN | (TH_FIN << 8))
		q->state |= (dir == MATCH_FORWARD ) ? flags : (flags << 8);
		switch (q->state) {
		case TH_SYN:				/* opening */
			q->expire = time_uptime + V_dyn_syn_lifetime;
			break;

		case BOTH_SYN:			/* move to established */
		case BOTH_SYN | TH_FIN :	/* one side tries to close */
		case BOTH_SYN | (TH_FIN << 8) :
 			if (tcp) {
#define _SEQ_GE(a,b) ((int)(a) - (int)(b) >= 0)
			    u_int32_t ack = ntohl(tcp->th_ack);
			    if (dir == MATCH_FORWARD) {
				if (q->ack_fwd == 0 || _SEQ_GE(ack, q->ack_fwd))
				    q->ack_fwd = ack;
				else { /* ignore out-of-sequence */
				    break;
				}
			    } else {
				if (q->ack_rev == 0 || _SEQ_GE(ack, q->ack_rev))
				    q->ack_rev = ack;
				else { /* ignore out-of-sequence */
				    break;
				}
			    }
			}
			q->expire = time_uptime + V_dyn_ack_lifetime;
			break;

		case BOTH_SYN | BOTH_FIN:	/* both sides closed */
			if (V_dyn_fin_lifetime >= V_dyn_keepalive_period)
				V_dyn_fin_lifetime = V_dyn_keepalive_period - 1;
			q->expire = time_uptime + V_dyn_fin_lifetime;
			break;

		default:
#if 0
			/*
			 * reset or some invalid combination, but can also
			 * occur if we use keep-state the wrong way.
			 */
			if ( (q->state & ((TH_RST << 8)|TH_RST)) == 0)
				printf("invalid state: 0x%x\n", q->state);
#endif
			if (V_dyn_rst_lifetime >= V_dyn_keepalive_period)
				V_dyn_rst_lifetime = V_dyn_keepalive_period - 1;
			q->expire = time_uptime + V_dyn_rst_lifetime;
			break;
		}
	} else if (pkt->proto == IPPROTO_UDP) {
		q->expire = time_uptime + V_dyn_udp_lifetime;
	} else {
		/* other protocols */
		q->expire = time_uptime + V_dyn_short_lifetime;
	}
done:
	if (match_direction)
		*match_direction = dir;
	return q;
}

ipfw_dyn_rule *
ipfw_lookup_dyn_rule(struct ipfw_flow_id *pkt, int *match_direction,
    struct tcphdr *tcp)
{
	ipfw_dyn_rule *q;

	IPFW_DYN_LOCK();
	q = lookup_dyn_rule_locked(pkt, match_direction, tcp);
	if (q == NULL)
		IPFW_DYN_UNLOCK();
	/* NB: return table locked when q is not NULL */
	return q;
}

static void
realloc_dynamic_table(void)
{
	IPFW_DYN_LOCK_ASSERT();

	/*
	 * Try reallocation, make sure we have a power of 2 and do
	 * not allow more than 64k entries. In case of overflow,
	 * default to 1024.
	 */

	if (V_dyn_buckets > 65536)
		V_dyn_buckets = 1024;
	if ((V_dyn_buckets & (V_dyn_buckets-1)) != 0) { /* not a power of 2 */
		V_dyn_buckets = V_curr_dyn_buckets; /* reset */
		return;
	}
	V_curr_dyn_buckets = V_dyn_buckets;
	if (V_ipfw_dyn_v != NULL)
		free(V_ipfw_dyn_v, M_IPFW);
	for (;;) {
		V_ipfw_dyn_v = malloc(V_curr_dyn_buckets * sizeof(ipfw_dyn_rule *),
		       M_IPFW, M_NOWAIT | M_ZERO);
		if (V_ipfw_dyn_v != NULL || V_curr_dyn_buckets <= 2)
			break;
		V_curr_dyn_buckets /= 2;
	}
}

/**
 * Install state of type 'type' for a dynamic session.
 * The hash table contains two type of rules:
 * - regular rules (O_KEEP_STATE)
 * - rules for sessions with limited number of sess per user
 *   (O_LIMIT). When they are created, the parent is
 *   increased by 1, and decreased on delete. In this case,
 *   the third parameter is the parent rule and not the chain.
 * - "parent" rules for the above (O_LIMIT_PARENT).
 */
static ipfw_dyn_rule *
add_dyn_rule(struct ipfw_flow_id *id, u_int8_t dyn_type, struct ip_fw *rule)
{
	ipfw_dyn_rule *r;
	int i;

	IPFW_DYN_LOCK_ASSERT();

	if (V_ipfw_dyn_v == NULL ||
	    (V_dyn_count == 0 && V_dyn_buckets != V_curr_dyn_buckets)) {
		realloc_dynamic_table();
		if (V_ipfw_dyn_v == NULL)
			return NULL; /* failed ! */
	}
	i = hash_packet(id);

	r = uma_zalloc(ipfw_dyn_rule_zone, M_NOWAIT | M_ZERO);
	if (r == NULL) {
		printf ("ipfw: sorry cannot allocate state\n");
		return NULL;
	}

	/* increase refcount on parent, and set pointer */
	if (dyn_type == O_LIMIT) {
		ipfw_dyn_rule *parent = (ipfw_dyn_rule *)rule;
		if ( parent->dyn_type != O_LIMIT_PARENT)
			panic("invalid parent");
		parent->count++;
		r->parent = parent;
		rule = parent->rule;
	}

	r->id = *id;
	r->expire = time_uptime + V_dyn_syn_lifetime;
	r->rule = rule;
	r->dyn_type = dyn_type;
	r->pcnt = r->bcnt = 0;
	r->count = 0;

	r->bucket = i;
	r->next = V_ipfw_dyn_v[i];
	V_ipfw_dyn_v[i] = r;
	V_dyn_count++;
	DEB({
		struct in_addr da;
#ifdef INET6
		char src[INET6_ADDRSTRLEN];
		char dst[INET6_ADDRSTRLEN];
#else
		char src[INET_ADDRSTRLEN];
		char dst[INET_ADDRSTRLEN];
#endif

#ifdef INET6
		if (IS_IP6_FLOW_ID(&(r->id))) {
			ip6_sprintf(src, &r->id.src_ip6);
			ip6_sprintf(dst, &r->id.dst_ip6);
		} else
#endif
		{
			da.s_addr = htonl(r->id.src_ip);
			inet_ntoa_r(da, src);
			da.s_addr = htonl(r->id.dst_ip);
			inet_ntoa_r(da, dst);
		}
		printf("ipfw: add dyn entry ty %d %s %d -> %s %d, total %d\n",
		    dyn_type, src, r->id.src_port, dst, r->id.dst_port,
		    V_dyn_count);
	})
	return r;
}

/**
 * lookup dynamic parent rule using pkt and rule as search keys.
 * If the lookup fails, then install one.
 */
static ipfw_dyn_rule *
lookup_dyn_parent(struct ipfw_flow_id *pkt, struct ip_fw *rule)
{
	ipfw_dyn_rule *q;
	int i;

	IPFW_DYN_LOCK_ASSERT();

	if (V_ipfw_dyn_v) {
		int is_v6 = IS_IP6_FLOW_ID(pkt);
		i = hash_packet( pkt );
		for (q = V_ipfw_dyn_v[i] ; q != NULL ; q=q->next)
			if (q->dyn_type == O_LIMIT_PARENT &&
			    rule== q->rule &&
			    pkt->proto == q->id.proto &&
			    pkt->src_port == q->id.src_port &&
			    pkt->dst_port == q->id.dst_port &&
			    (
				(is_v6 &&
				 IN6_ARE_ADDR_EQUAL(&(pkt->src_ip6),
					&(q->id.src_ip6)) &&
				 IN6_ARE_ADDR_EQUAL(&(pkt->dst_ip6),
					&(q->id.dst_ip6))) ||
				(!is_v6 &&
				 pkt->src_ip == q->id.src_ip &&
				 pkt->dst_ip == q->id.dst_ip)
			    )
			) {
				q->expire = time_uptime + V_dyn_short_lifetime;
				DEB(printf("ipfw: lookup_dyn_parent found 0x%p\n",q);)
				return q;
			}
	}
	return add_dyn_rule(pkt, O_LIMIT_PARENT, rule);
}

/**
 * Install dynamic state for rule type cmd->o.opcode
 *
 * Returns 1 (failure) if state is not installed because of errors or because
 * session limitations are enforced.
 */
int
ipfw_install_state(struct ip_fw *rule, ipfw_insn_limit *cmd,
    struct ip_fw_args *args, uint32_t tablearg)
{
	static int last_log;
	ipfw_dyn_rule *q;
	struct in_addr da;
#ifdef INET6
	char src[INET6_ADDRSTRLEN + 2], dst[INET6_ADDRSTRLEN + 2];
#else
	char src[INET_ADDRSTRLEN], dst[INET_ADDRSTRLEN];
#endif

	src[0] = '\0';
	dst[0] = '\0';

	IPFW_DYN_LOCK();

	DEB(
#ifdef INET6
	if (IS_IP6_FLOW_ID(&(args->f_id))) {
		ip6_sprintf(src, &args->f_id.src_ip6);
		ip6_sprintf(dst, &args->f_id.dst_ip6);
	} else
#endif
	{
		da.s_addr = htonl(args->f_id.src_ip);
		inet_ntoa_r(da, src);
		da.s_addr = htonl(args->f_id.dst_ip);
		inet_ntoa_r(da, dst);
	}
	printf("ipfw: %s: type %d %s %u -> %s %u\n",
	    __func__, cmd->o.opcode, src, args->f_id.src_port,
	    dst, args->f_id.dst_port);
	src[0] = '\0';
	dst[0] = '\0';
	)

	q = lookup_dyn_rule_locked(&args->f_id, NULL, NULL);

	if (q != NULL) {	/* should never occur */
		DEB(
		if (last_log != time_uptime) {
			last_log = time_uptime;
			printf("ipfw: %s: entry already present, done\n",
			    __func__);
		})
		IPFW_DYN_UNLOCK();
		return (0);
	}

	if (V_dyn_count >= V_dyn_max)
		/* Run out of slots, try to remove any expired rule. */
		remove_dyn_rule(NULL, (ipfw_dyn_rule *)1);

	if (V_dyn_count >= V_dyn_max) {
		if (last_log != time_uptime) {
			last_log = time_uptime;
			printf("ipfw: %s: Too many dynamic rules\n", __func__);
		}
		IPFW_DYN_UNLOCK();
		return (1);	/* cannot install, notify caller */
	}

	switch (cmd->o.opcode) {
	case O_KEEP_STATE:	/* bidir rule */
		add_dyn_rule(&args->f_id, O_KEEP_STATE, rule);
		break;

	case O_LIMIT: {		/* limit number of sessions */
		struct ipfw_flow_id id;
		ipfw_dyn_rule *parent;
		uint32_t conn_limit;
		uint16_t limit_mask = cmd->limit_mask;

		conn_limit = (cmd->conn_limit == IP_FW_TABLEARG) ?
		    tablearg : cmd->conn_limit;
		  
		DEB(
		if (cmd->conn_limit == IP_FW_TABLEARG)
			printf("ipfw: %s: O_LIMIT rule, conn_limit: %u "
			    "(tablearg)\n", __func__, conn_limit);
		else
			printf("ipfw: %s: O_LIMIT rule, conn_limit: %u\n",
			    __func__, conn_limit);
		)

		id.dst_ip = id.src_ip = id.dst_port = id.src_port = 0;
		id.proto = args->f_id.proto;
		id.addr_type = args->f_id.addr_type;
		id.fib = M_GETFIB(args->m);

		if (IS_IP6_FLOW_ID (&(args->f_id))) {
			if (limit_mask & DYN_SRC_ADDR)
				id.src_ip6 = args->f_id.src_ip6;
			if (limit_mask & DYN_DST_ADDR)
				id.dst_ip6 = args->f_id.dst_ip6;
		} else {
			if (limit_mask & DYN_SRC_ADDR)
				id.src_ip = args->f_id.src_ip;
			if (limit_mask & DYN_DST_ADDR)
				id.dst_ip = args->f_id.dst_ip;
		}
		if (limit_mask & DYN_SRC_PORT)
			id.src_port = args->f_id.src_port;
		if (limit_mask & DYN_DST_PORT)
			id.dst_port = args->f_id.dst_port;
		if ((parent = lookup_dyn_parent(&id, rule)) == NULL) {
			printf("ipfw: %s: add parent failed\n", __func__);
			IPFW_DYN_UNLOCK();
			return (1);
		}

		if (parent->count >= conn_limit) {
			/* See if we can remove some expired rule. */
			remove_dyn_rule(rule, parent);
			if (parent->count >= conn_limit) {
				if (V_fw_verbose && last_log != time_uptime) {
					last_log = time_uptime;
#ifdef INET6
					/*
					 * XXX IPv6 flows are not
					 * supported yet.
					 */
					if (IS_IP6_FLOW_ID(&(args->f_id))) {
						char ip6buf[INET6_ADDRSTRLEN];
						snprintf(src, sizeof(src),
						    "[%s]", ip6_sprintf(ip6buf,
							&args->f_id.src_ip6));
						snprintf(dst, sizeof(dst),
						    "[%s]", ip6_sprintf(ip6buf,
							&args->f_id.dst_ip6));
					} else
#endif
					{
						da.s_addr =
						    htonl(args->f_id.src_ip);
						inet_ntoa_r(da, src);
						da.s_addr =
						    htonl(args->f_id.dst_ip);
						inet_ntoa_r(da, dst);
					}
					log(LOG_SECURITY | LOG_DEBUG,
					    "ipfw: %d %s %s:%u -> %s:%u, %s\n",
					    parent->rule->rulenum,
					    "drop session",
					    src, (args->f_id.src_port),
					    dst, (args->f_id.dst_port),
					    "too many entries");
				}
				IPFW_DYN_UNLOCK();
				return (1);
			}
		}
		add_dyn_rule(&args->f_id, O_LIMIT, (struct ip_fw *)parent);
		break;
	}
	default:
		printf("ipfw: %s: unknown dynamic rule type %u\n",
		    __func__, cmd->o.opcode);
		IPFW_DYN_UNLOCK();
		return (1);
	}

	/* XXX just set lifetime */
	lookup_dyn_rule_locked(&args->f_id, NULL, NULL);

	IPFW_DYN_UNLOCK();
	return (0);
}

/*
 * Generate a TCP packet, containing either a RST or a keepalive.
 * When flags & TH_RST, we are sending a RST packet, because of a
 * "reset" action matched the packet.
 * Otherwise we are sending a keepalive, and flags & TH_
 * The 'replyto' mbuf is the mbuf being replied to, if any, and is required
 * so that MAC can label the reply appropriately.
 */
struct mbuf *
ipfw_send_pkt(struct mbuf *replyto, struct ipfw_flow_id *id, u_int32_t seq,
    u_int32_t ack, int flags)
{
#ifndef __FreeBSD__
	return NULL;
#else
	struct mbuf *m;
	int len, dir;
	struct ip *h = NULL;		/* stupid compiler */
#ifdef INET6
	struct ip6_hdr *h6 = NULL;
#endif
	struct tcphdr *th = NULL;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return (NULL);

	M_SETFIB(m, id->fib);
#ifdef MAC
	if (replyto != NULL)
		mac_netinet_firewall_reply(replyto, m);
	else
		mac_netinet_firewall_send(m);
#else
	(void)replyto;		/* don't warn about unused arg */
#endif

	switch (id->addr_type) {
	case 4:
		len = sizeof(struct ip) + sizeof(struct tcphdr);
		break;
#ifdef INET6
	case 6:
		len = sizeof(struct ip6_hdr) + sizeof(struct tcphdr);
		break;
#endif
	default:
		/* XXX: log me?!? */
		FREE_PKT(m);
		return (NULL);
	}
	dir = ((flags & (TH_SYN | TH_RST)) == TH_SYN);

	m->m_data += max_linkhdr;
	m->m_flags |= M_SKIP_FIREWALL;
	m->m_pkthdr.len = m->m_len = len;
	m->m_pkthdr.rcvif = NULL;
	bzero(m->m_data, len);

	switch (id->addr_type) {
	case 4:
		h = mtod(m, struct ip *);

		/* prepare for checksum */
		h->ip_p = IPPROTO_TCP;
		h->ip_len = htons(sizeof(struct tcphdr));
		if (dir) {
			h->ip_src.s_addr = htonl(id->src_ip);
			h->ip_dst.s_addr = htonl(id->dst_ip);
		} else {
			h->ip_src.s_addr = htonl(id->dst_ip);
			h->ip_dst.s_addr = htonl(id->src_ip);
		}

		th = (struct tcphdr *)(h + 1);
		break;
#ifdef INET6
	case 6:
		h6 = mtod(m, struct ip6_hdr *);

		/* prepare for checksum */
		h6->ip6_nxt = IPPROTO_TCP;
		h6->ip6_plen = htons(sizeof(struct tcphdr));
		if (dir) {
			h6->ip6_src = id->src_ip6;
			h6->ip6_dst = id->dst_ip6;
		} else {
			h6->ip6_src = id->dst_ip6;
			h6->ip6_dst = id->src_ip6;
		}

		th = (struct tcphdr *)(h6 + 1);
		break;
#endif
	}

	if (dir) {
		th->th_sport = htons(id->src_port);
		th->th_dport = htons(id->dst_port);
	} else {
		th->th_sport = htons(id->dst_port);
		th->th_dport = htons(id->src_port);
	}
	th->th_off = sizeof(struct tcphdr) >> 2;

	if (flags & TH_RST) {
		if (flags & TH_ACK) {
			th->th_seq = htonl(ack);
			th->th_flags = TH_RST;
		} else {
			if (flags & TH_SYN)
				seq++;
			th->th_ack = htonl(seq);
			th->th_flags = TH_RST | TH_ACK;
		}
	} else {
		/*
		 * Keepalive - use caller provided sequence numbers
		 */
		th->th_seq = htonl(seq);
		th->th_ack = htonl(ack);
		th->th_flags = TH_ACK;
	}

	switch (id->addr_type) {
	case 4:
		th->th_sum = in_cksum(m, len);

		/* finish the ip header */
		h->ip_v = 4;
		h->ip_hl = sizeof(*h) >> 2;
		h->ip_tos = IPTOS_LOWDELAY;
		h->ip_off = 0;
		/* ip_len must be in host format for ip_output */
		h->ip_len = len;
		h->ip_ttl = V_ip_defttl;
		h->ip_sum = 0;
		break;
#ifdef INET6
	case 6:
		th->th_sum = in6_cksum(m, IPPROTO_TCP, sizeof(*h6),
		    sizeof(struct tcphdr));

		/* finish the ip6 header */
		h6->ip6_vfc |= IPV6_VERSION;
		h6->ip6_hlim = IPV6_DEFHLIM;
		break;
#endif
	}

	return (m);
#endif /* __FreeBSD__ */
}

/*
 * This procedure is only used to handle keepalives. It is invoked
 * every dyn_keepalive_period
 */
static void
ipfw_tick(void * vnetx) 
{
	struct mbuf *m0, *m, *mnext, **mtailp;
#ifdef INET6
	struct mbuf *m6, **m6_tailp;
#endif
	int i;
	ipfw_dyn_rule *q;
#ifdef VIMAGE
	struct vnet *vp = vnetx;
#endif

	CURVNET_SET(vp);
	if (V_dyn_keepalive == 0 || V_ipfw_dyn_v == NULL || V_dyn_count == 0)
		goto done;

	/*
	 * We make a chain of packets to go out here -- not deferring
	 * until after we drop the IPFW dynamic rule lock would result
	 * in a lock order reversal with the normal packet input -> ipfw
	 * call stack.
	 */
	m0 = NULL;
	mtailp = &m0;
#ifdef INET6
	m6 = NULL;
	m6_tailp = &m6;
#endif
	IPFW_DYN_LOCK();
	for (i = 0 ; i < V_curr_dyn_buckets ; i++) {
		for (q = V_ipfw_dyn_v[i] ; q ; q = q->next ) {
			if (q->dyn_type == O_LIMIT_PARENT)
				continue;
			if (q->id.proto != IPPROTO_TCP)
				continue;
			if ( (q->state & BOTH_SYN) != BOTH_SYN)
				continue;
			if (TIME_LEQ(time_uptime + V_dyn_keepalive_interval,
			    q->expire))
				continue;	/* too early */
			if (TIME_LEQ(q->expire, time_uptime))
				continue;	/* too late, rule expired */

			m = ipfw_send_pkt(NULL, &(q->id), q->ack_rev - 1,
				q->ack_fwd, TH_SYN);
			mnext = ipfw_send_pkt(NULL, &(q->id), q->ack_fwd - 1,
				q->ack_rev, 0);

			switch (q->id.addr_type) {
			case 4:
				if (m != NULL) {
					*mtailp = m;
					mtailp = &(*mtailp)->m_nextpkt;
				}
				if (mnext != NULL) {
					*mtailp = mnext;
					mtailp = &(*mtailp)->m_nextpkt;
				}
				break;
#ifdef INET6
			case 6:
				if (m != NULL) {
					*m6_tailp = m;
					m6_tailp = &(*m6_tailp)->m_nextpkt;
				}
				if (mnext != NULL) {
					*m6_tailp = mnext;
					m6_tailp = &(*m6_tailp)->m_nextpkt;
				}
				break;
#endif
			}

			m = mnext = NULL;
		}
	}
	IPFW_DYN_UNLOCK();
	for (m = mnext = m0; m != NULL; m = mnext) {
		mnext = m->m_nextpkt;
		m->m_nextpkt = NULL;
		ip_output(m, NULL, NULL, 0, NULL, NULL);
	}
#ifdef INET6
	for (m = mnext = m6; m != NULL; m = mnext) {
		mnext = m->m_nextpkt;
		m->m_nextpkt = NULL;
		ip6_output(m, NULL, NULL, 0, NULL, NULL, NULL);
	}
#endif
done:
	callout_reset(&V_ipfw_timeout, V_dyn_keepalive_period * hz,
		      ipfw_tick, vnetx);
	CURVNET_RESTORE();
}

void
ipfw_dyn_attach(void)
{
        ipfw_dyn_rule_zone = uma_zcreate("IPFW dynamic rule",
            sizeof(ipfw_dyn_rule), NULL, NULL, NULL, NULL,
            UMA_ALIGN_PTR, 0);

        IPFW_DYN_LOCK_INIT();
}

void
ipfw_dyn_detach(void)
{
        uma_zdestroy(ipfw_dyn_rule_zone);
        IPFW_DYN_LOCK_DESTROY();
}

void
ipfw_dyn_init(void)
{
        V_ipfw_dyn_v = NULL;
        V_dyn_buckets = 256;    /* must be power of 2 */
        V_curr_dyn_buckets = 256; /* must be power of 2 */
 
        V_dyn_ack_lifetime = 300;
        V_dyn_syn_lifetime = 20;
        V_dyn_fin_lifetime = 1;
        V_dyn_rst_lifetime = 1;
        V_dyn_udp_lifetime = 10;
        V_dyn_short_lifetime = 5;

        V_dyn_keepalive_interval = 20;
        V_dyn_keepalive_period = 5;
        V_dyn_keepalive = 1;    /* do send keepalives */
        
        V_dyn_max = 4096;       /* max # of dynamic rules */
        callout_init(&V_ipfw_timeout, CALLOUT_MPSAFE);
        callout_reset(&V_ipfw_timeout, hz, ipfw_tick, curvnet);
}

void
ipfw_dyn_uninit(int pass)
{
	if (pass == 0)
		callout_drain(&V_ipfw_timeout);
	else {
		if (V_ipfw_dyn_v != NULL)
			free(V_ipfw_dyn_v, M_IPFW);
	}
}

int
ipfw_dyn_len(void)
{
	return (V_ipfw_dyn_v == NULL) ? 0 :
		(V_dyn_count * sizeof(ipfw_dyn_rule));
}

void
ipfw_get_dynamic(char **pbp, const char *ep)
{
	ipfw_dyn_rule *p, *last = NULL;
	char *bp;
	int i;

	if (V_ipfw_dyn_v == NULL)
		return;
	bp = *pbp;

	IPFW_DYN_LOCK();
	for (i = 0 ; i < V_curr_dyn_buckets; i++)
		for (p = V_ipfw_dyn_v[i] ; p != NULL; p = p->next) {
			if (bp + sizeof *p <= ep) {
				ipfw_dyn_rule *dst =
					(ipfw_dyn_rule *)bp;
				bcopy(p, dst, sizeof *p);
				bcopy(&(p->rule->rulenum), &(dst->rule),
				    sizeof(p->rule->rulenum));
				/*
				 * store set number into high word of
				 * dst->rule pointer.
				 */
				bcopy(&(p->rule->set),
				    (char *)&dst->rule +
				    sizeof(p->rule->rulenum),
				    sizeof(p->rule->set));
				/*
				 * store a non-null value in "next".
				 * The userland code will interpret a
				 * NULL here as a marker
				 * for the last dynamic rule.
				 */
				bcopy(&dst, &dst->next, sizeof(dst));
				last = dst;
				dst->expire =
				    TIME_LEQ(dst->expire, time_uptime) ?
					0 : dst->expire - time_uptime ;
				bp += sizeof(ipfw_dyn_rule);
			}
		}
	IPFW_DYN_UNLOCK();
	if (last != NULL) /* mark last dynamic rule */
		bzero(&last->next, sizeof(last));
	*pbp = bp;
}
/* end of file */
