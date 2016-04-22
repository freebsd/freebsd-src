/*-
 * Copyright (c) 2006 Bernd Walter.  All rights reserved.
 * Copyright (c) 2006 M. Warner Losh.  All rights reserved.
 * Copyright (c) 2010 Greg Ansley.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "opt_platform.h"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bio.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/endian.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/queue.h>
#include <sys/resource.h>
#include <sys/rman.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/timetc.h>
#include <sys/watchdog.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/resource.h>
#include <machine/intr.h>

#include <arm/at91/at91var.h>
#include <arm/at91/at91_mcireg.h>
#include <arm/at91/at91_pdcreg.h>

#include <dev/mmc/bridge.h>
#include <dev/mmc/mmcreg.h>
#include <dev/mmc/mmcbrvar.h>

#ifdef FDT
#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#endif

#include "mmcbr_if.h"

#include "opt_at91.h"

/*
 * About running the MCI bus above 25MHz
 *
 * Historically, the MCI bus has been run at 30MHz on systems with a 60MHz
 * master clock, in part due to a bug in dev/mmc.c making always request
 * 30MHz, and in part over clocking the bus because 15MHz was too slow.
 * Fixing that bug causes the mmc driver to request a 25MHz clock (as it
 * should) and the logic in at91_mci_update_ios() picks the highest speed that
 * doesn't exceed that limit.  With a 60MHz MCK that would be 15MHz, and
 * that's a real performance buzzkill when you've been getting away with 30MHz
 * all along.
 *
 * By defining AT91_MCI_ALLOW_OVERCLOCK (or setting the allow_overclock=1
 * device hint or sysctl) you can enable logic in at91_mci_update_ios() to
 * overlcock the SD bus a little by running it at MCK / 2 when the requested
 * speed is 25MHz and the next highest speed is 15MHz or less.  This appears
 * to work on virtually all SD cards, since it is what this driver has been
 * doing prior to the introduction of this option, where the overclocking vs
 * underclocking decision was automaticly "overclock".  Modern SD cards can
 * run at 45mhz/1-bit in standard mode (high speed mode enable commands not
 * sent) without problems.
 *
 * Speaking of high-speed mode, the rm9200 manual says the MCI device supports
 * the SD v1.0 specification and can run up to 50MHz.  This is interesting in
 * that the SD v1.0 spec caps the speed at 25MHz; high speed mode was added in
 * the v1.10 spec.  Furthermore, high speed mode doesn't just crank up the
 * clock, it alters the signal timing.  The rm9200 MCI device doesn't support
 * these altered timings.  So while speeds over 25MHz may work, they only work
 * in what the SD spec calls "default" speed mode, and it amounts to violating
 * the spec by overclocking the bus.
 *
 * If you also enable 4-wire mode it's possible transfers faster than 25MHz
 * will fail.  On the AT91RM9200, due to bugs in the bus contention logic, if
 * you have the USB host device and OHCI driver enabled will fail.  Even
 * underclocking to 15MHz, intermittant overrun and underrun errors occur.
 * Note that you don't even need to have usb devices attached to the system,
 * the errors begin to occur as soon as the OHCI driver sets the register bit
 * to enable periodic transfers.  It appears (based on brief investigation)
 * that the usb host controller uses so much ASB bandwidth that sometimes the
 * DMA for MCI transfers doesn't get a bus grant in time and data gets
 * dropped.  Adding even a modicum of network activity changes the symptom
 * from intermittant to very frequent.  Members of the AT91SAM9 family have
 * corrected this problem, or are at least better about their use of the bus.
 */
#ifndef AT91_MCI_ALLOW_OVERCLOCK
#define AT91_MCI_ALLOW_OVERCLOCK 1
#endif

/*
 * Allocate 2 bounce buffers we'll use to endian-swap the data due to the rm9200
 * erratum.  We use a pair of buffers because when reading that lets us begin
 * endian-swapping the data in the first buffer while the DMA is reading into
 * the second buffer.  (We can't use the same trick for writing because we might
 * not get all the data in the 2nd buffer swapped before the hardware needs it;
 * dealing with that would add complexity to the driver.)
 *
 * The buffers are sized at 16K each due to the way the busdma cache sync
 * operations work on arm.  A dcache_inv_range() operation on a range larger
 * than 16K gets turned into a dcache_wbinv_all().  That needlessly flushes the
 * entire data cache, impacting overall system performance.
 */
#define BBCOUNT     2
#define BBSIZE      (16*1024)
#define MAX_BLOCKS  ((BBSIZE*BBCOUNT)/512)

static int mci_debug;

struct at91_mci_softc {
	void *intrhand;			/* Interrupt handle */
	device_t dev;
	int sc_cap;
#define	CAP_HAS_4WIRE		1	/* Has 4 wire bus */
#define	CAP_NEEDS_BYTESWAP	2	/* broken hardware needing bounce */
#define	CAP_MCI1_REV2XX		4	/* MCI 1 rev 2.x */
	int flags;
#define PENDING_CMD	0x01
#define PENDING_STOP	0x02
#define CMD_MULTIREAD	0x10
#define CMD_MULTIWRITE	0x20
	int has_4wire;
	int allow_overclock;
	struct resource *irq_res;	/* IRQ resource */
	struct resource	*mem_res;	/* Memory resource */
	struct mtx sc_mtx;
	bus_dma_tag_t dmatag;
	struct mmc_host host;
	int bus_busy;
	struct mmc_request *req;
	struct mmc_command *curcmd;
	bus_dmamap_t bbuf_map[BBCOUNT];
	char      *  bbuf_vaddr[BBCOUNT]; /* bounce bufs in KVA space */
	uint32_t     bbuf_len[BBCOUNT];	  /* len currently queued for bounce buf */
	uint32_t     bbuf_curidx;	  /* which bbuf is the active DMA buffer */
	uint32_t     xfer_offset;	  /* offset so far into caller's buf */
};

/* bus entry points */
static int at91_mci_probe(device_t dev);
static int at91_mci_attach(device_t dev);
static int at91_mci_detach(device_t dev);
static void at91_mci_intr(void *);

/* helper routines */
static int at91_mci_activate(device_t dev);
static void at91_mci_deactivate(device_t dev);
static int at91_mci_is_mci1rev2xx(void);

