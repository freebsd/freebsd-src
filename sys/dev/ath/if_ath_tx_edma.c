/*-
 * Copyright (c) 2012 Adrian Chadd <adrian@FreeBSD.org>
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
/*
 * This is needed for register operations which are performed
 * by the driver - eg, calls to ath_hal_gettsf32().
 *
 * It's also required for any AH_DEBUG checks in here, eg the
 * module dependencies.
 */
#include "opt_ah.h"
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
#include <sys/module.h>
#include <sys/ktr.h>
#include <sys/smp.h>	/* for mp_ncpus */

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

#include <net/bpf.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#endif

#include <dev/ath/if_athvar.h>
#include <dev/ath/ath_hal/ah_devid.h>		/* XXX for softled */
#include <dev/ath/ath_hal/ah_diagcodes.h>

#include <dev/ath/if_ath_debug.h>
#include <dev/ath/if_ath_misc.h>
#include <dev/ath/if_ath_tsf.h>
#include <dev/ath/if_ath_tx.h>
#include <dev/ath/if_ath_sysctl.h>
#include <dev/ath/if_ath_led.h>
#include <dev/ath/if_ath_keycache.h>
#include <dev/ath/if_ath_rx.h>
#include <dev/ath/if_ath_beacon.h>
#include <dev/ath/if_athdfs.h>

#ifdef ATH_TX99_DIAG
#include <dev/ath/ath_tx99/ath_tx99.h>
#endif

#include <dev/ath/if_ath_tx_edma.h>

/*
 * some general macros
 */
#define	INCR(_l, _sz)		(_l) ++; (_l) &= ((_sz) - 1)
#define	DECR(_l, _sz)		(_l) --; (_l) &= ((_sz) - 1)

/*
 * XXX doesn't belong here, and should be tunable
 */
#define	ATH_TXSTATUS_RING_SIZE	512

MALLOC_DECLARE(M_ATHDEV);

/*
 * Re-initialise the DMA FIFO with the current contents of
 * said TXQ.
 *
 * This should only be called as part of the chip reset path, as it
 * assumes the FIFO is currently empty.
 *
 * TODO: verify that a cold/warm reset does clear the TX FIFO, so
 * writing in a partially-filled FIFO will not cause double-entries
 * to appear.
 */
static void
ath_edma_dma_restart(struct ath_softc *sc, struct ath_txq *txq)
{

	device_printf(sc->sc_dev, "%s: called: txq=%p, qnum=%d\n",
	    __func__,
	    txq,
	    txq->axq_qnum);
}

/*
 * Hand off this frame to a hardware queue.
 *
 * Things are a bit hairy in the EDMA world.  The TX FIFO is only
 * 8 entries deep, so we need to keep track of exactly what we've
 * pushed into the FIFO and what's just sitting in the TX queue,
 * waiting to go out.
 *
 * So this is split into two halves - frames get appended to the
 * TXQ; then a scheduler is called to push some frames into the
 * actual TX FIFO.
 */
static void
ath_edma_xmit_handoff_hw(struct ath_softc *sc, struct ath_txq *txq,
    struct ath_buf *bf)
{
	struct ath_hal *ah = sc->sc_ah;

	ATH_TXQ_LOCK_ASSERT(txq);

	KASSERT((bf->bf_flags & ATH_BUF_BUSY) == 0,
	    ("%s: busy status 0x%x", __func__, bf->bf_flags));

	/*
	 * XXX TODO: write a hard-coded check to ensure that
	 * the queue id in the TX descriptor matches txq->axq_qnum.
	 */

	/* Update aggr stats */
	if (bf->bf_state.bfs_aggr)
		txq->axq_aggr_depth++;

	/* Push and update frame stats */
	ATH_TXQ_INSERT_TAIL(txq, bf, bf_list);

	/* Only schedule to the FIFO if there's space */
	if (txq->axq_fifo_depth < HAL_TXFIFO_DEPTH) {
		ath_hal_puttxbuf(ah, txq->axq_qnum, bf->bf_daddr);
		ath_hal_txstart(ah, txq->axq_qnum);
	}
}

