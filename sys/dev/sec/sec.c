/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (C) 2008-2009 Semihalf, Piotr Ziecik
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
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Freescale integrated Security Engine (SEC) driver. Currently SEC 2.0 and
 * 3.0 are supported.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/random.h>
#include <sys/rman.h>

#include <machine/_inttypes.h>
#include <machine/bus.h>
#include <machine/resource.h>

#include <opencrypto/cryptodev.h>
#include <opencrypto/xform_auth.h>
#include "cryptodev_if.h"

#include <dev/ofw/ofw_bus_subr.h>
#include <dev/sec/sec.h>

static int	sec_probe(device_t dev);
static int	sec_attach(device_t dev);
static int	sec_detach(device_t dev);
static int	sec_suspend(device_t dev);
static int	sec_resume(device_t dev);
static int	sec_shutdown(device_t dev);
static void	sec_primary_intr(void *arg);
static void	sec_secondary_intr(void *arg);
static int	sec_setup_intr(struct sec_softc *sc, struct resource **ires,
    void **ihand, int *irid, driver_intr_t handler, const char *iname);
static void	sec_release_intr(struct sec_softc *sc, struct resource *ires,
    void *ihand, int irid, const char *iname);
static int	sec_controller_reset(struct sec_softc *sc);
static int	sec_channel_reset(struct sec_softc *sc, int channel, int full);
static int	sec_init(struct sec_softc *sc);
static int	sec_alloc_dma_mem(struct sec_softc *sc,
    struct sec_dma_mem *dma_mem, bus_size_t size);
static int	sec_desc_map_dma(struct sec_softc *sc,
    struct sec_dma_mem *dma_mem, struct cryptop *crp, bus_size_t size,
    struct sec_desc_map_info *sdmi);
static void	sec_free_dma_mem(struct sec_dma_mem *dma_mem);
static void	sec_enqueue(struct sec_softc *sc);
static int	sec_enqueue_desc(struct sec_softc *sc, struct sec_desc *desc,
    int channel);
static int	sec_eu_channel(struct sec_softc *sc, int eu);
static int	sec_make_pointer(struct sec_softc *sc, struct sec_desc *desc,
    u_int n, struct cryptop *crp, bus_size_t doffset, bus_size_t dsize);
static int	sec_make_pointer_direct(struct sec_softc *sc,
    struct sec_desc *desc, u_int n, bus_addr_t data, bus_size_t dsize);
static int	sec_probesession(device_t dev,
    const struct crypto_session_params *csp);
static int	sec_newsession(device_t dev, crypto_session_t cses,
    const struct crypto_session_params *csp);
static int	sec_process(device_t dev, struct cryptop *crp, int hint);
static int	sec_build_common_ns_desc(struct sec_softc *sc,
    struct sec_desc *desc, const struct crypto_session_params *csp,
    struct cryptop *crp);
static int	sec_build_common_s_desc(struct sec_softc *sc,
    struct sec_desc *desc, const struct crypto_session_params *csp,
    struct cryptop *crp);

static struct sec_desc *sec_find_desc(struct sec_softc *sc, bus_addr_t paddr);

/* AESU */
static bool	sec_aesu_newsession(const struct crypto_session_params *csp);
static int	sec_aesu_make_desc(struct sec_softc *sc,
    const struct crypto_session_params *csp, struct sec_desc *desc,
    struct cryptop *crp);

/* MDEU */
static bool	sec_mdeu_can_handle(u_int alg);
static int	sec_mdeu_config(const struct crypto_session_params *csp,
    u_int *eu, u_int *mode, u_int *hashlen);
static bool	sec_mdeu_newsession(const struct crypto_session_params *csp);
static int	sec_mdeu_make_desc(struct sec_softc *sc,
    const struct crypto_session_params *csp, struct sec_desc *desc,
    struct cryptop *crp);

static device_method_t sec_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		sec_probe),
	DEVMETHOD(device_attach,	sec_attach),
	DEVMETHOD(device_detach,	sec_detach),

	DEVMETHOD(device_suspend,	sec_suspend),
	DEVMETHOD(device_resume,	sec_resume),
	DEVMETHOD(device_shutdown,	sec_shutdown),

	/* Crypto methods */
	DEVMETHOD(cryptodev_probesession, sec_probesession),
	DEVMETHOD(cryptodev_newsession,	sec_newsession),
	DEVMETHOD(cryptodev_process,	sec_process),

	DEVMETHOD_END
};
static driver_t sec_driver = {
	"sec",
	sec_methods,
	sizeof(struct sec_softc),
};

DRIVER_MODULE(sec, simplebus, sec_driver, 0, 0);
MODULE_DEPEND(sec, crypto, 1, 1, 1);

static struct sec_eu_methods sec_eus[] = {
	{
		sec_aesu_newsession,
		sec_aesu_make_desc,
	},
	{
		sec_mdeu_newsession,
		sec_mdeu_make_desc,
	},
	{ NULL, NULL }
};

static inline void
sec_sync_dma_mem(struct sec_dma_mem *dma_mem, bus_dmasync_op_t op)
{

	/* Sync only if dma memory is valid */
	if (dma_mem->dma_vaddr != NULL)
		bus_dmamap_sync(dma_mem->dma_tag, dma_mem->dma_map, op);
}

static inline void *
sec_get_pointer_data(struct sec_desc *desc, u_int n)
{

	return (desc->sd_ptr_dmem[n].dma_vaddr);
}

static int
sec_probe(device_t dev)
{
	struct sec_softc *sc;
	uint64_t id;

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_is_compatible(dev, "fsl,sec2.0"))
		return (ENXIO);

	sc = device_get_softc(dev);

	sc->sc_rrid = 0;
	sc->sc_rres = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &sc->sc_rrid,
	    RF_ACTIVE);

	if (sc->sc_rres == NULL)
		return (ENXIO);

	sc->sc_bas.bsh = rman_get_bushandle(sc->sc_rres);
	sc->sc_bas.bst = rman_get_bustag(sc->sc_rres);

	id = SEC_READ(sc, SEC_ID);

	bus_release_resource(dev, SYS_RES_MEMORY, sc->sc_rrid, sc->sc_rres);

	switch (id) {
	case SEC_20_ID:
		device_set_desc(dev, "Freescale Security Engine 2.0");
		sc->sc_version = 2;
		break;
	case SEC_30_ID:
		device_set_desc(dev, "Freescale Security Engine 3.0");
		sc->sc_version = 3;
		break;
	case SEC_31_ID:
		device_set_desc(dev, "Freescale Security Engine 3.1");
		sc->sc_version = 3;
		break;
	default:
		device_printf(dev, "unknown SEC ID 0x%016"PRIx64"!\n", id);
		return (ENXIO);
	}

	return (0);
}

