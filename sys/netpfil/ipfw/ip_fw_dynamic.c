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

#include "opt_ipfw.h"
#include "opt_inet.h"
#ifndef INET
#error IPFIREWALL requires INET.
#endif /* INET */
#include "opt_inet6.h"

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
#include <netinet/tcp_var.h>
#include <netinet/udp.h>

#include <netinet/ip6.h>	/* IN6_ARE_ADDR_EQUAL */
#ifdef INET6
#include <netinet6/in6_var.h>
#include <netinet6/ip6_var.h>
#endif

#include <netpfil/ipfw/ip_fw_private.h>

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
 * The total number of dynamic rules is equal to UMA zone items count.
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

struct ipfw_dyn_bucket {
	struct mtx	mtx;		/* Bucket protecting lock */
	ipfw_dyn_rule	*head;		/* Pointer to first rule */
};

/*
 * Static variables followed by global ones
 */
static VNET_DEFINE(struct ipfw_dyn_bucket *, ipfw_dyn_v);
static VNET_DEFINE(u_int32_t, dyn_buckets_max);
static VNET_DEFINE(u_int32_t, curr_dyn_buckets);
static VNET_DEFINE(struct callout, ipfw_timeout);
#define	V_ipfw_dyn_v			VNET(ipfw_dyn_v)
#define	V_dyn_buckets_max		VNET(dyn_buckets_max)
#define	V_curr_dyn_buckets		VNET(curr_dyn_buckets)
#define V_ipfw_timeout                  VNET(ipfw_timeout)

static VNET_DEFINE(uma_zone_t, ipfw_dyn_rule_zone);
#define	V_ipfw_dyn_rule_zone		VNET(ipfw_dyn_rule_zone)

#define	IPFW_BUCK_LOCK_INIT(b)	\
	mtx_init(&(b)->mtx, "IPFW dynamic bucket", NULL, MTX_DEF)
#define	IPFW_BUCK_LOCK_DESTROY(b)	\
	mtx_destroy(&(b)->mtx)
#define	IPFW_BUCK_LOCK(i)	mtx_lock(&V_ipfw_dyn_v[(i)].mtx)
#define	IPFW_BUCK_UNLOCK(i)	mtx_unlock(&V_ipfw_dyn_v[(i)].mtx)
#define	IPFW_BUCK_ASSERT(i)	mtx_assert(&V_ipfw_dyn_v[(i)].mtx, MA_OWNED)

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
static VNET_DEFINE(time_t, dyn_keepalive_last);

#define	V_dyn_keepalive_interval	VNET(dyn_keepalive_interval)
#define	V_dyn_keepalive_period		VNET(dyn_keepalive_period)
#define	V_dyn_keepalive			VNET(dyn_keepalive)
#define	V_dyn_keepalive_last		VNET(dyn_keepalive_last)

static VNET_DEFINE(u_int32_t, dyn_max);		/* max # of dynamic rules */

#define	DYN_COUNT			uma_zone_get_cur(V_ipfw_dyn_rule_zone)
#define	V_dyn_max			VNET(dyn_max)

static int last_log;	/* Log ratelimiting */

static void ipfw_dyn_tick(void *vnetx);
static void check_dyn_rules(struct ip_fw_chain *, struct ip_fw *,
    int, int, int);
#ifdef SYSCTL_NODE

static int sysctl_ipfw_dyn_count(SYSCTL_HANDLER_ARGS);
static int sysctl_ipfw_dyn_max(SYSCTL_HANDLER_ARGS);

SYSBEGIN(f2)

SYSCTL_DECL(_net_inet_ip_fw);
SYSCTL_VNET_UINT(_net_inet_ip_fw, OID_AUTO, dyn_buckets,
    CTLFLAG_RW, &VNET_NAME(dyn_buckets_max), 0,
    "Max number of dyn. buckets");
SYSCTL_VNET_UINT(_net_inet_ip_fw, OID_AUTO, curr_dyn_buckets,
    CTLFLAG_RD, &VNET_NAME(curr_dyn_buckets), 0,
    "Current Number of dyn. buckets");
SYSCTL_VNET_PROC(_net_inet_ip_fw, OID_AUTO, dyn_count,
    CTLTYPE_UINT|CTLFLAG_RD, 0, 0, sysctl_ipfw_dyn_count, "IU",
    "Number of dyn. rules");
SYSCTL_VNET_PROC(_net_inet_ip_fw, OID_AUTO, dyn_max,
    CTLTYPE_UINT|CTLFLAG_RW, 0, 0, sysctl_ipfw_dyn_max, "IU",
    "Max number of dyn. rules");
SYSCTL_VNET_UINT(_net_inet_ip_fw, OID_AUTO, dyn_ack_lifetime,
    CTLFLAG_RW, &VNET_NAME(dyn_ack_lifetime), 0,
    "Lifetime of dyn. rules for acks");
