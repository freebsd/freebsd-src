/*-
 * Copyright (c) 2010-2011
 * 	Swinburne University of Technology, Melbourne, Australia.
 * All rights reserved.
 *
 * This software was developed at the Centre for Advanced Internet
 * Architectures, Swinburne University of Technology, by Sebastian Zander, made
 * possible in part by a gift from The Cisco University Research Program Fund, a
 * corporate advised fund of Silicon Valley Community Foundation.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Functions to manage the flow table (originally based on ip_fw_dynamic.c).
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#ifdef _KERNEL
#if !defined(KLD_MODULE)
#include "opt_ipfw.h"
#include "opt_inet.h"
#include "opt_inet6.h"
#ifndef INET
#error DIFFUSE requires INET.
#endif
#endif

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#endif /* _KERNEL */
#include <sys/socket.h>

#include <net/if.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/ip_fw.h>
#include <netinet/ip_diffuse.h>
#include <netinet/ip_diffuse_export.h>
#include <netinet/tcp.h>
#include <netinet/tcp_var.h>
#include <netinet/udp.h>
#include <netinet/ip6.h>

#ifdef INET6
#include <netinet6/in6_var.h>
#include <netinet6/ip6_var.h>
#endif

#include <netinet/ipfw/diffuse_common.h>
#include <netinet/ipfw/diffuse_feature.h>
#ifdef _KERNEL
#include <netinet/ipfw/diffuse_private.h>
#include <netinet/ipfw/ip_fw_private.h>
#endif

#ifdef _KERNEL
#ifdef MAC
#include <security/mac/mac_framework.h>
#endif
#endif

#ifndef _KERNEL
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>

#define	KPI_USER_COMPAT
#include <netinet/ipfw/diffuse_user_compat.h> /* Must come after stdlib.h */
#endif

/*
 * Description of flow table.
 *
 * Flow features are stored in lists accessed through a hash table
 * (diffuse_ft_v) whose size is ft_curr_buckets. This value can be modified
 * through the sysctl variable ft_buckets which is updated when the table
 * becomes empty.
 *
 * When a packet is received, its address fields are first masked with the mask
 * defined for the rule, then hashed, then matched against the entries in the
 * corresponding list. Then all the features configured are computed.
 *
 * The lifetime of flow entries is regulated by ft_*_lifetime, measured in
 * seconds and depending on the TCP flags.
 *
 * The total number of flows is stored in ft_count. The max number of flows is
 * ft_max. We stop creating flow table entries when ft_max is reached. This is
 * done to avoid consuming too much memory, but also too much time when
 * searching on each packet (we should try instead to put a limit on the length
 * of the list on each bucket...).
 *
 * Each flow entry holds a pointer to the creating ipfw rule. Flows are removed
 * when the rule is removed.
 */

static VNET_DEFINE(struct di_ft_entry **, diffuse_ft_v);
static VNET_DEFINE(u_int32_t, ft_buckets);
static VNET_DEFINE(u_int32_t, ft_curr_buckets);
#define	V_diffuse_ft_v		VNET(diffuse_ft_v)
#define	V_ft_buckets		VNET(ft_buckets)
#define	V_ft_curr_buckets	VNET(ft_curr_buckets)

#ifdef _KERNEL

LIST_HEAD(di_to_head, di_to_entry);

static VNET_DEFINE(struct di_to_head *, diffuse_to_v);
static VNET_DEFINE(u_int32_t, to_buckets);
static VNET_DEFINE(u_int32_t, to_curr_buckets);
#define	V_diffuse_to_v		VNET(diffuse_to_v)
#define	V_to_buckets		VNET(to_buckets)
#define	V_to_curr_buckets	VNET(to_curr_buckets)

static uma_zone_t di_ft_zone;

#ifndef __FreeBSD__
DEFINE_SPINLOCK(di_ft_mtx);
#else
static struct rwlock di_ft_mtx;
#endif

#define	DI_FT_LOCK_INIT()	rw_init(&di_ft_mtx, "DIFFUSE flow table")
#define	DI_FT_LOCK_DESTROY()	rw_destroy(&di_ft_mtx)
#define	DI_FT_RLOCK()		rw_rlock(&di_ft_mtx)
#define	DI_FT_WLOCK()		rw_wlock(&di_ft_mtx)
#define	DI_FT_UNLOCK()		rw_unlock(&di_ft_mtx)
#define	DI_FT_RLOCK_ASSERT()	rw_assert(&di_ft_mtx, RA_RLOCKED)
#define	DI_FT_WLOCK_ASSERT()	rw_assert(&di_ft_mtx, RA_WLOCKED)
#define	DI_FT_LOCK_ASSERT()	rw_assert(&di_ft_mtx, RA_LOCKED)

#endif /* _KERNEL */

/* Timeouts for ending flows. */
static VNET_DEFINE(u_int32_t, ft_ack_lifetime);
static VNET_DEFINE(u_int32_t, ft_syn_lifetime);
static VNET_DEFINE(u_int32_t, ft_fin_lifetime);
static VNET_DEFINE(u_int32_t, ft_rst_lifetime);
static VNET_DEFINE(u_int32_t, ft_udp_lifetime);
static VNET_DEFINE(u_int32_t, ft_short_lifetime);
#define	V_ft_ack_lifetime	VNET(ft_ack_lifetime)
#define	V_ft_syn_lifetime	VNET(ft_syn_lifetime)
#define	V_ft_fin_lifetime	VNET(ft_fin_lifetime)
#define	V_ft_rst_lifetime	VNET(ft_rst_lifetime)
#define	V_ft_udp_lifetime	VNET(ft_udp_lifetime)
#define	V_ft_short_lifetime	VNET(ft_short_lifetime)

static VNET_DEFINE(u_int32_t, ft_count);	/* # of flows. */
static VNET_DEFINE(u_int32_t, ft_max);		/* Max # of flows. */
#define	V_ft_count		VNET(ft_count)
#define	V_ft_max		VNET(ft_max)

#ifdef _KERNEL
#ifdef SYSCTL_NODE
SYSBEGIN(xxx)
SYSCTL_DECL(_net_inet_ip_diffuse);
SYSCTL_VNET_UINT(_net_inet_ip_diffuse, OID_AUTO, ft_buckets,
    CTLFLAG_RW, &VNET_NAME(ft_buckets), 0,
    "Number of buckets");
