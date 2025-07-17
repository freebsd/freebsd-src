/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2003-2009 Sam Leffler, Errno Consulting
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
/*
 * IEEE 802.11 support (FreeBSD-specific code)
 */
#include "opt_wlan.h"

#include <sys/param.h>
#include <sys/systm.h> 
#include <sys/eventhandler.h>
#include <sys/kernel.h>
#include <sys/linker.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>   
#include <sys/module.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/stdarg.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>

#include <sys/socket.h>

#include <net/bpf.h>
#include <net/debugnet.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_dl.h>
#include <net/if_clone.h>
#include <net/if_media.h>
#include <net/if_private.h>
#include <net/if_types.h>
#include <net/ethernet.h>
#include <net/route.h>
#include <net/vnet.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_input.h>

DEBUGNET_DEFINE(ieee80211);
SYSCTL_NODE(_net, OID_AUTO, wlan, CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
    "IEEE 80211 parameters");

#ifdef IEEE80211_DEBUG
static int	ieee80211_debug = 0;
SYSCTL_INT(_net_wlan, OID_AUTO, debug, CTLFLAG_RW, &ieee80211_debug,
	    0, "debugging printfs");
#endif

static const char wlanname[] = "wlan";
static struct if_clone *wlan_cloner;

/*
 * priv(9) NET80211 checks.
 * Return 0 if operation is allowed, E* (usually EPERM) otherwise.
 */
int
ieee80211_priv_check_vap_getkey(u_long cmd __unused,
     struct ieee80211vap *vap __unused, struct ifnet *ifp __unused)
{

	return (priv_check(curthread, PRIV_NET80211_VAP_GETKEY));
}

int
ieee80211_priv_check_vap_manage(u_long cmd __unused,
     struct ieee80211vap *vap __unused, struct ifnet *ifp __unused)
{

	return (priv_check(curthread, PRIV_NET80211_VAP_MANAGE));
}

int
ieee80211_priv_check_vap_setmac(u_long cmd __unused,
     struct ieee80211vap *vap __unused, struct ifnet *ifp __unused)
{

	return (priv_check(curthread, PRIV_NET80211_VAP_SETMAC));
}

int
ieee80211_priv_check_create_vap(u_long cmd __unused,
    struct ieee80211vap *vap __unused, struct ifnet *ifp __unused)
{

	return (priv_check(curthread, PRIV_NET80211_CREATE_VAP));
}

static int
wlan_clone_create(struct if_clone *ifc, char *name, size_t len,
    struct ifc_data *ifd, struct ifnet **ifpp)
{
	struct ieee80211_clone_params cp;
	struct ieee80211vap *vap;
	struct ieee80211com *ic;
	int error;

	error = ieee80211_priv_check_create_vap(0, NULL, NULL);
	if (error)
		return error;

	error = ifc_copyin(ifd, &cp, sizeof(cp));
	if (error)
		return error;
	ic = ieee80211_find_com(cp.icp_parent);
	if (ic == NULL)
		return ENXIO;
	if (cp.icp_opmode >= IEEE80211_OPMODE_MAX) {
		ic_printf(ic, "%s: invalid opmode %d\n", __func__,
		    cp.icp_opmode);
		return EINVAL;
	}
	if ((ic->ic_caps & ieee80211_opcap[cp.icp_opmode]) == 0) {
		ic_printf(ic, "%s mode not supported\n",
		    ieee80211_opmode_name[cp.icp_opmode]);
		return EOPNOTSUPP;
	}
	if ((cp.icp_flags & IEEE80211_CLONE_TDMA) &&
#ifdef IEEE80211_SUPPORT_TDMA
	    (ic->ic_caps & IEEE80211_C_TDMA) == 0
#else
	    (1)
#endif
	) {
		ic_printf(ic, "TDMA not supported\n");
		return EOPNOTSUPP;
	}
	vap = ic->ic_vap_create(ic, wlanname, ifd->unit,
			cp.icp_opmode, cp.icp_flags, cp.icp_bssid,
			cp.icp_flags & IEEE80211_CLONE_MACADDR ?
			    cp.icp_macaddr : ic->ic_macaddr);

	if (vap == NULL)
		return (EIO);

#ifdef DEBUGNET
	if (ic->ic_debugnet_meth != NULL)
		DEBUGNET_SET(vap->iv_ifp, ieee80211);
#endif
	*ifpp = vap->iv_ifp;

	return (0);
}

static int
wlan_clone_destroy(struct if_clone *ifc, struct ifnet *ifp, uint32_t flags)
{
	struct ieee80211vap *vap = ifp->if_softc;
	struct ieee80211com *ic = vap->iv_ic;

	ic->ic_vap_delete(vap);

	return (0);
}

void
ieee80211_vap_destroy(struct ieee80211vap *vap)
{
	CURVNET_SET(vap->iv_ifp->if_vnet);
	if_clone_destroyif(wlan_cloner, vap->iv_ifp);
	CURVNET_RESTORE();
}

int
ieee80211_sysctl_msecs_ticks(SYSCTL_HANDLER_ARGS)
{
	int msecs = ticks_to_msecs(*(int *)arg1);
	int error;

	error = sysctl_handle_int(oidp, &msecs, 0, req);
	if (error || !req->newptr)
		return error;
	*(int *)arg1 = msecs_to_ticks(msecs);
	return 0;
}

static int
ieee80211_sysctl_inact(SYSCTL_HANDLER_ARGS)
{
	int inact = (*(int *)arg1) * IEEE80211_INACT_WAIT;
	int error;

	error = sysctl_handle_int(oidp, &inact, 0, req);
	if (error || !req->newptr)
		return error;
	*(int *)arg1 = inact / IEEE80211_INACT_WAIT;
	return 0;
}

static int
ieee80211_sysctl_parent(SYSCTL_HANDLER_ARGS)
{
	struct ieee80211com *ic = arg1;

	return SYSCTL_OUT_STR(req, ic->ic_name);
}

