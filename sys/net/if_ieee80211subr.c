/*	$NetBSD: if_ieee80211subr.c,v 1.22 2002/10/16 11:29:30 onoe Exp $	*/
/*	$FreeBSD$	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Atsushi Onoe.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * IEEE 802.11 generic handler
 */

#include <sys/cdefs.h>

#include "opt_inet.h"
#define	NBPFILTER	1

#include <sys/param.h>
#include <sys/systm.h> 
#include <sys/mbuf.h>   
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/endian.h>
#include <sys/errno.h>
#include <sys/bus.h>
#include <sys/proc.h>
#include <sys/sysctl.h>

#include <crypto/rc4/rc4.h>
#define	arc4_ctxlen()			sizeof (struct rc4_state)
#define	arc4_setkey(_c,_k,_l)		rc4_init(_c,_k,_l)
#define	arc4_encrypt(_c,_d,_s,_l)	rc4_crypt(_c,_s,_d,_l)
 
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/ethernet.h>
#include <net/if_llc.h>
#include <net/if_ieee80211.h>

#if NBPFILTER > 0 
#include <net/bpf.h>
#endif 

#ifdef INET
#include <netinet/in.h> 
#include <netinet/if_ether.h>
#endif

#include <dev/wi/if_wavelan_ieee.h>

#define	IEEE80211_DEBUG
#ifdef IEEE80211_DEBUG
int ieee80211_debug = 0;
#define	DPRINTF(X)	if (ieee80211_debug) printf X
#define	DPRINTF2(X)	if (ieee80211_debug>1) printf X

SYSCTL_INT(_debug, OID_AUTO, ieee80211, CTLFLAG_RW, &ieee80211_debug,
	    0, "IEEE 802.11 media debugging printfs");
#else
#define	DPRINTF(X)
#define	DPRINTF2(X)
#endif

/* XXX belongs elsewhere */
#ifndef ALIGNED_POINTER
/*
 * ALIGNED_POINTER is a boolean macro that checks whether an address
 * is valid to fetch data elements of type t from on this architecture.
 * This does not reflect the optimal alignment, just the possibility
 * (within reasonable limits). 
 *
 */
#define ALIGNED_POINTER(p,t)	1
#endif

static int ieee80211_send_prreq(struct ieee80211com *,
    struct ieee80211_node *, int, int);
static int ieee80211_send_prresp(struct ieee80211com *,
    struct ieee80211_node *, int, int);
static int ieee80211_send_auth(struct ieee80211com *,
    struct ieee80211_node *, int, int);
static int ieee80211_send_deauth(struct ieee80211com *,
    struct ieee80211_node *, int, int);
static int ieee80211_send_asreq(struct ieee80211com *,
    struct ieee80211_node *, int, int);
static int ieee80211_send_asresp(struct ieee80211com *,
    struct ieee80211_node *, int, int);
static int ieee80211_send_disassoc(struct ieee80211com *,
    struct ieee80211_node *, int, int);

static void ieee80211_recv_beacon(struct ieee80211com *,
    struct mbuf *, int, u_int32_t);
static void ieee80211_recv_prreq(struct ieee80211com *,
    struct mbuf *, int, u_int32_t);
static void ieee80211_recv_auth(struct ieee80211com *,
    struct mbuf *, int, u_int32_t);
static void ieee80211_recv_asreq(struct ieee80211com *,
    struct mbuf *, int, u_int32_t);
static void ieee80211_recv_asresp(struct ieee80211com *,
    struct mbuf *, int, u_int32_t);
static void ieee80211_recv_disassoc(struct ieee80211com *,
    struct mbuf *, int, u_int32_t);
static void ieee80211_recv_deauth(struct ieee80211com *,
    struct mbuf *, int, u_int32_t);

static void ieee80211_crc_init(void);
static u_int32_t ieee80211_crc_update(u_int32_t, u_int8_t *, int);

static const char *ieee80211_mgt_subtype_name[] = {
	"assoc_req",	"assoc_resp",	"reassoc_req",	"reassoc_resp",
	"probe_req",	"probe_resp",	"reserved#6",	"reserved#7",
	"beacon",	"atim",		"disassoc",	"auth",
	"deauth",	"reserved#13",	"reserved#14",	"reserved#15"
};

void
ieee80211_ifattach(struct ifnet *ifp)
{
	struct ieee80211com *ic = (void *)ifp;
	int i, rate;

	/* XXX need unit */
	mtx_init(&ic->ic_mtx, ifp->if_name, "802.11 link layer", MTX_DEF);

	ether_ifattach(ifp, ic->ic_myaddr);
#if NBPFILTER > 0
	bpfattach2(ifp, DLT_IEEE802_11,
	    sizeof(struct ieee80211_frame_addr4), &ic->ic_rawbpf);
#endif
	ieee80211_crc_init();
	ic->ic_iv = arc4random();
	memcpy(ic->ic_chan_active, ic->ic_chan_avail,
	    sizeof(ic->ic_chan_active));
	if (isclr(ic->ic_chan_active, ic->ic_ibss_chan)) {
		for (i = 0; i <= IEEE80211_CHAN_MAX; i++) {
			if (isset(ic->ic_chan_active, i)) {
				ic->ic_ibss_chan = i;
				break;
			}
		}
	}
	ic->ic_des_chan = IEEE80211_CHAN_ANY;
	ic->ic_fixed_rate = -1;
	if (ic->ic_lintval == 0)
		ic->ic_lintval = 100;	/* default sleep */
	TAILQ_INIT(&ic->ic_node);
	mtx_init(&ic->ic_mgtq.ifq_mtx, ifp->if_name, "mgmt send q", MTX_DEF);

	rate = 0;
	for (i = 0; i < IEEE80211_RATE_SIZE; i++) {
		if (ic->ic_sup_rates[i] != 0)
			rate = (ic->ic_sup_rates[i] & IEEE80211_RATE_VAL) / 2;
	}
	if (rate)
		ifp->if_baudrate = IF_Mbps(rate);
	ifp->if_hdrlen = sizeof(struct ieee80211_frame);

	/* initialize management frame handler */
	ic->ic_recv_mgmt[IEEE80211_FC0_SUBTYPE_PROBE_RESP
	    >> IEEE80211_FC0_SUBTYPE_SHIFT] = ieee80211_recv_beacon;
	ic->ic_recv_mgmt[IEEE80211_FC0_SUBTYPE_BEACON
	    >> IEEE80211_FC0_SUBTYPE_SHIFT] = ieee80211_recv_beacon;
	ic->ic_recv_mgmt[IEEE80211_FC0_SUBTYPE_PROBE_REQ
	    >> IEEE80211_FC0_SUBTYPE_SHIFT] = ieee80211_recv_prreq;
	ic->ic_recv_mgmt[IEEE80211_FC0_SUBTYPE_AUTH
	    >> IEEE80211_FC0_SUBTYPE_SHIFT] = ieee80211_recv_auth;
	ic->ic_recv_mgmt[IEEE80211_FC0_SUBTYPE_ASSOC_REQ
	    >> IEEE80211_FC0_SUBTYPE_SHIFT] = ieee80211_recv_asreq;
	ic->ic_recv_mgmt[IEEE80211_FC0_SUBTYPE_REASSOC_REQ
	    >> IEEE80211_FC0_SUBTYPE_SHIFT] = ieee80211_recv_asreq;
	ic->ic_recv_mgmt[IEEE80211_FC0_SUBTYPE_ASSOC_RESP
	    >> IEEE80211_FC0_SUBTYPE_SHIFT] = ieee80211_recv_asresp;
	ic->ic_recv_mgmt[IEEE80211_FC0_SUBTYPE_REASSOC_RESP
	    >> IEEE80211_FC0_SUBTYPE_SHIFT] = ieee80211_recv_asresp;
	ic->ic_recv_mgmt[IEEE80211_FC0_SUBTYPE_DEAUTH
	    >> IEEE80211_FC0_SUBTYPE_SHIFT] = ieee80211_recv_deauth;
	ic->ic_recv_mgmt[IEEE80211_FC0_SUBTYPE_DISASSOC
	    >> IEEE80211_FC0_SUBTYPE_SHIFT] = ieee80211_recv_disassoc;

	ic->ic_send_mgmt[IEEE80211_FC0_SUBTYPE_PROBE_REQ
	    >> IEEE80211_FC0_SUBTYPE_SHIFT] = ieee80211_send_prreq;
	ic->ic_send_mgmt[IEEE80211_FC0_SUBTYPE_PROBE_RESP
	    >> IEEE80211_FC0_SUBTYPE_SHIFT] = ieee80211_send_prresp;
	ic->ic_send_mgmt[IEEE80211_FC0_SUBTYPE_AUTH
	    >> IEEE80211_FC0_SUBTYPE_SHIFT] = ieee80211_send_auth;
	ic->ic_send_mgmt[IEEE80211_FC0_SUBTYPE_DEAUTH
	    >> IEEE80211_FC0_SUBTYPE_SHIFT] = ieee80211_send_deauth;
	ic->ic_send_mgmt[IEEE80211_FC0_SUBTYPE_ASSOC_REQ
	    >> IEEE80211_FC0_SUBTYPE_SHIFT] = ieee80211_send_asreq;
	ic->ic_send_mgmt[IEEE80211_FC0_SUBTYPE_REASSOC_REQ
	    >> IEEE80211_FC0_SUBTYPE_SHIFT] = ieee80211_send_asreq;
	ic->ic_send_mgmt[IEEE80211_FC0_SUBTYPE_ASSOC_RESP
	    >> IEEE80211_FC0_SUBTYPE_SHIFT] = ieee80211_send_asresp;
	ic->ic_send_mgmt[IEEE80211_FC0_SUBTYPE_REASSOC_RESP
	    >> IEEE80211_FC0_SUBTYPE_SHIFT] = ieee80211_send_asresp;
	ic->ic_send_mgmt[IEEE80211_FC0_SUBTYPE_DISASSOC
	    >> IEEE80211_FC0_SUBTYPE_SHIFT] = ieee80211_send_disassoc;
}

void
ieee80211_ifdetach(struct ifnet *ifp)
{
	struct ieee80211com *ic = (void *)ifp;

	IEEE80211_LOCK(ic);
	IF_DRAIN(&ic->ic_mgtq);
	mtx_destroy(&ic->ic_mgtq.ifq_mtx);
	if (ic->ic_wep_ctx != NULL) {
		free(ic->ic_wep_ctx, M_DEVBUF);
		ic->ic_wep_ctx = NULL;
	}
	ieee80211_free_allnodes(ic);
#if NBPFILTER > 0
	bpfdetach(ifp);
#endif
	ether_ifdetach(ifp);
	IEEE80211_UNLOCK(ic);
	mtx_destroy(&ic->ic_mtx);
}

void
ieee80211_input(struct ifnet *ifp, struct mbuf *m, int rssi, u_int32_t rstamp)
{
	struct ieee80211com *ic = (void *)ifp;
	struct ieee80211_node *ni = NULL;
	struct ieee80211_frame *wh;
	struct ether_header *eh;
	void (*rh)(struct ieee80211com *, struct mbuf *, int, u_int);
	struct mbuf *m1;
	int len;
	u_int8_t dir, subtype;
	u_int8_t *bssid;
	u_int16_t rxseq;

	/* trim CRC here for WEP can find its own CRC at the end of packet. */
	if (m->m_flags & M_HASFCS) {
		m_adj(m, -IEEE80211_CRC_LEN);
		m->m_flags &= ~M_HASFCS;
	}

	wh = mtod(m, struct ieee80211_frame *);
	if ((wh->i_fc[0] & IEEE80211_FC0_VERSION_MASK) !=
	    IEEE80211_FC0_VERSION_0) {
		if (ifp->if_flags & IFF_DEBUG)
			if_printf(ifp, "receive packet with wrong version: %x\n",
			    wh->i_fc[0]);
		goto err;
	}

	dir = wh->i_fc[1] & IEEE80211_FC1_DIR_MASK;

	if (ic->ic_state != IEEE80211_S_SCAN) {
		switch (ic->ic_opmode) {
		case IEEE80211_M_STA:
			ni = &ic->ic_bss;
			if (!IEEE80211_ADDR_EQ(wh->i_addr2, ni->ni_bssid)) {
				DPRINTF2(("ieee80211_input: other bss %s\n",
				    ether_sprintf(wh->i_addr2)));
				/* not interested in */
				goto out;
			}
			break;
		case IEEE80211_M_IBSS:
		case IEEE80211_M_AHDEMO:
		case IEEE80211_M_HOSTAP:
			if (dir == IEEE80211_FC1_DIR_NODS)
				bssid = wh->i_addr3;
			else
				bssid = wh->i_addr1;
			if (!IEEE80211_ADDR_EQ(bssid, ic->ic_bss.ni_bssid) &&
			    !IEEE80211_ADDR_EQ(bssid, ifp->if_broadcastaddr)) {
				/* not interested in */
				DPRINTF2(("ieee80211_input: other bss %s\n",
				    ether_sprintf(wh->i_addr3)));
				goto out;
			}
			ni = ieee80211_find_node(ic, wh->i_addr2);
			if (ni == NULL) {
				DPRINTF2(("ieee80211_input: unknown src %s\n",
				    ether_sprintf(wh->i_addr2)));
				ni = &ic->ic_bss;	/* XXX allocate? */
			}
			break;
		}
		ni->ni_rssi = rssi;
		ni->ni_rstamp = rstamp;
		rxseq = ni->ni_rxseq;
		ni->ni_rxseq =
		    le16toh(*(u_int16_t *)wh->i_seq) >> IEEE80211_SEQ_SEQ_SHIFT;
		/* TODO: fragment */
		if ((wh->i_fc[1] & IEEE80211_FC1_RETRY) &&
		    rxseq == ni->ni_rxseq) {
			/* duplicate, silently discarded */
			goto out;
		}
		ni->ni_inact = 0;
	}

	switch (wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) {
	case IEEE80211_FC0_TYPE_DATA:
		switch (ic->ic_opmode) {
		case IEEE80211_M_STA:
			if (dir != IEEE80211_FC1_DIR_FROMDS)
				goto out;
			if ((ifp->if_flags & IFF_SIMPLEX) &&
			    IEEE80211_IS_MULTICAST(wh->i_addr1) &&
			    IEEE80211_ADDR_EQ(wh->i_addr3, ic->ic_myaddr)) {
				/*
				 * In IEEE802.11 network, multicast packet
				 * sent from me is broadcasted from AP.
				 * It should be silently discarded for
				 * SIMPLEX interface.
				 */
				goto out;
			}
			break;
		case IEEE80211_M_IBSS:
		case IEEE80211_M_AHDEMO:
			if (dir != IEEE80211_FC1_DIR_NODS)
				goto out;
			break;
		case IEEE80211_M_HOSTAP:
			if (dir != IEEE80211_FC1_DIR_TODS)
				goto out;
			/* check if source STA is associated */
			ni = ieee80211_find_node(ic, wh->i_addr2);
			if (ni == NULL) {
				DPRINTF(("ieee80211_input: "
				    "data from unknown src %s\n",
				    ether_sprintf(wh->i_addr2)));
				if ((ni = ieee80211_alloc_node(ic, wh->i_addr2,
				    1)) != NULL) {
					IEEE80211_SEND_MGMT(ic, ni,
					    IEEE80211_FC0_SUBTYPE_DEAUTH,
					    IEEE80211_REASON_NOT_AUTHED);
					ieee80211_free_node(ic, ni);
				}
				goto err;
			}
			if (ni->ni_associd == 0) {
				DPRINTF(("ieee80211_input: "
				    "data from unassoc src %s\n",
				    ether_sprintf(wh->i_addr2)));
				IEEE80211_SEND_MGMT(ic, ni,
				    IEEE80211_FC0_SUBTYPE_DISASSOC,
				    IEEE80211_REASON_NOT_ASSOCED);
				goto err;
			}
			break;
		}
		if (wh->i_fc[1] & IEEE80211_FC1_WEP) {
			if (ic->ic_flags & IEEE80211_F_WEPON) {
				m = ieee80211_wep_crypt(ifp, m, 0);
				if (m == NULL)
					goto err;
				wh = mtod(m, struct ieee80211_frame *);
			} else
				goto out;
		}
		/* copy to listener after decrypt */
#if NBPFILTER > 0
		if (ic->ic_rawbpf)
			bpf_mtap(ic->ic_rawbpf, m);
#endif
		m = ieee80211_decap(ifp, m);
		if (m == NULL)
			goto err;
		ifp->if_ipackets++;

		/* perform as a bridge within the AP */
		m1 = NULL;
		if (ic->ic_opmode == IEEE80211_M_HOSTAP) {
			eh = mtod(m, struct ether_header *);
			if (ETHER_IS_MULTICAST(eh->ether_dhost)) {
				m1 = m_copym(m, 0, M_COPYALL, M_NOWAIT);
				if (m1 == NULL)
					ifp->if_oerrors++;
				else
					m1->m_flags |= M_MCAST;
			} else {
				ni = ieee80211_find_node(ic, eh->ether_dhost);
				if (ni != NULL && ni->ni_associd != 0) {
					m1 = m;
					m = NULL;
				}
			}
			if (m1 != NULL) {
#ifdef ALTQ
				if (ALTQ_IS_ENABLED(&ifp->if_snd))
					altq_etherclassify(&ifp->if_snd, m1,
					    &pktattr);
#endif
				len = m1->m_pkthdr.len;
				IF_ENQUEUE(&ifp->if_snd, m1);
				if (m != NULL)
					ifp->if_omcasts++;
				ifp->if_obytes += len;
			}
		}
		if (m != NULL)
			(*ifp->if_input)(ifp, m);
		return;

	case IEEE80211_FC0_TYPE_MGT:
		if (dir != IEEE80211_FC1_DIR_NODS)
			goto err;
		if (ic->ic_opmode == IEEE80211_M_AHDEMO)
			goto out;
		subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;

		/* drop frames without interest */
		if (ic->ic_state == IEEE80211_S_SCAN) {
			if (subtype != IEEE80211_FC0_SUBTYPE_BEACON &&
			    subtype != IEEE80211_FC0_SUBTYPE_PROBE_RESP)
				goto out;
		} else {
			if (ic->ic_opmode != IEEE80211_M_IBSS &&
			    subtype == IEEE80211_FC0_SUBTYPE_BEACON)
				goto out;
		}

		if (ifp->if_flags & IFF_DEBUG) {
			/* avoid to print too many frames */
			int doprint = 0;

			switch (subtype) {
			case IEEE80211_FC0_SUBTYPE_BEACON:
				if (ic->ic_state == IEEE80211_S_SCAN)
					doprint = 1;
				break;
			case IEEE80211_FC0_SUBTYPE_PROBE_REQ:
				if (ic->ic_opmode == IEEE80211_M_IBSS)
					doprint = 1;
				break;
			default:
				doprint = 1;
				break;
			}
#ifdef IEEE80211_DEBUG
			doprint += ieee80211_debug;
#endif
			if (doprint)
				if_printf(ifp, "received %s from %s rssi %d\n",
				    ieee80211_mgt_subtype_name[subtype
				    >> IEEE80211_FC0_SUBTYPE_SHIFT],
				    ether_sprintf(wh->i_addr2), rssi);
		}
#if NBPFILTER > 0
		if (ic->ic_rawbpf)
			bpf_mtap(ic->ic_rawbpf, m);
#endif
		rh = ic->ic_recv_mgmt[subtype >> IEEE80211_FC0_SUBTYPE_SHIFT];
		if (rh != NULL)
			(*rh)(ic, m, rssi, rstamp);
		m_freem(m);
		return;

	case IEEE80211_FC0_TYPE_CTL:
	default:
		DPRINTF(("ieee80211_input: bad type %x\n", wh->i_fc[0]));
		/* should not come here */
		break;
	}
  err:
	ifp->if_ierrors++;
  out:
	if (m != NULL) {
#if NBPFILTER > 0
		if (ic->ic_rawbpf)
			bpf_mtap(ic->ic_rawbpf, m);
#endif
		m_freem(m);
	}
}

