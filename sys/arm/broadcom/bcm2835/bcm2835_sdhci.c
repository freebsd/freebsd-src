/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2012 Oleksandr Tymoshenko <gonzo@freebsd.org>
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
 *
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <sys/sysctl.h>
#include <sys/taskqueue.h>

#include <machine/bus.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/mmc/bridge.h>
#include <dev/mmc/mmcreg.h>
#include <dev/mmc/mmc_fdt_helpers.h>

#include <dev/sdhci/sdhci.h>

#include "mmcbr_if.h"
#include "sdhci_if.h"

#include "opt_mmccam.h"

#include "bcm2835_dma.h"
#include <arm/broadcom/bcm2835/bcm2835_mbox_prop.h>
#ifdef NOTYET
#include <arm/broadcom/bcm2835/bcm2835_clkman.h>
#endif
#include <arm/broadcom/bcm2835/bcm2835_vcbus.h>

#define	BCM2835_DEFAULT_SDHCI_FREQ	50
#define	BCM2838_DEFAULT_SDHCI_FREQ	100

#define	BCM_SDHCI_BUFFER_SIZE		512
/*
 * NUM_DMA_SEGS is the number of DMA segments we want to accommodate on average.
 * We add in a number of segments based on how much we may need to spill into
 * another segment due to crossing page boundaries.  e.g. up to PAGE_SIZE, an
 * extra page is needed as we can cross a page boundary exactly once.
 */
#define	NUM_DMA_SEGS			1
#define	NUM_DMA_SPILL_SEGS		\
	((((NUM_DMA_SEGS * BCM_SDHCI_BUFFER_SIZE) - 1) / PAGE_SIZE) + 1)
#define	ALLOCATED_DMA_SEGS		(NUM_DMA_SEGS +	NUM_DMA_SPILL_SEGS)
#define	BCM_DMA_MAXSIZE			(NUM_DMA_SEGS * BCM_SDHCI_BUFFER_SIZE)

#define	BCM_SDHCI_SLOT_LEFT(slot)	\
	((slot)->curcmd->data->len - (slot)->offset)

#define	BCM_SDHCI_SEGSZ_LEFT(slot)	\
	min(BCM_DMA_MAXSIZE,		\
	    rounddown(BCM_SDHCI_SLOT_LEFT(slot), BCM_SDHCI_BUFFER_SIZE))

#define	DATA_PENDING_MASK	(SDHCI_INT_DATA_AVAIL | SDHCI_INT_SPACE_AVAIL)
#define	DATA_XFER_MASK		(DATA_PENDING_MASK | SDHCI_INT_DATA_END)

#ifdef DEBUG
static int bcm2835_sdhci_debug = 0;

TUNABLE_INT("hw.bcm2835.sdhci.debug", &bcm2835_sdhci_debug);
SYSCTL_INT(_hw_sdhci, OID_AUTO, bcm2835_sdhci_debug, CTLFLAG_RWTUN,
    &bcm2835_sdhci_debug, 0, "bcm2835 SDHCI debug level");