static int
ieee80211_sysctl_radar(SYSCTL_HANDLER_ARGS)
{
	struct ieee80211com *ic = arg1;
	int t = 0, error;

	error = sysctl_handle_int(oidp, &t, 0, req);
	if (error || !req->newptr)
		return error;
	IEEE80211_LOCK(ic);
	ieee80211_dfs_notify_radar(ic, ic->ic_curchan);
	IEEE80211_UNLOCK(ic);
	return 0;
}

/*
 * For now, just restart everything.
 *
 * Later on, it'd be nice to have a separate VAP restart to
 * full-device restart.
 */
static int
ieee80211_sysctl_vap_restart(SYSCTL_HANDLER_ARGS)
{
	struct ieee80211vap *vap = arg1;
	int t = 0, error;

	error = sysctl_handle_int(oidp, &t, 0, req);
	if (error || !req->newptr)
		return error;

	ieee80211_restart_all(vap->iv_ic);
	return 0;
}

void
ieee80211_sysctl_attach(struct ieee80211com *ic)
{
}

void
ieee80211_sysctl_detach(struct ieee80211com *ic)
{
}

void
ieee80211_sysctl_vattach(struct ieee80211vap *vap)
{
	struct ifnet *ifp = vap->iv_ifp;
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid *oid;
	char num[14];			/* sufficient for 32 bits */

	ctx = (struct sysctl_ctx_list *) IEEE80211_MALLOC(sizeof(struct sysctl_ctx_list),
		M_DEVBUF, IEEE80211_M_NOWAIT | IEEE80211_M_ZERO);
	if (ctx == NULL) {
		net80211_vap_printf(vap,
		    "%s: cannot allocate sysctl context!\n", __func__);
		return;
	}
	sysctl_ctx_init(ctx);
	snprintf(num, sizeof(num), "%u", ifp->if_dunit);
	oid = SYSCTL_ADD_NODE(ctx, &SYSCTL_NODE_CHILDREN(_net, wlan),
	    OID_AUTO, num, CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "%parent", CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_NEEDGIANT,
	    vap->iv_ic, 0, ieee80211_sysctl_parent, "A", "parent device");
	SYSCTL_ADD_UINT(ctx, SYSCTL_CHILDREN(oid), OID_AUTO,
		"driver_caps", CTLFLAG_RW, &vap->iv_caps, 0,
		"driver capabilities");
#ifdef IEEE80211_DEBUG
	vap->iv_debug = ieee80211_debug;
	SYSCTL_ADD_UINT(ctx, SYSCTL_CHILDREN(oid), OID_AUTO,
		"debug", CTLFLAG_RW, &vap->iv_debug, 0,
		"control debugging printfs");
#endif
	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(oid), OID_AUTO,
		"bmiss_max", CTLFLAG_RW, &vap->iv_bmiss_max, 0,
		"consecutive beacon misses before scanning");
	/* XXX inherit from tunables */
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "inact_run", CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_NEEDGIANT,
	    &vap->iv_inact_run, 0, ieee80211_sysctl_inact, "I",
	    "station inactivity timeout (sec)");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "inact_probe", CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_NEEDGIANT,
	    &vap->iv_inact_probe, 0, ieee80211_sysctl_inact, "I",
	    "station inactivity probe timeout (sec)");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "inact_auth", CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_NEEDGIANT,
	    &vap->iv_inact_auth, 0, ieee80211_sysctl_inact, "I",
	    "station authentication timeout (sec)");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "inact_init", CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_NEEDGIANT,
	    &vap->iv_inact_init, 0, ieee80211_sysctl_inact, "I",
	    "station initial state timeout (sec)");
	if (vap->iv_htcaps & IEEE80211_HTC_HT) {
		SYSCTL_ADD_UINT(ctx, SYSCTL_CHILDREN(oid), OID_AUTO,
			"ampdu_mintraffic_bk", CTLFLAG_RW,
			&vap->iv_ampdu_mintraffic[WME_AC_BK], 0,
			"BK traffic tx aggr threshold (pps)");
		SYSCTL_ADD_UINT(ctx, SYSCTL_CHILDREN(oid), OID_AUTO,
			"ampdu_mintraffic_be", CTLFLAG_RW,
			&vap->iv_ampdu_mintraffic[WME_AC_BE], 0,
			"BE traffic tx aggr threshold (pps)");
		SYSCTL_ADD_UINT(ctx, SYSCTL_CHILDREN(oid), OID_AUTO,
			"ampdu_mintraffic_vo", CTLFLAG_RW,
			&vap->iv_ampdu_mintraffic[WME_AC_VO], 0,
			"VO traffic tx aggr threshold (pps)");
		SYSCTL_ADD_UINT(ctx, SYSCTL_CHILDREN(oid), OID_AUTO,
			"ampdu_mintraffic_vi", CTLFLAG_RW,
			&vap->iv_ampdu_mintraffic[WME_AC_VI], 0,
			"VI traffic tx aggr threshold (pps)");
	}

	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "force_restart", CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_NEEDGIANT,
	    vap, 0, ieee80211_sysctl_vap_restart, "I", "force a VAP restart");

	if (vap->iv_caps & IEEE80211_C_DFS) {
		SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(oid), OID_AUTO,
		    "radar", CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_NEEDGIANT,
		    vap->iv_ic, 0, ieee80211_sysctl_radar, "I",
		    "simulate radar event");
	}
	vap->iv_sysctl = ctx;
	vap->iv_oid = oid;
}

void
ieee80211_sysctl_vdetach(struct ieee80211vap *vap)
{

	if (vap->iv_sysctl != NULL) {
		sysctl_ctx_free(vap->iv_sysctl);
		IEEE80211_FREE(vap->iv_sysctl, M_DEVBUF);
		vap->iv_sysctl = NULL;
	}
}

