/*-
 * Copyright (c) 2002-2009 Sam Leffler, Errno Consulting
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    similar to the "NO WARRANTY" disclaimer below ("Disclaimer") and any
 *    redistribution must be conditioned upon including a substantially
 *    similar Disclaimer requirement for further binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT, MERCHANTIBILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Driver for the Atheros Wireless LAN controller.
 *
 * This software is derived from work of Atsushi Onoe; his contribution
 * is greatly appreciated.
 */

#include "opt_inet.h"
#include "opt_ath.h"
#include "opt_wlan.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/errno.h>
#include <sys/callout.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/kthread.h>
#include <sys/taskqueue.h>
#include <sys/priv.h>

#include <machine/bus.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_llc.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_regdomain.h>
#ifdef IEEE80211_SUPPORT_SUPERG
#include <net80211/ieee80211_superg.h>
#endif
#ifdef IEEE80211_SUPPORT_TDMA
#include <net80211/ieee80211_tdma.h>
#endif
#include <net80211/ieee80211_ht.h>

#include <net/bpf.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#endif

#include <dev/ath/if_athvar.h>
#include <dev/ath/ath_hal/ah_devid.h>		/* XXX for softled */
#include <dev/ath/ath_hal/ah_diagcodes.h>

#include <dev/ath/if_ath_debug.h>

#ifdef ATH_TX99_DIAG
#include <dev/ath/ath_tx99/ath_tx99.h>
#endif

#include <dev/ath/if_ath_misc.h>
#include <dev/ath/if_ath_tx.h>
#include <dev/ath/if_ath_tx_ht.h>

static int ath_tx_ampdu_pending(struct ath_softc *sc, struct ath_node *an,
    int tid);
static int ath_tx_ampdu_running(struct ath_softc *sc, struct ath_node *an,
    int tid);
static ieee80211_seq ath_tx_tid_seqno_assign(struct ath_softc *sc,
    struct ieee80211_node *ni, struct ath_buf *bf, struct mbuf *m0);
static int ath_tx_action_frame_override_queue(struct ath_softc *sc,
    struct ieee80211_node *ni, struct mbuf *m0, int *tid);

/*
 * Whether to use the 11n rate scenario functions or not
 */
static inline int
ath_tx_is_11n(struct ath_softc *sc)
{
	return (sc->sc_ah->ah_magic == 0x20065416);
}

void
ath_txfrag_cleanup(struct ath_softc *sc,
	ath_bufhead *frags, struct ieee80211_node *ni)
{
	struct ath_buf *bf, *next;

	ATH_TXBUF_LOCK_ASSERT(sc);

	STAILQ_FOREACH_SAFE(bf, frags, bf_list, next) {
		/* NB: bf assumed clean */
		STAILQ_REMOVE_HEAD(frags, bf_list);
		STAILQ_INSERT_HEAD(&sc->sc_txbuf, bf, bf_list);
		ieee80211_node_decref(ni);
	}
}

/*
 * Setup xmit of a fragmented frame.  Allocate a buffer
 * for each frag and bump the node reference count to
 * reflect the held reference to be setup by ath_tx_start.
 */
int
ath_txfrag_setup(struct ath_softc *sc, ath_bufhead *frags,
	struct mbuf *m0, struct ieee80211_node *ni)
{
	struct mbuf *m;
	struct ath_buf *bf;

	ATH_TXBUF_LOCK(sc);
	for (m = m0->m_nextpkt; m != NULL; m = m->m_nextpkt) {
		bf = _ath_getbuf_locked(sc);
		if (bf == NULL) {	/* out of buffers, cleanup */
			ath_txfrag_cleanup(sc, frags, ni);
			break;
		}
		ieee80211_node_incref(ni);
		STAILQ_INSERT_TAIL(frags, bf, bf_list);
	}
	ATH_TXBUF_UNLOCK(sc);

	return !STAILQ_EMPTY(frags);
}

/*
 * Reclaim mbuf resources.  For fragmented frames we
 * need to claim each frag chained with m_nextpkt.
 */
void
ath_freetx(struct mbuf *m)
{
	struct mbuf *next;

	do {
		next = m->m_nextpkt;
		m->m_nextpkt = NULL;
		m_freem(m);
	} while ((m = next) != NULL);
}

static int
ath_tx_dmasetup(struct ath_softc *sc, struct ath_buf *bf, struct mbuf *m0)
{
	struct mbuf *m;
	int error;

	/*
	 * Load the DMA map so any coalescing is done.  This
	 * also calculates the number of descriptors we need.
	 */
	error = bus_dmamap_load_mbuf_sg(sc->sc_dmat, bf->bf_dmamap, m0,
				     bf->bf_segs, &bf->bf_nseg,
				     BUS_DMA_NOWAIT);
	if (error == EFBIG) {
		/* XXX packet requires too many descriptors */
		bf->bf_nseg = ATH_TXDESC+1;
	} else if (error != 0) {
		sc->sc_stats.ast_tx_busdma++;
		ath_freetx(m0);
		return error;
	}
	/*
	 * Discard null packets and check for packets that
	 * require too many TX descriptors.  We try to convert
	 * the latter to a cluster.
	 */
	if (bf->bf_nseg > ATH_TXDESC) {		/* too many desc's, linearize */
		sc->sc_stats.ast_tx_linear++;
		m = m_collapse(m0, M_DONTWAIT, ATH_TXDESC);
		if (m == NULL) {
			ath_freetx(m0);
			sc->sc_stats.ast_tx_nombuf++;
			return ENOMEM;
		}
		m0 = m;
		error = bus_dmamap_load_mbuf_sg(sc->sc_dmat, bf->bf_dmamap, m0,
					     bf->bf_segs, &bf->bf_nseg,
					     BUS_DMA_NOWAIT);
		if (error != 0) {
			sc->sc_stats.ast_tx_busdma++;
			ath_freetx(m0);
			return error;
		}
		KASSERT(bf->bf_nseg <= ATH_TXDESC,
		    ("too many segments after defrag; nseg %u", bf->bf_nseg));
	} else if (bf->bf_nseg == 0) {		/* null packet, discard */
		sc->sc_stats.ast_tx_nodata++;
		ath_freetx(m0);
		return EIO;
	}
	DPRINTF(sc, ATH_DEBUG_XMIT, "%s: m %p len %u\n",
		__func__, m0, m0->m_pkthdr.len);
	bus_dmamap_sync(sc->sc_dmat, bf->bf_dmamap, BUS_DMASYNC_PREWRITE);
	bf->bf_m = m0;

	return 0;
}

static void
ath_tx_chaindesclist(struct ath_softc *sc, struct ath_buf *bf)
{
	struct ath_hal *ah = sc->sc_ah;
	struct ath_desc *ds, *ds0;
	int i;

	/*
	 * Fillin the remainder of the descriptor info.
	 */
	ds0 = ds = bf->bf_desc;
	for (i = 0; i < bf->bf_nseg; i++, ds++) {
		ds->ds_data = bf->bf_segs[i].ds_addr;
		if (i == bf->bf_nseg - 1)
			ds->ds_link = 0;
		else
			ds->ds_link = bf->bf_daddr + sizeof(*ds) * (i + 1);
		ath_hal_filltxdesc(ah, ds
			, bf->bf_segs[i].ds_len	/* segment length */
			, i == 0		/* first segment */
			, i == bf->bf_nseg - 1	/* last segment */
			, ds0			/* first descriptor */
		);
		DPRINTF(sc, ATH_DEBUG_XMIT,
			"%s: %d: %08x %08x %08x %08x %08x %08x\n",
			__func__, i, ds->ds_link, ds->ds_data,
			ds->ds_ctl0, ds->ds_ctl1, ds->ds_hw[0], ds->ds_hw[1]);
	}

}

static void
ath_tx_handoff_mcast(struct ath_softc *sc, struct ath_txq *txq, struct ath_buf *bf)
{
	ATH_TXQ_LOCK_ASSERT(txq);
	KASSERT((bf->bf_flags & ATH_BUF_BUSY) == 0,
	     ("busy status 0x%x", bf->bf_flags));
	if (txq->axq_link != NULL) {
		struct ath_buf *last = ATH_TXQ_LAST(txq);
		struct ieee80211_frame *wh;

		/* mark previous frame */
		wh = mtod(last->bf_m, struct ieee80211_frame *);
		wh->i_fc[1] |= IEEE80211_FC1_MORE_DATA;
		bus_dmamap_sync(sc->sc_dmat, last->bf_dmamap,
		    BUS_DMASYNC_PREWRITE);

		/* link descriptor */
		*txq->axq_link = bf->bf_daddr;
	}
	ATH_TXQ_INSERT_TAIL(txq, bf, bf_list);
	txq->axq_link = &bf->bf_desc[bf->bf_nseg - 1].ds_link;
}