#define	dprintf(fmt, args...)					\
	do {							\
		if (bcm2835_sdhci_debug)			\
			printf("%s: " fmt, __func__, ##args);	\
	}  while (0)
#else
#define dprintf(fmt, args...)
#endif

static int bcm2835_sdhci_hs = 1;
static int bcm2835_sdhci_pio_mode = 0;

struct bcm_mmc_conf {
	int	clock_id;
	int	clock_src;
	int	default_freq;
	int	quirks;
	int	emmc_dreq;
};

struct bcm_mmc_conf bcm2835_sdhci_conf = {
	.clock_id	= BCM2835_MBOX_CLOCK_ID_EMMC,
	.clock_src	= -1,
	.default_freq	= BCM2835_DEFAULT_SDHCI_FREQ,
	.quirks		= SDHCI_QUIRK_DATA_TIMEOUT_USES_SDCLK |
	    SDHCI_QUIRK_BROKEN_TIMEOUT_VAL | SDHCI_QUIRK_DONT_SET_HISPD_BIT |
	    SDHCI_QUIRK_MISSING_CAPS,
	.emmc_dreq	= BCM_DMA_DREQ_EMMC,
};

struct bcm_mmc_conf bcm2838_emmc2_conf = {
	.clock_id	= BCM2838_MBOX_CLOCK_ID_EMMC2,
	.clock_src	= -1,
	.default_freq	= BCM2838_DEFAULT_SDHCI_FREQ,
	.quirks		= 0,
	.emmc_dreq	= BCM_DMA_DREQ_NONE,
};

static struct ofw_compat_data compat_data[] = {
	{"broadcom,bcm2835-sdhci",	(uintptr_t)&bcm2835_sdhci_conf},
	{"brcm,bcm2835-sdhci",		(uintptr_t)&bcm2835_sdhci_conf},
	{"brcm,bcm2835-mmc",		(uintptr_t)&bcm2835_sdhci_conf},
	{"brcm,bcm2711-emmc2",		(uintptr_t)&bcm2838_emmc2_conf},
	{"brcm,bcm2838-emmc2",		(uintptr_t)&bcm2838_emmc2_conf},
	{NULL,				0}
};

TUNABLE_INT("hw.bcm2835.sdhci.hs", &bcm2835_sdhci_hs);
TUNABLE_INT("hw.bcm2835.sdhci.pio_mode", &bcm2835_sdhci_pio_mode);

struct bcm_sdhci_softc {
	device_t		sc_dev;
	struct resource *	sc_mem_res;
	struct resource *	sc_irq_res;
	bus_space_tag_t		sc_bst;
	bus_space_handle_t	sc_bsh;
	void *			sc_intrhand;
	struct mmc_request *	sc_req;
	struct sdhci_slot	sc_slot;
	struct mmc_helper	sc_mmc_helper;
	int			sc_dma_ch;
	bus_dma_tag_t		sc_dma_tag;
	bus_dmamap_t		sc_dma_map;
	vm_paddr_t		sc_sdhci_buffer_phys;
	bus_addr_t		dmamap_seg_addrs[ALLOCATED_DMA_SEGS];
	bus_size_t		dmamap_seg_sizes[ALLOCATED_DMA_SEGS];
	int			dmamap_seg_count;
	int			dmamap_seg_index;
	int			dmamap_status;
	uint32_t		blksz_and_count;
	uint32_t		cmd_and_mode;
	bool			need_update_blk;
#ifdef NOTYET
	device_t		clkman;
#endif
	struct bcm_mmc_conf *	conf;
};

static int bcm_sdhci_probe(device_t);
static int bcm_sdhci_attach(device_t);
static int bcm_sdhci_detach(device_t);
static void bcm_sdhci_intr(void *);

static int bcm_sdhci_get_ro(device_t, device_t);
static void bcm_sdhci_dma_intr(int ch, void *arg);
static void bcm_sdhci_start_dma(struct sdhci_slot *slot);

static void
bcm_sdhci_dmacb(void *arg, bus_dma_segment_t *segs, int nseg, int err)
{
	struct bcm_sdhci_softc *sc = arg;
	int i;

	/* Sanity check: we can only ever have one mapping at a time. */
	KASSERT(sc->dmamap_seg_count == 0, ("leaked DMA segment"));
	sc->dmamap_status = err;
	sc->dmamap_seg_count = nseg;

	/* Note nseg is guaranteed to be zero if err is non-zero. */
	for (i = 0; i < nseg; i++) {
		sc->dmamap_seg_addrs[i] = segs[i].ds_addr;
		sc->dmamap_seg_sizes[i] = segs[i].ds_len;
	}
}

static int
bcm_sdhci_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "Broadcom 2708 SDHCI controller");

	return (BUS_PROBE_DEFAULT);
}

