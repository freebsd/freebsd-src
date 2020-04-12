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
	rn_close_t		*rnh_close;	/*do something when the last ref drops*/
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

/* Macro for verifying fields in af-specific 'struct route' structures */
#define CHK_STRUCT_FIELD_GENERIC(_s1, _f1, _s2, _f2)			\
_Static_assert(sizeof(((_s1 *)0)->_f1) == sizeof(((_s2 *)0)->_f2),	\
		"Fields " #_f1 " and " #_f2 " size differs");		\
_Static_assert(__offsetof(_s1, _f1) == __offsetof(_s2, _f2),		\
		"Fields " #_f1 " and " #_f2 " offset differs");

#define _CHK_ROUTE_FIELD(_route_new, _field) \
	CHK_STRUCT_FIELD_GENERIC(struct route, _field, _route_new, _field)

#define CHK_STRUCT_ROUTE_FIELDS(_route_new)	\
	_CHK_ROUTE_FIELD(_route_new, ro_rt)	\
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

struct rib_head *rt_tables_get_rnh(int fib, int family);
void rt_mpath_init_rnh(struct rib_head *rnh);

VNET_PCPUSTAT_DECLARE(struct rtstat, rtstat);
#define	RTSTAT_ADD(name, val)	\
	VNET_PCPUSTAT_ADD(struct rtstat, rtstat, name, (val))
#define	RTSTAT_INC(name)	RTSTAT_ADD(name, 1)

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

#endif
