/*-
 * Copyright (c) 2002-2009 Sam Leffler, Errno Consulting
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

#include "opt_wlan.h"

#include <sys/param.h>
#include <sys/systm.h> 
#include <sys/mbuf.h>   
#include <sys/kernel.h>
#include <sys/endian.h>

#include <sys/socket.h>
 
#include <net/bpf.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_llc.h>
#include <net/if_media.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_input.h>
#include <net80211/ieee80211_phy.h>
#include <net80211/ieee80211_superg.h>

#define	ETHER_HEADER_COPY(dst, src) \
	memcpy(dst, src, sizeof(struct ether_header))

void
ieee80211_superg_attach(struct ieee80211com *ic)
{
}

void
ieee80211_superg_detach(struct ieee80211com *ic)
{
}

void
ieee80211_superg_vattach(struct ieee80211vap *vap)
{
	if (vap->iv_caps & IEEE80211_C_FF)
		vap->iv_flags |= IEEE80211_F_FF;
	if (vap->iv_caps & IEEE80211_C_TURBOP)
		vap->iv_flags |= IEEE80211_F_TURBOP;
}

void
ieee80211_superg_vdetach(struct ieee80211vap *vap)
{
}

#define	ATH_OUI_BYTES		0x00, 0x03, 0x7f
/*
 * Add a WME information element to a frame.
 */
uint8_t *
ieee80211_add_ath(uint8_t *frm, uint8_t caps, uint16_t defkeyix)
{
	static const struct ieee80211_ath_ie info = {
		.ath_id		= IEEE80211_ELEMID_VENDOR,
		.ath_len	= sizeof(struct ieee80211_ath_ie) - 2,
		.ath_oui	= { ATH_OUI_BYTES },
		.ath_oui_type	= ATH_OUI_TYPE,
		.ath_oui_subtype= ATH_OUI_SUBTYPE,
		.ath_version	= ATH_OUI_VERSION,
	};
	struct ieee80211_ath_ie *ath = (struct ieee80211_ath_ie *) frm;

	memcpy(frm, &info, sizeof(info));
	ath->ath_capability = caps;
	ath->ath_defkeyix[0] = (defkeyix & 0xff);
	ath->ath_defkeyix[1] = ((defkeyix >> 8) & 0xff);
	return frm + sizeof(info); 
}
#undef ATH_OUI_BYTES

void
ieee80211_parse_ath(struct ieee80211_node *ni, uint8_t *ie)
{
	const struct ieee80211_ath_ie *ath =
		(const struct ieee80211_ath_ie *) ie;

	ni->ni_ath_flags = ath->ath_capability;
	ni->ni_ath_defkeyix = LE_READ_2(&ath->ath_defkeyix);
}

