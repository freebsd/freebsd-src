/*-
 * Copyright (c) 2001 Atsushi Onoe
 * Copyright (c) 2002, 2003 Sam Leffler, Errno Consulting
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

#include "opt_inet.h"

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

#include <machine/atomic.h>
 
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_llc.h>

#include <net80211/ieee80211_var.h>

#include <net/bpf.h>

#ifdef INET
#include <netinet/in.h> 
#include <netinet/if_ether.h>
#endif

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
void
ieee80211_input(struct ifnet *ifp, struct mbuf *m, struct ieee80211_node *ni,
	int rssi, u_int32_t rstamp)
{
	struct ieee80211com *ic = (void *)ifp;
	struct ieee80211_frame *wh;
	struct ether_header *eh;
	struct mbuf *m1;
	int len;
	u_int8_t dir, type, subtype;
	u_int8_t *bssid;
	u_int16_t rxseq;

	KASSERT(ni != NULL, ("null node"));

	/* trim CRC here so WEP can find its own CRC at the end of packet. */
	if (m->m_flags & M_HASFCS) {
		m_adj(m, -IEEE80211_CRC_LEN);
		m->m_flags &= ~M_HASFCS;
	}
	KASSERT(m->m_pkthdr.len >= sizeof(struct ieee80211_frame_min),
		("frame length too short: %u", m->m_pkthdr.len));

	/*
	 * In monitor mode, send everything directly to bpf.
	 * XXX may want to include the CRC
	 */
	if (ic->ic_opmode == IEEE80211_M_MONITOR)
		goto out;

	wh = mtod(m, struct ieee80211_frame *);
	if ((wh->i_fc[0] & IEEE80211_FC0_VERSION_MASK) !=
	    IEEE80211_FC0_VERSION_0) {
		if (ifp->if_flags & IFF_DEBUG)
			if_printf(ifp, "receive packet with wrong version: %x\n",
			    wh->i_fc[0]);
		ieee80211_unref_node(&ni);
		ic->ic_stats.is_rx_badversion++;
		goto err;
	}

	dir = wh->i_fc[1] & IEEE80211_FC1_DIR_MASK;
	type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
	/*
	 * NB: We are not yet prepared to handle control frames,
	 *     but permitting drivers to send them to us allows
	 *     them to go through bpf tapping at the 802.11 layer.
	 */
	if (m->m_pkthdr.len < sizeof(struct ieee80211_frame)) {
		/* XXX statistic */
		IEEE80211_DPRINTF2(("%s: frame too short, len %u\n",
			__func__, m->m_pkthdr.len));
		ic->ic_stats.is_rx_tooshort++;
		goto out;		/* XXX */
	}
	if (ic->ic_state != IEEE80211_S_SCAN) {
		switch (ic->ic_opmode) {
		case IEEE80211_M_STA:
			if (!IEEE80211_ADDR_EQ(wh->i_addr2, ni->ni_bssid)) {
				/* not interested in */
				IEEE80211_DPRINTF2(("%s: discard frame from "
					"bss %s\n", __func__,
					ether_sprintf(wh->i_addr2)));
				ic->ic_stats.is_rx_wrongbss++;
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
			if (!IEEE80211_ADDR_EQ(bssid, ic->ic_bss->ni_bssid) &&
			    !IEEE80211_ADDR_EQ(bssid, ifp->if_broadcastaddr)) {
				/* not interested in */
				IEEE80211_DPRINTF2(("%s: discard frame from "
					"bss %s\n", __func__,
					ether_sprintf(bssid)));
				ic->ic_stats.is_rx_wrongbss++;
				goto out;
			}
			break;
		case IEEE80211_M_MONITOR:
			goto out;
		default:
			/* XXX catch bad values */
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
			ic->ic_stats.is_rx_dup++; /* XXX per-station stat */
			goto out;
		}
		ni->ni_inact = 0;
	}

	switch (type) {
	case IEEE80211_FC0_TYPE_DATA:
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
			break;
		case IEEE80211_M_HOSTAP:
			if (dir != IEEE80211_FC1_DIR_TODS) {
				ic->ic_stats.is_rx_wrongdir++;
				goto out;
			}
			/* check if source STA is associated */
			if (ni == ic->ic_bss) {
				IEEE80211_DPRINTF(("%s: data from unknown src "
					"%s\n", __func__,
					ether_sprintf(wh->i_addr2)));
				/* NB: caller deals with reference */
				ni = ieee80211_dup_bss(ic, wh->i_addr2);
				if (ni != NULL) {
					IEEE80211_SEND_MGMT(ic, ni,
					    IEEE80211_FC0_SUBTYPE_DEAUTH,
					    IEEE80211_REASON_NOT_AUTHED);
					ieee80211_free_node(ic, ni);
				}
				ic->ic_stats.is_rx_notassoc++;
				goto err;
			}
			if (ni->ni_associd == 0) {
				IEEE80211_DPRINTF(("ieee80211_input: "
				    "data from unassoc src %s\n",
				    ether_sprintf(wh->i_addr2)));
				IEEE80211_SEND_MGMT(ic, ni,
				    IEEE80211_FC0_SUBTYPE_DISASSOC,
				    IEEE80211_REASON_NOT_ASSOCED);
				ieee80211_unref_node(&ni);
				ic->ic_stats.is_rx_notassoc++;
				goto err;
			}
			break;
		case IEEE80211_M_MONITOR:
			break;
		}
		if (wh->i_fc[1] & IEEE80211_FC1_WEP) {
			if (ic->ic_flags & IEEE80211_F_WEPON) {
				m = ieee80211_wep_crypt(ifp, m, 0);
				if (m == NULL) {
					ic->ic_stats.is_rx_wepfail++;
					goto err;
				}
				wh = mtod(m, struct ieee80211_frame *);
			} else {
				ic->ic_stats.is_rx_nowep++;
				goto out;
			}
		}
		/* copy to listener after decrypt */
		if (ic->ic_rawbpf)
			bpf_mtap(ic->ic_rawbpf, m);
		m = ieee80211_decap(ifp, m);
		if (m == NULL) {
			ic->ic_stats.is_rx_decap++;
			goto err;
		}
		ifp->if_ipackets++;

		/* perform as a bridge within the AP */
		m1 = NULL;
		if (ic->ic_opmode == IEEE80211_M_HOSTAP) {
			eh = mtod(m, struct ether_header *);
			if (ETHER_IS_MULTICAST(eh->ether_dhost)) {
				m1 = m_copypacket(m, M_DONTWAIT);
				if (m1 == NULL)
					ifp->if_oerrors++;
				else
					m1->m_flags |= M_MCAST;
			} else {
				ni = ieee80211_find_node(ic, eh->ether_dhost);
				if (ni != NULL) {
					if (ni->ni_associd != 0) {
						m1 = m;
						m = NULL;
					}
					ieee80211_unref_node(&ni);
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
		if (dir != IEEE80211_FC1_DIR_NODS) {
			ic->ic_stats.is_rx_wrongdir++;
			goto err;
		}
		if (ic->ic_opmode == IEEE80211_M_AHDEMO) {
			ic->ic_stats.is_rx_ahdemo_mgt++;
			goto out;
		}
		subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;

		/* drop frames without interest */
		if (ic->ic_state == IEEE80211_S_SCAN) {
			if (subtype != IEEE80211_FC0_SUBTYPE_BEACON &&
			    subtype != IEEE80211_FC0_SUBTYPE_PROBE_RESP) {
				ic->ic_stats.is_rx_mgtdiscard++;
				goto out;
			}
		} else {
			if (ic->ic_opmode != IEEE80211_M_IBSS &&
			    subtype == IEEE80211_FC0_SUBTYPE_BEACON) {
				ic->ic_stats.is_rx_mgtdiscard++;
				goto out;
			}
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
		if (ic->ic_rawbpf)
			bpf_mtap(ic->ic_rawbpf, m);
		(*ic->ic_recv_mgmt)(ic, m, ni, subtype, rssi, rstamp);
		m_freem(m);
		return;

	case IEEE80211_FC0_TYPE_CTL:
		ic->ic_stats.is_rx_ctl++;
		goto out;
	default:
		IEEE80211_DPRINTF(("%s: bad type %x\n", __func__, type));
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
		IEEE80211_DPRINTF(("%s: DS to DS\n", __func__));
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
			IEEE80211_DPRINTF(("%s: extended rate set too large;"
				" only using %u of %u rates\n",
				__func__, nxrates, xrates[1]));
			ic->ic_stats.is_rx_rstoobig++;
		}
		memcpy(rs->rs_rates + rs->rs_nrates, xrates+2, nxrates);
		rs->rs_nrates += nxrates;
	}
	return ieee80211_fix_rate(ic, ni, flags);
}

/* Verify the existence and length of __elem or get out. */
#define IEEE80211_VERIFY_ELEMENT(__elem, __maxlen) do {			\
	if ((__elem) == NULL) {						\
		IEEE80211_DPRINTF(("%s: no " #__elem "in %s frame\n",	\
			__func__, ieee80211_mgt_subtype_name[subtype >>	\
				IEEE80211_FC0_SUBTYPE_SHIFT]));		\
		ic->ic_stats.is_rx_elem_missing++;			\
		return;							\
	}								\
	if ((__elem)[1] > (__maxlen)) {					\
		IEEE80211_DPRINTF(("%s: bad " #__elem " len %d in %s "	\
			"frame from %s\n", __func__, (__elem)[1],	\
			ieee80211_mgt_subtype_name[subtype >>		\
				IEEE80211_FC0_SUBTYPE_SHIFT],		\
			ether_sprintf(wh->i_addr2)));			\
		ic->ic_stats.is_rx_elem_toobig++;			\
		return;							\
	}								\
} while (0)

#define	IEEE80211_VERIFY_LENGTH(_len, _minlen) do {			\
	if ((_len) < (_minlen)) {					\
		IEEE80211_DPRINTF(("%s: %s frame too short from %s\n",	\
			__func__,					\
			ieee80211_mgt_subtype_name[subtype >>		\
				IEEE80211_FC0_SUBTYPE_SHIFT],		\
			ether_sprintf(wh->i_addr2)));			\
		ic->ic_stats.is_rx_elem_toosmall++;			\
		return;							\
	}								\
} while (0)

