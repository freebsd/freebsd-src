/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2009 Sam Leffler, Errno Consulting
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
 * IEEE 802.11 radiotap support.
 */
#include "opt_wlan.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>   
#include <sys/malloc.h>
#include <sys/endian.h>
#include <sys/kernel.h>

#include <sys/socket.h>

#include <net/bpf.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_private.h>
#include <net/if_media.h>
#include <net/ethernet.h>
#include <net/route.h>

#include <net80211/ieee80211_var.h>

static int radiotap_offset(struct ieee80211_radiotap_header *, int, int);

void
ieee80211_radiotap_attach(struct ieee80211com *ic,
	struct ieee80211_radiotap_header *th, int tlen, uint32_t tx_radiotap,
	struct ieee80211_radiotap_header *rh, int rlen, uint32_t rx_radiotap)
{
	ieee80211_radiotap_attachv(ic, th, tlen, 0, tx_radiotap,
	    rh, rlen, 0, rx_radiotap);
}

void
ieee80211_radiotap_attachv(struct ieee80211com *ic,
	struct ieee80211_radiotap_header *th,
	int tlen, int n_tx_v, uint32_t tx_radiotap,
	struct ieee80211_radiotap_header *rh,
	int rlen, int n_rx_v, uint32_t rx_radiotap)
{
#define	B(_v)	(1<<(_v))
	int off;

	th->it_len = htole16(roundup2(tlen, sizeof(uint32_t)));
	th->it_present = htole32(tx_radiotap);
	ic->ic_th = th;
	/* calculate offset to channel data */
	off = -1;
	if (tx_radiotap & B(IEEE80211_RADIOTAP_CHANNEL))
		off = radiotap_offset(th, n_tx_v, IEEE80211_RADIOTAP_CHANNEL);
	else if (tx_radiotap & B(IEEE80211_RADIOTAP_XCHANNEL))
		off = radiotap_offset(th, n_tx_v, IEEE80211_RADIOTAP_XCHANNEL);
	if (off == -1) {
		ic_printf(ic, "%s: no tx channel, radiotap 0x%x\n", __func__,
		    tx_radiotap);
		/* NB: we handle this case but data will have no chan spec */
	} else
		ic->ic_txchan = ((uint8_t *) th) + off;

	rh->it_len = htole16(roundup2(rlen, sizeof(uint32_t)));
	rh->it_present = htole32(rx_radiotap);
	ic->ic_rh = rh;
	/* calculate offset to channel data */
	off = -1;
	if (rx_radiotap & B(IEEE80211_RADIOTAP_CHANNEL))
		off = radiotap_offset(rh, n_rx_v, IEEE80211_RADIOTAP_CHANNEL);
	else if (rx_radiotap & B(IEEE80211_RADIOTAP_XCHANNEL))
		off = radiotap_offset(rh, n_rx_v, IEEE80211_RADIOTAP_XCHANNEL);
	if (off == -1) {
		ic_printf(ic, "%s: no rx channel, radiotap 0x%x\n", __func__,
		    rx_radiotap);
		/* NB: we handle this case but data will have no chan spec */
	} else
		ic->ic_rxchan = ((uint8_t *) rh) + off;
#undef B
}

void
ieee80211_radiotap_detach(struct ieee80211com *ic)
{
}

void
ieee80211_radiotap_vdetach(struct ieee80211vap *vap)
{
	/* NB: bpfdetach is called by ether_ifdetach and claims all taps */
}

static void
set_channel(void *p, const struct ieee80211_channel *c)
{
	struct {
		uint16_t	freq;
		uint16_t	flags;
	} *rc = p;

	rc->freq = htole16(c->ic_freq);
	rc->flags = htole16(c->ic_flags);
}

static void
set_xchannel(void *p, const struct ieee80211_channel *c)
{
	struct {
		uint32_t	flags;
		uint16_t	freq;
		uint8_t		ieee;
		uint8_t		maxpow;
	} *rc = p;

	rc->flags = htole32(c->ic_flags);
	rc->freq = htole16(c->ic_freq);
	rc->ieee = c->ic_ieee;
	rc->maxpow = c->ic_maxregpower;
}

