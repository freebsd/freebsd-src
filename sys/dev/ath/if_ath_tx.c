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

/*
 * How many retries to perform in software
 */
#define	SWMAX_RETRIES		10

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

/*
 * Obtain the current TID from the given frame.
 *
 * Non-QoS frames need to go into TID 16 (IEEE80211_NONQOS_TID.)
 * This has implications for which AC/priority the packet is placed
 * in.
 */
static int
ath_tx_gettid(struct ath_softc *sc, const struct mbuf *m0)
{
	const struct ieee80211_frame *wh;
	int pri = M_WME_GETAC(m0);

	wh = mtod(m0, const struct ieee80211_frame *);
	if (! IEEE80211_QOS_HAS_SEQ(wh))
		return IEEE80211_NONQOS_TID;
	else
		return WME_AC_TO_TID(pri);
}

/*
 * Determine what the correct AC queue for the given frame
 * should be.
 *
 * This code assumes that the TIDs map consistently to
 * the underlying hardware (or software) ath_txq.
 * Since the sender may try to set an AC which is
 * arbitrary, non-QoS TIDs may end up being put on
 * completely different ACs. There's no way to put a
 * TID into multiple ath_txq's for scheduling, so
 * for now we override the AC/TXQ selection and set
 * non-QOS TID frames into the BE queue.
 *
 * This may be completely incorrect - specifically,
 * some management frames may end up out of order
 * compared to the QoS traffic they're controlling.
 * I'll look into this later.
 */
static int
ath_tx_getac(struct ath_softc *sc, const struct mbuf *m0)
{
	const struct ieee80211_frame *wh;
	int pri = M_WME_GETAC(m0);
	wh = mtod(m0, const struct ieee80211_frame *);
	if (IEEE80211_QOS_HAS_SEQ(wh))
		return pri;

	return WME_AC_BE;
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
		bf->bf_lastds = ds;
	}
}

static void
ath_tx_handoff_mcast(struct ath_softc *sc, struct ath_txq *txq,
    struct ath_buf *bf)
{
	ATH_TXQ_LOCK_ASSERT(txq);
	KASSERT((bf->bf_flags & ATH_BUF_BUSY) == 0,
	     ("%s: busy status 0x%x", __func__, bf->bf_flags));
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
	     ("%s: busy status 0x%x", __func__, bf->bf_flags));
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
 *
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


/*
 * Setup the descriptor chain for a normal or fast-frame
 * frame.
 */
static void
ath_tx_setds(struct ath_softc *sc, struct ath_buf *bf)
{
	struct ath_desc *ds = bf->bf_desc;
	struct ath_hal *ah = sc->sc_ah;

	ath_hal_setuptxdesc(ah, ds
		, bf->bf_state.bfs_pktlen	/* packet length */
		, bf->bf_state.bfs_hdrlen	/* header length */
		, bf->bf_state.bfs_atype	/* Atheros packet type */
		, bf->bf_state.bfs_txpower	/* txpower */
		, bf->bf_state.bfs_txrate0
		, bf->bf_state.bfs_try0		/* series 0 rate/tries */
		, bf->bf_state.bfs_keyix	/* key cache index */
		, bf->bf_state.bfs_txantenna	/* antenna mode */
		, bf->bf_state.bfs_flags	/* flags */
		, bf->bf_state.bfs_ctsrate	/* rts/cts rate */
		, bf->bf_state.bfs_ctsduration	/* rts/cts duration */
	);
	bf->bf_lastds = ds;

	/* XXX TODO: Setup descriptor chain */
}

/*
 * Set the rate control fields in the given descriptor based on
 * the bf_state fields and node state.
 *
 * The bfs fields should already be set with the relevant rate
 * control information, including whether MRR is to be enabled.
 *
 * Since the FreeBSD HAL currently sets up the first TX rate
 * in ath_hal_setuptxdesc(), this will setup the MRR
 * conditionally for the pre-11n chips, and call ath_buf_set_rate
 * unconditionally for 11n chips. These require the 11n rate
 * scenario to be set if MCS rates are enabled, so it's easier
 * to just always call it. The caller can then only set rates 2, 3
 * and 4 if multi-rate retry is needed.
 */
static void
ath_tx_set_ratectrl(struct ath_softc *sc, struct ieee80211_node *ni,
    struct ath_buf *bf)
{
	const HAL_RATE_TABLE *rt = sc->sc_currates;
	struct ath_rc_series *rc = bf->bf_state.bfs_rc;
	uint8_t rate[4];
	int i;

	if (ath_tx_is_11n(sc)) {
		/* Always setup rate series */
		ath_buf_set_rate(sc, ni, bf);
	} else if (bf->bf_state.bfs_ismrr) {
		/* Only call for legacy NICs if MRR */
		for (i = 0; i < 4; i++) {
			rate[i] = rt->info[rc[i].rix].rateCode;
			if (bf->bf_state.bfs_shpream) {
				rate[i] |= rt->info[rc[i].rix].shortPreamble;
			}
		}
		ath_hal_setupxtxdesc(sc->sc_ah, bf->bf_desc
			, rate[1], rc[1].tries
			, rate[2], rc[2].tries
			, rate[3], rc[3].tries
		);
	}
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

	/* Blank the legacy rate array */
	bzero(&bf->bf_state.bfs_rc, sizeof(bf->bf_state.bfs_rc));

	/*
	 * ath_buf_set_rate needs at least one rate/try to setup
	 * the rate scenario.
	 */
	bf->bf_state.bfs_rc[0].rix = rix;
	bf->bf_state.bfs_rc[0].tries = try0;

	/* Store the decided rate index values away */
	bf->bf_state.bfs_pktlen = pktlen;
	bf->bf_state.bfs_hdrlen = hdrlen;
	bf->bf_state.bfs_atype = atype;
	bf->bf_state.bfs_txpower = ni->ni_txpower;
	bf->bf_state.bfs_txrate0 = txrate;
	bf->bf_state.bfs_try0 = try0;
	bf->bf_state.bfs_keyix = keyix;
	bf->bf_state.bfs_txantenna = sc->sc_txantenna;
	bf->bf_state.bfs_flags = flags;
	bf->bf_txflags = flags;
	bf->bf_state.bfs_shpream = shortPreamble;

	/* XXX this should be done in ath_tx_setrate() */
	bf->bf_state.bfs_ctsrate = ctsrate;
	bf->bf_state.bfs_ctsduration = ctsduration;
	bf->bf_state.bfs_ismrr = ismrr;

	/*
	 * Setup the multi-rate retry state only when we're
	 * going to use it.  This assumes ath_hal_setuptxdesc
	 * initializes the descriptors (so we don't have to)
	 * when the hardware supports multi-rate retry and
	 * we don't use it.
	 */
        if (ismrr) {
		ATH_NODE_LOCK(an);
		ath_rate_getxtxrates(sc, an, rix, bf->bf_state.bfs_rc);
		ATH_NODE_UNLOCK(an);
        }

	return 0;
}

