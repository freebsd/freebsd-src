/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1989 Stephen Deering
 * Copyright (c) 1992, 1993
 *      The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Stephen Deering of Stanford University.
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
 */

/*
 * IP multicast forwarding procedures
 *
 * Written by David Waitzman, BBN Labs, August 1988.
 * Modified by Steve Deering, Stanford, February 1989.
 * Modified by Mark J. Steiglitz, Stanford, May, 1991
 * Modified by Van Jacobson, LBL, January 1993
 * Modified by Ajit Thyagarajan, PARC, August 1993
 * Modified by Bill Fenner, PARC, April 1995
 * Modified by Ahmed Helmy, SGI, June 1996
 * Modified by George Edmond Eddy (Rusty), ISI, February 1998
 * Modified by Pavlin Radoslavov, USC/ISI, May 1998, August 1999, October 2000
 * Modified by Hitoshi Asaeda, WIDE, August 2000
 * Modified by Pavlin Radoslavov, ICSI, October 2002
 * Modified by Wojciech Macek, Semihalf, May 2021
 *
 * MROUTING Revision: 3.5
 * and PIM-SMv2 and PIM-DM support, advanced API support,
 * bandwidth metering and signaling
 */

/*
 * TODO: Prefix functions with ipmf_.
 * TODO: Maintain a refcount on if_allmulti() in ifnet or in the protocol
 * domain attachment (if_afdata) so we can track consumers of that service.
 * TODO: Deprecate routing socket path for SIOCGETSGCNT and SIOCGETVIFCNT,
 * move it to socket options.
 * TODO: Cleanup LSRR removal further.
 * TODO: Push RSVP stubs into raw_ip.c.
 * TODO: Use bitstring.h for vif set.
 * TODO: Fix mrt6_ioctl dangling ref when dynamically loaded.
 * TODO: Sync ip6_mroute.c with this file.
 */

#include <sys/cdefs.h>
#include "opt_inet.h"
#include "opt_mrouting.h"

#define _PIM_VT 1

#include <sys/types.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/stddef.h>
#include <sys/condvar.h>
#include <sys/eventhandler.h>
#include <sys/lock.h>
#include <sys/kthread.h>
#include <sys/ktr.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/priv.h>
#include <sys/protosw.h>
#include <sys/signalvar.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sockio.h>
#include <sys/sx.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/systm.h>
#include <sys/taskqueue.h>
#include <sys/time.h>
#include <sys/counter.h>
#include <machine/atomic.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_private.h>
#include <net/if_types.h>
#include <net/netisr.h>
#include <net/route.h>
#include <net/vnet.h>

#include <netinet/in.h>
#include <netinet/igmp.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/ip_encap.h>
#include <netinet/ip_mroute.h>
#include <netinet/ip_var.h>
#include <netinet/ip_options.h>
#include <netinet/pim.h>
#include <netinet/pim_var.h>
#include <netinet/udp.h>

#include <machine/in_cksum.h>

#ifndef KTR_IPMF
#define KTR_IPMF KTR_INET
#endif

#define		VIFI_INVALID	((vifi_t) -1)

static MALLOC_DEFINE(M_MRTABLE, "mroutetbl", "multicast forwarding cache");

/*
 * Locking.  We use two locks: one for the virtual interface table and
 * one for the forwarding table.  These locks may be nested in which case
 * the VIF lock must always be taken first.  Note that each lock is used
 * to cover not only the specific data structure but also related data
 * structures.
 */

static struct sx __exclusive_cache_line mrouter_teardown;
#define	MRW_TEARDOWN_WLOCK()	sx_xlock(&mrouter_teardown)
#define	MRW_TEARDOWN_WUNLOCK()	sx_xunlock(&mrouter_teardown)
#define	MRW_TEARDOWN_LOCK_INIT()				\
	sx_init(&mrouter_teardown, "IPv4 multicast forwarding teardown")
#define	MRW_TEARDOWN_LOCK_DESTROY()	sx_destroy(&mrouter_teardown)

static struct rwlock mrouter_lock;
#define	MRW_RLOCK()		rw_rlock(&mrouter_lock)
#define	MRW_WLOCK()		rw_wlock(&mrouter_lock)
#define	MRW_RUNLOCK()		rw_runlock(&mrouter_lock)
#define	MRW_WUNLOCK()		rw_wunlock(&mrouter_lock)
#define	MRW_UNLOCK()		rw_unlock(&mrouter_lock)
#define	MRW_LOCK_ASSERT()	rw_assert(&mrouter_lock, RA_LOCKED)
#define	MRW_WLOCK_ASSERT()	rw_assert(&mrouter_lock, RA_WLOCKED)
#define	MRW_LOCK_TRY_UPGRADE()	rw_try_upgrade(&mrouter_lock)
#define	MRW_WOWNED()		rw_wowned(&mrouter_lock)
#define	MRW_LOCK_INIT()						\
	rw_init(&mrouter_lock, "IPv4 multicast forwarding")
#define	MRW_LOCK_DESTROY()	rw_destroy(&mrouter_lock)

static int ip_mrouter_cnt;	/* # of vnets with active mrouters */
static int ip_mrouter_unloading; /* Allow no more V_ip_mrouter sockets */

VNET_PCPUSTAT_DEFINE_STATIC(struct mrtstat, mrtstat);
VNET_PCPUSTAT_SYSINIT(mrtstat);
VNET_PCPUSTAT_SYSUNINIT(mrtstat);
SYSCTL_VNET_PCPUSTAT(_net_inet_ip, OID_AUTO, mrtstat, struct mrtstat,
    mrtstat, "IPv4 Multicast Forwarding Statistics (struct mrtstat, "
    "netinet/ip_mroute.h)");

VNET_DEFINE_STATIC(u_long, mfchash);
#define	V_mfchash		VNET(mfchash)
#define	MFCHASH(a, g)							\
	((((a).s_addr >> 20) ^ ((a).s_addr >> 10) ^ (a).s_addr ^ \
	  ((g).s_addr >> 20) ^ ((g).s_addr >> 10) ^ (g).s_addr) & V_mfchash)
#define	MFCHASHSIZE	256

static u_long mfchashsize = MFCHASHSIZE;	/* Hash size */
SYSCTL_ULONG(_net_inet_ip, OID_AUTO, mfchashsize, CTLFLAG_RDTUN,
    &mfchashsize, 0, "IPv4 Multicast Forwarding Table hash size");
VNET_DEFINE_STATIC(u_char *, nexpire);		/* 0..mfchashsize-1 */
#define	V_nexpire		VNET(nexpire)
VNET_DEFINE_STATIC(LIST_HEAD(mfchashhdr, mfc)*, mfchashtbl);
#define	V_mfchashtbl		VNET(mfchashtbl)
VNET_DEFINE_STATIC(struct taskqueue *, task_queue);
#define	V_task_queue		VNET(task_queue)
VNET_DEFINE_STATIC(struct task, task);
#define	V_task		VNET(task)

VNET_DEFINE_STATIC(vifi_t, numvifs);
#define	V_numvifs		VNET(numvifs)
VNET_DEFINE_STATIC(struct vif *, viftable);
#define	V_viftable		VNET(viftable)

static eventhandler_tag if_detach_event_tag = NULL;

VNET_DEFINE_STATIC(struct callout, expire_upcalls_ch);
#define	V_expire_upcalls_ch	VNET(expire_upcalls_ch)

VNET_DEFINE_STATIC(struct mtx, buf_ring_mtx);
#define	V_buf_ring_mtx	VNET(buf_ring_mtx)

#define		EXPIRE_TIMEOUT	(hz / 4)	/* 4x / second		*/
#define		UPCALL_EXPIRE	6		/* number of timeouts	*/

/*
 * Bandwidth meter variables and constants
 */
static MALLOC_DEFINE(M_BWMETER, "bwmeter", "multicast upcall bw meters");

/*
 * Pending upcalls are stored in a ring which is flushed when
 * full, or periodically
 */
VNET_DEFINE_STATIC(struct callout, bw_upcalls_ch);
#define	V_bw_upcalls_ch		VNET(bw_upcalls_ch)
VNET_DEFINE_STATIC(struct buf_ring *, bw_upcalls_ring);
#define	V_bw_upcalls_ring    	VNET(bw_upcalls_ring)
VNET_DEFINE_STATIC(struct mtx, bw_upcalls_ring_mtx);
#define	V_bw_upcalls_ring_mtx    	VNET(bw_upcalls_ring_mtx)

#define BW_UPCALLS_PERIOD (hz)		/* periodical flush of bw upcalls */

VNET_PCPUSTAT_DEFINE_STATIC(struct pimstat, pimstat);
VNET_PCPUSTAT_SYSINIT(pimstat);
VNET_PCPUSTAT_SYSUNINIT(pimstat);

SYSCTL_NODE(_net_inet, IPPROTO_PIM, pim, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "PIM");
SYSCTL_VNET_PCPUSTAT(_net_inet_pim, PIMCTL_STATS, stats, struct pimstat,
    pimstat, "PIM Statistics (struct pimstat, netinet/pim_var.h)");

static u_long	pim_squelch_wholepkt = 0;
SYSCTL_ULONG(_net_inet_pim, OID_AUTO, squelch_wholepkt, CTLFLAG_RWTUN,
    &pim_squelch_wholepkt, 0,
    "Disable IGMP_WHOLEPKT notifications if rendezvous point is unspecified");

static const struct encaptab *pim_encap_cookie;
static int pim_encapcheck(const struct mbuf *, int, int, void *);
static int pim_input(struct mbuf *, int, int, void *);

extern int in_mcast_loop;

static const struct encap_config ipv4_encap_cfg = {
	.proto = IPPROTO_PIM,
	.min_length = sizeof(struct ip) + PIM_MINLEN,
	.exact_match = 8,
	.check = pim_encapcheck,
	.input = pim_input
};

/*
 * Note: the PIM Register encapsulation adds the following in front of a
 * data packet:
 *
 * struct pim_encap_hdr {
 *    struct ip ip;
 *    struct pim_encap_pimhdr  pim;
 * }
 *
 */

struct pim_encap_pimhdr {
	struct pim pim;
	uint32_t   flags;
};
#define		PIM_ENCAP_TTL	64

static struct ip pim_encap_iphdr = {
#if BYTE_ORDER == LITTLE_ENDIAN
	sizeof(struct ip) >> 2,
	IPVERSION,
#else
	IPVERSION,
	sizeof(struct ip) >> 2,
#endif
	0,			/* tos */
	sizeof(struct ip),	/* total length */
	0,			/* id */
	0,			/* frag offset */
	PIM_ENCAP_TTL,
	IPPROTO_PIM,
	0,			/* checksum */
};

static struct pim_encap_pimhdr pim_encap_pimhdr = {
    {
	PIM_MAKE_VT(PIM_VERSION, PIM_REGISTER), /* PIM vers and message type */
	0,			/* reserved */
	0,			/* checksum */
    },
    0				/* flags */
};

VNET_DEFINE_STATIC(vifi_t, reg_vif_num) = VIFI_INVALID;
#define	V_reg_vif_num		VNET(reg_vif_num)
VNET_DEFINE_STATIC(struct ifnet *, multicast_register_if);
#define	V_multicast_register_if	VNET(multicast_register_if)

/*
 * Private variables.
 */

static u_long	X_ip_mcast_src(int);
static int	X_ip_mforward(struct ip *, struct ifnet *, struct mbuf *,
		    struct ip_moptions *);
static int	X_ip_mrouter_done(void);
static int	X_ip_mrouter_get(struct socket *, struct sockopt *);
static int	X_ip_mrouter_set(struct socket *, struct sockopt *);
static int	X_legal_vif_num(int);
static int	X_mrt_ioctl(u_long, caddr_t, int);

static int	add_bw_upcall(struct bw_upcall *);
static int	add_mfc(struct mfcctl2 *);
static int	add_vif(struct vifctl *);
static void	bw_meter_prepare_upcall(struct bw_meter *, struct timeval *);
static void	bw_meter_geq_receive_packet(struct bw_meter *, int,
		    struct timeval *);
static void	bw_upcalls_send(void);
static int	del_bw_upcall(struct bw_upcall *);
static int	del_mfc(struct mfcctl2 *);
static int	del_vif(vifi_t);
static int	del_vif_locked(vifi_t, struct ifnet **, struct ifnet **);
static void	expire_bw_upcalls_send(void *);
static void	expire_mfc(struct mfc *);
static void	expire_upcalls(void *);
static void	free_bw_list(struct bw_meter *);
static int	get_sg_cnt(struct sioc_sg_req *);
static int	get_vif_cnt(struct sioc_vif_req *);
static void	if_detached_event(void *, struct ifnet *);
static int	ip_mdq(struct mbuf *, struct ifnet *, struct mfc *, vifi_t);
static int	ip_mrouter_init(struct socket *, int);
static __inline struct mfc *
		mfc_find(struct in_addr *, struct in_addr *);
