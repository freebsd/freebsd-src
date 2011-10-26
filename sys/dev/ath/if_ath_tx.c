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

	TAILQ_FOREACH_SAFE(bf, frags, bf_list, next) {
		/* NB: bf assumed clean */
		TAILQ_REMOVE(frags, bf, bf_list);
		TAILQ_INSERT_HEAD(&sc->sc_txbuf, bf, bf_list);
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
		TAILQ_INSERT_TAIL(frags, bf, bf_list);
	}
	ATH_TXBUF_UNLOCK(sc);

	return !TAILQ_EMPTY(frags);
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

/*
 * Chain together segments+descriptors for a non-11n frame.
 */
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

/*
 * Fill in the descriptor list for a aggregate subframe.
 *
 * The subframe is returned with the ds_link field in the last subframe
 * pointing to 0.
 */
static void
ath_tx_chaindesclist_subframe(struct ath_softc *sc, struct ath_buf *bf)
{
	struct ath_hal *ah = sc->sc_ah;
	struct ath_desc *ds, *ds0;
	int i;

	ds0 = ds = bf->bf_desc;

	/*
	 * There's no need to call ath_hal_setupfirsttxdesc here;
	 * That's only going to occur for the first frame in an aggregate.
	 */
	for (i = 0; i < bf->bf_nseg; i++, ds++) {
		ds->ds_data = bf->bf_segs[i].ds_addr;
		if (i == bf->bf_nseg - 1)
			ds->ds_link = 0;
		else
			ds->ds_link = bf->bf_daddr + sizeof(*ds) * (i + 1);

		/*
		 * This performs the setup for an aggregate frame.
		 * This includes enabling the aggregate flags if needed.
		 */
		ath_hal_chaintxdesc(ah, ds,
		    bf->bf_state.bfs_pktlen,
		    bf->bf_state.bfs_hdrlen,
		    HAL_PKT_TYPE_AMPDU,	/* forces aggregate bits to be set */
		    bf->bf_state.bfs_keyix,
		    0,			/* cipher, calculated from keyix */
		    bf->bf_state.bfs_ndelim,
		    bf->bf_segs[i].ds_len,	/* segment length */
		    i == 0,		/* first segment */
		    i == bf->bf_nseg - 1	/* last segment */
		);

		DPRINTF(sc, ATH_DEBUG_XMIT,
			"%s: %d: %08x %08x %08x %08x %08x %08x\n",
			__func__, i, ds->ds_link, ds->ds_data,
			ds->ds_ctl0, ds->ds_ctl1, ds->ds_hw[0], ds->ds_hw[1]);
		bf->bf_lastds = ds;
	}
}

/*
 * Setup segments+descriptors for an 11n aggregate.
 * bf_first is the first buffer in the aggregate.
 * The descriptor list must already been linked together using
 * bf->bf_next.
 */
static void
ath_tx_setds_11n(struct ath_softc *sc, struct ath_buf *bf_first)
{
	struct ath_buf *bf, *bf_prev = NULL;

	DPRINTF(sc, ATH_DEBUG_SW_TX_AGGR, "%s: nframes=%d, al=%d\n",
	    __func__, bf_first->bf_state.bfs_nframes,
	    bf_first->bf_state.bfs_al);

	/*
	 * Setup all descriptors of all subframes.
	 */
	bf = bf_first;
	while (bf != NULL) {
		DPRINTF(sc, ATH_DEBUG_SW_TX_AGGR,
		    "%s: bf=%p, nseg=%d, pktlen=%d, seqno=%d\n",
		    __func__, bf, bf->bf_nseg, bf->bf_state.bfs_pktlen,
		    SEQNO(bf->bf_state.bfs_seqno));

		/* Sub-frame setup */
		ath_tx_chaindesclist_subframe(sc, bf);

		/*
		 * Link the last descriptor of the previous frame
		 * to the beginning descriptor of this frame.
		 */
		if (bf_prev != NULL)
			bf_prev->bf_lastds->ds_link = bf->bf_daddr;

		/* Save a copy so we can link the next descriptor in */
		bf_prev = bf;
		bf = bf->bf_next;
	}

	/*
	 * Setup first descriptor of first frame.
	 * chaintxdesc() overwrites the descriptor entries;
	 * setupfirsttxdesc() merges in things.
	 * Otherwise various fields aren't set correctly (eg flags).
	 */
	ath_hal_setupfirsttxdesc(sc->sc_ah,
	    bf_first->bf_desc,
	    bf_first->bf_state.bfs_al,
	    bf_first->bf_state.bfs_flags | HAL_TXDESC_INTREQ,
	    bf_first->bf_state.bfs_txpower,
	    bf_first->bf_state.bfs_txrate0,
	    bf_first->bf_state.bfs_try0,
	    bf_first->bf_state.bfs_txantenna,
	    bf_first->bf_state.bfs_ctsrate,
	    bf_first->bf_state.bfs_ctsduration);

	/*
	 * Setup the last descriptor in the list.
	 * bf_prev points to the last; bf is NULL here.
	 */
	ath_hal_setuplasttxdesc(sc->sc_ah, bf_prev->bf_desc, bf_first->bf_desc);

	/*
	 * Set the first descriptor bf_lastds field to point to
	 * the last descriptor in the last subframe, that's where
	 * the status update will occur.
	 */
	bf_first->bf_lastds = bf_prev->bf_lastds;

	/*
	 * And bf_last in the first descriptor points to the end of
	 * the aggregate list.
	 */
	bf_first->bf_last = bf_prev;

	DPRINTF(sc, ATH_DEBUG_SW_TX_AGGR, "%s: end\n", __func__);
}

static void
ath_tx_handoff_mcast(struct ath_softc *sc, struct ath_txq *txq,
    struct ath_buf *bf)
{
	ATH_TXQ_LOCK_ASSERT(txq);
	KASSERT((bf->bf_flags & ATH_BUF_BUSY) == 0,
	     ("%s: busy status 0x%x", __func__, bf->bf_flags));
	if (txq->axq_link != NULL) {
		struct ath_buf *last = ATH_TXQ_LAST(txq, axq_q_s);
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
	txq->axq_link = &bf->bf_lastds->ds_link;
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
					TAILQ_FIRST(&txq->axq_q)->bf_daddr);
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
		if (bf->bf_state.bfs_aggr)
			txq->axq_aggr_depth++;
		txq->axq_link = &bf->bf_lastds->ds_link;
		ath_hal_txstart(ah, txq->axq_qnum);
	}
}

/*
 * Restart TX DMA for the given TXQ.
 *
 * This must be called whether the queue is empty or not.
 */
void
ath_txq_restart_dma(struct ath_softc *sc, struct ath_txq *txq)
{
	struct ath_hal *ah = sc->sc_ah;
	struct ath_buf *bf;

	ATH_TXQ_LOCK_ASSERT(txq);

	/* This is always going to be cleared, empty or not */
	txq->axq_flags &= ~ATH_TXQ_PUTPENDING;

	bf = TAILQ_FIRST(&txq->axq_q);
	if (bf == NULL)
		return;

	ath_hal_puttxbuf(ah, txq->axq_qnum, bf->bf_daddr);
	txq->axq_link = &bf->bf_lastds->ds_link;
	ath_hal_txstart(ah, txq->axq_qnum);
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
    int cix, int shortPreamble)
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
 * Update the given ath_buf with updated rts/cts setup and duration
 * values.
 *
 * To support rate lookups for each software retry, the rts/cts rate
 * and cts duration must be re-calculated.
 *
 * This function assumes the RTS/CTS flags have been set as needed;
 * mrr has been disabled; and the rate control lookup has been done.
 *
 * XXX TODO: MRR need only be disabled for the pre-11n NICs.
 * XXX The 11n NICs support per-rate RTS/CTS configuration.
 */
static void
ath_tx_set_rtscts(struct ath_softc *sc, struct ath_buf *bf)
{
	uint16_t ctsduration = 0;
	uint8_t ctsrate = 0;
	uint8_t rix = bf->bf_state.bfs_rc[0].rix;
	uint8_t cix = 0;
	const HAL_RATE_TABLE *rt = sc->sc_currates;

	/*
	 * No RTS/CTS enabled? Don't bother.
	 */
	if ((bf->bf_state.bfs_flags &
	    (HAL_TXDESC_RTSENA | HAL_TXDESC_CTSENA)) == 0) {
		/* XXX is this really needed? */
		bf->bf_state.bfs_ctsrate = 0;
		bf->bf_state.bfs_ctsduration = 0;
		return;
	}

	/*
	 * If protection is enabled, use the protection rix control
	 * rate. Otherwise use the rate0 control rate.
	 */
	if (bf->bf_state.bfs_doprot)
		rix = sc->sc_protrix;
	else
		rix = bf->bf_state.bfs_rc[0].rix;

	/*
	 * If the raw path has hard-coded ctsrate0 to something,
	 * use it.
	 */
	if (bf->bf_state.bfs_ctsrate0 != 0)
		cix = ath_tx_findrix(sc, bf->bf_state.bfs_ctsrate0);
	else
		/* Control rate from above */
		cix = rt->info[rix].controlRate;

	/* Calculate the rtscts rate for the given cix */
	ctsrate = ath_tx_get_rtscts_rate(sc->sc_ah, rt, cix,
	    bf->bf_state.bfs_shpream);

	/* The 11n chipsets do ctsduration calculations for you */
	if (! ath_tx_is_11n(sc))
		ctsduration = ath_tx_calc_ctsduration(sc->sc_ah, rix, cix,
		    bf->bf_state.bfs_shpream, bf->bf_state.bfs_pktlen,
		    rt, bf->bf_state.bfs_flags);

	/* Squirrel away in ath_buf */
	bf->bf_state.bfs_ctsrate = ctsrate;
	bf->bf_state.bfs_ctsduration = ctsduration;
	
	/*
	 * Must disable multi-rate retry when using RTS/CTS.
	 * XXX TODO: only for pre-11n NICs.
	 */
	bf->bf_state.bfs_ismrr = 0;
	bf->bf_state.bfs_try0 =
	    bf->bf_state.bfs_rc[0].tries = ATH_TXMGTTRY;	/* XXX ew */
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

	/*
	 * This will be overriden when the descriptor chain is written.
	 */
	bf->bf_lastds = ds;
	bf->bf_last = bf;

	/* XXX TODO: Setup descriptor chain */
}

