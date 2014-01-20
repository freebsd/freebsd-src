/*-
 * Copyright (c) 2011 Jakub Wojciech Klama <jceel@FreeBSD.org>
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

#include <sys/kdb.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/resource.h>
#include <machine/intr.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/mmc/bridge.h>
#include <dev/mmc/mmcreg.h>
#include <dev/mmc/mmcbrvar.h>

#include <arm/lpc/lpcreg.h>
#include <arm/lpc/lpcvar.h>

#ifdef DEBUG
#define debugf(fmt, args...) do { printf("%s(): ", __func__);   \
    printf(fmt,##args); } while (0)
#else
#define debugf(fmt, args...)
#endif

struct lpc_mmc_dmamap_arg {
	bus_addr_t		lm_dma_busaddr;
};

struct lpc_mmc_softc {
	device_t		lm_dev;
	struct mtx		lm_mtx;
	struct resource *	lm_mem_res;
	struct resource *	lm_irq_res;
	bus_space_tag_t		lm_bst;
	bus_space_handle_t	lm_bsh;
	void *			lm_intrhand;
	struct mmc_host		lm_host;
	struct mmc_request *	lm_req;
	struct mmc_data *	lm_data;
	uint32_t		lm_flags;
#define	LPC_SD_FLAGS_IGNORECRC		(1 << 0)
	int			lm_xfer_direction;
#define	DIRECTION_READ		0
#define	DIRECTION_WRITE		1
	int			lm_xfer_done;
	int			lm_bus_busy;
	bus_dma_tag_t		lm_dma_tag;
	bus_dmamap_t		lm_dma_map;
	bus_addr_t		lm_buffer_phys;
	void *			lm_buffer;
};

#define	LPC_SD_MAX_BLOCKSIZE	1024
/* XXX */
#define	LPC_MMC_DMACH_READ	1
#define	LPC_MMC_DMACH_WRITE	0


static int lpc_mmc_probe(device_t);
static int lpc_mmc_attach(device_t);
static int lpc_mmc_detach(device_t);
static void lpc_mmc_intr(void *);

static void lpc_mmc_cmd(struct lpc_mmc_softc *, struct mmc_command *);
static void lpc_mmc_setup_xfer(struct lpc_mmc_softc *, struct mmc_data *);

static int lpc_mmc_update_ios(device_t, device_t);
static int lpc_mmc_request(device_t, device_t, struct mmc_request *);
static int lpc_mmc_get_ro(device_t, device_t);
static int lpc_mmc_acquire_host(device_t, device_t);
static int lpc_mmc_release_host(device_t, device_t);

static void lpc_mmc_dma_rxfinish(void *);
static void lpc_mmc_dma_rxerror(void *);
static void lpc_mmc_dma_txfinish(void *);
static void lpc_mmc_dma_txerror(void *);

static void lpc_mmc_dmamap_cb(void *, bus_dma_segment_t *, int, int);

#define	lpc_mmc_lock(_sc)						\
    mtx_lock(&_sc->lm_mtx);
#define	lpc_mmc_unlock(_sc)						\
    mtx_unlock(&_sc->lm_mtx);
#define	lpc_mmc_read_4(_sc, _reg)					\
    bus_space_read_4(_sc->lm_bst, _sc->lm_bsh, _reg)
#define	lpc_mmc_write_4(_sc, _reg, _value)				\
    bus_space_write_4(_sc->lm_bst, _sc->lm_bsh, _reg, _value)

static struct lpc_dmac_channel_config lpc_mmc_dma_rxconf = {
	.ldc_fcntl = LPC_DMAC_FLOW_D_P2M,
	.ldc_src_periph = LPC_DMAC_SD_ID,
	.ldc_src_width = LPC_DMAC_CH_CONTROL_WIDTH_4,
	.ldc_src_incr = 0,
	.ldc_src_burst = LPC_DMAC_CH_CONTROL_BURST_8,
	.ldc_dst_periph = LPC_DMAC_SD_ID,
	.ldc_dst_width = LPC_DMAC_CH_CONTROL_WIDTH_4,
	.ldc_dst_incr = 1,
	.ldc_dst_burst = LPC_DMAC_CH_CONTROL_BURST_8,
	.ldc_success_handler = lpc_mmc_dma_rxfinish,
	.ldc_error_handler = lpc_mmc_dma_rxerror,
};