static void	phyint_send(struct ip *, struct vif *, struct mbuf *);
static struct mbuf *
		pim_register_prepare(struct ip *, struct mbuf *);
static int	pim_register_send(struct ip *, struct vif *,
		    struct mbuf *, struct mfc *);
static int	pim_register_send_rp(struct ip *, struct vif *,
		    struct mbuf *, struct mfc *);
static int	pim_register_send_upcall(struct ip *, struct vif *,
		    struct mbuf *, struct mfc *);
static void	send_packet(struct vif *, struct mbuf *);
static int	set_api_config(uint32_t *);
static int	set_assert(int);
static int	socket_send(struct socket *, struct mbuf *,
		    struct sockaddr_in *);

/*
 * Kernel multicast forwarding API capabilities and setup.
 * If more API capabilities are added to the kernel, they should be
 * recorded in `mrt_api_support'.
 */
#define MRT_API_VERSION		0x0305

static const int mrt_api_version = MRT_API_VERSION;
static const uint32_t mrt_api_support = (MRT_MFC_FLAGS_DISABLE_WRONGVIF |
					 MRT_MFC_FLAGS_BORDER_VIF |
					 MRT_MFC_RP |
					 MRT_MFC_BW_UPCALL);
VNET_DEFINE_STATIC(uint32_t, mrt_api_config);
#define	V_mrt_api_config	VNET(mrt_api_config)
VNET_DEFINE_STATIC(int, pim_assert_enabled);
#define	V_pim_assert_enabled	VNET(pim_assert_enabled)
static struct timeval pim_assert_interval = { 3, 0 };	/* Rate limit */

/*
 * Find a route for a given origin IP address and multicast group address.
 * Statistics must be updated by the caller.
 */
static __inline struct mfc *
mfc_find(struct in_addr *o, struct in_addr *g)
{
	struct mfc *rt;

	/*
	 * Might be called both RLOCK and WLOCK.
	 * Check if any, it's caller responsibility
	 * to choose correct option.
	 */
	MRW_LOCK_ASSERT();

	LIST_FOREACH(rt, &V_mfchashtbl[MFCHASH(*o, *g)], mfc_hash) {
		if (in_hosteq(rt->mfc_origin, *o) &&
		    in_hosteq(rt->mfc_mcastgrp, *g) &&
		    buf_ring_empty(rt->mfc_stall_ring))
			break;
	}

	return (rt);
}

static __inline struct mfc *
mfc_alloc(void)
{
	struct mfc *rt;
	rt = malloc(sizeof(*rt), M_MRTABLE, M_NOWAIT | M_ZERO);
	if (rt == NULL)
		return rt;

	rt->mfc_stall_ring = buf_ring_alloc(MAX_UPQ, M_MRTABLE,
	    M_NOWAIT, &V_buf_ring_mtx);
	if (rt->mfc_stall_ring == NULL) {
		free(rt, M_MRTABLE);
		return NULL;
	}

	return rt;
}

/*
 * Handle MRT setsockopt commands to modify the multicast forwarding tables.
 */
static int
X_ip_mrouter_set(struct socket *so, struct sockopt *sopt)
{
	int error, optval;
	vifi_t vifi;
	struct vifctl vifc;
	struct mfcctl2 mfc;
	struct bw_upcall bw_upcall;
	uint32_t i;

	if (so != V_ip_mrouter && sopt->sopt_name != MRT_INIT)
		return EPERM;

	error = 0;
	switch (sopt->sopt_name) {
	case MRT_INIT:
		error = sooptcopyin(sopt, &optval, sizeof optval, sizeof optval);
		if (error)
			break;
		error = ip_mrouter_init(so, optval);
		break;
	case MRT_DONE:
		error = ip_mrouter_done();
		break;
	case MRT_ADD_VIF:
		error = sooptcopyin(sopt, &vifc, sizeof vifc, sizeof vifc);
		if (error)
			break;
		error = add_vif(&vifc);
		break;
	case MRT_DEL_VIF:
		error = sooptcopyin(sopt, &vifi, sizeof vifi, sizeof vifi);
		if (error)
			break;
		error = del_vif(vifi);
		break;
	case MRT_ADD_MFC:
	case MRT_DEL_MFC:
		/*
		 * select data size depending on API version.
		 */
		if (sopt->sopt_name == MRT_ADD_MFC &&
		    V_mrt_api_config & MRT_API_FLAGS_ALL) {
			error = sooptcopyin(sopt, &mfc, sizeof(struct mfcctl2),
			    sizeof(struct mfcctl2));
		} else {
			error = sooptcopyin(sopt, &mfc, sizeof(struct mfcctl),
			    sizeof(struct mfcctl));
			bzero((caddr_t)&mfc + sizeof(struct mfcctl),
			    sizeof(mfc) - sizeof(struct mfcctl));
		}
		if (error)
			break;
		if (sopt->sopt_name == MRT_ADD_MFC)
			error = add_mfc(&mfc);
		else
			error = del_mfc(&mfc);
		break;

	case MRT_ASSERT:
		error = sooptcopyin(sopt, &optval, sizeof optval, sizeof optval);
		if (error)
			break;
		set_assert(optval);
		break;

	case MRT_API_CONFIG:
		error = sooptcopyin(sopt, &i, sizeof i, sizeof i);
		if (!error)
			error = set_api_config(&i);
		if (!error)
			error = sooptcopyout(sopt, &i, sizeof i);
		break;

	case MRT_ADD_BW_UPCALL:
	case MRT_DEL_BW_UPCALL:
		error = sooptcopyin(sopt, &bw_upcall, sizeof bw_upcall,
		    sizeof bw_upcall);
		if (error)
			break;
		if (sopt->sopt_name == MRT_ADD_BW_UPCALL)
			error = add_bw_upcall(&bw_upcall);
		else
			error = del_bw_upcall(&bw_upcall);
		break;

	default:
		error = EOPNOTSUPP;
		break;
	}
	return error;
}

/*
 * Handle MRT getsockopt commands
 */
static int
X_ip_mrouter_get(struct socket *so, struct sockopt *sopt)
{
	int error;

	switch (sopt->sopt_name) {
	case MRT_VERSION:
		error = sooptcopyout(sopt, &mrt_api_version,
		    sizeof mrt_api_version);
		break;
	case MRT_ASSERT:
		error = sooptcopyout(sopt, &V_pim_assert_enabled,
		    sizeof V_pim_assert_enabled);
		break;
	case MRT_API_SUPPORT:
		error = sooptcopyout(sopt, &mrt_api_support,
		    sizeof mrt_api_support);
		break;
	case MRT_API_CONFIG:
		error = sooptcopyout(sopt, &V_mrt_api_config,
		    sizeof V_mrt_api_config);
		break;
	default:
		error = EOPNOTSUPP;
		break;
	}
	return error;
}

/*
 * Handle ioctl commands to obtain information from the cache
 */
static int
X_mrt_ioctl(u_long cmd, caddr_t data, int fibnum __unused)
{
	int error;

	/*
	 * Currently the only function calling this ioctl routine is rtioctl_fib().
	 * Typically, only root can create the raw socket in order to execute
	 * this ioctl method, however the request might be coming from a prison
	 */
	error = priv_check(curthread, PRIV_NETINET_MROUTE);
	if (error)
		return (error);
	switch (cmd) {
	case (SIOCGETVIFCNT):
		error = get_vif_cnt((struct sioc_vif_req *)data);
		break;

	case (SIOCGETSGCNT):
		error = get_sg_cnt((struct sioc_sg_req *)data);
		break;

	default:
		error = EINVAL;
		break;
	}
	return error;
}

/*
 * returns the packet, byte, rpf-failure count for the source group provided
 */
static int
get_sg_cnt(struct sioc_sg_req *req)
{
	struct mfc *rt;

	MRW_RLOCK();
	rt = mfc_find(&req->src, &req->grp);
	if (rt == NULL) {
		MRW_RUNLOCK();
		req->pktcnt = req->bytecnt = req->wrong_if = 0xffffffff;
		return EADDRNOTAVAIL;
	}
	req->pktcnt = rt->mfc_pkt_cnt;
	req->bytecnt = rt->mfc_byte_cnt;
	req->wrong_if = rt->mfc_wrong_if;
	MRW_RUNLOCK();
	return 0;
}

/*
 * returns the input and output packet and byte counts on the vif provided
 */
static int
get_vif_cnt(struct sioc_vif_req *req)
{
	vifi_t vifi = req->vifi;

	MRW_RLOCK();
	if (vifi >= V_numvifs) {
		MRW_RUNLOCK();
		return EINVAL;
	}

	mtx_lock_spin(&V_viftable[vifi].v_spin);
	req->icount = V_viftable[vifi].v_pkt_in;
	req->ocount = V_viftable[vifi].v_pkt_out;
	req->ibytes = V_viftable[vifi].v_bytes_in;
	req->obytes = V_viftable[vifi].v_bytes_out;
	mtx_unlock_spin(&V_viftable[vifi].v_spin);
	MRW_RUNLOCK();

	return 0;
}

static void
if_detached_event(void *arg __unused, struct ifnet *ifp)
{
	vifi_t vifi;
	u_long i, vifi_cnt = 0;
	struct ifnet *free_ptr, *multi_leave;

	MRW_WLOCK();

	if (V_ip_mrouter == NULL) {
		MRW_WUNLOCK();
		return;
	}

	/*
	 * Tear down multicast forwarder state associated with this ifnet.
	 * 1. Walk the vif list, matching vifs against this ifnet.
	 * 2. Walk the multicast forwarding cache (mfc) looking for
	 *    inner matches with this vif's index.
	 * 3. Expire any matching multicast forwarding cache entries.
	 * 4. Free vif state. This should disable ALLMULTI on the interface.
	 */
restart:
	for (vifi = 0; vifi < V_numvifs; vifi++) {
		if (V_viftable[vifi].v_ifp != ifp)
			continue;
		for (i = 0; i < mfchashsize; i++) {
			struct mfc *rt, *nrt;

			LIST_FOREACH_SAFE(rt, &V_mfchashtbl[i], mfc_hash, nrt) {
				if (rt->mfc_parent == vifi) {
					expire_mfc(rt);
				}
			}
		}
		del_vif_locked(vifi, &multi_leave, &free_ptr);
		if (free_ptr != NULL)
			vifi_cnt++;
		if (multi_leave) {
			MRW_WUNLOCK();
			if_allmulti(multi_leave, 0);
			MRW_WLOCK();
			goto restart;
		}
	}

	MRW_WUNLOCK();

	/*
	 * Free IFP. We don't have to use free_ptr here as it is the same
	 * that ifp. Perform free as many times as required in case
	 * refcount is greater than 1.
	 */
	for (i = 0; i < vifi_cnt; i++)
		if_free(ifp);
}

static void
ip_mrouter_upcall_thread(void *arg, int pending __unused)
{
	CURVNET_SET((struct vnet *) arg);

	MRW_WLOCK();
	bw_upcalls_send();
	MRW_WUNLOCK();

	CURVNET_RESTORE();
}

/*
 * Enable multicast forwarding.
 */
static int
ip_mrouter_init(struct socket *so, int version)
{

	CTR2(KTR_IPMF, "%s: so %p", __func__, so);

	if (version != 1)
		return ENOPROTOOPT;

	MRW_TEARDOWN_WLOCK();
	MRW_WLOCK();

	if (ip_mrouter_unloading) {
		MRW_WUNLOCK();
		MRW_TEARDOWN_WUNLOCK();
		return ENOPROTOOPT;
	}

	if (V_ip_mrouter != NULL) {
		MRW_WUNLOCK();
		MRW_TEARDOWN_WUNLOCK();
		return EADDRINUSE;
	}

	V_mfchashtbl = hashinit_flags(mfchashsize, M_MRTABLE, &V_mfchash,
	    HASH_NOWAIT);
	if (V_mfchashtbl == NULL) {
		MRW_WUNLOCK();
		MRW_TEARDOWN_WUNLOCK();
		return (ENOMEM);
	}

	/* Create upcall ring */
	mtx_init(&V_bw_upcalls_ring_mtx, "mroute upcall buf_ring mtx", NULL, MTX_DEF);
	V_bw_upcalls_ring = buf_ring_alloc(BW_UPCALLS_MAX, M_MRTABLE,
	    M_NOWAIT, &V_bw_upcalls_ring_mtx);
	if (!V_bw_upcalls_ring) {
		MRW_WUNLOCK();
		MRW_TEARDOWN_WUNLOCK();
		return (ENOMEM);
	}

	TASK_INIT(&V_task, 0, ip_mrouter_upcall_thread, curvnet);
	taskqueue_cancel(V_task_queue, &V_task, NULL);
	taskqueue_unblock(V_task_queue);

	callout_reset(&V_expire_upcalls_ch, EXPIRE_TIMEOUT, expire_upcalls,
	    curvnet);
	callout_reset(&V_bw_upcalls_ch, BW_UPCALLS_PERIOD, expire_bw_upcalls_send,
	    curvnet);

	V_ip_mrouter = so;
	atomic_add_int(&ip_mrouter_cnt, 1);

	/* This is a mutex required by buf_ring init, but not used internally */
	mtx_init(&V_buf_ring_mtx, "mroute buf_ring mtx", NULL, MTX_DEF);

	MRW_WUNLOCK();
	MRW_TEARDOWN_WUNLOCK();

	CTR1(KTR_IPMF, "%s: done", __func__);

	return 0;
}