static int
sec_attach(device_t dev)
{
	struct sec_softc *sc;
	struct sec_hw_lt *lt;
	int error = 0;
	int i;

	sc = device_get_softc(dev);
	sc->sc_dev = dev;
	sc->sc_blocked = 0;
	sc->sc_shutdown = 0;

	sc->sc_cid = crypto_get_driverid(dev, sizeof(struct sec_session),
	    CRYPTOCAP_F_HARDWARE);
	if (sc->sc_cid < 0) {
		device_printf(dev, "could not get crypto driver ID!\n");
		return (ENXIO);
	}

	/* Init locks */
	mtx_init(&sc->sc_controller_lock, device_get_nameunit(dev),
	    "SEC Controller lock", MTX_DEF);
	mtx_init(&sc->sc_descriptors_lock, device_get_nameunit(dev),
	    "SEC Descriptors lock", MTX_DEF);

	/* Allocate I/O memory for SEC registers */
	sc->sc_rrid = 0;
	sc->sc_rres = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &sc->sc_rrid,
	    RF_ACTIVE);

	if (sc->sc_rres == NULL) {
		device_printf(dev, "could not allocate I/O memory!\n");
		goto fail1;
	}

	sc->sc_bas.bsh = rman_get_bushandle(sc->sc_rres);
	sc->sc_bas.bst = rman_get_bustag(sc->sc_rres);

	/* Setup interrupts */
	sc->sc_pri_irid = 0;
	error = sec_setup_intr(sc, &sc->sc_pri_ires, &sc->sc_pri_ihand,
	    &sc->sc_pri_irid, sec_primary_intr, "primary");

	if (error)
		goto fail2;

	if (sc->sc_version == 3) {
		sc->sc_sec_irid = 1;
		error = sec_setup_intr(sc, &sc->sc_sec_ires, &sc->sc_sec_ihand,
		    &sc->sc_sec_irid, sec_secondary_intr, "secondary");

		if (error)
			goto fail3;
	}

	/* Alloc DMA memory for descriptors and link tables */
	error = sec_alloc_dma_mem(sc, &(sc->sc_desc_dmem),
	    SEC_DESCRIPTORS * sizeof(struct sec_hw_desc));

	if (error)
		goto fail4;

	error = sec_alloc_dma_mem(sc, &(sc->sc_lt_dmem),
	    (SEC_LT_ENTRIES + 1) * sizeof(struct sec_hw_lt));

	if (error)
		goto fail5;

	/* Fill in descriptors and link tables */
	for (i = 0; i < SEC_DESCRIPTORS; i++) {
		sc->sc_desc[i].sd_desc =
		    (struct sec_hw_desc*)(sc->sc_desc_dmem.dma_vaddr) + i;
		sc->sc_desc[i].sd_desc_paddr = sc->sc_desc_dmem.dma_paddr +
		    (i * sizeof(struct sec_hw_desc));
	}

	for (i = 0; i < SEC_LT_ENTRIES + 1; i++) {
		sc->sc_lt[i].sl_lt =
		    (struct sec_hw_lt*)(sc->sc_lt_dmem.dma_vaddr) + i;
		sc->sc_lt[i].sl_lt_paddr = sc->sc_lt_dmem.dma_paddr +
		    (i * sizeof(struct sec_hw_lt));
	}

	/* Last entry in link table is used to create a circle */
	lt = sc->sc_lt[SEC_LT_ENTRIES].sl_lt;
	lt->shl_length = 0;
	lt->shl_r = 0;
	lt->shl_n = 1;
	lt->shl_ptr = sc->sc_lt[0].sl_lt_paddr;

	/* Init descriptor and link table queues pointers */
	SEC_CNT_INIT(sc, sc_free_desc_get_cnt, SEC_DESCRIPTORS);
	SEC_CNT_INIT(sc, sc_free_desc_put_cnt, SEC_DESCRIPTORS);
	SEC_CNT_INIT(sc, sc_ready_desc_get_cnt, SEC_DESCRIPTORS);
	SEC_CNT_INIT(sc, sc_ready_desc_put_cnt, SEC_DESCRIPTORS);
	SEC_CNT_INIT(sc, sc_queued_desc_get_cnt, SEC_DESCRIPTORS);
	SEC_CNT_INIT(sc, sc_queued_desc_put_cnt, SEC_DESCRIPTORS);
	SEC_CNT_INIT(sc, sc_lt_alloc_cnt, SEC_LT_ENTRIES);
	SEC_CNT_INIT(sc, sc_lt_free_cnt, SEC_LT_ENTRIES);

	/* Create masks for fast checks */
	sc->sc_int_error_mask = 0;
	for (i = 0; i < SEC_CHANNELS; i++)
		sc->sc_int_error_mask |= (~0ULL & SEC_INT_CH_ERR(i));

	switch (sc->sc_version) {
	case 2:
		sc->sc_channel_idle_mask =
		    (SEC_CHAN_CSR2_FFLVL_M << SEC_CHAN_CSR2_FFLVL_S) |
		    (SEC_CHAN_CSR2_MSTATE_M << SEC_CHAN_CSR2_MSTATE_S) |
		    (SEC_CHAN_CSR2_PSTATE_M << SEC_CHAN_CSR2_PSTATE_S) |
		    (SEC_CHAN_CSR2_GSTATE_M << SEC_CHAN_CSR2_GSTATE_S);
		break;
	case 3:
		sc->sc_channel_idle_mask =
		    (SEC_CHAN_CSR3_FFLVL_M << SEC_CHAN_CSR3_FFLVL_S) |
		    (SEC_CHAN_CSR3_MSTATE_M << SEC_CHAN_CSR3_MSTATE_S) |
		    (SEC_CHAN_CSR3_PSTATE_M << SEC_CHAN_CSR3_PSTATE_S) |
		    (SEC_CHAN_CSR3_GSTATE_M << SEC_CHAN_CSR3_GSTATE_S);
		break;
	}

	/* Init hardware */
	error = sec_init(sc);

	if (error)
		goto fail6;

	return (0);

fail6:
	sec_free_dma_mem(&(sc->sc_lt_dmem));
fail5:
	sec_free_dma_mem(&(sc->sc_desc_dmem));
fail4:
	sec_release_intr(sc, sc->sc_sec_ires, sc->sc_sec_ihand,
	    sc->sc_sec_irid, "secondary");
fail3:
	sec_release_intr(sc, sc->sc_pri_ires, sc->sc_pri_ihand,
	    sc->sc_pri_irid, "primary");