static struct lpc_dmac_channel_config lpc_mmc_dma_txconf = {
	.ldc_fcntl = LPC_DMAC_FLOW_P_M2P,
	.ldc_src_periph = LPC_DMAC_SD_ID,
	.ldc_src_width = LPC_DMAC_CH_CONTROL_WIDTH_4,
	.ldc_src_incr = 1,
	.ldc_src_burst = LPC_DMAC_CH_CONTROL_BURST_8,
	.ldc_dst_periph = LPC_DMAC_SD_ID,
	.ldc_dst_width = LPC_DMAC_CH_CONTROL_WIDTH_4,
	.ldc_dst_incr = 0,
	.ldc_dst_burst = LPC_DMAC_CH_CONTROL_BURST_8,
	.ldc_success_handler = lpc_mmc_dma_txfinish,
	.ldc_error_handler = lpc_mmc_dma_txerror,
};

static int
lpc_mmc_probe(device_t dev)
{
	if (!ofw_bus_is_compatible(dev, "lpc,mmc"))
		return (ENXIO);

	device_set_desc(dev, "LPC32x0 MMC/SD controller");
	return (BUS_PROBE_DEFAULT);
}

static int
lpc_mmc_attach(device_t dev)
{
	struct lpc_mmc_softc *sc = device_get_softc(dev);
	struct lpc_mmc_dmamap_arg ctx;
	device_t child;
	int rid, err;

	sc->lm_dev = dev;
	sc->lm_req = NULL;

	mtx_init(&sc->lm_mtx, "lpcmmc", "mmc", MTX_DEF);

	rid = 0;
	sc->lm_mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (!sc->lm_mem_res) {
		device_printf(dev, "cannot allocate memory window\n");
		return (ENXIO);
	}

	sc->lm_bst = rman_get_bustag(sc->lm_mem_res);
	sc->lm_bsh = rman_get_bushandle(sc->lm_mem_res);

	debugf("virtual register space: 0x%08lx\n", sc->lm_bsh);

	rid = 0;
	sc->lm_irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE);
	if (!sc->lm_irq_res) {
		device_printf(dev, "cannot allocate interrupt\n");
		bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->lm_mem_res);
		return (ENXIO);
	}

	if (bus_setup_intr(dev, sc->lm_irq_res, INTR_TYPE_MISC | INTR_MPSAFE,
	    NULL, lpc_mmc_intr, sc, &sc->lm_intrhand))
	{
		bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->lm_mem_res);
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->lm_irq_res);
		device_printf(dev, "cannot setup interrupt handler\n");
		return (ENXIO);
	}

	sc->lm_host.f_min = 312500;
	sc->lm_host.f_max = 2500000;
	sc->lm_host.host_ocr = MMC_OCR_300_310 | MMC_OCR_310_320 |
	    MMC_OCR_320_330 | MMC_OCR_330_340;
#if 0
	sc->lm_host.caps = MMC_CAP_4_BIT_DATA;