/*
 * Do a rate lookup.
 *
 * This performs a rate lookup for the given ath_buf only if it's required.
 * Non-data frames and raw frames don't require it.
 *
 * This populates the primary and MRR entries; MRR values are
 * then disabled later on if something requires it (eg RTS/CTS on
 * pre-11n chipsets.
 *
 * This needs to be done before the RTS/CTS fields are calculated
 * as they may depend upon the rate chosen.
 */
static void
ath_tx_do_ratelookup(struct ath_softc *sc, struct ath_buf *bf)
{
	uint8_t rate, rix;
	int try0;

	if (! bf->bf_state.bfs_doratelookup)
		return;

	/* Get rid of any previous state */
	bzero(bf->bf_state.bfs_rc, sizeof(bf->bf_state.bfs_rc));

	ATH_NODE_LOCK(ATH_NODE(bf->bf_node));
	ath_rate_findrate(sc, ATH_NODE(bf->bf_node), bf->bf_state.bfs_shpream,
	    bf->bf_state.bfs_pktlen, &rix, &try0, &rate);

	/* In case MRR is disabled, make sure rc[0] is setup correctly */
	bf->bf_state.bfs_rc[0].rix = rix;
	bf->bf_state.bfs_rc[0].ratecode = rate;
	bf->bf_state.bfs_rc[0].tries = try0;

	if (bf->bf_state.bfs_ismrr && try0 != ATH_TXMAXTRY)
		ath_rate_getxtxrates(sc, ATH_NODE(bf->bf_node), rix,
		    bf->bf_state.bfs_rc);
	ATH_NODE_UNLOCK(ATH_NODE(bf->bf_node));

	sc->sc_txrix = rix;	/* for LED blinking */
	sc->sc_lastdatarix = rix;	/* for fast frames */
	bf->bf_state.bfs_try0 = try0;
	bf->bf_state.bfs_txrate0 = rate;
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
	struct ath_rc_series *rc = bf->bf_state.bfs_rc;

	/* If mrr is disabled, blank tries 1, 2, 3 */
	if (! bf->bf_state.bfs_ismrr)
		rc[1].tries = rc[2].tries = rc[3].tries = 0;

	/*
	 * Always call - that way a retried descriptor will
	 * have the MRR fields overwritten.
	 *
	 * XXX TODO: see if this is really needed - setting up
	 * the first descriptor should set the MRR fields to 0
	 * for us anyway.
	 */
	if (ath_tx_is_11n(sc)) {
		ath_buf_set_rate(sc, ni, bf);
	} else {
		ath_hal_setupxtxdesc(sc->sc_ah, bf->bf_desc
			, rc[1].ratecode, rc[1].tries
			, rc[2].ratecode, rc[2].tries
			, rc[3].ratecode, rc[3].tries
		);
	}
}

/*
 * Transmit the given frame to the hardware.
 *
 * The frame must already be setup; rate control must already have
 * been done.
 *
 * XXX since the TXQ lock is being held here (and I dislike holding
 * it for this long when not doing software aggregation), later on
 * break this function into "setup_normal" and "xmit_normal". The
 * lock only needs to be held for the ath_tx_handoff call.
 */
static void
ath_tx_xmit_normal(struct ath_softc *sc, struct ath_txq *txq,
    struct ath_buf *bf)
{

	ATH_TXQ_LOCK_ASSERT(txq);

	/* Setup the descriptor before handoff */
	ath_tx_do_ratelookup(sc, bf);
	ath_tx_rate_fill_rcflags(sc, bf);
	ath_tx_set_rtscts(sc, bf);
	ath_tx_setds(sc, bf);
	ath_tx_set_ratectrl(sc, bf->bf_node, bf);
	ath_tx_chaindesclist(sc, bf);