int
ieee80211_com_vincref(struct ieee80211vap *vap)
{
	uint32_t ostate;

	ostate = atomic_fetchadd_32(&vap->iv_com_state, IEEE80211_COM_REF_ADD);

	if (ostate & IEEE80211_COM_DETACHED) {
		atomic_subtract_32(&vap->iv_com_state, IEEE80211_COM_REF_ADD);
		return (ENETDOWN);
	}

	if (_IEEE80211_MASKSHIFT(ostate, IEEE80211_COM_REF) ==
	    IEEE80211_COM_REF_MAX) {
		atomic_subtract_32(&vap->iv_com_state, IEEE80211_COM_REF_ADD);
		return (EOVERFLOW);
	}

	return (0);
}

void
ieee80211_com_vdecref(struct ieee80211vap *vap)
{
	uint32_t ostate;

	ostate = atomic_fetchadd_32(&vap->iv_com_state, -IEEE80211_COM_REF_ADD);

	KASSERT(_IEEE80211_MASKSHIFT(ostate, IEEE80211_COM_REF) != 0,
	    ("com reference counter underflow"));

	(void) ostate;
}

void
ieee80211_com_vdetach(struct ieee80211vap *vap)
{
	int sleep_time;

	sleep_time = msecs_to_ticks(250);
	atomic_set_32(&vap->iv_com_state, IEEE80211_COM_DETACHED);
	while (_IEEE80211_MASKSHIFT(atomic_load_32(&vap->iv_com_state),
	    IEEE80211_COM_REF) != 0)
		pause("comref", sleep_time);
}

int
ieee80211_node_dectestref(struct ieee80211_node *ni)
{
	/* XXX need equivalent of atomic_dec_and_test */
	atomic_subtract_int(&ni->ni_refcnt, 1);
	return atomic_cmpset_int(&ni->ni_refcnt, 0, 1);
}

void
ieee80211_drain_ifq(struct ifqueue *ifq)
{
	struct ieee80211_node *ni;
	struct mbuf *m;

	for (;;) {
		IF_DEQUEUE(ifq, m);
		if (m == NULL)
			break;

		ni = (struct ieee80211_node *)m->m_pkthdr.rcvif;
		KASSERT(ni != NULL, ("frame w/o node"));
		ieee80211_free_node(ni);
		m->m_pkthdr.rcvif = NULL;

		m_freem(m);
	}
}

void
ieee80211_flush_ifq(struct ifqueue *ifq, struct ieee80211vap *vap)
{
	struct ieee80211_node *ni;
	struct mbuf *m, **mprev;

	IF_LOCK(ifq);
	mprev = &ifq->ifq_head;
	while ((m = *mprev) != NULL) {
		ni = (struct ieee80211_node *)m->m_pkthdr.rcvif;
		if (ni != NULL && ni->ni_vap == vap) {
			*mprev = m->m_nextpkt;		/* remove from list */
			ifq->ifq_len--;

			m_freem(m);
			ieee80211_free_node(ni);	/* reclaim ref */
		} else
			mprev = &m->m_nextpkt;
	}
	/* recalculate tail ptr */
	m = ifq->ifq_head;
	for (; m != NULL && m->m_nextpkt != NULL; m = m->m_nextpkt)
		;
	ifq->ifq_tail = m;
	IF_UNLOCK(ifq);
}

/*
 * As above, for mbufs allocated with m_gethdr/MGETHDR
 * or initialized by M_COPY_PKTHDR.
 */
#define	MC_ALIGN(m, len)						\
do {									\
	(m)->m_data += rounddown2(MCLBYTES - (len), sizeof(long));	\
} while (/* CONSTCOND */ 0)

/*
 * Allocate and setup a management frame of the specified
 * size.  We return the mbuf and a pointer to the start
 * of the contiguous data area that's been reserved based
 * on the packet length.  The data area is forced to 32-bit
 * alignment and the buffer length to a multiple of 4 bytes.
 * This is done mainly so beacon frames (that require this)
 * can use this interface too.
 */
struct mbuf *
ieee80211_getmgtframe(uint8_t **frm, int headroom, int pktlen)
{
	struct mbuf *m;
	u_int len;

	/*
	 * NB: we know the mbuf routines will align the data area
	 *     so we don't need to do anything special.
	 */
	len = roundup2(headroom + pktlen, 4);
	KASSERT(len <= MCLBYTES, ("802.11 mgt frame too large: %u", len));
	if (len < MINCLSIZE) {
		m = m_gethdr(IEEE80211_M_NOWAIT, MT_DATA);
		/*
		 * Align the data in case additional headers are added.
		 * This should only happen when a WEP header is added
		 * which only happens for shared key authentication mgt
		 * frames which all fit in MHLEN.
		 */
		if (m != NULL)
			M_ALIGN(m, len);
	} else {
		m = m_getcl(IEEE80211_M_NOWAIT, MT_DATA, M_PKTHDR);
		if (m != NULL)
			MC_ALIGN(m, len);
	}
	if (m != NULL) {
		m->m_data += headroom;
		*frm = m->m_data;
	}
	return m;
}

#ifndef __NO_STRICT_ALIGNMENT
/*
 * Re-align the payload in the mbuf.  This is mainly used (right now)
 * to handle IP header alignment requirements on certain architectures.
 */
struct mbuf *
ieee80211_realign(struct ieee80211vap *vap, struct mbuf *m, size_t align)
{
	int pktlen, space;
	struct mbuf *n;

	pktlen = m->m_pkthdr.len;
	space = pktlen + align;
	if (space < MINCLSIZE)
		n = m_gethdr(IEEE80211_M_NOWAIT, MT_DATA);
	else {
		n = m_getjcl(IEEE80211_M_NOWAIT, MT_DATA, M_PKTHDR,
		    space <= MCLBYTES ?     MCLBYTES :
#if MJUMPAGESIZE != MCLBYTES
		    space <= MJUMPAGESIZE ? MJUMPAGESIZE :
#endif
		    space <= MJUM9BYTES ?   MJUM9BYTES : MJUM16BYTES);
	}
	if (__predict_true(n != NULL)) {
		m_move_pkthdr(n, m);
		n->m_data = (caddr_t)(ALIGN(n->m_data + align) - align);
		m_copydata(m, 0, pktlen, mtod(n, caddr_t));
		n->m_len = pktlen;
	} else {
		IEEE80211_DISCARD(vap, IEEE80211_MSG_ANY,
		    mtod(m, const struct ieee80211_frame *), NULL,
		    "%s", "no mbuf to realign");
		vap->iv_stats.is_rx_badalign++;
	}
	m_freem(m);
	return n;
}
#endif /* !__NO_STRICT_ALIGNMENT */