#endif

	lpc_pwr_write(dev, LPC_CLKPWR_MS_CTRL,
	    LPC_CLKPWR_MS_CTRL_CLOCK_EN | LPC_CLKPWR_MS_CTRL_SD_CLOCK | 1);
	lpc_mmc_write_4(sc, LPC_SD_POWER, LPC_SD_POWER_CTRL_ON);

	device_set_ivars(dev, &sc->lm_host);

	child = device_add_child(dev, "mmc", -1);
	if (!child) {
		device_printf(dev, "attaching MMC bus failed!\n");
		bus_teardown_intr(dev, sc->lm_irq_res, sc->lm_intrhand);
		bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->lm_mem_res);
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->lm_irq_res);
		return (ENXIO);
	}

	/* Alloc DMA memory */
	err = bus_dma_tag_create(
	    bus_get_dma_tag(sc->lm_dev),
	    4, 0,			/* alignment, boundary */
	    BUS_SPACE_MAXADDR_32BIT,	/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filter, filterarg */
	    LPC_SD_MAX_BLOCKSIZE, 1,	/* maxsize, nsegments */
	    LPC_SD_MAX_BLOCKSIZE, 0,	/* maxsegsize, flags */
	    NULL, NULL,			/* lockfunc, lockarg */
	    &sc->lm_dma_tag);

	err = bus_dmamem_alloc(sc->lm_dma_tag, (void **)&sc->lm_buffer,
	    0, &sc->lm_dma_map);
	if (err) {
		device_printf(dev, "cannot allocate framebuffer\n");
		goto fail;
	}

	err = bus_dmamap_load(sc->lm_dma_tag, sc->lm_dma_map, sc->lm_buffer,
	    LPC_SD_MAX_BLOCKSIZE, lpc_mmc_dmamap_cb, &ctx, BUS_DMA_NOWAIT);
	if (err) {
		device_printf(dev, "cannot load DMA map\n");
		goto fail;
	}

	sc->lm_buffer_phys = ctx.lm_dma_busaddr;

	lpc_mmc_dma_rxconf.ldc_handler_arg = (void *)sc;
	err = lpc_dmac_config_channel(dev, LPC_MMC_DMACH_READ, &lpc_mmc_dma_rxconf);
	if (err) {
		device_printf(dev, "cannot allocate RX DMA channel\n");
		goto fail;
	}


	lpc_mmc_dma_txconf.ldc_handler_arg = (void *)sc;
	err = lpc_dmac_config_channel(dev, LPC_MMC_DMACH_WRITE, &lpc_mmc_dma_txconf);	
	if (err) {
		device_printf(dev, "cannot allocate TX DMA channel\n");
		goto fail;
	}

	bus_generic_probe(dev);
	bus_generic_attach(dev);

	return (0);

fail:
	if (sc->lm_intrhand)
		bus_teardown_intr(dev, sc->lm_irq_res, sc->lm_intrhand);
	if (sc->lm_irq_res)
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->lm_irq_res);
	if (sc->lm_mem_res)
		bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->lm_mem_res);
	return (err);
}

static int
lpc_mmc_detach(device_t dev)
{
	return (EBUSY);
}

