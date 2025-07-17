/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2021 Adrian Chadd <adrian@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>

#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/smp.h>
#include <sys/queue.h>
#include <sys/taskqueue.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_media.h>
#include <net/ethernet.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/qcom_ess_edma/qcom_ess_edma_var.h>
#include <dev/qcom_ess_edma/qcom_ess_edma_reg.h>
#include <dev/qcom_ess_edma/qcom_ess_edma_hw.h>
#include <dev/qcom_ess_edma/qcom_ess_edma_desc.h>
#include <dev/qcom_ess_edma/qcom_ess_edma_rx.h>
#include <dev/qcom_ess_edma/qcom_ess_edma_tx.h>
#include <dev/qcom_ess_edma/qcom_ess_edma_debug.h>
#include <dev/qcom_ess_edma/qcom_ess_edma_gmac.h>

static int
qcom_ess_edma_probe(device_t dev)
{

	if (! ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_is_compatible(dev, "qcom,ess-edma") == 0)
		return (ENXIO);

	device_set_desc(dev,
	    "Qualcomm Atheros IPQ4018/IPQ4019 Ethernet driver");
	return (0);
}

static int
qcom_ess_edma_release_intr(struct qcom_ess_edma_softc *sc,
    struct qcom_ess_edma_intr *intr)
{

	if (intr->irq_res == NULL)
		return (0);

	if (intr->irq_intr != NULL)
		bus_teardown_intr(sc->sc_dev, intr->irq_res, intr->irq_intr);
	if (intr->irq_res != NULL)
		bus_release_resource(sc->sc_dev, SYS_RES_IRQ, intr->irq_rid,
		    intr->irq_res);

	return (0);
}

static void
qcom_ess_edma_tx_queue_xmit(struct qcom_ess_edma_softc *sc, int queue_id)
{
	struct qcom_ess_edma_tx_state *txs = &sc->sc_tx_state[queue_id];
	int n = 0;
	int ret;

	QCOM_ESS_EDMA_DPRINTF(sc, QCOM_ESS_EDMA_DBG_TX_TASK,
	    "%s: called; TX queue %d\n", __func__, queue_id);

	EDMA_RING_LOCK_ASSERT(&sc->sc_tx_ring[queue_id]);

	sc->sc_tx_ring[queue_id].stats.num_tx_xmit_defer++;

	(void) atomic_cmpset_int(&txs->enqueue_is_running, 1, 0);

	/* Don't do any work if the ring is empty */
	if (buf_ring_empty(txs->br))
		return;

	/*
	 * The ring isn't empty, dequeue frames and hand
	 * them to the hardware; defer updating the
	 * transmit ring pointer until we're done.
	 */
	while (! buf_ring_empty(txs->br)) {
		if_t ifp;
		struct qcom_ess_edma_gmac *gmac;
		struct mbuf *m;

		m = buf_ring_peek_clear_sc(txs->br);
		if (m == NULL)
			break;

		ifp = m->m_pkthdr.rcvif;
		gmac = if_getsoftc(ifp);

		/*
		 * The only way we'll know if we have space is to
		 * to try and transmit it.
		 */
		ret = qcom_ess_edma_tx_ring_frame(sc, queue_id, &m,
		    gmac->port_mask, gmac->vlan_id);
		if (ret == 0) {
			if_inc_counter(gmac->ifp, IFCOUNTER_OPACKETS, 1);
			buf_ring_advance_sc(txs->br);
		} else {
			/* Put whatever we tried to transmit back */
			if_inc_counter(gmac->ifp, IFCOUNTER_OERRORS, 1);
			buf_ring_putback_sc(txs->br, m);
			break;
		}
		n++;
	}

	/*
	 * Only push the updated descriptor ring stuff to the hardware
	 * if we actually queued something.
	 */
	if (n != 0)
		(void) qcom_ess_edma_tx_ring_frame_update(sc, queue_id);
}

/*
 * Enqueued when a deferred TX needs to happen.
 */
static void
qcom_ess_edma_tx_queue_xmit_task(void *arg, int npending)
{
	struct qcom_ess_edma_tx_state *txs = arg;
	struct qcom_ess_edma_softc *sc = txs->sc;

	QCOM_ESS_EDMA_DPRINTF(sc, QCOM_ESS_EDMA_DBG_INTERRUPT,
	    "%s: called; TX queue %d\n", __func__, txs->queue_id);

	EDMA_RING_LOCK(&sc->sc_tx_ring[txs->queue_id]);

	sc->sc_tx_ring[txs->queue_id].stats.num_tx_xmit_task++;
	qcom_ess_edma_tx_queue_xmit(sc, txs->queue_id);

	EDMA_RING_UNLOCK(&sc->sc_tx_ring[txs->queue_id]);
}