#define AT91_MCI_LOCK(_sc)		mtx_lock(&(_sc)->sc_mtx)
#define	AT91_MCI_UNLOCK(_sc)		mtx_unlock(&(_sc)->sc_mtx)
#define AT91_MCI_LOCK_INIT(_sc) \
	mtx_init(&_sc->sc_mtx, device_get_nameunit(_sc->dev), \
	    "mci", MTX_DEF)
#define AT91_MCI_LOCK_DESTROY(_sc)	mtx_destroy(&_sc->sc_mtx);
#define AT91_MCI_ASSERT_LOCKED(_sc)	mtx_assert(&_sc->sc_mtx, MA_OWNED);
#define AT91_MCI_ASSERT_UNLOCKED(_sc) mtx_assert(&_sc->sc_mtx, MA_NOTOWNED);

static inline uint32_t
RD4(struct at91_mci_softc *sc, bus_size_t off)
{
	return (bus_read_4(sc->mem_res, off));
}

static inline void
WR4(struct at91_mci_softc *sc, bus_size_t off, uint32_t val)
{
	bus_write_4(sc->mem_res, off, val);
}

static void
at91_bswap_buf(struct at91_mci_softc *sc, void * dptr, void * sptr, uint32_t memsize)
{
	uint32_t * dst = (uint32_t *)dptr;
	uint32_t * src = (uint32_t *)sptr;
	uint32_t   i;

	/*
	 * If the hardware doesn't need byte-swapping, let bcopy() do the
	 * work.  Use bounce buffer even if we don't need byteswap, since
	 * buffer may straddle a page boundry, and we don't handle
	 * multi-segment transfers in hardware.  Seen from 'bsdlabel -w' which
	 * uses raw geom access to the volume.  Greg Ansley (gja (at)
	 * ansley.com)
	 */
	if (!(sc->sc_cap & CAP_NEEDS_BYTESWAP)) {
		memcpy(dptr, sptr, memsize);
		return;
	}

	/*
	 * Nice performance boost for slightly unrolling this loop.
	 * (But very little extra boost for further unrolling it.)
	 */
	for (i = 0; i < memsize; i += 16) {
		*dst++ = bswap32(*src++);
		*dst++ = bswap32(*src++);
		*dst++ = bswap32(*src++);
		*dst++ = bswap32(*src++);
	}

	/* Mop up the last 1-3 words, if any. */
	for (i = 0; i < (memsize & 0x0F); i += 4) {
		*dst++ = bswap32(*src++);
	}
}

static void
at91_mci_getaddr(void *arg, bus_dma_segment_t *segs, int nsegs, int error)
{
	if (error != 0)
		return;
	*(bus_addr_t *)arg = segs[0].ds_addr;
}

static void
at91_mci_pdc_disable(struct at91_mci_softc *sc)
{
	WR4(sc, PDC_PTCR, PDC_PTCR_TXTDIS | PDC_PTCR_RXTDIS);
	WR4(sc, PDC_RPR, 0);
	WR4(sc, PDC_RCR, 0);
	WR4(sc, PDC_RNPR, 0);
	WR4(sc, PDC_RNCR, 0);
	WR4(sc, PDC_TPR, 0);
	WR4(sc, PDC_TCR, 0);
	WR4(sc, PDC_TNPR, 0);
	WR4(sc, PDC_TNCR, 0);
}

/*
 * Reset the controller, then restore most of the current state.
 *
 * This is called after detecting an error.  It's also called after stopping a
 * multi-block write, to un-wedge the device so that it will handle the NOTBUSY
 * signal correctly.  See comments in at91_mci_stop_done() for more details.
 */
static void at91_mci_reset(struct at91_mci_softc *sc)
{
	uint32_t mr;
	uint32_t sdcr;
	uint32_t dtor;
	uint32_t imr;

	at91_mci_pdc_disable(sc);

	/* save current state */

	imr  = RD4(sc, MCI_IMR);
	mr   = RD4(sc, MCI_MR) & 0x7fff;
	sdcr = RD4(sc, MCI_SDCR);
	dtor = RD4(sc, MCI_DTOR);

	/* reset the controller */

	WR4(sc, MCI_IDR, 0xffffffff);
	WR4(sc, MCI_CR, MCI_CR_MCIDIS | MCI_CR_SWRST);

	/* restore state */

	WR4(sc, MCI_CR, MCI_CR_MCIEN|MCI_CR_PWSEN);
	WR4(sc, MCI_MR, mr);
	WR4(sc, MCI_SDCR, sdcr);
	WR4(sc, MCI_DTOR, dtor);
	WR4(sc, MCI_IER, imr);

	/*
	 * Make sure sdio interrupts will fire.  Not sure why reading
	 * SR ensures that, but this is in the linux driver.
	 */

	RD4(sc, MCI_SR);
}

static void
at91_mci_init(device_t dev)
{
	struct at91_mci_softc *sc = device_get_softc(dev);
	uint32_t val;

	WR4(sc, MCI_CR, MCI_CR_MCIDIS | MCI_CR_SWRST); /* device into reset */
	WR4(sc, MCI_IDR, 0xffffffff);		/* Turn off interrupts */
	WR4(sc, MCI_DTOR, MCI_DTOR_DTOMUL_1M | 1);
	val = MCI_MR_PDCMODE;
	val |= 0x34a;				/* PWSDIV = 3; CLKDIV = 74 */
//	if (sc->sc_cap & CAP_MCI1_REV2XX)
//		val |= MCI_MR_RDPROOF | MCI_MR_WRPROOF;
	WR4(sc, MCI_MR, val);
#ifndef  AT91_MCI_SLOT_B
	WR4(sc, MCI_SDCR, 0);			/* SLOT A, 1 bit bus */
#else
	/*
	 * XXX Really should add second "unit" but nobody using using
	 * a two slot card that we know of. XXX
	 */
	WR4(sc, MCI_SDCR, 1);			/* SLOT B, 1 bit bus */
#endif
	/*
	 * Enable controller, including power-save.  The slower clock
	 * of the power-save mode is only in effect when there is no
	 * transfer in progress, so it can be left in this mode all
	 * the time.
	 */
	WR4(sc, MCI_CR, MCI_CR_MCIEN|MCI_CR_PWSEN);
}

static void
at91_mci_fini(device_t dev)
{
	struct at91_mci_softc *sc = device_get_softc(dev);

	WR4(sc, MCI_IDR, 0xffffffff);		/* Turn off interrupts */
	at91_mci_pdc_disable(sc);
	WR4(sc, MCI_CR, MCI_CR_MCIDIS | MCI_CR_SWRST); /* device into reset */
}