static void
lpc_mmc_intr(void *arg)
{
	struct lpc_mmc_softc *sc = (struct lpc_mmc_softc *)arg;
	struct mmc_command *cmd;
	uint32_t status;

	status = lpc_mmc_read_4(sc, LPC_SD_STATUS);

	debugf("interrupt: 0x%08x\n", status);

	if (status & LPC_SD_STATUS_CMDCRCFAIL) {
		cmd = sc->lm_req->cmd;
		cmd->error = sc->lm_flags & LPC_SD_FLAGS_IGNORECRC
		    ? MMC_ERR_NONE : MMC_ERR_BADCRC;
		cmd->resp[0] = lpc_mmc_read_4(sc, LPC_SD_RESP0);
		sc->lm_req->done(sc->lm_req);
		sc->lm_req = NULL;
		lpc_mmc_write_4(sc, LPC_SD_CLEAR, LPC_SD_STATUS_CMDCRCFAIL);	
	}

	if (status & LPC_SD_STATUS_CMDACTIVE)
	{
		debugf("command active\n");
		cmd = sc->lm_req->cmd;
		cmd->resp[0] = lpc_mmc_read_4(sc, LPC_SD_RESP0);
		sc->lm_req->done(sc->lm_req);
		sc->lm_req = NULL;
	}
	
	if (status & LPC_SD_STATUS_DATATIMEOUT) {
		device_printf(sc->lm_dev, "data timeout\n");
		lpc_mmc_write_4(sc, LPC_SD_CLEAR, LPC_SD_STATUS_DATATIMEOUT);
	}

	if (status & LPC_SD_STATUS_TXUNDERRUN) {
		device_printf(sc->lm_dev, "TX underrun\n");
		lpc_mmc_write_4(sc, LPC_SD_CLEAR, LPC_SD_STATUS_TXUNDERRUN);
	}
	
	if (status & LPC_SD_STATUS_CMDRESPEND) {
		debugf("command response\n");
		cmd = sc->lm_req->cmd;
		
		if (cmd->flags & MMC_RSP_136) {
			cmd->resp[3] = lpc_mmc_read_4(sc, LPC_SD_RESP3);
			cmd->resp[2] = lpc_mmc_read_4(sc, LPC_SD_RESP2);
			cmd->resp[1] = lpc_mmc_read_4(sc, LPC_SD_RESP1);
		}

		cmd->resp[0] = lpc_mmc_read_4(sc, LPC_SD_RESP0);
		cmd->error = MMC_ERR_NONE;
	
		if (cmd->data && (cmd->data->flags & MMC_DATA_WRITE))
			lpc_mmc_setup_xfer(sc, sc->lm_req->cmd->data);

		if (!cmd->data) {	
			sc->lm_req->done(sc->lm_req);
			sc->lm_req = NULL;
		}

		lpc_mmc_write_4(sc, LPC_SD_CLEAR, LPC_SD_STATUS_CMDRESPEND);
	}

	if (status & LPC_SD_STATUS_CMDSENT) {
		debugf("command sent\n");
		cmd = sc->lm_req->cmd;
		cmd->error = MMC_ERR_NONE;
		sc->lm_req->done(sc->lm_req);
		sc->lm_req = NULL;
		lpc_mmc_write_4(sc, LPC_SD_CLEAR, LPC_SD_STATUS_CMDSENT);
	}
	
	if (status & LPC_SD_STATUS_DATAEND) {
		if (sc->lm_xfer_direction == DIRECTION_READ)
			lpc_dmac_start_burst(sc->lm_dev, LPC_DMAC_SD_ID);

		lpc_mmc_write_4(sc, LPC_SD_CLEAR, LPC_SD_STATUS_DATAEND);
	}

	if (status & LPC_SD_STATUS_CMDTIMEOUT) {
		device_printf(sc->lm_dev, "command response timeout\n");
		cmd = sc->lm_req->cmd;
		cmd->error = MMC_ERR_TIMEOUT;
		sc->lm_req->done(sc->lm_req);
		sc->lm_req = NULL;
		lpc_mmc_write_4(sc, LPC_SD_CLEAR, LPC_SD_STATUS_CMDTIMEOUT);
		return;
	}

	if (status & LPC_SD_STATUS_STARTBITERR) {
		device_printf(sc->lm_dev, "start bit error\n");
		lpc_mmc_write_4(sc, LPC_SD_CLEAR, LPC_SD_STATUS_STARTBITERR);
	}

	if (status & LPC_SD_STATUS_DATACRCFAIL) {		
		device_printf(sc->lm_dev, "data CRC error\n");
		debugf("data buffer: %p\n", sc->lm_buffer);
		cmd = sc->lm_req->cmd;
		cmd->error = MMC_ERR_BADCRC;
		sc->lm_req->done(sc->lm_req);
		sc->lm_req = NULL;

		if (sc->lm_xfer_direction == DIRECTION_READ)
			lpc_dmac_start_burst(sc->lm_dev, LPC_DMAC_SD_ID);

		lpc_mmc_write_4(sc, LPC_SD_CLEAR, LPC_SD_STATUS_DATACRCFAIL);
	}

	if (status & LPC_SD_STATUS_DATABLOCKEND) {
		debugf("data block end\n");
		if (sc->lm_xfer_direction == DIRECTION_READ)
			memcpy(sc->lm_data->data, sc->lm_buffer, sc->lm_data->len);

		if (sc->lm_xfer_direction == DIRECTION_WRITE) {
			lpc_dmac_disable_channel(sc->lm_dev, LPC_MMC_DMACH_WRITE);
			lpc_mmc_write_4(sc, LPC_SD_DATACTRL, 0);
		}
	
		sc->lm_req->done(sc->lm_req);
		sc->lm_req = NULL;
		lpc_mmc_write_4(sc, LPC_SD_CLEAR, LPC_SD_STATUS_DATABLOCKEND);
	}

	debugf("done\n");
}

