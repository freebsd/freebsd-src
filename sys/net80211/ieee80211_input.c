/*-
 * Copyright (c) 2001 Atsushi Onoe
 * Copyright (c) 2002-2007 Sam Leffler, Errno Consulting
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>   
#include <sys/malloc.h>
#include <sys/endian.h>
#include <sys/kernel.h>
 
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_media.h>
#include <net/ethernet.h>
#include <net/if_llc.h>
#include <net/if_vlan_var.h>

#include <net80211/ieee80211_var.h>

#include <net/bpf.h>

#ifdef IEEE80211_DEBUG
#include <machine/stdarg.h>

/*
 * Decide if a received management frame should be
 * printed when debugging is enabled.  This filters some
 * of the less interesting frames that come frequently
 * (e.g. beacons).
 */
static __inline int
doprint(struct ieee80211com *ic, int subtype)
{
	switch (subtype) {
	case IEEE80211_FC0_SUBTYPE_BEACON:
		return (ic->ic_flags & IEEE80211_F_SCAN);
	case IEEE80211_FC0_SUBTYPE_PROBE_REQ:
		return (ic->ic_opmode == IEEE80211_M_IBSS);
	}
	return 1;
}

static const uint8_t *ieee80211_getbssid(struct ieee80211com *,
	const struct ieee80211_frame *);
#endif /* IEEE80211_DEBUG */

static struct mbuf *ieee80211_defrag(struct ieee80211com *,
	struct ieee80211_node *, struct mbuf *, int);
static struct mbuf *ieee80211_decap(struct ieee80211com *, struct mbuf *, int);
static void ieee80211_send_error(struct ieee80211com *, struct ieee80211_node *,
		const uint8_t *mac, int subtype, int arg);
static struct mbuf *ieee80211_decap_fastframe(struct ieee80211com *,
	struct ieee80211_node *, struct mbuf *);
static void ieee80211_recv_pspoll(struct ieee80211com *,
	struct ieee80211_node *, struct mbuf *);

/*
 * Process a received frame.  The node associated with the sender
 * should be supplied.  If nothing was found in the node table then
 * the caller is assumed to supply a reference to ic_bss instead.
 * The RSSI and a timestamp are also supplied.  The RSSI data is used
 * during AP scanning to select a AP to associate with; it can have
 * any units so long as values have consistent units and higher values
 * mean ``better signal''.  The receive timestamp is currently not used
 * by the 802.11 layer.
 */
int
ieee80211_input(struct ieee80211com *ic, struct mbuf *m,
	struct ieee80211_node *ni, int rssi, int noise, uint32_t rstamp)
{
#define	SEQ_LEQ(a,b)	((int)((a)-(b)) <= 0)
#define	HAS_SEQ(type)	((type & 0x4) == 0)
	struct ifnet *ifp = ic->ic_ifp;
	struct ieee80211_frame *wh;
	struct ieee80211_key *key;
	struct ether_header *eh;
	int hdrspace, need_tap;
	uint8_t dir, type, subtype, qos;
	uint8_t *bssid;
	uint16_t rxseq;

	if (m->m_flags & M_AMPDU) {
		/*
		 * Fastpath for A-MPDU reorder q resubmission.  Frames
		 * w/ M_AMPDU marked have already passed through here
		 * but were received out of order and been held on the
		 * reorder queue.  When resubmitted they are marked
		 * with the M_AMPDU flag and we can bypass most of the
		 * normal processing.
		 */
		wh = mtod(m, struct ieee80211_frame *);
		type = IEEE80211_FC0_TYPE_DATA;
		dir = wh->i_fc[1] & IEEE80211_FC1_DIR_MASK;
		subtype = IEEE80211_FC0_SUBTYPE_QOS;
		hdrspace = ieee80211_hdrspace(ic, wh);	/* XXX optimize? */
		need_tap = 0;
		goto resubmit_ampdu;
	}

	KASSERT(ni != NULL, ("null node"));
	ni->ni_inact = ni->ni_inact_reload;

	need_tap = 1;			/* mbuf need to be tapped. */
	type = -1;			/* undefined */
	/*
	 * In monitor mode, send everything directly to bpf.
	 * XXX may want to include the CRC
	 */
	if (ic->ic_opmode == IEEE80211_M_MONITOR)
		goto out;

	if (m->m_pkthdr.len < sizeof(struct ieee80211_frame_min)) {
		IEEE80211_DISCARD_MAC(ic, IEEE80211_MSG_ANY,
		    ni->ni_macaddr, NULL,
		    "too short (1): len %u", m->m_pkthdr.len);
		ic->ic_stats.is_rx_tooshort++;
		goto out;
	}
	/*
	 * Bit of a cheat here, we use a pointer for a 3-address
	 * frame format but don't reference fields past outside
	 * ieee80211_frame_min w/o first validating the data is
	 * present.
	 */
	wh = mtod(m, struct ieee80211_frame *);

	if ((wh->i_fc[0] & IEEE80211_FC0_VERSION_MASK) !=
	    IEEE80211_FC0_VERSION_0) {
		IEEE80211_DISCARD_MAC(ic, IEEE80211_MSG_ANY,
		    ni->ni_macaddr, NULL, "wrong version %x", wh->i_fc[0]);
		ic->ic_stats.is_rx_badversion++;
		goto err;
	}

	dir = wh->i_fc[1] & IEEE80211_FC1_DIR_MASK;
	type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
	subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;
	if ((ic->ic_flags & IEEE80211_F_SCAN) == 0) {
		switch (ic->ic_opmode) {
		case IEEE80211_M_STA:
			bssid = wh->i_addr2;
			if (!IEEE80211_ADDR_EQ(bssid, ni->ni_bssid)) {
				/* not interested in */
				IEEE80211_DISCARD_MAC(ic, IEEE80211_MSG_INPUT,
				    bssid, NULL, "%s", "not to bss");
				ic->ic_stats.is_rx_wrongbss++;
				goto out;
			}
			break;
		case IEEE80211_M_IBSS:
		case IEEE80211_M_AHDEMO:
		case IEEE80211_M_HOSTAP:
			if (dir != IEEE80211_FC1_DIR_NODS)
				bssid = wh->i_addr1;
			else if (type == IEEE80211_FC0_TYPE_CTL)
				bssid = wh->i_addr1;
			else {
				if (m->m_pkthdr.len < sizeof(struct ieee80211_frame)) {
					IEEE80211_DISCARD_MAC(ic,
					    IEEE80211_MSG_ANY, ni->ni_macaddr,
					    NULL, "too short (2): len %u",
					    m->m_pkthdr.len);
					ic->ic_stats.is_rx_tooshort++;
					goto out;
				}
				bssid = wh->i_addr3;
			}
			if (type != IEEE80211_FC0_TYPE_DATA)
				break;
			/*
			 * Data frame, validate the bssid.
			 */
			if (!IEEE80211_ADDR_EQ(bssid, ic->ic_bss->ni_bssid) &&
			    !IEEE80211_ADDR_EQ(bssid, ifp->if_broadcastaddr)) {
				/* not interested in */
				IEEE80211_DISCARD_MAC(ic, IEEE80211_MSG_INPUT,
				    bssid, NULL, "%s", "not to bss");
				ic->ic_stats.is_rx_wrongbss++;
				goto out;
			}
			/*
			 * For adhoc mode we cons up a node when it doesn't
			 * exist. This should probably done after an ACL check.
			 */
			if (ni == ic->ic_bss &&
			    ic->ic_opmode != IEEE80211_M_HOSTAP &&
			    !IEEE80211_ADDR_EQ(wh->i_addr2, ni->ni_macaddr)) {
				/*
				 * Fake up a node for this newly
				 * discovered member of the IBSS.
				 */
				ni = ieee80211_fakeup_adhoc_node(&ic->ic_sta,
						    wh->i_addr2);
				if (ni == NULL) {
					/* NB: stat kept for alloc failure */
					goto err;
				}
			}
			break;
		default:
			goto out;
		}
		ni->ni_rssi = rssi;
		ni->ni_noise = noise;
		ni->ni_rstamp = rstamp;
		if (HAS_SEQ(type)) {
			uint8_t tid;
			if (IEEE80211_QOS_HAS_SEQ(wh)) {
				tid = ((struct ieee80211_qosframe *)wh)->
					i_qos[0] & IEEE80211_QOS_TID;
				if (TID_TO_WME_AC(tid) >= WME_AC_VI)
					ic->ic_wme.wme_hipri_traffic++;
				tid++;
			} else
				tid = IEEE80211_NONQOS_TID;
			rxseq = le16toh(*(uint16_t *)wh->i_seq);
			if ((ni->ni_flags & IEEE80211_NODE_HT) == 0 &&
			    (wh->i_fc[1] & IEEE80211_FC1_RETRY) &&
			    SEQ_LEQ(rxseq, ni->ni_rxseqs[tid])) {
				/* duplicate, discard */
				IEEE80211_DISCARD_MAC(ic, IEEE80211_MSG_INPUT,
				    bssid, "duplicate",
				    "seqno <%u,%u> fragno <%u,%u> tid %u",
				    rxseq >> IEEE80211_SEQ_SEQ_SHIFT,
				    ni->ni_rxseqs[tid] >>
					IEEE80211_SEQ_SEQ_SHIFT,
				    rxseq & IEEE80211_SEQ_FRAG_MASK,
				    ni->ni_rxseqs[tid] &
					IEEE80211_SEQ_FRAG_MASK,
				    tid);
				ic->ic_stats.is_rx_dup++;
				IEEE80211_NODE_STAT(ni, rx_dup);
				goto out;
			}
			ni->ni_rxseqs[tid] = rxseq;
		}
	}

