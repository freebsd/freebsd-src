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

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <arm/lpc/lpcreg.h>
#include <arm/lpc/lpcvar.h>

struct lpc_dmac_channel
{
	struct lpc_dmac_channel_config *ldc_config;
	int			ldc_flags;
};

struct lpc_dmac_softc
{
	device_t		ld_dev;
	struct mtx		ld_mtx;
	struct resource *	ld_mem_res;
	struct resource *	ld_irq_res;
	bus_space_tag_t		ld_bst;
	bus_space_handle_t	ld_bsh;
	void *			ld_intrhand;
	struct lpc_dmac_channel	ld_channels[8];
};

static struct lpc_dmac_softc *lpc_dmac_sc = NULL;

static int lpc_dmac_probe(device_t);
static int lpc_dmac_attach(device_t);
static void lpc_dmac_intr(void *);

#define	lpc_dmac_read_4(_sc, _reg) \
    bus_space_read_4(_sc->ld_bst, _sc->ld_bsh, _reg)
#define	lpc_dmac_write_4(_sc, _reg, _value) \
    bus_space_write_4(_sc->ld_bst, _sc->ld_bsh, _reg, _value)
#define	lpc_dmac_read_ch_4(_sc, _n, _reg) \
    bus_space_read_4(_sc->ld_bst, _sc->ld_bsh, (_reg + LPC_DMAC_CHADDR(_n)))
#define	lpc_dmac_write_ch_4(_sc, _n, _reg, _value) \
    bus_space_write_4(_sc->ld_bst, _sc->ld_bsh, (_reg + LPC_DMAC_CHADDR(_n)), _value)

static int lpc_dmac_probe(device_t dev)
{
	if (!ofw_bus_is_compatible(dev, "lpc,dmac"))
		return (ENXIO);

	device_set_desc(dev, "LPC32x0 General Purpose DMA controller");
	return (BUS_PROBE_DEFAULT);
}

static int lpc_dmac_attach(device_t dev)
{
	struct lpc_dmac_softc *sc = device_get_softc(dev);
	int rid;

	rid = 0;
	sc->ld_mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (!sc->ld_mem_res) {
		device_printf(dev, "cannot allocate memory window\n");
		return (ENXIO);
	}

	sc->ld_bst = rman_get_bustag(sc->ld_mem_res);
	sc->ld_bsh = rman_get_bushandle(sc->ld_mem_res);

	rid = 0;
	sc->ld_irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE);
	if (!sc->ld_irq_res) {
		device_printf(dev, "cannot allocate cmd interrupt\n");
		bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->ld_mem_res);
		return (ENXIO);
	}

	if (bus_setup_intr(dev, sc->ld_irq_res, INTR_TYPE_MISC | INTR_MPSAFE,
	    NULL, lpc_dmac_intr, sc, &sc->ld_intrhand))
	{
		bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->ld_mem_res);
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->ld_irq_res);
		device_printf(dev, "cannot setup interrupt handler\n");
		return (ENXIO);
	}

	lpc_dmac_sc = sc;

	lpc_pwr_write(dev, LPC_CLKPWR_DMACLK_CTRL, LPC_CLKPWR_DMACLK_CTRL_EN);
	lpc_dmac_write_4(sc, LPC_DMAC_CONFIG, LPC_DMAC_CONFIG_ENABLE);

	lpc_dmac_write_4(sc, LPC_DMAC_INTTCCLEAR, 0xff);
	lpc_dmac_write_4(sc, LPC_DMAC_INTERRCLEAR, 0xff);

	return (0);
}

static void lpc_dmac_intr(void *arg)
{
	struct lpc_dmac_softc *sc = (struct lpc_dmac_softc *)arg;
	struct lpc_dmac_channel *ch;
	uint32_t intstat, tcstat, errstat;
	int i;

	do {
		intstat = lpc_dmac_read_4(sc, LPC_DMAC_INTSTAT);

		for (i = 0; i < LPC_DMAC_CHNUM; i++) {
			if ((intstat & (1 << i)) == 0)
				continue;
	
			ch = &sc->ld_channels[i];
			tcstat = lpc_dmac_read_4(sc, LPC_DMAC_INTTCSTAT);
			errstat = lpc_dmac_read_4(sc, LPC_DMAC_INTERRSTAT);

			if (tcstat & (1 << i)) {
				ch->ldc_config->ldc_success_handler(
				    ch->ldc_config->ldc_handler_arg);
				lpc_dmac_write_4(sc, LPC_DMAC_INTTCCLEAR, (1 << i));
			}

			if (errstat & (1 << i)) {
				ch->ldc_config->ldc_error_handler(
				    ch->ldc_config->ldc_handler_arg);
				lpc_dmac_write_4(sc, LPC_DMAC_INTERRCLEAR, (1 << i));
			}
		}

	} while (intstat);
}