SYSCTL_VNET_UINT(_net_inet_ip_diffuse, OID_AUTO, ft_curr_buckets,
    CTLFLAG_RD, &VNET_NAME(ft_curr_buckets), 0,
    "Current Number of buckets");
SYSCTL_VNET_UINT(_net_inet_ip_diffuse, OID_AUTO, ft_count,
    CTLFLAG_RD, &VNET_NAME(ft_count), 0,
    "Number of entries");
SYSCTL_VNET_UINT(_net_inet_ip_diffuse, OID_AUTO, ft_max,
    CTLFLAG_RW, &VNET_NAME(ft_max), 0,
    "Max number of entries");
SYSCTL_VNET_UINT(_net_inet_ip_diffuse, OID_AUTO, ft_ack_lifetime,
    CTLFLAG_RW, &VNET_NAME(ft_ack_lifetime), 0,
    "Lifetime of entries for acks");
SYSCTL_VNET_UINT(_net_inet_ip_diffuse, OID_AUTO, ft_syn_lifetime,
    CTLFLAG_RW, &VNET_NAME(ft_syn_lifetime), 0,
    "Lifetime of entries for syn");
SYSCTL_VNET_UINT(_net_inet_ip_diffuse, OID_AUTO, ft_fin_lifetime,
    CTLFLAG_RW, &VNET_NAME(ft_fin_lifetime), 0,
    "Lifetime of entries for fin");
SYSCTL_VNET_UINT(_net_inet_ip_diffuse, OID_AUTO, ft_rst_lifetime,
    CTLFLAG_RW, &VNET_NAME(ft_rst_lifetime), 0,
    "Lifetime of entries for rst");
SYSCTL_VNET_UINT(_net_inet_ip_diffuse, OID_AUTO, ft_udp_lifetime,
    CTLFLAG_RW, &VNET_NAME(ft_udp_lifetime), 0,
    "Lifetime of entries for UDP");
SYSCTL_VNET_UINT(_net_inet_ip_diffuse, OID_AUTO, ft_short_lifetime,
    CTLFLAG_RW, &VNET_NAME(ft_short_lifetime), 0,
    "Lifetime of entries for other situations");
SYSCTL_VNET_UINT(_net_inet_ip_diffuse, OID_AUTO, to_buckets,
    CTLFLAG_RW, &VNET_NAME(to_buckets), 0,
    "Number of timeout buckets");
SYSCTL_VNET_UINT(_net_inet_ip_diffuse, OID_AUTO, to_curr_buckets,
    CTLFLAG_RD, &VNET_NAME(to_curr_buckets), 0,
    "Current Number of timeout buckets");
SYSEND
#endif /* SYSCTL_NODE */
#else /* _KERNEL */
       /* Only for userspace (ipfw_fstats). */
       uint64_t id_cnt = 0;
#endif /* _KERNEL */

#ifdef DIFFUSE_DEBUG
/* Wrapper so this works in kernel and userspace. */
static char *
_inet_ntoa_r(struct in_addr in, char *buf, int len)
{
#ifdef _KERNEL
	return (inet_ntoa_r(in, buf));
#else
	return (inet_ntoa_r(in, buf, len));
#endif
}
#endif

/*
 * XXX: Maybe use a better hash function? Doesn't need to be commutative,
 * can do two lookups.
 */
static inline int
hash_packet6(struct ipfw_flow_id *id)
{
	uint32_t i;

	i = (id->dst_ip6.__u6_addr.__u6_addr32[2]) ^
	    (id->dst_ip6.__u6_addr.__u6_addr32[3]) ^
	    (id->src_ip6.__u6_addr.__u6_addr32[2]) ^
	    (id->src_ip6.__u6_addr.__u6_addr32[3]) ^
	    (id->dst_port) ^ (id->src_port);

	return (i);
}

/*
 * IMPORTANT: the hash function for dynamic rules must be commutative
 * in source and destination (ip,port), because rules are bidirectional
 * and we want to find both in the same bucket.
 */
static inline int
hash_packet(struct ipfw_flow_id *id)
{
	uint32_t i;

#ifdef INET6
	if (IS_IP6_FLOW_ID(id))
		i = hash_packet6(id);
	else
#endif /* INET6 */
		i = (id->dst_ip) ^ (id->src_ip) ^ (id->dst_port) ^
		    (id->src_port);
	i &= (V_ft_curr_buckets - 1);

	return (i);
}

static inline void
unlink_entry_print(struct ipfw_flow_id *id)
{
#ifdef DIFFUSE_DEBUG
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
		_inet_ntoa_r(da, src, sizeof(src));
		da.s_addr = htonl(id->dst_ip);
		_inet_ntoa_r(da, dst, sizeof(dst));
	}
	DID("ft unlink entry %s %d -> %s %d, %d left",
	    src, id->src_port, dst, id->dst_port, V_ft_count - 1);
#endif /* DIFFUSE_DEBUG */
}

/*
 * Unlink a dynamic rule from a chain. prev is a pointer to the previous one, q
 * is a pointer to the rule to delete, head is a pointer to the head of the
 * queue. Modifies q and potentially also head.
 */
#define	UNLINK_DYN_RULE(prev, head, q) do {				\
	struct di_ft_entry *old_q = q;					\
									\
	unlink_entry_print(&q->id);					\
	if (prev != NULL)						\
		prev->next = q = q->next;				\
	else								\
		head = q = q->next;					\
	V_ft_count--;							\
	uma_zfree(di_ft_zone, old_q);					\
} while (0)

#define	TIME_LEQ(a,b) ((int)((a)-(b)) <= 0)

#ifdef _KERNEL

/* Delete tags list, called under lock. */
static void
remove_classes(struct di_ft_entry *e)
{
	struct di_flow_class *s;

	DI_FT_WLOCK_ASSERT();

	while (!SLIST_EMPTY(&e->flow_classes)) {
		s = SLIST_FIRST(&e->flow_classes);
		SLIST_REMOVE_HEAD(&e->flow_classes, next);
		free(s, M_DIFFUSE);
		e->tcnt--;
	}
}