	switch (type) {
	case IEEE80211_FC0_TYPE_DATA:
		hdrspace = ieee80211_hdrspace(ic, wh);
		if (m->m_len < hdrspace &&
		    (m = m_pullup(m, hdrspace)) == NULL) {
			IEEE80211_DISCARD_MAC(ic, IEEE80211_MSG_ANY,
			    ni->ni_macaddr, NULL,
			    "data too short: expecting %u", hdrspace);
			ic->ic_stats.is_rx_tooshort++;
			goto out;		/* XXX */
		}
		switch (ic->ic_opmode) {
		case IEEE80211_M_STA:
			if (dir != IEEE80211_FC1_DIR_FROMDS) {
				IEEE80211_DISCARD(ic, IEEE80211_MSG_INPUT,
				    wh, "data", "unknown dir 0x%x", dir);
				ic->ic_stats.is_rx_wrongdir++;
				goto out;
			}
			if ((ifp->if_flags & IFF_SIMPLEX) &&
			    IEEE80211_IS_MULTICAST(wh->i_addr1) &&
			    IEEE80211_ADDR_EQ(wh->i_addr3, ic->ic_myaddr)) {
				/*
				 * In IEEE802.11 network, multicast packet
				 * sent from me is broadcasted from AP.
				 * It should be silently discarded for
				 * SIMPLEX interface.
				 */
				IEEE80211_DISCARD(ic, IEEE80211_MSG_INPUT,
				    wh, NULL, "%s", "multicast echo");
				ic->ic_stats.is_rx_mcastecho++;
				goto out;
			}
			break;
		case IEEE80211_M_IBSS:
		case IEEE80211_M_AHDEMO:
			if (dir != IEEE80211_FC1_DIR_NODS) {
				IEEE80211_DISCARD(ic, IEEE80211_MSG_INPUT,
				    wh, "data", "unknown dir 0x%x", dir);
				ic->ic_stats.is_rx_wrongdir++;
				goto out;
			}
			/* XXX no power-save support */
			break;
		case IEEE80211_M_HOSTAP:
			if (dir != IEEE80211_FC1_DIR_TODS) {
				IEEE80211_DISCARD(ic, IEEE80211_MSG_INPUT,
				    wh, "data", "unknown dir 0x%x", dir);
				ic->ic_stats.is_rx_wrongdir++;
				goto out;
			}
			/* check if source STA is associated */
			if (ni == ic->ic_bss) {
				IEEE80211_DISCARD(ic, IEEE80211_MSG_INPUT,
				    wh, "data", "%s", "unknown src");
				ieee80211_send_error(ic, ni, wh->i_addr2,
				    IEEE80211_FC0_SUBTYPE_DEAUTH,
				    IEEE80211_REASON_NOT_AUTHED);
				ic->ic_stats.is_rx_notassoc++;
				goto err;
			}
			if (ni->ni_associd == 0) {
				IEEE80211_DISCARD(ic, IEEE80211_MSG_INPUT,
				    wh, "data", "%s", "unassoc src");
				IEEE80211_SEND_MGMT(ic, ni,
				    IEEE80211_FC0_SUBTYPE_DISASSOC,
				    IEEE80211_REASON_NOT_ASSOCED);
				ic->ic_stats.is_rx_notassoc++;
				goto err;
			}

			/*
			 * Check for power save state change.
			 * XXX out-of-order A-MPDU frames?
			 */
			if (((wh->i_fc[1] & IEEE80211_FC1_PWR_MGT) ^
			    (ni->ni_flags & IEEE80211_NODE_PWR_MGT)))
				ieee80211_node_pwrsave(ni,
					wh->i_fc[1] & IEEE80211_FC1_PWR_MGT);
			break;
		default:
			/* XXX here to keep compiler happy */
			goto out;
		}

		/*
		 * Handle A-MPDU re-ordering.  The station must be
		 * associated and negotiated HT.  The frame must be
		 * a QoS frame (not QoS null data) and not previously
		 * processed for A-MPDU re-ordering.  If the frame is
		 * to be processed directly then ieee80211_ampdu_reorder
		 * will return 0; otherwise it has consumed the mbuf
		 * and we should do nothing more with it.
		 */
		if ((ni->ni_flags & IEEE80211_NODE_HT) &&
		    subtype == IEEE80211_FC0_SUBTYPE_QOS &&
		    ieee80211_ampdu_reorder(ni, m) != 0) {
			m = NULL;
			goto out;
		}
	resubmit_ampdu:

		/*
		 * Handle privacy requirements.  Note that we
		 * must not be preempted from here until after
		 * we (potentially) call ieee80211_crypto_demic;
		 * otherwise we may violate assumptions in the
		 * crypto cipher modules used to do delayed update
		 * of replay sequence numbers.
		 */
		if (wh->i_fc[1] & IEEE80211_FC1_WEP) {
			if ((ic->ic_flags & IEEE80211_F_PRIVACY) == 0) {
				/*
				 * Discard encrypted frames when privacy is off.
				 */
				IEEE80211_DISCARD(ic, IEEE80211_MSG_INPUT,
				    wh, "WEP", "%s", "PRIVACY off");
				ic->ic_stats.is_rx_noprivacy++;
				IEEE80211_NODE_STAT(ni, rx_noprivacy);
				goto out;
			}
			key = ieee80211_crypto_decap(ic, ni, m, hdrspace);
			if (key == NULL) {
				/* NB: stats+msgs handled in crypto_decap */
				IEEE80211_NODE_STAT(ni, rx_wepfail);
				goto out;
			}
			wh = mtod(m, struct ieee80211_frame *);
			wh->i_fc[1] &= ~IEEE80211_FC1_WEP;
		} else {
			key = NULL;
		}

		/*
		 * Save QoS bits for use below--before we strip the header.
		 */
		if (subtype == IEEE80211_FC0_SUBTYPE_QOS) {
			qos = (dir == IEEE80211_FC1_DIR_DSTODS) ?
			    ((struct ieee80211_qosframe_addr4 *)wh)->i_qos[0] :
			    ((struct ieee80211_qosframe *)wh)->i_qos[0];
		} else
			qos = 0;

		/*
		 * Next up, any fragmentation.
		 */
		if (!IEEE80211_IS_MULTICAST(wh->i_addr1)) {
			m = ieee80211_defrag(ic, ni, m, hdrspace);
			if (m == NULL) {
				/* Fragment dropped or frame not complete yet */
				goto out;
			}
		}
		wh = NULL;		/* no longer valid, catch any uses */

		/*
		 * Next strip any MSDU crypto bits.
		 */
		if (key != NULL && !ieee80211_crypto_demic(ic, key, m, 0)) {
			IEEE80211_DISCARD_MAC(ic, IEEE80211_MSG_INPUT,
			    ni->ni_macaddr, "data", "%s", "demic error");
			ic->ic_stats.is_rx_demicfail++;
			IEEE80211_NODE_STAT(ni, rx_demicfail);
			goto out;
		}

		/* copy to listener after decrypt */
		if (bpf_peers_present(ic->ic_rawbpf))
			bpf_mtap(ic->ic_rawbpf, m);
		need_tap = 0;

		/*
		 * Finally, strip the 802.11 header.
		 */
		m = ieee80211_decap(ic, m, hdrspace);
		if (m == NULL) {
			/* don't count Null data frames as errors */
			if (subtype == IEEE80211_FC0_SUBTYPE_NODATA ||
			    subtype == IEEE80211_FC0_SUBTYPE_QOS_NULL)
				goto out;
			IEEE80211_DISCARD_MAC(ic, IEEE80211_MSG_INPUT,
			    ni->ni_macaddr, "data", "%s", "decap error");
			ic->ic_stats.is_rx_decap++;
			IEEE80211_NODE_STAT(ni, rx_decap);
			goto err;
		}
		eh = mtod(m, struct ether_header *);
		if (!ieee80211_node_is_authorized(ni)) {
			/*
			 * Deny any non-PAE frames received prior to
			 * authorization.  For open/shared-key
			 * authentication the port is mark authorized
			 * after authentication completes.  For 802.1x
			 * the port is not marked authorized by the
			 * authenticator until the handshake has completed.
			 */
			if (eh->ether_type != htons(ETHERTYPE_PAE)) {
				IEEE80211_DISCARD_MAC(ic, IEEE80211_MSG_INPUT,
				    eh->ether_shost, "data",
				    "unauthorized port: ether type 0x%x len %u",
				    eh->ether_type, m->m_pkthdr.len);
				ic->ic_stats.is_rx_unauth++;
				IEEE80211_NODE_STAT(ni, rx_unauth);
				goto err;
			}
		} else {
			/*
			 * When denying unencrypted frames, discard
			 * any non-PAE frames received without encryption.
			 */
			if ((ic->ic_flags & IEEE80211_F_DROPUNENC) &&
			    key == NULL &&
			    eh->ether_type != htons(ETHERTYPE_PAE)) {
				/*
				 * Drop unencrypted frames.
				 */
				ic->ic_stats.is_rx_unencrypted++;
				IEEE80211_NODE_STAT(ni, rx_unencrypted);
				goto out;
			}
		}
		/* XXX require HT? */
		if (qos & IEEE80211_QOS_AMSDU) {
			m = ieee80211_decap_amsdu(ni, m);
			if (m == NULL)
				return IEEE80211_FC0_TYPE_DATA;
		} else if ((ni->ni_ath_flags & IEEE80211_NODE_FF) &&
#define	FF_LLC_SIZE	(sizeof(struct ether_header) + sizeof(struct llc))
		    m->m_pkthdr.len >= 3*FF_LLC_SIZE) {
			struct llc *llc;

			/*
			 * Check for fast-frame tunnel encapsulation.
			 */
			if (m->m_len < FF_LLC_SIZE &&
			    (m = m_pullup(m, FF_LLC_SIZE)) == NULL) {
				IEEE80211_DISCARD_MAC(ic, IEEE80211_MSG_ANY,
				    ni->ni_macaddr, "fast-frame",
				    "%s", "m_pullup(llc) failed");
				ic->ic_stats.is_rx_tooshort++;
				return IEEE80211_FC0_TYPE_DATA;
			}
			llc = (struct llc *)(mtod(m, uint8_t *) + 
				sizeof(struct ether_header));
			if (llc->llc_snap.ether_type == htons(ATH_FF_ETH_TYPE)) {
				m_adj(m, FF_LLC_SIZE);
				m = ieee80211_decap_fastframe(ic, ni, m);
				if (m == NULL)
					return IEEE80211_FC0_TYPE_DATA;
			}
		}
#undef FF_LLC_SIZE
		ieee80211_deliver_data(ic, ni, m);
		return IEEE80211_FC0_TYPE_DATA;

	case IEEE80211_FC0_TYPE_MGT:
		ic->ic_stats.is_rx_mgmt++;
		IEEE80211_NODE_STAT(ni, rx_mgmt);
		if (dir != IEEE80211_FC1_DIR_NODS) {
			IEEE80211_DISCARD(ic, IEEE80211_MSG_INPUT,
			    wh, "data", "unknown dir 0x%x", dir);
			ic->ic_stats.is_rx_wrongdir++;
			goto err;
		}
		if (m->m_pkthdr.len < sizeof(struct ieee80211_frame)) {
			IEEE80211_DISCARD_MAC(ic, IEEE80211_MSG_ANY,
			    ni->ni_macaddr, "mgt", "too short: len %u",
			    m->m_pkthdr.len);
			ic->ic_stats.is_rx_tooshort++;
			goto out;
		}
#ifdef IEEE80211_DEBUG
		if ((ieee80211_msg_debug(ic) && doprint(ic, subtype)) ||
		    ieee80211_msg_dumppkts(ic)) {
			if_printf(ic->ic_ifp, "received %s from %s rssi %d\n",
			    ieee80211_mgt_subtype_name[subtype >>
				IEEE80211_FC0_SUBTYPE_SHIFT],
			    ether_sprintf(wh->i_addr2), rssi);
		}
#endif
		if (wh->i_fc[1] & IEEE80211_FC1_WEP) {
			if (subtype != IEEE80211_FC0_SUBTYPE_AUTH) {
				/*
				 * Only shared key auth frames with a challenge
				 * should be encrypted, discard all others.
				 */
				IEEE80211_DISCARD(ic, IEEE80211_MSG_INPUT,
				    wh, ieee80211_mgt_subtype_name[subtype >>
					IEEE80211_FC0_SUBTYPE_SHIFT],
				    "%s", "WEP set but not permitted");
				ic->ic_stats.is_rx_mgtdiscard++; /* XXX */
				goto out;
			}
			if ((ic->ic_flags & IEEE80211_F_PRIVACY) == 0) {
				/*
				 * Discard encrypted frames when privacy is off.
				 */
				IEEE80211_DISCARD(ic, IEEE80211_MSG_INPUT,
				    wh, "mgt", "%s", "WEP set but PRIVACY off");
				ic->ic_stats.is_rx_noprivacy++;
				goto out;
			}
			hdrspace = ieee80211_hdrspace(ic, wh);
			key = ieee80211_crypto_decap(ic, ni, m, hdrspace);
			if (key == NULL) {
				/* NB: stats+msgs handled in crypto_decap */
				goto out;
			}
			wh = mtod(m, struct ieee80211_frame *);
			wh->i_fc[1] &= ~IEEE80211_FC1_WEP;
		}
		if (bpf_peers_present(ic->ic_rawbpf))
			bpf_mtap(ic->ic_rawbpf, m);
		(*ic->ic_recv_mgmt)(ic, m, ni, subtype, rssi, noise, rstamp);
		m_freem(m);
		return IEEE80211_FC0_TYPE_MGT;

	case IEEE80211_FC0_TYPE_CTL:
		ic->ic_stats.is_rx_ctl++;
		IEEE80211_NODE_STAT(ni, rx_ctrl);
		if (ic->ic_opmode == IEEE80211_M_HOSTAP) {
			switch (subtype) {
			case IEEE80211_FC0_SUBTYPE_PS_POLL:
				ieee80211_recv_pspoll(ic, ni, m);
				break;
			case IEEE80211_FC0_SUBTYPE_BAR:
				ieee80211_recv_bar(ni, m);
				break;
			}
		}
		goto out;
	default:
		IEEE80211_DISCARD(ic, IEEE80211_MSG_ANY,
		    wh, NULL, "bad frame type 0x%x", type);
		/* should not come here */
		break;
	}
err:
	ifp->if_ierrors++;
out:
	if (m != NULL) {
		if (bpf_peers_present(ic->ic_rawbpf) && need_tap)
			bpf_mtap(ic->ic_rawbpf, m);
		m_freem(m);
	}
	return type;
#undef SEQ_LEQ
}

/*
 * This function reassemble fragments.
 */
static struct mbuf *
ieee80211_defrag(struct ieee80211com *ic, struct ieee80211_node *ni,
	struct mbuf *m, int hdrspace)
{
	struct ieee80211_frame *wh = mtod(m, struct ieee80211_frame *);
	struct ieee80211_frame *lwh;
	uint16_t rxseq;
	uint8_t fragno;
	uint8_t more_frag = wh->i_fc[1] & IEEE80211_FC1_MORE_FRAG;
	struct mbuf *mfrag;

	KASSERT(!IEEE80211_IS_MULTICAST(wh->i_addr1), ("multicast fragm?"));

	rxseq = le16toh(*(uint16_t *)wh->i_seq);
	fragno = rxseq & IEEE80211_SEQ_FRAG_MASK;

	/* Quick way out, if there's nothing to defragment */
	if (!more_frag && fragno == 0 && ni->ni_rxfrag[0] == NULL)
		return m;

	/*
	 * Remove frag to insure it doesn't get reaped by timer.
	 */
	if (ni->ni_table == NULL) {
		/*
		 * Should never happen.  If the node is orphaned (not in
		 * the table) then input packets should not reach here.
		 * Otherwise, a concurrent request that yanks the table
		 * should be blocked by other interlocking and/or by first
		 * shutting the driver down.  Regardless, be defensive
		 * here and just bail
		 */
		/* XXX need msg+stat */
		m_freem(m);
		return NULL;
	}
	IEEE80211_NODE_LOCK(ni->ni_table);
	mfrag = ni->ni_rxfrag[0];
	ni->ni_rxfrag[0] = NULL;
	IEEE80211_NODE_UNLOCK(ni->ni_table);

	/*
	 * Validate new fragment is in order and
	 * related to the previous ones.
	 */
	if (mfrag != NULL) {
		uint16_t last_rxseq;

		lwh = mtod(mfrag, struct ieee80211_frame *);
		last_rxseq = le16toh(*(uint16_t *)lwh->i_seq);
		/* NB: check seq # and frag together */
		if (rxseq != last_rxseq+1 ||
		    !IEEE80211_ADDR_EQ(wh->i_addr1, lwh->i_addr1) ||
		    !IEEE80211_ADDR_EQ(wh->i_addr2, lwh->i_addr2)) {
			/*
			 * Unrelated fragment or no space for it,
			 * clear current fragments.
			 */
			m_freem(mfrag);
			mfrag = NULL;
		}
	}

 	if (mfrag == NULL) {
		if (fragno != 0) {		/* !first fragment, discard */
			ic->ic_stats.is_rx_defrag++;
			IEEE80211_NODE_STAT(ni, rx_defrag);
			m_freem(m);
			return NULL;
		}
		mfrag = m;
	} else {				/* concatenate */
		m_adj(m, hdrspace);		/* strip header */
		m_cat(mfrag, m);
		/* NB: m_cat doesn't update the packet header */
		mfrag->m_pkthdr.len += m->m_pkthdr.len;
		/* track last seqnum and fragno */
		lwh = mtod(mfrag, struct ieee80211_frame *);
		*(uint16_t *) lwh->i_seq = *(uint16_t *) wh->i_seq;
	}
	if (more_frag) {			/* more to come, save */
		ni->ni_rxfragstamp = ticks;
		ni->ni_rxfrag[0] = mfrag;
		mfrag = NULL;
	}
	return mfrag;
}

void
ieee80211_deliver_data(struct ieee80211com *ic,
	struct ieee80211_node *ni, struct mbuf *m)
{
	struct ether_header *eh = mtod(m, struct ether_header *);
	struct ifnet *ifp = ic->ic_ifp;

	/*
	 * Do accounting.
	 */
	ifp->if_ipackets++;
	IEEE80211_NODE_STAT(ni, rx_data);
	IEEE80211_NODE_STAT_ADD(ni, rx_bytes, m->m_pkthdr.len);
	if (ETHER_IS_MULTICAST(eh->ether_dhost)) {
		m->m_flags |= M_MCAST;		/* XXX M_BCAST? */
		IEEE80211_NODE_STAT(ni, rx_mcast);
	} else
		IEEE80211_NODE_STAT(ni, rx_ucast);

	/* perform as a bridge within the AP */
	if (ic->ic_opmode == IEEE80211_M_HOSTAP &&
	    (ic->ic_flags & IEEE80211_F_NOBRIDGE) == 0) {
		struct mbuf *m1 = NULL;

		if (m->m_flags & M_MCAST) {
			m1 = m_dup(m, M_DONTWAIT);
			if (m1 == NULL)
				ifp->if_oerrors++;
			else
				m1->m_flags |= M_MCAST;
		} else {
			/*
			 * Check if the destination is known; if so
			 * and the port is authorized dispatch directly.
			 */
			struct ieee80211_node *sta =
			    ieee80211_find_node(&ic->ic_sta, eh->ether_dhost);
			if (sta != NULL) {
				if (ieee80211_node_is_authorized(sta)) {
					/*
					 * Beware of sending to ourself; this
					 * needs to happen via the normal
					 * input path.
					 */
					if (sta != ic->ic_bss) {
						m1 = m;
						m = NULL;
					}
				} else {
					ic->ic_stats.is_rx_unauth++;
					IEEE80211_NODE_STAT(sta, rx_unauth);
				}
				ieee80211_free_node(sta);
			}
		}
		if (m1 != NULL)
			(void) IF_HANDOFF(&ifp->if_snd, m1, ifp);
	}
	if (m != NULL) {
		m->m_pkthdr.rcvif = ifp;
		if (ni->ni_vlan != 0) {
			/* attach vlan tag */
			m->m_pkthdr.ether_vtag = ni->ni_vlan;
			m->m_flags |= M_VLANTAG;
		}
		(*ifp->if_input)(ifp, m);
	}
}