/*
 * Hand-off packet to a hardware queue.
 */
static void
ath_tx_handoff_hw(struct ath_softc *sc, struct ath_txq *txq, struct ath_buf *bf)
{
	struct ath_hal *ah = sc->sc_ah;

	/*
	 * Insert the frame on the outbound list and pass it on
	 * to the hardware.  Multicast frames buffered for power
	 * save stations and transmit from the CAB queue are stored
	 * on a s/w only queue and loaded on to the CAB queue in
	 * the SWBA handler since frames only go out on DTIM and
	 * to avoid possible races.
	 */
	ATH_TXQ_LOCK_ASSERT(txq);
	KASSERT((bf->bf_flags & ATH_BUF_BUSY) == 0,
	     ("busy status 0x%x", bf->bf_flags));
	KASSERT(txq->axq_qnum != ATH_TXQ_SWQ,
	     ("ath_tx_handoff_hw called for mcast queue"));

	/* For now, so not to generate whitespace diffs */
	if (1) {
#ifdef IEEE80211_SUPPORT_TDMA
		int qbusy;

		ATH_TXQ_INSERT_TAIL(txq, bf, bf_list);
		qbusy = ath_hal_txqenabled(ah, txq->axq_qnum);
		if (txq->axq_link == NULL) {
			/*
			 * Be careful writing the address to TXDP.  If
			 * the tx q is enabled then this write will be
			 * ignored.  Normally this is not an issue but
			 * when tdma is in use and the q is beacon gated
			 * this race can occur.  If the q is busy then
			 * defer the work to later--either when another
			 * packet comes along or when we prepare a beacon
			 * frame at SWBA.
			 */
			if (!qbusy) {
				ath_hal_puttxbuf(ah, txq->axq_qnum, bf->bf_daddr);
				txq->axq_flags &= ~ATH_TXQ_PUTPENDING;
				DPRINTF(sc, ATH_DEBUG_XMIT,
				    "%s: TXDP[%u] = %p (%p) depth %d\n",
				    __func__, txq->axq_qnum,
				    (caddr_t)bf->bf_daddr, bf->bf_desc,
				    txq->axq_depth);
			} else {
				txq->axq_flags |= ATH_TXQ_PUTPENDING;
				DPRINTF(sc, ATH_DEBUG_TDMA | ATH_DEBUG_XMIT,
				    "%s: Q%u busy, defer enable\n", __func__,
				    txq->axq_qnum);
			}
		} else {
			*txq->axq_link = bf->bf_daddr;
			DPRINTF(sc, ATH_DEBUG_XMIT,
			    "%s: link[%u](%p)=%p (%p) depth %d\n", __func__,
			    txq->axq_qnum, txq->axq_link,
			    (caddr_t)bf->bf_daddr, bf->bf_desc, txq->axq_depth);
			if ((txq->axq_flags & ATH_TXQ_PUTPENDING) && !qbusy) {
				/*
				 * The q was busy when we previously tried
				 * to write the address of the first buffer
				 * in the chain.  Since it's not busy now
				 * handle this chore.  We are certain the
				 * buffer at the front is the right one since
				 * axq_link is NULL only when the buffer list
				 * is/was empty.
				 */
				ath_hal_puttxbuf(ah, txq->axq_qnum,
					STAILQ_FIRST(&txq->axq_q)->bf_daddr);
				txq->axq_flags &= ~ATH_TXQ_PUTPENDING;
				DPRINTF(sc, ATH_DEBUG_TDMA | ATH_DEBUG_XMIT,
				    "%s: Q%u restarted\n", __func__,
				    txq->axq_qnum);
			}
		}
#else
		ATH_TXQ_INSERT_TAIL(txq, bf, bf_list);
		if (txq->axq_link == NULL) {
			ath_hal_puttxbuf(ah, txq->axq_qnum, bf->bf_daddr);
			DPRINTF(sc, ATH_DEBUG_XMIT,
			    "%s: TXDP[%u] = %p (%p) depth %d\n",
			    __func__, txq->axq_qnum,
			    (caddr_t)bf->bf_daddr, bf->bf_desc,
			    txq->axq_depth);
		} else {
			*txq->axq_link = bf->bf_daddr;
			DPRINTF(sc, ATH_DEBUG_XMIT,
			    "%s: link[%u](%p)=%p (%p) depth %d\n", __func__,
			    txq->axq_qnum, txq->axq_link,
			    (caddr_t)bf->bf_daddr, bf->bf_desc, txq->axq_depth);
		}
#endif /* IEEE80211_SUPPORT_TDMA */
		txq->axq_link = &bf->bf_desc[bf->bf_nseg - 1].ds_link;
		ath_hal_txstart(ah, txq->axq_qnum);
	}
}

/*
 * Hand off a packet to the hardware (or mcast queue.)
 * The relevant hardware txq should be locked.
 */
static void
ath_tx_handoff(struct ath_softc *sc, struct ath_txq *txq, struct ath_buf *bf)
{
	ATH_TXQ_LOCK_ASSERT(txq);

	if (txq->axq_qnum == ATH_TXQ_SWQ)
		ath_tx_handoff_mcast(sc, txq, bf);
	else
		ath_tx_handoff_hw(sc, txq, bf);
}

static int
ath_tx_tag_crypto(struct ath_softc *sc, struct ieee80211_node *ni,
    struct mbuf *m0, int iswep, int isfrag, int *hdrlen, int *pktlen, int *keyix)
{
	if (iswep) {
		const struct ieee80211_cipher *cip;
		struct ieee80211_key *k;

		/*
		 * Construct the 802.11 header+trailer for an encrypted
		 * frame. The only reason this can fail is because of an
		 * unknown or unsupported cipher/key type.
		 */
		k = ieee80211_crypto_encap(ni, m0);
		if (k == NULL) {
			/*
			 * This can happen when the key is yanked after the
			 * frame was queued.  Just discard the frame; the
			 * 802.11 layer counts failures and provides
			 * debugging/diagnostics.
			 */
			return 0;
		}
		/*
		 * Adjust the packet + header lengths for the crypto
		 * additions and calculate the h/w key index.  When
		 * a s/w mic is done the frame will have had any mic
		 * added to it prior to entry so m0->m_pkthdr.len will
		 * account for it. Otherwise we need to add it to the
		 * packet length.
		 */
		cip = k->wk_cipher;
		(*hdrlen) += cip->ic_header;
		(*pktlen) += cip->ic_header + cip->ic_trailer;
		/* NB: frags always have any TKIP MIC done in s/w */
		if ((k->wk_flags & IEEE80211_KEY_SWMIC) == 0 && !isfrag)
			(*pktlen) += cip->ic_miclen;
		(*keyix) = k->wk_keyix;
	} else if (ni->ni_ucastkey.wk_cipher == &ieee80211_cipher_none) {
		/*
		 * Use station key cache slot, if assigned.
		 */
		(*keyix) = ni->ni_ucastkey.wk_keyix;
		if ((*keyix) == IEEE80211_KEYIX_NONE)
			(*keyix) = HAL_TXKEYIX_INVALID;
	} else
		(*keyix) = HAL_TXKEYIX_INVALID;

	return 1;
}

static uint8_t
ath_tx_get_rtscts_rate(struct ath_hal *ah, const HAL_RATE_TABLE *rt,
    int rix, int cix, int shortPreamble)
{
	uint8_t ctsrate;

	/*
	 * CTS transmit rate is derived from the transmit rate
	 * by looking in the h/w rate table.  We must also factor
	 * in whether or not a short preamble is to be used.
	 */
	/* NB: cix is set above where RTS/CTS is enabled */
	KASSERT(cix != 0xff, ("cix not setup"));
	ctsrate = rt->info[cix].rateCode;

	/* XXX this should only matter for legacy rates */
	if (shortPreamble)
		ctsrate |= rt->info[cix].shortPreamble;

	return ctsrate;
}


/*
 * Calculate the RTS/CTS duration for legacy frames.
 */
