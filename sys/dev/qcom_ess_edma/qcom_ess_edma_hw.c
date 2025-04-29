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
#include <sys/socket.h>
#include <sys/sockio.h>

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
#include <dev/qcom_ess_edma/qcom_ess_edma_debug.h>

/*
 * Reset the ESS EDMA core.
 *
 * This is ... problematic.  There's only a single clock control
 * for the ESS core - and that includes both the EDMA (ethernet)
 * and switch hardware.
 *
 * AND, it's a placeholder for what the linux ess-edma driver
 * is doing directly to the ess core because in some instances
 * where there's a single PHY hooked up, it's possible that
 * ess-switch won't be initialised.  In that case it defaults
 * to a very minimal switch config.  Now, that's honestly pretty
 * bad, and instead we should be doing that kind of awareness
 * in ar40xx_switch.
 *
 * So, for now this is a big no-op, at least until everything
 * is implemented enough that I can get the switch/phy code and
 * this EDMA driver code to co-exist.
 */
int
qcom_ess_edma_hw_reset(struct qcom_ess_edma_softc *sc)
{

	EDMA_LOCK_ASSERT(sc);

	device_printf(sc->sc_dev, "%s: called, TODO!\n", __func__);

	/*
	 * This is where the linux ess-edma driver would reset the
	 * ESS core.
	 */

	/*
	 * and here's where the linux ess-edma driver would program
	 * in the initial port config, rgmii control, traffic
	 * port forwarding and broadcast/multicast traffic forwarding.
	 *
	 * instead, this should be done by the ar40xx_switch driver!
	 */

	return (0);
}

/*
 * Get the TX interrupt moderation timer.
 *
 * The resolution of this register is 2uS.
 */
int
qcom_ess_edma_hw_get_tx_intr_moderation(struct qcom_ess_edma_softc *sc,
    uint32_t *usec)
{
	uint32_t reg;

	EDMA_LOCK_ASSERT(sc);

	EDMA_REG_BARRIER_READ(sc);
	reg = EDMA_REG_READ(sc, EDMA_REG_IRQ_MODRT_TIMER_INIT);
	reg = reg >> EDMA_IRQ_MODRT_TX_TIMER_SHIFT;
	reg &= EDMA_IRQ_MODRT_TIMER_MASK;

	*usec = reg * 2;

	return (0);
}


/*
 * Set the TX interrupt moderation timer.
 *
 * The resolution of this register is 2uS.
 */
int
qcom_ess_edma_hw_set_tx_intr_moderation(struct qcom_ess_edma_softc *sc,
    uint32_t usec)
{
	uint32_t reg;

	usec = usec / 2;

	EDMA_LOCK_ASSERT(sc);

	EDMA_REG_BARRIER_READ(sc);
	reg = EDMA_REG_READ(sc, EDMA_REG_IRQ_MODRT_TIMER_INIT);
	reg &= ~(EDMA_IRQ_MODRT_TIMER_MASK << EDMA_IRQ_MODRT_TX_TIMER_SHIFT);
	reg |= (usec & EDMA_IRQ_MODRT_TIMER_MASK)
	    << EDMA_IRQ_MODRT_TX_TIMER_SHIFT;
	EDMA_REG_WRITE(sc, EDMA_REG_IRQ_MODRT_TIMER_INIT, reg);
	EDMA_REG_BARRIER_WRITE(sc);

	return (0);
}

/*
 * Set the RX interrupt moderation timer.
 *
 * The resolution of this register is 2uS.
 */
