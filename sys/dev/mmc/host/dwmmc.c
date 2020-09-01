/*-
 * Copyright (c) 2014-2019 Ruslan Bukin <br@bsdpad.com>
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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

/*
 * Synopsys DesignWare Mobile Storage Host Controller
 * Chapter 14, Altera Cyclone V Device Handbook (CV-5V2 2014.07.22)
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <sys/queue.h>
#include <sys/taskqueue.h>

#include <dev/mmc/bridge.h>
#include <dev/mmc/mmcbrvar.h>
#include <dev/mmc/mmc_fdt_helpers.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/intr.h>

#ifdef EXT_RESOURCES
#include <dev/extres/clk/clk.h>
#endif

#include <dev/mmc/host/dwmmc_reg.h>
#include <dev/mmc/host/dwmmc_var.h>

#include "opt_mmccam.h"

#ifdef MMCCAM
#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_debug.h>
#include <cam/cam_sim.h>
#include <cam/cam_xpt_sim.h>
#endif

#include "mmcbr_if.h"

#ifdef DEBUG
#define dprintf(fmt, args...) printf(fmt, ##args)
#else
#define dprintf(x, arg...)
#endif

#define	READ4(_sc, _reg) \
	bus_read_4((_sc)->res[0], _reg)
#define	WRITE4(_sc, _reg, _val) \
	bus_write_4((_sc)->res[0], _reg, _val)

#define	DIV_ROUND_UP(n, d)		howmany(n, d)

#define	DWMMC_LOCK(_sc)			mtx_lock(&(_sc)->sc_mtx)
#define	DWMMC_UNLOCK(_sc)		mtx_unlock(&(_sc)->sc_mtx)
#define	DWMMC_LOCK_INIT(_sc) \
	mtx_init(&_sc->sc_mtx, device_get_nameunit(_sc->dev), \
	    "dwmmc", MTX_DEF)
#define	DWMMC_LOCK_DESTROY(_sc)		mtx_destroy(&_sc->sc_mtx);
#define	DWMMC_ASSERT_LOCKED(_sc)	mtx_assert(&_sc->sc_mtx, MA_OWNED);
#define	DWMMC_ASSERT_UNLOCKED(_sc)	mtx_assert(&_sc->sc_mtx, MA_NOTOWNED);

#define	PENDING_CMD	0x01
#define	PENDING_STOP	0x02
#define	CARD_INIT_DONE	0x04

#define	DWMMC_DATA_ERR_FLAGS	(SDMMC_INTMASK_DRT | SDMMC_INTMASK_DCRC \
				|SDMMC_INTMASK_HTO | SDMMC_INTMASK_SBE \
				|SDMMC_INTMASK_EBE)
#define	DWMMC_CMD_ERR_FLAGS	(SDMMC_INTMASK_RTO | SDMMC_INTMASK_RCRC \
				|SDMMC_INTMASK_RE)
#define	DWMMC_ERR_FLAGS		(DWMMC_DATA_ERR_FLAGS | DWMMC_CMD_ERR_FLAGS \
				|SDMMC_INTMASK_HLE)

#define	DES0_DIC	(1 << 1)	/* Disable Interrupt on Completion */
#define	DES0_LD		(1 << 2)	/* Last Descriptor */
#define	DES0_FS		(1 << 3)	/* First Descriptor */
#define	DES0_CH		(1 << 4)	/* second address CHained */
#define	DES0_ER		(1 << 5)	/* End of Ring */
#define	DES0_CES	(1 << 30)	/* Card Error Summary */
#define	DES0_OWN	(1 << 31)	/* OWN */

#define	DES1_BS1_MASK	0x1fff

struct idmac_desc {
	uint32_t	des0;	/* control */
	uint32_t	des1;	/* bufsize */
	uint32_t	des2;	/* buf1 phys addr */
	uint32_t	des3;	/* buf2 phys addr or next descr */
};

#define	IDMAC_DESC_SEGS	(PAGE_SIZE / (sizeof(struct idmac_desc)))
#define	IDMAC_DESC_SIZE	(sizeof(struct idmac_desc) * IDMAC_DESC_SEGS)
#define	DEF_MSIZE	0x2	/* Burst size of multiple transaction */
#define	IDMAC_MAX_SIZE	4096

static void dwmmc_next_operation(struct dwmmc_softc *);
static int dwmmc_setup_bus(struct dwmmc_softc *, int);
static int dma_done(struct dwmmc_softc *, struct mmc_command *);
static int dma_stop(struct dwmmc_softc *);
static void pio_read(struct dwmmc_softc *, struct mmc_command *);
static void pio_write(struct dwmmc_softc *, struct mmc_command *);
static void dwmmc_handle_card_present(struct dwmmc_softc *sc, bool is_present);
static int dwmmc_switch_vccq(device_t, device_t);
#ifdef MMCCAM
static void dwmmc_cam_action(struct cam_sim *, union ccb *);
static void dwmmc_cam_poll(struct cam_sim *);
static int dwmmc_cam_settran_settings(struct dwmmc_softc *, union ccb *);
static int dwmmc_cam_request(struct dwmmc_softc *, union ccb *);
static void dwmmc_cam_handle_mmcio(struct cam_sim *, union ccb *);
#endif

static struct resource_spec dwmmc_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ SYS_RES_IRQ,		0,	RF_ACTIVE },
	{ -1, 0 }
};

#define	HWTYPE_MASK		(0x0000ffff)
#define	HWFLAG_MASK		(0xffff << 16)

static void
dwmmc_get1paddr(void *arg, bus_dma_segment_t *segs, int nsegs, int error)
{

	if (error != 0)
		return;
	*(bus_addr_t *)arg = segs[0].ds_addr;
}

static void
dwmmc_ring_setup(void *arg, bus_dma_segment_t *segs, int nsegs, int error)
{
	struct dwmmc_softc *sc;
	int idx;

	if (error != 0)
		return;

	sc = arg;

	dprintf("nsegs %d seg0len %lu\n", nsegs, segs[0].ds_len);

	for (idx = 0; idx < nsegs; idx++) {
		sc->desc_ring[idx].des0 = (DES0_OWN | DES0_DIC | DES0_CH);
		sc->desc_ring[idx].des1 = segs[idx].ds_len & DES1_BS1_MASK;
		sc->desc_ring[idx].des2 = segs[idx].ds_addr;

		if (idx == 0)
			sc->desc_ring[idx].des0 |= DES0_FS;

		if (idx == (nsegs - 1)) {
			sc->desc_ring[idx].des0 &= ~(DES0_DIC | DES0_CH);
			sc->desc_ring[idx].des0 |= DES0_LD;
		}
	}
}

static int
dwmmc_ctrl_reset(struct dwmmc_softc *sc, int reset_bits)
{
	int reg;
	int i;

	reg = READ4(sc, SDMMC_CTRL);
	reg |= (reset_bits);
	WRITE4(sc, SDMMC_CTRL, reg);

	/* Wait reset done */
	for (i = 0; i < 100; i++) {
		if (!(READ4(sc, SDMMC_CTRL) & reset_bits))
			return (0);
		DELAY(10);
	}

	device_printf(sc->dev, "Reset failed\n");

	return (1);
}

