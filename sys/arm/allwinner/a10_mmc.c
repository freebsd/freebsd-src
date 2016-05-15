/*-
 * Copyright (c) 2013 Alexander Fedorov
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
#include <sys/resource.h>
#include <sys/rman.h>
#include <sys/sysctl.h>

#include <machine/bus.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/mmc/bridge.h>
#include <dev/mmc/mmcreg.h>
#include <dev/mmc/mmcbrvar.h>

#include <arm/allwinner/allwinner_machdep.h>
#include <arm/allwinner/a10_mmc.h>
#include <dev/extres/clk/clk.h>
#include <dev/extres/hwreset/hwreset.h>

#define	A10_MMC_MEMRES		0
#define	A10_MMC_IRQRES		1
#define	A10_MMC_RESSZ		2
#define	A10_MMC_DMA_SEGS	16
#define	A10_MMC_DMA_MAX_SIZE	0x2000
#define	A10_MMC_DMA_FTRGLEVEL	0x20070008

#define	CARD_ID_FREQUENCY	400000

static int a10_mmc_pio_mode = 0;

TUNABLE_INT("hw.a10.mmc.pio_mode", &a10_mmc_pio_mode);

static struct ofw_compat_data compat_data[] = {
	{"allwinner,sun4i-a10-mmc", 1},
	{"allwinner,sun5i-a13-mmc", 1},
	{NULL,             0}
};

struct a10_mmc_softc {
	bus_space_handle_t	a10_bsh;
	bus_space_tag_t		a10_bst;
	device_t		a10_dev;
	clk_t			a10_clk_ahb;
	clk_t			a10_clk_mmc;
	hwreset_t		a10_rst_ahb;
	int			a10_bus_busy;
	int			a10_id;
	int			a10_resid;
	int			a10_timeout;
	struct callout		a10_timeoutc;
	struct mmc_host		a10_host;
	struct mmc_request *	a10_req;
	struct mtx		a10_mtx;
	struct resource *	a10_res[A10_MMC_RESSZ];
	uint32_t		a10_intr;
	uint32_t		a10_intr_wait;
	void *			a10_intrhand;
	bus_size_t		a10_fifo_reg;

	/* Fields required for DMA access. */
	bus_addr_t	  	a10_dma_desc_phys;
	bus_dmamap_t		a10_dma_map;
	bus_dma_tag_t 		a10_dma_tag;
	void * 			a10_dma_desc;
	bus_dmamap_t		a10_dma_buf_map;
	bus_dma_tag_t		a10_dma_buf_tag;
	int			a10_dma_inuse;
	int			a10_dma_map_err;
};

static struct resource_spec a10_mmc_res_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ SYS_RES_IRQ,		0,	RF_ACTIVE | RF_SHAREABLE },
	{ -1,			0,	0 }
};

static int a10_mmc_probe(device_t);
static int a10_mmc_attach(device_t);
static int a10_mmc_detach(device_t);
static int a10_mmc_setup_dma(struct a10_mmc_softc *);
static int a10_mmc_reset(struct a10_mmc_softc *);
static void a10_mmc_intr(void *);
static int a10_mmc_update_clock(struct a10_mmc_softc *);

static int a10_mmc_update_ios(device_t, device_t);
static int a10_mmc_request(device_t, device_t, struct mmc_request *);
static int a10_mmc_get_ro(device_t, device_t);
static int a10_mmc_acquire_host(device_t, device_t);
static int a10_mmc_release_host(device_t, device_t);

#define	A10_MMC_LOCK(_sc)	mtx_lock(&(_sc)->a10_mtx)
#define	A10_MMC_UNLOCK(_sc)	mtx_unlock(&(_sc)->a10_mtx)
#define	A10_MMC_READ_4(_sc, _reg)					\
	bus_space_read_4((_sc)->a10_bst, (_sc)->a10_bsh, _reg)
#define	A10_MMC_WRITE_4(_sc, _reg, _value)				\
	bus_space_write_4((_sc)->a10_bst, (_sc)->a10_bsh, _reg, _value)

static int
a10_mmc_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);
	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "Allwinner Integrated MMC/SD controller");

	return (BUS_PROBE_DEFAULT);
}