static int
at91_mci_probe(device_t dev)
{
#ifdef FDT
	if (!ofw_bus_is_compatible(dev, "atmel,hsmci"))
		return (ENXIO);
#endif
	device_set_desc(dev, "MCI mmc/sd host bridge");
	return (0);
}

static int
at91_mci_attach(device_t dev)
{
	struct at91_mci_softc *sc = device_get_softc(dev);
	struct sysctl_ctx_list *sctx;
	struct sysctl_oid *soid;
	device_t child;
	int err, i;

	sctx = device_get_sysctl_ctx(dev);
	soid = device_get_sysctl_tree(dev);

	sc->dev = dev;
	sc->sc_cap = 0;
	if (at91_is_rm92())
		sc->sc_cap |= CAP_NEEDS_BYTESWAP;
	/*
	 * MCI1 Rev 2 controllers need some workarounds, flag if so.
	 */
	if (at91_mci_is_mci1rev2xx())
		sc->sc_cap |= CAP_MCI1_REV2XX;

	err = at91_mci_activate(dev);
	if (err)
		goto out;

	AT91_MCI_LOCK_INIT(sc);

	at91_mci_fini(dev);
	at91_mci_init(dev);

	/*
	 * Allocate DMA tags and maps and bounce buffers.
	 *
	 * The parms in the tag_create call cause the dmamem_alloc call to
	 * create each bounce buffer as a single contiguous buffer of BBSIZE
	 * bytes aligned to a 4096 byte boundary.
	 *
	 * Do not use DMA_COHERENT for these buffers because that maps the
	 * memory as non-cachable, which prevents cache line burst fills/writes,
	 * which is something we need since we're trying to overlap the
	 * byte-swapping with the DMA operations.
	 */
	err = bus_dma_tag_create(bus_get_dma_tag(dev), 4096, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL,
	    BBSIZE, 1, BBSIZE, 0, NULL, NULL, &sc->dmatag);
	if (err != 0)
		goto out;

	for (i = 0; i < BBCOUNT; ++i) {
		err = bus_dmamem_alloc(sc->dmatag, (void **)&sc->bbuf_vaddr[i],
		    BUS_DMA_NOWAIT, &sc->bbuf_map[i]);
		if (err != 0)
			goto out;
	}

	/*
	 * Activate the interrupt
	 */
	err = bus_setup_intr(dev, sc->irq_res, INTR_TYPE_MISC | INTR_MPSAFE,
	    NULL, at91_mci_intr, sc, &sc->intrhand);
	if (err) {
		AT91_MCI_LOCK_DESTROY(sc);
		goto out;
	}

	/*
	 * Allow 4-wire to be initially set via #define.
	 * Allow a device hint to override that.
	 * Allow a sysctl to override that.
	 */
#if defined(AT91_MCI_HAS_4WIRE) && AT91_MCI_HAS_4WIRE != 0
	sc->has_4wire = 1;
#endif
	resource_int_value(device_get_name(dev), device_get_unit(dev),
			   "4wire", &sc->has_4wire);
	SYSCTL_ADD_UINT(sctx, SYSCTL_CHILDREN(soid), OID_AUTO, "4wire",
	    CTLFLAG_RW, &sc->has_4wire, 0, "has 4 wire SD Card bus");
	if (sc->has_4wire)
		sc->sc_cap |= CAP_HAS_4WIRE;

	sc->allow_overclock = AT91_MCI_ALLOW_OVERCLOCK;
	resource_int_value(device_get_name(dev), device_get_unit(dev),
			   "allow_overclock", &sc->allow_overclock);
	SYSCTL_ADD_UINT(sctx, SYSCTL_CHILDREN(soid), OID_AUTO, "allow_overclock",
	    CTLFLAG_RW, &sc->allow_overclock, 0,
	    "Allow up to 30MHz clock for 25MHz request when next highest speed 15MHz or less.");

	SYSCTL_ADD_UINT(sctx, SYSCTL_CHILDREN(soid), OID_AUTO, "debug",
	    CTLFLAG_RWTUN, &mci_debug, 0, "enable debug output");

	/*
	 * Our real min freq is master_clock/512, but upper driver layers are
	 * going to set the min speed during card discovery, and the right speed
	 * for that is 400kHz, so advertise a safe value just under that.
	 *
	 * For max speed, while the rm9200 manual says the max is 50mhz, it also
	 * says it supports only the SD v1.0 spec, which means the real limit is
	 * 25mhz. On the other hand, historical use has been to slightly violate
	 * the standard by running the bus at 30MHz.  For more information on
	 * that, see the comments at the top of this file.
	 */
	sc->host.f_min = 375000;
	sc->host.f_max = at91_master_clock / 2;
	if (sc->host.f_max > 25000000)
		sc->host.f_max = 25000000;
	sc->host.host_ocr = MMC_OCR_320_330 | MMC_OCR_330_340;
	sc->host.caps = 0;
	if (sc->sc_cap & CAP_HAS_4WIRE)
		sc->host.caps |= MMC_CAP_4_BIT_DATA;

	child = device_add_child(dev, "mmc", 0);
	device_set_ivars(dev, &sc->host);
	err = bus_generic_attach(dev);
out:
	if (err)
		at91_mci_deactivate(dev);
	return (err);
}

static int
at91_mci_detach(device_t dev)
{
	struct at91_mci_softc *sc = device_get_softc(dev);

	at91_mci_fini(dev);
	at91_mci_deactivate(dev);

	bus_dmamem_free(sc->dmatag, sc->bbuf_vaddr[0], sc->bbuf_map[0]);
	bus_dmamem_free(sc->dmatag, sc->bbuf_vaddr[1], sc->bbuf_map[1]);
	bus_dma_tag_destroy(sc->dmatag);

	return (EBUSY);	/* XXX */
}

static int
at91_mci_activate(device_t dev)
{
	struct at91_mci_softc *sc;
	int rid;

	sc = device_get_softc(dev);
	rid = 0;
	sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->mem_res == NULL)
		goto errout;

	rid = 0;
	sc->irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE);
	if (sc->irq_res == NULL)
		goto errout;

	return (0);
errout:
	at91_mci_deactivate(dev);
	return (ENOMEM);
}