/* Delete tags list, called under lock. */
static void
remove_exports(struct di_ft_entry *e)
{
	struct di_exp *s;

	DI_FT_WLOCK_ASSERT();

	while (!SLIST_EMPTY(&e->ex_list)) {
		s = SLIST_FIRST(&e->ex_list);
		SLIST_REMOVE_HEAD(&e->ex_list, next);
		free(s, M_DIFFUSE);
	}
}

/* Remove a rule timeout (if present). */
static void
remove_timeout(struct di_ft_entry *e)
{

	DI_FT_WLOCK_ASSERT();

	if (e->to) {
		LIST_REMOVE(e->to, next);
		free(e->to, M_DIFFUSE);
		e->to = NULL;
	}
}

/* Export end of flow record. */
static void
remove_rule_msg(struct di_ft_entry *q)
{
	struct di_exp *s;

	DI_FT_WLOCK_ASSERT();

	SLIST_FOREACH(s, &q->ex_list, next) {
		if (q->ex_time.tv_sec > 0)
			diffuse_export_add_rec(q, s->ex, 0);
	}
}

#endif /* _KERNEL */

/* Free memory allocated by entry. Send remove rule msg if needed. */
static void
destroy_entry(struct di_ft_entry *q, int remove_msg)
{
	int i;

	DI_FT_WLOCK_ASSERT();

#ifdef _KERNEL
	if (remove_msg) {
		/* Signal end of flow for non-expired entries. */
		remove_rule_msg(q);
	}
#endif

	/* Destroy features */
	for (i = 0; i < q->fcnt; i++) {
		q->features[i]->alg->destroy_stats(&q->features[i]->conf,
		    &q->fwd_data[i]);
		if (!(q->features[i]->alg->type &
		    DI_FEATURE_ALG_BIDIRECTIONAL) &&
		    (q->ftype & DI_FLOW_TYPE_BIDIRECTIONAL)) {
			q->features[i]->alg->destroy_stats(&q->features[i]->conf,
			    &q->bck_data[i]);
		}
	}

#ifdef _KERNEL
	remove_classes(q);
	remove_exports(q);
	remove_timeout(q);
#endif
}

/*
 * Remove flows pointing to "rule", or all of them if rule == NULL.
 *
 * If expired_only == 0, flows are deleted even if not expired, otherwise only
 * expired flows are removed.
 */
static void
remove_entry(struct ip_fw *rule, int expired_only)
{
	static uint32_t last_remove = 0;
	struct di_ft_entry *prev, *q;
	int i;

	DI_FT_WLOCK_ASSERT();

	if (V_diffuse_ft_v == NULL || V_ft_count == 0)
		return;

	/* Do not expire more than once per second, it is useless. */
	if (expired_only != 0 && last_remove == time_uptime)
		return;

	last_remove = time_uptime;

	for (i = 0; i < V_ft_curr_buckets; i++) {
		for (prev = NULL, q = V_diffuse_ft_v[i]; q; ) {
			if (rule != NULL && rule != q->rule)
				goto next;

			if (expired_only != 0 &&
			    !TIME_LEQ(q->expire, time_uptime))
				goto next;

			destroy_entry(q, 1);
			UNLINK_DYN_RULE(prev, V_diffuse_ft_v[i], q);
			continue;
next:
			prev = q;
			q = q->next;
		}
	}
}

void
diffuse_ft_remove_entries(struct ip_fw *rule)
{

	DI_FT_WLOCK();
	remove_entry(rule, 0); /* 0 forces removal. */
	DI_FT_UNLOCK();
}

/*
 * Updates flow lifetime. If we don't see start of TCP flow -> very short
 * timeout.
 */
static void
update_lifetime(struct di_ft_entry *q, struct ipfw_flow_id *pkt, int dir)
{
	uint32_t timeout;
	unsigned char flags;
#ifdef _KERNEL
	struct di_to_entry *te;
	uint32_t slot, old_expire;

	old_expire = q->expire;
#endif

	DI_FT_WLOCK_ASSERT();

	if (pkt->proto == IPPROTO_TCP) { /* Update state according to flags. */
		flags = pkt->_flags & (TH_FIN|TH_SYN|TH_RST);

#define	BOTH_SYN (TH_SYN | (TH_SYN << 8))
#define	BOTH_FIN (TH_FIN | (TH_FIN << 8))
		q->state |= (dir == MATCH_FORWARD ) ? flags : (flags << 8);
		switch (q->state) {
		case TH_SYN:			/* Opening. */
			timeout = V_ft_syn_lifetime;
			break;

		case BOTH_SYN:			/* Move to established. */
		case BOTH_SYN | TH_FIN :	/* One side tries to close. */
		case BOTH_SYN | (TH_FIN << 8) :
			timeout = V_ft_ack_lifetime;
			break;

		case BOTH_SYN | BOTH_FIN:	/* Both sides closed. */
			timeout = V_ft_fin_lifetime;
			break;

		default:
			timeout = V_ft_rst_lifetime;
			break;
		}
	} else if (pkt->proto == IPPROTO_UDP) {
		timeout = V_ft_udp_lifetime;
	} else {
		/* Other protocols. */
		timeout = V_ft_short_lifetime;
	}

	q->expire = time_uptime + timeout;

#ifdef _KERNEL
	
	if (di_conf.an_rule_removal == DIP_TIMEOUT_NONE &&
	    (!q->to || q->expire != old_expire)) {
		/*
		 * Guard against too big timeout values.
		 * XXX: use SYSCTL_PROC to check setting of timeouts
		 */
		if (timeout > V_to_curr_buckets)
			q->expire = time_uptime + V_to_curr_buckets;

		slot = q->expire & (V_to_curr_buckets - 1);

		if (q->to) {
			/* Update existing timeout. */
			DID2("updating timeout to slot %u", slot);
			LIST_REMOVE(q->to, next);
			LIST_INSERT_HEAD(&V_diffuse_to_v[slot], q->to, next);
		} else {
			/* Add new timeout. */
			DID2("inserting timeout in slot %u", slot);
			te = malloc(sizeof(*te), M_DIFFUSE, M_NOWAIT | M_ZERO);
			if (te != NULL) {
				te->flow = q;
				q->to = te;
				LIST_INSERT_HEAD(&V_diffuse_to_v[slot], te, next);
			}
		}
	}
#endif

	DID2("expiry update %d %d %d", pkt->proto, q->state, q->expire);
}