static struct mbuf *
ieee80211_decap(struct ieee80211com *ic, struct mbuf *m, int hdrlen)
{
	struct ieee80211_qosframe_addr4 wh;	/* Max size address frames */
	struct ether_header *eh;
	struct llc *llc;

	if (m->m_len < hdrlen + sizeof(*llc) &&
	    (m = m_pullup(m, hdrlen + sizeof(*llc))) == NULL) {
		/* XXX stat, msg */
		return NULL;
	}
	memcpy(&wh, mtod(m, caddr_t), hdrlen);
	llc = (struct llc *)(mtod(m, caddr_t) + hdrlen);
	if (llc->llc_dsap == LLC_SNAP_LSAP && llc->llc_ssap == LLC_SNAP_LSAP &&
	    llc->llc_control == LLC_UI && llc->llc_snap.org_code[0] == 0 &&
	    llc->llc_snap.org_code[1] == 0 && llc->llc_snap.org_code[2] == 0) {
		m_adj(m, hdrlen + sizeof(struct llc) - sizeof(*eh));
		llc = NULL;
	} else {
		m_adj(m, hdrlen - sizeof(*eh));
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
		IEEE80211_ADDR_COPY(eh->ether_dhost, wh.i_addr3);
		IEEE80211_ADDR_COPY(eh->ether_shost, wh.i_addr4);
		break;
	}
#ifdef ALIGNED_POINTER
	if (!ALIGNED_POINTER(mtod(m, caddr_t) + sizeof(*eh), uint32_t)) {
		struct mbuf *n, *n0, **np;
		caddr_t newdata;
		int off, pktlen;

		n0 = NULL;
		np = &n0;
		off = 0;
		pktlen = m->m_pkthdr.len;
		while (pktlen > off) {
			if (n0 == NULL) {
				MGETHDR(n, M_DONTWAIT, MT_DATA);
				if (n == NULL) {
					m_freem(m);
					return NULL;
				}
				M_MOVE_PKTHDR(n, m);
				n->m_len = MHLEN;
			} else {
				MGET(n, M_DONTWAIT, MT_DATA);
				if (n == NULL) {
					m_freem(m);
					m_freem(n0);
					return NULL;
				}
				n->m_len = MLEN;
			}
			if (pktlen - off >= MINCLSIZE) {
				MCLGET(n, M_DONTWAIT);
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
#endif /* ALIGNED_POINTER */
	if (llc != NULL) {
		eh = mtod(m, struct ether_header *);
		eh->ether_type = htons(m->m_pkthdr.len - sizeof(*eh));
	}
	return m;
}

/*
 * Decap a frame encapsulated in a fast-frame/A-MSDU.
 */
struct mbuf *
ieee80211_decap1(struct mbuf *m, int *framelen)
{
#define	FF_LLC_SIZE	(sizeof(struct ether_header) + sizeof(struct llc))
	struct ether_header *eh;
	struct llc *llc;

	/*
	 * The frame has an 802.3 header followed by an 802.2
	 * LLC header.  The encapsulated frame length is in the
	 * first header type field; save that and overwrite it 
	 * with the true type field found in the second.  Then
	 * copy the 802.3 header up to where it belongs and
	 * adjust the mbuf contents to remove the void.
	 */
	if (m->m_len < FF_LLC_SIZE && (m = m_pullup(m, FF_LLC_SIZE)) == NULL)
		return NULL;
	eh = mtod(m, struct ether_header *);	/* 802.3 header is first */
	llc = (struct llc *)&eh[1];		/* 802.2 header follows */
	*framelen = ntohs(eh->ether_type)	/* encap'd frame size */
		  + sizeof(struct ether_header) - sizeof(struct llc);
	eh->ether_type = llc->llc_un.type_snap.ether_type;
	ovbcopy(eh, mtod(m, uint8_t *) + sizeof(struct llc),
		sizeof(struct ether_header));
	m_adj(m, sizeof(struct llc));
	return m;
#undef FF_LLC_SIZE
}

/*
 * Decap the encapsulated frame pair and dispatch the first
 * for delivery.  The second frame is returned for delivery
 * via the normal path.
 */
static struct mbuf *
ieee80211_decap_fastframe(struct ieee80211com *ic,
	struct ieee80211_node *ni, struct mbuf *m)
{
#define	MS(x,f)	(((x) & f) >> f##_S)
	uint32_t ath;
	struct mbuf *n;
	int framelen;

	m_copydata(m, 0, sizeof(uint32_t), (caddr_t) &ath);
	if (MS(ath, ATH_FF_PROTO) != ATH_FF_PROTO_L2TUNNEL) {
		IEEE80211_DISCARD_MAC(ic, IEEE80211_MSG_ANY,
		    ni->ni_macaddr, "fast-frame",
		    "unsupport tunnel protocol, header 0x%x", ath);
		ic->ic_stats.is_ff_badhdr++;
		m_freem(m);
		return NULL;
	}
	/* NB: skip header and alignment padding */
	m_adj(m, roundup(sizeof(uint32_t) - 2, 4) + 2);

	ic->ic_stats.is_ff_decap++;

	/*
	 * Decap the first frame, bust it apart from the
	 * second and deliver; then decap the second frame
	 * and return it to the caller for normal delivery.
	 */
	m = ieee80211_decap1(m, &framelen);
	if (m == NULL) {
		IEEE80211_DISCARD_MAC(ic, IEEE80211_MSG_ANY,
		    ni->ni_macaddr, "fast-frame", "%s", "first decap failed");
		ic->ic_stats.is_ff_tooshort++;
		return NULL;
	}
	n = m_split(m, framelen, M_NOWAIT);
	if (n == NULL) {
		IEEE80211_DISCARD_MAC(ic, IEEE80211_MSG_ANY,
		    ni->ni_macaddr, "fast-frame",
		    "%s", "unable to split encapsulated frames");
		ic->ic_stats.is_ff_split++;
		m_freem(m);			/* NB: must reclaim */
		return NULL;
	}
	ieee80211_deliver_data(ic, ni, m);	/* 1st of pair */

	/*
	 * Decap second frame.
	 */
	m_adj(n, roundup2(framelen, 4) - framelen);	/* padding */
	n = ieee80211_decap1(n, &framelen);
	if (n == NULL) {
		IEEE80211_DISCARD_MAC(ic, IEEE80211_MSG_ANY,
		    ni->ni_macaddr, "fast-frame", "%s", "second decap failed");
		ic->ic_stats.is_ff_tooshort++;
	}
	/* XXX verify framelen against mbuf contents */
	return n;				/* 2nd delivered by caller */
#undef MS
}

/*
 * Install received rate set information in the node's state block.
 */
int
ieee80211_setup_rates(struct ieee80211_node *ni,
	const uint8_t *rates, const uint8_t *xrates, int flags)
{
	struct ieee80211com *ic = ni->ni_ic;
	struct ieee80211_rateset *rs = &ni->ni_rates;

	memset(rs, 0, sizeof(*rs));
	rs->rs_nrates = rates[1];
	memcpy(rs->rs_rates, rates + 2, rs->rs_nrates);
	if (xrates != NULL) {
		uint8_t nxrates;
		/*
		 * Tack on 11g extended supported rate element.
		 */
		nxrates = xrates[1];
		if (rs->rs_nrates + nxrates > IEEE80211_RATE_MAXSIZE) {
			nxrates = IEEE80211_RATE_MAXSIZE - rs->rs_nrates;
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_XRATE,
			     "[%s] extended rate set too large;"
			     " only using %u of %u rates\n",
			     ether_sprintf(ni->ni_macaddr), nxrates, xrates[1]);
			ic->ic_stats.is_rx_rstoobig++;
		}
		memcpy(rs->rs_rates + rs->rs_nrates, xrates+2, nxrates);
		rs->rs_nrates += nxrates;
	}
	return ieee80211_fix_rate(ni, rs, flags);
}

static void
ieee80211_auth_open(struct ieee80211com *ic, struct ieee80211_frame *wh,
    struct ieee80211_node *ni, int rssi, int noise, uint32_t rstamp,
    uint16_t seq, uint16_t status)
{

	if (ni->ni_authmode == IEEE80211_AUTH_SHARED) {
		IEEE80211_DISCARD_MAC(ic, IEEE80211_MSG_AUTH,
		    ni->ni_macaddr, "open auth",
		    "bad sta auth mode %u", ni->ni_authmode);
		ic->ic_stats.is_rx_bad_auth++;	/* XXX */
		if (ic->ic_opmode == IEEE80211_M_HOSTAP) {
			/*
			 * Clear any challenge text that may be there if
			 * a previous shared key auth failed and then an
			 * open auth is attempted.
			 */
			if (ni->ni_challenge != NULL) {
				FREE(ni->ni_challenge, M_80211_NODE);
				ni->ni_challenge = NULL;
			}
			/* XXX hack to workaround calling convention */
			ieee80211_send_error(ic, ni, wh->i_addr2, 
			    IEEE80211_FC0_SUBTYPE_AUTH,
			    (seq + 1) | (IEEE80211_STATUS_ALG<<16));
		}
		return;
	}
	switch (ic->ic_opmode) {
	case IEEE80211_M_IBSS:
	case IEEE80211_M_AHDEMO:
	case IEEE80211_M_MONITOR:
	case IEEE80211_M_WDS:
		/* should not come here */
		IEEE80211_DISCARD_MAC(ic, IEEE80211_MSG_AUTH,
		    ni->ni_macaddr, "open auth",
		    "bad operating mode %u", ic->ic_opmode);
		break;

	case IEEE80211_M_HOSTAP:
		if (ic->ic_state != IEEE80211_S_RUN ||
		    seq != IEEE80211_AUTH_OPEN_REQUEST) {
			ic->ic_stats.is_rx_bad_auth++;
			return;
		}
		/* always accept open authentication requests */
		if (ni == ic->ic_bss) {
			ni = ieee80211_dup_bss(&ic->ic_sta, wh->i_addr2);
			if (ni == NULL)
				return;
		} else if ((ni->ni_flags & IEEE80211_NODE_AREF) == 0)
			(void) ieee80211_ref_node(ni);
		/*
		 * Mark the node as referenced to reflect that it's
		 * reference count has been bumped to insure it remains
		 * after the transaction completes.
		 */
		ni->ni_flags |= IEEE80211_NODE_AREF;

		IEEE80211_SEND_MGMT(ic, ni,
			IEEE80211_FC0_SUBTYPE_AUTH, seq + 1);
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_DEBUG | IEEE80211_MSG_AUTH,
		    "[%s] station authenticated (open)\n",
		    ether_sprintf(ni->ni_macaddr));
		/*
		 * When 802.1x is not in use mark the port
		 * authorized at this point so traffic can flow.
		 */
		if (ni->ni_authmode != IEEE80211_AUTH_8021X)
			ieee80211_node_authorize(ni);
		break;

	case IEEE80211_M_STA:
		if (ic->ic_state != IEEE80211_S_AUTH ||
		    seq != IEEE80211_AUTH_OPEN_RESPONSE) {
			ic->ic_stats.is_rx_bad_auth++;
			return;
		}
		if (status != 0) {
			IEEE80211_DPRINTF(ic,
			    IEEE80211_MSG_DEBUG | IEEE80211_MSG_AUTH,
			    "[%s] open auth failed (reason %d)\n",
			    ether_sprintf(ni->ni_macaddr), status);
			/* XXX can this happen? */
			if (ni != ic->ic_bss)
				ni->ni_fails++;
			ic->ic_stats.is_rx_auth_fail++;
			ieee80211_new_state(ic, IEEE80211_S_SCAN,
			    IEEE80211_SCAN_FAIL_STATUS);
		} else
			ieee80211_new_state(ic, IEEE80211_S_ASSOC, 0);
		break;
	}
}

/*
 * Send a management frame error response to the specified
 * station.  If ni is associated with the station then use
 * it; otherwise allocate a temporary node suitable for
 * transmitting the frame and then free the reference so
 * it will go away as soon as the frame has been transmitted.
 */
static void
ieee80211_send_error(struct ieee80211com *ic, struct ieee80211_node *ni,
	const uint8_t *mac, int subtype, int arg)
{
	int istmp;

	if (ni == ic->ic_bss) {
		ni = ieee80211_tmp_node(ic, mac);
		if (ni == NULL) {
			/* XXX msg */
			return;
		}
		istmp = 1;
	} else
		istmp = 0;
	IEEE80211_SEND_MGMT(ic, ni, subtype, arg);
	if (istmp)
		ieee80211_free_node(ni);
}

static int
alloc_challenge(struct ieee80211com *ic, struct ieee80211_node *ni)
{
	if (ni->ni_challenge == NULL)
		MALLOC(ni->ni_challenge, uint32_t*, IEEE80211_CHALLENGE_LEN,
		    M_80211_NODE, M_NOWAIT);
	if (ni->ni_challenge == NULL) {
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_DEBUG | IEEE80211_MSG_AUTH,
		    "[%s] shared key challenge alloc failed\n",
		    ether_sprintf(ni->ni_macaddr));
		/* XXX statistic */
	}
	return (ni->ni_challenge != NULL);
}

/* XXX TODO: add statistics */
static void
ieee80211_auth_shared(struct ieee80211com *ic, struct ieee80211_frame *wh,
    uint8_t *frm, uint8_t *efrm, struct ieee80211_node *ni,
    int rssi, int noise, uint32_t rstamp, uint16_t seq, uint16_t status)
{
	uint8_t *challenge;
	int allocbs, estatus;

	/*
	 * NB: this can happen as we allow pre-shared key
	 * authentication to be enabled w/o wep being turned
	 * on so that configuration of these can be done
	 * in any order.  It may be better to enforce the
	 * ordering in which case this check would just be
	 * for sanity/consistency.
	 */
	if ((ic->ic_flags & IEEE80211_F_PRIVACY) == 0) {
		IEEE80211_DISCARD_MAC(ic, IEEE80211_MSG_AUTH,
		    ni->ni_macaddr, "shared key auth",
		    "%s", " PRIVACY is disabled");
		estatus = IEEE80211_STATUS_ALG;
		goto bad;
	}
	/*
	 * Pre-shared key authentication is evil; accept
	 * it only if explicitly configured (it is supported
	 * mainly for compatibility with clients like OS X).
	 */
	if (ni->ni_authmode != IEEE80211_AUTH_AUTO &&
	    ni->ni_authmode != IEEE80211_AUTH_SHARED) {
		IEEE80211_DISCARD_MAC(ic, IEEE80211_MSG_AUTH,
		    ni->ni_macaddr, "shared key auth",
		    "bad sta auth mode %u", ni->ni_authmode);
		ic->ic_stats.is_rx_bad_auth++;	/* XXX maybe a unique error? */
		estatus = IEEE80211_STATUS_ALG;
		goto bad;
	}

	challenge = NULL;
	if (frm + 1 < efrm) {
		if ((frm[1] + 2) > (efrm - frm)) {
			IEEE80211_DISCARD_MAC(ic, IEEE80211_MSG_AUTH,
			    ni->ni_macaddr, "shared key auth",
			    "ie %d/%d too long",
			    frm[0], (frm[1] + 2) - (efrm - frm));
			ic->ic_stats.is_rx_bad_auth++;
			estatus = IEEE80211_STATUS_CHALLENGE;
			goto bad;
		}
		if (*frm == IEEE80211_ELEMID_CHALLENGE)
			challenge = frm;
		frm += frm[1] + 2;
	}
	switch (seq) {
	case IEEE80211_AUTH_SHARED_CHALLENGE:
	case IEEE80211_AUTH_SHARED_RESPONSE:
		if (challenge == NULL) {
			IEEE80211_DISCARD_MAC(ic, IEEE80211_MSG_AUTH,
			    ni->ni_macaddr, "shared key auth",
			    "%s", "no challenge");
			ic->ic_stats.is_rx_bad_auth++;
			estatus = IEEE80211_STATUS_CHALLENGE;
			goto bad;
		}
		if (challenge[1] != IEEE80211_CHALLENGE_LEN) {
			IEEE80211_DISCARD_MAC(ic, IEEE80211_MSG_AUTH,
			    ni->ni_macaddr, "shared key auth",
			    "bad challenge len %d", challenge[1]);
			ic->ic_stats.is_rx_bad_auth++;
			estatus = IEEE80211_STATUS_CHALLENGE;
			goto bad;
		}
	default:
		break;
	}
	switch (ic->ic_opmode) {
	case IEEE80211_M_MONITOR:
	case IEEE80211_M_AHDEMO:
	case IEEE80211_M_IBSS:
	case IEEE80211_M_WDS:
		IEEE80211_DISCARD_MAC(ic, IEEE80211_MSG_AUTH,
		    ni->ni_macaddr, "shared key auth",
		    "bad operating mode %u", ic->ic_opmode);
		return;
	case IEEE80211_M_HOSTAP:
		if (ic->ic_state != IEEE80211_S_RUN) {
			IEEE80211_DISCARD_MAC(ic, IEEE80211_MSG_AUTH,
			    ni->ni_macaddr, "shared key auth",
			    "bad state %u", ic->ic_state);
			estatus = IEEE80211_STATUS_ALG;	/* XXX */
			goto bad;
		}
		switch (seq) {
		case IEEE80211_AUTH_SHARED_REQUEST:
			if (ni == ic->ic_bss) {
				ni = ieee80211_dup_bss(&ic->ic_sta, wh->i_addr2);
				if (ni == NULL) {
					/* NB: no way to return an error */
					return;
				}
				allocbs = 1;
			} else {
				if ((ni->ni_flags & IEEE80211_NODE_AREF) == 0)
					(void) ieee80211_ref_node(ni);
				allocbs = 0;
			}
			/*
			 * Mark the node as referenced to reflect that it's
			 * reference count has been bumped to insure it remains
			 * after the transaction completes.
			 */
			ni->ni_flags |= IEEE80211_NODE_AREF;
			ni->ni_rssi = rssi;
			ni->ni_noise = noise;
			ni->ni_rstamp = rstamp;
			if (!alloc_challenge(ic, ni)) {
				/* NB: don't return error so they rexmit */
				return;
			}
			get_random_bytes(ni->ni_challenge,
				IEEE80211_CHALLENGE_LEN);
			IEEE80211_DPRINTF(ic,
				IEEE80211_MSG_DEBUG | IEEE80211_MSG_AUTH,
				"[%s] shared key %sauth request\n",
				ether_sprintf(ni->ni_macaddr),
				allocbs ? "" : "re");
			break;
		case IEEE80211_AUTH_SHARED_RESPONSE:
			if (ni == ic->ic_bss) {
				IEEE80211_DISCARD_MAC(ic, IEEE80211_MSG_AUTH,
				    ni->ni_macaddr, "shared key response",
				    "%s", "unknown station");
				/* NB: don't send a response */
				return;
			}
			if (ni->ni_challenge == NULL) {
				IEEE80211_DISCARD_MAC(ic, IEEE80211_MSG_AUTH,
				    ni->ni_macaddr, "shared key response",
				    "%s", "no challenge recorded");
				ic->ic_stats.is_rx_bad_auth++;
				estatus = IEEE80211_STATUS_CHALLENGE;
				goto bad;
			}
			if (memcmp(ni->ni_challenge, &challenge[2],
			           challenge[1]) != 0) {
				IEEE80211_DISCARD_MAC(ic, IEEE80211_MSG_AUTH,
				    ni->ni_macaddr, "shared key response",
				    "%s", "challenge mismatch");
				ic->ic_stats.is_rx_auth_fail++;
				estatus = IEEE80211_STATUS_CHALLENGE;
				goto bad;
			}
			IEEE80211_DPRINTF(ic,
			    IEEE80211_MSG_DEBUG | IEEE80211_MSG_AUTH,
			    "[%s] station authenticated (shared key)\n",
			    ether_sprintf(ni->ni_macaddr));
			ieee80211_node_authorize(ni);
			break;
		default:
			IEEE80211_DISCARD_MAC(ic, IEEE80211_MSG_AUTH,
			    ni->ni_macaddr, "shared key auth",
			    "bad seq %d", seq);
			ic->ic_stats.is_rx_bad_auth++;
			estatus = IEEE80211_STATUS_SEQUENCE;
			goto bad;
		}
		IEEE80211_SEND_MGMT(ic, ni,
			IEEE80211_FC0_SUBTYPE_AUTH, seq + 1);
		break;

	case IEEE80211_M_STA:
		if (ic->ic_state != IEEE80211_S_AUTH)
			return;
		switch (seq) {
		case IEEE80211_AUTH_SHARED_PASS:
			if (ni->ni_challenge != NULL) {
				FREE(ni->ni_challenge, M_80211_NODE);
				ni->ni_challenge = NULL;
			}
			if (status != 0) {
				IEEE80211_DPRINTF(ic,
				    IEEE80211_MSG_DEBUG | IEEE80211_MSG_AUTH,
				    "[%s] shared key auth failed (reason %d)\n",
				    ether_sprintf(ieee80211_getbssid(ic, wh)),
				    status);
				/* XXX can this happen? */
				if (ni != ic->ic_bss)
					ni->ni_fails++;
				ic->ic_stats.is_rx_auth_fail++;
				return;
			}
			ieee80211_new_state(ic, IEEE80211_S_ASSOC, 0);
			break;
		case IEEE80211_AUTH_SHARED_CHALLENGE:
			if (!alloc_challenge(ic, ni))
				return;
			/* XXX could optimize by passing recvd challenge */
			memcpy(ni->ni_challenge, &challenge[2], challenge[1]);
			IEEE80211_SEND_MGMT(ic, ni,
				IEEE80211_FC0_SUBTYPE_AUTH, seq + 1);
			break;
		default:
			IEEE80211_DISCARD(ic, IEEE80211_MSG_AUTH,
			    wh, "shared key auth", "bad seq %d", seq);
			ic->ic_stats.is_rx_bad_auth++;
			return;
		}
		break;
	}
	return;
bad:
	/*
	 * Send an error response; but only when operating as an AP.
	 */
	if (ic->ic_opmode == IEEE80211_M_HOSTAP) {
		/* XXX hack to workaround calling convention */
		ieee80211_send_error(ic, ni, wh->i_addr2,
		    IEEE80211_FC0_SUBTYPE_AUTH,
		    (seq + 1) | (estatus<<16));
	} else if (ic->ic_opmode == IEEE80211_M_STA) {
		/*
		 * Kick the state machine.  This short-circuits
		 * using the mgt frame timeout to trigger the
		 * state transition.
		 */
		if (ic->ic_state == IEEE80211_S_AUTH)
			ieee80211_new_state(ic, IEEE80211_S_SCAN,
			    IEEE80211_SCAN_FAIL_STATUS);
	}
}