/*
 * Direct-dispatch the current frame to the hardware.
 *
 * This can be called by the net80211 code.
 *
 * XXX what about locking? Or, push the seqno assign into the
 * XXX aggregate scheduler so its serialised?
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
	uint8_t type, subtype;

	/*
	 * Determine the target hardware queue.
	 *
	 * For multicast frames, the txq gets overridden to be the
	 * software TXQ and it's done via direct-dispatch.
	 *
	 * For any other frame, we do a TID/QoS lookup inside the frame
	 * to see what the TID should be. If it's a non-QoS frame, the
	 * AC and TID are overridden. The TID/TXQ code assumes the
	 * TID is on a predictable hardware TXQ, so we don't support
	 * having a node TID queued to multiple hardware TXQs.
	 * This may change in the future but would require some locking
	 * fudgery.
	 */
	pri = ath_tx_getac(sc, m0);
	tid = ath_tx_gettid(sc, m0);

	txq = sc->sc_ac2q[pri];
	wh = mtod(m0, struct ieee80211_frame *);
	ismcast = IEEE80211_IS_MULTICAST(wh->i_addr1);
	type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
	subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;

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
	/* XXX should just bzero the bf_state? */
	bf->bf_state.bfs_dobaw = 0;

	/* A-MPDU TX? Manually set sequence number */
	/* Don't do it whilst pending; the net80211 layer still assigns them */
	if (is_ampdu_tx) {
		/*
		 * Always call; this function will
		 * handle making sure that null data frames
		 * don't get a sequence number from the current
		 * TID and thus mess with the BAW.
		 */
		seqno = ath_tx_tid_seqno_assign(sc, ni, bf, m0);
		if (IEEE80211_QOS_HAS_SEQ(wh) &&
		    subtype != IEEE80211_FC0_SUBTYPE_QOS_NULL) {
			bf->bf_state.bfs_dobaw = 1;
		}
	}

	/*
	 * If needed, the sequence number has been assigned.
	 * Squirrel it away somewhere easy to get to.
	 */
	bf->bf_state.bfs_seqno = M_SEQNO_GET(m0) << IEEE80211_SEQ_SEQ_SHIFT;

	/* Is ampdu pending? fetch the seqno and print it out */
	if (is_ampdu_pending)
		DPRINTF(sc, ATH_DEBUG_SW_TX,
		    "%s: tid %d: ampdu pending, seqno %d\n",
		    __func__, tid, M_SEQNO_GET(m0));

	/* This also sets up the DMA map */
	r = ath_tx_normal_setup(sc, ni, bf, m0);

	if (r != 0)
		return r;

	/* At this point m0 could have changed! */
	m0 = bf->bf_m;

#if 1
	/*
	 * If it's a multicast frame, do a direct-dispatch to the
	 * destination hardware queue. Don't bother software
	 * queuing it.
	 */
	/*
	 * If it's a BAR frame, do a direct dispatch to the
	 * destination hardware queue. Don't bother software
	 * queuing it, as the TID will now be paused.
	 */
	if (ismcast) {
		/* Setup the descriptor before handoff */
		ath_tx_setds(sc, bf);
		ath_tx_set_ratectrl(sc, ni, bf);
		ath_tx_chaindesclist(sc, bf);

		ATH_TXQ_LOCK(txq);
		ath_tx_handoff(sc, txq, bf);
		ATH_TXQ_UNLOCK(txq);
	} else if (type == IEEE80211_FC0_TYPE_CTL &&
		    subtype == IEEE80211_FC0_SUBTYPE_BAR) {
		/*
		 * XXX The following is dirty but needed for now.
		 *
		 * Sending a BAR frame can occur from the net80211 txa timer
		 * (ie, retries) or from the ath txtask (completion call.)
		 * It queues directly to hardware because the TID is paused
		 * at this point (and won't be unpaused until the BAR has
		 * either been TXed successfully or max retries has been
		 * reached.)
		 *
		 * TODO: sending a BAR should be done at the management rate!
		 */
		DPRINTF(sc, ATH_DEBUG_SW_TX_CTRL,
		    "%s: BAR: TX'ing direct\n", __func__);

		/* Setup the descriptor before handoff */
		ath_tx_setds(sc, bf);
		ath_tx_set_ratectrl(sc, ni, bf);
		ath_tx_chaindesclist(sc, bf);

		ATH_TXQ_LOCK(txq);
		ath_tx_handoff(sc, txq, bf);
		ATH_TXQ_UNLOCK(txq);
	} else {
		/* add to software queue */
		ath_tx_swq(sc, ni, txq, bf);

		/* Schedule a TX scheduler task call to occur */
		ath_tx_sched_proc_sched(sc);
	}
#else
	/*
	 * For now, since there's no software queue,
	 * direct-dispatch to the hardware.
	 */

	/* Setup the descriptor before handoff */
	ath_tx_setds(sc, bf);
	ath_tx_set_ratectrl(sc, ni, bf);
	ath_tx_chaindesclist(sc, bf);

	ATH_TXQ_LOCK(txq);
	ath_tx_handoff(sc, txq, bf);
	ATH_TXQ_UNLOCK(txq);
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
	u_int8_t rix, cix, txrate, ctsrate;
	struct ieee80211_frame *wh;
	u_int flags, ctsduration;
	HAL_PKT_TYPE atype;
	const HAL_RATE_TABLE *rt;
	struct ath_desc *ds;
	u_int pri;
	int o_tid = -1;
	int do_override;

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

	/* Do the generic frame setup */
	/* XXX should just bzero the bf_state? */
	bf->bf_state.bfs_dobaw = 0;

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
	/* Override pri if the frame isn't a QoS one */
	if (! IEEE80211_QOS_HAS_SEQ(wh))
		pri = ath_tx_getac(sc, m0);

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

	/* Store the decided rate index values away */
	bf->bf_state.bfs_pktlen = pktlen;
	bf->bf_state.bfs_hdrlen = hdrlen;
	bf->bf_state.bfs_atype = atype;
	bf->bf_state.bfs_txpower = params->ibp_power;
	bf->bf_state.bfs_txrate0 = txrate;
	bf->bf_state.bfs_try0 = try0;
	bf->bf_state.bfs_keyix = keyix;
	bf->bf_state.bfs_txantenna = txantenna;
	bf->bf_state.bfs_flags = flags;
	bf->bf_txflags = flags;
	bf->bf_state.bfs_shpream =
	    !! (params->ibp_flags & IEEE80211_BPF_SHORTPRE);

	/* XXX this should be done in ath_tx_setrate() */
	bf->bf_state.bfs_ctsrate = ctsrate;
	bf->bf_state.bfs_ctsduration = ctsduration;
	bf->bf_state.bfs_ismrr = ismrr;

	/* Blank the legacy rate array */
	bzero(&bf->bf_state.bfs_rc, sizeof(bf->bf_state.bfs_rc));

	bf->bf_state.bfs_rc[0].rix =
	    ath_tx_findrix(sc, params->ibp_rate0);
	bf->bf_state.bfs_rc[0].tries = try0;

	if (ismrr) {
		bf->bf_state.bfs_rc[1].rix =
		    ath_tx_findrix(sc, params->ibp_rate1);
		bf->bf_state.bfs_rc[2].rix =
		    ath_tx_findrix(sc, params->ibp_rate2);
		bf->bf_state.bfs_rc[3].rix =
		    ath_tx_findrix(sc, params->ibp_rate3);

		bf->bf_state.bfs_rc[1].tries = params->ibp_try1;
		bf->bf_state.bfs_rc[2].tries = params->ibp_try2;
		bf->bf_state.bfs_rc[3].tries = params->ibp_try3;
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

	/*
	 * If we're overiding the ADDBA destination, dump directly
	 * into the hardware queue, right after any pending
	 * frames to that node are.
	 */

	if (do_override) {
		ATH_TXQ_LOCK(sc->sc_ac2q[pri]);
		ath_tx_setds(sc, bf);
		ath_tx_set_ratectrl(sc, ni, bf);
		ath_tx_chaindesclist(sc, bf);
		ath_tx_handoff(sc, sc->sc_ac2q[pri], bf);
		ATH_TXQ_UNLOCK(sc->sc_ac2q[pri]);
	}
	else {
		/* Queue to software queue */
		ath_tx_swq(sc, ni, sc->sc_ac2q[pri], bf);
	}

	return 0;
}