int
ieee80211_add_callback(struct mbuf *m,
	void (*func)(struct ieee80211_node *, void *, int), void *arg)
{
	struct m_tag *mtag;
	struct ieee80211_cb *cb;

	mtag = m_tag_alloc(MTAG_ABI_NET80211, NET80211_TAG_CALLBACK,
			sizeof(struct ieee80211_cb), IEEE80211_M_NOWAIT);
	if (mtag == NULL)
		return 0;

	cb = (struct ieee80211_cb *)(mtag+1);
	cb->func = func;
	cb->arg = arg;
	m_tag_prepend(m, mtag);
	m->m_flags |= M_TXCB;
	return 1;
}

int
ieee80211_add_xmit_params(struct mbuf *m,
    const struct ieee80211_bpf_params *params)
{
	struct m_tag *mtag;
	struct ieee80211_tx_params *tx;

	mtag = m_tag_alloc(MTAG_ABI_NET80211, NET80211_TAG_XMIT_PARAMS,
	    sizeof(struct ieee80211_tx_params), IEEE80211_M_NOWAIT);
	if (mtag == NULL)
		return (0);

	tx = (struct ieee80211_tx_params *)(mtag+1);
	memcpy(&tx->params, params, sizeof(struct ieee80211_bpf_params));
	m_tag_prepend(m, mtag);
	return (1);
}

int
ieee80211_get_xmit_params(struct mbuf *m,
    struct ieee80211_bpf_params *params)
{
	struct m_tag *mtag;
	struct ieee80211_tx_params *tx;

	mtag = m_tag_locate(m, MTAG_ABI_NET80211, NET80211_TAG_XMIT_PARAMS,
	    NULL);
	if (mtag == NULL)
		return (-1);
	tx = (struct ieee80211_tx_params *)(mtag + 1);
	memcpy(params, &tx->params, sizeof(struct ieee80211_bpf_params));
	return (0);
}

void
ieee80211_process_callback(struct ieee80211_node *ni,
	struct mbuf *m, int status)
{
	struct m_tag *mtag;

	mtag = m_tag_locate(m, MTAG_ABI_NET80211, NET80211_TAG_CALLBACK, NULL);
	if (mtag != NULL) {
		struct ieee80211_cb *cb = (struct ieee80211_cb *)(mtag+1);
		cb->func(ni, cb->arg, status);
	}
}

/*
 * Add RX parameters to the given mbuf.
 *
 * Returns 1 if OK, 0 on error.
 */
int
ieee80211_add_rx_params(struct mbuf *m, const struct ieee80211_rx_stats *rxs)
{
	struct m_tag *mtag;
	struct ieee80211_rx_params *rx;

	mtag = m_tag_alloc(MTAG_ABI_NET80211, NET80211_TAG_RECV_PARAMS,
	    sizeof(struct ieee80211_rx_stats), IEEE80211_M_NOWAIT);
	if (mtag == NULL)
		return (0);

	rx = (struct ieee80211_rx_params *)(mtag + 1);
	memcpy(&rx->params, rxs, sizeof(*rxs));
	m_tag_prepend(m, mtag);
	return (1);
}

int
ieee80211_get_rx_params(struct mbuf *m, struct ieee80211_rx_stats *rxs)
{
	struct m_tag *mtag;
	struct ieee80211_rx_params *rx;

	mtag = m_tag_locate(m, MTAG_ABI_NET80211, NET80211_TAG_RECV_PARAMS,
	    NULL);
	if (mtag == NULL)
		return (-1);
	rx = (struct ieee80211_rx_params *)(mtag + 1);
	memcpy(rxs, &rx->params, sizeof(*rxs));
	return (0);
}

const struct ieee80211_rx_stats *
ieee80211_get_rx_params_ptr(struct mbuf *m)
{
	struct m_tag *mtag;
	struct ieee80211_rx_params *rx;

	mtag = m_tag_locate(m, MTAG_ABI_NET80211, NET80211_TAG_RECV_PARAMS,
	    NULL);
	if (mtag == NULL)
		return (NULL);
	rx = (struct ieee80211_rx_params *)(mtag + 1);
	return (&rx->params);
}

/*
 * Add TOA parameters to the given mbuf.
 */
int
ieee80211_add_toa_params(struct mbuf *m, const struct ieee80211_toa_params *p)
{
	struct m_tag *mtag;
	struct ieee80211_toa_params *rp;

	mtag = m_tag_alloc(MTAG_ABI_NET80211, NET80211_TAG_TOA_PARAMS,
	    sizeof(struct ieee80211_toa_params), IEEE80211_M_NOWAIT);
	if (mtag == NULL)
		return (0);

	rp = (struct ieee80211_toa_params *)(mtag + 1);
	memcpy(rp, p, sizeof(*rp));
	m_tag_prepend(m, mtag);
	return (1);
}

int
ieee80211_get_toa_params(struct mbuf *m, struct ieee80211_toa_params *p)
{
	struct m_tag *mtag;
	struct ieee80211_toa_params *rp;

	mtag = m_tag_locate(m, MTAG_ABI_NET80211, NET80211_TAG_TOA_PARAMS,
	    NULL);
	if (mtag == NULL)
		return (0);
	rp = (struct ieee80211_toa_params *)(mtag + 1);
	if (p != NULL)
		memcpy(p, rp, sizeof(*p));
	return (1);
}