/*
 * Hand off this frame to a multicast software queue.
 *
 * Unlike legacy DMA, this doesn't chain together frames via the
 * link pointer.  Instead, they're just added to the queue.
 * When it comes time to populate the CABQ, these frames should
 * be individually pushed into the FIFO as appropriate.
 *
 * Yes, this does mean that I'll eventually have to flesh out some
 * replacement code to handle populating the CABQ, rather than
 * what's done in ath_beacon_generate().  It'll have to push each
 * frame from the HW CABQ to the FIFO rather than just appending
 * it to the existing TXQ and kicking off DMA.
 */
static void
ath_edma_xmit_handoff_mcast(struct ath_softc *sc, struct ath_txq *txq,
    struct ath_buf *bf)
{

	ATH_TXQ_LOCK_ASSERT(txq);
	KASSERT((bf->bf_flags & ATH_BUF_BUSY) == 0,
	    ("%s: busy status 0x%x", __func__, bf->bf_flags));

	/*
	 * XXX this is mostly duplicated in ath_tx_handoff_mcast().
	 */
	if (ATH_TXQ_FIRST(txq) != NULL) {
		struct ath_buf *bf_last = ATH_TXQ_LAST(txq, axq_q_s);
		struct ieee80211_frame *wh;

		/* mark previous frame */
		wh = mtod(bf_last->bf_m, struct ieee80211_frame *);
		wh->i_fc[1] |= IEEE80211_FC1_MORE_DATA;

		/* sync descriptor to memory */
		bus_dmamap_sync(sc->sc_dmat, bf_last->bf_dmamap,
		   BUS_DMASYNC_PREWRITE);
	}

	ATH_TXQ_INSERT_TAIL(txq, bf, bf_list);
}

/*
 * Handoff this frame to the hardware.
 *
 * For the multicast queue, this will treat it as a software queue
 * and append it to the list, after updating the MORE_DATA flag
 * in the previous frame.  The cabq processing code will ensure
 * that the queue contents gets transferred over.
 *
 * For the hardware queues, this will queue a frame to the queue
 * like before, then populate the FIFO from that.  Since the
 * EDMA hardware has 8 FIFO slots per TXQ, this ensures that
 * frames such as management frames don't get prematurely dropped.
 *
 * This does imply that a similar flush-hwq-to-fifoq method will
 * need to be called from the processq function, before the
 * per-node software scheduler is called.
 */
static void
ath_edma_xmit_handoff(struct ath_softc *sc, struct ath_txq *txq,
    struct ath_buf *bf)
{

	ATH_TXQ_LOCK_ASSERT(txq);

	device_printf(sc->sc_dev, "%s: called; bf=%p, txq=%p, qnum=%d\n",
	    __func__,
	    bf,
	    txq,
	    txq->axq_qnum);

	if (txq->axq_qnum == ATH_TXQ_SWQ)
		ath_edma_xmit_handoff_mcast(sc, txq, bf);
	else
		ath_edma_xmit_handoff_hw(sc, txq, bf);

#if 0
	/*
	 * XXX For now this is a placeholder; free the buffer
	 * and inform the stack that the TX failed.
	 */
	ath_tx_default_comp(sc, bf, 1);
#endif
}

static int
ath_edma_setup_txfifo(struct ath_softc *sc, int qnum)
{
	struct ath_tx_edma_fifo *te = &sc->sc_txedma[qnum];

	te->m_fifo = malloc(sizeof(struct ath_buf *) * HAL_TXFIFO_DEPTH,
	    M_ATHDEV,
	    M_NOWAIT | M_ZERO);
	if (te->m_fifo == NULL) {
		device_printf(sc->sc_dev, "%s: malloc failed\n",
		    __func__);
		return (-ENOMEM);
	}

	/*
	 * Set initial "empty" state.
	 */
	te->m_fifo_head = te->m_fifo_tail = te->m_fifo_depth = 0;
	
	return (0);
}

static int
ath_edma_free_txfifo(struct ath_softc *sc, int qnum)
{
	struct ath_tx_edma_fifo *te = &sc->sc_txedma[qnum];

	/* XXX TODO: actually deref the ath_buf entries? */
	free(te->m_fifo, M_ATHDEV);
	return (0);
}