int
ieee80211_mgmt_output(struct ifnet *ifp, struct ieee80211_node *ni,
    struct mbuf *m, int type)
{
	struct ieee80211com *ic = (void *)ifp;
	struct ieee80211_frame *wh;

	if (ni == NULL)
		ni = &ic->ic_bss;
	ni->ni_inact = 0;
	M_PREPEND(m, sizeof(struct ieee80211_frame), M_NOWAIT);
	if (m == NULL)
		return ENOMEM;
	wh = mtod(m, struct ieee80211_frame *);
	wh->i_fc[0] = IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_MGT | type;
	wh->i_fc[1] = IEEE80211_FC1_DIR_NODS;
	*(u_int16_t *)wh->i_dur = 0;
	*(u_int16_t *)wh->i_seq =
	    htole16(ni->ni_txseq << IEEE80211_SEQ_SEQ_SHIFT);
	ni->ni_txseq++;
	IEEE80211_ADDR_COPY(wh->i_addr1, ni->ni_macaddr);
	IEEE80211_ADDR_COPY(wh->i_addr2, ic->ic_myaddr);
	IEEE80211_ADDR_COPY(wh->i_addr3, ni->ni_bssid);

	if (ifp->if_flags & IFF_DEBUG) {
		/* avoid to print too many frames */
		if (ic->ic_opmode == IEEE80211_M_IBSS ||
#ifdef IEEE80211_DEBUG
		    ieee80211_debug > 1 ||
#endif
		    (type & IEEE80211_FC0_SUBTYPE_MASK) !=
		    IEEE80211_FC0_SUBTYPE_PROBE_RESP)
			if_printf(ifp, "sending %s to %s\n",
			    ieee80211_mgt_subtype_name[
			    (type & IEEE80211_FC0_SUBTYPE_MASK)
			    >> IEEE80211_FC0_SUBTYPE_SHIFT],
			    ether_sprintf(ni->ni_macaddr));
	}
	IF_ENQUEUE(&ic->ic_mgtq, m);
	ifp->if_timer = 1;
	(*ifp->if_start)(ifp);
	return 0;
}

struct mbuf *
ieee80211_encap(struct ifnet *ifp, struct mbuf *m)
{
	struct ieee80211com *ic = (void *)ifp;
	struct ether_header eh;
	struct ieee80211_frame *wh;
	struct llc *llc;
	struct ieee80211_node *ni;

	if (m->m_len < sizeof(struct ether_header)) {
		m = m_pullup(m, sizeof(struct ether_header));
		if (m == NULL)
			return NULL;
	}
	memcpy(&eh, mtod(m, caddr_t), sizeof(struct ether_header));

	if (!IEEE80211_IS_MULTICAST(eh.ether_dhost) &&
	    (ic->ic_opmode == IEEE80211_M_IBSS ||
	     ic->ic_opmode == IEEE80211_M_HOSTAP)) {
		ni = ieee80211_find_node(ic, eh.ether_dhost);
		if (ni == NULL)
			ni = &ic->ic_bss;	/*XXX*/
	} else
		ni = &ic->ic_bss;
	ni->ni_inact = 0;

	m_adj(m, sizeof(struct ether_header) - sizeof(struct llc));
	llc = mtod(m, struct llc *);
	llc->llc_dsap = llc->llc_ssap = LLC_SNAP_LSAP;
	llc->llc_control = LLC_UI;
	llc->llc_snap.org_code[0] = 0;
	llc->llc_snap.org_code[1] = 0;
	llc->llc_snap.org_code[2] = 0;
	llc->llc_snap.ether_type = eh.ether_type;
	M_PREPEND(m, sizeof(struct ieee80211_frame), M_NOWAIT);
	if (m == NULL)
		return NULL;
	wh = mtod(m, struct ieee80211_frame *);
	wh->i_fc[0] = IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_DATA;
	*(u_int16_t *)wh->i_dur = 0;
	*(u_int16_t *)wh->i_seq =
	    htole16(ni->ni_txseq << IEEE80211_SEQ_SEQ_SHIFT);
	ni->ni_txseq++;
	switch (ic->ic_opmode) {
	case IEEE80211_M_STA:
		wh->i_fc[1] = IEEE80211_FC1_DIR_TODS;
		IEEE80211_ADDR_COPY(wh->i_addr1, ni->ni_bssid);
		IEEE80211_ADDR_COPY(wh->i_addr2, eh.ether_shost);
		IEEE80211_ADDR_COPY(wh->i_addr3, eh.ether_dhost);
		break;
	case IEEE80211_M_IBSS:
	case IEEE80211_M_AHDEMO:
		wh->i_fc[1] = IEEE80211_FC1_DIR_NODS;
		IEEE80211_ADDR_COPY(wh->i_addr1, eh.ether_dhost);
		IEEE80211_ADDR_COPY(wh->i_addr2, eh.ether_shost);
		IEEE80211_ADDR_COPY(wh->i_addr3, ni->ni_bssid);
		break;
	case IEEE80211_M_HOSTAP:
		wh->i_fc[1] = IEEE80211_FC1_DIR_FROMDS;
		IEEE80211_ADDR_COPY(wh->i_addr1, eh.ether_dhost);
		IEEE80211_ADDR_COPY(wh->i_addr2, ni->ni_bssid);
		IEEE80211_ADDR_COPY(wh->i_addr3, eh.ether_shost);
		break;
	}
	return m;
}

struct mbuf *
ieee80211_decap(struct ifnet *ifp, struct mbuf *m)
{
	struct ether_header *eh;
	struct ieee80211_frame wh;
	struct llc *llc;

	if (m->m_len < sizeof(wh) + sizeof(*llc)) {
		m = m_pullup(m, sizeof(wh) + sizeof(*llc));
		if (m == NULL)
			return NULL;
	}
	memcpy(&wh, mtod(m, caddr_t), sizeof(wh));
	llc = (struct llc *)(mtod(m, caddr_t) + sizeof(wh));
	if (llc->llc_dsap == LLC_SNAP_LSAP && llc->llc_ssap == LLC_SNAP_LSAP &&
	    llc->llc_control == LLC_UI && llc->llc_snap.org_code[0] == 0 &&
	    llc->llc_snap.org_code[1] == 0 && llc->llc_snap.org_code[2] == 0) {
		m_adj(m, sizeof(wh) + sizeof(struct llc) - sizeof(*eh));
		llc = NULL;
	} else {
		m_adj(m, sizeof(wh) - sizeof(*eh));
	}
	eh = mtod(m, struct ether_header *);
	switch (wh.i_fc[1] & IEEE80211_FC1_DIR_MASK) {
	case IEEE80211_FC1_DIR_NODS:
		IEEE80211_ADDR_COPY(eh->ether_dhost, wh.i_addr1);
		IEEE80211_ADDR_COPY(eh->ether_shost, wh.i_addr2);
		break;
	case IEEE80211_FC1_DIR_TODS:
		IEEE80211_ADDR_COPY(eh->ether_dhost, wh.i_addr3);
		IEEE80211_ADDR_COPY(eh->ether_shost, wh.i_addr2);
		break;
	case IEEE80211_FC1_DIR_FROMDS:
		IEEE80211_ADDR_COPY(eh->ether_dhost, wh.i_addr1);
		IEEE80211_ADDR_COPY(eh->ether_shost, wh.i_addr3);
		break;
	case IEEE80211_FC1_DIR_DSTODS:
		/* not yet supported */
		DPRINTF(("ieee80211_decap: DS to DS\n"));
		m_freem(m);
		return NULL;
	}
	if (!ALIGNED_POINTER(mtod(m, caddr_t) + sizeof(*eh), u_int32_t)) {
		struct mbuf *n, *n0, **np;
		caddr_t newdata;
		int off, pktlen;

		n0 = NULL;
		np = &n0;
		off = 0;
		pktlen = m->m_pkthdr.len;
		while (pktlen > off) {
			if (n0 == NULL) {
				MGETHDR(n, M_NOWAIT, MT_DATA);
				if (n == NULL) {
					m_freem(m);
					return NULL;
				}
				M_MOVE_PKTHDR(n, m);
				n->m_len = MHLEN;
			} else {
				MGET(n, M_NOWAIT, MT_DATA);
				if (n == NULL) {
					m_freem(m);
					m_freem(n0);
					return NULL;
				}
				n->m_len = MLEN;
			}
			if (pktlen - off >= MINCLSIZE) {
				MCLGET(n, M_NOWAIT);
				if (n->m_flags & M_EXT)
					n->m_len = n->m_ext.ext_size;
			}
			if (n0 == NULL) {
				newdata =
				    (caddr_t)ALIGN(n->m_data + sizeof(*eh)) -
				    sizeof(*eh);
				n->m_len -= newdata - n->m_data;
				n->m_data = newdata;
			}
			if (n->m_len > pktlen - off)
				n->m_len = pktlen - off;
			m_copydata(m, off, n->m_len, mtod(n, caddr_t));
			off += n->m_len;
			*np = n;
			np = &n->m_next;
		}
		m_freem(m);
		m = n0;
	}
	if (llc != NULL) {
		eh = mtod(m, struct ether_header *);
		eh->ether_type = htons(m->m_pkthdr.len - sizeof(*eh));
	}
	return m;
}