static int
bcm_sdhci_attach(device_t dev)
{
	struct bcm_sdhci_softc *sc = device_get_softc(dev);
	int rid, err;
	phandle_t node;
	pcell_t cell;
	u_int default_freq;

	sc->sc_dev = dev;
	sc->sc_req = NULL;

	sc->conf = (struct bcm_mmc_conf *)ofw_bus_search_compatible(dev,
	    compat_data)->ocd_data;
	if (sc->conf == 0)
	    return (ENXIO);

	err = bcm2835_mbox_set_power_state(BCM2835_MBOX_POWER_ID_EMMC, TRUE);
	if (err != 0) {
		if (bootverbose)
			device_printf(dev, "Unable to enable the power\n");
		return (err);
	}

	default_freq = 0;
	err = bcm2835_mbox_get_clock_rate(sc->conf->clock_id, &default_freq);
	if (err == 0) {
		/* Convert to MHz */
		default_freq /= 1000000;
	}
	if (default_freq == 0) {
		node = ofw_bus_get_node(sc->sc_dev);
		if ((OF_getencprop(node, "clock-frequency", &cell,
		    sizeof(cell))) > 0)
			default_freq = cell / 1000000;
	}
	if (default_freq == 0)
		default_freq = sc->conf->default_freq;

	if (bootverbose)
		device_printf(dev, "SDHCI frequency: %dMHz\n", default_freq);
#ifdef NOTYET
	if (sc->conf->clock_src > 0) {
		uint32_t f;
		sc->clkman = devclass_get_device(
		    devclass_find("bcm2835_clkman"), 0);
		if (sc->clkman == NULL) {
			device_printf(dev, "cannot find Clock Manager\n");
			return (ENXIO);
		}

		f = bcm2835_clkman_set_frequency(sc->clkman,
		    sc->conf->clock_src, default_freq);
		if (f == 0)
			return (EINVAL);

		if (bootverbose)
			device_printf(dev, "Clock source frequency: %dMHz\n",
			    f);
	}
#endif

	rid = 0;
	sc->sc_mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (!sc->sc_mem_res) {
		device_printf(dev, "cannot allocate memory window\n");
		err = ENXIO;
		goto fail;
	}

	sc->sc_bst = rman_get_bustag(sc->sc_mem_res);
	sc->sc_bsh = rman_get_bushandle(sc->sc_mem_res);

	rid = 0;
	sc->sc_irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE | RF_SHAREABLE);
	if (!sc->sc_irq_res) {
		device_printf(dev, "cannot allocate interrupt\n");
		err = ENXIO;
		goto fail;
	}

	if (bus_setup_intr(dev, sc->sc_irq_res, INTR_TYPE_BIO | INTR_MPSAFE,
	    NULL, bcm_sdhci_intr, sc, &sc->sc_intrhand)) {
		device_printf(dev, "cannot setup interrupt handler\n");
		err = ENXIO;
		goto fail;
	}

	if (!bcm2835_sdhci_pio_mode)
		sc->sc_slot.opt = SDHCI_PLATFORM_TRANSFER;

	sc->sc_slot.caps = SDHCI_CAN_VDD_330 | SDHCI_CAN_VDD_180;
	if (bcm2835_sdhci_hs)
		sc->sc_slot.caps |= SDHCI_CAN_DO_HISPD;
	sc->sc_slot.caps |= (default_freq << SDHCI_CLOCK_BASE_SHIFT);
	sc->sc_slot.quirks = sc->conf->quirks;

	sdhci_init_slot(dev, &sc->sc_slot, 0);
	mmc_fdt_parse(dev, 0, &sc->sc_mmc_helper, &sc->sc_slot.host);

	sc->sc_dma_ch = bcm_dma_allocate(BCM_DMA_CH_ANY);
	if (sc->sc_dma_ch == BCM_DMA_CH_INVALID)
		goto fail;

	err = bcm_dma_setup_intr(sc->sc_dma_ch, bcm_sdhci_dma_intr, sc);
	if (err != 0) {
		device_printf(dev,
		    "cannot setup dma interrupt handler\n");
		err = ENXIO;
		goto fail;
	}

	/* Allocate bus_dma resources. */
	err = bus_dma_tag_create(bus_get_dma_tag(dev),
	    1, 0, bcm283x_dmabus_peripheral_lowaddr(),
	    BUS_SPACE_MAXADDR, NULL, NULL,
	    BCM_DMA_MAXSIZE, ALLOCATED_DMA_SEGS, BCM_SDHCI_BUFFER_SIZE,
	    BUS_DMA_ALLOCNOW, NULL, NULL,
	    &sc->sc_dma_tag);

	if (err) {
		device_printf(dev, "failed allocate DMA tag");
		goto fail;
	}

	err = bus_dmamap_create(sc->sc_dma_tag, 0, &sc->sc_dma_map);
	if (err) {
		device_printf(dev, "bus_dmamap_create failed\n");
		goto fail;
	}

	/* FIXME: Fix along with other BUS_SPACE_PHYSADDR instances */
	sc->sc_sdhci_buffer_phys = rman_get_start(sc->sc_mem_res) +
	    SDHCI_BUFFER;

	bus_generic_probe(dev);
	bus_generic_attach(dev);

	sdhci_start_slot(&sc->sc_slot);

	/* Seed our copies. */
	sc->blksz_and_count = SDHCI_READ_4(dev, &sc->sc_slot, SDHCI_BLOCK_SIZE);
	sc->cmd_and_mode = SDHCI_READ_4(dev, &sc->sc_slot, SDHCI_TRANSFER_MODE);

	return (0);

