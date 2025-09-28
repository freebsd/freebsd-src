/*-
* Copyright (c) 2019 - 2025 Priit Trees <trees@neti.ee>
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
#include <sys/queue.h>
#include <sys/taskqueue.h>

#include <machine/bus.h>

#include <dev/clk/clk.h>
#include <dev/regulator/regulator.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/mmc/bridge.h>
#include <dev/mmc/mmcreg.h>
#include <dev/mmc/mmcbrvar.h>
#include <dev/mmc/mmc_fdt_helpers.h>

#include <arm64/mediatek/mt_mmc.h>

#undef MTK_MMC_DEBUG

#define	MTK_MMC_MEMRES		0
#define	MTK_MMC_IRQRES		1
#define	MTK_MMC_RESSZ		2
#define	MTK_MMC_MAX_GPD		(1 + 1)	/* one null gpd */
#define	MTK_MMC_MAX_BD		1024
#define	MTK_MSC_DMA_MAX_SIZE	(64 * 1024 - MMC_SECTOR_SIZE)
#define	MTK_MMC_DEFAULT_DTOC	40	/* data timeout counter. 65536x40 sclk. */

#define MTK_MSC_INT_ERR_BITS	(MTK_MSDC_INT_SDRCRCER | MTK_MSDC_INT_SDACDRCRCER | \
			       MTK_MSDC_INT_SDCTO | MTK_MSDC_INT_SDACDCTO | \
			       MTK_MSDC_INT_SDDCRCERR | MTK_MSDC_INT_SDDTO)

struct mt_mmc_softc {
       device_t		sc_dev;
       clk_t			sclk;
       clk_t			hclk;

       int			sc_bus_busy;
       int			sc_resid;
       int			sc_timeout;
       struct callout		sc_timeoutc;
       struct mmc_host		sc_host;
       struct mmc_fdt_helper	mmc_helper;

       struct mmc_request *	sc_req;

       struct mtx		sc_mtx;
       struct resource *	sc_res[MTK_MMC_RESSZ];

       uint32_t		sc_intr_mask;
       uint32_t		sc_intr_wait;
       void *			sc_intrhand;
       uint32_t		sc_intr_seen;
       uint32_t		socid;
       int			sc_clock;
       device_t		child;

       /* Fields required for DMA access. */
       bus_addr_t		sc_dma_gpd_addr;
       bus_dmamap_t		sc_dma_gpd_map;
       bus_dma_tag_t		sc_dma_gpd_tag;
       void *			sc_dma_gpd;

       bus_addr_t		sc_dma_bd_addr;
       bus_dma_tag_t		sc_dma_bd_tag;
       bus_dmamap_t		sc_dma_bd_map;
       void *			sc_dma_bd;
       bus_dma_tag_t		sc_dma_buf_tag;
       bus_dmamap_t		sc_dma_buf_map;
       int			sc_dma_map_err;

       uint32_t		sc_dma_ctl;
};

static struct resource_spec mtk_mmc_res_spec[] = {
       { SYS_RES_MEMORY,	0,	RF_ACTIVE },
       { SYS_RES_IRQ,		0,	RF_ACTIVE | RF_SHAREABLE },
       { -1,			0,	0 }
};

static int mtk_mmc_probe(device_t);
static int mtk_mmc_attach(device_t);
static int mtk_mmc_detach(device_t);
static int mtk_mmc_setup_dma(struct mt_mmc_softc *);
static void mtk_mmc_teardown_dma(struct mt_mmc_softc *sc);
static int mtk_mmc_reset(struct mt_mmc_softc *);

static void mtk_mmc_intr(void *);
static int mtk_mmc_config_clock(struct mt_mmc_softc *, uint32_t);
static void mtk_mmc_helper_cd_handler(device_t, bool);

static int mtk_mmc_update_ios(device_t, device_t);
static int mtk_mmc_request(device_t, device_t, struct mmc_request *);
static int mtk_mmc_get_ro(device_t, device_t);
static int mtk_mmc_acquire_host(device_t, device_t);
static int mtk_mmc_release_host(device_t, device_t);

#define	MTK_MMC_LOCK(_sc)	mtx_lock(&(_sc)->sc_mtx)
#define	MTK_MMC_UNLOCK(_sc)	mtx_unlock(&(_sc)->sc_mtx)
#define	MTK_MMC_READ_4(_sc, _reg)					\
       bus_read_4((_sc)->sc_res[MTK_MMC_MEMRES], _reg)
#define MTK_MMC_WRITE_4(_sc, _reg, _value)				\
       bus_write_4((_sc)->sc_res[MTK_MMC_MEMRES], _reg, _value)

static struct ofw_compat_data compat_data[] = {
       { "mediatek,mt7623-mmc",	1},
       { "mediatek,mt7622-mmc",	1},
       { NULL,                         0}
};

static int
mtk_mmc_probe(device_t dev)
{
       struct mt_mmc_softc *sc = device_get_softc(dev);

       if (!ofw_bus_status_okay(dev))
	       return (ENXIO);

       sc->socid = ofw_bus_search_compatible(dev, compat_data)->ocd_data;
       if (sc->socid == 0)
	       return (ENXIO);

       device_set_desc(dev, "Mediatek MMC/SD controller");

       return (0);
}