static int
ath_tx_calc_ctsduration(struct ath_hal *ah, int rix, int cix,
    int shortPreamble, int pktlen, const HAL_RATE_TABLE *rt,
    int flags)
{
	int ctsduration = 0;

	/* This mustn't be called for HT modes */
	if (rt->info[cix].phy == IEEE80211_T_HT) {
		printf("%s: HT rate where it shouldn't be (0x%x)\n",
		    __func__, rt->info[cix].rateCode);
		return -1;
	}

	/*
	 * Compute the transmit duration based on the frame
	 * size and the size of an ACK frame.  We call into the
	 * HAL to do the computation since it depends on the
	 * characteristics of the actual PHY being used.
	 *
	 * NB: CTS is assumed the same size as an ACK so we can
	 *     use the precalculated ACK durations.
	 */
	if (shortPreamble) {
		if (flags & HAL_TXDESC_RTSENA)		/* SIFS + CTS */
			ctsduration += rt->info[cix].spAckDuration;
		ctsduration += ath_hal_computetxtime(ah,
			rt, pktlen, rix, AH_TRUE);
		if ((flags & HAL_TXDESC_NOACK) == 0)	/* SIFS + ACK */
			ctsduration += rt->info[rix].spAckDuration;
	} else {
		if (flags & HAL_TXDESC_RTSENA)		/* SIFS + CTS */
			ctsduration += rt->info[cix].lpAckDuration;
		ctsduration += ath_hal_computetxtime(ah,
			rt, pktlen, rix, AH_FALSE);
		if ((flags & HAL_TXDESC_NOACK) == 0)	/* SIFS + ACK */
			ctsduration += rt->info[rix].lpAckDuration;
	}

	return ctsduration;
}

static int
ath_tx_normal_setup(struct ath_softc *sc, struct ieee80211_node *ni,
    struct ath_buf *bf, struct mbuf *m0)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ath_vap *avp = ATH_VAP(vap);
	struct ath_hal *ah = sc->sc_ah;
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	const struct chanAccParams *cap = &ic->ic_wme.wme_chanParams;
	int error, iswep, ismcast, isfrag, ismrr;
	int keyix, hdrlen, pktlen, try0;
	u_int8_t rix, txrate, ctsrate;
	u_int8_t cix = 0xff;		/* NB: silence compiler */
	struct ath_desc *ds;
	struct ath_txq *txq;
	struct ieee80211_frame *wh;
	u_int subtype, flags, ctsduration;
	HAL_PKT_TYPE atype;
	const HAL_RATE_TABLE *rt;
	HAL_BOOL shortPreamble;
	struct ath_node *an;
	u_int pri;
	uint8_t try[4], rate[4];

	bzero(try, sizeof(try));
	bzero(rate, sizeof(rate));

	wh = mtod(m0, struct ieee80211_frame *);
	iswep = wh->i_fc[1] & IEEE80211_FC1_WEP;
	ismcast = IEEE80211_IS_MULTICAST(wh->i_addr1);
	isfrag = m0->m_flags & M_FRAG;
	hdrlen = ieee80211_anyhdrsize(wh);
	/*
	 * Packet length must not include any
	 * pad bytes; deduct them here.
	 */
	pktlen = m0->m_pkthdr.len - (hdrlen & 3);

	/* Handle encryption twiddling if needed */
	if (! ath_tx_tag_crypto(sc, ni, m0, iswep, isfrag, &hdrlen,
	    &pktlen, &keyix)) {
		ath_freetx(m0);
		return EIO;
	}

	/* packet header may have moved, reset our local pointer */
	wh = mtod(m0, struct ieee80211_frame *);

	pktlen += IEEE80211_CRC_LEN;

	/*
	 * Load the DMA map so any coalescing is done.  This
	 * also calculates the number of descriptors we need.
	 */
	error = ath_tx_dmasetup(sc, bf, m0);
	if (error != 0)
		return error;
	bf->bf_node = ni;			/* NB: held reference */
	m0 = bf->bf_m;				/* NB: may have changed */
	wh = mtod(m0, struct ieee80211_frame *);

	/* setup descriptors */
	ds = bf->bf_desc;
	rt = sc->sc_currates;
	KASSERT(rt != NULL, ("no rate table, mode %u", sc->sc_curmode));

	/*
	 * NB: the 802.11 layer marks whether or not we should
	 * use short preamble based on the current mode and
	 * negotiated parameters.
	 */
	if ((ic->ic_flags & IEEE80211_F_SHPREAMBLE) &&
	    (ni->ni_capinfo & IEEE80211_CAPINFO_SHORT_PREAMBLE)) {
		shortPreamble = AH_TRUE;
		sc->sc_stats.ast_tx_shortpre++;
	} else {
		shortPreamble = AH_FALSE;
	}

	an = ATH_NODE(ni);
	flags = HAL_TXDESC_CLRDMASK;		/* XXX needed for crypto errs */
	ismrr = 0;				/* default no multi-rate retry*/
	pri = M_WME_GETAC(m0);			/* honor classification */
	/* XXX use txparams instead of fixed values */
	/*
	 * Calculate Atheros packet type from IEEE80211 packet header,
	 * setup for rate calculations, and select h/w transmit queue.
	 */
	switch (wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) {
	case IEEE80211_FC0_TYPE_MGT:
		subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;
		if (subtype == IEEE80211_FC0_SUBTYPE_BEACON)
			atype = HAL_PKT_TYPE_BEACON;
		else if (subtype == IEEE80211_FC0_SUBTYPE_PROBE_RESP)
			atype = HAL_PKT_TYPE_PROBE_RESP;
		else if (subtype == IEEE80211_FC0_SUBTYPE_ATIM)
			atype = HAL_PKT_TYPE_ATIM;
		else
			atype = HAL_PKT_TYPE_NORMAL;	/* XXX */
		rix = an->an_mgmtrix;
		txrate = rt->info[rix].rateCode;
		if (shortPreamble)
			txrate |= rt->info[rix].shortPreamble;
		try0 = ATH_TXMGTTRY;
		flags |= HAL_TXDESC_INTREQ;	/* force interrupt */
		break;
	case IEEE80211_FC0_TYPE_CTL:
		atype = HAL_PKT_TYPE_PSPOLL;	/* stop setting of duration */
		rix = an->an_mgmtrix;
		txrate = rt->info[rix].rateCode;
		if (shortPreamble)
			txrate |= rt->info[rix].shortPreamble;
		try0 = ATH_TXMGTTRY;
		flags |= HAL_TXDESC_INTREQ;	/* force interrupt */
		break;
	case IEEE80211_FC0_TYPE_DATA:
		atype = HAL_PKT_TYPE_NORMAL;		/* default */
		/*
		 * Data frames: multicast frames go out at a fixed rate,
		 * EAPOL frames use the mgmt frame rate; otherwise consult
		 * the rate control module for the rate to use.
		 */
		if (ismcast) {
			rix = an->an_mcastrix;
			txrate = rt->info[rix].rateCode;
			if (shortPreamble)
				txrate |= rt->info[rix].shortPreamble;
			try0 = 1;
		} else if (m0->m_flags & M_EAPOL) {
			/* XXX? maybe always use long preamble? */
			rix = an->an_mgmtrix;
			txrate = rt->info[rix].rateCode;
			if (shortPreamble)
				txrate |= rt->info[rix].shortPreamble;
			try0 = ATH_TXMAXTRY;	/* XXX?too many? */
		} else {
			ATH_NODE_LOCK(an);
			ath_rate_findrate(sc, an, shortPreamble, pktlen,
				&rix, &try0, &txrate);
			ATH_NODE_UNLOCK(an);
			sc->sc_txrix = rix;		/* for LED blinking */
			sc->sc_lastdatarix = rix;	/* for fast frames */
			if (try0 != ATH_TXMAXTRY)
				ismrr = 1;
		}
		if (cap->cap_wmeParams[pri].wmep_noackPolicy)
			flags |= HAL_TXDESC_NOACK;
		break;
	default:
		if_printf(ifp, "bogus frame type 0x%x (%s)\n",
			wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK, __func__);
		/* XXX statistic */
		ath_freetx(m0);
		return EIO;
	}
	txq = sc->sc_ac2q[pri];

	/*
	 * When servicing one or more stations in power-save mode
	 * (or) if there is some mcast data waiting on the mcast
	 * queue (to prevent out of order delivery) multicast
	 * frames must be buffered until after the beacon.
	 */
	if (ismcast && (vap->iv_ps_sta || avp->av_mcastq.axq_depth))
		txq = &avp->av_mcastq;

	/*
	 * Calculate miscellaneous flags.
	 */
	if (ismcast) {
		flags |= HAL_TXDESC_NOACK;	/* no ack on broad/multicast */
	} else if (pktlen > vap->iv_rtsthreshold &&
	    (ni->ni_ath_flags & IEEE80211_NODE_FF) == 0) {
		flags |= HAL_TXDESC_RTSENA;	/* RTS based on frame length */
		cix = rt->info[rix].controlRate;
		sc->sc_stats.ast_tx_rts++;
	}
	if (flags & HAL_TXDESC_NOACK)		/* NB: avoid double counting */
		sc->sc_stats.ast_tx_noack++;