static int
dma_setup(struct dwmmc_softc *sc)
{
	int error;
	int nidx;
	int idx;

	/*
	 * Set up TX descriptor ring, descriptors, and dma maps.
	 */
	error = bus_dma_tag_create(
	    bus_get_dma_tag(sc->dev),	/* Parent tag. */
	    4096, 0,			/* alignment, boundary */
	    BUS_SPACE_MAXADDR_32BIT,	/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filter, filterarg */
	    IDMAC_DESC_SIZE, 1,		/* maxsize, nsegments */
	    IDMAC_DESC_SIZE,		/* maxsegsize */
	    0,				/* flags */
	    NULL, NULL,			/* lockfunc, lockarg */
	    &sc->desc_tag);
	if (error != 0) {
		device_printf(sc->dev,
		    "could not create ring DMA tag.\n");
		return (1);
	}

	error = bus_dmamem_alloc(sc->desc_tag, (void**)&sc->desc_ring,
	    BUS_DMA_COHERENT | BUS_DMA_WAITOK | BUS_DMA_ZERO,
	    &sc->desc_map);
	if (error != 0) {
		device_printf(sc->dev,
		    "could not allocate descriptor ring.\n");
		return (1);
	}

	error = bus_dmamap_load(sc->desc_tag, sc->desc_map,
	    sc->desc_ring, IDMAC_DESC_SIZE, dwmmc_get1paddr,
	    &sc->desc_ring_paddr, 0);
	if (error != 0) {
		device_printf(sc->dev,
		    "could not load descriptor ring map.\n");
		return (1);
	}

	for (idx = 0; idx < IDMAC_DESC_SEGS; idx++) {
		sc->desc_ring[idx].des0 = DES0_CH;
		sc->desc_ring[idx].des1 = 0;
		nidx = (idx + 1) % IDMAC_DESC_SEGS;
		sc->desc_ring[idx].des3 = sc->desc_ring_paddr + \
		    (nidx * sizeof(struct idmac_desc));
	}
	sc->desc_ring[idx - 1].des3 = sc->desc_ring_paddr;
	sc->desc_ring[idx - 1].des0 |= DES0_ER;

	error = bus_dma_tag_create(
	    bus_get_dma_tag(sc->dev),	/* Parent tag. */
	    CACHE_LINE_SIZE, 0,		/* alignment, boundary */
	    BUS_SPACE_MAXADDR_32BIT,	/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filter, filterarg */
	    IDMAC_MAX_SIZE * IDMAC_DESC_SEGS,	/* maxsize */
	    IDMAC_DESC_SEGS,		/* nsegments */
	    IDMAC_MAX_SIZE,		/* maxsegsize */
	    0,				/* flags */
	    NULL, NULL,			/* lockfunc, lockarg */
	    &sc->buf_tag);
	if (error != 0) {
		device_printf(sc->dev,
		    "could not create ring DMA tag.\n");
		return (1);
	}

	error = bus_dmamap_create(sc->buf_tag, 0,
	    &sc->buf_map);
	if (error != 0) {
		device_printf(sc->dev,
		    "could not create TX buffer DMA map.\n");
		return (1);
	}

	return (0);
}

static void
dwmmc_cmd_done(struct dwmmc_softc *sc)
{
	struct mmc_command *cmd;
#ifdef MMCCAM
	union ccb *ccb;
#endif

#ifdef MMCCAM
	ccb = sc->ccb;
	if (ccb == NULL)
		return;
	cmd = &ccb->mmcio.cmd;
#else
	cmd = sc->curcmd;
#endif
	if (cmd == NULL)
		return;

	if (cmd->flags & MMC_RSP_PRESENT) {
		if (cmd->flags & MMC_RSP_136) {
			cmd->resp[3] = READ4(sc, SDMMC_RESP0);
			cmd->resp[2] = READ4(sc, SDMMC_RESP1);
			cmd->resp[1] = READ4(sc, SDMMC_RESP2);
			cmd->resp[0] = READ4(sc, SDMMC_RESP3);
		} else {
			cmd->resp[3] = 0;
			cmd->resp[2] = 0;
			cmd->resp[1] = 0;
			cmd->resp[0] = READ4(sc, SDMMC_RESP0);
		}
	}
}

static void
dwmmc_tasklet(struct dwmmc_softc *sc)
{
	struct mmc_command *cmd;

	cmd = sc->curcmd;
	if (cmd == NULL)
		return;

	if (!sc->cmd_done)
		return;

	if (cmd->error != MMC_ERR_NONE || !cmd->data) {
		dwmmc_next_operation(sc);
	} else if (cmd->data && sc->dto_rcvd) {
		if ((cmd->opcode == MMC_WRITE_MULTIPLE_BLOCK ||
		     cmd->opcode == MMC_READ_MULTIPLE_BLOCK) &&
		     sc->use_auto_stop) {
			if (sc->acd_rcvd)
				dwmmc_next_operation(sc);
		} else {
			dwmmc_next_operation(sc);
		}
	}
}

static void
dwmmc_intr(void *arg)
{
	struct mmc_command *cmd;
	struct dwmmc_softc *sc;
	uint32_t reg;

	sc = arg;

	DWMMC_LOCK(sc);

	cmd = sc->curcmd;

	/* First handle SDMMC controller interrupts */
	reg = READ4(sc, SDMMC_MINTSTS);
	if (reg) {
		dprintf("%s 0x%08x\n", __func__, reg);

		if (reg & DWMMC_CMD_ERR_FLAGS) {
			dprintf("cmd err 0x%08x cmd 0x%08x\n",
				reg, cmd->opcode);
			cmd->error = MMC_ERR_TIMEOUT;
		}

		if (reg & DWMMC_DATA_ERR_FLAGS) {
			dprintf("data err 0x%08x cmd 0x%08x\n",
				reg, cmd->opcode);
			cmd->error = MMC_ERR_FAILED;
			if (!sc->use_pio) {
				dma_done(sc, cmd);
				dma_stop(sc);
			}
		}

		if (reg & SDMMC_INTMASK_CMD_DONE) {
			dwmmc_cmd_done(sc);
			sc->cmd_done = 1;
		}

		if (reg & SDMMC_INTMASK_ACD)
			sc->acd_rcvd = 1;

		if (reg & SDMMC_INTMASK_DTO)
			sc->dto_rcvd = 1;

		if (reg & SDMMC_INTMASK_CD) {
			dwmmc_handle_card_present(sc,
			    READ4(sc, SDMMC_CDETECT) == 0 ? true : false);
		}
	}

	/* Ack interrupts */
	WRITE4(sc, SDMMC_RINTSTS, reg);

	if (sc->use_pio) {
		if (reg & (SDMMC_INTMASK_RXDR|SDMMC_INTMASK_DTO)) {
			pio_read(sc, cmd);
		}
		if (reg & (SDMMC_INTMASK_TXDR|SDMMC_INTMASK_DTO)) {
			pio_write(sc, cmd);
		}
	} else {
		/* Now handle DMA interrupts */
		reg = READ4(sc, SDMMC_IDSTS);
		if (reg) {
			dprintf("dma intr 0x%08x\n", reg);
			if (reg & (SDMMC_IDINTEN_TI | SDMMC_IDINTEN_RI)) {
				WRITE4(sc, SDMMC_IDSTS, (SDMMC_IDINTEN_TI |
							 SDMMC_IDINTEN_RI));
				WRITE4(sc, SDMMC_IDSTS, SDMMC_IDINTEN_NI);
				dma_done(sc, cmd);
			}
		}
	}

	dwmmc_tasklet(sc);

	DWMMC_UNLOCK(sc);
}

