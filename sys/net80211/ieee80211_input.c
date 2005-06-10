/*-
 * Copyright (c) 2001 Atsushi Onoe
 * Copyright (c) 2002-2005 Sam Leffler, Errno Consulting
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
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

/*
 * Emit a debug message about discarding a frame or information
 * element.  One format is for extracting the mac address from
 * the frame header; the other is for when a header is not
 * available or otherwise appropriate.
 */
#define	IEEE80211_DISCARD(_ic, _m, _wh, _type, _fmt, ...) do {		\
	if ((_ic)->ic_debug & (_m))					\
		ieee80211_discard_frame(_ic, _wh, _type, _fmt, __VA_ARGS__);\
} while (0)
#define	IEEE80211_DISCARD_IE(_ic, _m, _wh, _type, _fmt, ...) do {	\
	if ((_ic)->ic_debug & (_m))					\
		ieee80211_discard_ie(_ic, _wh, _type, _fmt, __VA_ARGS__);\
} while (0)
#define	IEEE80211_DISCARD_MAC(_ic, _m, _mac, _type, _fmt, ...) do {	\
	if ((_ic)->ic_debug & (_m))					\
		ieee80211_discard_mac(_ic, _mac, _type, _fmt, __VA_ARGS__);\
} while (0)

static const u_int8_t *ieee80211_getbssid(struct ieee80211com *,
	const struct ieee80211_frame *);
static void ieee80211_discard_frame(struct ieee80211com *,
	const struct ieee80211_frame *, const char *type, const char *fmt, ...);
static void ieee80211_discard_ie(struct ieee80211com *,
	const struct ieee80211_frame *, const char *type, const char *fmt, ...);
static void ieee80211_discard_mac(struct ieee80211com *,
	const u_int8_t mac[IEEE80211_ADDR_LEN], const char *type,
	const char *fmt, ...);
#else
#define	IEEE80211_DISCARD(_ic, _m, _wh, _type, _fmt, ...)
#define	IEEE80211_DISCARD_IE(_ic, _m, _wh, _type, _fmt, ...)
#define	IEEE80211_DISCARD_MAC(_ic, _m, _mac, _type, _fmt, ...)
#endif /* IEEE80211_DEBUG */

static struct mbuf *ieee80211_defrag(struct ieee80211com *,
	struct ieee80211_node *, struct mbuf *);
static struct mbuf *ieee80211_decap(struct ieee80211com *, struct mbuf *);
static void ieee80211_send_error(struct ieee80211com *, struct ieee80211_node *,
		const u_int8_t *mac, int subtype, int arg);
static void ieee80211_node_pwrsave(struct ieee80211_node *, int enable);
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
	struct ieee80211_node *ni, int rssi, u_int32_t rstamp)
{
#define	SEQ_LEQ(a,b)	((int)((a)-(b)) <= 0)
#define	HAS_SEQ(type)	((type & 0x4) == 0)
	struct ifnet *ifp = ic->ic_ifp;
	struct ieee80211_frame *wh;
	struct ieee80211_key *key;
	struct ether_header *eh;
	int len, hdrsize, off;
	u_int8_t dir, type, subtype;
	u_int8_t *bssid;
	u_int16_t rxseq;

	KASSERT(ni != NULL, ("null node"));
	ni->ni_inact = ni->ni_inact_reload;