static int
a10_mmc_attach(device_t dev)
{
	device_t child;
	struct a10_mmc_softc *sc;
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid_list *tree;
	uint32_t bus_width;
	phandle_t node;
	int error;

	node = ofw_bus_get_node(dev);
	sc = device_get_softc(dev);
	sc->a10_dev = dev;
	sc->a10_req = NULL;
	sc->a10_id = device_get_unit(dev);
	if (sc->a10_id > 3) {
		device_printf(dev, "only 4 hosts are supported (0-3)\n");
		return (ENXIO);
	}
	if (bus_alloc_resources(dev, a10_mmc_res_spec, sc->a10_res) != 0) {
		device_printf(dev, "cannot allocate device resources\n");
		return (ENXIO);
	}
	sc->a10_bst = rman_get_bustag(sc->a10_res[A10_MMC_MEMRES]);
	sc->a10_bsh = rman_get_bushandle(sc->a10_res[A10_MMC_MEMRES]);
	if (bus_setup_intr(dev, sc->a10_res[A10_MMC_IRQRES],
	    INTR_TYPE_MISC | INTR_MPSAFE, NULL, a10_mmc_intr, sc,
	    &sc->a10_intrhand)) {
		bus_release_resources(dev, a10_mmc_res_spec, sc->a10_res);
		device_printf(dev, "cannot setup interrupt handler\n");
		return (ENXIO);
	}
	mtx_init(&sc->a10_mtx, device_get_nameunit(sc->a10_dev), "a10_mmc",
	    MTX_DEF);
	callout_init_mtx(&sc->a10_timeoutc, &sc->a10_mtx, 0);

	/*
	 * Later chips use a different FIFO offset. Unfortunately the FDT
	 * uses the same compatible string for old and new implementations.
	 */
	switch (allwinner_soc_family()) {
	case ALLWINNERSOC_SUN4I:
	case ALLWINNERSOC_SUN5I:
	case ALLWINNERSOC_SUN7I:
		sc->a10_fifo_reg = A10_MMC_FIFO;
		break;
	default:
		sc->a10_fifo_reg = A31_MMC_FIFO;
		break;
	}

	/* De-assert reset */
	if (hwreset_get_by_ofw_name(dev, "ahb", &sc->a10_rst_ahb) == 0) {
		error = hwreset_deassert(sc->a10_rst_ahb);
		if (error != 0) {
			device_printf(dev, "cannot de-assert reset\n");
			return (error);
		}
	}

	/* Activate the module clock. */
	error = clk_get_by_ofw_name(dev, "ahb", &sc->a10_clk_ahb);
	if (error != 0) {
		device_printf(dev, "cannot get ahb clock\n");
		goto fail;
	}
	error = clk_enable(sc->a10_clk_ahb);
	if (error != 0) {
		device_printf(dev, "cannot enable ahb clock\n");
		goto fail;
	}
	error = clk_get_by_ofw_name(dev, "mmc", &sc->a10_clk_mmc);
	if (error != 0) {
		device_printf(dev, "cannot get mmc clock\n");
		goto fail;
	}
	error = clk_set_freq(sc->a10_clk_mmc, CARD_ID_FREQUENCY,
	    CLK_SET_ROUND_DOWN);
	if (error != 0) {
		device_printf(dev, "cannot init mmc clock\n");
		goto fail;
	}
	error = clk_enable(sc->a10_clk_mmc);
	if (error != 0) {
		device_printf(dev, "cannot enable mmc clock\n");
		goto fail;
	}

	sc->a10_timeout = 10;
	ctx = device_get_sysctl_ctx(dev);
	tree = SYSCTL_CHILDREN(device_get_sysctl_tree(dev));
	SYSCTL_ADD_INT(ctx, tree, OID_AUTO, "req_timeout", CTLFLAG_RW,
	    &sc->a10_timeout, 0, "Request timeout in seconds");

	/* Reset controller. */
	if (a10_mmc_reset(sc) != 0) {
		device_printf(dev, "cannot reset the controller\n");
		goto fail;
	}

	if (a10_mmc_pio_mode == 0 && a10_mmc_setup_dma(sc) != 0) {
		device_printf(sc->a10_dev, "Couldn't setup DMA!\n");
		a10_mmc_pio_mode = 1;
	}
	if (bootverbose)
		device_printf(sc->a10_dev, "DMA status: %s\n",
		    a10_mmc_pio_mode ? "disabled" : "enabled");

	if (OF_getencprop(node, "bus-width", &bus_width, sizeof(uint32_t)) <= 0)
		bus_width = 1;

	sc->a10_host.f_min = 400000;
	sc->a10_host.f_max = 50000000;
	sc->a10_host.host_ocr = MMC_OCR_320_330 | MMC_OCR_330_340;
	sc->a10_host.mode = mode_sd;
	sc->a10_host.caps = MMC_CAP_HSPEED;
	if (bus_width >= 4)
		sc->a10_host.caps |= MMC_CAP_4_BIT_DATA;
	if (bus_width >= 8)
		sc->a10_host.caps |= MMC_CAP_8_BIT_DATA;

	child = device_add_child(dev, "mmc", -1);
	if (child == NULL) {
		device_printf(dev, "attaching MMC bus failed!\n");
		goto fail;
	}
	if (device_probe_and_attach(child) != 0) {
		device_printf(dev, "attaching MMC child failed!\n");
		device_delete_child(dev, child);
		goto fail;
	}

	return (0);

fail:
	callout_drain(&sc->a10_timeoutc);
	mtx_destroy(&sc->a10_mtx);
	bus_teardown_intr(dev, sc->a10_res[A10_MMC_IRQRES], sc->a10_intrhand);
	bus_release_resources(dev, a10_mmc_res_spec, sc->a10_res);

	return (ENXIO);
}