static void
dwmmc_handle_card_present(struct dwmmc_softc *sc, bool is_present)
{
	bool was_present;

	was_present = sc->child != NULL;

	if (!was_present && is_present) {
		taskqueue_enqueue_timeout(taskqueue_swi_giant,
		  &sc->card_delayed_task, -(hz / 2));
	} else if (was_present && !is_present) {
		taskqueue_enqueue(taskqueue_swi_giant, &sc->card_task);
	}
}

static void
dwmmc_card_task(void *arg, int pending __unused)
{
	struct dwmmc_softc *sc = arg;

#ifdef MMCCAM
	mmccam_start_discovery(sc->sim);
#else
	DWMMC_LOCK(sc);

	if (READ4(sc, SDMMC_CDETECT) == 0) {
		if (sc->child == NULL) {
			if (bootverbose)
				device_printf(sc->dev, "Card inserted\n");

			sc->child = device_add_child(sc->dev, "mmc", -1);
			DWMMC_UNLOCK(sc);
			if (sc->child) {
				device_set_ivars(sc->child, sc);
				(void)device_probe_and_attach(sc->child);
			}
		} else
			DWMMC_UNLOCK(sc);
	} else {
		/* Card isn't present, detach if necessary */
		if (sc->child != NULL) {
			if (bootverbose)
				device_printf(sc->dev, "Card removed\n");

			DWMMC_UNLOCK(sc);
			device_delete_child(sc->dev, sc->child);
			sc->child = NULL;
		} else
			DWMMC_UNLOCK(sc);
	}
#endif /* MMCCAM */
}

static int
parse_fdt(struct dwmmc_softc *sc)
{
	pcell_t dts_value[3];
	phandle_t node;
	uint32_t bus_hz = 0;
	int len;
#ifdef EXT_RESOURCES
	int error;
#endif

	if ((node = ofw_bus_get_node(sc->dev)) == -1)
		return (ENXIO);

	/* Set some defaults for freq and supported mode */
	sc->host.f_min = 400000;
	sc->host.f_max = 200000000;
	sc->host.host_ocr = MMC_OCR_320_330 | MMC_OCR_330_340;
	sc->host.caps = MMC_CAP_HSPEED | MMC_CAP_SIGNALING_330;
	mmc_fdt_parse(sc->dev, node, &sc->mmc_helper, &sc->host);

	/* fifo-depth */
	if ((len = OF_getproplen(node, "fifo-depth")) > 0) {
		OF_getencprop(node, "fifo-depth", dts_value, len);
		sc->fifo_depth = dts_value[0];
	}

	/* num-slots (Deprecated) */
	sc->num_slots = 1;
	if ((len = OF_getproplen(node, "num-slots")) > 0) {
		device_printf(sc->dev, "num-slots property is deprecated\n");
		OF_getencprop(node, "num-slots", dts_value, len);
		sc->num_slots = dts_value[0];
	}

	/* clock-frequency */
	if ((len = OF_getproplen(node, "clock-frequency")) > 0) {
		OF_getencprop(node, "clock-frequency", dts_value, len);
		bus_hz = dts_value[0];
	}

#ifdef EXT_RESOURCES

	/* IP block reset is optional */
	error = hwreset_get_by_ofw_name(sc->dev, 0, "reset", &sc->hwreset);
	if (error != 0 &&
	    error != ENOENT &&
	    error != ENODEV) {
		device_printf(sc->dev, "Cannot get reset\n");
		goto fail;
	}

	/* vmmc regulator is optional */
	error = regulator_get_by_ofw_property(sc->dev, 0, "vmmc-supply",
	     &sc->vmmc);
	if (error != 0 &&
	    error != ENOENT &&
	    error != ENODEV) {
		device_printf(sc->dev, "Cannot get regulator 'vmmc-supply'\n");
		goto fail;
	}

	/* vqmmc regulator is optional */
	error = regulator_get_by_ofw_property(sc->dev, 0, "vqmmc-supply",
	     &sc->vqmmc);
	if (error != 0 &&
	    error != ENOENT &&
	    error != ENODEV) {
		device_printf(sc->dev, "Cannot get regulator 'vqmmc-supply'\n");
		goto fail;
	}

	/* Assert reset first */
	if (sc->hwreset != NULL) {
		error = hwreset_assert(sc->hwreset);
		if (error != 0) {
			device_printf(sc->dev, "Cannot assert reset\n");
			goto fail;
		}
	}

	/* BIU (Bus Interface Unit clock) is optional */
	error = clk_get_by_ofw_name(sc->dev, 0, "biu", &sc->biu);
	if (error != 0 &&
	    error != ENOENT &&
	    error != ENODEV) {
		device_printf(sc->dev, "Cannot get 'biu' clock\n");
		goto fail;
	}

	if (sc->biu) {
		error = clk_enable(sc->biu);
		if (error != 0) {
			device_printf(sc->dev, "cannot enable biu clock\n");
			goto fail;
		}
	}

	/*
	 * CIU (Controller Interface Unit clock) is mandatory
	 * if no clock-frequency property is given
	 */
	error = clk_get_by_ofw_name(sc->dev, 0, "ciu", &sc->ciu);
	if (error != 0 &&
	    error != ENOENT &&
	    error != ENODEV) {
		device_printf(sc->dev, "Cannot get 'ciu' clock\n");
		goto fail;
	}

	if (sc->ciu) {
		if (bus_hz != 0) {
			error = clk_set_freq(sc->ciu, bus_hz, 0);
			if (error != 0)
				device_printf(sc->dev,
				    "cannot set ciu clock to %u\n", bus_hz);
		}
		error = clk_enable(sc->ciu);
		if (error != 0) {
			device_printf(sc->dev, "cannot enable ciu clock\n");
			goto fail;
		}
		clk_get_freq(sc->ciu, &sc->bus_hz);
	}

	/* Enable regulators */
	if (sc->vmmc != NULL) {
		error = regulator_enable(sc->vmmc);
		if (error != 0) {
			device_printf(sc->dev, "Cannot enable vmmc regulator\n");
			goto fail;
		}
	}
	if (sc->vqmmc != NULL) {
		error = regulator_enable(sc->vqmmc);
		if (error != 0) {
			device_printf(sc->dev, "Cannot enable vqmmc regulator\n");
			goto fail;
		}
	}

	/* Take dwmmc out of reset */
	if (sc->hwreset != NULL) {
		error = hwreset_deassert(sc->hwreset);
		if (error != 0) {
			device_printf(sc->dev, "Cannot deassert reset\n");
			goto fail;
		}
	}
#endif /* EXT_RESOURCES */

	if (sc->bus_hz == 0) {
		device_printf(sc->dev, "No bus speed provided\n");
		goto fail;
	}

	return (0);

fail:
	return (ENXIO);
}