/* Verify the existence and length of __elem or get out. */
#define IEEE80211_VERIFY_ELEMENT(__elem, __maxlen) do {			\
	if ((__elem) == NULL) {						\
		IEEE80211_DISCARD(ic, IEEE80211_MSG_ELEMID,		\
		    wh, ieee80211_mgt_subtype_name[subtype >>		\
			IEEE80211_FC0_SUBTYPE_SHIFT],			\
		    "%s", "no " #__elem );				\
		ic->ic_stats.is_rx_elem_missing++;			\
		return;							\
	}								\
	if ((__elem)[1] > (__maxlen)) {					\
		IEEE80211_DISCARD(ic, IEEE80211_MSG_ELEMID,		\
		    wh, ieee80211_mgt_subtype_name[subtype >>		\
			IEEE80211_FC0_SUBTYPE_SHIFT],			\
		    "bad " #__elem " len %d", (__elem)[1]);		\
		ic->ic_stats.is_rx_elem_toobig++;			\
		return;							\
	}								\
} while (0)

#define	IEEE80211_VERIFY_LENGTH(_len, _minlen, _action) do {		\
	if ((_len) < (_minlen)) {					\
		IEEE80211_DISCARD(ic, IEEE80211_MSG_ELEMID,		\
		    wh, ieee80211_mgt_subtype_name[subtype >>		\
			IEEE80211_FC0_SUBTYPE_SHIFT],			\
		    "ie too short, got %d, expected %d",		\
		    (_len), (_minlen));					\
		ic->ic_stats.is_rx_elem_toosmall++;			\
		_action;						\
	}								\
} while (0)

#ifdef IEEE80211_DEBUG
static void
ieee80211_ssid_mismatch(struct ieee80211com *ic, const char *tag,
	uint8_t mac[IEEE80211_ADDR_LEN], uint8_t *ssid)
{
	printf("[%s] discard %s frame, ssid mismatch: ",
		ether_sprintf(mac), tag);
	ieee80211_print_essid(ssid + 2, ssid[1]);
	printf("\n");
}

#define	IEEE80211_VERIFY_SSID(_ni, _ssid) do {				\
	if ((_ssid)[1] != 0 &&						\
	    ((_ssid)[1] != (_ni)->ni_esslen ||				\
	    memcmp((_ssid) + 2, (_ni)->ni_essid, (_ssid)[1]) != 0)) {	\
		if (ieee80211_msg_input(ic))				\
			ieee80211_ssid_mismatch(ic, 			\
			    ieee80211_mgt_subtype_name[subtype >>	\
				IEEE80211_FC0_SUBTYPE_SHIFT],		\
				wh->i_addr2, _ssid);			\
		ic->ic_stats.is_rx_ssidmismatch++;			\
		return;							\
	}								\
} while (0)
#else /* !IEEE80211_DEBUG */
#define	IEEE80211_VERIFY_SSID(_ni, _ssid) do {				\
	if ((_ssid)[1] != 0 &&						\
	    ((_ssid)[1] != (_ni)->ni_esslen ||				\
	    memcmp((_ssid) + 2, (_ni)->ni_essid, (_ssid)[1]) != 0)) {	\
		ic->ic_stats.is_rx_ssidmismatch++;			\
		return;							\
	}								\
} while (0)
#endif /* !IEEE80211_DEBUG */

/* unalligned little endian access */     
#define LE_READ_2(p)					\
	((uint16_t)					\
	 ((((const uint8_t *)(p))[0]      ) |		\
	  (((const uint8_t *)(p))[1] <<  8)))
#define LE_READ_4(p)					\
	((uint32_t)					\
	 ((((const uint8_t *)(p))[0]      ) |		\
	  (((const uint8_t *)(p))[1] <<  8) |		\
	  (((const uint8_t *)(p))[2] << 16) |		\
	  (((const uint8_t *)(p))[3] << 24)))

static __inline int
iswpaoui(const uint8_t *frm)
{
	return frm[1] > 3 && LE_READ_4(frm+2) == ((WPA_OUI_TYPE<<24)|WPA_OUI);
}

static __inline int
iswmeoui(const uint8_t *frm)
{
	return frm[1] > 3 && LE_READ_4(frm+2) == ((WME_OUI_TYPE<<24)|WME_OUI);
}

static __inline int
iswmeparam(const uint8_t *frm)
{
	return frm[1] > 5 && LE_READ_4(frm+2) == ((WME_OUI_TYPE<<24)|WME_OUI) &&
		frm[6] == WME_PARAM_OUI_SUBTYPE;
}

static __inline int
iswmeinfo(const uint8_t *frm)
{
	return frm[1] > 5 && LE_READ_4(frm+2) == ((WME_OUI_TYPE<<24)|WME_OUI) &&
		frm[6] == WME_INFO_OUI_SUBTYPE;
}

static __inline int
isatherosoui(const uint8_t *frm)
{
	return frm[1] > 3 && LE_READ_4(frm+2) == ((ATH_OUI_TYPE<<24)|ATH_OUI);
}

static __inline int
ishtcapoui(const uint8_t *frm)
{
	return frm[1] > 3 && LE_READ_4(frm+2) == ((BCM_OUI_HTCAP<<24)|BCM_OUI);
}

static __inline int
ishtinfooui(const uint8_t *frm)
{
	return frm[1] > 3 && LE_READ_4(frm+2) == ((BCM_OUI_HTINFO<<24)|BCM_OUI);
}

/*
 * Convert a WPA cipher selector OUI to an internal
 * cipher algorithm.  Where appropriate we also
 * record any key length.
 */
static int
wpa_cipher(uint8_t *sel, uint8_t *keylen)
{
#define	WPA_SEL(x)	(((x)<<24)|WPA_OUI)
	uint32_t w = LE_READ_4(sel);

	switch (w) {
	case WPA_SEL(WPA_CSE_NULL):
		return IEEE80211_CIPHER_NONE;
	case WPA_SEL(WPA_CSE_WEP40):
		if (keylen)
			*keylen = 40 / NBBY;
		return IEEE80211_CIPHER_WEP;
	case WPA_SEL(WPA_CSE_WEP104):
		if (keylen)
			*keylen = 104 / NBBY;
		return IEEE80211_CIPHER_WEP;
	case WPA_SEL(WPA_CSE_TKIP):
		return IEEE80211_CIPHER_TKIP;
	case WPA_SEL(WPA_CSE_CCMP):
		return IEEE80211_CIPHER_AES_CCM;
	}
	return 32;		/* NB: so 1<< is discarded */
#undef WPA_SEL
}

/*
 * Convert a WPA key management/authentication algorithm
 * to an internal code.
 */
static int
wpa_keymgmt(uint8_t *sel)
{
#define	WPA_SEL(x)	(((x)<<24)|WPA_OUI)
	uint32_t w = LE_READ_4(sel);

	switch (w) {
	case WPA_SEL(WPA_ASE_8021X_UNSPEC):
		return WPA_ASE_8021X_UNSPEC;
	case WPA_SEL(WPA_ASE_8021X_PSK):
		return WPA_ASE_8021X_PSK;
	case WPA_SEL(WPA_ASE_NONE):
		return WPA_ASE_NONE;
	}
	return 0;		/* NB: so is discarded */
#undef WPA_SEL
}

/*
 * Parse a WPA information element to collect parameters
 * and validate the parameters against what has been
 * configured for the system.
 */
static int
ieee80211_parse_wpa(struct ieee80211com *ic, uint8_t *frm,
	struct ieee80211_rsnparms *rsn, const struct ieee80211_frame *wh)
{
	uint8_t len = frm[1];
	uint32_t w;
	int n;

	/*
	 * Check the length once for fixed parts: OUI, type,
	 * version, mcast cipher, and 2 selector counts.
	 * Other, variable-length data, must be checked separately.
	 */
	if ((ic->ic_flags & IEEE80211_F_WPA1) == 0) {
		IEEE80211_DISCARD_IE(ic,
		    IEEE80211_MSG_ELEMID | IEEE80211_MSG_WPA,
		    wh, "WPA", "not WPA, flags 0x%x", ic->ic_flags);
		return IEEE80211_REASON_IE_INVALID;
	}
	if (len < 14) {
		IEEE80211_DISCARD_IE(ic,
		    IEEE80211_MSG_ELEMID | IEEE80211_MSG_WPA,
		    wh, "WPA", "too short, len %u", len);
		return IEEE80211_REASON_IE_INVALID;
	}
	frm += 6, len -= 4;		/* NB: len is payload only */
	/* NB: iswapoui already validated the OUI and type */
	w = LE_READ_2(frm);
	if (w != WPA_VERSION) {
		IEEE80211_DISCARD_IE(ic,
		    IEEE80211_MSG_ELEMID | IEEE80211_MSG_WPA,
		    wh, "WPA", "bad version %u", w);
		return IEEE80211_REASON_IE_INVALID;
	}
	frm += 2, len -= 2;

	/* multicast/group cipher */
	w = wpa_cipher(frm, &rsn->rsn_mcastkeylen);
	if (w != rsn->rsn_mcastcipher) {
		IEEE80211_DISCARD_IE(ic,
		    IEEE80211_MSG_ELEMID | IEEE80211_MSG_WPA,
		    wh, "WPA", "mcast cipher mismatch; got %u, expected %u",
		    w, rsn->rsn_mcastcipher);
		return IEEE80211_REASON_IE_INVALID;
	}
	frm += 4, len -= 4;

	/* unicast ciphers */
	n = LE_READ_2(frm);
	frm += 2, len -= 2;
	if (len < n*4+2) {
		IEEE80211_DISCARD_IE(ic,
		    IEEE80211_MSG_ELEMID | IEEE80211_MSG_WPA,
		    wh, "WPA", "ucast cipher data too short; len %u, n %u",
		    len, n);
		return IEEE80211_REASON_IE_INVALID;
	}
	w = 0;
	for (; n > 0; n--) {
		w |= 1<<wpa_cipher(frm, &rsn->rsn_ucastkeylen);
		frm += 4, len -= 4;
	}
	w &= rsn->rsn_ucastcipherset;
	if (w == 0) {
		IEEE80211_DISCARD_IE(ic,
		    IEEE80211_MSG_ELEMID | IEEE80211_MSG_WPA,
		    wh, "WPA", "%s", "ucast cipher set empty");
		return IEEE80211_REASON_IE_INVALID;
	}
	if (w & (1<<IEEE80211_CIPHER_TKIP))
		rsn->rsn_ucastcipher = IEEE80211_CIPHER_TKIP;
	else
		rsn->rsn_ucastcipher = IEEE80211_CIPHER_AES_CCM;

	/* key management algorithms */
	n = LE_READ_2(frm);
	frm += 2, len -= 2;
	if (len < n*4) {
		IEEE80211_DISCARD_IE(ic,
		    IEEE80211_MSG_ELEMID | IEEE80211_MSG_WPA,
		    wh, "WPA", "key mgmt alg data too short; len %u, n %u",
		    len, n);
		return IEEE80211_REASON_IE_INVALID;
	}
	w = 0;
	for (; n > 0; n--) {
		w |= wpa_keymgmt(frm);
		frm += 4, len -= 4;
	}
	w &= rsn->rsn_keymgmtset;
	if (w == 0) {
		IEEE80211_DISCARD_IE(ic,
		    IEEE80211_MSG_ELEMID | IEEE80211_MSG_WPA,
		    wh, "WPA", "%s", "no acceptable key mgmt alg");
		return IEEE80211_REASON_IE_INVALID;
	}
	if (w & WPA_ASE_8021X_UNSPEC)
		rsn->rsn_keymgmt = WPA_ASE_8021X_UNSPEC;
	else
		rsn->rsn_keymgmt = WPA_ASE_8021X_PSK;

	if (len > 2)		/* optional capabilities */
		rsn->rsn_caps = LE_READ_2(frm);

	return 0;
}

/*
 * Convert an RSN cipher selector OUI to an internal
 * cipher algorithm.  Where appropriate we also
 * record any key length.
 */
static int
rsn_cipher(uint8_t *sel, uint8_t *keylen)
{
#define	RSN_SEL(x)	(((x)<<24)|RSN_OUI)
	uint32_t w = LE_READ_4(sel);

	switch (w) {
	case RSN_SEL(RSN_CSE_NULL):
		return IEEE80211_CIPHER_NONE;
	case RSN_SEL(RSN_CSE_WEP40):
		if (keylen)
			*keylen = 40 / NBBY;
		return IEEE80211_CIPHER_WEP;
	case RSN_SEL(RSN_CSE_WEP104):
		if (keylen)
			*keylen = 104 / NBBY;
		return IEEE80211_CIPHER_WEP;
	case RSN_SEL(RSN_CSE_TKIP):
		return IEEE80211_CIPHER_TKIP;
	case RSN_SEL(RSN_CSE_CCMP):
		return IEEE80211_CIPHER_AES_CCM;
	case RSN_SEL(RSN_CSE_WRAP):
		return IEEE80211_CIPHER_AES_OCB;
	}
	return 32;		/* NB: so 1<< is discarded */
#undef WPA_SEL
}