static int
ath_edma_dma_txsetup(struct ath_softc *sc)
{
	int error;
	int i;

	error = ath_descdma_alloc_desc(sc, &sc->sc_txsdma,
	    NULL, "txcomp", sc->sc_tx_statuslen, ATH_TXSTATUS_RING_SIZE);
	if (error != 0)
		return (error);

	ath_hal_setuptxstatusring(sc->sc_ah,
	    (void *) sc->sc_txsdma.dd_desc,
	    sc->sc_txsdma.dd_desc_paddr,
	    ATH_TXSTATUS_RING_SIZE);

	for (i = 0; i < HAL_NUM_TX_QUEUES; i++) {
		ath_edma_setup_txfifo(sc, i);
	}


	return (0);
}

static int
ath_edma_dma_txteardown(struct ath_softc *sc)
{
	int i;

	for (i = 0; i < HAL_NUM_TX_QUEUES; i++) {
		ath_edma_free_txfifo(sc, i);
	}

	ath_descdma_cleanup(sc, &sc->sc_txsdma, NULL);
	return (0);
}

/*
 * Drain all TXQs, potentially after completing the existing completed
 * frames.
 */
static void
ath_edma_tx_drain(struct ath_softc *sc, ATH_RESET_TYPE reset_type)
{

	device_printf(sc->sc_dev, "%s: called\n", __func__);
}

/*
 * Process the TX status queue.
 */
static void
ath_edma_tx_proc(void *arg, int npending)
{
	struct ath_softc *sc = (struct ath_softc *) arg;
	struct ath_hal *ah = sc->sc_ah;
	HAL_STATUS status;
	struct ath_tx_status ts;
	struct ath_txq *txq;

	device_printf(sc->sc_dev, "%s: called, npending=%d\n",
	    __func__, npending);

	for (;;) {
		ATH_TXSTATUS_LOCK(sc);
		status = ath_hal_txprocdesc(ah, NULL, (void *) &ts);
		ATH_TXSTATUS_UNLOCK(sc);

		if (status != HAL_OK)
			break;

		/*
		 * At this point we have a valid status descriptor.
		 * The QID and descriptor ID (which currently isn't set)
		 * is part of the status.
		 *
		 * We then assume that the descriptor in question is the
		 * -head- of the given QID.  Eventually we should verify
		 * this by using the descriptor ID.
		 */
		device_printf(sc->sc_dev, "%s: qcuid=%d\n",
		    __func__,
		    ts.ts_queue_id);

		txq = &sc->sc_txq[ts.ts_queue_id];
	}
}

static void
ath_edma_attach_comp_func(struct ath_softc *sc)
{

	TASK_INIT(&sc->sc_txtask, 0, ath_edma_tx_proc, sc);
}

void
ath_xmit_setup_edma(struct ath_softc *sc)
{

	/* Fetch EDMA field and buffer sizes */
	(void) ath_hal_gettxdesclen(sc->sc_ah, &sc->sc_tx_desclen);
	(void) ath_hal_gettxstatuslen(sc->sc_ah, &sc->sc_tx_statuslen);
	(void) ath_hal_getntxmaps(sc->sc_ah, &sc->sc_tx_nmaps);

	device_printf(sc->sc_dev, "TX descriptor length: %d\n",
	    sc->sc_tx_desclen);
	device_printf(sc->sc_dev, "TX status length: %d\n",
	    sc->sc_tx_statuslen);
	device_printf(sc->sc_dev, "TX buffers per descriptor: %d\n",
	    sc->sc_tx_nmaps);

	sc->sc_tx.xmit_setup = ath_edma_dma_txsetup;
	sc->sc_tx.xmit_teardown = ath_edma_dma_txteardown;
	sc->sc_tx.xmit_attach_comp_func = ath_edma_attach_comp_func;

	sc->sc_tx.xmit_dma_restart = ath_edma_dma_restart;
	sc->sc_tx.xmit_handoff = ath_edma_xmit_handoff;
	sc->sc_tx.xmit_drain = ath_edma_tx_drain;
}