int
ieee80211_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct ieee80211com *ic = (void *)ifp;
	int error = 0;
	u_int kid, len;
	struct ieee80211req *ireq;
	u_int8_t tmpkey[IEEE80211_KEYBUF_SIZE];
	char tmpssid[IEEE80211_NWID_LEN];

	switch (cmd) {
	case SIOCG80211:
		ireq = (struct ieee80211req *) data;
		switch (ireq->i_type) {
		case IEEE80211_IOC_SSID:
			switch (ic->ic_state) {
			case IEEE80211_S_INIT:
			case IEEE80211_S_SCAN:
				ireq->i_len = ic->ic_des_esslen;
				memcpy(tmpssid, ic->ic_des_essid, ireq->i_len);
				break;
			default:
				ireq->i_len = ic->ic_bss.ni_esslen;
				memcpy(tmpssid, ic->ic_bss.ni_essid,
					ireq->i_len);
				break;
			}
			error = copyout(tmpssid, ireq->i_data, ireq->i_len);
			break;
		case IEEE80211_IOC_NUMSSIDS:
			ireq->i_val = 1;
			break;
		case IEEE80211_IOC_WEP:
			if ((ic->ic_flags & IEEE80211_F_HASWEP) == 0) {
				ireq->i_val = IEEE80211_WEP_NOSUP; 
			} else {
				if (ic->ic_flags & IEEE80211_F_WEPON) {
					ireq->i_val =
					    IEEE80211_WEP_MIXED;
				} else {
					ireq->i_val =
					    IEEE80211_WEP_OFF;
				}
			}
			break;
		case IEEE80211_IOC_WEPKEY:
			if ((ic->ic_flags & IEEE80211_F_HASWEP) == 0) {
				error = EINVAL;
				break;
			}
			kid = (u_int) ireq->i_val;
			if (kid >= IEEE80211_WEP_NKID) {
				error = EINVAL;
				break;
			}
			len = (u_int) ic->ic_nw_keys[kid].wk_len;
			/* NB: only root can read WEP keys */
			if (suser(curthread)) {
				bcopy(ic->ic_nw_keys[kid].wk_key, tmpkey, len);
			} else {
				bzero(tmpkey, len);
			}
			ireq->i_len = len;
			error = copyout(tmpkey, ireq->i_data, len);
			break;
		case IEEE80211_IOC_NUMWEPKEYS:
			if ((ic->ic_flags & IEEE80211_F_HASWEP) == 0)
				error = EINVAL;
			else
				ireq->i_val = IEEE80211_WEP_NKID;
			break;
		case IEEE80211_IOC_WEPTXKEY:
			if ((ic->ic_flags & IEEE80211_F_HASWEP) == 0)
				error = EINVAL;
			else
				ireq->i_val = ic->ic_wep_txkey;
			break;
		case IEEE80211_IOC_AUTHMODE:
			ireq->i_val = IEEE80211_AUTH_OPEN;
			break;
		case IEEE80211_IOC_CHANNEL:
			switch (ic->ic_state) {
			case IEEE80211_S_INIT:
			case IEEE80211_S_SCAN:
				if (ic->ic_opmode == IEEE80211_M_STA)
					ireq->i_val = ic->ic_des_chan;
				else
					ireq->i_val = ic->ic_ibss_chan;
				break;
			default:
				ireq->i_val = ic->ic_bss.ni_chan;
				break;
			}
			break;
		case IEEE80211_IOC_POWERSAVE:
			if (ic->ic_flags & IEEE80211_F_PMGTON)
				ireq->i_val = IEEE80211_POWERSAVE_ON;
			else
				ireq->i_val = IEEE80211_POWERSAVE_OFF;
			break;
		case IEEE80211_IOC_POWERSAVESLEEP:
			ireq->i_val = ic->ic_lintval;
			break;
		default:
			error = EINVAL;
		}
		break;
	case SIOCS80211:
		error = suser(curthread);
		if (error)
			break;
		ireq = (struct ieee80211req *) data;
		switch (ireq->i_type) {
		case IEEE80211_IOC_SSID:
			if (ireq->i_val != 0 ||
			    ireq->i_len > IEEE80211_NWID_LEN) {
				error = EINVAL;
				break;
			}
			error = copyin(ireq->i_data, tmpssid, ireq->i_len);
			if (error)
				break;
			memset(ic->ic_des_essid, 0, IEEE80211_NWID_LEN);
			ic->ic_des_esslen = ireq->i_len;
			memcpy(ic->ic_des_essid, tmpssid, ireq->i_len);
			error = ENETRESET;
			break;
		case IEEE80211_IOC_WEP:
			/*
			 * These cards only support one mode so
			 * we just turn wep on what ever is
			 * passed in if it's not OFF.
			 */
			if (ireq->i_val == IEEE80211_WEP_OFF) {
				ic->ic_flags &= ~IEEE80211_F_WEPON;
			} else {
				ic->ic_flags |= IEEE80211_F_WEPON;
			}
			error = ENETRESET;
			break;
		case IEEE80211_IOC_WEPKEY:
			if ((ic->ic_flags & IEEE80211_F_HASWEP) == 0) {
				error = EINVAL;
				break;
			}
			kid = (u_int) ireq->i_val;
			if (kid >= IEEE80211_WEP_NKID) {
				error = EINVAL;
				break;
			} 
			if (ireq->i_len > sizeof(tmpkey)) {
				error = EINVAL;
				break;
			}
			memset(tmpkey, 0, sizeof(tmpkey));
			error = copyin(ireq->i_data, tmpkey, ireq->i_len);
			if (error)
				break;
			memcpy(ic->ic_nw_keys[kid].wk_key, tmpkey,
				sizeof(tmpkey));
			ic->ic_nw_keys[kid].wk_len = ireq->i_len;
			error = ENETRESET;
			break;
		case IEEE80211_IOC_WEPTXKEY:
			kid = (u_int) ireq->i_val;
			if (kid >= IEEE80211_WEP_NKID) {
				error = EINVAL;
				break;
			}
			ic->ic_wep_txkey = kid;
			error = ENETRESET;
			break;
#if 0
		case IEEE80211_IOC_AUTHMODE:
			sc->wi_authmode = ireq->i_val;
			break;
#endif
		case IEEE80211_IOC_CHANNEL:
			/* XXX 0xffff overflows 16-bit signed */
			if (ireq->i_val == (int16_t) IEEE80211_CHAN_ANY)
				ic->ic_des_chan = IEEE80211_CHAN_ANY;
			else if ((u_int) ireq->i_val > IEEE80211_CHAN_MAX ||
			    isclr(ic->ic_chan_active, ireq->i_val)) {
				error = EINVAL;
				break;
			} else
				ic->ic_ibss_chan = ic->ic_des_chan = ireq->i_val;
			switch (ic->ic_state) {
			case IEEE80211_S_INIT:
			case IEEE80211_S_SCAN:
				error = ENETRESET;
				break;
			default:
				if (ic->ic_opmode == IEEE80211_M_STA) {
					if (ic->ic_des_chan != IEEE80211_CHAN_ANY &&
					    ic->ic_bss.ni_chan != ic->ic_des_chan)
						error = ENETRESET;
				} else {
					if (ic->ic_bss.ni_chan != ic->ic_ibss_chan)
						error = ENETRESET;
				}
				break;
			}
			break;
		case IEEE80211_IOC_POWERSAVE:
			switch (ireq->i_val) {
			case IEEE80211_POWERSAVE_OFF:
				if (ic->ic_flags & IEEE80211_F_PMGTON) {
					ic->ic_flags &= ~IEEE80211_F_PMGTON;
					error = ENETRESET;
				}
				break;
			case IEEE80211_POWERSAVE_ON:
				if ((ic->ic_flags & IEEE80211_F_HASPMGT) == 0)
					error = EINVAL;
				else if ((ic->ic_flags & IEEE80211_F_PMGTON) == 0) {
					ic->ic_flags |= IEEE80211_F_PMGTON;
					error = ENETRESET;
				}
				break;
			default:
				error = EINVAL;
				break;
			}
			break;
		case IEEE80211_IOC_POWERSAVESLEEP:
			if (ireq->i_val < 0) {
				error = EINVAL;
				break;
			}
			ic->ic_lintval = ireq->i_val;
			error = ENETRESET;
			break;
		default:
			error = EINVAL;
			break;
		}
		break;
	case SIOCGIFGENERIC:
		error = ieee80211_cfgget(ifp, cmd, data);
		break;
	case SIOCSIFGENERIC:
		error = suser(curthread);
		if (error)
			break;
		error = ieee80211_cfgset(ifp, cmd, data);
		break;
	default:
		error = ether_ioctl(ifp, cmd, data);
		break;
	}
	return error;
}

void
ieee80211_print_essid(u_int8_t *essid, int len)
{
	int i;
	u_int8_t *p; 

	if (len > IEEE80211_NWID_LEN)
		len = IEEE80211_NWID_LEN;
	/* determine printable or not */
	for (i = 0, p = essid; i < len; i++, p++) {
		if (*p < ' ' || *p > 0x7e)
			break;
	}
	if (i == len) {
		printf("\"");
		for (i = 0, p = essid; i < len; i++, p++)
			printf("%c", *p);
		printf("\"");
	} else {
		printf("0x");
		for (i = 0, p = essid; i < len; i++, p++)
			printf("%02x", *p);
	}
}

void
ieee80211_dump_pkt(u_int8_t *buf, int len, int rate, int rssi)
{
	struct ieee80211_frame *wh;
	int i;

	wh = (struct ieee80211_frame *)buf;
	switch (wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) {
	case IEEE80211_FC1_DIR_NODS:
		printf("NODS %s", ether_sprintf(wh->i_addr2));
		printf("->%s", ether_sprintf(wh->i_addr1));
		printf("(%s)", ether_sprintf(wh->i_addr3));
		break;
	case IEEE80211_FC1_DIR_TODS:
		printf("TODS %s", ether_sprintf(wh->i_addr2));
		printf("->%s", ether_sprintf(wh->i_addr3));
		printf("(%s)", ether_sprintf(wh->i_addr1));
		break;
	case IEEE80211_FC1_DIR_FROMDS:
		printf("FRDS %s", ether_sprintf(wh->i_addr3));
		printf("->%s", ether_sprintf(wh->i_addr1));
		printf("(%s)", ether_sprintf(wh->i_addr2));
		break;
	case IEEE80211_FC1_DIR_DSTODS:
		printf("DSDS %s", ether_sprintf((u_int8_t *)&wh[1]));
		printf("->%s", ether_sprintf(wh->i_addr3));
		printf("(%s", ether_sprintf(wh->i_addr2));
		printf("->%s)", ether_sprintf(wh->i_addr1));
		break;
	}
	switch (wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) {
	case IEEE80211_FC0_TYPE_DATA:
		printf(" data");
		break;
	case IEEE80211_FC0_TYPE_MGT:
		printf(" %s", ieee80211_mgt_subtype_name[
		    (wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK)
		    >> IEEE80211_FC0_SUBTYPE_SHIFT]);
		break;
	default:
		printf(" type#%d", wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK);
		break;
	}
	if (wh->i_fc[1] & IEEE80211_FC1_WEP)
		printf(" WEP");
	if (rate >= 0)
		printf(" %dM", rate / 2);
	if (rssi >= 0)
		printf(" +%d", rssi);
	printf("\n");
	if (len > 0) {
		for (i = 0; i < len; i++) {
			if ((i & 1) == 0)
				printf(" ");
			printf("%02x", buf[i]);
		}
		printf("\n");
	}
}

void
ieee80211_watchdog(struct ifnet *ifp)
{
	struct ieee80211com *ic = (void *)ifp;
	struct ieee80211_node *ni, *nextbs;

	if (ic->ic_scan_timer) {
		if (--ic->ic_scan_timer == 0) {
			if (ic->ic_state == IEEE80211_S_SCAN)
				ieee80211_end_scan(ifp);
		}
	}
	if (ic->ic_mgt_timer) {
		if (--ic->ic_mgt_timer == 0)
			ieee80211_new_state(ifp, IEEE80211_S_SCAN, -1);
	}
	if (ic->ic_inact_timer) {
		if (--ic->ic_inact_timer == 0) {
			for (ni = TAILQ_FIRST(&ic->ic_node); ni != NULL; ) {
				if (++ni->ni_inact <= IEEE80211_INACT_MAX) {
					ni = TAILQ_NEXT(ni, ni_list);
					continue;
				}
				if (ifp->if_flags & IFF_DEBUG)
					if_printf(ifp, "station %s deauthenticate"
					    " (reason %d)\n",
					    ether_sprintf(ni->ni_macaddr),
					    IEEE80211_REASON_AUTH_EXPIRE);
				nextbs = TAILQ_NEXT(ni, ni_list);
				IEEE80211_SEND_MGMT(ic, ni,
				    IEEE80211_FC0_SUBTYPE_DEAUTH,
				    IEEE80211_REASON_AUTH_EXPIRE);
				ieee80211_free_node(ic, ni);
				ni = nextbs;
			}
			if (!TAILQ_EMPTY(&ic->ic_node))
				ic->ic_inact_timer = IEEE80211_INACT_WAIT;
		}
	}
	if (ic->ic_scan_timer != 0 || ic->ic_mgt_timer != 0 ||
	    ic->ic_inact_timer != 0)
		ifp->if_timer = 1;
}

void
ieee80211_next_scan(struct ifnet *ifp)
{
	struct ieee80211com *ic = (void *)ifp;
	int chan;

	chan = ic->ic_bss.ni_chan;
	for (;;) {
		chan = (chan + 1) % (IEEE80211_CHAN_MAX + 1);
		if (isset(ic->ic_chan_active, chan))
			break;
		if (chan == ic->ic_bss.ni_chan) {
			DPRINTF(("ieee80211_next_scan: no chan available\n"));
			return;
		}
	}
	DPRINTF(("ieee80211_next_scan: chan %d->%d\n",
	    ic->ic_bss.ni_chan, chan));
	ic->ic_bss.ni_chan = chan;
	ieee80211_new_state(ifp, IEEE80211_S_SCAN, -1);
}

void
ieee80211_end_scan(struct ifnet *ifp)
{
	struct ieee80211com *ic = (void *)ifp;
	struct ieee80211_node *ni, *nextbs, *selbs;
	void *p;
	u_int8_t rate;
	int i, fail;

	ni = TAILQ_FIRST(&ic->ic_node);
	if (ni == NULL) {
		DPRINTF(("ieee80211_end_scan: no scan candidate\n"));
  notfound:
		if (ic->ic_opmode == IEEE80211_M_IBSS &&
		    (ic->ic_flags & IEEE80211_F_IBSSON) &&
		    ic->ic_des_esslen != 0) {
			ni = &ic->ic_bss;
			if (ifp->if_flags & IFF_DEBUG)
				if_printf(ifp, "creating ibss\n");
			ic->ic_flags |= IEEE80211_F_SIBSS;
			ni->ni_nrate = 0;
			for (i = 0; i < IEEE80211_RATE_SIZE; i++) {
				if (ic->ic_sup_rates[i])
					ni->ni_rates[ni->ni_nrate++] =
					    ic->ic_sup_rates[i];
			}
			IEEE80211_ADDR_COPY(ni->ni_macaddr, ic->ic_myaddr);
			IEEE80211_ADDR_COPY(ni->ni_bssid, ic->ic_myaddr);
			ni->ni_bssid[0] |= 0x02;	/* local bit for IBSS */
			ni->ni_esslen = ic->ic_des_esslen;
			memcpy(ni->ni_essid, ic->ic_des_essid, ni->ni_esslen);
			ni->ni_rssi = 0;
			ni->ni_rstamp = 0;
			memset(ni->ni_tstamp, 0, sizeof(ni->ni_tstamp));
			ni->ni_intval = ic->ic_lintval;
			ni->ni_capinfo = IEEE80211_CAPINFO_IBSS;
			if (ic->ic_flags & IEEE80211_F_WEPON)
				ni->ni_capinfo |= IEEE80211_CAPINFO_PRIVACY;
			ni->ni_chan = ic->ic_ibss_chan;
			if (ic->ic_phytype == IEEE80211_T_FH) {
				ni->ni_fhdwell = 200;	/* XXX */
				ni->ni_fhindex = 1;
			}
			ieee80211_new_state(ifp, IEEE80211_S_RUN, -1);
			return;
		}
		if (ic->ic_flags & IEEE80211_F_ASCAN) {
			if (ifp->if_flags & IFF_DEBUG)
				if_printf(ifp, "entering passive scan mode\n");
			ic->ic_flags &= ~IEEE80211_F_ASCAN;
		}
		ieee80211_next_scan(ifp);
		return;
	}
	selbs = NULL;
	if (ifp->if_flags & IFF_DEBUG)
		if_printf(ifp, "\tmacaddr          bssid         chan  rssi rate flag  wep  essid\n");
	for (; ni != NULL; ni = nextbs) {
		nextbs = TAILQ_NEXT(ni, ni_list);
		if (ni->ni_fails) {
			/*
			 * The configuration of the access points may change
			 * during my scan.  So delete the entry for the AP
			 * and retry to associate if there is another beacon.
			 */
			if (ni->ni_fails++ > 2)
				ieee80211_free_node(ic, ni);
			continue;
		}
		fail = 0;
		if (isclr(ic->ic_chan_active, ni->ni_chan))
			fail |= 0x01;
		if (ic->ic_des_chan != IEEE80211_CHAN_ANY &&
		    ni->ni_chan != ic->ic_des_chan)
			fail |= 0x01;
		if (ic->ic_opmode == IEEE80211_M_IBSS) {
			if ((ni->ni_capinfo & IEEE80211_CAPINFO_IBSS) == 0)
				fail |= 0x02;
		} else {
			if ((ni->ni_capinfo & IEEE80211_CAPINFO_ESS) == 0)
				fail |= 0x02;
		}
		if (ic->ic_flags & IEEE80211_F_WEPON) {
			if ((ni->ni_capinfo & IEEE80211_CAPINFO_PRIVACY) == 0)
				fail |= 0x04;
		} else {
			if (ni->ni_capinfo & IEEE80211_CAPINFO_PRIVACY)
				fail |= 0x04;
		}
		rate = ieee80211_fix_rate(ic, ni, IEEE80211_F_DONEGO);
		if (rate & IEEE80211_RATE_BASIC)
			fail |= 0x08;
		if (ic->ic_des_esslen != 0 &&
		    (ni->ni_esslen != ic->ic_des_esslen ||
		     memcmp(ni->ni_essid, ic->ic_des_essid,
		     ic->ic_des_esslen != 0)))
			fail |= 0x10;
		if ((ic->ic_flags & IEEE80211_F_DESBSSID) &&
		    !IEEE80211_ADDR_EQ(ic->ic_des_bssid, ni->ni_bssid))
			fail |= 0x20;
		if (ifp->if_flags & IFF_DEBUG) {
			printf(" %c %s", fail ? '-' : '+',
			    ether_sprintf(ni->ni_macaddr));
			printf(" %s%c", ether_sprintf(ni->ni_bssid),
			    fail & 0x20 ? '!' : ' ');
			printf(" %3d%c", ni->ni_chan, fail & 0x01 ? '!' : ' ');
			printf(" %+4d", ni->ni_rssi);
			printf(" %2dM%c", (rate & IEEE80211_RATE_VAL) / 2,
			    fail & 0x08 ? '!' : ' ');
			printf(" %4s%c",
			    (ni->ni_capinfo & IEEE80211_CAPINFO_ESS) ? "ess" :
			    (ni->ni_capinfo & IEEE80211_CAPINFO_IBSS) ? "ibss" :
			    "????",
			    fail & 0x02 ? '!' : ' ');
			printf(" %3s%c ",
			    (ni->ni_capinfo & IEEE80211_CAPINFO_PRIVACY) ?
			    "wep" : "no",
			    fail & 0x04 ? '!' : ' ');
			ieee80211_print_essid(ni->ni_essid, ni->ni_esslen);
			printf("%s\n", fail & 0x10 ? "!" : "");
		}
		if (!fail) {
			if (selbs == NULL || ni->ni_rssi > selbs->ni_rssi)
				selbs = ni;
		}
	}
	if (selbs == NULL)
		goto notfound;
	p = ic->ic_bss.ni_private;
	ic->ic_bss = *selbs;
	ic->ic_bss.ni_private = p;
	if (p != NULL && ic->ic_node_privlen)
		memcpy(p, selbs->ni_private, ic->ic_node_privlen);
	if (ic->ic_opmode == IEEE80211_M_IBSS) {
		ieee80211_fix_rate(ic, &ic->ic_bss, IEEE80211_F_DOFRATE |
		    IEEE80211_F_DONEGO | IEEE80211_F_DODEL);
		if (ic->ic_bss.ni_nrate == 0) {
			selbs->ni_fails++;
			goto notfound;
		}
		ieee80211_new_state(ifp, IEEE80211_S_RUN, -1);
	} else
		ieee80211_new_state(ifp, IEEE80211_S_AUTH, -1);
}