static int
mtk_mmc_attach(device_t dev)
{
       struct mt_mmc_softc *sc;
       struct sysctl_ctx_list *ctx;
       struct sysctl_oid_list *tree;
       uint32_t val;
       int error;

       sc = device_get_softc(dev);
       sc->sc_dev = dev;

       sc->sc_req = NULL;

       if (bus_alloc_resources(dev, mtk_mmc_res_spec, sc->sc_res) != 0) {
	       device_printf(dev, "cannot allocate device resources\n");
	       return (ENXIO);
       }

       if (bus_setup_intr(dev, sc->sc_res[MTK_MMC_IRQRES],
	       INTR_TYPE_MISC | INTR_MPSAFE, NULL, mtk_mmc_intr, sc,
	       &sc->sc_intrhand)) {
	       bus_release_resources(dev, mtk_mmc_res_spec, sc->sc_res);
	       device_printf(dev, "cannot setup interrupt handler\n");
	       return (ENXIO);
       }

       //	val = MTK_MMC_READ_4(sc, MTK_SDC_CFG);
       //	device_printf(sc->sc_dev, "%s MTK_SDC_CFG 0x%x \n", __func__, val);
       //
       //	val = MTK_MMC_READ_4(sc, MTK_MSDC_CFG);
       //	device_printf(sc->sc_dev, "%s MTK_MSDC_CFG 0x%x \n", __func__, val);

       mtx_init(&sc->sc_mtx, device_get_nameunit(sc->sc_dev), "mtk_mmc",
	   MTX_DEF);
       callout_init_mtx(&sc->sc_timeoutc, &sc->sc_mtx, 0);

       sc->sc_timeout = 10;
       ctx = device_get_sysctl_ctx(dev);
       tree = SYSCTL_CHILDREN(device_get_sysctl_tree(dev));
       SYSCTL_ADD_INT(ctx, tree, OID_AUTO, "req_timeout", CTLFLAG_RW,
	   &sc->sc_timeout, 0, "Request timeout in seconds");

       /* Setup DMA */
       if (mtk_mmc_setup_dma(sc) != 0) {
	       device_printf(dev, "Couldn't setup DMA!\n");
	       goto fail;
       }

       /* Set some default for freq and supported mode*/
       if (clk_get_by_ofw_name(dev, 0, "source", &sc->sclk)) {
	       device_printf(dev, "cannot get clock\n");
	       goto fail;
       }

       if (clk_get_by_ofw_name(dev, 0, "hclk", &sc->hclk)) {
	       device_printf(dev, "cannot get clock\n");
	       goto fail;
       }

       sc->sc_host.f_max = 25000000;
       sc->sc_host.f_min = 260000;
       sc->sc_host.host_ocr = MMC_OCR_320_330 | MMC_OCR_330_340;
       sc->sc_host.caps = MMC_CAP_HSPEED | MMC_CAP_SIGNALING_330;
       sc->sc_clock = 0;

       /* Set to MMC/SD mode */
       val = MTK_MMC_READ_4(sc, MTK_MSDC_CFG);
       val |= MTK_MSDC_CFG_MSDC;
       MTK_MMC_WRITE_4(sc, MTK_MSDC_CFG, val);

       /* Reset controller. */
       if (mtk_mmc_reset(sc) != 0) {
	       device_printf(dev, "cannot reset the controller\n");
	       goto fail;
       }

       /* Configure to default data timeout */
       val = MTK_MMC_READ_4(sc, MTK_SDC_CFG);
       val &= ~MTK_SDC_CFG_DTOC_MASK;
       val |= (MTK_MMC_DEFAULT_DTOC << MTK_SDC_CFG_DTOC_SHIFT)
	   & MTK_SDC_CFG_DTOC_MASK;
       MTK_MMC_WRITE_4(sc, MTK_SDC_CFG, val);

       /* write crc timeout detection */
       val = MTK_MMC_READ_4(sc, MTK_MSDC_PATCH_BIT0);
       val |= MTK_MSDC_PATCH_BIT0_PTCH30;
       MTK_MMC_WRITE_4(sc, MTK_MSDC_PATCH_BIT0, val);

       error = clk_enable(sc->sclk);
       if (error) {
	       device_printf(sc->sc_dev, "cannot enable sourse clock\n");
	       goto fail;
       }

       if (sc->child == NULL) {
	       sc->child = device_add_child(sc->sc_dev, "mmc", -1);
	       if (sc->child) {
		       device_set_ivars(sc->child, sc);
		       (void)device_probe_and_attach(sc->child);
	       }
       }
       mmc_fdt_parse(dev, 0, &sc->mmc_helper, &sc->sc_host);
       mmc_fdt_gpio_setup(dev, 0, &sc->mmc_helper, mtk_mmc_helper_cd_handler);
       return (0);

fail:
       callout_drain(&sc->sc_timeoutc);
       mtx_destroy(&sc->sc_mtx);
       bus_teardown_intr(dev, sc->sc_res[MTK_MMC_IRQRES], sc->sc_intrhand);
       bus_release_resources(dev, mtk_mmc_res_spec, sc->sc_res);
       return (ENXIO);
}

static int
mtk_mmc_detach(device_t dev)
{
       struct mt_mmc_softc *sc;
       device_t d;

       sc= device_get_softc(dev);

       mmc_fdt_gpio_teardown(&sc->mmc_helper);

       callout_drain(&sc->sc_timeoutc);

       MTK_MMC_LOCK(sc);
       d = sc->child;
       sc->child = NULL;
       MTK_MMC_UNLOCK(sc);
       device_delete_child(sc->sc_dev, d);

       mtk_mmc_teardown_dma(sc);

       mtx_destroy(&sc->sc_mtx);

       bus_teardown_intr(dev, sc->sc_res[MTK_MMC_IRQRES], sc->sc_intrhand);
       bus_release_resources(dev, mtk_mmc_res_spec, sc->sc_res);
       return (0);
}

static void
mtk_mmc_dma_gpd_cb(void *arg, bus_dma_segment_t *segs, int nsegs, int err)
{
       struct mt_mmc_softc *sc;

       sc = (struct mt_mmc_softc *)arg;
       if (err) {
	       sc->sc_dma_map_err = err;
	       return;
       }
       sc->sc_dma_gpd_addr = segs[0].ds_addr;
}

static void
mtk_mmc_dma_bd_cb(void *arg, bus_dma_segment_t *segs, int nsegs, int err)
{
       struct mt_mmc_softc *sc;

       sc = (struct mt_mmc_softc *)arg;
       if (err) {
	       sc->sc_dma_map_err = err;
	       return;
       }
       sc->sc_dma_bd_addr = segs[0].ds_addr;
}