/*
 * @brief Transmit a frame to the parent interface.
 *
 * Transmit an 802.11 or 802.3 frame to the parent interface.
 *
 * This is called as part of 802.11 processing to enqueue a frame
 * from net80211 into the device for transmit.
 *
 * If the interface is marked as 802.3 via IEEE80211_C_8023ENCAP
 * (ie, doing offload), then an 802.3 frame will be sent and the
 * driver will need to understand what to do.
 *
 * If the interface is marked as 802.11 (ie, no offload), then
 * an encapsulated 802.11 frame will be queued.  In the case
 * of an 802.11 fragmented frame this will be a list of frames
 * representing the fragments making up the 802.11 frame, linked
 * via m_nextpkt.
 *
 * A fragmented frame list will consist of:
 * + only the first frame with M_SEQNO_SET() assigned the sequence number;
 * + only the first frame with the node reference and node in rcvif;
 * + all frames will have the sequence + fragment number populated in
 *   the 802.11 header.
 *
 * The driver must ensure it doesn't try releasing a node reference
 * for each fragment in the list.
 *
 * The provided mbuf/list is consumed both upon success and error.
 *
 * @param ic	struct ieee80211com device to enqueue frame to
 * @param m	struct mbuf chain / packet list to enqueue
 * @returns	0 if successful, errno if error.
 */
int
ieee80211_parent_xmitpkt(struct ieee80211com *ic, struct mbuf *m)
{
	int error;

	/*
	 * Assert the IC TX lock is held - this enforces the
	 * processing -> queuing order is maintained
	 */
	IEEE80211_TX_LOCK_ASSERT(ic);
	error = ic->ic_transmit(ic, m);
	if (error) {
		struct ieee80211_node *ni;

		ni = (struct ieee80211_node *)m->m_pkthdr.rcvif;

		/* XXX number of fragments */
		if_inc_counter(ni->ni_vap->iv_ifp, IFCOUNTER_OERRORS, 1);

		/* Note: there's only one node reference for a fragment list */
		ieee80211_free_node(ni);
		ieee80211_free_mbuf(m);
	}
	return (error);
}

/*
 * @brief Transmit an 802.3 frame to the VAP interface.
 *
 * This is the entry point for the wifi stack to enqueue 802.3
 * encapsulated frames for transmit to the given vap/ifnet instance.
 * This is used in paths where 802.3 frames have been received
 * or queued, and need to be pushed through the VAP encapsulation
 * and transmit processing pipeline.
 *
 * The provided mbuf/list is consumed both upon success and error.
 *
 * @param vap	struct ieee80211vap instance to transmit frame to
 * @param m	mbuf to transmit
 * @returns	0 if OK, errno if error
 */
int
ieee80211_vap_xmitpkt(struct ieee80211vap *vap, struct mbuf *m)
{
	struct ifnet *ifp = vap->iv_ifp;

	/*
	 * When transmitting via the VAP, we shouldn't hold
	 * any IC TX lock as the VAP TX path will acquire it.
	 */
	IEEE80211_TX_UNLOCK_ASSERT(vap->iv_ic);

	return (ifp->if_transmit(ifp, m));

}

#include <sys/libkern.h>

void
net80211_get_random_bytes(void *p, size_t n)
{
	uint8_t *dp = p;

	while (n > 0) {
		uint32_t v = arc4random();
		size_t nb = n > sizeof(uint32_t) ? sizeof(uint32_t) : n;
		bcopy(&v, dp, n > sizeof(uint32_t) ? sizeof(uint32_t) : n);
		dp += sizeof(uint32_t), n -= nb;
	}
}

/*
 * Helper function for events that pass just a single mac address.
 */
static void
notify_macaddr(struct ifnet *ifp, int op, const uint8_t mac[IEEE80211_ADDR_LEN])
{
	struct ieee80211_join_event iev;

	CURVNET_SET(ifp->if_vnet);
	memset(&iev, 0, sizeof(iev));
	IEEE80211_ADDR_COPY(iev.iev_addr, mac);
	rt_ieee80211msg(ifp, op, &iev, sizeof(iev));
	CURVNET_RESTORE();
}

void
ieee80211_notify_node_join(struct ieee80211_node *ni, int newassoc)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ifnet *ifp = vap->iv_ifp;

	CURVNET_SET_QUIET(ifp->if_vnet);
	IEEE80211_NOTE(vap, IEEE80211_MSG_NODE, ni, "%snode join",
	    (ni == vap->iv_bss) ? "bss " : "");

	if (ni == vap->iv_bss) {
		notify_macaddr(ifp, newassoc ?
		    RTM_IEEE80211_ASSOC : RTM_IEEE80211_REASSOC, ni->ni_bssid);
		if_link_state_change(ifp, LINK_STATE_UP);
	} else {
		notify_macaddr(ifp, newassoc ?
		    RTM_IEEE80211_JOIN : RTM_IEEE80211_REJOIN, ni->ni_macaddr);
	}
	CURVNET_RESTORE();
}

void
ieee80211_notify_node_leave(struct ieee80211_node *ni)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ifnet *ifp = vap->iv_ifp;

	CURVNET_SET_QUIET(ifp->if_vnet);
	IEEE80211_NOTE(vap, IEEE80211_MSG_NODE, ni, "%snode leave",
	    (ni == vap->iv_bss) ? "bss " : "");

	if (ni == vap->iv_bss) {
		rt_ieee80211msg(ifp, RTM_IEEE80211_DISASSOC, NULL, 0);
		if_link_state_change(ifp, LINK_STATE_DOWN);
	} else {
		/* fire off wireless event station leaving */
		notify_macaddr(ifp, RTM_IEEE80211_LEAVE, ni->ni_macaddr);
	}
	CURVNET_RESTORE();
}

void
ieee80211_notify_scan_done(struct ieee80211vap *vap)
{
	struct ifnet *ifp = vap->iv_ifp;

	IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN, "%s\n", "notify scan done");

	/* dispatch wireless event indicating scan completed */
	CURVNET_SET(ifp->if_vnet);
	rt_ieee80211msg(ifp, RTM_IEEE80211_SCAN, NULL, 0);
	CURVNET_RESTORE();
}