/*
 * Send a raw frame.
 *
 * This can be called by net80211.
 */
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

	/*
	 * This kicks off a TX packet scheduler task,
	 * pushing packet scheduling out of this thread
	 * and into a separate context. This will (hopefully)
	 * simplify locking/contention in the long run.
	 */
	ath_tx_sched_proc_sched(sc);

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
 * Add the current packet to the given BAW.
 * It is assumed that the current packet
 *
 * + fits inside the BAW;
 * + already has had a sequence number allocated.
 */
void
ath_tx_addto_baw(struct ath_softc *sc, struct ath_node *an,
    struct ath_tid *tid, struct ath_buf *bf)
{
	int index, cindex;
	struct ieee80211_tx_ampdu *tap;

	if (bf->bf_state.bfs_isretried)
		return;

	tap = ath_tx_get_tx_tid(an, tid->tid);
	DPRINTF(sc, ATH_DEBUG_SW_TX_BAW, "%s: tid=%d, seqno %d; window %d:%d\n",
		    __func__, tid->tid, SEQNO(bf->bf_state.bfs_seqno),
		    tap->txa_start, tap->txa_wnd);

	/*
	 * ni->ni_txseqs[] is the currently allocated seqno.
	 * the txa state contains the current baw start.
	 */
	DPRINTF(sc, ATH_DEBUG_SW_TX_BAW, "%s: tap->txa_start: %d, seqno: %d\n",
	    __func__, tap->txa_start, SEQNO(bf->bf_state.bfs_seqno));
	index  = ATH_BA_INDEX(tap->txa_start, SEQNO(bf->bf_state.bfs_seqno));
	cindex = (tid->baw_head + index) & (ATH_TID_MAX_BUFS - 1);
	DPRINTF(sc, ATH_DEBUG_SW_TX_BAW,
	    "%s: index=%d, cindex=%d, baw head=%d, tail=%d\n",
	    __func__, index, cindex, tid->baw_head, tid->baw_tail);

#if 0
	assert(tid->tx_buf[cindex] == NULL);
#endif
	if (tid->tx_buf[cindex] != NULL) {
		device_printf(sc->sc_dev,
		    "%s: ba packet dup (index=%d, cindex=%d, "
		    "head=%d, tail=%d)\n",
		    __func__, index, cindex, tid->baw_head, tid->baw_tail);
	}
	tid->tx_buf[cindex] = bf;

	if (index >= ((tid->baw_tail - tid->baw_head) & (ATH_TID_MAX_BUFS - 1))) {
		tid->baw_tail = cindex;
		INCR(tid->baw_tail, ATH_TID_MAX_BUFS);
	}
}

/*
 * seq_start - left edge of BAW
 * seq_next - current/next sequence number to allocate
 */
static void
ath_tx_update_baw(struct ath_softc *sc, struct ath_node *an,
    struct ath_tid *tid, int seqno)
{
	int index, cindex;
	struct ieee80211_tx_ampdu *tap;

	tap = ath_tx_get_tx_tid(an, tid->tid);
	DPRINTF(sc, ATH_DEBUG_SW_TX, "%s: tid=%d, baw=%d:%d, seqno=%d\n",
	    __func__, tid->tid, tap->txa_start, tap->txa_wnd, seqno);

	index  = ATH_BA_INDEX(tap->txa_start, seqno);
	cindex = (tid->baw_head + index) & (ATH_TID_MAX_BUFS - 1);

	DPRINTF(sc, ATH_DEBUG_SW_TX_BAW,
	    "%s: index=%d, cindex=%d, baw head=%d, tail=%d\n",
	    __func__, index, cindex, tid->baw_head, tid->baw_tail);

	tid->tx_buf[cindex] = NULL;

	while (tid->baw_head != tid->baw_tail && !tid->tx_buf[tid->baw_head]) {
		INCR(tap->txa_start, IEEE80211_SEQ_RANGE);
		INCR(tid->baw_head, ATH_TID_MAX_BUFS);
	}
}

/*
 * Mark the current node/TID as ready to TX.
 *
 * This is done to make it easy for the software scheduler to
 * find which nodes have data to send.
 *
 * The TXQ lock must be held.
 */
static void
ath_tx_tid_sched(struct ath_softc *sc, struct ath_node *an, int tid)
{
	struct ath_tid *atid = &an->an_tid[tid];
	struct ath_txq *txq = sc->sc_ac2q[atid->ac];

	ATH_TXQ_LOCK_ASSERT(txq);

	if (atid->paused)
		return;		/* paused, can't schedule yet */

	if (atid->sched)
		return;		/* already scheduled */

	atid->sched = 1;

	STAILQ_INSERT_TAIL(&txq->axq_tidq, atid, axq_qelem);
}

/*
 * Mark the current node as no longer needing to be polled for
 * TX packets.
 *
 * The TXQ lock must be held.
 */
static void
ath_tx_tid_unsched(struct ath_softc *sc, struct ath_node *an, int tid)
{
	struct ath_tid *atid = &an->an_tid[tid];
	struct ath_txq *txq = sc->sc_ac2q[atid->ac];

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
	uint8_t subtype;

	/* TID lookup */
	wh = mtod(m0, struct ieee80211_frame *);
	pri = M_WME_GETAC(m0);			/* honor classification */
	tid = WME_AC_TO_TID(pri);
	DPRINTF(sc, ATH_DEBUG_SW_TX, "%s: pri=%d, tid=%d, qos has seq=%d\n",
	    __func__, pri, tid, IEEE80211_QOS_HAS_SEQ(wh));

	/* XXX Is it a control frame? Ignore */

	/* Does the packet require a sequence number? */
	if (! IEEE80211_QOS_HAS_SEQ(wh))
		return -1;

	/*
	 * Is it a QOS NULL Data frame? Give it a sequence number from
	 * the default TID (IEEE80211_NONQOS_TID.)
	 *
	 * The RX path of everything I've looked at doesn't include the NULL
	 * data frame sequence number in the aggregation state updates, so
	 * assigning it a sequence number there will cause a BAW hole on the
	 * RX side.
	 */
	subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;
	if (subtype == IEEE80211_FC0_SUBTYPE_QOS_NULL) {
		seqno = ni->ni_txseqs[IEEE80211_NONQOS_TID];
		INCR(ni->ni_txseqs[IEEE80211_NONQOS_TID], IEEE80211_SEQ_RANGE);
	} else {
		/* Manually assign sequence number */
		seqno = ni->ni_txseqs[tid];
		INCR(ni->ni_txseqs[tid], IEEE80211_SEQ_RANGE);
	}
	*(uint16_t *)&wh->i_seq[0] = htole16(seqno << IEEE80211_SEQ_SEQ_SHIFT);
	M_SEQNO_SET(m0, seqno);

	/* Return so caller can do something with it if needed */
	DPRINTF(sc, ATH_DEBUG_SW_TX, "%s:  -> seqno=%d\n", __func__, seqno);
	return seqno;
}

/*
 * Queue the given packet on the relevant software queue.
 *
 * This however doesn't queue the packet to the hardware!
 */
void
ath_tx_swq(struct ath_softc *sc, struct ieee80211_node *ni, struct ath_txq *txq,
    struct ath_buf *bf)
{
	struct ath_node *an = ATH_NODE(ni);
	struct ieee80211_frame *wh;
	struct ath_tid *atid;
	int pri, tid;
	struct mbuf *m0 = bf->bf_m;