SYSCTL_VNET_UINT(_net_inet_ip_fw, OID_AUTO, dyn_syn_lifetime,
    CTLFLAG_RW, &VNET_NAME(dyn_syn_lifetime), 0,
    "Lifetime of dyn. rules for syn");
SYSCTL_VNET_UINT(_net_inet_ip_fw, OID_AUTO, dyn_fin_lifetime,
    CTLFLAG_RW, &VNET_NAME(dyn_fin_lifetime), 0,
    "Lifetime of dyn. rules for fin");
SYSCTL_VNET_UINT(_net_inet_ip_fw, OID_AUTO, dyn_rst_lifetime,
    CTLFLAG_RW, &VNET_NAME(dyn_rst_lifetime), 0,
    "Lifetime of dyn. rules for rst");
SYSCTL_VNET_UINT(_net_inet_ip_fw, OID_AUTO, dyn_udp_lifetime,
    CTLFLAG_RW, &VNET_NAME(dyn_udp_lifetime), 0,
    "Lifetime of dyn. rules for UDP");
SYSCTL_VNET_UINT(_net_inet_ip_fw, OID_AUTO, dyn_short_lifetime,
    CTLFLAG_RW, &VNET_NAME(dyn_short_lifetime), 0,
    "Lifetime of dyn. rules for other situations");
SYSCTL_VNET_UINT(_net_inet_ip_fw, OID_AUTO, dyn_keepalive,
    CTLFLAG_RW, &VNET_NAME(dyn_keepalive), 0,
    "Enable keepalives for dyn. rules");

SYSEND

#endif /* SYSCTL_NODE */


#ifdef INET6
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
#endif

/*
 * IMPORTANT: the hash function for dynamic rules must be commutative
 * in source and destination (ip,port), because rules are bidirectional
 * and we want to find both in the same bucket.
 */
static __inline int
hash_packet(struct ipfw_flow_id *id, int buckets)
{
	u_int32_t i;

#ifdef INET6
	if (IS_IP6_FLOW_ID(id)) 
		i = hash_packet6(id);
	else
#endif /* INET6 */
	i = (id->dst_ip) ^ (id->src_ip) ^ (id->dst_port) ^ (id->src_port);
	i &= (buckets - 1);
	return i;
}

/**
 * Print customizable flow id description via log(9) facility.
 */
static void
print_dyn_rule_flags(struct ipfw_flow_id *id, int dyn_type, int log_flags,
    char *prefix, char *postfix)
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
		inet_ntop(AF_INET, &da, src, sizeof(src));
		da.s_addr = htonl(id->dst_ip);
		inet_ntop(AF_INET, &da, dst, sizeof(dst));
	}
	log(log_flags, "ipfw: %s type %d %s %d -> %s %d, %d %s\n",
	    prefix, dyn_type, src, id->src_port, dst,
	    id->dst_port, DYN_COUNT, postfix);
}

#define	print_dyn_rule(id, dtype, prefix, postfix)	\
	print_dyn_rule_flags(id, dtype, LOG_DEBUG, prefix, postfix)

#define TIME_LEQ(a,b)       ((int)((a)-(b)) <= 0)

/*
 * Lookup a dynamic rule, locked version.
 */
static ipfw_dyn_rule *
lookup_dyn_rule_locked(struct ipfw_flow_id *pkt, int i, int *match_direction,
    struct tcphdr *tcp)
{
	/*
	 * Stateful ipfw extensions.
	 * Lookup into dynamic session queue.
	 */
#define MATCH_REVERSE	0
#define MATCH_FORWARD	1
#define MATCH_NONE	2
#define MATCH_UNKNOWN	3
	int dir = MATCH_NONE;
	ipfw_dyn_rule *prev, *q = NULL;

	IPFW_BUCK_ASSERT(i);