static int
lpc_mmc_request(device_t bus, device_t child, struct mmc_request *req)
{
	struct lpc_mmc_softc *sc = device_get_softc(bus);

	debugf("request: %p\n", req);

	lpc_mmc_lock(sc);
	if (sc->lm_req)
		return (EBUSY);

	sc->lm_req = req;

	if (req->cmd->data && req->cmd->data->flags & MMC_DATA_WRITE) {
		memcpy(sc->lm_buffer, req->cmd->data->data, req->cmd->data->len);
		lpc_mmc_cmd(sc, req->cmd);
		lpc_mmc_unlock(sc);
		return (0);
	}

	if (req->cmd->data)
		lpc_mmc_setup_xfer(sc, req->cmd->data);

	lpc_mmc_cmd(sc, req->cmd);
	lpc_mmc_unlock(sc);

	return (0);
}

static void
lpc_mmc_cmd(struct lpc_mmc_softc *sc, struct mmc_command *cmd)
{
	uint32_t cmdreg = 0;

	debugf("cmd: %d arg: 0x%08x\n", cmd->opcode, cmd->arg);

	if (lpc_mmc_read_4(sc, LPC_SD_COMMAND) & LPC_SD_COMMAND_ENABLE) {
		lpc_mmc_write_4(sc, LPC_SD_COMMAND, 0);
		DELAY(1000);
	}

	sc->lm_flags &= ~LPC_SD_FLAGS_IGNORECRC;

	if (cmd->flags & MMC_RSP_PRESENT)
		cmdreg |= LPC_SD_COMMAND_RESPONSE;

	if (MMC_RSP(cmd->flags) == MMC_RSP_R2)
		cmdreg |= LPC_SD_COMMAND_LONGRSP;

	if (MMC_RSP(cmd->flags) == MMC_RSP_R3)
		sc->lm_flags |= LPC_SD_FLAGS_IGNORECRC;

	cmdreg |= LPC_SD_COMMAND_ENABLE;
	cmdreg |= (cmd->opcode & LPC_SD_COMMAND_CMDINDEXMASK);

	lpc_mmc_write_4(sc, LPC_SD_MASK0, 0xffffffff);
	lpc_mmc_write_4(sc, LPC_SD_MASK1, 0xffffffff);
	lpc_mmc_write_4(sc, LPC_SD_ARGUMENT, cmd->arg);
	lpc_mmc_write_4(sc, LPC_SD_COMMAND, cmdreg);
}

static void
lpc_mmc_setup_xfer(struct lpc_mmc_softc *sc, struct mmc_data *data)
{
	uint32_t datactrl = 0;
	int data_words = data->len / 4;

	sc->lm_data = data;
	sc->lm_xfer_done = 0;

	debugf("data: %p, len: %d, %s\n", data,
	    data->len, (data->flags & MMC_DATA_READ) ? "read" : "write");

	if (data->flags & MMC_DATA_READ) {
		sc->lm_xfer_direction = DIRECTION_READ;
		lpc_dmac_setup_transfer(sc->lm_dev, LPC_MMC_DMACH_READ,
		    LPC_SD_PHYS_BASE + LPC_SD_FIFO, sc->lm_buffer_phys,
		    data_words, 0);
	}

	if (data->flags & MMC_DATA_WRITE) {
		sc->lm_xfer_direction = DIRECTION_WRITE;
		lpc_dmac_setup_transfer(sc->lm_dev, LPC_MMC_DMACH_WRITE,
		    sc->lm_buffer_phys, LPC_SD_PHYS_BASE + LPC_SD_FIFO,
		    data_words, 0);
	}

	datactrl |= (sc->lm_xfer_direction 
	    ? LPC_SD_DATACTRL_WRITE 
	    : LPC_SD_DATACTRL_READ);

	datactrl |= LPC_SD_DATACTRL_DMAENABLE | LPC_SD_DATACTRL_ENABLE;
	datactrl |= (ffs(data->len) - 1) << 4;

	debugf("datactrl: 0x%08x\n", datactrl);

	lpc_mmc_write_4(sc, LPC_SD_DATATIMER, 0xFFFF0000);
	lpc_mmc_write_4(sc, LPC_SD_DATALENGTH, data->len);
	lpc_mmc_write_4(sc, LPC_SD_DATACTRL, datactrl);
}