void
ieee80211_recv_mgmt(struct ieee80211com *ic, struct mbuf *m0,
	struct ieee80211_node *ni,
	int subtype, int rssi, u_int32_t rstamp)
{
#define	ISPROBE(_st)	((_st) == IEEE80211_FC0_SUBTYPE_PROBE_RESP)
	struct ifnet *ifp = &ic->ic_if;
	struct ieee80211_frame *wh;
	u_int8_t *frm, *efrm;
	u_int8_t *ssid, *rates, *xrates;
	int reassoc, resp, newassoc, allocbs;

	wh = mtod(m0, struct ieee80211_frame *);
	frm = (u_int8_t *)&wh[1];
	efrm = mtod(m0, u_int8_t *) + m0->m_len;
	switch (subtype) {
	case IEEE80211_FC0_SUBTYPE_PROBE_RESP:
	case IEEE80211_FC0_SUBTYPE_BEACON: {
		u_int8_t *tstamp, *bintval, *capinfo, *country;
		u_int8_t chan, bchan, fhindex, erp;
		u_int16_t fhdwell;

		if (ic->ic_opmode != IEEE80211_M_IBSS &&
		    ic->ic_state != IEEE80211_S_SCAN) {
			/* XXX: may be useful for background scan */
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
		 */
		IEEE80211_VERIFY_LENGTH(efrm - frm, 12);
		tstamp  = frm;	frm += 8;
		bintval = frm;	frm += 2;
		capinfo = frm;	frm += 2;
		ssid = rates = xrates = country = NULL;
		bchan = ieee80211_chan2ieee(ic, ic->ic_bss->ni_chan);
		chan = bchan;
		fhdwell = 0;
		fhindex = 0;
		erp = 0;
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
					fhdwell = (frm[3] << 8) | frm[2];
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
				break;
			case IEEE80211_ELEMID_XRATES:
				xrates = frm;
				break;
			case IEEE80211_ELEMID_ERP:
				if (frm[1] != 1) {
					IEEE80211_DPRINTF(("%s: invalid ERP "
						"element; length %u, expecting "
						"1\n", __func__, frm[1]));
					ic->ic_stats.is_rx_elem_toobig++;
					break;
				}
				erp = frm[2];
				break;
			default:
				IEEE80211_DPRINTF2(("%s: element id %u/len %u "
					"ignored\n", __func__, *frm, frm[1]));
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
			IEEE80211_DPRINTF(("%s: ignore %s with invalid channel "
				"%u\n", __func__,
				ISPROBE(subtype) ? "probe response" : "beacon",
				chan));
			ic->ic_stats.is_rx_badchan++;
			return;
		}
		if (chan != bchan) {
			/*
			 * Frame was received on a channel different from the
			 * one indicated in the DS/FH params element id;
			 * silently discard it.
			 *
			 * NB: this can happen due to signal leakage.
			 */
			IEEE80211_DPRINTF(("%s: ignore %s on channel %u marked "
				"for channel %u\n", __func__,
				ISPROBE(subtype) ? "probe response" : "beacon",
				bchan, chan));
			ic->ic_stats.is_rx_chanmismatch++;
			return;
		}

		/*
		 * Use mac and channel for lookup so we collect all
		 * potential AP's when scanning.  Otherwise we may
		 * see the same AP on multiple channels and will only
		 * record the last one.  We could filter APs here based
		 * on rssi, etc. but leave that to the end of the scan
		 * so we can keep the selection criteria in one spot.
		 * This may result in a bloat of the scanned AP list but
		 * it shouldn't be too much.
		 */
		ni = ieee80211_lookup_node(ic, wh->i_addr2,
				&ic->ic_channels[chan]);
#ifdef IEEE80211_DEBUG
		if (ieee80211_debug &&
		    (ni == NULL || ic->ic_state == IEEE80211_S_SCAN)) {
			printf("%s: %s%s on chan %u (bss chan %u) ",
			    __func__, (ni == NULL ? "new " : ""),
			    ISPROBE(subtype) ? "probe response" : "beacon",
			    chan, bchan);
			ieee80211_print_essid(ssid + 2, ssid[1]);
			printf(" from %s\n", ether_sprintf(wh->i_addr2));
			printf("%s: caps 0x%x bintval %u erp 0x%x\n",
				__func__, le16toh(*(u_int16_t *)capinfo),
				le16toh(*(u_int16_t *)bintval), erp);
			if (country)
				printf("%s: country info %*D\n",
					__func__, country[1], country+2, " ");
		}
#endif
		if (ni == NULL) {
			ni = ieee80211_alloc_node(ic, wh->i_addr2);
			if (ni == NULL) {
				ic->ic_stats.is_rx_nodealloc++;
				return;
			}
			ni->ni_esslen = ssid[1];
			memset(ni->ni_essid, 0, sizeof(ni->ni_essid));
			memcpy(ni->ni_essid, ssid + 2, ssid[1]);
		} else if (ssid[1] != 0 && ISPROBE(subtype)) {
			/*
			 * Update ESSID at probe response to adopt hidden AP by
			 * Lucent/Cisco, which announces null ESSID in beacon.
			 */
			ni->ni_esslen = ssid[1];
			memset(ni->ni_essid, 0, sizeof(ni->ni_essid));
			memcpy(ni->ni_essid, ssid + 2, ssid[1]);
		}
		IEEE80211_ADDR_COPY(ni->ni_bssid, wh->i_addr3);
		ni->ni_rssi = rssi;
		ni->ni_rstamp = rstamp;
		memcpy(ni->ni_tstamp, tstamp, sizeof(ni->ni_tstamp));
		ni->ni_intval = le16toh(*(u_int16_t *)bintval);
		ni->ni_capinfo = le16toh(*(u_int16_t *)capinfo);
		/* XXX validate channel # */
		ni->ni_chan = &ic->ic_channels[chan];
		ni->ni_fhdwell = fhdwell;
		ni->ni_fhindex = fhindex;
		ni->ni_erp = erp;
		/* NB: must be after ni_chan is setup */
		ieee80211_setup_rates(ic, ni, rates, xrates, IEEE80211_F_DOSORT);
		ieee80211_unref_node(&ni);
		break;
	}

	case IEEE80211_FC0_SUBTYPE_PROBE_REQ: {
		u_int8_t rate;

		if (ic->ic_opmode == IEEE80211_M_STA)
			return;
		if (ic->ic_state != IEEE80211_S_RUN)
			return;

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
		if (ssid[1] != 0 &&
		    (ssid[1] != ic->ic_bss->ni_esslen ||
		    memcmp(ssid + 2, ic->ic_bss->ni_essid, ic->ic_bss->ni_esslen) != 0)) {
#ifdef IEEE80211_DEBUG
			if (ieee80211_debug) {
				printf("%s: ssid unmatch ", __func__);
				ieee80211_print_essid(ssid + 2, ssid[1]);
				printf(" from %s\n", ether_sprintf(wh->i_addr2));
			}
#endif
			ic->ic_stats.is_rx_ssidmismatch++;
			return;
		}

		if (ni == ic->ic_bss) {
			ni = ieee80211_dup_bss(ic, wh->i_addr2);
			if (ni == NULL) {
				ic->ic_stats.is_rx_nodealloc++;
				return;
			}
			IEEE80211_DPRINTF(("%s: new req from %s\n",
				__func__, ether_sprintf(wh->i_addr2)));
			allocbs = 1;
		} else
			allocbs = 0;
		ni->ni_rssi = rssi;
		ni->ni_rstamp = rstamp;
		rate = ieee80211_setup_rates(ic, ni, rates, xrates,
				IEEE80211_F_DOSORT | IEEE80211_F_DOFRATE
				| IEEE80211_F_DONEGO | IEEE80211_F_DODEL);
		if (rate & IEEE80211_RATE_BASIC) {
			IEEE80211_DPRINTF(("%s: rate negotiation failed: %s\n",
				__func__,ether_sprintf(wh->i_addr2)));
		} else {
			IEEE80211_SEND_MGMT(ic, ni,
				IEEE80211_FC0_SUBTYPE_PROBE_RESP, 0);
		}
		if (allocbs) {
			/* XXX just use free? */
			if (ic->ic_opmode == IEEE80211_M_HOSTAP)
				ieee80211_free_node(ic, ni);
			else
				ieee80211_unref_node(&ni);
		}
		break;
	}

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
		if (algo != IEEE80211_AUTH_ALG_OPEN) {
			/* TODO: shared key auth */
			IEEE80211_DPRINTF(("%s: unsupported auth %d from %s\n",
				__func__, algo, ether_sprintf(wh->i_addr2)));
			ic->ic_stats.is_rx_auth_unsupported++;
			return;
		}
		switch (ic->ic_opmode) {
		case IEEE80211_M_IBSS:
			if (ic->ic_state != IEEE80211_S_RUN || seq != 1) {
				IEEE80211_DPRINTF(("%s: discard auth from %s; "
					"state %u, seq %u\n", __func__,
					ether_sprintf(wh->i_addr2),
					ic->ic_state, seq));
				ic->ic_stats.is_rx_bad_auth++;
				break;
			}
			ieee80211_new_state(ic, IEEE80211_S_AUTH,
			    wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK);
			break;

		case IEEE80211_M_AHDEMO:
			/* should not come here */
			break;

		case IEEE80211_M_HOSTAP:
			if (ic->ic_state != IEEE80211_S_RUN || seq != 1) {
				IEEE80211_DPRINTF(("%s: discard auth from %s; "
					"state %u, seq %u\n", __func__,
					ether_sprintf(wh->i_addr2),
					ic->ic_state, seq));
				ic->ic_stats.is_rx_bad_auth++;
				break;
			}
			if (ni == ic->ic_bss) {
				ni = ieee80211_alloc_node(ic, wh->i_addr2);
				if (ni == NULL) {
					ic->ic_stats.is_rx_nodealloc++;
					return;
				}
				IEEE80211_ADDR_COPY(ni->ni_bssid, ic->ic_bss->ni_bssid);
				ni->ni_rssi = rssi;
				ni->ni_rstamp = rstamp;
				ni->ni_chan = ic->ic_bss->ni_chan;
				allocbs = 1;
			} else
				allocbs = 0;
			IEEE80211_SEND_MGMT(ic, ni,
				IEEE80211_FC0_SUBTYPE_AUTH, 2);
			if (ifp->if_flags & IFF_DEBUG)
				if_printf(ifp, "station %s %s authenticated\n",
				    (allocbs ? "newly" : "already"),
				    ether_sprintf(ni->ni_macaddr));
			break;

		case IEEE80211_M_STA:
			if (ic->ic_state != IEEE80211_S_AUTH || seq != 2) {
				IEEE80211_DPRINTF(("%s: discard auth from %s; "
					"state %u, seq %u\n", __func__,
					ether_sprintf(wh->i_addr2),
					ic->ic_state, seq));
				ic->ic_stats.is_rx_bad_auth++;
				break;
			}
			if (status != 0) {
				if_printf(&ic->ic_if,
				    "authentication failed (reason %d) for %s\n",
				    status,
				    ether_sprintf(wh->i_addr3));
				if (ni != ic->ic_bss)
					ni->ni_fails++;
				ic->ic_stats.is_rx_auth_fail++;
				return;
			}
			ieee80211_new_state(ic, IEEE80211_S_ASSOC,
			    wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK);
			break;
		case IEEE80211_M_MONITOR:
			break;
		}
		break;
	}

	case IEEE80211_FC0_SUBTYPE_ASSOC_REQ:
	case IEEE80211_FC0_SUBTYPE_REASSOC_REQ: {
		u_int16_t capinfo, bintval;

		if (ic->ic_opmode != IEEE80211_M_HOSTAP ||
		    (ic->ic_state != IEEE80211_S_RUN))
			return;

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
		 */
		IEEE80211_VERIFY_LENGTH(efrm - frm, (reassoc ? 10 : 4));
		if (!IEEE80211_ADDR_EQ(wh->i_addr3, ic->ic_bss->ni_bssid)) {
			IEEE80211_DPRINTF(("%s: ignore other bss from %s\n",
				__func__, ether_sprintf(wh->i_addr2)));
			ic->ic_stats.is_rx_assoc_bss++;
			return;
		}
		capinfo = le16toh(*(u_int16_t *)frm);	frm += 2;
		bintval = le16toh(*(u_int16_t *)frm);	frm += 2;
		if (reassoc)
			frm += 6;	/* ignore current AP info */
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
		if (ssid[1] != ic->ic_bss->ni_esslen ||
		    memcmp(ssid + 2, ic->ic_bss->ni_essid, ssid[1]) != 0) {
#ifdef IEEE80211_DEBUG
			if (ieee80211_debug) {
				printf("%s: ssid unmatch ", __func__);
				ieee80211_print_essid(ssid + 2, ssid[1]);
				printf(" from %s\n", ether_sprintf(wh->i_addr2));
			}
#endif
			ic->ic_stats.is_rx_ssidmismatch++;
			return;
		}
		if (ni == ic->ic_bss) {
			IEEE80211_DPRINTF(("%s: not authenticated for %s\n",
				__func__, ether_sprintf(wh->i_addr2)));
			ni = ieee80211_dup_bss(ic, wh->i_addr2);
			if (ni != NULL) {
				IEEE80211_SEND_MGMT(ic, ni,
				    IEEE80211_FC0_SUBTYPE_DEAUTH,
				    IEEE80211_REASON_ASSOC_NOT_AUTHED);
				ieee80211_free_node(ic, ni);
			}
			ic->ic_stats.is_rx_assoc_notauth++;
			return;
		}
		/* XXX per-node cipher suite */
		/* XXX some stations use the privacy bit for handling APs
		       that suport both encrypted and unencrypted traffic */
		if ((capinfo & IEEE80211_CAPINFO_ESS) == 0 ||
		    (capinfo & IEEE80211_CAPINFO_PRIVACY) !=
		    ((ic->ic_flags & IEEE80211_F_WEPON) ?
		     IEEE80211_CAPINFO_PRIVACY : 0)) {
			IEEE80211_DPRINTF(("%s: capability mismatch %x for %s\n",
				__func__, capinfo, ether_sprintf(wh->i_addr2)));
			ni->ni_associd = 0;
			IEEE80211_SEND_MGMT(ic, ni, resp,
				IEEE80211_STATUS_CAPINFO);
			ic->ic_stats.is_rx_assoc_capmismatch++;
			return;
		}
		ieee80211_setup_rates(ic, ni, rates, xrates,
				IEEE80211_F_DOSORT | IEEE80211_F_DOFRATE |
				IEEE80211_F_DONEGO | IEEE80211_F_DODEL);
		if (ni->ni_rates.rs_nrates == 0) {
			IEEE80211_DPRINTF(("%s: rate unmatch for %s\n",
				__func__, ether_sprintf(wh->i_addr2)));
			ni->ni_associd = 0;
			IEEE80211_SEND_MGMT(ic, ni, resp,
				IEEE80211_STATUS_BASIC_RATE);
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
		if (ni->ni_associd == 0) {
			/* XXX handle rollover at 2007 */
			/* XXX guarantee uniqueness */
			ni->ni_associd = 0xc000 | ic->ic_bss->ni_associd++;
			newassoc = 1;
		} else
			newassoc = 0;
		/* XXX for 11g must turn off short slot time if long
	           slot time sta associates */
		IEEE80211_SEND_MGMT(ic, ni, resp, IEEE80211_STATUS_SUCCESS);
		if (ifp->if_flags & IFF_DEBUG)
			if_printf(ifp, "station %s %s associated\n",
			    (newassoc ? "newly" : "already"),
			    ether_sprintf(ni->ni_macaddr));
		/* give driver a chance to setup state like ni_txrate */
		if (ic->ic_newassoc)
			(*ic->ic_newassoc)(ic, ni, newassoc);
		break;
	}

	case IEEE80211_FC0_SUBTYPE_ASSOC_RESP:
	case IEEE80211_FC0_SUBTYPE_REASSOC_RESP: {
		u_int16_t status;

		if (ic->ic_opmode != IEEE80211_M_STA ||
		    ic->ic_state != IEEE80211_S_ASSOC)
			return;

		/*
		 * asresp frame format
		 *	[2] capability information
		 *	[2] status
		 *	[2] association ID
		 *	[tlv] supported rates
		 *	[tlv] extended supported rates
		 */
		IEEE80211_VERIFY_LENGTH(efrm - frm, 6);
		ni = ic->ic_bss;
		ni->ni_capinfo = le16toh(*(u_int16_t *)frm);
		frm += 2;

		status = le16toh(*(u_int16_t *)frm);
		frm += 2;
		if (status != 0) {
			if_printf(ifp, "association failed (reason %d) for %s\n",
			    status, ether_sprintf(wh->i_addr3));
			if (ni != ic->ic_bss)
				ni->ni_fails++;
			ic->ic_stats.is_rx_auth_fail++;
			return;
		}
		ni->ni_associd = le16toh(*(u_int16_t *)frm);
		frm += 2;

		rates = xrates = NULL;
		while (frm < efrm) {
			switch (*frm) {
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
		ieee80211_setup_rates(ic, ni, rates, xrates,
				IEEE80211_F_DOSORT | IEEE80211_F_DOFRATE |
				IEEE80211_F_DONEGO | IEEE80211_F_DODEL);
		if (ni->ni_rates.rs_nrates != 0)
			ieee80211_new_state(ic, IEEE80211_S_RUN,
				wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK);
		break;
	}

	case IEEE80211_FC0_SUBTYPE_DEAUTH: {
		u_int16_t reason;
		/*
		 * deauth frame format
		 *	[2] reason
		 */
		IEEE80211_VERIFY_LENGTH(efrm - frm, 2);
		reason = le16toh(*(u_int16_t *)frm);
		ic->ic_stats.is_rx_deauth++;
		switch (ic->ic_opmode) {
		case IEEE80211_M_STA:
			ieee80211_new_state(ic, IEEE80211_S_AUTH,
			    wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK);
			break;
		case IEEE80211_M_HOSTAP:
			if (ni != ic->ic_bss) {
				if (ifp->if_flags & IFF_DEBUG)
					if_printf(ifp, "station %s deauthenticated"
					    " by peer (reason %d)\n",
					    ether_sprintf(ni->ni_macaddr), reason);
				/* node will be free'd on return */
				ieee80211_unref_node(&ni);
			}
			break;
		default:
			break;
		}
		break;
	}

	case IEEE80211_FC0_SUBTYPE_DISASSOC: {
		u_int16_t reason;
		/*
		 * disassoc frame format
		 *	[2] reason
		 */
		IEEE80211_VERIFY_LENGTH(efrm - frm, 2);
		reason = le16toh(*(u_int16_t *)frm);
		ic->ic_stats.is_rx_disassoc++;
		switch (ic->ic_opmode) {
		case IEEE80211_M_STA:
			ieee80211_new_state(ic, IEEE80211_S_ASSOC,
			    wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK);
			break;
		case IEEE80211_M_HOSTAP:
			if (ni != ic->ic_bss) {
				if (ifp->if_flags & IFF_DEBUG)
					if_printf(ifp, "station %s disassociated"
					    " by peer (reason %d)\n",
					    ether_sprintf(ni->ni_macaddr), reason);
				ni->ni_associd = 0;
				/* XXX node reclaimed how? */
			}
			break;
		default:
			break;
		}
		break;
	}
	default:
		IEEE80211_DPRINTF(("%s: mgmt frame with subtype 0x%x not "
			"handled\n", __func__, subtype));
		ic->ic_stats.is_rx_badsubtype++;
		break;
	}
#undef ISPROBE
}
#undef IEEE80211_VERIFY_LENGTH
#undef IEEE80211_VERIFY_ELEMENT