	for (prev = NULL, q = V_ipfw_dyn_v[i].head; q; prev = q, q = q->next) {
		if (q->dyn_type == O_LIMIT_PARENT && q->count)
			continue;

		if (pkt->proto != q->id.proto || q->dyn_type == O_LIMIT_PARENT)
			continue;

		if (IS_IP6_FLOW_ID(pkt)) {
			if (IN6_ARE_ADDR_EQUAL(&pkt->src_ip6, &q->id.src_ip6) &&
			    IN6_ARE_ADDR_EQUAL(&pkt->dst_ip6, &q->id.dst_ip6) &&
			    pkt->src_port == q->id.src_port &&
			    pkt->dst_port == q->id.dst_port) {
				dir = MATCH_FORWARD;
				break;
			}
			if (IN6_ARE_ADDR_EQUAL(&pkt->src_ip6, &q->id.dst_ip6) &&
			    IN6_ARE_ADDR_EQUAL(&pkt->dst_ip6, &q->id.src_ip6) &&
			    pkt->src_port == q->id.dst_port &&
			    pkt->dst_port == q->id.src_port) {
				dir = MATCH_REVERSE;
				break;
			}
		} else {
			if (pkt->src_ip == q->id.src_ip &&
			    pkt->dst_ip == q->id.dst_ip &&
			    pkt->src_port == q->id.src_port &&
			    pkt->dst_port == q->id.dst_port) {
				dir = MATCH_FORWARD;
				break;
			}
			if (pkt->src_ip == q->id.dst_ip &&
			    pkt->dst_ip == q->id.src_ip &&
			    pkt->src_port == q->id.dst_port &&
			    pkt->dst_port == q->id.src_port) {
				dir = MATCH_REVERSE;
				break;
			}
		}
	}
	if (q == NULL)
		goto done;	/* q = NULL, not found */

