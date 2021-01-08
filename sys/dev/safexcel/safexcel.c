/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2020 Rubicon Communications, LLC (Netgate)
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

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <sys/sglist.h>
#include <sys/sysctl.h>

#include <machine/atomic.h>
#include <machine/bus.h>

#include <crypto/rijndael/rijndael.h>
#include <opencrypto/cryptodev.h>
#include <opencrypto/xform.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "cryptodev_if.h"

#include "safexcel_reg.h"
#include "safexcel_var.h"

static MALLOC_DEFINE(M_SAFEXCEL, "safexcel_req", "safexcel request buffers");

/*
 * We only support the EIP97 for now.
 */
static struct ofw_compat_data safexcel_compat[] = {
	{ "inside-secure,safexcel-eip97ies",	(uintptr_t)97 },
	{ "inside-secure,safexcel-eip97",	(uintptr_t)97 },
	{ NULL,					0 }
};

const struct safexcel_reg_offsets eip97_regs_offset = {
	.hia_aic	= SAFEXCEL_EIP97_HIA_AIC_BASE,
	.hia_aic_g	= SAFEXCEL_EIP97_HIA_AIC_G_BASE,
	.hia_aic_r	= SAFEXCEL_EIP97_HIA_AIC_R_BASE,
	.hia_aic_xdr	= SAFEXCEL_EIP97_HIA_AIC_xDR_BASE,
	.hia_dfe	= SAFEXCEL_EIP97_HIA_DFE_BASE,
	.hia_dfe_thr	= SAFEXCEL_EIP97_HIA_DFE_THR_BASE,
	.hia_dse	= SAFEXCEL_EIP97_HIA_DSE_BASE,
	.hia_dse_thr	= SAFEXCEL_EIP97_HIA_DSE_THR_BASE,
	.hia_gen_cfg	= SAFEXCEL_EIP97_HIA_GEN_CFG_BASE,
	.pe		= SAFEXCEL_EIP97_PE_BASE,
};

const struct safexcel_reg_offsets eip197_regs_offset = {
	.hia_aic	= SAFEXCEL_EIP197_HIA_AIC_BASE,
	.hia_aic_g	= SAFEXCEL_EIP197_HIA_AIC_G_BASE,
	.hia_aic_r	= SAFEXCEL_EIP197_HIA_AIC_R_BASE,
	.hia_aic_xdr	= SAFEXCEL_EIP197_HIA_AIC_xDR_BASE,
	.hia_dfe	= SAFEXCEL_EIP197_HIA_DFE_BASE,
	.hia_dfe_thr	= SAFEXCEL_EIP197_HIA_DFE_THR_BASE,
	.hia_dse	= SAFEXCEL_EIP197_HIA_DSE_BASE,
	.hia_dse_thr	= SAFEXCEL_EIP197_HIA_DSE_THR_BASE,
	.hia_gen_cfg	= SAFEXCEL_EIP197_HIA_GEN_CFG_BASE,
	.pe		= SAFEXCEL_EIP197_PE_BASE,
};

static struct safexcel_cmd_descr *
safexcel_cmd_descr_next(struct safexcel_cmd_descr_ring *ring)
{
	struct safexcel_cmd_descr *cdesc;

	if (ring->write == ring->read)
		return (NULL);
	cdesc = &ring->desc[ring->read];
	ring->read = (ring->read + 1) % SAFEXCEL_RING_SIZE;
	return (cdesc);
}

static struct safexcel_res_descr *
safexcel_res_descr_next(struct safexcel_res_descr_ring *ring)
{
	struct safexcel_res_descr *rdesc;

	if (ring->write == ring->read)
		return (NULL);
	rdesc = &ring->desc[ring->read];
	ring->read = (ring->read + 1) % SAFEXCEL_RING_SIZE;
	return (rdesc);
}

static struct safexcel_request *
safexcel_alloc_request(struct safexcel_softc *sc, struct safexcel_ring *ring)
{
	struct safexcel_request *req;

	mtx_assert(&ring->mtx, MA_OWNED);

	if ((req = STAILQ_FIRST(&ring->free_requests)) != NULL)
		STAILQ_REMOVE_HEAD(&ring->free_requests, link);
	return (req);
}

static void
safexcel_free_request(struct safexcel_ring *ring, struct safexcel_request *req)
{
	struct safexcel_context_record *ctx;

	mtx_assert(&ring->mtx, MA_OWNED);

	if (req->dmap_loaded) {
		bus_dmamap_unload(ring->data_dtag, req->dmap);
		req->dmap_loaded = false;
	}
	ctx = (struct safexcel_context_record *)req->ctx.vaddr;
	explicit_bzero(ctx->data, sizeof(ctx->data));
	explicit_bzero(req->iv, sizeof(req->iv));
	STAILQ_INSERT_TAIL(&ring->free_requests, req, link);
}

static void
safexcel_enqueue_request(struct safexcel_softc *sc, struct safexcel_ring *ring,
    struct safexcel_request *req)
{
	mtx_assert(&ring->mtx, MA_OWNED);

	STAILQ_INSERT_TAIL(&ring->ready_requests, req, link);
}