fail2:
	bus_release_resource(dev, SYS_RES_MEMORY, sc->sc_rrid, sc->sc_rres);
fail1:
	mtx_destroy(&sc->sc_controller_lock);
	mtx_destroy(&sc->sc_descriptors_lock);

	return (ENXIO);
}

static int
sec_detach(device_t dev)
{
	struct sec_softc *sc = device_get_softc(dev);
	int i, error, timeout = SEC_TIMEOUT;

	/* Prepare driver to shutdown */
	SEC_LOCK(sc, descriptors);
	sc->sc_shutdown = 1;
	SEC_UNLOCK(sc, descriptors);

	/* Wait until all queued processing finishes */
	while (1) {
		SEC_LOCK(sc, descriptors);
		i = SEC_READY_DESC_CNT(sc) + SEC_QUEUED_DESC_CNT(sc);
		SEC_UNLOCK(sc, descriptors);

		if (i == 0)
			break;

		if (timeout < 0) {
			device_printf(dev, "queue flush timeout!\n");

			/* DMA can be still active - stop it */
			for (i = 0; i < SEC_CHANNELS; i++)
				sec_channel_reset(sc, i, 1);

			break;
		}

		timeout -= 1000;
		DELAY(1000);
	}

	/* Disable interrupts */
	SEC_WRITE(sc, SEC_IER, 0);

	/* Unregister from OCF */
	crypto_unregister_all(sc->sc_cid);

	/* Free DMA memory */
	for (i = 0; i < SEC_DESCRIPTORS; i++)
		SEC_DESC_FREE_POINTERS(&(sc->sc_desc[i]));

	sec_free_dma_mem(&(sc->sc_lt_dmem));
	sec_free_dma_mem(&(sc->sc_desc_dmem));

	/* Release interrupts */
	sec_release_intr(sc, sc->sc_pri_ires, sc->sc_pri_ihand,
	    sc->sc_pri_irid, "primary");
	sec_release_intr(sc, sc->sc_sec_ires, sc->sc_sec_ihand,
	    sc->sc_sec_irid, "secondary");

	/* Release memory */
	if (sc->sc_rres) {
		error = bus_release_resource(dev, SYS_RES_MEMORY, sc->sc_rrid,
		    sc->sc_rres);
		if (error)
			device_printf(dev, "bus_release_resource() failed for"
			    " I/O memory, error %d\n", error);

		sc->sc_rres = NULL;
	}

	mtx_destroy(&sc->sc_controller_lock);
	mtx_destroy(&sc->sc_descriptors_lock);

	return (0);
}

static int
sec_suspend(device_t dev)
{

	return (0);
}

static int
sec_resume(device_t dev)
{

	return (0);
}

static int
sec_shutdown(device_t dev)
{

	return (0);
}

static int
sec_setup_intr(struct sec_softc *sc, struct resource **ires, void **ihand,
    int *irid, driver_intr_t handler, const char *iname)
{
	int error;

	(*ires) = bus_alloc_resource_any(sc->sc_dev, SYS_RES_IRQ, irid,
	    RF_ACTIVE);

	if ((*ires) == NULL) {
		device_printf(sc->sc_dev, "could not allocate %s IRQ\n", iname);
		return (ENXIO);
	}

	error = bus_setup_intr(sc->sc_dev, *ires, INTR_MPSAFE | INTR_TYPE_NET,
	    NULL, handler, sc, ihand);

	if (error) {
		device_printf(sc->sc_dev, "failed to set up %s IRQ\n", iname);
		if (bus_release_resource(sc->sc_dev, SYS_RES_IRQ, *irid, *ires))
			device_printf(sc->sc_dev, "could not release %s IRQ\n",
			    iname);

		(*ires) = NULL;
		return (error);
	}

	return (0);
}

static void
sec_release_intr(struct sec_softc *sc, struct resource *ires, void *ihand,
    int irid, const char *iname)
{
	int error;

	if (ires == NULL)
		return;

	error = bus_teardown_intr(sc->sc_dev, ires, ihand);
	if (error)
		device_printf(sc->sc_dev, "bus_teardown_intr() failed for %s"
		    " IRQ, error %d\n", iname, error);

	error = bus_release_resource(sc->sc_dev, SYS_RES_IRQ, irid, ires);
	if (error)
		device_printf(sc->sc_dev, "bus_release_resource() failed for %s"
		    " IRQ, error %d\n", iname, error);
}