int
lpc_dmac_config_channel(device_t dev, int chno, struct lpc_dmac_channel_config *cfg)
{
	struct lpc_dmac_softc *sc = lpc_dmac_sc;
	struct lpc_dmac_channel *ch;

	if (sc == NULL)
		return (ENXIO);

	ch = &sc->ld_channels[chno];
	ch->ldc_config = cfg;

	return 0;
}

int
lpc_dmac_setup_transfer(device_t dev, int chno, bus_addr_t src, bus_addr_t dst,
    bus_size_t size, int flags)
{
	struct lpc_dmac_softc *sc = lpc_dmac_sc;
	struct lpc_dmac_channel *ch;
	uint32_t ctrl, cfg;

	if (sc == NULL)
		return (ENXIO);

	ch = &sc->ld_channels[chno];

	ctrl = LPC_DMAC_CH_CONTROL_I |
	    (ch->ldc_config->ldc_dst_incr ? LPC_DMAC_CH_CONTROL_DI : 0) | 
	    (ch->ldc_config->ldc_src_incr ? LPC_DMAC_CH_CONTROL_SI : 0) |
	    LPC_DMAC_CH_CONTROL_DWIDTH(ch->ldc_config->ldc_dst_width) |
	    LPC_DMAC_CH_CONTROL_SWIDTH(ch->ldc_config->ldc_src_width) |
	    LPC_DMAC_CH_CONTROL_DBSIZE(ch->ldc_config->ldc_dst_burst) |
	    LPC_DMAC_CH_CONTROL_SBSIZE(ch->ldc_config->ldc_src_burst) |
	    size;

	cfg = LPC_DMAC_CH_CONFIG_ITC | LPC_DMAC_CH_CONFIG_IE |
	    LPC_DMAC_CH_CONFIG_FLOWCNTL(ch->ldc_config->ldc_fcntl) |
	    LPC_DMAC_CH_CONFIG_DESTP(ch->ldc_config->ldc_dst_periph) |
	    LPC_DMAC_CH_CONFIG_SRCP(ch->ldc_config->ldc_src_periph) | LPC_DMAC_CH_CONFIG_E;
	lpc_dmac_write_ch_4(sc, chno, LPC_DMAC_CH_SRCADDR, src);
	lpc_dmac_write_ch_4(sc, chno, LPC_DMAC_CH_DSTADDR, dst);
	lpc_dmac_write_ch_4(sc, chno, LPC_DMAC_CH_LLI, 0);
	lpc_dmac_write_ch_4(sc, chno, LPC_DMAC_CH_CONTROL, ctrl);
	lpc_dmac_write_ch_4(sc, chno, LPC_DMAC_CH_CONFIG, cfg);

	return 0;
}

int
lpc_dmac_enable_channel(device_t dev, int chno)
{
	struct lpc_dmac_softc *sc = lpc_dmac_sc;
	uint32_t cfg;

	if (sc == NULL)
		return (ENXIO);

	cfg = lpc_dmac_read_ch_4(sc, chno, LPC_DMAC_CH_CONFIG);
	cfg |= LPC_DMAC_CH_CONFIG_E;

	lpc_dmac_write_ch_4(sc, chno, LPC_DMAC_CH_CONFIG, cfg);

	return 0;
}

int
lpc_dmac_disable_channel(device_t dev, int chno)
{
	struct lpc_dmac_softc *sc = lpc_dmac_sc;
	uint32_t cfg;

	if (sc == NULL)
		return (ENXIO);

	cfg = lpc_dmac_read_ch_4(sc, chno, LPC_DMAC_CH_CONFIG);
	cfg &= ~LPC_DMAC_CH_CONFIG_E;

	lpc_dmac_write_ch_4(sc, chno, LPC_DMAC_CH_CONFIG, cfg);

	return 0;
}

int
lpc_dmac_start_burst(device_t dev, int id)
{
	struct lpc_dmac_softc *sc = lpc_dmac_sc;

	lpc_dmac_write_4(sc, LPC_DMAC_SOFTBREQ, (1 << id));
	return (0);
}

static device_method_t lpc_dmac_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		lpc_dmac_probe),
	DEVMETHOD(device_attach,	lpc_dmac_attach),

	{ 0, 0 },
};

static devclass_t lpc_dmac_devclass;

static driver_t lpc_dmac_driver = {
	"dmac",
	lpc_dmac_methods,
	sizeof(struct lpc_dmac_softc),
};

DRIVER_MODULE(dmac, simplebus, lpc_dmac_driver, lpc_dmac_devclass, 0, 0);