static void
at91_mci_deactivate(device_t dev)
{
	struct at91_mci_softc *sc;

	sc = device_get_softc(dev);
	if (sc->intrhand)
		bus_teardown_intr(dev, sc->irq_res, sc->intrhand);
	sc->intrhand = NULL;
	bus_generic_detach(sc->dev);
	if (sc->mem_res)
		bus_release_resource(dev, SYS_RES_MEMORY,
		    rman_get_rid(sc->mem_res), sc->mem_res);
	sc->mem_res = NULL;
	if (sc->irq_res)
		bus_release_resource(dev, SYS_RES_IRQ,
		    rman_get_rid(sc->irq_res), sc->irq_res);
	sc->irq_res = NULL;
	return;
}

static int
at91_mci_is_mci1rev2xx(void)
{

	switch (soc_info.type) {
	case AT91_T_SAM9260:
	case AT91_T_SAM9263:
	case AT91_T_CAP9:
	case AT91_T_SAM9G10:
	case AT91_T_SAM9G20:
	case AT91_T_SAM9RL:
		return(1);
	default:
		return (0);
	}
}

static int
at91_mci_update_ios(device_t brdev, device_t reqdev)
{
	struct at91_mci_softc *sc;
	struct mmc_ios *ios;
	uint32_t clkdiv;
	uint32_t freq;

	sc = device_get_softc(brdev);
	ios = &sc->host.ios;

	/*
	 * Calculate our closest available clock speed that doesn't exceed the
	 * requested speed.
	 *
	 * When overclocking is allowed, the requested clock is 25MHz, the
	 * computed frequency is 15MHz or smaller and clockdiv is 1, use
	 * clockdiv of 0 to double that.  If less than 12.5MHz, double
	 * regardless of the overclocking setting.
	 *
	 * Whatever we come up with, store it back into ios->clock so that the
	 * upper layer drivers can report the actual speed of the bus.
	 */
	if (ios->clock == 0) {
		WR4(sc, MCI_CR, MCI_CR_MCIDIS);
		clkdiv = 0;
	} else {
		WR4(sc, MCI_CR, MCI_CR_MCIEN|MCI_CR_PWSEN);
		if ((at91_master_clock % (ios->clock * 2)) == 0)
			clkdiv = ((at91_master_clock / ios->clock) / 2) - 1;
		else
			clkdiv = (at91_master_clock / ios->clock) / 2;
		freq = at91_master_clock / ((clkdiv+1) * 2);
		if (clkdiv == 1 && ios->clock == 25000000 && freq <= 15000000) {
			if (sc->allow_overclock || freq <= 12500000) {
				clkdiv = 0;
				freq = at91_master_clock / ((clkdiv+1) * 2);
			}
		}
		ios->clock = freq;
	}
	if (ios->bus_width == bus_width_4)
		WR4(sc, MCI_SDCR, RD4(sc, MCI_SDCR) | MCI_SDCR_SDCBUS);
	else
		WR4(sc, MCI_SDCR, RD4(sc, MCI_SDCR) & ~MCI_SDCR_SDCBUS);
	WR4(sc, MCI_MR, (RD4(sc, MCI_MR) & ~MCI_MR_CLKDIV) | clkdiv);
	/* Do we need a settle time here? */
	/* XXX We need to turn the device on/off here with a GPIO pin */
	return (0);
}