	/* Fetch the TID - non-QoS frames get assigned to TID 16 */
	wh = mtod(m0, struct ieee80211_frame *);
	pri = ath_tx_getac(sc, m0);
	tid = ath_tx_gettid(sc, m0);
	atid = &an->an_tid[tid];

	DPRINTF(sc, ATH_DEBUG_SW_TX, "%s: bf=%p, pri=%d, tid=%d, qos=%d\n",
	    __func__, bf, pri, tid, IEEE80211_QOS_HAS_SEQ(wh));

	/* Set local packet state, used to queue packets to hardware */
	bf->bf_state.bfs_tid = tid;
	bf->bf_state.bfs_txq = txq;
	bf->bf_state.bfs_pri = pri;
	bf->bf_state.bfs_aggr = 0;
	bf->bf_state.bfs_aggrburst = 0;

	/* Queue frame to the tail of the software queue */
	ATH_TXQ_LOCK(atid);
	ATH_TXQ_INSERT_TAIL(atid, bf, bf_list);
	ATH_TXQ_UNLOCK(atid);

	/* Mark the given tid as having packets to dequeue */
	ATH_TXQ_LOCK(txq);
	ath_tx_tid_sched(sc, an, tid);
	ATH_TXQ_UNLOCK(txq);
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
	int i, j;
	struct ath_tid *atid;

	for (i = 0; i < IEEE80211_TID_SIZE; i++) {
		atid = &an->an_tid[i];
		STAILQ_INIT(&atid->axq_q);
		atid->tid = i;
		atid->an = an;
		for (j = 0; j < ATH_TID_MAX_BUFS; j++)
			atid->tx_buf[j] = NULL;
		atid->baw_head = atid->baw_tail = 0;
		atid->paused = 0;
		atid->sched = 0;
		atid->cleanup_inprogress = 0;
		if (i == IEEE80211_NONQOS_TID)
			atid->ac = WME_AC_BE;
		else
			atid->ac = TID_TO_WME_AC(i);
		snprintf(atid->axq_name, 48, "%p %s %d",
		    an, device_get_nameunit(sc->sc_dev), i);
		mtx_init(&atid->axq_lock, atid->axq_name, NULL, MTX_DEF);
	}
}

/*
 * Pause the current TID. This stops packets from being transmitted
 * on it.
 *
 * Since this is also called from upper layers as well as the driver,
 * it will get the TID lock.
 */
static void
ath_tx_tid_pause(struct ath_softc *sc, struct ath_tid *tid)
{
	ATH_TXQ_LOCK(tid);
	tid->paused++;
	DPRINTF(sc, ATH_DEBUG_SW_TX_CTRL, "%s: paused = %d\n",
	    __func__, tid->paused);
	ATH_TXQ_UNLOCK(tid);
}

/*
 * Unpause the current TID, and schedule it if needed.
 *
 * Since this is called from upper layers as well as the driver,
 * it will get the TID lock and the TXQ lock if needed.
 */
static void
ath_tx_tid_resume(struct ath_softc *sc, struct ath_tid *tid)
{
	struct ath_txq *txq = sc->sc_ac2q[tid->ac];

	ATH_TXQ_LOCK(tid);

	tid->paused--;

	DPRINTF(sc, ATH_DEBUG_SW_TX_CTRL, "%s: unpaused = %d\n",
	    __func__, tid->paused);

	if (tid->paused || tid->axq_depth == 0) {
		ATH_TXQ_UNLOCK(tid);
		return;
	}
	ATH_TXQ_UNLOCK(tid);

	ATH_TXQ_LOCK(txq);
	ath_tx_tid_sched(sc, tid->an, tid->tid);
	ATH_TXQ_UNLOCK(txq);

	ath_tx_sched_proc_sched(sc);
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

	/* Walk the queue, free frames */
	for (;;) {
		ATH_TXQ_LOCK(atid);
		bf = STAILQ_FIRST(&atid->axq_q);
		if (bf == NULL) {
			ATH_TXQ_UNLOCK(atid);
			break;
		}

		/*
		 * If the current TID is running AMPDU, update
		 * the BAW.
		 */
		if (ath_tx_ampdu_running(sc, an, tid) &&
		    bf->bf_state.bfs_dobaw)
			ath_tx_update_baw(sc, an, atid,
			    SEQNO(bf->bf_state.bfs_seqno));
		ATH_TXQ_REMOVE_HEAD(atid, bf_list);
		ATH_TXQ_UNLOCK(atid);
		ath_tx_freebuf(sc, bf, -1);
	}
}

/*
 * Flush all software queued packets for the given node.
 *
 * Work around recursive TXQ locking.
 * This occurs when a completion handler frees the last buffer
 * for a node, and the node is thus freed. This causes the node
 * to be cleaned up, which ends up calling ath_tx_node_flush.
 */
void
ath_tx_node_flush(struct ath_softc *sc, struct ath_node *an)
{
	int tid;

	for (tid = 0; tid < IEEE80211_TID_SIZE; tid++) {
		struct ath_tid *atid = &an->an_tid[tid];
		struct ath_txq *txq = sc->sc_ac2q[atid->ac];

		/* Remove this tid from the list of active tids */
		ATH_TXQ_LOCK(txq);
		ath_tx_tid_unsched(sc, an, tid);
		ATH_TXQ_UNLOCK(txq);

		/* Free packets */
		ath_tx_tid_free_pkts(sc, an, tid);
	}
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
		mtx_destroy(&atid->axq_lock);
	}
}

/*
 * Handle completion of non-aggregate session frames.
 */
void
ath_tx_normal_comp(struct ath_softc *sc, struct ath_buf *bf, int fail)
{
	DPRINTF(sc, ATH_DEBUG_SW_TX, "%s: bf=%p: fail=%d\n", __func__,
	    bf, fail);
	ath_tx_default_comp(sc, bf, fail);
}

/*
 * Handle cleanup of aggregate session packets that aren't
 * an A-MPDU.
 */
static void
ath_tx_comp_cleanup_unaggr(struct ath_softc *sc, struct ath_buf *bf)
{
	struct ieee80211_node *ni = bf->bf_node;
	struct ath_node *an = ATH_NODE(ni);
	int tid = bf->bf_state.bfs_tid;
	struct ath_tid *atid = &an->an_tid[tid];

	DPRINTF(sc, ATH_DEBUG_SW_TX_CTRL, "%s: TID %d: incomp=%d\n",
	    __func__, tid, atid->incomp);

	ath_tx_default_comp(sc, bf, 0);

	atid->incomp--;
	if (atid->incomp == 0) {
		DPRINTF(sc, ATH_DEBUG_SW_TX_CTRL,
		    "%s: TID %d: cleaned up! resume!\n",
		    __func__, tid);
		atid->cleanup_inprogress = 0;
		ath_tx_tid_resume(sc, atid);
	}

}

/*
 * Performs transmit side cleanup when TID changes from aggregated to
 * unaggregated.
 *
 * - Discard all retry frames from the s/w queue.
 * - Fix the tx completion function for all buffers in s/w queue.
 * - Count the number of unacked frames, and let transmit completion
 *   handle it later.
 *
 * The caller is responsible for pausing the TID.
 */
