/*-
 * Copyright (c) 2014-2016 Jared D. McNeill <jmcneill@invisible.ca>
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
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*
 * Allwinner A10/A20 DMA controller
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_bus.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/condvar.h>
#include <sys/kernel.h>
#include <sys/module.h>

#include <machine/bus.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "sunxi_dma_if.h"

#include "a10_dmac.h"
#include "a10_clk.h"

#define NDMA_CHANNELS	8
#define DDMA_CHANNELS	8

enum a10dmac_type {
	CH_NDMA,
	CH_DDMA
};

struct a10dmac_softc;

struct a10dmac_channel {
	struct a10dmac_softc *	ch_sc;
	uint8_t			ch_index;
	enum a10dmac_type	ch_type;
	void			(*ch_callback)(void *);
	void *			ch_callbackarg;
	uint32_t		ch_regoff;
};

struct a10dmac_softc {
	device_t		sc_dev;
	struct resource *	sc_res;
	struct resource *	sc_irq;
	struct mtx		sc_mtx;
	bus_space_tag_t		sc_bst;
	bus_space_handle_t	sc_bsh;
	void *			sc_ih;

	struct a10dmac_channel	sc_ndma_channels[NDMA_CHANNELS];
	struct a10dmac_channel	sc_ddma_channels[DDMA_CHANNELS];
};

#define DMA_READ(sc, reg)		\
    bus_space_read_4((sc)->sc_bst, (sc)->sc_bsh, (reg))
#define DMA_WRITE(sc, reg, val)		\
    bus_space_write_4((sc)->sc_bst, (sc)->sc_bsh, (reg), (val))
#define DMACH_READ(ch, reg)		\
    DMA_READ((ch)->ch_sc, (reg) + (ch)->ch_regoff)
#define DMACH_WRITE(ch, reg, val)	\
    DMA_WRITE((ch)->ch_sc, (reg) + (ch)->ch_regoff, (val))

static void a10dmac_intr(void *);

static int
a10dmac_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_is_compatible(dev, "allwinner,sun4i-a10-dma"))
		return (ENXIO);

	device_set_desc(dev, "Allwinner DMA controller");
	return (BUS_PROBE_DEFAULT);
}

static int
a10dmac_attach(device_t dev)
{
	struct a10dmac_softc *sc;
	unsigned int index;
	int rid, error;

	sc = device_get_softc(dev);
	sc->sc_dev = dev;

	mtx_init(&sc->sc_mtx, "a10 dmac", NULL, MTX_SPIN);

	rid = 0;
	sc->sc_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->sc_res == NULL) {
		device_printf(dev, "unable to map memory\n");
		error = ENXIO;
		goto fail;
	}
	sc->sc_bst = rman_get_bustag(sc->sc_res);
	sc->sc_bsh = rman_get_bushandle(sc->sc_res);

	rid = 0;
	sc->sc_irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_SHAREABLE | RF_ACTIVE);
	if (sc->sc_irq == NULL) {
		device_printf(dev, "cannot allocate IRQ resources\n");
		error = ENXIO;
		goto fail;
	}

	/* Activate DMA controller clock */
	a10_clk_dmac_activate();

	/* Disable all interrupts and clear pending status */
	DMA_WRITE(sc, AWIN_DMA_IRQ_EN_REG, 0);
	DMA_WRITE(sc, AWIN_DMA_IRQ_PEND_STA_REG, ~0);

	/* Initialize channels */
	for (index = 0; index < NDMA_CHANNELS; index++) {
		sc->sc_ndma_channels[index].ch_sc = sc;
		sc->sc_ndma_channels[index].ch_index = index;
		sc->sc_ndma_channels[index].ch_type = CH_NDMA;
		sc->sc_ndma_channels[index].ch_callback = NULL;
		sc->sc_ndma_channels[index].ch_callbackarg = NULL;
		sc->sc_ndma_channels[index].ch_regoff = AWIN_NDMA_REG(index);
		DMACH_WRITE(&sc->sc_ndma_channels[index], AWIN_NDMA_CTL_REG, 0);
	}
	for (index = 0; index < DDMA_CHANNELS; index++) {
		sc->sc_ddma_channels[index].ch_sc = sc;
		sc->sc_ddma_channels[index].ch_index = index;
		sc->sc_ddma_channels[index].ch_type = CH_DDMA;
		sc->sc_ddma_channels[index].ch_callback = NULL;
		sc->sc_ddma_channels[index].ch_callbackarg = NULL;
		sc->sc_ddma_channels[index].ch_regoff = AWIN_DDMA_REG(index);
		DMACH_WRITE(&sc->sc_ddma_channels[index], AWIN_DDMA_CTL_REG, 0);
	}

	error = bus_setup_intr(dev, sc->sc_irq, INTR_MPSAFE | INTR_TYPE_MISC,
	    NULL, a10dmac_intr, sc, &sc->sc_ih);
	if (error != 0) {
		device_printf(dev, "could not setup interrupt handler\n");
		goto fail;
	}

	return (0);