static void
at91_mci_start_cmd(struct at91_mci_softc *sc, struct mmc_command *cmd)
{
	uint32_t cmdr, mr;
	struct mmc_data *data;

	sc->curcmd = cmd;
	data = cmd->data;

	/* XXX Upper layers don't always set this */
	cmd->mrq = sc->req;

	/* Begin setting up command register. */

	cmdr = cmd->opcode;

	if (sc->host.ios.bus_mode == opendrain)
		cmdr |= MCI_CMDR_OPDCMD;

	/* Set up response handling.  Allow max timeout for responses. */

	if (MMC_RSP(cmd->flags) == MMC_RSP_NONE)
		cmdr |= MCI_CMDR_RSPTYP_NO;
	else {
		cmdr |= MCI_CMDR_MAXLAT;
		if (cmd->flags & MMC_RSP_136)
			cmdr |= MCI_CMDR_RSPTYP_136;
		else
			cmdr |= MCI_CMDR_RSPTYP_48;
	}

	/*
	 * If there is no data transfer, just set up the right interrupt mask
	 * and start the command.
	 *
	 * The interrupt mask needs to be CMDRDY plus all non-data-transfer
	 * errors. It's important to leave the transfer-related errors out, to
	 * avoid spurious timeout or crc errors on a STOP command following a
	 * multiblock read.  When a multiblock read is in progress, sending a
	 * STOP in the middle of a block occasionally triggers such errors, but
	 * we're totally disinterested in them because we've already gotten all
	 * the data we wanted without error before sending the STOP command.
	 */

	if (data == NULL) {
		uint32_t ier = MCI_SR_CMDRDY |
		    MCI_SR_RTOE | MCI_SR_RENDE |
		    MCI_SR_RCRCE | MCI_SR_RDIRE | MCI_SR_RINDE;

		at91_mci_pdc_disable(sc);

		if (cmd->opcode == MMC_STOP_TRANSMISSION)
			cmdr |= MCI_CMDR_TRCMD_STOP;

		/* Ignore response CRC on CMD2 and ACMD41, per standard. */

		if (cmd->opcode == MMC_SEND_OP_COND ||
		    cmd->opcode == ACMD_SD_SEND_OP_COND)
			ier &= ~MCI_SR_RCRCE;

		if (mci_debug)
			printf("CMDR %x (opcode %d) ARGR %x no data\n",
			    cmdr, cmd->opcode, cmd->arg);

		WR4(sc, MCI_ARGR, cmd->arg);
		WR4(sc, MCI_CMDR, cmdr);
		WR4(sc, MCI_IDR, 0xffffffff);
		WR4(sc, MCI_IER, ier);
		return;
	}

	/* There is data, set up the transfer-related parts of the command. */

	if (data->flags & MMC_DATA_READ)
		cmdr |= MCI_CMDR_TRDIR;

	if (data->flags & (MMC_DATA_READ | MMC_DATA_WRITE))
		cmdr |= MCI_CMDR_TRCMD_START;

	if (data->flags & MMC_DATA_STREAM)
		cmdr |= MCI_CMDR_TRTYP_STREAM;
	else if (data->flags & MMC_DATA_MULTI) {
		cmdr |= MCI_CMDR_TRTYP_MULTIPLE;
		sc->flags |= (data->flags & MMC_DATA_READ) ?
		    CMD_MULTIREAD : CMD_MULTIWRITE;
	}

	/*
	 * Disable PDC until we're ready.
	 *
	 * Set block size and turn on PDC mode for dma xfer.
	 * Note that the block size is the smaller of the amount of data to be
	 * transferred, or 512 bytes.  The 512 size is fixed by the standard;
	 * smaller blocks are possible, but never larger.
	 */

	WR4(sc, PDC_PTCR, PDC_PTCR_RXTDIS | PDC_PTCR_TXTDIS);

	mr = RD4(sc,MCI_MR) & ~MCI_MR_BLKLEN;
	mr |=  min(data->len, 512) << 16;
	WR4(sc, MCI_MR, mr | MCI_MR_PDCMODE|MCI_MR_PDCPADV);

	/*
	 * Set up DMA.
	 *
	 * Use bounce buffers even if we don't need to byteswap, because doing
	 * multi-block IO with large DMA buffers is way fast (compared to
	 * single-block IO), even after incurring the overhead of also copying
	 * from/to the caller's buffers (which may be in non-contiguous physical
	 * pages).
	 *
	 * In an ideal non-byteswap world we could create a dma tag that allows
	 * for discontiguous segments and do the IO directly from/to the
	 * caller's buffer(s), using ENDRX/ENDTX interrupts to chain the
	 * discontiguous buffers through the PDC. Someday.
	 *
	 * If a read is bigger than 2k, split it in half so that we can start
	 * byte-swapping the first half while the second half is on the wire.
	 * It would be best if we could split it into 8k chunks, but we can't
	 * always keep up with the byte-swapping due to other system activity,
	 * and if an RXBUFF interrupt happens while we're still handling the
	 * byte-swap from the prior buffer (IE, we haven't returned from
	 * handling the prior interrupt yet), then data will get dropped on the
	 * floor and we can't easily recover from that.  The right fix for that
	 * would be to have the interrupt handling only keep the DMA flowing and
	 * enqueue filled buffers to be byte-swapped in a non-interrupt context.
	 * Even that won't work on the write side of things though; in that
	 * context we have to have all the data ready to go before starting the
	 * dma.
	 *
	 * XXX what about stream transfers?
	 */
	sc->xfer_offset = 0;
	sc->bbuf_curidx = 0;

	if (data->flags & (MMC_DATA_READ | MMC_DATA_WRITE)) {
		uint32_t len;
		uint32_t remaining = data->len;
		bus_addr_t paddr;
		int err;

		if (remaining > (BBCOUNT*BBSIZE))
			panic("IO read size exceeds MAXDATA\n");

		if (data->flags & MMC_DATA_READ) {
			if (remaining > 2048) // XXX
				len = remaining / 2;
			else
				len = remaining;
			err = bus_dmamap_load(sc->dmatag, sc->bbuf_map[0],
			    sc->bbuf_vaddr[0], len, at91_mci_getaddr,
			    &paddr, BUS_DMA_NOWAIT);
			if (err != 0)
				panic("IO read dmamap_load failed\n");
			bus_dmamap_sync(sc->dmatag, sc->bbuf_map[0],
			    BUS_DMASYNC_PREREAD);
			WR4(sc, PDC_RPR, paddr);
			WR4(sc, PDC_RCR, len / 4);
			sc->bbuf_len[0] = len;
			remaining -= len;
			if (remaining == 0) {
				sc->bbuf_len[1] = 0;
			} else {
				len = remaining;
				err = bus_dmamap_load(sc->dmatag, sc->bbuf_map[1],
				    sc->bbuf_vaddr[1], len, at91_mci_getaddr,
				    &paddr, BUS_DMA_NOWAIT);
				if (err != 0)
					panic("IO read dmamap_load failed\n");
				bus_dmamap_sync(sc->dmatag, sc->bbuf_map[1],
				    BUS_DMASYNC_PREREAD);
				WR4(sc, PDC_RNPR, paddr);
				WR4(sc, PDC_RNCR, len / 4);
				sc->bbuf_len[1] = len;
				remaining -= len;
			}
			WR4(sc, PDC_PTCR, PDC_PTCR_RXTEN);
		} else {
			len = min(BBSIZE, remaining);
			at91_bswap_buf(sc, sc->bbuf_vaddr[0], data->data, len);
			err = bus_dmamap_load(sc->dmatag, sc->bbuf_map[0],
			    sc->bbuf_vaddr[0], len, at91_mci_getaddr,
			    &paddr, BUS_DMA_NOWAIT);
			if (err != 0)
				panic("IO write dmamap_load failed\n");
			bus_dmamap_sync(sc->dmatag, sc->bbuf_map[0],
			    BUS_DMASYNC_PREWRITE);
			/*
			 * Erratum workaround:  PDC transfer length on a write
			 * must not be smaller than 12 bytes (3 words); only
			 * blklen bytes (set above) are actually transferred.
			 */
			WR4(sc, PDC_TPR,paddr);
			WR4(sc, PDC_TCR, (len < 12) ? 3 : len / 4);
			sc->bbuf_len[0] = len;
			remaining -= len;
			if (remaining == 0) {
				sc->bbuf_len[1] = 0;
			} else {
				len = remaining;
				at91_bswap_buf(sc, sc->bbuf_vaddr[1],
				    ((char *)data->data)+BBSIZE, len);
				err = bus_dmamap_load(sc->dmatag, sc->bbuf_map[1],
				    sc->bbuf_vaddr[1], len, at91_mci_getaddr,
				    &paddr, BUS_DMA_NOWAIT);
				if (err != 0)
					panic("IO write dmamap_load failed\n");
				bus_dmamap_sync(sc->dmatag, sc->bbuf_map[1],
				    BUS_DMASYNC_PREWRITE);
				WR4(sc, PDC_TNPR, paddr);
				WR4(sc, PDC_TNCR, (len < 12) ? 3 : len / 4);
				sc->bbuf_len[1] = len;
				remaining -= len;
			}
			/* do not enable PDC xfer until CMDRDY asserted */
		}
		data->xfer_len = 0; /* XXX what's this? appears to be unused. */
	}

	if (mci_debug)
		printf("CMDR %x (opcode %d) ARGR %x with data len %d\n",
		       cmdr, cmd->opcode, cmd->arg, cmd->data->len);

	WR4(sc, MCI_ARGR, cmd->arg);
	WR4(sc, MCI_CMDR, cmdr);
	WR4(sc, MCI_IER, MCI_SR_ERROR | MCI_SR_CMDRDY);
}