int
dwmmc_attach(device_t dev)
{
	struct dwmmc_softc *sc;
	int error;
	int slot;

	sc = device_get_softc(dev);

	sc->dev = dev;

	/* Why not to use Auto Stop? It save a hundred of irq per second */
	sc->use_auto_stop = 1;

	error = parse_fdt(sc);
	if (error != 0) {
		device_printf(dev, "Can't get FDT property.\n");
		return (ENXIO);
	}

	DWMMC_LOCK_INIT(sc);

	if (bus_alloc_resources(dev, dwmmc_spec, sc->res)) {
		device_printf(dev, "could not allocate resources\n");
		return (ENXIO);
	}

	/* Setup interrupt handler. */
	error = bus_setup_intr(dev, sc->res[1], INTR_TYPE_NET | INTR_MPSAFE,
	    NULL, dwmmc_intr, sc, &sc->intr_cookie);
	if (error != 0) {
		device_printf(dev, "could not setup interrupt handler.\n");
		return (ENXIO);
	}

	device_printf(dev, "Hardware version ID is %04x\n",
		READ4(sc, SDMMC_VERID) & 0xffff);

	/* XXX: we support operation for slot index 0 only */
	slot = 0;
	if (sc->pwren_inverted) {
		WRITE4(sc, SDMMC_PWREN, (0 << slot));
	} else {
		WRITE4(sc, SDMMC_PWREN, (1 << slot));
	}

	/* Reset all */
	if (dwmmc_ctrl_reset(sc, (SDMMC_CTRL_RESET |
				  SDMMC_CTRL_FIFO_RESET |
				  SDMMC_CTRL_DMA_RESET)))
		return (ENXIO);

	dwmmc_setup_bus(sc, sc->host.f_min);

	if (sc->fifo_depth == 0) {
		sc->fifo_depth = 1 +
		    ((READ4(sc, SDMMC_FIFOTH) >> SDMMC_FIFOTH_RXWMARK_S) & 0xfff);
		device_printf(dev, "No fifo-depth, using FIFOTH %x\n",
		    sc->fifo_depth);
	}

	if (!sc->use_pio) {
		dma_stop(sc);
		if (dma_setup(sc))
			return (ENXIO);

		/* Install desc base */
		WRITE4(sc, SDMMC_DBADDR, sc->desc_ring_paddr);

		/* Enable DMA interrupts */
		WRITE4(sc, SDMMC_IDSTS, SDMMC_IDINTEN_MASK);
		WRITE4(sc, SDMMC_IDINTEN, (SDMMC_IDINTEN_NI |
					   SDMMC_IDINTEN_RI |
					   SDMMC_IDINTEN_TI));
	}

	/* Clear and disable interrups for a while */
	WRITE4(sc, SDMMC_RINTSTS, 0xffffffff);
	WRITE4(sc, SDMMC_INTMASK, 0);

	/* Maximum timeout */
	WRITE4(sc, SDMMC_TMOUT, 0xffffffff);

	/* Enable interrupts */
	WRITE4(sc, SDMMC_RINTSTS, 0xffffffff);
	WRITE4(sc, SDMMC_INTMASK, (SDMMC_INTMASK_CMD_DONE |
				   SDMMC_INTMASK_DTO |
				   SDMMC_INTMASK_ACD |
				   SDMMC_INTMASK_TXDR |
				   SDMMC_INTMASK_RXDR |
				   DWMMC_ERR_FLAGS |
				   SDMMC_INTMASK_CD));
	WRITE4(sc, SDMMC_CTRL, SDMMC_CTRL_INT_ENABLE);

	TASK_INIT(&sc->card_task, 0, dwmmc_card_task, sc);
	TIMEOUT_TASK_INIT(taskqueue_swi_giant, &sc->card_delayed_task, 0,
		dwmmc_card_task, sc);

#ifdef MMCCAM
	sc->ccb = NULL;
	if ((sc->devq = cam_simq_alloc(1)) == NULL) {
		goto fail;
	}

	mtx_init(&sc->sim_mtx, "dwmmcsim", NULL, MTX_DEF);
	sc->sim = cam_sim_alloc_dev(dwmmc_cam_action, dwmmc_cam_poll,
	    "dw_mmc_sim", sc, dev,
	    &sc->sim_mtx, 1, 1, sc->devq);

	if (sc->sim == NULL) {
                cam_simq_free(sc->devq);
                device_printf(dev, "cannot allocate CAM SIM\n");
                goto fail;
        }

	mtx_lock(&sc->sim_mtx);
        if (xpt_bus_register(sc->sim, sc->dev, 0) != 0) {
                device_printf(sc->dev, "cannot register SCSI pass-through bus\n");
                cam_sim_free(sc->sim, FALSE);
                cam_simq_free(sc->devq);
                mtx_unlock(&sc->sim_mtx);
                goto fail;
        }

fail:
        mtx_unlock(&sc->sim_mtx);
#endif
	/* 
	 * Schedule a card detection as we won't get an interrupt
	 * if the card is inserted when we attach
	 */
	dwmmc_card_task(sc, 0);
	return (0);
}

int
dwmmc_detach(device_t dev)
{
	struct dwmmc_softc *sc;
	int ret;

	sc = device_get_softc(dev);

	ret = device_delete_children(dev);
	if (ret != 0)
		return (ret);

	taskqueue_drain(taskqueue_swi_giant, &sc->card_task);
	taskqueue_drain_timeout(taskqueue_swi_giant, &sc->card_delayed_task);

	if (sc->intr_cookie != NULL) {
		ret = bus_teardown_intr(dev, sc->res[1], sc->intr_cookie);
		if (ret != 0)
			return (ret);
	}
	bus_release_resources(dev, dwmmc_spec, sc->res);

	DWMMC_LOCK_DESTROY(sc);

#ifdef EXT_RESOURCES
	if (sc->hwreset != NULL && hwreset_deassert(sc->hwreset) != 0)
		device_printf(sc->dev, "cannot deassert reset\n");
	if (sc->biu != NULL && clk_disable(sc->biu) != 0)
		device_printf(sc->dev, "cannot disable biu clock\n");
	if (sc->ciu != NULL && clk_disable(sc->ciu) != 0)
			device_printf(sc->dev, "cannot disable ciu clock\n");

	if (sc->vmmc && regulator_disable(sc->vmmc) != 0)
		device_printf(sc->dev, "Cannot disable vmmc regulator\n");
	if (sc->vqmmc && regulator_disable(sc->vqmmc) != 0)
		device_printf(sc->dev, "Cannot disable vqmmc regulator\n");
#endif

	return (0);
}