/*
 * Enqueued when a TX completion interrupt occurs.
 */
static void
qcom_ess_edma_tx_queue_complete_task(void *arg, int npending)
{
	struct qcom_ess_edma_tx_state *txs = arg;
	struct qcom_ess_edma_softc *sc = txs->sc;

	/* Transmit queue */
	QCOM_ESS_EDMA_DPRINTF(sc, QCOM_ESS_EDMA_DBG_INTERRUPT,
	    "%s: called; TX queue %d\n", __func__, txs->queue_id);

	EDMA_RING_LOCK(&sc->sc_tx_ring[txs->queue_id]);

	/*
	 * Complete/free tx mbufs.
	 */
	(void) qcom_ess_edma_tx_ring_complete(sc, txs->queue_id);

	/*
	 * ACK the interrupt.
	 */
	(void) qcom_ess_edma_hw_intr_tx_ack(sc, txs->queue_id);

	/*
	 * Re-enable the interrupt.
	 */
	(void) qcom_ess_edma_hw_intr_tx_intr_set_enable(sc, txs->queue_id,
	    true);

	/*
	 * Do any pending TX work if there's any buffers in the ring.
	 */
	if (! buf_ring_empty(txs->br))
		qcom_ess_edma_tx_queue_xmit(sc, txs->queue_id);

	EDMA_RING_UNLOCK(&sc->sc_tx_ring[txs->queue_id]);
}

static int
qcom_ess_edma_setup_tx_state(struct qcom_ess_edma_softc *sc, int txq, int cpu)
{
	struct qcom_ess_edma_tx_state *txs;
	struct qcom_ess_edma_desc_ring *ring;
	cpuset_t mask;

	txs = &sc->sc_tx_state[txq];
	ring = &sc->sc_tx_ring[txq];

	snprintf(txs->label, QCOM_ESS_EDMA_LABEL_SZ - 1, "txq%d_compl", txq);

	CPU_ZERO(&mask);
	CPU_SET(cpu, &mask);

	txs->queue_id = txq;
	txs->sc = sc;
	txs->completion_tq = taskqueue_create_fast(txs->label, M_NOWAIT,
	    taskqueue_thread_enqueue, &txs->completion_tq);
#if 0
	taskqueue_start_threads_cpuset(&txs->completion_tq, 1, PI_NET,
	    &mask, "%s", txs->label);
#else
	taskqueue_start_threads(&txs->completion_tq, 1, PI_NET,
	    "%s", txs->label);
#endif

	TASK_INIT(&txs->completion_task, 0,
	    qcom_ess_edma_tx_queue_complete_task, txs);
	TASK_INIT(&txs->xmit_task, 0,
	    qcom_ess_edma_tx_queue_xmit_task, txs);

	txs->br = buf_ring_alloc(EDMA_TX_BUFRING_SIZE, M_DEVBUF, M_WAITOK,
	    &ring->mtx);

	return (0);
}

/*
 * Free the transmit ring state.
 *
 * This assumes that the taskqueues have been drained and DMA has
 * stopped - all we're doing here is freeing the allocated resources.
 */
static int
qcom_ess_edma_free_tx_state(struct qcom_ess_edma_softc *sc, int txq)
{
	struct qcom_ess_edma_tx_state *txs;

	txs = &sc->sc_tx_state[txq];

	taskqueue_free(txs->completion_tq);

	while (! buf_ring_empty(txs->br)) {
		struct mbuf *m;

		m = buf_ring_dequeue_sc(txs->br);
		m_freem(m);
	}

	buf_ring_free(txs->br, M_DEVBUF);

	return (0);
}

static void
qcom_ess_edma_rx_queue_complete_task(void *arg, int npending)
{
	struct qcom_ess_edma_rx_state *rxs = arg;
	struct qcom_ess_edma_softc *sc = rxs->sc;
	struct mbufq mq;

	mbufq_init(&mq, EDMA_RX_RING_SIZE);

	/* Receive queue */
	QCOM_ESS_EDMA_DPRINTF(sc, QCOM_ESS_EDMA_DBG_INTERRUPT,
	    "%s: called; RX queue %d\n",
	    __func__, rxs->queue_id);

	EDMA_RING_LOCK(&sc->sc_rx_ring[rxs->queue_id]);

	/*
	 * Do receive work, get completed mbufs.
	 */
	(void) qcom_ess_edma_rx_ring_complete(sc, rxs->queue_id, &mq);

	/*
	 * ACK the interrupt.
	 */
	(void) qcom_ess_edma_hw_intr_rx_ack(sc, rxs->queue_id);

	/*
	 * Re-enable interrupt for this ring.
	 */
	(void) qcom_ess_edma_hw_intr_rx_intr_set_enable(sc, rxs->queue_id,
	    true);

	EDMA_RING_UNLOCK(&sc->sc_rx_ring[rxs->queue_id]);

	/* Push frames into networking stack */
	(void) qcom_ess_edma_gmac_receive_frames(sc, rxs->queue_id, &mq);
}