/*
 * Disable multicast forwarding.
 */
static int
X_ip_mrouter_done(void)
{
	struct ifnet **ifps;
	int nifp;
	u_long i;
	vifi_t vifi;
	struct bw_upcall *bu;

	MRW_TEARDOWN_WLOCK();

	if (V_ip_mrouter == NULL) {
		MRW_TEARDOWN_WUNLOCK();
		return (EINVAL);
	}

	/*
	 * Detach/disable hooks to the reset of the system.
	 */
	V_ip_mrouter = NULL;
	atomic_subtract_int(&ip_mrouter_cnt, 1);
	V_mrt_api_config = 0;

	/*
	 * Wait for all epoch sections to complete to ensure
	 * V_ip_mrouter = NULL is visible to others.
	 */
	NET_EPOCH_WAIT();

	/* Stop and drain task queue */
	taskqueue_block(V_task_queue);
	while (taskqueue_cancel(V_task_queue, &V_task, NULL)) {
		taskqueue_drain(V_task_queue, &V_task);
	}

	ifps = malloc(MAXVIFS * sizeof(*ifps), M_TEMP, M_WAITOK);

	MRW_WLOCK();
	taskqueue_cancel(V_task_queue, &V_task, NULL);

	/* Destroy upcall ring */
	while ((bu = buf_ring_dequeue_mc(V_bw_upcalls_ring)) != NULL) {
		free(bu, M_MRTABLE);
	}
	buf_ring_free(V_bw_upcalls_ring, M_MRTABLE);
	mtx_destroy(&V_bw_upcalls_ring_mtx);

	/*
	 * For each phyint in use, prepare to disable promiscuous reception
	 * of all IP multicasts.  Defer the actual call until the lock is released;
	 * just record the list of interfaces while locked.  Some interfaces use
	 * sx locks in their ioctl routines, which is not allowed while holding
	 * a non-sleepable lock.
	 */
	KASSERT(V_numvifs <= MAXVIFS, ("More vifs than possible"));
	for (vifi = 0, nifp = 0; vifi < V_numvifs; vifi++) {
		if (!in_nullhost(V_viftable[vifi].v_lcl_addr) &&
		    !(V_viftable[vifi].v_flags & (VIFF_TUNNEL | VIFF_REGISTER))) {
			ifps[nifp++] = V_viftable[vifi].v_ifp;
		}
	}
	bzero((caddr_t)V_viftable, sizeof(*V_viftable) * MAXVIFS);
	V_numvifs = 0;
	V_pim_assert_enabled = 0;

	callout_stop(&V_expire_upcalls_ch);
	callout_stop(&V_bw_upcalls_ch);

	/*
	 * Free all multicast forwarding cache entries.
	 * Do not use hashdestroy(), as we must perform other cleanup.
	 */
	for (i = 0; i < mfchashsize; i++) {
		struct mfc *rt, *nrt;

		LIST_FOREACH_SAFE(rt, &V_mfchashtbl[i], mfc_hash, nrt) {
			expire_mfc(rt);
		}
	}
	free(V_mfchashtbl, M_MRTABLE);
	V_mfchashtbl = NULL;

	bzero(V_nexpire, sizeof(V_nexpire[0]) * mfchashsize);

	V_reg_vif_num = VIFI_INVALID;

	mtx_destroy(&V_buf_ring_mtx);

	MRW_WUNLOCK();
	MRW_TEARDOWN_WUNLOCK();

	/*
	 * Now drop our claim on promiscuous multicast on the interfaces recorded
	 * above.  This is safe to do now because ALLMULTI is reference counted.
	 */
	for (vifi = 0; vifi < nifp; vifi++)
		if_allmulti(ifps[vifi], 0);
	free(ifps, M_TEMP);

	CTR1(KTR_IPMF, "%s: done", __func__);

	return 0;
}

/*
 * Set PIM assert processing global
 */
static int
set_assert(int i)
{
	if ((i != 1) && (i != 0))
		return EINVAL;

	V_pim_assert_enabled = i;

	return 0;
}

/*
 * Configure API capabilities
 */
int
set_api_config(uint32_t *apival)
{
	u_long i;

	/*
	 * We can set the API capabilities only if it is the first operation
	 * after MRT_INIT. I.e.:
	 *  - there are no vifs installed
	 *  - pim_assert is not enabled
	 *  - the MFC table is empty
	 */
	if (V_numvifs > 0) {
		*apival = 0;
		return EPERM;
	}
	if (V_pim_assert_enabled) {
		*apival = 0;
		return EPERM;
	}

	MRW_RLOCK();

	for (i = 0; i < mfchashsize; i++) {
		if (LIST_FIRST(&V_mfchashtbl[i]) != NULL) {
			MRW_RUNLOCK();
			*apival = 0;
			return EPERM;
		}
	}

	MRW_RUNLOCK();

	V_mrt_api_config = *apival & mrt_api_support;
	*apival = V_mrt_api_config;

	return 0;
}

/*
 * Add a vif to the vif table
 */
static int
add_vif(struct vifctl *vifcp)
{
	struct vif *vifp = V_viftable + vifcp->vifc_vifi;
	struct sockaddr_in sin = {sizeof sin, AF_INET};
	struct ifaddr *ifa;
	struct ifnet *ifp;
	int error;

	if (vifcp->vifc_vifi >= MAXVIFS)
		return EINVAL;
	/* rate limiting is no longer supported by this code */
	if (vifcp->vifc_rate_limit != 0) {
		log(LOG_ERR, "rate limiting is no longer supported\n");
		return EINVAL;
	}

	if (in_nullhost(vifcp->vifc_lcl_addr))
		return EADDRNOTAVAIL;

	/* Find the interface with an address in AF_INET family */
	if (vifcp->vifc_flags & VIFF_REGISTER) {
		/*
		 * XXX: Because VIFF_REGISTER does not really need a valid
		 * local interface (e.g. it could be 127.0.0.2), we don't
		 * check its address.
		 */
		ifp = NULL;
	} else {
		struct epoch_tracker et;

		sin.sin_addr = vifcp->vifc_lcl_addr;
		NET_EPOCH_ENTER(et);
		ifa = ifa_ifwithaddr((struct sockaddr *)&sin);
		if (ifa == NULL) {
			NET_EPOCH_EXIT(et);
			return EADDRNOTAVAIL;
		}
		ifp = ifa->ifa_ifp;
		/* XXX FIXME we need to take a ref on ifp and cleanup properly! */
		NET_EPOCH_EXIT(et);
	}

	if ((vifcp->vifc_flags & VIFF_TUNNEL) != 0) {
		CTR1(KTR_IPMF, "%s: tunnels are no longer supported", __func__);
		return EOPNOTSUPP;
	} else if (vifcp->vifc_flags & VIFF_REGISTER) {
		ifp = V_multicast_register_if = if_alloc(IFT_LOOP);
		CTR2(KTR_IPMF, "%s: add register vif for ifp %p", __func__, ifp);
		if (V_reg_vif_num == VIFI_INVALID) {
			if_initname(V_multicast_register_if, "register_vif", 0);
			V_reg_vif_num = vifcp->vifc_vifi;
		}
	} else {		/* Make sure the interface supports multicast */
		if ((ifp->if_flags & IFF_MULTICAST) == 0)
			return EOPNOTSUPP;

		/* Enable promiscuous reception of all IP multicasts from the if */
		error = if_allmulti(ifp, 1);
		if (error)
			return error;
	}

	MRW_WLOCK();

	if (!in_nullhost(vifp->v_lcl_addr)) {
		if (ifp)
			V_multicast_register_if = NULL;
		MRW_WUNLOCK();
		if (ifp)
			if_free(ifp);
		return EADDRINUSE;
	}

	vifp->v_flags     = vifcp->vifc_flags;
	vifp->v_threshold = vifcp->vifc_threshold;
	vifp->v_lcl_addr  = vifcp->vifc_lcl_addr;
	vifp->v_rmt_addr  = vifcp->vifc_rmt_addr;
	vifp->v_ifp       = ifp;
	/* initialize per vif pkt counters */
	vifp->v_pkt_in    = 0;
	vifp->v_pkt_out   = 0;
	vifp->v_bytes_in  = 0;
	vifp->v_bytes_out = 0;
	sprintf(vifp->v_spin_name, "BM[%d] spin", vifcp->vifc_vifi);
	mtx_init(&vifp->v_spin, vifp->v_spin_name, NULL, MTX_SPIN);

	/* Adjust numvifs up if the vifi is higher than numvifs */
	if (V_numvifs <= vifcp->vifc_vifi)
		V_numvifs = vifcp->vifc_vifi + 1;

	MRW_WUNLOCK();

	CTR4(KTR_IPMF, "%s: add vif %d laddr 0x%08x thresh %x", __func__,
	    (int)vifcp->vifc_vifi, ntohl(vifcp->vifc_lcl_addr.s_addr),
	    (int)vifcp->vifc_threshold);

	return 0;
}

/*
 * Delete a vif from the vif table
 */
static int
del_vif_locked(vifi_t vifi, struct ifnet **ifp_multi_leave, struct ifnet **ifp_free)
{
	struct vif *vifp;

	*ifp_free = NULL;
	*ifp_multi_leave = NULL;

	MRW_WLOCK_ASSERT();

	if (vifi >= V_numvifs) {
		return EINVAL;
	}
	vifp = &V_viftable[vifi];
	if (in_nullhost(vifp->v_lcl_addr)) {
		return EADDRNOTAVAIL;
	}

	if (!(vifp->v_flags & (VIFF_TUNNEL | VIFF_REGISTER)))
		*ifp_multi_leave = vifp->v_ifp;

	if (vifp->v_flags & VIFF_REGISTER) {
		V_reg_vif_num = VIFI_INVALID;
		if (vifp->v_ifp) {
			if (vifp->v_ifp == V_multicast_register_if)
				V_multicast_register_if = NULL;
			*ifp_free = vifp->v_ifp;
		}
	}

	mtx_destroy(&vifp->v_spin);

	bzero((caddr_t)vifp, sizeof (*vifp));

	CTR2(KTR_IPMF, "%s: delete vif %d", __func__, (int)vifi);

	/* Adjust numvifs down */
	for (vifi = V_numvifs; vifi > 0; vifi--)
		if (!in_nullhost(V_viftable[vifi-1].v_lcl_addr))
			break;
	V_numvifs = vifi;

	return 0;
}

static int
del_vif(vifi_t vifi)
{
	int cc;
	struct ifnet *free_ptr, *multi_leave;

	MRW_WLOCK();
	cc = del_vif_locked(vifi, &multi_leave, &free_ptr);
	MRW_WUNLOCK();

	if (multi_leave)
		if_allmulti(multi_leave, 0);
	if (free_ptr) {
		if_free(free_ptr);
	}

	return cc;
}

/*
 * update an mfc entry without resetting counters and S,G addresses.
 */
static void
update_mfc_params(struct mfc *rt, struct mfcctl2 *mfccp)
{
	int i;

	rt->mfc_parent = mfccp->mfcc_parent;
	for (i = 0; i < V_numvifs; i++) {
		rt->mfc_ttls[i] = mfccp->mfcc_ttls[i];
		rt->mfc_flags[i] = mfccp->mfcc_flags[i] & V_mrt_api_config &
			MRT_MFC_FLAGS_ALL;
	}
	/* set the RP address */
	if (V_mrt_api_config & MRT_MFC_RP)
		rt->mfc_rp = mfccp->mfcc_rp;
	else
		rt->mfc_rp.s_addr = INADDR_ANY;
}

/*
 * fully initialize an mfc entry from the parameter.
 */
static void
init_mfc_params(struct mfc *rt, struct mfcctl2 *mfccp)
{
	rt->mfc_origin     = mfccp->mfcc_origin;
	rt->mfc_mcastgrp   = mfccp->mfcc_mcastgrp;

	update_mfc_params(rt, mfccp);

	/* initialize pkt counters per src-grp */
	rt->mfc_pkt_cnt    = 0;
	rt->mfc_byte_cnt   = 0;
	rt->mfc_wrong_if   = 0;
	timevalclear(&rt->mfc_last_assert);
}

