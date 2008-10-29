/*-
 * Copyright (c) 2006 Bernd Walter.  All rights reserved.
 * Copyright (c) 2006 M. Warner Losh.  All rights reserved.
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
#include <sys/time.h>
#include <sys/timetc.h>
#include <sys/watchdog.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/resource.h>
#include <machine/frame.h>
#include <machine/intr.h>
#include <arm/at91/at91rm92reg.h>
#include <arm/at91/at91var.h>
#include <arm/at91/at91_mcireg.h>
#include <arm/at91/at91_pdcreg.h>
#include <dev/mmc/bridge.h>
#include <dev/mmc/mmcreg.h>
#include <dev/mmc/mmcbrvar.h>

#include "mmcbr_if.h"

#define BBSZ	512

struct at91_mci_softc {
	void *intrhand;			/* Interrupt handle */
	device_t dev;
	int flags;
#define CMD_STARTED	1
#define STOP_STARTED	2
	struct resource *irq_res;	/* IRQ resource */
	struct resource	*mem_res;	/* Memory resource */
	struct mtx sc_mtx;
	bus_dma_tag_t dmatag;
	bus_dmamap_t map;
	int mapped;
	struct mmc_host host;
	int wire4;
	int bus_busy;
	struct mmc_request *req;
	struct mmc_command *curcmd;
	char bounce_buffer[BBSZ];
};

static inline uint32_t
RD4(struct at91_mci_softc *sc, bus_size_t off)
{
	return bus_read_4(sc->mem_res, off);
}

static inline void
WR4(struct at91_mci_softc *sc, bus_size_t off, uint32_t val)
{
	bus_write_4(sc->mem_res, off, val);
}

/* bus entry points */
static int at91_mci_probe(device_t dev);
static int at91_mci_attach(device_t dev);
static int at91_mci_detach(device_t dev);
static void at91_mci_intr(void *);

/* helper routines */
static int at91_mci_activate(device_t dev);
static void at91_mci_deactivate(device_t dev);

#define AT91_MCI_LOCK(_sc)		mtx_lock(&(_sc)->sc_mtx)
#define	AT91_MCI_UNLOCK(_sc)		mtx_unlock(&(_sc)->sc_mtx)
#define AT91_MCI_LOCK_INIT(_sc) \
	mtx_init(&_sc->sc_mtx, device_get_nameunit(_sc->dev), \
	    "mci", MTX_DEF)
#define AT91_MCI_LOCK_DESTROY(_sc)	mtx_destroy(&_sc->sc_mtx);
#define AT91_MCI_ASSERT_LOCKED(_sc)	mtx_assert(&_sc->sc_mtx, MA_OWNED);
#define AT91_MCI_ASSERT_UNLOCKED(_sc) mtx_assert(&_sc->sc_mtx, MA_NOTOWNED);

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

static void
at91_mci_init(device_t dev)
{
	struct at91_mci_softc *sc = device_get_softc(dev);

	WR4(sc, MCI_CR, MCI_CR_MCIEN);		/* Enable controller */
	WR4(sc, MCI_IDR, 0xffffffff);		/* Turn off interrupts */
	WR4(sc, MCI_DTOR, MCI_DTOR_DTOMUL_1M | 1);
	WR4(sc, MCI_MR, 0x834a);	// XXX GROSS HACK FROM LINUX
	WR4(sc, MCI_SDCR, 0);			/* SLOT A, 1 bit bus */
}

static void
at91_mci_fini(device_t dev)
{
	struct at91_mci_softc *sc = device_get_softc(dev);

	WR4(sc, MCI_IDR, 0xffffffff);		/* Turn off interrupts */
	at91_mci_pdc_disable(sc);
	WR4(sc, MCI_CR, MCI_CR_MCIDIS | MCI_CR_SWRST); /* Put the device into reset */
}

static int
at91_mci_probe(device_t dev)
{

	device_set_desc(dev, "MCI mmc/sd host bridge");
	return (0);
}