#ifdef IEEE80211_SUPPORT_TDMA
	if (sc->sc_tdma && (flags & HAL_TXDESC_NOACK) == 0) {
		DPRINTF(sc, ATH_DEBUG_TDMA,
		    "%s: discard frame, ACK required w/ TDMA\n", __func__);
		sc->sc_stats.ast_tdma_ack++;
		ath_freetx(m0);
		return EIO;
	}
#endif

	/*
	 * If 802.11g protection is enabled, determine whether
	 * to use RTS/CTS or just CTS.  Note that this is only
	 * done for OFDM unicast frames.
	 */
	if ((ic->ic_flags & IEEE80211_F_USEPROT) &&
	    rt->info[rix].phy == IEEE80211_T_OFDM &&
	    (flags & HAL_TXDESC_NOACK) == 0) {
		/* XXX fragments must use CCK rates w/ protection */
		if (ic->ic_protmode == IEEE80211_PROT_RTSCTS)
			flags |= HAL_TXDESC_RTSENA;
		else if (ic->ic_protmode == IEEE80211_PROT_CTSONLY)
			flags |= HAL_TXDESC_CTSENA;
		if (isfrag) {
			/*
			 * For frags it would be desirable to use the
			 * highest CCK rate for RTS/CTS.  But stations
			 * farther away may detect it at a lower CCK rate
			 * so use the configured protection rate instead
			 * (for now).
			 */
			cix = rt->info[sc->sc_protrix].controlRate;
		} else
			cix = rt->info[sc->sc_protrix].controlRate;
		sc->sc_stats.ast_tx_protect++;
	}

#if 0
	/*
	 * If 11n protection is enabled and it's a HT frame,
	 * enable RTS.
	 *
	 * XXX ic_htprotmode or ic_curhtprotmode?
	 * XXX should it_htprotmode only matter if ic_curhtprotmode 
	 * XXX indicates it's not a HT pure environment?
	 */
	if ((ic->ic_htprotmode == IEEE80211_PROT_RTSCTS) &&
	    rt->info[rix].phy == IEEE80211_T_HT &&
	    (flags & HAL_TXDESC_NOACK) == 0) {
		cix = rt->info[sc->sc_protrix].controlRate;
	    	flags |= HAL_TXDESC_RTSENA;
		sc->sc_stats.ast_tx_htprotect++;
	}
#endif

	/*
	 * Calculate duration.  This logically belongs in the 802.11
	 * layer but it lacks sufficient information to calculate it.
	 */
	if ((flags & HAL_TXDESC_NOACK) == 0 &&
	    (wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) != IEEE80211_FC0_TYPE_CTL) {
		u_int16_t dur;
		if (shortPreamble)
			dur = rt->info[rix].spAckDuration;
		else
			dur = rt->info[rix].lpAckDuration;
		if (wh->i_fc[1] & IEEE80211_FC1_MORE_FRAG) {
			dur += dur;		/* additional SIFS+ACK */
			KASSERT(m0->m_nextpkt != NULL, ("no fragment"));
			/*
			 * Include the size of next fragment so NAV is
			 * updated properly.  The last fragment uses only
			 * the ACK duration
			 */
			dur += ath_hal_computetxtime(ah, rt,
					m0->m_nextpkt->m_pkthdr.len,
					rix, shortPreamble);
		}
		if (isfrag) {
			/*
			 * Force hardware to use computed duration for next
			 * fragment by disabling multi-rate retry which updates
			 * duration based on the multi-rate duration table.
			 */
			ismrr = 0;
			try0 = ATH_TXMGTTRY;	/* XXX? */
		}
		*(u_int16_t *)wh->i_dur = htole16(dur);
	}

	/*
	 * Calculate RTS/CTS rate and duration if needed.
	 */
	ctsduration = 0;
	if (flags & (HAL_TXDESC_RTSENA|HAL_TXDESC_CTSENA)) {
		ctsrate = ath_tx_get_rtscts_rate(ah, rt, rix, cix, shortPreamble);

		/* The 11n chipsets do ctsduration calculations for you */
		if (! ath_tx_is_11n(sc))
			ctsduration = ath_tx_calc_ctsduration(ah, rix, cix, shortPreamble,
			    pktlen, rt, flags);
		/*
		 * Must disable multi-rate retry when using RTS/CTS.
		 */
		ismrr = 0;
		try0 = ATH_TXMGTTRY;		/* XXX */
	} else
		ctsrate = 0;


	/*
	 * Determine if a tx interrupt should be generated for
	 * this descriptor.  We take a tx interrupt to reap
	 * descriptors when the h/w hits an EOL condition or
	 * when the descriptor is specifically marked to generate
	 * an interrupt.  We periodically mark descriptors in this
	 * way to insure timely replenishing of the supply needed
	 * for sending frames.  Defering interrupts reduces system
	 * load and potentially allows more concurrent work to be
	 * done but if done to aggressively can cause senders to
	 * backup.
	 *
	 * NB: use >= to deal with sc_txintrperiod changing
	 *     dynamically through sysctl.
	 */
	if (flags & HAL_TXDESC_INTREQ) {
		txq->axq_intrcnt = 0;
	} else if (++txq->axq_intrcnt >= sc->sc_txintrperiod) {
		flags |= HAL_TXDESC_INTREQ;
		txq->axq_intrcnt = 0;
	}

	/* This point forward is actual TX bits */


	/*
	 * At this point we are committed to sending the frame
	 * and we don't need to look at m_nextpkt; clear it in
	 * case this frame is part of frag chain.
	 */
	m0->m_nextpkt = NULL;

	if (IFF_DUMPPKTS(sc, ATH_DEBUG_XMIT))
		ieee80211_dump_pkt(ic, mtod(m0, const uint8_t *), m0->m_len,
		    sc->sc_hwmap[rix].ieeerate, -1);

	if (ieee80211_radiotap_active_vap(vap)) {
		u_int64_t tsf = ath_hal_gettsf64(ah);

		sc->sc_tx_th.wt_tsf = htole64(tsf);
		sc->sc_tx_th.wt_flags = sc->sc_hwmap[rix].txflags;
		if (iswep)
			sc->sc_tx_th.wt_flags |= IEEE80211_RADIOTAP_F_WEP;
		if (isfrag)
			sc->sc_tx_th.wt_flags |= IEEE80211_RADIOTAP_F_FRAG;
		sc->sc_tx_th.wt_rate = sc->sc_hwmap[rix].ieeerate;
		sc->sc_tx_th.wt_txpower = ni->ni_txpower;
		sc->sc_tx_th.wt_antenna = sc->sc_txantenna;

		ieee80211_radiotap_tx(vap, m0);
	}


	if (ath_tx_is_11n(sc)) {
		rate[0] = rix;
		try[0] = try0;
	}

	/*
	 * Formulate first tx descriptor with tx controls.
	 */
	/* XXX check return value? */
	/* XXX is this ok to call for 11n descriptors? */
	/* XXX or should it go through the first, next, last 11n calls? */
	ath_hal_setuptxdesc(ah, ds
		, pktlen		/* packet length */
		, hdrlen		/* header length */
		, atype			/* Atheros packet type */
		, ni->ni_txpower	/* txpower */
		, txrate, try0		/* series 0 rate/tries */
		, keyix			/* key cache index */
		, sc->sc_txantenna	/* antenna mode */
		, flags			/* flags */
		, ctsrate		/* rts/cts rate */
		, ctsduration		/* rts/cts duration */
	);
	bf->bf_txflags = flags;
	/*
	 * Setup the multi-rate retry state only when we're
	 * going to use it.  This assumes ath_hal_setuptxdesc
	 * initializes the descriptors (so we don't have to)
	 * when the hardware supports multi-rate retry and
	 * we don't use it.
	 */
        if (ismrr) {
		ATH_NODE_LOCK(an);
                if (ath_tx_is_11n(sc))
                        ath_rate_getxtxrates(sc, an, rix, rate, try);
                else
                        ath_rate_setupxtxdesc(sc, an, ds, shortPreamble, rix);
		ATH_NODE_UNLOCK(an);
        }

        if (ath_tx_is_11n(sc)) {
                ath_buf_set_rate(sc, ni, bf, pktlen, flags, ctsrate,
		    (atype == HAL_PKT_TYPE_PSPOLL), rate, try);
        }

	return 0;
}

/*
 * Direct-dispatch the current frame to the hardware.
 */