#ifdef _KERNEL
/* We rely on this function being called regularly. */
void
diffuse_ft_check_timeouts(di_to_handler_fn_t f)
{
	static uint32_t last_time = 0;
	struct di_to_entry *t, *tmp;
	struct di_export_rec *ex_rec;
	struct di_exp *s;
	uint32_t i, slot;

	if (last_time == 0)
		last_time = time_uptime - 1;

	DI_FT_WLOCK();

	if (V_diffuse_to_v != NULL && (time_uptime - last_time > 0)) {
		for (i = last_time + 1; i <= time_uptime; i++) {
			slot = i & (V_to_curr_buckets - 1);

			DID2("checking timeout slot %u", slot);
			LIST_FOREACH_SAFE(t, &V_diffuse_to_v[slot], next, tmp) {

				SLIST_FOREACH(s, &t->flow->ex_list, next) {
					if (t->flow->ex_time.tv_sec > 0) {
						ex_rec = f(t->flow, s->ex, 0);
						t->flow->ex_time = ex_rec->time;
					}
				}

				LIST_REMOVE(t, next);
				t->flow->to = NULL;
				free(t, M_DIFFUSE);
			}

			last_time++;
		}
	}

	DI_FT_UNLOCK();
}
#endif

static void
update_features(struct di_ft_entry *q, struct ipfw_flow_id *pkt,
    struct ip_fw_args *args, void *ulp, int dir)
{
	struct di_feature *f;
	int i;

	DI_FT_WLOCK_ASSERT();

	for (i = 0; i < q->fcnt; i++) {
		f = q->features[i];

		if (dir == MATCH_FORWARD ||
		    q->features[i]->alg->type & DI_FEATURE_ALG_BIDIRECTIONAL) {
			f->alg->update_stats(&f->conf, &q->fwd_data[i], args->m,
			    pkt->proto, ulp, dir);
		} else if (dir == MATCH_REVERSE) { /* Only possible if bidir. */
			f->alg->update_stats(&f->conf, &q->bck_data[i], args->m,
			    pkt->proto, ulp, dir);
		}
	}
}

/* Lookup a dynamic rule, locked version. */
static struct di_ft_entry *
lookup_entry_locked(struct ipfw_flow_id *pkt, int *match_direction)
{
	struct di_ft_entry *prev, *q;
	int i, dir;

	DI_FT_WLOCK_ASSERT();

	q = NULL;
	dir = MATCH_NONE;

	if (V_diffuse_ft_v == NULL)
		goto done;

	i = hash_packet(pkt);

	for (prev = NULL, q = V_diffuse_ft_v[i]; q != NULL; ) {
		if (TIME_LEQ(q->expire, time_uptime)) { /* Expire entry. */
			/* End of flow record already sent via timeout. */
			destroy_entry(q, 0);
			UNLINK_DYN_RULE(prev, V_diffuse_ft_v[i], q);
			continue;
		}
		if (pkt->proto == q->id.proto) {
#ifdef INET6
			if (IS_IP6_FLOW_ID(pkt)) {
				if (IN6_ARE_ADDR_EQUAL(&(pkt->src_ip6),
				    &(q->id.src_ip6)) &&
				    IN6_ARE_ADDR_EQUAL(&(pkt->dst_ip6),
				    &(q->id.dst_ip6)) &&
				    pkt->src_port == q->id.src_port &&
				    pkt->dst_port == q->id.dst_port) {
					dir = MATCH_FORWARD;
					break;
				}
				if (q->ftype & DI_FLOW_TYPE_BIDIRECTIONAL &&
				    IN6_ARE_ADDR_EQUAL(&(pkt->src_ip6),
				    &(q->id.dst_ip6)) &&
				    IN6_ARE_ADDR_EQUAL(&(pkt->dst_ip6),
				    &(q->id.src_ip6)) &&
				    pkt->src_port == q->id.dst_port &&
				    pkt->dst_port == q->id.src_port) {
					dir = MATCH_REVERSE;
					break;
				}
			} else
#endif
			{
				if (pkt->src_ip == q->id.src_ip &&
				    pkt->dst_ip == q->id.dst_ip &&
				    pkt->src_port == q->id.src_port &&
				    pkt->dst_port == q->id.dst_port) {
					dir = MATCH_FORWARD;
					break;
				}
				if (q->ftype & DI_FLOW_TYPE_BIDIRECTIONAL &&
				    pkt->src_ip == q->id.dst_ip &&
				    pkt->dst_ip == q->id.src_ip &&
				    pkt->src_port == q->id.dst_port &&
				    pkt->dst_port == q->id.src_port) {
					dir = MATCH_REVERSE;
					break;
				}
			}
		}
		prev = q;
		q = q->next;
	}

	if (q == NULL)
		goto done;

	if (prev != NULL) { /* Found and not in front. */
		prev->next = q->next;
		q->next = V_diffuse_ft_v[i];
		V_diffuse_ft_v[i] = q;
	}

	update_lifetime(q, pkt, dir);

done:
	if (match_direction)
		*match_direction = dir;

	return (q);
}

/* NOTE: function returns with lock held, caller must unlock. */
struct di_ft_entry *
diffuse_ft_lookup_entry(struct ipfw_flow_id *pkt, struct ip_fw_args *args,
    void *ulp, int pktlen, int *match_direction)
{
	struct di_ft_entry *q;

	DI_FT_WLOCK();

	q = lookup_entry_locked(pkt, match_direction);
	if (q == NULL) {
		DI_FT_UNLOCK();
		return (NULL);
	}

	update_features(q, &args->f_id, args, ulp, *match_direction);
	q->pcnt++;
	q->bcnt += pktlen;

	return (q);
}

/* Return a single statistic. */
int
diffuse_ft_get_stat(struct di_ft_entry *q, int fdir,
    struct di_feature *fptr, int sidx, int32_t *val)
{
	int i, ret;

	ret = 0;