static int
mtk_mmc_setup_dma(struct mt_mmc_softc *sc)
{
       int dma_gpd_size, dma_bd_size;
       int error;

       /* Allocate the DMA General Packet Descriptor memory. */
       dma_gpd_size = sizeof(struct mtk_mmc_dma_gpd) * MTK_MMC_MAX_GPD;
       error = bus_dma_tag_create(
	   bus_get_dma_tag(sc->sc_dev),	/* parent */
	   1, 0,				/* align, boundary */
	   BUS_SPACE_MAXADDR_32BIT,		/* lowaddr */
	   BUS_SPACE_MAXADDR,			/* highaddr */
	   NULL, NULL,				/* filter, filterarg*/
	   dma_gpd_size, 1,			/* maxsize, nsegment */
	   dma_gpd_size,			/* maxsegsize */
	   0,					/* flags */
	   NULL, NULL,				/* lock, lockarg*/
	   &sc->sc_dma_gpd_tag);
       if (error)
	       return (error);

       error = bus_dmamem_alloc(sc->sc_dma_gpd_tag, &sc->sc_dma_gpd,
	   BUS_DMA_COHERENT | BUS_DMA_WAITOK | BUS_DMA_ZERO,
	   &sc->sc_dma_gpd_map);
       if (error)
	       return (error);

       error = bus_dmamap_load(sc->sc_dma_gpd_tag,
	   sc->sc_dma_gpd_map,
	   sc->sc_dma_gpd, dma_gpd_size,
	   mtk_mmc_dma_gpd_cb, sc, 0);
       if (error)
	       return (error);
       if (sc->sc_dma_map_err)
	       return (sc->sc_dma_map_err);

       /* Allocate the DMA Buffer Descriptor memory. */
       dma_bd_size = sizeof(struct mtk_mmc_dma_bd) * MTK_MMC_MAX_BD;
       error = bus_dma_tag_create(
	   bus_get_dma_tag(sc->sc_dev),	/* parent */
	   1, 0,				/* align, boundary */
	   BUS_SPACE_MAXADDR_32BIT,		/* lowaddr */
	   BUS_SPACE_MAXADDR,			/* highaddr */
	   NULL, NULL,				/* filter, filterarg*/
	   dma_bd_size, 1,			/* maxsize, nsegment */
	   dma_bd_size,			/* maxsegsize */
	   0,					/* flags */
	   NULL, NULL,				/* lock, lockarg*/
	   &sc->sc_dma_bd_tag);
       if (error)
	       return (error);

       error = bus_dmamem_alloc(sc->sc_dma_bd_tag, &sc->sc_dma_bd,
	   BUS_DMA_COHERENT | BUS_DMA_WAITOK | BUS_DMA_ZERO,
	   &sc->sc_dma_bd_map);
       if (error)
	       return (error);

       error = bus_dmamap_load(sc->sc_dma_bd_tag,
	   sc->sc_dma_bd_map,
	   sc->sc_dma_bd, dma_bd_size,
	   mtk_mmc_dma_bd_cb, sc, 0);
       if (error)
	       return (error);
       if (sc->sc_dma_map_err)
	       return (sc->sc_dma_map_err);

       /* Create the DMA map for data transfers. */
       error = bus_dma_tag_create(
	   bus_get_dma_tag(sc->sc_dev),	/* parent */
	   1, 0,				/* align, boundary */
	   BUS_SPACE_MAXADDR_32BIT,		/* lowaddr */
	   BUS_SPACE_MAXADDR,			/* highaddr */
	   NULL, NULL,				/* filter, filterarg*/
	   MTK_MSC_DMA_MAX_SIZE *
	       MTK_MMC_MAX_BD, 			/* maxsize */
	   MTK_MMC_MAX_BD,			/* nsegments */
	   MTK_MSC_DMA_MAX_SIZE,		/* maxsegsize */
	   BUS_DMA_ALLOCNOW,			/* flags */
	   NULL, NULL,				/* lock, lockarg*/
	   &sc->sc_dma_buf_tag);
       if (error)
	       return (error);
       error = bus_dmamap_create(sc->sc_dma_buf_tag, 0,
	   &sc->sc_dma_buf_map);
       if (error)
	       return (error);

       return (0);
}

static void
mtk_mmc_teardown_dma(struct mt_mmc_softc *sc)
{
       bus_dmamap_unload(sc->sc_dma_gpd_tag, sc->sc_dma_gpd_map);
       bus_dmamem_free(sc->sc_dma_gpd_tag, sc->sc_dma_gpd, sc->sc_dma_gpd_map);
       if (bus_dma_tag_destroy(sc->sc_dma_gpd_tag) != 0)
	       device_printf(sc->sc_dev, "Cannot destroy the dma gpd tag\n");

       bus_dmamap_unload(sc->sc_dma_bd_tag, sc->sc_dma_bd_map);
       bus_dmamem_free(sc->sc_dma_bd_tag, sc->sc_dma_bd, sc->sc_dma_bd_map);
       if (bus_dma_tag_destroy(sc->sc_dma_bd_tag) != 0)
	       device_printf(sc->sc_dev, "Cannot destroy the dma bd tag\n");

       bus_dmamap_unload(sc->sc_dma_buf_tag, sc->sc_dma_buf_map);
       bus_dmamap_destroy(sc->sc_dma_buf_tag, sc->sc_dma_buf_map);
       if (bus_dma_tag_destroy(sc->sc_dma_buf_tag) != 0)
	       device_printf(sc->sc_dev, "Cannot destroy the dma buf tag\n");
}

static uint8_t
mtk_mmc_chksum_calcs(uint8_t *buf, uint32_t len)
{
       uint32_t i, sum = 0;

       for (i = 0; i < len; i++)
	       sum += buf[i];

       return (0xFF - (uint8_t)sum);
}