/*
 * Convert an RSN key management/authentication algorithm
 * to an internal code.
 */
static int
rsn_keymgmt(uint8_t *sel)
{
#define	RSN_SEL(x)	(((x)<<24)|RSN_OUI)
	uint32_t w = LE_READ_4(sel);

	switch (w) {
	case RSN_SEL(RSN_ASE_8021X_UNSPEC):
		return RSN_ASE_8021X_UNSPEC;
	case RSN_SEL(RSN_ASE_8021X_PSK):
		return RSN_ASE_8021X_PSK;
	case RSN_SEL(RSN_ASE_NONE):
		return RSN_ASE_NONE;
	}
	return 0;		/* NB: so is discarded */
#undef RSN_SEL
}

/*
 * Parse a WPA/RSN information element to collect parameters
 * and validate the parameters against what has been
 * configured for the system.
 */
static int
ieee80211_parse_rsn(struct ieee80211com *ic, uint8_t *frm,
	struct ieee80211_rsnparms *rsn, const struct ieee80211_frame *wh)
{
	uint8_t len = frm[1];
	uint32_t w;
	int n;

	/*
	 * Check the length once for fixed parts: 
	 * version, mcast cipher, and 2 selector counts.
	 * Other, variable-length data, must be checked separately.
	 */
	if ((ic->ic_flags & IEEE80211_F_WPA2) == 0) {
		IEEE80211_DISCARD_IE(ic,
		    IEEE80211_MSG_ELEMID | IEEE80211_MSG_WPA,
		    wh, "WPA", "not RSN, flags 0x%x", ic->ic_flags);
		return IEEE80211_REASON_IE_INVALID;
	}
	if (len < 10) {
		IEEE80211_DISCARD_IE(ic,
		    IEEE80211_MSG_ELEMID | IEEE80211_MSG_WPA,
		    wh, "RSN", "too short, len %u", len);
		return IEEE80211_REASON_IE_INVALID;
	}
	frm += 2;
	w = LE_READ_2(frm);
	if (w != RSN_VERSION) {
		IEEE80211_DISCARD_IE(ic,
		    IEEE80211_MSG_ELEMID | IEEE80211_MSG_WPA,
		    wh, "RSN", "bad version %u", w);
		return IEEE80211_REASON_IE_INVALID;
	}
	frm += 2, len -= 2;

	/* multicast/group cipher */
	w = rsn_cipher(frm, &rsn->rsn_mcastkeylen);
	if (w != rsn->rsn_mcastcipher) {
		IEEE80211_DISCARD_IE(ic,
		    IEEE80211_MSG_ELEMID | IEEE80211_MSG_WPA,
		    wh, "RSN", "mcast cipher mismatch; got %u, expected %u",
		    w, rsn->rsn_mcastcipher);
		return IEEE80211_REASON_IE_INVALID;
	}
	frm += 4, len -= 4;

	/* unicast ciphers */
	n = LE_READ_2(frm);
	frm += 2, len -= 2;
	if (len < n*4+2) {
		IEEE80211_DISCARD_IE(ic,
		    IEEE80211_MSG_ELEMID | IEEE80211_MSG_WPA,
		    wh, "RSN", "ucast cipher data too short; len %u, n %u",
		    len, n);
		return IEEE80211_REASON_IE_INVALID;
	}
	w = 0;
	for (; n > 0; n--) {
		w |= 1<<rsn_cipher(frm, &rsn->rsn_ucastkeylen);
		frm += 4, len -= 4;
	}
	w &= rsn->rsn_ucastcipherset;
	if (w == 0) {
		IEEE80211_DISCARD_IE(ic,
		    IEEE80211_MSG_ELEMID | IEEE80211_MSG_WPA,
		    wh, "RSN", "%s", "ucast cipher set empty");
		return IEEE80211_REASON_IE_INVALID;
	}
	if (w & (1<<IEEE80211_CIPHER_TKIP))
		rsn->rsn_ucastcipher = IEEE80211_CIPHER_TKIP;
	else
		rsn->rsn_ucastcipher = IEEE80211_CIPHER_AES_CCM;

	/* key management algorithms */
	n = LE_READ_2(frm);
	frm += 2, len -= 2;
	if (len < n*4) {
		IEEE80211_DISCARD_IE(ic, 
		    IEEE80211_MSG_ELEMID | IEEE80211_MSG_WPA,
		    wh, "RSN", "key mgmt alg data too short; len %u, n %u",
		    len, n);
		return IEEE80211_REASON_IE_INVALID;
	}
	w = 0;
	for (; n > 0; n--) {
		w |= rsn_keymgmt(frm);
		frm += 4, len -= 4;
	}
	w &= rsn->rsn_keymgmtset;
	if (w == 0) {
		IEEE80211_DISCARD_IE(ic,
		    IEEE80211_MSG_ELEMID | IEEE80211_MSG_WPA,
		    wh, "RSN", "%s", "no acceptable key mgmt alg");
		return IEEE80211_REASON_IE_INVALID;
	}
	if (w & RSN_ASE_8021X_UNSPEC)
		rsn->rsn_keymgmt = RSN_ASE_8021X_UNSPEC;
	else
		rsn->rsn_keymgmt = RSN_ASE_8021X_PSK;

	/* optional RSN capabilities */
	if (len > 2)
		rsn->rsn_caps = LE_READ_2(frm);
	/* XXXPMKID */

	return 0;
}

static int
ieee80211_parse_wmeparams(struct ieee80211com *ic, uint8_t *frm,
	const struct ieee80211_frame *wh)
{
#define	MS(_v, _f)	(((_v) & _f) >> _f##_S)
	struct ieee80211_wme_state *wme = &ic->ic_wme;
	u_int len = frm[1], qosinfo;
	int i;

	if (len < sizeof(struct ieee80211_wme_param)-2) {
		IEEE80211_DISCARD_IE(ic,
		    IEEE80211_MSG_ELEMID | IEEE80211_MSG_WME,
		    wh, "WME", "too short, len %u", len);
		return -1;
	}
	qosinfo = frm[__offsetof(struct ieee80211_wme_param, param_qosInfo)];
	qosinfo &= WME_QOSINFO_COUNT;
	/* XXX do proper check for wraparound */
	if (qosinfo == wme->wme_wmeChanParams.cap_info)
		return 0;
	frm += __offsetof(struct ieee80211_wme_param, params_acParams);
	for (i = 0; i < WME_NUM_AC; i++) {
		struct wmeParams *wmep =
			&wme->wme_wmeChanParams.cap_wmeParams[i];
		/* NB: ACI not used */
		wmep->wmep_acm = MS(frm[0], WME_PARAM_ACM);
		wmep->wmep_aifsn = MS(frm[0], WME_PARAM_AIFSN);
		wmep->wmep_logcwmin = MS(frm[1], WME_PARAM_LOGCWMIN);
		wmep->wmep_logcwmax = MS(frm[1], WME_PARAM_LOGCWMAX);
		wmep->wmep_txopLimit = LE_READ_2(frm+2);
		frm += 4;
	}
	wme->wme_wmeChanParams.cap_info = qosinfo;
	return 1;
#undef MS
}

static int
ieee80211_parse_athparams(struct ieee80211_node *ni, uint8_t *frm,
	const struct ieee80211_frame *wh)
{
	struct ieee80211com *ic = ni->ni_ic;
	const struct ieee80211_ath_ie *ath;
	u_int len = frm[1];
	int capschanged;
	uint16_t defkeyix;

	if (len < sizeof(struct ieee80211_ath_ie)-2) {
		IEEE80211_DISCARD_IE(ic,
		    IEEE80211_MSG_ELEMID | IEEE80211_MSG_SUPERG,
		    wh, "Atheros", "too short, len %u", len);
		return -1;
	}
	ath = (const struct ieee80211_ath_ie *)frm;
	capschanged = (ni->ni_ath_flags != ath->ath_capability);
	defkeyix = LE_READ_2(ath->ath_defkeyix);
	if (capschanged || defkeyix != ni->ni_ath_defkeyix) {
		ni->ni_ath_flags = ath->ath_capability;
		ni->ni_ath_defkeyix = defkeyix;
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_SUPERG,
		    "[%s] ath ie change: new caps 0x%x defkeyix 0x%x\n",
		    ether_sprintf(ni->ni_macaddr),
		    ni->ni_ath_flags, ni->ni_ath_defkeyix);
	}
	if (IEEE80211_ATH_CAP(ic, ni, ATHEROS_CAP_TURBO_PRIME)) {
		uint16_t curflags, newflags;

		/*
		 * Check for turbo mode switch.  Calculate flags
		 * for the new mode and effect the switch.
		 */
		newflags = curflags = ic->ic_bsschan->ic_flags;
		/* NB: BOOST is not in ic_flags, so get it from the ie */
		if (ath->ath_capability & ATHEROS_CAP_BOOST) 
			newflags |= IEEE80211_CHAN_TURBO;
		else
			newflags &= ~IEEE80211_CHAN_TURBO;
		if (newflags != curflags)
			ieee80211_dturbo_switch(ic, newflags);
	}
	return capschanged;
}

void
ieee80211_saveath(struct ieee80211_node *ni, uint8_t *ie)
{
	const struct ieee80211_ath_ie *ath =
		(const struct ieee80211_ath_ie *) ie;

	ni->ni_ath_flags = ath->ath_capability;
	ni->ni_ath_defkeyix = LE_READ_2(&ath->ath_defkeyix);
	ieee80211_saveie(&ni->ni_ath_ie, ie);
}

void
ieee80211_saveie(uint8_t **iep, const uint8_t *ie)
{
	u_int ielen = ie[1]+2;
	/*
	 * Record information element for later use.
	 */
	if (*iep == NULL || (*iep)[1] != ie[1]) {
		if (*iep != NULL)
			FREE(*iep, M_80211_NODE);
		MALLOC(*iep, void*, ielen, M_80211_NODE, M_NOWAIT);
	}
	if (*iep != NULL)
		memcpy(*iep, ie, ielen);
	/* XXX note failure */
}

/* XXX find a better place for definition */
struct l2_update_frame {
	struct ether_header eh;
	uint8_t dsap;
	uint8_t ssap;
	uint8_t control;
	uint8_t xid[3];
}  __packed;

/*
 * Deliver a TGf L2UF frame on behalf of a station.
 * This primes any bridge when the station is roaming
 * between ap's on the same wired network.
 */
static void
ieee80211_deliver_l2uf(struct ieee80211_node *ni)
{
	struct ieee80211com *ic = ni->ni_ic;
	struct ifnet *ifp = ic->ic_ifp;
	struct mbuf *m;
	struct l2_update_frame *l2uf;
	struct ether_header *eh;
	
	m = m_gethdr(M_NOWAIT, MT_DATA);
	if (m == NULL) {
		IEEE80211_NOTE(ic, IEEE80211_MSG_ASSOC, ni,
		    "%s", "no mbuf for l2uf frame");
		ic->ic_stats.is_rx_nobuf++;	/* XXX not right */
		return;
	}
	l2uf = mtod(m, struct l2_update_frame *);
	eh = &l2uf->eh;
	/* dst: Broadcast address */
	IEEE80211_ADDR_COPY(eh->ether_dhost, ifp->if_broadcastaddr);
	/* src: associated STA */
	IEEE80211_ADDR_COPY(eh->ether_shost, ni->ni_macaddr);
	eh->ether_type = htons(sizeof(*l2uf) - sizeof(*eh));
	
	l2uf->dsap = 0;
	l2uf->ssap = 0;
	l2uf->control = 0xf5;
	l2uf->xid[0] = 0x81;
	l2uf->xid[1] = 0x80;
	l2uf->xid[2] = 0x00;
	
	m->m_pkthdr.len = m->m_len = sizeof(*l2uf);
	ieee80211_deliver_data(ic, ni, m);
}

static __inline int
contbgscan(struct ieee80211com *ic)
{
	return ((ic->ic_flags_ext & IEEE80211_FEXT_BGSCAN) &&
	    time_after(ticks, ic->ic_lastdata + ic->ic_bgscanidle));
}

static __inline int
startbgscan(struct ieee80211com *ic)
{
	return ((ic->ic_flags & IEEE80211_F_BGSCAN) &&
	    !IEEE80211_IS_CHAN_DTURBO(ic->ic_curchan) &&
	    time_after(ticks, ic->ic_lastscan + ic->ic_bgscanintvl) &&
	    time_after(ticks, ic->ic_lastdata + ic->ic_bgscanidle));
}

static void
ratesetmismatch(struct ieee80211_node *ni, const struct ieee80211_frame *wh,
	int reassoc, int resp, const char *tag, int rate)
{
	struct ieee80211com *ic = ni->ni_ic;

	IEEE80211_DPRINTF(ic, IEEE80211_MSG_ANY,
	    "[%s] deny %s request, %s rate set mismatch, rate 0x%x\n",
	    ether_sprintf(wh->i_addr2),
	    reassoc ? "reassoc" : "assoc", tag, rate);
	IEEE80211_SEND_MGMT(ic, ni, resp, IEEE80211_STATUS_BASIC_RATE);
	ieee80211_node_leave(ic, ni);
	ic->ic_stats.is_rx_assoc_norate++;
}

static void
capinfomismatch(struct ieee80211_node *ni, const struct ieee80211_frame *wh,
	int reassoc, int resp, const char *tag, int capinfo)
{
	struct ieee80211com *ic = ni->ni_ic;

	IEEE80211_DPRINTF(ic, IEEE80211_MSG_ANY,
	    "[%s] deny %s request, %s mismatch 0x%x\n",
	    ether_sprintf(wh->i_addr2),
	    reassoc ? "reassoc" : "assoc", tag, capinfo);
	IEEE80211_SEND_MGMT(ic, ni, resp, IEEE80211_STATUS_CAPINFO);
	ieee80211_node_leave(ic, ni);
	ic->ic_stats.is_rx_assoc_capmismatch++;
}

