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

int
ieee80211_mgmt_output(struct ifnet *ifp, struct ieee80211_node *ni,
    struct mbuf *m, int type)
{
	struct ieee80211com *ic = (void *)ifp;
	struct ieee80211_frame *wh;

	/* XXX this probably shouldn't be permitted */
	KASSERT(ni != NULL, ("%s: null node", __func__));
	ni->ni_inact = 0;

	M_PREPEND(m, sizeof(struct ieee80211_frame), M_DONTWAIT);
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
			if_printf(ifp, "sending %s to %s on channel %u\n",
			    ieee80211_mgt_subtype_name[
			    (type & IEEE80211_FC0_SUBTYPE_MASK)
			    >> IEEE80211_FC0_SUBTYPE_SHIFT],
			    ether_sprintf(ni->ni_macaddr),
			    ieee80211_chan2ieee(ic, ni->ni_chan));
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

	ni = ieee80211_find_node(ic, eh.ether_dhost);
	if (ni == NULL)			/*ic_opmode?? XXX*/
		ni = ieee80211_ref_node(ic->ic_bss);
	ni->ni_inact = 0;

	m_adj(m, sizeof(struct ether_header) - sizeof(struct llc));
	llc = mtod(m, struct llc *);
	llc->llc_dsap = llc->llc_ssap = LLC_SNAP_LSAP;
	llc->llc_control = LLC_UI;
	llc->llc_snap.org_code[0] = 0;
	llc->llc_snap.org_code[1] = 0;
	llc->llc_snap.org_code[2] = 0;
	llc->llc_snap.ether_type = eh.ether_type;
	M_PREPEND(m, sizeof(struct ieee80211_frame), M_DONTWAIT);
	if (m == NULL) {
		ieee80211_unref_node(&ni);
		return NULL;
	}
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
	case IEEE80211_M_MONITOR:
		m_freem(m), m = NULL;
		break;
	}
	ieee80211_unref_node(&ni);
	return m;
}

/*
 * Add a supported rates element id to a frame.
 */
u_int8_t *
ieee80211_add_rates(u_int8_t *frm, const struct ieee80211_rateset *rs)
{
	int nrates;

	*frm++ = IEEE80211_ELEMID_RATES;
	nrates = rs->rs_nrates;
	if (nrates > IEEE80211_RATE_SIZE)
		nrates = IEEE80211_RATE_SIZE;
	*frm++ = nrates;
	memcpy(frm, rs->rs_rates, nrates);
	return frm + nrates;
}

/*
 * Add an extended supported rates element id to a frame.
 */
u_int8_t *
ieee80211_add_xrates(u_int8_t *frm, const struct ieee80211_rateset *rs)
{
	/*
	 * Add an extended supported rates element if operating in 11g mode.
	 */
	if (rs->rs_nrates > IEEE80211_RATE_SIZE) {
		int nrates = rs->rs_nrates - IEEE80211_RATE_SIZE;
		*frm++ = IEEE80211_ELEMID_XRATES;
		*frm++ = nrates;
		memcpy(frm, rs->rs_rates + IEEE80211_RATE_SIZE, nrates);
		frm += nrates;
	}
	return frm;
}

/* 
 * Add an ssid elemet to a frame.
 */
static u_int8_t *
ieee80211_add_ssid(u_int8_t *frm, const u_int8_t *ssid, u_int len)
{
	*frm++ = IEEE80211_ELEMID_SSID;
	*frm++ = len;
	memcpy(frm, ssid, len);
	return frm + len;
}

static struct mbuf *
ieee80211_getmbuf(int flags, int type, u_int pktlen)
{
	struct mbuf *m;

	if (pktlen > MHLEN)
		MGETHDR(m, flags, type);
	else
		m = m_getcl(flags, type, M_PKTHDR);
	return m;
}