static void
mtk_mmc_dma_cb(void *arg, bus_dma_segment_t *segs, int nsegs, int err)
{
       struct mt_mmc_softc *sc;
       struct mtk_mmc_dma_gpd *dma_gpd;
       struct mtk_mmc_dma_bd *dma_bd;
       uint32_t dma_bd_addr;
       int i;

       sc = (struct mt_mmc_softc *)arg;
       sc->sc_dma_map_err = err;

       if (err)
	       return;

       dma_bd = sc->sc_dma_bd;
       dma_bd_addr = sc->sc_dma_bd_addr;

       for (i = 0; i < nsegs; i++) {
	       dma_bd[i].bd_cfg2 = 0;
	       dma_bd_addr += sizeof(struct mtk_mmc_dma_bd);
	       dma_bd[i].next_bd = dma_bd_addr;
	       dma_bd[i].buf_addr = segs[i].ds_addr;
	       dma_bd[i].buf_len = segs[i].ds_len;
	       if (i == nsegs - 1) {
		       dma_bd[i].bd_cfg1 = MTK_BD_EOL;	/* setup the last */
	       } else {
		       dma_bd[i].bd_cfg1 = 0;
	       }
	       dma_bd[i].bd_chksum = 0;	/* checksume need to clear first */
	       dma_bd[i].bd_chksum = mtk_mmc_chksum_calcs(
		   (uint8_t *)(&dma_bd[i]), 16);
       }

       dma_gpd = sc->sc_dma_gpd;
       dma_gpd->gpd_cfg2 = 0;
       dma_gpd->gpd_cfg1 = MTK_GPD_HWO | MTK_GPD_BDP;	/* hw will clear HWO */
       dma_gpd->next_gpd = sc->sc_dma_gpd_addr
	   + sizeof(struct mtk_mmc_dma_gpd);
       dma_gpd->buf_addr = sc->sc_dma_bd_addr;
       dma_gpd->gpd_chksum = 0;	/* checksume need to clear first. */
       dma_gpd->gpd_chksum = mtk_mmc_chksum_calcs((uint8_t *)dma_gpd, 16);
}

static int
mtk_mmc_prepare_dma(struct mt_mmc_softc *sc)
{
       bus_dmasync_op_t sync_op;
       int error;
       struct mmc_command *cmd;
       uint32_t val;

       cmd = sc->sc_req->cmd;
       if (cmd->data->len > MTK_MSC_DMA_MAX_SIZE * MTK_MMC_MAX_BD)
	       return (EFBIG);
       error = bus_dmamap_load(sc->sc_dma_buf_tag, sc->sc_dma_buf_map,
	   cmd->data->data, cmd->data->len, mtk_mmc_dma_cb, sc,
	   BUS_DMA_NOWAIT);
       if (error)
	       return (error);
       if (sc->sc_dma_map_err)
	       return (sc->sc_dma_map_err);

       if (cmd->data->flags & MMC_DATA_WRITE)
	       sync_op = BUS_DMASYNC_PREWRITE;
       else
	       sync_op = BUS_DMASYNC_PREREAD;
       bus_dmamap_sync(sc->sc_dma_buf_tag, sc->sc_dma_buf_map, sync_op);
       bus_dmamap_sync(sc->sc_dma_bd_tag, sc->sc_dma_bd_map, BUS_DMASYNC_PREWRITE);

       /* Setup default DMA parameters for Descriptor DMA mode */
       sc->sc_dma_ctl = MTK_MSDC_DMA_CTRL_DMAMOD |
	   MTK_MSDC_DMA_CTRL_BSTSZ_64B | MTK_MSDC_DMA_CTRL_DMASTART;

       /* CLear POI mode */
       val = MTK_MMC_READ_4(sc, MTK_MSDC_CFG);
       val &= ~MTK_MSDC_CFG_PIO;
       MTK_MMC_WRITE_4(sc, MTK_MSDC_CFG, val);

       /* Enambe Description DMA mode */
       val = MTK_MMC_READ_4(sc, MTK_MSDC_DMA_CFG);
       val |= MTK_MSDC_DMA_CFG_DSCPCSEN;
       MTK_MMC_WRITE_4(sc, MTK_MSDC_DMA_CFG, val);

       return (0);
}

static void
mtk_mmc_start_dma(struct mt_mmc_softc *sc)
{
       /* Set the address of the first descriptor */
       MTK_MMC_WRITE_4(sc, MTK_MSDC_DMA_SA, sc->sc_dma_gpd_addr);
       /* Enable and start the dma engine */
       MTK_MMC_WRITE_4(sc, MTK_MSDC_DMA_CTRL, sc->sc_dma_ctl);
}

static int
mtk_mmc_reset(struct mt_mmc_softc *sc)
{
       int timeout;
       uint32_t val;

       /* Reset */
       val = MTK_MMC_READ_4(sc, MTK_MSDC_CFG);
       val |= MTK_MSDC_CFG_RST;
       MTK_MMC_WRITE_4(sc, MTK_MSDC_CFG, val);
       timeout = 100;
       while (--timeout > 0) {
	       if ((MTK_MMC_READ_4(sc, MTK_MSDC_CFG) & MTK_MSDC_CFG_RST) == 0)
		       break;
	       DELAY(1000);
       }
       if (timeout == 0)
	       return (ETIMEDOUT);

       /* Clear FIFO */
       val = MTK_MMC_READ_4(sc, MTK_MSDC_FIFOCS);
       val |= MTK_MSDC_FIFOCS_FIFOCLR;
       MTK_MMC_WRITE_4(sc, MTK_MSDC_FIFOCS, val);
       timeout = 10;
       while (--timeout > 0) {
	       if ((MTK_MMC_READ_4(sc, MTK_MSDC_FIFOCS) &
		       MTK_MSDC_FIFOCS_FIFOCLR) == 0)
		       break;
	       DELAY(100);
       }
       if (timeout == 0) {
	       return (ETIMEDOUT);
       }

       /* Clear interirput */
       MTK_MMC_WRITE_4(sc, MTK_MSDC_INT, MTK_MMC_READ_4(sc, MTK_MSDC_INT));

       /* Remember interrupts we always want */
       sc->sc_intr_mask = MTK_MSC_INT_ERR_BITS;

       return (0);
}