fail:
	if (sc->sc_intrhand)
		bus_teardown_intr(dev, sc->sc_irq_res, sc->sc_intrhand);
	if (sc->sc_irq_res)
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->sc_irq_res);
	if (sc->sc_mem_res)
		bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->sc_mem_res);

	return (err);
}

static int
bcm_sdhci_detach(device_t dev)
{

	return (EBUSY);
}

static void
bcm_sdhci_intr(void *arg)
{
	struct bcm_sdhci_softc *sc = arg;

	sdhci_generic_intr(&sc->sc_slot);
}

static int
bcm_sdhci_update_ios(device_t bus, device_t child)
{
#ifdef EXT_RESOURCES
	struct bcm_sdhci_softc *sc;
	struct mmc_ios *ios;
#endif
	int rv;

#ifdef EXT_RESOURCES
	sc = device_get_softc(bus);
	ios = &sc->sc_slot.host.ios;

	if (ios->power_mode == power_up) {
		if (sc->sc_mmc_helper.vmmc_supply)
			regulator_enable(sc->sc_mmc_helper.vmmc_supply);
		if (sc->sc_mmc_helper.vqmmc_supply)
			regulator_enable(sc->sc_mmc_helper.vqmmc_supply);
	}
#endif

	rv = sdhci_generic_update_ios(bus, child);
	if (rv != 0)
		return (rv);

#ifdef EXT_RESOURCES
	if (ios->power_mode == power_off) {
		if (sc->sc_mmc_helper.vmmc_supply)
			regulator_disable(sc->sc_mmc_helper.vmmc_supply);
		if (sc->sc_mmc_helper.vqmmc_supply)
			regulator_disable(sc->sc_mmc_helper.vqmmc_supply);
	}
#endif

	return (0);
}

static int
bcm_sdhci_get_ro(device_t bus, device_t child)
{

	return (0);
}

static inline uint32_t
RD4(struct bcm_sdhci_softc *sc, bus_size_t off)
{
	uint32_t val = bus_space_read_4(sc->sc_bst, sc->sc_bsh, off);
	return val;
}

static inline void
WR4(struct bcm_sdhci_softc *sc, bus_size_t off, uint32_t val)
{

	bus_space_write_4(sc->sc_bst, sc->sc_bsh, off, val);
	/*
	 * The Arasan HC has a bug where it may lose the content of
	 * consecutive writes to registers that are within two SD-card
	 * clock cycles of each other (a clock domain crossing problem).
	 */
	if (sc->sc_slot.clock > 0)
		DELAY(((2 * 1000000) / sc->sc_slot.clock) + 1);
}

static uint8_t
bcm_sdhci_read_1(device_t dev, struct sdhci_slot *slot, bus_size_t off)
{
	struct bcm_sdhci_softc *sc = device_get_softc(dev);
	uint32_t val = RD4(sc, off & ~3);

	return ((val >> (off & 3)*8) & 0xff);
}