static int
at91_mci_attach(device_t dev)
{
	struct at91_mci_softc *sc = device_get_softc(dev);
	int err;
	device_t child;

	sc->dev = dev;
	err = at91_mci_activate(dev);
	if (err)
		goto out;

	AT91_MCI_LOCK_INIT(sc);

	/*
	 * Allocate DMA tags and maps
	 */
	err = bus_dma_tag_create(bus_get_dma_tag(dev), 1, 0,
	    BUS_SPACE_MAXADDR_32BIT, US_SPACE_MAXADDR, NULL, NULL, MAXPHYS, 1,
	    MAXPHYS, BUS_DMA_ALLOCNOW, NULL, NULL, &sc->dmatag);
	if (err != 0)
		goto out;

	err = bus_dmamap_create(sc->dmatag, 0,  &sc->map);
	if (err != 0)
		goto out;

	at91_mci_fini(dev);
	at91_mci_init(dev);

	/*
	 * Activate the interrupt
	 */
	err = bus_setup_intr(dev, sc->irq_res, INTR_TYPE_MISC | INTR_MPSAFE,
	    NULL, at91_mci_intr, sc, &sc->intrhand);
	if (err) {
		AT91_MCI_LOCK_DESTROY(sc);
		goto out;
	}
	sc->host.f_min = 375000;
	sc->host.f_max = 30000000;
	sc->host.host_ocr = MMC_OCR_320_330 | MMC_OCR_330_340;
	sc->host.caps = MMC_CAP_4_BIT_DATA;
	child = device_add_child(dev, "mmc", 0);
	device_set_ivars(dev, &sc->host);
	err = bus_generic_attach(dev);
out:;
	if (err)
		at91_mci_deactivate(dev);
	return (err);
}

static int
at91_mci_detach(device_t dev)
{
	at91_mci_fini(dev);
	at91_mci_deactivate(dev);
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
	sc->intrhand = 0;
	bus_generic_detach(sc->dev);
	if (sc->mem_res)
		bus_release_resource(dev, SYS_RES_IOPORT,
		    rman_get_rid(sc->mem_res), sc->mem_res);
	sc->mem_res = 0;
	if (sc->irq_res)
		bus_release_resource(dev, SYS_RES_IRQ,
		    rman_get_rid(sc->irq_res), sc->irq_res);
	sc->irq_res = 0;
	return;
}

static void
at91_mci_getaddr(void *arg, bus_dma_segment_t *segs, int nsegs, int error)
{
	if (error != 0)
		return;
	*(bus_addr_t *)arg = segs[0].ds_addr;
}

static int
at91_mci_update_ios(device_t brdev, device_t reqdev)
{
	uint32_t at91_master_clock = AT91C_MASTER_CLOCK;
	struct at91_mci_softc *sc;
	struct mmc_host *host;
	struct mmc_ios *ios;
	uint32_t clkdiv;

	sc = device_get_softc(brdev);
	host = &sc->host;
	ios = &host->ios;
	// bus mode?
	if (ios->clock == 0) {
		WR4(sc, MCI_CR, MCI_CR_MCIDIS);
		clkdiv = 0;
	} else {
		WR4(sc, MCI_CR, MCI_CR_MCIEN);
		if ((at91_master_clock % (ios->clock * 2)) == 0)
			clkdiv = ((at91_master_clock / ios->clock) / 2) - 1;
		else
			clkdiv = (at91_master_clock / ios->clock) / 2;
	}
	if (ios->bus_width == bus_width_4 && sc->wire4)
		WR4(sc, MCI_SDCR, RD4(sc, MCI_SDCR) | MCI_SDCR_SDCBUS);
	else
		WR4(sc, MCI_SDCR, RD4(sc, MCI_SDCR) & ~MCI_SDCR_SDCBUS);
	WR4(sc, MCI_MR, (RD4(sc, MCI_MR) & ~MCI_MR_CLKDIV) | clkdiv);
	/* XXX We need to turn the device on/off here with a GPIO pin */
	return (0);
}