static void
at91_mci_next_operation(struct at91_mci_softc *sc)
{
	struct mmc_request *req;

	req = sc->req;
	if (req == NULL)
		return;

	if (sc->flags & PENDING_CMD) {
		sc->flags &= ~PENDING_CMD;
		at91_mci_start_cmd(sc, req->cmd);
		return;
	} else if (sc->flags & PENDING_STOP) {
		sc->flags &= ~PENDING_STOP;
		at91_mci_start_cmd(sc, req->stop);
		return;
	}

	WR4(sc, MCI_IDR, 0xffffffff);
	sc->req = NULL;
	sc->curcmd = NULL;
	//printf("req done\n");
	req->done(req);
}

static int
at91_mci_request(device_t brdev, device_t reqdev, struct mmc_request *req)
{
	struct at91_mci_softc *sc = device_get_softc(brdev);

	AT91_MCI_LOCK(sc);
	if (sc->req != NULL) {
		AT91_MCI_UNLOCK(sc);
		return (EBUSY);
	}
	//printf("new req\n");
	sc->req = req;
	sc->flags = PENDING_CMD;
	if (sc->req->stop)
		sc->flags |= PENDING_STOP;
	at91_mci_next_operation(sc);
	AT91_MCI_UNLOCK(sc);
	return (0);
}

static int
at91_mci_get_ro(device_t brdev, device_t reqdev)
{
	return (0);
}

static int
at91_mci_acquire_host(device_t brdev, device_t reqdev)
{
	struct at91_mci_softc *sc = device_get_softc(brdev);
	int err = 0;

	AT91_MCI_LOCK(sc);
	while (sc->bus_busy)
		msleep(sc, &sc->sc_mtx, PZERO, "mciah", hz / 5);
	sc->bus_busy++;
	AT91_MCI_UNLOCK(sc);
	return (err);
}

static int
at91_mci_release_host(device_t brdev, device_t reqdev)
{
	struct at91_mci_softc *sc = device_get_softc(brdev);

	AT91_MCI_LOCK(sc);
	sc->bus_busy--;
	wakeup(sc);
	AT91_MCI_UNLOCK(sc);
	return (0);
}

static void
at91_mci_read_done(struct at91_mci_softc *sc, uint32_t sr)
{
	struct mmc_command *cmd = sc->curcmd;
	char * dataptr = (char *)cmd->data->data;
	uint32_t curidx = sc->bbuf_curidx;
	uint32_t len = sc->bbuf_len[curidx];

	/*
	 * We arrive here when a DMA transfer for a read is done, whether it's
	 * a single or multi-block read.
	 *
	 * We byte-swap the buffer that just completed, and if that is the
	 * last buffer that's part of this read then we move on to the next
	 * operation, otherwise we wait for another ENDRX for the next bufer.
	 */

	bus_dmamap_sync(sc->dmatag, sc->bbuf_map[curidx], BUS_DMASYNC_POSTREAD);
	bus_dmamap_unload(sc->dmatag, sc->bbuf_map[curidx]);

	at91_bswap_buf(sc, dataptr + sc->xfer_offset, sc->bbuf_vaddr[curidx], len);

	if (mci_debug) {
		printf("read done sr %x curidx %d len %d xfer_offset %d\n",
		       sr, curidx, len, sc->xfer_offset);
	}

	sc->xfer_offset += len;
	sc->bbuf_curidx = !curidx; /* swap buffers */

	/*
	 * If we've transferred all the data, move on to the next operation.
	 *
	 * If we're still transferring the last buffer, RNCR is already zero but
	 * we have to write a zero anyway to clear the ENDRX status so we don't
	 * re-interrupt until the last buffer is done.
	 */
	if (sc->xfer_offset == cmd->data->len) {
		WR4(sc, PDC_PTCR, PDC_PTCR_RXTDIS | PDC_PTCR_TXTDIS);
		cmd->error = MMC_ERR_NONE;
		at91_mci_next_operation(sc);
	} else {
		WR4(sc, PDC_RNCR, 0);
		WR4(sc, MCI_IER, MCI_SR_ERROR | MCI_SR_ENDRX);
	}
}