/*
 * Update radiotap state on channel change.
 */
void
ieee80211_radiotap_chan_change(struct ieee80211com *ic)
{
	if (ic->ic_rxchan != NULL) {
		struct ieee80211_radiotap_header *rh = ic->ic_rh;

		if (rh->it_present & htole32(1<<IEEE80211_RADIOTAP_XCHANNEL))
			set_xchannel(ic->ic_rxchan, ic->ic_curchan);
		else if (rh->it_present & htole32(1<<IEEE80211_RADIOTAP_CHANNEL))
			set_channel(ic->ic_rxchan, ic->ic_curchan);
	}
	if (ic->ic_txchan != NULL) {
		struct ieee80211_radiotap_header *th = ic->ic_th;

		if (th->it_present & htole32(1<<IEEE80211_RADIOTAP_XCHANNEL))
			set_xchannel(ic->ic_txchan, ic->ic_curchan);
		else if (th->it_present & htole32(1<<IEEE80211_RADIOTAP_CHANNEL))
			set_channel(ic->ic_txchan, ic->ic_curchan);
	}
}

/*
 * Distribute radiotap data (+packet) to all monitor mode
 * vaps with an active tap other than vap0.
 */
static void
spam_vaps(struct ieee80211vap *vap0, struct mbuf *m,
	struct ieee80211_radiotap_header *rh, int len)
{
	struct ieee80211com *ic = vap0->iv_ic;
	struct ieee80211vap *vap;

	TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
		if (vap != vap0 &&
		    vap->iv_opmode == IEEE80211_M_MONITOR &&
		    (vap->iv_flags_ext & IEEE80211_FEXT_BPF) &&
		    vap->iv_state != IEEE80211_S_INIT)
			bpf_mtap2(vap->iv_rawbpf, rh, len, m);
	}
}

/*
 * Dispatch radiotap data for transmitted packet.
 */
void
ieee80211_radiotap_tx(struct ieee80211vap *vap0, struct mbuf *m)
{
	struct ieee80211com *ic = vap0->iv_ic;
	struct ieee80211_radiotap_header *th = ic->ic_th;
	int len;

	KASSERT(th != NULL, ("no tx radiotap header"));
	len = le16toh(th->it_len);

	if (vap0->iv_flags_ext & IEEE80211_FEXT_BPF)
		bpf_mtap2(vap0->iv_rawbpf, th, len, m);
	/*
	 * Spam monitor mode vaps.
	 */
	if (ic->ic_montaps != 0)
		spam_vaps(vap0, m, th, len);
}

/*
 * Dispatch radiotap data for received packet.
 */
void
ieee80211_radiotap_rx(struct ieee80211vap *vap0, struct mbuf *m)
{
	struct ieee80211com *ic = vap0->iv_ic;
	struct ieee80211_radiotap_header *rh = ic->ic_rh;
	int len;

	KASSERT(rh != NULL, ("no rx radiotap header"));
	len = le16toh(rh->it_len);

	if (vap0->iv_flags_ext & IEEE80211_FEXT_BPF)
		bpf_mtap2(vap0->iv_rawbpf, rh, len, m);
	/*
	 * Spam monitor mode vaps with unicast frames.  Multicast
	 * frames are handled by passing through ieee80211_input_all
	 * which distributes copies to the monitor mode vaps.
	 */
	if (ic->ic_montaps != 0 && (m->m_flags & M_BCAST) == 0)
		spam_vaps(vap0, m, rh, len);
}

/*
 * Dispatch radiotap data for a packet received outside the normal
 * rx processing path; this is used, for example, to handle frames
 * received with errors that would otherwise be dropped.
 */
void
ieee80211_radiotap_rx_all(struct ieee80211com *ic, struct mbuf *m)
{
	struct ieee80211_radiotap_header *rh = ic->ic_rh;
	int len = le16toh(rh->it_len);
	struct ieee80211vap *vap;

	/* XXX locking? */
	TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
		if (ieee80211_radiotap_active_vap(vap) &&
		    vap->iv_state != IEEE80211_S_INIT)
			bpf_mtap2(vap->iv_rawbpf, rh, len, m);
	}
}