static int
dwmmc_setup_bus(struct dwmmc_softc *sc, int freq)
{
	int tout;
	int div;

	if (freq == 0) {
		WRITE4(sc, SDMMC_CLKENA, 0);
		WRITE4(sc, SDMMC_CMD, (SDMMC_CMD_WAIT_PRVDATA |
			SDMMC_CMD_UPD_CLK_ONLY | SDMMC_CMD_START));

		tout = 1000;
		do {
			if (tout-- < 0) {
				device_printf(sc->dev, "Failed update clk\n");
				return (1);
			}
		} while (READ4(sc, SDMMC_CMD) & SDMMC_CMD_START);

		return (0);
	}

	WRITE4(sc, SDMMC_CLKENA, 0);
	WRITE4(sc, SDMMC_CLKSRC, 0);

	div = (sc->bus_hz != freq) ? DIV_ROUND_UP(sc->bus_hz, 2 * freq) : 0;

	WRITE4(sc, SDMMC_CLKDIV, div);
	WRITE4(sc, SDMMC_CMD, (SDMMC_CMD_WAIT_PRVDATA |
			SDMMC_CMD_UPD_CLK_ONLY | SDMMC_CMD_START));

	tout = 1000;
	do {
		if (tout-- < 0) {
			device_printf(sc->dev, "Failed to update clk");
			return (1);
		}
	} while (READ4(sc, SDMMC_CMD) & SDMMC_CMD_START);

	WRITE4(sc, SDMMC_CLKENA, (SDMMC_CLKENA_CCLK_EN | SDMMC_CLKENA_LP));
	WRITE4(sc, SDMMC_CMD, SDMMC_CMD_WAIT_PRVDATA |
			SDMMC_CMD_UPD_CLK_ONLY | SDMMC_CMD_START);

	tout = 1000;
	do {
		if (tout-- < 0) {
			device_printf(sc->dev, "Failed to enable clk\n");
			return (1);
		}
	} while (READ4(sc, SDMMC_CMD) & SDMMC_CMD_START);

	return (0);
}

static int
dwmmc_update_ios(device_t brdev, device_t reqdev)
{
	struct dwmmc_softc *sc;
	struct mmc_ios *ios;
	uint32_t reg;
	int ret = 0;

	sc = device_get_softc(brdev);
	ios = &sc->host.ios;

	dprintf("Setting up clk %u bus_width %d\n",
		ios->clock, ios->bus_width);

	if (ios->bus_width == bus_width_8)
		WRITE4(sc, SDMMC_CTYPE, SDMMC_CTYPE_8BIT);
	else if (ios->bus_width == bus_width_4)
		WRITE4(sc, SDMMC_CTYPE, SDMMC_CTYPE_4BIT);
	else
		WRITE4(sc, SDMMC_CTYPE, 0);

	if ((sc->hwtype & HWTYPE_MASK) == HWTYPE_EXYNOS) {
		/* XXX: take care about DDR or SDR use here */
		WRITE4(sc, SDMMC_CLKSEL, sc->sdr_timing);
	}

	/* Set DDR mode */
	reg = READ4(sc, SDMMC_UHS_REG);
	if (ios->timing == bus_timing_uhs_ddr50 ||
	    ios->timing == bus_timing_mmc_ddr52 ||
	    ios->timing == bus_timing_mmc_hs400)
		reg |= (SDMMC_UHS_REG_DDR);
	else
		reg &= ~(SDMMC_UHS_REG_DDR);
	WRITE4(sc, SDMMC_UHS_REG, reg);

	if (sc->update_ios)
		ret = sc->update_ios(sc, ios);

	dwmmc_setup_bus(sc, ios->clock);

	return (ret);
}

static int
dma_done(struct dwmmc_softc *sc, struct mmc_command *cmd)
{
	struct mmc_data *data;

	data = cmd->data;

	if (data->flags & MMC_DATA_WRITE)
		bus_dmamap_sync(sc->buf_tag, sc->buf_map,
			BUS_DMASYNC_POSTWRITE);
	else
		bus_dmamap_sync(sc->buf_tag, sc->buf_map,
			BUS_DMASYNC_POSTREAD);

	bus_dmamap_sync(sc->desc_tag, sc->desc_map,
	    BUS_DMASYNC_POSTWRITE);

	bus_dmamap_unload(sc->buf_tag, sc->buf_map);

	return (0);
}

static int
dma_stop(struct dwmmc_softc *sc)
{
	int reg;

	reg = READ4(sc, SDMMC_CTRL);
	reg &= ~(SDMMC_CTRL_USE_IDMAC);
	reg |= (SDMMC_CTRL_DMA_RESET);
	WRITE4(sc, SDMMC_CTRL, reg);

	reg = READ4(sc, SDMMC_BMOD);
	reg &= ~(SDMMC_BMOD_DE | SDMMC_BMOD_FB);
	reg |= (SDMMC_BMOD_SWR);
	WRITE4(sc, SDMMC_BMOD, reg);

	return (0);
}

static int
dma_prepare(struct dwmmc_softc *sc, struct mmc_command *cmd)
{
	struct mmc_data *data;
	int err;
	int reg;

	data = cmd->data;

	reg = READ4(sc, SDMMC_INTMASK);
	reg &= ~(SDMMC_INTMASK_TXDR | SDMMC_INTMASK_RXDR);
	WRITE4(sc, SDMMC_INTMASK, reg);

	err = bus_dmamap_load(sc->buf_tag, sc->buf_map,
		data->data, data->len, dwmmc_ring_setup,
		sc, BUS_DMA_NOWAIT);
	if (err != 0)
		panic("dmamap_load failed\n");

	/* Ensure the device can see the desc */
	bus_dmamap_sync(sc->desc_tag, sc->desc_map,
	    BUS_DMASYNC_PREWRITE);

	if (data->flags & MMC_DATA_WRITE)
		bus_dmamap_sync(sc->buf_tag, sc->buf_map,
			BUS_DMASYNC_PREWRITE);
	else
		bus_dmamap_sync(sc->buf_tag, sc->buf_map,
			BUS_DMASYNC_PREREAD);

	reg = (DEF_MSIZE << SDMMC_FIFOTH_MSIZE_S);
	reg |= ((sc->fifo_depth / 2) - 1) << SDMMC_FIFOTH_RXWMARK_S;
	reg |= (sc->fifo_depth / 2) << SDMMC_FIFOTH_TXWMARK_S;

	WRITE4(sc, SDMMC_FIFOTH, reg);
	wmb();

	reg = READ4(sc, SDMMC_CTRL);
	reg |= (SDMMC_CTRL_USE_IDMAC | SDMMC_CTRL_DMA_ENABLE);
	WRITE4(sc, SDMMC_CTRL, reg);
	wmb();

	reg = READ4(sc, SDMMC_BMOD);
	reg |= (SDMMC_BMOD_DE | SDMMC_BMOD_FB);
	WRITE4(sc, SDMMC_BMOD, reg);

	/* Start */
	WRITE4(sc, SDMMC_PLDMND, 1);

	return (0);
}