static int
qcom_ess_edma_setup_rx_state(struct qcom_ess_edma_softc *sc, int rxq, int cpu)
{
	struct qcom_ess_edma_rx_state *rxs;
	cpuset_t mask;

	rxs = &sc->sc_rx_state[rxq];

	snprintf(rxs->label, QCOM_ESS_EDMA_LABEL_SZ - 1, "rxq%d_compl", rxq);

	CPU_ZERO(&mask);
	CPU_SET(cpu, &mask);

	rxs->queue_id = rxq;
	rxs->sc = sc;
	rxs->completion_tq = taskqueue_create_fast(rxs->label, M_NOWAIT,
	    taskqueue_thread_enqueue, &rxs->completion_tq);
#if 0
	taskqueue_start_threads_cpuset(&rxs->completion_tq, 1, PI_NET,
	    &mask, "%s", rxs->label);
#else
	taskqueue_start_threads(&rxs->completion_tq, 1, PI_NET,
	    "%s", rxs->label);
#endif

	TASK_INIT(&rxs->completion_task, 0,
	    qcom_ess_edma_rx_queue_complete_task, rxs);
	return (0);
}

/*
 * Free the receive ring state.
 *
 * This assumes that the taskqueues have been drained and DMA has
 * stopped - all we're doing here is freeing the allocated resources.
 */

static int
qcom_ess_edma_free_rx_state(struct qcom_ess_edma_softc *sc, int rxq)
{
	struct qcom_ess_edma_rx_state *rxs;

	rxs = &sc->sc_rx_state[rxq];

	taskqueue_free(rxs->completion_tq);

	return (0);
}


static int
qcom_ess_edma_detach(device_t dev)
{
	struct qcom_ess_edma_softc *sc = device_get_softc(dev);
	int i;

	for (i = 0; i < QCOM_ESS_EDMA_NUM_TX_IRQS; i++) {
		(void) qcom_ess_edma_release_intr(sc, &sc->sc_tx_irq[i]);
	}
	for (i = 0; i < QCOM_ESS_EDMA_NUM_RX_IRQS; i++) {
		(void) qcom_ess_edma_release_intr(sc, &sc->sc_rx_irq[i]);
	}

	for (i = 0; i < QCOM_ESS_EDMA_NUM_TX_RINGS; i++) {
		(void) qcom_ess_edma_free_tx_state(sc, i);
		(void) qcom_ess_edma_tx_ring_clean(sc, &sc->sc_rx_ring[i]);
		(void) qcom_ess_edma_desc_ring_free(sc, &sc->sc_tx_ring[i]);
	}

	for (i = 0; i < QCOM_ESS_EDMA_NUM_RX_RINGS; i++) {
		(void) qcom_ess_edma_free_rx_state(sc, i);
		(void) qcom_ess_edma_rx_ring_clean(sc, &sc->sc_rx_ring[i]);
		(void) qcom_ess_edma_desc_ring_free(sc, &sc->sc_rx_ring[i]);
	}

	if (sc->sc_dma_tag) {
		bus_dma_tag_destroy(sc->sc_dma_tag);
		sc->sc_dma_tag = NULL;
	}

	if (sc->sc_mem_res)
		bus_release_resource(dev, SYS_RES_MEMORY, sc->sc_mem_rid,
		    sc->sc_mem_res);
	mtx_destroy(&sc->sc_mtx);

	return(0);
}

static int
qcom_ess_edma_filter(void *arg)
{
	struct qcom_ess_edma_intr *intr = arg;
	struct qcom_ess_edma_softc *sc = intr->sc;

	if (intr->irq_rid < QCOM_ESS_EDMA_NUM_TX_IRQS) {
		int tx_queue = intr->irq_rid;

		intr->stats.num_intr++;

		/*
		 * Disable the interrupt for this ring.
		 */
		(void) qcom_ess_edma_hw_intr_tx_intr_set_enable(sc, tx_queue,
		    false);

		/*
		 * Schedule taskqueue to run for this queue.
		 */
		taskqueue_enqueue(sc->sc_tx_state[tx_queue].completion_tq,
		    &sc->sc_tx_state[tx_queue].completion_task);

		return (FILTER_HANDLED);
	} else {
		int rx_queue = intr->irq_rid - QCOM_ESS_EDMA_NUM_TX_IRQS;

		intr->stats.num_intr++;

		/*
		 * Disable the interrupt for this ring.
		 */
		(void) qcom_ess_edma_hw_intr_rx_intr_set_enable(sc, rx_queue,
		    false);

		/*
		 * Schedule taskqueue to run for this queue.
		 */
		taskqueue_enqueue(sc->sc_rx_state[rx_queue].completion_tq,
		    &sc->sc_rx_state[rx_queue].completion_task);

		return (FILTER_HANDLED);
	}
}