	/* Hand off to hardware */
	ath_tx_handoff(sc, txq, bf);
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
	int keyix, hdrlen, pktlen, try0 = 0;
	u_int8_t rix = 0, txrate = 0;
	struct ath_desc *ds;
	struct ath_txq *txq;
	struct ieee80211_frame *wh;
	u_int subtype, flags;
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
			/*
			 * Do rate lookup on each TX, rather than using
			 * the hard-coded TX information decided here.
			 */
			ismrr = 1;
			bf->bf_state.bfs_doratelookup = 1;
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
		bf->bf_state.bfs_doprot = 1;
		/* XXX fragments must use CCK rates w/ protection */
		if (ic->ic_protmode == IEEE80211_PROT_RTSCTS) {
			flags |= HAL_TXDESC_RTSENA;
		} else if (ic->ic_protmode == IEEE80211_PROT_CTSONLY) {
			flags |= HAL_TXDESC_CTSENA;
		}
		/*
		 * For frags it would be desirable to use the
		 * highest CCK rate for RTS/CTS.  But stations
		 * farther away may detect it at a lower CCK rate
		 * so use the configured protection rate instead
		 * (for now).
		 */
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
	bf->bf_state.bfs_rc[0].ratecode = txrate;

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
	bf->bf_state.bfs_ctsrate0 = 0;	/* ie, no hard-coded ctsrate */
	bf->bf_state.bfs_ctsrate = 0;	/* calculated later */
	bf->bf_state.bfs_ctsduration = 0;
	bf->bf_state.bfs_ismrr = ismrr;

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
	/* XXX do we need locking here? */
	if (is_ampdu_tx) {
		ATH_TXQ_LOCK(txq);
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
		ATH_TXQ_UNLOCK(txq);
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
	 * Sending a BAR frame can occur from the net80211 txa timer
	 * (ie, retries) or from the ath txtask (completion call.)
	 * It queues directly to hardware because the TID is paused
	 * at this point (and won't be unpaused until the BAR has
	 * either been TXed successfully or max retries has been
	 * reached.)
	 */
	if (txq == &avp->av_mcastq) {
		ATH_TXQ_LOCK(txq);
		ath_tx_xmit_normal(sc, txq, bf);
		ATH_TXQ_UNLOCK(txq);
	} else if (type == IEEE80211_FC0_TYPE_CTL &&
		    subtype == IEEE80211_FC0_SUBTYPE_BAR) {
		DPRINTF(sc, ATH_DEBUG_SW_TX_CTRL,
		    "%s: BAR: TX'ing direct\n", __func__);
		ATH_TXQ_LOCK(txq);
		ath_tx_xmit_normal(sc, txq, bf);
		ATH_TXQ_UNLOCK(txq);
	} else {
		/* add to software queue */
		ath_tx_swq(sc, ni, txq, bf);
	}
#else
	/*
	 * For now, since there's no software queue,
	 * direct-dispatch to the hardware.
	 */
	ATH_TXQ_LOCK(txq);
	ath_tx_xmit_normal(sc, txq, bf);
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
	u_int8_t rix, txrate;
	struct ieee80211_frame *wh;
	u_int flags;
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


	DPRINTF(sc, ATH_DEBUG_SW_TX, "%s: ismcast=%d\n",
	    __func__, ismcast);

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
	else if (params->ibp_flags & IEEE80211_BPF_CTS) {
		/* XXX assume 11g/11n protection? */
		bf->bf_state.bfs_doprot = 1;
		flags |= HAL_TXDESC_CTSENA;
	}
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

	/*
	 * Since ctsrate is fixed, store it away for later
	 * use when the descriptor fields are being set.
	 */
	if (flags & (HAL_TXDESC_RTSENA|HAL_TXDESC_CTSENA))
		bf->bf_state.bfs_ctsrate0 = params->ibp_ctsrate;

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
	bf->bf_state.bfs_ctsrate = 0;
	bf->bf_state.bfs_ctsduration = 0;
	bf->bf_state.bfs_ismrr = ismrr;

	/* Blank the legacy rate array */
	bzero(&bf->bf_state.bfs_rc, sizeof(bf->bf_state.bfs_rc));

	bf->bf_state.bfs_rc[0].rix =
	    ath_tx_findrix(sc, params->ibp_rate0);
	bf->bf_state.bfs_rc[0].tries = try0;
	bf->bf_state.bfs_rc[0].ratecode = txrate;

	if (ismrr) {
		int rix;

		rix = ath_tx_findrix(sc, params->ibp_rate1);
		bf->bf_state.bfs_rc[1].rix = rix;
		bf->bf_state.bfs_rc[1].tries = params->ibp_try1;

		rix = ath_tx_findrix(sc, params->ibp_rate2);
		bf->bf_state.bfs_rc[2].rix = rix;
		bf->bf_state.bfs_rc[2].tries = params->ibp_try2;

		rix = ath_tx_findrix(sc, params->ibp_rate3);
		bf->bf_state.bfs_rc[3].rix = rix;
		bf->bf_state.bfs_rc[3].tries = params->ibp_try3;
	}
	/*
	 * All the required rate control decisions have been made;
	 * fill in the rc flags.
	 */
	ath_tx_rate_fill_rcflags(sc, bf);

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
	DPRINTF(sc, ATH_DEBUG_SW_TX, "%s: dooverride=%d\n",
	    __func__, do_override);

	if (do_override) {
		ATH_TXQ_LOCK(sc->sc_ac2q[pri]);
		ath_tx_xmit_normal(sc, sc->sc_ac2q[pri], bf);
		ATH_TXQ_UNLOCK(sc->sc_ac2q[pri]);
	} else {
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

	return 0;
bad2:
	ATH_TXBUF_LOCK(sc);
	TAILQ_INSERT_HEAD(&sc->sc_txbuf, bf, bf_list);
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
 *
 * Since the BAW status may be modified by both the ath task and
 * the net80211/ifnet contexts, the TID must be locked.
 */
void
ath_tx_addto_baw(struct ath_softc *sc, struct ath_node *an,
    struct ath_tid *tid, struct ath_buf *bf)
{
	int index, cindex;
	struct ieee80211_tx_ampdu *tap;

	ATH_TXQ_LOCK_ASSERT(sc->sc_ac2q[tid->ac]);

	if (bf->bf_state.bfs_isretried)
		return;

	tap = ath_tx_get_tx_tid(an, tid->tid);

	if (bf->bf_state.bfs_addedbaw)
		device_printf(sc->sc_dev,
		    "%s: re-added? tid=%d, seqno %d; window %d:%d; baw head=%d tail=%d\n",
		    __func__, tid->tid, SEQNO(bf->bf_state.bfs_seqno),
		    tap->txa_start, tap->txa_wnd, tid->baw_head, tid->baw_tail);

	/*
	 * ni->ni_txseqs[] is the currently allocated seqno.
	 * the txa state contains the current baw start.
	 */
	index  = ATH_BA_INDEX(tap->txa_start, SEQNO(bf->bf_state.bfs_seqno));
	cindex = (tid->baw_head + index) & (ATH_TID_MAX_BUFS - 1);
	DPRINTF(sc, ATH_DEBUG_SW_TX_BAW,
	    "%s: tid=%d, seqno %d; window %d:%d; index=%d cindex=%d baw head=%d tail=%d\n",
	    __func__, tid->tid, SEQNO(bf->bf_state.bfs_seqno),
	    tap->txa_start, tap->txa_wnd, index, cindex, tid->baw_head, tid->baw_tail);


#if 0
	assert(tid->tx_buf[cindex] == NULL);
#endif
	if (tid->tx_buf[cindex] != NULL) {
		device_printf(sc->sc_dev,
		    "%s: ba packet dup (index=%d, cindex=%d, "
		    "head=%d, tail=%d)\n",
		    __func__, index, cindex, tid->baw_head, tid->baw_tail);
		device_printf(sc->sc_dev,
		    "%s: BA bf: %p; seqno=%d ; new bf: %p; seqno=%d\n",
		    __func__,
		    tid->tx_buf[cindex],
		    SEQNO(tid->tx_buf[cindex]->bf_state.bfs_seqno),
		    bf,
		    SEQNO(bf->bf_state.bfs_seqno)
		);
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
 *
 * Since the BAW status may be modified by both the ath task and
 * the net80211/ifnet contexts, the TID must be locked.
 */
static void
ath_tx_update_baw(struct ath_softc *sc, struct ath_node *an,
    struct ath_tid *tid, const struct ath_buf *bf)
{
	int index, cindex;
	struct ieee80211_tx_ampdu *tap;
	int seqno = SEQNO(bf->bf_state.bfs_seqno);

	ATH_TXQ_LOCK_ASSERT(sc->sc_ac2q[tid->ac]);

	tap = ath_tx_get_tx_tid(an, tid->tid);
	index  = ATH_BA_INDEX(tap->txa_start, seqno);
	cindex = (tid->baw_head + index) & (ATH_TID_MAX_BUFS - 1);

	DPRINTF(sc, ATH_DEBUG_SW_TX_BAW,
	    "%s: tid=%d, baw=%d:%d, seqno=%d, index=%d, cindex=%d, baw head=%d, tail=%d\n",
	    __func__, tid->tid, tap->txa_start, tap->txa_wnd, seqno, index,
	    cindex, tid->baw_head, tid->baw_tail);

	/*
	 * If this occurs then we have a big problem - something else
	 * has slid tap->txa_start along without updating the BAW
	 * tracking start/end pointers. Thus the TX BAW state is now
	 * completely busted.
	 *
	 * But for now, since I haven't yet fixed TDMA and buffer cloning,
	 * it's quite possible that a cloned buffer is making its way
	 * here and causing it to fire off. Disable TDMA for now.
	 */
	if (tid->tx_buf[cindex] != bf) {
		device_printf(sc->sc_dev,
		    "%s: comp bf=%p, seq=%d; slot bf=%p, seqno=%d\n",
		    __func__,
		    bf, SEQNO(bf->bf_state.bfs_seqno),
		    tid->tx_buf[cindex],
		    SEQNO(tid->tx_buf[cindex]->bf_state.bfs_seqno));
	}

	tid->tx_buf[cindex] = NULL;

	while (tid->baw_head != tid->baw_tail && !tid->tx_buf[tid->baw_head]) {
		INCR(tap->txa_start, IEEE80211_SEQ_RANGE);
		INCR(tid->baw_head, ATH_TID_MAX_BUFS);
	}
	DPRINTF(sc, ATH_DEBUG_SW_TX_BAW, "%s: baw is now %d:%d, baw head=%d\n",
	    __func__, tap->txa_start, tap->txa_wnd, tid->baw_head);
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
ath_tx_tid_sched(struct ath_softc *sc, struct ath_tid *tid)
{
	struct ath_txq *txq = sc->sc_ac2q[tid->ac];

	ATH_TXQ_LOCK_ASSERT(txq);

	if (tid->paused)
		return;		/* paused, can't schedule yet */

	if (tid->sched)
		return;		/* already scheduled */

	tid->sched = 1;

	TAILQ_INSERT_TAIL(&txq->axq_tidq, tid, axq_qelem);
}

/*
 * Mark the current node as no longer needing to be polled for
 * TX packets.
 *
 * The TXQ lock must be held.
 */
static void
ath_tx_tid_unsched(struct ath_softc *sc, struct ath_tid *tid)
{
	struct ath_txq *txq = sc->sc_ac2q[tid->ac];

	ATH_TXQ_LOCK_ASSERT(txq);

	if (tid->sched == 0)
		return;

	tid->sched = 0;
	TAILQ_REMOVE(&txq->axq_tidq, tid, axq_qelem);
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
 * Attempt to direct dispatch an aggregate frame to hardware.
 * If the frame is out of BAW, queue.
 * Otherwise, schedule it as a single frame.
 */
static void
ath_tx_xmit_aggr(struct ath_softc *sc, struct ath_node *an, struct ath_buf *bf)
{
	struct ath_tid *tid = &an->an_tid[bf->bf_state.bfs_tid];
	struct ath_txq *txq = bf->bf_state.bfs_txq;
	struct ieee80211_tx_ampdu *tap;

	ATH_TXQ_LOCK_ASSERT(txq);

	tap = ath_tx_get_tx_tid(an, tid->tid);

	/* paused? queue */
	if (tid->paused) {
		ATH_TXQ_INSERT_TAIL(tid, bf, bf_list);
		return;
	}

	/* outside baw? queue */
	if (bf->bf_state.bfs_dobaw &&
	    (! BAW_WITHIN(tap->txa_start, tap->txa_wnd,
	    SEQNO(bf->bf_state.bfs_seqno)))) {
		ATH_TXQ_INSERT_TAIL(tid, bf, bf_list);
		ath_tx_tid_sched(sc, tid);
		return;
	}

	/* Direct dispatch to hardware */
	ath_tx_do_ratelookup(sc, bf);
	ath_tx_rate_fill_rcflags(sc, bf);
	ath_tx_set_rtscts(sc, bf);
	ath_tx_setds(sc, bf);
	ath_tx_set_ratectrl(sc, bf->bf_node, bf);
	ath_tx_chaindesclist(sc, bf);

	/* Statistics */
	sc->sc_aggr_stats.aggr_low_hwq_single_pkt++;

	/* Track per-TID hardware queue depth correctly */
	tid->hwq_depth++;

	/* Add to BAW */
	if (bf->bf_state.bfs_dobaw) {
		ath_tx_addto_baw(sc, an, tid, bf);
		bf->bf_state.bfs_addedbaw = 1;
	}

	/* Set completion handler, multi-frame aggregate or not */
	bf->bf_comp = ath_tx_aggr_comp;

	/* Hand off to hardware */
	ath_tx_handoff(sc, txq, bf);
}

/*
 * Attempt to send the packet.
 * If the queue isn't busy, direct-dispatch.
 * If the queue is busy enough, queue the given packet on the
 *  relevant software queue.
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

	/*
	 * If the hardware queue isn't busy, queue it directly.
	 * If the hardware queue is busy, queue it.
	 * If the TID is paused or the traffic it outside BAW, software
	 * queue it.
	 */
	ATH_TXQ_LOCK(txq);
	if (atid->paused) {
		/* TID is paused, queue */
		ATH_TXQ_INSERT_TAIL(atid, bf, bf_list);
	} else if (ath_tx_ampdu_pending(sc, an, tid)) {
		/* AMPDU pending; queue */
		ATH_TXQ_INSERT_TAIL(atid, bf, bf_list);
		/* XXX sched? */
	} else if (ath_tx_ampdu_running(sc, an, tid)) {
		/* AMPDU running, attempt direct dispatch if possible */
		if (txq->axq_depth < sc->sc_hwq_limit)
			ath_tx_xmit_aggr(sc, an, bf);
		else {
			ATH_TXQ_INSERT_TAIL(atid, bf, bf_list);
			ath_tx_tid_sched(sc, atid);
		}
	} else if (txq->axq_depth < sc->sc_hwq_limit) {
		/* AMPDU not running, attempt direct dispatch */
		ath_tx_xmit_normal(sc, txq, bf);
	} else {
		/* Busy; queue */
		ATH_TXQ_INSERT_TAIL(atid, bf, bf_list);
		ath_tx_tid_sched(sc, atid);
	}
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
		TAILQ_INIT(&atid->axq_q);
		atid->tid = i;
		atid->an = an;
		for (j = 0; j < ATH_TID_MAX_BUFS; j++)
			atid->tx_buf[j] = NULL;
		atid->baw_head = atid->baw_tail = 0;
		atid->paused = 0;
		atid->sched = 0;
		atid->hwq_depth = 0;
		atid->cleanup_inprogress = 0;
		if (i == IEEE80211_NONQOS_TID)
			atid->ac = WME_AC_BE;
		else
			atid->ac = TID_TO_WME_AC(i);
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
	ATH_TXQ_LOCK(sc->sc_ac2q[tid->ac]);
	tid->paused++;
	DPRINTF(sc, ATH_DEBUG_SW_TX_CTRL, "%s: paused = %d\n",
	    __func__, tid->paused);
	ATH_TXQ_UNLOCK(sc->sc_ac2q[tid->ac]);
}

/*
 * Unpause the current TID, and schedule it if needed.
 */
static void
ath_tx_tid_resume(struct ath_softc *sc, struct ath_tid *tid)
{
	ATH_TXQ_LOCK_ASSERT(sc->sc_ac2q[tid->ac]);

	tid->paused--;

	DPRINTF(sc, ATH_DEBUG_SW_TX_CTRL, "%s: unpaused = %d\n",
	    __func__, tid->paused);

	if (tid->paused || tid->axq_depth == 0) {
		return;
	}

	ath_tx_tid_sched(sc, tid);
	/* Punt some frames to the hardware if needed */
	ath_txq_sched(sc, sc->sc_ac2q[tid->ac]);
}

/*
 * Free any packets currently pending in the software TX queue.
 *
 * This will be called when a node is being deleted.
 *
 * It can also be called on an active node during an interface
 * reset or state transition.
 *
 * (From Linux/reference):
 *
 * TODO: For frame(s) that are in the retry state, we will reuse the
 * sequence number(s) without setting the retry bit. The
 * alternative is to give up on these and BAR the receiver's window
 * forward.
 */
static void
ath_tx_tid_drain(struct ath_softc *sc, struct ath_node *an, struct ath_tid *tid,
    ath_bufhead *bf_cq)
{
	struct ath_buf *bf;
	struct ieee80211_tx_ampdu *tap;
	struct ieee80211_node *ni = &an->an_node;
	int t = 0;
	struct ath_txq *txq = sc->sc_ac2q[tid->ac];

	tap = ath_tx_get_tx_tid(an, tid->tid);

	ATH_TXQ_LOCK_ASSERT(sc->sc_ac2q[tid->ac]);

	/* Walk the queue, free frames */
	for (;;) {
		bf = TAILQ_FIRST(&tid->axq_q);
		if (bf == NULL) {
			break;
		}

		if (t == 0) {
			device_printf(sc->sc_dev,
			    "%s: node %p: tid %d: txq_depth=%d, "
			    "txq_aggr_depth=%d, sched=%d, paused=%d, "
			    "hwq_depth=%d, incomp=%d, baw_head=%d, baw_tail=%d "
			    "txa_start=%d, ni_txseqs=%d\n",
			     __func__, ni, tid->tid, txq->axq_depth,
			     txq->axq_aggr_depth, tid->sched, tid->paused,
			     tid->hwq_depth, tid->incomp, tid->baw_head,
			     tid->baw_tail, tap == NULL ? -1 : tap->txa_start,
			     ni->ni_txseqs[tid->tid]);
			t = 1;
		}


		/*
		 * If the current TID is running AMPDU, update
		 * the BAW.
		 */
		if (ath_tx_ampdu_running(sc, an, tid->tid) &&
		    bf->bf_state.bfs_dobaw) {
			/*
			 * Only remove the frame from the BAW if it's
			 * been transmitted at least once; this means
			 * the frame was in the BAW to begin with.
			 */
			if (bf->bf_state.bfs_retries > 0) {
				ath_tx_update_baw(sc, an, tid, bf);
				bf->bf_state.bfs_dobaw = 0;
			}
			/*
			 * This has become a non-fatal error now
			 */
			if (! bf->bf_state.bfs_addedbaw)
				device_printf(sc->sc_dev,
				    "%s: wasn't added: seqno %d\n",
				    __func__, SEQNO(bf->bf_state.bfs_seqno));
		}
		ATH_TXQ_REMOVE(tid, bf, bf_list);
		TAILQ_INSERT_TAIL(bf_cq, bf, bf_list);
	}

	/*
	 * Now that it's completed, grab the TID lock and update
	 * the sequence number and BAW window.
	 * Because sequence numbers have been assigned to frames
	 * that haven't been sent yet, it's entirely possible
	 * we'll be called with some pending frames that have not
	 * been transmitted.
	 *
	 * The cleaner solution is to do the sequence number allocation
	 * when the packet is first transmitted - and thus the "retries"
	 * check above would be enough to update the BAW/seqno.
	 */

	/* But don't do it for non-QoS TIDs */
	if (tap) {
#if 0
		DPRINTF(sc, ATH_DEBUG_SW_TX_CTRL,
		    "%s: node %p: TID %d: sliding BAW left edge to %d\n",
		    __func__, an, tid->tid, tap->txa_start);
#endif
		ni->ni_txseqs[tid->tid] = tap->txa_start;
		tid->baw_tail = tid->baw_head;
	}
}

/*
 * Flush all software queued packets for the given node.
 *
 * This occurs when a completion handler frees the last buffer
 * for a node, and the node is thus freed. This causes the node
 * to be cleaned up, which ends up calling ath_tx_node_flush.
 */
void
ath_tx_node_flush(struct ath_softc *sc, struct ath_node *an)
{
	int tid;
	ath_bufhead bf_cq;
	struct ath_buf *bf;

	TAILQ_INIT(&bf_cq);

	for (tid = 0; tid < IEEE80211_TID_SIZE; tid++) {
		struct ath_tid *atid = &an->an_tid[tid];
		struct ath_txq *txq = sc->sc_ac2q[atid->ac];

		/* Remove this tid from the list of active tids */
		ATH_TXQ_LOCK(txq);
		ath_tx_tid_unsched(sc, atid);

		/* Free packets */
		ath_tx_tid_drain(sc, an, atid, &bf_cq);
		ATH_TXQ_UNLOCK(txq);
	}

	/* Handle completed frames */
	while ((bf = TAILQ_FIRST(&bf_cq)) != NULL) {
		TAILQ_REMOVE(&bf_cq, bf, bf_list);
		ath_tx_default_comp(sc, bf, 0);
	}
}

/*
 * Drain all the software TXQs currently with traffic queued.
 */
void
ath_tx_txq_drain(struct ath_softc *sc, struct ath_txq *txq)
{
	struct ath_tid *tid;
	ath_bufhead bf_cq;
	struct ath_buf *bf;

	TAILQ_INIT(&bf_cq);
	ATH_TXQ_LOCK(txq);

	/*
	 * Iterate over all active tids for the given txq,
	 * flushing and unsched'ing them
	 */
	while (! TAILQ_EMPTY(&txq->axq_tidq)) {
		tid = TAILQ_FIRST(&txq->axq_tidq);
		ath_tx_tid_drain(sc, tid->an, tid, &bf_cq);
		ath_tx_tid_unsched(sc, tid);
	}

	ATH_TXQ_UNLOCK(txq);

	while ((bf = TAILQ_FIRST(&bf_cq)) != NULL) {
		TAILQ_REMOVE(&bf_cq, bf, bf_list);
		ath_tx_default_comp(sc, bf, 0);
	}
}

/*
 * Handle completion of non-aggregate session frames.
 */
void
ath_tx_normal_comp(struct ath_softc *sc, struct ath_buf *bf, int fail)
{
	struct ieee80211_node *ni = bf->bf_node;
	struct ath_node *an = ATH_NODE(ni);
	int tid = bf->bf_state.bfs_tid;
	struct ath_tid *atid = &an->an_tid[tid];
	struct ath_tx_status *ts = &bf->bf_status.ds_txstat;

	/* The TID state is protected behind the TXQ lock */
	ATH_TXQ_LOCK(sc->sc_ac2q[atid->ac]);

	DPRINTF(sc, ATH_DEBUG_SW_TX, "%s: bf=%p: fail=%d, hwq_depth now %d\n",
	    __func__, bf, fail, atid->hwq_depth - 1);

	atid->hwq_depth--;
	if (atid->hwq_depth < 0)
		device_printf(sc->sc_dev, "%s: hwq_depth < 0: %d\n",
		    __func__, atid->hwq_depth);
	ATH_TXQ_UNLOCK(sc->sc_ac2q[atid->ac]);

	/*
	 * punt to rate control if we're not being cleaned up
	 * during a hw queue drain and the frame wanted an ACK.
	 */
	if (fail == 0 && ((bf->bf_txflags & HAL_TXDESC_NOACK) == 0))
		ath_tx_update_ratectrl(sc, ni, bf->bf_state.bfs_rc,
		    ts, bf->bf_state.bfs_pktlen,
		    1, (ts->ts_status == 0) ? 0 : 1);

	ath_tx_default_comp(sc, bf, fail);
}

/*
 * Handle cleanup of aggregate session packets that aren't
 * an A-MPDU.
 *
 * There's no need to update the BAW here - the session is being
 * torn down.
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

	ATH_TXQ_LOCK(sc->sc_ac2q[atid->ac]);
	atid->incomp--;
	if (atid->incomp == 0) {
		DPRINTF(sc, ATH_DEBUG_SW_TX_CTRL,
		    "%s: TID %d: cleaned up! resume!\n",
		    __func__, tid);
		atid->cleanup_inprogress = 0;
		ath_tx_tid_resume(sc, atid);
	}
	ATH_TXQ_UNLOCK(sc->sc_ac2q[atid->ac]);

	ath_tx_default_comp(sc, bf, 0);
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
	ath_bufhead bf_cq;

	DPRINTF(sc, ATH_DEBUG_SW_TX_CTRL,
	    "%s: TID %d: called\n", __func__, tid);

	TAILQ_INIT(&bf_cq);
	ATH_TXQ_LOCK(sc->sc_ac2q[atid->ac]);

	/*
	 * Update the frames in the software TX queue:
	 *
	 * + Discard retry frames in the queue
	 * + Fix the completion function to be non-aggregate
	 */
	bf = TAILQ_FIRST(&atid->axq_q);
	while (bf) {
		if (bf->bf_state.bfs_isretried) {
			bf_next = TAILQ_NEXT(bf, bf_list);
			TAILQ_REMOVE(&atid->axq_q, bf, bf_list);
			atid->axq_depth--;
			if (bf->bf_state.bfs_dobaw) {
				ath_tx_update_baw(sc, an, atid, bf);
				if (! bf->bf_state.bfs_addedbaw)
					device_printf(sc->sc_dev,
					    "%s: wasn't added: seqno %d\n",
					    __func__, SEQNO(bf->bf_state.bfs_seqno));
			}
			bf->bf_state.bfs_dobaw = 0;
			/*
			 * Call the default completion handler with "fail" just
			 * so upper levels are suitably notified about this.
			 */
			TAILQ_INSERT_TAIL(&bf_cq, bf, bf_list);
			bf = bf_next;
			continue;
		}
		/* Give these the default completion handler */
		bf->bf_comp = ath_tx_normal_comp;
		bf = TAILQ_NEXT(bf, bf_list);
	}

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
	/* Need the lock - fiddling with BAW */
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
	ATH_TXQ_UNLOCK(sc->sc_ac2q[atid->ac]);

	/* Handle completing frames and fail them */
	while ((bf = TAILQ_FIRST(&bf_cq)) != NULL) {
		TAILQ_REMOVE(&bf_cq, bf, bf_list);
		ath_tx_default_comp(sc, bf, 1);
	}
}

static void
ath_tx_set_retry(struct ath_softc *sc, struct ath_buf *bf)
{
	struct ieee80211_frame *wh;

	wh = mtod(bf->bf_m, struct ieee80211_frame *);
	/* Only update/resync if needed */
	if (bf->bf_state.bfs_isretried == 0) {
		wh->i_fc[1] |= IEEE80211_FC1_RETRY;
		bus_dmamap_sync(sc->sc_dmat, bf->bf_dmamap,
		    BUS_DMASYNC_PREWRITE);
	}
	sc->sc_stats.ast_tx_swretries++;
	bf->bf_state.bfs_isretried = 1;
	bf->bf_state.bfs_retries ++;
}

static struct ath_buf *
ath_tx_retry_clone(struct ath_softc *sc, struct ath_buf *bf)
{
	struct ath_buf *nbf;
	int error;

	nbf = ath_buf_clone(sc, bf);

#if 0
	device_printf(sc->sc_dev, "%s: ATH_BUF_BUSY; cloning\n",
	    __func__);
#endif

	if (nbf == NULL) {
		/* Failed to clone */
		device_printf(sc->sc_dev,
		    "%s: failed to clone a busy buffer\n",
		    __func__);
		return NULL;
	}

	/* Setup the dma for the new buffer */
	error = ath_tx_dmasetup(sc, nbf, nbf->bf_m);
	if (error != 0) {
		device_printf(sc->sc_dev,
		    "%s: failed to setup dma for clone\n",
		    __func__);
		/*
		 * Put this at the head of the list, not tail;
		 * that way it doesn't interfere with the
		 * busy buffer logic (which uses the tail of
		 * the list.)
		 */
		ATH_TXBUF_LOCK(sc);
		TAILQ_INSERT_HEAD(&sc->sc_txbuf, nbf, bf_list);
		ATH_TXBUF_UNLOCK(sc);
		return NULL;
	}

	/* Free current buffer; return the older buffer */
	bf->bf_m = NULL;
	bf->bf_node = NULL;
	ath_freebuf(sc, bf);
	return nbf;
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
	int txseq;

	ATH_TXQ_LOCK(sc->sc_ac2q[atid->ac]);

	tap = ath_tx_get_tx_tid(an, tid);

	/*
	 * If the buffer is marked as busy, we can't directly
	 * reuse it. Instead, try to clone the buffer.
	 * If the clone is successful, recycle the old buffer.
	 * If the clone is unsuccessful, set bfs_retries to max
	 * to force the next bit of code to free the buffer
	 * for us.
	 */
	if ((bf->bf_state.bfs_retries < SWMAX_RETRIES) &&
	    (bf->bf_flags & ATH_BUF_BUSY)) {
		struct ath_buf *nbf;
		nbf = ath_tx_retry_clone(sc, bf);
		if (nbf)
			/* bf has been freed at this point */
			bf = nbf;
		else
			bf->bf_state.bfs_retries = SWMAX_RETRIES + 1;
	}

	if (bf->bf_state.bfs_retries >= SWMAX_RETRIES) {
		DPRINTF(sc, ATH_DEBUG_SW_TX_RETRIES,
		    "%s: exceeded retries; seqno %d\n",
		    __func__, SEQNO(bf->bf_state.bfs_seqno));
		sc->sc_stats.ast_tx_swretrymax++;

		/* Update BAW anyway */
		if (bf->bf_state.bfs_dobaw) {
			ath_tx_update_baw(sc, an, atid, bf);
			if (! bf->bf_state.bfs_addedbaw)
				device_printf(sc->sc_dev,
				    "%s: wasn't added: seqno %d\n",
				    __func__, SEQNO(bf->bf_state.bfs_seqno));
		}
		bf->bf_state.bfs_dobaw = 0;

		/* Send BAR frame */
		/*
		 * This'll end up going into net80211 and back out
		 * again, via ic->ic_raw_xmit().
		 */
		txseq = tap->txa_start;
		ATH_TXQ_UNLOCK(sc->sc_ac2q[atid->ac]);

		device_printf(sc->sc_dev,
		    "%s: TID %d: send BAR; seq %d\n", __func__, tid, txseq);

		/* XXX TODO: send BAR */

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
	 * Insert this at the head of the queue, so it's
	 * retried before any current/subsequent frames.
	 */
	ATH_TXQ_INSERT_HEAD(atid, bf, bf_list);
	ath_tx_tid_sched(sc, atid);

	ATH_TXQ_UNLOCK(sc->sc_ac2q[atid->ac]);
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

	ATH_TXQ_LOCK_ASSERT(sc->sc_ac2q[atid->ac]);

	ath_hal_clr11n_aggr(sc->sc_ah, bf->bf_desc);
	ath_hal_set11nburstduration(sc->sc_ah, bf->bf_desc, 0);
	/* ath_hal_set11n_virtualmorefrag(sc->sc_ah, bf->bf_desc, 0); */

	/*
	 * If the buffer is marked as busy, we can't directly
	 * reuse it. Instead, try to clone the buffer.
	 * If the clone is successful, recycle the old buffer.
	 * If the clone is unsuccessful, set bfs_retries to max
	 * to force the next bit of code to free the buffer
	 * for us.
	 */
	if ((bf->bf_state.bfs_retries < SWMAX_RETRIES) &&
	    (bf->bf_flags & ATH_BUF_BUSY)) {
		struct ath_buf *nbf;
		nbf = ath_tx_retry_clone(sc, bf);
		if (nbf)
			/* bf has been freed at this point */
			bf = nbf;
		else
			bf->bf_state.bfs_retries = SWMAX_RETRIES + 1;
	}

	if (bf->bf_state.bfs_retries >= SWMAX_RETRIES) {
		sc->sc_stats.ast_tx_swretrymax++;
		DPRINTF(sc, ATH_DEBUG_SW_TX_RETRIES,
		    "%s: max retries: seqno %d\n",
		    __func__, SEQNO(bf->bf_state.bfs_seqno));
		ath_tx_update_baw(sc, an, atid, bf);
		if (! bf->bf_state.bfs_addedbaw)
			device_printf(sc->sc_dev,
			    "%s: wasn't added: seqno %d\n",
			    __func__, SEQNO(bf->bf_state.bfs_seqno));
		bf->bf_state.bfs_dobaw = 0;
		return 1;
	}

	ath_tx_set_retry(sc, bf);
	bf->bf_next = NULL;		/* Just to make sure */

	TAILQ_INSERT_TAIL(bf_q, bf, bf_list);
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
	ath_bufhead bf_cq;

	TAILQ_INIT(&bf_q);
	TAILQ_INIT(&bf_cq);
	sc->sc_stats.ast_tx_aggrfail++;

	/*
	 * Update rate control - all frames have failed.
	 *
	 * XXX use the length in the first frame in the series;
	 * XXX just so things are consistent for now.
	 */
	ath_tx_update_ratectrl(sc, ni, bf_first->bf_state.bfs_rc,
	    &bf_first->bf_status.ds_txstat,
	    bf_first->bf_state.bfs_pktlen,
	    bf_first->bf_state.bfs_nframes, bf_first->bf_state.bfs_nframes);

	ATH_TXQ_LOCK(sc->sc_ac2q[tid->ac]);
	tap = ath_tx_get_tx_tid(an, tid->tid);

	/* Retry all subframes */
	bf = bf_first;
	while (bf) {
		bf_next = bf->bf_next;
		bf->bf_next = NULL;	/* Remove it from the aggr list */
		if (ath_tx_retry_subframe(sc, bf, &bf_q)) {
			drops++;
			bf->bf_next = NULL;
			TAILQ_INSERT_TAIL(&bf_cq, bf, bf_list);
		}
		bf = bf_next;
	}

	/* Prepend all frames to the beginning of the queue */
	while ((bf = TAILQ_LAST(&bf_q, ath_bufhead_s)) != NULL) {
		TAILQ_REMOVE(&bf_q, bf, bf_list);
		ATH_TXQ_INSERT_HEAD(tid, bf, bf_list);
	}

	ath_tx_tid_sched(sc, tid);

	/*
	 * send bar if we dropped any frames
	 *
	 * Keep the txq lock held for now, as we need to ensure
	 * that ni_txseqs[] is consistent (as it's being updated
	 * in the ifnet TX context or raw TX context.)
	 */
	if (drops) {
		int txseq = tap->txa_start;
		ATH_TXQ_UNLOCK(sc->sc_ac2q[tid->ac]);
		device_printf(sc->sc_dev,
		    "%s: TID %d: send BAR; seq %d\n",
		    __func__, tid->tid, txseq);

		/* XXX TODO: send BAR */
	} else {
		ATH_TXQ_UNLOCK(sc->sc_ac2q[tid->ac]);
	}

	/* Complete frames which errored out */
	while ((bf = TAILQ_FIRST(&bf_cq)) != NULL) {
		TAILQ_REMOVE(&bf_cq, bf, bf_list);
		ath_tx_default_comp(sc, bf, 0);
	}
}

/*
 * Handle clean-up of packets from an aggregate list.
 *
 * There's no need to update the BAW here - the session is being
 * torn down.
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

	ATH_TXQ_LOCK(sc->sc_ac2q[atid->ac]);

	/* update incomp */
	while (bf) {
		atid->incomp--;
		bf = bf->bf_next;
	}

	if (atid->incomp == 0) {
		DPRINTF(sc, ATH_DEBUG_SW_TX_CTRL,
		    "%s: TID %d: cleaned up! resume!\n",
		    __func__, tid);
		atid->cleanup_inprogress = 0;
		ath_tx_tid_resume(sc, atid);
	}
	ATH_TXQ_UNLOCK(sc->sc_ac2q[atid->ac]);

	/* Handle frame completion */
	while (bf) {
		bf_next = bf->bf_next;
		ath_tx_default_comp(sc, bf, 1);
		bf = bf_next;
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
	struct ath_tx_status ts;
	struct ieee80211_tx_ampdu *tap;
	ath_bufhead bf_q;
	ath_bufhead bf_cq;
	int seq_st, tx_ok;
	int hasba, isaggr;
	uint32_t ba[2];
	struct ath_buf *bf, *bf_next;
	int ba_index;
	int drops = 0;
	int nframes = 0, nbad = 0, nf;
	int pktlen;
	/* XXX there's too much on the stack? */
	struct ath_rc_series rc[4];
	int txseq;

	DPRINTF(sc, ATH_DEBUG_SW_TX_AGGR, "%s: called; hwq_depth=%d\n",
	    __func__, atid->hwq_depth);

	/* The TID state is kept behind the TXQ lock */
	ATH_TXQ_LOCK(sc->sc_ac2q[atid->ac]);

	atid->hwq_depth--;
	if (atid->hwq_depth < 0)
		device_printf(sc->sc_dev, "%s: hwq_depth < 0: %d\n",
		    __func__, atid->hwq_depth);

	/*
	 * Punt cleanup to the relevant function, not our problem now
	 */
	if (atid->cleanup_inprogress) {
		ATH_TXQ_UNLOCK(sc->sc_ac2q[atid->ac]);
		ath_tx_comp_cleanup_aggr(sc, bf_first);
		return;
	}

	/*
	 * Take a copy; this may be needed -after- bf_first
	 * has been completed and freed.
	 */
	ts = bf_first->bf_status.ds_txstat;
	/*
	 * XXX for now, use the first frame in the aggregate for
	 * XXX rate control completion; it's at least consistent.
	 */
	pktlen = bf_first->bf_state.bfs_pktlen;

	/*
	 * handle errors first
	 */
	if (ts.ts_status & HAL_TXERR_XRETRY) {
		ATH_TXQ_UNLOCK(sc->sc_ac2q[atid->ac]);
		ath_tx_comp_aggr_error(sc, bf_first, atid);
		return;
	}

	TAILQ_INIT(&bf_q);
	TAILQ_INIT(&bf_cq);
	tap = ath_tx_get_tx_tid(an, tid);

	/*
	 * extract starting sequence and block-ack bitmap
	 */
	/* XXX endian-ness of seq_st, ba? */
	seq_st = ts.ts_seqnum;
	hasba = !! (ts.ts_flags & HAL_TX_BA);
	tx_ok = (ts.ts_status == 0);
	isaggr = bf_first->bf_state.bfs_aggr;
	ba[0] = ts.ts_ba_low;
	ba[1] = ts.ts_ba_high;

	/*
	 * Copy the TX completion status and the rate control
	 * series from the first descriptor, as it may be freed
	 * before the rate control code can get its grubby fingers
	 * into things.
	 */
	memcpy(rc, bf_first->bf_state.bfs_rc, sizeof(rc));

	DPRINTF(sc, ATH_DEBUG_SW_TX_AGGR,
	    "%s: txa_start=%d, tx_ok=%d, status=%.8x, flags=%.8x, isaggr=%d, seq_st=%d, hasba=%d, ba=%.8x, %.8x\n",
	    __func__, tap->txa_start, tx_ok, ts.ts_status, ts.ts_flags,
	    isaggr, seq_st, hasba, ba[0], ba[1]);

	/* Occasionally, the MAC sends a tx status for the wrong TID. */
	if (tid != ts.ts_tid) {
		device_printf(sc->sc_dev, "%s: tid %d != hw tid %d\n",
		    __func__, tid, ts.ts_tid);
		tx_ok = 0;
	}

	/* AR5416 BA bug; this requires an interface reset */
	if (isaggr && tx_ok && (! hasba)) {
		device_printf(sc->sc_dev,
		    "%s: AR5416 bug: hasba=%d; txok=%d, isaggr=%d, seq_st=%d\n",
		    __func__, hasba, tx_ok, isaggr, seq_st);
		/* XXX TODO: schedule an interface reset */
	}

	/*
	 * Walk the list of frames, figure out which ones were correctly
	 * sent and which weren't.
	 */
	bf = bf_first;
	nf = bf_first->bf_state.bfs_nframes;

	/* bf_first is going to be invalid once this list is walked */
	bf_first = NULL;

	/*
	 * Walk the list of completed frames and determine
	 * which need to be completed and which need to be
	 * retransmitted.
	 *
	 * For completed frames, the completion functions need
	 * to be called at the end of this function as the last
	 * node reference may free the node.
	 *
	 * Finally, since the TXQ lock can't be held during the
	 * completion callback (to avoid lock recursion),
	 * the completion calls have to be done outside of the
	 * lock.
	 */
	while (bf) {
		nframes++;
		ba_index = ATH_BA_INDEX(seq_st, SEQNO(bf->bf_state.bfs_seqno));
		bf_next = bf->bf_next;
		bf->bf_next = NULL;	/* Remove it from the aggr list */

		DPRINTF(sc, ATH_DEBUG_SW_TX_AGGR,
		    "%s: checking bf=%p seqno=%d; ack=%d\n",
		    __func__, bf, SEQNO(bf->bf_state.bfs_seqno),
		    ATH_BA_ISSET(ba, ba_index));

		if (tx_ok && ATH_BA_ISSET(ba, ba_index)) {
			ath_tx_update_baw(sc, an, atid, bf);
			bf->bf_state.bfs_dobaw = 0;
			if (! bf->bf_state.bfs_addedbaw)
				device_printf(sc->sc_dev,
				    "%s: wasn't added: seqno %d\n",
				    __func__, SEQNO(bf->bf_state.bfs_seqno));
			bf->bf_next = NULL;
			TAILQ_INSERT_TAIL(&bf_cq, bf, bf_list);
		} else {
			if (ath_tx_retry_subframe(sc, bf, &bf_q)) {
				drops++;
				bf->bf_next = NULL;
				TAILQ_INSERT_TAIL(&bf_cq, bf, bf_list);
			}
			nbad++;
		}
		bf = bf_next;
	}

	/*
	 * Now that the BAW updates have been done, unlock
	 *
	 * txseq is grabbed before the lock is released so we
	 * have a consistent view of what -was- in the BAW.
	 * Anything after this point will not yet have been
	 * TXed.
	 */
	txseq = tap->txa_start;
	ATH_TXQ_UNLOCK(sc->sc_ac2q[atid->ac]);

	if (nframes != nf)
		device_printf(sc->sc_dev,
		    "%s: num frames seen=%d; bf nframes=%d\n",
		    __func__, nframes, nf);

	/*
	 * Now we know how many frames were bad, call the rate
	 * control code.
	 */
	if (fail == 0)
		ath_tx_update_ratectrl(sc, ni, rc, &ts, pktlen, nframes, nbad);

	/*
	 * send bar if we dropped any frames
	 */
	if (drops) {
		device_printf(sc->sc_dev,
		    "%s: TID %d: send BAR; seq %d\n", __func__, tid, txseq);
		/* XXX TODO: send BAR */
	}

	/* Prepend all frames to the beginning of the queue */
	ATH_TXQ_LOCK(sc->sc_ac2q[atid->ac]);
	while ((bf = TAILQ_LAST(&bf_q, ath_bufhead_s)) != NULL) {
		TAILQ_REMOVE(&bf_q, bf, bf_list);
		ATH_TXQ_INSERT_HEAD(atid, bf, bf_list);
	}
	ath_tx_tid_sched(sc, atid);
	ATH_TXQ_UNLOCK(sc->sc_ac2q[atid->ac]);

	DPRINTF(sc, ATH_DEBUG_SW_TX_AGGR,
	    "%s: txa_start now %d\n", __func__, tap->txa_start);

	/* Do deferred completion */
	while ((bf = TAILQ_FIRST(&bf_cq)) != NULL) {
		TAILQ_REMOVE(&bf_cq, bf, bf_list);
		ath_tx_default_comp(sc, bf, 0);
	}
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

	/*
	 * Update rate control status here, before we possibly
	 * punt to retry or cleanup.
	 *
	 * Do it outside of the TXQ lock.
	 */
	if (fail == 0 && ((bf->bf_txflags & HAL_TXDESC_NOACK) == 0))
		ath_tx_update_ratectrl(sc, ni, bf->bf_state.bfs_rc,
		    &bf->bf_status.ds_txstat,
		    bf->bf_state.bfs_pktlen,
		    1, (ts->ts_status == 0) ? 0 : 1);

	/*
	 * This is called early so atid->hwq_depth can be tracked.
	 * This unfortunately means that it's released and regrabbed
	 * during retry and cleanup. That's rather inefficient.
	 */
	ATH_TXQ_LOCK(sc->sc_ac2q[atid->ac]);

	if (tid == IEEE80211_NONQOS_TID)
		device_printf(sc->sc_dev, "%s: TID=16!\n", __func__);

	DPRINTF(sc, ATH_DEBUG_SW_TX, "%s: bf=%p: tid=%d, hwq_depth=%d\n",
	    __func__, bf, bf->bf_state.bfs_tid, atid->hwq_depth);

	atid->hwq_depth--;
	if (atid->hwq_depth < 0)
		device_printf(sc->sc_dev, "%s: hwq_depth < 0: %d\n",
		    __func__, atid->hwq_depth);

	/*
	 * If a cleanup is in progress, punt to comp_cleanup;
	 * rather than handling it here. It's thus their
	 * responsibility to clean up, call the completion
	 * function in net80211, etc.
	 */
	if (atid->cleanup_inprogress) {
		ATH_TXQ_UNLOCK(sc->sc_ac2q[atid->ac]);
		ath_tx_comp_cleanup_unaggr(sc, bf);
		return;
	}

	/*
	 * Don't bother with the retry check if all frames
	 * are being failed (eg during queue deletion.)
	 */
	if (fail == 0 && ts->ts_status & HAL_TXERR_XRETRY) {
		ATH_TXQ_UNLOCK(sc->sc_ac2q[atid->ac]);
		ath_tx_aggr_retry_unaggr(sc, bf);
		return;
	}

	/* Success? Complete */
	DPRINTF(sc, ATH_DEBUG_SW_TX, "%s: TID=%d, seqno %d\n",
	    __func__, tid, SEQNO(bf->bf_state.bfs_seqno));
	if (bf->bf_state.bfs_dobaw) {
		ath_tx_update_baw(sc, an, atid, bf);
		bf->bf_state.bfs_dobaw = 0;
		if (! bf->bf_state.bfs_addedbaw)
			device_printf(sc->sc_dev,
			    "%s: wasn't added: seqno %d\n",
			    __func__, SEQNO(bf->bf_state.bfs_seqno));
	}

	ATH_TXQ_UNLOCK(sc->sc_ac2q[atid->ac]);

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
ath_tx_tid_hw_queue_aggr(struct ath_softc *sc, struct ath_node *an,
    struct ath_tid *tid)
{
	struct ath_buf *bf;
	struct ath_txq *txq = sc->sc_ac2q[tid->ac];
	struct ieee80211_tx_ampdu *tap;
	struct ieee80211_node *ni = &an->an_node;
	ATH_AGGR_STATUS status;
	ath_bufhead bf_q;

	DPRINTF(sc, ATH_DEBUG_SW_TX, "%s: tid=%d\n", __func__, tid->tid);
	ATH_TXQ_LOCK_ASSERT(txq);

	tap = ath_tx_get_tx_tid(an, tid->tid);

	if (tid->tid == IEEE80211_NONQOS_TID)
		device_printf(sc->sc_dev, "%s: called for TID=NONQOS_TID?\n",
		    __func__);

	for (;;) {
		status = ATH_AGGR_DONE;

		/*
		 * If the upper layer has paused the TID, don't
		 * queue any further packets.
		 *
		 * This can also occur from the completion task because
		 * of packet loss; but as its serialised with this code,
		 * it won't "appear" half way through queuing packets.
		 */
		if (tid->paused)
			break;

		bf = TAILQ_FIRST(&tid->axq_q);
		if (bf == NULL) {
			break;
		}

		/*
		 * If the packet doesn't fall within the BAW (eg a NULL
		 * data frame), schedule it directly; continue.
		 */
		if (! bf->bf_state.bfs_dobaw) {
			DPRINTF(sc, ATH_DEBUG_SW_TX_AGGR, "%s: non-baw packet\n",
			    __func__);
			ATH_TXQ_REMOVE(tid, bf, bf_list);
			bf->bf_state.bfs_aggr = 0;
			ath_tx_do_ratelookup(sc, bf);
			ath_tx_rate_fill_rcflags(sc, bf);
			ath_tx_set_rtscts(sc, bf);
			ath_tx_setds(sc, bf);
			ath_tx_chaindesclist(sc, bf);
			ath_hal_clr11n_aggr(sc->sc_ah, bf->bf_desc);
			ath_tx_set_ratectrl(sc, ni, bf);

			sc->sc_aggr_stats.aggr_nonbaw_pkt++;

			/* Queue the packet; continue */
			goto queuepkt;
		}

		TAILQ_INIT(&bf_q);

		/*
		 * Do a rate control lookup on the first frame in the
		 * list. The rate control code needs that to occur
		 * before it can determine whether to TX.
		 * It's inaccurate because the rate control code doesn't
		 * really "do" aggregate lookups, so it only considers
		 * the size of the first frame.
		 */
		ath_tx_do_ratelookup(sc, bf);
		bf->bf_state.bfs_rc[3].rix = 0;
		bf->bf_state.bfs_rc[3].tries = 0;
		ath_tx_rate_fill_rcflags(sc, bf);

		status = ath_tx_form_aggr(sc, an, tid, &bf_q);

		DPRINTF(sc, ATH_DEBUG_SW_TX_AGGR,
		    "%s: ath_tx_form_aggr() status=%d\n", __func__, status);

		/*
		 * No frames to be picked up - out of BAW
		 */
		if (TAILQ_EMPTY(&bf_q))
			break;

		/*
		 * This assumes that the descriptor list in the ath_bufhead
		 * are already linked together via bf_next pointers.
		 */
		bf = TAILQ_FIRST(&bf_q);

		/*
		 * If it's the only frame send as non-aggregate
		 * assume that ath_tx_form_aggr() has checked
		 * whether it's in the BAW and added it appropriately.
		 */
		if (bf->bf_state.bfs_nframes == 1) {
			DPRINTF(sc, ATH_DEBUG_SW_TX_AGGR,
			    "%s: single-frame aggregate\n", __func__);
			bf->bf_state.bfs_aggr = 0;
			ath_tx_set_rtscts(sc, bf);
			ath_tx_setds(sc, bf);
			ath_tx_chaindesclist(sc, bf);
			ath_hal_clr11n_aggr(sc->sc_ah, bf->bf_desc);
			ath_tx_set_ratectrl(sc, ni, bf);
			if (status == ATH_AGGR_BAW_CLOSED)
				sc->sc_aggr_stats.aggr_baw_closed_single_pkt++;
			else
				sc->sc_aggr_stats.aggr_single_pkt++;
		} else {
			DPRINTF(sc, ATH_DEBUG_SW_TX_AGGR,
			    "%s: multi-frame aggregate: %d frames, length %d\n",
			     __func__, bf->bf_state.bfs_nframes,
			    bf->bf_state.bfs_al);
			bf->bf_state.bfs_aggr = 1;
			sc->sc_aggr_stats.aggr_pkts[bf->bf_state.bfs_nframes]++;
			sc->sc_aggr_stats.aggr_aggr_pkt++;

			/*
			 * Update the rate and rtscts information based on the
			 * rate decision made by the rate control code;
			 * the first frame in the aggregate needs it.
			 */
			ath_tx_set_rtscts(sc, bf);

			/*
			 * Setup the relevant descriptor fields
			 * for aggregation. The first descriptor
			 * already points to the rest in the chain.
			 */
			ath_tx_setds_11n(sc, bf);

			/*
			 * setup first desc with rate and aggr info
			 */
			ath_tx_set_ratectrl(sc, ni, bf);
		}
	queuepkt:
		//txq = bf->bf_state.bfs_txq;

		/* Set completion handler, multi-frame aggregate or not */
		bf->bf_comp = ath_tx_aggr_comp;

		if (bf->bf_state.bfs_tid == IEEE80211_NONQOS_TID)
		    device_printf(sc->sc_dev, "%s: TID=16?\n", __func__);

		/* Punt to txq */
		ath_tx_handoff(sc, txq, bf);

		/* Track outstanding buffer count to hardware */
		/* aggregates are "one" buffer */
		tid->hwq_depth++;

		/*
		 * Break out if ath_tx_form_aggr() indicated
		 * there can't be any further progress (eg BAW is full.)
		 * Checking for an empty txq is done above.
		 *
		 * XXX locking on txq here?
		 */
		if (txq->axq_aggr_depth >= sc->sc_hwq_limit ||
		    status == ATH_AGGR_BAW_CLOSED)
			break;
	}
}

/*
 * Schedule some packets from the given node/TID to the hardware.
 */
void
ath_tx_tid_hw_queue_norm(struct ath_softc *sc, struct ath_node *an,
    struct ath_tid *tid)
{
	struct ath_buf *bf;
	struct ath_txq *txq = sc->sc_ac2q[tid->ac];
	struct ieee80211_node *ni = &an->an_node;

	DPRINTF(sc, ATH_DEBUG_SW_TX, "%s: node %p: TID %d: called\n",
	    __func__, an, tid->tid);

	ATH_TXQ_LOCK_ASSERT(txq);

	/* Check - is AMPDU pending or running? then print out something */
	if (ath_tx_ampdu_pending(sc, an, tid->tid))
		device_printf(sc->sc_dev, "%s: tid=%d, ampdu pending?\n",
		    __func__, tid->tid);
	if (ath_tx_ampdu_running(sc, an, tid->tid))
		device_printf(sc->sc_dev, "%s: tid=%d, ampdu running?\n",
		    __func__, tid->tid);

	for (;;) {

		/*
		 * If the upper layers have paused the TID, don't
		 * queue any further packets.
		 */
		if (tid->paused)
			break;

		bf = TAILQ_FIRST(&tid->axq_q);
		if (bf == NULL) {
			break;
		}

		ATH_TXQ_REMOVE(tid, bf, bf_list);

		KASSERT(txq == bf->bf_state.bfs_txq, ("txqs not equal!\n"));

		/* Sanity check! */
		if (tid->tid != bf->bf_state.bfs_tid) {
			device_printf(sc->sc_dev, "%s: bfs_tid %d !="
			    " tid %d\n",
			    __func__, bf->bf_state.bfs_tid, tid->tid);
		}
		/* Normal completion handler */
		bf->bf_comp = ath_tx_normal_comp;

		/* Program descriptors + rate control */
		ath_tx_do_ratelookup(sc, bf);
		ath_tx_rate_fill_rcflags(sc, bf);
		ath_tx_set_rtscts(sc, bf);
		ath_tx_setds(sc, bf);
		ath_tx_chaindesclist(sc, bf);
		ath_tx_set_ratectrl(sc, ni, bf);

		/* Track outstanding buffer count to hardware */
		/* aggregates are "one" buffer */
		tid->hwq_depth++;

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
 * TID scheduling is implemented as a FIFO, with TIDs being
 * added to the end of the queue after some frames have been
 * scheduled.
 */
void
ath_txq_sched(struct ath_softc *sc, struct ath_txq *txq)
{
	struct ath_tid *tid, *next, *last;

	ATH_TXQ_LOCK_ASSERT(txq);

	/*
	 * Don't schedule if the hardware queue is busy.
	 * This (hopefully) gives some more time to aggregate
	 * some packets in the aggregation queue.
	 */
	if (txq->axq_aggr_depth >= sc->sc_hwq_limit) {
		sc->sc_aggr_stats.aggr_sched_nopkt++;
		return;
	}

	last = TAILQ_LAST(&txq->axq_tidq, axq_t_s);

	TAILQ_FOREACH_SAFE(tid, &txq->axq_tidq, axq_qelem, next) {
		/*
		 * Suspend paused queues here; they'll be resumed
		 * once the addba completes or times out.
		 */
		DPRINTF(sc, ATH_DEBUG_SW_TX, "%s: tid=%d, paused=%d\n",
		    __func__, tid->tid, tid->paused);
		ath_tx_tid_unsched(sc, tid);
		if (tid->paused) {
			continue;
		}
		if (ath_tx_ampdu_running(sc, tid->an, tid->tid))
			ath_tx_tid_hw_queue_aggr(sc, tid->an, tid);
		else
			ath_tx_tid_hw_queue_norm(sc, tid->an, tid);

		/* Not empty? Re-schedule */
		if (tid->axq_depth != 0)
			ath_tx_tid_sched(sc, tid);

		/* Give the software queue time to aggregate more packets */
		if (txq->axq_aggr_depth >= sc->sc_hwq_limit) {
			break;
		}

		/*
		 * If this was the last entry on the original list, stop.
		 * Otherwise nodes that have been rescheduled onto the end
		 * of the TID FIFO list will just keep being rescheduled.
		 */
		if (tid == last)
			break;
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
	 * XXX danger Will Robinson!
	 *
	 * Although the taskqueue may be running and scheduling some more
	 * packets, these should all be _before_ the addba sequence number.
	 * However, net80211 will keep self-assigning sequence numbers
	 * until addba has been negotiated.
	 *
	 * In the past, these packets would be "paused" (which still works
	 * fine, as they're being scheduled to the driver in the same
	 * serialised method which is calling the addba request routine)
	 * and when the aggregation session begins, they'll be dequeued
	 * as aggregate packets and added to the BAW. However, now there's
	 * a "bf->bf_state.bfs_dobaw" flag, and this isn't set for these
	 * packets. Thus they never get included in the BAW tracking and
	 * this can cause the initial burst of packets after the addba
	 * negotiation to "hang", as they quickly fall outside the BAW.
	 *
	 * The "eventual" solution should be to tag these packets with
	 * dobaw. Although net80211 has given us a sequence number,
	 * it'll be "after" the left edge of the BAW and thus it'll
	 * fall within it.
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

	ATH_TXQ_LOCK(sc->sc_ac2q[atid->ac]);
	/*
	 * XXX dirty!
	 * Slide the BAW left edge to wherever net80211 left it for us.
	 * Read above for more information.
	 */
	tap->txa_start = ni->ni_txseqs[tid];
	ath_tx_tid_resume(sc, atid);
	ATH_TXQ_UNLOCK(sc->sc_ac2q[atid->ac]);
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

	/*
	 * ath_tx_cleanup will resume the TID if possible, otherwise
	 * it'll set the cleanup flag, and it'll be unpaused once
	 * things have been cleaned up.
	 */
	ath_tx_cleanup(sc, an, tid);
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
	if (status == 0 || attempts == 50) {
		ATH_TXQ_LOCK(sc->sc_ac2q[atid->ac]);
		ath_tx_tid_resume(sc, atid);
		ATH_TXQ_UNLOCK(sc->sc_ac2q[atid->ac]);
	}
}

/*
 * This is called whenever the pending ADDBA request times out.
 * Unpause and reschedule the TID.
 */
void
ath_addba_response_timeout(struct ieee80211_node *ni,
    struct ieee80211_tx_ampdu *tap)
{
	struct ath_softc *sc = ni->ni_ic->ic_ifp->if_softc;
	int tid = WME_AC_TO_TID(tap->txa_ac);
	struct ath_node *an = ATH_NODE(ni);
	struct ath_tid *atid = &an->an_tid[tid];

	DPRINTF(sc, ATH_DEBUG_SW_TX_CTRL,
	    "%s: called; resuming\n", __func__);

	/* Note: This updates the aggregate state to (again) pending */
	sc->sc_addba_response_timeout(ni, tap);

	/* Unpause the TID; which reschedules it */
	ATH_TXQ_LOCK(sc->sc_ac2q[atid->ac]);
	ath_tx_tid_resume(sc, atid);
	ATH_TXQ_UNLOCK(sc->sc_ac2q[atid->ac]);
}
