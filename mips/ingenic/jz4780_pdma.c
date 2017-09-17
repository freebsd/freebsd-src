/*-
 * Copyright (c) 2016 Ruslan Bukin <br@bsdpad.com>
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract FA8750-10-C-0237
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

/* Ingenic JZ4780 PDMA Controller. */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_platform.h"
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/resource.h>
#include <sys/rman.h>

#include <machine/bus.h>
#include <machine/cache.h>

#ifdef FDT
#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#endif

#include <dev/xdma/xdma.h>

#include <mips/ingenic/jz4780_common.h>
#include <mips/ingenic/jz4780_pdma.h>

#include "xdma_if.h"

struct pdma_softc {
	device_t		dev;
	struct resource		*res[2];
	bus_space_tag_t		bst;
	bus_space_handle_t	bsh;
	void			*ih;
};

struct pdma_fdt_data {
	int tx;
	int rx;
	int chan;
};

struct pdma_channel {
	xdma_channel_t		*xchan;
	struct pdma_fdt_data	data;
	int			cur_desc;
	int			used;
	int			index;
	int			flags;
#define	CHAN_DESCR_RELINK	(1 << 0)
};

#define	PDMA_NCHANNELS	32
struct pdma_channel pdma_channels[PDMA_NCHANNELS];

static struct resource_spec pdma_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ SYS_RES_IRQ,		0,	RF_ACTIVE },
	{ -1, 0 }
};

static int pdma_probe(device_t dev);
static int pdma_attach(device_t dev);
static int pdma_detach(device_t dev);
static int chan_start(struct pdma_softc *sc, struct pdma_channel *chan);

static void
pdma_intr(void *arg)
{
	struct pdma_channel *chan;
	struct pdma_softc *sc;
	xdma_channel_t *xchan;
	xdma_config_t *conf;
	int pending;
	int i;

	sc = arg;

	pending = READ4(sc, PDMA_DIRQP);

	/* Ack all the channels. */
	WRITE4(sc, PDMA_DIRQP, 0);

	for (i = 0; i < PDMA_NCHANNELS; i++) {
		if (pending & (1 << i)) {
			chan = &pdma_channels[i];
			xchan = chan->xchan;
			conf = &xchan->conf;

			/* TODO: check for AR, HLT error bits here. */

			/* Disable channel */
			WRITE4(sc, PDMA_DCS(chan->index), 0);

			if (chan->flags & CHAN_DESCR_RELINK) {
				/* Enable again */
				chan->cur_desc = (chan->cur_desc + 1) % \
				    conf->block_num;
				chan_start(sc, chan);
			}

			xdma_callback(chan->xchan);
		}
	}
}

static int
pdma_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_is_compatible(dev, "ingenic,jz4780-dma"))
		return (ENXIO);

	device_set_desc(dev, "Ingenic JZ4780 PDMA Controller");

	return (BUS_PROBE_DEFAULT);
}

static int
pdma_attach(device_t dev)
{
	struct pdma_softc *sc;
	phandle_t xref, node;
	int err;
	int reg;

	sc = device_get_softc(dev);
	sc->dev = dev;

	if (bus_alloc_resources(dev, pdma_spec, sc->res)) {
		device_printf(dev, "could not allocate resources for device\n");
		return (ENXIO);
	}

	/* Memory interface */
	sc->bst = rman_get_bustag(sc->res[0]);
	sc->bsh = rman_get_bushandle(sc->res[0]);

	/* Setup interrupt handler */
	err = bus_setup_intr(dev, sc->res[1], INTR_TYPE_MISC | INTR_MPSAFE,
	    NULL, pdma_intr, sc, &sc->ih);
	if (err) {
		device_printf(dev, "Unable to alloc interrupt resource.\n");
		return (ENXIO);
	}

	node = ofw_bus_get_node(dev);
	xref = OF_xref_from_node(node);
	OF_device_register_xref(xref, dev);

	reg = READ4(sc, PDMA_DMAC);
	reg &= ~(DMAC_HLT | DMAC_AR);
	reg |= (DMAC_DMAE);
	WRITE4(sc, PDMA_DMAC, reg);

	WRITE4(sc, PDMA_DMACP, 0);

	return (0);
}

static int
pdma_detach(device_t dev)
{
	struct pdma_softc *sc;

	sc = device_get_softc(dev);

	bus_release_resources(dev, pdma_spec, sc->res);

	return (0);
}