static void
at91_mci_write_done(struct at91_mci_softc *sc, uint32_t sr)
{
	struct mmc_command *cmd = sc->curcmd;

	/*
	 * We arrive here when the entire DMA transfer for a write is done,
	 * whether it's a single or multi-block write.  If it's multi-block we
	 * have to immediately move on to the next operation which is to send
	 * the stop command.  If it's a single-block transfer we need to wait
	 * for NOTBUSY, but if that's already asserted we can avoid another
	 * interrupt and just move on to completing the request right away.
	 */

	WR4(sc, PDC_PTCR, PDC_PTCR_RXTDIS | PDC_PTCR_TXTDIS);

	bus_dmamap_sync(sc->dmatag, sc->bbuf_map[sc->bbuf_curidx],
	    BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(sc->dmatag, sc->bbuf_map[sc->bbuf_curidx]);

	if ((cmd->data->flags & MMC_DATA_MULTI) || (sr & MCI_SR_NOTBUSY)) {
		cmd->error = MMC_ERR_NONE;
		at91_mci_next_operation(sc);
	} else {
		WR4(sc, MCI_IER, MCI_SR_ERROR | MCI_SR_NOTBUSY);
	}
}

static void
at91_mci_notbusy(struct at91_mci_softc *sc)
{
	struct mmc_command *cmd = sc->curcmd;

	/*
	 * We arrive here by either completion of a single-block write, or
	 * completion of the stop command that ended a multi-block write (and,
	 * I suppose, after a card-select or erase, but I haven't tested
	 * those).  Anyway, we're done and it's time to move on to the next
	 * command.
	 */

	cmd->error = MMC_ERR_NONE;
	at91_mci_next_operation(sc);
}

static void
at91_mci_stop_done(struct at91_mci_softc *sc, uint32_t sr)
{
	struct mmc_command *cmd = sc->curcmd;

	/*
	 * We arrive here after receiving CMDRDY for a MMC_STOP_TRANSMISSION
	 * command.  Depending on the operation being stopped, we may have to
	 * do some unusual things to work around hardware bugs.
	 */

	/*
	 * This is known to be true of at91rm9200 hardware; it may or may not
	 * apply to more recent chips:
	 *
	 * After stopping a multi-block write, the NOTBUSY bit in MCI_SR does
	 * not properly reflect the actual busy state of the card as signaled
	 * on the DAT0 line; it always claims the card is not-busy.  If we
	 * believe that and let operations continue, following commands will
	 * fail with response timeouts (except of course MMC_SEND_STATUS -- it
	 * indicates the card is busy in the PRG state, which was the smoking
	 * gun that showed MCI_SR NOTBUSY was not tracking DAT0 correctly).
	 *
	 * The atmel docs are emphatic: "This flag [NOTBUSY] must be used only
	 * for Write Operations."  I guess technically since we sent a stop
	 * it's not a write operation anymore.  But then just what did they
	 * think it meant for the stop command to have "...an optional busy
	 * signal transmitted on the data line" according to the SD spec?
	 *
	 * I tried a variety of things to un-wedge the MCI and get the status
	 * register to reflect NOTBUSY correctly again, but the only thing
	 * that worked was a full device reset.  It feels like an awfully big
	 * hammer, but doing a full reset after every multiblock write is
	 * still faster than doing single-block IO (by almost two orders of
	 * magnitude: 20KB/sec improves to about 1.8MB/sec best case).
	 *
	 * After doing the reset, wait for a NOTBUSY interrupt before
	 * continuing with the next operation.
	 *
	 * This workaround breaks multiwrite on the rev2xx parts, but some other
	 * workaround is needed.
	 */
	if ((sc->flags & CMD_MULTIWRITE) && (sc->sc_cap & CAP_NEEDS_BYTESWAP)) {
		at91_mci_reset(sc);
		WR4(sc, MCI_IER, MCI_SR_ERROR | MCI_SR_NOTBUSY);
		return;
	}

	/*
	 * This is known to be true of at91rm9200 hardware; it may or may not
	 * apply to more recent chips:
	 *
	 * After stopping a multi-block read, loop to read and discard any
	 * data that coasts in after we sent the stop command.  The docs don't
	 * say anything about it, but empirical testing shows that 1-3
	 * additional words of data get buffered up in some unmentioned
	 * internal fifo and if we don't read and discard them here they end
	 * up on the front of the next read DMA transfer we do.
	 *
	 * This appears to be unnecessary for rev2xx parts.
	 */
	if ((sc->flags & CMD_MULTIREAD) && (sc->sc_cap & CAP_NEEDS_BYTESWAP)) {
		uint32_t sr;
		int count = 0;

		do {
			sr = RD4(sc, MCI_SR);
			if (sr & MCI_SR_RXRDY) {
				RD4(sc,  MCI_RDR);
				++count;
			}
		} while (sr & MCI_SR_RXRDY);
		at91_mci_reset(sc);
	}

	cmd->error = MMC_ERR_NONE;
	at91_mci_next_operation(sc);

}

static void
at91_mci_cmdrdy(struct at91_mci_softc *sc, uint32_t sr)
{
	struct mmc_command *cmd = sc->curcmd;
	int i;

	if (cmd == NULL)
		return;

	/*
	 * We get here at the end of EVERY command.  We retrieve the command
	 * response (if any) then decide what to do next based on the command.
	 */

	if (cmd->flags & MMC_RSP_PRESENT) {
		for (i = 0; i < ((cmd->flags & MMC_RSP_136) ? 4 : 1); i++) {
			cmd->resp[i] = RD4(sc, MCI_RSPR + i * 4);
			if (mci_debug)
				printf("RSPR[%d] = %x sr=%x\n", i, cmd->resp[i],  sr);
		}
	}

	/*
	 * If this was a stop command, go handle the various special
	 * conditions (read: bugs) that have to be dealt with following a stop.
	 */
	if (cmd->opcode == MMC_STOP_TRANSMISSION) {
		at91_mci_stop_done(sc, sr);
		return;
	}

	/*
	 * If this command can continue to assert BUSY beyond the response then
	 * we need to wait for NOTBUSY before the command is really done.
	 *
	 * Note that this may not work properly on the at91rm9200.  It certainly
	 * doesn't work for the STOP command that follows a multi-block write,
	 * so post-stop CMDRDY is handled separately; see the special handling
	 * in at91_mci_stop_done().
	 *
	 * Beside STOP, there are other R1B-type commands that use the busy
	 * signal after CMDRDY: CMD7 (card select), CMD28-29 (write protect),
	 * CMD38 (erase). I haven't tested any of them, but I rather expect
	 * them all to have the same sort of problem with MCI_SR not actually
	 * reflecting the state of the DAT0-line busy indicator.  So this code
	 * may need to grow some sort of special handling for them too. (This
	 * just in: CMD7 isn't a problem right now because dev/mmc.c incorrectly
	 * sets the response flags to R1 rather than R1B.) XXX
	 */
	if ((cmd->flags & MMC_RSP_BUSY)) {
		WR4(sc, MCI_IER, MCI_SR_ERROR | MCI_SR_NOTBUSY);
		return;
	}

	/*
	 * If there is a data transfer with this command, then...
	 * - If it's a read, we need to wait for ENDRX.
	 * - If it's a write, now is the time to enable the PDC, and we need
	 *   to wait for a BLKE that follows a TXBUFE, because if we're doing
	 *   a split transfer we get a BLKE after the first half (when TPR/TCR
	 *   get loaded from TNPR/TNCR).  So first we wait for the TXBUFE, and
	 *   the handling for that interrupt will then invoke the wait for the
	 *   subsequent BLKE which indicates actual completion.
	 */
	if (cmd->data) {
		uint32_t ier;
		if (cmd->data->flags & MMC_DATA_READ) {
			ier = MCI_SR_ENDRX;
		} else {
			ier = MCI_SR_TXBUFE;
			WR4(sc, PDC_PTCR, PDC_PTCR_TXTEN);
		}
		WR4(sc, MCI_IER, MCI_SR_ERROR | ier);
		return;
	}

	/*
	 * If we made it to here, we don't need to wait for anything more for
	 * the current command, move on to the next command (will complete the
	 * request if there is no next command).
	 */
	cmd->error = MMC_ERR_NONE;
	at91_mci_next_operation(sc);
}

static void
at91_mci_intr(void *arg)
{
	struct at91_mci_softc *sc = (struct at91_mci_softc*)arg;
	struct mmc_command *cmd = sc->curcmd;
	uint32_t sr, isr;

	AT91_MCI_LOCK(sc);

	sr = RD4(sc, MCI_SR);
	isr = sr & RD4(sc, MCI_IMR);

	if (mci_debug)
		printf("i 0x%x sr 0x%x\n", isr, sr);

	/*
	 * All interrupts are one-shot; disable it now.
	 * The next operation will re-enable whatever interrupts it wants.
	 */
	WR4(sc, MCI_IDR, isr);
	if (isr & MCI_SR_ERROR) {
		if (isr & (MCI_SR_RTOE | MCI_SR_DTOE))
			cmd->error = MMC_ERR_TIMEOUT;
		else if (isr & (MCI_SR_RCRCE | MCI_SR_DCRCE))
			cmd->error = MMC_ERR_BADCRC;
		else if (isr & (MCI_SR_OVRE | MCI_SR_UNRE))
			cmd->error = MMC_ERR_FIFO;
		else
			cmd->error = MMC_ERR_FAILED;
		/*
		 * CMD8 is used to probe for SDHC cards, a standard SD card
		 * will get a response timeout; don't report it because it's a
		 * normal and expected condition.  One might argue that all
		 * error reporting should be left to higher levels, but when
		 * they report at all it's always EIO, which isn't very
		 * helpful. XXX bootverbose?
		 */
		if (cmd->opcode != 8) {
			device_printf(sc->dev,
			    "IO error; status MCI_SR = 0x%b cmd opcode = %d%s\n",
			    sr, MCI_SR_BITSTRING, cmd->opcode,
			    (cmd->opcode != 12) ? "" :
			    (sc->flags & CMD_MULTIREAD) ? " after read" : " after write");
			/* XXX not sure RTOE needs a full reset, just a retry */
			at91_mci_reset(sc);
		}
		at91_mci_next_operation(sc);
	} else {
		if (isr & MCI_SR_TXBUFE) {
//			printf("TXBUFE\n");
			/*
			 * We need to wait for a BLKE that follows TXBUFE
			 * (intermediate BLKEs might happen after ENDTXes if
			 * we're chaining multiple buffers).  If BLKE is also
			 * asserted at the time we get TXBUFE, we can avoid
			 * another interrupt and process it right away, below.
			 */
			if (sr & MCI_SR_BLKE)
				isr |= MCI_SR_BLKE;
			else
				WR4(sc, MCI_IER, MCI_SR_BLKE);
		}
		if (isr & MCI_SR_RXBUFF) {
//			printf("RXBUFF\n");
		}
		if (isr & MCI_SR_ENDTX) {
//			printf("ENDTX\n");
		}
		if (isr & MCI_SR_ENDRX) {
//			printf("ENDRX\n");
			at91_mci_read_done(sc, sr);
		}
		if (isr & MCI_SR_NOTBUSY) {
//			printf("NOTBUSY\n");
			at91_mci_notbusy(sc);
		}
		if (isr & MCI_SR_DTIP) {
//			printf("Data transfer in progress\n");
		}
		if (isr & MCI_SR_BLKE) {
//			printf("Block transfer end\n");
			at91_mci_write_done(sc, sr);
		}
		if (isr & MCI_SR_TXRDY) {
//			printf("Ready to transmit\n");
		}
		if (isr & MCI_SR_RXRDY) {
//			printf("Ready to receive\n");
		}
		if (isr & MCI_SR_CMDRDY) {
//			printf("Command ready\n");
			at91_mci_cmdrdy(sc, sr);
		}
	}
	AT91_MCI_UNLOCK(sc);
}

static int
at91_mci_read_ivar(device_t bus, device_t child, int which, uintptr_t *result)
{
	struct at91_mci_softc *sc = device_get_softc(bus);

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
	case MMCBR_IVAR_CAPS:
		if (sc->has_4wire) {
			sc->sc_cap |= CAP_HAS_4WIRE;
			sc->host.caps |= MMC_CAP_4_BIT_DATA;
		} else {
			sc->sc_cap &= ~CAP_HAS_4WIRE;
			sc->host.caps &= ~MMC_CAP_4_BIT_DATA;
		}
		*(int *)result = sc->host.caps;
		break;
	case MMCBR_IVAR_MAX_DATA:
		/*
		 * Something is wrong with the 2x parts and multiblock, so
		 * just do 1 block at a time for now, which really kills
		 * performance.
		 */
		if (sc->sc_cap & CAP_MCI1_REV2XX)
			*(int *)result = 1;
		else
			*(int *)result = MAX_BLOCKS;
		break;
	}
	return (0);
}