int
ath_tx_start(struct ath_softc *sc, struct ieee80211_node *ni,
    struct ath_buf *bf, struct mbuf *m0)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ath_vap *avp = ATH_VAP(vap);
	int r;
	u_int pri;
	int tid;
	struct ath_txq *txq;
	int ismcast;
	const struct ieee80211_frame *wh;
	int is_ampdu, is_ampdu_tx, is_ampdu_pending;
	ieee80211_seq seqno;

	/* Determine the target hardware queue! */
	pri = M_WME_GETAC(m0);			/* honor classification */
	tid = WME_AC_TO_TID(pri);
	txq = sc->sc_ac2q[pri];
	wh = mtod(m0, struct ieee80211_frame *);
	ismcast = IEEE80211_IS_MULTICAST(wh->i_addr1);

	/* A-MPDU TX */
	is_ampdu_tx = ath_tx_ampdu_running(sc, ATH_NODE(ni), tid);
	is_ampdu_pending = ath_tx_ampdu_pending(sc, ATH_NODE(ni), tid);
	is_ampdu = is_ampdu_tx | is_ampdu_pending;

	DPRINTF(sc, ATH_DEBUG_SW_TX, "%s: tid=%d, ac=%d, is_ampdu=%d\n", 
	    __func__, tid, pri, is_ampdu);

	/* Multicast frames go onto the software multicast queue */
	if (ismcast)
		txq = &avp->av_mcastq;
 	if ((! is_ampdu) && (vap->iv_ps_sta || avp->av_mcastq.axq_depth))
		txq = &avp->av_mcastq;

	/* Do the generic frame setup */

	/* A-MPDU TX? Manually set sequence number */
	/* Don't do it whilst pending; the net80211 layer still assigns them */
	if (is_ampdu_tx)
		seqno = ath_tx_tid_seqno_assign(sc, ni, bf, m0);

	/*
	 * If needed, the sequence number has been assigned.
	 * Squirrel it away somewhere easy to get to.
	 */
	bf->bf_state.bfs_seqno = M_SEQNO_GET(m0);

#if 0
	/* Is ampdu pending? fetch the seqno and print it out */
	if (is_ampdu_pending)
		device_printf(sc->sc_dev,
		    "%s: tid %d: ampdu pending, seqno %d\n",
		    __func__, tid, M_SEQNO_GET(m0));
#endif

	/* This also sets up the DMA map */
	r = ath_tx_normal_setup(sc, ni, bf, m0);

	if (r != 0)
		return r;

	/* At this point m0 could have changed! */
	//m0 = bf->bf_m;

	/* Fill in the details in the descriptor list */
	ath_tx_chaindesclist(sc, bf);

#if 1
	ATH_TXQ_LOCK(txq);
	/* add to software queue */
	ath_tx_swq(sc, ni, txq, bf, m0);
	/* Kick txq */
	ath_txq_sched(sc, txq);
	ATH_TXQ_UNLOCK(txq);
#else

	/*
	 * For now, since there's no software queue,
	 * direct-dispatch to the hardware.
	 */
	if (ismcast)
		ath_tx_handoff_mcast(sc, txq, bf);
	else
		ath_tx_handoff_hw(sc, txq, bf);
#endif

	return 0;
}

static int
ath_tx_raw_start(struct ath_softc *sc, struct ieee80211_node *ni,
	struct ath_buf *bf, struct mbuf *m0,
	const struct ieee80211_bpf_params *params)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	struct ath_hal *ah = sc->sc_ah;
	struct ieee80211vap *vap = ni->ni_vap;
	int error, ismcast, ismrr;
	int keyix, hdrlen, pktlen, try0, txantenna;
	u_int8_t rix, cix, txrate, ctsrate, rate1, rate2, rate3;
	struct ieee80211_frame *wh;
	u_int flags, ctsduration;
	HAL_PKT_TYPE atype;
	const HAL_RATE_TABLE *rt;
	struct ath_desc *ds;
	u_int pri;
	uint8_t try[4], rate[4];
	int o_tid = -1;
	int do_override;

	bzero(try, sizeof(try));
	bzero(rate, sizeof(rate));

	wh = mtod(m0, struct ieee80211_frame *);
	ismcast = IEEE80211_IS_MULTICAST(wh->i_addr1);
	hdrlen = ieee80211_anyhdrsize(wh);
	/*
	 * Packet length must not include any
	 * pad bytes; deduct them here.
	 */
	/* XXX honor IEEE80211_BPF_DATAPAD */
	pktlen = m0->m_pkthdr.len - (hdrlen & 3) + IEEE80211_CRC_LEN;

	/* Handle encryption twiddling if needed */
	if (! ath_tx_tag_crypto(sc, ni,
	    m0, params->ibp_flags & IEEE80211_BPF_CRYPTO, 0,
	    &hdrlen, &pktlen, &keyix)) {
		ath_freetx(m0);
		return EIO;
	}
	/* packet header may have moved, reset our local pointer */
	wh = mtod(m0, struct ieee80211_frame *);

	error = ath_tx_dmasetup(sc, bf, m0);
	if (error != 0)
		return error;
	m0 = bf->bf_m;				/* NB: may have changed */
	wh = mtod(m0, struct ieee80211_frame *);
	bf->bf_node = ni;			/* NB: held reference */

	flags = HAL_TXDESC_CLRDMASK;		/* XXX needed for crypto errs */
	flags |= HAL_TXDESC_INTREQ;		/* force interrupt */
	if (params->ibp_flags & IEEE80211_BPF_RTS)
		flags |= HAL_TXDESC_RTSENA;
	else if (params->ibp_flags & IEEE80211_BPF_CTS)
		flags |= HAL_TXDESC_CTSENA;
	/* XXX leave ismcast to injector? */
	if ((params->ibp_flags & IEEE80211_BPF_NOACK) || ismcast)
		flags |= HAL_TXDESC_NOACK;

	rt = sc->sc_currates;
	KASSERT(rt != NULL, ("no rate table, mode %u", sc->sc_curmode));
	rix = ath_tx_findrix(sc, params->ibp_rate0);
	txrate = rt->info[rix].rateCode;
	if (params->ibp_flags & IEEE80211_BPF_SHORTPRE)
		txrate |= rt->info[rix].shortPreamble;
	sc->sc_txrix = rix;
	try0 = params->ibp_try0;
	ismrr = (params->ibp_try1 != 0);
	txantenna = params->ibp_pri >> 2;
	if (txantenna == 0)			/* XXX? */
		txantenna = sc->sc_txantenna;

	ctsduration = 0;
	if (flags & (HAL_TXDESC_RTSENA|HAL_TXDESC_CTSENA)) {
		cix = ath_tx_findrix(sc, params->ibp_ctsrate);
		ctsrate = ath_tx_get_rtscts_rate(ah, rt, rix, cix, params->ibp_flags & IEEE80211_BPF_SHORTPRE);
		/* The 11n chipsets do ctsduration calculations for you */
		if (! ath_tx_is_11n(sc))
			ctsduration = ath_tx_calc_ctsduration(ah, rix, cix,
			    params->ibp_flags & IEEE80211_BPF_SHORTPRE, pktlen,
			    rt, flags);
		/*
		 * Must disable multi-rate retry when using RTS/CTS.
		 */
		ismrr = 0;			/* XXX */
	} else
		ctsrate = 0;

	pri = params->ibp_pri & 3;
	/*
	 * NB: we mark all packets as type PSPOLL so the h/w won't
	 * set the sequence number, duration, etc.
	 */
	atype = HAL_PKT_TYPE_PSPOLL;

	if (IFF_DUMPPKTS(sc, ATH_DEBUG_XMIT))
		ieee80211_dump_pkt(ic, mtod(m0, caddr_t), m0->m_len,
		    sc->sc_hwmap[rix].ieeerate, -1);

	if (ieee80211_radiotap_active_vap(vap)) {
		u_int64_t tsf = ath_hal_gettsf64(ah);

		sc->sc_tx_th.wt_tsf = htole64(tsf);
		sc->sc_tx_th.wt_flags = sc->sc_hwmap[rix].txflags;
		if (wh->i_fc[1] & IEEE80211_FC1_WEP)
			sc->sc_tx_th.wt_flags |= IEEE80211_RADIOTAP_F_WEP;
		if (m0->m_flags & M_FRAG)
			sc->sc_tx_th.wt_flags |= IEEE80211_RADIOTAP_F_FRAG;
		sc->sc_tx_th.wt_rate = sc->sc_hwmap[rix].ieeerate;
		sc->sc_tx_th.wt_txpower = ni->ni_txpower;
		sc->sc_tx_th.wt_antenna = sc->sc_txantenna;

		ieee80211_radiotap_tx(vap, m0);
	}

	/*
	 * Formulate first tx descriptor with tx controls.
	 */
	ds = bf->bf_desc;
	/* XXX check return value? */
	ath_hal_setuptxdesc(ah, ds
		, pktlen		/* packet length */
		, hdrlen		/* header length */
		, atype			/* Atheros packet type */
		, params->ibp_power	/* txpower */
		, txrate, try0		/* series 0 rate/tries */
		, keyix			/* key cache index */
		, txantenna		/* antenna mode */
		, flags			/* flags */
		, ctsrate		/* rts/cts rate */
		, ctsduration		/* rts/cts duration */
	);
	bf->bf_txflags = flags;

	if (ath_tx_is_11n(sc)) {
		rate[0] = ath_tx_findrix(sc, params->ibp_rate0);
		try[0] = params->ibp_try0;

		if (ismrr) {
			/* Remember, rate[] is actually an array of rix's -adrian */
			rate[0] = ath_tx_findrix(sc, params->ibp_rate0);
			rate[1] = ath_tx_findrix(sc, params->ibp_rate1);
			rate[2] = ath_tx_findrix(sc, params->ibp_rate2);
			rate[3] = ath_tx_findrix(sc, params->ibp_rate3);

			try[0] = params->ibp_try0;
			try[1] = params->ibp_try1;
			try[2] = params->ibp_try2;
			try[3] = params->ibp_try3;
		}
	} else {
		if (ismrr) {
			rix = ath_tx_findrix(sc, params->ibp_rate1);
			rate1 = rt->info[rix].rateCode;
			if (params->ibp_flags & IEEE80211_BPF_SHORTPRE)
				rate1 |= rt->info[rix].shortPreamble;
			if (params->ibp_try2) {
				rix = ath_tx_findrix(sc, params->ibp_rate2);
				rate2 = rt->info[rix].rateCode;
				if (params->ibp_flags & IEEE80211_BPF_SHORTPRE)
					rate2 |= rt->info[rix].shortPreamble;
			} else
				rate2 = 0;
			if (params->ibp_try3) {
				rix = ath_tx_findrix(sc, params->ibp_rate3);
				rate3 = rt->info[rix].rateCode;
				if (params->ibp_flags & IEEE80211_BPF_SHORTPRE)
					rate3 |= rt->info[rix].shortPreamble;
			} else
				rate3 = 0;
			ath_hal_setupxtxdesc(ah, ds
				, rate1, params->ibp_try1	/* series 1 */
				, rate2, params->ibp_try2	/* series 2 */
				, rate3, params->ibp_try3	/* series 3 */
			);
		}
	}

	if (ath_tx_is_11n(sc)) {
		/*
		 * notice that rix doesn't include any of the "magic" flags txrate
		 * does for communicating "other stuff" to the HAL.
		 */
		ath_buf_set_rate(sc, ni, bf, pktlen, flags, ctsrate,
		    (atype == HAL_PKT_TYPE_PSPOLL), rate, try);
	}

	/* NB: no buffered multicast in power save support */

	/* XXX If it's an ADDBA, override the correct queue */
	do_override = ath_tx_action_frame_override_queue(sc, ni, m0, &o_tid);

	/* Map ADDBA to the correct priority */
	if (do_override) {
#if 0
		device_printf(sc->sc_dev,
		    "%s: overriding tid %d pri %d -> %d\n",
		    __func__, o_tid, pri, TID_TO_WME_AC(o_tid));
#endif
		pri = TID_TO_WME_AC(o_tid);
	}

	/* Fill in the details in the descriptor list */
	ath_tx_chaindesclist(sc, bf);

	/*
	 * If we're overiding the ADDBA destination, dump directly
	 * into the hardware queue, right after any pending
	 * frames to that node are.
	 */

	ATH_TXQ_LOCK(sc->sc_ac2q[pri]);
	if (do_override)
		ath_tx_handoff(sc, sc->sc_ac2q[pri], bf);
	else {
		/* Queue to software queue */
		ath_tx_swq(sc, ni, sc->sc_ac2q[pri], bf, m0);

		/* Kick txq */
		ath_txq_sched(sc, sc->sc_ac2q[pri]);
	}
	ATH_TXQ_UNLOCK(sc->sc_ac2q[pri]);

	return 0;
}