fail:
	if (sc->sc_res != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->sc_res);
	if (sc->sc_irq != NULL)
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->sc_irq);

	return (error);
}

static void
a10dmac_intr(void *priv)
{
	struct a10dmac_softc *sc = priv;
	uint32_t sta, bit, mask;
	uint8_t index;

	mtx_lock(&sc->sc_mtx);
	sta = DMA_READ(sc, AWIN_DMA_IRQ_PEND_STA_REG);
	DMA_WRITE(sc, AWIN_DMA_IRQ_PEND_STA_REG, sta);
	mtx_unlock(&sc->sc_mtx);

	while ((bit = ffs(sta & AWIN_DMA_IRQ_END_MASK)) != 0) {
		mask = __BIT(bit - 1);
		sta &= ~mask;
		index = ((bit - 1) / 2) & 7;
		if (mask & AWIN_DMA_IRQ_NDMA) {
			if (sc->sc_ndma_channels[index].ch_callback == NULL)
				continue;
			sc->sc_ndma_channels[index].ch_callback(
			    sc->sc_ndma_channels[index].ch_callbackarg);
		} else {
			if (sc->sc_ddma_channels[index].ch_callback == NULL)
				continue;
			sc->sc_ddma_channels[index].ch_callback(
			    sc->sc_ddma_channels[index].ch_callbackarg);
		}
	}
}

static uint32_t
a10dmac_get_config(void *priv)
{
	struct a10dmac_channel *ch = priv;

	if (ch->ch_type == CH_NDMA) {
		return (DMACH_READ(ch, AWIN_NDMA_CTL_REG));
	} else {
		return (DMACH_READ(ch, AWIN_DDMA_CTL_REG));
	}
}

static void
a10dmac_set_config(void *priv, uint32_t val)
{
	struct a10dmac_channel *ch = priv;

	if (ch->ch_type == CH_NDMA) {
		DMACH_WRITE(ch, AWIN_NDMA_CTL_REG, val);
	} else {
		DMACH_WRITE(ch, AWIN_DDMA_CTL_REG, val);
	}
}

static void *
a10dmac_alloc(device_t dev, bool dedicated, void (*cb)(void *), void *cbarg)
{
	struct a10dmac_softc *sc = device_get_softc(dev);
	struct a10dmac_channel *ch_list;
	struct a10dmac_channel *ch = NULL;
	uint32_t irqen;
	uint8_t ch_count, index;

	if (dedicated) {
		ch_list = sc->sc_ddma_channels;
		ch_count = DDMA_CHANNELS;
	} else {
		ch_list = sc->sc_ndma_channels;
		ch_count = NDMA_CHANNELS;
	}

	mtx_lock(&sc->sc_mtx);
	for (index = 0; index < ch_count; index++) {
		if (ch_list[index].ch_callback == NULL) {
			ch = &ch_list[index];
			ch->ch_callback = cb;
			ch->ch_callbackarg = cbarg;

			irqen = DMA_READ(sc, AWIN_DMA_IRQ_EN_REG);
			if (ch->ch_type == CH_NDMA)
				irqen |= AWIN_DMA_IRQ_NDMA_END(index);
			else
				irqen |= AWIN_DMA_IRQ_DDMA_END(index);
			DMA_WRITE(sc, AWIN_DMA_IRQ_EN_REG, irqen);

			break;
		}
	}
	mtx_unlock(&sc->sc_mtx);

	return (ch);
}

