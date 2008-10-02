/*-
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
 * 4. Neither the name of the University nor the names of its contributors
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
 *      @(#)ip_mroute.c 8.2 (Berkeley) 11/15/93
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
 *
 * MROUTING Revision: 3.5
 * and PIM-SMv2 and PIM-DM support, advanced API support,
 * bandwidth metering and signaling
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_mac.h"
#include "opt_mrouting.h"

#define _PIM_VT 1

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/lock.h>
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
#include <sys/time.h>
#include <sys/vimage.h>
#include <net/if.h>
#include <net/netisr.h>
#include <net/route.h>
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
#ifdef INET6
#include <netinet/ip6.h>
#include <netinet6/in6_var.h>
#include <netinet6/ip6_mroute.h>
#include <netinet6/ip6_var.h>
#endif
#include <machine/in_cksum.h>

#include <security/mac/mac_framework.h>

/*
 * Control debugging code for rsvp and multicast routing code.
 * Can only set them with the debugger.
 */
static u_int    rsvpdebug;		/* non-zero enables debugging	*/

static u_int	mrtdebug;		/* any set of the flags below	*/
#define		DEBUG_MFC	0x02
#define		DEBUG_FORWARD	0x04
#define		DEBUG_EXPIRE	0x08
#define		DEBUG_XMIT	0x10
#define		DEBUG_PIM	0x20

#define		VIFI_INVALID	((vifi_t) -1)

#define M_HASCL(m)	((m)->m_flags & M_EXT)

static MALLOC_DEFINE(M_MRTABLE, "mroutetbl", "multicast routing tables");

/*
 * Locking.  We use two locks: one for the virtual interface table and
 * one for the forwarding table.  These locks may be nested in which case
 * the VIF lock must always be taken first.  Note that each lock is used
 * to cover not only the specific data structure but also related data
 * structures.  It may be better to add more fine-grained locking later;
 * it's not clear how performance-critical this code is.
 *
 * XXX: This module could particularly benefit from being cleaned
 *      up to use the <sys/queue.h> macros.
 *
 */

static struct mrtstat	mrtstat;
SYSCTL_STRUCT(_net_inet_ip, OID_AUTO, mrtstat, CTLFLAG_RW,
    &mrtstat, mrtstat,
    "Multicast Routing Statistics (struct mrtstat, netinet/ip_mroute.h)");

static struct mfc	*mfctable[MFCTBLSIZ];
SYSCTL_OPAQUE(_net_inet_ip, OID_AUTO, mfctable, CTLFLAG_RD,
    &mfctable, sizeof(mfctable), "S,*mfc[MFCTBLSIZ]",
    "Multicast Forwarding Table (struct *mfc[MFCTBLSIZ], netinet/ip_mroute.h)");

static struct mtx mrouter_mtx;
#define	MROUTER_LOCK()		mtx_lock(&mrouter_mtx)
#define	MROUTER_UNLOCK()	mtx_unlock(&mrouter_mtx)
#define	MROUTER_LOCK_ASSERT()	mtx_assert(&mrouter_mtx, MA_OWNED)
#define	MROUTER_LOCK_INIT()	\
	mtx_init(&mrouter_mtx, "IPv4 multicast forwarding", NULL, MTX_DEF)
#define	MROUTER_LOCK_DESTROY()	mtx_destroy(&mrouter_mtx)

static struct mtx mfc_mtx;
#define	MFC_LOCK()	mtx_lock(&mfc_mtx)
#define	MFC_UNLOCK()	mtx_unlock(&mfc_mtx)
#define	MFC_LOCK_ASSERT()	mtx_assert(&mfc_mtx, MA_OWNED)
#define	MFC_LOCK_INIT()	mtx_init(&mfc_mtx, "mroute mfc table", NULL, MTX_DEF)
#define	MFC_LOCK_DESTROY()	mtx_destroy(&mfc_mtx)

static struct vif	viftable[MAXVIFS];
SYSCTL_OPAQUE(_net_inet_ip, OID_AUTO, viftable, CTLFLAG_RD,
    &viftable, sizeof(viftable), "S,vif[MAXVIFS]",
    "Multicast Virtual Interfaces (struct vif[MAXVIFS], netinet/ip_mroute.h)");

static struct mtx vif_mtx;
#define	VIF_LOCK()	mtx_lock(&vif_mtx)
#define	VIF_UNLOCK()	mtx_unlock(&vif_mtx)
#define	VIF_LOCK_ASSERT()	mtx_assert(&vif_mtx, MA_OWNED)
#define	VIF_LOCK_INIT()	mtx_init(&vif_mtx, "mroute vif table", NULL, MTX_DEF)
#define	VIF_LOCK_DESTROY()	mtx_destroy(&vif_mtx)

static u_char		nexpire[MFCTBLSIZ];

static eventhandler_tag if_detach_event_tag = NULL;

static struct callout expire_upcalls_ch;

#define		EXPIRE_TIMEOUT	(hz / 4)	/* 4x / second		*/
#define		UPCALL_EXPIRE	6		/* number of timeouts	*/

#define ENCAP_TTL 64

/*
 * Bandwidth meter variables and constants
 */
static MALLOC_DEFINE(M_BWMETER, "bwmeter", "multicast upcall bw meters");
/*
 * Pending timeouts are stored in a hash table, the key being the
 * expiration time. Periodically, the entries are analysed and processed.
 */
#define BW_METER_BUCKETS	1024
static struct bw_meter *bw_meter_timers[BW_METER_BUCKETS];
static struct callout bw_meter_ch;
#define BW_METER_PERIOD (hz)		/* periodical handling of bw meters */

/*
 * Pending upcalls are stored in a vector which is flushed when
 * full, or periodically
 */
static struct bw_upcall	bw_upcalls[BW_UPCALLS_MAX];
static u_int	bw_upcalls_n; /* # of pending upcalls */
static struct callout bw_upcalls_ch;
#define BW_UPCALLS_PERIOD (hz)		/* periodical flush of bw upcalls */

static struct pimstat pimstat;

SYSCTL_NODE(_net_inet, IPPROTO_PIM, pim, CTLFLAG_RW, 0, "PIM");
SYSCTL_STRUCT(_net_inet_pim, PIMCTL_STATS, stats, CTLFLAG_RD,
    &pimstat, pimstat,
    "PIM Statistics (struct pimstat, netinet/pim_var.h)");

static u_long	pim_squelch_wholepkt = 0;
SYSCTL_ULONG(_net_inet_pim, OID_AUTO, squelch_wholepkt, CTLFLAG_RW,
    &pim_squelch_wholepkt, 0,
    "Disable IGMP_WHOLEPKT notifications if rendezvous point is unspecified");

extern  struct domain inetdomain;
struct protosw in_pim_protosw = {
	.pr_type =		SOCK_RAW,
	.pr_domain =		&inetdomain,
	.pr_protocol =		IPPROTO_PIM,
	.pr_flags =		PR_ATOMIC|PR_ADDR|PR_LASTHDR,
	.pr_input =		pim_input,
	.pr_output =		(pr_output_t*)rip_output,
	.pr_ctloutput =		rip_ctloutput,
	.pr_usrreqs =		&rip_usrreqs
};
static const struct encaptab *pim_encap_cookie;

#ifdef INET6
/* ip6_mroute.c glue */
extern struct in6_protosw in6_pim_protosw;
static const struct encaptab *pim6_encap_cookie;

extern int X_ip6_mrouter_set(struct socket *, struct sockopt *);
extern int X_ip6_mrouter_get(struct socket *, struct sockopt *);
extern int X_ip6_mrouter_done(void);
extern int X_ip6_mforward(struct ip6_hdr *, struct ifnet *, struct mbuf *);
extern int X_mrt6_ioctl(int, caddr_t);
#endif

static int pim_encapcheck(const struct mbuf *, int, int, void *);

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
	ENCAP_TTL,
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

static struct ifnet multicast_register_if;
static vifi_t reg_vif_num = VIFI_INVALID;

/*
 * Private variables.
 */
static vifi_t	   numvifs;

static u_long	X_ip_mcast_src(int vifi);
static int	X_ip_mforward(struct ip *ip, struct ifnet *ifp,
			struct mbuf *m, struct ip_moptions *imo);
static int	X_ip_mrouter_done(void);
static int	X_ip_mrouter_get(struct socket *so, struct sockopt *m);
static int	X_ip_mrouter_set(struct socket *so, struct sockopt *m);
static int	X_legal_vif_num(int vif);
static int	X_mrt_ioctl(int cmd, caddr_t data, int fibnum);

static int get_sg_cnt(struct sioc_sg_req *);
static int get_vif_cnt(struct sioc_vif_req *);
static void if_detached_event(void *arg __unused, struct ifnet *);
static int ip_mrouter_init(struct socket *, int);
static int add_vif(struct vifctl *);
static int del_vif_locked(vifi_t);
static int del_vif(vifi_t);
static int add_mfc(struct mfcctl2 *);
static int del_mfc(struct mfcctl2 *);
static int set_api_config(uint32_t *); /* chose API capabilities */
static int socket_send(struct socket *, struct mbuf *, struct sockaddr_in *);
static int set_assert(int);
static void expire_upcalls(void *);
static int ip_mdq(struct mbuf *, struct ifnet *, struct mfc *, vifi_t);
static void phyint_send(struct ip *, struct vif *, struct mbuf *);
static void send_packet(struct vif *, struct mbuf *);

/*
 * Bandwidth monitoring
 */
static void free_bw_list(struct bw_meter *list);
static int add_bw_upcall(struct bw_upcall *);
static int del_bw_upcall(struct bw_upcall *);
static void bw_meter_receive_packet(struct bw_meter *x, int plen,
		struct timeval *nowp);
static void bw_meter_prepare_upcall(struct bw_meter *x, struct timeval *nowp);
static void bw_upcalls_send(void);
static void schedule_bw_meter(struct bw_meter *x, struct timeval *nowp);
static void unschedule_bw_meter(struct bw_meter *x);
static void bw_meter_process(void);
static void expire_bw_upcalls_send(void *);
static void expire_bw_meter_process(void *);

static int pim_register_send(struct ip *, struct vif *,
		struct mbuf *, struct mfc *);
static int pim_register_send_rp(struct ip *, struct vif *,
		struct mbuf *, struct mfc *);
static int pim_register_send_upcall(struct ip *, struct vif *,
		struct mbuf *, struct mfc *);
static struct mbuf *pim_register_prepare(struct ip *, struct mbuf *);

/*
 * whether or not special PIM assert processing is enabled.
 */
static int pim_assert;
/*
 * Rate limit for assert notification messages, in usec
 */
#define ASSERT_MSG_TIME		3000000

/*
 * Kernel multicast routing API capabilities and setup.
 * If more API capabilities are added to the kernel, they should be
 * recorded in `mrt_api_support'.
 */