/*
 * Return the offset of the specified item in the radiotap
 * header description.  If the item is not present or is not
 * known -1 is returned.
 */
static int
radiotap_offset(struct ieee80211_radiotap_header *rh,
    int n_vendor_attributes, int item)
{
	static const struct {
		size_t	align, width;
	} items[] = {
		[IEEE80211_RADIOTAP_TSFT] = {
		    .align	= sizeof(uint64_t),
		    .width	= sizeof(uint64_t),
		},
		[IEEE80211_RADIOTAP_FLAGS] = {
		    .align	= sizeof(uint8_t),
		    .width	= sizeof(uint8_t),
		},
		[IEEE80211_RADIOTAP_RATE] = {
		    .align	= sizeof(uint8_t),
		    .width	= sizeof(uint8_t),
		},
		[IEEE80211_RADIOTAP_CHANNEL] = {
		    .align	= sizeof(uint16_t),
		    .width	= 2*sizeof(uint16_t),
		},
		[IEEE80211_RADIOTAP_FHSS] = {
		    .align	= sizeof(uint16_t),
		    .width	= sizeof(uint16_t),
		},
		[IEEE80211_RADIOTAP_DBM_ANTSIGNAL] = {
		    .align	= sizeof(uint8_t),
		    .width	= sizeof(uint8_t),
		},
		[IEEE80211_RADIOTAP_DBM_ANTNOISE] = {
		    .align	= sizeof(uint8_t),
		    .width	= sizeof(uint8_t),
		},
		[IEEE80211_RADIOTAP_LOCK_QUALITY] = {
		    .align	= sizeof(uint16_t),
		    .width	= sizeof(uint16_t),
		},
		[IEEE80211_RADIOTAP_TX_ATTENUATION] = {
		    .align	= sizeof(uint16_t),
		    .width	= sizeof(uint16_t),
		},
		[IEEE80211_RADIOTAP_DB_TX_ATTENUATION] = {
		    .align	= sizeof(uint16_t),
		    .width	= sizeof(uint16_t),
		},
		[IEEE80211_RADIOTAP_DBM_TX_POWER] = {
		    .align	= sizeof(uint8_t),
		    .width	= sizeof(uint8_t),
		},
		[IEEE80211_RADIOTAP_ANTENNA] = {
		    .align	= sizeof(uint8_t),
		    .width	= sizeof(uint8_t),
		},
		[IEEE80211_RADIOTAP_DB_ANTSIGNAL] = {
		    .align	= sizeof(uint8_t),
		    .width	= sizeof(uint8_t),
		},
		[IEEE80211_RADIOTAP_DB_ANTNOISE] = {
		    .align	= sizeof(uint8_t),
		    .width	= sizeof(uint8_t),
		},
		[IEEE80211_RADIOTAP_XCHANNEL] = {
		    .align	= sizeof(uint32_t),
		    .width	= 2*sizeof(uint32_t),
		},
		[IEEE80211_RADIOTAP_MCS] = {
		    .align	= sizeof(uint8_t),
		    .width	= 3*sizeof(uint8_t),
		},
	};
	uint32_t present = le32toh(rh->it_present);
	int off, i;

	off = sizeof(struct ieee80211_radiotap_header);
	off += n_vendor_attributes * (sizeof(uint32_t));

	for (i = 0; i < IEEE80211_RADIOTAP_EXT; i++) {
		if ((present & (1<<i)) == 0)
			continue;
		if (items[i].align == 0) {
			/* NB: unidentified element, don't guess */
			net80211_printf("%s: unknown item %d\n", __func__, i);
			return -1;
		}
		off = roundup2(off, items[i].align);
		if (i == item) {
			if (off + items[i].width > le16toh(rh->it_len)) {
				/* NB: item does not fit in header data */
				net80211_printf("%s: item %d not in header data, "
				    "off %d width %zu len %d\n", __func__, i,
				    off, items[i].width, le16toh(rh->it_len));
				return -1;
			}
			return off;
		}
		off += items[i].width;
	}
	return -1;
}