static uint16_t
bcm_sdhci_read_2(device_t dev, struct sdhci_slot *slot, bus_size_t off)
{
	struct bcm_sdhci_softc *sc = device_get_softc(dev);
	uint32_t val32;

	/*
	 * Standard 32-bit handling of command and transfer mode, as
	 * well as block size and count.
	 */
	if ((off == SDHCI_BLOCK_SIZE || off == SDHCI_BLOCK_COUNT) &&
	    sc->need_update_blk)
		val32 = sc->blksz_and_count;
	else if (off == SDHCI_TRANSFER_MODE || off == SDHCI_COMMAND_FLAGS)
		val32 = sc->cmd_and_mode;
	else
		val32 = RD4(sc, off & ~3);

	return ((val32 >> (off & 3)*8) & 0xffff);
}

static uint32_t
bcm_sdhci_read_4(device_t dev, struct sdhci_slot *slot, bus_size_t off)
{
	struct bcm_sdhci_softc *sc = device_get_softc(dev);

	return RD4(sc, off);
}

static void
bcm_sdhci_read_multi_4(device_t dev, struct sdhci_slot *slot, bus_size_t off,
    uint32_t *data, bus_size_t count)
{
	struct bcm_sdhci_softc *sc = device_get_softc(dev);

	bus_space_read_multi_4(sc->sc_bst, sc->sc_bsh, off, data, count);
}

static void
bcm_sdhci_write_1(device_t dev, struct sdhci_slot *slot, bus_size_t off,
    uint8_t val)
{
	struct bcm_sdhci_softc *sc = device_get_softc(dev);
	uint32_t val32 = RD4(sc, off & ~3);
	val32 &= ~(0xff << (off & 3)*8);
	val32 |= (val << (off & 3)*8);
	WR4(sc, off & ~3, val32);
}

static void
bcm_sdhci_write_2(device_t dev, struct sdhci_slot *slot, bus_size_t off,
    uint16_t val)
{
	struct bcm_sdhci_softc *sc = device_get_softc(dev);
	uint32_t val32;

	/*
	 * If we have a queued up 16bit value for blk size or count, use and
	 * update the saved value rather than doing any real register access.
	 * If we did not touch either since the last write, then read from
	 * register as at least block count can change.
	 * Similarly, if we are about to issue a command, always use the saved
	 * value for transfer mode as we can never write that without issuing
	 * a command.
	 */
	if ((off == SDHCI_BLOCK_SIZE || off == SDHCI_BLOCK_COUNT) &&
	    sc->need_update_blk)
		val32 = sc->blksz_and_count;
	else if (off == SDHCI_COMMAND_FLAGS)
		val32 = sc->cmd_and_mode;
	else
		val32 = RD4(sc, off & ~3);

	val32 &= ~(0xffff << (off & 3)*8);
	val32 |= (val << (off & 3)*8);

	if (off == SDHCI_TRANSFER_MODE)
		sc->cmd_and_mode = val32;
	else if (off == SDHCI_BLOCK_SIZE || off == SDHCI_BLOCK_COUNT) {
		sc->blksz_and_count = val32;
		sc->need_update_blk = true;
	} else {
		if (off == SDHCI_COMMAND_FLAGS) {
			/* If we saved blk writes, do them now before cmd. */
			if (sc->need_update_blk) {
				WR4(sc, SDHCI_BLOCK_SIZE, sc->blksz_and_count);
				sc->need_update_blk = false;
			}
			/* Always save cmd and mode registers. */
			sc->cmd_and_mode = val32;
		}
		WR4(sc, off & ~3, val32);
	}
}

static void
bcm_sdhci_write_4(device_t dev, struct sdhci_slot *slot, bus_size_t off,
    uint32_t val)
{
	struct bcm_sdhci_softc *sc = device_get_softc(dev);
	WR4(sc, off, val);
}

static void
bcm_sdhci_write_multi_4(device_t dev, struct sdhci_slot *slot, bus_size_t off,
    uint32_t *data, bus_size_t count)
{
	struct bcm_sdhci_softc *sc = device_get_softc(dev);

	bus_space_write_multi_4(sc->sc_bst, sc->sc_bsh, off, data, count);
}