static int
a10_mmc_detach(device_t dev)
{

	return (EBUSY);
}

static void
a10_dma_desc_cb(void *arg, bus_dma_segment_t *segs, int nsegs, int err)
{
	struct a10_mmc_softc *sc;

	sc = (struct a10_mmc_softc *)arg;
	if (err) {
		sc->a10_dma_map_err = err;
		return;
	}
	sc->a10_dma_desc_phys = segs[0].ds_addr;
}

static int
a10_mmc_setup_dma(struct a10_mmc_softc *sc)
{
	int dma_desc_size, error;

	/* Allocate the DMA descriptor memory. */
	dma_desc_size = sizeof(struct a10_mmc_dma_desc) * A10_MMC_DMA_SEGS;
	error = bus_dma_tag_create(bus_get_dma_tag(sc->a10_dev), 1, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL,
	    dma_desc_size, 1, dma_desc_size, 0, NULL, NULL, &sc->a10_dma_tag);
	if (error)
		return (error);
	error = bus_dmamem_alloc(sc->a10_dma_tag, &sc->a10_dma_desc,
	    BUS_DMA_WAITOK | BUS_DMA_ZERO, &sc->a10_dma_map);
	if (error)
		return (error);

	error = bus_dmamap_load(sc->a10_dma_tag, sc->a10_dma_map,
	    sc->a10_dma_desc, dma_desc_size, a10_dma_desc_cb, sc, 0);
	if (error)
		return (error);
	if (sc->a10_dma_map_err)
		return (sc->a10_dma_map_err);

	/* Create the DMA map for data transfers. */
	error = bus_dma_tag_create(bus_get_dma_tag(sc->a10_dev), 1, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL,
	    A10_MMC_DMA_MAX_SIZE * A10_MMC_DMA_SEGS, A10_MMC_DMA_SEGS,
	    A10_MMC_DMA_MAX_SIZE, BUS_DMA_ALLOCNOW, NULL, NULL,
	    &sc->a10_dma_buf_tag);
	if (error)
		return (error);
	error = bus_dmamap_create(sc->a10_dma_buf_tag, 0,
	    &sc->a10_dma_buf_map);
	if (error)
		return (error);

	return (0);
}

static void
a10_dma_cb(void *arg, bus_dma_segment_t *segs, int nsegs, int err)
{
	int i;
	struct a10_mmc_dma_desc *dma_desc;
	struct a10_mmc_softc *sc;

	sc = (struct a10_mmc_softc *)arg;
	sc->a10_dma_map_err = err;
	dma_desc = sc->a10_dma_desc;
	/* Note nsegs is guaranteed to be zero if err is non-zero. */
	for (i = 0; i < nsegs; i++) {
		dma_desc[i].buf_size = segs[i].ds_len;
		dma_desc[i].buf_addr = segs[i].ds_addr;
		dma_desc[i].config = A10_MMC_DMA_CONFIG_CH |
		    A10_MMC_DMA_CONFIG_OWN;
		if (i == 0)
			dma_desc[i].config |= A10_MMC_DMA_CONFIG_FD;
		if (i < (nsegs - 1)) {
			dma_desc[i].config |= A10_MMC_DMA_CONFIG_DIC;
			dma_desc[i].next = sc->a10_dma_desc_phys +
			    ((i + 1) * sizeof(struct a10_mmc_dma_desc));
		} else {
			dma_desc[i].config |= A10_MMC_DMA_CONFIG_LD |
			    A10_MMC_DMA_CONFIG_ER;
			dma_desc[i].next = 0;
		}
 	}
}