	DI_FT_RLOCK();

	for (i = 0; i < q->fcnt; i++) {
		if (q->features[i] == fptr) {
			if (fdir == DI_MATCH_DIR_NONE ||
			    fdir == DI_MATCH_DIR_FWD ||
			    (q->features[i]->alg->type &
			    DI_FEATURE_ALG_BIDIRECTIONAL)) {
				ret = q->features[i]->alg->get_stat(
				    &q->features[i]->conf, &q->fwd_data[i], sidx,
				    val);
			} else if (q->ftype & DI_FLOW_TYPE_BIDIRECTIONAL) {
				ret = q->features[i]->alg->get_stat(
				    &q->features[i]->conf,
				    &q->bck_data[i], sidx, val);
			}
			break;
		}
	}

	DI_FT_UNLOCK();

	return (ret);
}

/* Get multiple statistics */
int
diffuse_ft_get_stats(struct di_ft_entry *q, int fscnt,
    struct di_feature_stat *fstats, struct di_feature_stat_ptr *fstats_ptr,
    int32_t *fvec)
{
	int i, j, ret;

	ret = 0;

	DI_FT_RLOCK();

	for (i = 0; i < fscnt; i++) {
		for (j = 0; j < q->fcnt; j++) {
			if (q->features[j] == fstats_ptr[i].fptr) {
				if (fstats[i].fdir == DI_MATCH_DIR_NONE ||
				    fstats[i].fdir == DI_MATCH_DIR_FWD ||
				    (q->features[j]->alg->type &
				    DI_FEATURE_ALG_BIDIRECTIONAL)) {
					ret = q->features[j]->alg->get_stat(
					    &q->features[j]->conf,
					    &q->fwd_data[j], fstats_ptr[i].sidx,
					    &fvec[i]);
				} else if (q->ftype &
				    DI_FLOW_TYPE_BIDIRECTIONAL) {
					ret = q->features[j]->alg->get_stat(
					    &q->features[j]->conf,
					    &q->bck_data[j],
					    fstats_ptr[i].sidx,
					    &fvec[i]);
				}
				/* If any statistic is still missing, return. */
				if (!ret) {
					DI_FT_UNLOCK();
					return (ret);
				}
			}
		}
	}

	DI_FT_UNLOCK();

	return (ret);
}

void
diffuse_ft_unlock(void)
{

	DI_FT_UNLOCK();
}

static void
realloc_flow_table(void)
{
#ifdef _KERNEL
	int i;
#endif

	DI_FT_WLOCK_ASSERT();

	/*
	 * Try reallocation, make sure we have a power of 2 and do not allow
	 * more than 64k entries. In case of overflow, default to 1024.
	 */

	if (V_ft_buckets > 65536)
		V_ft_buckets = 1024;

	if ((V_ft_buckets & (V_ft_buckets - 1)) != 0) {
		V_ft_buckets = V_ft_curr_buckets; /* Not a power of 2, reset. */
		return;
	}

#ifdef _KERNEL
	if (V_to_buckets > 4096)
		V_to_buckets = 4096;

	if ((V_to_buckets & (V_to_buckets - 1)) != 0) {
		V_to_buckets = V_to_curr_buckets; /* Not a power of 2, reset. */
		return;
	}

	/* Must be larger than all of the timeout values. */
	if (V_to_buckets < V_ft_ack_lifetime ||
	    V_to_buckets < V_ft_syn_lifetime ||
	    V_to_buckets < V_ft_fin_lifetime ||
	    V_to_buckets < V_ft_rst_lifetime ||
	    V_to_buckets < V_ft_udp_lifetime ||
	    V_to_buckets < V_ft_short_lifetime) {
		V_to_buckets = V_to_curr_buckets; /* Reset. */
		return;
	}
#endif

	V_ft_curr_buckets = V_ft_buckets;
	if (V_diffuse_ft_v != NULL)
		free(V_diffuse_ft_v, M_DIFFUSE);

	for (;;) {
		V_diffuse_ft_v = malloc(V_ft_curr_buckets *
		    sizeof(struct di_ft_entry *), M_DIFFUSE, M_NOWAIT | M_ZERO);

		if (V_diffuse_ft_v != NULL || V_ft_curr_buckets <= 2)
			break;

		V_ft_curr_buckets >>= 1;
	}

#ifdef _KERNEL
	V_to_curr_buckets = V_to_buckets;
	if (V_diffuse_to_v != NULL)
		free(V_diffuse_to_v, M_DIFFUSE);

	V_diffuse_to_v = malloc(V_to_curr_buckets *
	    sizeof(struct di_to_head), M_DIFFUSE, M_NOWAIT | M_ZERO);

	if (V_diffuse_to_v != NULL) {
		for(i = 0; i < V_to_curr_buckets; i++)
			LIST_INIT(&V_diffuse_to_v[i]);
	}
#endif
}

/* Add entry for a new flow. */
static struct di_ft_entry *
add_entry(struct ipfw_flow_id *id, struct ip_fw *rule)
{
	struct di_ft_entry *r;
	int i;

	DI_FT_WLOCK_ASSERT();

	if (V_diffuse_ft_v == NULL ||
	    (V_ft_count == 0 && V_ft_buckets != V_ft_curr_buckets)
#ifdef _KERNEL
	    || V_diffuse_to_v == NULL ||
	    (V_ft_count == 0 && V_to_buckets != V_to_curr_buckets)
#endif
	) {
		realloc_flow_table();
		if (V_diffuse_ft_v == NULL)
			return (NULL);
#ifdef _KERNEL
		if (V_diffuse_to_v == NULL)
			return (NULL);
#endif
	}

	i = hash_packet(id);

#ifdef _KERNEL
	r = uma_zalloc(di_ft_zone, M_NOWAIT | M_ZERO);
#else
	r = calloc(sizeof(struct di_ft_entry), 1);
#endif
	if (r == NULL) {
		DID("add entry cannot allocate state");
		return (NULL);
	}

	r->id = *id;
	r->expire = time_uptime + V_ft_syn_lifetime;
	r->rule = rule;
	r->pcnt = r->bcnt = 0;
	r->bucket = i;
	r->tcnt = 0;
#ifdef _KERNEL
	r->ex_time.tv_sec = 0;
	r->ex_time.tv_usec = 0;
	SLIST_INIT(&r->flow_classes);
	SLIST_INIT(&r->ex_list);
	r->to = NULL;
#else
	r->flow_id = id_cnt++;
#endif
	r->next = V_diffuse_ft_v[i];
	V_diffuse_ft_v[i] = r;
	V_ft_count++;
#ifdef DIFFUSE_DEBUG
	{
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
			_inet_ntoa_r(da, src, sizeof(src));
			da.s_addr = htonl(r->id.dst_ip);
			_inet_ntoa_r(da, dst, sizeof(dst));
		}
		DID("add ft entry %s %d -> %s %d, total %d",
		    src, r->id.src_port, dst, r->id.dst_port,
		    V_ft_count);
	}