static const uint32_t mrt_api_support = (MRT_MFC_FLAGS_DISABLE_WRONGVIF |
					 MRT_MFC_FLAGS_BORDER_VIF |
					 MRT_MFC_RP |
					 MRT_MFC_BW_UPCALL);
static uint32_t mrt_api_config = 0;

/*
 * Hash function for a source, group entry
 */
#define MFCHASH(a, g) MFCHASHMOD(((a) >> 20) ^ ((a) >> 10) ^ (a) ^ \
			((g) >> 20) ^ ((g) >> 10) ^ (g))

/*
 * Find a route for a given origin IP address and Multicast group address
 * Statistics are updated by the caller if needed
 * (mrtstat.mrts_mfc_lookups and mrtstat.mrts_mfc_misses)
 */
static struct mfc *
mfc_find(in_addr_t o, in_addr_t g)
{
    struct mfc *rt;

    MFC_LOCK_ASSERT();

    for (rt = mfctable[MFCHASH(o,g)]; rt; rt = rt->mfc_next)
	if ((rt->mfc_origin.s_addr == o) &&
		(rt->mfc_mcastgrp.s_addr == g) && (rt->mfc_stall == NULL))
	    break;
    return rt;
}

/*
 * Macros to compute elapsed time efficiently
 * Borrowed from Van Jacobson's scheduling code
 */
#define TV_DELTA(a, b, delta) {					\
	int xxs;						\
	delta = (a).tv_usec - (b).tv_usec;			\
	if ((xxs = (a).tv_sec - (b).tv_sec)) {			\
		switch (xxs) {					\
		case 2:						\
		      delta += 1000000;				\
		      /* FALLTHROUGH */				\
		case 1:						\
		      delta += 1000000;				\
		      break;					\
		default:					\
		      delta += (1000000 * xxs);			\
		}						\
	}							\
}

#define TV_LT(a, b) (((a).tv_usec < (b).tv_usec && \
	      (a).tv_sec <= (b).tv_sec) || (a).tv_sec < (b).tv_sec)

/*
 * Handle MRT setsockopt commands to modify the multicast routing tables.
 */