static int
chan_start(struct pdma_softc *sc, struct pdma_channel *chan)
{
	struct xdma_channel *xchan;

	xchan = chan->xchan;

	/* 8 byte descriptor. */
	WRITE4(sc, PDMA_DCS(chan->index), DCS_DES8);
	WRITE4(sc, PDMA_DDA(chan->index), xchan->descs_phys[chan->cur_desc].ds_addr);
	WRITE4(sc, PDMA_DDS, (1 << chan->index));

	/* Channel transfer enable. */
	WRITE4(sc, PDMA_DCS(chan->index), (DCS_DES8 | DCS_CTE));

	return (0);
}

static int
chan_stop(struct pdma_softc *sc, struct pdma_channel *chan)
{
	int timeout;

	WRITE4(sc, PDMA_DCS(chan->index), 0);

	timeout = 100;

	do {
		if ((READ4(sc, PDMA_DCS(chan->index)) & DCS_CTE) == 0) {
			break;
		}
	} while (timeout--);

	if (timeout == 0) {
		device_printf(sc->dev, "%s: Can't stop channel %d\n",
		    __func__, chan->index);
	}

	return (0);
}

static int
pdma_channel_alloc(device_t dev, struct xdma_channel *xchan)
{
	struct pdma_channel *chan;
	struct pdma_softc *sc;
	int i;

	sc = device_get_softc(dev);

	xdma_assert_locked();

	for (i = 0; i < PDMA_NCHANNELS; i++) {
		chan = &pdma_channels[i];
		if (chan->used == 0) {
			chan->xchan = xchan;
			xchan->chan = (void *)chan;
			chan->used = 1;
			chan->index = i;

			return (0);
		}
	}

	return (-1);
}

static int
pdma_channel_free(device_t dev, struct xdma_channel *xchan)
{
	struct pdma_channel *chan;
	struct pdma_softc *sc;

	sc = device_get_softc(dev);

	xdma_assert_locked();

	chan = (struct pdma_channel *)xchan->chan;
	chan->used = 0;

	return (0);
}

static int
pdma_channel_prep_memcpy(device_t dev, struct xdma_channel *xchan)
{
	struct pdma_channel *chan;
	struct pdma_hwdesc *desc;
	struct pdma_softc *sc;
	xdma_config_t *conf;
	int ret;

	sc = device_get_softc(dev);

	chan = (struct pdma_channel *)xchan->chan;
	/* Ensure we are not in operation */
	chan_stop(sc, chan);

	ret = xdma_desc_alloc(xchan, sizeof(struct pdma_hwdesc), 8);
	if (ret != 0) {
		device_printf(sc->dev,
		    "%s: Can't allocate descriptors.\n", __func__);
		return (-1);
	}

	conf = &xchan->conf;
	desc = (struct pdma_hwdesc *)xchan->descs;
	desc[0].dsa = conf->src_addr;
	desc[0].dta = conf->dst_addr;
	desc[0].drt = DRT_AUTO;
	desc[0].dcm = DCM_SAI | DCM_DAI;

	/* 4 byte copy for now. */
	desc[0].dtc = (conf->block_len / 4);
	desc[0].dcm |= DCM_SP_4 | DCM_DP_4 | DCM_TSZ_4;
	desc[0].dcm |= DCM_TIE;

	return (0);
}

static int
access_width(xdma_config_t *conf, uint32_t *dcm, uint32_t *max_width)
{

	*dcm = 0;
	*max_width = max(conf->src_width, conf->dst_width);

	switch (conf->src_width) {
	case 1:
		*dcm |= DCM_SP_1;
		break;
	case 2:
		*dcm |= DCM_SP_2;
		break;
	case 4:
		*dcm |= DCM_SP_4;
		break;
	default:
		return (-1);
	}

	switch (conf->dst_width) {
	case 1:
		*dcm |= DCM_DP_1;
		break;
	case 2:
		*dcm |= DCM_DP_2;
		break;
	case 4:
		*dcm |= DCM_DP_4;
		break;
	default:
		return (-1);
	}

	switch (*max_width) {
	case 1:
		*dcm |= DCM_TSZ_1;
		break;
	case 2:
		*dcm |= DCM_TSZ_2;
		break;
	case 4:
		*dcm |= DCM_TSZ_4;
		break;
	default:
		return (-1);
	};

	return (0);
}