int
ath_raw_xmit(struct ieee80211_node *ni, struct mbuf *m,
	const struct ieee80211_bpf_params *params)
{
	struct ieee80211com *ic = ni->ni_ic;
	struct ifnet *ifp = ic->ic_ifp;
	struct ath_softc *sc = ifp->if_softc;
	struct ath_buf *bf;
	int error;

	if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0 || sc->sc_invalid) {
		DPRINTF(sc, ATH_DEBUG_XMIT, "%s: discard frame, %s", __func__,
		    (ifp->if_drv_flags & IFF_DRV_RUNNING) == 0 ?
			"!running" : "invalid");
		m_freem(m);
		error = ENETDOWN;
		goto bad;
	}
	/*
	 * Grab a TX buffer and associated resources.
	 */
	bf = ath_getbuf(sc);
	if (bf == NULL) {
		sc->sc_stats.ast_tx_nobuf++;
		m_freem(m);
		error = ENOBUFS;
		goto bad;
	}

	if (params == NULL) {
		/*
		 * Legacy path; interpret frame contents to decide
		 * precisely how to send the frame.
		 */
		if (ath_tx_start(sc, ni, bf, m)) {
			error = EIO;		/* XXX */
			goto bad2;
		}
	} else {
		/*
		 * Caller supplied explicit parameters to use in
		 * sending the frame.
		 */
		if (ath_tx_raw_start(sc, ni, bf, m, params)) {
			error = EIO;		/* XXX */
			goto bad2;
		}
	}
	sc->sc_wd_timer = 5;
	ifp->if_opackets++;
	sc->sc_stats.ast_tx_raw++;

	return 0;
bad2:
	ATH_TXBUF_LOCK(sc);
	STAILQ_INSERT_HEAD(&sc->sc_txbuf, bf, bf_list);
	ATH_TXBUF_UNLOCK(sc);
bad:
	ifp->if_oerrors++;
	sc->sc_stats.ast_tx_raw_fail++;
	ieee80211_free_node(ni);
	return error;
}

/* Some helper functions */

/*
 * ADDBA (and potentially others) need to be placed in the same
 * hardware queue as the TID/node it's relating to. This is so
 * it goes out after any pending non-aggregate frames to the
 * same node/TID.
 *
 * If this isn't done, the ADDBA can go out before the frames
 * queued in hardware. Even though these frames have a sequence
 * number -earlier- than the ADDBA can be transmitted (but
 * no frames whose sequence numbers are after the ADDBA should
 * be!) they'll arrive after the ADDBA - and the receiving end
 * will simply drop them as being out of the BAW.
 *
 * The frames can't be appended to the TID software queue - it'll
 * never be sent out. So these frames have to be directly
 * dispatched to the hardware, rather than queued in software.
 * So if this function returns true, the TXQ has to be
 * overridden and it has to be directly dispatched.
 *
 * It's a dirty hack, but someone's gotta do it.
 */

/*
 * XXX doesn't belong here!
 */
static int
ieee80211_is_action(struct ieee80211_frame *wh)
{
	/* Type: Management frame? */
	if ((wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) !=
	    IEEE80211_FC0_TYPE_MGT)
		return 0;

	/* Subtype: Action frame? */
	if ((wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) !=
	    IEEE80211_FC0_SUBTYPE_ACTION)
		return 0;

	return 1;
}

#define	MS(_v, _f)	(((_v) & _f) >> _f##_S)
/*
 * Return an alternate TID for ADDBA request frames.
 *
 * Yes, this likely should be done in the net80211 layer.
 */
static int
ath_tx_action_frame_override_queue(struct ath_softc *sc,
    struct ieee80211_node *ni,
    struct mbuf *m0, int *tid)
{
	struct ieee80211_frame *wh = mtod(m0, struct ieee80211_frame *);
	struct ieee80211_action_ba_addbarequest *ia;
	uint8_t *frm;
	uint16_t baparamset;

	/* Not action frame? Bail */
	if (! ieee80211_is_action(wh))
		return 0;

	/* XXX Not needed for frames we send? */
#if 0
	/* Correct length? */
	if (! ieee80211_parse_action(ni, m))
		return 0;
#endif

	/* Extract out action frame */
	frm = (u_int8_t *)&wh[1];
	ia = (struct ieee80211_action_ba_addbarequest *) frm;

	/* Not ADDBA? Bail */
	if (ia->rq_header.ia_category != IEEE80211_ACTION_CAT_BA)
		return 0;
	if (ia->rq_header.ia_action != IEEE80211_ACTION_BA_ADDBA_REQUEST)
		return 0;