int
ieee80211_parse_athparams(struct ieee80211_node *ni, uint8_t *frm,
	const struct ieee80211_frame *wh)
{
	struct ieee80211vap *vap = ni->ni_vap;
	const struct ieee80211_ath_ie *ath;
	u_int len = frm[1];
	int capschanged;
	uint16_t defkeyix;

	if (len < sizeof(struct ieee80211_ath_ie)-2) {
		IEEE80211_DISCARD_IE(vap,
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
		IEEE80211_NOTE(vap, IEEE80211_MSG_SUPERG, ni,
		    "ath ie change: new caps 0x%x defkeyix 0x%x",
		    ni->ni_ath_flags, ni->ni_ath_defkeyix);
	}
	if (IEEE80211_ATH_CAP(vap, ni, ATHEROS_CAP_TURBO_PRIME)) {
		uint16_t curflags, newflags;

		/*
		 * Check for turbo mode switch.  Calculate flags
		 * for the new mode and effect the switch.
		 */
		newflags = curflags = vap->iv_ic->ic_bsschan->ic_flags;
		/* NB: BOOST is not in ic_flags, so get it from the ie */
		if (ath->ath_capability & ATHEROS_CAP_BOOST) 
			newflags |= IEEE80211_CHAN_TURBO;
		else
			newflags &= ~IEEE80211_CHAN_TURBO;
		if (newflags != curflags)
			ieee80211_dturbo_switch(vap, newflags);
	}
	return capschanged;
}

/*
 * Decap the encapsulated frame pair and dispatch the first
 * for delivery.  The second frame is returned for delivery
 * via the normal path.
 */
struct mbuf *
ieee80211_ff_decap(struct ieee80211_node *ni, struct mbuf *m)
{
#define	FF_LLC_SIZE	(sizeof(struct ether_header) + sizeof(struct llc))
#define	MS(x,f)	(((x) & f) >> f##_S)
	struct ieee80211vap *vap = ni->ni_vap;
	struct llc *llc;
	uint32_t ath;
	struct mbuf *n;
	int framelen;

	/* NB: we assume caller does this check for us */
	KASSERT(IEEE80211_ATH_CAP(vap, ni, IEEE80211_NODE_FF),
	    ("ff not negotiated"));
	/*
	 * Check for fast-frame tunnel encapsulation.
	 */
	if (m->m_pkthdr.len < 3*FF_LLC_SIZE)
		return m;
	if (m->m_len < FF_LLC_SIZE &&
	    (m = m_pullup(m, FF_LLC_SIZE)) == NULL) {
		IEEE80211_DISCARD_MAC(vap, IEEE80211_MSG_ANY,
		    ni->ni_macaddr, "fast-frame",
		    "%s", "m_pullup(llc) failed");
		vap->iv_stats.is_rx_tooshort++;
		return NULL;
	}
	llc = (struct llc *)(mtod(m, uint8_t *) +
	    sizeof(struct ether_header));
	if (llc->llc_snap.ether_type != htons(ATH_FF_ETH_TYPE))
		return m;
	m_adj(m, FF_LLC_SIZE);
	m_copydata(m, 0, sizeof(uint32_t), (caddr_t) &ath);
	if (MS(ath, ATH_FF_PROTO) != ATH_FF_PROTO_L2TUNNEL) {
		IEEE80211_DISCARD_MAC(vap, IEEE80211_MSG_ANY,
		    ni->ni_macaddr, "fast-frame",
		    "unsupport tunnel protocol, header 0x%x", ath);
		vap->iv_stats.is_ff_badhdr++;
		m_freem(m);
		return NULL;
	}
	/* NB: skip header and alignment padding */
	m_adj(m, roundup(sizeof(uint32_t) - 2, 4) + 2);

	vap->iv_stats.is_ff_decap++;

	/*
	 * Decap the first frame, bust it apart from the
	 * second and deliver; then decap the second frame
	 * and return it to the caller for normal delivery.
	 */
	m = ieee80211_decap1(m, &framelen);
	if (m == NULL) {
		IEEE80211_DISCARD_MAC(vap, IEEE80211_MSG_ANY,
		    ni->ni_macaddr, "fast-frame", "%s", "first decap failed");
		vap->iv_stats.is_ff_tooshort++;
		return NULL;
	}
	n = m_split(m, framelen, M_NOWAIT);
	if (n == NULL) {
		IEEE80211_DISCARD_MAC(vap, IEEE80211_MSG_ANY,
		    ni->ni_macaddr, "fast-frame",
		    "%s", "unable to split encapsulated frames");
		vap->iv_stats.is_ff_split++;
		m_freem(m);			/* NB: must reclaim */
		return NULL;
	}
	/* XXX not right for WDS */
	vap->iv_deliver_data(vap, ni, m);	/* 1st of pair */

	/*
	 * Decap second frame.
	 */
	m_adj(n, roundup2(framelen, 4) - framelen);	/* padding */
	n = ieee80211_decap1(n, &framelen);
	if (n == NULL) {
		IEEE80211_DISCARD_MAC(vap, IEEE80211_MSG_ANY,
		    ni->ni_macaddr, "fast-frame", "%s", "second decap failed");
		vap->iv_stats.is_ff_tooshort++;
	}
	/* XXX verify framelen against mbuf contents */
	return n;				/* 2nd delivered by caller */
#undef MS
#undef FF_LLC_SIZE
}

/*
 * Do Ethernet-LLC encapsulation for each payload in a fast frame
 * tunnel encapsulation.  The frame is assumed to have an Ethernet
 * header at the front that must be stripped before prepending the
 * LLC followed by the Ethernet header passed in (with an Ethernet
 * type that specifies the payload size).
 */
static struct mbuf *
ff_encap1(struct ieee80211vap *vap, struct mbuf *m,
	const struct ether_header *eh)
{
	struct llc *llc;
	uint16_t payload;

	/* XXX optimize by combining m_adj+M_PREPEND */
	m_adj(m, sizeof(struct ether_header) - sizeof(struct llc));
	llc = mtod(m, struct llc *);
	llc->llc_dsap = llc->llc_ssap = LLC_SNAP_LSAP;
	llc->llc_control = LLC_UI;
	llc->llc_snap.org_code[0] = 0;
	llc->llc_snap.org_code[1] = 0;
	llc->llc_snap.org_code[2] = 0;
	llc->llc_snap.ether_type = eh->ether_type;
	payload = m->m_pkthdr.len;		/* NB: w/o Ethernet header */

	M_PREPEND(m, sizeof(struct ether_header), M_DONTWAIT);
	if (m == NULL) {		/* XXX cannot happen */
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_SUPERG,
			"%s: no space for ether_header\n", __func__);
		vap->iv_stats.is_tx_nobuf++;
		return NULL;
	}
	ETHER_HEADER_COPY(mtod(m, void *), eh);
	mtod(m, struct ether_header *)->ether_type = htons(payload);
	return m;
}