void
ieee80211_recv_mgmt(struct ieee80211com *ic, struct mbuf *m0,
	struct ieee80211_node *ni,
	int subtype, int rssi, int noise, uint32_t rstamp)
{
#define	ISPROBE(_st)	((_st) == IEEE80211_FC0_SUBTYPE_PROBE_RESP)
#define	ISREASSOC(_st)	((_st) == IEEE80211_FC0_SUBTYPE_REASSOC_RESP)
	struct ieee80211_frame *wh;
	uint8_t *frm, *efrm;
	uint8_t *ssid, *rates, *xrates, *wpa, *rsn, *wme, *ath, *htcap;
	int reassoc, resp, allocbs;
	uint8_t rate;

	wh = mtod(m0, struct ieee80211_frame *);
	frm = (uint8_t *)&wh[1];
	efrm = mtod(m0, uint8_t *) + m0->m_len;
	switch (subtype) {
	case IEEE80211_FC0_SUBTYPE_PROBE_RESP:
	case IEEE80211_FC0_SUBTYPE_BEACON: {
		struct ieee80211_scanparams scan;

		/*
		 * We process beacon/probe response frames:
		 *    o when scanning, or
		 *    o station mode when associated (to collect state
		 *      updates such as 802.11g slot time), or
		 *    o adhoc mode (to discover neighbors)
		 * Frames otherwise received are discarded.
		 */ 
		if (!((ic->ic_flags & IEEE80211_F_SCAN) ||
		      (ic->ic_opmode == IEEE80211_M_STA && ni->ni_associd) ||
		       ic->ic_opmode == IEEE80211_M_IBSS)) {
			ic->ic_stats.is_rx_mgtdiscard++;
			return;
		}
		/*
		 * beacon/probe response frame format
		 *	[8] time stamp
		 *	[2] beacon interval
		 *	[2] capability information
		 *	[tlv] ssid
		 *	[tlv] supported rates
		 *	[tlv] country information
		 *	[tlv] parameter set (FH/DS)
		 *	[tlv] erp information
		 *	[tlv] extended supported rates
		 *	[tlv] WME
		 *	[tlv] WPA or RSN
		 *	[tlv] HT capabilities
		 *	[tlv] HT information
		 *	[tlv] Atheros capabilities
		 */
		IEEE80211_VERIFY_LENGTH(efrm - frm, 12, return);
		memset(&scan, 0, sizeof(scan));
		scan.tstamp  = frm;				frm += 8;
		scan.bintval = le16toh(*(uint16_t *)frm);	frm += 2;
		scan.capinfo = le16toh(*(uint16_t *)frm);	frm += 2;
		scan.bchan = IEEE80211_CHAN2IEEE(ic->ic_curchan);
		scan.curchan = ic->ic_curchan;

		while (efrm - frm > 1) {
			IEEE80211_VERIFY_LENGTH(efrm - frm, frm[1] + 2, return);
			switch (*frm) {
			case IEEE80211_ELEMID_SSID:
				scan.ssid = frm;
				break;
			case IEEE80211_ELEMID_RATES:
				scan.rates = frm;
				break;
			case IEEE80211_ELEMID_COUNTRY:
				scan.country = frm;
				break;
			case IEEE80211_ELEMID_FHPARMS:
				if (ic->ic_phytype == IEEE80211_T_FH) {
					scan.fhdwell = LE_READ_2(&frm[2]);
					scan.bchan = IEEE80211_FH_CHAN(frm[4], frm[5]);
					scan.fhindex = frm[6];
				}
				break;
			case IEEE80211_ELEMID_DSPARMS:
				/*
				 * XXX hack this since depending on phytype
				 * is problematic for multi-mode devices.
				 */
				if (ic->ic_phytype != IEEE80211_T_FH)
					scan.bchan = frm[2];
				break;
			case IEEE80211_ELEMID_TIM:
				/* XXX ATIM? */
				scan.tim = frm;
				scan.timoff = frm - mtod(m0, uint8_t *);
				break;
			case IEEE80211_ELEMID_IBSSPARMS:
				break;
			case IEEE80211_ELEMID_XRATES:
				scan.xrates = frm;
				break;
			case IEEE80211_ELEMID_ERP:
				if (frm[1] != 1) {
					IEEE80211_DISCARD_IE(ic,
					    IEEE80211_MSG_ELEMID, wh, "ERP",
					    "bad len %u", frm[1]);
					ic->ic_stats.is_rx_elem_toobig++;
					break;
				}
				scan.erp = frm[2];
				break;
			case IEEE80211_ELEMID_HTCAP:
				scan.htcap = frm;
				break;
			case IEEE80211_ELEMID_RSN:
				scan.rsn = frm;
				break;
			case IEEE80211_ELEMID_HTINFO:
				scan.htinfo = frm;
				break;
			case IEEE80211_ELEMID_VENDOR:
				if (iswpaoui(frm))
					scan.wpa = frm;
				else if (iswmeparam(frm) || iswmeinfo(frm))
					scan.wme = frm;
				else if (isatherosoui(frm))
					scan.ath = frm;
				else if (ic->ic_flags_ext & IEEE80211_FEXT_HTCOMPAT) {
					/*
					 * Accept pre-draft HT ie's if the
					 * standard ones have not been seen.
					 */
					if (ishtcapoui(frm)) {
						if (scan.htcap == NULL)
							scan.htcap = frm;
					} else if (ishtinfooui(frm)) {
						if (scan.htinfo == NULL)
							scan.htcap = frm;
					}
				}
				break;
			default:
				IEEE80211_DISCARD_IE(ic, IEEE80211_MSG_ELEMID,
				    wh, "unhandled",
				    "id %u, len %u", *frm, frm[1]);
				ic->ic_stats.is_rx_elem_unknown++;
				break;
			}
			frm += frm[1] + 2;
		}
		IEEE80211_VERIFY_ELEMENT(scan.rates, IEEE80211_RATE_MAXSIZE);
		if (scan.xrates != NULL)
			IEEE80211_VERIFY_ELEMENT(scan.xrates,
				IEEE80211_RATE_MAXSIZE - scan.rates[1]);
		IEEE80211_VERIFY_ELEMENT(scan.ssid, IEEE80211_NWID_LEN);
#if IEEE80211_CHAN_MAX < 255
		if (scan.chan > IEEE80211_CHAN_MAX) {
			IEEE80211_DISCARD(ic, IEEE80211_MSG_ELEMID,
			    wh, ieee80211_mgt_subtype_name[subtype >>
				IEEE80211_FC0_SUBTYPE_SHIFT],
			    "invalid channel %u", scan.chan);
			ic->ic_stats.is_rx_badchan++;
			return;
		}
#endif
		if (IEEE80211_CHAN2IEEE(scan.curchan) != scan.bchan &&
		    ic->ic_phytype != IEEE80211_T_FH) {
			/*
			 * Frame was received on a channel different from the
			 * one indicated in the DS params element id;
			 * silently discard it.
			 *
			 * NB: this can happen due to signal leakage.
			 *     But we should take it for FH phy because
			 *     the rssi value should be correct even for
			 *     different hop pattern in FH.
			 */
			IEEE80211_DISCARD(ic,
			    IEEE80211_MSG_ELEMID | IEEE80211_MSG_INPUT,
			    wh, ieee80211_mgt_subtype_name[subtype >>
				IEEE80211_FC0_SUBTYPE_SHIFT],
			    "for off-channel %u",
			    IEEE80211_CHAN2IEEE(scan.curchan));
			ic->ic_stats.is_rx_chanmismatch++;
			return;
		}
		if (!(IEEE80211_BINTVAL_MIN <= scan.bintval &&
		      scan.bintval <= IEEE80211_BINTVAL_MAX)) {
			IEEE80211_DISCARD(ic,
			    IEEE80211_MSG_ELEMID | IEEE80211_MSG_INPUT,
			    wh, ieee80211_mgt_subtype_name[subtype >>
				IEEE80211_FC0_SUBTYPE_SHIFT],
			    "bogus beacon interval", scan.bintval);
			ic->ic_stats.is_rx_badbintval++;
			return;
		}
		/*
		 * Process HT ie's.  This is complicated by our
		 * accepting both the standard ie's and the pre-draft
		 * vendor OUI ie's that some vendors still use/require.
		 */
		if (scan.htcap != NULL) {
			IEEE80211_VERIFY_LENGTH(scan.htcap[1],
			     scan.htcap[0] == IEEE80211_ELEMID_VENDOR ?
			         4 + sizeof(struct ieee80211_ie_htcap)-2 :
			         sizeof(struct ieee80211_ie_htcap)-2,
			     scan.htcap = NULL);
		}
		if (scan.htinfo != NULL) {
			IEEE80211_VERIFY_LENGTH(scan.htinfo[1],
			     scan.htinfo[0] == IEEE80211_ELEMID_VENDOR ?
			         4 + sizeof(struct ieee80211_ie_htinfo)-2 :
			         sizeof(struct ieee80211_ie_htinfo)-2,
			     scan.htinfo = NULL);
		}

		/*
		 * Count frame now that we know it's to be processed.
		 */
		if (subtype == IEEE80211_FC0_SUBTYPE_BEACON) {
			ic->ic_stats.is_rx_beacon++;		/* XXX remove */
			IEEE80211_NODE_STAT(ni, rx_beacons);
		} else
			IEEE80211_NODE_STAT(ni, rx_proberesp);

		/*
		 * When operating in station mode, check for state updates.
		 * Be careful to ignore beacons received while doing a
		 * background scan.  We consider only 11g/WMM stuff right now.
		 */
		if (ic->ic_opmode == IEEE80211_M_STA &&
		    ni->ni_associd != 0 &&
		    ((ic->ic_flags & IEEE80211_F_SCAN) == 0 ||
		     IEEE80211_ADDR_EQ(wh->i_addr2, ni->ni_bssid))) {
			/* record tsf of last beacon */
			memcpy(ni->ni_tstamp.data, scan.tstamp,
				sizeof(ni->ni_tstamp));
			/* count beacon frame for s/w bmiss handling */
			ic->ic_swbmiss_count++;
			ic->ic_bmiss_count = 0;
			if (ni->ni_erp != scan.erp) {
				IEEE80211_DPRINTF(ic, IEEE80211_MSG_ASSOC,
				    "[%s] erp change: was 0x%x, now 0x%x\n",
				    ether_sprintf(wh->i_addr2),
				    ni->ni_erp, scan.erp);
				if (IEEE80211_IS_CHAN_ANYG(ic->ic_curchan) &&
				    (ni->ni_erp & IEEE80211_ERP_USE_PROTECTION))
					ic->ic_flags |= IEEE80211_F_USEPROT;
				else
					ic->ic_flags &= ~IEEE80211_F_USEPROT;
				ni->ni_erp = scan.erp;
				/* XXX statistic */
			}
			if ((ni->ni_capinfo ^ scan.capinfo) & IEEE80211_CAPINFO_SHORT_SLOTTIME) {
				IEEE80211_DPRINTF(ic, IEEE80211_MSG_ASSOC,
				    "[%s] capabilities change: before 0x%x,"
				     " now 0x%x\n",
				     ether_sprintf(wh->i_addr2),
				     ni->ni_capinfo, scan.capinfo);
				/*
				 * NB: we assume short preamble doesn't
				 *     change dynamically
				 */
				ieee80211_set_shortslottime(ic,
					IEEE80211_IS_CHAN_A(ic->ic_bsschan) ||
					(scan.capinfo & IEEE80211_CAPINFO_SHORT_SLOTTIME));
				ni->ni_capinfo = (ni->ni_capinfo &~ IEEE80211_CAPINFO_SHORT_SLOTTIME)
					       | (scan.capinfo & IEEE80211_CAPINFO_SHORT_SLOTTIME);
				/* XXX statistic */
			}
			if (scan.wme != NULL &&
			    (ni->ni_flags & IEEE80211_NODE_QOS) &&
			    ieee80211_parse_wmeparams(ic, scan.wme, wh) > 0)
				ieee80211_wme_updateparams(ic);
			if (scan.ath != NULL)
				ieee80211_parse_athparams(ni, scan.ath, wh);
			if (scan.htcap != NULL)
				ieee80211_parse_htcap(ni, scan.htcap);
			if (scan.htinfo != NULL)
				ieee80211_parse_htinfo(ni, scan.htinfo);
			if (scan.tim != NULL) {
				struct ieee80211_tim_ie *tim =
				    (struct ieee80211_tim_ie *) scan.tim;
#if 0
				int aid = IEEE80211_AID(ni->ni_associd);
				int ix = aid / NBBY;
				int min = tim->tim_bitctl &~ 1;
				int max = tim->tim_len + min - 4;
				if ((tim->tim_bitctl&1) ||
				    (min <= ix && ix <= max &&
				     isset(tim->tim_bitmap - min, aid))) {
					/* 
					 * XXX Do not let bg scan kick off
					 * we are expecting data.
					 */
					ic->ic_lastdata = ticks;
					ieee80211_sta_pwrsave(ic, 0);
				}
#endif
				ni->ni_dtim_count = tim->tim_count;
				ni->ni_dtim_period = tim->tim_period;
			}
			/*
			 * If scanning, pass the info to the scan module.
			 * Otherwise, check if it's the right time to do
			 * a background scan.  Background scanning must
			 * be enabled and we must not be operating in the
			 * turbo phase of dynamic turbo mode.  Then,
			 * it's been a while since the last background
			 * scan and if no data frames have come through
			 * recently, kick off a scan.  Note that this
			 * is the mechanism by which a background scan
			 * is started _and_ continued each time we
			 * return on-channel to receive a beacon from
			 * our ap.
			 */
			if (ic->ic_flags & IEEE80211_F_SCAN) {
				ieee80211_add_scan(ic, &scan, wh,
					subtype, rssi, noise, rstamp);
			} else if (contbgscan(ic)) {
				ieee80211_bg_scan(ic);
			} else if (startbgscan(ic)) {
#if 0
				/* wakeup if we are sleeing */
				ieee80211_set_pwrsave(ic, 0);
#endif
				ieee80211_bg_scan(ic);
			}
			return;
		}
		/*
		 * If scanning, just pass information to the scan module.
		 */
		if (ic->ic_flags & IEEE80211_F_SCAN) {
			if (ic->ic_flags_ext & IEEE80211_FEXT_PROBECHAN) {
				/*
				 * Actively scanning a channel marked passive;
				 * send a probe request now that we know there
				 * is 802.11 traffic present.
				 *
				 * XXX check if the beacon we recv'd gives
				 * us what we need and suppress the probe req
				 */
				ieee80211_probe_curchan(ic, 1);
				ic->ic_flags_ext &= ~IEEE80211_FEXT_PROBECHAN;
			}
			ieee80211_add_scan(ic, &scan, wh,
				subtype, rssi, noise, rstamp);
			return;
		}
		if (scan.capinfo & IEEE80211_CAPINFO_IBSS) {
			if (!IEEE80211_ADDR_EQ(wh->i_addr2, ni->ni_macaddr)) {
				/*
				 * Create a new entry in the neighbor table.
				 */
				ni = ieee80211_add_neighbor(ic, wh, &scan);
			} else if (ni->ni_capinfo == 0) {
				/*
				 * Update faked node created on transmit.
				 * Note this also updates the tsf.
				 */
				ieee80211_init_neighbor(ni, wh, &scan);
			} else {
				/*
				 * Record tsf for potential resync.
				 */
				memcpy(ni->ni_tstamp.data, scan.tstamp,
					sizeof(ni->ni_tstamp));
			}
			if (ni != NULL) {
				ni->ni_rssi = rssi;
				ni->ni_noise = noise;
				ni->ni_rstamp = rstamp;
			}
		}
		break;
	}

	case IEEE80211_FC0_SUBTYPE_PROBE_REQ:
		if (ic->ic_opmode == IEEE80211_M_STA ||
		    ic->ic_state != IEEE80211_S_RUN) {
			ic->ic_stats.is_rx_mgtdiscard++;
			return;
		}
		if (IEEE80211_IS_MULTICAST(wh->i_addr2)) {
			/* frame must be directed */
			ic->ic_stats.is_rx_mgtdiscard++;	/* XXX stat */
			return;
		}

		/*
		 * prreq frame format
		 *	[tlv] ssid
		 *	[tlv] supported rates
		 *	[tlv] extended supported rates
		 *	[tlv] Atheros capabilities
		 */
		ssid = rates = xrates = ath = NULL;
		while (efrm - frm > 1) {
			IEEE80211_VERIFY_LENGTH(efrm - frm, frm[1] + 2, return);
			switch (*frm) {
			case IEEE80211_ELEMID_SSID:
				ssid = frm;
				break;
			case IEEE80211_ELEMID_RATES:
				rates = frm;
				break;
			case IEEE80211_ELEMID_XRATES:
				xrates = frm;
				break;
			case IEEE80211_ELEMID_VENDOR:
				if (isatherosoui(frm))
					ath = frm;
				break;
			}
			frm += frm[1] + 2;
		}
		IEEE80211_VERIFY_ELEMENT(rates, IEEE80211_RATE_MAXSIZE);
		if (xrates != NULL)
			IEEE80211_VERIFY_ELEMENT(xrates,
				IEEE80211_RATE_MAXSIZE - rates[1]);
		IEEE80211_VERIFY_ELEMENT(ssid, IEEE80211_NWID_LEN);
		IEEE80211_VERIFY_SSID(ic->ic_bss, ssid);
		if ((ic->ic_flags & IEEE80211_F_HIDESSID) && ssid[1] == 0) {
			IEEE80211_DISCARD(ic, IEEE80211_MSG_INPUT,
			    wh, ieee80211_mgt_subtype_name[subtype >>
				IEEE80211_FC0_SUBTYPE_SHIFT],
			    "%s", "no ssid with ssid suppression enabled");
			ic->ic_stats.is_rx_ssidmismatch++; /*XXX*/
			return;
		}

		allocbs = 0;
		if (ni == ic->ic_bss) {
			if (ic->ic_opmode != IEEE80211_M_IBSS) {
				ni = ieee80211_tmp_node(ic, wh->i_addr2);
				allocbs = 1;
			} else if (!IEEE80211_ADDR_EQ(wh->i_addr2, ni->ni_macaddr)) {
				/*
				 * XXX Cannot tell if the sender is operating
				 * in ibss mode.  But we need a new node to
				 * send the response so blindly add them to the
				 * neighbor table.
				 */
				ni = ieee80211_fakeup_adhoc_node(&ic->ic_sta,
					wh->i_addr2);
			}
			if (ni == NULL)
				return;
		}
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_ASSOC,
		    "[%s] recv probe req\n", ether_sprintf(wh->i_addr2));
		ni->ni_rssi = rssi;
		ni->ni_rstamp = rstamp;
		rate = ieee80211_setup_rates(ni, rates, xrates,
			  IEEE80211_F_DOSORT | IEEE80211_F_DOFRATE
			| IEEE80211_F_DONEGO | IEEE80211_F_DODEL);
		if (rate & IEEE80211_RATE_BASIC) {
			IEEE80211_DISCARD(ic, IEEE80211_MSG_XRATE,
			    wh, ieee80211_mgt_subtype_name[subtype >>
				IEEE80211_FC0_SUBTYPE_SHIFT],
			    "%s", "recv'd rate set invalid");
		} else {
			IEEE80211_SEND_MGMT(ic, ni,
				IEEE80211_FC0_SUBTYPE_PROBE_RESP, 0);
		}
		if (allocbs) {
			/*
			 * Temporary node created just to send a
			 * response, reclaim immediately.
			 */
			ieee80211_free_node(ni);
		} else if (ath != NULL)
			ieee80211_saveath(ni, ath);
		break;

	case IEEE80211_FC0_SUBTYPE_AUTH: {
		uint16_t algo, seq, status;
		/*
		 * auth frame format
		 *	[2] algorithm
		 *	[2] sequence
		 *	[2] status
		 *	[tlv*] challenge
		 */
		IEEE80211_VERIFY_LENGTH(efrm - frm, 6, return);
		algo   = le16toh(*(uint16_t *)frm);
		seq    = le16toh(*(uint16_t *)(frm + 2));
		status = le16toh(*(uint16_t *)(frm + 4));
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_AUTH,
		    "[%s] recv auth frame with algorithm %d seq %d\n",
		    ether_sprintf(wh->i_addr2), algo, seq);
		/*
		 * Consult the ACL policy module if setup.
		 */
		if (ic->ic_acl != NULL &&
		    !ic->ic_acl->iac_check(ic, wh->i_addr2)) {
			IEEE80211_DISCARD(ic, IEEE80211_MSG_ACL,
			    wh, "auth", "%s", "disallowed by ACL");
			ic->ic_stats.is_rx_acl++;
			if (ic->ic_opmode == IEEE80211_M_HOSTAP) {
				IEEE80211_SEND_MGMT(ic, ni,
				    IEEE80211_FC0_SUBTYPE_AUTH,
				    (seq+1) | (IEEE80211_STATUS_UNSPECIFIED<<16));
			}
			return;
		}
		if (ic->ic_flags & IEEE80211_F_COUNTERM) {
			IEEE80211_DISCARD(ic,
			    IEEE80211_MSG_AUTH | IEEE80211_MSG_CRYPTO,
			    wh, "auth", "%s", "TKIP countermeasures enabled");
			ic->ic_stats.is_rx_auth_countermeasures++;
			if (ic->ic_opmode == IEEE80211_M_HOSTAP) {
				IEEE80211_SEND_MGMT(ic, ni,
					IEEE80211_FC0_SUBTYPE_AUTH,
					IEEE80211_REASON_MIC_FAILURE);
			}
			return;
		}
		if (algo == IEEE80211_AUTH_ALG_SHARED)
			ieee80211_auth_shared(ic, wh, frm + 6, efrm, ni, rssi,
			    noise, rstamp, seq, status);
		else if (algo == IEEE80211_AUTH_ALG_OPEN)
			ieee80211_auth_open(ic, wh, ni, rssi, noise, rstamp,
			    seq, status);
		else {
			IEEE80211_DISCARD(ic, IEEE80211_MSG_ANY,
			    wh, "auth", "unsupported alg %d", algo);
			ic->ic_stats.is_rx_auth_unsupported++;
			if (ic->ic_opmode == IEEE80211_M_HOSTAP) {
				/* XXX not right */
				IEEE80211_SEND_MGMT(ic, ni,
					IEEE80211_FC0_SUBTYPE_AUTH,
					(seq+1) | (IEEE80211_STATUS_ALG<<16));
			}
			return;
		} 
		break;
	}

	case IEEE80211_FC0_SUBTYPE_ASSOC_REQ:
	case IEEE80211_FC0_SUBTYPE_REASSOC_REQ: {
		uint16_t capinfo, lintval;
		struct ieee80211_rsnparms rsnparms;
		uint8_t reason;
		int badwparsn;

		if (ic->ic_opmode != IEEE80211_M_HOSTAP ||
		    ic->ic_state != IEEE80211_S_RUN) {
			ic->ic_stats.is_rx_mgtdiscard++;
			return;
		}

		if (subtype == IEEE80211_FC0_SUBTYPE_REASSOC_REQ) {
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
		 *	[tlv] extended supported rates
		 *	[tlv] WPA or RSN
		 *	[tlv] HT capabilities
		 *	[tlv] Atheros capabilities
		 */
		IEEE80211_VERIFY_LENGTH(efrm - frm, (reassoc ? 10 : 4), return);
		if (!IEEE80211_ADDR_EQ(wh->i_addr3, ic->ic_bss->ni_bssid)) {
			IEEE80211_DISCARD(ic, IEEE80211_MSG_ANY,
			    wh, ieee80211_mgt_subtype_name[subtype >>
				IEEE80211_FC0_SUBTYPE_SHIFT],
			    "%s", "wrong bssid");
			ic->ic_stats.is_rx_assoc_bss++;
			return;
		}
		capinfo = le16toh(*(uint16_t *)frm);	frm += 2;
		lintval = le16toh(*(uint16_t *)frm);	frm += 2;
		if (reassoc)
			frm += 6;	/* ignore current AP info */
		ssid = rates = xrates = wpa = rsn = wme = ath = htcap = NULL;
		while (efrm - frm > 1) {
			IEEE80211_VERIFY_LENGTH(efrm - frm, frm[1] + 2, return);
			switch (*frm) {
			case IEEE80211_ELEMID_SSID:
				ssid = frm;
				break;
			case IEEE80211_ELEMID_RATES:
				rates = frm;
				break;
			case IEEE80211_ELEMID_XRATES:
				xrates = frm;
				break;
			/* XXX verify only one of RSN and WPA ie's? */
			case IEEE80211_ELEMID_RSN:
				rsn = frm;
				break;
			case IEEE80211_ELEMID_HTCAP:
				htcap = frm;
				break;
			case IEEE80211_ELEMID_VENDOR:
				if (iswpaoui(frm))
					wpa = frm;
				else if (iswmeinfo(frm))
					wme = frm;
				else if (isatherosoui(frm))
					ath = frm;
				else if (ic->ic_flags_ext & IEEE80211_FEXT_HTCOMPAT) {
					if (ishtcapoui(frm) && htcap == NULL)
						htcap = frm;
				}
				break;
			}
			frm += frm[1] + 2;
		}
		IEEE80211_VERIFY_ELEMENT(rates, IEEE80211_RATE_MAXSIZE);
		if (xrates != NULL)
			IEEE80211_VERIFY_ELEMENT(xrates,
				IEEE80211_RATE_MAXSIZE - rates[1]);
		IEEE80211_VERIFY_ELEMENT(ssid, IEEE80211_NWID_LEN);
		IEEE80211_VERIFY_SSID(ic->ic_bss, ssid);
		if (htcap != NULL) {
			IEEE80211_VERIFY_LENGTH(htcap[1],
			     htcap[0] == IEEE80211_ELEMID_VENDOR ?
			         4 + sizeof(struct ieee80211_ie_htcap)-2 :
			         sizeof(struct ieee80211_ie_htcap)-2,
			     return);		/* XXX just NULL out? */
		}

		if (ni == ic->ic_bss) {
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_ANY,
			    "[%s] deny %s request, sta not authenticated\n",
			    ether_sprintf(wh->i_addr2),
			    reassoc ? "reassoc" : "assoc");
			ieee80211_send_error(ic, ni, wh->i_addr2,
			    IEEE80211_FC0_SUBTYPE_DEAUTH,
			    IEEE80211_REASON_ASSOC_NOT_AUTHED);
			ic->ic_stats.is_rx_assoc_notauth++;
			return;
		}
		/* assert right association security credentials */
		badwparsn = 0;
		switch (ic->ic_flags & IEEE80211_F_WPA) {
		case IEEE80211_F_WPA1:
			if (wpa == NULL)
				badwparsn = 1;
			break;
		case IEEE80211_F_WPA2:
			if (rsn == NULL)
				badwparsn = 1;
			break;
		case IEEE80211_F_WPA1|IEEE80211_F_WPA2:
			if (wpa == NULL && rsn == NULL)
				badwparsn = 1;
			break;
		}
		if (badwparsn) {
			IEEE80211_DPRINTF(ic,
			    IEEE80211_MSG_ASSOC | IEEE80211_MSG_WPA,
			    "[%s] no WPA/RSN IE in association request\n",
			    ether_sprintf(wh->i_addr2));
			IEEE80211_SEND_MGMT(ic, ni,
			    IEEE80211_FC0_SUBTYPE_DEAUTH,
			    IEEE80211_REASON_RSN_REQUIRED);
			ieee80211_node_leave(ic, ni);
			ic->ic_stats.is_rx_assoc_badwpaie++;
			return;
		}
		if (wpa != NULL || rsn != NULL) {
			/*
			 * Parse WPA/RSN information element.  Note that
			 * we initialize the param block from the node
			 * state so that information in the IE overrides
			 * our defaults.  The resulting parameters are
			 * installed below after the association is assured.
			 */
			rsnparms = ni->ni_rsn;
			if (wpa != NULL)
				reason = ieee80211_parse_wpa(ic, wpa, &rsnparms, wh);
			else
				reason = ieee80211_parse_rsn(ic, rsn, &rsnparms, wh);
			if (reason != 0) {
				IEEE80211_SEND_MGMT(ic, ni,
				    IEEE80211_FC0_SUBTYPE_DEAUTH, reason);
				ieee80211_node_leave(ic, ni);
				/* XXX distinguish WPA/RSN? */
				ic->ic_stats.is_rx_assoc_badwpaie++;
				return;
			}
			IEEE80211_DPRINTF(ic,
			    IEEE80211_MSG_ASSOC | IEEE80211_MSG_WPA,
			    "[%s] %s ie: mc %u/%u uc %u/%u key %u caps 0x%x\n",
			    ether_sprintf(wh->i_addr2),
			    wpa != NULL ? "WPA" : "RSN",
			    rsnparms.rsn_mcastcipher, rsnparms.rsn_mcastkeylen,
			    rsnparms.rsn_ucastcipher, rsnparms.rsn_ucastkeylen,
			    rsnparms.rsn_keymgmt, rsnparms.rsn_caps);
		}
		/* discard challenge after association */
		if (ni->ni_challenge != NULL) {
			FREE(ni->ni_challenge, M_80211_NODE);
			ni->ni_challenge = NULL;
		}
		/* NB: 802.11 spec says to ignore station's privacy bit */
		if ((capinfo & IEEE80211_CAPINFO_ESS) == 0) {
			capinfomismatch(ni, wh, reassoc, resp,
			    "capability", capinfo);
			return;
		}
		/*
		 * Disallow re-associate w/ invalid slot time setting.
		 */
		if (ni->ni_associd != 0 &&
		    IEEE80211_IS_CHAN_ANYG(ic->ic_bsschan) &&
		    ((ni->ni_capinfo ^ capinfo) & IEEE80211_CAPINFO_SHORT_SLOTTIME)) {
			capinfomismatch(ni, wh, reassoc, resp,
			    "slot time", capinfo);
			return;
		}
		rate = ieee80211_setup_rates(ni, rates, xrates,
				IEEE80211_F_DOSORT | IEEE80211_F_DOFRATE |
				IEEE80211_F_DONEGO | IEEE80211_F_DODEL);
		if (rate & IEEE80211_RATE_BASIC) {
			ratesetmismatch(ni, wh, reassoc, resp, "basic", rate);
			return;
		}
		/*
		 * If constrained to 11g-only stations reject an
		 * 11b-only station.  We cheat a bit here by looking
		 * at the max negotiated xmit rate and assuming anyone
		 * with a best rate <24Mb/s is an 11b station.
		 */
		if ((ic->ic_flags & IEEE80211_F_PUREG) && rate < 48) {
			ratesetmismatch(ni, wh, reassoc, resp, "11g", rate);
			return;
		}
		/* XXX enforce PUREN */
		/* 802.11n-specific rateset handling */
		if (IEEE80211_IS_CHAN_HT(ic->ic_curchan) && htcap != NULL) {
			rate = ieee80211_setup_htrates(ni, htcap,
				IEEE80211_F_DOFRATE | IEEE80211_F_DONEGO |
				IEEE80211_F_DOBRS);
			if (rate & IEEE80211_RATE_BASIC) {
				/* XXX 11n-specific stat */
				ratesetmismatch(ni, wh, reassoc, resp,
				    "HT", rate);
				return;
			}
			ieee80211_ht_node_init(ni, htcap);
		} else if (ni->ni_flags & IEEE80211_NODE_HT)
			ieee80211_ht_node_cleanup(ni);
		ni->ni_rssi = rssi;
		ni->ni_noise = noise;
		ni->ni_rstamp = rstamp;
		ni->ni_intval = lintval;
		ni->ni_capinfo = capinfo;
		ni->ni_chan = ic->ic_bsschan;
		ni->ni_fhdwell = ic->ic_bss->ni_fhdwell;
		ni->ni_fhindex = ic->ic_bss->ni_fhindex;
		if (wpa != NULL) {
			/*
			 * Record WPA parameters for station, mark
			 * node as using WPA and record information element
			 * for applications that require it.
			 */
			ni->ni_rsn = rsnparms;
			ieee80211_saveie(&ni->ni_wpa_ie, wpa);
		} else if (ni->ni_wpa_ie != NULL) {
			/*
			 * Flush any state from a previous association.
			 */
			FREE(ni->ni_wpa_ie, M_80211_NODE);
			ni->ni_wpa_ie = NULL;
		}
		if (rsn != NULL) {
			/*
			 * Record RSN parameters for station, mark
			 * node as using WPA and record information element
			 * for applications that require it.
			 */
			ni->ni_rsn = rsnparms;
			ieee80211_saveie(&ni->ni_rsn_ie, rsn);
		} else if (ni->ni_rsn_ie != NULL) {
			/*
			 * Flush any state from a previous association.
			 */
			FREE(ni->ni_rsn_ie, M_80211_NODE);
			ni->ni_rsn_ie = NULL;
		}
		if (wme != NULL) {
			/*
			 * Record WME parameters for station, mark node
			 * as capable of QoS and record information
			 * element for applications that require it.
			 */
			ieee80211_saveie(&ni->ni_wme_ie, wme);
			ni->ni_flags |= IEEE80211_NODE_QOS;
		} else if (ni->ni_wme_ie != NULL) {
			/*
			 * Flush any state from a previous association.
			 */
			FREE(ni->ni_wme_ie, M_80211_NODE);
			ni->ni_wme_ie = NULL;
			ni->ni_flags &= ~IEEE80211_NODE_QOS;
		}
		if (ath != NULL) {
			/* 
			 * Record ATH parameters for station, mark
			 * node with appropriate capabilities, and
			 * record the information element for
			 * applications that require it.
			 */
			ieee80211_saveath(ni, ath);
		} else if (ni->ni_ath_ie != NULL) {
			/*
			 * Flush any state from a previous association.
			 */
			FREE(ni->ni_ath_ie, M_80211_NODE);
			ni->ni_ath_ie = NULL;
			ni->ni_ath_flags = 0;
		}
		ieee80211_node_join(ic, ni, resp);
		ieee80211_deliver_l2uf(ni);
		break;
	}

	case IEEE80211_FC0_SUBTYPE_ASSOC_RESP:
	case IEEE80211_FC0_SUBTYPE_REASSOC_RESP: {
		uint16_t capinfo, associd;
		uint16_t status;

		if (ic->ic_opmode != IEEE80211_M_STA ||
		    ic->ic_state != IEEE80211_S_ASSOC) {
			ic->ic_stats.is_rx_mgtdiscard++;
			return;
		}

		/*
		 * asresp frame format
		 *	[2] capability information
		 *	[2] status
		 *	[2] association ID
		 *	[tlv] supported rates
		 *	[tlv] extended supported rates
		 *	[tlv] WME
		 *	[tlv] HT capabilities
		 */
		IEEE80211_VERIFY_LENGTH(efrm - frm, 6, return);
		ni = ic->ic_bss;
		capinfo = le16toh(*(uint16_t *)frm);
		frm += 2;
		status = le16toh(*(uint16_t *)frm);
		frm += 2;
		if (status != 0) {
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_ASSOC,
			    "[%s] %sassoc failed (reason %d)\n",
			    ether_sprintf(wh->i_addr2),
			    ISREASSOC(subtype) ?  "re" : "", status);
			if (ni != ic->ic_bss)	/* XXX never true? */
				ni->ni_fails++;
			ic->ic_stats.is_rx_auth_fail++;	/* XXX */
			return;
		}
		associd = le16toh(*(uint16_t *)frm);
		frm += 2;

		rates = xrates = wme = htcap = NULL;
		while (efrm - frm > 1) {
			IEEE80211_VERIFY_LENGTH(efrm - frm, frm[1] + 2, return);
			switch (*frm) {
			case IEEE80211_ELEMID_RATES:
				rates = frm;
				break;
			case IEEE80211_ELEMID_XRATES:
				xrates = frm;
				break;
			case IEEE80211_ELEMID_HTCAP:
				htcap = frm;
				break;
			case IEEE80211_ELEMID_VENDOR:
				if (iswmeoui(frm))
					wme = frm;
				/* XXX Atheros OUI support */
				break;
			}
			frm += frm[1] + 2;
		}

		IEEE80211_VERIFY_ELEMENT(rates, IEEE80211_RATE_MAXSIZE);
		if (xrates != NULL)
			IEEE80211_VERIFY_ELEMENT(xrates,
				IEEE80211_RATE_MAXSIZE - rates[1]);
		rate = ieee80211_setup_rates(ni, rates, xrates,
				IEEE80211_F_JOIN |
				IEEE80211_F_DOSORT | IEEE80211_F_DOFRATE |
				IEEE80211_F_DONEGO | IEEE80211_F_DODEL);
		if (rate & IEEE80211_RATE_BASIC) {
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_ASSOC,
			    "[%s] %sassoc failed (rate set mismatch)\n",
			    ether_sprintf(wh->i_addr2),
			    ISREASSOC(subtype) ?  "re" : "");
			if (ni != ic->ic_bss)	/* XXX never true? */
				ni->ni_fails++;
			ic->ic_stats.is_rx_assoc_norate++;
			ieee80211_new_state(ic, IEEE80211_S_SCAN,
			    IEEE80211_SCAN_FAIL_STATUS);
			return;
		}

		ni->ni_capinfo = capinfo;
		ni->ni_associd = associd;
		if (wme != NULL &&
		    ieee80211_parse_wmeparams(ic, wme, wh) >= 0) {
			ni->ni_flags |= IEEE80211_NODE_QOS;
			ieee80211_wme_updateparams(ic);
		} else
			ni->ni_flags &= ~IEEE80211_NODE_QOS;
		/*
		 * Configure state now that we are associated.
		 *
		 * XXX may need different/additional driver callbacks?
		 */
		if (IEEE80211_IS_CHAN_A(ic->ic_curchan) ||
		    (ni->ni_capinfo & IEEE80211_CAPINFO_SHORT_PREAMBLE)) {
			ic->ic_flags |= IEEE80211_F_SHPREAMBLE;
			ic->ic_flags &= ~IEEE80211_F_USEBARKER;
		} else {
			ic->ic_flags &= ~IEEE80211_F_SHPREAMBLE;
			ic->ic_flags |= IEEE80211_F_USEBARKER;
		}
		ieee80211_set_shortslottime(ic,
			IEEE80211_IS_CHAN_A(ic->ic_curchan) ||
			(ni->ni_capinfo & IEEE80211_CAPINFO_SHORT_SLOTTIME));
		/*
		 * Honor ERP protection.
		 *
		 * NB: ni_erp should zero for non-11g operation.
		 */
		if (IEEE80211_IS_CHAN_ANYG(ic->ic_curchan) &&
		    (ni->ni_erp & IEEE80211_ERP_USE_PROTECTION))
			ic->ic_flags |= IEEE80211_F_USEPROT;
		else
			ic->ic_flags &= ~IEEE80211_F_USEPROT;
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_ASSOC,
		    "[%s] %sassoc success: %s preamble, %s slot time%s%s%s%s\n",
		    ether_sprintf(wh->i_addr2),
		    ISREASSOC(subtype) ? "re" : "",
		    ic->ic_flags&IEEE80211_F_SHPREAMBLE ? "short" : "long",
		    ic->ic_flags&IEEE80211_F_SHSLOT ? "short" : "long",
		    ic->ic_flags&IEEE80211_F_USEPROT ? ", protection" : "",
		    ni->ni_flags & IEEE80211_NODE_QOS ? ", QoS" : "",
		    IEEE80211_ATH_CAP(ic, ni, IEEE80211_NODE_FF) ?
			", fast-frames" : "",
		    IEEE80211_ATH_CAP(ic, ni, IEEE80211_NODE_TURBOP) ?
			", turbo" : ""
		);
		ieee80211_new_state(ic, IEEE80211_S_RUN, subtype);
		break;
	}

	case IEEE80211_FC0_SUBTYPE_DEAUTH: {
		uint16_t reason;

		if (ic->ic_state == IEEE80211_S_SCAN) {
			ic->ic_stats.is_rx_mgtdiscard++;
			return;
		}
		/*
		 * deauth frame format
		 *	[2] reason
		 */
		IEEE80211_VERIFY_LENGTH(efrm - frm, 2, return);
		reason = le16toh(*(uint16_t *)frm);
		ic->ic_stats.is_rx_deauth++;
		IEEE80211_NODE_STAT(ni, rx_deauth);

		if (!IEEE80211_ADDR_EQ(wh->i_addr1, ic->ic_myaddr)) {
			/* NB: can happen when in promiscuous mode */
			ic->ic_stats.is_rx_mgtdiscard++;
			break;
		}
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_AUTH,
		    "[%s] recv deauthenticate (reason %d)\n",
		    ether_sprintf(ni->ni_macaddr), reason);
		switch (ic->ic_opmode) {
		case IEEE80211_M_STA:
			ieee80211_new_state(ic, IEEE80211_S_AUTH,
			    (reason << 8) | IEEE80211_FC0_SUBTYPE_DEAUTH);
			break;
		case IEEE80211_M_HOSTAP:
			if (ni != ic->ic_bss)
				ieee80211_node_leave(ic, ni);
			break;
		default:
			ic->ic_stats.is_rx_mgtdiscard++;
			break;
		}
		break;
	}

	case IEEE80211_FC0_SUBTYPE_DISASSOC: {
		uint16_t reason;

		if (ic->ic_state != IEEE80211_S_RUN &&
		    ic->ic_state != IEEE80211_S_ASSOC &&
		    ic->ic_state != IEEE80211_S_AUTH) {
			ic->ic_stats.is_rx_mgtdiscard++;
			return;
		}
		/*
		 * disassoc frame format
		 *	[2] reason
		 */
		IEEE80211_VERIFY_LENGTH(efrm - frm, 2, return);
		reason = le16toh(*(uint16_t *)frm);
		ic->ic_stats.is_rx_disassoc++;
		IEEE80211_NODE_STAT(ni, rx_disassoc);

		if (!IEEE80211_ADDR_EQ(wh->i_addr1, ic->ic_myaddr)) {
			/* NB: can happen when in promiscuous mode */
			ic->ic_stats.is_rx_mgtdiscard++;
			break;
		}
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_ASSOC,
		    "[%s] recv disassociate (reason %d)\n",
		    ether_sprintf(ni->ni_macaddr), reason);
		switch (ic->ic_opmode) {
		case IEEE80211_M_STA:
			ieee80211_new_state(ic, IEEE80211_S_ASSOC, 0);
			break;
		case IEEE80211_M_HOSTAP:
			if (ni != ic->ic_bss)
				ieee80211_node_leave(ic, ni);
			break;
		default:
			ic->ic_stats.is_rx_mgtdiscard++;
			break;
		}
		break;
	}

	case IEEE80211_FC0_SUBTYPE_ACTION: {
		const struct ieee80211_action *ia;

		if (ic->ic_state != IEEE80211_S_RUN &&
		    ic->ic_state != IEEE80211_S_ASSOC &&
		    ic->ic_state != IEEE80211_S_AUTH) {
			ic->ic_stats.is_rx_mgtdiscard++;
			return;
		}
		/*
		 * action frame format:
		 *	[1] category
		 *	[1] action
		 *	[tlv] parameters
		 */
		IEEE80211_VERIFY_LENGTH(efrm - frm,
			sizeof(struct ieee80211_action), return);
		ia = (const struct ieee80211_action *) frm;

		ic->ic_stats.is_rx_action++;
		IEEE80211_NODE_STAT(ni, rx_action);

		/* verify frame payloads but defer processing */
		/* XXX maybe push this to method */
		switch (ia->ia_category) {
		case IEEE80211_ACTION_CAT_BA:
			switch (ia->ia_action) {
			case IEEE80211_ACTION_BA_ADDBA_REQUEST:
				IEEE80211_VERIFY_LENGTH(efrm - frm,
				    sizeof(struct ieee80211_action_ba_addbarequest),
				    return);
				break;
			case IEEE80211_ACTION_BA_ADDBA_RESPONSE:
				IEEE80211_VERIFY_LENGTH(efrm - frm,
				    sizeof(struct ieee80211_action_ba_addbaresponse),
				    return);
				break;
			case IEEE80211_ACTION_BA_DELBA:
				IEEE80211_VERIFY_LENGTH(efrm - frm,
				    sizeof(struct ieee80211_action_ba_delba),
				    return);
				break;
			}
			break;
		case IEEE80211_ACTION_CAT_HT:
			switch (ia->ia_action) {
			case IEEE80211_ACTION_HT_TXCHWIDTH:
				IEEE80211_VERIFY_LENGTH(efrm - frm,
				    sizeof(struct ieee80211_action_ht_txchwidth),
				    return);
				break;
			}
			break;
		}
		ic->ic_recv_action(ni, frm, efrm);
		break;
	}

	default:
		IEEE80211_DISCARD(ic, IEEE80211_MSG_ANY,
		     wh, "mgt", "subtype 0x%x not handled", subtype);
		ic->ic_stats.is_rx_badsubtype++;
		break;
	}