int
qcom_ess_edma_hw_set_rx_intr_moderation(struct qcom_ess_edma_softc *sc,
    uint32_t usec)
{
	uint32_t reg;

	EDMA_LOCK_ASSERT(sc);

	EDMA_REG_BARRIER_READ(sc);
	reg = EDMA_REG_READ(sc, EDMA_REG_IRQ_MODRT_TIMER_INIT);
	reg &= ~(EDMA_IRQ_MODRT_TIMER_MASK << EDMA_IRQ_MODRT_RX_TIMER_SHIFT);
	reg |= (usec & EDMA_IRQ_MODRT_TIMER_MASK)
	    << EDMA_IRQ_MODRT_RX_TIMER_SHIFT;
	EDMA_REG_WRITE(sc, EDMA_REG_IRQ_MODRT_TIMER_INIT, reg);
	EDMA_REG_BARRIER_WRITE(sc);

	return (0);
}

/*
 * Disable all interrupts.
 */
int
qcom_ess_edma_hw_intr_disable(struct qcom_ess_edma_softc *sc)
{
	int i;

	/* Disable TX interrupts */
	for (i = 0; i < QCOM_ESS_EDMA_NUM_TX_IRQS; i++) {
		EDMA_REG_WRITE(sc, EDMA_REG_TX_INT_MASK_Q(i), 0);
	}

	/* Disable RX interrupts */
	for (i = 0; i < QCOM_ESS_EDMA_NUM_RX_IRQS; i++) {
		EDMA_REG_WRITE(sc, EDMA_REG_RX_INT_MASK_Q(i), 0);
	}

	/* Disable misc/WOL interrupts */
	EDMA_REG_WRITE(sc, EDMA_REG_MISC_IMR, 0);
	EDMA_REG_WRITE(sc, EDMA_REG_WOL_IMR, 0);

	EDMA_REG_BARRIER_WRITE(sc);

	return (0);
}

/*
 * Enable/disable the given RX ring interrupt.
 */
int
qcom_ess_edma_hw_intr_rx_intr_set_enable(struct qcom_ess_edma_softc *sc,
    int rxq, bool state)
{
	EDMA_REG_WRITE(sc, EDMA_REG_RX_INT_MASK_Q(rxq), state ? 1 : 0);
	EDMA_REG_BARRIER_WRITE(sc);

	return (0);
}

/*
 * Enable/disable the given TX ring interrupt.
 */
int
qcom_ess_edma_hw_intr_tx_intr_set_enable(struct qcom_ess_edma_softc *sc,
    int txq, bool state)
{

	EDMA_REG_WRITE(sc, EDMA_REG_TX_INT_MASK_Q(txq), state ? 1 : 0);
	EDMA_REG_BARRIER_WRITE(sc);

	return (0);
}

/*
 * Enable interrupts.
 */
int
qcom_ess_edma_hw_intr_enable(struct qcom_ess_edma_softc *sc)
{
	int i;

	/* ACK, then Enable TX interrupts */
	EDMA_REG_WRITE(sc, EDMA_REG_TX_ISR, 0xffff);
	for (i = 0; i < QCOM_ESS_EDMA_NUM_TX_IRQS; i++) {
		EDMA_REG_WRITE(sc, EDMA_REG_TX_INT_MASK_Q(i),
		    sc->sc_config.tx_intr_mask);
	}

	/* ACK, then Enable RX interrupts */
	EDMA_REG_WRITE(sc, EDMA_REG_RX_ISR, 0xff);
	for (i = 0; i < QCOM_ESS_EDMA_NUM_RX_IRQS; i++) {
		EDMA_REG_WRITE(sc, EDMA_REG_RX_INT_MASK_Q(i),
		    sc->sc_config.rx_intr_mask);
	}

	/* Disable misc/WOL interrupts */
	EDMA_REG_WRITE(sc, EDMA_REG_MISC_IMR, 0);
	EDMA_REG_WRITE(sc, EDMA_REG_WOL_IMR, 0);

	EDMA_REG_BARRIER_WRITE(sc);

	return (0);
}

/*
 * Clear interrupt status.
 */