struct ieee80211_node *
ieee80211_alloc_node(struct ieee80211com *ic, u_int8_t *macaddr, int copy)
{
	struct ieee80211_node *ni;
	int hash;

	ni = malloc(sizeof(struct ieee80211_node) + ic->ic_node_privlen,
	    M_DEVBUF, M_NOWAIT);
	if (ni == NULL)
		return NULL;
	if (copy)
		memcpy(ni, &ic->ic_bss, sizeof(struct ieee80211_node));
	else
		memset(ni, 0, sizeof(struct ieee80211_node));
	IEEE80211_ADDR_COPY(ni->ni_macaddr, macaddr);
	if (ic->ic_node_privlen) {
		ni->ni_private = &ni[1];
		memset(ni->ni_private, 0, ic->ic_node_privlen);
	} else
		ni->ni_private = NULL;

	hash = IEEE80211_NODE_HASH(macaddr);
	IEEE80211_LOCK(ic);
	TAILQ_INSERT_TAIL(&ic->ic_node, ni, ni_list);
	LIST_INSERT_HEAD(&ic->ic_hash[hash], ni, ni_hash);
	IEEE80211_UNLOCK(ic);
	ic->ic_inact_timer = IEEE80211_INACT_WAIT;
	return ni;
}

struct ieee80211_node *
ieee80211_find_node(struct ieee80211com *ic, u_int8_t *macaddr)
{
	struct ieee80211_node *ni;
	int hash;

	hash = IEEE80211_NODE_HASH(macaddr);
	IEEE80211_LOCK(ic);
	LIST_FOREACH(ni, &ic->ic_hash[hash], ni_hash) {
		if (IEEE80211_ADDR_EQ(ni->ni_macaddr, macaddr))
			break;
	}
	IEEE80211_UNLOCK(ic);
	return ni;
}

void
ieee80211_free_node(struct ieee80211com *ic, struct ieee80211_node *ni)
{
	IEEE80211_LOCK(ic);
	if (ic->ic_node_free != NULL)
		(*ic->ic_node_free)(ic, ni);
	TAILQ_REMOVE(&ic->ic_node, ni, ni_list);
	LIST_REMOVE(ni, ni_hash);
	IEEE80211_UNLOCK(ic);
	free(ni, M_DEVBUF);
	if (TAILQ_EMPTY(&ic->ic_node))
		ic->ic_inact_timer = 0;
}

void
ieee80211_free_allnodes(struct ieee80211com *ic)
{
	struct ieee80211_node *ni;

	while ((ni = TAILQ_FIRST(&ic->ic_node)) != NULL)
		ieee80211_free_node(ic, ni);  
}

int
ieee80211_fix_rate(struct ieee80211com *ic, struct ieee80211_node *ni, int flags)
{
	int i, j, ignore, error;
	int okrate, badrate;
	u_int8_t r;

	error = 0;
	okrate = badrate = 0;
	for (i = 0; i < ni->ni_nrate; ) {
		ignore = 0;
		if (flags & IEEE80211_F_DOSORT) {
			for (j = i + 1; j < ni->ni_nrate; j++) {
				if ((ni->ni_rates[i] & IEEE80211_RATE_VAL) >
				    (ni->ni_rates[j] & IEEE80211_RATE_VAL)) {
					r = ni->ni_rates[i];
					ni->ni_rates[i] = ni->ni_rates[j];
					ni->ni_rates[j] = r;
				}
			}
		}
		r = ni->ni_rates[i] & IEEE80211_RATE_VAL;
		badrate = r;
		if (flags & IEEE80211_F_DOFRATE) {
			if (ic->ic_fixed_rate >= 0 &&
			    r != (ic->ic_sup_rates[ic->ic_fixed_rate] &
			    IEEE80211_RATE_VAL))
				ignore++;
		}
		if (flags & IEEE80211_F_DONEGO) {
			for (j = 0; j < IEEE80211_RATE_SIZE; j++) {
				if (r ==
				    (ic->ic_sup_rates[j] & IEEE80211_RATE_VAL))
					break;
			}
			if (j == IEEE80211_RATE_SIZE) {
				if (ni->ni_rates[i] & IEEE80211_RATE_BASIC)
					error++;
				ignore++;
			}
		}
		if (flags & IEEE80211_F_DODEL) {
			if (ignore) {
				ni->ni_nrate--;
				for (j = i; j < ni->ni_nrate; j++)
					ni->ni_rates[j] = ni->ni_rates[j + 1];
				ni->ni_rates[j] = 0;
				continue;
			}
		}
		if (!ignore)
			okrate = ni->ni_rates[i];
		i++;
	}
	if (okrate == 0 || error != 0)
		return badrate | IEEE80211_RATE_BASIC;
	return okrate & IEEE80211_RATE_VAL;
}

static int
ieee80211_send_prreq(struct ieee80211com *ic, struct ieee80211_node *ni,
    int type, int dummy)
{
	int i, ret;
	struct mbuf *m;
	u_int8_t *frm;

	/*
	 * prreq frame format
	 *	[tlv] ssid
	 *	[tlv] supported rates
	 */
	MGETHDR(m, M_NOWAIT, MT_DATA);
	if (m == NULL)
		return ENOMEM;
	m->m_data += sizeof(struct ieee80211_frame);
	frm = mtod(m, u_int8_t *);

	*frm++ = IEEE80211_ELEMID_SSID;
	*frm++ = ic->ic_des_esslen;
	memcpy(frm, ic->ic_des_essid, ic->ic_des_esslen);
	frm += ic->ic_des_esslen;

	*frm++ = IEEE80211_ELEMID_RATES;
	for (i = 0; i < IEEE80211_RATE_SIZE; i++) {
		if (ic->ic_sup_rates[i] != 0)
			frm[i + 1] = ic->ic_sup_rates[i];
	}
	*frm++ = i;
	frm += i;
	m->m_pkthdr.len = m->m_len = frm - mtod(m, u_int8_t *);

	ret = ieee80211_mgmt_output(&ic->ic_if, ni, m, type);
	ic->ic_mgt_timer = IEEE80211_TRANS_WAIT;
	return ret;
}

static int
ieee80211_send_prresp(struct ieee80211com *ic, struct ieee80211_node *bs0,
    int type, int dummy)
{
	struct mbuf *m;
	u_int8_t *frm;
	struct ieee80211_node *ni = &ic->ic_bss;
	u_int16_t capinfo;

	/*
	 * probe response frame format
	 *	[8] time stamp
	 *	[2] beacon interval
	 *	[2] cabability information
	 *	[tlv] ssid
	 *	[tlv] supported rates
	 *	[tlv] parameter set (IBSS)
	 */
	MGETHDR(m, M_NOWAIT, MT_DATA);
	if (m == NULL)
		return ENOMEM;
	m->m_data += sizeof(struct ieee80211_frame);
	frm = mtod(m, u_int8_t *);

	memset(frm, 0, 8);	/* timestamp should be filled later */
	frm += 8;
	*(u_int16_t *)frm = htole16(ni->ni_intval);
	frm += 2;
	if (ic->ic_opmode == IEEE80211_M_IBSS)
		capinfo = IEEE80211_CAPINFO_IBSS;
	else
		capinfo = IEEE80211_CAPINFO_ESS;
	if (ic->ic_flags & IEEE80211_F_WEPON)
		capinfo |= IEEE80211_CAPINFO_PRIVACY;
	*(u_int16_t *)frm = htole16(capinfo);
	frm += 2;
	*frm++ = IEEE80211_ELEMID_SSID;
	*frm++ = ni->ni_esslen;
	memcpy(frm, ni->ni_essid, ni->ni_esslen);
	frm += ni->ni_esslen;
	*frm++ = IEEE80211_ELEMID_RATES;
	*frm++ = ni->ni_nrate;
	memcpy(frm, ni->ni_rates, ni->ni_nrate);
	frm += ni->ni_nrate;
	if (ic->ic_opmode == IEEE80211_M_IBSS) {
		*frm++ = IEEE80211_ELEMID_IBSSPARMS;
		*frm++ = 2;
		*frm++ = 0; *frm++ = 0;		/* TODO: ATIM window */
	} else {	/* IEEE80211_M_HOSTAP */
		/* TODO: TIM */
		*frm++ = IEEE80211_ELEMID_TIM;
		*frm++ = 4;	/* length */
		*frm++ = 0;	/* DTIM count */
		*frm++ = 1;	/* DTIM period */
		*frm++ = 0;	/* bitmap control */
		*frm++ = 0;	/* Partial Virtual Bitmap (variable length) */
	}
	/* TODO: check MHLEN limit */
	m->m_pkthdr.len = m->m_len = frm - mtod(m, u_int8_t *);

	return ieee80211_mgmt_output(&ic->ic_if, bs0, m, type);
}

static int
ieee80211_send_auth(struct ieee80211com *ic, struct ieee80211_node *ni,
    int type, int seq)
{
	struct mbuf *m;
	u_int16_t *frm;
	int ret;

	MGETHDR(m, M_NOWAIT, MT_DATA);
	if (m == NULL)
		return ENOMEM;
	MH_ALIGN(m, 2 * 3);
	m->m_pkthdr.len = m->m_len = 6;
	frm = mtod(m, u_int16_t *);
	/* TODO: shared key auth */
	frm[0] = htole16(IEEE80211_AUTH_ALG_OPEN);
	frm[1] = htole16(seq);
	frm[2] = 0;			/* status */
	ret = ieee80211_mgmt_output(&ic->ic_if, ni, m, type);
	if (ic->ic_opmode == IEEE80211_M_STA)
		ic->ic_mgt_timer = IEEE80211_TRANS_WAIT;
	return ret;
}

static int
ieee80211_send_deauth(struct ieee80211com *ic, struct ieee80211_node *ni,
    int type, int reason)
{
	struct ifnet *ifp = &ic->ic_if;
	struct mbuf *m;

	if (ifp->if_flags & IFF_DEBUG)
		if_printf(ifp, "station %s deauthenticate (reason %d)\n",
		    ether_sprintf(ni->ni_macaddr), reason);
	MGETHDR(m, M_NOWAIT, MT_DATA);
	if (m == NULL)
		return ENOMEM;
	MH_ALIGN(m, 2);
	m->m_pkthdr.len = m->m_len = 2;
	*mtod(m, u_int16_t *) = htole16(reason);
	return ieee80211_mgmt_output(&ic->ic_if, ni, m, type);
}

static int
ieee80211_send_asreq(struct ieee80211com *ic, struct ieee80211_node *ni,
    int type, int dummy)
{
	struct mbuf *m;
	u_int8_t *frm, *rates;
	u_int16_t capinfo;
	int i, ret;

	/*
	 * asreq frame format
	 *	[2] capability information
	 *	[2] listen interval
	 *	[6*] current AP address (reassoc only)
	 *	[tlv] ssid
	 *	[tlv] supported rates
	 */
	MGETHDR(m, M_NOWAIT, MT_DATA);
	if (m == NULL)
		return ENOMEM;
	m->m_data += sizeof(struct ieee80211_frame);
	frm = mtod(m, u_int8_t *);

	capinfo = 0;
	if (ic->ic_opmode == IEEE80211_M_IBSS)
		capinfo |= IEEE80211_CAPINFO_IBSS;
	else		/* IEEE80211_M_STA */
		capinfo |= IEEE80211_CAPINFO_ESS;
	if (ic->ic_flags & IEEE80211_F_WEPON)
		capinfo |= IEEE80211_CAPINFO_PRIVACY;
	*(u_int16_t *)frm = htole16(capinfo);
	frm += 2;

	*(u_int16_t *)frm = htole16(ic->ic_lintval);
	frm += 2;

	if (type == IEEE80211_FC0_SUBTYPE_REASSOC_REQ) {
		IEEE80211_ADDR_COPY(frm, ic->ic_bss.ni_bssid);
		frm += IEEE80211_ADDR_LEN;
	}

	*frm++ = IEEE80211_ELEMID_SSID;
	*frm++ = ni->ni_esslen;
	memcpy(frm, ni->ni_essid, ni->ni_esslen);
	frm += ni->ni_esslen;

	*frm++ = IEEE80211_ELEMID_RATES;
	rates = frm++;	/* update later */
	for (i = 0; i < IEEE80211_RATE_SIZE; i++) {
		if (ni->ni_rates[i] != 0)
			*frm++ = ni->ni_rates[i];
	}
	*rates = frm - (rates + 1);
	m->m_pkthdr.len = m->m_len = frm - mtod(m, u_int8_t *);
	ret = ieee80211_mgmt_output(&ic->ic_if, ni, m, type);
	ic->ic_mgt_timer = IEEE80211_TRANS_WAIT;
	return ret;
}

static int
ieee80211_send_asresp(struct ieee80211com *ic, struct ieee80211_node *ni,
    int type, int status)
{
	struct mbuf *m;
	u_int8_t *frm, *rates, *r;
	u_int16_t capinfo;
	int i;

	/*
	 * asreq frame format
	 *	[2] capability information
	 *	[2] status
	 *	[2] association ID
	 *	[tlv] supported rates
	 */
	MGETHDR(m, M_NOWAIT, MT_DATA);
	if (m == NULL)
		return ENOMEM;
	m->m_data += sizeof(struct ieee80211_frame);
	frm = mtod(m, u_int8_t *);

	capinfo = IEEE80211_CAPINFO_ESS;
	if (ic->ic_flags & IEEE80211_F_WEPON)
		capinfo |= IEEE80211_CAPINFO_PRIVACY;
	*(u_int16_t *)frm = htole16(capinfo);
	frm += 2;

	*(u_int16_t *)frm = htole16(status);
	frm += 2;

	if (status == IEEE80211_STATUS_SUCCESS && ni != NULL)
		*(u_int16_t *)frm = htole16(ni->ni_associd);
	else
		*(u_int16_t *)frm = htole16(0);
	frm += 2;

	*frm++ = IEEE80211_ELEMID_RATES;
	rates = frm++;	/* update later */
	if (ni != NULL)
		r = ni->ni_rates;
	else
		r = ic->ic_bss.ni_rates;
	for (i = 0; i < IEEE80211_RATE_SIZE; i++, r++) {
		if (*r != 0)
			*frm++ = *r;
	}
	*rates = frm - (rates + 1);
	m->m_pkthdr.len = m->m_len = frm - mtod(m, u_int8_t *);
	return ieee80211_mgmt_output(&ic->ic_if, ni, m, type);
}

static int
ieee80211_send_disassoc(struct ieee80211com *ic, struct ieee80211_node *ni,
    int type, int reason)
{
	struct ifnet *ifp = &ic->ic_if;
	struct mbuf *m;

	if (ifp->if_flags & IFF_DEBUG)
		if_printf(ifp, "station %s disassociate (reason %d)\n",
		    ether_sprintf(ni->ni_macaddr), reason);
	MGETHDR(m, M_NOWAIT, MT_DATA);
	if (m == NULL)
		return ENOMEM;
	MH_ALIGN(m, 2);
	m->m_pkthdr.len = m->m_len = 2;
	*mtod(m, u_int16_t *) = htole16(reason);
	return ieee80211_mgmt_output(&ic->ic_if, ni, m,
	    IEEE80211_FC0_SUBTYPE_DISASSOC);
}