static void
ath_tx_cleanup(struct ath_softc *sc, struct ath_node *an, int tid)
{
	struct ath_tid *atid = &an->an_tid[tid];
	struct ieee80211_tx_ampdu *tap;
	struct ath_buf *bf, *bf_next;

	DPRINTF(sc, ATH_DEBUG_SW_TX_CTRL,
	    "%s: TID %d: called\n", __func__, tid);

	/*
	 * Update the frames in the software TX queue:
	 *
	 * + Discard retry frames in the queue
	 * + Fix the completion function to be non-aggregate
	 */
	ATH_TXQ_LOCK(atid);
	bf = STAILQ_FIRST(&atid->axq_q);
	while (bf) {
		if (bf->bf_state.bfs_isretried) {
			bf_next = STAILQ_NEXT(bf, bf_list);
			STAILQ_REMOVE(&atid->axq_q, bf, ath_buf, bf_list);
			atid->axq_depth--;
			if (bf->bf_state.bfs_dobaw)
				ath_tx_update_baw(sc, an, atid,
				    SEQNO(bf->bf_state.bfs_seqno));
			/*
			 * Call the default completion handler with "fail" just
			 * so upper levels are suitably notified about this.
			 */
			ath_tx_default_comp(sc, bf, 1);
			bf = bf_next;
			continue;
		}
		/* Give these the default completion handler */
		bf->bf_comp = ath_tx_normal_comp;
		bf = STAILQ_NEXT(bf, bf_list);
	}
	ATH_TXQ_UNLOCK(atid);

	/* The caller is required to pause the TID */
#if 0
	/* Pause the TID */
	ath_tx_tid_pause(sc, atid);
#endif

	/*
	 * Calculate what hardware-queued frames exist based
	 * on the current BAW size. Ie, what frames have been
	 * added to the TX hardware queue for this TID but
	 * not yet ACKed.
	 */
	tap = ath_tx_get_tx_tid(an, tid);
	while (atid->baw_head != atid->baw_tail) {
		if (atid->tx_buf[atid->baw_head]) {
			atid->incomp++;
			atid->cleanup_inprogress = 1;
			atid->tx_buf[atid->baw_head] = NULL;
		}
		INCR(atid->baw_head, ATH_TID_MAX_BUFS);
		INCR(tap->txa_start, IEEE80211_SEQ_RANGE);
	}

	/*
	 * If cleanup is required, defer TID scheduling
	 * until all the HW queued packets have been
	 * sent.
	 */
	if (! atid->cleanup_inprogress)
		ath_tx_tid_resume(sc, atid);

	if (atid->cleanup_inprogress)
		DPRINTF(sc, ATH_DEBUG_SW_TX_CTRL,
		    "%s: TID %d: cleanup needed: %d packets\n",
		    __func__, tid, atid->incomp);
}

static void
ath_tx_set_retry(struct ath_softc *sc, struct ath_buf *bf)
{
	struct ieee80211_frame *wh;

	sc->sc_stats.ast_tx_swretries++;
	bf->bf_state.bfs_isretried = 1;
	bf->bf_state.bfs_retries ++;
	wh = mtod(bf->bf_m, struct ieee80211_frame *);
	wh->i_fc[1] |= IEEE80211_FC1_RETRY;
}

/*
 * Handle retrying an unaggregate frame in an aggregate
 * session.
 *
 * If too many retries occur, pause the TID, wait for
 * any further retransmits (as there's no reason why
 * non-aggregate frames in an aggregate session are
 * transmitted in-order; they just have to be in-BAW)
 * and then queue a BAR.
 */
static void
ath_tx_aggr_retry_unaggr(struct ath_softc *sc, struct ath_buf *bf)
{
	struct ieee80211_node *ni = bf->bf_node;
	struct ath_node *an = ATH_NODE(ni);
	int tid = bf->bf_state.bfs_tid;
	struct ath_tid *atid = &an->an_tid[tid];
	struct ieee80211_tx_ampdu *tap;

	tap = ath_tx_get_tx_tid(an, tid);

	if (bf->bf_state.bfs_retries >= SWMAX_RETRIES) {
		sc->sc_stats.ast_tx_swretrymax++;

		/* Update BAW anyway */
		if (bf->bf_state.bfs_dobaw)
			ath_tx_update_baw(sc, an, atid,
			    SEQNO(bf->bf_state.bfs_seqno));

		/* Send BAR frame */
		/*
		 * This'll end up going into net80211 and back out
		 * again, via ic->ic_raw_xmit().
		 */
		DPRINTF(sc, ATH_DEBUG_SW_TX_CTRL, "%s: TID %d: send BAR\n",
		    __func__, tid);
		if (ieee80211_send_bar(ni, tap, ni->ni_txseqs[tid]) == 0) {
			/*
			 * Pause the TID if this was successful.
			 * An un-successful BAR TX would never call
			 * the BAR complete / timeout methods.
			 */
			ath_tx_tid_pause(sc, atid);
		} else {
			/* BAR TX failed */
			device_printf(sc->sc_dev,
			    "%s: TID %d: BAR TX failed\n",
			    __func__, tid);
		}

		/* Free buffer, bf is free after this call */
		ath_tx_default_comp(sc, bf, 0);
		return;
	}

	/*
	 * This increments the retry counter as well as
	 * sets the retry flag in the ath_buf and packet
	 * body.
	 */
	ath_tx_set_retry(sc, bf);

	/*
	 * XXX Clear the ATH_BUF_BUSY flag. This is likely incorrect
	 * XXX and must be revisited before this is merged into -HEAD.
	 *
	 * This flag is set in ath_tx_processq() if the HW TXQ has
	 * further frames on it. The hardware may currently be processing
	 * the link field in the descriptor (because for TDMA, the
	 * QCU (TX queue DMA engine) can stop until the next TX slot is
	 * available and a recycled buffer may still contain a descriptor
	 * which the currently-paused QCU still points to.
	 *
	 * Since I'm not worried about TDMA just for now, I'm going to blank
	 * the flag.
	 */
	if (bf->bf_flags & ATH_BUF_BUSY) {
		bf->bf_flags &= ~ ATH_BUF_BUSY;
		DPRINTF(sc, ATH_DEBUG_SW_TX_CTRL,
		    "%s: bf %p: ATH_BUF_BUSY\n", __func__, bf);
	}

	/*
	 * Insert this at the head of the queue, so it's
	 * retried before any current/subsequent frames.
	 */
	ATH_TXQ_LOCK(atid);
	ATH_TXQ_INSERT_HEAD(atid, bf, bf_list);
	ATH_TXQ_UNLOCK(atid);

	ATH_TXQ_LOCK(bf->bf_state.bfs_txq);
	ath_tx_tid_sched(sc, an, atid->tid);
	ATH_TXQ_UNLOCK(bf->bf_state.bfs_txq);
}

/*
 * Common code for aggregate excessive retry/subframe retry.
 * If retrying, queues buffers to bf_q. If not, frees the
 * buffers.
 *
 * XXX should unify this with ath_tx_aggr_retry_unaggr()
 */
static int
ath_tx_retry_subframe(struct ath_softc *sc, struct ath_buf *bf,
    ath_bufhead *bf_q)
{
	struct ieee80211_node *ni = bf->bf_node;
	struct ath_node *an = ATH_NODE(ni);
	int tid = bf->bf_state.bfs_tid;
	struct ath_tid *atid = &an->an_tid[tid];

	ath_hal_clr11n_aggr(sc->sc_ah, bf->bf_desc);
	ath_hal_set11nburstduration(sc->sc_ah, bf->bf_desc, 0);
	/* ath_hal_set11n_virtualmorefrag(sc->sc_ah, bf->bf_desc, 0); */

	if (bf->bf_state.bfs_retries >= SWMAX_RETRIES) {
		ath_tx_update_baw(sc, an, atid, SEQNO(bf->bf_state.bfs_seqno));
		/* XXX subframe completion status? is that valid here? */
		ath_tx_default_comp(sc, bf, 0);
		return 1;
	}

	if (bf->bf_flags & ATH_BUF_BUSY) {
		bf->bf_flags &= ~ ATH_BUF_BUSY;
		DPRINTF(sc, ATH_DEBUG_SW_TX_CTRL,
		    "%s: bf %p: ATH_BUF_BUSY\n", __func__, bf);
	}

	ath_tx_set_retry(sc, bf);

	STAILQ_INSERT_TAIL(bf_q, bf, bf_list);
	return 0;
}