static int
qcom_ess_edma_setup_intr(struct qcom_ess_edma_softc *sc,
    struct qcom_ess_edma_intr *intr, int rid, int cpu_id)
{

	QCOM_ESS_EDMA_DPRINTF(sc, QCOM_ESS_EDMA_DBG_INTERRUPT,
	    "%s: setting up interrupt id %d\n", __func__, rid);
	intr->sc = sc;
	intr->irq_rid = rid;
	intr->irq_res = bus_alloc_resource_any(sc->sc_dev,
	    SYS_RES_IRQ, &intr->irq_rid, RF_ACTIVE);
	if (intr->irq_res == NULL) {
		device_printf(sc->sc_dev,
		    "ERROR: couldn't allocate IRQ %d\n",
		    rid);
		return (ENXIO);
	}

	if ((bus_setup_intr(sc->sc_dev, intr->irq_res,
	    INTR_TYPE_NET | INTR_MPSAFE,
	    qcom_ess_edma_filter, NULL, intr,
	        &intr->irq_intr))) {
		device_printf(sc->sc_dev,
		    "ERROR: unable to register interrupt handler for"
		    " IRQ %d\n", rid);
		return (ENXIO);
	}

	/* If requested, bind the interrupt to the given CPU. */
	if (cpu_id != -1) {
		if (intr_bind_irq(sc->sc_dev, intr->irq_res, cpu_id) != 0) {
			device_printf(sc->sc_dev,
			    "ERROR: unable to bind IRQ %d to CPU %d\n",
			    rid, cpu_id);
		}
		/* Note: don't completely error out here */
	}

	return (0);
}

static int
qcom_ess_edma_sysctl_dump_state(SYSCTL_HANDLER_ARGS)
{
	struct qcom_ess_edma_softc *sc = arg1;
	int val = 0;
	int error;
	int i;

	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error || !req->newptr)
		return (error);
	if (val == 0)
		return (0);

	EDMA_LOCK(sc);
	for (i = 0; i < QCOM_ESS_EDMA_NUM_RX_RINGS; i++) {
		device_printf(sc->sc_dev,
		    "RXQ[%d]: prod=%u, cons=%u, hw prod=%u, hw cons=%u,"
		    " REG_SW_CONS_IDX=0x%08x\n",
		    i,
		    sc->sc_rx_ring[i].next_to_fill,
		    sc->sc_rx_ring[i].next_to_clean,
		    EDMA_REG_READ(sc,
		        EDMA_REG_RFD_IDX_Q(i)) & EDMA_RFD_PROD_IDX_BITS,
		    qcom_ess_edma_hw_rfd_get_cons_index(sc, i),
		    EDMA_REG_READ(sc, EDMA_REG_RX_SW_CONS_IDX_Q(i)));
	}

	for (i = 0; i < QCOM_ESS_EDMA_NUM_TX_RINGS; i++) {
		device_printf(sc->sc_dev,
		    "TXQ[%d]: prod=%u, cons=%u, hw prod=%u, hw cons=%u\n",
		    i,
		    sc->sc_tx_ring[i].next_to_fill,
		    sc->sc_tx_ring[i].next_to_clean,
		    (EDMA_REG_READ(sc, EDMA_REG_TPD_IDX_Q(i))
		      >> EDMA_TPD_CONS_IDX_SHIFT) & EDMA_TPD_CONS_IDX_MASK,
		    EDMA_REG_READ(sc, EDMA_REG_TX_SW_CONS_IDX_Q(i)));
	}

	device_printf(sc->sc_dev, "EDMA_REG_TXQ_CTRL=0x%08x\n",
	    EDMA_REG_READ(sc, EDMA_REG_TXQ_CTRL));
	device_printf(sc->sc_dev, "EDMA_REG_RXQ_CTRL=0x%08x\n",
	    EDMA_REG_READ(sc, EDMA_REG_RXQ_CTRL));
	device_printf(sc->sc_dev, "EDMA_REG_RX_DESC0=0x%08x\n",
	    EDMA_REG_READ(sc, EDMA_REG_RX_DESC0));
	device_printf(sc->sc_dev, "EDMA_REG_RX_DESC1=0x%08x\n",
	    EDMA_REG_READ(sc, EDMA_REG_RX_DESC1));
	device_printf(sc->sc_dev, "EDMA_REG_RX_ISR=0x%08x\n",
	    EDMA_REG_READ(sc, EDMA_REG_RX_ISR));
	device_printf(sc->sc_dev, "EDMA_REG_TX_ISR=0x%08x\n",
	    EDMA_REG_READ(sc, EDMA_REG_TX_ISR));
	device_printf(sc->sc_dev, "EDMA_REG_MISC_ISR=0x%08x\n",
	    EDMA_REG_READ(sc, EDMA_REG_MISC_ISR));
	device_printf(sc->sc_dev, "EDMA_REG_WOL_ISR=0x%08x\n",
	    EDMA_REG_READ(sc, EDMA_REG_WOL_ISR));

	EDMA_UNLOCK(sc);

	return (0);
}