static int
a10_mmc_prepare_dma(struct a10_mmc_softc *sc)
{
	bus_dmasync_op_t sync_op;
	int error;
	struct mmc_command *cmd;
	uint32_t val;

	cmd = sc->a10_req->cmd;
	if (cmd->data->len > A10_MMC_DMA_MAX_SIZE * A10_MMC_DMA_SEGS)
		return (EFBIG);
	error = bus_dmamap_load(sc->a10_dma_buf_tag, sc->a10_dma_buf_map,
	    cmd->data->data, cmd->data->len, a10_dma_cb, sc, BUS_DMA_NOWAIT);
	if (error)
		return (error);
	if (sc->a10_dma_map_err)
		return (sc->a10_dma_map_err);

	sc->a10_dma_inuse = 1;
	if (cmd->data->flags & MMC_DATA_WRITE)
		sync_op = BUS_DMASYNC_PREWRITE;
	else
		sync_op = BUS_DMASYNC_PREREAD;
	bus_dmamap_sync(sc->a10_dma_buf_tag, sc->a10_dma_buf_map, sync_op);
	bus_dmamap_sync(sc->a10_dma_tag, sc->a10_dma_map, BUS_DMASYNC_PREWRITE);

	val = A10_MMC_READ_4(sc, A10_MMC_IMASK);
	val &= ~(A10_MMC_RX_DATA_REQ | A10_MMC_TX_DATA_REQ);
	A10_MMC_WRITE_4(sc, A10_MMC_IMASK, val);
	val = A10_MMC_READ_4(sc, A10_MMC_GCTRL);
	val &= ~A10_MMC_ACCESS_BY_AHB;
	val |= A10_MMC_DMA_ENABLE;
	A10_MMC_WRITE_4(sc, A10_MMC_GCTRL, val);
	val |= A10_MMC_DMA_RESET;
	A10_MMC_WRITE_4(sc, A10_MMC_GCTRL, val);
	A10_MMC_WRITE_4(sc, A10_MMC_DMAC, A10_MMC_IDMAC_SOFT_RST);
	A10_MMC_WRITE_4(sc, A10_MMC_DMAC,
	    A10_MMC_IDMAC_IDMA_ON | A10_MMC_IDMAC_FIX_BURST);
	val = A10_MMC_READ_4(sc, A10_MMC_IDIE);
	val &= ~(A10_MMC_IDMAC_RECEIVE_INT | A10_MMC_IDMAC_TRANSMIT_INT);
	if (cmd->data->flags & MMC_DATA_WRITE)
		val |= A10_MMC_IDMAC_TRANSMIT_INT;
	else
		val |= A10_MMC_IDMAC_RECEIVE_INT;
	A10_MMC_WRITE_4(sc, A10_MMC_IDIE, val);
	A10_MMC_WRITE_4(sc, A10_MMC_DLBA, sc->a10_dma_desc_phys);
	A10_MMC_WRITE_4(sc, A10_MMC_FTRGL, A10_MMC_DMA_FTRGLEVEL);

	return (0);
}

static int
a10_mmc_reset(struct a10_mmc_softc *sc)
{
	int timeout;

	A10_MMC_WRITE_4(sc, A10_MMC_GCTRL,
	    A10_MMC_READ_4(sc, A10_MMC_GCTRL) | A10_MMC_RESET);
	timeout = 1000;
	while (--timeout > 0) {
		if ((A10_MMC_READ_4(sc, A10_MMC_GCTRL) & A10_MMC_RESET) == 0)
			break;
		DELAY(100);
	}
	if (timeout == 0)
		return (ETIMEDOUT);

	/* Set the timeout. */
	A10_MMC_WRITE_4(sc, A10_MMC_TIMEOUT, 0xffffffff);

	/* Clear pending interrupts. */
	A10_MMC_WRITE_4(sc, A10_MMC_RINTR, 0xffffffff);
	A10_MMC_WRITE_4(sc, A10_MMC_IDST, 0xffffffff);
	/* Unmask interrupts. */
	A10_MMC_WRITE_4(sc, A10_MMC_IMASK,
	    A10_MMC_CMD_DONE | A10_MMC_INT_ERR_BIT |
	    A10_MMC_DATA_OVER | A10_MMC_AUTOCMD_DONE);
	/* Enable interrupts and AHB access. */
	A10_MMC_WRITE_4(sc, A10_MMC_GCTRL,
	    A10_MMC_READ_4(sc, A10_MMC_GCTRL) | A10_MMC_INT_ENABLE);

	return (0);
}