/*
 * error pkt completion for an aggregate destination
 */
static void
ath_tx_comp_aggr_error(struct ath_softc *sc, struct ath_buf *bf_first,
    struct ath_tid *tid)
{
	struct ieee80211_node *ni = bf_first->bf_node;
	struct ath_node *an = ATH_NODE(ni);
	struct ath_buf *bf_next, *bf;
	ath_bufhead bf_q;
	int drops = 0;
	struct ieee80211_tx_ampdu *tap;

	tap = ath_tx_get_tx_tid(an, tid->tid);

	STAILQ_INIT(&bf_q);

	/* Retry all subframes */
	bf = bf_first;
	while (bf) {
		bf_next = bf->bf_next;
		drops += ath_tx_retry_subframe(sc, bf, &bf_q);
		bf = bf_next;
	}

	/* Update rate control module about aggregation */
	/* XXX todo */

	/*
	 * send bar if we dropped any frames
	 */
	if (drops) {
		if (ieee80211_send_bar(ni, tap, ni->ni_txseqs[tid->tid]) == 0) {
			/*
			 * Pause the TID if this was successful.
			 * An un-successful BAR TX would never call
			 * the BAR complete / timeout methods.
			 */
			ath_tx_tid_pause(sc, tid);
		} else {
			/* BAR TX failed */
			device_printf(sc->sc_dev,
			    "%s: TID %d: BAR TX failed\n",
			    __func__, tid->tid);
		}
	}

	/* Prepend all frames to the beginning of the queue */
	ATH_TXQ_LOCK(tid);
	while ((bf = STAILQ_FIRST(&bf_q)) != NULL) {
		ATH_TXQ_INSERT_HEAD(tid, bf, bf_list);
		STAILQ_REMOVE_HEAD(&bf_q, bf_list);
	}
	ATH_TXQ_UNLOCK(tid);
}

/*
 * Handle clean-up of packets from an aggregate list.
 */
static void
ath_tx_comp_cleanup_aggr(struct ath_softc *sc, struct ath_buf *bf_first)
{
	struct ath_buf *bf, *bf_next;
	struct ieee80211_node *ni = bf_first->bf_node;
	struct ath_node *an = ATH_NODE(ni);
	int tid = bf_first->bf_state.bfs_tid;
	struct ath_tid *atid = &an->an_tid[tid];

	bf = bf_first;

	while (bf) {
		atid->incomp--;
		bf_next = bf->bf_next;
		ath_tx_default_comp(sc, bf, -1);
		bf = bf_next;
	}

	if (atid->incomp == 0) {
		DPRINTF(sc, ATH_DEBUG_SW_TX_CTRL,
		    "%s: TID %d: cleaned up! resume!\n",
		    __func__, tid);
		atid->cleanup_inprogress = 0;
		ath_tx_tid_resume(sc, atid);
	}

}

/*
 * Handle completion of an set of aggregate frames.
 *
 * XXX for now, simply complete each sub-frame.
 *
 * Note: the completion handler is the last descriptor in the aggregate,
 * not the last descriptor in the first frame.
 */
static void
ath_tx_aggr_comp_aggr(struct ath_softc *sc, struct ath_buf *bf_first, int fail)
{
	//struct ath_desc *ds = bf->bf_lastds;
	struct ieee80211_node *ni = bf_first->bf_node;
	struct ath_node *an = ATH_NODE(ni);
	int tid = bf_first->bf_state.bfs_tid;
	struct ath_tid *atid = &an->an_tid[tid];
	struct ath_tx_status *ts = &bf_first->bf_status.ds_txstat;
	struct ieee80211_tx_ampdu *tap;
	ath_bufhead bf_q;
	int seq_st, tx_ok;
	int hasba, isaggr;
	uint32_t ba[2];
	struct ath_buf *bf, *bf_next;
	int ba_index;
	int drops = 0;

	/*
	 * Punt cleanup to the relevant function, not our problem now
	 */
	if (atid->cleanup_inprogress) {
		ath_tx_comp_cleanup_aggr(sc, bf_first);
		return;
	}

	/*
	 * handle errors first
	 */
	if (ts->ts_status & HAL_TXERR_XRETRY) {
		ath_tx_comp_aggr_error(sc, bf_first, atid);
		return;
	}

	STAILQ_INIT(&bf_q);
	tap = ath_tx_get_tx_tid(an, tid);

	/*
	 * extract starting sequence and block-ack bitmap
	 */
	/* XXX endian-ness of seq_st, ba? */
	seq_st = ts->ts_seqnum;
	hasba = !! (ts->ts_flags & HAL_TX_BA);
	tx_ok = (ts->ts_status == 0);
	isaggr = bf_first->bf_state.bfs_aggr;
	ba[0] = ts->ts_ba_low;
	ba[1] = ts->ts_ba_high;

	/* Occasionally, the MAC sends a tx status for the wrong TID. */
	if (tid != ts->ts_tid) {
		device_printf(sc->sc_dev, "%s: tid %d != hw tid %d\n",
		    __func__, tid, ts->ts_tid);
		tx_ok = 0;
	}

	/* AR5416 BA bug; this requires an interface reset */
	/* XXX TODO */

	/*
	 * Walk the list of frames, figure out which ones were correctly
	 * sent and which weren't.
	 */
	bf = bf_first;

	while (bf) {
		ba_index = ATH_BA_INDEX(seq_st, SEQNO(bf->bf_state.bfs_seqno));
		bf_next = bf->bf_next;

		if (tx_ok && ATH_BA_ISSET(ba, ba_index)) {
			ath_tx_update_baw(sc, an, atid,
			    SEQNO(bf->bf_state.bfs_seqno));
			ath_tx_default_comp(sc, bf, 0);
		} else {
			drops += ath_tx_retry_subframe(sc, bf, &bf_q);
		}
		bf = bf_next;
	}

	/* update rate control module about aggregate status */
	/* XXX TODO */

	/*
	 * send bar if we dropped any frames
	 */
	if (drops) {
		if (ieee80211_send_bar(ni, tap, ni->ni_txseqs[tid]) == 0) {
			/*
			 * Pause the TID if this was successful.
			 * An un-successful BAR TX would never call
			 * the BAR complete / timeout methods.
			 */
			ath_tx_tid_pause(sc, atid);
		} else {
			/* BAR TX failed */
			device_printf(sc->sc_dev,
			    "%s: TID %d: BAR TX failed\n",
			    __func__, tid);
		}
	}

	/* Prepend all frames to the beginning of the queue */
	ATH_TXQ_LOCK(atid);
	while ((bf = STAILQ_FIRST(&bf_q)) != NULL) {
		ATH_TXQ_INSERT_HEAD(atid, bf, bf_list);
		STAILQ_REMOVE_HEAD(&bf_q, bf_list);
	}
	ATH_TXQ_UNLOCK(atid);
}

/*
 * Handle completion of unaggregated frames in an ADDBA
 * session.
 *
 * Fail is set to 1 if the entry is being freed via a call to
 * ath_tx_draintxq().
 */