static void
ieee80211_recv_beacon(struct ieee80211com *ic, struct mbuf *m0, int rssi,
    u_int32_t rstamp)
{
	struct ieee80211_frame *wh;
	struct ieee80211_node *ni;
	u_int8_t *frm, *efrm, *tstamp, *bintval, *capinfo, *ssid, *rates;
	u_int8_t chan, fhindex;
	u_int16_t fhdwell;

	if (ic->ic_opmode != IEEE80211_M_IBSS &&
	    ic->ic_state != IEEE80211_S_SCAN) {
		/* XXX: may be useful for background scan */
		return;
	}

	wh = mtod(m0, struct ieee80211_frame *);
	frm = (u_int8_t *)&wh[1];
	efrm = mtod(m0, u_int8_t *) + m0->m_len;
	/*
	 * beacon frame format
	 *	[8] time stamp
	 *	[2] beacon interval
	 *	[2] cabability information
	 *	[tlv] ssid
	 *	[tlv] supported rates
	 *	[tlv] parameter set (FH/DS)
	 */
	tstamp  = frm;	frm += 8;
	bintval = frm;	frm += 2;
	capinfo = frm;	frm += 2;
	ssid = rates = NULL;
	chan = ic->ic_bss.ni_chan;
	fhdwell = 0;
	fhindex = 0;
	while (frm < efrm) {
		switch (*frm) {
		case IEEE80211_ELEMID_SSID:
			ssid = frm;
			break;
		case IEEE80211_ELEMID_RATES:
			rates = frm;
			break;
		case IEEE80211_ELEMID_FHPARMS:
			if (ic->ic_phytype == IEEE80211_T_FH) {
				fhdwell = (frm[3] << 8) | frm[2];
				chan = IEEE80211_FH_CHAN(frm[4], frm[5]);
				fhindex = frm[6];
			}
			break;
		case IEEE80211_ELEMID_DSPARMS:
			if (ic->ic_phytype == IEEE80211_T_DS)
				chan = frm[2];
			break;
		}
		frm += frm[1] + 2;
	}
	if (ssid == NULL || rates == NULL) {
		DPRINTF(("ieee80211_recv_beacon: ssid=%p, rates=%p, chan=%d\n",
		    ssid, rates, chan));
		return;
	}
	if (ssid[1] > IEEE80211_NWID_LEN) {
		DPRINTF(("ieee80211_recv_beacon: bad ssid len %d from %s\n",
		    ssid[1], ether_sprintf(wh->i_addr2)));
		return;
	}
	ni = ieee80211_find_node(ic, wh->i_addr2);
#ifdef IEEE80211_DEBUG
	if (ieee80211_debug &&
	    (ieee80211_debug > 1 || ni == NULL ||
	    ic->ic_state == IEEE80211_S_SCAN)) {
		printf("ieee80211_recv_prreq: %sbeacon on chan %u (bss chan %u) ",
		    (ni == NULL ? "new " : ""), chan, ic->ic_bss.ni_chan);
		ieee80211_print_essid(ssid + 2, ssid[1]);
		printf(" from %s\n", ether_sprintf(wh->i_addr2));
	}
#endif
	if (ni == NULL) {
		if ((ni = ieee80211_alloc_node(ic, wh->i_addr2, 0)) == NULL)
			return;
		ni->ni_esslen = ssid[1];
		memset(ni->ni_essid, 0, sizeof(ni->ni_essid));
		memcpy(ni->ni_essid, ssid + 2, ssid[1]);
	} else {
		if (ssid[1] != 0) {
			/*
			 * Update ESSID at probe response to adopt hidden AP by
			 * Lucent/Cisco, which announces null ESSID in beacon.
			 */
			if ((wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) ==
			    IEEE80211_FC0_SUBTYPE_PROBE_RESP) {
				ni->ni_esslen = ssid[1];
				memset(ni->ni_essid, 0, sizeof(ni->ni_essid));
				memcpy(ni->ni_essid, ssid + 2, ssid[1]);
			}
		}
	}
	IEEE80211_ADDR_COPY(ni->ni_bssid, wh->i_addr3);
	memset(ni->ni_rates, 0, IEEE80211_RATE_SIZE);
	ni->ni_nrate = rates[1];
	memcpy(ni->ni_rates, rates + 2, ni->ni_nrate);
	ieee80211_fix_rate(ic, ni, IEEE80211_F_DOSORT);
	ni->ni_rssi = rssi;
	ni->ni_rstamp = rstamp;
	memcpy(ni->ni_tstamp, tstamp, sizeof(ni->ni_tstamp));
	ni->ni_intval = le16toh(*(u_int16_t *)bintval);
	ni->ni_capinfo = le16toh(*(u_int16_t *)capinfo);
	ni->ni_chan = chan;
	ni->ni_fhdwell = fhdwell;
	ni->ni_fhindex = fhindex;
	if (ic->ic_state == IEEE80211_S_SCAN && ic->ic_scan_timer == 0)
		ieee80211_end_scan(&ic->ic_if);
}

static void
ieee80211_recv_prreq(struct ieee80211com *ic, struct mbuf *m0, int rssi,
    u_int32_t rstamp)
{
	struct ieee80211_frame *wh;
	struct ieee80211_node *ni;
	u_int8_t *frm, *efrm, *ssid, *rates;
	u_int8_t rate;
	int allocbs;

	if (ic->ic_opmode == IEEE80211_M_STA)
		return;
	if (ic->ic_state != IEEE80211_S_RUN)
		return;

	wh = mtod(m0, struct ieee80211_frame *);
	frm = (u_int8_t *)&wh[1];
	efrm = mtod(m0, u_int8_t *) + m0->m_len;
	/*
	 * prreq frame format
	 *	[tlv] ssid
	 *	[tlv] supported rates
	 */
	ssid = rates = NULL;
	while (frm < efrm) {
		switch (*frm) {
		case IEEE80211_ELEMID_SSID:
			ssid = frm;
			break;
		case IEEE80211_ELEMID_RATES:
			rates = frm;
			break;
		}
		frm += frm[1] + 2;
	}
	if (ssid == NULL || rates == NULL) {
		DPRINTF(("ieee80211_recv_prreq: ssid=%p, rates=%p\n",
		    ssid, rates));
		return;
	}
	if (ssid[1] != 0 &&
	    (ssid[1] != ic->ic_bss.ni_esslen ||
	    memcmp(ssid + 2, ic->ic_bss.ni_essid, ic->ic_bss.ni_esslen) != 0)) {
#ifdef IEEE80211_DEBUG
		if (ieee80211_debug) {
			printf("ieee80211_recv_prreq: ssid unmatch ");
			ieee80211_print_essid(ssid + 2, ssid[1]);
			printf(" from %s\n", ether_sprintf(wh->i_addr2));
		}
#endif
		return;
	}

	ni = ieee80211_find_node(ic, wh->i_addr2);
	if (ni == NULL) {
		if ((ni = ieee80211_alloc_node(ic, wh->i_addr2, 1)) == NULL)
			return;
		DPRINTF(("ieee80211_recv_prreq: new req from %s\n",
		    ether_sprintf(wh->i_addr2)));
		allocbs = 1;
	} else
		allocbs = 0;
	memset(ni->ni_rates, 0, IEEE80211_RATE_SIZE);
	ni->ni_nrate = rates[1];
	memcpy(ni->ni_rates, rates + 2, ni->ni_nrate);
	ni->ni_rssi = rssi;
	ni->ni_rstamp = rstamp;
	rate = ieee80211_fix_rate(ic, ni, IEEE80211_F_DOSORT |
	    IEEE80211_F_DOFRATE | IEEE80211_F_DONEGO | IEEE80211_F_DODEL);
	if (rate & IEEE80211_RATE_BASIC) {
		DPRINTF(("ieee80211_recv_prreq: rate negotiation failed: %s\n",
		    ether_sprintf(wh->i_addr2)));
	} else {
		IEEE80211_SEND_MGMT(ic, ni, IEEE80211_FC0_SUBTYPE_PROBE_RESP,
		    0);
	}
	if (allocbs && (ic->ic_opmode == IEEE80211_M_HOSTAP))
		ieee80211_free_node(ic, ni);
}

static void
ieee80211_recv_auth(struct ieee80211com *ic, struct mbuf *m0, int rssi,
    u_int32_t rstamp)
{
	struct ifnet *ifp = &ic->ic_if;
	struct ieee80211_frame *wh;
	struct ieee80211_node *ni;
	u_int8_t *frm, *efrm;
	u_int16_t algo, seq, status;
	int allocbs;

	wh = mtod(m0, struct ieee80211_frame *);
	frm = (u_int8_t *)&wh[1];
	efrm = mtod(m0, u_int8_t *) + m0->m_len;
	/*
	 * auth frame format
	 *	[2] algorithm
	 *	[2] sequence
	 *	[2] status
	 *	[tlv*] challenge
	 */
	if (frm + 6 > efrm) {
		DPRINTF(("ieee80211_recv_auth: too short from %s\n",
		    ether_sprintf(wh->i_addr2)));
		return;
	}
	algo   = le16toh(*(u_int16_t *)frm);
	seq    = le16toh(*(u_int16_t *)(frm + 2));
	status = le16toh(*(u_int16_t *)(frm + 4));
	if (algo != IEEE80211_AUTH_ALG_OPEN) {
		/* TODO: shared key auth */
		DPRINTF(("ieee80211_recv_auth: unsupported auth %d from %s\n",
		    algo, ether_sprintf(wh->i_addr2)));
		return;
	}
	switch (ic->ic_opmode) {
	case IEEE80211_M_IBSS:
		if (ic->ic_state != IEEE80211_S_RUN || seq != 1)
			return;
		ieee80211_new_state(&ic->ic_if, IEEE80211_S_AUTH,
		    wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK);
		break;

	case IEEE80211_M_AHDEMO:
		/* should not come here */
		break;

	case IEEE80211_M_HOSTAP:
		if (ic->ic_state != IEEE80211_S_RUN || seq != 1)
			return;
		allocbs = 0;
		ni = ieee80211_find_node(ic, wh->i_addr2);
		if (ni == NULL) {
			ni = ieee80211_alloc_node(ic, wh->i_addr2, 0);
			if (ni == NULL)
				return;
			IEEE80211_ADDR_COPY(ni->ni_bssid, ic->ic_bss.ni_bssid);
			allocbs = 1;
		}
		IEEE80211_SEND_MGMT(ic, ni, IEEE80211_FC0_SUBTYPE_AUTH, 2);
		if (ifp->if_flags & IFF_DEBUG)
			if_printf(ifp, "station %s %s authenticated\n",
			    (allocbs ? "newly" : "already"),
			    ether_sprintf(ni->ni_macaddr));
		break;

	case IEEE80211_M_STA:
		if (ic->ic_state != IEEE80211_S_AUTH || seq != 2)
			return;
		if (status != 0) {
			if_printf(&ic->ic_if,
			    "authentication failed (reason %d) for %s\n",
			    status,
			    ether_sprintf(wh->i_addr3));
			ni = ieee80211_find_node(ic, wh->i_addr2);
			if (ni != NULL)
				ni->ni_fails++;
			return;
		}
		ieee80211_new_state(&ic->ic_if, IEEE80211_S_ASSOC,
		    wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK);
		break;
	}
}

static void
ieee80211_recv_asreq(struct ieee80211com *ic, struct mbuf *m0, int rssi,
    u_int32_t rstamp)
{
	struct ifnet *ifp = &ic->ic_if;
	struct ieee80211_frame *wh;
	struct ieee80211_node *ni = &ic->ic_bss;
	u_int8_t *frm, *efrm, *ssid, *rates;
	u_int16_t capinfo, bintval;
	int reassoc, resp, newassoc;

	if (ic->ic_opmode != IEEE80211_M_HOSTAP ||
	    (ic->ic_state != IEEE80211_S_RUN))
		return;

	wh = mtod(m0, struct ieee80211_frame *);
	frm = (u_int8_t *)&wh[1];
	efrm = mtod(m0, u_int8_t *) + m0->m_len;
	if ((wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) ==
	    IEEE80211_FC0_SUBTYPE_REASSOC_REQ) {
		reassoc = 1;
		resp = IEEE80211_FC0_SUBTYPE_REASSOC_RESP;
	} else {
		reassoc = 0;
		resp = IEEE80211_FC0_SUBTYPE_ASSOC_RESP;
	}
	/*
	 * asreq frame format
	 *	[2] capability information
	 *	[2] listen interval
	 *	[6*] current AP address (reassoc only)
	 *	[tlv] ssid
	 *	[tlv] supported rates
	 */
	if (frm + (reassoc ? 10 : 4) > efrm) {
		DPRINTF(("ieee80211_recv_asreq: too short from %s\n",
		    ether_sprintf(wh->i_addr2)));
		return;
	}

	if (!IEEE80211_ADDR_EQ(wh->i_addr3, ic->ic_bss.ni_bssid)) {
		DPRINTF(("ieee80211_recv_asreq: ignore other bss from %s\n",
		    ether_sprintf(wh->i_addr2)));
		return;
	}
	capinfo = le16toh(*(u_int16_t *)frm);	frm += 2;
	bintval = le16toh(*(u_int16_t *)frm);	frm += 2;
	if (reassoc)
		frm += 6;	/* ignore current AP info */
	ssid = rates = NULL;
	while (frm < efrm) {
		switch (*frm) {
		case IEEE80211_ELEMID_SSID:
			ssid = frm;
			break;
		case IEEE80211_ELEMID_RATES:
			rates = frm;
			break;
		}
		frm += frm[1] + 2;
	}
	if (ssid == NULL || rates == NULL) {
		DPRINTF(("ieee80211_recv_asreq: ssid=%p, rates=%p\n",
		    ssid, rates));
		return;
	}
	if (ssid[1] > IEEE80211_NWID_LEN) {
		DPRINTF(("ieee80211_recv_asreq: bad ssid len %d from %s\n",
		    ssid[1], ether_sprintf(wh->i_addr2)));
		return;
	}
	if (ssid[1] != ic->ic_bss.ni_esslen ||
	    memcmp(ssid + 2, ic->ic_bss.ni_essid, ssid[1]) != 0) {
#ifdef IEEE80211_DEBUG
		if (ieee80211_debug) {
			printf("ieee80211_recv_asreq: ssid unmatch ");
			ieee80211_print_essid(ssid + 2, ssid[1]);
			printf(" from %s\n", ether_sprintf(wh->i_addr2));
		}
#endif
		return;
	}
	ni = ieee80211_find_node(ic, wh->i_addr2);
	if (ni == NULL) {
		DPRINTF(("ieee80211_recv_asreq: not authenticated for %s\n",
		    ether_sprintf(wh->i_addr2)));
		if ((ni = ieee80211_alloc_node(ic, wh->i_addr2, 1)) == NULL)
			return;
		IEEE80211_SEND_MGMT(ic, ni, IEEE80211_FC0_SUBTYPE_DEAUTH,
		    IEEE80211_REASON_ASSOC_NOT_AUTHED);
		ieee80211_free_node(ic, ni);
		return;
	}
	if ((capinfo & IEEE80211_CAPINFO_ESS) == 0 ||
	    (capinfo & IEEE80211_CAPINFO_PRIVACY) !=
	    ((ic->ic_flags & IEEE80211_F_WEPON) ?
	     IEEE80211_CAPINFO_PRIVACY : 0)) {
		DPRINTF(("ieee80211_recv_asreq: capability unmatch %x for %s\n",
		    capinfo, ether_sprintf(wh->i_addr2)));
		ni->ni_associd = 0;
		IEEE80211_SEND_MGMT(ic, ni, resp, IEEE80211_STATUS_CAPINFO);
		return;
	}
	memset(ni->ni_rates, 0, IEEE80211_RATE_SIZE);
	ni->ni_nrate = rates[1];
	memcpy(ni->ni_rates, rates + 2, ni->ni_nrate);
	ieee80211_fix_rate(ic, ni, IEEE80211_F_DOSORT | IEEE80211_F_DOFRATE |
	    IEEE80211_F_DONEGO | IEEE80211_F_DODEL);
	if (ni->ni_nrate == 0) {
		DPRINTF(("ieee80211_recv_asreq: rate unmatch for %s\n",
		    ether_sprintf(wh->i_addr2)));
		ni->ni_associd = 0;
		IEEE80211_SEND_MGMT(ic, ni, resp, IEEE80211_STATUS_BASIC_RATE);
		return;
	}
	ni->ni_rssi = rssi;
	ni->ni_rstamp = rstamp;
	ni->ni_intval = bintval;
	ni->ni_capinfo = capinfo;
	ni->ni_chan = ic->ic_bss.ni_chan;
	ni->ni_fhdwell = ic->ic_bss.ni_fhdwell;
	ni->ni_fhindex = ic->ic_bss.ni_fhindex;
	if (ni->ni_associd == 0) {
		ni->ni_associd = 0xc000 | ic->ic_bss.ni_associd++;
		newassoc = 1;
	} else
		newassoc = 0;
	IEEE80211_SEND_MGMT(ic, ni, resp, IEEE80211_STATUS_SUCCESS);
	if (ifp->if_flags & IFF_DEBUG)
		if_printf(ifp, "station %s %s associated\n",
		    (newassoc ? "newly" : "already"),
		    ether_sprintf(ni->ni_macaddr));
}