static void
mtk_mmc_req_done(struct mt_mmc_softc *sc)
{
       struct mmc_command *cmd;
       struct mmc_request *req;

       cmd = sc->sc_req->cmd;
       /* Reset the in case of errors */
       if (cmd->error != MMC_ERR_NONE)
	       mtk_mmc_reset(sc);

       callout_stop(&sc->sc_timeoutc);

       sc->sc_resid = 0;
       sc->sc_dma_map_err = 0;
       sc->sc_intr_wait = 0;

       sc->sc_intr_seen = 0;

       req = sc->sc_req;
       sc->sc_req = NULL;
       req->done(req);
}

static void
mtk_mmc_req_ok(struct mt_mmc_softc *sc)
{
       struct mmc_command *cmd;

       cmd = sc->sc_req->cmd;

       if (cmd->flags & MMC_RSP_PRESENT) {
	       if (cmd->flags & MMC_RSP_136) {
		       cmd->resp[0] = MTK_MMC_READ_4(sc, MTK_SDC_RESP3);
		       cmd->resp[1] = MTK_MMC_READ_4(sc, MTK_SDC_RESP2);
		       cmd->resp[2] = MTK_MMC_READ_4(sc, MTK_SDC_RESP1);
		       cmd->resp[3] = MTK_MMC_READ_4(sc, MTK_SDC_RESP0);
	       } else {
		       cmd->resp[0] = MTK_MMC_READ_4(sc, MTK_SDC_RESP0);
	       }
       }

       /* All data has been transferred ? */
       if (cmd->data != NULL && (sc->sc_resid << 2) < cmd->data->len)
	       cmd->error = MMC_ERR_FAILED;
       mtk_mmc_req_done(sc);
}

static void
mtk_mmc_timeout(void *arg)
{
       struct mt_mmc_softc *sc;

       sc = (struct mt_mmc_softc *)arg;
       if (sc->sc_req != NULL) {
	       device_printf(sc->sc_dev,
		   "controller timeout, msdc_int %#x msdc_inten %#x\n",
		   MTK_MMC_READ_4(sc, MTK_MSDC_INT),
		   MTK_MMC_READ_4(sc, MTK_MSDC_INTEN));
	       sc->sc_req->cmd->error = MMC_ERR_TIMEOUT;
	       mtk_mmc_req_done(sc);
       } else
	       device_printf(sc->sc_dev,
		   "Spurious timeout - no active request\n");
}

static void
mtk_mmc_intr(void *arg)
{
       bus_dmasync_op_t sync_op;
       struct mt_mmc_softc *sc;
       struct mmc_data *data;
       uint32_t rint;

       sc = (struct mt_mmc_softc *)arg;
       MTK_MMC_LOCK(sc);
       rint  = MTK_MMC_READ_4(sc, MTK_MSDC_INT);
#if defined(MTK_MMC_DEBUG)
       device_printf(sc->sc_dev, "rint: %#x, stat: %#x\n",
	   rint, MTK_MMC_READ_4(sc, MTK_MSDC_INTEN));
#endif
       if (sc->sc_req == NULL) {
	       device_printf(sc->sc_dev,
		   "Spurious interrupt - no active request, rint: 0x%08X\n",
		   rint);
	       goto end;
       }
       if (rint & MTK_MSC_INT_ERR_BITS) {
	       device_printf(sc->sc_dev,
		   "controller error, rint %#x stat %#x\n",
		   rint, MTK_MMC_READ_4(sc, MTK_MSDC_INTEN));

	       if (rint & (MTK_MSDC_INT_SDCTO | MTK_MSDC_INT_SDACDCTO |
			      MTK_MSDC_INT_SDDTO))
		       sc->sc_req->cmd->error = MMC_ERR_TIMEOUT;
	       else
		       sc->sc_req->cmd->error = MMC_ERR_FAILED;
	       mtk_mmc_req_done(sc);
	       goto end;
       }

       data = sc->sc_req->cmd->data;

       /* Check for command response */
       if (rint & MTK_MSDC_INT_SDCRDY) {
	       mtk_mmc_start_dma(sc);
       }
       /* Unmap DMA if necessary */
       if (data != NULL && (rint & MTK_MSDC_INT_DMAXFDNE) != 0) {
	       if (data->flags & MMC_DATA_WRITE)
		       sync_op = BUS_DMASYNC_POSTWRITE;
	       else
		       sync_op = BUS_DMASYNC_POSTREAD;
	       bus_dmamap_sync(sc->sc_dma_buf_tag, sc->sc_dma_buf_map,
		   sync_op);
	       bus_dmamap_sync(sc->sc_dma_bd_tag, sc->sc_dma_bd_map,
		   BUS_DMASYNC_POSTWRITE);
	       bus_dmamap_unload(sc->sc_dma_buf_tag, sc->sc_dma_buf_map);
	       sc->sc_resid = data->len >> 2;
       }
       sc->sc_intr_seen |= rint;
       if ((sc->sc_intr_seen & sc->sc_intr_wait) == sc->sc_intr_wait)
	       mtk_mmc_req_ok(sc);
end:
       MTK_MMC_WRITE_4(sc, MTK_MSDC_INT, rint);
       MTK_MMC_UNLOCK(sc);
}