int
qcom_ess_edma_hw_intr_status_clear(struct qcom_ess_edma_softc *sc)
{

	EDMA_REG_WRITE(sc, EDMA_REG_RX_ISR, 0xff);
	EDMA_REG_WRITE(sc, EDMA_REG_TX_ISR, 0xffff);
	EDMA_REG_WRITE(sc, EDMA_REG_MISC_ISR, 0x1fff);
	EDMA_REG_WRITE(sc, EDMA_REG_WOL_ISR, 0x1);

	return (0);
}

/*
 * ACK the given RX queue ISR.
 *
 * Must be called with the RX ring lock held!
 */
int
qcom_ess_edma_hw_intr_rx_ack(struct qcom_ess_edma_softc *sc, int rx_queue)
{

	EDMA_RING_LOCK_ASSERT(&sc->sc_rx_ring[rx_queue]);
	EDMA_REG_WRITE(sc, EDMA_REG_RX_ISR, (1U << rx_queue));
	(void) EDMA_REG_READ(sc, EDMA_REG_RX_ISR);

	return (0);
}

/*
 * ACK the given TX queue ISR.
 *
 * Must be called with the TX ring lock held!
 */
int
qcom_ess_edma_hw_intr_tx_ack(struct qcom_ess_edma_softc *sc, int tx_queue)
{

	EDMA_RING_LOCK_ASSERT(&sc->sc_tx_ring[tx_queue]);
	EDMA_REG_WRITE(sc, EDMA_REG_TX_ISR, (1U << tx_queue));
	(void) EDMA_REG_READ(sc, EDMA_REG_TX_ISR);

	return (0);
}

/*
 * Configure the default RSS indirection table.
 */
int
qcom_ess_edma_hw_configure_rss_table(struct qcom_ess_edma_softc *sc)
{
	int i;

	/*
	 * The default IDT value configures the hash buckets
	 * to a repeating pattern of q0, q2, q4, q6.
	 */
	for (i = 0; i < EDMA_NUM_IDT; i++) {
		EDMA_REG_WRITE(sc, EDMA_REG_RSS_IDT(i), EDMA_RSS_IDT_VALUE);
	}
	EDMA_REG_BARRIER_WRITE(sc);

	return (0);
}

/*
 * Configure the default load balance mapping table.
 */
int
qcom_ess_edma_hw_configure_load_balance_table(struct qcom_ess_edma_softc *sc)
{

	/*
	 * I think this is mapping things to queues 0,2,4,6.
	 * Linux says it's 0,1,3,4 but that doesn't match the
	 * EDMA_LB_REG_VALUE field.
	 */
	EDMA_REG_WRITE(sc, EDMA_REG_LB_RING, EDMA_LB_REG_VALUE);
	EDMA_REG_BARRIER_WRITE(sc);
	return (0);
}

/*
 * Configure the default virtual tx ring queues.
 */
int
qcom_ess_edma_hw_configure_tx_virtual_queue(struct qcom_ess_edma_softc *sc)
{

	EDMA_REG_WRITE(sc, EDMA_REG_VQ_CTRL0, EDMA_VQ_REG_VALUE);
	EDMA_REG_WRITE(sc, EDMA_REG_VQ_CTRL1, EDMA_VQ_REG_VALUE);

	EDMA_REG_BARRIER_WRITE(sc);
	return (0);
}

/*
 * Configure the default maximum AXI bus transaction size.
 */
int
qcom_ess_edma_hw_configure_default_axi_transaction_size(
    struct qcom_ess_edma_softc *sc)
{

	EDMA_REG_WRITE(sc, EDMA_REG_AXIW_CTRL_MAXWRSIZE,
	    EDMA_AXIW_MAXWRSIZE_VALUE);
	return (0);
}

/*
 * Stop the TX/RX queues.
 */