static void
ath_tx_aggr_comp_unaggr(struct ath_softc *sc, struct ath_buf *bf, int fail)
{
	struct ieee80211_node *ni = bf->bf_node;
	struct ath_node *an = ATH_NODE(ni);
	int tid = bf->bf_state.bfs_tid;
	struct ath_tid *atid = &an->an_tid[tid];
	struct ath_tx_status *ts = &bf->bf_status.ds_txstat;

	if (tid == IEEE80211_NONQOS_TID)
		device_printf(sc->sc_dev, "%s: TID=16!\n", __func__);
	DPRINTF(sc, ATH_DEBUG_SW_TX, "%s: bf=%p: tid=%d\n",
	    __func__, bf, bf->bf_state.bfs_tid);

	/*
	 * If a cleanup is in progress, punt to comp_cleanup;
	 * rather than handling it here. It's thus their
	 * responsibility to clean up, call the completion
	 * function in net80211, update rate control, etc.
	 */
	if (atid->cleanup_inprogress) {
		ath_tx_comp_cleanup_unaggr(sc, bf);
		return;
	}

	/*
	 * Don't bother with the retry check if all frames
	 * are being failed (eg during queue deletion.)
	 */
	if (fail == 0 && ts->ts_status & HAL_TXERR_XRETRY) {
		ath_tx_aggr_retry_unaggr(sc, bf);
		return;
	}

	/* Success? Complete */
	DPRINTF(sc, ATH_DEBUG_SW_TX, "%s: TID=%d, seqno %d\n",
	    __func__, tid, SEQNO(bf->bf_state.bfs_seqno));
	if (bf->bf_state.bfs_dobaw)
		ath_tx_update_baw(sc, an, atid, SEQNO(bf->bf_state.bfs_seqno));

	ath_tx_default_comp(sc, bf, fail);
	/* bf is freed at this point */
}

void
ath_tx_aggr_comp(struct ath_softc *sc, struct ath_buf *bf, int fail)
{
	if (bf->bf_state.bfs_aggr)
		ath_tx_aggr_comp_aggr(sc, bf, fail);
	else
		ath_tx_aggr_comp_unaggr(sc, bf, fail);
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
	struct ieee80211_tx_ampdu *tap;
	struct ieee80211_node *ni = &an->an_node;

	DPRINTF(sc, ATH_DEBUG_SW_TX, "%s: tid=%d\n", __func__, tid);

	tap = ath_tx_get_tx_tid(an, tid);

	if (tid == IEEE80211_NONQOS_TID)
		device_printf(sc->sc_dev, "%s: called for TID=NONQOS_TID?\n",
		    __func__);

	for (;;) {
		ATH_TXQ_LOCK(atid);

		/*
		 * If the upper layer has paused the TID, don't
		 * queue any further packets.
		 *
		 * This can also occur from the completion task because
		 * of packet loss; but as its serialised with this code,
		 * it won't "appear" half way through queuing packets.
		 */
		if (atid->paused)
			break;

                bf = STAILQ_FIRST(&atid->axq_q);
		if (bf == NULL) {
			ATH_TXQ_UNLOCK(atid);
			break;
		}

		DPRINTF(sc, ATH_DEBUG_SW_TX, "%s: bf=%p: tid=%d\n",
		    __func__, bf, bf->bf_state.bfs_tid);
		if (bf->bf_state.bfs_tid != tid)
			device_printf(sc->sc_dev, "%s: TID: tid=%d, ac=%d, bf tid=%d\n",
			    __func__, tid, atid->ac, bf->bf_state.bfs_tid);
		if (sc->sc_ac2q[TID_TO_WME_AC(tid)] != bf->bf_state.bfs_txq)
			device_printf(sc->sc_dev, "%s: TXQ: tid=%d, ac=%d, bf tid=%d\n",
			    __func__, tid, atid->ac, bf->bf_state.bfs_tid);

		/* Check if seqno is outside of BAW, if so don't queue it */
		if (bf->bf_state.bfs_dobaw &&
		    (! BAW_WITHIN(tap->txa_start, tap->txa_wnd,
		    SEQNO(bf->bf_state.bfs_seqno)))) {
			DPRINTF(sc, ATH_DEBUG_SW_TX_BAW,
			    "%s: seq %d outside of %d/%d; waiting\n",
			    __func__, SEQNO(bf->bf_state.bfs_seqno),
			    tap->txa_start, tap->txa_wnd);
			ATH_TXQ_UNLOCK(atid);
			break;
		}

		/*
		 * XXX If the seqno is out of BAW, then we should pause this TID
		 * XXX until a completion for this TID allows the BAW to be advanced.
		 * XXX Otherwise it's possible that we'll simply keep getting called
		 * XXX for this node/TID until some TX completion has occured
		 * XXX and progress can be made.
		 */

		/* We've committed to sending it, so remove it from the list */
		ATH_TXQ_REMOVE_HEAD(atid, bf_list);
		ATH_TXQ_UNLOCK(atid);

		/* Don't add packets to the BAW that don't contribute to it */
		if (bf->bf_state.bfs_dobaw)
			ath_tx_addto_baw(sc, an, atid, bf);

		txq = bf->bf_state.bfs_txq;

		/* Sanity check! */
		if (tid != bf->bf_state.bfs_tid) {
			device_printf(sc->sc_dev, "%s: bfs_tid %d !="
			    " tid %d\n",
			    __func__, bf->bf_state.bfs_tid, tid);
		}

		/* Set completion handler */
		bf->bf_comp = ath_tx_aggr_comp;
		if (bf->bf_state.bfs_tid == IEEE80211_NONQOS_TID)
			device_printf(sc->sc_dev, "%s: TID=16?\n", __func__);

		/* Program descriptor */
		ath_tx_setds(sc, bf);
		ath_tx_set_ratectrl(sc, ni, bf);
		ath_tx_chaindesclist(sc, bf);

		/* Punt to hardware or software txq */
		ATH_TXQ_LOCK(txq);
		ath_tx_handoff(sc, txq, bf);
		ATH_TXQ_UNLOCK(txq);
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
	struct ieee80211_node *ni = &an->an_node;

	DPRINTF(sc, ATH_DEBUG_SW_TX, "%s: node %p: TID %d: called\n",
	    __func__, an, tid);

	/* Check - is AMPDU pending or running? then print out something */
	if (ath_tx_ampdu_pending(sc, an, tid))
		device_printf(sc->sc_dev, "%s: tid=%d, ampdu pending?\n",
		    __func__, tid);
	if (ath_tx_ampdu_running(sc, an, tid))
		device_printf(sc->sc_dev, "%s: tid=%d, ampdu running?\n",
		    __func__, tid);

	for (;;) {
		ATH_TXQ_LOCK(atid);

		/*
		 * If the upper layers have paused the TID, don't
		 * queue any further packets.
		 */
		if (atid->paused)
			break;

		bf = STAILQ_FIRST(&atid->axq_q);
		if (bf == NULL) {
			ATH_TXQ_UNLOCK(atid);
			break;
		}

		ATH_TXQ_REMOVE_HEAD(atid, bf_list);
		ATH_TXQ_UNLOCK(atid);

		txq = bf->bf_state.bfs_txq;

		/* Sanity check! */
		if (tid != bf->bf_state.bfs_tid) {
			device_printf(sc->sc_dev, "%s: bfs_tid %d !="
			    " tid %d\n",
			    __func__, bf->bf_state.bfs_tid, tid);
		}
		/* Normal completion handler */
		bf->bf_comp = ath_tx_normal_comp;

		/* Program descriptor */
		ath_tx_setds(sc, bf);
		ath_tx_set_ratectrl(sc, ni, bf);
		ath_tx_chaindesclist(sc, bf);

		/* Punt to hardware or software txq */
		ATH_TXQ_LOCK(txq);
		ath_tx_handoff(sc, txq, bf);
		ATH_TXQ_UNLOCK(txq);
	}
}

/*
 * Schedule some packets to the given hardware queue.
 *
 * This function walks the list of TIDs (ie, ath_node TIDs
 * with queued traffic) and attempts to schedule traffic
 * from them.
 */
void
ath_txq_sched(struct ath_softc *sc, struct ath_txq *txq)
{
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
		 *
		 * Since this touches tid->paused, it should lock
		 * the TID lock before checking.
		 */
		DPRINTF(sc, ATH_DEBUG_SW_TX, "%s: tid=%d, paused=%d\n",
		    __func__, atid->tid, atid->paused);
		ATH_TXQ_LOCK(atid);
		if (atid->paused) {
			ATH_TXQ_UNLOCK(atid);
			ATH_TXQ_LOCK(txq);
			ath_tx_tid_unsched(sc, atid->an, atid->tid);
			ATH_TXQ_UNLOCK(txq);
			continue;
		}
		ATH_TXQ_UNLOCK(atid);
		if (ath_tx_ampdu_running(sc, atid->an, atid->tid))
			ath_tx_tid_hw_queue_aggr(sc, atid->an, atid->tid);
		else
			ath_tx_tid_hw_queue_norm(sc, atid->an, atid->tid);

		/* Empty? Remove */
		ATH_TXQ_LOCK(txq);
		if (atid->axq_depth == 0)
			ath_tx_tid_unsched(sc, atid->an, atid->tid);
		ATH_TXQ_UNLOCK(txq);
	}
}