static void
expire_mfc(struct mfc *rt)
{
	struct rtdetq *rte;

	MRW_WLOCK_ASSERT();

	free_bw_list(rt->mfc_bw_meter_leq);
	free_bw_list(rt->mfc_bw_meter_geq);

	while (!buf_ring_empty(rt->mfc_stall_ring)) {
		rte = buf_ring_dequeue_mc(rt->mfc_stall_ring);
		if (rte) {
			m_freem(rte->m);
			free(rte, M_MRTABLE);
		}
	}
	buf_ring_free(rt->mfc_stall_ring, M_MRTABLE);

	LIST_REMOVE(rt, mfc_hash);
	free(rt, M_MRTABLE);
}

/*
 * Add an mfc entry
 */
static int
add_mfc(struct mfcctl2 *mfccp)
{
	struct mfc *rt;
	struct rtdetq *rte;
	u_long hash = 0;
	u_short nstl;
	struct epoch_tracker et;

	MRW_WLOCK();
	rt = mfc_find(&mfccp->mfcc_origin, &mfccp->mfcc_mcastgrp);

	/* If an entry already exists, just update the fields */
	if (rt) {
		CTR4(KTR_IPMF, "%s: update mfc orig 0x%08x group %lx parent %x",
		    __func__, ntohl(mfccp->mfcc_origin.s_addr),
		    (u_long)ntohl(mfccp->mfcc_mcastgrp.s_addr),
		    mfccp->mfcc_parent);
		update_mfc_params(rt, mfccp);
		MRW_WUNLOCK();
		return (0);
	}

	/*
	 * Find the entry for which the upcall was made and update
	 */
	nstl = 0;
	hash = MFCHASH(mfccp->mfcc_origin, mfccp->mfcc_mcastgrp);
	NET_EPOCH_ENTER(et);
	LIST_FOREACH(rt, &V_mfchashtbl[hash], mfc_hash) {
		if (in_hosteq(rt->mfc_origin, mfccp->mfcc_origin) &&
		    in_hosteq(rt->mfc_mcastgrp, mfccp->mfcc_mcastgrp) &&
		    !buf_ring_empty(rt->mfc_stall_ring)) {
			CTR5(KTR_IPMF,
			   "%s: add mfc orig 0x%08x group %lx parent %x qh %p",
			    __func__, ntohl(mfccp->mfcc_origin.s_addr),
			    (u_long)ntohl(mfccp->mfcc_mcastgrp.s_addr),
			    mfccp->mfcc_parent,
			    rt->mfc_stall_ring);
			if (nstl++)
				CTR1(KTR_IPMF, "%s: multiple matches", __func__);

			init_mfc_params(rt, mfccp);
			rt->mfc_expire = 0;	/* Don't clean this guy up */
			V_nexpire[hash]--;

			/* Free queued packets, but attempt to forward them first. */
			while (!buf_ring_empty(rt->mfc_stall_ring)) {
				rte = buf_ring_dequeue_mc(rt->mfc_stall_ring);
				if (rte->ifp != NULL)
					ip_mdq(rte->m, rte->ifp, rt, -1);
				m_freem(rte->m);
				free(rte, M_MRTABLE);
			}
		}
	}
	NET_EPOCH_EXIT(et);

	/*
	 * It is possible that an entry is being inserted without an upcall
	 */
	if (nstl == 0) {
		CTR1(KTR_IPMF, "%s: adding mfc w/o upcall", __func__);
		LIST_FOREACH(rt, &V_mfchashtbl[hash], mfc_hash) {
			if (in_hosteq(rt->mfc_origin, mfccp->mfcc_origin) &&
			    in_hosteq(rt->mfc_mcastgrp, mfccp->mfcc_mcastgrp)) {
				init_mfc_params(rt, mfccp);
				if (rt->mfc_expire)
					V_nexpire[hash]--;
				rt->mfc_expire = 0;
				break; /* XXX */
			}
		}

		if (rt == NULL) {		/* no upcall, so make a new entry */
			rt = mfc_alloc();
			if (rt == NULL) {
				MRW_WUNLOCK();
				return (ENOBUFS);
			}

			init_mfc_params(rt, mfccp);

			rt->mfc_expire     = 0;
			rt->mfc_bw_meter_leq = NULL;
			rt->mfc_bw_meter_geq = NULL;

			/* insert new entry at head of hash chain */
			LIST_INSERT_HEAD(&V_mfchashtbl[hash], rt, mfc_hash);
		}
	}

	MRW_WUNLOCK();

	return (0);
}

/*
 * Delete an mfc entry
 */
static int
del_mfc(struct mfcctl2 *mfccp)
{
	struct in_addr origin;
	struct in_addr mcastgrp;
	struct mfc *rt;

	origin = mfccp->mfcc_origin;
	mcastgrp = mfccp->mfcc_mcastgrp;

	CTR3(KTR_IPMF, "%s: delete mfc orig 0x%08x group %lx", __func__,
			ntohl(origin.s_addr), (u_long)ntohl(mcastgrp.s_addr));

	MRW_WLOCK();

	LIST_FOREACH(rt, &V_mfchashtbl[MFCHASH(origin, mcastgrp)], mfc_hash) {
		if (in_hosteq(rt->mfc_origin, origin) &&
		    in_hosteq(rt->mfc_mcastgrp, mcastgrp))
			break;
	}
	if (rt == NULL) {
		MRW_WUNLOCK();
		return EADDRNOTAVAIL;
	}

	expire_mfc(rt);

	MRW_WUNLOCK();

	return (0);
}

/*
 * Send a message to the routing daemon on the multicast routing socket.
 */
static int
socket_send(struct socket *s, struct mbuf *mm, struct sockaddr_in *src)
{
	if (s) {
		SOCKBUF_LOCK(&s->so_rcv);
		if (sbappendaddr_locked(&s->so_rcv, (struct sockaddr *)src, mm,
		    NULL) != 0) {
			sorwakeup_locked(s);
			return 0;
		}
		soroverflow_locked(s);
	}
	m_freem(mm);
	return -1;
}

/*
 * IP multicast forwarding function. This function assumes that the packet
 * pointed to by "ip" has arrived on (or is about to be sent to) the interface
 * pointed to by "ifp", and the packet is to be relayed to other networks
 * that have members of the packet's destination IP multicast group.
 *
 * The packet is returned unscathed to the caller, unless it is
 * erroneous, in which case a non-zero return value tells the caller to
 * discard it.
 */

#define TUNNEL_LEN  12  /* # bytes of IP option for tunnel encapsulation  */

static int
X_ip_mforward(struct ip *ip, struct ifnet *ifp, struct mbuf *m,
    struct ip_moptions *imo)
{
	struct mfc *rt;
	int error;
	vifi_t vifi;
	struct mbuf *mb0;
	struct rtdetq *rte;
	u_long hash;
	int hlen;

	M_ASSERTMAPPED(m);

	CTR3(KTR_IPMF, "ip_mforward: delete mfc orig 0x%08x group %lx ifp %p",
	    ntohl(ip->ip_src.s_addr), (u_long)ntohl(ip->ip_dst.s_addr), ifp);

	if (ip->ip_hl < (sizeof(struct ip) + TUNNEL_LEN) >> 2 ||
	    ((u_char *)(ip + 1))[1] != IPOPT_LSRR) {
		/*
		 * Packet arrived via a physical interface or
		 * an encapsulated tunnel or a register_vif.
		 */
	} else {
		/*
		 * Packet arrived through a source-route tunnel.
		 * Source-route tunnels are no longer supported.
		 */
		return (1);
	}

	/*
	 * BEGIN: MCAST ROUTING HOT PATH
	 */
	MRW_RLOCK();
	if (imo && ((vifi = imo->imo_multicast_vif) < V_numvifs)) {
		if (ip->ip_ttl < MAXTTL)
			ip->ip_ttl++; /* compensate for -1 in *_send routines */
		error = ip_mdq(m, ifp, NULL, vifi);
		MRW_RUNLOCK();
		return error;
	}

	/*
	 * Don't forward a packet with time-to-live of zero or one,
	 * or a packet destined to a local-only group.
	 */
	if (ip->ip_ttl <= 1 || IN_LOCAL_GROUP(ntohl(ip->ip_dst.s_addr))) {
		MRW_RUNLOCK();
		return 0;
	}

mfc_find_retry:
	/*
	 * Determine forwarding vifs from the forwarding cache table
	 */
	MRTSTAT_INC(mrts_mfc_lookups);
	rt = mfc_find(&ip->ip_src, &ip->ip_dst);

	/* Entry exists, so forward if necessary */
	if (rt != NULL) {
		error = ip_mdq(m, ifp, rt, -1);
		/* Generic unlock here as we might release R or W lock */
		MRW_UNLOCK();
		return error;
	}

	/*
	 * END: MCAST ROUTING HOT PATH
	 */

	/* Further processing must be done with WLOCK taken */
	if ((MRW_WOWNED() == 0) && (MRW_LOCK_TRY_UPGRADE() == 0)) {
		MRW_RUNLOCK();
		MRW_WLOCK();
		goto mfc_find_retry;
	}

	/*
	 * If we don't have a route for packet's origin,
	 * Make a copy of the packet & send message to routing daemon
	 */
	hlen = ip->ip_hl << 2;

	MRTSTAT_INC(mrts_mfc_misses);
	MRTSTAT_INC(mrts_no_route);
	CTR2(KTR_IPMF, "ip_mforward: no mfc for (0x%08x,%lx)",
	    ntohl(ip->ip_src.s_addr), (u_long)ntohl(ip->ip_dst.s_addr));

	/*
	 * Allocate mbufs early so that we don't do extra work if we are
	 * just going to fail anyway.  Make sure to pullup the header so
	 * that other people can't step on it.
	 */
	rte = malloc((sizeof *rte), M_MRTABLE, M_NOWAIT|M_ZERO);
	if (rte == NULL) {
		MRW_WUNLOCK();
		return ENOBUFS;
	}

	mb0 = m_copypacket(m, M_NOWAIT);
	if (mb0 && (!M_WRITABLE(mb0) || mb0->m_len < hlen))
		mb0 = m_pullup(mb0, hlen);
	if (mb0 == NULL) {
		free(rte, M_MRTABLE);
		MRW_WUNLOCK();
		return ENOBUFS;
	}

	/* is there an upcall waiting for this flow ? */
	hash = MFCHASH(ip->ip_src, ip->ip_dst);
	LIST_FOREACH(rt, &V_mfchashtbl[hash], mfc_hash)
	{
		if (in_hosteq(ip->ip_src, rt->mfc_origin) &&
		    in_hosteq(ip->ip_dst, rt->mfc_mcastgrp) &&
		    !buf_ring_empty(rt->mfc_stall_ring))
			break;
	}

	if (rt == NULL) {
		int i;
		struct igmpmsg *im;
		struct sockaddr_in k_igmpsrc = { sizeof k_igmpsrc, AF_INET };
		struct mbuf *mm;

		/*
		 * Locate the vifi for the incoming interface for this packet.
		 * If none found, drop packet.
		 */
		for (vifi = 0; vifi < V_numvifs &&
		    V_viftable[vifi].v_ifp != ifp; vifi++)
			;
		if (vifi >= V_numvifs) /* vif not found, drop packet */
			goto non_fatal;

		/* no upcall, so make a new entry */
		rt = mfc_alloc();
		if (rt == NULL)
			goto fail;

		/* Make a copy of the header to send to the user level process */
		mm = m_copym(mb0, 0, hlen, M_NOWAIT);
		if (mm == NULL)
			goto fail1;

		/*
		 * Send message to routing daemon to install
		 * a route into the kernel table
		 */

		im = mtod(mm, struct igmpmsg*);
		im->im_msgtype = IGMPMSG_NOCACHE;
		im->im_mbz = 0;
		im->im_vif = vifi;

		MRTSTAT_INC(mrts_upcalls);

		k_igmpsrc.sin_addr = ip->ip_src;
		if (socket_send(V_ip_mrouter, mm, &k_igmpsrc) < 0) {
			CTR0(KTR_IPMF, "ip_mforward: socket queue full");
			MRTSTAT_INC(mrts_upq_sockfull);
			fail1: free(rt, M_MRTABLE);
			fail: free(rte, M_MRTABLE);
			m_freem(mb0);
			MRW_WUNLOCK();
			return ENOBUFS;
		}

		/* insert new entry at head of hash chain */
		rt->mfc_origin.s_addr = ip->ip_src.s_addr;
		rt->mfc_mcastgrp.s_addr = ip->ip_dst.s_addr;
		rt->mfc_expire = UPCALL_EXPIRE;
		V_nexpire[hash]++;
		for (i = 0; i < V_numvifs; i++) {
			rt->mfc_ttls[i] = 0;
			rt->mfc_flags[i] = 0;
		}
		rt->mfc_parent = -1;

		/* clear the RP address */
		rt->mfc_rp.s_addr = INADDR_ANY;
		rt->mfc_bw_meter_leq = NULL;
		rt->mfc_bw_meter_geq = NULL;

		/* initialize pkt counters per src-grp */
		rt->mfc_pkt_cnt = 0;
		rt->mfc_byte_cnt = 0;
		rt->mfc_wrong_if = 0;
		timevalclear(&rt->mfc_last_assert);

		buf_ring_enqueue(rt->mfc_stall_ring, rte);

		/* Add RT to hashtable as it didn't exist before */
		LIST_INSERT_HEAD(&V_mfchashtbl[hash], rt, mfc_hash);
	} else {
		/* determine if queue has overflowed */
		if (buf_ring_full(rt->mfc_stall_ring)) {
			MRTSTAT_INC(mrts_upq_ovflw);
			non_fatal: free(rte, M_MRTABLE);
			m_freem(mb0);
			MRW_WUNLOCK();
			return (0);
		}

		buf_ring_enqueue(rt->mfc_stall_ring, rte);
	}