	/* Extract TID, return it */
	baparamset = le16toh(ia->rq_baparamset);
	*tid = (int) MS(baparamset, IEEE80211_BAPS_TID);

	return 1;
}
#undef	MS

/* Per-node software queue operations */

/*
 * Mark the current node/TID as ready to TX.
 *
 * This is done to make it easy for the software scheduler to
 * find which nodes have data to send.
 *
 * The relevant hw txq lock should be held.
 */
static void
ath_tx_node_sched(struct ath_softc *sc, struct ath_node *an, int tid)
{
	struct ath_tid *atid = &an->an_tid[tid];
	int ac = TID_TO_WME_AC(tid);
	struct ath_txq *txq = sc->sc_ac2q[ac];

	ATH_TXQ_LOCK_ASSERT(txq);

	if (atid->sched)
		return;		/* already scheduled */

	atid->sched = 1;

	STAILQ_INSERT_TAIL(&txq->axq_tidq, atid, axq_qelem);
}

/*
 * Mark the current node as no longer needing to be polled for
 * TX packets.
 *
 * The relevant hw txq lock should be held.
 */
static void
ath_tx_node_unsched(struct ath_softc *sc, struct ath_node *an, int tid)
{
	struct ath_tid *atid = &an->an_tid[tid];
	int ac = TID_TO_WME_AC(tid);
	struct ath_txq *txq = sc->sc_ac2q[ac];

	ATH_TXQ_LOCK_ASSERT(txq);

	if (atid->sched == 0)
		return;

	atid->sched = 0;
	STAILQ_REMOVE(&txq->axq_tidq, atid, ath_tid, axq_qelem);
}

/*
 * Assign a sequence number manually to the given frame.
 * 
 * This should only be called for A-MPDU TX frames. 
 */
static ieee80211_seq
ath_tx_tid_seqno_assign(struct ath_softc *sc, struct ieee80211_node *ni,
    struct ath_buf *bf, struct mbuf *m0)
{
	struct ieee80211_frame *wh;
	int tid, pri;
	ieee80211_seq seqno;

	/* TID lookup */
	wh = mtod(m0, struct ieee80211_frame *);
	pri = M_WME_GETAC(m0);			/* honor classification */
	tid = WME_AC_TO_TID(pri);

	/* Does the packet require a sequence number? */
	if (! IEEE80211_QOS_HAS_SEQ(wh))
		return -1;

	/* Manually assign sequence number */
	seqno = ni->ni_txseqs[tid]++;
	*(uint16_t *)&wh->i_seq[0] = htole16(seqno << IEEE80211_SEQ_SEQ_SHIFT);
	M_SEQNO_SET(m0, seqno);

	/* Return so caller can do something with it if needed */
	return seqno;
}


/*
 * Queue the given packet on the relevant software queue.
 *
 * This however doesn't queue the packet to the hardware!
 */
void
ath_tx_swq(struct ath_softc *sc, struct ieee80211_node *ni, struct ath_txq *txq,
    struct ath_buf *bf, struct mbuf *m0)
{
	struct ath_node *an = ATH_NODE(ni);
	struct ieee80211_frame *wh;
	struct ath_tid *atid;
	int pri, tid;

	ATH_TXQ_LOCK_ASSERT(txq);

	/* Fetch the TID - non-QoS frames get assigned to TID 16 */
	wh = mtod(m0, struct ieee80211_frame *);
	pri = M_WME_GETAC(m0);			/* honor classification */
	tid = WME_AC_TO_TID(pri);
	atid = &an->an_tid[tid];

	/* Set local packet state, used to queue packets to hardware */
	bf->bf_state.bfs_tid = tid;
	bf->bf_state.bfs_txq = txq;

	/* Queue frame to the tail of the software queue */
	ATH_TXQ_INSERT_TAIL(atid, bf, bf_list);

	/* Mark the given tid as having packets to dequeue */
	ath_tx_node_sched(sc, an, tid);
}

/*
 * Do the basic frame setup stuff that's required before the frame
 * is added to a software queue.
 *
 * All frames get mostly the same treatment and it's done once.
 * Retransmits fiddle with things like the rate control setup,
 * setting the retransmit bit in the packet; doing relevant DMA/bus
 * syncing and relinking it (back) into the hardware TX queue.
 *
 * Note that this may cause the mbuf to be reallocated, so
 * m0 may not be valid.
 */


/*
 * Configure the per-TID node state.
 *
 * This likely belongs in if_ath_node.c but I can't think of anywhere
 * else to put it just yet.
 *
 * This sets up the SLISTs and the mutex as appropriate.
 */
void
ath_tx_tid_init(struct ath_softc *sc, struct ath_node *an)
{
	int i;
	struct ath_tid *atid;

	for (i = 0; i < IEEE80211_TID_SIZE; i++) {
		atid = &an->an_tid[i];
		STAILQ_INIT(&atid->axq_q);
		atid->tid = i;
		atid->an = an;
	}
}

/*
 * Pause the current TID. This stops packets from being transmitted
 * on it.
 */
static void
ath_tx_tid_pause(struct ath_softc *sc, struct ath_tid *tid)
{
	int ac = TID_TO_WME_AC(tid->tid);
	struct ath_txq *txq = sc->sc_ac2q[ac];

	ATH_TXQ_LOCK_ASSERT(txq);
	tid->paused++;
}

static void
ath_tx_tid_resume(struct ath_softc *sc, struct ath_tid *tid)
{
	int ac = TID_TO_WME_AC(tid->tid);
	struct ath_txq *txq = sc->sc_ac2q[ac];

	ATH_TXQ_LOCK_ASSERT(txq);
	tid->paused--;

	if (tid->paused)
		return;
	if (tid->axq_depth == 0)
		return;

	ath_tx_node_sched(sc, tid->an, tid->tid);
	ath_txq_sched(sc, txq);
}

/*
 * Mark packets currently in the hardware TXQ from this TID
 * as now having no parent software TXQ.
 *
 * XXX not yet needed; there shouldn't be any packets left
 * XXX for this node in any of the hardware queues; the node
 * XXX isn't freed until the last TX packet has been sent.
 */
static void
ath_tx_tid_txq_unmark(struct ath_softc *sc, struct ath_node *an,
    int tid)
{
	/* XXX TODO */
}

/*
 * Free any packets currently pending in the software TX queue.
 *
 * Since net80211 shouldn't free the node until the last packets
 * have been sent, this function should never have to free any
 * packets when a node is freed.
 *
 * It can also be called on an active node during an interface
 * reset or state transition.
 */
static void
ath_tx_tid_free_pkts(struct ath_softc *sc, struct ath_node *an,
    int tid)
{
	struct ath_tid *atid = &an->an_tid[tid];
	struct ath_buf *bf;
	int ac = TID_TO_WME_AC(tid);
	struct ath_txq *txq = sc->sc_ac2q[ac];

	ATH_TXQ_LOCK_ASSERT(txq);

	/* Walk the queue, free frames */
	for (;;) {
		bf = STAILQ_FIRST(&atid->axq_q);
		if (bf == NULL) {
			break;
		}
		ATH_TXQ_REMOVE_HEAD(atid, bf_list);
		ath_tx_freebuf(sc, bf, -1);
	}
}

/*
 * Flush all software queued packets for the given node.
 */
void
ath_tx_node_flush(struct ath_softc *sc, struct ath_node *an)
{
	int tid;

	for (tid = 0; tid < IEEE80211_TID_SIZE; tid++) {
		int ac = TID_TO_WME_AC(tid);
		struct ath_txq *txq = sc->sc_ac2q[ac];

		ATH_TXQ_LOCK(txq);
		/* Remove this tid from the list of active tids */
		ath_tx_node_unsched(sc, an, tid);

		/* Free packets */
		ath_tx_tid_free_pkts(sc, an, tid);
		ATH_TXQ_UNLOCK(txq);
	}

	/*
	 * Don't hold the node lock across free_pkts;
	 * freeing buffers may release the node and
	 * that will acquire the IEEE80211_NODE_LOCK (node table).
	 * That then causes a lock reversal.
	 */
}

/*
 * Free the per-TID node state.
 *
 * This frees any packets currently in the software queue and frees
 * any other TID state.
 */
void
ath_tx_tid_cleanup(struct ath_softc *sc, struct ath_node *an)
{
	int tid;
	struct ath_tid *atid;

	/* Flush all packets currently in the sw queues for this node */
	ath_tx_node_flush(sc, an);

	for (tid = 0; tid < IEEE80211_TID_SIZE; tid++) {
		atid = &an->an_tid[tid];
		/* Mark hw-queued packets as having no parent now */
		ath_tx_tid_txq_unmark(sc, an, tid);
	}
}