static void
ieee80211_recv_asresp(struct ieee80211com *ic, struct mbuf *m0, int rssi,
    u_int32_t rstamp)
{
	struct ifnet *ifp = &ic->ic_if;
	struct ieee80211_frame *wh;
	struct ieee80211_node *ni = &ic->ic_bss;
	u_int8_t *frm, *efrm, *rates;
	int status;

	if (ic->ic_opmode != IEEE80211_M_STA ||
	    ic->ic_state != IEEE80211_S_ASSOC)
		return;

	wh = mtod(m0, struct ieee80211_frame *);
	frm = (u_int8_t *)&wh[1];
	efrm = mtod(m0, u_int8_t *) + m0->m_len;
	/*
	 * asresp frame format
	 *	[2] capability information
	 *	[2] status
	 *	[2] association ID
	 *	[tlv] supported rates
	 */
	if (frm + 6 > efrm) {
		DPRINTF(("ieee80211_recv_asresp: too short from %s\n",
		    ether_sprintf(wh->i_addr2)));
		return;
	}

	ni->ni_capinfo = le16toh(*(u_int16_t *)frm);
	frm += 2;

	status = le16toh(*(u_int16_t *)frm);
	frm += 2;
	if (status != 0) {
		if_printf(ifp, "association failed (reason %d) for %s\n",
		    status, ether_sprintf(wh->i_addr3));
		ni = ieee80211_find_node(ic, wh->i_addr2);
		if (ni != NULL)
			ni->ni_fails++;
		return;
	}
	ni->ni_associd = le16toh(*(u_int16_t *)frm);
	frm += 2;
	rates = frm;

	memset(ni->ni_rates, 0, IEEE80211_RATE_SIZE);
	ni->ni_nrate = rates[1];
	memcpy(ni->ni_rates, rates + 2, ni->ni_nrate);
	ieee80211_fix_rate(ic, ni, IEEE80211_F_DOSORT | IEEE80211_F_DOFRATE |
	    IEEE80211_F_DONEGO | IEEE80211_F_DODEL);
	if (ni->ni_nrate == 0)
		return;
	ieee80211_new_state(ifp, IEEE80211_S_RUN,
	    wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK);
}

static void
ieee80211_recv_disassoc(struct ieee80211com *ic, struct mbuf *m0, int rssi,
    u_int32_t rstamp)
{
	struct ifnet *ifp = &ic->ic_if;
	struct ieee80211_frame *wh;
	struct ieee80211_node *ni;
	u_int8_t *frm, *efrm;
	u_int16_t reason;

	wh = mtod(m0, struct ieee80211_frame *);
	frm = (u_int8_t *)&wh[1];
	efrm = mtod(m0, u_int8_t *) + m0->m_len;
	/*
	 * disassoc frame format
	 *	[2] reason
	 */
	if (frm + 2 > efrm) {
		DPRINTF(("ieee80211_recv_disassoc: too short from %s\n",
		    ether_sprintf(wh->i_addr2)));
		return;
	}
	reason = le16toh(*(u_int16_t *)frm);
	switch (ic->ic_opmode) {
	case IEEE80211_M_STA:
		ieee80211_new_state(&ic->ic_if, IEEE80211_S_ASSOC,
		    wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK);
		break;
	case IEEE80211_M_HOSTAP:
		if ((ni = ieee80211_find_node(ic, wh->i_addr2)) != NULL) {
			if (ifp->if_flags & IFF_DEBUG)
				if_printf(ifp, "station %s disassociated"
				    " by peer (reason %d)\n",
				    ether_sprintf(ni->ni_macaddr), reason);
			ni->ni_associd = 0;
		}
		break;
	default:
		break;
	}
}

static void
ieee80211_recv_deauth(struct ieee80211com *ic, struct mbuf *m0, int rssi,
    u_int32_t rstamp)
{
	struct ifnet *ifp = &ic->ic_if;
	struct ieee80211_frame *wh;
	struct ieee80211_node *ni;
	u_int8_t *frm, *efrm;
	u_int16_t reason;

	wh = mtod(m0, struct ieee80211_frame *);
	frm = (u_int8_t *)&wh[1];
	efrm = mtod(m0, u_int8_t *) + m0->m_len;
	/*
	 * dauth frame format
	 *	[2] reason
	 */
	if (frm + 2 > efrm) {
		DPRINTF(("ieee80211_recv_deauth: too short from %s\n",
		    ether_sprintf(wh->i_addr2)));
		return;
	}
	reason = le16toh(*(u_int16_t *)frm);
	switch (ic->ic_opmode) {
	case IEEE80211_M_STA:
		ieee80211_new_state(&ic->ic_if, IEEE80211_S_AUTH,
		    wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK);
		break;
	case IEEE80211_M_HOSTAP:
		if ((ni = ieee80211_find_node(ic, wh->i_addr2)) != NULL) {
			if (ifp->if_flags & IFF_DEBUG)
				if_printf(ifp, "station %s deauthenticated"
				    " by peer (reason %d)\n",
				    ether_sprintf(ni->ni_macaddr), reason);
			ieee80211_free_node(ic, ni);
		}
		break;
	default:
		break;
	}
}

int
ieee80211_new_state(struct ifnet *ifp, enum ieee80211_state nstate, int mgt)
{
	struct ieee80211com *ic = (void *)ifp;
	struct ieee80211_node *ni = &ic->ic_bss;
	int i, error, ostate;
#ifdef IEEE80211_DEBUG
	static const char *stname[] = 
	    { "INIT", "SCAN", "AUTH", "ASSOC", "RUN" };
#endif

	ostate = ic->ic_state;
	DPRINTF(("ieee80211_new_state: %s -> %s\n",
	    stname[ostate], stname[nstate]));
	if (ic->ic_newstate) {
		error = (*ic->ic_newstate)(ic->ic_softc, nstate);
		if (error == EINPROGRESS)
			return 0;
		if (error != 0)
			return error;
	}

	/* state transition */
	ic->ic_state = nstate;
	switch (nstate) {
	case IEEE80211_S_INIT:
		switch (ostate) {
		case IEEE80211_S_INIT:
			break;
		case IEEE80211_S_RUN:
			switch (ic->ic_opmode) {
			case IEEE80211_M_STA:
				IEEE80211_SEND_MGMT(ic, ni,
				    IEEE80211_FC0_SUBTYPE_DISASSOC,
				    IEEE80211_REASON_ASSOC_LEAVE);
				break;
			case IEEE80211_M_HOSTAP:
				TAILQ_FOREACH(ni, &ic->ic_node, ni_list) {
					if (ni->ni_associd == 0)
						continue;
					IEEE80211_SEND_MGMT(ic, ni,
					    IEEE80211_FC0_SUBTYPE_DISASSOC,
					    IEEE80211_REASON_ASSOC_LEAVE);
				}
				break;
			default:
				break;
			}
			/* FALLTHRU */
		case IEEE80211_S_ASSOC:
			switch (ic->ic_opmode) {
			case IEEE80211_M_STA:
				IEEE80211_SEND_MGMT(ic, ni,
				    IEEE80211_FC0_SUBTYPE_DEAUTH,
				    IEEE80211_REASON_AUTH_LEAVE);
				break;
			case IEEE80211_M_HOSTAP:
				TAILQ_FOREACH(ni, &ic->ic_node, ni_list) {
					IEEE80211_SEND_MGMT(ic, ni,
					    IEEE80211_FC0_SUBTYPE_DEAUTH,
					    IEEE80211_REASON_AUTH_LEAVE);
				}
				break;
			default:
				break;
			}
			/* FALLTHRU */
		case IEEE80211_S_AUTH:
		case IEEE80211_S_SCAN:
			ic->ic_scan_timer = 0;
			ic->ic_mgt_timer = 0;
			IF_DRAIN(&ic->ic_mgtq);
			if (ic->ic_wep_ctx != NULL) {
				free(ic->ic_wep_ctx, M_DEVBUF);
				ic->ic_wep_ctx = NULL;
			}
			ieee80211_free_allnodes(ic);
			break;
		}
		break;
	case IEEE80211_S_SCAN:
		ic->ic_flags &= ~IEEE80211_F_SIBSS;
		ni = &ic->ic_bss;
		/* initialize bss for probe request */
		IEEE80211_ADDR_COPY(ni->ni_macaddr, ifp->if_broadcastaddr);
		IEEE80211_ADDR_COPY(ni->ni_bssid, ifp->if_broadcastaddr);
		ni->ni_nrate = 0;
		memset(ni->ni_rates, 0, IEEE80211_RATE_SIZE);
		for (i = 0; i < IEEE80211_RATE_SIZE; i++) {
			if (ic->ic_sup_rates[i] != 0)
				ni->ni_rates[ni->ni_nrate++] =
				    ic->ic_sup_rates[i];
		}
		ni->ni_associd = 0;
		ni->ni_rstamp = 0;
		switch (ostate) {
		case IEEE80211_S_INIT:
			ic->ic_flags |= IEEE80211_F_ASCAN;
			ic->ic_scan_timer = IEEE80211_ASCAN_WAIT;
			/* use lowest rate */
			ni->ni_txrate = 0;
			IEEE80211_SEND_MGMT(ic, ni,
			    IEEE80211_FC0_SUBTYPE_PROBE_REQ, 0);
			break;
		case IEEE80211_S_SCAN:
			/* scan next */
			if (ic->ic_flags & IEEE80211_F_ASCAN) {
				if (ic->ic_scan_timer == 0)
					ic->ic_scan_timer =
					    IEEE80211_ASCAN_WAIT;
				IEEE80211_SEND_MGMT(ic, ni,
				    IEEE80211_FC0_SUBTYPE_PROBE_REQ, 0);
			} else {
				if (ic->ic_scan_timer == 0)
					ic->ic_scan_timer =
					    IEEE80211_PSCAN_WAIT;
				ifp->if_timer = 1;
			}
			break;
		case IEEE80211_S_RUN:
			/* beacon miss */
			if (ifp->if_flags & IFF_DEBUG) {
				/* XXX bssid clobbered above */
				if_printf(ifp, "no recent beacons from %s;"
				    " rescanning\n",
				    ether_sprintf(ic->ic_bss.ni_bssid));
			}
			ieee80211_free_allnodes(ic);
			/* FALLTHRU */
		case IEEE80211_S_AUTH:
		case IEEE80211_S_ASSOC:
			/* timeout restart scan */
			ni = ieee80211_find_node(ic, ic->ic_bss.ni_macaddr);
			if (ni != NULL)
				ni->ni_fails++;
			ic->ic_flags |= IEEE80211_F_ASCAN;
			ic->ic_scan_timer = IEEE80211_ASCAN_WAIT;
			IEEE80211_SEND_MGMT(ic, &ic->ic_bss,
			    IEEE80211_FC0_SUBTYPE_PROBE_REQ, 0);
			break;
		}
		break;
	case IEEE80211_S_AUTH:
		switch (ostate) {
		case IEEE80211_S_INIT:
			DPRINTF(("ieee80211_new_state: invalid transition\n"));
			break;
		case IEEE80211_S_SCAN:
			IEEE80211_SEND_MGMT(ic, ni,
			    IEEE80211_FC0_SUBTYPE_AUTH, 1);
			break;
		case IEEE80211_S_AUTH:
		case IEEE80211_S_ASSOC:
			switch (mgt) {
			case IEEE80211_FC0_SUBTYPE_AUTH:
				/* ??? */
				IEEE80211_SEND_MGMT(ic, ni,
				    IEEE80211_FC0_SUBTYPE_AUTH, 2);
				break;
			case IEEE80211_FC0_SUBTYPE_DEAUTH:
				/* ignore and retry scan on timeout */
				break;
			}
			break;
		case IEEE80211_S_RUN:
			switch (mgt) {
			case IEEE80211_FC0_SUBTYPE_AUTH:
				IEEE80211_SEND_MGMT(ic, ni,
				    IEEE80211_FC0_SUBTYPE_AUTH, 2);
				ic->ic_state = ostate;	/* stay RUN */
				break;
			case IEEE80211_FC0_SUBTYPE_DEAUTH:
				/* try to reauth */
				IEEE80211_SEND_MGMT(ic, ni,
				    IEEE80211_FC0_SUBTYPE_AUTH, 1);
				break;
			}
			break;
		}
		break;
	case IEEE80211_S_ASSOC:
		switch (ostate) {
		case IEEE80211_S_INIT:
		case IEEE80211_S_SCAN:
		case IEEE80211_S_ASSOC:
			DPRINTF(("ieee80211_new_state: invalid transition\n"));
			break;
		case IEEE80211_S_AUTH:
			IEEE80211_SEND_MGMT(ic, ni,
			    IEEE80211_FC0_SUBTYPE_ASSOC_REQ, 0);
			break;
		case IEEE80211_S_RUN:
			IEEE80211_SEND_MGMT(ic, ni,
			    IEEE80211_FC0_SUBTYPE_ASSOC_REQ, 1);
			break;
		}
		break;
	case IEEE80211_S_RUN:
		switch (ostate) {
		case IEEE80211_S_INIT:
		case IEEE80211_S_AUTH:
		case IEEE80211_S_RUN:
			DPRINTF(("ieee80211_new_state: invalid transition\n"));
			break;
		case IEEE80211_S_SCAN:		/* adhoc mode */
		case IEEE80211_S_ASSOC:		/* infra mode */
			if (ifp->if_flags & IFF_DEBUG) {
				if_printf(ifp, " ");
				if (ic->ic_opmode == IEEE80211_M_STA)
					printf("associated ");
				else
					printf("synchronized ");
				printf("with %s ssid ",
				    ether_sprintf(ic->ic_bss.ni_bssid));
				ieee80211_print_essid(ic->ic_bss.ni_essid,
				    ic->ic_bss.ni_esslen);
				printf(" channel %d\n", ic->ic_bss.ni_chan);
			}
			/* start with highest negotiated rate */
			ic->ic_bss.ni_txrate = ic->ic_bss.ni_nrate - 1;
			ic->ic_mgt_timer = 0;
			(*ifp->if_start)(ifp);
			break;
		}
		break;
	}
	return 0;
}