#undef ISREASSOC
#undef ISPROBE
}
#undef IEEE80211_VERIFY_LENGTH
#undef IEEE80211_VERIFY_ELEMENT

/*
 * Process a received ps-poll frame.
 */
static void
ieee80211_recv_pspoll(struct ieee80211com *ic,
	struct ieee80211_node *ni, struct mbuf *m0)
{
	struct ieee80211_frame_min *wh;
	struct mbuf *m;
	uint16_t aid;
	int qlen;

	wh = mtod(m0, struct ieee80211_frame_min *);
	if (ni->ni_associd == 0) {
		IEEE80211_DISCARD(ic, IEEE80211_MSG_POWER | IEEE80211_MSG_DEBUG,
		    (struct ieee80211_frame *) wh, "ps-poll",
		    "%s", "unassociated station");
		ic->ic_stats.is_ps_unassoc++;
		IEEE80211_SEND_MGMT(ic, ni, IEEE80211_FC0_SUBTYPE_DEAUTH,
			IEEE80211_REASON_NOT_ASSOCED);
		return;
	}

	aid = le16toh(*(uint16_t *)wh->i_dur);
	if (aid != ni->ni_associd) {
		IEEE80211_DISCARD(ic, IEEE80211_MSG_POWER | IEEE80211_MSG_DEBUG,
		    (struct ieee80211_frame *) wh, "ps-poll",
		    "aid mismatch: sta aid 0x%x poll aid 0x%x",
		    ni->ni_associd, aid);
		ic->ic_stats.is_ps_badaid++;
		IEEE80211_SEND_MGMT(ic, ni, IEEE80211_FC0_SUBTYPE_DEAUTH,
			IEEE80211_REASON_NOT_ASSOCED);
		return;
	}