static void
a10_mmc_req_done(struct a10_mmc_softc *sc)
{
	struct mmc_command *cmd;
	struct mmc_request *req;

	cmd = sc->a10_req->cmd;
	if (cmd->error != MMC_ERR_NONE) {
		/* Reset the controller. */
		a10_mmc_reset(sc);
		a10_mmc_update_clock(sc);
	}
	if (sc->a10_dma_inuse == 0) {
		/* Reset the FIFO. */
		A10_MMC_WRITE_4(sc, A10_MMC_GCTRL,
		    A10_MMC_READ_4(sc, A10_MMC_GCTRL) | A10_MMC_FIFO_RESET);
	}

	req = sc->a10_req;
	callout_stop(&sc->a10_timeoutc);
	sc->a10_req = NULL;
	sc->a10_intr = 0;
	sc->a10_resid = 0;
	sc->a10_dma_inuse = 0;
	sc->a10_dma_map_err = 0;
	sc->a10_intr_wait = 0;
	req->done(req);
}

static void
a10_mmc_req_ok(struct a10_mmc_softc *sc)
{
	int timeout;
	struct mmc_command *cmd;
	uint32_t status;

	timeout = 1000;
	while (--timeout > 0) {
		status = A10_MMC_READ_4(sc, A10_MMC_STAS);
		if ((status & A10_MMC_CARD_DATA_BUSY) == 0)
			break;
		DELAY(1000);
	}
	cmd = sc->a10_req->cmd;
	if (timeout == 0) {
		cmd->error = MMC_ERR_FAILED;
		a10_mmc_req_done(sc);
		return;
	}
	if (cmd->flags & MMC_RSP_PRESENT) {
		if (cmd->flags & MMC_RSP_136) {
			cmd->resp[0] = A10_MMC_READ_4(sc, A10_MMC_RESP3);
			cmd->resp[1] = A10_MMC_READ_4(sc, A10_MMC_RESP2);
			cmd->resp[2] = A10_MMC_READ_4(sc, A10_MMC_RESP1);
			cmd->resp[3] = A10_MMC_READ_4(sc, A10_MMC_RESP0);
		} else
			cmd->resp[0] = A10_MMC_READ_4(sc, A10_MMC_RESP0);
	}
	/* All data has been transferred ? */
	if (cmd->data != NULL && (sc->a10_resid << 2) < cmd->data->len)
		cmd->error = MMC_ERR_FAILED;
	a10_mmc_req_done(sc);
}

static void
a10_mmc_timeout(void *arg)
{
	struct a10_mmc_softc *sc;

	sc = (struct a10_mmc_softc *)arg;
	if (sc->a10_req != NULL) {
		device_printf(sc->a10_dev, "controller timeout\n");
		sc->a10_req->cmd->error = MMC_ERR_TIMEOUT;
		a10_mmc_req_done(sc);
	} else
		device_printf(sc->a10_dev,
		    "Spurious timeout - no active request\n");
}

static int
a10_mmc_pio_transfer(struct a10_mmc_softc *sc, struct mmc_data *data)
{
	int i, write;
	uint32_t bit, *buf;

	buf = (uint32_t *)data->data;
	write = (data->flags & MMC_DATA_WRITE) ? 1 : 0;
	bit = write ? A10_MMC_FIFO_FULL : A10_MMC_FIFO_EMPTY;
	for (i = sc->a10_resid; i < (data->len >> 2); i++) {
		if ((A10_MMC_READ_4(sc, A10_MMC_STAS) & bit))
			return (1);
		if (write)
			A10_MMC_WRITE_4(sc, sc->a10_fifo_reg, buf[i]);
		else
			buf[i] = A10_MMC_READ_4(sc, sc->a10_fifo_reg);
		sc->a10_resid = i + 1;
	}

	return (0);
}