void
ieee80211_notify_replay_failure(struct ieee80211vap *vap,
	const struct ieee80211_frame *wh, const struct ieee80211_key *k,
	u_int64_t rsc, int tid)
{
	struct ifnet *ifp = vap->iv_ifp;

	IEEE80211_NOTE_MAC(vap, IEEE80211_MSG_CRYPTO, wh->i_addr2,
	    "%s replay detected tid %d <rsc %ju (%jx), csc %ju (%jx), keyix %u rxkeyix %u>",
	    k->wk_cipher->ic_name, tid,
	    (intmax_t) rsc,
	    (intmax_t) rsc,
	    (intmax_t) k->wk_keyrsc[tid],
	    (intmax_t) k->wk_keyrsc[tid],
	    k->wk_keyix, k->wk_rxkeyix);

	if (ifp != NULL) {		/* NB: for cipher test modules */
		struct ieee80211_replay_event iev;

		IEEE80211_ADDR_COPY(iev.iev_dst, wh->i_addr1);
		IEEE80211_ADDR_COPY(iev.iev_src, wh->i_addr2);
		iev.iev_cipher = k->wk_cipher->ic_cipher;
		if (k->wk_rxkeyix != IEEE80211_KEYIX_NONE)
			iev.iev_keyix = k->wk_rxkeyix;
		else
			iev.iev_keyix = k->wk_keyix;
		iev.iev_keyrsc = k->wk_keyrsc[tid];
		iev.iev_rsc = rsc;
		CURVNET_SET(ifp->if_vnet);
		rt_ieee80211msg(ifp, RTM_IEEE80211_REPLAY, &iev, sizeof(iev));
		CURVNET_RESTORE();
	}
}

void
ieee80211_notify_michael_failure(struct ieee80211vap *vap,
	const struct ieee80211_frame *wh, ieee80211_keyix keyix)
{
	struct ifnet *ifp = vap->iv_ifp;

	IEEE80211_NOTE_MAC(vap, IEEE80211_MSG_CRYPTO, wh->i_addr2,
	    "michael MIC verification failed <keyix %u>", keyix);
	vap->iv_stats.is_rx_tkipmic++;

	if (ifp != NULL) {		/* NB: for cipher test modules */
		struct ieee80211_michael_event iev;

		IEEE80211_ADDR_COPY(iev.iev_dst, wh->i_addr1);
		IEEE80211_ADDR_COPY(iev.iev_src, wh->i_addr2);
		iev.iev_cipher = IEEE80211_CIPHER_TKIP;
		iev.iev_keyix = keyix;
		CURVNET_SET(ifp->if_vnet);
		rt_ieee80211msg(ifp, RTM_IEEE80211_MICHAEL, &iev, sizeof(iev));
		CURVNET_RESTORE();
	}
}

void
ieee80211_notify_wds_discover(struct ieee80211_node *ni)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ifnet *ifp = vap->iv_ifp;

	notify_macaddr(ifp, RTM_IEEE80211_WDS, ni->ni_macaddr);
}

void
ieee80211_notify_csa(struct ieee80211com *ic,
	const struct ieee80211_channel *c, int mode, int count)
{
	struct ieee80211_csa_event iev;
	struct ieee80211vap *vap;
	struct ifnet *ifp;

	memset(&iev, 0, sizeof(iev));
	iev.iev_flags = c->ic_flags;
	iev.iev_freq = c->ic_freq;
	iev.iev_ieee = c->ic_ieee;
	iev.iev_mode = mode;
	iev.iev_count = count;
	TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
		ifp = vap->iv_ifp;
		CURVNET_SET(ifp->if_vnet);
		rt_ieee80211msg(ifp, RTM_IEEE80211_CSA, &iev, sizeof(iev));
		CURVNET_RESTORE();
	}
}

void
ieee80211_notify_radar(struct ieee80211com *ic,
	const struct ieee80211_channel *c)
{
	struct ieee80211_radar_event iev;
	struct ieee80211vap *vap;
	struct ifnet *ifp;

	memset(&iev, 0, sizeof(iev));
	iev.iev_flags = c->ic_flags;
	iev.iev_freq = c->ic_freq;
	iev.iev_ieee = c->ic_ieee;
	TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
		ifp = vap->iv_ifp;
		CURVNET_SET(ifp->if_vnet);
		rt_ieee80211msg(ifp, RTM_IEEE80211_RADAR, &iev, sizeof(iev));
		CURVNET_RESTORE();
	}
}

void
ieee80211_notify_cac(struct ieee80211com *ic,
	const struct ieee80211_channel *c, enum ieee80211_notify_cac_event type)
{
	struct ieee80211_cac_event iev;
	struct ieee80211vap *vap;
	struct ifnet *ifp;

	memset(&iev, 0, sizeof(iev));
	iev.iev_flags = c->ic_flags;
	iev.iev_freq = c->ic_freq;
	iev.iev_ieee = c->ic_ieee;
	iev.iev_type = type;
	TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
		ifp = vap->iv_ifp;
		CURVNET_SET(ifp->if_vnet);
		rt_ieee80211msg(ifp, RTM_IEEE80211_CAC, &iev, sizeof(iev));
		CURVNET_RESTORE();
	}
}

void
ieee80211_notify_node_deauth(struct ieee80211_node *ni)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ifnet *ifp = vap->iv_ifp;

	IEEE80211_NOTE(vap, IEEE80211_MSG_NODE, ni, "%s", "node deauth");

	notify_macaddr(ifp, RTM_IEEE80211_DEAUTH, ni->ni_macaddr);
}

void
ieee80211_notify_node_auth(struct ieee80211_node *ni)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ifnet *ifp = vap->iv_ifp;

	IEEE80211_NOTE(vap, IEEE80211_MSG_NODE, ni, "%s", "node auth");

	notify_macaddr(ifp, RTM_IEEE80211_AUTH, ni->ni_macaddr);
}