int
qcom_ess_edma_hw_stop_txrx_queues(struct qcom_ess_edma_softc *sc)
{
	uint32_t reg;

	EDMA_REG_BARRIER_READ(sc);
	reg = EDMA_REG_READ(sc, EDMA_REG_RXQ_CTRL);
	reg &= ~EDMA_RXQ_CTRL_EN;
	EDMA_REG_WRITE(sc, EDMA_REG_RXQ_CTRL, reg);
	EDMA_REG_BARRIER_WRITE(sc);

	EDMA_REG_BARRIER_READ(sc);
	reg = EDMA_REG_READ(sc, EDMA_REG_TXQ_CTRL);
	reg &= ~EDMA_TXQ_CTRL_TXQ_EN;
	EDMA_REG_WRITE(sc, EDMA_REG_TXQ_CTRL, reg);
	EDMA_REG_BARRIER_WRITE(sc);
	return (0);
}

/*
 * Stop the EDMA block, disable interrupts.
 */
int
qcom_ess_edma_hw_stop(struct qcom_ess_edma_softc *sc)
{
	int ret;

	EDMA_LOCK_ASSERT(sc);

	ret = qcom_ess_edma_hw_intr_disable(sc);
	if (ret != 0) {
		QCOM_ESS_EDMA_DPRINTF(sc, QCOM_ESS_EDMA_DBG_INTERRUPT,
		    "%s: hw_intr_disable failed (%d)\n",
		    __func__,
		    ret);
	}

	ret = qcom_ess_edma_hw_intr_status_clear(sc);
	if (ret != 0) {
		QCOM_ESS_EDMA_DPRINTF(sc, QCOM_ESS_EDMA_DBG_INTERRUPT,
		    "%s: hw_intr_status_clear failed (%d)\n",
		    __func__,
		    ret);
	}

	ret = qcom_ess_edma_hw_stop_txrx_queues(sc);
	if (ret != 0) {
		QCOM_ESS_EDMA_DPRINTF(sc, QCOM_ESS_EDMA_DBG_INTERRUPT,
		    "%s: hw_stop_txrx_queues failed (%d)\n",
		    __func__,
		    ret);
	}

	return (0);
}

/*
 * Update the producer index for the given receive queue.
 *
 * Note: the RX ring lock must be held!
 *
 * Return 0 if OK, an error number if there's an error.
 */
int
qcom_ess_edma_hw_rfd_prod_index_update(struct qcom_ess_edma_softc *sc,
    int queue, int idx)
{
	uint32_t reg;

	EDMA_RING_LOCK_ASSERT(&sc->sc_rx_ring[queue]);

	QCOM_ESS_EDMA_DPRINTF(sc, QCOM_ESS_EDMA_DBG_RX_RING_MGMT,
	    "%s: called; q=%d idx=0x%x\n",
	    __func__, queue, idx);

	EDMA_REG_BARRIER_READ(sc);
	reg = EDMA_REG_READ(sc, EDMA_REG_RFD_IDX_Q(queue));
	QCOM_ESS_EDMA_DPRINTF(sc, QCOM_ESS_EDMA_DBG_RX_RING_MGMT,
	    "%s: q=%d reg was 0x%08x\n", __func__, queue, reg);
	reg &= ~EDMA_RFD_PROD_IDX_BITS;
	reg |= idx;
	EDMA_REG_WRITE(sc, EDMA_REG_RFD_IDX_Q(queue), reg);
	QCOM_ESS_EDMA_DPRINTF(sc, QCOM_ESS_EDMA_DBG_RX_RING_MGMT,
	    "%s: q=%d reg now 0x%08x\n", __func__, queue, reg);
	EDMA_REG_BARRIER_WRITE(sc);

	return (0);
}

/*
 * Fetch the consumer index for the given receive queue.
 *
 * Returns the current consumer index.
 *
 * Note - since it's used in statistics/debugging it isn't asserting the
 * RX ring lock, so be careful when/how you use this!
 */
int
qcom_ess_edma_hw_rfd_get_cons_index(struct qcom_ess_edma_softc *sc, int queue)
{
	uint32_t reg;

	EDMA_REG_BARRIER_READ(sc);
	reg = EDMA_REG_READ(sc, EDMA_REG_RFD_IDX_Q(queue));
	return (reg >> EDMA_RFD_CONS_IDX_SHIFT) & EDMA_RFD_CONS_IDX_MASK;
}