	rte->m = mb0;
	rte->ifp = ifp;

	MRW_WUNLOCK();

	return 0;
}

/*
 * Clean up the cache entry if upcall is not serviced
 */
static void
expire_upcalls(void *arg)
{
	u_long i;

	CURVNET_SET((struct vnet *) arg);

	/*This callout is always run with MRW_WLOCK taken. */

	for (i = 0; i < mfchashsize; i++) {
		struct mfc *rt, *nrt;

		if (V_nexpire[i] == 0)
			continue;

		LIST_FOREACH_SAFE(rt, &V_mfchashtbl[i], mfc_hash, nrt) {
			if (buf_ring_empty(rt->mfc_stall_ring))
				continue;

			if (rt->mfc_expire == 0 || --rt->mfc_expire > 0)
				continue;

			MRTSTAT_INC(mrts_cache_cleanups);
			CTR3(KTR_IPMF, "%s: expire (%lx, %lx)", __func__,
			    (u_long)ntohl(rt->mfc_origin.s_addr),
			    (u_long)ntohl(rt->mfc_mcastgrp.s_addr));

			expire_mfc(rt);
		}
	}

	callout_reset(&V_expire_upcalls_ch, EXPIRE_TIMEOUT, expire_upcalls,
	    curvnet);

	CURVNET_RESTORE();
}

/*
 * Packet forwarding routine once entry in the cache is made
 */
static int
ip_mdq(struct mbuf *m, struct ifnet *ifp, struct mfc *rt, vifi_t xmt_vif)
{
	struct ip *ip = mtod(m, struct ip *);
	vifi_t vifi;
	int plen = ntohs(ip->ip_len);

	M_ASSERTMAPPED(m);
	MRW_LOCK_ASSERT();
	NET_EPOCH_ASSERT();

	/*
	 * If xmt_vif is not -1, send on only the requested vif.
	 *
	 * (since vifi_t is u_short, -1 becomes MAXUSHORT, which > numvifs.)
	 */
	if (xmt_vif < V_numvifs) {
		if (V_viftable[xmt_vif].v_flags & VIFF_REGISTER)
			pim_register_send(ip, V_viftable + xmt_vif, m, rt);
		else
			phyint_send(ip, V_viftable + xmt_vif, m);
		return 1;
	}

	/*
	 * Don't forward if it didn't arrive from the parent vif for its origin.
	 */
	vifi = rt->mfc_parent;
	if ((vifi >= V_numvifs) || (V_viftable[vifi].v_ifp != ifp)) {
		CTR4(KTR_IPMF, "%s: rx on wrong ifp %p (vifi %d, v_ifp %p)",
				__func__, ifp, (int)vifi, V_viftable[vifi].v_ifp);
		MRTSTAT_INC(mrts_wrong_if);
		++rt->mfc_wrong_if;
		/*
		 * If we are doing PIM assert processing, send a message
		 * to the routing daemon.
		 *
		 * XXX: A PIM-SM router needs the WRONGVIF detection so it
		 * can complete the SPT switch, regardless of the type
		 * of the iif (broadcast media, GRE tunnel, etc).
		 */
		if (V_pim_assert_enabled && (vifi < V_numvifs) &&
		    V_viftable[vifi].v_ifp) {
			if (ifp == V_multicast_register_if)
				PIMSTAT_INC(pims_rcv_registers_wrongiif);

			/* Get vifi for the incoming packet */
			for (vifi = 0; vifi < V_numvifs && V_viftable[vifi].v_ifp != ifp; vifi++)
				;
			if (vifi >= V_numvifs)
				return 0;	/* The iif is not found: ignore the packet. */

			if (rt->mfc_flags[vifi] & MRT_MFC_FLAGS_DISABLE_WRONGVIF)
				return 0;	/* WRONGVIF disabled: ignore the packet */

			if (ratecheck(&rt->mfc_last_assert, &pim_assert_interval)) {
				struct sockaddr_in k_igmpsrc = { sizeof k_igmpsrc, AF_INET };
				struct igmpmsg *im;
				int hlen = ip->ip_hl << 2;
				struct mbuf *mm = m_copym(m, 0, hlen, M_NOWAIT);

				if (mm && (!M_WRITABLE(mm) || mm->m_len < hlen))
					mm = m_pullup(mm, hlen);
				if (mm == NULL)
					return ENOBUFS;

				im = mtod(mm, struct igmpmsg *);
				im->im_msgtype = IGMPMSG_WRONGVIF;
				im->im_mbz = 0;
				im->im_vif = vifi;

				MRTSTAT_INC(mrts_upcalls);

				k_igmpsrc.sin_addr = im->im_src;
				if (socket_send(V_ip_mrouter, mm, &k_igmpsrc) < 0) {
					CTR1(KTR_IPMF, "%s: socket queue full", __func__);
					MRTSTAT_INC(mrts_upq_sockfull);
					return ENOBUFS;
				}
			}
		}
		return 0;
	}

	/* If I sourced this packet, it counts as output, else it was input. */
	mtx_lock_spin(&V_viftable[vifi].v_spin);
	if (in_hosteq(ip->ip_src, V_viftable[vifi].v_lcl_addr)) {
		V_viftable[vifi].v_pkt_out++;
		V_viftable[vifi].v_bytes_out += plen;
	} else {
		V_viftable[vifi].v_pkt_in++;
		V_viftable[vifi].v_bytes_in += plen;
	}
	mtx_unlock_spin(&V_viftable[vifi].v_spin);

	rt->mfc_pkt_cnt++;
	rt->mfc_byte_cnt += plen;

	/*
	 * For each vif, decide if a copy of the packet should be forwarded.
	 * Forward if:
	 *		- the ttl exceeds the vif's threshold
	 *		- there are group members downstream on interface
	 */
	for (vifi = 0; vifi < V_numvifs; vifi++)
		if ((rt->mfc_ttls[vifi] > 0) && (ip->ip_ttl > rt->mfc_ttls[vifi])) {
			V_viftable[vifi].v_pkt_out++;
			V_viftable[vifi].v_bytes_out += plen;
			if (V_viftable[vifi].v_flags & VIFF_REGISTER)
				pim_register_send(ip, V_viftable + vifi, m, rt);
			else
				phyint_send(ip, V_viftable + vifi, m);
		}

	/*
	 * Perform upcall-related bw measuring.
	 */
	if ((rt->mfc_bw_meter_geq != NULL) || (rt->mfc_bw_meter_leq != NULL)) {
		struct bw_meter *x;
		struct timeval now;

		microtime(&now);
		/* Process meters for Greater-or-EQual case */
		for (x = rt->mfc_bw_meter_geq; x != NULL; x = x->bm_mfc_next)
			bw_meter_geq_receive_packet(x, plen, &now);

		/* Process meters for Lower-or-EQual case */
		for (x = rt->mfc_bw_meter_leq; x != NULL; x = x->bm_mfc_next) {
			/*
			 * Record that a packet is received.
			 * Spin lock has to be taken as callout context
			 * (expire_bw_meter_leq) might modify these fields
			 * as well
			 */
			mtx_lock_spin(&x->bm_spin);
			x->bm_measured.b_packets++;
			x->bm_measured.b_bytes += plen;
			mtx_unlock_spin(&x->bm_spin);
		}
	}

	return 0;
}

/*
 * Check if a vif number is legal/ok. This is used by in_mcast.c.
 */
static int
X_legal_vif_num(int vif)
{
	int ret;

	ret = 0;
	if (vif < 0)
		return (ret);

	MRW_RLOCK();
	if (vif < V_numvifs)
		ret = 1;
	MRW_RUNLOCK();

	return (ret);
}

/*
 * Return the local address used by this vif
 */
static u_long
X_ip_mcast_src(int vifi)
{
	in_addr_t addr;

	addr = INADDR_ANY;
	if (vifi < 0)
		return (addr);

	MRW_RLOCK();
	if (vifi < V_numvifs)
		addr = V_viftable[vifi].v_lcl_addr.s_addr;
	MRW_RUNLOCK();

	return (addr);
}

static void
phyint_send(struct ip *ip, struct vif *vifp, struct mbuf *m)
{
	struct mbuf *mb_copy;
	int hlen = ip->ip_hl << 2;

	MRW_LOCK_ASSERT();
	M_ASSERTMAPPED(m);

	/*
	 * Make a new reference to the packet; make sure that
	 * the IP header is actually copied, not just referenced,
	 * so that ip_output() only scribbles on the copy.
	 */
	mb_copy = m_copypacket(m, M_NOWAIT);
	if (mb_copy && (!M_WRITABLE(mb_copy) || mb_copy->m_len < hlen))
		mb_copy = m_pullup(mb_copy, hlen);
	if (mb_copy == NULL)
		return;

	send_packet(vifp, mb_copy);
}

static void
send_packet(struct vif *vifp, struct mbuf *m)
{
	struct ip_moptions imo;
	int error __unused;

	MRW_LOCK_ASSERT();
	NET_EPOCH_ASSERT();

	imo.imo_multicast_ifp  = vifp->v_ifp;
	imo.imo_multicast_ttl  = mtod(m, struct ip *)->ip_ttl - 1;
	imo.imo_multicast_loop = !!in_mcast_loop;
	imo.imo_multicast_vif  = -1;
	STAILQ_INIT(&imo.imo_head);

	/*
	 * Re-entrancy should not be a problem here, because
	 * the packets that we send out and are looped back at us
	 * should get rejected because they appear to come from
	 * the loopback interface, thus preventing looping.
	 */
	error = ip_output(m, NULL, NULL, IP_FORWARDING, &imo, NULL);
	CTR3(KTR_IPMF, "%s: vif %td err %d", __func__,
	    (ptrdiff_t)(vifp - V_viftable), error);
}

/*
 * Stubs for old RSVP socket shim implementation.
 */

static int
X_ip_rsvp_vif(struct socket *so __unused, struct sockopt *sopt __unused)
{

	return (EOPNOTSUPP);
}

static void
X_ip_rsvp_force_done(struct socket *so __unused)
{

}

static int
X_rsvp_input(struct mbuf **mp, int *offp, int proto)
{
	struct mbuf *m;

	m = *mp;
	*mp = NULL;
	if (!V_rsvp_on)
		m_freem(m);
	return (IPPROTO_DONE);
}

/*
 * Code for bandwidth monitors
 */

/*
 * Define common interface for timeval-related methods
 */
#define	BW_TIMEVALCMP(tvp, uvp, cmp) timevalcmp((tvp), (uvp), cmp)
#define	BW_TIMEVALDECR(vvp, uvp) timevalsub((vvp), (uvp))
#define	BW_TIMEVALADD(vvp, uvp) timevaladd((vvp), (uvp))

static uint32_t
compute_bw_meter_flags(struct bw_upcall *req)
{
	uint32_t flags = 0;

	if (req->bu_flags & BW_UPCALL_UNIT_PACKETS)
		flags |= BW_METER_UNIT_PACKETS;
	if (req->bu_flags & BW_UPCALL_UNIT_BYTES)
		flags |= BW_METER_UNIT_BYTES;
	if (req->bu_flags & BW_UPCALL_GEQ)
		flags |= BW_METER_GEQ;
	if (req->bu_flags & BW_UPCALL_LEQ)
		flags |= BW_METER_LEQ;

	return flags;
}