static int
pio_prepare(struct dwmmc_softc *sc, struct mmc_command *cmd)
{
	struct mmc_data *data;
	int reg;

	data = cmd->data;
	data->xfer_len = 0;

	reg = (DEF_MSIZE << SDMMC_FIFOTH_MSIZE_S);
	reg |= ((sc->fifo_depth / 2) - 1) << SDMMC_FIFOTH_RXWMARK_S;
	reg |= (sc->fifo_depth / 2) << SDMMC_FIFOTH_TXWMARK_S;

	WRITE4(sc, SDMMC_FIFOTH, reg);
	wmb();

	return (0);
}

static void
pio_read(struct dwmmc_softc *sc, struct mmc_command *cmd)
{
	struct mmc_data *data;
	uint32_t *p, status;

	if (cmd == NULL || cmd->data == NULL)
		return;

	data = cmd->data;
	if ((data->flags & MMC_DATA_READ) == 0)
		return;

	KASSERT((data->xfer_len & 3) == 0, ("xfer_len not aligned"));
	p = (uint32_t *)data->data + (data->xfer_len >> 2);

	while (data->xfer_len < data->len) {
		status = READ4(sc, SDMMC_STATUS);
		if (status & SDMMC_STATUS_FIFO_EMPTY)
			break;
		*p++ = READ4(sc, SDMMC_DATA);
		data->xfer_len += 4;
	}

	WRITE4(sc, SDMMC_RINTSTS, SDMMC_INTMASK_RXDR);
}

static void
pio_write(struct dwmmc_softc *sc, struct mmc_command *cmd)
{
	struct mmc_data *data;
	uint32_t *p, status;

	if (cmd == NULL || cmd->data == NULL)
		return;

	data = cmd->data;
	if ((data->flags & MMC_DATA_WRITE) == 0)
		return;

	KASSERT((data->xfer_len & 3) == 0, ("xfer_len not aligned"));
	p = (uint32_t *)data->data + (data->xfer_len >> 2);

	while (data->xfer_len < data->len) {
		status = READ4(sc, SDMMC_STATUS);
		if (status & SDMMC_STATUS_FIFO_FULL)
			break;
		WRITE4(sc, SDMMC_DATA, *p++);
		data->xfer_len += 4;
	}

	WRITE4(sc, SDMMC_RINTSTS, SDMMC_INTMASK_TXDR);
}

static void
dwmmc_start_cmd(struct dwmmc_softc *sc, struct mmc_command *cmd)
{
	struct mmc_data *data;
	uint32_t blksz;
	uint32_t cmdr;

	dprintf("%s\n", __func__);
	sc->curcmd = cmd;
	data = cmd->data;

	if ((sc->hwtype & HWTYPE_MASK) == HWTYPE_ROCKCHIP)
		dwmmc_setup_bus(sc, sc->host.ios.clock);

#ifndef MMCCAM
	/* XXX Upper layers don't always set this */
	cmd->mrq = sc->req;
#endif
	/* Begin setting up command register. */

	cmdr = cmd->opcode;

	dprintf("cmd->opcode 0x%08x\n", cmd->opcode);

	if (cmd->opcode == MMC_STOP_TRANSMISSION ||
	    cmd->opcode == MMC_GO_IDLE_STATE ||
	    cmd->opcode == MMC_GO_INACTIVE_STATE)
		cmdr |= SDMMC_CMD_STOP_ABORT;
	else if (cmd->opcode != MMC_SEND_STATUS && data)
		cmdr |= SDMMC_CMD_WAIT_PRVDATA;

	/* Set up response handling. */
	if (MMC_RSP(cmd->flags) != MMC_RSP_NONE) {
		cmdr |= SDMMC_CMD_RESP_EXP;
		if (cmd->flags & MMC_RSP_136)
			cmdr |= SDMMC_CMD_RESP_LONG;
	}

	if (cmd->flags & MMC_RSP_CRC)
		cmdr |= SDMMC_CMD_RESP_CRC;

	/*
	 * XXX: Not all platforms want this.
	 */
	cmdr |= SDMMC_CMD_USE_HOLD_REG;

	if ((sc->flags & CARD_INIT_DONE) == 0) {
		sc->flags |= (CARD_INIT_DONE);
		cmdr |= SDMMC_CMD_SEND_INIT;
	}

	if (data) {
		if ((cmd->opcode == MMC_WRITE_MULTIPLE_BLOCK ||
		     cmd->opcode == MMC_READ_MULTIPLE_BLOCK) &&
		     sc->use_auto_stop)
			cmdr |= SDMMC_CMD_SEND_ASTOP;

		cmdr |= SDMMC_CMD_DATA_EXP;
		if (data->flags & MMC_DATA_STREAM)
			cmdr |= SDMMC_CMD_MODE_STREAM;
		if (data->flags & MMC_DATA_WRITE)
			cmdr |= SDMMC_CMD_DATA_WRITE;

		WRITE4(sc, SDMMC_TMOUT, 0xffffffff);
		WRITE4(sc, SDMMC_BYTCNT, data->len);
		blksz = (data->len < MMC_SECTOR_SIZE) ? \
			 data->len : MMC_SECTOR_SIZE;
		WRITE4(sc, SDMMC_BLKSIZ, blksz);

		if (sc->use_pio) {
			pio_prepare(sc, cmd);
		} else {
			dma_prepare(sc, cmd);
		}
		wmb();
	}

	dprintf("cmdr 0x%08x\n", cmdr);

	WRITE4(sc, SDMMC_CMDARG, cmd->arg);
	wmb();
	WRITE4(sc, SDMMC_CMD, cmdr | SDMMC_CMD_START);
};

static void
dwmmc_next_operation(struct dwmmc_softc *sc)
{
	struct mmc_command *cmd;
	dprintf("%s\n", __func__);
#ifdef MMCCAM
	union ccb *ccb;

	ccb = sc->ccb;
	if (ccb == NULL)
		return;
	cmd = &ccb->mmcio.cmd;
#else
	struct mmc_request *req;

	req = sc->req;
	if (req == NULL)
		return;
	cmd = req->cmd;
#endif

	sc->acd_rcvd = 0;
	sc->dto_rcvd = 0;
	sc->cmd_done = 0;

	/*
	 * XXX: Wait until card is still busy.
	 * We do need this to prevent data timeouts,
	 * mostly caused by multi-block write command
	 * followed by single-read.
	 */
	while(READ4(sc, SDMMC_STATUS) & (SDMMC_STATUS_DATA_BUSY))
		continue;

	if (sc->flags & PENDING_CMD) {
		sc->flags &= ~PENDING_CMD;
		dwmmc_start_cmd(sc, cmd);
		return;
	} else if (sc->flags & PENDING_STOP && !sc->use_auto_stop) {
		sc->flags &= ~PENDING_STOP;
		/// XXX: What to do with this?
		//dwmmc_start_cmd(sc, req->stop);
		return;
	}

#ifdef MMCCAM
	sc->ccb = NULL;
	sc->curcmd = NULL;
	ccb->ccb_h.status =
		(ccb->mmcio.cmd.error == 0 ? CAM_REQ_CMP : CAM_REQ_CMP_ERR);
	xpt_done(ccb);
#else
	sc->req = NULL;
	sc->curcmd = NULL;
	req->done(req);
#endif
}