/*
 * Fast frame encapsulation.  There must be two packets
 * chained with m_nextpkt.  We do header adjustment for
 * each, add the tunnel encapsulation, and then concatenate
 * the mbuf chains to form a single frame for transmission.
 */
struct mbuf *
ieee80211_ff_encap(struct ieee80211vap *vap, struct mbuf *m1, int hdrspace,
	struct ieee80211_key *key)
{
	struct mbuf *m2;
	struct ether_header eh1, eh2;
	struct llc *llc;
	struct mbuf *m;
	int pad;

	m2 = m1->m_nextpkt;
	if (m2 == NULL) {
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_SUPERG,
		    "%s: only one frame\n", __func__);
		goto bad;
	}
	m1->m_nextpkt = NULL;
	/*
	 * Include fast frame headers in adjusting header
	 * layout; this allocates space according to what
	 * ff_encap will do.
	 */
	m1 = ieee80211_mbuf_adjust(vap,
		hdrspace + sizeof(struct llc) + sizeof(uint32_t) + 2 +
		    sizeof(struct ether_header),
		key, m1);
	if (m1 == NULL) {
		/* NB: ieee80211_mbuf_adjust handles msgs+statistics */
		m_freem(m2);
		goto bad;
	}

	/*
	 * Copy second frame's Ethernet header out of line
	 * and adjust for encapsulation headers.  Note that
	 * we make room for padding in case there isn't room
	 * at the end of first frame.
	 */
	KASSERT(m2->m_len >= sizeof(eh2), ("no ethernet header!"));
	ETHER_HEADER_COPY(&eh2, mtod(m2, caddr_t));
	m2 = ieee80211_mbuf_adjust(vap,
		ATH_FF_MAX_HDR_PAD + sizeof(struct ether_header),
		NULL, m2);
	if (m2 == NULL) {
		/* NB: ieee80211_mbuf_adjust handles msgs+statistics */
		goto bad;
	}

	/*
	 * Now do tunnel encapsulation.  First, each
	 * frame gets a standard encapsulation.
	 */
	m1 = ff_encap1(vap, m1, &eh1);
	if (m1 == NULL)
		goto bad;
	m2 = ff_encap1(vap, m2, &eh2);
	if (m2 == NULL)
		goto bad;

	/*
	 * Pad leading frame to a 4-byte boundary.  If there
	 * is space at the end of the first frame, put it
	 * there; otherwise prepend to the front of the second
	 * frame.  We know doing the second will always work
	 * because we reserve space above.  We prefer appending
	 * as this typically has better DMA alignment properties.
	 */
	for (m = m1; m->m_next != NULL; m = m->m_next)
		;
	pad = roundup2(m1->m_pkthdr.len, 4) - m1->m_pkthdr.len;
	if (pad) {
		if (M_TRAILINGSPACE(m) < pad) {		/* prepend to second */
			m2->m_data -= pad;
			m2->m_len += pad;
			m2->m_pkthdr.len += pad;
		} else {				/* append to first */
			m->m_len += pad;
			m1->m_pkthdr.len += pad;
		}
	}

	/*
	 * Now, stick 'em together and prepend the tunnel headers;
	 * first the Atheros tunnel header (all zero for now) and
	 * then a special fast frame LLC.
	 *
	 * XXX optimize by prepending together
	 */
	m->m_next = m2;			/* NB: last mbuf from above */
	m1->m_pkthdr.len += m2->m_pkthdr.len;
	M_PREPEND(m1, sizeof(uint32_t)+2, M_DONTWAIT);
	if (m1 == NULL) {		/* XXX cannot happen */
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_SUPERG,
		    "%s: no space for tunnel header\n", __func__);
		vap->iv_stats.is_tx_nobuf++;
		return NULL;
	}
	memset(mtod(m1, void *), 0, sizeof(uint32_t)+2);

	M_PREPEND(m1, sizeof(struct llc), M_DONTWAIT);
	if (m1 == NULL) {		/* XXX cannot happen */
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_SUPERG,
		    "%s: no space for llc header\n", __func__);
		vap->iv_stats.is_tx_nobuf++;
		return NULL;
	}
	llc = mtod(m1, struct llc *);
	llc->llc_dsap = llc->llc_ssap = LLC_SNAP_LSAP;
	llc->llc_control = LLC_UI;
	llc->llc_snap.org_code[0] = ATH_FF_SNAP_ORGCODE_0;
	llc->llc_snap.org_code[1] = ATH_FF_SNAP_ORGCODE_1;
	llc->llc_snap.org_code[2] = ATH_FF_SNAP_ORGCODE_2;
	llc->llc_snap.ether_type = htons(ATH_FF_ETH_TYPE);

	vap->iv_stats.is_ff_encap++;

	return m1;