static void
expire_bw_meter_leq(void *arg)
{
	struct bw_meter *x = arg;
	struct timeval now;
	/*
	 * INFO:
	 * callout is always executed with MRW_WLOCK taken
	 */

	CURVNET_SET((struct vnet *)x->arg);

	microtime(&now);

	/*
	 * Test if we should deliver an upcall
	 */
	if (((x->bm_flags & BW_METER_UNIT_PACKETS) &&
	    (x->bm_measured.b_packets <= x->bm_threshold.b_packets)) ||
	    ((x->bm_flags & BW_METER_UNIT_BYTES) &&
	    (x->bm_measured.b_bytes <= x->bm_threshold.b_bytes))) {
		/* Prepare an upcall for delivery */
		bw_meter_prepare_upcall(x, &now);
	}

	/* Send all upcalls that are pending delivery */
	taskqueue_enqueue(V_task_queue, &V_task);

	/* Reset counters */
	x->bm_start_time = now;
	/* Spin lock has to be taken as ip_forward context
	 * might modify these fields as well
	 */
	mtx_lock_spin(&x->bm_spin);
	x->bm_measured.b_bytes = 0;
	x->bm_measured.b_packets = 0;
	mtx_unlock_spin(&x->bm_spin);

	callout_schedule(&x->bm_meter_callout, tvtohz(&x->bm_threshold.b_time));

	CURVNET_RESTORE();
}

/*
 * Add a bw_meter entry
 */
static int
add_bw_upcall(struct bw_upcall *req)
{
	struct mfc *mfc;
	struct timeval delta = { BW_UPCALL_THRESHOLD_INTERVAL_MIN_SEC,
	BW_UPCALL_THRESHOLD_INTERVAL_MIN_USEC };
	struct timeval now;
	struct bw_meter *x, **bwm_ptr;
	uint32_t flags;

	if (!(V_mrt_api_config & MRT_MFC_BW_UPCALL))
		return EOPNOTSUPP;

	/* Test if the flags are valid */
	if (!(req->bu_flags & (BW_UPCALL_UNIT_PACKETS | BW_UPCALL_UNIT_BYTES)))
		return EINVAL;
	if (!(req->bu_flags & (BW_UPCALL_GEQ | BW_UPCALL_LEQ)))
		return EINVAL;
	if ((req->bu_flags & (BW_UPCALL_GEQ | BW_UPCALL_LEQ)) == (BW_UPCALL_GEQ | BW_UPCALL_LEQ))
		return EINVAL;

	/* Test if the threshold time interval is valid */
	if (BW_TIMEVALCMP(&req->bu_threshold.b_time, &delta, <))
		return EINVAL;

	flags = compute_bw_meter_flags(req);

	/*
	 * Find if we have already same bw_meter entry
	 */
	MRW_WLOCK();
	mfc = mfc_find(&req->bu_src, &req->bu_dst);
	if (mfc == NULL) {
		MRW_WUNLOCK();
		return EADDRNOTAVAIL;
	}

	/* Choose an appropriate bw_meter list */
	if (req->bu_flags & BW_UPCALL_GEQ)
		bwm_ptr = &mfc->mfc_bw_meter_geq;
	else
		bwm_ptr = &mfc->mfc_bw_meter_leq;

	for (x = *bwm_ptr; x != NULL; x = x->bm_mfc_next) {
		if ((BW_TIMEVALCMP(&x->bm_threshold.b_time,
		    &req->bu_threshold.b_time, ==))
		    && (x->bm_threshold.b_packets
		    == req->bu_threshold.b_packets)
		    && (x->bm_threshold.b_bytes
		    == req->bu_threshold.b_bytes)
		    && (x->bm_flags & BW_METER_USER_FLAGS)
		    == flags) {
			MRW_WUNLOCK();
			return 0; /* XXX Already installed */
		}
	}

	/* Allocate the new bw_meter entry */
	x = malloc(sizeof(*x), M_BWMETER, M_ZERO | M_NOWAIT);
	if (x == NULL) {
		MRW_WUNLOCK();
		return ENOBUFS;
	}

	/* Set the new bw_meter entry */
	x->bm_threshold.b_time = req->bu_threshold.b_time;
	microtime(&now);
	x->bm_start_time = now;
	x->bm_threshold.b_packets = req->bu_threshold.b_packets;
	x->bm_threshold.b_bytes = req->bu_threshold.b_bytes;
	x->bm_measured.b_packets = 0;
	x->bm_measured.b_bytes = 0;
	x->bm_flags = flags;
	x->bm_time_next = NULL;
	x->bm_mfc = mfc;
	x->arg = curvnet;
	sprintf(x->bm_spin_name, "BM spin %p", x);
	mtx_init(&x->bm_spin, x->bm_spin_name, NULL, MTX_SPIN);

	/* For LEQ case create periodic callout */
	if (req->bu_flags & BW_UPCALL_LEQ) {
		callout_init_rw(&x->bm_meter_callout, &mrouter_lock, CALLOUT_SHAREDLOCK);
		callout_reset(&x->bm_meter_callout, tvtohz(&x->bm_threshold.b_time),
		    expire_bw_meter_leq, x);
	}

	/* Add the new bw_meter entry to the front of entries for this MFC */
	x->bm_mfc_next = *bwm_ptr;
	*bwm_ptr = x;

	MRW_WUNLOCK();

	return 0;
}

static void
free_bw_list(struct bw_meter *list)
{
	while (list != NULL) {
		struct bw_meter *x = list;

		/* MRW_WLOCK must be held here */
		if (x->bm_flags & BW_METER_LEQ) {
			callout_drain(&x->bm_meter_callout);
			mtx_destroy(&x->bm_spin);
		}

		list = list->bm_mfc_next;
		free(x, M_BWMETER);
	}
}

/*
 * Delete one or multiple bw_meter entries
 */
static int
del_bw_upcall(struct bw_upcall *req)
{
	struct mfc *mfc;
	struct bw_meter *x, **bwm_ptr;

	if (!(V_mrt_api_config & MRT_MFC_BW_UPCALL))
		return EOPNOTSUPP;

	MRW_WLOCK();

	/* Find the corresponding MFC entry */
	mfc = mfc_find(&req->bu_src, &req->bu_dst);
	if (mfc == NULL) {
		MRW_WUNLOCK();
		return EADDRNOTAVAIL;
	} else if (req->bu_flags & BW_UPCALL_DELETE_ALL) {
		/*
		 * Delete all bw_meter entries for this mfc
		 */
		struct bw_meter *list;

		/* Free LEQ list */
		list = mfc->mfc_bw_meter_leq;
		mfc->mfc_bw_meter_leq = NULL;
		free_bw_list(list);

		/* Free GEQ list */
		list = mfc->mfc_bw_meter_geq;
		mfc->mfc_bw_meter_geq = NULL;
		free_bw_list(list);
		MRW_WUNLOCK();
		return 0;
	} else {			/* Delete a single bw_meter entry */
		struct bw_meter *prev;
		uint32_t flags = 0;

		flags = compute_bw_meter_flags(req);

		/* Choose an appropriate bw_meter list */
		if (req->bu_flags & BW_UPCALL_GEQ)
			bwm_ptr = &mfc->mfc_bw_meter_geq;
		else
			bwm_ptr = &mfc->mfc_bw_meter_leq;

		/* Find the bw_meter entry to delete */
		for (prev = NULL, x = *bwm_ptr; x != NULL;
				prev = x, x = x->bm_mfc_next) {
			if ((BW_TIMEVALCMP(&x->bm_threshold.b_time, &req->bu_threshold.b_time, ==)) &&
			    (x->bm_threshold.b_packets == req->bu_threshold.b_packets) &&
			    (x->bm_threshold.b_bytes == req->bu_threshold.b_bytes) &&
			    (x->bm_flags & BW_METER_USER_FLAGS) == flags)
				break;
		}
		if (x != NULL) { /* Delete entry from the list for this MFC */
			if (prev != NULL)
				prev->bm_mfc_next = x->bm_mfc_next;	/* remove from middle*/
			else
				*bwm_ptr = x->bm_mfc_next;/* new head of list */

			if (req->bu_flags & BW_UPCALL_LEQ)
				callout_stop(&x->bm_meter_callout);

			MRW_WUNLOCK();
			/* Free the bw_meter entry */
			free(x, M_BWMETER);
			return 0;
		} else {
			MRW_WUNLOCK();
			return EINVAL;
		}
	}
	__assert_unreachable();
}

/*
 * Perform bandwidth measurement processing that may result in an upcall
 */
static void
bw_meter_geq_receive_packet(struct bw_meter *x, int plen, struct timeval *nowp)
{
	struct timeval delta;

	MRW_LOCK_ASSERT();

	delta = *nowp;
	BW_TIMEVALDECR(&delta, &x->bm_start_time);

	/*
	 * Processing for ">=" type of bw_meter entry.
	 * bm_spin does not have to be hold here as in GEQ
	 * case this is the only context accessing bm_measured.
	 */
	if (BW_TIMEVALCMP(&delta, &x->bm_threshold.b_time, >)) {
	    /* Reset the bw_meter entry */
	    x->bm_start_time = *nowp;
	    x->bm_measured.b_packets = 0;
	    x->bm_measured.b_bytes = 0;
	    x->bm_flags &= ~BW_METER_UPCALL_DELIVERED;
	}

	/* Record that a packet is received */
	x->bm_measured.b_packets++;
	x->bm_measured.b_bytes += plen;

	/*
	 * Test if we should deliver an upcall
	 */
	if (!(x->bm_flags & BW_METER_UPCALL_DELIVERED)) {
		if (((x->bm_flags & BW_METER_UNIT_PACKETS) &&
		    (x->bm_measured.b_packets >= x->bm_threshold.b_packets)) ||
		    ((x->bm_flags & BW_METER_UNIT_BYTES) &&
		    (x->bm_measured.b_bytes >= x->bm_threshold.b_bytes))) {
			/* Prepare an upcall for delivery */
			bw_meter_prepare_upcall(x, nowp);
			x->bm_flags |= BW_METER_UPCALL_DELIVERED;
		}
	}
}

/*
 * Prepare a bandwidth-related upcall
 */
static void
bw_meter_prepare_upcall(struct bw_meter *x, struct timeval *nowp)
{
	struct timeval delta;
	struct bw_upcall *u;

	MRW_LOCK_ASSERT();

	/*
	 * Compute the measured time interval
	 */
	delta = *nowp;
	BW_TIMEVALDECR(&delta, &x->bm_start_time);

	/*
	 * Set the bw_upcall entry
	 */
	u = malloc(sizeof(struct bw_upcall), M_MRTABLE, M_NOWAIT | M_ZERO);
	if (!u) {
		log(LOG_WARNING, "bw_meter_prepare_upcall: cannot allocate entry\n");
		return;
	}
	u->bu_src = x->bm_mfc->mfc_origin;
	u->bu_dst = x->bm_mfc->mfc_mcastgrp;
	u->bu_threshold.b_time = x->bm_threshold.b_time;
	u->bu_threshold.b_packets = x->bm_threshold.b_packets;
	u->bu_threshold.b_bytes = x->bm_threshold.b_bytes;
	u->bu_measured.b_time = delta;
	u->bu_measured.b_packets = x->bm_measured.b_packets;
	u->bu_measured.b_bytes = x->bm_measured.b_bytes;
	u->bu_flags = 0;
	if (x->bm_flags & BW_METER_UNIT_PACKETS)
		u->bu_flags |= BW_UPCALL_UNIT_PACKETS;
	if (x->bm_flags & BW_METER_UNIT_BYTES)
		u->bu_flags |= BW_UPCALL_UNIT_BYTES;
	if (x->bm_flags & BW_METER_GEQ)
		u->bu_flags |= BW_UPCALL_GEQ;
	if (x->bm_flags & BW_METER_LEQ)
		u->bu_flags |= BW_UPCALL_LEQ;

	if (buf_ring_enqueue(V_bw_upcalls_ring, u))
		log(LOG_WARNING, "bw_meter_prepare_upcall: cannot enqueue upcall\n");
	if (buf_ring_count(V_bw_upcalls_ring) > (BW_UPCALLS_MAX / 2)) {
		taskqueue_enqueue(V_task_queue, &V_task);
	}
}
/*
 * Send the pending bandwidth-related upcalls
 */