static int
lpc_mmc_read_ivar(device_t bus, device_t child, int which, 
    uintptr_t *result)
{
	struct lpc_mmc_softc *sc = device_get_softc(bus);

	switch (which) {
	default:
		return (EINVAL);
	case MMCBR_IVAR_BUS_MODE:
		*(int *)result = sc->lm_host.ios.bus_mode;
		break;
	case MMCBR_IVAR_BUS_WIDTH:
		*(int *)result = sc->lm_host.ios.bus_width;
		break;
	case MMCBR_IVAR_CHIP_SELECT:
		*(int *)result = sc->lm_host.ios.chip_select;
		break;
	case MMCBR_IVAR_CLOCK:
		*(int *)result = sc->lm_host.ios.clock;
		break;
	case MMCBR_IVAR_F_MIN:
		*(int *)result = sc->lm_host.f_min;
		break;
	case MMCBR_IVAR_F_MAX:
		*(int *)result = sc->lm_host.f_max;
		break;
	case MMCBR_IVAR_HOST_OCR:
		*(int *)result = sc->lm_host.host_ocr;
		break;
	case MMCBR_IVAR_MODE:
		*(int *)result = sc->lm_host.mode;
		break;
	case MMCBR_IVAR_OCR:
		*(int *)result = sc->lm_host.ocr;
		break;
	case MMCBR_IVAR_POWER_MODE:
		*(int *)result = sc->lm_host.ios.power_mode;
		break;
	case MMCBR_IVAR_VDD:
		*(int *)result = sc->lm_host.ios.vdd;
		break;
	case MMCBR_IVAR_CAPS:
		*(int *)result = sc->lm_host.caps;
		break;
	case MMCBR_IVAR_MAX_DATA:
		*(int *)result = 1;
		break;
	}

	return (0);
}