static void
safexcel_rdr_intr(struct safexcel_softc *sc, int ringidx)
{
	struct safexcel_cmd_descr *cdesc;
	struct safexcel_res_descr *rdesc;
	struct safexcel_request *req;
	struct safexcel_ring *ring;
	uint32_t blocked, error, i, ncdescs, nrdescs, nreqs;

	blocked = 0;
	ring = &sc->sc_ring[ringidx];

	mtx_lock(&ring->mtx);
	nreqs = SAFEXCEL_READ(sc,
	    SAFEXCEL_HIA_RDR(sc, ringidx) + SAFEXCEL_HIA_xDR_PROC_COUNT);
	nreqs >>= SAFEXCEL_xDR_PROC_xD_PKT_OFFSET;
	nreqs &= SAFEXCEL_xDR_PROC_xD_PKT_MASK;
	if (nreqs == 0) {
		SAFEXCEL_DPRINTF(sc, 1,
		    "zero pending requests on ring %d\n", ringidx);
		goto out;
	}

	ring = &sc->sc_ring[ringidx];
	bus_dmamap_sync(ring->rdr.dma.tag, ring->rdr.dma.map,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
	bus_dmamap_sync(ring->cdr.dma.tag, ring->cdr.dma.map,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
	bus_dmamap_sync(ring->dma_atok.tag, ring->dma_atok.map,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	ncdescs = nrdescs = 0;
	for (i = 0; i < nreqs; i++) {
		req = STAILQ_FIRST(&ring->queued_requests);
		KASSERT(req != NULL, ("%s: expected %d pending requests",
		    __func__, nreqs));
                STAILQ_REMOVE_HEAD(&ring->queued_requests, link);
		mtx_unlock(&ring->mtx);

		bus_dmamap_sync(req->ctx.tag, req->ctx.map,
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
		bus_dmamap_sync(ring->data_dtag, req->dmap,
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

		ncdescs += req->cdescs;
		while (req->cdescs-- > 0) {
			cdesc = safexcel_cmd_descr_next(&ring->cdr);
			KASSERT(cdesc != NULL,
			    ("%s: missing control descriptor", __func__));
			if (req->cdescs == 0)
				KASSERT(cdesc->last_seg,
				    ("%s: chain is not terminated", __func__));
		}
		nrdescs += req->rdescs;
		while (req->rdescs-- > 0) {
			rdesc = safexcel_res_descr_next(&ring->rdr);
			error = rdesc->result_data.error_code;
			if (error != 0) {
				if (error == SAFEXCEL_RESULT_ERR_AUTH_FAILED &&
				    req->crp->crp_etype == 0) {
					req->crp->crp_etype = EBADMSG;
				} else {
					SAFEXCEL_DPRINTF(sc, 1,
					    "error code %#x\n", error);
					req->crp->crp_etype = EIO;
				}
			}
		}

		crypto_done(req->crp);
		mtx_lock(&ring->mtx);
		safexcel_free_request(ring, req);
	}

	if (nreqs != 0) {
		SAFEXCEL_WRITE(sc,
		    SAFEXCEL_HIA_RDR(sc, ringidx) + SAFEXCEL_HIA_xDR_PROC_COUNT,
		    SAFEXCEL_xDR_PROC_xD_PKT(nreqs) |
		    (sc->sc_config.rd_offset * nrdescs * sizeof(uint32_t)));
		blocked = ring->blocked;
		ring->blocked = 0;
	}
out:
	if (!STAILQ_EMPTY(&ring->queued_requests)) {
		SAFEXCEL_WRITE(sc,
		    SAFEXCEL_HIA_RDR(sc, ringidx) + SAFEXCEL_HIA_xDR_THRESH,
		    SAFEXCEL_HIA_CDR_THRESH_PKT_MODE | 1);
	}
	mtx_unlock(&ring->mtx);

	if (blocked)
		crypto_unblock(sc->sc_cid, blocked);
}

static void
safexcel_ring_intr(void *arg)
{
	struct safexcel_softc *sc;
	struct safexcel_intr_handle *ih;
	uint32_t status, stat;
	int ring;
	bool rdrpending;

	ih = arg;
	sc = ih->sc;
	ring = ih->ring;

	status = SAFEXCEL_READ(sc, SAFEXCEL_HIA_AIC_R(sc) +
	    SAFEXCEL_HIA_AIC_R_ENABLED_STAT(ring));
	/* CDR interrupts */
	if (status & SAFEXCEL_CDR_IRQ(ring)) {
		stat = SAFEXCEL_READ(sc,
		    SAFEXCEL_HIA_CDR(sc, ring) + SAFEXCEL_HIA_xDR_STAT);
		SAFEXCEL_WRITE(sc,
		    SAFEXCEL_HIA_CDR(sc, ring) + SAFEXCEL_HIA_xDR_STAT,
		    stat & SAFEXCEL_CDR_INTR_MASK);
	}
	/* RDR interrupts */
	rdrpending = false;
	if (status & SAFEXCEL_RDR_IRQ(ring)) {
		stat = SAFEXCEL_READ(sc,
		    SAFEXCEL_HIA_RDR(sc, ring) + SAFEXCEL_HIA_xDR_STAT);
		if ((stat & SAFEXCEL_xDR_ERR) == 0)
			rdrpending = true;
		SAFEXCEL_WRITE(sc,
		    SAFEXCEL_HIA_RDR(sc, ring) + SAFEXCEL_HIA_xDR_STAT,
		    stat & SAFEXCEL_RDR_INTR_MASK);
	}
	SAFEXCEL_WRITE(sc,
	    SAFEXCEL_HIA_AIC_R(sc) + SAFEXCEL_HIA_AIC_R_ACK(ring),
	    status);

	if (rdrpending)
		safexcel_rdr_intr(sc, ring);
}

static int
safexcel_configure(struct safexcel_softc *sc)
{
	uint32_t i, mask, pemask, reg;
	device_t dev;

	if (sc->sc_type == 197) {
		sc->sc_offsets = eip197_regs_offset;
		pemask = SAFEXCEL_N_PES_MASK;
	} else {
		sc->sc_offsets = eip97_regs_offset;
		pemask = EIP97_N_PES_MASK;
	}

	dev = sc->sc_dev;

	/* Scan for valid ring interrupt controllers. */
	for (i = 0; i < SAFEXCEL_MAX_RING_AIC; i++) {
		reg = SAFEXCEL_READ(sc, SAFEXCEL_HIA_AIC_R(sc) +
		    SAFEXCEL_HIA_AIC_R_VERSION(i));
		if (SAFEXCEL_REG_LO16(reg) != EIP201_VERSION_LE)
			break;
	}
	sc->sc_config.aic_rings = i;
	if (sc->sc_config.aic_rings == 0)
		return (-1);

	reg = SAFEXCEL_READ(sc, SAFEXCEL_HIA_AIC_G(sc) + SAFEXCEL_HIA_OPTIONS);
	/* Check for 64bit addressing. */
	if ((reg & SAFEXCEL_OPT_ADDR_64) == 0)
		return (-1);
	/* Check alignment constraints (which we do not support). */
	if (((reg & SAFEXCEL_OPT_TGT_ALIGN_MASK) >>
	    SAFEXCEL_OPT_TGT_ALIGN_OFFSET) != 0)
		return (-1);

	sc->sc_config.hdw =
	    (reg & SAFEXCEL_xDR_HDW_MASK) >> SAFEXCEL_xDR_HDW_OFFSET;
	mask = (1 << sc->sc_config.hdw) - 1;

	sc->sc_config.rings = reg & SAFEXCEL_N_RINGS_MASK;
	/* Limit the number of rings to the number of the AIC Rings. */
	sc->sc_config.rings = MIN(sc->sc_config.rings, sc->sc_config.aic_rings);

	sc->sc_config.pes = (reg & pemask) >> SAFEXCEL_N_PES_OFFSET;

	sc->sc_config.cd_size =
	    sizeof(struct safexcel_cmd_descr) / sizeof(uint32_t);
	sc->sc_config.cd_offset = (sc->sc_config.cd_size + mask) & ~mask;

	sc->sc_config.rd_size =
	    sizeof(struct safexcel_res_descr) / sizeof(uint32_t);
	sc->sc_config.rd_offset = (sc->sc_config.rd_size + mask) & ~mask;

	sc->sc_config.atok_offset =
	    (SAFEXCEL_MAX_ATOKENS * sizeof(struct safexcel_instr) + mask) &
	    ~mask;

	return (0);
}

static void
safexcel_init_hia_bus_access(struct safexcel_softc *sc)
{
	uint32_t version, val;

	/* Determine endianness and configure byte swap. */
	version = SAFEXCEL_READ(sc,
	    SAFEXCEL_HIA_AIC(sc) + SAFEXCEL_HIA_VERSION);
	val = SAFEXCEL_READ(sc, SAFEXCEL_HIA_AIC(sc) + SAFEXCEL_HIA_MST_CTRL);
	if (SAFEXCEL_REG_HI16(version) == SAFEXCEL_HIA_VERSION_BE) {
		val = SAFEXCEL_READ(sc,
		    SAFEXCEL_HIA_AIC(sc) + SAFEXCEL_HIA_MST_CTRL);
		val = val ^ (SAFEXCEL_MST_CTRL_NO_BYTE_SWAP >> 24);
		SAFEXCEL_WRITE(sc,
		    SAFEXCEL_HIA_AIC(sc) + SAFEXCEL_HIA_MST_CTRL,
		    val);
	}

	/* Configure wr/rd cache values. */
	SAFEXCEL_WRITE(sc, SAFEXCEL_HIA_GEN_CFG(sc) + SAFEXCEL_HIA_MST_CTRL,
	    SAFEXCEL_MST_CTRL_RD_CACHE(RD_CACHE_4BITS) |
	    SAFEXCEL_MST_CTRL_WD_CACHE(WR_CACHE_4BITS));
}

static void
safexcel_disable_global_interrupts(struct safexcel_softc *sc)
{
	/* Disable and clear pending interrupts. */
	SAFEXCEL_WRITE(sc,
	    SAFEXCEL_HIA_AIC_G(sc) + SAFEXCEL_HIA_AIC_G_ENABLE_CTRL, 0);
	SAFEXCEL_WRITE(sc,
	    SAFEXCEL_HIA_AIC_G(sc) + SAFEXCEL_HIA_AIC_G_ACK,
	    SAFEXCEL_AIC_G_ACK_ALL_MASK);
}

/*
 * Configure the data fetch engine.  This component parses command descriptors
 * and sets up DMA transfers from host memory to the corresponding processing
 * engine.
 */
static void
safexcel_configure_dfe_engine(struct safexcel_softc *sc, int pe)
{
	/* Reset all DFE threads. */
	SAFEXCEL_WRITE(sc,
	    SAFEXCEL_HIA_DFE_THR(sc) + SAFEXCEL_HIA_DFE_THR_CTRL(pe),
	    SAFEXCEL_DxE_THR_CTRL_RESET_PE);

	/* Deassert the DFE reset. */
	SAFEXCEL_WRITE(sc,
	    SAFEXCEL_HIA_DFE_THR(sc) + SAFEXCEL_HIA_DFE_THR_CTRL(pe), 0);

	/* DMA transfer size to use. */
	SAFEXCEL_WRITE(sc, SAFEXCEL_HIA_DFE(sc) + SAFEXCEL_HIA_DFE_CFG(pe),
	    SAFEXCEL_HIA_DFE_CFG_DIS_DEBUG |
	    SAFEXCEL_HIA_DxE_CFG_MIN_DATA_SIZE(6) |
	    SAFEXCEL_HIA_DxE_CFG_MAX_DATA_SIZE(9) |
	    SAFEXCEL_HIA_DxE_CFG_MIN_CTRL_SIZE(6) |
	    SAFEXCEL_HIA_DxE_CFG_MAX_CTRL_SIZE(7) |
	    SAFEXCEL_HIA_DxE_CFG_DATA_CACHE_CTRL(RD_CACHE_3BITS) |
	    SAFEXCEL_HIA_DxE_CFG_CTRL_CACHE_CTRL(RD_CACHE_3BITS));

	/* Configure the PE DMA transfer thresholds. */
	SAFEXCEL_WRITE(sc, SAFEXCEL_PE(sc) + SAFEXCEL_PE_IN_DBUF_THRES(pe),
	    SAFEXCEL_PE_IN_xBUF_THRES_MIN(6) |
	    SAFEXCEL_PE_IN_xBUF_THRES_MAX(9));
	SAFEXCEL_WRITE(sc, SAFEXCEL_PE(sc) + SAFEXCEL_PE_IN_TBUF_THRES(pe),
	    SAFEXCEL_PE_IN_xBUF_THRES_MIN(6) |
	    SAFEXCEL_PE_IN_xBUF_THRES_MAX(7));
}

/*
 * Configure the data store engine.  This component parses result descriptors
 * and sets up DMA transfers from the processing engine to host memory.
 */
static int
safexcel_configure_dse(struct safexcel_softc *sc, int pe)
{
	uint32_t val;
	int count;

	/* Disable and reset all DSE threads. */
	SAFEXCEL_WRITE(sc,
	    SAFEXCEL_HIA_DSE_THR(sc) + SAFEXCEL_HIA_DSE_THR_CTRL(pe),
	    SAFEXCEL_DxE_THR_CTRL_RESET_PE);

	/* Wait for a second for threads to go idle. */
	for (count = 0;;) {
		val = SAFEXCEL_READ(sc,
		    SAFEXCEL_HIA_DSE_THR(sc) + SAFEXCEL_HIA_DSE_THR_STAT(pe));
		if ((val & SAFEXCEL_DSE_THR_RDR_ID_MASK) ==
		    SAFEXCEL_DSE_THR_RDR_ID_MASK)
			break;
		if (count++ > 10000) {
			device_printf(sc->sc_dev, "DSE reset timeout\n");
			return (-1);
		}
		DELAY(100);
	}

	/* Exit the reset state. */
	SAFEXCEL_WRITE(sc,
	    SAFEXCEL_HIA_DSE_THR(sc) + SAFEXCEL_HIA_DSE_THR_CTRL(pe), 0);

	/* DMA transfer size to use */
	SAFEXCEL_WRITE(sc, SAFEXCEL_HIA_DSE(sc) + SAFEXCEL_HIA_DSE_CFG(pe),
	    SAFEXCEL_HIA_DSE_CFG_DIS_DEBUG |
	    SAFEXCEL_HIA_DxE_CFG_MIN_DATA_SIZE(7) |
	    SAFEXCEL_HIA_DxE_CFG_MAX_DATA_SIZE(8) |
	    SAFEXCEL_HIA_DxE_CFG_DATA_CACHE_CTRL(WR_CACHE_3BITS) |
	    SAFEXCEL_HIA_DSE_CFG_ALLWAYS_BUFFERABLE);

	/* Configure the procesing engine thresholds */
	SAFEXCEL_WRITE(sc,
	    SAFEXCEL_PE(sc) + SAFEXCEL_PE_OUT_DBUF_THRES(pe),
	    SAFEXCEL_PE_OUT_DBUF_THRES_MIN(7) |
	    SAFEXCEL_PE_OUT_DBUF_THRES_MAX(8));

	return (0);
}

static void
safexcel_hw_prepare_rings(struct safexcel_softc *sc)
{
	int i;

	for (i = 0; i < sc->sc_config.rings; i++) {
		/*
		 * Command descriptors.
		 */

		/* Clear interrupts for this ring. */
		SAFEXCEL_WRITE(sc,
		    SAFEXCEL_HIA_AIC_R(sc) + SAFEXCEL_HIA_AIC_R_ENABLE_CLR(i),
		    SAFEXCEL_HIA_AIC_R_ENABLE_CLR_ALL_MASK);

		/* Disable external triggering. */
		SAFEXCEL_WRITE(sc,
		    SAFEXCEL_HIA_CDR(sc, i) + SAFEXCEL_HIA_xDR_CFG, 0);

		/* Clear the pending prepared counter. */
		SAFEXCEL_WRITE(sc,
		    SAFEXCEL_HIA_CDR(sc, i) + SAFEXCEL_HIA_xDR_PREP_COUNT,
		    SAFEXCEL_xDR_PREP_CLR_COUNT);

		/* Clear the pending processed counter. */
		SAFEXCEL_WRITE(sc,
		    SAFEXCEL_HIA_CDR(sc, i) + SAFEXCEL_HIA_xDR_PROC_COUNT,
		    SAFEXCEL_xDR_PROC_CLR_COUNT);

		SAFEXCEL_WRITE(sc,
		    SAFEXCEL_HIA_CDR(sc, i) + SAFEXCEL_HIA_xDR_PREP_PNTR, 0);
		SAFEXCEL_WRITE(sc,
		    SAFEXCEL_HIA_CDR(sc, i) + SAFEXCEL_HIA_xDR_PROC_PNTR, 0);

		SAFEXCEL_WRITE(sc,
		    SAFEXCEL_HIA_CDR(sc, i) + SAFEXCEL_HIA_xDR_RING_SIZE,
		    SAFEXCEL_RING_SIZE * sc->sc_config.cd_offset *
		    sizeof(uint32_t));

		/*
		 * Result descriptors.
		 */

		/* Disable external triggering. */
		SAFEXCEL_WRITE(sc,
		    SAFEXCEL_HIA_RDR(sc, i) + SAFEXCEL_HIA_xDR_CFG, 0);

		/* Clear the pending prepared counter. */
		SAFEXCEL_WRITE(sc,
		    SAFEXCEL_HIA_RDR(sc, i) + SAFEXCEL_HIA_xDR_PREP_COUNT,
		    SAFEXCEL_xDR_PREP_CLR_COUNT);

		/* Clear the pending processed counter. */
		SAFEXCEL_WRITE(sc,
		    SAFEXCEL_HIA_RDR(sc, i) + SAFEXCEL_HIA_xDR_PROC_COUNT,
		    SAFEXCEL_xDR_PROC_CLR_COUNT);

		SAFEXCEL_WRITE(sc,
		    SAFEXCEL_HIA_RDR(sc, i) + SAFEXCEL_HIA_xDR_PREP_PNTR, 0);
		SAFEXCEL_WRITE(sc,
		    SAFEXCEL_HIA_RDR(sc, i) + SAFEXCEL_HIA_xDR_PROC_PNTR, 0);

		/* Ring size. */
		SAFEXCEL_WRITE(sc,
		    SAFEXCEL_HIA_RDR(sc, i) + SAFEXCEL_HIA_xDR_RING_SIZE,
		    SAFEXCEL_RING_SIZE * sc->sc_config.rd_offset *
		    sizeof(uint32_t));
	}
}

static void
safexcel_hw_setup_rings(struct safexcel_softc *sc)
{
	struct safexcel_ring *ring;
	uint32_t cd_size_rnd, mask, rd_size_rnd, val;
	int i;

	mask = (1 << sc->sc_config.hdw) - 1;
	cd_size_rnd = (sc->sc_config.cd_size + mask) >> sc->sc_config.hdw;
	val = (sizeof(struct safexcel_res_descr) -
	    sizeof(struct safexcel_res_data)) / sizeof(uint32_t);
	rd_size_rnd = (val + mask) >> sc->sc_config.hdw;

	for (i = 0; i < sc->sc_config.rings; i++) {
		ring = &sc->sc_ring[i];

		/*
		 * Command descriptors.
		 */

		/* Ring base address. */
		SAFEXCEL_WRITE(sc, SAFEXCEL_HIA_CDR(sc, i) +
		    SAFEXCEL_HIA_xDR_RING_BASE_ADDR_LO,
		    SAFEXCEL_ADDR_LO(ring->cdr.dma.paddr));
		SAFEXCEL_WRITE(sc, SAFEXCEL_HIA_CDR(sc, i) +
		    SAFEXCEL_HIA_xDR_RING_BASE_ADDR_HI,
		    SAFEXCEL_ADDR_HI(ring->cdr.dma.paddr));

		SAFEXCEL_WRITE(sc,
		    SAFEXCEL_HIA_CDR(sc, i) + SAFEXCEL_HIA_xDR_DESC_SIZE,
		    SAFEXCEL_xDR_DESC_MODE_64BIT | SAFEXCEL_CDR_DESC_MODE_ADCP |
		    (sc->sc_config.cd_offset << SAFEXCEL_xDR_DESC_xD_OFFSET) |
		    sc->sc_config.cd_size);

		SAFEXCEL_WRITE(sc,
		    SAFEXCEL_HIA_CDR(sc, i) + SAFEXCEL_HIA_xDR_CFG,
		    ((SAFEXCEL_FETCH_COUNT * (cd_size_rnd << sc->sc_config.hdw)) <<
		      SAFEXCEL_xDR_xD_FETCH_THRESH) |
		    (SAFEXCEL_FETCH_COUNT * sc->sc_config.cd_offset));

		/* Configure DMA tx control. */
		SAFEXCEL_WRITE(sc,
		    SAFEXCEL_HIA_CDR(sc, i) + SAFEXCEL_HIA_xDR_DMA_CFG,
		    SAFEXCEL_HIA_xDR_CFG_WR_CACHE(WR_CACHE_3BITS) |
		    SAFEXCEL_HIA_xDR_CFG_RD_CACHE(RD_CACHE_3BITS));

		/* Clear any pending interrupt. */
		SAFEXCEL_WRITE(sc,
		    SAFEXCEL_HIA_CDR(sc, i) + SAFEXCEL_HIA_xDR_STAT,
		    SAFEXCEL_CDR_INTR_MASK);

		/*
		 * Result descriptors.
		 */

		/* Ring base address. */
		SAFEXCEL_WRITE(sc, SAFEXCEL_HIA_RDR(sc, i) +
		    SAFEXCEL_HIA_xDR_RING_BASE_ADDR_LO,
		    SAFEXCEL_ADDR_LO(ring->rdr.dma.paddr));
		SAFEXCEL_WRITE(sc, SAFEXCEL_HIA_RDR(sc, i) +
		    SAFEXCEL_HIA_xDR_RING_BASE_ADDR_HI,
		    SAFEXCEL_ADDR_HI(ring->rdr.dma.paddr));

		SAFEXCEL_WRITE(sc,
		    SAFEXCEL_HIA_RDR(sc, i) + SAFEXCEL_HIA_xDR_DESC_SIZE,
		    SAFEXCEL_xDR_DESC_MODE_64BIT |
		    (sc->sc_config.rd_offset << SAFEXCEL_xDR_DESC_xD_OFFSET) |
		    sc->sc_config.rd_size);

		SAFEXCEL_WRITE(sc,
		    SAFEXCEL_HIA_RDR(sc, i) + SAFEXCEL_HIA_xDR_CFG,
		    ((SAFEXCEL_FETCH_COUNT * (rd_size_rnd << sc->sc_config.hdw)) <<
		    SAFEXCEL_xDR_xD_FETCH_THRESH) |
		    (SAFEXCEL_FETCH_COUNT * sc->sc_config.rd_offset));

		/* Configure DMA tx control. */
		SAFEXCEL_WRITE(sc,
		    SAFEXCEL_HIA_RDR(sc, i) + SAFEXCEL_HIA_xDR_DMA_CFG,
		    SAFEXCEL_HIA_xDR_CFG_WR_CACHE(WR_CACHE_3BITS) |
		    SAFEXCEL_HIA_xDR_CFG_RD_CACHE(RD_CACHE_3BITS) |
		    SAFEXCEL_HIA_xDR_WR_RES_BUF | SAFEXCEL_HIA_xDR_WR_CTRL_BUF);

		/* Clear any pending interrupt. */
		SAFEXCEL_WRITE(sc,
		    SAFEXCEL_HIA_RDR(sc, i) + SAFEXCEL_HIA_xDR_STAT,
		    SAFEXCEL_RDR_INTR_MASK);

		/* Enable ring interrupt. */
		SAFEXCEL_WRITE(sc,
		    SAFEXCEL_HIA_AIC_R(sc) + SAFEXCEL_HIA_AIC_R_ENABLE_CTRL(i),
		    SAFEXCEL_RDR_IRQ(i));
	}
}

/* Reset the command and result descriptor rings. */
static void
safexcel_hw_reset_rings(struct safexcel_softc *sc)
{
	int i;

	for (i = 0; i < sc->sc_config.rings; i++) {
		/*
		 * Result descriptor ring operations.
		 */

		/* Reset ring base address. */
		SAFEXCEL_WRITE(sc, SAFEXCEL_HIA_RDR(sc, i) +
		    SAFEXCEL_HIA_xDR_RING_BASE_ADDR_LO, 0);
		SAFEXCEL_WRITE(sc, SAFEXCEL_HIA_RDR(sc, i) +
		    SAFEXCEL_HIA_xDR_RING_BASE_ADDR_HI, 0);

		/* Clear the pending prepared counter. */
		SAFEXCEL_WRITE(sc,
		    SAFEXCEL_HIA_RDR(sc, i) + SAFEXCEL_HIA_xDR_PREP_COUNT,
		    SAFEXCEL_xDR_PREP_CLR_COUNT);

		/* Clear the pending processed counter. */
		SAFEXCEL_WRITE(sc,
		    SAFEXCEL_HIA_RDR(sc, i) + SAFEXCEL_HIA_xDR_PROC_COUNT,
		    SAFEXCEL_xDR_PROC_CLR_COUNT);

		SAFEXCEL_WRITE(sc,
		    SAFEXCEL_HIA_RDR(sc, i) + SAFEXCEL_HIA_xDR_PREP_PNTR, 0);
		SAFEXCEL_WRITE(sc,
		    SAFEXCEL_HIA_RDR(sc, i) + SAFEXCEL_HIA_xDR_PROC_PNTR, 0);

		SAFEXCEL_WRITE(sc,
		    SAFEXCEL_HIA_RDR(sc, i) + SAFEXCEL_HIA_xDR_RING_SIZE, 0);

		/* Clear any pending interrupt. */
		SAFEXCEL_WRITE(sc,
		    SAFEXCEL_HIA_RDR(sc, i) + SAFEXCEL_HIA_xDR_STAT,
		    SAFEXCEL_RDR_INTR_MASK);

		/* Disable ring interrupt. */
		SAFEXCEL_WRITE(sc,
		    SAFEXCEL_HIA_AIC_R(sc) + SAFEXCEL_HIA_AIC_R_ENABLE_CLR(i),
		    SAFEXCEL_RDR_IRQ(i));

		/*
		 * Command descriptor ring operations.
		 */

		/* Reset ring base address. */
		SAFEXCEL_WRITE(sc, SAFEXCEL_HIA_CDR(sc, i) +
		    SAFEXCEL_HIA_xDR_RING_BASE_ADDR_LO, 0);
		SAFEXCEL_WRITE(sc, SAFEXCEL_HIA_CDR(sc, i) +
		    SAFEXCEL_HIA_xDR_RING_BASE_ADDR_HI, 0);

		/* Clear the pending prepared counter. */
		SAFEXCEL_WRITE(sc,
		    SAFEXCEL_HIA_CDR(sc, i) + SAFEXCEL_HIA_xDR_PREP_COUNT,
		    SAFEXCEL_xDR_PREP_CLR_COUNT);

		/* Clear the pending processed counter. */
		SAFEXCEL_WRITE(sc,
		    SAFEXCEL_HIA_CDR(sc, i) + SAFEXCEL_HIA_xDR_PROC_COUNT,
		    SAFEXCEL_xDR_PROC_CLR_COUNT);

		SAFEXCEL_WRITE(sc,
		    SAFEXCEL_HIA_CDR(sc, i) + SAFEXCEL_HIA_xDR_PREP_PNTR, 0);
		SAFEXCEL_WRITE(sc,
		    SAFEXCEL_HIA_CDR(sc, i) + SAFEXCEL_HIA_xDR_PROC_PNTR, 0);

		SAFEXCEL_WRITE(sc,
		    SAFEXCEL_HIA_CDR(sc, i) + SAFEXCEL_HIA_xDR_RING_SIZE, 0);

		/* Clear any pending interrupt. */
		SAFEXCEL_WRITE(sc,
		    SAFEXCEL_HIA_CDR(sc, i) + SAFEXCEL_HIA_xDR_STAT,
		    SAFEXCEL_CDR_INTR_MASK);
	}
}

static void
safexcel_enable_pe_engine(struct safexcel_softc *sc, int pe)
{
	int i, ring_mask;

	for (ring_mask = 0, i = 0; i < sc->sc_config.rings; i++) {
		ring_mask <<= 1;
		ring_mask |= 1;
	}

	/* Enable command descriptor rings. */
	SAFEXCEL_WRITE(sc, SAFEXCEL_HIA_DFE_THR(sc) + SAFEXCEL_HIA_DFE_THR_CTRL(pe),
	    SAFEXCEL_DxE_THR_CTRL_EN | ring_mask);

	/* Enable result descriptor rings. */
	SAFEXCEL_WRITE(sc, SAFEXCEL_HIA_DSE_THR(sc) + SAFEXCEL_HIA_DSE_THR_CTRL(pe),
	    SAFEXCEL_DxE_THR_CTRL_EN | ring_mask);

	/* Clear any HIA interrupt. */
	SAFEXCEL_WRITE(sc, SAFEXCEL_HIA_AIC_G(sc) + SAFEXCEL_HIA_AIC_G_ACK,
	    SAFEXCEL_AIC_G_ACK_HIA_MASK);
}

static void
safexcel_execute(struct safexcel_softc *sc, struct safexcel_ring *ring,
    struct safexcel_request *req)
{
	uint32_t ncdescs, nrdescs, nreqs;
	int ringidx;
	bool busy;

	mtx_assert(&ring->mtx, MA_OWNED);

	ringidx = req->sess->ringidx;
	if (STAILQ_EMPTY(&ring->ready_requests))
		return;
	busy = !STAILQ_EMPTY(&ring->queued_requests);
	ncdescs = nrdescs = nreqs = 0;
	while ((req = STAILQ_FIRST(&ring->ready_requests)) != NULL &&
	    req->cdescs + ncdescs <= SAFEXCEL_MAX_BATCH_SIZE &&
	    req->rdescs + nrdescs <= SAFEXCEL_MAX_BATCH_SIZE) {
		STAILQ_REMOVE_HEAD(&ring->ready_requests, link);
		STAILQ_INSERT_TAIL(&ring->queued_requests, req, link);
		ncdescs += req->cdescs;
		nrdescs += req->rdescs;
		nreqs++;
	}

	if (!busy) {
		SAFEXCEL_WRITE(sc,
		    SAFEXCEL_HIA_RDR(sc, ringidx) + SAFEXCEL_HIA_xDR_THRESH,
		    SAFEXCEL_HIA_CDR_THRESH_PKT_MODE | nreqs);
	}
	SAFEXCEL_WRITE(sc,
	    SAFEXCEL_HIA_RDR(sc, ringidx) + SAFEXCEL_HIA_xDR_PREP_COUNT,
	    nrdescs * sc->sc_config.rd_offset * sizeof(uint32_t));
	SAFEXCEL_WRITE(sc,
	    SAFEXCEL_HIA_CDR(sc, ringidx) + SAFEXCEL_HIA_xDR_PREP_COUNT,
	    ncdescs * sc->sc_config.cd_offset * sizeof(uint32_t));
}

static void
safexcel_init_rings(struct safexcel_softc *sc)
{
	struct safexcel_cmd_descr *cdesc;
	struct safexcel_ring *ring;
	uint64_t atok;
	int i, j;

	for (i = 0; i < sc->sc_config.rings; i++) {
		ring = &sc->sc_ring[i];

		snprintf(ring->lockname, sizeof(ring->lockname),
		    "safexcel_ring%d", i);
		mtx_init(&ring->mtx, ring->lockname, NULL, MTX_DEF);
		STAILQ_INIT(&ring->free_requests);
		STAILQ_INIT(&ring->ready_requests);
		STAILQ_INIT(&ring->queued_requests);

		ring->cdr.read = ring->cdr.write = 0;
		ring->rdr.read = ring->rdr.write = 0;
		for (j = 0; j < SAFEXCEL_RING_SIZE; j++) {
			cdesc = &ring->cdr.desc[j];
			atok = ring->dma_atok.paddr +
			    sc->sc_config.atok_offset * j;
			cdesc->atok_lo = SAFEXCEL_ADDR_LO(atok);
			cdesc->atok_hi = SAFEXCEL_ADDR_HI(atok);
		}
	}
}

static void
safexcel_dma_alloc_mem_cb(void *arg, bus_dma_segment_t *segs, int nseg,
    int error)
{
	struct safexcel_dma_mem *sdm;

	if (error != 0)
		return;

	KASSERT(nseg == 1, ("%s: nsegs is %d", __func__, nseg));
	sdm = arg;
	sdm->paddr = segs->ds_addr;
}

static int
safexcel_dma_alloc_mem(struct safexcel_softc *sc, struct safexcel_dma_mem *sdm,
    bus_size_t size)
{
	int error;

	KASSERT(sdm->vaddr == NULL,
	    ("%s: DMA memory descriptor in use.", __func__));

	error = bus_dma_tag_create(bus_get_dma_tag(sc->sc_dev), /* parent */
	    PAGE_SIZE, 0,		/* alignment, boundary */
	    BUS_SPACE_MAXADDR_32BIT,	/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filtfunc, filtfuncarg */
	    size, 1,			/* maxsize, nsegments */
	    size, BUS_DMA_COHERENT,	/* maxsegsz, flags */
	    NULL, NULL,			/* lockfunc, lockfuncarg */
	    &sdm->tag);			/* dmat */
	if (error != 0) {
		device_printf(sc->sc_dev,
		    "failed to allocate busdma tag, error %d\n", error);
		goto err1;
	}

	error = bus_dmamem_alloc(sdm->tag, (void **)&sdm->vaddr,
	    BUS_DMA_WAITOK | BUS_DMA_ZERO | BUS_DMA_COHERENT, &sdm->map);
	if (error != 0) {
		device_printf(sc->sc_dev,
		    "failed to allocate DMA safe memory, error %d\n", error);
		goto err2;
	}

	error = bus_dmamap_load(sdm->tag, sdm->map, sdm->vaddr, size,
	    safexcel_dma_alloc_mem_cb, sdm, BUS_DMA_NOWAIT);
	if (error != 0) {
		device_printf(sc->sc_dev,
		    "cannot get address of the DMA memory, error %d\n", error);
		goto err3;
	}

	return (0);
err3:
	bus_dmamem_free(sdm->tag, sdm->vaddr, sdm->map);
err2:
	bus_dma_tag_destroy(sdm->tag);
err1:
	sdm->vaddr = NULL;

	return (error);
}

static void
safexcel_dma_free_mem(struct safexcel_dma_mem *sdm)
{
	bus_dmamap_unload(sdm->tag, sdm->map);
	bus_dmamem_free(sdm->tag, sdm->vaddr, sdm->map);
	bus_dma_tag_destroy(sdm->tag);
}

static void
safexcel_dma_free_rings(struct safexcel_softc *sc)
{
	struct safexcel_ring *ring;
	int i;

	for (i = 0; i < sc->sc_config.rings; i++) {
		ring = &sc->sc_ring[i];
		safexcel_dma_free_mem(&ring->cdr.dma);
		safexcel_dma_free_mem(&ring->dma_atok);
		safexcel_dma_free_mem(&ring->rdr.dma);
		bus_dma_tag_destroy(ring->data_dtag);
		mtx_destroy(&ring->mtx);
	}
}

static int
safexcel_dma_init(struct safexcel_softc *sc)
{
	struct safexcel_ring *ring;
	bus_size_t size;
	int error, i;

	for (i = 0; i < sc->sc_config.rings; i++) {
		ring = &sc->sc_ring[i];

		error = bus_dma_tag_create(
		    bus_get_dma_tag(sc->sc_dev),/* parent */
		    1, 0,			/* alignment, boundary */
		    BUS_SPACE_MAXADDR_32BIT,	/* lowaddr */
		    BUS_SPACE_MAXADDR,		/* highaddr */
		    NULL, NULL,			/* filtfunc, filtfuncarg */
		    SAFEXCEL_MAX_REQUEST_SIZE,	/* maxsize */
		    SAFEXCEL_MAX_FRAGMENTS,	/* nsegments */
		    SAFEXCEL_MAX_REQUEST_SIZE,	/* maxsegsz */
		    BUS_DMA_COHERENT,		/* flags */
		    NULL, NULL,			/* lockfunc, lockfuncarg */
		    &ring->data_dtag);		/* dmat */
		if (error != 0) {
			device_printf(sc->sc_dev,
			    "bus_dma_tag_create main failed; error %d\n", error);
			return (error);
		}

		size = sizeof(uint32_t) * sc->sc_config.cd_offset *
		    SAFEXCEL_RING_SIZE;
		error = safexcel_dma_alloc_mem(sc, &ring->cdr.dma, size);
		if (error != 0) {
			device_printf(sc->sc_dev,
			    "failed to allocate CDR DMA memory, error %d\n",
			    error);
			goto err;
		}
		ring->cdr.desc =
		    (struct safexcel_cmd_descr *)ring->cdr.dma.vaddr;

		/* Allocate additional CDR token memory. */
		size = (bus_size_t)sc->sc_config.atok_offset *
		    SAFEXCEL_RING_SIZE;
		error = safexcel_dma_alloc_mem(sc, &ring->dma_atok, size);
		if (error != 0) {
			device_printf(sc->sc_dev,
			    "failed to allocate atoken DMA memory, error %d\n",
			    error);
			goto err;
		}

		size = sizeof(uint32_t) * sc->sc_config.rd_offset *
		    SAFEXCEL_RING_SIZE;
		error = safexcel_dma_alloc_mem(sc, &ring->rdr.dma, size);
		if (error) {
			device_printf(sc->sc_dev,
			    "failed to allocate RDR DMA memory, error %d\n",
			    error);
			goto err;
		}
		ring->rdr.desc =
		    (struct safexcel_res_descr *)ring->rdr.dma.vaddr;
	}

	return (0);
err:
	safexcel_dma_free_rings(sc);
	return (error);
}

static void
safexcel_deinit_hw(struct safexcel_softc *sc)
{
	safexcel_hw_reset_rings(sc);
	safexcel_dma_free_rings(sc);
}

static int
safexcel_init_hw(struct safexcel_softc *sc)
{
	int pe;

	/* 23.3.7 Initialization */
	if (safexcel_configure(sc) != 0)
		return (EINVAL);

	if (safexcel_dma_init(sc) != 0)
		return (ENOMEM);

	safexcel_init_rings(sc);

	safexcel_init_hia_bus_access(sc);

	/* 23.3.7.2 Disable EIP-97 global Interrupts */
	safexcel_disable_global_interrupts(sc);

	for (pe = 0; pe < sc->sc_config.pes; pe++) {
		/* 23.3.7.3 Configure Data Fetch Engine */
		safexcel_configure_dfe_engine(sc, pe);

		/* 23.3.7.4 Configure Data Store Engine */
		if (safexcel_configure_dse(sc, pe)) {
			safexcel_deinit_hw(sc);
			return (-1);
		}

		/* 23.3.7.5 1. Protocol enables */
		SAFEXCEL_WRITE(sc,
		    SAFEXCEL_PE(sc) + SAFEXCEL_PE_EIP96_FUNCTION_EN(pe),
		    0xffffffff);
		SAFEXCEL_WRITE(sc,
		    SAFEXCEL_PE(sc) + SAFEXCEL_PE_EIP96_FUNCTION2_EN(pe),
		    0xffffffff);
	}

	safexcel_hw_prepare_rings(sc);

	/* 23.3.7.5 Configure the Processing Engine(s). */
	for (pe = 0; pe < sc->sc_config.pes; pe++)
		safexcel_enable_pe_engine(sc, pe);

	safexcel_hw_setup_rings(sc);

	return (0);
}

static int
safexcel_setup_dev_interrupts(struct safexcel_softc *sc)
{
	int i, j;

	for (i = 0; i < SAFEXCEL_MAX_RINGS && sc->sc_intr[i] != NULL; i++) {
		sc->sc_ih[i].sc = sc;
		sc->sc_ih[i].ring = i;

		if (bus_setup_intr(sc->sc_dev, sc->sc_intr[i],
		    INTR_TYPE_NET | INTR_MPSAFE, NULL, safexcel_ring_intr,
		    &sc->sc_ih[i], &sc->sc_ih[i].handle)) {
			device_printf(sc->sc_dev,
			    "couldn't setup interrupt %d\n", i);
			goto err;
		}
	}

	return (0);

err:
	for (j = 0; j < i; j++)
		bus_teardown_intr(sc->sc_dev, sc->sc_intr[j],
		    sc->sc_ih[j].handle);

	return (ENXIO);
}

static void
safexcel_teardown_dev_interrupts(struct safexcel_softc *sc)
{
	int i;

	for (i = 0; i < SAFEXCEL_MAX_RINGS; i++)
		bus_teardown_intr(sc->sc_dev, sc->sc_intr[i],
		    sc->sc_ih[i].handle);
}

static int
safexcel_alloc_dev_resources(struct safexcel_softc *sc)
{
	char name[16];
	device_t dev;
	phandle_t node;
	int error, i, rid;

	dev = sc->sc_dev;
	node = ofw_bus_get_node(dev);

	rid = 0;
	sc->sc_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->sc_res == NULL) {
		device_printf(dev, "couldn't allocate memory resources\n");
		return (ENXIO);
	}

	for (i = 0; i < SAFEXCEL_MAX_RINGS; i++) {
		(void)snprintf(name, sizeof(name), "ring%d", i);
		error = ofw_bus_find_string_index(node, "interrupt-names", name,
		    &rid);
		if (error != 0)
			break;

		sc->sc_intr[i] = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
		    RF_ACTIVE | RF_SHAREABLE);
		if (sc->sc_intr[i] == NULL) {
			error = ENXIO;
			goto out;
		}
	}
	if (i == 0) {
		device_printf(dev, "couldn't allocate interrupt resources\n");
		error = ENXIO;
		goto out;
	}

	return (0);

out:
	for (i = 0; i < SAFEXCEL_MAX_RINGS && sc->sc_intr[i] != NULL; i++)
		bus_release_resource(dev, SYS_RES_IRQ,
		    rman_get_rid(sc->sc_intr[i]), sc->sc_intr[i]);
	bus_release_resource(dev, SYS_RES_MEMORY, rman_get_rid(sc->sc_res),
	    sc->sc_res);
	return (error);
}

static void
safexcel_free_dev_resources(struct safexcel_softc *sc)
{
	int i;

	for (i = 0; i < SAFEXCEL_MAX_RINGS && sc->sc_intr[i] != NULL; i++)
		bus_release_resource(sc->sc_dev, SYS_RES_IRQ,
		    rman_get_rid(sc->sc_intr[i]), sc->sc_intr[i]);
	if (sc->sc_res != NULL)
		bus_release_resource(sc->sc_dev, SYS_RES_MEMORY,
		    rman_get_rid(sc->sc_res), sc->sc_res);
}

static int
safexcel_probe(device_t dev)
{
	struct safexcel_softc *sc;

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	sc = device_get_softc(dev);
	sc->sc_type = ofw_bus_search_compatible(dev, safexcel_compat)->ocd_data;
	if (sc->sc_type == 0)
		return (ENXIO);

	device_set_desc(dev, "SafeXcel EIP-97 crypto accelerator");

	return (BUS_PROBE_DEFAULT);
}

static int
safexcel_attach(device_t dev)
{
	struct sysctl_ctx_list *sctx;
	struct safexcel_softc *sc;
	struct safexcel_request *req;
	struct safexcel_ring *ring;
	int i, j, ringidx;

	sc = device_get_softc(dev);
	sc->sc_dev = dev;
	sc->sc_cid = -1;

	if (safexcel_alloc_dev_resources(sc))
		goto err;

	if (safexcel_setup_dev_interrupts(sc))
		goto err1;

	if (safexcel_init_hw(sc))
		goto err2;

	for (ringidx = 0; ringidx < sc->sc_config.rings; ringidx++) {
		ring = &sc->sc_ring[ringidx];

		ring->cmd_data = sglist_alloc(SAFEXCEL_MAX_FRAGMENTS, M_WAITOK);
		ring->res_data = sglist_alloc(SAFEXCEL_MAX_FRAGMENTS, M_WAITOK);

		ring->requests = mallocarray(SAFEXCEL_REQUESTS_PER_RING,
		    sizeof(struct safexcel_request), M_SAFEXCEL,
		    M_WAITOK | M_ZERO);

		for (i = 0; i < SAFEXCEL_REQUESTS_PER_RING; i++) {
			req = &ring->requests[i];
			req->sc = sc;
			if (bus_dmamap_create(ring->data_dtag,
			    BUS_DMA_COHERENT, &req->dmap) != 0) {
				for (j = 0; j < i; j++)
					bus_dmamap_destroy(ring->data_dtag,
					    ring->requests[j].dmap);
				goto err2;
			}
			if (safexcel_dma_alloc_mem(sc, &req->ctx,
			    sizeof(struct safexcel_context_record)) != 0) {
				for (j = 0; j < i; j++) {
					bus_dmamap_destroy(ring->data_dtag,
					    ring->requests[j].dmap);
					safexcel_dma_free_mem(
					    &ring->requests[j].ctx);
				}
				goto err2;
			}
			STAILQ_INSERT_TAIL(&ring->free_requests, req, link);
		}
	}

	sctx = device_get_sysctl_ctx(dev);
	SYSCTL_ADD_INT(sctx, SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "debug", CTLFLAG_RWTUN, &sc->sc_debug, 0,
	    "Debug message verbosity");

	sc->sc_cid = crypto_get_driverid(dev, sizeof(struct safexcel_session),
	    CRYPTOCAP_F_HARDWARE);
	if (sc->sc_cid < 0)
		goto err2;

	return (0);

err2:
	safexcel_teardown_dev_interrupts(sc);
err1:
	safexcel_free_dev_resources(sc);
err:
	return (ENXIO);
}

static int
safexcel_detach(device_t dev)
{
	struct safexcel_ring *ring;
	struct safexcel_softc *sc;
	int i, ringidx;

	sc = device_get_softc(dev);

	if (sc->sc_cid >= 0)
		crypto_unregister_all(sc->sc_cid);
	for (ringidx = 0; ringidx < sc->sc_config.rings; ringidx++) {
		ring = &sc->sc_ring[ringidx];
		for (i = 0; i < SAFEXCEL_REQUESTS_PER_RING; i++) {
			bus_dmamap_destroy(ring->data_dtag,
			    ring->requests[i].dmap);
			safexcel_dma_free_mem(&ring->requests[i].ctx);
		}
		free(ring->requests, M_SAFEXCEL);
		sglist_free(ring->cmd_data);
		sglist_free(ring->res_data);
	}
	safexcel_deinit_hw(sc);
	safexcel_teardown_dev_interrupts(sc);
	safexcel_free_dev_resources(sc);

	return (0);
}

/*
 * Populate the request's context record with pre-computed key material.
 */
static int
safexcel_set_context(struct safexcel_request *req)
{
	const struct crypto_session_params *csp;
	struct cryptop *crp;
	struct safexcel_context_record *ctx;
	struct safexcel_session *sess;
	uint8_t *data;
	int off;

	crp = req->crp;
	csp = crypto_get_params(crp->crp_session);
	sess = req->sess;

	ctx = (struct safexcel_context_record *)req->ctx.vaddr;
	data = (uint8_t *)ctx->data;
	if (csp->csp_cipher_alg != 0) {
		if (crp->crp_cipher_key != NULL)
			memcpy(data, crp->crp_cipher_key, sess->klen);
		else
			memcpy(data, csp->csp_cipher_key, sess->klen);
		off = sess->klen;
	} else if (csp->csp_auth_alg == CRYPTO_AES_NIST_GMAC) {
		if (crp->crp_auth_key != NULL)
			memcpy(data, crp->crp_auth_key, sess->klen);
		else
			memcpy(data, csp->csp_auth_key, sess->klen);
		off = sess->klen;
	} else {
		off = 0;
	}

	switch (csp->csp_cipher_alg) {
	case CRYPTO_AES_NIST_GCM_16:
		memcpy(data + off, sess->ghash_key, GMAC_BLOCK_LEN);
		off += GMAC_BLOCK_LEN;
		break;
	case CRYPTO_AES_CCM_16:
		memcpy(data + off, sess->xcbc_key,
		    AES_BLOCK_LEN * 2 + sess->klen);
		off += AES_BLOCK_LEN * 2 + sess->klen;
		break;
	case CRYPTO_AES_XTS:
		memcpy(data + off, sess->tweak_key, sess->klen);
		off += sess->klen;
		break;
	}

	switch (csp->csp_auth_alg) {
	case CRYPTO_AES_NIST_GMAC:
		memcpy(data + off, sess->ghash_key, GMAC_BLOCK_LEN);
		off += GMAC_BLOCK_LEN;
		break;
	case CRYPTO_SHA1_HMAC:
	case CRYPTO_SHA2_224_HMAC:
	case CRYPTO_SHA2_256_HMAC:
	case CRYPTO_SHA2_384_HMAC:
	case CRYPTO_SHA2_512_HMAC:
		memcpy(data + off, sess->hmac_ipad, sess->statelen);
		off += sess->statelen;
		memcpy(data + off, sess->hmac_opad, sess->statelen);
		off += sess->statelen;
		break;
	}

	return (off);
}

/*
 * Populate fields in the first command descriptor of the chain used to encode
 * the specified request.  These fields indicate the algorithms used, the size
 * of the key material stored in the associated context record, the primitive
 * operations to be performed on input data, and the location of the IV if any.
 */
static void
safexcel_set_command(struct safexcel_request *req,
    struct safexcel_cmd_descr *cdesc)
{
	const struct crypto_session_params *csp;
	struct cryptop *crp;
	struct safexcel_session *sess;
	uint32_t ctrl0, ctrl1, ctxr_len;
	int alg;

	crp = req->crp;
	csp = crypto_get_params(crp->crp_session);
	sess = req->sess;

	ctrl0 = sess->alg | sess->digest | sess->hash;
	ctrl1 = sess->mode;

	ctxr_len = safexcel_set_context(req) / sizeof(uint32_t);
	ctrl0 |= SAFEXCEL_CONTROL0_SIZE(ctxr_len);

	alg = csp->csp_cipher_alg;
	if (alg == 0)
		alg = csp->csp_auth_alg;

	switch (alg) {
	case CRYPTO_AES_CCM_16:
		if (CRYPTO_OP_IS_ENCRYPT(crp->crp_op)) {
			ctrl0 |= SAFEXCEL_CONTROL0_TYPE_HASH_ENCRYPT_OUT |
			    SAFEXCEL_CONTROL0_KEY_EN;
		} else {
			ctrl0 |= SAFEXCEL_CONTROL0_TYPE_DECRYPT_HASH_IN |
			    SAFEXCEL_CONTROL0_KEY_EN;
		}
		ctrl1 |= SAFEXCEL_CONTROL1_IV0 | SAFEXCEL_CONTROL1_IV1 |
		    SAFEXCEL_CONTROL1_IV2 | SAFEXCEL_CONTROL1_IV3;
		break;
	case CRYPTO_AES_CBC:
	case CRYPTO_AES_ICM:
	case CRYPTO_AES_XTS:
		if (CRYPTO_OP_IS_ENCRYPT(crp->crp_op)) {
			ctrl0 |= SAFEXCEL_CONTROL0_TYPE_CRYPTO_OUT |
			    SAFEXCEL_CONTROL0_KEY_EN;
			if (csp->csp_auth_alg != 0)
				ctrl0 |=
				    SAFEXCEL_CONTROL0_TYPE_ENCRYPT_HASH_OUT;
		} else {
			ctrl0 |= SAFEXCEL_CONTROL0_TYPE_CRYPTO_IN |
			    SAFEXCEL_CONTROL0_KEY_EN;
			if (csp->csp_auth_alg != 0)
				ctrl0 |= SAFEXCEL_CONTROL0_TYPE_HASH_DECRYPT_IN;
		}
		break;
	case CRYPTO_AES_NIST_GCM_16:
	case CRYPTO_AES_NIST_GMAC:
		if (CRYPTO_OP_IS_ENCRYPT(crp->crp_op) ||
		    csp->csp_auth_alg != 0) {
			ctrl0 |= SAFEXCEL_CONTROL0_TYPE_CRYPTO_OUT |
			    SAFEXCEL_CONTROL0_KEY_EN |
			    SAFEXCEL_CONTROL0_TYPE_HASH_OUT;
		} else {
			ctrl0 |= SAFEXCEL_CONTROL0_TYPE_CRYPTO_IN |
			    SAFEXCEL_CONTROL0_KEY_EN |
			    SAFEXCEL_CONTROL0_TYPE_HASH_DECRYPT_IN;
		}
		if (csp->csp_cipher_alg == CRYPTO_AES_NIST_GCM_16) {
			ctrl1 |= SAFEXCEL_CONTROL1_COUNTER_MODE |
			    SAFEXCEL_CONTROL1_IV0 | SAFEXCEL_CONTROL1_IV1 |
			    SAFEXCEL_CONTROL1_IV2;
		}
		break;
	case CRYPTO_SHA1:
	case CRYPTO_SHA2_224:
	case CRYPTO_SHA2_256:
	case CRYPTO_SHA2_384:
	case CRYPTO_SHA2_512:
		ctrl0 |= SAFEXCEL_CONTROL0_RESTART_HASH;
		/* FALLTHROUGH */
	case CRYPTO_SHA1_HMAC:
	case CRYPTO_SHA2_224_HMAC:
	case CRYPTO_SHA2_256_HMAC:
	case CRYPTO_SHA2_384_HMAC:
	case CRYPTO_SHA2_512_HMAC:
		ctrl0 |= SAFEXCEL_CONTROL0_TYPE_HASH_OUT;
		break;
	}

	cdesc->control_data.control0 = ctrl0;
	cdesc->control_data.control1 = ctrl1;
}

/*
 * Construct a no-op instruction, used to pad input tokens.
 */
static void
safexcel_instr_nop(struct safexcel_instr **instrp)
{
	struct safexcel_instr *instr;

	instr = *instrp;
	instr->opcode = SAFEXCEL_INSTR_OPCODE_INSERT;
	instr->length = (1 << 2);
	instr->status = 0;
	instr->instructions = 0;

	*instrp = instr + 1;
}

/*
 * Insert the digest of the input payload.  This is typically the last
 * instruction of a sequence.
 */
static void
safexcel_instr_insert_digest(struct safexcel_instr **instrp, int len)
{
	struct safexcel_instr *instr;

	instr = *instrp;
	instr->opcode = SAFEXCEL_INSTR_OPCODE_INSERT;
	instr->length = len;
	instr->status = SAFEXCEL_INSTR_STATUS_LAST_HASH |
	    SAFEXCEL_INSTR_STATUS_LAST_PACKET;
	instr->instructions = SAFEXCEL_INSTR_DEST_OUTPUT |
	    SAFEXCEL_INSTR_INSERT_HASH_DIGEST;

	*instrp = instr + 1;
}

/*
 * Retrieve and verify a digest.
 */
static void
safexcel_instr_retrieve_digest(struct safexcel_instr **instrp, int len)
{
	struct safexcel_instr *instr;

	instr = *instrp;
	instr->opcode = SAFEXCEL_INSTR_OPCODE_RETRIEVE;
	instr->length = len;
	instr->status = SAFEXCEL_INSTR_STATUS_LAST_HASH |
	    SAFEXCEL_INSTR_STATUS_LAST_PACKET;
	instr->instructions = SAFEXCEL_INSTR_INSERT_HASH_DIGEST;
	instr++;

	instr->opcode = SAFEXCEL_INSTR_OPCODE_VERIFY_FIELDS;
	instr->length = len | SAFEXCEL_INSTR_VERIFY_HASH;
	instr->status = SAFEXCEL_INSTR_STATUS_LAST_HASH |
	    SAFEXCEL_INSTR_STATUS_LAST_PACKET;
	instr->instructions = SAFEXCEL_INSTR_VERIFY_PADDING;

	*instrp = instr + 1;
}

static void
safexcel_instr_temp_aes_block(struct safexcel_instr **instrp)
{
	struct safexcel_instr *instr;

	instr = *instrp;
	instr->opcode = SAFEXCEL_INSTR_OPCODE_INSERT_REMOVE_RESULT;
	instr->length = 0;
	instr->status = 0;
	instr->instructions = AES_BLOCK_LEN;
	instr++;

	instr->opcode = SAFEXCEL_INSTR_OPCODE_INSERT;
	instr->length = AES_BLOCK_LEN;
	instr->status = 0;
	instr->instructions = SAFEXCEL_INSTR_DEST_OUTPUT |
	    SAFEXCEL_INSTR_DEST_CRYPTO;

	*instrp = instr + 1;
}

/*
 * Handle a request for an unauthenticated block cipher.
 */
static void
safexcel_instr_cipher(struct safexcel_request *req,
    struct safexcel_instr *instr, struct safexcel_cmd_descr *cdesc)
{
	struct cryptop *crp;

	crp = req->crp;

	/* Insert the payload. */
	instr->opcode = SAFEXCEL_INSTR_OPCODE_DIRECTION;
	instr->length = crp->crp_payload_length;
	instr->status = SAFEXCEL_INSTR_STATUS_LAST_PACKET |
	    SAFEXCEL_INSTR_STATUS_LAST_HASH;
	instr->instructions = SAFEXCEL_INSTR_INS_LAST |
	    SAFEXCEL_INSTR_DEST_CRYPTO | SAFEXCEL_INSTR_DEST_OUTPUT;

	cdesc->additional_cdata_size = 1;
}

static void
safexcel_instr_eta(struct safexcel_request *req, struct safexcel_instr *instr,
    struct safexcel_cmd_descr *cdesc)
{
	const struct crypto_session_params *csp;
	struct cryptop *crp;
	struct safexcel_instr *start;

	crp = req->crp;
	csp = crypto_get_params(crp->crp_session);
	start = instr;

	/* Insert the AAD. */
	instr->opcode = SAFEXCEL_INSTR_OPCODE_DIRECTION;
	instr->length = crp->crp_aad_length;
	instr->status = crp->crp_payload_length == 0 ?
	    SAFEXCEL_INSTR_STATUS_LAST_HASH : 0;
	instr->instructions = SAFEXCEL_INSTR_INS_LAST |
	    SAFEXCEL_INSTR_DEST_HASH;
	instr++;

	/* Encrypt any data left in the request. */
	if (crp->crp_payload_length > 0) {
		instr->opcode = SAFEXCEL_INSTR_OPCODE_DIRECTION;
		instr->length = crp->crp_payload_length;
		instr->status = SAFEXCEL_INSTR_STATUS_LAST_HASH;
		instr->instructions = SAFEXCEL_INSTR_INS_LAST |
		    SAFEXCEL_INSTR_DEST_CRYPTO |
		    SAFEXCEL_INSTR_DEST_HASH |
		    SAFEXCEL_INSTR_DEST_OUTPUT;
		instr++;
	}

	/*
	 * Compute the digest, or extract it and place it in the output stream.
	 */
	if (CRYPTO_OP_IS_ENCRYPT(crp->crp_op))
		safexcel_instr_insert_digest(&instr, req->sess->digestlen);
	else
		safexcel_instr_retrieve_digest(&instr, req->sess->digestlen);
	cdesc->additional_cdata_size = instr - start;
}

static void
safexcel_instr_sha_hash(struct safexcel_request *req,
    struct safexcel_instr *instr)
{
	struct cryptop *crp;
	struct safexcel_instr *start;

	crp = req->crp;
	start = instr;

	/* Pass the input data to the hash engine. */
	instr->opcode = SAFEXCEL_INSTR_OPCODE_DIRECTION;
	instr->length = crp->crp_payload_length;
	instr->status = SAFEXCEL_INSTR_STATUS_LAST_HASH;
	instr->instructions = SAFEXCEL_INSTR_DEST_HASH;
	instr++;

	/* Insert the hash result into the output stream. */
	safexcel_instr_insert_digest(&instr, req->sess->digestlen);

	/* Pad the rest of the inline instruction space. */
	while (instr != start + SAFEXCEL_MAX_ITOKENS)
		safexcel_instr_nop(&instr);
}

static void
safexcel_instr_ccm(struct safexcel_request *req, struct safexcel_instr *instr,
    struct safexcel_cmd_descr *cdesc)
{
	struct cryptop *crp;
	struct safexcel_instr *start;
	uint8_t *a0, *b0, *alenp, L;
	int aalign, blen;

	crp = req->crp;
	start = instr;

	/*
	 * Construct two blocks, A0 and B0, used in encryption and
	 * authentication, respectively.  A0 is embedded in the token
	 * descriptor, and B0 is inserted directly into the data stream using
	 * instructions below.
	 *
	 * OCF seems to assume a 12-byte IV, fixing L (the payload length size)
	 * at 3 bytes due to the layout of B0.  This is fine since the driver
	 * has a maximum of 65535 bytes anyway.
	 */
	blen = AES_BLOCK_LEN;
	L = 3;

	a0 = (uint8_t *)&cdesc->control_data.token[0];
	memset(a0, 0, blen);
	a0[0] = L - 1;
	memcpy(&a0[1], req->iv, AES_CCM_IV_LEN);

	/*
	 * Insert B0 and the AAD length into the input stream.
	 */
	instr->opcode = SAFEXCEL_INSTR_OPCODE_INSERT;
	instr->length = blen + (crp->crp_aad_length > 0 ? 2 : 0);
	instr->status = 0;
	instr->instructions = SAFEXCEL_INSTR_DEST_HASH |
	    SAFEXCEL_INSTR_INSERT_IMMEDIATE;
	instr++;

	b0 = (uint8_t *)instr;
	memset(b0, 0, blen);
	b0[0] =
	    (L - 1) | /* payload length size */
	    ((CCM_CBC_MAX_DIGEST_LEN - 2) / 2) << 3 /* digest length */ |
	    (crp->crp_aad_length > 0 ? 1 : 0) << 6 /* AAD present bit */;
	memcpy(&b0[1], req->iv, AES_CCM_IV_LEN);
	b0[14] = crp->crp_payload_length >> 8;
	b0[15] = crp->crp_payload_length & 0xff;
	instr += blen / sizeof(*instr);

	/* Insert the AAD length and data into the input stream. */
	if (crp->crp_aad_length > 0) {
		alenp = (uint8_t *)instr;
		alenp[0] = crp->crp_aad_length >> 8;
		alenp[1] = crp->crp_aad_length & 0xff;
		alenp[2] = 0;
		alenp[3] = 0;
		instr++;

		instr->opcode = SAFEXCEL_INSTR_OPCODE_DIRECTION;
		instr->length = crp->crp_aad_length;
		instr->status = 0;
		instr->instructions = SAFEXCEL_INSTR_DEST_HASH;
		instr++;

		/* Insert zero padding. */
		aalign = (crp->crp_aad_length + 2) & (blen - 1);
		instr->opcode = SAFEXCEL_INSTR_OPCODE_INSERT;
		instr->length = aalign == 0 ? 0 :
		    blen - ((crp->crp_aad_length + 2) & (blen - 1));
		instr->status = crp->crp_payload_length == 0 ?
		    SAFEXCEL_INSTR_STATUS_LAST_HASH : 0;
		instr->instructions = SAFEXCEL_INSTR_DEST_HASH;
		instr++;
	}

	safexcel_instr_temp_aes_block(&instr);

	/* Insert the cipher payload into the input stream. */
	if (crp->crp_payload_length > 0) {
		instr->opcode = SAFEXCEL_INSTR_OPCODE_DIRECTION;
		instr->length = crp->crp_payload_length;
		instr->status = (crp->crp_payload_length & (blen - 1)) == 0 ?
		    SAFEXCEL_INSTR_STATUS_LAST_HASH : 0;
		instr->instructions = SAFEXCEL_INSTR_DEST_OUTPUT |
		    SAFEXCEL_INSTR_DEST_CRYPTO |
		    SAFEXCEL_INSTR_DEST_HASH |
		    SAFEXCEL_INSTR_INS_LAST;
		instr++;

		/* Insert zero padding. */
		if (crp->crp_payload_length & (blen - 1)) {
			instr->opcode = SAFEXCEL_INSTR_OPCODE_INSERT;
			instr->length = blen -
			    (crp->crp_payload_length & (blen - 1));
			instr->status = SAFEXCEL_INSTR_STATUS_LAST_HASH;
			instr->instructions = SAFEXCEL_INSTR_DEST_HASH;
			instr++;
		}
	}

	/*
	 * Compute the digest, or extract it and place it in the output stream.
	 */
	if (CRYPTO_OP_IS_ENCRYPT(crp->crp_op))
		safexcel_instr_insert_digest(&instr, req->sess->digestlen);
	else
		safexcel_instr_retrieve_digest(&instr, req->sess->digestlen);

	cdesc->additional_cdata_size = instr - start;
}

static void
safexcel_instr_gcm(struct safexcel_request *req, struct safexcel_instr *instr,
    struct safexcel_cmd_descr *cdesc)
{
	struct cryptop *crp;
	struct safexcel_instr *start;

	memcpy(cdesc->control_data.token, req->iv, AES_GCM_IV_LEN);
	cdesc->control_data.token[3] = htobe32(1);

	crp = req->crp;
	start = instr;

	/* Insert the AAD into the input stream. */
	instr->opcode = SAFEXCEL_INSTR_OPCODE_DIRECTION;
	instr->length = crp->crp_aad_length;
	instr->status = crp->crp_payload_length == 0 ?
	    SAFEXCEL_INSTR_STATUS_LAST_HASH : 0;
	instr->instructions = SAFEXCEL_INSTR_INS_LAST |
	    SAFEXCEL_INSTR_DEST_HASH;
	instr++;

	safexcel_instr_temp_aes_block(&instr);

	/* Insert the cipher payload into the input stream. */
	if (crp->crp_payload_length > 0) {
		instr->opcode = SAFEXCEL_INSTR_OPCODE_DIRECTION;
		instr->length = crp->crp_payload_length;
		instr->status = SAFEXCEL_INSTR_STATUS_LAST_HASH;
		instr->instructions = SAFEXCEL_INSTR_DEST_OUTPUT |
		    SAFEXCEL_INSTR_DEST_CRYPTO | SAFEXCEL_INSTR_DEST_HASH |
		    SAFEXCEL_INSTR_INS_LAST;
		instr++;
	}

	/*
	 * Compute the digest, or extract it and place it in the output stream.
	 */
	if (CRYPTO_OP_IS_ENCRYPT(crp->crp_op))
		safexcel_instr_insert_digest(&instr, req->sess->digestlen);
	else
		safexcel_instr_retrieve_digest(&instr, req->sess->digestlen);

	cdesc->additional_cdata_size = instr - start;
}

static void
safexcel_instr_gmac(struct safexcel_request *req, struct safexcel_instr *instr,
    struct safexcel_cmd_descr *cdesc)
{
	struct cryptop *crp;
	struct safexcel_instr *start;

	memcpy(cdesc->control_data.token, req->iv, AES_GCM_IV_LEN);
	cdesc->control_data.token[3] = htobe32(1);

	crp = req->crp;
	start = instr;

	instr->opcode = SAFEXCEL_INSTR_OPCODE_DIRECTION;
	instr->length = crp->crp_payload_length;
	instr->status = SAFEXCEL_INSTR_STATUS_LAST_HASH;
	instr->instructions = SAFEXCEL_INSTR_INS_LAST |
	    SAFEXCEL_INSTR_DEST_HASH;
	instr++;

	safexcel_instr_temp_aes_block(&instr);

	safexcel_instr_insert_digest(&instr, req->sess->digestlen);

	cdesc->additional_cdata_size = instr - start;
}

static void
safexcel_set_token(struct safexcel_request *req)
{
	const struct crypto_session_params *csp;
	struct safexcel_cmd_descr *cdesc;
	struct safexcel_instr *instr;
	struct safexcel_softc *sc;
	int ringidx;

	csp = crypto_get_params(req->crp->crp_session);
	cdesc = req->cdesc;
	sc = req->sc;
	ringidx = req->sess->ringidx;

	safexcel_set_command(req, cdesc);

	/*
	 * For keyless hash operations, the token instructions can be embedded
	 * in the token itself.  Otherwise we use an additional token descriptor
	 * and the embedded instruction space is used to store the IV.
	 */
	if (csp->csp_cipher_alg == 0 &&
	    csp->csp_auth_alg != CRYPTO_AES_NIST_GMAC) {
		instr = (void *)cdesc->control_data.token;
	} else {
		instr = (void *)(sc->sc_ring[ringidx].dma_atok.vaddr +
		    sc->sc_config.atok_offset *
		    (cdesc - sc->sc_ring[ringidx].cdr.desc));
		cdesc->control_data.options |= SAFEXCEL_OPTION_4_TOKEN_IV_CMD;
	}

	switch (csp->csp_cipher_alg) {
	case CRYPTO_AES_NIST_GCM_16:
		safexcel_instr_gcm(req, instr, cdesc);
		break;
	case CRYPTO_AES_CCM_16:
		safexcel_instr_ccm(req, instr, cdesc);
		break;
	case CRYPTO_AES_XTS:
		memcpy(cdesc->control_data.token, req->iv, AES_XTS_IV_LEN);
		memset(cdesc->control_data.token +
		    AES_XTS_IV_LEN / sizeof(uint32_t), 0, AES_XTS_IV_LEN);

		safexcel_instr_cipher(req, instr, cdesc);
		break;
	case CRYPTO_AES_CBC:
	case CRYPTO_AES_ICM:
		memcpy(cdesc->control_data.token, req->iv, AES_BLOCK_LEN);
		if (csp->csp_auth_alg != 0)
			safexcel_instr_eta(req, instr, cdesc);
		else
			safexcel_instr_cipher(req, instr, cdesc);
		break;
	default:
		switch (csp->csp_auth_alg) {
		case CRYPTO_SHA1:
		case CRYPTO_SHA1_HMAC:
		case CRYPTO_SHA2_224:
		case CRYPTO_SHA2_224_HMAC:
		case CRYPTO_SHA2_256:
		case CRYPTO_SHA2_256_HMAC:
		case CRYPTO_SHA2_384:
		case CRYPTO_SHA2_384_HMAC:
		case CRYPTO_SHA2_512:
		case CRYPTO_SHA2_512_HMAC:
			safexcel_instr_sha_hash(req, instr);
			break;
		case CRYPTO_AES_NIST_GMAC:
			safexcel_instr_gmac(req, instr, cdesc);
			break;
		default:
			panic("unhandled auth request %d", csp->csp_auth_alg);
		}
		break;
	}
}

static struct safexcel_res_descr *
safexcel_res_descr_add(struct safexcel_ring *ring, bool first, bool last,
    bus_addr_t data, uint32_t len)
{
	struct safexcel_res_descr *rdesc;
	struct safexcel_res_descr_ring *rring;

	mtx_assert(&ring->mtx, MA_OWNED);

	rring = &ring->rdr;
	if ((rring->write + 1) % SAFEXCEL_RING_SIZE == rring->read)
		return (NULL);

	rdesc = &rring->desc[rring->write];
	rring->write = (rring->write + 1) % SAFEXCEL_RING_SIZE;

	rdesc->particle_size = len;
	rdesc->rsvd0 = 0;
	rdesc->descriptor_overflow = 0;
	rdesc->buffer_overflow = 0;
	rdesc->last_seg = last;
	rdesc->first_seg = first;
	rdesc->result_size =
	    sizeof(struct safexcel_res_data) / sizeof(uint32_t);
	rdesc->rsvd1 = 0;
	rdesc->data_lo = SAFEXCEL_ADDR_LO(data);
	rdesc->data_hi = SAFEXCEL_ADDR_HI(data);

	if (first) {
		rdesc->result_data.packet_length = 0;
		rdesc->result_data.error_code = 0;
	}

	return (rdesc);
}

static struct safexcel_cmd_descr *
safexcel_cmd_descr_add(struct safexcel_ring *ring, bool first, bool last,
    bus_addr_t data, uint32_t seglen, uint32_t reqlen, bus_addr_t context)
{
	struct safexcel_cmd_descr *cdesc;
	struct safexcel_cmd_descr_ring *cring;

	KASSERT(reqlen <= SAFEXCEL_MAX_REQUEST_SIZE,
	    ("%s: request length %u too long", __func__, reqlen));
	mtx_assert(&ring->mtx, MA_OWNED);

	cring = &ring->cdr;
	if ((cring->write + 1) % SAFEXCEL_RING_SIZE == cring->read)
		return (NULL);

	cdesc = &cring->desc[cring->write];
	cring->write = (cring->write + 1) % SAFEXCEL_RING_SIZE;

	cdesc->particle_size = seglen;
	cdesc->rsvd0 = 0;
	cdesc->last_seg = last;
	cdesc->first_seg = first;
	cdesc->additional_cdata_size = 0;
	cdesc->rsvd1 = 0;
	cdesc->data_lo = SAFEXCEL_ADDR_LO(data);
	cdesc->data_hi = SAFEXCEL_ADDR_HI(data);
	if (first) {
		cdesc->control_data.packet_length = reqlen;
		cdesc->control_data.options = SAFEXCEL_OPTION_IP |
		    SAFEXCEL_OPTION_CP | SAFEXCEL_OPTION_CTX_CTRL_IN_CMD |
		    SAFEXCEL_OPTION_RC_AUTO;
		cdesc->control_data.type = SAFEXCEL_TOKEN_TYPE_BYPASS;
		cdesc->control_data.context_lo = SAFEXCEL_ADDR_LO(context) |
		    SAFEXCEL_CONTEXT_SMALL;
		cdesc->control_data.context_hi = SAFEXCEL_ADDR_HI(context);
	}

	return (cdesc);
}

static void
safexcel_cmd_descr_rollback(struct safexcel_ring *ring, int count)
{
	struct safexcel_cmd_descr_ring *cring;

	mtx_assert(&ring->mtx, MA_OWNED);

	cring = &ring->cdr;
	cring->write -= count;
	if (cring->write < 0)
		cring->write += SAFEXCEL_RING_SIZE;
}

static void
safexcel_res_descr_rollback(struct safexcel_ring *ring, int count)
{
	struct safexcel_res_descr_ring *rring;

	mtx_assert(&ring->mtx, MA_OWNED);

	rring = &ring->rdr;
	rring->write -= count;
	if (rring->write < 0)
		rring->write += SAFEXCEL_RING_SIZE;
}

static void
safexcel_append_segs(bus_dma_segment_t *segs, int nseg, struct sglist *sg,
    int start, int len)
{
	bus_dma_segment_t *seg;
	size_t seglen;
	int error, i;

	for (i = 0; i < nseg && len > 0; i++) {
		seg = &segs[i];

		if (seg->ds_len <= start) {
			start -= seg->ds_len;
			continue;
		}

		seglen = MIN(len, seg->ds_len - start);
		error = sglist_append_phys(sg, seg->ds_addr + start, seglen);
		if (error != 0)
			panic("%s: ran out of segments: %d", __func__, error);
		len -= seglen;
		start = 0;
	}
}

static void
safexcel_create_chain_cb(void *arg, bus_dma_segment_t *segs, int nseg,
    int error)
{
	const struct crypto_session_params *csp;
	struct cryptop *crp;
	struct safexcel_cmd_descr *cdesc;
	struct safexcel_request *req;
	struct safexcel_ring *ring;
	struct safexcel_session *sess;
	struct sglist *sg;
	size_t inlen;
	int i;
	bool first, last;

	req = arg;
	if (error != 0) {
		req->error = error;
		return;
	}

	crp = req->crp;
	csp = crypto_get_params(crp->crp_session);
	sess = req->sess;
	ring = &req->sc->sc_ring[sess->ringidx];

	mtx_assert(&ring->mtx, MA_OWNED);

	/*
	 * Set up descriptors for input and output data.
	 *
	 * The processing engine programs require that any AAD comes first,
	 * followed by the cipher plaintext, followed by the digest.  Some
	 * consumers place the digest first in the input buffer, in which case
	 * we have to create an extra descriptor.
	 *
	 * As an optimization, unmodified data is not passed to the output
	 * stream.
	 */
	sglist_reset(ring->cmd_data);
	sglist_reset(ring->res_data);
	if (crp->crp_aad_length != 0) {
		safexcel_append_segs(segs, nseg, ring->cmd_data,
		    crp->crp_aad_start, crp->crp_aad_length);
	}
	safexcel_append_segs(segs, nseg, ring->cmd_data,
	    crp->crp_payload_start, crp->crp_payload_length);
	if (csp->csp_cipher_alg != 0) {
		safexcel_append_segs(segs, nseg, ring->res_data,
		    crp->crp_payload_start, crp->crp_payload_length);
	}
	if (sess->digestlen > 0) {
		if ((crp->crp_op & CRYPTO_OP_VERIFY_DIGEST) != 0) {
			safexcel_append_segs(segs, nseg, ring->cmd_data,
			    crp->crp_digest_start, sess->digestlen);
		} else {
			safexcel_append_segs(segs, nseg, ring->res_data,
			    crp->crp_digest_start, sess->digestlen);
		}
	}

	sg = ring->cmd_data;
	if (sg->sg_nseg == 0) {
		/*
		 * Fake a segment for the command descriptor if the input has
		 * length zero.  The EIP97 apparently does not handle
		 * zero-length packets properly since subsequent requests return
		 * bogus errors, so provide a dummy segment using the context
		 * descriptor.
		 */
		(void)sglist_append_phys(sg, req->ctx.paddr, 1);
	}
	for (i = 0, inlen = 0; i < sg->sg_nseg; i++)
		inlen += sg->sg_segs[i].ss_len;
	for (i = 0; i < sg->sg_nseg; i++) {
		first = i == 0;
		last = i == sg->sg_nseg - 1;

		cdesc = safexcel_cmd_descr_add(ring, first, last,
		    sg->sg_segs[i].ss_paddr, sg->sg_segs[i].ss_len,
		    (uint32_t)inlen, req->ctx.paddr);
		if (cdesc == NULL) {
			safexcel_cmd_descr_rollback(ring, i);
			req->error = EAGAIN;
			return;
		}
		if (i == 0)
			req->cdesc = cdesc;
	}
	req->cdescs = sg->sg_nseg;

	sg = ring->res_data;
	if (sg->sg_nseg == 0) {
		/*
		 * We need a result descriptor even if the output stream will be
		 * empty, for example when verifying an AAD digest.
		 */
		sg->sg_segs[0].ss_paddr = 0;
		sg->sg_segs[0].ss_len = 0;
		sg->sg_nseg = 1;
	}
	for (i = 0; i < sg->sg_nseg; i++) {
		first = i == 0;
		last = i == sg->sg_nseg - 1;

		if (safexcel_res_descr_add(ring, first, last,
		    sg->sg_segs[i].ss_paddr, sg->sg_segs[i].ss_len) == NULL) {
			safexcel_cmd_descr_rollback(ring,
			    ring->cmd_data->sg_nseg);
			safexcel_res_descr_rollback(ring, i);
			req->error = EAGAIN;
			return;
		}
	}
	req->rdescs = sg->sg_nseg;
}

static int
safexcel_create_chain(struct safexcel_ring *ring, struct safexcel_request *req)
{
	int error;

	req->error = 0;
	req->cdescs = req->rdescs = 0;

	error = bus_dmamap_load_crp(ring->data_dtag, req->dmap, req->crp,
	    safexcel_create_chain_cb, req, BUS_DMA_NOWAIT);
	if (error == 0)
		req->dmap_loaded = true;

	if (req->error != 0)
		error = req->error;

	return (error);
}

static bool
safexcel_probe_cipher(const struct crypto_session_params *csp)
{
	switch (csp->csp_cipher_alg) {
	case CRYPTO_AES_CBC:
	case CRYPTO_AES_ICM:
		if (csp->csp_ivlen != AES_BLOCK_LEN)
			return (false);
		break;
	case CRYPTO_AES_XTS:
		if (csp->csp_ivlen != AES_XTS_IV_LEN)
			return (false);
		break;
	default:
		return (false);
	}

	return (true);
}

/*
 * Determine whether the driver can implement a session with the requested
 * parameters.
 */
static int
safexcel_probesession(device_t dev, const struct crypto_session_params *csp)
{
	switch (csp->csp_mode) {
	case CSP_MODE_CIPHER:
		if (!safexcel_probe_cipher(csp))
			return (EINVAL);
		break;
	case CSP_MODE_DIGEST:
		switch (csp->csp_auth_alg) {
		case CRYPTO_AES_NIST_GMAC:
			if (csp->csp_ivlen != AES_GCM_IV_LEN)
				return (EINVAL);
			break;
		case CRYPTO_SHA1:
		case CRYPTO_SHA1_HMAC:
		case CRYPTO_SHA2_224:
		case CRYPTO_SHA2_224_HMAC:
		case CRYPTO_SHA2_256:
		case CRYPTO_SHA2_256_HMAC:
		case CRYPTO_SHA2_384:
		case CRYPTO_SHA2_384_HMAC:
		case CRYPTO_SHA2_512:
		case CRYPTO_SHA2_512_HMAC:
			break;
		default:
			return (EINVAL);
		}
		break;
	case CSP_MODE_AEAD:
		switch (csp->csp_cipher_alg) {
		case CRYPTO_AES_NIST_GCM_16:
			if (csp->csp_ivlen != AES_GCM_IV_LEN)
				return (EINVAL);
			break;
		case CRYPTO_AES_CCM_16:
			if (csp->csp_ivlen != AES_CCM_IV_LEN)
				return (EINVAL);
			break;
		default:
			return (EINVAL);
		}
		break;
	case CSP_MODE_ETA:
		if (!safexcel_probe_cipher(csp))
			return (EINVAL);
		switch (csp->csp_cipher_alg) {
		case CRYPTO_AES_CBC:
		case CRYPTO_AES_ICM:
			/*
			 * The EIP-97 does not support combining AES-XTS with
			 * hash operations.
			 */
			if (csp->csp_auth_alg != CRYPTO_SHA1_HMAC &&
			    csp->csp_auth_alg != CRYPTO_SHA2_224_HMAC &&
			    csp->csp_auth_alg != CRYPTO_SHA2_256_HMAC &&
			    csp->csp_auth_alg != CRYPTO_SHA2_384_HMAC &&
			    csp->csp_auth_alg != CRYPTO_SHA2_512_HMAC)
				return (EINVAL);
			break;
		default:
			return (EINVAL);
		}
		break;
	default:
		return (EINVAL);
	}

	return (CRYPTODEV_PROBE_HARDWARE);
}

/*
 * Pre-compute the hash key used in GHASH, which is a block of zeroes encrypted
 * using the cipher key.
 */
static void
safexcel_setkey_ghash(struct safexcel_session *sess, const uint8_t *key,
    int klen)
{
	uint32_t ks[4 * (RIJNDAEL_MAXNR + 1)];
	uint8_t zeros[AES_BLOCK_LEN];
	int i, rounds;

	memset(zeros, 0, sizeof(zeros));

	rounds = rijndaelKeySetupEnc(ks, key, klen * NBBY);
	rijndaelEncrypt(ks, rounds, zeros, (uint8_t *)sess->ghash_key);
	for (i = 0; i < GMAC_BLOCK_LEN / sizeof(uint32_t); i++)
		sess->ghash_key[i] = htobe32(sess->ghash_key[i]);

	explicit_bzero(ks, sizeof(ks));
}

/*
 * Pre-compute the combined CBC-MAC key, which consists of three keys K1, K2, K3
 * in the hardware implementation.  K1 is the cipher key and comes last in the
 * buffer since K2 and K3 have a fixed size of AES_BLOCK_LEN.  For now XCBC-MAC
 * is not implemented so K2 and K3 are fixed.
 */
static void
safexcel_setkey_xcbcmac(struct safexcel_session *sess, const uint8_t *key,
    int klen)
{
	int i, off;

	memset(sess->xcbc_key, 0, sizeof(sess->xcbc_key));
	off = 2 * AES_BLOCK_LEN / sizeof(uint32_t);
	for (i = 0; i < klen / sizeof(uint32_t); i++, key += 4)
		sess->xcbc_key[i + off] = htobe32(le32dec(key));
}

static void
safexcel_setkey_hmac_digest(struct auth_hash *ahash, union authctx *ctx,
    char *buf)
{
	int hashwords, i;

	switch (ahash->type) {
	case CRYPTO_SHA1_HMAC:
		hashwords = ahash->hashsize / sizeof(uint32_t);
		for (i = 0; i < hashwords; i++)
			((uint32_t *)buf)[i] = htobe32(ctx->sha1ctx.h.b32[i]);
		break;
	case CRYPTO_SHA2_224_HMAC:
		hashwords = auth_hash_hmac_sha2_256.hashsize / sizeof(uint32_t);
		for (i = 0; i < hashwords; i++)
			((uint32_t *)buf)[i] = htobe32(ctx->sha224ctx.state[i]);
		break;
	case CRYPTO_SHA2_256_HMAC:
		hashwords = ahash->hashsize / sizeof(uint32_t);
		for (i = 0; i < hashwords; i++)
			((uint32_t *)buf)[i] = htobe32(ctx->sha256ctx.state[i]);
		break;
	case CRYPTO_SHA2_384_HMAC:
		hashwords = auth_hash_hmac_sha2_512.hashsize / sizeof(uint64_t);
		for (i = 0; i < hashwords; i++)
			((uint64_t *)buf)[i] = htobe64(ctx->sha384ctx.state[i]);
		break;
	case CRYPTO_SHA2_512_HMAC:
		hashwords = ahash->hashsize / sizeof(uint64_t);
		for (i = 0; i < hashwords; i++)
			((uint64_t *)buf)[i] = htobe64(ctx->sha512ctx.state[i]);
		break;
	}
}

/*
 * Pre-compute the inner and outer digests used in the HMAC algorithm.
 */
static void
safexcel_setkey_hmac(const struct crypto_session_params *csp,
    struct safexcel_session *sess, const uint8_t *key, int klen)
{
	union authctx ctx;
	struct auth_hash *ahash;

	ahash = crypto_auth_hash(csp);
	hmac_init_ipad(ahash, key, klen, &ctx);
	safexcel_setkey_hmac_digest(ahash, &ctx, sess->hmac_ipad);
	hmac_init_opad(ahash, key, klen, &ctx);
	safexcel_setkey_hmac_digest(ahash, &ctx, sess->hmac_opad);
	explicit_bzero(&ctx, ahash->ctxsize);
}

static void
safexcel_setkey_xts(struct safexcel_session *sess, const uint8_t *key, int klen)
{
	memcpy(sess->tweak_key, key + klen / 2, klen / 2);
}

static void
safexcel_setkey(struct safexcel_session *sess,
    const struct crypto_session_params *csp, struct cryptop *crp)
{
	const uint8_t *akey, *ckey;
	int aklen, cklen;

	aklen = csp->csp_auth_klen;
	cklen = csp->csp_cipher_klen;
	akey = ckey = NULL;
	if (crp != NULL) {
		akey = crp->crp_auth_key;
		ckey = crp->crp_cipher_key;
	}
	if (akey == NULL)
		akey = csp->csp_auth_key;
	if (ckey == NULL)
		ckey = csp->csp_cipher_key;

	sess->klen = cklen;
	switch (csp->csp_cipher_alg) {
	case CRYPTO_AES_NIST_GCM_16:
		safexcel_setkey_ghash(sess, ckey, cklen);
		break;
	case CRYPTO_AES_CCM_16:
		safexcel_setkey_xcbcmac(sess, ckey, cklen);
		break;
	case CRYPTO_AES_XTS:
		safexcel_setkey_xts(sess, ckey, cklen);
		sess->klen /= 2;
		break;
	}

	switch (csp->csp_auth_alg) {
	case CRYPTO_SHA1_HMAC:
	case CRYPTO_SHA2_224_HMAC:
	case CRYPTO_SHA2_256_HMAC:
	case CRYPTO_SHA2_384_HMAC:
	case CRYPTO_SHA2_512_HMAC:
		safexcel_setkey_hmac(csp, sess, akey, aklen);
		break;
	case CRYPTO_AES_NIST_GMAC:
		sess->klen = aklen;
		safexcel_setkey_ghash(sess, akey, aklen);
		break;
	}
}

static uint32_t
safexcel_aes_algid(int keylen)
{
	switch (keylen) {
	case 16:
		return (SAFEXCEL_CONTROL0_CRYPTO_ALG_AES128);
	case 24:
		return (SAFEXCEL_CONTROL0_CRYPTO_ALG_AES192);
	case 32:
		return (SAFEXCEL_CONTROL0_CRYPTO_ALG_AES256);
	default:
		panic("invalid AES key length %d", keylen);
	}
}

static uint32_t
safexcel_aes_ccm_hashid(int keylen)
{
	switch (keylen) {
	case 16:
		return (SAFEXCEL_CONTROL0_HASH_ALG_XCBC128);
	case 24:
		return (SAFEXCEL_CONTROL0_HASH_ALG_XCBC192);
	case 32:
		return (SAFEXCEL_CONTROL0_HASH_ALG_XCBC256);
	default:
		panic("invalid AES key length %d", keylen);
	}
}

static uint32_t
safexcel_sha_hashid(int alg)
{
	switch (alg) {
	case CRYPTO_SHA1:
	case CRYPTO_SHA1_HMAC:
		return (SAFEXCEL_CONTROL0_HASH_ALG_SHA1);
	case CRYPTO_SHA2_224:
	case CRYPTO_SHA2_224_HMAC:
		return (SAFEXCEL_CONTROL0_HASH_ALG_SHA224);
	case CRYPTO_SHA2_256:
	case CRYPTO_SHA2_256_HMAC:
		return (SAFEXCEL_CONTROL0_HASH_ALG_SHA256);
	case CRYPTO_SHA2_384:
	case CRYPTO_SHA2_384_HMAC:
		return (SAFEXCEL_CONTROL0_HASH_ALG_SHA384);
	case CRYPTO_SHA2_512:
	case CRYPTO_SHA2_512_HMAC:
		return (SAFEXCEL_CONTROL0_HASH_ALG_SHA512);
	default:
		__assert_unreachable();
	}
}

static int
safexcel_sha_hashlen(int alg)
{
	switch (alg) {
	case CRYPTO_SHA1:
	case CRYPTO_SHA1_HMAC:
		return (SHA1_HASH_LEN);
	case CRYPTO_SHA2_224:
	case CRYPTO_SHA2_224_HMAC:
		return (SHA2_224_HASH_LEN);
	case CRYPTO_SHA2_256:
	case CRYPTO_SHA2_256_HMAC:
		return (SHA2_256_HASH_LEN);
	case CRYPTO_SHA2_384:
	case CRYPTO_SHA2_384_HMAC:
		return (SHA2_384_HASH_LEN);
	case CRYPTO_SHA2_512:
	case CRYPTO_SHA2_512_HMAC:
		return (SHA2_512_HASH_LEN);
	default:
		__assert_unreachable();
	}
}

static int
safexcel_sha_statelen(int alg)
{
	switch (alg) {
	case CRYPTO_SHA1:
	case CRYPTO_SHA1_HMAC:
		return (SHA1_HASH_LEN);
	case CRYPTO_SHA2_224:
	case CRYPTO_SHA2_224_HMAC:
	case CRYPTO_SHA2_256:
	case CRYPTO_SHA2_256_HMAC:
		return (SHA2_256_HASH_LEN);
	case CRYPTO_SHA2_384:
	case CRYPTO_SHA2_384_HMAC:
	case CRYPTO_SHA2_512:
	case CRYPTO_SHA2_512_HMAC:
		return (SHA2_512_HASH_LEN);
	default:
		__assert_unreachable();
	}
}

static int
safexcel_newsession(device_t dev, crypto_session_t cses,
    const struct crypto_session_params *csp)
{
	struct safexcel_session *sess;
	struct safexcel_softc *sc;

	sc = device_get_softc(dev);
	sess = crypto_get_driver_session(cses);

	switch (csp->csp_auth_alg) {
	case CRYPTO_SHA1:
	case CRYPTO_SHA2_224:
	case CRYPTO_SHA2_256:
	case CRYPTO_SHA2_384:
	case CRYPTO_SHA2_512:
		sess->digest = SAFEXCEL_CONTROL0_DIGEST_PRECOMPUTED;
		sess->hash = safexcel_sha_hashid(csp->csp_auth_alg);
		sess->digestlen = safexcel_sha_hashlen(csp->csp_auth_alg);
		sess->statelen = safexcel_sha_statelen(csp->csp_auth_alg);
		break;
	case CRYPTO_SHA1_HMAC:
	case CRYPTO_SHA2_224_HMAC:
	case CRYPTO_SHA2_256_HMAC:
	case CRYPTO_SHA2_384_HMAC:
	case CRYPTO_SHA2_512_HMAC:
		sess->digest = SAFEXCEL_CONTROL0_DIGEST_HMAC;
		sess->hash = safexcel_sha_hashid(csp->csp_auth_alg);
		sess->digestlen = safexcel_sha_hashlen(csp->csp_auth_alg);
		sess->statelen = safexcel_sha_statelen(csp->csp_auth_alg);
		break;
	case CRYPTO_AES_NIST_GMAC:
		sess->digest = SAFEXCEL_CONTROL0_DIGEST_GMAC;
		sess->digestlen = GMAC_DIGEST_LEN;
		sess->hash = SAFEXCEL_CONTROL0_HASH_ALG_GHASH;
		sess->alg = safexcel_aes_algid(csp->csp_auth_klen);
		sess->mode = SAFEXCEL_CONTROL1_CRYPTO_MODE_GCM;
		break;
	}

	switch (csp->csp_cipher_alg) {
	case CRYPTO_AES_NIST_GCM_16:
		sess->digest = SAFEXCEL_CONTROL0_DIGEST_GMAC;
		sess->digestlen = GMAC_DIGEST_LEN;
		sess->hash = SAFEXCEL_CONTROL0_HASH_ALG_GHASH;
		sess->alg = safexcel_aes_algid(csp->csp_cipher_klen);
		sess->mode = SAFEXCEL_CONTROL1_CRYPTO_MODE_GCM;
		break;
	case CRYPTO_AES_CCM_16:
		sess->hash = safexcel_aes_ccm_hashid(csp->csp_cipher_klen);
		sess->digest = SAFEXCEL_CONTROL0_DIGEST_CCM;
		sess->digestlen = CCM_CBC_MAX_DIGEST_LEN;
		sess->alg = safexcel_aes_algid(csp->csp_cipher_klen);
		sess->mode = SAFEXCEL_CONTROL1_CRYPTO_MODE_CCM;
		break;
	case CRYPTO_AES_CBC:
		sess->alg = safexcel_aes_algid(csp->csp_cipher_klen);
		sess->mode = SAFEXCEL_CONTROL1_CRYPTO_MODE_CBC;
		break;
	case CRYPTO_AES_ICM:
		sess->alg = safexcel_aes_algid(csp->csp_cipher_klen);
		sess->mode = SAFEXCEL_CONTROL1_CRYPTO_MODE_CTR;
		break;
	case CRYPTO_AES_XTS:
		sess->alg = safexcel_aes_algid(csp->csp_cipher_klen / 2);
		sess->mode = SAFEXCEL_CONTROL1_CRYPTO_MODE_XTS;
		break;
	}

	if (csp->csp_auth_mlen != 0)
		sess->digestlen = csp->csp_auth_mlen;

	safexcel_setkey(sess, csp, NULL);

	/* Bind each session to a fixed ring to minimize lock contention. */
	sess->ringidx = atomic_fetchadd_int(&sc->sc_ringidx, 1);
	sess->ringidx %= sc->sc_config.rings;

	return (0);
}

static int
safexcel_process(device_t dev, struct cryptop *crp, int hint)
{
	const struct crypto_session_params *csp;
	struct safexcel_request *req;
	struct safexcel_ring *ring;
	struct safexcel_session *sess;
	struct safexcel_softc *sc;
	int error;

	sc = device_get_softc(dev);
	sess = crypto_get_driver_session(crp->crp_session);
	csp = crypto_get_params(crp->crp_session);

	if (__predict_false(crypto_buffer_len(&crp->crp_buf) >
	    SAFEXCEL_MAX_REQUEST_SIZE)) {
		crp->crp_etype = E2BIG;
		crypto_done(crp);
		return (0);
	}

	if (crp->crp_cipher_key != NULL || crp->crp_auth_key != NULL)
		safexcel_setkey(sess, csp, crp);

	ring = &sc->sc_ring[sess->ringidx];
	mtx_lock(&ring->mtx);
	req = safexcel_alloc_request(sc, ring);
        if (__predict_false(req == NULL)) {
		ring->blocked = CRYPTO_SYMQ;
		mtx_unlock(&ring->mtx);
		return (ERESTART);
	}

	req->crp = crp;
	req->sess = sess;

	crypto_read_iv(crp, req->iv);

	error = safexcel_create_chain(ring, req);
	if (__predict_false(error != 0)) {
		safexcel_free_request(ring, req);
		mtx_unlock(&ring->mtx);
		crp->crp_etype = error;
		crypto_done(crp);
		return (0);
	}

	safexcel_set_token(req);

	bus_dmamap_sync(ring->data_dtag, req->dmap,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	bus_dmamap_sync(req->ctx.tag, req->ctx.map,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	bus_dmamap_sync(ring->cdr.dma.tag, ring->cdr.dma.map,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	bus_dmamap_sync(ring->dma_atok.tag, ring->dma_atok.map,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	bus_dmamap_sync(ring->rdr.dma.tag, ring->rdr.dma.map,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	safexcel_enqueue_request(sc, ring, req);

	if ((hint & CRYPTO_HINT_MORE) == 0)
		safexcel_execute(sc, ring, req);
	mtx_unlock(&ring->mtx);

	return (0);
}

static device_method_t safexcel_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		safexcel_probe),
	DEVMETHOD(device_attach,	safexcel_attach),
	DEVMETHOD(device_detach,	safexcel_detach),

	/* Cryptodev interface */
	DEVMETHOD(cryptodev_probesession, safexcel_probesession),
	DEVMETHOD(cryptodev_newsession,	safexcel_newsession),
	DEVMETHOD(cryptodev_process,	safexcel_process),

	DEVMETHOD_END
};

static devclass_t safexcel_devclass;

static driver_t safexcel_driver = {
	.name 		= "safexcel",
	.methods 	= safexcel_methods,
	.size		= sizeof(struct safexcel_softc),
};

DRIVER_MODULE(safexcel, simplebus, safexcel_driver, safexcel_devclass, 0, 0);
MODULE_VERSION(safexcel, 1);
MODULE_DEPEND(safexcel, crypto, 1, 1, 1);