#endif /* DIFFUSE_DEBUG */
	return (r);
}

#ifdef _KERNEL

/*
 * Add a class tag to a flow entry, or change the class of an existing tag, or
 * just increase confirm when class is unchanged.
 */
void
diffuse_ft_add_class(struct di_ft_entry *e, char *cname, int class,
    int *prev_class, int *confirm)
{
	struct di_flow_class *s, *c;

	c = NULL;

	DI_FT_WLOCK();

	SLIST_FOREACH(s, &e->flow_classes, next) {
		if (strcmp(s->cname, cname) == 0) {
			if (s->class == class) {
				/* XXX: Why 0xFFFF? */
				if (s->confirm < 0xFFFF)
					s->confirm++;
			} else {
				/*
				 * Memorise last class if well confirmed.
				 * XXX: 10 is a magic constant.
				 * XXX: Do something smarter like selecting the
				 *      most frequent class within last x
				 *      packets.
				 */
				if ((s->prev_class < 0 &&
				    s->confirm >= *confirm) ||
				    (s->confirm >= *confirm * 10)) {
					s->prev_class = s->class;
				}
				s->class = class;
				s->confirm = 0;
			}

			*prev_class = s->prev_class;
			*confirm = s->confirm;

			DI_FT_UNLOCK();

			return;
		}
	}

	c = malloc(sizeof(struct di_flow_class), M_DIFFUSE, M_NOWAIT | M_ZERO);
	if (c != NULL) {
		strcpy(c->cname, cname);
		c->class = class;
		c->prev_class = -1;
		c->confirm = 0;

		SLIST_INSERT_HEAD(&e->flow_classes, c, next);
		e->tcnt++;

		*prev_class = c->prev_class;
		*confirm = c->confirm;
	}

	DI_FT_UNLOCK();
}

/* Return flow class for classifier cname or -1 if not classified. */
int
diffuse_ft_get_class(struct di_ft_entry *e, char *cname, int *prev_class,
    int *confirm)
{
	struct di_flow_class *s;
	int class;

	class = -1;

	DI_FT_RLOCK();

	SLIST_FOREACH(s, &e->flow_classes, next) {
		if (strcmp(s->cname, cname) == 0) {
			class = s->class;
			*prev_class = s->prev_class;
			*confirm = s->confirm;
			break;
		}
	}

	DI_FT_UNLOCK();

	return (class);
}

/* Add pointer to exporter, if not exist. */
void
diffuse_ft_add_export(struct di_ft_entry *q, struct di_export_rec *ex_rec,
    struct di_export *nex)
{
	struct di_exp *s, *e;

	e = NULL;

	DI_FT_WLOCK();

	/* Store pointer to export record and last export record time. */
	if (ex_rec != NULL)
		q->ex_time = ex_rec->time;

	/* Link pointer to export to this flow entry (if not there yet). */
	SLIST_FOREACH(s, &q->ex_list, next) {
		if (nex == s->ex) {
			DI_FT_UNLOCK();
			return;
		}
	}

	e = malloc(sizeof(struct di_exp), M_DIFFUSE, M_NOWAIT | M_ZERO);
	if (e != NULL) {
		e->ex = nex;
		SLIST_INSERT_HEAD(&q->ex_list, e, next);
	}

	DI_FT_UNLOCK();
}
#endif /* _KERNEL */

/*
 * Install state for new flow
 *
 * Returns NULL (failure) if state is not installed because of errors or because
 * session limitations are enforced.
 */
struct di_ft_entry *
diffuse_ft_install_state(struct ip_fw *rule, ipfw_insn_features *cmd,
    struct ip_fw_args *args, void *ulp, int pktlen)
{
	static int last_log;
	struct di_ft_entry *q;
	struct di_feature *f;
	int i;
#ifdef DIFFUSE_DEBUG
	struct in_addr da;
#ifdef INET6
	char src[INET6_ADDRSTRLEN], dst[INET6_ADDRSTRLEN];
#else
	char src[INET_ADDRSTRLEN], dst[INET_ADDRSTRLEN];
#endif
#endif

	q = NULL;

	DI_FT_WLOCK();

#ifdef DIFFUSE_DEBUG
#ifdef INET6
	if (IS_IP6_FLOW_ID(&(args->f_id))) {
		ip6_sprintf(src, &args->f_id.src_ip6);
		ip6_sprintf(dst, &args->f_id.dst_ip6);
	} else
#endif
	{
		da.s_addr = htonl(args->f_id.src_ip);
		_inet_ntoa_r(da, src, sizeof(src));
		da.s_addr = htonl(args->f_id.dst_ip);
		_inet_ntoa_r(da, dst, sizeof(dst));
	}
	DID("ft %s: type %d %s %u -> %s %u",
	    __func__, cmd->o.opcode, src, args->f_id.src_port, dst,
	    args->f_id.dst_port);
#endif

	q = lookup_entry_locked(&args->f_id, NULL);

	if (q != NULL) {
		if (last_log != time_uptime) {
			last_log = time_uptime;
			DID("ft entry already present, done");
		}
		DI_FT_UNLOCK();
		return (q);
	}

	if (V_ft_count >= V_ft_max) {
		/* Run out of slots, try to remove any expired rule. */
		remove_entry(NULL, 1);
	}

	if (V_ft_count >= V_ft_max) {
		if (last_log != time_uptime) {
			last_log = time_uptime;
			DID("ft too many entries");
		}
		DI_FT_UNLOCK();
		return (NULL); /* Cannot install, notify caller. */
	}