static int
X_ip_mrouter_set(struct socket *so, struct sockopt *sopt)
{
    INIT_VNET_INET(curvnet);
    int	error, optval;
    vifi_t	vifi;
    struct	vifctl vifc;
    struct	mfcctl2 mfc;
    struct	bw_upcall bw_upcall;
    uint32_t	i;

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
		mrt_api_config & MRT_API_FLAGS_ALL) {
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
    static int version = 0x0305; /* !!! why is this here? XXX */

    switch (sopt->sopt_name) {
    case MRT_VERSION:
	error = sooptcopyout(sopt, &version, sizeof version);
	break;

    case MRT_ASSERT:
	error = sooptcopyout(sopt, &pim_assert, sizeof pim_assert);
	break;

    case MRT_API_SUPPORT:
	error = sooptcopyout(sopt, &mrt_api_support, sizeof mrt_api_support);
	break;

    case MRT_API_CONFIG:
	error = sooptcopyout(sopt, &mrt_api_config, sizeof mrt_api_config);
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
X_mrt_ioctl(int cmd, caddr_t data, int fibnum)
{
    int error = 0;

    /*
     * Currently the only function calling this ioctl routine is rtioctl().
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

    MFC_LOCK();
    rt = mfc_find(req->src.s_addr, req->grp.s_addr);
    if (rt == NULL) {
	MFC_UNLOCK();
	req->pktcnt = req->bytecnt = req->wrong_if = 0xffffffff;
	return EADDRNOTAVAIL;
    }
    req->pktcnt = rt->mfc_pkt_cnt;
    req->bytecnt = rt->mfc_byte_cnt;
    req->wrong_if = rt->mfc_wrong_if;
    MFC_UNLOCK();
    return 0;
}

/*
 * returns the input and output packet and byte counts on the vif provided
 */
static int
get_vif_cnt(struct sioc_vif_req *req)
{
    vifi_t vifi = req->vifi;

    VIF_LOCK();
    if (vifi >= numvifs) {
	VIF_UNLOCK();
	return EINVAL;
    }

    req->icount = viftable[vifi].v_pkt_in;
    req->ocount = viftable[vifi].v_pkt_out;
    req->ibytes = viftable[vifi].v_bytes_in;
    req->obytes = viftable[vifi].v_bytes_out;
    VIF_UNLOCK();

    return 0;
}

static void
ip_mrouter_reset(void)
{
    bzero((caddr_t)mfctable, sizeof(mfctable));
    bzero((caddr_t)nexpire, sizeof(nexpire));

    pim_assert = 0;
    mrt_api_config = 0;

    callout_init(&expire_upcalls_ch, CALLOUT_MPSAFE);

    bw_upcalls_n = 0;
    bzero((caddr_t)bw_meter_timers, sizeof(bw_meter_timers));
    callout_init(&bw_upcalls_ch, CALLOUT_MPSAFE);
    callout_init(&bw_meter_ch, CALLOUT_MPSAFE);
}

static void
if_detached_event(void *arg __unused, struct ifnet *ifp)
{
    INIT_VNET_INET(curvnet);
    vifi_t vifi;
    int i;
    struct mfc *mfc;
    struct mfc *nmfc;
    struct mfc **ppmfc;	/* Pointer to previous node's next-pointer */
    struct rtdetq *pq;
    struct rtdetq *npq;

    MROUTER_LOCK();
    if (V_ip_mrouter == NULL) {
	MROUTER_UNLOCK();
    }

    /*
     * Tear down multicast forwarder state associated with this ifnet.
     * 1. Walk the vif list, matching vifs against this ifnet.
     * 2. Walk the multicast forwarding cache (mfc) looking for
     *    inner matches with this vif's index.
     * 3. Free any pending mbufs for this mfc.
     * 4. Free the associated mfc entry and state associated with this vif.
     *    Be very careful about unlinking from a singly-linked list whose
     *    "head node" is a pointer in a simple array.
     * 5. Free vif state. This should disable ALLMULTI on the interface.
     */
    VIF_LOCK();
    MFC_LOCK();
    for (vifi = 0; vifi < numvifs; vifi++) {
	if (viftable[vifi].v_ifp != ifp)
		continue;
	for (i = 0; i < MFCTBLSIZ; i++) {
	    ppmfc = &mfctable[i];
	    for (mfc = mfctable[i]; mfc != NULL; ) {
		nmfc = mfc->mfc_next;
		if (mfc->mfc_parent == vifi) {
		    for (pq = mfc->mfc_stall; pq != NULL; ) {
			npq = pq->next;
			m_freem(pq->m);
			free(pq, M_MRTABLE);
			pq = npq;
		    }
		    free_bw_list(mfc->mfc_bw_meter);
		    free(mfc, M_MRTABLE);
		    *ppmfc = nmfc;
		} else {
		    ppmfc = &mfc->mfc_next;
		}
		mfc = nmfc;
	    }
	}
	del_vif_locked(vifi);
    }
    MFC_UNLOCK();
    VIF_UNLOCK();

    MROUTER_UNLOCK();
}
                        
/*
 * Enable multicast routing
 */
static int
ip_mrouter_init(struct socket *so, int version)
{
    INIT_VNET_INET(curvnet);

    if (mrtdebug)
	log(LOG_DEBUG, "ip_mrouter_init: so_type = %d, pr_protocol = %d\n",
	    so->so_type, so->so_proto->pr_protocol);

    if (so->so_type != SOCK_RAW || so->so_proto->pr_protocol != IPPROTO_IGMP)
	return EOPNOTSUPP;

    if (version != 1)
	return ENOPROTOOPT;

    MROUTER_LOCK();

    if (V_ip_mrouter != NULL) {
	MROUTER_UNLOCK();
	return EADDRINUSE;
    }

    if_detach_event_tag = EVENTHANDLER_REGISTER(ifnet_departure_event, 
        if_detached_event, NULL, EVENTHANDLER_PRI_ANY);
    if (if_detach_event_tag == NULL) {
	MROUTER_UNLOCK();
	return (ENOMEM);
    }

    callout_reset(&expire_upcalls_ch, EXPIRE_TIMEOUT, expire_upcalls, NULL);

    callout_reset(&bw_upcalls_ch, BW_UPCALLS_PERIOD,
	expire_bw_upcalls_send, NULL);
    callout_reset(&bw_meter_ch, BW_METER_PERIOD, expire_bw_meter_process, NULL);

    V_ip_mrouter = so;

    MROUTER_UNLOCK();

    if (mrtdebug)
	log(LOG_DEBUG, "ip_mrouter_init\n");

    return 0;
}

/*
 * Disable multicast routing
 */
static int
X_ip_mrouter_done(void)
{
    INIT_VNET_INET(curvnet);
    vifi_t vifi;
    int i;
    struct ifnet *ifp;
    struct ifreq ifr;
    struct mfc *rt;
    struct rtdetq *rte;

    MROUTER_LOCK();

    if (V_ip_mrouter == NULL) {
	MROUTER_UNLOCK();
	return EINVAL;
    }

    /*
     * Detach/disable hooks to the reset of the system.
     */
    V_ip_mrouter = NULL;
    mrt_api_config = 0;

    VIF_LOCK();
    /*
     * For each phyint in use, disable promiscuous reception of all IP
     * multicasts.
     */
    for (vifi = 0; vifi < numvifs; vifi++) {
	if (viftable[vifi].v_lcl_addr.s_addr != 0 &&
		!(viftable[vifi].v_flags & (VIFF_TUNNEL | VIFF_REGISTER))) {
	    struct sockaddr_in *so = (struct sockaddr_in *)&(ifr.ifr_addr);

	    so->sin_len = sizeof(struct sockaddr_in);
	    so->sin_family = AF_INET;
	    so->sin_addr.s_addr = INADDR_ANY;
	    ifp = viftable[vifi].v_ifp;
	    if_allmulti(ifp, 0);
	}
    }
    bzero((caddr_t)viftable, sizeof(viftable));
    numvifs = 0;
    pim_assert = 0;
    VIF_UNLOCK();
    EVENTHANDLER_DEREGISTER(ifnet_departure_event, if_detach_event_tag);

    /*
     * Free all multicast forwarding cache entries.
     */
    callout_stop(&expire_upcalls_ch);
    callout_stop(&bw_upcalls_ch);
    callout_stop(&bw_meter_ch);

    MFC_LOCK();
    for (i = 0; i < MFCTBLSIZ; i++) {
	for (rt = mfctable[i]; rt != NULL; ) {
	    struct mfc *nr = rt->mfc_next;

	    for (rte = rt->mfc_stall; rte != NULL; ) {
		struct rtdetq *n = rte->next;

		m_freem(rte->m);
		free(rte, M_MRTABLE);
		rte = n;
	    }
	    free_bw_list(rt->mfc_bw_meter);
	    free(rt, M_MRTABLE);
	    rt = nr;
	}
    }
    bzero((caddr_t)mfctable, sizeof(mfctable));
    bzero((caddr_t)nexpire, sizeof(nexpire));
    bw_upcalls_n = 0;
    bzero(bw_meter_timers, sizeof(bw_meter_timers));
    MFC_UNLOCK();

    reg_vif_num = VIFI_INVALID;

    MROUTER_UNLOCK();

    if (mrtdebug)
	log(LOG_DEBUG, "ip_mrouter_done\n");

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

    pim_assert = i;

    return 0;
}

/*
 * Configure API capabilities
 */
int
set_api_config(uint32_t *apival)
{
    int i;

    /*
     * We can set the API capabilities only if it is the first operation
     * after MRT_INIT. I.e.:
     *  - there are no vifs installed
     *  - pim_assert is not enabled
     *  - the MFC table is empty
     */
    if (numvifs > 0) {
	*apival = 0;
	return EPERM;
    }
    if (pim_assert) {
	*apival = 0;
	return EPERM;
    }
    for (i = 0; i < MFCTBLSIZ; i++) {
	if (mfctable[i] != NULL) {
	    *apival = 0;
	    return EPERM;
	}
    }

    mrt_api_config = *apival & mrt_api_support;
    *apival = mrt_api_config;

    return 0;
}

/*
 * Add a vif to the vif table
 */
static int
add_vif(struct vifctl *vifcp)
{
    struct vif *vifp = viftable + vifcp->vifc_vifi;
    struct sockaddr_in sin = {sizeof sin, AF_INET};
    struct ifaddr *ifa;
    struct ifnet *ifp;
    int error;

    VIF_LOCK();
    if (vifcp->vifc_vifi >= MAXVIFS) {
	VIF_UNLOCK();
	return EINVAL;
    }
    /* rate limiting is no longer supported by this code */
    if (vifcp->vifc_rate_limit != 0) {
	log(LOG_ERR, "rate limiting is no longer supported\n");
	VIF_UNLOCK();
	return EINVAL;
    }
    if (vifp->v_lcl_addr.s_addr != INADDR_ANY) {
	VIF_UNLOCK();
	return EADDRINUSE;
    }
    if (vifcp->vifc_lcl_addr.s_addr == INADDR_ANY) {
	VIF_UNLOCK();
	return EADDRNOTAVAIL;
    }

    /* Find the interface with an address in AF_INET family */
    if (vifcp->vifc_flags & VIFF_REGISTER) {
	/*
	 * XXX: Because VIFF_REGISTER does not really need a valid
	 * local interface (e.g. it could be 127.0.0.2), we don't
	 * check its address.
	 */
	ifp = NULL;
    } else {
	sin.sin_addr = vifcp->vifc_lcl_addr;
	ifa = ifa_ifwithaddr((struct sockaddr *)&sin);
	if (ifa == NULL) {
	    VIF_UNLOCK();
	    return EADDRNOTAVAIL;
	}
	ifp = ifa->ifa_ifp;
    }

    if ((vifcp->vifc_flags & VIFF_TUNNEL) != 0) {
	log(LOG_ERR, "tunnels are no longer supported\n");
	VIF_UNLOCK();
	return EOPNOTSUPP;
    } else if (vifcp->vifc_flags & VIFF_REGISTER) {
	ifp = &multicast_register_if;
	if (mrtdebug)
	    log(LOG_DEBUG, "Adding a register vif, ifp: %p\n",
		    (void *)&multicast_register_if);
	if (reg_vif_num == VIFI_INVALID) {
	    if_initname(&multicast_register_if, "register_vif", 0);
	    multicast_register_if.if_flags = IFF_LOOPBACK;
	    reg_vif_num = vifcp->vifc_vifi;
	}
    } else {		/* Make sure the interface supports multicast */
	if ((ifp->if_flags & IFF_MULTICAST) == 0) {
	    VIF_UNLOCK();
	    return EOPNOTSUPP;
	}

	/* Enable promiscuous reception of all IP multicasts from the if */
	error = if_allmulti(ifp, 1);
	if (error) {
	    VIF_UNLOCK();
	    return error;
	}
    }

    vifp->v_flags     = vifcp->vifc_flags;
    vifp->v_threshold = vifcp->vifc_threshold;
    vifp->v_lcl_addr  = vifcp->vifc_lcl_addr;
    vifp->v_rmt_addr  = vifcp->vifc_rmt_addr;
    vifp->v_ifp       = ifp;
    vifp->v_rsvp_on   = 0;
    vifp->v_rsvpd     = NULL;
    /* initialize per vif pkt counters */
    vifp->v_pkt_in    = 0;
    vifp->v_pkt_out   = 0;
    vifp->v_bytes_in  = 0;
    vifp->v_bytes_out = 0;
    bzero(&vifp->v_route, sizeof(vifp->v_route));

    /* Adjust numvifs up if the vifi is higher than numvifs */
    if (numvifs <= vifcp->vifc_vifi) numvifs = vifcp->vifc_vifi + 1;

    VIF_UNLOCK();

    if (mrtdebug)
	log(LOG_DEBUG, "add_vif #%d, lcladdr %lx, %s %lx, thresh %x\n",
	    vifcp->vifc_vifi,
	    (u_long)ntohl(vifcp->vifc_lcl_addr.s_addr),
	    (vifcp->vifc_flags & VIFF_TUNNEL) ? "rmtaddr" : "mask",
	    (u_long)ntohl(vifcp->vifc_rmt_addr.s_addr),
	    vifcp->vifc_threshold);

    return 0;
}

/*
 * Delete a vif from the vif table
 */
static int
del_vif_locked(vifi_t vifi)
{
    struct vif *vifp;

    VIF_LOCK_ASSERT();

    if (vifi >= numvifs) {
	return EINVAL;
    }
    vifp = &viftable[vifi];
    if (vifp->v_lcl_addr.s_addr == INADDR_ANY) {
	return EADDRNOTAVAIL;
    }

    if (!(vifp->v_flags & (VIFF_TUNNEL | VIFF_REGISTER)))
	if_allmulti(vifp->v_ifp, 0);

    if (vifp->v_flags & VIFF_REGISTER)
	reg_vif_num = VIFI_INVALID;

    bzero((caddr_t)vifp, sizeof (*vifp));

    if (mrtdebug)
	log(LOG_DEBUG, "del_vif %d, numvifs %d\n", vifi, numvifs);

    /* Adjust numvifs down */
    for (vifi = numvifs; vifi > 0; vifi--)
	if (viftable[vifi-1].v_lcl_addr.s_addr != INADDR_ANY)
	    break;
    numvifs = vifi;

    return 0;
}

static int
del_vif(vifi_t vifi)
{
    int cc;

    VIF_LOCK();
    cc = del_vif_locked(vifi);
    VIF_UNLOCK();

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
    for (i = 0; i < numvifs; i++) {
	rt->mfc_ttls[i] = mfccp->mfcc_ttls[i];
	rt->mfc_flags[i] = mfccp->mfcc_flags[i] & mrt_api_config &
	    MRT_MFC_FLAGS_ALL;
    }
    /* set the RP address */
    if (mrt_api_config & MRT_MFC_RP)
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
    rt->mfc_last_assert.tv_sec = rt->mfc_last_assert.tv_usec = 0;
}


/*
 * Add an mfc entry
 */
static int
add_mfc(struct mfcctl2 *mfccp)
{
    struct mfc *rt;
    u_long hash;
    struct rtdetq *rte;
    u_short nstl;

    VIF_LOCK();
    MFC_LOCK();

    rt = mfc_find(mfccp->mfcc_origin.s_addr, mfccp->mfcc_mcastgrp.s_addr);

    /* If an entry already exists, just update the fields */
    if (rt) {
	if (mrtdebug & DEBUG_MFC)
	    log(LOG_DEBUG,"add_mfc update o %lx g %lx p %x\n",
		(u_long)ntohl(mfccp->mfcc_origin.s_addr),
		(u_long)ntohl(mfccp->mfcc_mcastgrp.s_addr),
		mfccp->mfcc_parent);

	update_mfc_params(rt, mfccp);
	MFC_UNLOCK();
	VIF_UNLOCK();
	return 0;
    }

    /*
     * Find the entry for which the upcall was made and update
     */
    hash = MFCHASH(mfccp->mfcc_origin.s_addr, mfccp->mfcc_mcastgrp.s_addr);
    for (rt = mfctable[hash], nstl = 0; rt; rt = rt->mfc_next) {

	if ((rt->mfc_origin.s_addr == mfccp->mfcc_origin.s_addr) &&
		(rt->mfc_mcastgrp.s_addr == mfccp->mfcc_mcastgrp.s_addr) &&
		(rt->mfc_stall != NULL)) {

	    if (nstl++)
		log(LOG_ERR, "add_mfc %s o %lx g %lx p %x dbx %p\n",
		    "multiple kernel entries",
		    (u_long)ntohl(mfccp->mfcc_origin.s_addr),
		    (u_long)ntohl(mfccp->mfcc_mcastgrp.s_addr),
		    mfccp->mfcc_parent, (void *)rt->mfc_stall);

	    if (mrtdebug & DEBUG_MFC)
		log(LOG_DEBUG,"add_mfc o %lx g %lx p %x dbg %p\n",
		    (u_long)ntohl(mfccp->mfcc_origin.s_addr),
		    (u_long)ntohl(mfccp->mfcc_mcastgrp.s_addr),
		    mfccp->mfcc_parent, (void *)rt->mfc_stall);

	    init_mfc_params(rt, mfccp);

	    rt->mfc_expire = 0;	/* Don't clean this guy up */
	    nexpire[hash]--;

	    /* free packets Qed at the end of this entry */
	    for (rte = rt->mfc_stall; rte != NULL; ) {
		struct rtdetq *n = rte->next;

		ip_mdq(rte->m, rte->ifp, rt, -1);
		m_freem(rte->m);
		free(rte, M_MRTABLE);
		rte = n;
	    }
	    rt->mfc_stall = NULL;
	}
    }

    /*
     * It is possible that an entry is being inserted without an upcall
     */
    if (nstl == 0) {
	if (mrtdebug & DEBUG_MFC)
	    log(LOG_DEBUG,"add_mfc no upcall h %lu o %lx g %lx p %x\n",
		hash, (u_long)ntohl(mfccp->mfcc_origin.s_addr),
		(u_long)ntohl(mfccp->mfcc_mcastgrp.s_addr),
		mfccp->mfcc_parent);

	for (rt = mfctable[hash]; rt != NULL; rt = rt->mfc_next) {
	    if ((rt->mfc_origin.s_addr == mfccp->mfcc_origin.s_addr) &&
		    (rt->mfc_mcastgrp.s_addr == mfccp->mfcc_mcastgrp.s_addr)) {
		init_mfc_params(rt, mfccp);
		if (rt->mfc_expire)
		    nexpire[hash]--;
		rt->mfc_expire = 0;
		break; /* XXX */
	    }
	}
	if (rt == NULL) {		/* no upcall, so make a new entry */
	    rt = (struct mfc *)malloc(sizeof(*rt), M_MRTABLE, M_NOWAIT);
	    if (rt == NULL) {
		MFC_UNLOCK();
		VIF_UNLOCK();
		return ENOBUFS;
	    }

	    init_mfc_params(rt, mfccp);
	    rt->mfc_expire     = 0;
	    rt->mfc_stall      = NULL;

	    rt->mfc_bw_meter = NULL;
	    /* insert new entry at head of hash chain */
	    rt->mfc_next = mfctable[hash];
	    mfctable[hash] = rt;
	}
    }
    MFC_UNLOCK();
    VIF_UNLOCK();
    return 0;
}

/*
 * Delete an mfc entry
 */
static int
del_mfc(struct mfcctl2 *mfccp)
{
    struct in_addr	origin;
    struct in_addr	mcastgrp;
    struct mfc		*rt;
    struct mfc		**nptr;
    u_long		hash;
    struct bw_meter	*list;

    origin = mfccp->mfcc_origin;
    mcastgrp = mfccp->mfcc_mcastgrp;

    if (mrtdebug & DEBUG_MFC)
	log(LOG_DEBUG,"del_mfc orig %lx mcastgrp %lx\n",
	    (u_long)ntohl(origin.s_addr), (u_long)ntohl(mcastgrp.s_addr));

    MFC_LOCK();

    hash = MFCHASH(origin.s_addr, mcastgrp.s_addr);
    for (nptr = &mfctable[hash]; (rt = *nptr) != NULL; nptr = &rt->mfc_next)
	if (origin.s_addr == rt->mfc_origin.s_addr &&
		mcastgrp.s_addr == rt->mfc_mcastgrp.s_addr &&
		rt->mfc_stall == NULL)
	    break;
    if (rt == NULL) {
	MFC_UNLOCK();
	return EADDRNOTAVAIL;
    }

    *nptr = rt->mfc_next;

    /*
     * free the bw_meter entries
     */
    list = rt->mfc_bw_meter;
    rt->mfc_bw_meter = NULL;

    free(rt, M_MRTABLE);

    free_bw_list(list);

    MFC_UNLOCK();

    return 0;
}

/*
 * Send a message to the routing daemon on the multicast routing socket
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
	SOCKBUF_UNLOCK(&s->so_rcv);
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
    INIT_VNET_INET(curvnet);
    struct mfc *rt;
    int error;
    vifi_t vifi;

    if (mrtdebug & DEBUG_FORWARD)
	log(LOG_DEBUG, "ip_mforward: src %lx, dst %lx, ifp %p\n",
	    (u_long)ntohl(ip->ip_src.s_addr), (u_long)ntohl(ip->ip_dst.s_addr),
	    (void *)ifp);

    if (ip->ip_hl < (sizeof(struct ip) + TUNNEL_LEN) >> 2 ||
		((u_char *)(ip + 1))[1] != IPOPT_LSRR ) {
	/*
	 * Packet arrived via a physical interface or
	 * an encapsulated tunnel or a register_vif.
	 */
    } else {
	/*
	 * Packet arrived through a source-route tunnel.
	 * Source-route tunnels are no longer supported.
	 */
	static int last_log;
	if (last_log != time_uptime) {
	    last_log = time_uptime;
	    log(LOG_ERR,
		"ip_mforward: received source-routed packet from %lx\n",
		(u_long)ntohl(ip->ip_src.s_addr));
	}
	return 1;
    }

    VIF_LOCK();
    MFC_LOCK();
    if (imo && ((vifi = imo->imo_multicast_vif) < numvifs)) {
	if (ip->ip_ttl < MAXTTL)
	    ip->ip_ttl++;	/* compensate for -1 in *_send routines */
	if (rsvpdebug && ip->ip_p == IPPROTO_RSVP) {
	    struct vif *vifp = viftable + vifi;

	    printf("Sending IPPROTO_RSVP from %lx to %lx on vif %d (%s%s)\n",
		(long)ntohl(ip->ip_src.s_addr), (long)ntohl(ip->ip_dst.s_addr),
		vifi,
		(vifp->v_flags & VIFF_TUNNEL) ? "tunnel on " : "",
		vifp->v_ifp->if_xname);
	}
	error = ip_mdq(m, ifp, NULL, vifi);
	MFC_UNLOCK();
	VIF_UNLOCK();
	return error;
    }
    if (rsvpdebug && ip->ip_p == IPPROTO_RSVP) {
	printf("Warning: IPPROTO_RSVP from %lx to %lx without vif option\n",
	    (long)ntohl(ip->ip_src.s_addr), (long)ntohl(ip->ip_dst.s_addr));
	if (!imo)
	    printf("In fact, no options were specified at all\n");
    }

    /*
     * Don't forward a packet with time-to-live of zero or one,
     * or a packet destined to a local-only group.
     */
    if (ip->ip_ttl <= 1 || IN_LOCAL_GROUP(ntohl(ip->ip_dst.s_addr))) {
	MFC_UNLOCK();
	VIF_UNLOCK();
	return 0;
    }

    /*
     * Determine forwarding vifs from the forwarding cache table
     */
    ++mrtstat.mrts_mfc_lookups;
    rt = mfc_find(ip->ip_src.s_addr, ip->ip_dst.s_addr);

    /* Entry exists, so forward if necessary */
    if (rt != NULL) {
	error = ip_mdq(m, ifp, rt, -1);
	MFC_UNLOCK();
	VIF_UNLOCK();
	return error;
    } else {
	/*
	 * If we don't have a route for packet's origin,
	 * Make a copy of the packet & send message to routing daemon
	 */

	struct mbuf *mb0;
	struct rtdetq *rte;
	u_long hash;
	int hlen = ip->ip_hl << 2;

	++mrtstat.mrts_mfc_misses;

	mrtstat.mrts_no_route++;
	if (mrtdebug & (DEBUG_FORWARD | DEBUG_MFC))
	    log(LOG_DEBUG, "ip_mforward: no rte s %lx g %lx\n",
		(u_long)ntohl(ip->ip_src.s_addr),
		(u_long)ntohl(ip->ip_dst.s_addr));

	/*
	 * Allocate mbufs early so that we don't do extra work if we are
	 * just going to fail anyway.  Make sure to pullup the header so
	 * that other people can't step on it.
	 */
	rte = (struct rtdetq *)malloc((sizeof *rte), M_MRTABLE, M_NOWAIT);
	if (rte == NULL) {
	    MFC_UNLOCK();
	    VIF_UNLOCK();
	    return ENOBUFS;
	}
	mb0 = m_copypacket(m, M_DONTWAIT);
	if (mb0 && (M_HASCL(mb0) || mb0->m_len < hlen))
	    mb0 = m_pullup(mb0, hlen);
	if (mb0 == NULL) {
	    free(rte, M_MRTABLE);
	    MFC_UNLOCK();
	    VIF_UNLOCK();
	    return ENOBUFS;
	}

	/* is there an upcall waiting for this flow ? */
	hash = MFCHASH(ip->ip_src.s_addr, ip->ip_dst.s_addr);
	for (rt = mfctable[hash]; rt; rt = rt->mfc_next) {
	    if ((ip->ip_src.s_addr == rt->mfc_origin.s_addr) &&
		    (ip->ip_dst.s_addr == rt->mfc_mcastgrp.s_addr) &&
		    (rt->mfc_stall != NULL))
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
	    for (vifi=0; vifi < numvifs && viftable[vifi].v_ifp != ifp; vifi++)
		;
	    if (vifi >= numvifs)	/* vif not found, drop packet */
		goto non_fatal;

	    /* no upcall, so make a new entry */
	    rt = (struct mfc *)malloc(sizeof(*rt), M_MRTABLE, M_NOWAIT);
	    if (rt == NULL)
		goto fail;
	    /* Make a copy of the header to send to the user level process */
	    mm = m_copy(mb0, 0, hlen);
	    if (mm == NULL)
		goto fail1;

	    /*
	     * Send message to routing daemon to install
	     * a route into the kernel table
	     */

	    im = mtod(mm, struct igmpmsg *);
	    im->im_msgtype = IGMPMSG_NOCACHE;
	    im->im_mbz = 0;
	    im->im_vif = vifi;

	    mrtstat.mrts_upcalls++;

	    k_igmpsrc.sin_addr = ip->ip_src;
	    if (socket_send(V_ip_mrouter, mm, &k_igmpsrc) < 0) {
		log(LOG_WARNING, "ip_mforward: ip_mrouter socket queue full\n");
		++mrtstat.mrts_upq_sockfull;
fail1:
		free(rt, M_MRTABLE);
fail:
		free(rte, M_MRTABLE);
		m_freem(mb0);
		MFC_UNLOCK();
		VIF_UNLOCK();
		return ENOBUFS;
	    }

	    /* insert new entry at head of hash chain */
	    rt->mfc_origin.s_addr     = ip->ip_src.s_addr;
	    rt->mfc_mcastgrp.s_addr   = ip->ip_dst.s_addr;
	    rt->mfc_expire	      = UPCALL_EXPIRE;
	    nexpire[hash]++;
	    for (i = 0; i < numvifs; i++) {
		rt->mfc_ttls[i] = 0;
		rt->mfc_flags[i] = 0;
	    }
	    rt->mfc_parent = -1;

	    rt->mfc_rp.s_addr = INADDR_ANY; /* clear the RP address */

	    rt->mfc_bw_meter = NULL;

	    /* link into table */
	    rt->mfc_next   = mfctable[hash];
	    mfctable[hash] = rt;
	    rt->mfc_stall = rte;

	} else {
	    /* determine if q has overflowed */
	    int npkts = 0;
	    struct rtdetq **p;

	    /*
	     * XXX ouch! we need to append to the list, but we
	     * only have a pointer to the front, so we have to
	     * scan the entire list every time.
	     */
	    for (p = &rt->mfc_stall; *p != NULL; p = &(*p)->next)
		npkts++;

	    if (npkts > MAX_UPQ) {
		mrtstat.mrts_upq_ovflw++;
non_fatal:
		free(rte, M_MRTABLE);
		m_freem(mb0);
		MFC_UNLOCK();
		VIF_UNLOCK();
		return 0;
	    }

	    /* Add this entry to the end of the queue */
	    *p = rte;
	}

	rte->m			= mb0;
	rte->ifp		= ifp;
	rte->next		= NULL;

	MFC_UNLOCK();
	VIF_UNLOCK();

	return 0;
    }
}

/*
 * Clean up the cache entry if upcall is not serviced
 */
static void
expire_upcalls(void *unused)
{
    struct rtdetq *rte;
    struct mfc *mfc, **nptr;
    int i;

    MFC_LOCK();
    for (i = 0; i < MFCTBLSIZ; i++) {
	if (nexpire[i] == 0)
	    continue;
	nptr = &mfctable[i];
	for (mfc = *nptr; mfc != NULL; mfc = *nptr) {
	    /*
	     * Skip real cache entries
	     * Make sure it wasn't marked to not expire (shouldn't happen)
	     * If it expires now
	     */
	    if (mfc->mfc_stall != NULL && mfc->mfc_expire != 0 &&
		    --mfc->mfc_expire == 0) {
		if (mrtdebug & DEBUG_EXPIRE)
		    log(LOG_DEBUG, "expire_upcalls: expiring (%lx %lx)\n",
			(u_long)ntohl(mfc->mfc_origin.s_addr),
			(u_long)ntohl(mfc->mfc_mcastgrp.s_addr));
		/*
		 * drop all the packets
		 * free the mbuf with the pkt, if, timing info
		 */
		for (rte = mfc->mfc_stall; rte; ) {
		    struct rtdetq *n = rte->next;

		    m_freem(rte->m);
		    free(rte, M_MRTABLE);
		    rte = n;
		}
		++mrtstat.mrts_cache_cleanups;
		nexpire[i]--;

		/*
		 * free the bw_meter entries
		 */
		while (mfc->mfc_bw_meter != NULL) {
		    struct bw_meter *x = mfc->mfc_bw_meter;

		    mfc->mfc_bw_meter = x->bm_mfc_next;
		    free(x, M_BWMETER);
		}

		*nptr = mfc->mfc_next;
		free(mfc, M_MRTABLE);
	    } else {
		nptr = &mfc->mfc_next;
	    }
	}
    }
    MFC_UNLOCK();

    callout_reset(&expire_upcalls_ch, EXPIRE_TIMEOUT, expire_upcalls, NULL);
}

/*
 * Packet forwarding routine once entry in the cache is made
 */
static int
ip_mdq(struct mbuf *m, struct ifnet *ifp, struct mfc *rt, vifi_t xmt_vif)
{
    INIT_VNET_INET(curvnet);
    struct ip  *ip = mtod(m, struct ip *);
    vifi_t vifi;
    int plen = ip->ip_len;

    VIF_LOCK_ASSERT();

    /*
     * If xmt_vif is not -1, send on only the requested vif.
     *
     * (since vifi_t is u_short, -1 becomes MAXUSHORT, which > numvifs.)
     */
    if (xmt_vif < numvifs) {
	if (viftable[xmt_vif].v_flags & VIFF_REGISTER)
		pim_register_send(ip, viftable + xmt_vif, m, rt);
	else
		phyint_send(ip, viftable + xmt_vif, m);
	return 1;
    }

    /*
     * Don't forward if it didn't arrive from the parent vif for its origin.
     */
    vifi = rt->mfc_parent;
    if ((vifi >= numvifs) || (viftable[vifi].v_ifp != ifp)) {
	/* came in the wrong interface */
	if (mrtdebug & DEBUG_FORWARD)
	    log(LOG_DEBUG, "wrong if: ifp %p vifi %d vififp %p\n",
		(void *)ifp, vifi, (void *)viftable[vifi].v_ifp);
	++mrtstat.mrts_wrong_if;
	++rt->mfc_wrong_if;
	/*
	 * If we are doing PIM assert processing, send a message
	 * to the routing daemon.
	 *
	 * XXX: A PIM-SM router needs the WRONGVIF detection so it
	 * can complete the SPT switch, regardless of the type
	 * of the iif (broadcast media, GRE tunnel, etc).
	 */
	if (pim_assert && (vifi < numvifs) && viftable[vifi].v_ifp) {
	    struct timeval now;
	    u_long delta;

	    if (ifp == &multicast_register_if)
		pimstat.pims_rcv_registers_wrongiif++;

	    /* Get vifi for the incoming packet */
	    for (vifi=0; vifi < numvifs && viftable[vifi].v_ifp != ifp; vifi++)
		;
	    if (vifi >= numvifs)
		return 0;	/* The iif is not found: ignore the packet. */

	    if (rt->mfc_flags[vifi] & MRT_MFC_FLAGS_DISABLE_WRONGVIF)
		return 0;	/* WRONGVIF disabled: ignore the packet */

	    GET_TIME(now);

	    TV_DELTA(now, rt->mfc_last_assert, delta);

	    if (delta > ASSERT_MSG_TIME) {
		struct sockaddr_in k_igmpsrc = { sizeof k_igmpsrc, AF_INET };
		struct igmpmsg *im;
		int hlen = ip->ip_hl << 2;
		struct mbuf *mm = m_copy(m, 0, hlen);

		if (mm && (M_HASCL(mm) || mm->m_len < hlen))
		    mm = m_pullup(mm, hlen);
		if (mm == NULL)
		    return ENOBUFS;

		rt->mfc_last_assert = now;

		im = mtod(mm, struct igmpmsg *);
		im->im_msgtype	= IGMPMSG_WRONGVIF;
		im->im_mbz		= 0;
		im->im_vif		= vifi;

		mrtstat.mrts_upcalls++;

		k_igmpsrc.sin_addr = im->im_src;
		if (socket_send(V_ip_mrouter, mm, &k_igmpsrc) < 0) {
		    log(LOG_WARNING,
			"ip_mforward: ip_mrouter socket queue full\n");
		    ++mrtstat.mrts_upq_sockfull;
		    return ENOBUFS;
		}
	    }
	}
	return 0;
    }

    /* If I sourced this packet, it counts as output, else it was input. */
    if (ip->ip_src.s_addr == viftable[vifi].v_lcl_addr.s_addr) {
	viftable[vifi].v_pkt_out++;
	viftable[vifi].v_bytes_out += plen;
    } else {
	viftable[vifi].v_pkt_in++;
	viftable[vifi].v_bytes_in += plen;
    }
    rt->mfc_pkt_cnt++;
    rt->mfc_byte_cnt += plen;

    /*
     * For each vif, decide if a copy of the packet should be forwarded.
     * Forward if:
     *		- the ttl exceeds the vif's threshold
     *		- there are group members downstream on interface
     */
    for (vifi = 0; vifi < numvifs; vifi++)
	if ((rt->mfc_ttls[vifi] > 0) && (ip->ip_ttl > rt->mfc_ttls[vifi])) {
	    viftable[vifi].v_pkt_out++;
	    viftable[vifi].v_bytes_out += plen;
	    if (viftable[vifi].v_flags & VIFF_REGISTER)
		pim_register_send(ip, viftable + vifi, m, rt);
	    else
		phyint_send(ip, viftable + vifi, m);
	}

    /*
     * Perform upcall-related bw measuring.
     */
    if (rt->mfc_bw_meter != NULL) {
	struct bw_meter *x;
	struct timeval now;

	GET_TIME(now);
	MFC_LOCK_ASSERT();
	for (x = rt->mfc_bw_meter; x != NULL; x = x->bm_mfc_next)
	    bw_meter_receive_packet(x, plen, &now);
    }

    return 0;
}

/*
 * check if a vif number is legal/ok. This is used by ip_output.
 */
static int
X_legal_vif_num(int vif)
{
    /* XXX unlocked, matter? */
    return (vif >= 0 && vif < numvifs);
}

/*
 * Return the local address used by this vif
 */
static u_long
X_ip_mcast_src(int vifi)
{
    /* XXX unlocked, matter? */
    if (vifi >= 0 && vifi < numvifs)
	return viftable[vifi].v_lcl_addr.s_addr;
    else
	return INADDR_ANY;
}

static void
phyint_send(struct ip *ip, struct vif *vifp, struct mbuf *m)
{
    struct mbuf *mb_copy;
    int hlen = ip->ip_hl << 2;

    VIF_LOCK_ASSERT();

    /*
     * Make a new reference to the packet; make sure that
     * the IP header is actually copied, not just referenced,
     * so that ip_output() only scribbles on the copy.
     */
    mb_copy = m_copypacket(m, M_DONTWAIT);
    if (mb_copy && (M_HASCL(mb_copy) || mb_copy->m_len < hlen))
	mb_copy = m_pullup(mb_copy, hlen);
    if (mb_copy == NULL)
	return;

    send_packet(vifp, mb_copy);
}

static void
send_packet(struct vif *vifp, struct mbuf *m)
{
	struct ip_moptions imo;
	struct in_multi *imm[2];
	int error;

	VIF_LOCK_ASSERT();

	imo.imo_multicast_ifp  = vifp->v_ifp;
	imo.imo_multicast_ttl  = mtod(m, struct ip *)->ip_ttl - 1;
	imo.imo_multicast_loop = 1;
	imo.imo_multicast_vif  = -1;
	imo.imo_num_memberships = 0;
	imo.imo_max_memberships = 2;
	imo.imo_membership  = &imm[0];

	/*
	 * Re-entrancy should not be a problem here, because
	 * the packets that we send out and are looped back at us
	 * should get rejected because they appear to come from
	 * the loopback interface, thus preventing looping.
	 */
	error = ip_output(m, NULL, &vifp->v_route, IP_FORWARDING, &imo, NULL);
	if (mrtdebug & DEBUG_XMIT) {
	    log(LOG_DEBUG, "phyint_send on vif %td err %d\n",
		vifp - viftable, error);
	}
}

static int
X_ip_rsvp_vif(struct socket *so, struct sockopt *sopt)
{
    INIT_VNET_INET(curvnet);
    int error, vifi;

    if (so->so_type != SOCK_RAW || so->so_proto->pr_protocol != IPPROTO_RSVP)
	return EOPNOTSUPP;

    error = sooptcopyin(sopt, &vifi, sizeof vifi, sizeof vifi);
    if (error)
	return error;

    VIF_LOCK();

    if (vifi < 0 || vifi >= numvifs) {	/* Error if vif is invalid */
	VIF_UNLOCK();
	return EADDRNOTAVAIL;
    }

    if (sopt->sopt_name == IP_RSVP_VIF_ON) {
	/* Check if socket is available. */
	if (viftable[vifi].v_rsvpd != NULL) {
	    VIF_UNLOCK();
	    return EADDRINUSE;
	}

	viftable[vifi].v_rsvpd = so;
	/* This may seem silly, but we need to be sure we don't over-increment
	 * the RSVP counter, in case something slips up.
	 */
	if (!viftable[vifi].v_rsvp_on) {
	    viftable[vifi].v_rsvp_on = 1;
	    V_rsvp_on++;
	}
    } else { /* must be VIF_OFF */
	/*
	 * XXX as an additional consistency check, one could make sure
	 * that viftable[vifi].v_rsvpd == so, otherwise passing so as
	 * first parameter is pretty useless.
	 */
	viftable[vifi].v_rsvpd = NULL;
	/*
	 * This may seem silly, but we need to be sure we don't over-decrement
	 * the RSVP counter, in case something slips up.
	 */
	if (viftable[vifi].v_rsvp_on) {
	    viftable[vifi].v_rsvp_on = 0;
	    V_rsvp_on--;
	}
    }
    VIF_UNLOCK();
    return 0;
}

static void
X_ip_rsvp_force_done(struct socket *so)
{
    INIT_VNET_INET(curvnet);
    int vifi;

    /* Don't bother if it is not the right type of socket. */
    if (so->so_type != SOCK_RAW || so->so_proto->pr_protocol != IPPROTO_RSVP)
	return;

    VIF_LOCK();

    /* The socket may be attached to more than one vif...this
     * is perfectly legal.
     */
    for (vifi = 0; vifi < numvifs; vifi++) {
	if (viftable[vifi].v_rsvpd == so) {
	    viftable[vifi].v_rsvpd = NULL;
	    /* This may seem silly, but we need to be sure we don't
	     * over-decrement the RSVP counter, in case something slips up.
	     */
	    if (viftable[vifi].v_rsvp_on) {
		viftable[vifi].v_rsvp_on = 0;
		V_rsvp_on--;
	    }
	}
    }

    VIF_UNLOCK();
}

static void
X_rsvp_input(struct mbuf *m, int off)
{
    INIT_VNET_INET(curvnet);
    int vifi;
    struct ip *ip = mtod(m, struct ip *);
    struct sockaddr_in rsvp_src = { sizeof rsvp_src, AF_INET };
    struct ifnet *ifp;

    if (rsvpdebug)
	printf("rsvp_input: rsvp_on %d\n", V_rsvp_on);

    /* Can still get packets with rsvp_on = 0 if there is a local member
     * of the group to which the RSVP packet is addressed.  But in this
     * case we want to throw the packet away.
     */
    if (!V_rsvp_on) {
	m_freem(m);
	return;
    }

    if (rsvpdebug)
	printf("rsvp_input: check vifs\n");

#ifdef DIAGNOSTIC
    M_ASSERTPKTHDR(m);
#endif

    ifp = m->m_pkthdr.rcvif;

    VIF_LOCK();
    /* Find which vif the packet arrived on. */
    for (vifi = 0; vifi < numvifs; vifi++)
	if (viftable[vifi].v_ifp == ifp)
	    break;

    if (vifi == numvifs || viftable[vifi].v_rsvpd == NULL) {
	/*
	 * Drop the lock here to avoid holding it across rip_input.
	 * This could make rsvpdebug printfs wrong.  If you care,
	 * record the state of stuff before dropping the lock.
	 */
	VIF_UNLOCK();
	/*
	 * If the old-style non-vif-associated socket is set,
	 * then use it.  Otherwise, drop packet since there
	 * is no specific socket for this vif.
	 */
	if (V_ip_rsvpd != NULL) {
	    if (rsvpdebug)
		printf("rsvp_input: Sending packet up old-style socket\n");
	    rip_input(m, off);  /* xxx */
	} else {
	    if (rsvpdebug && vifi == numvifs)
		printf("rsvp_input: Can't find vif for packet.\n");
	    else if (rsvpdebug && viftable[vifi].v_rsvpd == NULL)
		printf("rsvp_input: No socket defined for vif %d\n",vifi);
	    m_freem(m);
	}
	return;
    }
    rsvp_src.sin_addr = ip->ip_src;

    if (rsvpdebug && m)
	printf("rsvp_input: m->m_len = %d, sbspace() = %ld\n",
	       m->m_len,sbspace(&(viftable[vifi].v_rsvpd->so_rcv)));

    if (socket_send(viftable[vifi].v_rsvpd, m, &rsvp_src) < 0) {
	if (rsvpdebug)
	    printf("rsvp_input: Failed to append to socket\n");
    } else {
	if (rsvpdebug)
	    printf("rsvp_input: send packet up\n");
    }
    VIF_UNLOCK();
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
    struct bw_meter *x;
    uint32_t flags;

    if (!(mrt_api_config & MRT_MFC_BW_UPCALL))
	return EOPNOTSUPP;

    /* Test if the flags are valid */
    if (!(req->bu_flags & (BW_UPCALL_UNIT_PACKETS | BW_UPCALL_UNIT_BYTES)))
	return EINVAL;
    if (!(req->bu_flags & (BW_UPCALL_GEQ | BW_UPCALL_LEQ)))
	return EINVAL;
    if ((req->bu_flags & (BW_UPCALL_GEQ | BW_UPCALL_LEQ))
	    == (BW_UPCALL_GEQ | BW_UPCALL_LEQ))
	return EINVAL;

    /* Test if the threshold time interval is valid */
    if (BW_TIMEVALCMP(&req->bu_threshold.b_time, &delta, <))
	return EINVAL;

    flags = compute_bw_meter_flags(req);

    /*
     * Find if we have already same bw_meter entry
     */
    MFC_LOCK();
    mfc = mfc_find(req->bu_src.s_addr, req->bu_dst.s_addr);
    if (mfc == NULL) {
	MFC_UNLOCK();
	return EADDRNOTAVAIL;
    }
    for (x = mfc->mfc_bw_meter; x != NULL; x = x->bm_mfc_next) {
	if ((BW_TIMEVALCMP(&x->bm_threshold.b_time,
			   &req->bu_threshold.b_time, ==)) &&
	    (x->bm_threshold.b_packets == req->bu_threshold.b_packets) &&
	    (x->bm_threshold.b_bytes == req->bu_threshold.b_bytes) &&
	    (x->bm_flags & BW_METER_USER_FLAGS) == flags)  {
	    MFC_UNLOCK();
	    return 0;		/* XXX Already installed */
	}
    }

    /* Allocate the new bw_meter entry */
    x = (struct bw_meter *)malloc(sizeof(*x), M_BWMETER, M_NOWAIT);
    if (x == NULL) {
	MFC_UNLOCK();
	return ENOBUFS;
    }

    /* Set the new bw_meter entry */
    x->bm_threshold.b_time = req->bu_threshold.b_time;
    GET_TIME(now);
    x->bm_start_time = now;
    x->bm_threshold.b_packets = req->bu_threshold.b_packets;
    x->bm_threshold.b_bytes = req->bu_threshold.b_bytes;
    x->bm_measured.b_packets = 0;
    x->bm_measured.b_bytes = 0;
    x->bm_flags = flags;
    x->bm_time_next = NULL;
    x->bm_time_hash = BW_METER_BUCKETS;

    /* Add the new bw_meter entry to the front of entries for this MFC */
    x->bm_mfc = mfc;
    x->bm_mfc_next = mfc->mfc_bw_meter;
    mfc->mfc_bw_meter = x;
    schedule_bw_meter(x, &now);
    MFC_UNLOCK();

    return 0;
}

static void
free_bw_list(struct bw_meter *list)
{
    while (list != NULL) {
	struct bw_meter *x = list;

	list = list->bm_mfc_next;
	unschedule_bw_meter(x);
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
    struct bw_meter *x;

    if (!(mrt_api_config & MRT_MFC_BW_UPCALL))
	return EOPNOTSUPP;

    MFC_LOCK();
    /* Find the corresponding MFC entry */
    mfc = mfc_find(req->bu_src.s_addr, req->bu_dst.s_addr);
    if (mfc == NULL) {
	MFC_UNLOCK();
	return EADDRNOTAVAIL;
    } else if (req->bu_flags & BW_UPCALL_DELETE_ALL) {
	/*
	 * Delete all bw_meter entries for this mfc
	 */
	struct bw_meter *list;

	list = mfc->mfc_bw_meter;
	mfc->mfc_bw_meter = NULL;
	free_bw_list(list);
	MFC_UNLOCK();
	return 0;
    } else {			/* Delete a single bw_meter entry */
	struct bw_meter *prev;
	uint32_t flags = 0;

	flags = compute_bw_meter_flags(req);

	/* Find the bw_meter entry to delete */
	for (prev = NULL, x = mfc->mfc_bw_meter; x != NULL;
	     prev = x, x = x->bm_mfc_next) {
	    if ((BW_TIMEVALCMP(&x->bm_threshold.b_time,
			       &req->bu_threshold.b_time, ==)) &&
		(x->bm_threshold.b_packets == req->bu_threshold.b_packets) &&
		(x->bm_threshold.b_bytes == req->bu_threshold.b_bytes) &&
		(x->bm_flags & BW_METER_USER_FLAGS) == flags)
		break;
	}
	if (x != NULL) { /* Delete entry from the list for this MFC */
	    if (prev != NULL)
		prev->bm_mfc_next = x->bm_mfc_next;	/* remove from middle*/
	    else
		x->bm_mfc->mfc_bw_meter = x->bm_mfc_next;/* new head of list */

	    unschedule_bw_meter(x);
	    MFC_UNLOCK();
	    /* Free the bw_meter entry */
	    free(x, M_BWMETER);
	    return 0;
	} else {
	    MFC_UNLOCK();
	    return EINVAL;
	}
    }
    /* NOTREACHED */
}

/*
 * Perform bandwidth measurement processing that may result in an upcall
 */
static void
bw_meter_receive_packet(struct bw_meter *x, int plen, struct timeval *nowp)
{
    struct timeval delta;

    MFC_LOCK_ASSERT();

    delta = *nowp;
    BW_TIMEVALDECR(&delta, &x->bm_start_time);

    if (x->bm_flags & BW_METER_GEQ) {
	/*
	 * Processing for ">=" type of bw_meter entry
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
    } else if (x->bm_flags & BW_METER_LEQ) {
	/*
	 * Processing for "<=" type of bw_meter entry
	 */
	if (BW_TIMEVALCMP(&delta, &x->bm_threshold.b_time, >)) {
	    /*
	     * We are behind time with the multicast forwarding table
	     * scanning for "<=" type of bw_meter entries, so test now
	     * if we should deliver an upcall.
	     */
	    if (((x->bm_flags & BW_METER_UNIT_PACKETS) &&
		 (x->bm_measured.b_packets <= x->bm_threshold.b_packets)) ||
		((x->bm_flags & BW_METER_UNIT_BYTES) &&
		 (x->bm_measured.b_bytes <= x->bm_threshold.b_bytes))) {
		/* Prepare an upcall for delivery */
		bw_meter_prepare_upcall(x, nowp);
	    }
	    /* Reschedule the bw_meter entry */
	    unschedule_bw_meter(x);
	    schedule_bw_meter(x, nowp);
	}

	/* Record that a packet is received */
	x->bm_measured.b_packets++;
	x->bm_measured.b_bytes += plen;

	/*
	 * Test if we should restart the measuring interval
	 */
	if ((x->bm_flags & BW_METER_UNIT_PACKETS &&
	     x->bm_measured.b_packets <= x->bm_threshold.b_packets) ||
	    (x->bm_flags & BW_METER_UNIT_BYTES &&
	     x->bm_measured.b_bytes <= x->bm_threshold.b_bytes)) {
	    /* Don't restart the measuring interval */
	} else {
	    /* Do restart the measuring interval */
	    /*
	     * XXX: note that we don't unschedule and schedule, because this
	     * might be too much overhead per packet. Instead, when we process
	     * all entries for a given timer hash bin, we check whether it is
	     * really a timeout. If not, we reschedule at that time.
	     */
	    x->bm_start_time = *nowp;
	    x->bm_measured.b_packets = 0;
	    x->bm_measured.b_bytes = 0;
	    x->bm_flags &= ~BW_METER_UPCALL_DELIVERED;
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

    MFC_LOCK_ASSERT();

    /*
     * Compute the measured time interval
     */
    delta = *nowp;
    BW_TIMEVALDECR(&delta, &x->bm_start_time);

    /*
     * If there are too many pending upcalls, deliver them now
     */
    if (bw_upcalls_n >= BW_UPCALLS_MAX)
	bw_upcalls_send();

    /*
     * Set the bw_upcall entry
     */
    u = &bw_upcalls[bw_upcalls_n++];
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
}

/*
 * Send the pending bandwidth-related upcalls
 */
static void
bw_upcalls_send(void)
{
    INIT_VNET_INET(curvnet);
    struct mbuf *m;
    int len = bw_upcalls_n * sizeof(bw_upcalls[0]);
    struct sockaddr_in k_igmpsrc = { sizeof k_igmpsrc, AF_INET };
    static struct igmpmsg igmpmsg = { 0,		/* unused1 */
				      0,		/* unused2 */
				      IGMPMSG_BW_UPCALL,/* im_msgtype */
				      0,		/* im_mbz  */
				      0,		/* im_vif  */
				      0,		/* unused3 */
				      { 0 },		/* im_src  */
				      { 0 } };		/* im_dst  */

    MFC_LOCK_ASSERT();

    if (bw_upcalls_n == 0)
	return;			/* No pending upcalls */

    bw_upcalls_n = 0;

    /*
     * Allocate a new mbuf, initialize it with the header and
     * the payload for the pending calls.
     */
    MGETHDR(m, M_DONTWAIT, MT_DATA);
    if (m == NULL) {
	log(LOG_WARNING, "bw_upcalls_send: cannot allocate mbuf\n");
	return;
    }

    m->m_len = m->m_pkthdr.len = 0;
    m_copyback(m, 0, sizeof(struct igmpmsg), (caddr_t)&igmpmsg);
    m_copyback(m, sizeof(struct igmpmsg), len, (caddr_t)&bw_upcalls[0]);

    /*
     * Send the upcalls
     * XXX do we need to set the address in k_igmpsrc ?
     */
    mrtstat.mrts_upcalls++;
    if (socket_send(V_ip_mrouter, m, &k_igmpsrc) < 0) {
	log(LOG_WARNING, "bw_upcalls_send: ip_mrouter socket queue full\n");
	++mrtstat.mrts_upq_sockfull;
    }
}

/*
 * Compute the timeout hash value for the bw_meter entries
 */
#define	BW_METER_TIMEHASH(bw_meter, hash)				\
    do {								\
	struct timeval next_timeval = (bw_meter)->bm_start_time;	\
									\
	BW_TIMEVALADD(&next_timeval, &(bw_meter)->bm_threshold.b_time); \
	(hash) = next_timeval.tv_sec;					\
	if (next_timeval.tv_usec)					\
	    (hash)++; /* XXX: make sure we don't timeout early */	\
	(hash) %= BW_METER_BUCKETS;					\
    } while (0)

/*
 * Schedule a timer to process periodically bw_meter entry of type "<="
 * by linking the entry in the proper hash bucket.
 */
static void
schedule_bw_meter(struct bw_meter *x, struct timeval *nowp)
{
    int time_hash;

    MFC_LOCK_ASSERT();

    if (!(x->bm_flags & BW_METER_LEQ))
	return;		/* XXX: we schedule timers only for "<=" entries */

    /*
     * Reset the bw_meter entry
     */
    x->bm_start_time = *nowp;
    x->bm_measured.b_packets = 0;
    x->bm_measured.b_bytes = 0;
    x->bm_flags &= ~BW_METER_UPCALL_DELIVERED;

    /*
     * Compute the timeout hash value and insert the entry
     */
    BW_METER_TIMEHASH(x, time_hash);
    x->bm_time_next = bw_meter_timers[time_hash];
    bw_meter_timers[time_hash] = x;
    x->bm_time_hash = time_hash;
}

/*
 * Unschedule the periodic timer that processes bw_meter entry of type "<="
 * by removing the entry from the proper hash bucket.
 */
static void
unschedule_bw_meter(struct bw_meter *x)
{
    int time_hash;
    struct bw_meter *prev, *tmp;

    MFC_LOCK_ASSERT();

    if (!(x->bm_flags & BW_METER_LEQ))
	return;		/* XXX: we schedule timers only for "<=" entries */

    /*
     * Compute the timeout hash value and delete the entry
     */
    time_hash = x->bm_time_hash;
    if (time_hash >= BW_METER_BUCKETS)
	return;		/* Entry was not scheduled */

    for (prev = NULL, tmp = bw_meter_timers[time_hash];
	     tmp != NULL; prev = tmp, tmp = tmp->bm_time_next)
	if (tmp == x)
	    break;

    if (tmp == NULL)
	panic("unschedule_bw_meter: bw_meter entry not found");

    if (prev != NULL)
	prev->bm_time_next = x->bm_time_next;
    else
	bw_meter_timers[time_hash] = x->bm_time_next;

    x->bm_time_next = NULL;
    x->bm_time_hash = BW_METER_BUCKETS;
}


/*
 * Process all "<=" type of bw_meter that should be processed now,
 * and for each entry prepare an upcall if necessary. Each processed
 * entry is rescheduled again for the (periodic) processing.
 *
 * This is run periodically (once per second normally). On each round,
 * all the potentially matching entries are in the hash slot that we are
 * looking at.
 */
static void
bw_meter_process()
{
    static uint32_t last_tv_sec;	/* last time we processed this */

    uint32_t loops;
    int i;
    struct timeval now, process_endtime;

    GET_TIME(now);
    if (last_tv_sec == now.tv_sec)
	return;		/* nothing to do */

    loops = now.tv_sec - last_tv_sec;
    last_tv_sec = now.tv_sec;
    if (loops > BW_METER_BUCKETS)
	loops = BW_METER_BUCKETS;

    MFC_LOCK();
    /*
     * Process all bins of bw_meter entries from the one after the last
     * processed to the current one. On entry, i points to the last bucket
     * visited, so we need to increment i at the beginning of the loop.
     */
    for (i = (now.tv_sec - loops) % BW_METER_BUCKETS; loops > 0; loops--) {
	struct bw_meter *x, *tmp_list;

	if (++i >= BW_METER_BUCKETS)
	    i = 0;

	/* Disconnect the list of bw_meter entries from the bin */
	tmp_list = bw_meter_timers[i];
	bw_meter_timers[i] = NULL;

	/* Process the list of bw_meter entries */
	while (tmp_list != NULL) {
	    x = tmp_list;
	    tmp_list = tmp_list->bm_time_next;

	    /* Test if the time interval is over */
	    process_endtime = x->bm_start_time;
	    BW_TIMEVALADD(&process_endtime, &x->bm_threshold.b_time);
	    if (BW_TIMEVALCMP(&process_endtime, &now, >)) {
		/* Not yet: reschedule, but don't reset */
		int time_hash;

		BW_METER_TIMEHASH(x, time_hash);
		if (time_hash == i && process_endtime.tv_sec == now.tv_sec) {
		    /*
		     * XXX: somehow the bin processing is a bit ahead of time.
		     * Put the entry in the next bin.
		     */
		    if (++time_hash >= BW_METER_BUCKETS)
			time_hash = 0;
		}
		x->bm_time_next = bw_meter_timers[time_hash];
		bw_meter_timers[time_hash] = x;
		x->bm_time_hash = time_hash;

		continue;
	    }

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

	    /*
	     * Reschedule for next processing
	     */
	    schedule_bw_meter(x, &now);
	}
    }

    /* Send all upcalls that are pending delivery */
    bw_upcalls_send();

    MFC_UNLOCK();
}

/*
 * A periodic function for sending all upcalls that are pending delivery
 */
static void
expire_bw_upcalls_send(void *unused)
{
    MFC_LOCK();
    bw_upcalls_send();
    MFC_UNLOCK();

    callout_reset(&bw_upcalls_ch, BW_UPCALLS_PERIOD,
	expire_bw_upcalls_send, NULL);
}

/*
 * A periodic function for periodic scanning of the multicast forwarding
 * table for processing all "<=" bw_meter entries.
 */
static void
expire_bw_meter_process(void *unused)
{
    if (mrt_api_config & MRT_MFC_BW_UPCALL)
	bw_meter_process();

    callout_reset(&bw_meter_ch, BW_METER_PERIOD, expire_bw_meter_process, NULL);
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

    if (mrtdebug & DEBUG_PIM)
	log(LOG_DEBUG, "pim_register_send: ");

    /*
     * Do not send IGMP_WHOLEPKT notifications to userland, if the
     * rendezvous point was unspecified, and we were told not to.
     */
    if (pim_squelch_wholepkt != 0 && (mrt_api_config & MRT_MFC_RP) &&
	(rt->mfc_rp.s_addr == INADDR_ANY))
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
	    if ((mrt_api_config & MRT_MFC_RP) &&
		(rt->mfc_rp.s_addr != INADDR_ANY)) {
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
    mb_copy = m_copypacket(m, M_DONTWAIT);
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

    if (ip->ip_len <= mtu) {
	/* Turn the IP header into a valid one */
	ip->ip_len = htons(ip->ip_len);
	ip->ip_off = htons(ip->ip_off);
	ip->ip_sum = 0;
	ip->ip_sum = in_cksum(mb_copy, ip->ip_hl << 2);
    } else {
	/* Fragment the packet */
	if (ip_fragment(ip, &mb_copy, mtu, 0, CSUM_DELAY_IP) != 0) {
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
    INIT_VNET_INET(curvnet);
    struct mbuf *mb_first;
    int len = ntohs(ip->ip_len);
    struct igmpmsg *im;
    struct sockaddr_in k_igmpsrc = { sizeof k_igmpsrc, AF_INET };

    VIF_LOCK_ASSERT();

    /*
     * Add a new mbuf with an upcall header
     */
    MGETHDR(mb_first, M_DONTWAIT, MT_DATA);
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
    im->im_vif		= vifp - viftable;
    im->im_src		= ip->ip_src;
    im->im_dst		= ip->ip_dst;

    k_igmpsrc.sin_addr	= ip->ip_src;

    mrtstat.mrts_upcalls++;

    if (socket_send(V_ip_mrouter, mb_first, &k_igmpsrc) < 0) {
	if (mrtdebug & DEBUG_PIM)
	    log(LOG_WARNING,
		"mcast: pim_register_send_upcall: ip_mrouter socket queue full");
	++mrtstat.mrts_upq_sockfull;
	return ENOBUFS;
    }

    /* Keep statistics */
    pimstat.pims_snd_registers_msgs++;
    pimstat.pims_snd_registers_bytes += len;

    return 0;
}

/*
 * Encapsulate the data packet in PIM Register message and send it to the RP.
 */
static int
pim_register_send_rp(struct ip *ip, struct vif *vifp, struct mbuf *mb_copy,
    struct mfc *rt)
{
    INIT_VNET_INET(curvnet);
    struct mbuf *mb_first;
    struct ip *ip_outer;
    struct pim_encap_pimhdr *pimhdr;
    int len = ntohs(ip->ip_len);
    vifi_t vifi = rt->mfc_parent;

    VIF_LOCK_ASSERT();

    if ((vifi >= numvifs) || (viftable[vifi].v_lcl_addr.s_addr == 0)) {
	m_freem(mb_copy);
	return EADDRNOTAVAIL;		/* The iif vif is invalid */
    }

    /*
     * Add a new mbuf with the encapsulating header
     */
    MGETHDR(mb_first, M_DONTWAIT, MT_DATA);
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
    ip_outer->ip_id = ip_newid();
    ip_outer->ip_len = len + sizeof(pim_encap_iphdr) + sizeof(pim_encap_pimhdr);
    ip_outer->ip_src = viftable[vifi].v_lcl_addr;
    ip_outer->ip_dst = rt->mfc_rp;
    /*
     * Copy the inner header TOS to the outer header, and take care of the
     * IP_DF bit.
     */
    ip_outer->ip_tos = ip->ip_tos;
    if (ntohs(ip->ip_off) & IP_DF)
	ip_outer->ip_off |= IP_DF;
    pimhdr = (struct pim_encap_pimhdr *)((caddr_t)ip_outer
					 + sizeof(pim_encap_iphdr));
    *pimhdr = pim_encap_pimhdr;
    /* If the iif crosses a border, set the Border-bit */
    if (rt->mfc_flags[vifi] & MRT_MFC_FLAGS_BORDER_VIF & mrt_api_config)
	pimhdr->flags |= htonl(PIM_BORDER_REGISTER);

    mb_first->m_data += sizeof(pim_encap_iphdr);
    pimhdr->pim.pim_cksum = in_cksum(mb_first, sizeof(pim_encap_pimhdr));
    mb_first->m_data -= sizeof(pim_encap_iphdr);

    send_packet(vifp, mb_first);

    /* Keep statistics */
    pimstat.pims_snd_registers_msgs++;
    pimstat.pims_snd_registers_bytes += len;

    return 0;
}

/*
 * pim_encapcheck() is called by the encap[46]_input() path at runtime to
 * determine if a packet is for PIM; allowing PIM to be dynamically loaded
 * into the kernel.
 */
static int
pim_encapcheck(const struct mbuf *m, int off, int proto, void *arg)
{

#ifdef DIAGNOSTIC
    KASSERT(proto == IPPROTO_PIM, ("not for IPPROTO_PIM"));
#endif
    if (proto != IPPROTO_PIM)
	return 0;	/* not for us; reject the datagram. */

    return 64;		/* claim the datagram. */
}

/*
 * PIM-SMv2 and PIM-DM messages processing.
 * Receives and verifies the PIM control messages, and passes them
 * up to the listening socket, using rip_input().
 * The only message with special processing is the PIM_REGISTER message
 * (used by PIM-SM): the PIM header is stripped off, and the inner packet
 * is passed to if_simloop().
 */
void
pim_input(struct mbuf *m, int off)
{
    struct ip *ip = mtod(m, struct ip *);
    struct pim *pim;
    int minlen;
    int datalen = ip->ip_len;
    int ip_tos;
    int iphlen = off;

    /* Keep statistics */
    pimstat.pims_rcv_total_msgs++;
    pimstat.pims_rcv_total_bytes += datalen;

    /*
     * Validate lengths
     */
    if (datalen < PIM_MINLEN) {
	pimstat.pims_rcv_tooshort++;
	log(LOG_ERR, "pim_input: packet size too small %d from %lx\n",
	    datalen, (u_long)ip->ip_src.s_addr);
	m_freem(m);
	return;
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
    if ((m->m_flags & M_EXT || m->m_len < minlen) &&
	(m = m_pullup(m, minlen)) == 0) {
	log(LOG_ERR, "pim_input: m_pullup failure\n");
	return;
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
	pimstat.pims_rcv_badsum++;
	if (mrtdebug & DEBUG_PIM)
	    log(LOG_DEBUG, "pim_input: invalid checksum");
	m_freem(m);
	return;
    }

    /* PIM version check */
    if (PIM_VT_V(pim->pim_vt) < PIM_VERSION) {
	pimstat.pims_rcv_badversion++;
	log(LOG_ERR, "pim_input: incorrect version %d, expecting %d\n",
	    PIM_VT_V(pim->pim_vt), PIM_VERSION);
	m_freem(m);
	return;
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

	VIF_LOCK();
	if ((reg_vif_num >= numvifs) || (reg_vif_num == VIFI_INVALID)) {
	    VIF_UNLOCK();
	    if (mrtdebug & DEBUG_PIM)
		log(LOG_DEBUG,
		    "pim_input: register vif not set: %d\n", reg_vif_num);
	    m_freem(m);
	    return;
	}
	/* XXX need refcnt? */
	vifp = viftable[reg_vif_num].v_ifp;
	VIF_UNLOCK();

	/*
	 * Validate length
	 */
	if (datalen < PIM_REG_MINLEN) {
	    pimstat.pims_rcv_tooshort++;
	    pimstat.pims_rcv_badregisters++;
	    log(LOG_ERR,
		"pim_input: register packet size too small %d from %lx\n",
		datalen, (u_long)ip->ip_src.s_addr);
	    m_freem(m);
	    return;
	}

	reghdr = (u_int32_t *)(pim + 1);
	encap_ip = (struct ip *)(reghdr + 1);

	if (mrtdebug & DEBUG_PIM) {
	    log(LOG_DEBUG,
		"pim_input[register], encap_ip: %lx -> %lx, encap_ip len %d\n",
		(u_long)ntohl(encap_ip->ip_src.s_addr),
		(u_long)ntohl(encap_ip->ip_dst.s_addr),
		ntohs(encap_ip->ip_len));
	}

	/* verify the version number of the inner packet */
	if (encap_ip->ip_v != IPVERSION) {
	    pimstat.pims_rcv_badregisters++;
	    if (mrtdebug & DEBUG_PIM) {
		log(LOG_DEBUG, "pim_input: invalid IP version (%d) "
		    "of the inner packet\n", encap_ip->ip_v);
	    }
	    m_freem(m);
	    return;
	}

	/* verify the inner packet is destined to a mcast group */
	if (!IN_MULTICAST(ntohl(encap_ip->ip_dst.s_addr))) {
	    pimstat.pims_rcv_badregisters++;
	    if (mrtdebug & DEBUG_PIM)
		log(LOG_DEBUG,
		    "pim_input: inner packet of register is not "
		    "multicast %lx\n",
		    (u_long)ntohl(encap_ip->ip_dst.s_addr));
	    m_freem(m);
	    return;
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
	mcp = m_copy(m, 0, iphlen + PIM_REG_MINLEN);
	if (mcp == NULL) {
	    log(LOG_ERR,
		"pim_input: pim register: could not copy register head\n");
	    m_freem(m);
	    return;
	}

	/* Keep statistics */
	/* XXX: registers_bytes include only the encap. mcast pkt */
	pimstat.pims_rcv_registers_msgs++;
	pimstat.pims_rcv_registers_bytes += ntohs(encap_ip->ip_len);

	/*
	 * forward the inner ip packet; point m_data at the inner ip.
	 */
	m_adj(m, iphlen + PIM_MINLEN);

	if (mrtdebug & DEBUG_PIM) {
	    log(LOG_DEBUG,
		"pim_input: forwarding decapsulated register: "
		"src %lx, dst %lx, vif %d\n",
		(u_long)ntohl(encap_ip->ip_src.s_addr),
		(u_long)ntohl(encap_ip->ip_dst.s_addr),
		reg_vif_num);
	}
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
    rip_input(m, iphlen);

    return;
}

/*
 * XXX: This is common code for dealing with initialization for both
 * the IPv4 and IPv6 multicast forwarding paths. It could do with cleanup.
 */
static int
ip_mroute_modevent(module_t mod, int type, void *unused)
{
    INIT_VNET_INET(curvnet);

    switch (type) {
    case MOD_LOAD:
	MROUTER_LOCK_INIT();
	MFC_LOCK_INIT();
	VIF_LOCK_INIT();
	ip_mrouter_reset();
	TUNABLE_ULONG_FETCH("net.inet.pim.squelch_wholepkt",
	    &pim_squelch_wholepkt);

	pim_encap_cookie = encap_attach_func(AF_INET, IPPROTO_PIM,
	    pim_encapcheck, &in_pim_protosw, NULL);
	if (pim_encap_cookie == NULL) {
		printf("ip_mroute: unable to attach pim encap\n");
		VIF_LOCK_DESTROY();
		MFC_LOCK_DESTROY();
		MROUTER_LOCK_DESTROY();
		return (EINVAL);
	}

#ifdef INET6
	pim6_encap_cookie = encap_attach_func(AF_INET6, IPPROTO_PIM,
	    pim_encapcheck, (struct protosw *)&in6_pim_protosw, NULL);
	if (pim6_encap_cookie == NULL) {
		printf("ip_mroute: unable to attach pim6 encap\n");
		if (pim_encap_cookie) {
		    encap_detach(pim_encap_cookie);
		    pim_encap_cookie = NULL;
		}
		VIF_LOCK_DESTROY();
		MFC_LOCK_DESTROY();
		MROUTER_LOCK_DESTROY();
		return (EINVAL);
	}
#endif

	ip_mcast_src = X_ip_mcast_src;
	ip_mforward = X_ip_mforward;
	ip_mrouter_done = X_ip_mrouter_done;
	ip_mrouter_get = X_ip_mrouter_get;
	ip_mrouter_set = X_ip_mrouter_set;

#ifdef INET6
	ip6_mforward = X_ip6_mforward;
	ip6_mrouter_done = X_ip6_mrouter_done;
	ip6_mrouter_get = X_ip6_mrouter_get;
	ip6_mrouter_set = X_ip6_mrouter_set;
	mrt6_ioctl = X_mrt6_ioctl;
#endif

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
	if (V_ip_mrouter
#ifdef INET6
	    || ip6_mrouter
#endif
	)
	    return EINVAL;

#ifdef INET6
	if (pim6_encap_cookie) {
	    encap_detach(pim6_encap_cookie);
	    pim6_encap_cookie = NULL;
	}
	X_ip6_mrouter_done();
	ip6_mforward = NULL;
	ip6_mrouter_done = NULL;
	ip6_mrouter_get = NULL;
	ip6_mrouter_set = NULL;
	mrt6_ioctl = NULL;
#endif

	if (pim_encap_cookie) {
	    encap_detach(pim_encap_cookie);
	    pim_encap_cookie = NULL;
	}
	X_ip_mrouter_done();
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

	VIF_LOCK_DESTROY();
	MFC_LOCK_DESTROY();
	MROUTER_LOCK_DESTROY();
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
DECLARE_MODULE(ip_mroute, ip_mroutemod, SI_SUB_PSEUDO, SI_ORDER_ANY);