static void
sec_primary_intr(void *arg)
{
	struct sec_session *ses;
	struct sec_softc *sc = arg;
	struct sec_desc *desc;
	struct cryptop *crp;
	uint64_t isr;
	uint8_t hash[HASH_MAX_LEN];
	int i, wakeup = 0;

	SEC_LOCK(sc, controller);

	/* Check for errors */
	isr = SEC_READ(sc, SEC_ISR);
	if (isr & sc->sc_int_error_mask) {
		/* Check each channel for error */
		for (i = 0; i < SEC_CHANNELS; i++) {
			if ((isr & SEC_INT_CH_ERR(i)) == 0)
				continue;

			device_printf(sc->sc_dev,
			    "I/O error on channel %i!\n", i);

			/* Find and mark problematic descriptor */
			desc = sec_find_desc(sc, SEC_READ(sc,
			    SEC_CHAN_CDPR(i)));

			if (desc != NULL)
				desc->sd_error = EIO;

			/* Do partial channel reset */
			sec_channel_reset(sc, i, 0);
		}
	}

	/* ACK interrupt */
	SEC_WRITE(sc, SEC_ICR, 0xFFFFFFFFFFFFFFFFULL);

	SEC_UNLOCK(sc, controller);
	SEC_LOCK(sc, descriptors);

	/* Handle processed descriptors */
	SEC_DESC_SYNC(sc, BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	while (SEC_QUEUED_DESC_CNT(sc) > 0) {
		desc = SEC_GET_QUEUED_DESC(sc);

		if (desc->sd_desc->shd_done != 0xFF && desc->sd_error == 0) {
			SEC_PUT_BACK_QUEUED_DESC(sc);
			break;
		}

		SEC_DESC_SYNC_POINTERS(desc, BUS_DMASYNC_PREREAD |
		    BUS_DMASYNC_PREWRITE);

		crp = desc->sd_crp;
		crp->crp_etype = desc->sd_error;
		if (crp->crp_etype == 0) {
			ses = crypto_get_driver_session(crp->crp_session);
			if (ses->ss_mlen != 0) {
				if (crp->crp_op & CRYPTO_OP_VERIFY_DIGEST) {
					crypto_copydata(crp,
					    crp->crp_digest_start,
					    ses->ss_mlen, hash);
					if (timingsafe_bcmp(
					    desc->sd_desc->shd_digest,
					    hash, ses->ss_mlen) != 0)
						crp->crp_etype = EBADMSG;
				} else
					crypto_copyback(crp,
					    crp->crp_digest_start,
					    ses->ss_mlen,
					    desc->sd_desc->shd_digest);
			}
		}
		crypto_done(desc->sd_crp);

		SEC_DESC_FREE_POINTERS(desc);
		SEC_DESC_FREE_LT(sc, desc);
		SEC_DESC_QUEUED2FREE(sc);
	}

	SEC_DESC_SYNC(sc, BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	if (!sc->sc_shutdown) {
		wakeup = sc->sc_blocked;
		sc->sc_blocked = 0;
	}

	SEC_UNLOCK(sc, descriptors);

	/* Enqueue ready descriptors in hardware */
	sec_enqueue(sc);

	if (wakeup)
		crypto_unblock(sc->sc_cid, wakeup);
}

static void
sec_secondary_intr(void *arg)
{
	struct sec_softc *sc = arg;

	device_printf(sc->sc_dev, "spurious secondary interrupt!\n");
	sec_primary_intr(arg);
}

static int
sec_controller_reset(struct sec_softc *sc)
{
	int timeout = SEC_TIMEOUT;

	/* Reset Controller */
	SEC_WRITE(sc, SEC_MCR, SEC_MCR_SWR);

	while (SEC_READ(sc, SEC_MCR) & SEC_MCR_SWR) {
		DELAY(1000);
		timeout -= 1000;

		if (timeout < 0) {
			device_printf(sc->sc_dev, "timeout while waiting for "
			    "device reset!\n");
			return (ETIMEDOUT);
		}
	}

	return (0);
}

static int
sec_channel_reset(struct sec_softc *sc, int channel, int full)
{
	int timeout = SEC_TIMEOUT;
	uint64_t bit = (full) ? SEC_CHAN_CCR_R : SEC_CHAN_CCR_CON;
	uint64_t reg;

	/* Reset Channel */
	reg = SEC_READ(sc, SEC_CHAN_CCR(channel));
	SEC_WRITE(sc, SEC_CHAN_CCR(channel), reg | bit);

	while (SEC_READ(sc, SEC_CHAN_CCR(channel)) & bit) {
		DELAY(1000);
		timeout -= 1000;

		if (timeout < 0) {
			device_printf(sc->sc_dev, "timeout while waiting for "
			    "channel reset!\n");
			return (ETIMEDOUT);
		}
	}

	if (full) {
		reg = SEC_CHAN_CCR_CDIE | SEC_CHAN_CCR_NT | SEC_CHAN_CCR_BS;

		switch(sc->sc_version) {
		case 2:
			reg |= SEC_CHAN_CCR_CDWE;
			break;
		case 3:
			reg |= SEC_CHAN_CCR_AWSE | SEC_CHAN_CCR_WGN;
			break;
		}

		SEC_WRITE(sc, SEC_CHAN_CCR(channel), reg);
	}

	return (0);
}

static int
sec_init(struct sec_softc *sc)
{
	uint64_t reg;
	int error, i;

	/* Reset controller twice to clear all pending interrupts */
	error = sec_controller_reset(sc);
	if (error)
		return (error);

	error = sec_controller_reset(sc);
	if (error)
		return (error);

	/* Reset channels */
	for (i = 0; i < SEC_CHANNELS; i++) {
		error = sec_channel_reset(sc, i, 1);
		if (error)
			return (error);
	}

	/* Enable Interrupts */
	reg = SEC_INT_ITO;
	for (i = 0; i < SEC_CHANNELS; i++)
		reg |= SEC_INT_CH_DN(i) | SEC_INT_CH_ERR(i);

	SEC_WRITE(sc, SEC_IER, reg);

	return (error);
}

static void
sec_alloc_dma_mem_cb(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	struct sec_dma_mem *dma_mem = arg;

	if (error)
		return;

	KASSERT(nseg == 1, ("Wrong number of segments, should be 1"));
	dma_mem->dma_paddr = segs->ds_addr;
}

static void
sec_dma_map_desc_cb(void *arg, bus_dma_segment_t *segs, int nseg,
    int error)
{
	struct sec_desc_map_info *sdmi = arg;
	struct sec_softc *sc = sdmi->sdmi_sc;
	struct sec_lt *lt = NULL;
	bus_addr_t addr;
	bus_size_t size;
	int i;

	SEC_LOCK_ASSERT(sc, descriptors);

	if (error)
		return;

	for (i = 0; i < nseg; i++) {
		addr = segs[i].ds_addr;
		size = segs[i].ds_len;

		/* Skip requested offset */
		if (sdmi->sdmi_offset >= size) {
			sdmi->sdmi_offset -= size;
			continue;
		}

		addr += sdmi->sdmi_offset;
		size -= sdmi->sdmi_offset;
		sdmi->sdmi_offset = 0;

		/* Do not link more than requested */
		if (sdmi->sdmi_size < size)
			size = sdmi->sdmi_size;

		lt = SEC_ALLOC_LT_ENTRY(sc);
		lt->sl_lt->shl_length = size;
		lt->sl_lt->shl_r = 0;
		lt->sl_lt->shl_n = 0;
		lt->sl_lt->shl_ptr = addr;

		if (sdmi->sdmi_lt_first == NULL)
			sdmi->sdmi_lt_first = lt;

		sdmi->sdmi_lt_used += 1;

		if ((sdmi->sdmi_size -= size) == 0)
			break;
	}

	sdmi->sdmi_lt_last = lt;
}

static int
sec_alloc_dma_mem(struct sec_softc *sc, struct sec_dma_mem *dma_mem,
    bus_size_t size)
{
	int error;

	if (dma_mem->dma_vaddr != NULL)
		return (EBUSY);

	error = bus_dma_tag_create(NULL,	/* parent */
		SEC_DMA_ALIGNMENT, 0,		/* alignment, boundary */
		BUS_SPACE_MAXADDR_32BIT,	/* lowaddr */
		BUS_SPACE_MAXADDR,		/* highaddr */
		NULL, NULL,			/* filtfunc, filtfuncarg */
		size, 1,			/* maxsize, nsegments */
		size, 0,			/* maxsegsz, flags */
		NULL, NULL,			/* lockfunc, lockfuncarg */
		&(dma_mem->dma_tag));		/* dmat */

	if (error) {
		device_printf(sc->sc_dev, "failed to allocate busdma tag, error"
		    " %i!\n", error);
		goto err1;
	}

	error = bus_dmamem_alloc(dma_mem->dma_tag, &(dma_mem->dma_vaddr),
	    BUS_DMA_NOWAIT | BUS_DMA_ZERO, &(dma_mem->dma_map));

	if (error) {
		device_printf(sc->sc_dev, "failed to allocate DMA safe"
		    " memory, error %i!\n", error);
		goto err2;
	}

	error = bus_dmamap_load(dma_mem->dma_tag, dma_mem->dma_map,
		    dma_mem->dma_vaddr, size, sec_alloc_dma_mem_cb, dma_mem,
		    BUS_DMA_NOWAIT);

	if (error) {
		device_printf(sc->sc_dev, "cannot get address of the DMA"
		    " memory, error %i\n", error);
		goto err3;
	}

	dma_mem->dma_is_map = 0;
	return (0);

err3:
	bus_dmamem_free(dma_mem->dma_tag, dma_mem->dma_vaddr, dma_mem->dma_map);
err2:
	bus_dma_tag_destroy(dma_mem->dma_tag);
err1:
	dma_mem->dma_vaddr = NULL;
	return(error);
}

static int
sec_desc_map_dma(struct sec_softc *sc, struct sec_dma_mem *dma_mem,
    struct cryptop *crp, bus_size_t size, struct sec_desc_map_info *sdmi)
{
	int error;

	if (dma_mem->dma_vaddr != NULL)
		return (EBUSY);

	switch (crp->crp_buf.cb_type) {
	case CRYPTO_BUF_CONTIG:
		break;
	case CRYPTO_BUF_UIO:
		size = SEC_FREE_LT_CNT(sc) * SEC_MAX_DMA_BLOCK_SIZE;
		break;
	case CRYPTO_BUF_MBUF:
		size = m_length(crp->crp_buf.cb_mbuf, NULL);
		break;
	case CRYPTO_BUF_SINGLE_MBUF:
		size = crp->crp_buf.cb_mbuf->m_len;
		break;
	case CRYPTO_BUF_VMPAGE:
		size = PAGE_SIZE - crp->crp_buf.cb_vm_page_offset;
		break;
	default:
		return (EINVAL);
	}

	error = bus_dma_tag_create(NULL,	/* parent */
		SEC_DMA_ALIGNMENT, 0,		/* alignment, boundary */
		BUS_SPACE_MAXADDR_32BIT,	/* lowaddr */
		BUS_SPACE_MAXADDR,		/* highaddr */
		NULL, NULL,			/* filtfunc, filtfuncarg */
		size,				/* maxsize */
		SEC_FREE_LT_CNT(sc),		/* nsegments */
		SEC_MAX_DMA_BLOCK_SIZE, 0,	/* maxsegsz, flags */
		NULL, NULL,			/* lockfunc, lockfuncarg */
		&(dma_mem->dma_tag));		/* dmat */

	if (error) {
		device_printf(sc->sc_dev, "failed to allocate busdma tag, error"
		    " %i!\n", error);
		dma_mem->dma_vaddr = NULL;
		return (error);
	}

	error = bus_dmamap_create(dma_mem->dma_tag, 0, &(dma_mem->dma_map));

	if (error) {
		device_printf(sc->sc_dev, "failed to create DMA map, error %i!"
		    "\n", error);
		bus_dma_tag_destroy(dma_mem->dma_tag);
		return (error);
	}

	error = bus_dmamap_load_crp(dma_mem->dma_tag, dma_mem->dma_map, crp,
	    sec_dma_map_desc_cb, sdmi, BUS_DMA_NOWAIT);

	if (error) {
		device_printf(sc->sc_dev, "cannot get address of the DMA"
		    " memory, error %i!\n", error);
		bus_dmamap_destroy(dma_mem->dma_tag, dma_mem->dma_map);
		bus_dma_tag_destroy(dma_mem->dma_tag);
		return (error);
	}

	dma_mem->dma_is_map = 1;
	dma_mem->dma_vaddr = crp;

	return (0);
}

static void
sec_free_dma_mem(struct sec_dma_mem *dma_mem)
{

	/* Check for double free */
	if (dma_mem->dma_vaddr == NULL)
		return;

	bus_dmamap_unload(dma_mem->dma_tag, dma_mem->dma_map);

	if (dma_mem->dma_is_map)
		bus_dmamap_destroy(dma_mem->dma_tag, dma_mem->dma_map);
	else
		bus_dmamem_free(dma_mem->dma_tag, dma_mem->dma_vaddr,
		    dma_mem->dma_map);

	bus_dma_tag_destroy(dma_mem->dma_tag);
	dma_mem->dma_vaddr = NULL;
}

static int
sec_eu_channel(struct sec_softc *sc, int eu)
{
	uint64_t reg;
	int channel = 0;

	SEC_LOCK_ASSERT(sc, controller);

	reg = SEC_READ(sc, SEC_EUASR);

	switch (eu) {
	case SEC_EU_AFEU:
		channel = SEC_EUASR_AFEU(reg);
		break;
	case SEC_EU_DEU:
		channel = SEC_EUASR_DEU(reg);
		break;
	case SEC_EU_MDEU_A:
	case SEC_EU_MDEU_B:
		channel = SEC_EUASR_MDEU(reg);
		break;
	case SEC_EU_RNGU:
		channel = SEC_EUASR_RNGU(reg);
		break;
	case SEC_EU_PKEU:
		channel = SEC_EUASR_PKEU(reg);
		break;
	case SEC_EU_AESU:
		channel = SEC_EUASR_AESU(reg);
		break;
	case SEC_EU_KEU:
		channel = SEC_EUASR_KEU(reg);
		break;
	case SEC_EU_CRCU:
		channel = SEC_EUASR_CRCU(reg);
		break;
	}

	return (channel - 1);
}

static int
sec_enqueue_desc(struct sec_softc *sc, struct sec_desc *desc, int channel)
{
	u_int fflvl = SEC_MAX_FIFO_LEVEL;
	uint64_t reg;
	int i;

	SEC_LOCK_ASSERT(sc, controller);

	/* Find free channel if have not got one */
	if (channel < 0) {
		for (i = 0; i < SEC_CHANNELS; i++) {
			reg = SEC_READ(sc, SEC_CHAN_CSR(channel));

			if ((reg & sc->sc_channel_idle_mask) == 0) {
				channel = i;
				break;
			}
		}
	}

	/* There is no free channel */
	if (channel < 0)
		return (-1);

	/* Check FIFO level on selected channel */
	reg = SEC_READ(sc, SEC_CHAN_CSR(channel));

	switch(sc->sc_version) {
	case 2:
		fflvl = (reg >> SEC_CHAN_CSR2_FFLVL_S) & SEC_CHAN_CSR2_FFLVL_M;
		break;
	case 3:
		fflvl = (reg >> SEC_CHAN_CSR3_FFLVL_S) & SEC_CHAN_CSR3_FFLVL_M;
		break;
	}

	if (fflvl >= SEC_MAX_FIFO_LEVEL)
		return (-1);

	/* Enqueue descriptor in channel */
	SEC_WRITE(sc, SEC_CHAN_FF(channel), desc->sd_desc_paddr);

	return (channel);
}

static void
sec_enqueue(struct sec_softc *sc)
{
	struct sec_desc *desc;
	int ch0, ch1;

	SEC_LOCK(sc, descriptors);
	SEC_LOCK(sc, controller);

	while (SEC_READY_DESC_CNT(sc) > 0) {
		desc = SEC_GET_READY_DESC(sc);

		ch0 = sec_eu_channel(sc, desc->sd_desc->shd_eu_sel0);
		ch1 = sec_eu_channel(sc, desc->sd_desc->shd_eu_sel1);

		/*
		 * Both EU are used by the same channel.
		 * Enqueue descriptor in channel used by busy EUs.
		 */
		if (ch0 >= 0 && ch0 == ch1) {
			if (sec_enqueue_desc(sc, desc, ch0) >= 0) {
				SEC_DESC_READY2QUEUED(sc);
				continue;
			}
		}

		/*
		 * Only one EU is free.
		 * Enqueue descriptor in channel used by busy EU.
		 */
		if ((ch0 >= 0 && ch1 < 0) || (ch1 >= 0 && ch0 < 0)) {
			if (sec_enqueue_desc(sc, desc, (ch0 >= 0) ? ch0 : ch1)
			    >= 0) {
				SEC_DESC_READY2QUEUED(sc);
				continue;
			}
		}

		/*
		 * Both EU are free.
		 * Enqueue descriptor in first free channel.
		 */
		if (ch0 < 0 && ch1 < 0) {
			if (sec_enqueue_desc(sc, desc, -1) >= 0) {
				SEC_DESC_READY2QUEUED(sc);
				continue;
			}
		}

		/* Current descriptor can not be queued at the moment */
		SEC_PUT_BACK_READY_DESC(sc);
		break;
	}

	SEC_UNLOCK(sc, controller);
	SEC_UNLOCK(sc, descriptors);
}

static struct sec_desc *
sec_find_desc(struct sec_softc *sc, bus_addr_t paddr)
{
	struct sec_desc *desc = NULL;
	int i;

	SEC_LOCK_ASSERT(sc, descriptors);

	for (i = 0; i < SEC_CHANNELS; i++) {
		if (sc->sc_desc[i].sd_desc_paddr == paddr) {
			desc = &(sc->sc_desc[i]);
			break;
		}
	}

	return (desc);
}

static int
sec_make_pointer_direct(struct sec_softc *sc, struct sec_desc *desc, u_int n,
    bus_addr_t data, bus_size_t dsize)
{
	struct sec_hw_desc_ptr *ptr;

	SEC_LOCK_ASSERT(sc, descriptors);

	ptr = &(desc->sd_desc->shd_pointer[n]);
	ptr->shdp_length = dsize;
	ptr->shdp_extent = 0;
	ptr->shdp_j = 0;
	ptr->shdp_ptr = data;

	return (0);
}

static int
sec_make_pointer(struct sec_softc *sc, struct sec_desc *desc,
    u_int n, struct cryptop *crp, bus_size_t doffset, bus_size_t dsize)
{
	struct sec_desc_map_info sdmi = { sc, dsize, doffset, NULL, NULL, 0 };
	struct sec_hw_desc_ptr *ptr;
	int error;

	SEC_LOCK_ASSERT(sc, descriptors);

	error = sec_desc_map_dma(sc, &(desc->sd_ptr_dmem[n]), crp, dsize,
	    &sdmi);

	if (error)
		return (error);

	sdmi.sdmi_lt_last->sl_lt->shl_r = 1;
	desc->sd_lt_used += sdmi.sdmi_lt_used;

	ptr = &(desc->sd_desc->shd_pointer[n]);
	ptr->shdp_length = dsize;
	ptr->shdp_extent = 0;
	ptr->shdp_j = 1;
	ptr->shdp_ptr = sdmi.sdmi_lt_first->sl_lt_paddr;

	return (0);
}

static bool
sec_cipher_supported(const struct crypto_session_params *csp)
{

	switch (csp->csp_cipher_alg) {
	case CRYPTO_AES_CBC:
		/* AESU */
		if (csp->csp_ivlen != AES_BLOCK_LEN)
			return (false);
		break;
	default:
		return (false);
	}

	if (csp->csp_cipher_klen == 0 || csp->csp_cipher_klen > SEC_MAX_KEY_LEN)
		return (false);

	return (true);
}

static bool
sec_auth_supported(struct sec_softc *sc,
    const struct crypto_session_params *csp)
{

	switch (csp->csp_auth_alg) {
	case CRYPTO_SHA2_384_HMAC:
	case CRYPTO_SHA2_512_HMAC:
		if (sc->sc_version < 3)
			return (false);
		/* FALLTHROUGH */
	case CRYPTO_SHA1_HMAC:
	case CRYPTO_SHA2_256_HMAC:
		if (csp->csp_auth_klen > SEC_MAX_KEY_LEN)
			return (false);
		break;
	case CRYPTO_SHA1:
		break;
	default:
		return (false);
	}
	return (true);
}

static int
sec_probesession(device_t dev, const struct crypto_session_params *csp)
{
	struct sec_softc *sc = device_get_softc(dev);

	if (csp->csp_flags != 0)
		return (EINVAL);
	switch (csp->csp_mode) {
	case CSP_MODE_DIGEST:
		if (!sec_auth_supported(sc, csp))
			return (EINVAL);
		break;
	case CSP_MODE_CIPHER:
		if (!sec_cipher_supported(csp))
			return (EINVAL);
		break;
	case CSP_MODE_ETA:
		if (!sec_auth_supported(sc, csp) || !sec_cipher_supported(csp))
			return (EINVAL);
		break;
	default:
		return (EINVAL);
	}
	return (CRYPTODEV_PROBE_HARDWARE);
}

static int
sec_newsession(device_t dev, crypto_session_t cses,
    const struct crypto_session_params *csp)
{
	struct sec_eu_methods *eu = sec_eus;
	struct sec_session *ses;

	ses = crypto_get_driver_session(cses);

	/* Find EU for this session */
	while (eu->sem_make_desc != NULL) {
		if (eu->sem_newsession(csp))
			break;
		eu++;
	}
	KASSERT(eu->sem_make_desc != NULL, ("failed to find eu for session"));

	/* Save cipher key */
	if (csp->csp_cipher_key != NULL)
		memcpy(ses->ss_key, csp->csp_cipher_key, csp->csp_cipher_klen);

	/* Save digest key */
	if (csp->csp_auth_key != NULL)
		memcpy(ses->ss_mkey, csp->csp_auth_key, csp->csp_auth_klen);

	if (csp->csp_auth_alg != 0) {
		if (csp->csp_auth_mlen == 0)
			ses->ss_mlen = crypto_auth_hash(csp)->hashsize;
		else
			ses->ss_mlen = csp->csp_auth_mlen;
	}

	return (0);
}

static int
sec_process(device_t dev, struct cryptop *crp, int hint)
{
	struct sec_softc *sc = device_get_softc(dev);
	struct sec_desc *desc = NULL;
	const struct crypto_session_params *csp;
	struct sec_session *ses;
	int error = 0;

	ses = crypto_get_driver_session(crp->crp_session);
	csp = crypto_get_params(crp->crp_session);

	/* Check for input length */
	if (crypto_buffer_len(&crp->crp_buf) > SEC_MAX_DMA_BLOCK_SIZE) {
		crp->crp_etype = E2BIG;
		crypto_done(crp);
		return (0);
	}

	SEC_LOCK(sc, descriptors);
	SEC_DESC_SYNC(sc, BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	/* Block driver if there is no free descriptors or we are going down */
	if (SEC_FREE_DESC_CNT(sc) == 0 || sc->sc_shutdown) {
		sc->sc_blocked |= CRYPTO_SYMQ;
		SEC_UNLOCK(sc, descriptors);
		return (ERESTART);
	}

	/* Prepare descriptor */
	desc = SEC_GET_FREE_DESC(sc);
	desc->sd_lt_used = 0;
	desc->sd_error = 0;
	desc->sd_crp = crp;

	if (csp->csp_cipher_alg != 0)
		crypto_read_iv(crp, desc->sd_desc->shd_iv);

	if (crp->crp_cipher_key != NULL)
		memcpy(ses->ss_key, crp->crp_cipher_key, csp->csp_cipher_klen);

	if (crp->crp_auth_key != NULL)
		memcpy(ses->ss_mkey, crp->crp_auth_key, csp->csp_auth_klen);

	memcpy(desc->sd_desc->shd_key, ses->ss_key, csp->csp_cipher_klen);
	memcpy(desc->sd_desc->shd_mkey, ses->ss_mkey, csp->csp_auth_klen);

	error = ses->ss_eu->sem_make_desc(sc, csp, desc, crp);

	if (error) {
		SEC_DESC_FREE_POINTERS(desc);
		SEC_DESC_PUT_BACK_LT(sc, desc);
		SEC_PUT_BACK_FREE_DESC(sc);
		SEC_UNLOCK(sc, descriptors);
		crp->crp_etype = error;
		crypto_done(crp);
		return (0);
	}

	/*
	 * Skip DONE interrupt if this is not last request in burst, but only
	 * if we are running on SEC 3.X. On SEC 2.X we have to enable DONE
	 * signaling on each descriptor.
	 */
	if ((hint & CRYPTO_HINT_MORE) && sc->sc_version == 3)
		desc->sd_desc->shd_dn = 0;
	else
		desc->sd_desc->shd_dn = 1;

	SEC_DESC_SYNC(sc, BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
	SEC_DESC_SYNC_POINTERS(desc, BUS_DMASYNC_POSTREAD |
	    BUS_DMASYNC_POSTWRITE);
	SEC_DESC_FREE2READY(sc);
	SEC_UNLOCK(sc, descriptors);

	/* Enqueue ready descriptors in hardware */
	sec_enqueue(sc);

	return (0);
}

static int
sec_build_common_ns_desc(struct sec_softc *sc, struct sec_desc *desc,
    const struct crypto_session_params *csp, struct cryptop *crp)
{
	struct sec_hw_desc *hd = desc->sd_desc;
	int error;

	hd->shd_desc_type = SEC_DT_COMMON_NONSNOOP;
	hd->shd_eu_sel1 = SEC_EU_NONE;
	hd->shd_mode1 = 0;

	/* Pointer 0: NULL */
	error = sec_make_pointer_direct(sc, desc, 0, 0, 0);
	if (error)
		return (error);

	/* Pointer 1: IV IN */
	error = sec_make_pointer_direct(sc, desc, 1, desc->sd_desc_paddr +
	    offsetof(struct sec_hw_desc, shd_iv), csp->csp_ivlen);
	if (error)
		return (error);

	/* Pointer 2: Cipher Key */
	error = sec_make_pointer_direct(sc, desc, 2, desc->sd_desc_paddr +
	    offsetof(struct sec_hw_desc, shd_key), csp->csp_cipher_klen);
 	if (error)
		return (error);

	/* Pointer 3: Data IN */
	error = sec_make_pointer(sc, desc, 3, crp, crp->crp_payload_start,
	    crp->crp_payload_length);
	if (error)
		return (error);

	/* Pointer 4: Data OUT */
	error = sec_make_pointer(sc, desc, 4, crp, crp->crp_payload_start,
	    crp->crp_payload_length);
	if (error)
		return (error);

	/* Pointer 5: IV OUT (Not used: NULL) */
	error = sec_make_pointer_direct(sc, desc, 5, 0, 0);
	if (error)
		return (error);

	/* Pointer 6: NULL */
	error = sec_make_pointer_direct(sc, desc, 6, 0, 0);

	return (error);
}

static int
sec_build_common_s_desc(struct sec_softc *sc, struct sec_desc *desc,
    const struct crypto_session_params *csp, struct cryptop *crp)
{
	struct sec_hw_desc *hd = desc->sd_desc;
	u_int eu, mode, hashlen;
	int error;

	error = sec_mdeu_config(csp, &eu, &mode, &hashlen);
	if (error)
		return (error);

	hd->shd_desc_type = SEC_DT_HMAC_SNOOP;
	hd->shd_eu_sel1 = eu;
	hd->shd_mode1 = mode;

	/* Pointer 0: HMAC Key */
	error = sec_make_pointer_direct(sc, desc, 0, desc->sd_desc_paddr +
	    offsetof(struct sec_hw_desc, shd_mkey), csp->csp_auth_klen);
	if (error)
		return (error);

	/* Pointer 1: HMAC-Only Data IN */
	error = sec_make_pointer(sc, desc, 1, crp, crp->crp_aad_start,
	    crp->crp_aad_length);
	if (error)
		return (error);

	/* Pointer 2: Cipher Key */
	error = sec_make_pointer_direct(sc, desc, 2, desc->sd_desc_paddr +
	    offsetof(struct sec_hw_desc, shd_key), csp->csp_cipher_klen);
 	if (error)
		return (error);

	/* Pointer 3: IV IN */
	error = sec_make_pointer_direct(sc, desc, 3, desc->sd_desc_paddr +
	    offsetof(struct sec_hw_desc, shd_iv), csp->csp_ivlen);
	if (error)
		return (error);

	/* Pointer 4: Data IN */
	error = sec_make_pointer(sc, desc, 4, crp, crp->crp_payload_start,
	    crp->crp_payload_length);
	if (error)
		return (error);

	/* Pointer 5: Data OUT */
	error = sec_make_pointer(sc, desc, 5, crp, crp->crp_payload_start,
	    crp->crp_payload_length);
	if (error)
		return (error);

	/* Pointer 6: HMAC OUT */
	error = sec_make_pointer_direct(sc, desc, 6, desc->sd_desc_paddr +
	    offsetof(struct sec_hw_desc, shd_digest), hashlen);

	return (error);
}

/* AESU */

static bool
sec_aesu_newsession(const struct crypto_session_params *csp)
{

	return (csp->csp_cipher_alg == CRYPTO_AES_CBC);
}

static int
sec_aesu_make_desc(struct sec_softc *sc,
    const struct crypto_session_params *csp, struct sec_desc *desc,
    struct cryptop *crp)
{
	struct sec_hw_desc *hd = desc->sd_desc;
	int error;

	hd->shd_eu_sel0 = SEC_EU_AESU;
	hd->shd_mode0 = SEC_AESU_MODE_CBC;

	if (CRYPTO_OP_IS_ENCRYPT(crp->crp_op)) {
		hd->shd_mode0 |= SEC_AESU_MODE_ED;
		hd->shd_dir = 0;
	} else
		hd->shd_dir = 1;

	if (csp->csp_mode == CSP_MODE_ETA)
		error = sec_build_common_s_desc(sc, desc, csp, crp);
	else
		error = sec_build_common_ns_desc(sc, desc, csp, crp);

	return (error);
}

/* MDEU */

static bool
sec_mdeu_can_handle(u_int alg)
{
	switch (alg) {
	case CRYPTO_SHA1:
	case CRYPTO_SHA1_HMAC:
	case CRYPTO_SHA2_256_HMAC:
	case CRYPTO_SHA2_384_HMAC:
	case CRYPTO_SHA2_512_HMAC:
		return (true);
	default:
		return (false);
	}
}

static int
sec_mdeu_config(const struct crypto_session_params *csp, u_int *eu, u_int *mode,
    u_int *hashlen)
{

	*mode = SEC_MDEU_MODE_PD | SEC_MDEU_MODE_INIT;
	*eu = SEC_EU_NONE;

	switch (csp->csp_auth_alg) {
	case CRYPTO_SHA1_HMAC:
		*mode |= SEC_MDEU_MODE_HMAC;
		/* FALLTHROUGH */
	case CRYPTO_SHA1:
		*eu = SEC_EU_MDEU_A;
		*mode |= SEC_MDEU_MODE_SHA1;
		*hashlen = SHA1_HASH_LEN;
		break;
	case CRYPTO_SHA2_256_HMAC:
		*mode |= SEC_MDEU_MODE_HMAC | SEC_MDEU_MODE_SHA256;
		*eu = SEC_EU_MDEU_A;
		break;
	case CRYPTO_SHA2_384_HMAC:
		*mode |= SEC_MDEU_MODE_HMAC | SEC_MDEU_MODE_SHA384;
		*eu = SEC_EU_MDEU_B;
		break;
	case CRYPTO_SHA2_512_HMAC:
		*mode |= SEC_MDEU_MODE_HMAC | SEC_MDEU_MODE_SHA512;
		*eu = SEC_EU_MDEU_B;
		break;
	default:
		return (EINVAL);
	}

	if (*mode & SEC_MDEU_MODE_HMAC)
		*hashlen = SEC_HMAC_HASH_LEN;

	return (0);
}

static bool
sec_mdeu_newsession(const struct crypto_session_params *csp)
{

	return (sec_mdeu_can_handle(csp->csp_auth_alg));
}

static int
sec_mdeu_make_desc(struct sec_softc *sc,
    const struct crypto_session_params *csp,
    struct sec_desc *desc, struct cryptop *crp)
{
	struct sec_hw_desc *hd = desc->sd_desc;
	u_int eu, mode, hashlen;
	int error;

	error = sec_mdeu_config(csp, &eu, &mode, &hashlen);
	if (error)
		return (error);

	hd->shd_desc_type = SEC_DT_COMMON_NONSNOOP;
	hd->shd_eu_sel0 = eu;
	hd->shd_mode0 = mode;
	hd->shd_eu_sel1 = SEC_EU_NONE;
	hd->shd_mode1 = 0;

	/* Pointer 0: NULL */
	error = sec_make_pointer_direct(sc, desc, 0, 0, 0);
	if (error)
		return (error);

	/* Pointer 1: Context In (Not used: NULL) */
	error = sec_make_pointer_direct(sc, desc, 1, 0, 0);
	if (error)
		return (error);

	/* Pointer 2: HMAC Key (or NULL, depending on digest type) */
	if (hd->shd_mode0 & SEC_MDEU_MODE_HMAC)
		error = sec_make_pointer_direct(sc, desc, 2,
		    desc->sd_desc_paddr + offsetof(struct sec_hw_desc,
		    shd_mkey), csp->csp_auth_klen);
	else
		error = sec_make_pointer_direct(sc, desc, 2, 0, 0);

	if (error)
		return (error);

	/* Pointer 3: Input Data */
	error = sec_make_pointer(sc, desc, 3, crp, crp->crp_payload_start,
	    crp->crp_payload_length);
	if (error)
		return (error);

	/* Pointer 4: NULL */
	error = sec_make_pointer_direct(sc, desc, 4, 0, 0);
	if (error)
		return (error);

	/* Pointer 5: Hash out */
	error = sec_make_pointer_direct(sc, desc, 5, desc->sd_desc_paddr +
	    offsetof(struct sec_hw_desc, shd_digest), hashlen);
	if (error)
		return (error);

	/* Pointer 6: NULL */
	error = sec_make_pointer_direct(sc, desc, 6, 0, 0);

	return (0);
}