static int
dwmmc_request(device_t brdev, device_t reqdev, struct mmc_request *req)
{
	struct dwmmc_softc *sc;

	sc = device_get_softc(brdev);

	dprintf("%s\n", __func__);

	DWMMC_LOCK(sc);

#ifdef MMCCAM
	sc->flags |= PENDING_CMD;
#else
	if (sc->req != NULL) {
		DWMMC_UNLOCK(sc);
		return (EBUSY);
	}

	sc->req = req;
	sc->flags |= PENDING_CMD;
	if (sc->req->stop)
		sc->flags |= PENDING_STOP;
#endif
	dwmmc_next_operation(sc);

	DWMMC_UNLOCK(sc);
	return (0);
}

static int
dwmmc_get_ro(device_t brdev, device_t reqdev)
{

	dprintf("%s\n", __func__);

	return (0);
}

static int
dwmmc_acquire_host(device_t brdev, device_t reqdev)
{
	struct dwmmc_softc *sc;

	sc = device_get_softc(brdev);

	DWMMC_LOCK(sc);
	while (sc->bus_busy)
		msleep(sc, &sc->sc_mtx, PZERO, "dwmmcah", hz / 5);
	sc->bus_busy++;
	DWMMC_UNLOCK(sc);
	return (0);
}

static int
dwmmc_release_host(device_t brdev, device_t reqdev)
{
	struct dwmmc_softc *sc;

	sc = device_get_softc(brdev);

	DWMMC_LOCK(sc);
	sc->bus_busy--;
	wakeup(sc);
	DWMMC_UNLOCK(sc);
	return (0);
}

static int
dwmmc_read_ivar(device_t bus, device_t child, int which, uintptr_t *result)
{
	struct dwmmc_softc *sc;

	sc = device_get_softc(bus);

	switch (which) {
	default:
		return (EINVAL);
	case MMCBR_IVAR_BUS_MODE:
		*(int *)result = sc->host.ios.bus_mode;
		break;
	case MMCBR_IVAR_BUS_WIDTH:
		*(int *)result = sc->host.ios.bus_width;
		break;
	case MMCBR_IVAR_CHIP_SELECT:
		*(int *)result = sc->host.ios.chip_select;
		break;
	case MMCBR_IVAR_CLOCK:
		*(int *)result = sc->host.ios.clock;
		break;
	case MMCBR_IVAR_F_MIN:
		*(int *)result = sc->host.f_min;
		break;
	case MMCBR_IVAR_F_MAX:
		*(int *)result = sc->host.f_max;
		break;
	case MMCBR_IVAR_HOST_OCR:
		*(int *)result = sc->host.host_ocr;
		break;
	case MMCBR_IVAR_MODE:
		*(int *)result = sc->host.mode;
		break;
	case MMCBR_IVAR_OCR:
		*(int *)result = sc->host.ocr;
		break;
	case MMCBR_IVAR_POWER_MODE:
		*(int *)result = sc->host.ios.power_mode;
		break;
	case MMCBR_IVAR_VDD:
		*(int *)result = sc->host.ios.vdd;
		break;
	case MMCBR_IVAR_VCCQ:
		*(int *)result = sc->host.ios.vccq;
		break;
	case MMCBR_IVAR_CAPS:
		*(int *)result = sc->host.caps;
		break;
	case MMCBR_IVAR_MAX_DATA:
		*(int *)result = (IDMAC_MAX_SIZE * IDMAC_DESC_SEGS) / MMC_SECTOR_SIZE;
		break;
	case MMCBR_IVAR_TIMING:
		*(int *)result = sc->host.ios.timing;
		break;
	}
	return (0);
}

static int
dwmmc_write_ivar(device_t bus, device_t child, int which, uintptr_t value)
{
	struct dwmmc_softc *sc;

	sc = device_get_softc(bus);

	switch (which) {
	default:
		return (EINVAL);
	case MMCBR_IVAR_BUS_MODE:
		sc->host.ios.bus_mode = value;
		break;
	case MMCBR_IVAR_BUS_WIDTH:
		sc->host.ios.bus_width = value;
		break;
	case MMCBR_IVAR_CHIP_SELECT:
		sc->host.ios.chip_select = value;
		break;
	case MMCBR_IVAR_CLOCK:
		sc->host.ios.clock = value;
		break;
	case MMCBR_IVAR_MODE:
		sc->host.mode = value;
		break;
	case MMCBR_IVAR_OCR:
		sc->host.ocr = value;
		break;
	case MMCBR_IVAR_POWER_MODE:
		sc->host.ios.power_mode = value;
		break;
	case MMCBR_IVAR_VDD:
		sc->host.ios.vdd = value;
		break;
	case MMCBR_IVAR_TIMING:
		sc->host.ios.timing = value;
		break;
	case MMCBR_IVAR_VCCQ:
		sc->host.ios.vccq = value;
		break;
	/* These are read-only */
	case MMCBR_IVAR_CAPS:
	case MMCBR_IVAR_HOST_OCR:
	case MMCBR_IVAR_F_MIN:
	case MMCBR_IVAR_F_MAX:
	case MMCBR_IVAR_MAX_DATA:
		return (EINVAL);
	}
	return (0);
}

/* Note: this function likely belongs to the specific driver impl */
static int
dwmmc_switch_vccq(device_t dev, device_t child)
{
	device_printf(dev, "This is a default impl of switch_vccq() that always fails\n");
	return EINVAL;
}

#ifdef MMCCAM
static void
dwmmc_cam_handle_mmcio(struct cam_sim *sim, union ccb *ccb)
{
	struct dwmmc_softc *sc;

	sc = cam_sim_softc(sim);

	dwmmc_cam_request(sc, ccb);
}