void
ieee80211_notify_country(struct ieee80211vap *vap,
	const uint8_t bssid[IEEE80211_ADDR_LEN], const uint8_t cc[2])
{
	struct ifnet *ifp = vap->iv_ifp;
	struct ieee80211_country_event iev;

	memset(&iev, 0, sizeof(iev));
	IEEE80211_ADDR_COPY(iev.iev_addr, bssid);
	iev.iev_cc[0] = cc[0];
	iev.iev_cc[1] = cc[1];
	CURVNET_SET(ifp->if_vnet);
	rt_ieee80211msg(ifp, RTM_IEEE80211_COUNTRY, &iev, sizeof(iev));
	CURVNET_RESTORE();
}

void
ieee80211_notify_radio(struct ieee80211com *ic, int state)
{
	struct ieee80211_radio_event iev;
	struct ieee80211vap *vap;
	struct ifnet *ifp;

	memset(&iev, 0, sizeof(iev));
	iev.iev_state = state;
	TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
		ifp = vap->iv_ifp;
		CURVNET_SET(ifp->if_vnet);
		rt_ieee80211msg(ifp, RTM_IEEE80211_RADIO, &iev, sizeof(iev));
		CURVNET_RESTORE();
	}
}

void
ieee80211_notify_ifnet_change(struct ieee80211vap *vap, int if_flags_mask)
{
	struct ifnet *ifp = vap->iv_ifp;

	IEEE80211_DPRINTF(vap, IEEE80211_MSG_DEBUG, "%s\n",
	    "interface state change");

	CURVNET_SET(ifp->if_vnet);
	rt_ifmsg(ifp, if_flags_mask);
	CURVNET_RESTORE();
}

void
ieee80211_load_module(const char *modname)
{

#ifdef notyet
	(void)kern_kldload(curthread, modname, NULL);
#else
	printf("%s: load the %s module by hand for now.\n", __func__, modname);
#endif
}

static eventhandler_tag wlan_bpfevent;
static eventhandler_tag wlan_ifllevent;

static void
bpf_track(void *arg, struct ifnet *ifp, int dlt, int attach)
{
	/* NB: identify vap's by if_init */
	if (dlt == DLT_IEEE802_11_RADIO &&
	    ifp->if_init == ieee80211_init) {
		struct ieee80211vap *vap = ifp->if_softc;
		/*
		 * Track bpf radiotap listener state.  We mark the vap
		 * to indicate if any listener is present and the com
		 * to indicate if any listener exists on any associated
		 * vap.  This flag is used by drivers to prepare radiotap
		 * state only when needed.
		 */
		if (attach) {
			ieee80211_syncflag_ext(vap, IEEE80211_FEXT_BPF);
			if (vap->iv_opmode == IEEE80211_M_MONITOR)
				atomic_add_int(&vap->iv_ic->ic_montaps, 1);
		} else if (!bpf_peers_present(vap->iv_rawbpf)) {
			ieee80211_syncflag_ext(vap, -IEEE80211_FEXT_BPF);
			if (vap->iv_opmode == IEEE80211_M_MONITOR)
				atomic_subtract_int(&vap->iv_ic->ic_montaps, 1);
		}
	}
}

/*
 * Change MAC address on the vap (if was not started).
 */
static void
wlan_iflladdr(void *arg __unused, struct ifnet *ifp)
{
	/* NB: identify vap's by if_init */
	if (ifp->if_init == ieee80211_init &&
	    (ifp->if_flags & IFF_UP) == 0) {
		struct ieee80211vap *vap = ifp->if_softc;

		IEEE80211_ADDR_COPY(vap->iv_myaddr, IF_LLADDR(ifp));
	}
}

/*
 * Fetch the VAP name.
 *
 * This returns a const char pointer suitable for debugging,
 * but don't expect it to stick around for much longer.
 */
const char *
ieee80211_get_vap_ifname(struct ieee80211vap *vap)
{
	if (vap->iv_ifp == NULL)
		return "(none)";
	return (if_name(vap->iv_ifp));
}

#ifdef DEBUGNET
static void
ieee80211_debugnet_init(struct ifnet *ifp, int *nrxr, int *ncl, int *clsize)
{
	struct ieee80211vap *vap;
	struct ieee80211com *ic;

	vap = if_getsoftc(ifp);
	ic = vap->iv_ic;

	IEEE80211_LOCK(ic);
	ic->ic_debugnet_meth->dn8_init(ic, nrxr, ncl, clsize);
	IEEE80211_UNLOCK(ic);
}

static void
ieee80211_debugnet_event(struct ifnet *ifp, enum debugnet_ev ev)
{
	struct ieee80211vap *vap;
	struct ieee80211com *ic;

	vap = if_getsoftc(ifp);
	ic = vap->iv_ic;

	IEEE80211_LOCK(ic);
	ic->ic_debugnet_meth->dn8_event(ic, ev);
	IEEE80211_UNLOCK(ic);
}

static int
ieee80211_debugnet_transmit(struct ifnet *ifp, struct mbuf *m)
{
	return (ieee80211_vap_transmit(ifp, m));
}

static int
ieee80211_debugnet_poll(struct ifnet *ifp, int count)
{
	struct ieee80211vap *vap;
	struct ieee80211com *ic;

	vap = if_getsoftc(ifp);
	ic = vap->iv_ic;

	return (ic->ic_debugnet_meth->dn8_poll(ic, count));
}
#endif

/**
 * @brief Check if the MAC address was changed by the upper layer.
 *
 * This is specifically to handle cases like the MAC address
 * being changed via an ioctl (eg SIOCSIFLLADDR).
 *
 * @param vap	VAP to sync MAC address for
 */
void
ieee80211_vap_sync_mac_address(struct ieee80211vap *vap)
{
	struct epoch_tracker et;
	const struct ifnet *ifp = vap->iv_ifp;

	/*
	 * Check if the MAC address was changed
	 * via SIOCSIFLLADDR ioctl.
	 *
	 * NB: device may be detached during initialization;
	 * use if_ioctl for existence check.
	 */
	NET_EPOCH_ENTER(et);
	if (ifp->if_ioctl == ieee80211_ioctl &&
	    (ifp->if_flags & IFF_UP) == 0 &&
	    !IEEE80211_ADDR_EQ(vap->iv_myaddr, IF_LLADDR(ifp)))
		IEEE80211_ADDR_COPY(vap->iv_myaddr, IF_LLADDR(ifp));
	NET_EPOCH_EXIT(et);
}