static void
a10_mmc_intr(void *arg)
{
	bus_dmasync_op_t sync_op;
	struct a10_mmc_softc *sc;
	struct mmc_data *data;
	uint32_t idst, imask, rint;

	sc = (struct a10_mmc_softc *)arg;
	A10_MMC_LOCK(sc);
	rint = A10_MMC_READ_4(sc, A10_MMC_RINTR);
	idst = A10_MMC_READ_4(sc, A10_MMC_IDST);
	imask = A10_MMC_READ_4(sc, A10_MMC_IMASK);
	if (idst == 0 && imask == 0 && rint == 0) {
		A10_MMC_UNLOCK(sc);
		return;
	}
#ifdef DEBUG
	device_printf(sc->a10_dev, "idst: %#x, imask: %#x, rint: %#x\n",
	    idst, imask, rint);
#endif
	if (sc->a10_req == NULL) {
		device_printf(sc->a10_dev,
		    "Spurious interrupt - no active request, rint: 0x%08X\n",
		    rint);
		goto end;
	}
	if (rint & A10_MMC_INT_ERR_BIT) {
		device_printf(sc->a10_dev, "error rint: 0x%08X\n", rint);
		if (rint & A10_MMC_RESP_TIMEOUT)
			sc->a10_req->cmd->error = MMC_ERR_TIMEOUT;
		else
			sc->a10_req->cmd->error = MMC_ERR_FAILED;
		a10_mmc_req_done(sc);
		goto end;
	}
	if (idst & A10_MMC_IDMAC_ERROR) {
		device_printf(sc->a10_dev, "error idst: 0x%08x\n", idst);
		sc->a10_req->cmd->error = MMC_ERR_FAILED;
		a10_mmc_req_done(sc);
		goto end;
	}

	sc->a10_intr |= rint;
	data = sc->a10_req->cmd->data;
	if (data != NULL && sc->a10_dma_inuse == 1 &&
	    (idst & A10_MMC_IDMAC_COMPLETE)) {
		if (data->flags & MMC_DATA_WRITE)
			sync_op = BUS_DMASYNC_POSTWRITE;
		else
			sync_op = BUS_DMASYNC_POSTREAD;
		bus_dmamap_sync(sc->a10_dma_buf_tag, sc->a10_dma_buf_map,
		    sync_op);
		bus_dmamap_sync(sc->a10_dma_tag, sc->a10_dma_map,
		    BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->a10_dma_buf_tag, sc->a10_dma_buf_map);
		sc->a10_resid = data->len >> 2;
	} else if (data != NULL && sc->a10_dma_inuse == 0 &&
	    (rint & (A10_MMC_DATA_OVER | A10_MMC_RX_DATA_REQ |
	    A10_MMC_TX_DATA_REQ)) != 0)
		a10_mmc_pio_transfer(sc, data);
	if ((sc->a10_intr & sc->a10_intr_wait) == sc->a10_intr_wait)
		a10_mmc_req_ok(sc);

end:
	A10_MMC_WRITE_4(sc, A10_MMC_IDST, idst);
	A10_MMC_WRITE_4(sc, A10_MMC_RINTR, rint);
	A10_MMC_UNLOCK(sc);
}

static int
a10_mmc_request(device_t bus, device_t child, struct mmc_request *req)
{
	int blksz;
	struct a10_mmc_softc *sc;
	struct mmc_command *cmd;
	uint32_t cmdreg, val;

	sc = device_get_softc(bus);
	A10_MMC_LOCK(sc);
	if (sc->a10_req) {
		A10_MMC_UNLOCK(sc);
		return (EBUSY);
	}
	sc->a10_req = req;
	cmd = req->cmd;
	cmdreg = A10_MMC_START;
	if (cmd->opcode == MMC_GO_IDLE_STATE)
		cmdreg |= A10_MMC_SEND_INIT_SEQ;
	if (cmd->flags & MMC_RSP_PRESENT)
		cmdreg |= A10_MMC_RESP_EXP;
	if (cmd->flags & MMC_RSP_136)
		cmdreg |= A10_MMC_LONG_RESP;
	if (cmd->flags & MMC_RSP_CRC)
		cmdreg |= A10_MMC_CHECK_RESP_CRC;

	sc->a10_intr = 0;
	sc->a10_resid = 0;
	sc->a10_intr_wait = A10_MMC_CMD_DONE;
	cmd->error = MMC_ERR_NONE;
	if (cmd->data != NULL) {
		sc->a10_intr_wait |= A10_MMC_DATA_OVER;
		cmdreg |= A10_MMC_DATA_EXP | A10_MMC_WAIT_PREOVER;
		if (cmd->data->flags & MMC_DATA_MULTI) {
			cmdreg |= A10_MMC_SEND_AUTOSTOP;
			sc->a10_intr_wait |= A10_MMC_AUTOCMD_DONE;
		}
		if (cmd->data->flags & MMC_DATA_WRITE)
			cmdreg |= A10_MMC_WRITE;
		blksz = min(cmd->data->len, MMC_SECTOR_SIZE);
		A10_MMC_WRITE_4(sc, A10_MMC_BLKSZ, blksz);
		A10_MMC_WRITE_4(sc, A10_MMC_BCNTR, cmd->data->len);

		if (a10_mmc_pio_mode == 0)
			a10_mmc_prepare_dma(sc);
		/* Enable PIO access if sc->a10_dma_inuse is not set. */
		if (sc->a10_dma_inuse == 0) {
			val = A10_MMC_READ_4(sc, A10_MMC_GCTRL);
			val &= ~A10_MMC_DMA_ENABLE;
			val |= A10_MMC_ACCESS_BY_AHB;
			A10_MMC_WRITE_4(sc, A10_MMC_GCTRL, val);
			val = A10_MMC_READ_4(sc, A10_MMC_IMASK);
			val |= A10_MMC_RX_DATA_REQ | A10_MMC_TX_DATA_REQ;
			A10_MMC_WRITE_4(sc, A10_MMC_IMASK, val);
		}
	}

	A10_MMC_WRITE_4(sc, A10_MMC_CARG, cmd->arg);
	A10_MMC_WRITE_4(sc, A10_MMC_CMDR, cmdreg | cmd->opcode);
	callout_reset(&sc->a10_timeoutc, sc->a10_timeout * hz,
	    a10_mmc_timeout, sc);
	A10_MMC_UNLOCK(sc);

	return (0);
}