static void
at91_mci_start_cmd(struct at91_mci_softc *sc, struct mmc_command *cmd)
{
	uint32_t cmdr, ier = 0, mr;
	uint32_t *src, *dst;
	int i;
	struct mmc_data *data;
	struct mmc_request *req;
	size_t block_size = 1 << 9;	// Fixed, per mmc/sd spec for 2GB cards
	void *vaddr;
	bus_addr_t paddr;

	sc->curcmd = cmd;
	data = cmd->data;
	cmdr = cmd->opcode;
	req = cmd->mrq;
	if (MMC_RSP(cmd->flags) == MMC_RSP_NONE)
		cmdr |= MCI_CMDR_RSPTYP_NO;
	else {
		/* Allow big timeout for responses */
		cmdr |= MCI_CMDR_MAXLAT;
		if (cmd->flags & MMC_RSP_136)
			cmdr |= MCI_CMDR_RSPTYP_136;
		else
			cmdr |= MCI_CMDR_RSPTYP_48;
	}
	if (cmd->opcode == MMC_STOP_TRANSMISSION)
		cmdr |= MCI_CMDR_TRCMD_STOP;
	if (sc->host.ios.bus_mode == opendrain)
		cmdr |= MCI_CMDR_OPDCMD;
	if (!data) {
		// The no data case is fairly simple
		at91_mci_pdc_disable(sc);
//		printf("CMDR %x ARGR %x\n", cmdr, cmd->arg);
		WR4(sc, MCI_ARGR, cmd->arg);
		WR4(sc, MCI_CMDR, cmdr);
		WR4(sc, MCI_IER, MCI_SR_ERROR | MCI_SR_CMDRDY);
		return;
	}
	if (data->flags & MMC_DATA_READ)
		cmdr |= MCI_CMDR_TRDIR;
	if (data->flags & (MMC_DATA_READ | MMC_DATA_WRITE))
		cmdr |= MCI_CMDR_TRCMD_START;
	if (data->flags & MMC_DATA_STREAM)
		cmdr |= MCI_CMDR_TRTYP_STREAM;
	if (data->flags & MMC_DATA_MULTI)
		cmdr |= MCI_CMDR_TRTYP_MULTIPLE;
	// Set block size and turn on PDC mode for dma xfer and disable
	// PDC until we're ready.
	mr = RD4(sc, MCI_MR) & ~MCI_MR_BLKLEN;
	WR4(sc, MCI_MR, mr | (block_size << 16) | MCI_MR_PDCMODE);
	WR4(sc, PDC_PTCR, PDC_PTCR_RXTDIS | PDC_PTCR_TXTDIS);
	if (cmdr & MCI_CMDR_TRCMD_START) {
		if (cmdr & MCI_CMDR_TRDIR)
			vaddr = cmd->data->data;
		else {
			if (data->len != BBSZ)
				panic("Write multiblock write support");
			vaddr = sc->bounce_buffer;
			src = (uint32_t *)cmd->data->data;
			dst = (uint32_t *)vaddr;
			for (i = 0; i < data->len / 4; i++)
				dst[i] = bswap32(src[i]);
		}
		data->xfer_len = 0;
		if (bus_dmamap_load(sc->dmatag, sc->map, vaddr, data->len,
		    at91_mci_getaddr, &paddr, 0) != 0) {
			if (req->cmd->flags & STOP_STARTED)
				req->stop->error = MMC_ERR_NO_MEMORY;
			else
				req->cmd->error = MMC_ERR_NO_MEMORY;
			sc->req = NULL;
			sc->curcmd = NULL;
			req->done(req);
			return;
		}
		sc->mapped++;
		if (cmdr & MCI_CMDR_TRDIR) {
			bus_dmamap_sync(sc->dmatag, sc->map, BUS_DMASYNC_PREREAD);
			WR4(sc, PDC_RPR, paddr);
			WR4(sc, PDC_RCR, data->len / 4);
			ier = MCI_SR_ENDRX;
		} else {
			bus_dmamap_sync(sc->dmatag, sc->map, BUS_DMASYNC_PREWRITE);
			WR4(sc, PDC_TPR, paddr);
			WR4(sc, PDC_TCR, data->len / 4);
			ier = MCI_SR_TXBUFE;
		}
	}
//	printf("CMDR %x ARGR %x with data\n", cmdr, cmd->arg);
	WR4(sc, MCI_ARGR, cmd->arg);
	if (cmdr & MCI_CMDR_TRCMD_START) {
		if (cmdr & MCI_CMDR_TRDIR) {
			WR4(sc, PDC_PTCR, PDC_PTCR_RXTEN);
			WR4(sc, MCI_CMDR, cmdr);
		} else {
			WR4(sc, MCI_CMDR, cmdr);
			WR4(sc, PDC_PTCR, PDC_PTCR_TXTEN);
		}
	}
	WR4(sc, MCI_IER, MCI_SR_ERROR | ier);
}