/*
 * TX addba handling
 */

/*
 * Return net80211 TID struct pointer, or NULL for none
 */
struct ieee80211_tx_ampdu *
ath_tx_get_tx_tid(struct ath_node *an, int tid)
{
	struct ieee80211_node *ni = &an->an_node;
	struct ieee80211_tx_ampdu *tap;
	int ac;

	if (tid == IEEE80211_NONQOS_TID)
		return NULL;

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

	if (tid == IEEE80211_NONQOS_TID)
		return 0;

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

	if (tid == IEEE80211_NONQOS_TID)
		return 0;

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

	/*
	 * XXX This isn't enough.
	 *
	 * The taskqueue may be running and scheduling some more packets.
	 * It acquires the TID lock to serialise access to the TID paused
	 * flag but as the rest of the code doesn't hold the TID lock
	 * for the duration of any activity (outside of adding/removing
	 * items from the software queue), it can't possibly guarantee
	 * consistency.
	 *
	 * This pauses future scheduling, but it doesn't interrupt the
	 * current scheduling, nor does it wait for that scheduling to
	 * finish. So the txseq window has moved, and those frames
	 * in the meantime have "normal" completion handlers.
	 *
	 * The addba teardown pause/resume likely has the same problem.
	 */
	ath_tx_tid_pause(sc, atid);

	DPRINTF(sc, ATH_DEBUG_SW_TX_CTRL,
	    "%s: called; dialogtoken=%d, baparamset=%d, batimeout=%d\n",
	    __func__, dialogtoken, baparamset, batimeout);
	DPRINTF(sc, ATH_DEBUG_SW_TX_CTRL,
	    "%s: txa_start=%d, ni_txseqs=%d\n",
	    __func__, tap->txa_start, ni->ni_txseqs[tid]);

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
 *
 * Note! net80211 keeps self-assigning sequence numbers until
 * ampdu is negotiated. This means the initially-negotiated BAW left
 * edge won't match the ni->ni_txseq.
 *
 * So, being very dirty, the BAW left edge is "slid" here to match
 * ni->ni_txseq.
 *
 * What likely SHOULD happen is that all packets subsequent to the
 * addba request should be tagged as aggregate and queued as non-aggregate
 * frames; thus updating the BAW. For now though, I'll just slide the
 * window.
 */
int
ath_addba_response(struct ieee80211_node *ni, struct ieee80211_tx_ampdu *tap,
    int status, int code, int batimeout)
{
	struct ath_softc *sc = ni->ni_ic->ic_ifp->if_softc;
	int tid = WME_AC_TO_TID(tap->txa_ac);
	struct ath_node *an = ATH_NODE(ni);
	struct ath_tid *atid = &an->an_tid[tid];
	int r;

	DPRINTF(sc, ATH_DEBUG_SW_TX_CTRL,
	    "%s: called; status=%d, code=%d, batimeout=%d\n", __func__,
	    status, code, batimeout);

	DPRINTF(sc, ATH_DEBUG_SW_TX_CTRL,
	    "%s: txa_start=%d, ni_txseqs=%d\n",
	    __func__, tap->txa_start, ni->ni_txseqs[tid]);

	/*
	 * Call this first, so the interface flags get updated
	 * before the TID is unpaused. Otherwise a race condition
	 * exists where the unpaused TID still doesn't yet have
	 * IEEE80211_AGGR_RUNNING set.
	 */
	r = sc->sc_addba_response(ni, tap, status, code, batimeout);

	/*
	 * XXX dirty!
	 * Slide the BAW left edge to wherever net80211 left it for us.
	 * Read above for more information.
	 */
	tap->txa_start = ni->ni_txseqs[tid];

	ath_tx_tid_resume(sc, atid);
	return r;
}


/*
 * Stop ADDBA on a queue.
 */
void
ath_addba_stop(struct ieee80211_node *ni, struct ieee80211_tx_ampdu *tap)
{
	struct ath_softc *sc = ni->ni_ic->ic_ifp->if_softc;
	int tid = WME_AC_TO_TID(tap->txa_ac);
	struct ath_node *an = ATH_NODE(ni);
	struct ath_tid *atid = &an->an_tid[tid];

	DPRINTF(sc, ATH_DEBUG_SW_TX_CTRL, "%s: called\n", __func__);

	/* Pause TID traffic early, so there aren't any races */
	ath_tx_tid_pause(sc, atid);

	/* There's no need to hold the TXQ lock here */
	sc->sc_addba_stop(ni, tap);

	ath_tx_cleanup(sc, an, tid);
	/*
	 * ath_tx_cleanup will resume the TID if possible, otherwise
	 * it'll set the cleanup flag, and it'll be unpaused once
	 * things have been cleaned up.
	 */
}

/*
 * Note: net80211 bar_timeout() doesn't call this function on BAR failure;
 * it simply tears down the aggregation session. Ew.
 *
 * It however will call ieee80211_ampdu_stop() which will call
 * ic->ic_addba_stop().
 *
 * XXX This uses a hard-coded max BAR count value; the whole
 * XXX BAR TX success or failure should be better handled!
 */
void
ath_bar_response(struct ieee80211_node *ni, struct ieee80211_tx_ampdu *tap,
    int status)
{
	struct ath_softc *sc = ni->ni_ic->ic_ifp->if_softc;
	int tid = WME_AC_TO_TID(tap->txa_ac);
	struct ath_node *an = ATH_NODE(ni);
	struct ath_tid *atid = &an->an_tid[tid];
	int attempts = tap->txa_attempts;

	DPRINTF(sc, ATH_DEBUG_SW_TX_CTRL,
	    "%s: called; status=%d\n", __func__, status);

	/* Note: This may update the BAW details */
	sc->sc_bar_response(ni, tap, status);

	/* Unpause the TID */
	/*
	 * XXX if this is attempt=50, the TID will be downgraded
	 * XXX to a non-aggregate session. So we must unpause the
	 * XXX TID here or it'll never be done.
	 */
	if (status == 0 || attempts == 50)
		ath_tx_tid_resume(sc, atid);
}