static void
bcm_sdhci_start_dma_seg(struct bcm_sdhci_softc *sc)
{
	struct sdhci_slot *slot;
	vm_paddr_t pdst, psrc;
	int err, idx, len, sync_op, width;

	slot = &sc->sc_slot;
	mtx_assert(&slot->mtx, MA_OWNED);
	idx = sc->dmamap_seg_index++;
	len = sc->dmamap_seg_sizes[idx];
	slot->offset += len;
	width = (len & 0xf ? BCM_DMA_32BIT : BCM_DMA_128BIT);

	if (slot->curcmd->data->flags & MMC_DATA_READ) {
		/*
		 * Peripherals on the AXI bus do not need DREQ pacing for reads
		 * from the ARM core, so we can safely set this to NONE.
		 */
		bcm_dma_setup_src(sc->sc_dma_ch, BCM_DMA_DREQ_NONE,
		    BCM_DMA_SAME_ADDR, BCM_DMA_32BIT);
		bcm_dma_setup_dst(sc->sc_dma_ch, BCM_DMA_DREQ_NONE,
		    BCM_DMA_INC_ADDR, width);
		psrc = sc->sc_sdhci_buffer_phys;
		pdst = sc->dmamap_seg_addrs[idx];
		sync_op = BUS_DMASYNC_PREREAD;
	} else {
		/*
		 * The ordering here is important, because the last write to
		 * dst/src in the dma control block writes the real dreq value.
		 */
		bcm_dma_setup_src(sc->sc_dma_ch, BCM_DMA_DREQ_NONE,
		    BCM_DMA_INC_ADDR, width);
		bcm_dma_setup_dst(sc->sc_dma_ch, sc->conf->emmc_dreq,
		    BCM_DMA_SAME_ADDR, BCM_DMA_32BIT);
		psrc = sc->dmamap_seg_addrs[idx];
		pdst = sc->sc_sdhci_buffer_phys;
		sync_op = BUS_DMASYNC_PREWRITE;
	}

	/*
	 * When starting a new DMA operation do the busdma sync operation, and
	 * disable SDCHI data interrrupts because we'll be driven by DMA
	 * interrupts (or SDHCI error interrupts) until the IO is done.
	 */
	if (idx == 0) {
		bus_dmamap_sync(sc->sc_dma_tag, sc->sc_dma_map, sync_op);

		slot->intmask &= ~DATA_XFER_MASK;
		bcm_sdhci_write_4(sc->sc_dev, slot, SDHCI_SIGNAL_ENABLE,
		    slot->intmask);
	}

	/*
	 * Start the DMA transfer.  Only programming errors (like failing to
	 * allocate a channel) cause a non-zero return from bcm_dma_start().
	 */
	err = bcm_dma_start(sc->sc_dma_ch, psrc, pdst, len);
	KASSERT((err == 0), ("bcm2835_sdhci: failed DMA start"));
}

static void
bcm_sdhci_dma_exit(struct bcm_sdhci_softc *sc)
{
	struct sdhci_slot *slot = &sc->sc_slot;

	mtx_assert(&slot->mtx, MA_OWNED);

	/* Re-enable interrupts */
	slot->intmask |= DATA_XFER_MASK;
	bcm_sdhci_write_4(slot->bus, slot, SDHCI_SIGNAL_ENABLE,
	    slot->intmask);
}