static int
lpc_mmc_write_ivar(device_t bus, device_t child, int which,
    uintptr_t value)
{
	struct lpc_mmc_softc *sc = device_get_softc(bus);

	switch (which) {
	default:
		return (EINVAL);
	case MMCBR_IVAR_BUS_MODE:
		sc->lm_host.ios.bus_mode = value;
		break;
	case MMCBR_IVAR_BUS_WIDTH:
		sc->lm_host.ios.bus_width = value;
		break;
	case MMCBR_IVAR_CHIP_SELECT:
		sc->lm_host.ios.chip_select = value;
		break;
	case MMCBR_IVAR_CLOCK:
		sc->lm_host.ios.clock = value;
		break;
	case MMCBR_IVAR_MODE:
		sc->lm_host.mode = value;
		break;
	case MMCBR_IVAR_OCR:
		sc->lm_host.ocr = value;
		break;
	case MMCBR_IVAR_POWER_MODE:
		sc->lm_host.ios.power_mode = value;
		break;
	case MMCBR_IVAR_VDD:
		sc->lm_host.ios.vdd = value;
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

static int
lpc_mmc_update_ios(device_t bus, device_t child)
{
	struct lpc_mmc_softc *sc = device_get_softc(bus);
	struct mmc_ios *ios = &sc->lm_host.ios;
	uint32_t clkdiv = 0, pwr = 0;

	if (ios->bus_width == bus_width_4)
		clkdiv |= LPC_SD_CLOCK_WIDEBUS;

	/* Calculate clock divider */
	clkdiv = (LPC_SD_CLK / (2 * ios->clock)) - 1;

	/* Clock rate should not exceed rate requested in ios */
	if ((LPC_SD_CLK / (2 * (clkdiv + 1))) > ios->clock)
		clkdiv++;

	debugf("clock: %dHz, clkdiv: %d\n", ios->clock, clkdiv);

	if (ios->bus_width == bus_width_4) {
		debugf("using wide bus mode\n");
		clkdiv |= LPC_SD_CLOCK_WIDEBUS;
	}

	lpc_mmc_write_4(sc, LPC_SD_CLOCK, clkdiv | LPC_SD_CLOCK_ENABLE);

	switch (ios->power_mode) {
	case power_off:
		pwr |= LPC_SD_POWER_CTRL_OFF;
		break;
	case power_up:
		pwr |= LPC_SD_POWER_CTRL_UP;
		break;
	case power_on:
		pwr |= LPC_SD_POWER_CTRL_ON;
		break;
	}

	if (ios->bus_mode == opendrain)
		pwr |= LPC_SD_POWER_OPENDRAIN;

	lpc_mmc_write_4(sc, LPC_SD_POWER, pwr);

	return (0);
}

static int
lpc_mmc_get_ro(device_t bus, device_t child)
{

	return (0);
}

static int
lpc_mmc_acquire_host(device_t bus, device_t child)
{
	struct lpc_mmc_softc *sc = device_get_softc(bus);
	int error = 0;

	lpc_mmc_lock(sc);
	while (sc->lm_bus_busy)
		error = mtx_sleep(sc, &sc->lm_mtx, PZERO, "mmcah", 0);

	sc->lm_bus_busy++;
	lpc_mmc_unlock(sc);
	return (error);
}

static int
lpc_mmc_release_host(device_t bus, device_t child)
{
	struct lpc_mmc_softc *sc = device_get_softc(bus);

	lpc_mmc_lock(sc);
	sc->lm_bus_busy--;
	wakeup(sc);
	lpc_mmc_unlock(sc);
	return (0);
}

static void lpc_mmc_dma_rxfinish(void *arg)
{
}

static void lpc_mmc_dma_rxerror(void *arg)
{
	struct lpc_mmc_softc *sc = (struct lpc_mmc_softc *)arg;
	device_printf(sc->lm_dev, "DMA RX error\n");
}

static void lpc_mmc_dma_txfinish(void *arg)
{
}

static void lpc_mmc_dma_txerror(void *arg)
{
	struct lpc_mmc_softc *sc = (struct lpc_mmc_softc *)arg;
	device_printf(sc->lm_dev, "DMA TX error\n");
}

static void
lpc_mmc_dmamap_cb(void *arg, bus_dma_segment_t *segs, int nseg, int err)
{
	struct lpc_mmc_dmamap_arg *ctx;

	if (err)
		return;

	ctx = (struct lpc_mmc_dmamap_arg *)arg;
	ctx->lm_dma_busaddr = segs[0].ds_addr;
}

static device_method_t lpc_mmc_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		lpc_mmc_probe),
	DEVMETHOD(device_attach,	lpc_mmc_attach),
	DEVMETHOD(device_detach,	lpc_mmc_detach),

	/* Bus interface */
	DEVMETHOD(bus_read_ivar,	lpc_mmc_read_ivar),
	DEVMETHOD(bus_write_ivar,	lpc_mmc_write_ivar),
	DEVMETHOD(bus_print_child,	bus_generic_print_child),

	/* MMC bridge interface */
	DEVMETHOD(mmcbr_update_ios,	lpc_mmc_update_ios),
	DEVMETHOD(mmcbr_request,	lpc_mmc_request),
	DEVMETHOD(mmcbr_get_ro,		lpc_mmc_get_ro),
	DEVMETHOD(mmcbr_acquire_host,	lpc_mmc_acquire_host),
	DEVMETHOD(mmcbr_release_host,	lpc_mmc_release_host),

	{ 0, 0 }
};

static devclass_t lpc_mmc_devclass;

static driver_t lpc_mmc_driver = {
	"lpcmmc",
	lpc_mmc_methods,
	sizeof(struct lpc_mmc_softc),
};

DRIVER_MODULE(lpcmmc, simplebus, lpc_mmc_driver, lpc_mmc_devclass, 0, 0);