/*
 * Update the software consumed index to the hardware, so
 * it knows what we've read.
 *
 * Note: the RX ring lock must be held when calling this!
 *
 * Returns 0 if OK, error number if error.
 */
int
qcom_ess_edma_hw_rfd_sw_cons_index_update(struct qcom_ess_edma_softc *sc,
    int queue, int idx)
{
	EDMA_RING_LOCK_ASSERT(&sc->sc_rx_ring[queue]);

	EDMA_REG_WRITE(sc, EDMA_REG_RX_SW_CONS_IDX_Q(queue), idx);
	EDMA_REG_BARRIER_WRITE(sc);

	return (0);
}

/*
 * Setup initial hardware configuration.
 */
int
qcom_ess_edma_hw_setup(struct qcom_ess_edma_softc *sc)
{
	uint32_t reg;

	EDMA_LOCK_ASSERT(sc);

	EDMA_REG_BARRIER_READ(sc);
	reg = EDMA_REG_READ(sc, EDMA_REG_INTR_CTRL);
	reg &= ~(1 << EDMA_INTR_SW_IDX_W_TYP_SHIFT);
	reg |= sc->sc_state.intr_sw_idx_w << EDMA_INTR_SW_IDX_W_TYP_SHIFT;
	EDMA_REG_WRITE(sc, EDMA_REG_INTR_CTRL, reg);


	/* Clear wake-on-lan config */
	EDMA_REG_WRITE(sc, EDMA_REG_WOL_CTRL, 0);

	/* configure initial interrupt moderation config */
	reg = (EDMA_TX_IMT << EDMA_IRQ_MODRT_TX_TIMER_SHIFT);
	reg |= (EDMA_RX_IMT << EDMA_IRQ_MODRT_RX_TIMER_SHIFT);
	EDMA_REG_WRITE(sc, EDMA_REG_IRQ_MODRT_TIMER_INIT, reg);

	EDMA_REG_BARRIER_WRITE(sc);

	return (0);
}

/*
 * Setup TX DMA burst configuration.
 */
int
qcom_ess_edma_hw_setup_tx(struct qcom_ess_edma_softc *sc)
{
	uint32_t reg;

	EDMA_LOCK_ASSERT(sc);

	reg = (EDMA_TPD_BURST << EDMA_TXQ_NUM_TPD_BURST_SHIFT);
	reg |= EDMA_TXQ_CTRL_TPD_BURST_EN;
	reg |= (EDMA_TXF_BURST << EDMA_TXQ_TXF_BURST_NUM_SHIFT);
	EDMA_REG_WRITE(sc, EDMA_REG_TXQ_CTRL, reg);
	EDMA_REG_BARRIER_WRITE(sc);

	return (0);
}

/*
 * Setup default RSS, RX burst/prefetch/interrupt thresholds.
 *
 * Strip VLANs, those are offloaded in the RX descriptor.
 */
int
qcom_ess_edma_hw_setup_rx(struct qcom_ess_edma_softc *sc)
{
	uint32_t reg;

	EDMA_LOCK_ASSERT(sc);

	/* Configure RSS types */
	EDMA_REG_WRITE(sc, EDMA_REG_RSS_TYPE, sc->sc_config.rss_type);

	/* Configure RFD burst */
	reg = (EDMA_RFD_BURST << EDMA_RXQ_RFD_BURST_NUM_SHIFT);
	/* .. and RFD prefetch threshold */
	reg |= (EDMA_RFD_THR << EDMA_RXQ_RFD_PF_THRESH_SHIFT);
	/* ... and threshold to generate RFD interrupt */
	reg |= (EDMA_RFD_LTHR << EDMA_RXQ_RFD_LOW_THRESH_SHIFT);
	EDMA_REG_WRITE(sc, EDMA_REG_RX_DESC1, reg);

	/* Set RX FIFO threshold to begin DMAing data to host */
	reg = EDMA_FIFO_THRESH_128_BYTE;
	/* Remove VLANs (??) */
	reg |= EDMA_RXQ_CTRL_RMV_VLAN;
	EDMA_REG_WRITE(sc, EDMA_REG_RXQ_CTRL, reg);

	EDMA_REG_BARRIER_WRITE(sc);
	return (0);
}