static int
at91_mci_write_ivar(device_t bus, device_t child, int which, uintptr_t value)
{
	struct at91_mci_softc *sc = device_get_softc(bus);

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

static device_method_t at91_mci_methods[] = {
	/* device_if */
	DEVMETHOD(device_probe, at91_mci_probe),
	DEVMETHOD(device_attach, at91_mci_attach),
	DEVMETHOD(device_detach, at91_mci_detach),

	/* Bus interface */
	DEVMETHOD(bus_read_ivar,	at91_mci_read_ivar),
	DEVMETHOD(bus_write_ivar,	at91_mci_write_ivar),

	/* mmcbr_if */
	DEVMETHOD(mmcbr_update_ios, at91_mci_update_ios),
	DEVMETHOD(mmcbr_request, at91_mci_request),
	DEVMETHOD(mmcbr_get_ro, at91_mci_get_ro),
	DEVMETHOD(mmcbr_acquire_host, at91_mci_acquire_host),
	DEVMETHOD(mmcbr_release_host, at91_mci_release_host),

	DEVMETHOD_END
};

static driver_t at91_mci_driver = {
	"at91_mci",
	at91_mci_methods,
	sizeof(struct at91_mci_softc),
};

static devclass_t at91_mci_devclass;

#ifdef FDT
DRIVER_MODULE(at91_mci, simplebus, at91_mci_driver, at91_mci_devclass, NULL,
    NULL);
#else
DRIVER_MODULE(at91_mci, atmelarm, at91_mci_driver, at91_mci_devclass, NULL,
    NULL);
#endif
DRIVER_MODULE(mmc, at91_mci, mmc_driver, mmc_devclass, NULL, NULL);
MODULE_DEPEND(at91_mci, mmc, 1, 1, 1);