/**
 * @brief Initial MAC address setup for a VAP.
 *
 * @param vap	VAP to sync MAC address for
 */
void
ieee80211_vap_copy_mac_address(struct ieee80211vap *vap)
{
	struct epoch_tracker et;

	NET_EPOCH_ENTER(et);
	IEEE80211_ADDR_COPY(vap->iv_myaddr, IF_LLADDR(vap->iv_ifp));
	NET_EPOCH_EXIT(et);
}

/**
 * @brief Deliver data into the upper ifp of the VAP interface
 *
 * This delivers an 802.3 frame from net80211 up to the operating
 * system network interface layer.
 *
 * @param vap	the current VAP
 * @param m	the 802.3 frame to pass up to the VAP interface
 *
 * Note: this API consumes the mbuf.
 */
void
ieee80211_vap_deliver_data(struct ieee80211vap *vap, struct mbuf *m)
{
	struct epoch_tracker et;

	NET_EPOCH_ENTER(et);
	if_input(vap->iv_ifp, m);
	NET_EPOCH_EXIT(et);
}

/**
 * @brief Return whether the VAP is configured with monitor mode
 *
 * This checks the operating system layer for whether monitor mode
 * is enabled.
 *
 * @param vap	the current VAP
 * @retval true if the underlying interface is in MONITOR mode, false otherwise
 */
bool
ieee80211_vap_ifp_check_is_monitor(struct ieee80211vap *vap)
{
	return ((if_getflags(vap->iv_ifp) & IFF_MONITOR) != 0);
}

/**
 * @brief Return whether the VAP is configured in simplex mode.
 *
 * This checks the operating system layer for whether simplex mode
 * is enabled.
 *
 * @param vap	the current VAP
 * @retval true if the underlying interface is in SIMPLEX mode, false otherwise
 */
bool
ieee80211_vap_ifp_check_is_simplex(struct ieee80211vap *vap)
{
	return ((if_getflags(vap->iv_ifp) & IFF_SIMPLEX) != 0);
}

/**
 * @brief Return if the VAP underlying network interface is running
 *
 * @param vap	the current VAP
 * @retval true if the underlying interface is running; false otherwise
 */
bool
ieee80211_vap_ifp_check_is_running(struct ieee80211vap *vap)
{
	return ((if_getdrvflags(vap->iv_ifp) & IFF_DRV_RUNNING) != 0);
}

/**
 * @brief Change the VAP underlying network interface state
 *
 * @param vap	the current VAP
 * @param state	true to mark the interface as RUNNING, false to clear
 */
void
ieee80211_vap_ifp_set_running_state(struct ieee80211vap *vap, bool state)
{
	if (state)
		if_setdrvflagbits(vap->iv_ifp, IFF_DRV_RUNNING, 0);
	else
		if_setdrvflagbits(vap->iv_ifp, 0, IFF_DRV_RUNNING);
}

/**
 * @brief Return the broadcast MAC address.
 *
 * @param vap	The current VAP
 * @retval a uint8_t array representing the ethernet broadcast address
 */
const uint8_t *
ieee80211_vap_get_broadcast_address(struct ieee80211vap *vap)
{
	return (if_getbroadcastaddr(vap->iv_ifp));
}

/**
 * @brief net80211 printf() (not vap/ic related)
 */
void
net80211_printf(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
}

/**
 * @brief VAP specific printf()
 */
void
net80211_vap_printf(const struct ieee80211vap *vap, const char *fmt, ...)
{
	char if_fmt[256];
	va_list ap;

	va_start(ap, fmt);
	snprintf(if_fmt, sizeof(if_fmt), "%s: %s", if_name(vap->iv_ifp), fmt);
	vlog(LOG_INFO, if_fmt, ap);
	va_end(ap);
}

/**
 * @brief ic specific printf()
 */
void
net80211_ic_printf(const struct ieee80211com *ic, const char *fmt, ...)
{
	va_list ap;

	/*
	 * TODO: do the vap_printf stuff above, use vlog(LOG_INFO, ...)
	 */
	printf("%s: ", ic->ic_name);
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
}

/*
 * Module glue.
 *
 * NB: the module name is "wlan" for compatibility with NetBSD.
 */
static int
wlan_modevent(module_t mod, int type, void *unused)
{
	switch (type) {
	case MOD_LOAD:
		if (bootverbose)
			printf("wlan: <802.11 Link Layer>\n");
		wlan_bpfevent = EVENTHANDLER_REGISTER(bpf_track,
		    bpf_track, 0, EVENTHANDLER_PRI_ANY);
		wlan_ifllevent = EVENTHANDLER_REGISTER(iflladdr_event,
		    wlan_iflladdr, NULL, EVENTHANDLER_PRI_ANY);
		struct if_clone_addreq req = {
			.create_f = wlan_clone_create,
			.destroy_f = wlan_clone_destroy,
			.flags = IFC_F_AUTOUNIT,
		};
		wlan_cloner = ifc_attach_cloner(wlanname, &req);
		return 0;
	case MOD_UNLOAD:
		ifc_detach_cloner(wlan_cloner);
		EVENTHANDLER_DEREGISTER(bpf_track, wlan_bpfevent);
		EVENTHANDLER_DEREGISTER(iflladdr_event, wlan_ifllevent);
		return 0;
	}
	return EINVAL;
}

static moduledata_t wlan_mod = {
	wlanname,
	wlan_modevent,
	0
};
DECLARE_MODULE(wlan, wlan_mod, SI_SUB_DRIVERS, SI_ORDER_FIRST);
MODULE_VERSION(wlan, 1);
MODULE_DEPEND(wlan, ether, 1, 1, 1);
#ifdef	IEEE80211_ALQ
MODULE_DEPEND(wlan, alq, 1, 1, 1);
#endif	/* IEEE80211_ALQ */