static void
bw_upcalls_send(void)
{
	struct mbuf *m;
	int len = 0;
	struct bw_upcall *bu;
	struct sockaddr_in k_igmpsrc = { sizeof k_igmpsrc, AF_INET };
	static struct igmpmsg igmpmsg = {
		0,		/* unused1 */
		0,		/* unused2 */
		IGMPMSG_BW_UPCALL,/* im_msgtype */
		0,		/* im_mbz  */
		0,		/* im_vif  */
		0,		/* unused3 */
		{ 0 },		/* im_src  */
		{ 0 }		/* im_dst  */
	};

	MRW_LOCK_ASSERT();

	if (buf_ring_empty(V_bw_upcalls_ring))
		return;

	/*
	 * Allocate a new mbuf, initialize it with the header and
	 * the payload for the pending calls.
	 */
	m = m_gethdr(M_NOWAIT, MT_DATA);
	if (m == NULL) {
		log(LOG_WARNING, "bw_upcalls_send: cannot allocate mbuf\n");
		return;
	}

	m_copyback(m, 0, sizeof(struct igmpmsg), (caddr_t)&igmpmsg);
	len += sizeof(struct igmpmsg);
	while ((bu = buf_ring_dequeue_mc(V_bw_upcalls_ring)) != NULL) {
		m_copyback(m, len, sizeof(struct bw_upcall), (caddr_t)bu);
		len += sizeof(struct bw_upcall);
		free(bu, M_MRTABLE);
	}

	/*
	 * Send the upcalls
	 * XXX do we need to set the address in k_igmpsrc ?
	 */
	MRTSTAT_INC(mrts_upcalls);
	if (socket_send(V_ip_mrouter, m, &k_igmpsrc) < 0) {
		log(LOG_WARNING, "bw_upcalls_send: ip_mrouter socket queue full\n");
		MRTSTAT_INC(mrts_upq_sockfull);
	}
}

/*
 * A periodic function for sending all upcalls that are pending delivery
 */
static void
expire_bw_upcalls_send(void *arg)
{
	CURVNET_SET((struct vnet *) arg);

	/* This callout is run with MRW_RLOCK taken */

	bw_upcalls_send();

	callout_reset(&V_bw_upcalls_ch, BW_UPCALLS_PERIOD, expire_bw_upcalls_send,
	    curvnet);
	CURVNET_RESTORE();
}

/*
 * End of bandwidth monitoring code
 */

/*
 * Send the packet up to the user daemon, or eventually do kernel encapsulation
 *
 */
static int
pim_register_send(struct ip *ip, struct vif *vifp, struct mbuf *m,
    struct mfc *rt)
{
	struct mbuf *mb_copy, *mm;

	/*
	 * Do not send IGMP_WHOLEPKT notifications to userland, if the
	 * rendezvous point was unspecified, and we were told not to.
	 */
	if (pim_squelch_wholepkt != 0 && (V_mrt_api_config & MRT_MFC_RP) &&
	    in_nullhost(rt->mfc_rp))
		return 0;

	mb_copy = pim_register_prepare(ip, m);
	if (mb_copy == NULL)
		return ENOBUFS;

	/*
	 * Send all the fragments. Note that the mbuf for each fragment
	 * is freed by the sending machinery.
	 */
	for (mm = mb_copy; mm; mm = mb_copy) {
		mb_copy = mm->m_nextpkt;
		mm->m_nextpkt = 0;
		mm = m_pullup(mm, sizeof(struct ip));
		if (mm != NULL) {
			ip = mtod(mm, struct ip *);
			if ((V_mrt_api_config & MRT_MFC_RP) && !in_nullhost(rt->mfc_rp)) {
				pim_register_send_rp(ip, vifp, mm, rt);
			} else {
				pim_register_send_upcall(ip, vifp, mm, rt);
			}
		}
	}

	return 0;
}

/*
 * Return a copy of the data packet that is ready for PIM Register
 * encapsulation.
 * XXX: Note that in the returned copy the IP header is a valid one.
 */
static struct mbuf *
pim_register_prepare(struct ip *ip, struct mbuf *m)
{
	struct mbuf *mb_copy = NULL;
	int mtu;

	/* Take care of delayed checksums */
	if (m->m_pkthdr.csum_flags & CSUM_DELAY_DATA) {
		in_delayed_cksum(m);
		m->m_pkthdr.csum_flags &= ~CSUM_DELAY_DATA;
	}

	/*
	 * Copy the old packet & pullup its IP header into the
	 * new mbuf so we can modify it.
	 */
	mb_copy = m_copypacket(m, M_NOWAIT);
	if (mb_copy == NULL)
		return NULL;
	mb_copy = m_pullup(mb_copy, ip->ip_hl << 2);
	if (mb_copy == NULL)
		return NULL;

	/* take care of the TTL */
	ip = mtod(mb_copy, struct ip *);
	--ip->ip_ttl;

	/* Compute the MTU after the PIM Register encapsulation */
	mtu = 0xffff - sizeof(pim_encap_iphdr) - sizeof(pim_encap_pimhdr);

	if (ntohs(ip->ip_len) <= mtu) {
		/* Turn the IP header into a valid one */
		ip->ip_sum = 0;
		ip->ip_sum = in_cksum(mb_copy, ip->ip_hl << 2);
	} else {
		/* Fragment the packet */
		mb_copy->m_pkthdr.csum_flags |= CSUM_IP;
		if (ip_fragment(ip, &mb_copy, mtu, 0) != 0) {
			m_freem(mb_copy);
			return NULL;
		}
	}
	return mb_copy;
}

/*
 * Send an upcall with the data packet to the user-level process.
 */
static int
pim_register_send_upcall(struct ip *ip, struct vif *vifp,
    struct mbuf *mb_copy, struct mfc *rt)
{
	struct mbuf *mb_first;
	int len = ntohs(ip->ip_len);
	struct igmpmsg *im;
	struct sockaddr_in k_igmpsrc = { sizeof k_igmpsrc, AF_INET };

	MRW_LOCK_ASSERT();

	/*
	 * Add a new mbuf with an upcall header
	 */
	mb_first = m_gethdr(M_NOWAIT, MT_DATA);
	if (mb_first == NULL) {
		m_freem(mb_copy);
		return ENOBUFS;
	}
	mb_first->m_data += max_linkhdr;
	mb_first->m_pkthdr.len = len + sizeof(struct igmpmsg);
	mb_first->m_len = sizeof(struct igmpmsg);
	mb_first->m_next = mb_copy;

	/* Send message to routing daemon */
	im = mtod(mb_first, struct igmpmsg *);
	im->im_msgtype	= IGMPMSG_WHOLEPKT;
	im->im_mbz		= 0;
	im->im_vif		= vifp - V_viftable;
	im->im_src		= ip->ip_src;
	im->im_dst		= ip->ip_dst;

	k_igmpsrc.sin_addr	= ip->ip_src;

	MRTSTAT_INC(mrts_upcalls);

	if (socket_send(V_ip_mrouter, mb_first, &k_igmpsrc) < 0) {
		CTR1(KTR_IPMF, "%s: socket queue full", __func__);
		MRTSTAT_INC(mrts_upq_sockfull);
		return ENOBUFS;
	}

	/* Keep statistics */
	PIMSTAT_INC(pims_snd_registers_msgs);
	PIMSTAT_ADD(pims_snd_registers_bytes, len);

	return 0;
}

/*
 * Encapsulate the data packet in PIM Register message and send it to the RP.
 */
static int
pim_register_send_rp(struct ip *ip, struct vif *vifp, struct mbuf *mb_copy,
    struct mfc *rt)
{
	struct mbuf *mb_first;
	struct ip *ip_outer;
	struct pim_encap_pimhdr *pimhdr;
	int len = ntohs(ip->ip_len);
	vifi_t vifi = rt->mfc_parent;

	MRW_LOCK_ASSERT();

	if ((vifi >= V_numvifs) || in_nullhost(V_viftable[vifi].v_lcl_addr)) {
		m_freem(mb_copy);
		return EADDRNOTAVAIL;		/* The iif vif is invalid */
	}

	/*
	 * Add a new mbuf with the encapsulating header
	 */
	mb_first = m_gethdr(M_NOWAIT, MT_DATA);
	if (mb_first == NULL) {
		m_freem(mb_copy);
		return ENOBUFS;
	}
	mb_first->m_data += max_linkhdr;
	mb_first->m_len = sizeof(pim_encap_iphdr) + sizeof(pim_encap_pimhdr);
	mb_first->m_next = mb_copy;

	mb_first->m_pkthdr.len = len + mb_first->m_len;

	/*
	 * Fill in the encapsulating IP and PIM header
	 */
	ip_outer = mtod(mb_first, struct ip *);
	*ip_outer = pim_encap_iphdr;
	ip_outer->ip_len = htons(len + sizeof(pim_encap_iphdr) +
			sizeof(pim_encap_pimhdr));
	ip_outer->ip_src = V_viftable[vifi].v_lcl_addr;
	ip_outer->ip_dst = rt->mfc_rp;
	/*
	 * Copy the inner header TOS to the outer header, and take care of the
	 * IP_DF bit.
	 */
	ip_outer->ip_tos = ip->ip_tos;
	if (ip->ip_off & htons(IP_DF))
		ip_outer->ip_off |= htons(IP_DF);
	ip_fillid(ip_outer, V_ip_random_id);
	pimhdr = (struct pim_encap_pimhdr *)((caddr_t)ip_outer
			+ sizeof(pim_encap_iphdr));
	*pimhdr = pim_encap_pimhdr;
	/* If the iif crosses a border, set the Border-bit */
	if (rt->mfc_flags[vifi] & MRT_MFC_FLAGS_BORDER_VIF & V_mrt_api_config)
		pimhdr->flags |= htonl(PIM_BORDER_REGISTER);

	mb_first->m_data += sizeof(pim_encap_iphdr);
	pimhdr->pim.pim_cksum = in_cksum(mb_first, sizeof(pim_encap_pimhdr));
	mb_first->m_data -= sizeof(pim_encap_iphdr);

	send_packet(vifp, mb_first);

	/* Keep statistics */
	PIMSTAT_INC(pims_snd_registers_msgs);
	PIMSTAT_ADD(pims_snd_registers_bytes, len);

	return 0;
}

/*
 * pim_encapcheck() is called by the encap4_input() path at runtime to
 * determine if a packet is for PIM; allowing PIM to be dynamically loaded
 * into the kernel.
 */
static int
pim_encapcheck(const struct mbuf *m __unused, int off __unused,
    int proto __unused, void *arg __unused)
{

	KASSERT(proto == IPPROTO_PIM, ("not for IPPROTO_PIM"));
	return (8);		/* claim the datagram. */
}

/*
 * PIM-SMv2 and PIM-DM messages processing.
 * Receives and verifies the PIM control messages, and passes them
 * up to the listening socket, using rip_input().
 * The only message with special processing is the PIM_REGISTER message
 * (used by PIM-SM): the PIM header is stripped off, and the inner packet
 * is passed to if_simloop().
 */