static int
mtk_mmc_request(device_t bus, device_t child, struct mmc_request *req)
{
       struct mt_mmc_softc *sc;
       struct mmc_command *cmd;
       uint32_t iwait, cmdr;
       int blksz, tout = 1000;
       uint32_t val;

       sc = device_get_softc(bus);

       MTK_MMC_LOCK(sc);
       if (sc->sc_req != NULL) {
	       MTK_MMC_UNLOCK(sc);
	       return (EBUSY);
       }

       /* Start with template value */
       sc->sc_req = req;
       cmd = req->cmd;
       cmd->error = MMC_ERR_NONE;
       cmdr = cmd->opcode;
       sc->sc_resid = 0;
       sc->sc_intr_seen = 0;
       iwait = MTK_MSDC_INT_SDCRDY;

       /* Configure response format */
       switch (MMC_RSP(cmd->flags)) {
       case MMC_RSP_R1:
	       cmdr |= MTK_SDC_CMD_RSPTYP_R1;
	       break;
       case MMC_RSP_R1B:
	       cmdr |= MTK_SDC_CMD_RSPTYP_R1B;
	       break;
       case MMC_RSP_R2:
	       cmdr |= MTK_SDC_CMD_RSPTYP_R2;
	       break;
       case MMC_RSP_R3:
	       cmdr |= MTK_SDC_CMD_RSPTYP_R3;
	       break;
       };

       if (cmd->data != NULL) {
	       if (cmd->data->flags & MMC_DATA_MULTI) {
		       cmdr |= MTK_SDC_CMD_ACMD;
		       cmdr |= MTK_SDC_CMD_DTYPE_MULTI;
		       iwait |= MTK_MSDC_INT_SDACDCRDY;
	       }
	       else if(cmd->data->flags & MMC_DATA_STREAM)
	       {
		       cmdr |= MTK_SDC_CMD_DTYPE_STREAM;
		       device_printf(sc->sc_dev,
			   "MMC_DATA_STREAM\n");
	       }
	       else
		       cmdr |= MTK_SDC_CMD_DTYPE_SINGLE;

	       if (cmd->data->flags & MMC_DATA_WRITE)
		       cmdr |= MTK_SDC_CMD_RW;

	       blksz = min(cmd->data->len, MMC_SECTOR_SIZE);
	       cmdr |= blksz << MTK_SDC_CMD_LEN_SHIFT;
	       MTK_MMC_WRITE_4(sc, MTK_SDC_BLK_NUM, (cmd->data->len / blksz));

	       mtk_mmc_prepare_dma(sc);

	       iwait |= MTK_MSDC_INT_SDXFCPL;
	       iwait |= MTK_MSDC_INT_DMAXFDNE;
       }

       if (cmd->opcode == MMC_STOP_TRANSMISSION) {
	       cmdr |= (MTK_SDC_CMD_STOP);
	       cmdr &= ~(0x0FFF << MTK_SDC_CMD_LEN_SHIFT);
       }

       /* Is SD Command line or SD controller busy */
       if (cmd->opcode == MMC_SEND_STATUS) {
	       while (MTK_MMC_READ_4(sc, MTK_SDC_STS) & MTK_SDC_STS_CMDBSY) {
		       if (tout-- < 0) {
			       device_printf(sc->sc_dev,
				   "SD command line busy: before CMD<%u>\n",
				   cmd->opcode);
			       break;
		       }
	       }
       } else {
	       while (MTK_MMC_READ_4(sc, MTK_SDC_STS) & MTK_SDC_STS_SDCBSY) {
		       if (tout-- < 0) {
			       device_printf(sc->sc_dev,
				   "SD controller busy: before CMD<%u>\n",
				   cmd->opcode);
			       break;
		       }
	       }
       }

       if (tout < 0) {
	       cmd->error = ETIMEDOUT;
	       mtk_mmc_timeout(sc);
	       MTK_MMC_UNLOCK(sc);
	       return (0);
       }

       sc->sc_intr_wait = iwait;
       val = MTK_MMC_READ_4(sc, MTK_MSDC_INTEN);
       val |= sc->sc_intr_mask | iwait;
       MTK_MMC_WRITE_4(sc, MTK_MSDC_INTEN, val);

#if defined(MTK_MMC_DEBUG)
       device_printf(sc->sc_dev,
	   "REQUEST: CMD%u arg %#x flags %#x cmdr %#x sc_intr_wait = %#x\n",
	   cmd->opcode, cmd->arg, cmd->flags, cmdr, sc->sc_intr_wait);
#endif

       MTK_MMC_WRITE_4(sc, MTK_SDC_ARG, cmd->arg);
       MTK_MMC_WRITE_4(sc, MTK_SDC_CMD, cmdr);

       callout_reset(&sc->sc_timeoutc, sc->sc_timeout * hz,
	   mtk_mmc_timeout, sc);
       MTK_MMC_UNLOCK(sc);

       return (0);
}

static void
mtk_mmc_helper_cd_handler(device_t dev, bool present)
{
       //	struct mtk_mmc_softc *sc;
       //
       //	sc = device_get_softc(dev);
       //	MTK_MMC_LOCK(sc);
       //	if (present) {
       //		if (sc->child == NULL) {
       //			printf("%s present %i\n", __func__, present);
       //			sc->child = device_add_child(sc->sc_dev, "mmc", -1);
       //			MTK_MMC_UNLOCK(sc);
       //			if (sc->child) {
       //				device_set_ivars(sc->child, sc);
       //				(void)device_probe_and_attach(sc->child);
       //			}
       //		} else
       //			MTK_MMC_UNLOCK(sc);
       //	} else {
       //		/* Card isn't present, detach if necessary */
       //		if (sc->child != NULL) {
       //			MTK_MMC_UNLOCK(sc);
       //			device_delete_child(sc->sc_dev, sc->child);
       //			sc->child = NULL;
       //		} else
       //			MTK_MMC_UNLOCK(sc);
       //	}
}