bad:
	if (m1 != NULL)
		m_freem(m1);
	if (m2 != NULL)
		m_freem(m2);
	return NULL;
}

/*
 * Switch between turbo and non-turbo operating modes.
 * Use the specified channel flags to locate the new
 * channel, update 802.11 state, and then call back into
 * the driver to effect the change.
 */
void
ieee80211_dturbo_switch(struct ieee80211vap *vap, int newflags)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_channel *chan;

	chan = ieee80211_find_channel(ic, ic->ic_bsschan->ic_freq, newflags);
	if (chan == NULL) {		/* XXX should not happen */
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_SUPERG,
		    "%s: no channel with freq %u flags 0x%x\n",
		    __func__, ic->ic_bsschan->ic_freq, newflags);
		return;
	}

	IEEE80211_DPRINTF(vap, IEEE80211_MSG_SUPERG,
	    "%s: %s -> %s (freq %u flags 0x%x)\n", __func__,
	    ieee80211_phymode_name[ieee80211_chan2mode(ic->ic_bsschan)],
	    ieee80211_phymode_name[ieee80211_chan2mode(chan)],
	    chan->ic_freq, chan->ic_flags);

	ic->ic_bsschan = chan;
	ic->ic_prevchan = ic->ic_curchan;
	ic->ic_curchan = chan;
	ic->ic_set_channel(ic);
	/* NB: do not need to reset ERP state 'cuz we're in sta mode */
}

/*
 * Return the current ``state'' of an Atheros capbility.
 * If associated in station mode report the negotiated
 * setting. Otherwise report the current setting.
 */
static int
getathcap(struct ieee80211vap *vap, int cap)
{
	if (vap->iv_opmode == IEEE80211_M_STA &&
	    vap->iv_state == IEEE80211_S_RUN)
		return IEEE80211_ATH_CAP(vap, vap->iv_bss, cap) != 0;
	else
		return (vap->iv_flags & cap) != 0;
}

static int
superg_ioctl_get80211(struct ieee80211vap *vap, struct ieee80211req *ireq)
{
	switch (ireq->i_type) {
	case IEEE80211_IOC_FF:
		ireq->i_val = getathcap(vap, IEEE80211_F_FF);
		break;
	case IEEE80211_IOC_TURBOP:
		ireq->i_val = getathcap(vap, IEEE80211_F_TURBOP);
		break;
	default:
		return ENOSYS;
	}
	return 0;
}
IEEE80211_IOCTL_GET(superg, superg_ioctl_get80211);

static int
superg_ioctl_set80211(struct ieee80211vap *vap, struct ieee80211req *ireq)
{
	switch (ireq->i_type) {
	case IEEE80211_IOC_FF:
		if (ireq->i_val) {
			if ((vap->iv_caps & IEEE80211_C_FF) == 0)
				return EOPNOTSUPP;
			vap->iv_flags |= IEEE80211_F_FF;
		} else
			vap->iv_flags &= ~IEEE80211_F_FF;
		return ERESTART;
	case IEEE80211_IOC_TURBOP:
		if (ireq->i_val) {
			if ((vap->iv_caps & IEEE80211_C_TURBOP) == 0)
				return EOPNOTSUPP;
			vap->iv_flags |= IEEE80211_F_TURBOP;
		} else
			vap->iv_flags &= ~IEEE80211_F_TURBOP;
		return ENETRESET;
	default:
		return ENOSYS;
	}
	return 0;
}
IEEE80211_IOCTL_SET(superg, superg_ioctl_set80211);