struct mbuf *
ieee80211_wep_crypt(struct ifnet *ifp, struct mbuf *m0, int txflag)
{
	struct ieee80211com *ic = (void *)ifp;
	struct mbuf *m, *n, *n0;
	struct ieee80211_frame *wh;
	int i, left, len, moff, noff, kid;
	u_int32_t iv, crc;
	u_int8_t *ivp;
	void *ctx;
	u_int8_t keybuf[IEEE80211_WEP_IVLEN + IEEE80211_KEYBUF_SIZE];
	u_int8_t crcbuf[IEEE80211_WEP_CRCLEN];

	n0 = NULL;
	if ((ctx = ic->ic_wep_ctx) == NULL) {
		ctx = malloc(arc4_ctxlen(), M_DEVBUF, M_NOWAIT);
		if (ctx == NULL)
			goto fail;
		ic->ic_wep_ctx = ctx;
	}
	m = m0;
	left = m->m_pkthdr.len;
	MGET(n, M_NOWAIT, m->m_type);
	n0 = n;
	if (n == NULL)
		goto fail;
	M_MOVE_PKTHDR(n, m);
	len = IEEE80211_WEP_IVLEN + IEEE80211_WEP_KIDLEN + IEEE80211_WEP_CRCLEN;
	if (txflag) {
		n->m_pkthdr.len += len;
	} else {
		n->m_pkthdr.len -= len;
		left -= len;
	}
	n->m_len = MHLEN;
	if (n->m_pkthdr.len >= MINCLSIZE) {
		MCLGET(n, M_NOWAIT);
		if (n->m_flags & M_EXT)
			n->m_len = n->m_ext.ext_size;
	}
	len = sizeof(struct ieee80211_frame);
	memcpy(mtod(n, caddr_t), mtod(m, caddr_t), len);
	wh = mtod(n, struct ieee80211_frame *);
	left -= len;
	moff = len;
	noff = len;
	if (txflag) {
		kid = ic->ic_wep_txkey;
		wh->i_fc[1] |= IEEE80211_FC1_WEP;
                iv = ic->ic_iv;
		/*
		 * Skip 'bad' IVs from Fluhrer/Mantin/Shamir:
		 * (B, 255, N) with 3 <= B < 8
		 */
		if (iv >= 0x03ff00 &&
		    (iv & 0xf8ff00) == 0x00ff00)
			iv += 0x000100;
		ic->ic_iv = iv + 1;
		/* put iv in little endian to prepare 802.11i */
		ivp = mtod(n, u_int8_t *) + noff;
		for (i = 0; i < IEEE80211_WEP_IVLEN; i++) {
			ivp[i] = iv & 0xff;
			iv >>= 8;
		}
		ivp[IEEE80211_WEP_IVLEN] = kid << 6;	/* pad and keyid */
		noff += IEEE80211_WEP_IVLEN + IEEE80211_WEP_KIDLEN;
	} else {
		wh->i_fc[1] &= ~IEEE80211_FC1_WEP;
		ivp = mtod(m, u_int8_t *) + moff;
		kid = ivp[IEEE80211_WEP_IVLEN] >> 6;
		moff += IEEE80211_WEP_IVLEN + IEEE80211_WEP_KIDLEN;
	}
	memcpy(keybuf, ivp, IEEE80211_WEP_IVLEN);
	memcpy(keybuf + IEEE80211_WEP_IVLEN, ic->ic_nw_keys[kid].wk_key,
	    ic->ic_nw_keys[kid].wk_len);
	arc4_setkey(ctx, keybuf,
	    IEEE80211_WEP_IVLEN + ic->ic_nw_keys[kid].wk_len);

	/* encrypt with calculating CRC */
	crc = ~0;
	while (left > 0) {
		len = m->m_len - moff;
		if (len == 0) {
			m = m->m_next;
			moff = 0;
			continue;
		}
		if (len > n->m_len - noff) {
			len = n->m_len - noff;
			if (len == 0) {
				MGET(n->m_next, M_NOWAIT, n->m_type);
				if (n->m_next == NULL)
					goto fail;
				n = n->m_next;
				n->m_len = MLEN;
				if (left >= MINCLSIZE) {
					MCLGET(n, M_NOWAIT);
					if (n->m_flags & M_EXT)
						n->m_len = n->m_ext.ext_size;
				}
				noff = 0;
				continue;
			}
		}
		if (len > left)
			len = left;
		arc4_encrypt(ctx, mtod(n, caddr_t) + noff,
		    mtod(m, caddr_t) + moff, len);
		if (txflag)
			crc = ieee80211_crc_update(crc,
			    mtod(m, u_int8_t *) + moff, len);
		else
			crc = ieee80211_crc_update(crc,
			    mtod(n, u_int8_t *) + noff, len);
		left -= len;
		moff += len;
		noff += len;
	}
	crc = ~crc;
	if (txflag) {
		*(u_int32_t *)crcbuf = htole32(crc);
		if (n->m_len >= noff + sizeof(crcbuf))
			n->m_len = noff + sizeof(crcbuf);
		else {
			n->m_len = noff;
			MGET(n->m_next, M_NOWAIT, n->m_type);
			if (n->m_next == NULL)
				goto fail;
			n = n->m_next;
			n->m_len = sizeof(crcbuf);
			noff = 0;
		}
		arc4_encrypt(ctx, mtod(n, caddr_t) + noff, crcbuf,
		    sizeof(crcbuf));
	} else {
		n->m_len = noff;
		for (noff = 0; noff < sizeof(crcbuf); noff += len) {
			len = sizeof(crcbuf) - noff;
			if (len > m->m_len - moff)
				len = m->m_len - moff;
			if (len > 0)
				arc4_encrypt(ctx, crcbuf + noff,
				    mtod(m, caddr_t) + moff, len);
			m = m->m_next;
			moff = 0;
		}
		if (crc != le32toh(*(u_int32_t *)crcbuf)) {
#ifdef IEEE80211_DEBUG
			if (ieee80211_debug) {
				if_printf(ifp, "decrypt CRC error\n");
				if (ieee80211_debug > 1)
					ieee80211_dump_pkt(n0->m_data,
					    n0->m_len, -1, -1);
			}
#endif
			goto fail;
		}
	}
	m_freem(m0);
	return n0;

  fail:
	m_freem(m0);
	m_freem(n0);
	return NULL;
}

/*
 * CRC 32 -- routine from RFC 2083
 */

/* Table of CRCs of all 8-bit messages */
static u_int32_t ieee80211_crc_table[256];

/* Make the table for a fast CRC. */
static void
ieee80211_crc_init(void)
{
	u_int32_t c;
	int n, k;

	for (n = 0; n < 256; n++) {
		c = (u_int32_t)n;
		for (k = 0; k < 8; k++) {
			if (c & 1)
				c = 0xedb88320UL ^ (c >> 1);
			else
				c = c >> 1;
		}
		ieee80211_crc_table[n] = c;
	}
}

/*
 * Update a running CRC with the bytes buf[0..len-1]--the CRC
 * should be initialized to all 1's, and the transmitted value
 * is the 1's complement of the final running CRC
 */

static u_int32_t
ieee80211_crc_update(u_int32_t crc, u_int8_t *buf, int len)
{
	u_int8_t *endbuf;

	for (endbuf = buf + len; buf < endbuf; buf++)
		crc = ieee80211_crc_table[(crc ^ *buf) & 0xff] ^ (crc >> 8);
	return crc;
}

/*
 * convert IEEE80211 rate value to ifmedia subtype.
 * ieee80211 rate is in unit of 0.5Mbps.
 */

int
ieee80211_rate2media(int rate, enum ieee80211_phytype phytype)
{
	int mword;

	mword = 0;
	switch (phytype) {
	case IEEE80211_T_FH:
		switch (rate & IEEE80211_RATE_VAL) {
		case 0:
			mword = IFM_AUTO;
			break;
		case 2:
			mword = IFM_IEEE80211_FH1;
			break;
		case 4:
			mword = IFM_IEEE80211_FH2;
			break;
		default:
			mword = IFM_NONE;
			break;
		}
		break;

	case IEEE80211_T_DS:
		switch (rate & IEEE80211_RATE_VAL) {
		case 0:
			mword = IFM_AUTO;
			break;
		case 2:
			mword = IFM_IEEE80211_DS1;
			break;
		case 4:
			mword = IFM_IEEE80211_DS2;
			break;
		case 11:
			mword = IFM_IEEE80211_DS5;
			break;
		case 22:
			mword = IFM_IEEE80211_DS11;
			break;
		default:
			mword = IFM_NONE;
			break;
		}
		break;

	case IEEE80211_T_OFDM:
		switch (rate & IEEE80211_RATE_VAL) {
		case 0:
			mword = IFM_AUTO;
			break;
		case 12:
			mword = IFM_IEEE80211_ODFM6;
			break;
		case 18:
			mword = IFM_IEEE80211_ODFM9;
			break;
		case 24:
			mword = IFM_IEEE80211_ODFM12;
			break;
		case 36:
			mword = IFM_IEEE80211_ODFM18;
			break;
		case 48:
			mword = IFM_IEEE80211_ODFM24;
			break;
		case 72:
			mword = IFM_IEEE80211_ODFM36;
			break;
		case 108:
			mword = IFM_IEEE80211_ODFM54;
			break;
		case 144:
			mword = IFM_IEEE80211_ODFM72;
			break;
		default:
			mword = IFM_NONE;
			break;
		}
		break;

	default:
		mword = IFM_MANUAL;
		break;
	}
	return mword;
}

int
ieee80211_media2rate(int mword, enum ieee80211_phytype phytype)
{
	int rate;

	rate = 0;
	switch (phytype) {
	case IEEE80211_T_FH:
		switch (IFM_SUBTYPE(mword)) {
		case IFM_IEEE80211_FH1:
			rate = 2;
			break;
		case IFM_IEEE80211_FH2:
			rate = 4;
			break;
		}
		break;

	case IEEE80211_T_DS:
		switch (IFM_SUBTYPE(mword)) {
		case IFM_IEEE80211_DS1:
			rate = 2;
			break;
		case IFM_IEEE80211_DS2:
			rate = 4;
			break;
		case IFM_IEEE80211_DS5:
			rate = 11;
			break;
		case IFM_IEEE80211_DS11:
			rate = 22;
			break;
		}
		break;

	default:
		break;
	}
	return rate;
}

/*
 * XXX
 * Wireless LAN specific configuration interface, which is compatible
 * with wiconfig(8).
 */

int
ieee80211_cfgget(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct ieee80211com *ic = (void *)ifp;
	int i, j, error;
	struct ifreq *ifr = (struct ifreq *)data;
	struct wi_req wreq;
	struct wi_ltv_keys *keys;
	struct wi_apinfo *ap;
	struct ieee80211_node *ni;
	struct wi_sigcache wsc;

	error = copyin(ifr->ifr_data, &wreq, sizeof(wreq));
	if (error)
		return error;
	wreq.wi_len = 0;
	switch (wreq.wi_type) {
	case WI_RID_SERIALNO:
		/* nothing appropriate */
		break;
	case WI_RID_NODENAME:
		strcpy((char *)&wreq.wi_val[1], hostname);
		wreq.wi_val[0] = htole16(strlen(hostname));
		wreq.wi_len = (1 + strlen(hostname) + 1) / 2;
		break;
	case WI_RID_CURRENT_SSID:
		if (ic->ic_state != IEEE80211_S_RUN) {
			wreq.wi_val[0] = 0;
			wreq.wi_len = 1;
			break;
		}
		wreq.wi_val[0] = htole16(ic->ic_bss.ni_esslen);
		memcpy(&wreq.wi_val[1], ic->ic_bss.ni_essid,
		    ic->ic_bss.ni_esslen);
		wreq.wi_len = (1 + ic->ic_bss.ni_esslen + 1) / 2;
		break;
	case WI_RID_OWN_SSID:
	case WI_RID_DESIRED_SSID:
		wreq.wi_val[0] = htole16(ic->ic_des_esslen);
		memcpy(&wreq.wi_val[1], ic->ic_des_essid, ic->ic_des_esslen);
		wreq.wi_len = (1 + ic->ic_des_esslen + 1) / 2;
		break;
	case WI_RID_CURRENT_BSSID:
		if (ic->ic_state == IEEE80211_S_RUN)
			IEEE80211_ADDR_COPY(wreq.wi_val, ic->ic_bss.ni_bssid);
		else
			memset(wreq.wi_val, 0, IEEE80211_ADDR_LEN);
		wreq.wi_len = IEEE80211_ADDR_LEN / 2;
		break;
	case WI_RID_CHANNEL_LIST:
		memset(wreq.wi_val, 0, sizeof(wreq.wi_val));
		/*
		 * Since channel 0 is not available for DS, channel 1
		 * is assigned to LSB on WaveLAN.
		 */
		if (ic->ic_phytype == IEEE80211_T_DS)
			i = 1;
		else
			i = 0;
		for (j = 0; i <= IEEE80211_CHAN_MAX; i++, j++) {
			if (isset(ic->ic_chan_active, i)) {
				setbit((u_int8_t *)wreq.wi_val, j);
				wreq.wi_len = j / 16 + 1;
			}
		}
		break;
	case WI_RID_OWN_CHNL:
		wreq.wi_val[0] = htole16(ic->ic_ibss_chan);
		wreq.wi_len = 1;
		break;
	case WI_RID_CURRENT_CHAN:
		wreq.wi_val[0] = htole16(ic->ic_bss.ni_chan);
		wreq.wi_len = 1;
		break;
	case WI_RID_COMMS_QUALITY:
		wreq.wi_val[0] = 0;				/* quality */
		wreq.wi_val[1] = htole16(ic->ic_bss.ni_rssi);	/* signal */
		wreq.wi_val[2] = 0;				/* noise */
		wreq.wi_len = 3;
		break;
	case WI_RID_PROMISC:
		wreq.wi_val[0] = htole16((ifp->if_flags & IFF_PROMISC) ? 1 : 0);
		wreq.wi_len = 1;
		break;
	case WI_RID_PORTTYPE:
		wreq.wi_val[0] = htole16(ic->ic_opmode);
		wreq.wi_len = 1;
		break;
	case WI_RID_MAC_NODE:
		IEEE80211_ADDR_COPY(wreq.wi_val, ic->ic_myaddr);
		wreq.wi_len = IEEE80211_ADDR_LEN / 2;
		break;
	case WI_RID_TX_RATE:
		if (ic->ic_fixed_rate == -1)
			wreq.wi_val[0] = 0;	/* auto */
		else
			wreq.wi_val[0] = htole16(
			    (ic->ic_sup_rates[ic->ic_fixed_rate] &
			    IEEE80211_RATE_VAL) / 2);
		wreq.wi_len = 1;
		break;
	case WI_RID_CUR_TX_RATE:
		wreq.wi_val[0] = htole16(
		    (ic->ic_bss.ni_rates[ic->ic_bss.ni_txrate] &
		    IEEE80211_RATE_VAL) / 2);
		wreq.wi_len = 1;
		break;
	case WI_RID_RTS_THRESH:
		wreq.wi_val[0] = htole16(IEEE80211_MAX_LEN);	/* TODO: RTS */
		wreq.wi_len = 1;
		break;
	case WI_RID_CREATE_IBSS:
		wreq.wi_val[0] =
		    htole16((ic->ic_flags & IEEE80211_F_IBSSON) ? 1 : 0);
		wreq.wi_len = 1;
		break;
	case WI_RID_MICROWAVE_OVEN:
		wreq.wi_val[0] = 0;	/* no ... not supported */
		wreq.wi_len = 1;
		break;
	case WI_RID_ROAMING_MODE:
		wreq.wi_val[0] = htole16(1);	/* enabled ... not supported */
		wreq.wi_len = 1;
		break;
	case WI_RID_SYSTEM_SCALE:
		wreq.wi_val[0] = htole16(1);	/* low density ... not supp */
		wreq.wi_len = 1;
		break;
	case WI_RID_PM_ENABLED:
		wreq.wi_val[0] =
		    htole16((ic->ic_flags & IEEE80211_F_PMGTON) ? 1 : 0);
		wreq.wi_len = 1;
		break;
	case WI_RID_MAX_SLEEP:
		wreq.wi_val[0] = htole16(ic->ic_lintval);
		wreq.wi_len = 1;
		break;
	case WI_RID_CUR_BEACON_INT:
		wreq.wi_val[0] = htole16(ic->ic_bss.ni_intval);
		wreq.wi_len = 1;
		break;
	case WI_RID_WEP_AVAIL:
		wreq.wi_val[0] =
		    htole16((ic->ic_flags & IEEE80211_F_HASWEP) ? 1 : 0);
		wreq.wi_len = 1;
		break;
	case WI_RID_CNFAUTHMODE:
		wreq.wi_val[0] = htole16(1);	/* TODO: open system only */
		wreq.wi_len = 1;
		break;
	case WI_RID_ENCRYPTION:
		wreq.wi_val[0] =
		    htole16((ic->ic_flags & IEEE80211_F_WEPON) ? 1 : 0);
		wreq.wi_len = 1;
		break;
	case WI_RID_TX_CRYPT_KEY:
		wreq.wi_val[0] = htole16(ic->ic_wep_txkey);
		wreq.wi_len = 1;
		break;
	case WI_RID_DEFLT_CRYPT_KEYS:
		keys = (struct wi_ltv_keys *)&wreq;
		/* do not show keys to non-root user */
		error = suser(curthread);
		if (error) {
			memset(keys, 0, sizeof(*keys));
			error = 0;
			break;
		}
		for (i = 0; i < IEEE80211_WEP_NKID; i++) {
			keys->wi_keys[i].wi_keylen =
			    htole16(ic->ic_nw_keys[i].wk_len);
			memcpy(keys->wi_keys[i].wi_keydat,
			    ic->ic_nw_keys[i].wk_key, ic->ic_nw_keys[i].wk_len);
		}
		wreq.wi_len = sizeof(*keys) / 2;
		break;
	case WI_RID_MAX_DATALEN:
		wreq.wi_val[0] = htole16(IEEE80211_MAX_LEN);	/* TODO: frag */
		wreq.wi_len = 1;
		break;
	case WI_RID_IFACE_STATS:
		/* XXX: should be implemented in lower drivers */
		break;
	case WI_RID_READ_APS:
		if (ic->ic_opmode != IEEE80211_M_HOSTAP) {
			for (i = 0; i < IEEE80211_PSCAN_WAIT; i++) {
				tsleep((caddr_t)ic, PWAIT | PCATCH, "i80211",
				    hz);
				if (ic->ic_state != IEEE80211_S_SCAN ||
				    (ic->ic_flags & IEEE80211_F_SCANAP) == 0 ||
				    (ic->ic_flags & IEEE80211_F_ASCAN) == 0)
					break;
			}
			ic->ic_flags &= ~IEEE80211_F_SCANAP;
			memcpy(ic->ic_chan_active, ic->ic_chan_avail,
			    sizeof(ic->ic_chan_active));
		}
		i = 0;
		ap = (void *)((char *)wreq.wi_val + sizeof(i));
		TAILQ_FOREACH(ni, &ic->ic_node, ni_list) {
			if ((caddr_t)(ap + 1) > (caddr_t)(&wreq + 1))
				break;
			memset(ap, 0, sizeof(*ap));
			if (ic->ic_opmode == IEEE80211_M_HOSTAP) {
				IEEE80211_ADDR_COPY(ap->bssid, ni->ni_macaddr);
				ap->namelen = ic->ic_des_esslen;
				if (ic->ic_des_esslen)
					memcpy(ap->name, ic->ic_des_essid,
					    ic->ic_des_esslen);
			} else {
				IEEE80211_ADDR_COPY(ap->bssid, ni->ni_bssid);
				ap->namelen = ni->ni_esslen;
				if (ni->ni_esslen)
					memcpy(ap->name, ni->ni_essid,
					    ni->ni_esslen);
			}
			ap->channel = ni->ni_chan;
			ap->signal = ni->ni_rssi;
			ap->capinfo = ni->ni_capinfo;
			ap->interval = ni->ni_intval;
			for (j = 0; j < ni->ni_nrate; j++) {
				if (ni->ni_rates[j] & IEEE80211_RATE_BASIC) {
					ap->rate = (ni->ni_rates[j] &
					    IEEE80211_RATE_VAL) * 5; /* XXX */
				}
			}
			i++;
			ap++;
		}
		memcpy(wreq.wi_val, &i, sizeof(i));
		wreq.wi_len = (sizeof(int) + sizeof(*ap) * i) / 2;
		break;
	case WI_RID_READ_CACHE:
		i = 0;
		TAILQ_FOREACH(ni, &ic->ic_node, ni_list) {
			if (i == (WI_MAX_DATALEN/sizeof(struct wi_sigcache))-1)
				break;
			IEEE80211_ADDR_COPY(wsc.macsrc, ni->ni_macaddr);
			memset(&wsc.ipsrc, 0, sizeof(wsc.ipsrc));
			wsc.signal = ni->ni_rssi;
			wsc.noise = 0;
			wsc.quality = 0;
			memcpy((caddr_t)wreq.wi_val + sizeof(wsc) * i,
			    &wsc, sizeof(wsc));
			i++;
		}
		wreq.wi_len = sizeof(wsc) * i / 2;
		break;
	case WI_RID_SCAN_APS:
		error = EINVAL;
		break;
	default:
		error = EINVAL;
		break;
	}
	if (error == 0) {
		wreq.wi_len++;
		error = copyout(&wreq, ifr->ifr_data, sizeof(wreq));
	}
	return error;
}