static void
a10dmac_free(void *priv)
{
	struct a10dmac_channel *ch = priv;
	struct a10dmac_softc *sc = ch->ch_sc;
	uint32_t irqen, sta, cfg;

	mtx_lock(&sc->sc_mtx);

	irqen = DMA_READ(sc, AWIN_DMA_IRQ_EN_REG);
	cfg = a10dmac_get_config(ch);
	if (ch->ch_type == CH_NDMA) {
		sta = AWIN_DMA_IRQ_NDMA_END(ch->ch_index);
		cfg &= ~AWIN_NDMA_CTL_DMA_LOADING;
	} else {
		sta = AWIN_DMA_IRQ_DDMA_END(ch->ch_index);
		cfg &= ~AWIN_DDMA_CTL_DMA_LOADING;
	}
	irqen &= ~sta;
	a10dmac_set_config(ch, cfg);
	DMA_WRITE(sc, AWIN_DMA_IRQ_EN_REG, irqen);
	DMA_WRITE(sc, AWIN_DMA_IRQ_PEND_STA_REG, sta);

	ch->ch_callback = NULL;
	ch->ch_callbackarg = NULL;

	mtx_unlock(&sc->sc_mtx);
}

static int
a10dmac_transfer(void *priv, bus_addr_t src, bus_addr_t dst,
    size_t nbytes)
{
	struct a10dmac_channel *ch = priv;
	uint32_t cfg;

	cfg = a10dmac_get_config(ch);
	if (ch->ch_type == CH_NDMA) {
		if (cfg & AWIN_NDMA_CTL_DMA_LOADING)
			return (EBUSY);

		DMACH_WRITE(ch, AWIN_NDMA_SRC_ADDR_REG, src);
		DMACH_WRITE(ch, AWIN_NDMA_DEST_ADDR_REG, dst);
		DMACH_WRITE(ch, AWIN_NDMA_BC_REG, nbytes);

		cfg |= AWIN_NDMA_CTL_DMA_LOADING;
		a10dmac_set_config(ch, cfg);
	} else {
		if (cfg & AWIN_DDMA_CTL_DMA_LOADING)
			return (EBUSY);

		DMACH_WRITE(ch, AWIN_DDMA_SRC_START_ADDR_REG, src);
		DMACH_WRITE(ch, AWIN_DDMA_DEST_START_ADDR_REG, dst);
		DMACH_WRITE(ch, AWIN_DDMA_BC_REG, nbytes);
		DMACH_WRITE(ch, AWIN_DDMA_PARA_REG,
		    __SHIFTIN(31, AWIN_DDMA_PARA_DST_DATA_BLK_SIZ) |
		    __SHIFTIN(7, AWIN_DDMA_PARA_DST_WAIT_CYC) |
		    __SHIFTIN(31, AWIN_DDMA_PARA_SRC_DATA_BLK_SIZ) |
		    __SHIFTIN(7, AWIN_DDMA_PARA_SRC_WAIT_CYC));

		cfg |= AWIN_DDMA_CTL_DMA_LOADING;
		a10dmac_set_config(ch, cfg);
	}

	return (0);
}

static void
a10dmac_halt(void *priv)
{
	struct a10dmac_channel *ch = priv;
	uint32_t cfg;

	cfg = a10dmac_get_config(ch);
	if (ch->ch_type == CH_NDMA) {
		cfg &= ~AWIN_NDMA_CTL_DMA_LOADING;
	} else {
		cfg &= ~AWIN_DDMA_CTL_DMA_LOADING;
	}
	a10dmac_set_config(ch, cfg);
}

static device_method_t a10dmac_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		a10dmac_probe),
	DEVMETHOD(device_attach,	a10dmac_attach),

	/* sunxi DMA interface */
	DEVMETHOD(sunxi_dma_alloc,	a10dmac_alloc),
	DEVMETHOD(sunxi_dma_free,	a10dmac_free),
	DEVMETHOD(sunxi_dma_get_config,	a10dmac_get_config),
	DEVMETHOD(sunxi_dma_set_config,	a10dmac_set_config),
	DEVMETHOD(sunxi_dma_transfer,	a10dmac_transfer),
	DEVMETHOD(sunxi_dma_halt,	a10dmac_halt),

	DEVMETHOD_END
};

static driver_t a10dmac_driver = {
	"a10dmac",
	a10dmac_methods,
	sizeof(struct a10dmac_softc)
};

static devclass_t a10dmac_devclass;

DRIVER_MODULE(a10dmac, simplebus, a10dmac_driver, a10dmac_devclass, 0, 0);