/*
 * Handle completion of aggregate frames.
 */
static void
ath_tx_aggr_comp(struct ath_softc *sc, struct ath_buf *bf)
{
	/* Success? Complete */
	ath_tx_default_comp(sc, bf);

	/*
	 * Not success and have retries left?
	 *
	 * Mark as retry, requeue at head of queue
	 */

	/*
	 * Not success and out of retries?
	 *
	 * Need to wait for the hardware TXQ to finish draining,
	 * then once we know what was successfully TXed and what
	 * wasn't, we send a BAR to advance the pointer to that
	 * place.
	 */
}

/*
 * Schedule some packets from the given node/TID to the hardware.
 *
 * This is the aggregate version.
 */
void
ath_tx_tid_hw_queue_aggr(struct ath_softc *sc, struct ath_node *an, int tid)
{
	struct ath_buf *bf;
	struct ath_txq *txq;
	struct ath_tid *atid = &an->an_tid[tid];

	for (;;) {
                bf = STAILQ_FIRST(&atid->axq_q);
		if (bf == NULL) {
			break;
		}

		/* XXX check if seqno is outside of BAW, if so don't queue it */

		/*
		 * XXX If the seqno is out of BAW, then we should pause this TID
		 * XXX until a completion for this TID allows the BAW to be advanced.
		 * XXX Otherwise it's possible that we'll simply keep getting called
		 * XXX for this node/TID until some TX completion has occured
		 * XXX and progress can be made.
		 */
		ATH_TXQ_REMOVE_HEAD(atid, bf_list);

		txq = bf->bf_state.bfs_txq;
		ATH_TXQ_LOCK_ASSERT(txq);

		/* Sanity check! */
		if (tid != bf->bf_state.bfs_tid) {
			device_printf(sc->sc_dev, "%s: bfs_tid %d !="
			    " tid %d\n",
			    __func__, bf->bf_state.bfs_tid, tid);
		}

		/* Set completion handler */
		bf->bf_comp = ath_tx_aggr_comp;

		/* Punt to hardware or software txq */
		ath_tx_handoff(sc, txq, bf);
	}
}

/*
 * Schedule some packets from the given node/TID to the hardware.
 */
void
ath_tx_tid_hw_queue_norm(struct ath_softc *sc, struct ath_node *an, int tid)
{
	struct ath_buf *bf;
	struct ath_txq *txq;
	struct ath_tid *atid = &an->an_tid[tid];

	for (;;) {
                bf = STAILQ_FIRST(&atid->axq_q);
		if (bf == NULL) {
			break;
		}
		ATH_TXQ_REMOVE_HEAD(atid, bf_list);

		txq = bf->bf_state.bfs_txq;
		ATH_TXQ_LOCK_ASSERT(txq);
		/* Sanity check! */
		if (tid != bf->bf_state.bfs_tid) {
			device_printf(sc->sc_dev, "%s: bfs_tid %d !="
			    " tid %d\n",
			    __func__, bf->bf_state.bfs_tid, tid);
		}

		/* Punt to hardware or software txq */
		ath_tx_handoff(sc, txq, bf);
	}
}

/*
 * Schedule some packets to the given hardware queue.
 *
 * This function walks the list of TIDs (ie, ath_node TIDs
 * with queued traffic) and attempts to schedule traffic
 * from them.
 *
 * This must be called with the HW TXQ lock held.
 */
void
ath_txq_sched(struct ath_softc *sc, struct ath_txq *txq)
{
	ATH_TXQ_LOCK_ASSERT(txq);
	struct ath_tid *atid, *next;

	/*
	 * For now, let's not worry about QoS, fair-scheduling
	 * or the like. That's a later problem. Just throw
	 * packets at the hardware.
	 */
	STAILQ_FOREACH_SAFE(atid, &txq->axq_tidq, axq_qelem, next) {
		/*
		 * Suspend paused queues here; they'll be resumed
		 * once the addba completes or times out.
		 */
		if (atid->paused) {
			ath_tx_node_unsched(sc, atid->an, atid->tid);
			continue;
		}
		if (ath_tx_ampdu_running(sc, atid->an, atid->tid))
			ath_tx_tid_hw_queue_aggr(sc, atid->an, atid->tid);
		else
			ath_tx_tid_hw_queue_norm(sc, atid->an, atid->tid);

		/* Empty? Remove */
		if (atid->axq_depth == 0)
			ath_tx_node_unsched(sc, atid->an, atid->tid);
	}
}

/*
 * TX addba handling
 */

/*
 * Return net80211 TID struct pointer, or NULL for none
 */
static struct ieee80211_tx_ampdu *
ath_tx_get_tx_tid(struct ath_node *an, int tid)
{
	struct ieee80211_node *ni = &an->an_node;
	struct ieee80211_tx_ampdu *tap;
	int ac;

	if (tid == IEEE80211_NONQOS_TID)
		return 0;

	ac = TID_TO_WME_AC(tid);

	tap = &ni->ni_tx_ampdu[ac];
	return tap;
}

/*
 * Is AMPDU-TX running?
 */
static int
ath_tx_ampdu_running(struct ath_softc *sc, struct ath_node *an, int tid)
{
	struct ieee80211_tx_ampdu *tap;

	tap = ath_tx_get_tx_tid(an, tid);
	if (tap == NULL)
		return 0;	/* Not valid; default to not running */

	return !! (tap->txa_flags & IEEE80211_AGGR_RUNNING);
}

/*
 * Is AMPDU-TX negotiation pending?
 */
static int
ath_tx_ampdu_pending(struct ath_softc *sc, struct ath_node *an, int tid)
{
	struct ieee80211_tx_ampdu *tap;

	tap = ath_tx_get_tx_tid(an, tid);
	if (tap == NULL)
		return 0;	/* Not valid; default to not pending */

	return !! (tap->txa_flags & IEEE80211_AGGR_XCHGPEND);
}


/*
 * Is AMPDU-TX pending for the given TID?
 */


/*
 * Method to handle sending an ADDBA request.
 *
 * We tap this so the relevant flags can be set to pause the TID
 * whilst waiting for the response.
 *
 * XXX there's no timeout handler we can override?
 */
int
ath_addba_request(struct ieee80211_node *ni, struct ieee80211_tx_ampdu *tap,
    int dialogtoken, int baparamset, int batimeout)
{
	struct ath_softc *sc = ni->ni_ic->ic_ifp->if_softc;
	int tid = WME_AC_TO_TID(tap->txa_ac);
	struct ath_node *an = ATH_NODE(ni);
	struct ath_tid *atid = &an->an_tid[tid];

	ATH_TXQ_LOCK(sc->sc_ac2q[tap->txa_ac]);
	ath_tx_tid_pause(sc, atid);
	ATH_TXQ_UNLOCK(sc->sc_ac2q[tap->txa_ac]);

	return sc->sc_addba_request(ni, tap, dialogtoken, baparamset,
	    batimeout);
}

/*
 * Handle an ADDBA response.
 *
 * We unpause the queue so TX'ing can resume.
 *
 * Any packets TX'ed from this point should be "aggregate" (whether
 * aggregate or not) so the BAW is updated.
 */
int
ath_addba_response(struct ieee80211_node *ni, struct ieee80211_tx_ampdu *tap,
    int dialogtoken, int code, int batimeout)
{
	struct ath_softc *sc = ni->ni_ic->ic_ifp->if_softc;
	int tid = WME_AC_TO_TID(tap->txa_ac);
	struct ath_node *an = ATH_NODE(ni);
	struct ath_tid *atid = &an->an_tid[tid];

	ATH_TXQ_LOCK(sc->sc_ac2q[tap->txa_ac]);
	ath_tx_tid_resume(sc, atid);
	ATH_TXQ_UNLOCK(sc->sc_ac2q[tap->txa_ac]);
	return sc->sc_addba_response(ni, tap, dialogtoken, code, batimeout);
}


/*
 * Stop ADDBA on a queue.
 *
 * In theory, queued ath_bufs should be turned back into non-aggregate
 * buffers. I'm not sure what to do about packets currently queued
 * to the hardware.
 */
void
ath_addba_stop(struct ieee80211_node *ni, struct ieee80211_tx_ampdu *tap)
{
	struct ath_softc *sc = ni->ni_ic->ic_ifp->if_softc;
#if 0
	int tid = WME_AC_TO_TID(tap->txa_ac);
	struct ath_node *an = ATH_NODE(ni);
	struct ath_tid *atid = an->an_tid[tid];
#endif

	sc->sc_addba_stop(ni, tap);
}