static void
bcm_sdhci_dma_unload(struct bcm_sdhci_softc *sc)
{
	struct sdhci_slot *slot = &sc->sc_slot;

	if (sc->dmamap_seg_count == 0)
		return;
	if ((slot->curcmd->data->flags & MMC_DATA_READ) != 0)
		bus_dmamap_sync(sc->sc_dma_tag, sc->sc_dma_map,
		    BUS_DMASYNC_POSTREAD);
	else
		bus_dmamap_sync(sc->sc_dma_tag, sc->sc_dma_map,
		    BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(sc->sc_dma_tag, sc->sc_dma_map);

	sc->dmamap_seg_count = 0;
	sc->dmamap_seg_index = 0;
}

static void
bcm_sdhci_dma_intr(int ch, void *arg)
{
	struct bcm_sdhci_softc *sc = (struct bcm_sdhci_softc *)arg;
	struct sdhci_slot *slot = &sc->sc_slot;
	uint32_t reg;

	mtx_lock(&slot->mtx);
	if (slot->curcmd == NULL)
		goto out;
	/*
	 * If there are more segments for the current dma, start the next one.
	 * Otherwise unload the dma map and decide what to do next based on the
	 * status of the sdhci controller and whether there's more data left.
	 */
	if (sc->dmamap_seg_index < sc->dmamap_seg_count) {
		bcm_sdhci_start_dma_seg(sc);
		goto out;
	}

	bcm_sdhci_dma_unload(sc);

	/*
	 * If we had no further segments pending, we need to determine how to
	 * proceed next.  If the 'data/space pending' bit is already set and we
	 * can continue via DMA, do so.  Otherwise, re-enable interrupts and
	 * return.
	 */
	reg = bcm_sdhci_read_4(slot->bus, slot, SDHCI_INT_STATUS) &
	    DATA_XFER_MASK;
	if ((reg & DATA_PENDING_MASK) != 0 &&
	    BCM_SDHCI_SEGSZ_LEFT(slot) >= BCM_SDHCI_BUFFER_SIZE) {
		/* ACK any pending interrupts */
		bcm_sdhci_write_4(slot->bus, slot, SDHCI_INT_STATUS,
		    DATA_PENDING_MASK);

		bcm_sdhci_start_dma(slot);
		if (slot->curcmd->error != 0) {
			/* We won't recover from this error for this command. */
			bcm_sdhci_dma_unload(sc);
			bcm_sdhci_dma_exit(sc);
			sdhci_finish_data(slot);
		}
	} else if ((reg & SDHCI_INT_DATA_END) != 0) {
		bcm_sdhci_dma_exit(sc);
		bcm_sdhci_write_4(slot->bus, slot, SDHCI_INT_STATUS,
		    reg);
		slot->flags &= ~PLATFORM_DATA_STARTED;
		sdhci_finish_data(slot);
	} else {
		bcm_sdhci_dma_exit(sc);
	}
out:
	mtx_unlock(&slot->mtx);
}

static void
bcm_sdhci_start_dma(struct sdhci_slot *slot)
{
	struct bcm_sdhci_softc *sc = device_get_softc(slot->bus);
	uint8_t *buf;
	size_t left;

	mtx_assert(&slot->mtx, MA_OWNED);

	left = BCM_SDHCI_SEGSZ_LEFT(slot);
	buf = (uint8_t *)slot->curcmd->data->data + slot->offset;
	KASSERT(left != 0,
	    ("%s: DMA handling incorrectly indicated", __func__));

	/*
	 * No need to check segment count here; if we've not yet unloaded
	 * previous segments, we'll catch that in bcm_sdhci_dmacb.
	 */
	if (bus_dmamap_load(sc->sc_dma_tag, sc->sc_dma_map, buf, left,
	    bcm_sdhci_dmacb, sc, BUS_DMA_NOWAIT) != 0 ||
	    sc->dmamap_status != 0) {
		slot->curcmd->error = MMC_ERR_NO_MEMORY;
		return;
	}

	/* DMA start */
	bcm_sdhci_start_dma_seg(sc);
}

static int
bcm_sdhci_will_handle_transfer(device_t dev, struct sdhci_slot *slot)
{
#ifdef INVARIANTS
	struct bcm_sdhci_softc *sc = device_get_softc(slot->bus);
#endif

	/*
	 * This indicates that we somehow let a data interrupt slip by into the
	 * SDHCI framework, when it should not have.  This really needs to be
	 * caught and fixed ASAP, as it really shouldn't happen.
	 */
	KASSERT(sc->dmamap_seg_count == 0,
	    ("data pending interrupt pushed through SDHCI framework"));

	/*
	 * Do not use DMA for transfers less than our block size.  Checking
	 * alignment serves little benefit, as we round transfer sizes down to
	 * a multiple of the block size and push the transfer back to
	 * SDHCI-driven PIO once we're below the block size.
	 */
	if (BCM_SDHCI_SEGSZ_LEFT(slot) < BCM_DMA_BLOCK_SIZE)
		return (0);

	return (1);
}

static void
bcm_sdhci_start_transfer(device_t dev, struct sdhci_slot *slot,
    uint32_t *intmask)
{

	/* DMA transfer FIFO 1KB */
	bcm_sdhci_start_dma(slot);
}

static void
bcm_sdhci_finish_transfer(device_t dev, struct sdhci_slot *slot)
{
	struct bcm_sdhci_softc *sc = device_get_softc(slot->bus);

	/*
	 * Clean up.  Interrupts are clearly enabled, because we received an
	 * SDHCI_INT_DATA_END to get this far -- just make sure we don't leave
	 * anything laying around.
	 */
	if (sc->dmamap_seg_count != 0) {
		/*
		 * Our segment math should have worked out such that we would
		 * never finish the transfer without having used up all of the
		 * segments.  If we haven't, that means we must have erroneously
		 * regressed to SDHCI-driven PIO to finish the operation and
		 * this is certainly caused by developer-error.
		 */
		bcm_sdhci_dma_unload(sc);
	}

	sdhci_finish_data(slot);
}

static device_method_t bcm_sdhci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		bcm_sdhci_probe),
	DEVMETHOD(device_attach,	bcm_sdhci_attach),
	DEVMETHOD(device_detach,	bcm_sdhci_detach),

	/* Bus interface */
	DEVMETHOD(bus_read_ivar,	sdhci_generic_read_ivar),
	DEVMETHOD(bus_write_ivar,	sdhci_generic_write_ivar),
	DEVMETHOD(bus_add_child,	bus_generic_add_child),

	/* MMC bridge interface */
	DEVMETHOD(mmcbr_update_ios,	bcm_sdhci_update_ios),
	DEVMETHOD(mmcbr_request,	sdhci_generic_request),
	DEVMETHOD(mmcbr_get_ro,		bcm_sdhci_get_ro),
	DEVMETHOD(mmcbr_acquire_host,	sdhci_generic_acquire_host),
	DEVMETHOD(mmcbr_release_host,	sdhci_generic_release_host),

	/* Platform transfer methods */
	DEVMETHOD(sdhci_platform_will_handle,		bcm_sdhci_will_handle_transfer),
	DEVMETHOD(sdhci_platform_start_transfer,	bcm_sdhci_start_transfer),
	DEVMETHOD(sdhci_platform_finish_transfer,	bcm_sdhci_finish_transfer),
	/* SDHCI registers accessors */
	DEVMETHOD(sdhci_read_1,		bcm_sdhci_read_1),
	DEVMETHOD(sdhci_read_2,		bcm_sdhci_read_2),
	DEVMETHOD(sdhci_read_4,		bcm_sdhci_read_4),
	DEVMETHOD(sdhci_read_multi_4,	bcm_sdhci_read_multi_4),
	DEVMETHOD(sdhci_write_1,	bcm_sdhci_write_1),
	DEVMETHOD(sdhci_write_2,	bcm_sdhci_write_2),
	DEVMETHOD(sdhci_write_4,	bcm_sdhci_write_4),
	DEVMETHOD(sdhci_write_multi_4,	bcm_sdhci_write_multi_4),

	DEVMETHOD_END
};

static devclass_t bcm_sdhci_devclass;

static driver_t bcm_sdhci_driver = {
	"sdhci_bcm",
	bcm_sdhci_methods,
	sizeof(struct bcm_sdhci_softc),
};

DRIVER_MODULE(sdhci_bcm, simplebus, bcm_sdhci_driver, bcm_sdhci_devclass,
    NULL, NULL);
#ifdef NOTYET
MODULE_DEPEND(sdhci_bcm, bcm2835_clkman, 1, 1, 1);
#endif
SDHCI_DEPEND(sdhci_bcm);
#ifndef MMCCAM
MMC_DECLARE_BRIDGE(sdhci_bcm);
#endif