static int
qcom_ess_edma_sysctl_dump_stats(SYSCTL_HANDLER_ARGS)
{
	struct qcom_ess_edma_softc *sc = arg1;
	int val = 0;
	int error;
	int i;

	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error || !req->newptr)
		return (error);
	if (val == 0)
		return (0);

	EDMA_LOCK(sc);
	for (i = 0; i < QCOM_ESS_EDMA_NUM_RX_RINGS; i++) {
		device_printf(sc->sc_dev,
		    "RXQ[%d]: num_added=%llu, num_cleaned=%llu,"
		    " num_dropped=%llu, num_enqueue_full=%llu,"
		    " num_rx_no_gmac=%llu, tx_mapfail=%llu,"
		    " num_tx_maxfrags=%llu, num_rx_ok=%llu\n",
		    i,
		    sc->sc_rx_ring[i].stats.num_added,
		    sc->sc_rx_ring[i].stats.num_cleaned,
		    sc->sc_rx_ring[i].stats.num_dropped,
		    sc->sc_rx_ring[i].stats.num_enqueue_full,
		    sc->sc_rx_ring[i].stats.num_rx_no_gmac,
		    sc->sc_rx_ring[i].stats.num_tx_mapfail,
		    sc->sc_rx_ring[i].stats.num_tx_maxfrags,
		    sc->sc_rx_ring[i].stats.num_rx_ok);
	}

	for (i = 0; i < QCOM_ESS_EDMA_NUM_TX_RINGS; i++) {
		device_printf(sc->sc_dev,
		    "TXQ[%d]: num_added=%llu, num_cleaned=%llu,"
		    " num_dropped=%llu, num_enqueue_full=%llu,"
		    " tx_mapfail=%llu, tx_complete=%llu, tx_xmit_defer=%llu,"
		    " tx_xmit_task=%llu, num_tx_maxfrags=%llu,"
		    " num_tx_ok=%llu\n",
		    i,
		    sc->sc_tx_ring[i].stats.num_added,
		    sc->sc_tx_ring[i].stats.num_cleaned,
		    sc->sc_tx_ring[i].stats.num_dropped,
		    sc->sc_tx_ring[i].stats.num_enqueue_full,
		    sc->sc_tx_ring[i].stats.num_tx_mapfail,
		    sc->sc_tx_ring[i].stats.num_tx_complete,
		    sc->sc_tx_ring[i].stats.num_tx_xmit_defer,
		    sc->sc_tx_ring[i].stats.num_tx_xmit_task,
		    sc->sc_tx_ring[i].stats.num_tx_maxfrags,
		    sc->sc_tx_ring[i].stats.num_tx_ok);
	}

	for (i = 0; i < QCOM_ESS_EDMA_NUM_RX_IRQS; i++) {
		device_printf(sc->sc_dev, "INTR_RXQ[%d]: num_intr=%llu\n",
		    i,
		    sc->sc_rx_irq[i].stats.num_intr);
	}

	for (i = 0; i < QCOM_ESS_EDMA_NUM_TX_IRQS; i++) {
		device_printf(sc->sc_dev, "INTR_TXQ[%d]: num_intr=%llu\n",
		    i,
		    sc->sc_tx_irq[i].stats.num_intr);
	}

	EDMA_UNLOCK(sc);

	return (0);
}