/*
 * XXX TODO: this particular routine is a bit big and likely should be split
 * across main, hw, desc, rx and tx.  But to expedite initial bring-up,
 * let's just commit the sins here and get receive up and going.
 */
int
qcom_ess_edma_hw_setup_txrx_desc_rings(struct qcom_ess_edma_softc *sc)
{
	uint32_t reg, i, idx;
	int len;

	EDMA_LOCK_ASSERT(sc);

	/*
	 * setup base addresses for each transmit ring, and
	 * read in the initial index to use for transmit.
	 */
	for (i = 0; i < QCOM_ESS_EDMA_NUM_TX_RINGS; i++) {
		/* Descriptor ring based address */
		QCOM_ESS_EDMA_DPRINTF(sc, QCOM_ESS_EDMA_DBG_TX_RING_MGMT,
		    "TXQ[%d]: ring paddr=0x%08lx\n",
		    i, sc->sc_tx_ring[i].hw_desc_paddr);
		EDMA_REG_WRITE(sc, EDMA_REG_TPD_BASE_ADDR_Q(i),
		    sc->sc_tx_ring[i].hw_desc_paddr);

		/* And now, grab the consumer index */
		reg = EDMA_REG_READ(sc, EDMA_REG_TPD_IDX_Q(i));
		idx = (reg >> EDMA_TPD_CONS_IDX_SHIFT) & 0xffff;

		sc->sc_tx_ring[i].next_to_fill = idx;
		sc->sc_tx_ring[i].next_to_clean = idx;

		/* Update prod and sw consumer indexes */
		reg &= ~(EDMA_TPD_PROD_IDX_MASK << EDMA_TPD_PROD_IDX_SHIFT);
		reg |= idx;
		EDMA_REG_WRITE(sc, EDMA_REG_TPD_IDX_Q(i), reg);
		EDMA_REG_WRITE(sc, EDMA_REG_TX_SW_CONS_IDX_Q(i), idx);

		/* Set the ring size */
		EDMA_REG_WRITE(sc, EDMA_REG_TPD_RING_SIZE,
		    sc->sc_config.tx_ring_count & EDMA_TPD_RING_SIZE_MASK);

	}

	/* Set base addresses for each RFD ring */
	for (i = 0; i < QCOM_ESS_EDMA_NUM_RX_RINGS; i++) {
		QCOM_ESS_EDMA_DPRINTF(sc, QCOM_ESS_EDMA_DBG_RX_RING_MGMT,
		    "RXQ[%d]: ring paddr=0x%08lx\n",
		    i, sc->sc_rx_ring[i].hw_desc_paddr);
		EDMA_REG_WRITE(sc, EDMA_REG_RFD_BASE_ADDR_Q(i),
		    sc->sc_rx_ring[i].hw_desc_paddr);
	}
	EDMA_REG_BARRIER_WRITE(sc);

	/* Configure RX buffer size */
	len = sc->sc_config.rx_buf_size;
	if (sc->sc_config.rx_buf_ether_align)
		len -= ETHER_ALIGN;
	reg = (len & EDMA_RX_BUF_SIZE_MASK)
	    << EDMA_RX_BUF_SIZE_SHIFT;
	/* .. and RFD ring size */
	reg |= (sc->sc_config.rx_ring_count & EDMA_RFD_RING_SIZE_MASK)
	    << EDMA_RFD_RING_SIZE_SHIFT;
	EDMA_REG_WRITE(sc, EDMA_REG_RX_DESC0, reg);

	/* Disable the TX low/high watermark (for interrupts?) */
	EDMA_REG_WRITE(sc, EDMA_REG_TXF_WATER_MARK, 0);

	EDMA_REG_BARRIER_WRITE(sc);

	/* Load all the ring base addresses into the hardware */
	EDMA_REG_BARRIER_READ(sc);
	reg = EDMA_REG_READ(sc, EDMA_REG_TX_SRAM_PART);
	reg |= 1 << EDMA_LOAD_PTR_SHIFT;
	EDMA_REG_WRITE(sc, EDMA_REG_TX_SRAM_PART, reg);
	EDMA_REG_BARRIER_WRITE(sc);

	return (0);
}

