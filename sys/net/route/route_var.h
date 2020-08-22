/*-
 * Copyright (c) 2015-2016
 * 	Alexander V. Chernikov <melifaro@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _NET_ROUTE_VAR_H_
#define _NET_ROUTE_VAR_H_

#ifndef RNF_NORMAL
#include <net/radix.h>
#endif
#include <sys/ck.h>
#include <sys/epoch.h>
#include <netinet/in.h>		/* struct sockaddr_in */
#include <sys/counter.h>

struct nh_control;
typedef int rnh_preadd_entry_f_t(u_int fibnum, const struct sockaddr *addr,
	const struct sockaddr *mask, struct nhop_object *nh);

struct rib_head {
	struct radix_head	head;
	rn_matchaddr_f_t	*rnh_matchaddr;	/* longest match for sockaddr */
	rn_addaddr_f_t		*rnh_addaddr;	/* add based on sockaddr*/
	rn_deladdr_f_t		*rnh_deladdr;	/* remove based on sockaddr */
	rn_lookup_f_t		*rnh_lookup;	/* exact match for sockaddr */
	rn_walktree_t		*rnh_walktree;	/* traverse tree */
	rn_walktree_from_t	*rnh_walktree_from; /* traverse tree below a */
	rnh_preadd_entry_f_t	*rnh_preadd;	/* hook to alter record prior to insertion */
	rt_gen_t		rnh_gen;	/* generation counter */
	int			rnh_multipath;	/* multipath capable ? */
	struct radix_node	rnh_nodes[3];	/* empty tree for common case */
	struct rmlock		rib_lock;	/* config/data path lock */
	struct radix_mask_head	rmhead;		/* masks radix head */
	struct vnet		*rib_vnet;	/* vnet pointer */
	int			rib_family;	/* AF of the rtable */
	u_int			rib_fibnum;	/* fib number */
	struct callout		expire_callout;	/* Callout for expiring dynamic routes */
	time_t			next_expire;	/* Next expire run ts */
	struct nh_control	*nh_control;	/* nexthop subsystem data */
	CK_STAILQ_HEAD(, rib_subscription)	rnh_subscribers;/* notification subscribers */
};

#define	RIB_RLOCK_TRACKER	struct rm_priotracker _rib_tracker
#define	RIB_LOCK_INIT(rh)	rm_init(&(rh)->rib_lock, "rib head lock")
#define	RIB_LOCK_DESTROY(rh)	rm_destroy(&(rh)->rib_lock)
#define	RIB_RLOCK(rh)		rm_rlock(&(rh)->rib_lock, &_rib_tracker)
#define	RIB_RUNLOCK(rh)		rm_runlock(&(rh)->rib_lock, &_rib_tracker)
#define	RIB_WLOCK(rh)		rm_wlock(&(rh)->rib_lock)
#define	RIB_WUNLOCK(rh)		rm_wunlock(&(rh)->rib_lock)
#define	RIB_LOCK_ASSERT(rh)	rm_assert(&(rh)->rib_lock, RA_LOCKED)
#define	RIB_WLOCK_ASSERT(rh)	rm_assert(&(rh)->rib_lock, RA_WLOCKED)

/* Constants */
#define	RIB_MAX_RETRIES	3
#define	RT_MAXFIBS	UINT16_MAX