	if (prev != NULL) {	/* found and not in front */
		prev->next = q->next;
		q->next = V_ipfw_dyn_v[i].head;
		V_ipfw_dyn_v[i].head = q;
	}
	if (pkt->proto == IPPROTO_TCP) { /* update state according to flags */
		uint32_t ack;
		u_char flags = pkt->_flags & (TH_FIN | TH_SYN | TH_RST);

#define BOTH_SYN	(TH_SYN | (TH_SYN << 8))
#define BOTH_FIN	(TH_FIN | (TH_FIN << 8))
#define	TCP_FLAGS	(TH_FLAGS | (TH_FLAGS << 8))
#define	ACK_FWD		0x10000			/* fwd ack seen */
#define	ACK_REV		0x20000			/* rev ack seen */

		q->state |= (dir == MATCH_FORWARD) ? flags : (flags << 8);
		switch (q->state & TCP_FLAGS) {
		case TH_SYN:			/* opening */
			q->expire = time_uptime + V_dyn_syn_lifetime;
			break;

		case BOTH_SYN:			/* move to established */
		case BOTH_SYN | TH_FIN:		/* one side tries to close */
		case BOTH_SYN | (TH_FIN << 8):
#define _SEQ_GE(a,b) ((int)(a) - (int)(b) >= 0)
			if (tcp == NULL)
				break;

			ack = ntohl(tcp->th_ack);
			if (dir == MATCH_FORWARD) {
				if (q->ack_fwd == 0 ||
				    _SEQ_GE(ack, q->ack_fwd)) {
					q->ack_fwd = ack;
					q->state |= ACK_FWD;
				}
			} else {
				if (q->ack_rev == 0 ||
				    _SEQ_GE(ack, q->ack_rev)) {
					q->ack_rev = ack;
					q->state |= ACK_REV;
				}
			}
			if ((q->state & (ACK_FWD | ACK_REV)) ==
			    (ACK_FWD | ACK_REV)) {
				q->expire = time_uptime + V_dyn_ack_lifetime;
				q->state &= ~(ACK_FWD | ACK_REV);
			}
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
	if (match_direction != NULL)
		*match_direction = dir;
	return (q);
}

ipfw_dyn_rule *
ipfw_lookup_dyn_rule(struct ipfw_flow_id *pkt, int *match_direction,
    struct tcphdr *tcp)
{
	ipfw_dyn_rule *q;
	int i;

	i = hash_packet(pkt, V_curr_dyn_buckets);

	IPFW_BUCK_LOCK(i);
	q = lookup_dyn_rule_locked(pkt, i, match_direction, tcp);
	if (q == NULL)
		IPFW_BUCK_UNLOCK(i);
	/* NB: return table locked when q is not NULL */
	return q;
}

/*
 * Unlock bucket mtx
 * @p - pointer to dynamic rule
 */
void
ipfw_dyn_unlock(ipfw_dyn_rule *q)
{

	IPFW_BUCK_UNLOCK(q->bucket);
}

static int
resize_dynamic_table(struct ip_fw_chain *chain, int nbuckets)
{
	int i, k, nbuckets_old;
	ipfw_dyn_rule *q;
	struct ipfw_dyn_bucket *dyn_v, *dyn_v_old;

	/* Check if given number is power of 2 and less than 64k */
	if ((nbuckets > 65536) || (!powerof2(nbuckets)))
		return 1;

	CTR3(KTR_NET, "%s: resize dynamic hash: %d -> %d", __func__,
	    V_curr_dyn_buckets, nbuckets);

	/* Allocate and initialize new hash */
	dyn_v = malloc(nbuckets * sizeof(ipfw_dyn_rule), M_IPFW,
	    M_WAITOK | M_ZERO);

	for (i = 0 ; i < nbuckets; i++)
		IPFW_BUCK_LOCK_INIT(&dyn_v[i]);

	/*
	 * Call upper half lock, as get_map() do to ease
	 * read-only access to dynamic rules hash from sysctl
	 */
	IPFW_UH_WLOCK(chain);

	/*
	 * Acquire chain write lock to permit hash access
	 * for main traffic path without additional locks
	 */
	IPFW_WLOCK(chain);

	/* Save old values */
	nbuckets_old = V_curr_dyn_buckets;
	dyn_v_old = V_ipfw_dyn_v;

	/* Skip relinking if array is not set up */
	if (V_ipfw_dyn_v == NULL)
		V_curr_dyn_buckets = 0;

	/* Re-link all dynamic states */
	for (i = 0 ; i < V_curr_dyn_buckets ; i++) {
		while (V_ipfw_dyn_v[i].head != NULL) {
			/* Remove from current chain */
			q = V_ipfw_dyn_v[i].head;
			V_ipfw_dyn_v[i].head = q->next;

			/* Get new hash value */
			k = hash_packet(&q->id, nbuckets);
			q->bucket = k;
			/* Add to the new head */
			q->next = dyn_v[k].head;
			dyn_v[k].head = q;
             }
	}

	/* Update current pointers/buckets values */
	V_curr_dyn_buckets = nbuckets;
	V_ipfw_dyn_v = dyn_v;

	IPFW_WUNLOCK(chain);

	IPFW_UH_WUNLOCK(chain);

	/* Start periodic callout on initial creation */
	if (dyn_v_old == NULL) {
        	callout_reset_on(&V_ipfw_timeout, hz, ipfw_dyn_tick, curvnet, 0);
		return (0);
	}

	/* Destroy all mutexes */
	for (i = 0 ; i < nbuckets_old ; i++)
		IPFW_BUCK_LOCK_DESTROY(&dyn_v_old[i]);

	/* Free old hash */
	free(dyn_v_old, M_IPFW);

	return 0;
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
add_dyn_rule(struct ipfw_flow_id *id, int i, u_int8_t dyn_type, struct ip_fw *rule)
{
	ipfw_dyn_rule *r;

	IPFW_BUCK_ASSERT(i);

	r = uma_zalloc(V_ipfw_dyn_rule_zone, M_NOWAIT | M_ZERO);
	if (r == NULL) {
		if (last_log != time_uptime) {
			last_log = time_uptime;
			log(LOG_DEBUG, "ipfw: %s: Cannot allocate rule\n",
			    __func__);
		}
		return NULL;
	}

	/*
	 * refcount on parent is already incremented, so
	 * it is safe to use parent unlocked.
	 */
	if (dyn_type == O_LIMIT) {
		ipfw_dyn_rule *parent = (ipfw_dyn_rule *)rule;
		if ( parent->dyn_type != O_LIMIT_PARENT)
			panic("invalid parent");
		r->parent = parent;
		rule = parent->rule;
	}

	r->id = *id;
	r->expire = time_uptime + V_dyn_syn_lifetime;
	r->rule = rule;
	r->dyn_type = dyn_type;
	IPFW_ZERO_DYN_COUNTER(r);
	r->count = 0;

	r->bucket = i;
	r->next = V_ipfw_dyn_v[i].head;
	V_ipfw_dyn_v[i].head = r;
	DEB(print_dyn_rule(id, dyn_type, "add dyn entry", "total");)
	return r;
}

/**
 * lookup dynamic parent rule using pkt and rule as search keys.
 * If the lookup fails, then install one.
 */
static ipfw_dyn_rule *
lookup_dyn_parent(struct ipfw_flow_id *pkt, int *pindex, struct ip_fw *rule)
{
	ipfw_dyn_rule *q;
	int i, is_v6;

	is_v6 = IS_IP6_FLOW_ID(pkt);
	i = hash_packet( pkt, V_curr_dyn_buckets );
	*pindex = i;
	IPFW_BUCK_LOCK(i);
	for (q = V_ipfw_dyn_v[i].head ; q != NULL ; q=q->next)
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
			DEB(print_dyn_rule(pkt, q->dyn_type,
			    "lookup_dyn_parent found", "");)
			return q;
		}

	/* Add virtual limiting rule */
	return add_dyn_rule(pkt, i, O_LIMIT_PARENT, rule);
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
	ipfw_dyn_rule *q;
	int i;

	DEB(print_dyn_rule(&args->f_id, cmd->o.opcode, "install_state", "");)
	
	i = hash_packet(&args->f_id, V_curr_dyn_buckets);

	IPFW_BUCK_LOCK(i);

	q = lookup_dyn_rule_locked(&args->f_id, i, NULL, NULL);

	if (q != NULL) {	/* should never occur */
		DEB(
		if (last_log != time_uptime) {
			last_log = time_uptime;
			printf("ipfw: %s: entry already present, done\n",
			    __func__);
		})
		IPFW_BUCK_UNLOCK(i);
		return (0);
	}

	/*
	 * State limiting is done via uma(9) zone limiting.
	 * Save pointer to newly-installed rule and reject
	 * packet if add_dyn_rule() returned NULL.
	 * Note q is currently set to NULL.
	 */

	switch (cmd->o.opcode) {
	case O_KEEP_STATE:	/* bidir rule */
		q = add_dyn_rule(&args->f_id, i, O_KEEP_STATE, rule);
		break;

	case O_LIMIT: {		/* limit number of sessions */
		struct ipfw_flow_id id;
		ipfw_dyn_rule *parent;
		uint32_t conn_limit;
		uint16_t limit_mask = cmd->limit_mask;
		int pindex;

		conn_limit = IP_FW_ARG_TABLEARG(cmd->conn_limit);
		  
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
			bzero(&id.src_ip6, sizeof(id.src_ip6));
			bzero(&id.dst_ip6, sizeof(id.dst_ip6));

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

		/*
		 * We have to release lock for previous bucket to
		 * avoid possible deadlock
		 */
		IPFW_BUCK_UNLOCK(i);

		if ((parent = lookup_dyn_parent(&id, &pindex, rule)) == NULL) {
			printf("ipfw: %s: add parent failed\n", __func__);
			IPFW_BUCK_UNLOCK(pindex);
			return (1);
		}

		if (parent->count >= conn_limit) {
			if (V_fw_verbose && last_log != time_uptime) {
				last_log = time_uptime;
				char sbuf[24];
				last_log = time_uptime;
				snprintf(sbuf, sizeof(sbuf),
				    "%d drop session",
				    parent->rule->rulenum);
				print_dyn_rule_flags(&args->f_id,
				    cmd->o.opcode,
				    LOG_SECURITY | LOG_DEBUG,
				    sbuf, "too many entries");
			}
			IPFW_BUCK_UNLOCK(pindex);
			return (1);
		}
		/* Increment counter on parent */
		parent->count++;
		IPFW_BUCK_UNLOCK(pindex);

		IPFW_BUCK_LOCK(i);
		q = add_dyn_rule(&args->f_id, i, O_LIMIT, (struct ip_fw *)parent);
		if (q == NULL) {
			/* Decrement index and notify caller */
			IPFW_BUCK_UNLOCK(i);
			IPFW_BUCK_LOCK(pindex);
			parent->count--;
			IPFW_BUCK_UNLOCK(pindex);
			return (1);
		}
		break;
	}
	default:
		printf("ipfw: %s: unknown dynamic rule type %u\n",
		    __func__, cmd->o.opcode);
	}

	if (q == NULL) {
		IPFW_BUCK_UNLOCK(i);
		return (1);	/* Notify caller about failure */
	}

	/* XXX just set lifetime */
	lookup_dyn_rule_locked(&args->f_id, i, NULL, NULL);

	IPFW_BUCK_UNLOCK(i);
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
	struct mbuf *m = NULL;		/* stupid compiler */
	int len, dir;
	struct ip *h = NULL;		/* stupid compiler */
#ifdef INET6
	struct ip6_hdr *h6 = NULL;
#endif
	struct tcphdr *th = NULL;

	MGETHDR(m, M_NOWAIT, MT_DATA);
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
		h->ip_off = htons(0);
		h->ip_len = htons(len);
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
}

/*
 * Queue keepalive packets for given dynamic rule
 */
static struct mbuf **
ipfw_dyn_send_ka(struct mbuf **mtailp, ipfw_dyn_rule *q)
{
	struct mbuf *m_rev, *m_fwd;

	m_rev = (q->state & ACK_REV) ? NULL :
	    ipfw_send_pkt(NULL, &(q->id), q->ack_rev - 1, q->ack_fwd, TH_SYN);
	m_fwd = (q->state & ACK_FWD) ? NULL :
	    ipfw_send_pkt(NULL, &(q->id), q->ack_fwd - 1, q->ack_rev, 0);

	if (m_rev != NULL) {
		*mtailp = m_rev;
		mtailp = &(*mtailp)->m_nextpkt;
	}
	if (m_fwd != NULL) {
		*mtailp = m_fwd;
		mtailp = &(*mtailp)->m_nextpkt;
	}

	return (mtailp);
}

/*
 * This procedure is used to perform various maintance
 * on dynamic hash list. Currently it is called every second.
 */
static void
ipfw_dyn_tick(void * vnetx) 
{
	struct ip_fw_chain *chain;
	int check_ka = 0;
#ifdef VIMAGE
	struct vnet *vp = vnetx;
#endif

	CURVNET_SET(vp);

	chain = &V_layer3_chain;

	/* Run keepalive checks every keepalive_period iff ka is enabled */
	if ((V_dyn_keepalive_last + V_dyn_keepalive_period <= time_uptime) &&
	    (V_dyn_keepalive != 0)) {
		V_dyn_keepalive_last = time_uptime;
		check_ka = 1;
	}

	check_dyn_rules(chain, NULL, RESVD_SET, check_ka, 1);

	callout_reset_on(&V_ipfw_timeout, hz, ipfw_dyn_tick, vnetx, 0);

	CURVNET_RESTORE();
}


/*
 * Walk thru all dynamic states doing generic maintance:
 * 1) free expired states
 * 2) free all states based on deleted rule / set
 * 3) send keepalives for states if needed
 *
 * @chain - pointer to current ipfw rules chain
 * @rule - delete all states originated by given rule if != NULL
 * @set - delete all states originated by any rule in set @set if != RESVD_SET
 * @check_ka - perform checking/sending keepalives
 * @timer - indicate call from timer routine.
 *
 * Timer routine must call this function unlocked to permit
 * sending keepalives/resizing table.
 *
 * Others has to call function with IPFW_UH_WLOCK held.
 * Additionally, function assume that dynamic rule/set is
 * ALREADY deleted so no new states can be generated by
 * 'deleted' rules.
 *
 * Write lock is needed to ensure that unused parent rules
 * are not freed by other instance (see stage 2, 3)
 */
static void
check_dyn_rules(struct ip_fw_chain *chain, struct ip_fw *rule,
    int set, int check_ka, int timer)
{
	struct mbuf *m0, *m, *mnext, **mtailp;
	struct ip *h;
	int i, dyn_count, new_buckets = 0, max_buckets;
	int expired = 0, expired_limits = 0, parents = 0, total = 0;
	ipfw_dyn_rule *q, *q_prev, *q_next;
	ipfw_dyn_rule *exp_head, **exptailp;
	ipfw_dyn_rule *exp_lhead, **expltailp;

	KASSERT(V_ipfw_dyn_v != NULL, ("%s: dynamic table not allocated",
	    __func__));

	/* Avoid possible LOR */
	KASSERT(!check_ka || timer, ("%s: keepalive check with lock held",
	    __func__));

	/*
	 * Do not perform any checks if we currently have no dynamic states
	 */
	if (DYN_COUNT == 0)
		return;

	/* Expired states */
	exp_head = NULL;
	exptailp = &exp_head;

	/* Expired limit states */
	exp_lhead = NULL;
	expltailp = &exp_lhead;

	/*
	 * We make a chain of packets to go out here -- not deferring
	 * until after we drop the IPFW dynamic rule lock would result
	 * in a lock order reversal with the normal packet input -> ipfw
	 * call stack.
	 */
	m0 = NULL;
	mtailp = &m0;

	/* Protect from hash resizing */
	if (timer != 0)
		IPFW_UH_WLOCK(chain);
	else
		IPFW_UH_WLOCK_ASSERT(chain);

#define	NEXT_RULE()	{ q_prev = q; q = q->next ; continue; }

	/* Stage 1: perform requested deletion */
	for (i = 0 ; i < V_curr_dyn_buckets ; i++) {
		IPFW_BUCK_LOCK(i);
		for (q = V_ipfw_dyn_v[i].head, q_prev = q; q ; ) {
			/* account every rule */
			total++;

			/* Skip parent rules at all */
			if (q->dyn_type == O_LIMIT_PARENT) {
				parents++;
				NEXT_RULE();
			}

			/*
			 * Remove rules which are:
			 * 1) expired
			 * 2) created by given rule
			 * 3) created by any rule in given set
			 */
			if ((TIME_LEQ(q->expire, time_uptime)) ||
			    ((rule != NULL) && (q->rule == rule)) ||
			    ((set != RESVD_SET) && (q->rule->set == set))) {
				/* Unlink q from current list */
				q_next = q->next;
				if (q == V_ipfw_dyn_v[i].head)
					V_ipfw_dyn_v[i].head = q_next;
				else
					q_prev->next = q_next;

				q->next = NULL;

				/* queue q to expire list */
				if (q->dyn_type != O_LIMIT) {
					*exptailp = q;
					exptailp = &(*exptailp)->next;
					DEB(print_dyn_rule(&q->id, q->dyn_type,
					    "unlink entry", "left");
					)
				} else {
					/* Separate list for limit rules */
					*expltailp = q;
					expltailp = &(*expltailp)->next;
					expired_limits++;
					DEB(print_dyn_rule(&q->id, q->dyn_type,
					    "unlink limit entry", "left");
					)
				}

				q = q_next;
				expired++;
				continue;
			}

			/*
			 * Check if we need to send keepalive:
			 * we need to ensure if is time to do KA,
			 * this is established TCP session, and
			 * expire time is within keepalive interval
			 */
			if ((check_ka != 0) && (q->id.proto == IPPROTO_TCP) &&
			    ((q->state & BOTH_SYN) == BOTH_SYN) &&
			    (TIME_LEQ(q->expire, time_uptime +
			      V_dyn_keepalive_interval)))
				mtailp = ipfw_dyn_send_ka(mtailp, q);

			NEXT_RULE();
		}
		IPFW_BUCK_UNLOCK(i);
	}

	/* Stage 2: decrement counters from O_LIMIT parents */
	if (expired_limits != 0) {
		/*
		 * XXX: Note that deleting set with more than one
		 * heavily-used LIMIT rules can result in overwhelming
		 * locking due to lack of per-hash value sorting
		 *
		 * We should probably think about:
		 * 1) pre-allocating hash of size, say,
		 * MAX(16, V_curr_dyn_buckets / 1024)
		 * 2) checking if expired_limits is large enough
		 * 3) If yes, init hash (or its part), re-link
		 * current list and start decrementing procedure in
		 * each bucket separately
		 */

		/*
		 * Small optimization: do not unlock bucket until
		 * we see the next item resides in different bucket
		 */
		if (exp_lhead != NULL) {
			i = exp_lhead->parent->bucket;
			IPFW_BUCK_LOCK(i);
		}
		for (q = exp_lhead; q != NULL; q = q->next) {
			if (i != q->parent->bucket) {
				IPFW_BUCK_UNLOCK(i);
				i = q->parent->bucket;
				IPFW_BUCK_LOCK(i);
			}

			/* Decrease parent refcount */
			q->parent->count--;
		}
		if (exp_lhead != NULL)
			IPFW_BUCK_UNLOCK(i);
	}

	/*
	 * We protectet ourselves from unused parent deletion
	 * (from the timer function) by holding UH write lock.
	 */

	/* Stage 3: remove unused parent rules */
	if ((parents != 0) && (expired != 0)) {
		for (i = 0 ; i < V_curr_dyn_buckets ; i++) {
			IPFW_BUCK_LOCK(i);
			for (q = V_ipfw_dyn_v[i].head, q_prev = q ; q ; ) {
				if (q->dyn_type != O_LIMIT_PARENT)
					NEXT_RULE();

				if (q->count != 0)
					NEXT_RULE();

				/* Parent rule without consumers */

				/* Unlink q from current list */
				q_next = q->next;
				if (q == V_ipfw_dyn_v[i].head)
					V_ipfw_dyn_v[i].head = q_next;
				else
					q_prev->next = q_next;

				q->next = NULL;

				/* Add to expired list */
				*exptailp = q;
				exptailp = &(*exptailp)->next;

				DEB(print_dyn_rule(&q->id, q->dyn_type,
				    "unlink parent entry", "left");
				)

				expired++;

				q = q_next;
			}
			IPFW_BUCK_UNLOCK(i);
		}
	}

#undef NEXT_RULE

	if (timer != 0) {
		/*
		 * Check if we need to resize hash:
		 * if current number of states exceeds number of buckes in hash,
		 * grow hash size to the minimum power of 2 which is bigger than
		 * current states count. Limit hash size by 64k.
		 */
		max_buckets = (V_dyn_buckets_max > 65536) ?
		    65536 : V_dyn_buckets_max;
	
		dyn_count = DYN_COUNT;
	
		if ((dyn_count > V_curr_dyn_buckets * 2) &&
		    (dyn_count < max_buckets)) {
			new_buckets = V_curr_dyn_buckets;
			while (new_buckets < dyn_count) {
				new_buckets *= 2;
	
				if (new_buckets >= max_buckets)
					break;
			}
		}

		IPFW_UH_WUNLOCK(chain);
	}

	/* Finally delete old states ad limits if any */
	for (q = exp_head; q != NULL; q = q_next) {
		q_next = q->next;
		uma_zfree(V_ipfw_dyn_rule_zone, q);
	}

	for (q = exp_lhead; q != NULL; q = q_next) {
		q_next = q->next;
		uma_zfree(V_ipfw_dyn_rule_zone, q);
	}

	/*
	 * The rest code MUST be called from timer routine only
	 * without holding any locks
	 */
	if (timer == 0)
		return;

	/* Send keepalive packets if any */
	for (m = m0; m != NULL; m = mnext) {
		mnext = m->m_nextpkt;
		m->m_nextpkt = NULL;
		h = mtod(m, struct ip *);
		if (h->ip_v == 4)
			ip_output(m, NULL, NULL, 0, NULL, NULL);
#ifdef INET6
		else
			ip6_output(m, NULL, NULL, 0, NULL, NULL, NULL);
#endif
	}

	/* Run table resize without holding any locks */
	if (new_buckets != 0)
		resize_dynamic_table(chain, new_buckets);
}

/*
 * Deletes all dynamic rules originated by given rule or all rules in
 * given set. Specify RESVD_SET to indicate set should not be used.
 * @chain - pointer to current ipfw rules chain
 * @rule - delete all states originated by given rule if != NULL
 * @set - delete all states originated by any rule in set @set if != RESVD_SET
 *
 * Function has to be called with IPFW_UH_WLOCK held.
 * Additionally, function assume that dynamic rule/set is
 * ALREADY deleted so no new states can be generated by
 * 'deleted' rules.
 */
void
ipfw_expire_dyn_rules(struct ip_fw_chain *chain, struct ip_fw *rule, int set)
{

	check_dyn_rules(chain, rule, set, 0, 0);
}

void
ipfw_dyn_init(struct ip_fw_chain *chain)
{

        V_ipfw_dyn_v = NULL;
        V_dyn_buckets_max = 256; /* must be power of 2 */
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
	V_dyn_keepalive_last = time_uptime;
        
        V_dyn_max = 4096;       /* max # of dynamic rules */

	V_ipfw_dyn_rule_zone = uma_zcreate("IPFW dynamic rule",
	    sizeof(ipfw_dyn_rule), NULL, NULL, NULL, NULL,
	    UMA_ALIGN_PTR, 0);

	/* Enforce limit on dynamic rules */
	uma_zone_set_max(V_ipfw_dyn_rule_zone, V_dyn_max);

        callout_init(&V_ipfw_timeout, CALLOUT_MPSAFE);

	/*
	 * This can potentially be done on first dynamic rule
	 * being added to chain.
	 */
	resize_dynamic_table(chain, V_curr_dyn_buckets);
}

void
ipfw_dyn_uninit(int pass)
{
	int i;

	if (pass == 0) {
		callout_drain(&V_ipfw_timeout);
		return;
	}

	if (V_ipfw_dyn_v != NULL) {
		/*
		 * Skip deleting all dynamic states -
		 * uma_zdestroy() does this more efficiently;
		 */

		/* Destroy all mutexes */
		for (i = 0 ; i < V_curr_dyn_buckets ; i++)
			IPFW_BUCK_LOCK_DESTROY(&V_ipfw_dyn_v[i]);
		free(V_ipfw_dyn_v, M_IPFW);
		V_ipfw_dyn_v = NULL;
	}

        uma_zdestroy(V_ipfw_dyn_rule_zone);
}

#ifdef SYSCTL_NODE
/*
 * Get/set maximum number of dynamic states in given VNET instance.
 */
static int
sysctl_ipfw_dyn_max(SYSCTL_HANDLER_ARGS)
{
	int error;
	unsigned int nstates;

	nstates = V_dyn_max;

	error = sysctl_handle_int(oidp, &nstates, 0, req);
	/* Read operation or some error */
	if ((error != 0) || (req->newptr == NULL))
		return (error);

	V_dyn_max = nstates;
	uma_zone_set_max(V_ipfw_dyn_rule_zone, V_dyn_max);

	return (0);
}

/*
 * Get current number of dynamic states in given VNET instance.
 */
static int
sysctl_ipfw_dyn_count(SYSCTL_HANDLER_ARGS)
{
	int error;
	unsigned int nstates;

	nstates = DYN_COUNT;

	error = sysctl_handle_int(oidp, &nstates, 0, req);

	return (error);
}
#endif

/*
 * Returns number of dynamic rules.
 */
int
ipfw_dyn_len(void)
{

	return (V_ipfw_dyn_v == NULL) ? 0 :
		(DYN_COUNT * sizeof(ipfw_dyn_rule));
}

/*
 * Fill given buffer with dynamic states.
 * IPFW_UH_RLOCK has to be held while calling.
 */
void
ipfw_get_dynamic(struct ip_fw_chain *chain, char **pbp, const char *ep)
{
	ipfw_dyn_rule *p, *last = NULL;
	char *bp;
	int i;

	if (V_ipfw_dyn_v == NULL)
		return;
	bp = *pbp;

	IPFW_UH_RLOCK_ASSERT(chain);

	for (i = 0 ; i < V_curr_dyn_buckets; i++) {
		IPFW_BUCK_LOCK(i);
		for (p = V_ipfw_dyn_v[i].head ; p != NULL; p = p->next) {
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
		IPFW_BUCK_UNLOCK(i);
	}

	if (last != NULL) /* mark last dynamic rule */
		bzero(&last->next, sizeof(last));
	*pbp = bp;
}
/* end of file */