	q = add_entry(&args->f_id, rule);
	if (!q) {
		DI_FT_UNLOCK();
		return (NULL);
	}

	/* Set lifetime. */
	update_lifetime(q, &args->f_id, MATCH_FORWARD);

	/* Setup everything. */
	q->fcnt = cmd->fcnt;
	q->ftype = cmd->ftype;
	q->sample_int = cmd->sample_int;
	q->sample_prob = cmd->sample_prob;
	q->pkts_after_last = cmd->sample_int; /* Ensure we match the first. */
	/* Link features. */
	memcpy(q->features, cmd->fptrs,
	    cmd->fcnt * sizeof(struct di_feature *));

	for (i = 0; i < q->fcnt; i++) {
		f = q->features[i];
		f->alg->init_stats(&f->conf, &q->fwd_data[i]);
		if (!(f->alg->type & DI_FEATURE_ALG_BIDIRECTIONAL) &&
		    q->ftype & DI_FLOW_TYPE_BIDIRECTIONAL) {
			f->alg->init_stats(&f->conf, &q->bck_data[i]);
		}
	}

	/* Update features. */
	update_features(q, &args->f_id, args, ulp, MATCH_FORWARD);

	/* Update counters. */
	q->pcnt++;
	q->bcnt += pktlen;

	DI_FT_UNLOCK();

	return (q);
}

/*
 * Add all features in cmd which are not already there and update their values.
 * If DI_MAX_FEATURES reached, return error.
 */
int
diffuse_ft_update_features(struct di_ft_entry *q, ipfw_insn_features *cmd,
    struct ip_fw_args *args, void *ulp)
{
	struct di_feature *f;
	int i, j;

	DI_FT_WLOCK();

	for (i = 0; i < cmd->fcnt; i++) {
		for (j = 0; j < q->fcnt; j++) {
			if (q->features[j] == cmd->fptrs[i])
				break;
		}
		if (j == q->fcnt) {
			/* Add missing feature. */
			if (q->fcnt == DI_MAX_FEATURES) {
				DI_FT_UNLOCK();
				return (-1);
			}

			f = q->features[q->fcnt] = cmd->fptrs[i];
			f->alg->init_stats(&f->conf, &q->fwd_data[q->fcnt]);
			if (!(q->features[q->fcnt]->alg->type &
			    DI_FEATURE_ALG_BIDIRECTIONAL) &&
			    (q->ftype & DI_FLOW_TYPE_BIDIRECTIONAL)) {
				f->alg->init_stats(&f->conf,
				    &q->bck_data[q->fcnt]);
			}
			q->fcnt++;
			update_features(q, &args->f_id, args, ulp, MATCH_FORWARD);
		}
	}

	DI_FT_UNLOCK();

	return (0);
}

/* Flush the flow table (or flow counters). */
void
diffuse_ft_flush(int reset_counters_only)
{
	struct di_ft_entry *prev, *q;
	int i;

	DI_FT_WLOCK();

	if (V_diffuse_ft_v == NULL || V_ft_count == 0) {
		DI_FT_UNLOCK();
		return;
	}

	for (i = 0; i < V_ft_curr_buckets; i++) {
		for (prev = NULL, q = V_diffuse_ft_v[i]; q; ) {
			if (reset_counters_only) {
				q->pcnt = 0;
				q->bcnt = 0;
			} else {
				destroy_entry(q, 1);
				UNLINK_DYN_RULE(prev, V_diffuse_ft_v[i], q);
				continue;
			}
			prev = q;
			q = q->next;
		}
	}

	DI_FT_UNLOCK();
}

#ifdef _KERNEL

/* For rule timeouts refresh x seconds before expiration. */
#define	TIME_BEFORE_EXPIRE 2

/* Returns with lock held, caller must unlock. */
int
diffuse_ft_do_export(struct di_ft_entry *q, uint16_t confirm)
{
	struct di_flow_class *c;
	struct timeval now;

	if (di_conf.an_rule_removal == DIP_TIMEOUT_RULE)
		getmicrotime(&now);

	DI_FT_RLOCK();

	/* Export if rule timeout used and we are close to expiry. */
	if (di_conf.an_rule_removal == DIP_TIMEOUT_RULE &&
	    q->ex_time.tv_sec > 0 &&
	    now.tv_sec > q->ex_time.tv_sec &&
	    q->expire - time_uptime < TIME_BEFORE_EXPIRE) {
		return (1);
	}

	/*
	 * Do export if one classifier has confirmed consecutive classifications.
	 * Do not export if we have more because class is unchanged.
	 * XXX: Currently it can happen that we export again even if class
	 * hasn't changed; to prevent that we would need to check the last
	 * exported class for each export,classifier combination.
	 */
	SLIST_FOREACH(c, &q->flow_classes, next) {
		if (c->confirm == confirm)
			return (1);
	}

	return (0);
}

void
diffuse_ft_attach(void)
{

	di_ft_zone = uma_zcreate("DIFFUSE flow table",
	    sizeof(struct di_ft_entry), NULL, NULL, NULL, NULL, UMA_ALIGN_PTR,
	    0);

	DI_FT_LOCK_INIT();
}

void
diffuse_ft_detach(void)
{

	uma_zdestroy(di_ft_zone);
	DI_FT_LOCK_DESTROY();
}

#endif

void
diffuse_ft_init(void)
{

	V_diffuse_ft_v = NULL;

#ifdef _KERNEL
	V_ft_buckets = 256;		/* Must be power of 2. */
	V_ft_curr_buckets = 256;	/* Must be power of 2. */
	V_ft_max = 4096;		/* Max # of flow entries. */
#else /* ipfw_fstats */
	V_ft_buckets = 4096;		/* Must be power of 2. */
	V_ft_curr_buckets = 4096;	/* Must be power of 2. */
	V_ft_max = 131072;
#endif

#ifdef _KERNEL
	V_diffuse_to_v = NULL;
	V_to_buckets = 512;		/* Must be power of 2. */
	V_to_curr_buckets = 512;	/* Must be power of 2. */
#endif

	V_ft_ack_lifetime = 300;
	V_ft_syn_lifetime = 20;
	/* Change following two defaults back to 1? */
	V_ft_fin_lifetime = 2;
	V_ft_rst_lifetime = 2;
	V_ft_udp_lifetime = 10;
	V_ft_short_lifetime = 5;
}

