/*-
 * Copyright (c) 2003-2007 Sam Leffler, Errno Consulting
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
__FBSDID("$FreeBSD$");

/*
 * IEEE 802.11 support (FreeBSD-specific code)
 */
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h> 
#include <sys/linker.h>
#include <sys/mbuf.h>   
#include <sys/module.h>
#include <sys/proc.h>
#include <sys/sysctl.h>

#include <sys/socket.h>

#include <net/if.h>
#include <net/if_media.h>
#include <net/ethernet.h>
#include <net/route.h>

#include <net80211/ieee80211_var.h>

SYSCTL_NODE(_net, OID_AUTO, wlan, CTLFLAG_RD, 0, "IEEE 80211 parameters");

#ifdef IEEE80211_DEBUG
int	ieee80211_debug = 0;
SYSCTL_INT(_net_wlan, OID_AUTO, debug, CTLFLAG_RW, &ieee80211_debug,
	    0, "debugging printfs");
#endif

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
	const char *name = ic->ic_ifp->if_xname;

	return SYSCTL_OUT(req, name, strlen(name));
}

void
ieee80211_sysctl_attach(struct ieee80211com *ic)
{
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid *oid;
	char num[14];			/* sufficient for 32 bits */

	MALLOC(ctx, struct sysctl_ctx_list *, sizeof(struct sysctl_ctx_list),
		M_DEVBUF, M_NOWAIT | M_ZERO);
	if (ctx == NULL) {
		if_printf(ic->ic_ifp, "%s: cannot allocate sysctl context!\n",
			__func__);
		return;
	}
	sysctl_ctx_init(ctx);
	snprintf(num, sizeof(num), "%u", ic->ic_vap);
	oid = SYSCTL_ADD_NODE(ctx, &SYSCTL_NODE_CHILDREN(_net, wlan),
		OID_AUTO, num, CTLFLAG_RD, NULL, "");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(oid), OID_AUTO,
		"%parent", CTLFLAG_RD, ic, 0, ieee80211_sysctl_parent, "A",
		"parent device");
#ifdef IEEE80211_DEBUG
	ic->ic_debug = ieee80211_debug;
	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(oid), OID_AUTO,
		"debug", CTLFLAG_RW, &ic->ic_debug, 0,
		"control debugging printfs");
#endif
	/* XXX inherit from tunables */
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(oid), OID_AUTO,
		"inact_run", CTLTYPE_INT | CTLFLAG_RW, &ic->ic_inact_run, 0,
		ieee80211_sysctl_inact, "I",
		"station inactivity timeout (sec)");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(oid), OID_AUTO,
		"inact_probe", CTLTYPE_INT | CTLFLAG_RW, &ic->ic_inact_probe, 0,
		ieee80211_sysctl_inact, "I",
		"station inactivity probe timeout (sec)");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(oid), OID_AUTO,
		"inact_auth", CTLTYPE_INT | CTLFLAG_RW, &ic->ic_inact_auth, 0,
		ieee80211_sysctl_inact, "I",
		"station authentication timeout (sec)");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(oid), OID_AUTO,
		"inact_init", CTLTYPE_INT | CTLFLAG_RW, &ic->ic_inact_init, 0,
		ieee80211_sysctl_inact, "I",
		"station initial state timeout (sec)");
	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(oid), OID_AUTO,
		"driver_caps", CTLFLAG_RW, &ic->ic_caps, 0,
		"driver capabilities");
	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(oid), OID_AUTO,
		"bmiss_max", CTLFLAG_RW, &ic->ic_bmiss_max, 0,
		"consecutive beacon misses before scanning");
	ic->ic_sysctl = ctx;
}

void
ieee80211_sysctl_detach(struct ieee80211com *ic)
{

	if (ic->ic_sysctl != NULL) {
		sysctl_ctx_free(ic->ic_sysctl);
		FREE(ic->ic_sysctl, M_DEVBUF);
		ic->ic_sysctl = NULL;
	}
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

/*
 * As above, for mbufs allocated with m_gethdr/MGETHDR
 * or initialized by M_COPY_PKTHDR.
 */
#define	MC_ALIGN(m, len)						\
do {									\
	(m)->m_data += (MCLBYTES - (len)) &~ (sizeof(long) - 1);	\
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
		m = m_gethdr(M_NOWAIT, MT_DATA);
		/*
		 * Align the data in case additional headers are added.
		 * This should only happen when a WEP header is added
		 * which only happens for shared key authentication mgt
		 * frames which all fit in MHLEN.
		 */
		if (m != NULL)
			MH_ALIGN(m, len);
	} else {
		m = m_getcl(M_NOWAIT, MT_DATA, M_PKTHDR);
		if (m != NULL)
			MC_ALIGN(m, len);
	}
	if (m != NULL) {
		m->m_data += headroom;
		*frm = m->m_data;
	}
	return m;
}