static int
mtk_mmc_read_ivar(device_t bus, device_t child, int which,
   uintptr_t *result)
{
       struct mt_mmc_softc *sc;

       sc = device_get_softc(bus);
       switch (which) {
       case MMCBR_IVAR_BUS_MODE:
	       *(int *)result = sc->sc_host.ios.bus_mode;
	       break;
       case MMCBR_IVAR_BUS_WIDTH:
	       *(int *)result = sc->sc_host.ios.bus_width;
	       break;
       case MMCBR_IVAR_CHIP_SELECT:
	       *(int *)result = sc->sc_host.ios.chip_select;
	       break;
       case MMCBR_IVAR_CLOCK:
	       *(int *)result = sc->sc_host.ios.clock;
	       break;
       case MMCBR_IVAR_F_MIN:
	       *(int *)result = sc->sc_host.f_min;
	       break;
       case MMCBR_IVAR_F_MAX:
	       *(int *)result = sc->sc_host.f_max;
	       break;
       case MMCBR_IVAR_HOST_OCR:
	       *(int *)result = sc->sc_host.host_ocr;
	       break;
       case MMCBR_IVAR_MODE:
	       *(int *)result = sc->sc_host.mode;
	       break;
       case MMCBR_IVAR_OCR:
	       *(int *)result = sc->sc_host.ocr;
	       break;
       case MMCBR_IVAR_POWER_MODE:
	       *(int *)result = sc->sc_host.ios.power_mode;
	       break;
       case MMCBR_IVAR_VDD:
	       *(int *)result = sc->sc_host.ios.vdd;
	       break;
       case MMCBR_IVAR_VCCQ:
	       *(int *)result = sc->sc_host.ios.vccq;
	       break;
       case MMCBR_IVAR_CAPS:
	       *(int *)result = sc->sc_host.caps;
	       break;
       case MMCBR_IVAR_TIMING:
	       *(int *)result = sc->sc_host.ios.timing;
	       break;
       case MMCBR_IVAR_MAX_DATA:
	       *(int *)result = (MTK_MSC_DMA_MAX_SIZE *
				    MTK_MMC_MAX_BD) / MMC_SECTOR_SIZE;
	       break;
       default:
	       return (EINVAL);
       }

       return (0);
}

static int
mtk_mmc_write_ivar(device_t bus, device_t child, int which,
   uintptr_t value)
{
       struct mt_mmc_softc *sc;

       sc = device_get_softc(bus);
       switch (which) {
       case MMCBR_IVAR_BUS_MODE:
	       sc->sc_host.ios.bus_mode = value;
	       break;
       case MMCBR_IVAR_BUS_WIDTH:
	       sc->sc_host.ios.bus_width = value;
	       break;
       case MMCBR_IVAR_CHIP_SELECT:
	       sc->sc_host.ios.chip_select = value;
	       break;
       case MMCBR_IVAR_CLOCK:
	       sc->sc_host.ios.clock = value;
	       break;
       case MMCBR_IVAR_MODE:
	       sc->sc_host.mode = value;
	       break;
       case MMCBR_IVAR_OCR:
	       sc->sc_host.ocr = value;
	       break;
       case MMCBR_IVAR_POWER_MODE:
	       sc->sc_host.ios.power_mode = value;
	       break;
       case MMCBR_IVAR_VDD:
	       sc->sc_host.ios.vdd = value;
	       break;
       case MMCBR_IVAR_VCCQ:
	       sc->sc_host.ios.vccq = value;
	       break;
       case MMCBR_IVAR_TIMING:
	       sc->sc_host.ios.timing = value;
	       break;
       /* These are read-only */
       case MMCBR_IVAR_CAPS:
       case MMCBR_IVAR_HOST_OCR:
       case MMCBR_IVAR_F_MIN:
       case MMCBR_IVAR_F_MAX:
       case MMCBR_IVAR_MAX_DATA:
       default:
	       return (EINVAL);
       }

       return (0);
}

/* */
static int
mtk_mmc_config_clock(struct mt_mmc_softc *sc, uint32_t freq)
{
       uint32_t mclk;
       uint64_t hclk;
       uint64_t sclk;
       uint32_t div;
       int mode;
       uint32_t val;
       int error;

       clk_get_freq(sc->sclk, &sclk);
       clk_get_freq(sc->hclk, &hclk);

       if (freq >= sclk) {
	       /* Use msdc source clock as bus clock */
	       mode = 1;
	       div  = 0;
	       mclk = sclk;
       } else {
	       /* Use clock divider msdc source clock */
	       mode = 0;
	       if (freq >= (sclk >> 1)) {
		       /* divider 1/2 */
		       div = 0;
		       mclk = sclk >> 1;
	       } else {
		       /* divider 1/(n * 4) n: 1 - 255 */
		       div = (sclk + ((freq << 2) - 1)) / (freq << 2);
		       mclk = (sclk >> 2) / div;
	       }
       }

       device_printf(sc->sc_dev, "%s, mclk %i sclk %lu hclk %lu div %i mode %i\n",
	   __func__, mclk, sclk, hclk, div, mode);

       val = MTK_MMC_READ_4(sc, MTK_MSDC_CFG);
       val &= ~MTK_MSDC_CFG_CCKPD;
       MTK_MMC_WRITE_4(sc, MTK_MSDC_CFG, val);

       error = clk_disable(sc->sclk);
       if (error)
	       device_printf(sc->sc_dev, "cannot didsble mmc clock\n");

       val = MTK_MMC_READ_4(sc, MTK_MSDC_CFG);
       device_printf(sc->sc_dev, "%s MTK_MSDC_CFG val %0x\n", __func__, val);
       if (mode) {
	       val |= MTK_MSDC_CFG_CCKMD;
       } else {
	       val &= ~MTK_MSDC_CFG_CCKMD;
       }

       val &= ~MTK_MSDC_CFG_CCKDIV_MASK;
       val |= (div << MTK_MSDC_CFG_CCKDIV_SHIFT)
	   & MTK_MSDC_CFG_CCKDIV_MASK;

       device_printf(sc->sc_dev, "%s MTK_MSDC_CFG val %0x\n", __func__, val);

       MTK_MMC_WRITE_4(sc, MTK_MSDC_CFG, val);

       error = clk_enable(sc->sclk);
       if (error)
	       device_printf(sc->sc_dev, "cannot enable mmc clock\n");

       //TODO Dangerous fix it. May get stuck in an infinite loop.
       while (!(MTK_MMC_READ_4(sc, MTK_MSDC_CFG) & MTK_MSDC_CFG_CCKSB))
	       DELAY(1000);

       val = MTK_MMC_READ_4(sc, MTK_MSDC_CFG);
       val |= MTK_MSDC_CFG_CCKPD;
       MTK_MMC_WRITE_4(sc, MTK_MSDC_CFG, val);

       return (0);
}