	/* Okay, take the first queued packet and put it out... */
	IEEE80211_NODE_SAVEQ_DEQUEUE(ni, m, qlen);
	if (m == NULL) {
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_POWER,
		    "[%s] recv ps-poll, but queue empty\n",
		    ether_sprintf(wh->i_addr2));
		ieee80211_send_nulldata(ieee80211_ref_node(ni));
		ic->ic_stats.is_ps_qempty++;	/* XXX node stat */
		if (ic->ic_set_tim != NULL)
			ic->ic_set_tim(ni, 0);	/* just in case */
		return;
	}
	/* 
	 * If there are more packets, set the more packets bit
	 * in the packet dispatched to the station; otherwise
	 * turn off the TIM bit.
	 */
	if (qlen != 0) {
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_POWER,
		    "[%s] recv ps-poll, send packet, %u still queued\n",
		    ether_sprintf(ni->ni_macaddr), qlen);
		m->m_flags |= M_MORE_DATA;
	} else {
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_POWER,
		    "[%s] recv ps-poll, send packet, queue empty\n",
		    ether_sprintf(ni->ni_macaddr));
		if (ic->ic_set_tim != NULL)
			ic->ic_set_tim(ni, 0);
	}
	m->m_flags |= M_PWR_SAV;		/* bypass PS handling */
	IF_ENQUEUE(&ic->ic_ifp->if_snd, m);
}

#ifdef IEEE80211_DEBUG
/*
 * Debugging support.
 */

/*
 * Return the bssid of a frame.
 */
static const uint8_t *
ieee80211_getbssid(struct ieee80211com *ic, const struct ieee80211_frame *wh)
{
	if (ic->ic_opmode == IEEE80211_M_STA)
		return wh->i_addr2;
	if ((wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) != IEEE80211_FC1_DIR_NODS)
		return wh->i_addr1;
	if ((wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) == IEEE80211_FC0_SUBTYPE_PS_POLL)
		return wh->i_addr1;
	return wh->i_addr3;
}

void
ieee80211_note(struct ieee80211com *ic, const char *fmt, ...)
{
	char buf[128];		/* XXX */
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	if_printf(ic->ic_ifp, "%s", buf);	/* NB: no \n */
}

void
ieee80211_note_frame(struct ieee80211com *ic,
	const struct ieee80211_frame *wh,
	const char *fmt, ...)
{
	char buf[128];		/* XXX */
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	if_printf(ic->ic_ifp, "[%s] %s\n",
		ether_sprintf(ieee80211_getbssid(ic, wh)), buf);
}

void
ieee80211_note_mac(struct ieee80211com *ic,
	const uint8_t mac[IEEE80211_ADDR_LEN],
	const char *fmt, ...)
{
	char buf[128];		/* XXX */
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	if_printf(ic->ic_ifp, "[%s] %s\n", ether_sprintf(mac), buf);
}

void
ieee80211_discard_frame(struct ieee80211com *ic,
	const struct ieee80211_frame *wh,
	const char *type, const char *fmt, ...)
{
	va_list ap;

	printf("[%s:%s] discard ", ic->ic_ifp->if_xname,
		ether_sprintf(ieee80211_getbssid(ic, wh)));
	if (type != NULL)
		printf("%s frame, ", type);
	else
		printf("frame, ");
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	printf("\n");
}

void
ieee80211_discard_ie(struct ieee80211com *ic,
	const struct ieee80211_frame *wh,
	const char *type, const char *fmt, ...)
{
	va_list ap;

	printf("[%s:%s] discard ", ic->ic_ifp->if_xname,
		ether_sprintf(ieee80211_getbssid(ic, wh)));
	if (type != NULL)
		printf("%s information element, ", type);
	else
		printf("information element, ");
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	printf("\n");
}

void
ieee80211_discard_mac(struct ieee80211com *ic,
	const uint8_t mac[IEEE80211_ADDR_LEN],
	const char *type, const char *fmt, ...)
{
	va_list ap;

	printf("[%s:%s] discard ", ic->ic_ifp->if_xname, ether_sprintf(mac));
	if (type != NULL)
		printf("%s frame, ", type);
	else
		printf("frame, ");
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	printf("\n");
}
#endif /* IEEE80211_DEBUG */