static int
a10_mmc_read_ivar(device_t bus, device_t child, int which,
    uintptr_t *result)
{
	struct a10_mmc_softc *sc;

	sc = device_get_softc(bus);
	switch (which) {
	default:
		return (EINVAL);
	case MMCBR_IVAR_BUS_MODE:
		*(int *)result = sc->a10_host.ios.bus_mode;
		break;
	case MMCBR_IVAR_BUS_WIDTH:
		*(int *)result = sc->a10_host.ios.bus_width;
		break;
	case MMCBR_IVAR_CHIP_SELECT:
		*(int *)result = sc->a10_host.ios.chip_select;
		break;
	case MMCBR_IVAR_CLOCK:
		*(int *)result = sc->a10_host.ios.clock;
		break;
	case MMCBR_IVAR_F_MIN:
		*(int *)result = sc->a10_host.f_min;
		break;
	case MMCBR_IVAR_F_MAX:
		*(int *)result = sc->a10_host.f_max;
		break;
	case MMCBR_IVAR_HOST_OCR:
		*(int *)result = sc->a10_host.host_ocr;
		break;
	case MMCBR_IVAR_MODE:
		*(int *)result = sc->a10_host.mode;
		break;
	case MMCBR_IVAR_OCR:
		*(int *)result = sc->a10_host.ocr;
		break;
	case MMCBR_IVAR_POWER_MODE:
		*(int *)result = sc->a10_host.ios.power_mode;
		break;
	case MMCBR_IVAR_VDD:
		*(int *)result = sc->a10_host.ios.vdd;
		break;
	case MMCBR_IVAR_CAPS:
		*(int *)result = sc->a10_host.caps;
		break;
	case MMCBR_IVAR_MAX_DATA:
		*(int *)result = 65535;
		break;
	}

	return (0);
}