static int
qcom_ess_edma_sysctl_tx_intmit(SYSCTL_HANDLER_ARGS)
{
	struct qcom_ess_edma_softc *sc = arg1;
	uint32_t usec;
	int val = 0;
	int error;

	EDMA_LOCK(sc);
	(void) qcom_ess_edma_hw_get_tx_intr_moderation(sc, &usec);
	EDMA_UNLOCK(sc);

	val = usec;

	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error || !req->newptr)
		goto finish;

	EDMA_LOCK(sc);
	error = qcom_ess_edma_hw_set_tx_intr_moderation(sc, (uint32_t) val);
	EDMA_UNLOCK(sc);
finish:
	return error;
}


static int
qcom_ess_edma_attach_sysctl(struct qcom_ess_edma_softc *sc)
{
	struct sysctl_ctx_list *ctx = device_get_sysctl_ctx(sc->sc_dev);
	struct sysctl_oid *tree = device_get_sysctl_tree(sc->sc_dev);

	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "debug", CTLFLAG_RW, &sc->sc_debug, 0,
	    "debugging flags");

	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "state", CTLTYPE_INT | CTLFLAG_RW, sc,
	    0, qcom_ess_edma_sysctl_dump_state, "I", "");

	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "stats", CTLTYPE_INT | CTLFLAG_RW, sc,
	    0, qcom_ess_edma_sysctl_dump_stats, "I", "");

	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "tx_intmit", CTLTYPE_INT | CTLFLAG_RW, sc,
	    0, qcom_ess_edma_sysctl_tx_intmit, "I", "");

	return (0);
}