int
ieee80211_send_mgmt(struct ieee80211com *ic, struct ieee80211_node *ni,
	int type, int arg)
{
	struct ifnet *ifp = &ic->ic_if;
	struct mbuf *m;
	u_int8_t *frm;
	enum ieee80211_phymode mode;
	u_int16_t capinfo;
	int ret, timer;

	timer = 0;
	switch (type) {
	case IEEE80211_FC0_SUBTYPE_PROBE_REQ:
		/*
		 * prreq frame format
		 *	[tlv] ssid
		 *	[tlv] supported rates
		 *	[tlv] extended supported rates
		 */
		m = ieee80211_getmbuf(M_DONTWAIT, MT_DATA,
			 2 + ic->ic_des_esslen
		       + 2 + IEEE80211_RATE_SIZE
		       + 2 + (IEEE80211_RATE_MAXSIZE - IEEE80211_RATE_SIZE));
		if (m == NULL)
			return ENOMEM;
		m->m_data += sizeof(struct ieee80211_frame);
		frm = mtod(m, u_int8_t *);
		frm = ieee80211_add_ssid(frm, ic->ic_des_essid, ic->ic_des_esslen);
		mode = ieee80211_chan2mode(ic, ni->ni_chan);
		frm = ieee80211_add_rates(frm, &ic->ic_sup_rates[mode]);
		frm = ieee80211_add_xrates(frm, &ic->ic_sup_rates[mode]);
		m->m_pkthdr.len = m->m_len = frm - mtod(m, u_int8_t *);

		timer = IEEE80211_TRANS_WAIT;
		break;

	case IEEE80211_FC0_SUBTYPE_PROBE_RESP:
		/*
		 * probe response frame format
		 *	[8] time stamp
		 *	[2] beacon interval
		 *	[2] cabability information
		 *	[tlv] ssid
		 *	[tlv] supported rates
		 *	[tlv] parameter set (IBSS)
		 *	[tlv] extended supported rates
		 */
		m = ieee80211_getmbuf(M_DONTWAIT, MT_DATA,
			 8 + 2 + 2 + 2
		       + 2 + ni->ni_esslen
		       + 2 + IEEE80211_RATE_SIZE
		       + 6
		       + 2 + (IEEE80211_RATE_MAXSIZE - IEEE80211_RATE_SIZE));
		if (m == NULL)
			return ENOMEM;
		m->m_data += sizeof(struct ieee80211_frame);
		frm = mtod(m, u_int8_t *);

		memset(frm, 0, 8);	/* timestamp should be filled later */
		frm += 8;
		*(u_int16_t *)frm = htole16(ic->ic_bss->ni_intval);
		frm += 2;
		if (ic->ic_opmode == IEEE80211_M_IBSS)
			capinfo = IEEE80211_CAPINFO_IBSS;
		else
			capinfo = IEEE80211_CAPINFO_ESS;
		if (ic->ic_flags & IEEE80211_F_WEPON)
			capinfo |= IEEE80211_CAPINFO_PRIVACY;
		*(u_int16_t *)frm = htole16(capinfo);
		frm += 2;

		frm = ieee80211_add_ssid(frm, ic->ic_bss->ni_essid,
				ic->ic_bss->ni_esslen);
		frm = ieee80211_add_rates(frm, &ic->ic_bss->ni_rates);

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
		frm = ieee80211_add_xrates(frm, &ic->ic_bss->ni_rates);
		m->m_pkthdr.len = m->m_len = frm - mtod(m, u_int8_t *);
		break;

	case IEEE80211_FC0_SUBTYPE_AUTH:
		MGETHDR(m, M_DONTWAIT, MT_DATA);
		if (m == NULL)
			return ENOMEM;
		MH_ALIGN(m, 2 * 3);
		m->m_pkthdr.len = m->m_len = 6;
		frm = mtod(m, u_int8_t *);
		/* TODO: shared key auth */
		((u_int16_t *)frm)[0] = htole16(IEEE80211_AUTH_ALG_OPEN);
		((u_int16_t *)frm)[1] = htole16(arg);	/* sequence number */
		((u_int16_t *)frm)[2] = 0;		/* status */
		if (ic->ic_opmode == IEEE80211_M_STA)
			timer = IEEE80211_TRANS_WAIT;
		break;

	case IEEE80211_FC0_SUBTYPE_DEAUTH:
		if (ifp->if_flags & IFF_DEBUG)
			if_printf(ifp, "station %s deauthenticate (reason %d)\n",
			    ether_sprintf(ni->ni_macaddr), arg);
		MGETHDR(m, M_DONTWAIT, MT_DATA);
		if (m == NULL)
			return ENOMEM;
		MH_ALIGN(m, 2);
		m->m_pkthdr.len = m->m_len = 2;
		*mtod(m, u_int16_t *) = htole16(arg);	/* reason */
		break;

	case IEEE80211_FC0_SUBTYPE_ASSOC_REQ:
	case IEEE80211_FC0_SUBTYPE_REASSOC_REQ:
		/*
		 * asreq frame format
		 *	[2] capability information
		 *	[2] listen interval
		 *	[6*] current AP address (reassoc only)
		 *	[tlv] ssid
		 *	[tlv] supported rates
		 *	[tlv] extended supported rates
		 */
		m = ieee80211_getmbuf(M_DONTWAIT, MT_DATA,
			 sizeof(capinfo)
		       + sizeof(u_int16_t)
		       + IEEE80211_ADDR_LEN
		       + 2 + ni->ni_esslen
		       + 2 + IEEE80211_RATE_SIZE
		       + 2 + (IEEE80211_RATE_MAXSIZE - IEEE80211_RATE_SIZE));
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
		if (ic->ic_flags & IEEE80211_F_SHPREAMBLE)
			capinfo |= IEEE80211_CAPINFO_SHORT_PREAMBLE;
		if (ic->ic_flags & IEEE80211_F_SHSLOT)
			capinfo |= IEEE80211_CAPINFO_SHORT_SLOTTIME;
		*(u_int16_t *)frm = htole16(capinfo);
		frm += 2;

		*(u_int16_t *)frm = htole16(ic->ic_lintval);
		frm += 2;

		if (type == IEEE80211_FC0_SUBTYPE_REASSOC_REQ) {
			IEEE80211_ADDR_COPY(frm, ic->ic_bss->ni_bssid);
			frm += IEEE80211_ADDR_LEN;
		}

		frm = ieee80211_add_ssid(frm, ni->ni_essid, ni->ni_esslen);
		frm = ieee80211_add_rates(frm, &ni->ni_rates);
		frm = ieee80211_add_xrates(frm, &ni->ni_rates);
		m->m_pkthdr.len = m->m_len = frm - mtod(m, u_int8_t *);

		timer = IEEE80211_TRANS_WAIT;
		break;

	case IEEE80211_FC0_SUBTYPE_ASSOC_RESP:
	case IEEE80211_FC0_SUBTYPE_REASSOC_RESP:
		/*
		 * asreq frame format
		 *	[2] capability information
		 *	[2] status
		 *	[2] association ID
		 *	[tlv] supported rates
		 *	[tlv] extended supported rates
		 */
		m = ieee80211_getmbuf(M_DONTWAIT, MT_DATA,
			 sizeof(capinfo)
		       + sizeof(u_int16_t)
		       + sizeof(u_int16_t)
		       + 2 + IEEE80211_RATE_SIZE
		       + 2 + (IEEE80211_RATE_MAXSIZE - IEEE80211_RATE_SIZE));
		if (m == NULL)
			return ENOMEM;
		m->m_data += sizeof(struct ieee80211_frame);
		frm = mtod(m, u_int8_t *);

		capinfo = IEEE80211_CAPINFO_ESS;
		if (ic->ic_flags & IEEE80211_F_WEPON)
			capinfo |= IEEE80211_CAPINFO_PRIVACY;
		*(u_int16_t *)frm = htole16(capinfo);
		frm += 2;

		*(u_int16_t *)frm = htole16(arg);	/* status */
		frm += 2;

		if (arg == IEEE80211_STATUS_SUCCESS && ni != NULL)
			*(u_int16_t *)frm = htole16(ni->ni_associd);
		else
			*(u_int16_t *)frm = htole16(0);
		frm += 2;

		if (ni != NULL) {
			frm = ieee80211_add_rates(frm, &ni->ni_rates);
			frm = ieee80211_add_xrates(frm, &ni->ni_rates);
		} else {
			frm = ieee80211_add_rates(frm, &ic->ic_bss->ni_rates);
			frm = ieee80211_add_xrates(frm, &ic->ic_bss->ni_rates);
		}
		m->m_pkthdr.len = m->m_len = frm - mtod(m, u_int8_t *);
		break;

	case IEEE80211_FC0_SUBTYPE_DISASSOC:
		if (ifp->if_flags & IFF_DEBUG)
			if_printf(ifp, "station %s disassociate (reason %d)\n",
			    ether_sprintf(ni->ni_macaddr), arg);
		MGETHDR(m, M_DONTWAIT, MT_DATA);
		if (m == NULL)
			return ENOMEM;
		MH_ALIGN(m, 2);
		m->m_pkthdr.len = m->m_len = 2;
		*mtod(m, u_int16_t *) = htole16(arg);	/* reason */
		break;

	default:
		IEEE80211_DPRINTF(("%s: invalid mgmt frame type %u\n",
			__func__, type));
		return EINVAL;
	}

	ret = ieee80211_mgmt_output(ifp, ni, m, type);
	if (ret == 0 && timer)
		ic->ic_mgt_timer = timer;
	return ret;
}