static int
mtk_mmc_switch_vccq(device_t bus, device_t child)
{
       struct mt_mmc_softc *sc;
       int uvolt, err;

       sc = device_get_softc(bus);

       if (sc->mmc_helper.vqmmc_supply == NULL)
	       return (EOPNOTSUPP);

       switch (sc->sc_host.ios.vccq) {
       case vccq_180:
	       uvolt = 1800000;
	       break;
       case vccq_330:
	       uvolt = 3300000;
	       break;
       default:
	       return (EINVAL);
       }

       err = regulator_set_voltage(sc->mmc_helper.vqmmc_supply, uvolt, uvolt);
       if (err != 0) {
	       device_printf(sc->sc_dev,
		   "Cannot set vqmmc to %d<->%d\n",
		   uvolt,
		   uvolt);
	       return (err);
       }

       return (0);
}

static int
mtk_mmc_update_ios(device_t bus, device_t child)
{
       struct mt_mmc_softc *sc;
       struct mmc_ios *ios;
       uint32_t buswd;
       uint32_t val;

       sc = device_get_softc(bus);

       ios = &sc->sc_host.ios;

       device_printf(bus, "%s Setting up clk %u bus_width %d, timing: %d\n",
	   __func__, ios->clock, ios->bus_width, ios->timing);

       /* Set the bus width. */
       switch (ios->bus_width) {
       case bus_width_1:
	       buswd = MTK_SDC_CFG_BUSWD_1BIT;
	       break;
       case bus_width_4:
	       buswd = MTK_SDC_CFG_BUSWD_4BIT;
	       break;
       case bus_width_8:
	       buswd = MTK_SDC_CFG_BUSWD_8BIT;
	       break;
       default:
	       return (EINVAL);
       }
       val = MTK_MMC_READ_4(sc, MTK_SDC_CFG);
       val &= ~MTK_SDC_CFG_BUSWD_MASK;
       val |= buswd & MTK_SDC_CFG_BUSWD_MASK;
       MTK_MMC_WRITE_4(sc, MTK_SDC_CFG, val);

       switch (ios->power_mode) {
       case power_on:
	       break;
       case power_off:
	       if (sc->mmc_helper.vmmc_supply)
		       regulator_disable(sc->mmc_helper.vmmc_supply);
	       if (sc->mmc_helper.vqmmc_supply)
		       regulator_disable(sc->mmc_helper.vqmmc_supply);
	       break;
       case power_up:
	       if (sc->mmc_helper.vmmc_supply)
		       regulator_enable(sc->mmc_helper.vmmc_supply);
	       if (sc->mmc_helper.vqmmc_supply)
		       regulator_enable(sc->mmc_helper.vqmmc_supply);
	       break;
       };

       if (ios->clock && ios->clock != sc->sc_clock) {
	       sc->sc_clock = ios->clock;
	       mtk_mmc_config_clock(sc, ios->clock);
       }

       return (0);
}

static int
mtk_mmc_get_ro(device_t bus, device_t child)
{
       //	struct mtk_mmc_softc *sc;

       //	sc = device_get_softc(bus);

       //	return (mmc_fdt_gpio_get_readonly(&sc->mmc_helper));
       return (0);
}

static int
mtk_mmc_acquire_host(device_t bus, device_t child)
{
       struct mt_mmc_softc *sc;
       int error;

       sc = device_get_softc(bus);
       MTK_MMC_LOCK(sc);
       while (sc->sc_bus_busy) {
	       error = msleep(sc, &sc->sc_mtx, PCATCH, "mmchw", 0);
	       if (error != 0) {
		       MTK_MMC_UNLOCK(sc);
		       return (error);
	       }
       }
       sc->sc_bus_busy++;
       MTK_MMC_UNLOCK(sc);

       return (0);
}

static int
mtk_mmc_release_host(device_t bus, device_t child)
{
       struct mt_mmc_softc *sc;

       sc = device_get_softc(bus);
       MTK_MMC_LOCK(sc);
       sc->sc_bus_busy--;
       wakeup(sc);
       MTK_MMC_UNLOCK(sc);

       return (0);
}

static device_method_t mt_mmc_methods[] = {
       /* Device interface */
       DEVMETHOD(device_probe,		mtk_mmc_probe),
       DEVMETHOD(device_attach,	mtk_mmc_attach),
       DEVMETHOD(device_detach,	mtk_mmc_detach),

       /* Bus interface */
       DEVMETHOD(bus_read_ivar,	mtk_mmc_read_ivar),
       DEVMETHOD(bus_write_ivar,	mtk_mmc_write_ivar),
       DEVMETHOD(bus_add_child,	bus_generic_add_child),

       /* MMC bridge interface */
       DEVMETHOD(mmcbr_update_ios,	mtk_mmc_update_ios),
       DEVMETHOD(mmcbr_request,	mtk_mmc_request),
       DEVMETHOD(mmcbr_get_ro,		mtk_mmc_get_ro),
       DEVMETHOD(mmcbr_switch_vccq,	mtk_mmc_switch_vccq),
       DEVMETHOD(mmcbr_acquire_host,	mtk_mmc_acquire_host),
       DEVMETHOD(mmcbr_release_host,	mtk_mmc_release_host),

       DEVMETHOD_END
};

static DEFINE_CLASS_0(mt_mmc, mt_mmc_driver, mt_mmc_methods,
    sizeof(struct mt_mmc_softc));
DRIVER_MODULE(mt_mmc, simplebus, mt_mmc_driver, NULL, NULL);
MMC_DECLARE_BRIDGE(mt_mmc);