static void
dwmmc_cam_action(struct cam_sim *sim, union ccb *ccb)
{
	struct dwmmc_softc *sc;

	sc = cam_sim_softc(sim);
	if (sc == NULL) {
		ccb->ccb_h.status = CAM_SEL_TIMEOUT;
		xpt_done(ccb);
		return;
	}

	mtx_assert(&sc->sim_mtx, MA_OWNED);

	switch (ccb->ccb_h.func_code) {
	case XPT_PATH_INQ:
		/* XXX: correctly calculate maxio here */
		mmc_path_inq(&ccb->cpi, "Deglitch Networks", sim, MMC_SECTOR_SIZE);
		break;

	case XPT_GET_TRAN_SETTINGS:
	{
		struct ccb_trans_settings *cts = &ccb->cts;

		cts->protocol = PROTO_MMCSD;
		cts->protocol_version = 1;
		cts->transport = XPORT_MMCSD;
		cts->transport_version = 1;
		cts->xport_specific.valid = 0;
		cts->proto_specific.mmc.host_ocr = sc->host.host_ocr;
		cts->proto_specific.mmc.host_f_min = sc->host.f_min;
		cts->proto_specific.mmc.host_f_max = sc->host.f_max;
		cts->proto_specific.mmc.host_caps = sc->host.caps;
		/* XXX: correctly calculate host_max_data */
		cts->proto_specific.mmc.host_max_data = 1;
		memcpy(&cts->proto_specific.mmc.ios, &sc->host.ios, sizeof(struct mmc_ios));
		ccb->ccb_h.status = CAM_REQ_CMP;
		break;
	}
	case XPT_SET_TRAN_SETTINGS:
	{
		dwmmc_cam_settran_settings(sc, ccb);
		ccb->ccb_h.status = CAM_REQ_CMP;
		break;
	}
	case XPT_RESET_BUS: {
		struct ccb_trans_settings_mmc *cts;

		cts = &ccb->cts.proto_specific.mmc;
		cts->ios_valid = MMC_PM;
		cts->ios.power_mode = power_off;
		/* Power off the MMC bus */
		if (dwmmc_cam_settran_settings(sc, ccb) != 0) {
			device_printf(sc->dev,"cannot power down the MMC bus\n");
			ccb->ccb_h.status = CAM_REQ_CMP_ERR;
			break;
		}

		/* Soft Reset controller and run initialization again */
		if (dwmmc_ctrl_reset(sc, (SDMMC_CTRL_RESET |
				  SDMMC_CTRL_FIFO_RESET |
				  SDMMC_CTRL_DMA_RESET)) != 0) {
			device_printf(sc->dev, "cannot reset the controller\n");
			ccb->ccb_h.status = CAM_REQ_CMP_ERR;
			break;
		}

		cts->ios_valid = MMC_PM;
		cts->ios.power_mode = power_on;
		/* Power off the MMC bus */
		if (dwmmc_cam_settran_settings(sc, ccb) != 0) {
			device_printf(sc->dev, "cannot power on the MMC bus\n");
			ccb->ccb_h.status = CAM_REQ_CMP_ERR;
			break;
		}

		ccb->ccb_h.status = CAM_REQ_CMP;
		break;
	}
	case XPT_MMC_IO:
		/*
		 * Here is the HW-dependent part of
		 * sending the command to the underlying h/w
		 * At some point in the future an interrupt comes.
		 * Then the request will be marked as completed.
		 */
		ccb->ccb_h.status = CAM_REQ_INPROG;

		dwmmc_cam_handle_mmcio(sim, ccb);
		return;
		/* NOTREACHED */
		break;
	default:
		ccb->ccb_h.status = CAM_REQ_INVALID;
		break;
	}
	xpt_done(ccb);
	return;
}

static void
dwmmc_cam_poll(struct cam_sim *sim)
{
	return;
}

static int
dwmmc_cam_settran_settings(struct dwmmc_softc *sc, union ccb *ccb)
{
	struct mmc_ios *ios;
	struct mmc_ios *new_ios;
	struct ccb_trans_settings_mmc *cts;
	int res;

	ios = &sc->host.ios;

	cts = &ccb->cts.proto_specific.mmc;
	new_ios = &cts->ios;

	/* Update only requested fields */
	if (cts->ios_valid & MMC_CLK) {
		ios->clock = new_ios->clock;
		if (bootverbose)
			device_printf(sc->dev, "Clock => %d\n", ios->clock);
	}
	if (cts->ios_valid & MMC_VDD) {
		ios->vdd = new_ios->vdd;
		if (bootverbose)
			device_printf(sc->dev, "VDD => %d\n", ios->vdd);
	}
	if (cts->ios_valid & MMC_CS) {
		ios->chip_select = new_ios->chip_select;
		if (bootverbose)
			device_printf(sc->dev, "CS => %d\n", ios->chip_select);
	}
	if (cts->ios_valid & MMC_BW) {
		ios->bus_width = new_ios->bus_width;
		if (bootverbose)
			device_printf(sc->dev, "Bus width => %d\n", ios->bus_width);
	}
	if (cts->ios_valid & MMC_PM) {
		ios->power_mode = new_ios->power_mode;
		if (bootverbose)
			device_printf(sc->dev, "Power mode => %d\n", ios->power_mode);
	}
	if (cts->ios_valid & MMC_BT) {
		ios->timing = new_ios->timing;
		if (bootverbose)
			device_printf(sc->dev, "Timing => %d\n", ios->timing);
	}
	if (cts->ios_valid & MMC_BM) {
		ios->bus_mode = new_ios->bus_mode;
		if (bootverbose)
			device_printf(sc->dev, "Bus mode => %d\n", ios->bus_mode);
	}
	if (cts->ios_valid & MMC_VCCQ) {
		ios->vccq = new_ios->vccq;
		if (bootverbose)
			device_printf(sc->dev, "VCCQ => %d\n", ios->vccq);
		res = dwmmc_switch_vccq(sc->dev, NULL);
		device_printf(sc->dev, "VCCQ switch result: %d\n", res);
	}

	return (dwmmc_update_ios(sc->dev, NULL));
}

static int
dwmmc_cam_request(struct dwmmc_softc *sc, union ccb *ccb)
{
	struct ccb_mmcio *mmcio;

	mmcio = &ccb->mmcio;

	DWMMC_LOCK(sc);

#ifdef DEBUG
	if (__predict_false(bootverbose)) {
		device_printf(sc->dev, "CMD%u arg %#x flags %#x dlen %u dflags %#x\n",
			    mmcio->cmd.opcode, mmcio->cmd.arg, mmcio->cmd.flags,
			    mmcio->cmd.data != NULL ? (unsigned int) mmcio->cmd.data->len : 0,
			    mmcio->cmd.data != NULL ? mmcio->cmd.data->flags: 0);
	}
#endif
	if (mmcio->cmd.data != NULL) {
		if (mmcio->cmd.data->len == 0 || mmcio->cmd.data->flags == 0)
			panic("data->len = %d, data->flags = %d -- something is b0rked",
			      (int)mmcio->cmd.data->len, mmcio->cmd.data->flags);
	}
	if (sc->ccb != NULL) {
		device_printf(sc->dev, "Controller still has an active command\n");
		return (EBUSY);
	}
	sc->ccb = ccb;
	DWMMC_UNLOCK(sc);
	dwmmc_request(sc->dev, NULL, NULL);

	return (0);
}
#endif

static device_method_t dwmmc_methods[] = {
	/* Bus interface */
	DEVMETHOD(bus_read_ivar,	dwmmc_read_ivar),
	DEVMETHOD(bus_write_ivar,	dwmmc_write_ivar),

	/* mmcbr_if */
	DEVMETHOD(mmcbr_update_ios,	dwmmc_update_ios),
	DEVMETHOD(mmcbr_request,	dwmmc_request),
	DEVMETHOD(mmcbr_get_ro,		dwmmc_get_ro),
	DEVMETHOD(mmcbr_acquire_host,	dwmmc_acquire_host),
	DEVMETHOD(mmcbr_release_host,	dwmmc_release_host),

	DEVMETHOD_END
};

DEFINE_CLASS_0(dwmmc, dwmmc_driver, dwmmc_methods,
    sizeof(struct dwmmc_softc));