int
ieee80211_add_callback(struct mbuf *m,
	void (*func)(struct ieee80211_node *, void *, int), void *arg)
{
	struct m_tag *mtag;
	struct ieee80211_cb *cb;

	mtag = m_tag_alloc(MTAG_ABI_NET80211, NET80211_TAG_CALLBACK,
			sizeof(struct ieee80211_cb), M_NOWAIT);
	if (mtag == NULL)
		return 0;

	cb = (struct ieee80211_cb *)(mtag+1);
	cb->func = func;
	cb->arg = arg;
	m_tag_prepend(m, mtag);
	m->m_flags |= M_TXCB;
	return 1;
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

#include <sys/libkern.h>

void
get_random_bytes(void *p, size_t n)
{
	uint8_t *dp = p;

	while (n > 0) {
		uint32_t v = arc4random();
		size_t nb = n > sizeof(uint32_t) ? sizeof(uint32_t) : n;
		bcopy(&v, dp, n > sizeof(uint32_t) ? sizeof(uint32_t) : n);
		dp += sizeof(uint32_t), n -= nb;
	}
}

void
ieee80211_notify_node_join(struct ieee80211com *ic, struct ieee80211_node *ni, int newassoc)
{
	struct ifnet *ifp = ic->ic_ifp;
	struct ieee80211_join_event iev;

	memset(&iev, 0, sizeof(iev));
	if (ni == ic->ic_bss) {
		IEEE80211_ADDR_COPY(iev.iev_addr, ni->ni_bssid);
		rt_ieee80211msg(ifp, newassoc ?
			RTM_IEEE80211_ASSOC : RTM_IEEE80211_REASSOC,
			&iev, sizeof(iev));
		if_link_state_change(ifp, LINK_STATE_UP);
	} else {
		IEEE80211_ADDR_COPY(iev.iev_addr, ni->ni_macaddr);
		rt_ieee80211msg(ifp, newassoc ?
			RTM_IEEE80211_JOIN : RTM_IEEE80211_REJOIN,
			&iev, sizeof(iev));
	}
}

void
ieee80211_notify_node_leave(struct ieee80211com *ic, struct ieee80211_node *ni)
{
	struct ifnet *ifp = ic->ic_ifp;
	struct ieee80211_leave_event iev;

	if (ni == ic->ic_bss) {
		rt_ieee80211msg(ifp, RTM_IEEE80211_DISASSOC, NULL, 0);
		if_link_state_change(ifp, LINK_STATE_DOWN);
	} else {
		/* fire off wireless event station leaving */
		memset(&iev, 0, sizeof(iev));
		IEEE80211_ADDR_COPY(iev.iev_addr, ni->ni_macaddr);
		rt_ieee80211msg(ifp, RTM_IEEE80211_LEAVE, &iev, sizeof(iev));
	}
}

void
ieee80211_notify_scan_done(struct ieee80211com *ic)
{
	struct ifnet *ifp = ic->ic_ifp;

	IEEE80211_DPRINTF(ic, IEEE80211_MSG_SCAN, "%s\n", "notify scan done");

	/* dispatch wireless event indicating scan completed */
	rt_ieee80211msg(ifp, RTM_IEEE80211_SCAN, NULL, 0);
}

void
ieee80211_notify_replay_failure(struct ieee80211com *ic,
	const struct ieee80211_frame *wh, const struct ieee80211_key *k,
	u_int64_t rsc)
{
	struct ifnet *ifp = ic->ic_ifp;

	IEEE80211_DPRINTF(ic, IEEE80211_MSG_CRYPTO,
	    "[%s] %s replay detected <rsc %ju, csc %ju, keyix %u rxkeyix %u>\n",
	    ether_sprintf(wh->i_addr2), k->wk_cipher->ic_name,
	    (intmax_t) rsc, (intmax_t) k->wk_keyrsc,
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
		iev.iev_keyrsc = k->wk_keyrsc;
		iev.iev_rsc = rsc;
		rt_ieee80211msg(ifp, RTM_IEEE80211_REPLAY, &iev, sizeof(iev));
	}
}

void
ieee80211_notify_michael_failure(struct ieee80211com *ic,
	const struct ieee80211_frame *wh, u_int keyix)
{
	struct ifnet *ifp = ic->ic_ifp;

	IEEE80211_DPRINTF(ic, IEEE80211_MSG_CRYPTO,
		"[%s] michael MIC verification failed <keyix %u>\n",
	       ether_sprintf(wh->i_addr2), keyix);
	ic->ic_stats.is_rx_tkipmic++;

	if (ifp != NULL) {		/* NB: for cipher test modules */
		struct ieee80211_michael_event iev;

		IEEE80211_ADDR_COPY(iev.iev_dst, wh->i_addr1);
		IEEE80211_ADDR_COPY(iev.iev_src, wh->i_addr2);
		iev.iev_cipher = IEEE80211_CIPHER_TKIP;
		iev.iev_keyix = keyix;
		rt_ieee80211msg(ifp, RTM_IEEE80211_MICHAEL, &iev, sizeof(iev));
	}
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
		return 0;
	case MOD_UNLOAD:
		return 0;
	}
	return EINVAL;
}

static moduledata_t wlan_mod = {
	"wlan",
	wlan_modevent,
	0
};
DECLARE_MODULE(wlan, wlan_mod, SI_SUB_DRIVERS, SI_ORDER_FIRST);
MODULE_VERSION(wlan, 1);
MODULE_DEPEND(wlan, ether, 1, 1, 1);