static int
pdma_channel_prep_cyclic(device_t dev, struct xdma_channel *xchan)
{
	struct pdma_fdt_data *data;
	struct pdma_channel *chan;
	struct pdma_hwdesc *desc;
	xdma_controller_t *xdma;
	struct pdma_softc *sc;
	xdma_config_t *conf;
	int max_width;
	uint32_t reg;
	uint32_t dcm;
	int ret;
	int i;

	sc = device_get_softc(dev);

	conf = &xchan->conf;
	xdma = xchan->xdma;
	data = (struct pdma_fdt_data *)xdma->data;

	ret = xdma_desc_alloc(xchan, sizeof(struct pdma_hwdesc), 8);
	if (ret != 0) {
		device_printf(sc->dev,
		    "%s: Can't allocate descriptors.\n", __func__);
		return (-1);
	}

	chan = (struct pdma_channel *)xchan->chan;
	/* Ensure we are not in operation */
	chan_stop(sc, chan);
	chan->flags = CHAN_DESCR_RELINK;
	chan->cur_desc = 0;

	desc = (struct pdma_hwdesc *)xchan->descs;

	for (i = 0; i < conf->block_num; i++) {
		if (conf->direction == XDMA_MEM_TO_DEV) {
			desc[i].dsa = conf->src_addr + (i * conf->block_len);
			desc[i].dta = conf->dst_addr;
			desc[i].drt = data->tx;
			desc[i].dcm = DCM_SAI;
		} else if (conf->direction == XDMA_DEV_TO_MEM) {
			desc[i].dsa = conf->src_addr;
			desc[i].dta = conf->dst_addr + (i * conf->block_len);
			desc[i].drt = data->rx;
			desc[i].dcm = DCM_DAI;
		} else if (conf->direction == XDMA_MEM_TO_MEM) {
			desc[i].dsa = conf->src_addr + (i * conf->block_len);
			desc[i].dta = conf->dst_addr + (i * conf->block_len);
			desc[i].drt = DRT_AUTO;
			desc[i].dcm = DCM_SAI | DCM_DAI;
		}

		if (access_width(conf, &dcm, &max_width) != 0) {
			device_printf(dev,
			    "%s: can't configure access width\n", __func__);
			return (-1);
		}

		desc[i].dcm |= dcm | DCM_TIE;
		desc[i].dtc = (conf->block_len / max_width);

		/*
		 * PDMA does not provide interrupt after processing each descriptor,
		 * but after processing all the chain only.
		 * As a workaround we do unlink descriptors here, so our chain will
		 * consists of single descriptor only. And then we reconfigure channel
		 * on each interrupt again.
		 */
		if ((chan->flags & CHAN_DESCR_RELINK) == 0) {
			if (i != (conf->block_num - 1)) {
				desc[i].dcm |= DCM_LINK;
				reg = ((i + 1) * sizeof(struct pdma_hwdesc));
				desc[i].dtc |= (reg >> 4) << 24;
			}
		}
	}

	return (0);
}

static int
pdma_channel_control(device_t dev, xdma_channel_t *xchan, int cmd)
{
	struct pdma_channel *chan;
	struct pdma_softc *sc;

	sc = device_get_softc(dev);

	chan = (struct pdma_channel *)xchan->chan;

	switch (cmd) {
	case XDMA_CMD_BEGIN:
		chan_start(sc, chan);
		break;
	case XDMA_CMD_TERMINATE:
		chan_stop(sc, chan);
		break;
	case XDMA_CMD_PAUSE:
		/* TODO: implement me */
		return (-1);
	}

	return (0);
}

#ifdef FDT
static int
pdma_ofw_md_data(device_t dev, pcell_t *cells, int ncells, void **ptr)
{
	struct pdma_fdt_data *data;

	if (ncells != 3) {
		return (-1);
	}

	data = malloc(sizeof(struct pdma_fdt_data), M_DEVBUF, (M_WAITOK | M_ZERO));
	if (data == NULL) {
		device_printf(dev, "%s: Cant allocate memory\n", __func__);
		return (-1);
	}

	data->tx = cells[0];
	data->rx = cells[1];
	data->chan = cells[2];

	*ptr = data;

	return (0);
}
#endif

static device_method_t pdma_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,			pdma_probe),
	DEVMETHOD(device_attach,		pdma_attach),
	DEVMETHOD(device_detach,		pdma_detach),

	/* xDMA Interface */
	DEVMETHOD(xdma_channel_alloc,		pdma_channel_alloc),
	DEVMETHOD(xdma_channel_free,		pdma_channel_free),
	DEVMETHOD(xdma_channel_prep_cyclic,	pdma_channel_prep_cyclic),
	DEVMETHOD(xdma_channel_prep_memcpy,	pdma_channel_prep_memcpy),
	DEVMETHOD(xdma_channel_control,		pdma_channel_control),
#ifdef FDT
	DEVMETHOD(xdma_ofw_md_data,		pdma_ofw_md_data),
#endif

	DEVMETHOD_END
};

static driver_t pdma_driver = {
	"pdma",
	pdma_methods,
	sizeof(struct pdma_softc),
};

static devclass_t pdma_devclass;

EARLY_DRIVER_MODULE(pdma, simplebus, pdma_driver, pdma_devclass, 0, 0,
    BUS_PASS_INTERRUPT + BUS_PASS_ORDER_LATE);