static int
qcom_ess_edma_attach(device_t dev)
{
	struct qcom_ess_edma_softc *sc = device_get_softc(dev);
	int i, ret;

	mtx_init(&sc->sc_mtx, device_get_nameunit(dev), NULL, MTX_DEF);

	sc->sc_dev = dev;
	sc->sc_debug = 0;

	(void) qcom_ess_edma_attach_sysctl(sc);

	/* Create parent DMA tag. */
	ret = bus_dma_tag_create(
	    bus_get_dma_tag(sc->sc_dev),	/* parent */
	    1, 0,				/* alignment, boundary */
	    BUS_SPACE_MAXADDR_32BIT,		/* lowaddr */
	    BUS_SPACE_MAXADDR,			/* highaddr */
	    NULL, NULL,				/* filter, filterarg */
	    BUS_SPACE_MAXSIZE_32BIT,		/* maxsize */
	    0,					/* nsegments */
	    BUS_SPACE_MAXSIZE_32BIT,		/* maxsegsize */
	    0,					/* flags */
	    NULL, NULL,				/* lockfunc, lockarg */
	    &sc->sc_dma_tag);
	if (ret != 0) {
		device_printf(sc->sc_dev,
		    "ERROR: failed to create parent DMA tag\n");
		goto error;
	}

	/* Map control/status registers. */
	sc->sc_mem_rid = 0;
	sc->sc_mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &sc->sc_mem_rid, RF_ACTIVE);

	if (sc->sc_mem_res == NULL) {
		device_printf(dev, "ERROR: couldn't map MMIO space\n");
		goto error;
	}

	sc->sc_mem_res_size = (size_t) bus_get_resource_count(dev,
	    SYS_RES_MEMORY, sc->sc_mem_rid);
	if (sc->sc_mem_res_size == 0) {
		device_printf(dev, "%s: failed to get device memory size\n",
		    __func__);
		goto error;
	}

	/*
	 * How many TX queues per CPU, for figuring out flowid/CPU
	 * mapping.
	 */
	sc->sc_config.num_tx_queue_per_cpu =
	    QCOM_ESS_EDMA_NUM_TX_RINGS / mp_ncpus;

	/* Allocate TX IRQs */
	for (i = 0; i < QCOM_ESS_EDMA_NUM_TX_IRQS; i++) {
		int cpu_id;

		/*
		 * The current mapping in the if_transmit() path
		 * will map mp_ncpu groups of flowids to the TXQs.
		 * So for a 4 CPU system the first four will be CPU 0,
		 * the second four will be CPU 1, etc.
		 */
		cpu_id = qcom_ess_edma_tx_queue_to_cpu(sc, i);
		if (qcom_ess_edma_setup_intr(sc, &sc->sc_tx_irq[i],
		    i, cpu_id) != 0)
			goto error;
		if (bootverbose)
			device_printf(sc->sc_dev,
			    "mapping TX IRQ %d to CPU %d\n",
			    i, cpu_id);
	}

	/* Allocate RX IRQs */
	for (i = 0; i < QCOM_ESS_EDMA_NUM_RX_IRQS; i++) {
		int cpu_id = qcom_ess_edma_rx_queue_to_cpu(sc, i);
		if (qcom_ess_edma_setup_intr(sc, &sc->sc_rx_irq[i],
		    i + QCOM_ESS_EDMA_NUM_TX_IRQS, cpu_id) != 0)
			goto error;
		if (bootverbose)
			device_printf(sc->sc_dev,
			    "mapping RX IRQ %d to CPU %d\n",
			    i, cpu_id);
	}

	/* Default receive frame size - before ETHER_ALIGN hack */
	sc->sc_config.rx_buf_size = 2048;
	sc->sc_config.rx_buf_ether_align = true;

	/* Default RSS paramters */
	sc->sc_config.rss_type =
	    EDMA_RSS_TYPE_IPV4TCP | EDMA_RSS_TYPE_IPV6_TCP
	    | EDMA_RSS_TYPE_IPV4_UDP | EDMA_RSS_TYPE_IPV6UDP
	    | EDMA_RSS_TYPE_IPV4 | EDMA_RSS_TYPE_IPV6;

	/* Default queue parameters */
	sc->sc_config.tx_ring_count = EDMA_TX_RING_SIZE;
	sc->sc_config.rx_ring_count = EDMA_RX_RING_SIZE;

	/* Default interrupt masks */
	sc->sc_config.rx_intr_mask = EDMA_RX_IMR_NORMAL_MASK;
	sc->sc_config.tx_intr_mask = EDMA_TX_IMR_NORMAL_MASK;
	sc->sc_state.misc_intr_mask = 0;
	sc->sc_state.wol_intr_mask = 0;
	sc->sc_state.intr_sw_idx_w = EDMA_INTR_SW_IDX_W_TYPE;

	/*
	 * Parse out the gmac count so we can start parsing out
	 * the gmac list and create us some ifnets.
	 */
	if (OF_getencprop(ofw_bus_get_node(dev), "qcom,num_gmac",
	    &sc->sc_config.num_gmac, sizeof(uint32_t)) > 0) {
		device_printf(sc->sc_dev, "Creating %d GMACs\n",
		    sc->sc_config.num_gmac);
	} else {
		device_printf(sc->sc_dev, "Defaulting to 1 GMAC\n");
		sc->sc_config.num_gmac = 1;
	}
	if (sc->sc_config.num_gmac > QCOM_ESS_EDMA_MAX_NUM_GMACS) {
		device_printf(sc->sc_dev, "Capping GMACs to %d\n",
		    QCOM_ESS_EDMA_MAX_NUM_GMACS);
		sc->sc_config.num_gmac = QCOM_ESS_EDMA_MAX_NUM_GMACS;
	}

	/*
	 * And now, create some gmac entries here; we'll create the
	 * ifnet's once this is all done.
	 */
	for (i = 0; i < sc->sc_config.num_gmac; i++) {
		ret = qcom_ess_edma_gmac_parse(sc, i);
		if (ret != 0) {
			device_printf(sc->sc_dev,
			    "Failed to parse gmac%d\n", i);
			goto error;
		}
	}

	/* allocate tx rings */
	for (i = 0; i < QCOM_ESS_EDMA_NUM_TX_RINGS; i++) {
		char label[QCOM_ESS_EDMA_LABEL_SZ];
		int cpu_id;

		snprintf(label, QCOM_ESS_EDMA_LABEL_SZ - 1, "tx_ring%d", i);
		if (qcom_ess_edma_desc_ring_setup(sc, &sc->sc_tx_ring[i],
		    label,
		    sc->sc_config.tx_ring_count,
		    sizeof(struct qcom_ess_edma_sw_desc_tx),
		    sizeof(struct qcom_ess_edma_tx_desc),
		    QCOM_ESS_EDMA_MAX_TXFRAGS,
		    ESS_EDMA_TX_BUFFER_ALIGN) != 0)
			goto error;
		if (qcom_ess_edma_tx_ring_setup(sc, &sc->sc_tx_ring[i]) != 0)
			goto error;

		/* Same CPU as the interrupts for now */
		cpu_id = qcom_ess_edma_tx_queue_to_cpu(sc, i);

		if (qcom_ess_edma_setup_tx_state(sc, i, cpu_id) != 0)
			goto error;
	}

	/* allocate rx rings */
	for (i = 0; i < QCOM_ESS_EDMA_NUM_RX_RINGS; i++) {
		char label[QCOM_ESS_EDMA_LABEL_SZ];
		int cpu_id;

		snprintf(label, QCOM_ESS_EDMA_LABEL_SZ - 1, "rx_ring%d", i);
		if (qcom_ess_edma_desc_ring_setup(sc, &sc->sc_rx_ring[i],
		    label,
		    sc->sc_config.rx_ring_count,
		    sizeof(struct qcom_ess_edma_sw_desc_rx),
		    sizeof(struct qcom_ess_edma_rx_free_desc),
		    1,
		    ESS_EDMA_RX_BUFFER_ALIGN) != 0)
			goto error;
		if (qcom_ess_edma_rx_ring_setup(sc, &sc->sc_rx_ring[i]) != 0)
			goto error;

		/* Same CPU as the interrupts for now */
		cpu_id = qcom_ess_edma_rx_queue_to_cpu(sc, i);

		if (qcom_ess_edma_setup_rx_state(sc, i, cpu_id) != 0)
			goto error;
	}

	/*
	 * map the gmac instances <-> port masks, so incoming frames know
	 * where they need to be forwarded to.
	 */
	for (i = 0; i < QCOM_ESS_EDMA_MAX_NUM_PORTS; i++)
		sc->sc_gmac_port_map[i] = -1;
	for (i = 0; i < sc->sc_config.num_gmac; i++) {
		ret = qcom_ess_edma_gmac_setup_port_mapping(sc, i);
		if (ret != 0) {
			device_printf(sc->sc_dev,
			    "Failed to setup port mpapping for gmac%d\n", i);
			goto error;
		}
	}


	/* Create ifnets */
	for (i = 0; i < sc->sc_config.num_gmac; i++) {
		ret = qcom_ess_edma_gmac_create_ifnet(sc, i);
		if (ret != 0) {
			device_printf(sc->sc_dev,
			    "Failed to create ifnet for gmac%d\n", i);
			goto error;
		}
	}

	/*
	 * NOTE: If there's no ess-switch / we're a single phy, we
	 * still need to reset the ess fabric to a fixed useful state.
	 * Otherwise we won't be able to pass packets to anything.
	 *
	 * Worry about this later.
	 */

	EDMA_LOCK(sc);

	/* disable all interrupts */
	ret = qcom_ess_edma_hw_intr_disable(sc);
	if (ret != 0) {
		device_printf(sc->sc_dev,
		    "Failed to disable interrupts (%d)\n",
		    ret);
		goto error_locked;
	}

	/* reset edma */
	ret = qcom_ess_edma_hw_stop(sc);

	/* fill RX ring here, explicitly */
	for (i = 0; i < QCOM_ESS_EDMA_NUM_RX_RINGS; i++) {
		EDMA_RING_LOCK(&sc->sc_rx_ring[i]);
		(void) qcom_ess_edma_rx_ring_fill(sc, i,
		    sc->sc_config.rx_ring_count);
		EDMA_RING_UNLOCK(&sc->sc_rx_ring[i]);
	}

	/* configure TX/RX rings; RSS config; initial interrupt rates, etc */
	ret = qcom_ess_edma_hw_setup(sc);
	ret = qcom_ess_edma_hw_setup_tx(sc);
	ret = qcom_ess_edma_hw_setup_rx(sc);
	ret = qcom_ess_edma_hw_setup_txrx_desc_rings(sc);

	/* setup rss indirection table */
	ret = qcom_ess_edma_hw_configure_rss_table(sc);

	/* setup load balancing table */
	ret = qcom_ess_edma_hw_configure_load_balance_table(sc);

	/* configure virtual queue */
	ret = qcom_ess_edma_hw_configure_tx_virtual_queue(sc);

	/* configure AXI burst max */
	ret = qcom_ess_edma_hw_configure_default_axi_transaction_size(sc);

	/* enable IRQs */
	ret = qcom_ess_edma_hw_intr_enable(sc);

	/* enable TX control */
	ret = qcom_ess_edma_hw_tx_enable(sc);

	/* enable RX control */
	ret = qcom_ess_edma_hw_rx_enable(sc);

	EDMA_UNLOCK(sc);

	return (0);

error_locked:
	EDMA_UNLOCK(sc);
error:
	qcom_ess_edma_detach(dev);
	return (ENXIO);
}

static device_method_t qcom_ess_edma_methods[] = {
	/* Driver */
	DEVMETHOD(device_probe, qcom_ess_edma_probe),
	DEVMETHOD(device_attach, qcom_ess_edma_attach),
	DEVMETHOD(device_detach, qcom_ess_edma_detach),

	{0, 0},
};

static driver_t qcom_ess_edma_driver = {
	"essedma",
	qcom_ess_edma_methods,
	sizeof(struct qcom_ess_edma_softc),
};

DRIVER_MODULE(qcom_ess_edma, simplebus, qcom_ess_edma_driver, NULL, 0);
DRIVER_MODULE(qcom_ess_edma, ofwbus, qcom_ess_edma_driver, NULL, 0);
MODULE_DEPEND(qcom_ess_edma, ether, 1, 1, 1);
MODULE_VERSION(qcom_ess_edma, 1);