static void
at91_mci_start(struct at91_mci_softc *sc)
{
	struct mmc_request *req;

	req = sc->req;
	if (req == NULL)
		return;
	// assert locked
	if (!(sc->flags & CMD_STARTED)) {
		sc->flags |= CMD_STARTED;
//		printf("Starting CMD\n");
		at91_mci_start_cmd(sc, req->cmd);
		return;
	}
	if (!(sc->flags & STOP_STARTED) && req->stop) {
//		printf("Starting Stop\n");
		sc->flags |= STOP_STARTED;
		at91_mci_start_cmd(sc, req->stop);
		return;
	}
	/* We must be done -- bad idea to do this while locked? */
	sc->req = NULL;
	sc->curcmd = NULL;
	req->done(req);
}

static int
at91_mci_request(device_t brdev, device_t reqdev, struct mmc_request *req)
{
	struct at91_mci_softc *sc = device_get_softc(brdev);

	AT91_MCI_LOCK(sc);
	// XXX do we want to be able to queue up multiple commands?
	// XXX sounds like a good idea, but all protocols are sync, so
	// XXX maybe the idea is naive...
	if (sc->req != NULL) {
		AT91_MCI_UNLOCK(sc);
		return EBUSY;
	}
	sc->req = req;
	sc->flags = 0;
	at91_mci_start(sc);
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
at91_mci_read_done(struct at91_mci_softc *sc)
{
	uint32_t *walker;
	struct mmc_command *cmd;
	int i, len;

	cmd = sc->curcmd;
	bus_dmamap_sync(sc->dmatag, sc->map, BUS_DMASYNC_POSTREAD);
	bus_dmamap_unload(sc->dmatag, sc->map);
	sc->mapped--;
	walker = (uint32_t *)cmd->data->data;
	len = cmd->data->len / 4;
	for (i = 0; i < len; i++)
		walker[i] = bswap32(walker[i]);
	// Finish up the sequence...
	WR4(sc, MCI_IDR, MCI_SR_ENDRX);
	WR4(sc, MCI_IER, MCI_SR_RXBUFF);
	WR4(sc, PDC_PTCR, PDC_PTCR_RXTDIS | PDC_PTCR_TXTDIS);
}

static void
at91_mci_xmit_done(struct at91_mci_softc *sc)
{
	// Finish up the sequence...
	WR4(sc, PDC_PTCR, PDC_PTCR_RXTDIS | PDC_PTCR_TXTDIS);
	WR4(sc, MCI_IDR, MCI_SR_TXBUFE);
	WR4(sc, MCI_IER, MCI_SR_NOTBUSY);
	bus_dmamap_sync(sc->dmatag, sc->map, BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(sc->dmatag, sc->map);
	sc->mapped--;
}

static void
at91_mci_intr(void *arg)
{
	struct at91_mci_softc *sc = (struct at91_mci_softc*)arg;
	uint32_t sr;
	int i, done = 0;
	struct mmc_command *cmd;

	AT91_MCI_LOCK(sc);
	sr = RD4(sc, MCI_SR) & RD4(sc, MCI_IMR);
//	printf("i 0x%x\n", sr);
	cmd = sc->curcmd;
	if (sr & MCI_SR_ERROR) {
		// Ignore CRC errors on CMD2 and ACMD47, per relevant standards
		if ((sr & MCI_SR_RCRCE) && (cmd->opcode == MMC_SEND_OP_COND ||
		    cmd->opcode == ACMD_SD_SEND_OP_COND))
			cmd->error = MMC_ERR_NONE;
		else if (sr & (MCI_SR_RTOE | MCI_SR_DTOE))
			cmd->error = MMC_ERR_TIMEOUT;
		else if (sr & (MCI_SR_RCRCE | MCI_SR_DCRCE))
			cmd->error = MMC_ERR_BADCRC;
		else if (sr & (MCI_SR_OVRE | MCI_SR_UNRE))
			cmd->error = MMC_ERR_FIFO;
		else
			cmd->error = MMC_ERR_FAILED;
		done = 1;
		if (sc->mapped && cmd->error) {
			bus_dmamap_unload(sc->dmatag, sc->map);
			sc->mapped--;
		}
	} else {
		if (sr & MCI_SR_TXBUFE) {
//			printf("TXBUFE\n");
			at91_mci_xmit_done(sc);
		}
		if (sr & MCI_SR_RXBUFF) {
//			printf("RXBUFF\n");
			WR4(sc, MCI_IDR, MCI_SR_RXBUFF);
			WR4(sc, MCI_IER, MCI_SR_CMDRDY);
		}
		if (sr & MCI_SR_ENDTX) {
//			printf("ENDTX\n");
		}
		if (sr & MCI_SR_ENDRX) {
//			printf("ENDRX\n");
			at91_mci_read_done(sc);
		}
		if (sr & MCI_SR_NOTBUSY) {
//			printf("NOTBUSY\n");
			WR4(sc, MCI_IDR, MCI_SR_NOTBUSY);
			WR4(sc, MCI_IER, MCI_SR_CMDRDY);
		}
		if (sr & MCI_SR_DTIP) {
//			printf("Data transfer in progress\n");
		}
		if (sr & MCI_SR_BLKE) {
//			printf("Block transfer end\n");
		}
		if (sr & MCI_SR_TXRDY) {
//			printf("Ready to transmit\n");
		}
		if (sr & MCI_SR_RXRDY) {
//			printf("Ready to receive\n");
		}
		if (sr & MCI_SR_CMDRDY) {
//			printf("Command ready\n");
			done = 1;
			cmd->error = MMC_ERR_NONE;
		}
	}
	if (done) {
		WR4(sc, MCI_IDR, 0xffffffff);
		if (cmd != NULL && (cmd->flags & MMC_RSP_PRESENT)) {
			for (i = 0; i < ((cmd->flags & MMC_RSP_136) ? 4 : 1);
			     i++) {
				cmd->resp[i] = RD4(sc, MCI_RSPR + i * 4);
//				printf("RSPR[%d] = %x\n", i, cmd->resp[i]);
			}
		}
		at91_mci_start(sc);
	}
	AT91_MCI_UNLOCK(sc);
}

static int
at91_mci_read_ivar(device_t bus, device_t child, int which, u_char *result)
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
	case MMCBR_IVAR_MAX_DATA:
		*(int *)result = 1;
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

	{0, 0},
};

static driver_t at91_mci_driver = {
	"at91_mci",
	at91_mci_methods,
	sizeof(struct at91_mci_softc),
};
static devclass_t at91_mci_devclass;


DRIVER_MODULE(at91_mci, atmelarm, at91_mci_driver, at91_mci_devclass, 0, 0);