static bool
bpf_ieee80211_chkdir(void *arg, const struct mbuf *m, int dir)
{
	struct ifnet *ifp = arg;
	struct ifnet *rcvif = m_rcvif(m);

	return ((dir == BPF_D_IN && ifp != rcvif) ||
	    (dir == BPF_D_OUT && ifp == rcvif));
}

static void
bpf_ieee80211_attach(void *sc)
{
	struct ieee80211vap *vap = if_getsoftc((if_t)sc);

	/*
	 * Track bpf radiotap listener state.  We mark the vap
	 * to indicate if any listener is present and the com
	 * to indicate if any listener exists on any associated
	 * vap.  This flag is used by drivers to prepare radiotap
	 * state only when needed.
	 */
	ieee80211_syncflag_ext(vap, IEEE80211_FEXT_BPF);
	if (vap->iv_opmode == IEEE80211_M_MONITOR)
		atomic_add_int(&vap->iv_ic->ic_montaps, 1);
}

static void
bpf_ieee80211_detach(void *sc)
{
	struct ieee80211vap *vap = if_getsoftc((if_t)sc);

	if (!bpf_peers_present(vap->iv_rawbpf)) {
		ieee80211_syncflag_ext(vap, -IEEE80211_FEXT_BPF);
		if (vap->iv_opmode == IEEE80211_M_MONITOR)
			atomic_subtract_int(&vap->iv_ic->ic_montaps, 1);
	}
}

static int
bpf_ieee80211_write(void *arg, struct mbuf *m, struct mbuf *mc, int flags)
{
	struct ifnet *ifp = arg;
	struct sockaddr dst = {
		.sa_family = AF_IEEE80211,
		/* XXXGL: value and XXX comment from 246b5467621a */
		.sa_len = 12,	/* XXX != 0 */
	};
	struct route ro = {
		.ro_prepend = (char *)&dst.sa_data,
		.ro_flags = RT_HAS_HEADER,
	};
	const struct ieee80211_bpf_params *p;
	u_int hlen;
	int error;

	NET_EPOCH_ASSERT();

	/*
	 * Collect true length from the parameter header.
	 * XXX check ibp_vers
	 */
	p = mtod(m, const struct ieee80211_bpf_params *);
	if (p->ibp_len > sizeof(dst.sa_data)) {
		m_freem(m);
		m_freem(mc);
		return (EMSGSIZE);
	}
	hlen = ro.ro_plen = p->ibp_len;
	bcopy(mtod(m, const void *), &dst.sa_data, hlen);
	m->m_pkthdr.len -= hlen;
	m->m_len -= hlen;
	m->m_data += hlen;

	CURVNET_SET(ifp->if_vnet);
	error = ifp->if_output(ifp, m, &dst, &ro);
	if (error != 0) {
		m_freem(mc);
	} else if (mc != NULL) {
		mc->m_pkthdr.rcvif = ifp;
		(void)ifp->if_input(ifp, mc);
	}
	CURVNET_RESTORE();

	return (error);
}

static const struct bif_methods bpf_ieee80211_methods = {
	.bif_chkdir = bpf_ieee80211_chkdir,
	.bif_attachd = bpf_ieee80211_attach,
	.bif_detachd = bpf_ieee80211_detach,
	.bif_promisc = bpf_ifnet_promisc,
	.bif_wrsize = bpf_ifnet_wrsize,
	.bif_write = bpf_ieee80211_write,
};

void
ieee80211_radiotap_vattach(struct ieee80211vap *vap)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_radiotap_header *th = ic->ic_th;

	if (th != NULL && ic->ic_rh != NULL) {
		/* radiotap DLT for raw 802.11 frames */
		vap->iv_rawbpf = bpf_attach(if_name(vap->iv_ifp),
		    DLT_IEEE802_11_RADIO,
		    sizeof(struct ieee80211_frame) + le16toh(th->it_len),
		    &bpf_ieee80211_methods, vap->iv_ifp);
		if_ref(vap->iv_ifp);
	}
}