static int
pim_input(struct mbuf *m, int off, int proto, void *arg __unused)
{
	struct ip *ip = mtod(m, struct ip *);
	struct pim *pim;
	int iphlen = off;
	int minlen;
	int datalen = ntohs(ip->ip_len) - iphlen;
	int ip_tos;

	/* Keep statistics */
	PIMSTAT_INC(pims_rcv_total_msgs);
	PIMSTAT_ADD(pims_rcv_total_bytes, datalen);

	/*
	 * Validate lengths
	 */
	if (datalen < PIM_MINLEN) {
		PIMSTAT_INC(pims_rcv_tooshort);
		CTR3(KTR_IPMF, "%s: short packet (%d) from 0x%08x",
		    __func__, datalen, ntohl(ip->ip_src.s_addr));
		m_freem(m);
		return (IPPROTO_DONE);
	}

	/*
	 * If the packet is at least as big as a REGISTER, go agead
	 * and grab the PIM REGISTER header size, to avoid another
	 * possible m_pullup() later.
	 *
	 * PIM_MINLEN       == pimhdr + u_int32_t == 4 + 4 = 8
	 * PIM_REG_MINLEN   == pimhdr + reghdr + encap_iphdr == 4 + 4 + 20 = 28
	 */
	minlen = iphlen + (datalen >= PIM_REG_MINLEN ? PIM_REG_MINLEN : PIM_MINLEN);
	/*
	 * Get the IP and PIM headers in contiguous memory, and
	 * possibly the PIM REGISTER header.
	 */
	if (m->m_len < minlen && (m = m_pullup(m, minlen)) == NULL) {
		CTR1(KTR_IPMF, "%s: m_pullup() failed", __func__);
		return (IPPROTO_DONE);
	}

	/* m_pullup() may have given us a new mbuf so reset ip. */
	ip = mtod(m, struct ip *);
	ip_tos = ip->ip_tos;

	/* adjust mbuf to point to the PIM header */
	m->m_data += iphlen;
	m->m_len  -= iphlen;
	pim = mtod(m, struct pim *);

	/*
	 * Validate checksum. If PIM REGISTER, exclude the data packet.
	 *
	 * XXX: some older PIMv2 implementations don't make this distinction,
	 * so for compatibility reason perform the checksum over part of the
	 * message, and if error, then over the whole message.
	 */
	if (PIM_VT_T(pim->pim_vt) == PIM_REGISTER && in_cksum(m, PIM_MINLEN) == 0) {
		/* do nothing, checksum okay */
	} else if (in_cksum(m, datalen)) {
		PIMSTAT_INC(pims_rcv_badsum);
		CTR1(KTR_IPMF, "%s: invalid checksum", __func__);
		m_freem(m);
		return (IPPROTO_DONE);
	}

	/* PIM version check */
	if (PIM_VT_V(pim->pim_vt) < PIM_VERSION) {
		PIMSTAT_INC(pims_rcv_badversion);
		CTR3(KTR_IPMF, "%s: bad version %d expect %d", __func__,
		    (int)PIM_VT_V(pim->pim_vt), PIM_VERSION);
		m_freem(m);
		return (IPPROTO_DONE);
	}

	/* restore mbuf back to the outer IP */
	m->m_data -= iphlen;
	m->m_len  += iphlen;

	if (PIM_VT_T(pim->pim_vt) == PIM_REGISTER) {
		/*
		 * Since this is a REGISTER, we'll make a copy of the register
		 * headers ip + pim + u_int32 + encap_ip, to be passed up to the
		 * routing daemon.
		 */
		struct sockaddr_in dst = { sizeof(dst), AF_INET };
		struct mbuf *mcp;
		struct ip *encap_ip;
		u_int32_t *reghdr;
		struct ifnet *vifp;

		MRW_RLOCK();
		if ((V_reg_vif_num >= V_numvifs) || (V_reg_vif_num == VIFI_INVALID)) {
			MRW_RUNLOCK();
			CTR2(KTR_IPMF, "%s: register vif not set: %d", __func__,
			    (int)V_reg_vif_num);
			m_freem(m);
			return (IPPROTO_DONE);
		}
		/* XXX need refcnt? */
		vifp = V_viftable[V_reg_vif_num].v_ifp;
		MRW_RUNLOCK();

		/*
		 * Validate length
		 */
		if (datalen < PIM_REG_MINLEN) {
			PIMSTAT_INC(pims_rcv_tooshort);
			PIMSTAT_INC(pims_rcv_badregisters);
			CTR1(KTR_IPMF, "%s: register packet size too small", __func__);
			m_freem(m);
			return (IPPROTO_DONE);
		}

		reghdr = (u_int32_t *)(pim + 1);
		encap_ip = (struct ip *)(reghdr + 1);

		CTR3(KTR_IPMF, "%s: register: encap ip src 0x%08x len %d",
		    __func__, ntohl(encap_ip->ip_src.s_addr),
		    ntohs(encap_ip->ip_len));

		/* verify the version number of the inner packet */
		if (encap_ip->ip_v != IPVERSION) {
			PIMSTAT_INC(pims_rcv_badregisters);
			CTR1(KTR_IPMF, "%s: bad encap ip version", __func__);
			m_freem(m);
			return (IPPROTO_DONE);
		}

		/* verify the inner packet is destined to a mcast group */
		if (!IN_MULTICAST(ntohl(encap_ip->ip_dst.s_addr))) {
			PIMSTAT_INC(pims_rcv_badregisters);
			CTR2(KTR_IPMF, "%s: bad encap ip dest 0x%08x", __func__,
			    ntohl(encap_ip->ip_dst.s_addr));
			m_freem(m);
			return (IPPROTO_DONE);
		}

		/* If a NULL_REGISTER, pass it to the daemon */
		if ((ntohl(*reghdr) & PIM_NULL_REGISTER))
			goto pim_input_to_daemon;

		/*
		 * Copy the TOS from the outer IP header to the inner IP header.
		 */
		if (encap_ip->ip_tos != ip_tos) {
			/* Outer TOS -> inner TOS */
			encap_ip->ip_tos = ip_tos;
			/* Recompute the inner header checksum. Sigh... */

			/* adjust mbuf to point to the inner IP header */
			m->m_data += (iphlen + PIM_MINLEN);
			m->m_len  -= (iphlen + PIM_MINLEN);

			encap_ip->ip_sum = 0;
			encap_ip->ip_sum = in_cksum(m, encap_ip->ip_hl << 2);

			/* restore mbuf to point back to the outer IP header */
			m->m_data -= (iphlen + PIM_MINLEN);
			m->m_len  += (iphlen + PIM_MINLEN);
		}

		/*
		 * Decapsulate the inner IP packet and loopback to forward it
		 * as a normal multicast packet. Also, make a copy of the
		 *     outer_iphdr + pimhdr + reghdr + encap_iphdr
		 * to pass to the daemon later, so it can take the appropriate
		 * actions (e.g., send back PIM_REGISTER_STOP).
		 * XXX: here m->m_data points to the outer IP header.
		 */
		mcp = m_copym(m, 0, iphlen + PIM_REG_MINLEN, M_NOWAIT);
		if (mcp == NULL) {
			CTR1(KTR_IPMF, "%s: m_copym() failed", __func__);
			m_freem(m);
			return (IPPROTO_DONE);
		}

		/* Keep statistics */
		/* XXX: registers_bytes include only the encap. mcast pkt */
		PIMSTAT_INC(pims_rcv_registers_msgs);
		PIMSTAT_ADD(pims_rcv_registers_bytes, ntohs(encap_ip->ip_len));

		/*
		 * forward the inner ip packet; point m_data at the inner ip.
		 */
		m_adj(m, iphlen + PIM_MINLEN);

		CTR4(KTR_IPMF,
		    "%s: forward decap'd REGISTER: src %lx dst %lx vif %d",
		    __func__,
		    (u_long)ntohl(encap_ip->ip_src.s_addr),
		    (u_long)ntohl(encap_ip->ip_dst.s_addr),
		    (int)V_reg_vif_num);

		/* NB: vifp was collected above; can it change on us? */
		if_simloop(vifp, m, dst.sin_family, 0);

		/* prepare the register head to send to the mrouting daemon */
		m = mcp;
	}

pim_input_to_daemon:
	/*
	 * Pass the PIM message up to the daemon; if it is a Register message,
	 * pass the 'head' only up to the daemon. This includes the
	 * outer IP header, PIM header, PIM-Register header and the
	 * inner IP header.
	 * XXX: the outer IP header pkt size of a Register is not adjust to
	 * reflect the fact that the inner multicast data is truncated.
	 */
	return (rip_input(&m, &off, proto));
}

static int
sysctl_mfctable(SYSCTL_HANDLER_ARGS)
{
	struct mfc	*rt;
	int		 error, i;

	if (req->newptr)
		return (EPERM);
	if (V_mfchashtbl == NULL)	/* XXX unlocked */
		return (0);
	error = sysctl_wire_old_buffer(req, 0);
	if (error)
		return (error);

	MRW_RLOCK();
	if (V_mfchashtbl == NULL)
		goto out_locked;

	for (i = 0; i < mfchashsize; i++) {
		LIST_FOREACH(rt, &V_mfchashtbl[i], mfc_hash) {
			error = SYSCTL_OUT(req, rt, sizeof(struct mfc));
			if (error)
				goto out_locked;
		}
	}
out_locked:
	MRW_RUNLOCK();
	return (error);
}

static SYSCTL_NODE(_net_inet_ip, OID_AUTO, mfctable,
    CTLFLAG_RD | CTLFLAG_MPSAFE, sysctl_mfctable,
    "IPv4 Multicast Forwarding Table "
    "(struct *mfc[mfchashsize], netinet/ip_mroute.h)");

static int
sysctl_viflist(SYSCTL_HANDLER_ARGS)
{
	int error, i;

	if (req->newptr)
		return (EPERM);
	if (V_viftable == NULL)		/* XXX unlocked */
		return (0);
	error = sysctl_wire_old_buffer(req, MROUTE_VIF_SYSCTL_LEN * MAXVIFS);
	if (error)
		return (error);

	MRW_RLOCK();
	/* Copy out user-visible portion of vif entry. */
	for (i = 0; i < MAXVIFS; i++) {
		error = SYSCTL_OUT(req, &V_viftable[i], MROUTE_VIF_SYSCTL_LEN);
		if (error)
			break;
	}
	MRW_RUNLOCK();
	return (error);
}

SYSCTL_PROC(_net_inet_ip, OID_AUTO, viftable,
    CTLTYPE_OPAQUE | CTLFLAG_VNET | CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, 0,
    sysctl_viflist, "S,vif[MAXVIFS]",
    "IPv4 Multicast Interfaces (struct vif[MAXVIFS], netinet/ip_mroute.h)");

static void
vnet_mroute_init(const void *unused __unused)
{

	V_nexpire = malloc(mfchashsize, M_MRTABLE, M_WAITOK|M_ZERO);

	V_viftable = mallocarray(MAXVIFS, sizeof(*V_viftable),
	    M_MRTABLE, M_WAITOK|M_ZERO);

	callout_init_rw(&V_expire_upcalls_ch, &mrouter_lock, 0);
	callout_init_rw(&V_bw_upcalls_ch, &mrouter_lock, 0);

	/* Prepare taskqueue */
	V_task_queue = taskqueue_create_fast("ip_mroute_tskq", M_NOWAIT,
		    taskqueue_thread_enqueue, &V_task_queue);
	taskqueue_start_threads(&V_task_queue, 1, PI_NET, "ip_mroute_tskq task");
}

VNET_SYSINIT(vnet_mroute_init, SI_SUB_PROTO_MC, SI_ORDER_ANY, vnet_mroute_init,
	NULL);

static void
vnet_mroute_uninit(const void *unused __unused)
{

	/* Taskqueue should be cancelled and drained before freeing */
	taskqueue_free(V_task_queue);

	free(V_viftable, M_MRTABLE);
	free(V_nexpire, M_MRTABLE);
	V_nexpire = NULL;
}

VNET_SYSUNINIT(vnet_mroute_uninit, SI_SUB_PROTO_MC, SI_ORDER_MIDDLE,
    vnet_mroute_uninit, NULL);

static int
ip_mroute_modevent(module_t mod, int type, void *unused)
{

	switch (type) {
	case MOD_LOAD:
		MRW_TEARDOWN_LOCK_INIT();
		MRW_LOCK_INIT();

		if_detach_event_tag = EVENTHANDLER_REGISTER(ifnet_departure_event,
		    if_detached_event, NULL, EVENTHANDLER_PRI_ANY);
		if (if_detach_event_tag == NULL) {
			printf("ip_mroute: unable to register "
					"ifnet_departure_event handler\n");
			MRW_LOCK_DESTROY();
			return (EINVAL);
		}

		if (!powerof2(mfchashsize)) {
			printf("WARNING: %s not a power of 2; using default\n",
					"net.inet.ip.mfchashsize");
			mfchashsize = MFCHASHSIZE;
		}

		pim_encap_cookie = ip_encap_attach(&ipv4_encap_cfg, NULL, M_WAITOK);

		ip_mcast_src = X_ip_mcast_src;
		ip_mforward = X_ip_mforward;
		ip_mrouter_done = X_ip_mrouter_done;
		ip_mrouter_get = X_ip_mrouter_get;
		ip_mrouter_set = X_ip_mrouter_set;

		ip_rsvp_force_done = X_ip_rsvp_force_done;
		ip_rsvp_vif = X_ip_rsvp_vif;

		legal_vif_num = X_legal_vif_num;
		mrt_ioctl = X_mrt_ioctl;
		rsvp_input_p = X_rsvp_input;
		break;

	case MOD_UNLOAD:
		/*
		 * Typically module unload happens after the user-level
		 * process has shutdown the kernel services (the check
		 * below insures someone can't just yank the module out
		 * from under a running process).  But if the module is
		 * just loaded and then unloaded w/o starting up a user
		 * process we still need to cleanup.
		 */
		MRW_WLOCK();
		if (ip_mrouter_cnt != 0) {
			MRW_WUNLOCK();
			return (EINVAL);
		}
		ip_mrouter_unloading = 1;
		MRW_WUNLOCK();

		EVENTHANDLER_DEREGISTER(ifnet_departure_event, if_detach_event_tag);

		if (pim_encap_cookie) {
			ip_encap_detach(pim_encap_cookie);
			pim_encap_cookie = NULL;
		}

		ip_mcast_src = NULL;
		ip_mforward = NULL;
		ip_mrouter_done = NULL;
		ip_mrouter_get = NULL;
		ip_mrouter_set = NULL;

		ip_rsvp_force_done = NULL;
		ip_rsvp_vif = NULL;

		legal_vif_num = NULL;
		mrt_ioctl = NULL;
		rsvp_input_p = NULL;

		MRW_LOCK_DESTROY();
		MRW_TEARDOWN_LOCK_DESTROY();
		break;

	default:
		return EOPNOTSUPP;
	}
	return 0;
}

static moduledata_t ip_mroutemod = {
	"ip_mroute",
	ip_mroute_modevent,
	0
};

DECLARE_MODULE(ip_mroute, ip_mroutemod, SI_SUB_PROTO_MC, SI_ORDER_MIDDLE);