void
diffuse_ft_uninit(void)
{
#ifdef _KERNEL
	struct di_to_entry *e;
	int i;
#endif

	if (V_diffuse_ft_v != NULL)
		free(V_diffuse_ft_v, M_DIFFUSE);

#ifdef _KERNEL
	if (V_diffuse_to_v != NULL) {
		for (i = 0; i < V_to_curr_buckets; i++) {
			while (!LIST_EMPTY(&V_diffuse_to_v[i])) {
				e = LIST_FIRST(&V_diffuse_to_v[i]);
				LIST_REMOVE(e, next);
				free(e, M_DIFFUSE);
			}
		}
		free(V_diffuse_to_v, M_DIFFUSE);
	}
#endif
}

int
diffuse_ft_entries(void)
{

	return (V_ft_count);
}

#ifdef _KERNEL

static int
get_entry_size(struct di_ft_entry *p)
{
	char **names;
	int j, n, needed;

	needed = 0;

	DI_FT_RLOCK_ASSERT();

	needed += sizeof(struct di_ft_export_entry);
	needed += 2 * p->fcnt * sizeof(uint8_t);

	for (j = 0; j < p->fcnt; j++) {
		n = p->features[j]->alg->get_stat_names(&names);

		if (!(p->features[j]->alg->type &
		    DI_FEATURE_ALG_BIDIRECTIONAL) &&
		    p->ftype & DI_FLOW_TYPE_BIDIRECTIONAL) {
			needed += 2 * n * sizeof(int32_t);
		} else {
			needed += n * sizeof(int32_t);
		}
	}

	/* Class tags. */
	needed += p->tcnt * sizeof(struct di_ft_flow_class);

	return (needed);
}

int
diffuse_ft_len(int expired)
{
	struct di_ft_entry *p;
	int i, needed;

	needed = 0;

	if (V_diffuse_ft_v == NULL)
		return (0);

	DI_FT_RLOCK();

	for (i = 0; i < V_ft_curr_buckets; i++) {
		for (p = V_diffuse_ft_v[i]; p != NULL; p = p->next) {
			if (!expired && TIME_LEQ(p->expire, time_uptime))
				continue;

			needed += get_entry_size(p);
		}
	}

	DI_FT_UNLOCK();

	return (needed);
}

/* Called for show command. */
void
diffuse_get_ft(char **pbp, const char *ep, int expired)
{
	struct di_ft_entry *p;
	struct di_ft_export_entry *dst;
	struct di_flow_class *s;
	char *bp, **names;
	uint8_t scnt[DI_MAX_FEATURES];
	int i, idx, j, needed;
	int32_t *stats;

	if (V_diffuse_ft_v == NULL)
		return;

	bp = *pbp;

	DI_FT_RLOCK();

	for (i = 0; i < V_ft_curr_buckets; i++) {
		for (p = V_diffuse_ft_v[i]; p != NULL; p = p->next) {
			needed = 0;

			if (!expired && TIME_LEQ(p->expire, time_uptime))
                                continue;

			needed = get_entry_size(p);

			if (bp + needed <= ep) {
				dst = (struct di_ft_export_entry *)bp;

				dst->ruleno = p->rule->rulenum;
				dst->setno = p->rule->set;
				dst->pcnt = p->pcnt;
				dst->bcnt = p->bcnt;
				memcpy(&dst->id, &p->id,
				    sizeof(struct ipfw_flow_id));
				dst->expire = TIME_LEQ(p->expire, time_uptime) ?
				    0 : p->expire - time_uptime;
				dst->bucket = p->bucket;
				dst->state = p->state;
				dst->fcnt = p->fcnt;
				dst->tcnt = p->tcnt;
				dst->ftype = p->ftype;
				dst->final = p->next == NULL ? 1 : 0;

				bp += sizeof(struct di_ft_export_entry);

				/*
				 * Copy feature number (as they are in the list
				 * copied earlier).
				 */
				for (j = 0; j < p->fcnt; j++) {
					idx = diffuse_get_feature_idx(
					    p->features[j]->name);
					*bp = idx;
					bp++;
				}

				/* Copy number of stats per feature. */
				for (j = 0; j < p->fcnt; j++) {
					scnt[j] = p->features[j]->alg->
					    get_stat_names(&names);
					*bp = scnt[j];
					bp++;
				}

				for (j = 0; j < p->fcnt; j++) {
					stats = NULL;
					p->features[j]->alg->get_stats(
					    &p->features[j]->conf,
					    &p->fwd_data[j], &stats);

					if (stats != NULL) {
						memcpy(bp, stats,
						    scnt[j] * sizeof(int32_t));
					} else {
						memset(bp, 0,
						    scnt[j] * sizeof(int32_t));
					}

					bp += scnt[j] * sizeof(int32_t);

					if (!(p->features[j]->alg->type &
					    DI_FEATURE_ALG_BIDIRECTIONAL) &&
					    (p->ftype &
					    DI_FLOW_TYPE_BIDIRECTIONAL)) {
						stats = NULL;
						p->features[j]->alg->get_stats(
						    &p->features[j]->conf,
						    &p->bck_data[j], &stats);

						if (stats != NULL) {
							memcpy(bp, stats,
							    scnt[j] *
							    sizeof(int32_t));
						} else {
							memset(bp, 0, scnt[j] *
							    sizeof(int32_t));
						}

						bp += scnt[j] * sizeof(int32_t);
					}
				}

				/* Class tags. */
				SLIST_FOREACH(s, &p->flow_classes, next) {
					memcpy(bp, s->cname, DI_MAX_NAME_STR_LEN);
					bp += DI_MAX_NAME_STR_LEN;
					memcpy(bp, &s->class, sizeof(uint16_t));
					bp += sizeof(uint16_t);
				}
			} else {
				goto done;
			}
		}
	}

done:

	DI_FT_UNLOCK();
	*pbp = bp;
}

#endif /* _KERNEL */