/*
 * Enable general MAC TX DMA.
 */
int
qcom_ess_edma_hw_tx_enable(struct qcom_ess_edma_softc *sc)
{
	uint32_t reg;

	EDMA_LOCK_ASSERT(sc);

	EDMA_REG_BARRIER_READ(sc);
	reg = EDMA_REG_READ(sc, EDMA_REG_TXQ_CTRL);
	reg |= EDMA_TXQ_CTRL_TXQ_EN;
	EDMA_REG_WRITE(sc, EDMA_REG_TXQ_CTRL, reg);
	EDMA_REG_BARRIER_WRITE(sc);

	return (0);
}

/*
 * Enable general MAC RX DMA.
 */
int
qcom_ess_edma_hw_rx_enable(struct qcom_ess_edma_softc *sc)
{
	EDMA_LOCK_ASSERT(sc);
	uint32_t reg;

	EDMA_REG_BARRIER_READ(sc);
	reg = EDMA_REG_READ(sc, EDMA_REG_RXQ_CTRL);
	reg |= EDMA_RXQ_CTRL_EN;
	EDMA_REG_WRITE(sc, EDMA_REG_RXQ_CTRL, reg);
	EDMA_REG_BARRIER_WRITE(sc);

	return (0);
}

/*
 * Read the TPD consumer index register for the given transmit ring.
 */
int
qcom_ess_edma_hw_tx_read_tpd_cons_idx(struct qcom_ess_edma_softc *sc,
    int queue_id, uint16_t *idx)
{
	uint32_t reg;

	EDMA_REG_BARRIER_READ(sc);
	reg = EDMA_REG_READ(sc, EDMA_REG_TPD_IDX_Q(queue_id));
	*idx = (reg >> EDMA_TPD_CONS_IDX_SHIFT) & EDMA_TPD_CONS_IDX_MASK;

	return (0);
}

/*
 * Update the TPD producer index for the given transmit wring.
 */
int
qcom_ess_edma_hw_tx_update_tpd_prod_idx(struct qcom_ess_edma_softc *sc,
    int queue_id, uint16_t idx)
{
	uint32_t reg;

	EDMA_REG_BARRIER_READ(sc);
	reg = EDMA_REG_READ(sc, EDMA_REG_TPD_IDX_Q(queue_id));
	reg &= ~EDMA_TPD_PROD_IDX_BITS;
	reg |= (idx & EDMA_TPD_PROD_IDX_MASK) << EDMA_TPD_PROD_IDX_SHIFT;
	EDMA_REG_WRITE(sc, EDMA_REG_TPD_IDX_Q(queue_id), reg);
	EDMA_REG_BARRIER_WRITE(sc);

	return (0);
}

/*
 * Update the TPD software consumer index register for the given
 * transmit ring - ie, what software has cleaned.
 */
int
qcom_ess_edma_hw_tx_update_cons_idx(struct qcom_ess_edma_softc *sc,
    int queue_id, uint16_t idx)
{

	EDMA_REG_WRITE(sc, EDMA_REG_TX_SW_CONS_IDX_Q(queue_id), idx);
	EDMA_REG_BARRIER_WRITE(sc);

	return (0);
}