/* Macro for verifying fields in af-specific 'struct route' structures */
#define CHK_STRUCT_FIELD_GENERIC(_s1, _f1, _s2, _f2)			\
_Static_assert(sizeof(((_s1 *)0)->_f1) == sizeof(((_s2 *)0)->_f2),	\
		"Fields " #_f1 " and " #_f2 " size differs");		\
_Static_assert(__offsetof(_s1, _f1) == __offsetof(_s2, _f2),		\
		"Fields " #_f1 " and " #_f2 " offset differs");

#define _CHK_ROUTE_FIELD(_route_new, _field) \
	CHK_STRUCT_FIELD_GENERIC(struct route, _field, _route_new, _field)

#define CHK_STRUCT_ROUTE_FIELDS(_route_new)	\
	_CHK_ROUTE_FIELD(_route_new, ro_nh)	\
	_CHK_ROUTE_FIELD(_route_new, ro_lle)	\
	_CHK_ROUTE_FIELD(_route_new, ro_prepend)\
	_CHK_ROUTE_FIELD(_route_new, ro_plen)	\
	_CHK_ROUTE_FIELD(_route_new, ro_flags)	\
	_CHK_ROUTE_FIELD(_route_new, ro_mtu)	\
	_CHK_ROUTE_FIELD(_route_new, spare)

#define CHK_STRUCT_ROUTE_COMPAT(_ro_new, _dst_new)				\
CHK_STRUCT_ROUTE_FIELDS(_ro_new);						\
_Static_assert(__offsetof(struct route, ro_dst) == __offsetof(_ro_new, _dst_new),\
		"ro_dst and " #_dst_new " are at different offset")

struct rib_head *rt_tables_get_rnh(uint32_t table, sa_family_t family);
void rt_mpath_init_rnh(struct rib_head *rnh);
int rt_getifa_fib(struct rt_addrinfo *info, u_int fibnum);
void rt_setmetrics(const struct rt_addrinfo *info, struct rtentry *rt);
#ifdef RADIX_MPATH
struct radix_node *rt_mpath_unlink(struct rib_head *rnh,
    struct rt_addrinfo *info, struct rtentry *rto, int *perror);
#endif
struct rib_cmd_info;

VNET_PCPUSTAT_DECLARE(struct rtstat, rtstat);
#define	RTSTAT_ADD(name, val)	\
	VNET_PCPUSTAT_ADD(struct rtstat, rtstat, name, (val))
#define	RTSTAT_INC(name)	RTSTAT_ADD(name, 1)


/*
 * Convert a 'struct radix_node *' to a 'struct rtentry *'.
 * The operation can be done safely (in this code) because a
 * 'struct rtentry' starts with two 'struct radix_node''s, the first
 * one representing leaf nodes in the routing tree, which is
 * what the code in radix.c passes us as a 'struct radix_node'.
 *
 * But because there are a lot of assumptions in this conversion,
 * do not cast explicitly, but always use the macro below.
 */
#define RNTORT(p)	((struct rtentry *)(p))

struct rtentry {
	struct	radix_node rt_nodes[2];	/* tree glue, and other values */
	/*
	 * XXX struct rtentry must begin with a struct radix_node (or two!)
	 * because the code does some casts of a 'struct radix_node *'
	 * to a 'struct rtentry *'
	 */
#define	rt_key(r)	(*((struct sockaddr **)(&(r)->rt_nodes->rn_key)))
#define	rt_mask(r)	(*((struct sockaddr **)(&(r)->rt_nodes->rn_mask)))
#define	rt_key_const(r)		(*((const struct sockaddr * const *)(&(r)->rt_nodes->rn_key)))
#define	rt_mask_const(r)	(*((const struct sockaddr * const *)(&(r)->rt_nodes->rn_mask)))

	/*
	 * 2 radix_node structurs above consists of 2x6 pointers, leaving
	 * 4 pointers (32 bytes) of the second cache line on amd64.
	 *
	 */
	struct nhop_object	*rt_nhop;	/* nexthop data */
	union {
		/*
		 * Destination address storage.
		 * sizeof(struct sockaddr_in6) == 28, however
		 * the dataplane-relevant part (e.g. address) lies
		 * at offset 8..24, making the address not crossing
		 * cacheline boundary.
		 */
		struct sockaddr_in	rt_dst4;
		struct sockaddr_in6	rt_dst6;
		struct sockaddr		rt_dst;
		char			rt_dstb[28];
	};

	int		rte_flags;	/* up/down?, host/net */
	u_long		rt_weight;	/* absolute weight */ 
	u_long		rt_expire;	/* lifetime for route, e.g. redirect */
#define	rt_endzero	rt_mtx
	struct mtx	rt_mtx;		/* mutex for routing entry */
	struct rtentry	*rt_chain;	/* pointer to next rtentry to delete */
	struct epoch_context	rt_epoch_ctx;	/* net epoch tracker */
};

#define	RT_LOCK_INIT(_rt) \
	mtx_init(&(_rt)->rt_mtx, "rtentry", NULL, MTX_DEF | MTX_DUPOK | MTX_NEW)
#define	RT_LOCK(_rt)		mtx_lock(&(_rt)->rt_mtx)
#define	RT_UNLOCK(_rt)		mtx_unlock(&(_rt)->rt_mtx)
#define	RT_LOCK_DESTROY(_rt)	mtx_destroy(&(_rt)->rt_mtx)
#define	RT_LOCK_ASSERT(_rt)	mtx_assert(&(_rt)->rt_mtx, MA_OWNED)
#define	RT_UNLOCK_COND(_rt)	do {				\
	if (mtx_owned(&(_rt)->rt_mtx))				\
		mtx_unlock(&(_rt)->rt_mtx);			\
} while (0)

/*
 * With the split between the routing entry and the nexthop,
 *  rt_flags has to be split between these 2 entries. As rtentry
 *  mostly contains prefix data and is thought to be generic enough
 *  so one can transparently change the nexthop pointer w/o requiring
 *  any other rtentry changes, most of rt_flags shifts to the particular nexthop.
 * /
 *
 * RTF_UP: rtentry, as an indication that it is linked.
 * RTF_HOST: rtentry, nhop. The latter indication is needed for the datapath
 * RTF_DYNAMIC: nhop, to make rtentry generic.
 * RTF_MODIFIED: nhop, to make rtentry generic. (legacy)
 * -- "native" path (nhop) properties:
 * RTF_GATEWAY, RTF_STATIC, RTF_PROTO1, RTF_PROTO2, RTF_PROTO3, RTF_FIXEDMTU,
 *  RTF_PINNED, RTF_REJECT, RTF_BLACKHOLE, RTF_BROADCAST
 */

/* Nexthop rt flags mask */
#define	NHOP_RT_FLAG_MASK	(RTF_GATEWAY | RTF_HOST | RTF_REJECT | RTF_DYNAMIC | \
    RTF_MODIFIED | RTF_STATIC | RTF_BLACKHOLE | RTF_PROTO1 | RTF_PROTO2 | \
    RTF_PROTO3 | RTF_FIXEDMTU | RTF_PINNED | RTF_BROADCAST)

/* rtentry rt flag mask */
#define	RTE_RT_FLAG_MASK	(RTF_UP | RTF_HOST)

/* Nexthop selection */
#define	_NH2MP(_nh)	((struct nhgrp_object *)(_nh))
#define	_SELECT_NHOP(_nh, _flowid)	\
	(_NH2MP(_nh))->nhops[(_flowid) % (_NH2MP(_nh))->mp_size]
#define	_RT_SELECT_NHOP(_nh, _flowid)	\
	((!NH_IS_MULTIPATH(_nh)) ? (_nh) : _SELECT_NHOP(_nh, _flowid))
#define	RT_SELECT_NHOP(_rt, _flowid)	_RT_SELECT_NHOP((_rt)->rt_nhop, _flowid)
 
/* rte<>nhop translation */
static inline uint16_t
fib_rte_to_nh_flags(int rt_flags)
{
	uint16_t res;

	res = (rt_flags & RTF_REJECT) ? NHF_REJECT : 0;
	res |= (rt_flags & RTF_HOST) ? NHF_HOST : 0;
	res |= (rt_flags & RTF_BLACKHOLE) ? NHF_BLACKHOLE : 0;
	res |= (rt_flags & (RTF_DYNAMIC|RTF_MODIFIED)) ? NHF_REDIRECT : 0;
	res |= (rt_flags & RTF_BROADCAST) ? NHF_BROADCAST : 0;
	res |= (rt_flags & RTF_GATEWAY) ? NHF_GATEWAY : 0;

	return (res);
}

void tmproutes_update(struct rib_head *rnh, struct rtentry *rt);
void tmproutes_init(struct rib_head *rh);
void tmproutes_destroy(struct rib_head *rh);

/* route_ctl.c */
void vnet_rtzone_init(void);
void vnet_rtzone_destroy(void);

#endif