int
ieee80211_cfgset(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct ieee80211com *ic = (void *)ifp;
	int i, j, len, error;
	struct ifreq *ifr = (struct ifreq *)data;
	struct wi_ltv_keys *keys;
	struct wi_req wreq;
	u_char chanlist[roundup(IEEE80211_CHAN_MAX, NBBY)];

	error = copyin(ifr->ifr_data, &wreq, sizeof(wreq));
	if (error)
		return error;
	if (wreq.wi_len-- < 1)
		return EINVAL;
	switch (wreq.wi_type) {
	case WI_RID_SERIALNO:
	case WI_RID_NODENAME:
		return EPERM;
	case WI_RID_CURRENT_SSID:
		return EPERM;
	case WI_RID_OWN_SSID:
	case WI_RID_DESIRED_SSID:
		len = le16toh(wreq.wi_val[0]);
		if (wreq.wi_len < (1 + len + 1) / 2)
			return EINVAL;
		if (len > IEEE80211_NWID_LEN)
			return EINVAL;
		ic->ic_des_esslen = len;
		memset(ic->ic_des_essid, 0, sizeof(ic->ic_des_essid));
		memcpy(ic->ic_des_essid, &wreq.wi_val[1], len);
		error = ENETRESET;
		break;
	case WI_RID_CURRENT_BSSID:
		return EPERM;
	case WI_RID_OWN_CHNL:
		if (wreq.wi_len != 1)
			return EINVAL;
		i = le16toh(wreq.wi_val[0]);
		if (i < 0 ||
		    i > IEEE80211_CHAN_MAX ||
		    isclr(ic->ic_chan_active, i))
			return EINVAL;
		ic->ic_ibss_chan = i;
		if (ic->ic_flags & IEEE80211_F_SIBSS)
			error = ENETRESET;
		break;
	case WI_RID_CURRENT_CHAN:
		return EPERM;
	case WI_RID_COMMS_QUALITY:
		return EPERM;
	case WI_RID_PROMISC:
		if (wreq.wi_len != 1)
			return EINVAL;
		if (ifp->if_flags & IFF_PROMISC) {
			if (wreq.wi_val[0] == 0) {
				ifp->if_flags &= ~IFF_PROMISC;
				error = ENETRESET;
			}
		} else {
			if (wreq.wi_val[0] != 0) {
				ifp->if_flags |= IFF_PROMISC;
				error = ENETRESET;
			}
		}
		break;
	case WI_RID_PORTTYPE:
		if (wreq.wi_len != 1)
			return EINVAL;
		switch (le16toh(wreq.wi_val[0])) {
		case IEEE80211_M_STA:
			break;
		case IEEE80211_M_IBSS:
			if (!(ic->ic_flags & IEEE80211_F_HASIBSS))
				return EINVAL;
			break;
		case IEEE80211_M_AHDEMO:
			if (ic->ic_phytype != IEEE80211_T_DS ||
			    !(ic->ic_flags & IEEE80211_F_HASAHDEMO))
				return EINVAL;
			break;
		case IEEE80211_M_HOSTAP:
			if (!(ic->ic_flags & IEEE80211_F_HASHOSTAP))
				return EINVAL;
			break;
		default:
			return EINVAL;
		}
		if (le16toh(wreq.wi_val[0]) != ic->ic_opmode) {
			ic->ic_opmode = le16toh(wreq.wi_val[0]);
			error = ENETRESET;
		}
		break;
#if 0
	case WI_RID_MAC_NODE:
		if (wreq.wi_len != IEEE80211_ADDR_LEN / 2)
			return EINVAL;
		IEEE80211_ADDR_COPY(LLADDR(ifp->if_sadl), wreq.wi_val);
		/* if_init will copy lladdr into ic_myaddr */
		error = ENETRESET;
		break;
#endif
	case WI_RID_TX_RATE:
		if (wreq.wi_len != 1)
			return EINVAL;
		if (wreq.wi_val[0] == 0) {
			/* auto */
			ic->ic_fixed_rate = -1;
			break;
		}
		for (i = 0; i < IEEE80211_RATE_SIZE; i++) {
			if (le16toh(wreq.wi_val[0]) ==
			    (ic->ic_sup_rates[i] & IEEE80211_RATE_VAL) / 2)
				break;
		}
		if (i == IEEE80211_RATE_SIZE)
			return EINVAL;
		ic->ic_fixed_rate = i;
		error = ENETRESET;
		break;
	case WI_RID_CUR_TX_RATE:
		return EPERM;
		break;
	case WI_RID_RTS_THRESH:
		if (wreq.wi_len != 1)
			return EINVAL;
		if (le16toh(wreq.wi_val[0]) != IEEE80211_MAX_LEN)
			return EINVAL;		/* TODO: RTS */
		break;
	case WI_RID_CREATE_IBSS:
		if (wreq.wi_len != 1)
			return EINVAL;
		if (wreq.wi_val[0] != 0) {
			if ((ic->ic_flags & IEEE80211_F_HASIBSS) == 0)
				return EINVAL;
			if ((ic->ic_flags & IEEE80211_F_IBSSON) == 0) {
				ic->ic_flags |= IEEE80211_F_IBSSON;
				if (ic->ic_opmode == IEEE80211_M_IBSS &&
				    ic->ic_state == IEEE80211_S_SCAN)
					error = ENETRESET;
			}
		} else {
			if (ic->ic_flags & IEEE80211_F_IBSSON) {
				ic->ic_flags &= ~IEEE80211_F_IBSSON;
				if (ic->ic_flags & IEEE80211_F_SIBSS) {
					ic->ic_flags &= ~IEEE80211_F_SIBSS;
					error = ENETRESET;
				}
			}
		}
		break;
	case WI_RID_MICROWAVE_OVEN:
		if (wreq.wi_len != 1)
			return EINVAL;
		if (wreq.wi_val[0] != 0)
			return EINVAL;		/* not supported */
		break;
	case WI_RID_ROAMING_MODE:
		if (wreq.wi_len != 1)
			return EINVAL;
		if (le16toh(wreq.wi_val[0]) != 1)
			return EINVAL;		/* not supported */
		break;
	case WI_RID_SYSTEM_SCALE:
		if (wreq.wi_len != 1)
			return EINVAL;
		if (le16toh(wreq.wi_val[0]) != 1)
			return EINVAL;		/* not supported */
		break;
	case WI_RID_PM_ENABLED:
		if (wreq.wi_len != 1)
			return EINVAL;
		if (wreq.wi_val[0] != 0) {
			if ((ic->ic_flags & IEEE80211_F_HASPMGT) == 0)
				return EINVAL;
			if ((ic->ic_flags & IEEE80211_F_PMGTON) == 0) {
				ic->ic_flags |= IEEE80211_F_PMGTON;
				error = ENETRESET;
			}
		} else {
			if (ic->ic_flags & IEEE80211_F_PMGTON) {
				ic->ic_flags &= ~IEEE80211_F_PMGTON;
				error = ENETRESET;
			}
		}
		break;
	case WI_RID_MAX_SLEEP:
		if (wreq.wi_len != 1)
			return EINVAL;
		ic->ic_lintval = le16toh(wreq.wi_val[0]);
		if (ic->ic_flags & IEEE80211_F_PMGTON)
			error = ENETRESET;
		break;
	case WI_RID_CUR_BEACON_INT:
		return EPERM;
	case WI_RID_WEP_AVAIL:
		return EPERM;
	case WI_RID_CNFAUTHMODE:
		if (wreq.wi_len != 1)
			return EINVAL;
		if (le16toh(wreq.wi_val[0]) != 1)
			return EINVAL;		/* TODO: shared key auth */
		break;
	case WI_RID_ENCRYPTION:
		if (wreq.wi_len != 1)
			return EINVAL;
		if (wreq.wi_val[0] != 0) {
			if ((ic->ic_flags & IEEE80211_F_HASWEP) == 0)
				return EINVAL;
			if ((ic->ic_flags & IEEE80211_F_WEPON) == 0) {
				ic->ic_flags |= IEEE80211_F_WEPON;
				error = ENETRESET;
			}
		} else {
			if (ic->ic_flags & IEEE80211_F_WEPON) {
				ic->ic_flags &= ~IEEE80211_F_WEPON;
				error = ENETRESET;
			}
		}
		break;
	case WI_RID_TX_CRYPT_KEY:
		if (wreq.wi_len != 1)
			return EINVAL;
		i = le16toh(wreq.wi_val[0]);
		if (i >= IEEE80211_WEP_NKID)
			return EINVAL;
		ic->ic_wep_txkey = i;
		break;
	case WI_RID_DEFLT_CRYPT_KEYS:
		if (wreq.wi_len != sizeof(struct wi_ltv_keys) / 2)
			return EINVAL;
		keys = (struct wi_ltv_keys *)&wreq;
		for (i = 0; i < IEEE80211_WEP_NKID; i++) {
			len = le16toh(keys->wi_keys[i].wi_keylen);
			if (len != 0 && len < IEEE80211_WEP_KEYLEN)
				return EINVAL;
			if (len > sizeof(ic->ic_nw_keys[i].wk_key))
				return EINVAL;
		}
		memset(ic->ic_nw_keys, 0, sizeof(ic->ic_nw_keys));
		for (i = 0; i < IEEE80211_WEP_NKID; i++) {
			len = le16toh(keys->wi_keys[i].wi_keylen);
			ic->ic_nw_keys[i].wk_len = len;
			memcpy(ic->ic_nw_keys[i].wk_key,
			    keys->wi_keys[i].wi_keydat, len);
		}
		error = ENETRESET;
		break;
	case WI_RID_MAX_DATALEN:
		if (wreq.wi_len != 1)
			return EINVAL;
		len = le16toh(wreq.wi_val[0]);
		if (len < 350 /* ? */ || len > IEEE80211_MAX_LEN)
			return EINVAL;
		if (len != IEEE80211_MAX_LEN)
			return EINVAL;		/* TODO: fragment */
		break;
	case WI_RID_IFACE_STATS:
		error = EPERM;
		break;
	case WI_RID_SCAN_APS:
		if (ic->ic_opmode == IEEE80211_M_HOSTAP)
			break;
		wreq.wi_len -= 2;	/* XXX: tx rate? */
		/* FALLTHRU */
	case WI_RID_CHANNEL_LIST:
		memset(chanlist, 0, sizeof(chanlist));
		/*
		 * Since channel 0 is not available for DS, channel 1
		 * is assigned to LSB on WaveLAN.
		 */
		if (ic->ic_phytype == IEEE80211_T_DS)
			i = 1;
		else
			i = 0;
		for (j = 0; i <= IEEE80211_CHAN_MAX; i++, j++) {
			if (j / 16 >= wreq.wi_len)
				break;
			if (isclr((u_int8_t *)wreq.wi_val, j))
				continue;
			if (isclr(ic->ic_chan_avail, i)) {
				if (wreq.wi_type == WI_RID_CHANNEL_LIST)
					error = EPERM;
				else
					continue;
			}
			setbit(chanlist, i);
		}
		if (error == EPERM && ic->ic_chancheck != NULL)
			error = (*ic->ic_chancheck)(ic->ic_softc, chanlist);
		if (error)
			return error;
		memcpy(ic->ic_chan_active, chanlist,
		    sizeof(ic->ic_chan_active));
		if (isclr(chanlist, ic->ic_ibss_chan)) {
			for (i = 0; i <= IEEE80211_CHAN_MAX; i++)
				if (isset(chanlist, i)) {
					ic->ic_ibss_chan = i;
					break;
				}
		}
		if (isclr(chanlist, ic->ic_bss.ni_chan))
			ic->ic_bss.ni_chan = ic->ic_ibss_chan;
		if (wreq.wi_type == WI_RID_CHANNEL_LIST)
			error = ENETRESET;
		else {
			ic->ic_flags |= IEEE80211_F_SCANAP;
			error = ieee80211_new_state(ifp, IEEE80211_S_SCAN, -1);
		}
		break;
	default:
		error = EINVAL;
		break;
	}
	return error;
}

/*
 * Module glue.
 *
 * NB: the module name is "wlan" for compatibility with NetBSD.
 */

static int
ieee80211_modevent(module_t mod, int type, void *unused)
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

static moduledata_t ieee80211_mod = {
	"wlan",
	ieee80211_modevent,
	0
};
DECLARE_MODULE(wlan, ieee80211_mod, SI_SUB_DRIVERS, SI_ORDER_FIRST);
MODULE_VERSION(wlan, 1);
MODULE_DEPEND(wlan, rc4, 1, 1, 1);