static int
a10_mmc_write_ivar(device_t bus, device_t child, int which,
    uintptr_t value)
{
	struct a10_mmc_softc *sc;

	sc = device_get_softc(bus);
	switch (which) {
	default:
		return (EINVAL);
	case MMCBR_IVAR_BUS_MODE:
		sc->a10_host.ios.bus_mode = value;
		break;
	case MMCBR_IVAR_BUS_WIDTH:
		sc->a10_host.ios.bus_width = value;
		break;
	case MMCBR_IVAR_CHIP_SELECT:
		sc->a10_host.ios.chip_select = value;
		break;
	case MMCBR_IVAR_CLOCK:
		sc->a10_host.ios.clock = value;
		break;
	case MMCBR_IVAR_MODE:
		sc->a10_host.mode = value;
		break;
	case MMCBR_IVAR_OCR:
		sc->a10_host.ocr = value;
		break;
	case MMCBR_IVAR_POWER_MODE:
		sc->a10_host.ios.power_mode = value;
		break;
	case MMCBR_IVAR_VDD:
		sc->a10_host.ios.vdd = value;
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
a10_mmc_update_clock(struct a10_mmc_softc *sc)
{
	uint32_t cmdreg;
	int retry;

	cmdreg = A10_MMC_START | A10_MMC_UPCLK_ONLY |
	    A10_MMC_WAIT_PREOVER;
	A10_MMC_WRITE_4(sc, A10_MMC_CMDR, cmdreg);
	retry = 0xfffff;
	while (--retry > 0) {
		if ((A10_MMC_READ_4(sc, A10_MMC_CMDR) & A10_MMC_START) == 0) {
			A10_MMC_WRITE_4(sc, A10_MMC_RINTR, 0xffffffff);
			return (0);
		}
		DELAY(10);
	}
	A10_MMC_WRITE_4(sc, A10_MMC_RINTR, 0xffffffff);
	device_printf(sc->a10_dev, "timeout updating clock\n");

	return (ETIMEDOUT);
}

static int
a10_mmc_update_ios(device_t bus, device_t child)
{
	int error;
	struct a10_mmc_softc *sc;
	struct mmc_ios *ios;
	uint32_t clkcr;

	sc = device_get_softc(bus);
	clkcr = A10_MMC_READ_4(sc, A10_MMC_CLKCR);
	if (clkcr & A10_MMC_CARD_CLK_ON) {
		/* Disable clock. */
		clkcr &= ~A10_MMC_CARD_CLK_ON;
		A10_MMC_WRITE_4(sc, A10_MMC_CLKCR, clkcr);
		error = a10_mmc_update_clock(sc);
		if (error != 0)
			return (error);
	}

	ios = &sc->a10_host.ios;
	if (ios->clock) {
		/* Reset the divider. */
		clkcr &= ~A10_MMC_CLKCR_DIV;
		A10_MMC_WRITE_4(sc, A10_MMC_CLKCR, clkcr);
		error = a10_mmc_update_clock(sc);
		if (error != 0)
			return (error);

		/* Set the MMC clock. */
		error = clk_set_freq(sc->a10_clk_mmc, ios->clock,
		    CLK_SET_ROUND_DOWN);
		if (error != 0) {
			device_printf(sc->a10_dev,
			    "failed to set frequency to %u Hz: %d\n",
			    ios->clock, error);
			return (error);
		}

		/* Enable clock. */
		clkcr |= A10_MMC_CARD_CLK_ON;
		A10_MMC_WRITE_4(sc, A10_MMC_CLKCR, clkcr);
		error = a10_mmc_update_clock(sc);
		if (error != 0)
			return (error);
	}

	/* Set the bus width. */
	switch (ios->bus_width) {
	case bus_width_1:
		A10_MMC_WRITE_4(sc, A10_MMC_WIDTH, A10_MMC_WIDTH1);
		break;
	case bus_width_4:
		A10_MMC_WRITE_4(sc, A10_MMC_WIDTH, A10_MMC_WIDTH4);
		break;
	case bus_width_8:
		A10_MMC_WRITE_4(sc, A10_MMC_WIDTH, A10_MMC_WIDTH8);
		break;
	}

	return (0);
}

static int
a10_mmc_get_ro(device_t bus, device_t child)
{

	return (0);
}

static int
a10_mmc_acquire_host(device_t bus, device_t child)
{
	struct a10_mmc_softc *sc;
	int error;

	sc = device_get_softc(bus);
	A10_MMC_LOCK(sc);
	while (sc->a10_bus_busy) {
		error = msleep(sc, &sc->a10_mtx, PCATCH, "mmchw", 0);
		if (error != 0) {
			A10_MMC_UNLOCK(sc);
			return (error);
		}
	}
	sc->a10_bus_busy++;
	A10_MMC_UNLOCK(sc);

	return (0);
}

static int
a10_mmc_release_host(device_t bus, device_t child)
{
	struct a10_mmc_softc *sc;

	sc = device_get_softc(bus);
	A10_MMC_LOCK(sc);
	sc->a10_bus_busy--;
	wakeup(sc);
	A10_MMC_UNLOCK(sc);

	return (0);
}

static device_method_t a10_mmc_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		a10_mmc_probe),
	DEVMETHOD(device_attach,	a10_mmc_attach),
	DEVMETHOD(device_detach,	a10_mmc_detach),

	/* Bus interface */
	DEVMETHOD(bus_read_ivar,	a10_mmc_read_ivar),
	DEVMETHOD(bus_write_ivar,	a10_mmc_write_ivar),
	DEVMETHOD(bus_print_child,	bus_generic_print_child),

	/* MMC bridge interface */
	DEVMETHOD(mmcbr_update_ios,	a10_mmc_update_ios),
	DEVMETHOD(mmcbr_request,	a10_mmc_request),
	DEVMETHOD(mmcbr_get_ro,		a10_mmc_get_ro),
	DEVMETHOD(mmcbr_acquire_host,	a10_mmc_acquire_host),
	DEVMETHOD(mmcbr_release_host,	a10_mmc_release_host),

	DEVMETHOD_END
};

static devclass_t a10_mmc_devclass;

static driver_t a10_mmc_driver = {
	"a10_mmc",
	a10_mmc_methods,
	sizeof(struct a10_mmc_softc),
};

DRIVER_MODULE(a10_mmc, simplebus, a10_mmc_driver, a10_mmc_devclass, 0, 0);
DRIVER_MODULE(mmc, a10_mmc, mmc_driver, mmc_devclass, NULL, NULL);
MODULE_DEPEND(a10_mmc, mmc, 1, 1, 1);