	/* trim CRC here so WEP can find its own CRC at the end of packet. */
	if (m->m_flags & M_HASFCS) {
		m_adj(m, -IEEE80211_CRC_LEN);
		m->m_flags &= ~M_HASFCS;
	}
	KASSERT(m->m_pkthdr.len >= sizeof(struct ieee80211_frame_min),
		("frame length too short: %u", m->m_pkthdr.len));

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
			    ic->ic_opmode != IEEE80211_M_HOSTAP) {
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
		ni->ni_rstamp = rstamp;
		if (HAS_SEQ(type)) {
			u_int8_t tid;
			if (IEEE80211_QOS_HAS_SEQ(wh)) {
				tid = ((struct ieee80211_qosframe *)wh)->
					i_qos[0] & IEEE80211_QOS_TID;
				if (tid >= WME_AC_VI)
					ic->ic_wme.wme_hipri_traffic++;
				tid++;
			} else
				tid = 0;
			rxseq = le16toh(*(u_int16_t *)wh->i_seq);
			if ((wh->i_fc[1] & IEEE80211_FC1_RETRY) &&
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
		hdrsize = ieee80211_hdrsize(wh);
		if (ic->ic_flags & IEEE80211_F_DATAPAD)
			hdrsize = roundup(hdrsize, sizeof(u_int32_t));
		if (m->m_len < hdrsize &&
		    (m = m_pullup(m, hdrsize)) == NULL) {
			IEEE80211_DISCARD_MAC(ic, IEEE80211_MSG_ANY,
			    ni->ni_macaddr, NULL,
			    "data too short: expecting %u", hdrsize);
			ic->ic_stats.is_rx_tooshort++;
			goto out;		/* XXX */
		}
		if (subtype & IEEE80211_FC0_SUBTYPE_QOS) {
			/* XXX discard if node w/o IEEE80211_NODE_QOS? */
			/*
			 * Strip QoS control and any padding so only a
			 * stock 802.11 header is at the front.
			 */
			/* XXX 4-address QoS frame */
			off = hdrsize - sizeof(struct ieee80211_frame);
			ovbcopy(mtod(m, u_int8_t *), mtod(m, u_int8_t *) + off,
				hdrsize - off);
			m_adj(m, off);
			wh = mtod(m, struct ieee80211_frame *);
			wh->i_fc[0] &= ~IEEE80211_FC0_SUBTYPE_QOS;
		} else {
			/* XXX copy up for 4-address frames w/ padding */
		}
		switch (ic->ic_opmode) {
		case IEEE80211_M_STA:
			if (dir != IEEE80211_FC1_DIR_FROMDS) {
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
				ic->ic_stats.is_rx_wrongdir++;
				goto out;
			}
			/* XXX no power-save support */
			break;
		case IEEE80211_M_HOSTAP:
			if (dir != IEEE80211_FC1_DIR_TODS) {
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
			key = ieee80211_crypto_decap(ic, ni, m);
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
		 * Next up, any fragmentation.
		 */
		if (!IEEE80211_IS_MULTICAST(wh->i_addr1)) {
			m = ieee80211_defrag(ic, ni, m);
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
			IEEE80211_NODE_STAT(ni, rx_demicfail);
			goto out;
		}

		/* copy to listener after decrypt */
		if (ic->ic_rawbpf)
			bpf_mtap(ic->ic_rawbpf, m);

		/*
		 * Finally, strip the 802.11 header.
		 */
		m = ieee80211_decap(ic, m);
		if (m == NULL) {
			/* don't count Null data frames as errors */
			if (subtype == IEEE80211_FC0_SUBTYPE_NODATA)
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
		ifp->if_ipackets++;
		IEEE80211_NODE_STAT(ni, rx_data);
		IEEE80211_NODE_STAT_ADD(ni, rx_bytes, m->m_pkthdr.len);

		/* perform as a bridge within the AP */
		if (ic->ic_opmode == IEEE80211_M_HOSTAP &&
		    (ic->ic_flags & IEEE80211_F_NOBRIDGE) == 0) {
			struct mbuf *m1 = NULL;

			if (ETHER_IS_MULTICAST(eh->ether_dhost)) {
				m1 = m_copypacket(m, M_DONTWAIT);
				if (m1 == NULL)
					ifp->if_oerrors++;
				else
					m1->m_flags |= M_MCAST;
			} else {
				/* XXX this dups work done in ieee80211_encap */
				/* check if destination is associated */
				struct ieee80211_node *ni1 =
				    ieee80211_find_node(&ic->ic_sta,
							eh->ether_dhost);
				if (ni1 != NULL) {
					/* XXX check if authorized */
					if (ni1->ni_associd != 0) {
						m1 = m;
						m = NULL;
					}
					/* XXX statistic? */
					ieee80211_free_node(ni1);
				}
			}
			if (m1 != NULL) {
				len = m1->m_pkthdr.len;
				IF_ENQUEUE(&ifp->if_snd, m1);
				if (m != NULL)
					ifp->if_omcasts++;
				ifp->if_obytes += len;
			}
		}
		if (m != NULL) {
			if (ni->ni_vlan != 0) {
				/* attach vlan tag */
				/* XXX goto err? */
				VLAN_INPUT_TAG(ifp, m, ni->ni_vlan, goto out);
			}
			(*ifp->if_input)(ifp, m);
		}
		return IEEE80211_FC0_TYPE_DATA;

	case IEEE80211_FC0_TYPE_MGT:
		IEEE80211_NODE_STAT(ni, rx_mgmt);
		if (dir != IEEE80211_FC1_DIR_NODS) {
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
			key = ieee80211_crypto_decap(ic, ni, m);
			if (key == NULL) {
				/* NB: stats+msgs handled in crypto_decap */
				goto out;
			}
			wh = mtod(m, struct ieee80211_frame *);
			wh->i_fc[1] &= ~IEEE80211_FC1_WEP;
		}
		if (ic->ic_rawbpf)
			bpf_mtap(ic->ic_rawbpf, m);
		(*ic->ic_recv_mgmt)(ic, m, ni, subtype, rssi, rstamp);
		m_freem(m);
		return type;

	case IEEE80211_FC0_TYPE_CTL:
		IEEE80211_NODE_STAT(ni, rx_ctrl);
		ic->ic_stats.is_rx_ctl++;
		if (ic->ic_opmode == IEEE80211_M_HOSTAP) {
			switch (subtype) {
			case IEEE80211_FC0_SUBTYPE_PS_POLL:
				ieee80211_recv_pspoll(ic, ni, m);
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
		if (ic->ic_rawbpf)
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
	struct mbuf *m)
{
	struct ieee80211_frame *wh = mtod(m, struct ieee80211_frame *);
	struct ieee80211_frame *lwh;
	u_int16_t rxseq;
	u_int8_t fragno;
	u_int8_t more_frag = wh->i_fc[1] & IEEE80211_FC1_MORE_FRAG;
	struct mbuf *mfrag;

	KASSERT(!IEEE80211_IS_MULTICAST(wh->i_addr1), ("multicast fragm?"));

	rxseq = le16toh(*(u_int16_t *)wh->i_seq);
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
		u_int16_t last_rxseq;

		lwh = mtod(mfrag, struct ieee80211_frame *);
		last_rxseq = le16toh(*(u_int16_t *)lwh->i_seq);
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
			IEEE80211_NODE_STAT(ni, rx_defrag);
			m_freem(m);
			return NULL;
		}
		mfrag = m;
	} else {				/* concatenate */
		m_cat(mfrag, m);
		/* NB: m_cat doesn't update the packet header */
		mfrag->m_pkthdr.len += m->m_pkthdr.len;
		/* track last seqnum and fragno */
		lwh = mtod(mfrag, struct ieee80211_frame *);
		*(u_int16_t *) lwh->i_seq = *(u_int16_t *) wh->i_seq;
	}
	if (more_frag) {			/* more to come, save */
		ni->ni_rxfragstamp = ticks;
		ni->ni_rxfrag[0] = mfrag;
		mfrag = NULL;
	}
	return mfrag;
}

static struct mbuf *
ieee80211_decap(struct ieee80211com *ic, struct mbuf *m)
{
	struct ieee80211_frame wh;	/* NB: QoS stripped above */
	struct ether_header *eh;
	struct llc *llc;

	if (m->m_len < sizeof(wh) + sizeof(*llc) &&
	    (m = m_pullup(m, sizeof(wh) + sizeof(*llc))) == NULL) {
		/* XXX stat, msg */
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
		IEEE80211_DISCARD(ic, IEEE80211_MSG_ANY,
		    &wh, "data", "%s", "DS to DS not supported");
		m_freem(m);
		return NULL;
	}
#ifdef ALIGNED_POINTER
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
 * Install received rate set information in the node's state block.
 */
static int
ieee80211_setup_rates(struct ieee80211com *ic, struct ieee80211_node *ni,
	u_int8_t *rates, u_int8_t *xrates, int flags)
{
	struct ieee80211_rateset *rs = &ni->ni_rates;

	memset(rs, 0, sizeof(*rs));
	rs->rs_nrates = rates[1];
	memcpy(rs->rs_rates, rates + 2, rs->rs_nrates);
	if (xrates != NULL) {
		u_int8_t nxrates;
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
	return ieee80211_fix_rate(ic, ni, flags);
}

static void
ieee80211_auth_open(struct ieee80211com *ic, struct ieee80211_frame *wh,
    struct ieee80211_node *ni, int rssi, u_int32_t rstamp, u_int16_t seq,
    u_int16_t status)
{

	if (ni->ni_authmode == IEEE80211_AUTH_SHARED) {
		IEEE80211_DISCARD_MAC(ic, IEEE80211_MSG_AUTH,
		    ni->ni_macaddr, "open auth",
		    "bad sta auth mode %u", ni->ni_authmode);
		ic->ic_stats.is_rx_bad_auth++;	/* XXX */
		if (ic->ic_opmode == IEEE80211_M_HOSTAP) {
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
		} else
			(void) ieee80211_ref_node(ni);
		IEEE80211_SEND_MGMT(ic, ni,
			IEEE80211_FC0_SUBTYPE_AUTH, seq + 1);
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_DEBUG | IEEE80211_MSG_AUTH,
		    "[%s] station authenticated (open)\n",
		    ether_sprintf(ni->ni_macaddr));
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
			return;
		}
		ieee80211_new_state(ic, IEEE80211_S_ASSOC,
		    wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK);
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
	const u_int8_t *mac, int subtype, int arg)
{
	int istmp;

	if (ni == ic->ic_bss) {
		ni = ieee80211_dup_bss(&ic->ic_sta, mac);
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
		MALLOC(ni->ni_challenge, u_int32_t*, IEEE80211_CHALLENGE_LEN,
		    M_DEVBUF, M_NOWAIT);
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
    u_int8_t *frm, u_int8_t *efrm, struct ieee80211_node *ni, int rssi,
    u_int32_t rstamp, u_int16_t seq, u_int16_t status)
{
	u_int8_t *challenge;
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
				(void) ieee80211_ref_node(ni);
				allocbs = 0;
			}
			ni->ni_rssi = rssi;
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
				FREE(ni->ni_challenge, M_DEVBUF);
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
			ieee80211_new_state(ic, IEEE80211_S_ASSOC,
			    wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK);
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

#define	IEEE80211_VERIFY_LENGTH(_len, _minlen) do {			\
	if ((_len) < (_minlen)) {					\
		IEEE80211_DISCARD(ic, IEEE80211_MSG_ELEMID,		\
		    wh, ieee80211_mgt_subtype_name[subtype >>		\
			IEEE80211_FC0_SUBTYPE_SHIFT],			\
		    "%s", "ie too short");				\
		ic->ic_stats.is_rx_elem_toosmall++;			\
		return;							\
	}								\
} while (0)

#ifdef IEEE80211_DEBUG
static void
ieee80211_ssid_mismatch(struct ieee80211com *ic, const char *tag,
	u_int8_t mac[IEEE80211_ADDR_LEN], u_int8_t *ssid)
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
	((u_int16_t)					\
	 ((((const u_int8_t *)(p))[0]      ) |		\
	  (((const u_int8_t *)(p))[1] <<  8)))
#define LE_READ_4(p)					\
	((u_int32_t)					\
	 ((((const u_int8_t *)(p))[0]      ) |		\
	  (((const u_int8_t *)(p))[1] <<  8) |		\
	  (((const u_int8_t *)(p))[2] << 16) |		\
	  (((const u_int8_t *)(p))[3] << 24)))

static int __inline
iswpaoui(const u_int8_t *frm)
{
	return frm[1] > 3 && LE_READ_4(frm+2) == ((WPA_OUI_TYPE<<24)|WPA_OUI);
}

static int __inline
iswmeoui(const u_int8_t *frm)
{
	return frm[1] > 3 && LE_READ_4(frm+2) == ((WME_OUI_TYPE<<24)|WME_OUI);
}

static int __inline
iswmeparam(const u_int8_t *frm)
{
	return frm[1] > 5 && LE_READ_4(frm+2) == ((WME_OUI_TYPE<<24)|WME_OUI) &&
		frm[6] == WME_PARAM_OUI_SUBTYPE;
}

static int __inline
iswmeinfo(const u_int8_t *frm)
{
	return frm[1] > 5 && LE_READ_4(frm+2) == ((WME_OUI_TYPE<<24)|WME_OUI) &&
		frm[6] == WME_INFO_OUI_SUBTYPE;
}

static int __inline
isatherosoui(const u_int8_t *frm)
{
	return frm[1] > 3 && LE_READ_4(frm+2) == ((ATH_OUI_TYPE<<24)|ATH_OUI);
}

/*
 * Convert a WPA cipher selector OUI to an internal
 * cipher algorithm.  Where appropriate we also
 * record any key length.
 */
static int
wpa_cipher(u_int8_t *sel, u_int8_t *keylen)
{
#define	WPA_SEL(x)	(((x)<<24)|WPA_OUI)
	u_int32_t w = LE_READ_4(sel);

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
wpa_keymgmt(u_int8_t *sel)
{
#define	WPA_SEL(x)	(((x)<<24)|WPA_OUI)
	u_int32_t w = LE_READ_4(sel);

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
ieee80211_parse_wpa(struct ieee80211com *ic, u_int8_t *frm,
	struct ieee80211_rsnparms *rsn, const struct ieee80211_frame *wh)
{
	u_int8_t len = frm[1];
	u_int32_t w;
	int n;

	/*
	 * Check the length once for fixed parts: OUI, type,
	 * version, mcast cipher, and 2 selector counts.
	 * Other, variable-length data, must be checked separately.
	 */
	KASSERT(ic->ic_flags & IEEE80211_F_WPA1,
		("not WPA, flags 0x%x", ic->ic_flags));
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
rsn_cipher(u_int8_t *sel, u_int8_t *keylen)
{
#define	RSN_SEL(x)	(((x)<<24)|RSN_OUI)
	u_int32_t w = LE_READ_4(sel);

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
rsn_keymgmt(u_int8_t *sel)
{
#define	RSN_SEL(x)	(((x)<<24)|RSN_OUI)
	u_int32_t w = LE_READ_4(sel);

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
ieee80211_parse_rsn(struct ieee80211com *ic, u_int8_t *frm,
	struct ieee80211_rsnparms *rsn, const struct ieee80211_frame *wh)
{
	u_int8_t len = frm[1];
	u_int32_t w;
	int n;

	/*
	 * Check the length once for fixed parts: 
	 * version, mcast cipher, and 2 selector counts.
	 * Other, variable-length data, must be checked separately.
	 */
	KASSERT(ic->ic_flags & IEEE80211_F_WPA2,
		("not RSN, flags 0x%x", ic->ic_flags));
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
ieee80211_parse_wmeparams(struct ieee80211com *ic, u_int8_t *frm,
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

static void
ieee80211_saveie(u_int8_t **iep, const u_int8_t *ie)
{
	u_int ielen = ie[1]+2;
	/*
	 * Record information element for later use.
	 */
	if (*iep == NULL || (*iep)[1] != ie[1]) {
		if (*iep != NULL)
			FREE(*iep, M_DEVBUF);
		MALLOC(*iep, void*, ielen, M_DEVBUF, M_NOWAIT);
	}
	if (*iep != NULL)
		memcpy(*iep, ie, ielen);
	/* XXX note failure */
}

#ifdef IEEE80211_DEBUG
static void
dump_probe_beacon(u_int8_t subtype, int isnew,
	const u_int8_t mac[IEEE80211_ADDR_LEN],
	u_int8_t chan, u_int8_t bchan, u_int16_t capinfo, u_int16_t bintval,
	u_int8_t erp, u_int8_t *ssid, u_int8_t *country)
{
	printf("[%s] %s%s on chan %u (bss chan %u) ",
	    ether_sprintf(mac), isnew ? "new " : "",
	    ieee80211_mgt_subtype_name[subtype >> IEEE80211_FC0_SUBTYPE_SHIFT],
	    chan, bchan);
	ieee80211_print_essid(ssid + 2, ssid[1]);
	printf("\n");

	if (isnew) {
		printf("[%s] caps 0x%x bintval %u erp 0x%x", 
			ether_sprintf(mac), capinfo, bintval, erp);
		if (country) {
#ifdef __FreeBSD__
			printf(" country info %*D", country[1], country+2, " ");
#else
			int i;
			printf(" country info");
			for (i = 0; i < country[1]; i++)
				printf(" %02x", country[i+2]);
#endif
		}
		printf("\n");
	}
}
#endif /* IEEE80211_DEBUG */

void
ieee80211_recv_mgmt(struct ieee80211com *ic, struct mbuf *m0,
	struct ieee80211_node *ni,
	int subtype, int rssi, u_int32_t rstamp)
{
#define	ISPROBE(_st)	((_st) == IEEE80211_FC0_SUBTYPE_PROBE_RESP)
#define	ISREASSOC(_st)	((_st) == IEEE80211_FC0_SUBTYPE_REASSOC_RESP)
	struct ieee80211_frame *wh;
	u_int8_t *frm, *efrm;
	u_int8_t *ssid, *rates, *xrates, *wpa, *wme;
	int reassoc, resp, allocbs;
	u_int8_t rate;

	wh = mtod(m0, struct ieee80211_frame *);
	frm = (u_int8_t *)&wh[1];
	efrm = mtod(m0, u_int8_t *) + m0->m_len;
	switch (subtype) {
	case IEEE80211_FC0_SUBTYPE_PROBE_RESP:
	case IEEE80211_FC0_SUBTYPE_BEACON: {
		u_int8_t *tstamp, *country, *tim;
		u_int8_t chan, bchan, fhindex, erp;
		u_int16_t capinfo, bintval, timoff;
		u_int16_t fhdwell;

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
		 */
		IEEE80211_VERIFY_LENGTH(efrm - frm, 12);
		tstamp  = frm;				frm += 8;
		bintval = le16toh(*(u_int16_t *)frm);	frm += 2;
		capinfo = le16toh(*(u_int16_t *)frm);	frm += 2;
		ssid = rates = xrates = country = wpa = wme = tim = NULL;
		bchan = ieee80211_chan2ieee(ic, ic->ic_bss->ni_chan);
		chan = bchan;
		fhdwell = 0;
		fhindex = 0;
		erp = 0;
		timoff = 0;
		while (frm < efrm) {
			switch (*frm) {
			case IEEE80211_ELEMID_SSID:
				ssid = frm;
				break;
			case IEEE80211_ELEMID_RATES:
				rates = frm;
				break;
			case IEEE80211_ELEMID_COUNTRY:
				country = frm;
				break;
			case IEEE80211_ELEMID_FHPARMS:
				if (ic->ic_phytype == IEEE80211_T_FH) {
					fhdwell = LE_READ_2(&frm[2]);
					chan = IEEE80211_FH_CHAN(frm[4], frm[5]);
					fhindex = frm[6];
				}
				break;
			case IEEE80211_ELEMID_DSPARMS:
				/*
				 * XXX hack this since depending on phytype
				 * is problematic for multi-mode devices.
				 */
				if (ic->ic_phytype != IEEE80211_T_FH)
					chan = frm[2];
				break;
			case IEEE80211_ELEMID_TIM:
				/* XXX ATIM? */
				tim = frm;
				timoff = frm - mtod(m0, u_int8_t *);
				break;
			case IEEE80211_ELEMID_IBSSPARMS:
				break;
			case IEEE80211_ELEMID_XRATES:
				xrates = frm;
				break;
			case IEEE80211_ELEMID_ERP:
				if (frm[1] != 1) {
					IEEE80211_DISCARD_IE(ic,
					    IEEE80211_MSG_ELEMID, wh, "ERP",
					    "bad len %u", frm[1]);
					ic->ic_stats.is_rx_elem_toobig++;
					break;
				}
				erp = frm[2];
				break;
			case IEEE80211_ELEMID_RSN:
				wpa = frm;
				break;
			case IEEE80211_ELEMID_VENDOR:
				if (iswpaoui(frm))
					wpa = frm;
				else if (iswmeparam(frm) || iswmeinfo(frm))
					wme = frm;
				/* XXX Atheros OUI support */
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
		IEEE80211_VERIFY_ELEMENT(rates, IEEE80211_RATE_MAXSIZE);
		IEEE80211_VERIFY_ELEMENT(ssid, IEEE80211_NWID_LEN);
		if (
#if IEEE80211_CHAN_MAX < 255
		    chan > IEEE80211_CHAN_MAX ||
#endif
		    isclr(ic->ic_chan_active, chan)) {
			IEEE80211_DISCARD(ic, IEEE80211_MSG_ELEMID,
			    wh, ieee80211_mgt_subtype_name[subtype >>
				IEEE80211_FC0_SUBTYPE_SHIFT],
			    "invalid channel %u", chan);
			ic->ic_stats.is_rx_badchan++;
			return;
		}
		if (chan != bchan && ic->ic_phytype != IEEE80211_T_FH) {
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
			IEEE80211_DISCARD(ic, IEEE80211_MSG_ELEMID,
			    wh, ieee80211_mgt_subtype_name[subtype >>
				IEEE80211_FC0_SUBTYPE_SHIFT],
			    "for off-channel %u", chan);
			ic->ic_stats.is_rx_chanmismatch++;
			return;
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
			memcpy(ni->ni_tstamp.data, tstamp,
				sizeof(ni->ni_tstamp));
			if (ni->ni_erp != erp) {
				IEEE80211_DPRINTF(ic, IEEE80211_MSG_ASSOC,
				    "[%s] erp change: was 0x%x, now 0x%x\n",
				    ether_sprintf(wh->i_addr2),
				    ni->ni_erp, erp);
				if (ic->ic_curmode == IEEE80211_MODE_11G &&
				    (ni->ni_erp & IEEE80211_ERP_USE_PROTECTION))
					ic->ic_flags |= IEEE80211_F_USEPROT;
				else
					ic->ic_flags &= ~IEEE80211_F_USEPROT;
				ni->ni_erp = erp;
				/* XXX statistic */
			}
			if ((ni->ni_capinfo ^ capinfo) & IEEE80211_CAPINFO_SHORT_SLOTTIME) {
				IEEE80211_DPRINTF(ic, IEEE80211_MSG_ASSOC,
				    "[%s] capabilities change: before 0x%x,"
				     " now 0x%x\n",
				     ether_sprintf(wh->i_addr2),
				     ni->ni_capinfo, capinfo);
				/*
				 * NB: we assume short preamble doesn't
				 *     change dynamically
				 */
				ieee80211_set_shortslottime(ic,
					ic->ic_curmode == IEEE80211_MODE_11A ||
					(ni->ni_capinfo & IEEE80211_CAPINFO_SHORT_SLOTTIME));
				ni->ni_capinfo = capinfo;
				/* XXX statistic */
			}
			if (wme != NULL &&
			    ieee80211_parse_wmeparams(ic, wme, wh) > 0)
				ieee80211_wme_updateparams(ic);
			if (tim != NULL) {
				struct ieee80211_tim_ie *ie =
				    (struct ieee80211_tim_ie *) tim;

				ni->ni_dtim_count = ie->tim_count;
				ni->ni_dtim_period = ie->tim_period;
			}
			/* NB: don't need the rest of this */
			if ((ic->ic_flags & IEEE80211_F_SCAN) == 0)
				return;
		}

		if (ni == ic->ic_bss) {
#ifdef IEEE80211_DEBUG
			if (ieee80211_msg_scan(ic))
				dump_probe_beacon(subtype, 1,
				    wh->i_addr2, chan, bchan, capinfo,
				    bintval, erp, ssid, country);
#endif
			/*
			 * Create a new entry.  If scanning the entry goes
			 * in the scan cache.  Otherwise, be particular when
			 * operating in adhoc mode--only take nodes marked
			 * as ibss participants so we don't populate our
			 * neighbor table with unintersting sta's.
			 */
			if ((ic->ic_flags & IEEE80211_F_SCAN) == 0) {
				if ((capinfo & IEEE80211_CAPINFO_IBSS) == 0)
					return;
				ni = ieee80211_fakeup_adhoc_node(&ic->ic_sta,
						wh->i_addr2);
			} else
				ni = ieee80211_dup_bss(&ic->ic_scan, wh->i_addr2);
			if (ni == NULL)
				return;
			ni->ni_esslen = ssid[1];
			memset(ni->ni_essid, 0, sizeof(ni->ni_essid));
			memcpy(ni->ni_essid, ssid + 2, ssid[1]);
		} else if (ssid[1] != 0 &&
		    (ISPROBE(subtype) || ni->ni_esslen == 0)) {
			/*
			 * Update ESSID at probe response to adopt
			 * hidden AP by Lucent/Cisco, which announces
			 * null ESSID in beacon.
			 */
#ifdef IEEE80211_DEBUG
			if (ieee80211_msg_scan(ic) ||
			    ieee80211_msg_debug(ic))
				dump_probe_beacon(subtype, 0,
				    wh->i_addr2, chan, bchan, capinfo,
				    bintval, erp, ssid, country);
#endif
			ni->ni_esslen = ssid[1];
			memset(ni->ni_essid, 0, sizeof(ni->ni_essid));
			memcpy(ni->ni_essid, ssid + 2, ssid[1]);
		}
		ni->ni_scangen = ic->ic_scan.nt_scangen;
		IEEE80211_ADDR_COPY(ni->ni_bssid, wh->i_addr3);
		ni->ni_rssi = rssi;
		ni->ni_rstamp = rstamp;
		memcpy(ni->ni_tstamp.data, tstamp, sizeof(ni->ni_tstamp));
		ni->ni_intval = bintval;
		ni->ni_capinfo = capinfo;
		ni->ni_chan = &ic->ic_channels[chan];
		ni->ni_fhdwell = fhdwell;
		ni->ni_fhindex = fhindex;
		ni->ni_erp = erp;
		if (tim != NULL) {
			struct ieee80211_tim_ie *ie =
			    (struct ieee80211_tim_ie *) tim;

			ni->ni_dtim_count = ie->tim_count;
			ni->ni_dtim_period = ie->tim_period;
		}
		/*
		 * Record the byte offset from the mac header to
		 * the start of the TIM information element for
		 * use by hardware and/or to speedup software
		 * processing of beacon frames.
		 */
		ni->ni_timoff = timoff;
		/*
		 * Record optional information elements that might be
		 * used by applications or drivers.
		 */
		if (wme != NULL)
			ieee80211_saveie(&ni->ni_wme_ie, wme);
		if (wpa != NULL)
			ieee80211_saveie(&ni->ni_wpa_ie, wpa);
		/* NB: must be after ni_chan is setup */
		ieee80211_setup_rates(ic, ni, rates, xrates, IEEE80211_F_DOSORT);
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
		 */
		ssid = rates = xrates = NULL;
		while (frm < efrm) {
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
			}
			frm += frm[1] + 2;
		}
		IEEE80211_VERIFY_ELEMENT(rates, IEEE80211_RATE_MAXSIZE);
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

		if (ni == ic->ic_bss) {
			if (ic->ic_opmode == IEEE80211_M_IBSS) {
				/*
				 * XXX Cannot tell if the sender is operating
				 * in ibss mode.  But we need a new node to
				 * send the response so blindly add them to the
				 * neighbor table.
				 */
				ni = ieee80211_fakeup_adhoc_node(&ic->ic_sta,
					wh->i_addr2);
			} else
				ni = ieee80211_dup_bss(&ic->ic_sta, wh->i_addr2);
			if (ni == NULL)
				return;
			allocbs = 1;
		} else
			allocbs = 0;
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_ASSOC,
		    "[%s] recv probe req\n", ether_sprintf(wh->i_addr2));
		ni->ni_rssi = rssi;
		ni->ni_rstamp = rstamp;
		rate = ieee80211_setup_rates(ic, ni, rates, xrates,
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
		if (allocbs && ic->ic_opmode != IEEE80211_M_IBSS) {
			/* reclaim immediately */
			ieee80211_free_node(ni);
		}
		break;

	case IEEE80211_FC0_SUBTYPE_AUTH: {
		u_int16_t algo, seq, status;
		/*
		 * auth frame format
		 *	[2] algorithm
		 *	[2] sequence
		 *	[2] status
		 *	[tlv*] challenge
		 */
		IEEE80211_VERIFY_LENGTH(efrm - frm, 6);
		algo   = le16toh(*(u_int16_t *)frm);
		seq    = le16toh(*(u_int16_t *)(frm + 2));
		status = le16toh(*(u_int16_t *)(frm + 4));
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
			    rstamp, seq, status);
		else if (algo == IEEE80211_AUTH_ALG_OPEN)
			ieee80211_auth_open(ic, wh, ni, rssi, rstamp, seq,
			    status);
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
		u_int16_t capinfo, bintval;
		struct ieee80211_rsnparms rsn;
		u_int8_t reason;

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
		 */
		IEEE80211_VERIFY_LENGTH(efrm - frm, (reassoc ? 10 : 4));
		if (!IEEE80211_ADDR_EQ(wh->i_addr3, ic->ic_bss->ni_bssid)) {
			IEEE80211_DISCARD(ic, IEEE80211_MSG_ANY,
			    wh, ieee80211_mgt_subtype_name[subtype >>
				IEEE80211_FC0_SUBTYPE_SHIFT],
			    "%s", "wrong bssid");
			ic->ic_stats.is_rx_assoc_bss++;
			return;
		}
		capinfo = le16toh(*(u_int16_t *)frm);	frm += 2;
		bintval = le16toh(*(u_int16_t *)frm);	frm += 2;
		if (reassoc)
			frm += 6;	/* ignore current AP info */
		ssid = rates = xrates = wpa = wme = NULL;
		while (frm < efrm) {
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
				wpa = frm;
				break;
			case IEEE80211_ELEMID_VENDOR:
				if (iswpaoui(frm)) {
					if (ic->ic_flags & IEEE80211_F_WPA1)
						wpa = frm;
				} else if (iswmeinfo(frm))
					wme = frm;
				/* XXX Atheros OUI support */
				break;
			}
			frm += frm[1] + 2;
		}
		IEEE80211_VERIFY_ELEMENT(rates, IEEE80211_RATE_MAXSIZE);
		IEEE80211_VERIFY_ELEMENT(ssid, IEEE80211_NWID_LEN);
		IEEE80211_VERIFY_SSID(ic->ic_bss, ssid);

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
		/* assert right associstion security credentials */
		if (wpa == NULL && (ic->ic_flags & IEEE80211_F_WPA)) {
			IEEE80211_DPRINTF(ic,
			    IEEE80211_MSG_ASSOC | IEEE80211_MSG_WPA,
			    "[%s] no WPA/RSN IE in association request\n",
			    ether_sprintf(wh->i_addr2));
			IEEE80211_SEND_MGMT(ic, ni,
			    IEEE80211_FC0_SUBTYPE_DEAUTH,
			    IEEE80211_REASON_RSN_REQUIRED);
			ieee80211_node_leave(ic, ni);
			/* XXX distinguish WPA/RSN? */
			ic->ic_stats.is_rx_assoc_badwpaie++;
			return;	
		}
		if (wpa != NULL) {
			/*
			 * Parse WPA information element.  Note that
			 * we initialize the param block from the node
			 * state so that information in the IE overrides
			 * our defaults.  The resulting parameters are
			 * installed below after the association is assured.
			 */
			rsn = ni->ni_rsn;
			if (wpa[0] != IEEE80211_ELEMID_RSN)
				reason = ieee80211_parse_wpa(ic, wpa, &rsn, wh);
			else
				reason = ieee80211_parse_rsn(ic, wpa, &rsn, wh);
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
			    wpa[0] != IEEE80211_ELEMID_RSN ?  "WPA" : "RSN",
			    rsn.rsn_mcastcipher, rsn.rsn_mcastkeylen,
			    rsn.rsn_ucastcipher, rsn.rsn_ucastkeylen,
			    rsn.rsn_keymgmt, rsn.rsn_caps);
		}
		/* discard challenge after association */
		if (ni->ni_challenge != NULL) {
			FREE(ni->ni_challenge, M_DEVBUF);
			ni->ni_challenge = NULL;
		}
		/* NB: 802.11 spec says to ignore station's privacy bit */
		if ((capinfo & IEEE80211_CAPINFO_ESS) == 0) {
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_ANY,
			    "[%s] deny %s request, capability mismatch 0x%x\n",
			    ether_sprintf(wh->i_addr2),
			    reassoc ? "reassoc" : "assoc", capinfo);
			IEEE80211_SEND_MGMT(ic, ni, resp,
				IEEE80211_STATUS_CAPINFO);
			ieee80211_node_leave(ic, ni);
			ic->ic_stats.is_rx_assoc_capmismatch++;
			return;
		}
		rate = ieee80211_setup_rates(ic, ni, rates, xrates,
				IEEE80211_F_DOSORT | IEEE80211_F_DOFRATE |
				IEEE80211_F_DONEGO | IEEE80211_F_DODEL);
		if (rate & IEEE80211_RATE_BASIC) {
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_ANY,
			    "[%s] deny %s request, rate set mismatch\n",
			    ether_sprintf(wh->i_addr2),
			    reassoc ? "reassoc" : "assoc");
			IEEE80211_SEND_MGMT(ic, ni, resp,
				IEEE80211_STATUS_BASIC_RATE);
			ieee80211_node_leave(ic, ni);
			ic->ic_stats.is_rx_assoc_norate++;
			return;
		}
		ni->ni_rssi = rssi;
		ni->ni_rstamp = rstamp;
		ni->ni_intval = bintval;
		ni->ni_capinfo = capinfo;
		ni->ni_chan = ic->ic_bss->ni_chan;
		ni->ni_fhdwell = ic->ic_bss->ni_fhdwell;
		ni->ni_fhindex = ic->ic_bss->ni_fhindex;
		if (wpa != NULL) {
			/*
			 * Record WPA/RSN parameters for station, mark
			 * node as using WPA and record information element
			 * for applications that require it.
			 */
			ni->ni_rsn = rsn;
			ieee80211_saveie(&ni->ni_wpa_ie, wpa);
		} else if (ni->ni_wpa_ie != NULL) {
			/*
			 * Flush any state from a previous association.
			 */
			FREE(ni->ni_wpa_ie, M_DEVBUF);
			ni->ni_wpa_ie = NULL;
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
			FREE(ni->ni_wme_ie, M_DEVBUF);
			ni->ni_wme_ie = NULL;
			ni->ni_flags &= ~IEEE80211_NODE_QOS;
		}
		ieee80211_node_join(ic, ni, resp);
		break;
	}

	case IEEE80211_FC0_SUBTYPE_ASSOC_RESP:
	case IEEE80211_FC0_SUBTYPE_REASSOC_RESP: {
		u_int16_t capinfo, associd;
		u_int16_t status;

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
		 */
		IEEE80211_VERIFY_LENGTH(efrm - frm, 6);
		ni = ic->ic_bss;
		capinfo = le16toh(*(u_int16_t *)frm);
		frm += 2;
		status = le16toh(*(u_int16_t *)frm);
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
		associd = le16toh(*(u_int16_t *)frm);
		frm += 2;

		rates = xrates = wpa = wme = NULL;
		while (frm < efrm) {
			switch (*frm) {
			case IEEE80211_ELEMID_RATES:
				rates = frm;
				break;
			case IEEE80211_ELEMID_XRATES:
				xrates = frm;
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
		rate = ieee80211_setup_rates(ic, ni, rates, xrates,
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
		if (ic->ic_curmode == IEEE80211_MODE_11A ||
		    (ni->ni_capinfo & IEEE80211_CAPINFO_SHORT_PREAMBLE)) {
			ic->ic_flags |= IEEE80211_F_SHPREAMBLE;
			ic->ic_flags &= ~IEEE80211_F_USEBARKER;
		} else {
			ic->ic_flags &= ~IEEE80211_F_SHPREAMBLE;
			ic->ic_flags |= IEEE80211_F_USEBARKER;
		}
		ieee80211_set_shortslottime(ic,
			ic->ic_curmode == IEEE80211_MODE_11A ||
			(ni->ni_capinfo & IEEE80211_CAPINFO_SHORT_SLOTTIME));
		/*
		 * Honor ERP protection.
		 *
		 * NB: ni_erp should zero for non-11g operation.
		 * XXX check ic_curmode anyway?
		 */
		if (ic->ic_curmode == IEEE80211_MODE_11G &&
		    (ni->ni_erp & IEEE80211_ERP_USE_PROTECTION))
			ic->ic_flags |= IEEE80211_F_USEPROT;
		else
			ic->ic_flags &= ~IEEE80211_F_USEPROT;
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_ASSOC,
		    "[%s] %sassoc success: %s preamble, %s slot time%s%s\n",
		    ether_sprintf(wh->i_addr2),
		    ISREASSOC(subtype) ? "re" : "",
		    ic->ic_flags&IEEE80211_F_SHPREAMBLE ? "short" : "long",
		    ic->ic_flags&IEEE80211_F_SHSLOT ? "short" : "long",
		    ic->ic_flags&IEEE80211_F_USEPROT ? ", protection" : "",
		    ni->ni_flags & IEEE80211_NODE_QOS ? ", QoS" : ""
		);
		ieee80211_new_state(ic, IEEE80211_S_RUN, subtype);
		break;
	}

	case IEEE80211_FC0_SUBTYPE_DEAUTH: {
		u_int16_t reason;

		if (ic->ic_state == IEEE80211_S_SCAN) {
			ic->ic_stats.is_rx_mgtdiscard++;
			return;
		}
		/*
		 * deauth frame format
		 *	[2] reason
		 */
		IEEE80211_VERIFY_LENGTH(efrm - frm, 2);
		reason = le16toh(*(u_int16_t *)frm);
		ic->ic_stats.is_rx_deauth++;
		IEEE80211_NODE_STAT(ni, rx_deauth);
		switch (ic->ic_opmode) {
		case IEEE80211_M_STA:
			ieee80211_new_state(ic, IEEE80211_S_AUTH,
			    wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK);
			break;
		case IEEE80211_M_HOSTAP:
			if (ni != ic->ic_bss) {
				IEEE80211_DPRINTF(ic, IEEE80211_MSG_AUTH,
				    "station %s deauthenticated by peer "
				    "(reason %d)\n",
				    ether_sprintf(ni->ni_macaddr), reason);
				ieee80211_node_leave(ic, ni);
			}
			break;
		default:
			ic->ic_stats.is_rx_mgtdiscard++;
			break;
		}
		break;
	}

	case IEEE80211_FC0_SUBTYPE_DISASSOC: {
		u_int16_t reason;

		if (ic->ic_state != IEEE80211_S_RUN &&
		    ic->ic_state != IEEE80211_S_AUTH) {
			ic->ic_stats.is_rx_mgtdiscard++;
			return;
		}
		/*
		 * disassoc frame format
		 *	[2] reason
		 */
		IEEE80211_VERIFY_LENGTH(efrm - frm, 2);
		reason = le16toh(*(u_int16_t *)frm);
		ic->ic_stats.is_rx_disassoc++;
		IEEE80211_NODE_STAT(ni, rx_disassoc);
		switch (ic->ic_opmode) {
		case IEEE80211_M_STA:
			ieee80211_new_state(ic, IEEE80211_S_ASSOC,
			    wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK);
			break;
		case IEEE80211_M_HOSTAP:
			if (ni != ic->ic_bss) {
				IEEE80211_DPRINTF(ic, IEEE80211_MSG_ASSOC,
				    "[%s] sta disassociated by peer (reason %d)\n",
				    ether_sprintf(ni->ni_macaddr), reason);
				ieee80211_node_leave(ic, ni);
			}
			break;
		default:
			ic->ic_stats.is_rx_mgtdiscard++;
			break;
		}
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
 * Handle station power-save state change.
 */
static void
ieee80211_node_pwrsave(struct ieee80211_node *ni, int enable)
{
	struct ieee80211com *ic = ni->ni_ic;
	struct mbuf *m;

	if (enable) {
		if ((ni->ni_flags & IEEE80211_NODE_PWR_MGT) == 0)
			ic->ic_ps_sta++;
		ni->ni_flags |= IEEE80211_NODE_PWR_MGT;
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_POWER,
		    "[%s] power save mode on, %u sta's in ps mode\n",
		    ether_sprintf(ni->ni_macaddr), ic->ic_ps_sta);
		return;
	}

	if (ni->ni_flags & IEEE80211_NODE_PWR_MGT)
		ic->ic_ps_sta--;
	ni->ni_flags &= ~IEEE80211_NODE_PWR_MGT;
	IEEE80211_DPRINTF(ic, IEEE80211_MSG_POWER,
	    "[%s] power save mode off, %u sta's in ps mode\n",
	    ether_sprintf(ni->ni_macaddr), ic->ic_ps_sta);
	/* XXX if no stations in ps mode, flush mc frames */

	/*
	 * Flush queued unicast frames.
	 */
	if (IEEE80211_NODE_SAVEQ_QLEN(ni) == 0) {
		if (ic->ic_set_tim != NULL)
			ic->ic_set_tim(ic, ni, 0);	/* just in case */
		return;
	}
	IEEE80211_DPRINTF(ic, IEEE80211_MSG_POWER,
	    "[%s] flush ps queue, %u packets queued\n",
	    ether_sprintf(ni->ni_macaddr), IEEE80211_NODE_SAVEQ_QLEN(ni));
	for (;;) {
		int qlen;

		IEEE80211_NODE_SAVEQ_DEQUEUE(ni, m, qlen);
		if (m == NULL)
			break;
		/* 
		 * If this is the last packet, turn off the TIM bit.
		 * If there are more packets, set the more packets bit
		 * in the packet dispatched to the station.
		 */
		if (qlen != 0) {
			struct ieee80211_frame_min *wh =
				mtod(m, struct ieee80211_frame_min *);
			wh->i_fc[1] |= IEEE80211_FC1_MORE_DATA;
		}
		/* XXX need different driver interface */
		/* XXX bypasses q max */
		IF_ENQUEUE(&ic->ic_ifp->if_snd, m);
	}
	if (ic->ic_set_tim != NULL)
		ic->ic_set_tim(ic, ni, 0);
}

/*
 * Process a received ps-poll frame.
 */
static void
ieee80211_recv_pspoll(struct ieee80211com *ic,
	struct ieee80211_node *ni, struct mbuf *m0)
{
	struct ieee80211_frame_min *wh;
	struct mbuf *m;
	u_int16_t aid;
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

	aid = le16toh(*(u_int16_t *)wh->i_dur);
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
		ieee80211_send_nulldata(ic, ni);
		ic->ic_stats.is_ps_qempty++;	/* XXX node stat */
		if (ic->ic_set_tim != NULL)
			ic->ic_set_tim(ic, ni, 0);	/* just in case */
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
		wh = mtod(m, struct ieee80211_frame_min *);
		wh->i_fc[1] |= IEEE80211_FC1_MORE_DATA;
	} else {
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_POWER,
		    "[%s] recv ps-poll, send packet, queue empty\n",
		    ether_sprintf(ni->ni_macaddr));
		if (ic->ic_set_tim != NULL)
			ic->ic_set_tim(ic, ni, 0);
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
static const u_int8_t *
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

static void
ieee80211_discard_frame(struct ieee80211com *ic,
	const struct ieee80211_frame *wh,
	const char *type, const char *fmt, ...)
{
	va_list ap;

	printf("[%s] discard ", ether_sprintf(ieee80211_getbssid(ic, wh)));
	if (type != NULL)
		printf(" %s frame, ", type);
	else
		printf(" frame, ");
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	printf("\n");
}

static void
ieee80211_discard_ie(struct ieee80211com *ic,
	const struct ieee80211_frame *wh,
	const char *type, const char *fmt, ...)
{
	va_list ap;

	printf("[%s] discard ", ether_sprintf(ieee80211_getbssid(ic, wh)));
	if (type != NULL)
		printf(" %s information element, ", type);
	else
		printf(" information element, ");
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	printf("\n");
}

static void
ieee80211_discard_mac(struct ieee80211com *ic,
	const u_int8_t mac[IEEE80211_ADDR_LEN],
	const char *type, const char *fmt, ...)
{
	va_list ap;

	printf("[%s] discard ", ether_sprintf(mac));
	if (type != NULL)
		printf(" %s frame, ", type);
	else
		printf(" frame, ");
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	printf("\n");
}
#endif /* IEEE80211_DEBUG */
