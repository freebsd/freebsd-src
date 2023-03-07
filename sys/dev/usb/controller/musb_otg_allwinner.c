/*-
 * Copyright (c) 2016 Jared McNeill <jmcneill@invisible.ca>
 * Copyright (c) 2018 Andrew Turner <andrew@FreeBSD.org>
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
 * Allwinner USB Dual-Role Device (DRD) controller
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/kernel.h>
#include <sys/condvar.h>
#include <sys/module.h>

#include <machine/bus.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>

#include <dev/usb/usb_core.h>
#include <dev/usb/usb_busdma.h>
#include <dev/usb/usb_process.h>
#include <dev/usb/usb_util.h>

#include <dev/usb/usb_controller.h>
#include <dev/usb/usb_bus.h>
#include <dev/usb/controller/musb_otg.h>

#include <dev/extres/clk/clk.h>
#include <dev/extres/hwreset/hwreset.h>
#include <dev/extres/phy/phy.h>
#include <dev/extres/phy/phy_usb.h>

#ifdef __arm__
#include <arm/allwinner/aw_machdep.h>
#include <arm/allwinner/a10_sramc.h>
#endif

#define	DRD_EP_MAX		5
#define	DRD_EP_MAX_H3		4

#define	MUSB2_REG_AWIN_VEND0	0x0043
#define	VEND0_PIO_MODE		0

#if defined(__arm__)
#define	bs_parent_space(bs)	((bs)->bs_parent)
typedef bus_space_tag_t	awusb_bs_tag;
#elif defined(__aarch64__)
#define	bs_parent_space(bs)	(bs)
typedef void *		awusb_bs_tag;
#endif

#define	AWUSB_OKAY		0x01
#define	AWUSB_NO_CONFDATA	0x02
static struct ofw_compat_data compat_data[] = {
	{ "allwinner,sun4i-a10-musb",	AWUSB_OKAY },
	{ "allwinner,sun6i-a31-musb",	AWUSB_OKAY },
	{ "allwinner,sun8i-a33-musb",	AWUSB_OKAY | AWUSB_NO_CONFDATA },
	{ "allwinner,sun8i-h3-musb",	AWUSB_OKAY | AWUSB_NO_CONFDATA },
	{ NULL,				0 }
};

static const struct musb_otg_ep_cfg musbotg_ep_allwinner[] = {
	{
		.ep_end = DRD_EP_MAX,
		.ep_fifosz_shift = 9,
		.ep_fifosz_reg = MUSB2_VAL_FIFOSZ_512,
	},
	{
		.ep_end = -1,
	},
};

static const struct musb_otg_ep_cfg musbotg_ep_allwinner_h3[] = {
	{
		.ep_end = DRD_EP_MAX_H3,
		.ep_fifosz_shift = 9,
		.ep_fifosz_reg = MUSB2_VAL_FIFOSZ_512,
	},
	{
		.ep_end = -1,
	},
};

struct awusbdrd_softc {
	struct musbotg_softc	sc;
	struct resource		*res[2];
	clk_t			clk;
	hwreset_t		reset;
	phy_t			phy;
	struct bus_space	bs;
	int			flags;
};

static struct resource_spec awusbdrd_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ SYS_RES_IRQ,		0,	RF_ACTIVE },
	{ -1, 0 }
};

#define	REMAPFLAG	0x8000
#define	REGDECL(a, b)	[(a)] = ((b) | REMAPFLAG)

/* Allwinner USB DRD register mappings */
static const uint16_t awusbdrd_regmap[] = {
	REGDECL(MUSB2_REG_EPFIFO(0),	0x0000),
	REGDECL(MUSB2_REG_EPFIFO(1),	0x0004),
	REGDECL(MUSB2_REG_EPFIFO(2),	0x0008),
	REGDECL(MUSB2_REG_EPFIFO(3),	0x000c),
	REGDECL(MUSB2_REG_EPFIFO(4),	0x0010),
	REGDECL(MUSB2_REG_EPFIFO(5),	0x0014),
	REGDECL(MUSB2_REG_POWER,	0x0040),
	REGDECL(MUSB2_REG_DEVCTL,	0x0041),
	REGDECL(MUSB2_REG_EPINDEX,	0x0042),
	REGDECL(MUSB2_REG_INTTX,	0x0044),
	REGDECL(MUSB2_REG_INTRX,	0x0046),
	REGDECL(MUSB2_REG_INTTXE,	0x0048),
	REGDECL(MUSB2_REG_INTRXE,	0x004a),
	REGDECL(MUSB2_REG_INTUSB,	0x004c),
	REGDECL(MUSB2_REG_INTUSBE,	0x0050),
	REGDECL(MUSB2_REG_FRAME,	0x0054),
	REGDECL(MUSB2_REG_TESTMODE,	0x007c),
	REGDECL(MUSB2_REG_TXMAXP,	0x0080),
	REGDECL(MUSB2_REG_TXCSRL,	0x0082),
	REGDECL(MUSB2_REG_TXCSRH,	0x0083),
	REGDECL(MUSB2_REG_RXMAXP,	0x0084),
	REGDECL(MUSB2_REG_RXCSRL,	0x0086),
	REGDECL(MUSB2_REG_RXCSRH,	0x0087),
	REGDECL(MUSB2_REG_RXCOUNT,	0x0088),
	REGDECL(MUSB2_REG_TXTI,		0x008c),
	REGDECL(MUSB2_REG_TXNAKLIMIT,	0x008d),
	REGDECL(MUSB2_REG_RXNAKLIMIT,	0x008f),
	REGDECL(MUSB2_REG_RXTI,		0x008e),
	REGDECL(MUSB2_REG_TXFIFOSZ,	0x0090),
	REGDECL(MUSB2_REG_TXFIFOADD,	0x0092),
	REGDECL(MUSB2_REG_RXFIFOSZ,	0x0094),
	REGDECL(MUSB2_REG_RXFIFOADD,	0x0096),
	REGDECL(MUSB2_REG_FADDR,	0x0098),
	REGDECL(MUSB2_REG_TXFADDR(0),	0x0098),
	REGDECL(MUSB2_REG_TXHADDR(0),	0x009a),
	REGDECL(MUSB2_REG_TXHUBPORT(0),	0x009b),
	REGDECL(MUSB2_REG_RXFADDR(0),	0x009c),
	REGDECL(MUSB2_REG_RXHADDR(0),	0x009e),
	REGDECL(MUSB2_REG_RXHUBPORT(0),	0x009f),
	REGDECL(MUSB2_REG_TXFADDR(1),	0x0098),
	REGDECL(MUSB2_REG_TXHADDR(1),	0x009a),
	REGDECL(MUSB2_REG_TXHUBPORT(1),	0x009b),
	REGDECL(MUSB2_REG_RXFADDR(1),	0x009c),
	REGDECL(MUSB2_REG_RXHADDR(1),	0x009e),
	REGDECL(MUSB2_REG_RXHUBPORT(1),	0x009f),
	REGDECL(MUSB2_REG_TXFADDR(2),	0x0098),
	REGDECL(MUSB2_REG_TXHADDR(2),	0x009a),
	REGDECL(MUSB2_REG_TXHUBPORT(2),	0x009b),
	REGDECL(MUSB2_REG_RXFADDR(2),	0x009c),
	REGDECL(MUSB2_REG_RXHADDR(2),	0x009e),
	REGDECL(MUSB2_REG_RXHUBPORT(2),	0x009f),
	REGDECL(MUSB2_REG_TXFADDR(3),	0x0098),
	REGDECL(MUSB2_REG_TXHADDR(3),	0x009a),
	REGDECL(MUSB2_REG_TXHUBPORT(3),	0x009b),
	REGDECL(MUSB2_REG_RXFADDR(3),	0x009c),
	REGDECL(MUSB2_REG_RXHADDR(3),	0x009e),
	REGDECL(MUSB2_REG_RXHUBPORT(3),	0x009f),
	REGDECL(MUSB2_REG_TXFADDR(4),	0x0098),
	REGDECL(MUSB2_REG_TXHADDR(4),	0x009a),
	REGDECL(MUSB2_REG_TXHUBPORT(4),	0x009b),
	REGDECL(MUSB2_REG_RXFADDR(4),	0x009c),
	REGDECL(MUSB2_REG_RXHADDR(4),	0x009e),
	REGDECL(MUSB2_REG_RXHUBPORT(4),	0x009f),
	REGDECL(MUSB2_REG_TXFADDR(5),	0x0098),
	REGDECL(MUSB2_REG_TXHADDR(5),	0x009a),
	REGDECL(MUSB2_REG_TXHUBPORT(5),	0x009b),
	REGDECL(MUSB2_REG_RXFADDR(5),	0x009c),
	REGDECL(MUSB2_REG_RXHADDR(5),	0x009e),
	REGDECL(MUSB2_REG_RXHUBPORT(5),	0x009f),
	REGDECL(MUSB2_REG_CONFDATA,	0x00c0),
};

static bus_size_t
awusbdrd_reg(bus_size_t o)
{
	bus_size_t v;

	KASSERT(o < nitems(awusbdrd_regmap),
	    ("%s: Invalid register %#lx", __func__, o));
	if (o >= nitems(awusbdrd_regmap))
		return (o);

	v = awusbdrd_regmap[o];

	KASSERT((v & REMAPFLAG) != 0, ("%s: reg %#lx not in regmap",
	    __func__, o));

	return (v & ~REMAPFLAG);
}

static int
awusbdrd_filt(bus_size_t o)
{
	switch (o) {
	case MUSB2_REG_MISC:
	case MUSB2_REG_RXDBDIS:
	case MUSB2_REG_TXDBDIS:
		return (1);
	default:
		return (0);
	}
}

static uint8_t
awusbdrd_bs_r_1(awusb_bs_tag t, bus_space_handle_t h, bus_size_t o)
{
	struct bus_space *bs = t;

	switch (o) {
	case MUSB2_REG_HWVERS:
		return (0);	/* no known equivalent */
	}

	return (bus_space_read_1(bs_parent_space(bs), h, awusbdrd_reg(o)));
}

static uint8_t
awusbdrd_bs_r_1_noconf(awusb_bs_tag t, bus_space_handle_t h, bus_size_t o)
{

	/*
	 * There is no confdata register on some SoCs, return the same
	 * magic value as Linux.
	 */
	if (o == MUSB2_REG_CONFDATA)
		return (0xde);

	return (awusbdrd_bs_r_1(t, h, o));
}


static uint16_t
awusbdrd_bs_r_2(awusb_bs_tag t, bus_space_handle_t h, bus_size_t o)
{
	struct bus_space *bs = t;

	if (awusbdrd_filt(o) != 0)
		return (0);
	return bus_space_read_2(bs_parent_space(bs), h, awusbdrd_reg(o));
}

static void
awusbdrd_bs_w_1(awusb_bs_tag t, bus_space_handle_t h, bus_size_t o,
    uint8_t v)
{
	struct bus_space *bs = t;

	if (awusbdrd_filt(o) != 0)
		return;

	bus_space_write_1(bs_parent_space(bs), h, awusbdrd_reg(o), v);
}

static void
awusbdrd_bs_w_2(awusb_bs_tag t, bus_space_handle_t h, bus_size_t o,
    uint16_t v)
{
	struct bus_space *bs = t;

	if (awusbdrd_filt(o) != 0)
		return;

	bus_space_write_2(bs_parent_space(bs), h, awusbdrd_reg(o), v);
}

static void
awusbdrd_bs_rm_1(awusb_bs_tag t, bus_space_handle_t h, bus_size_t o,
    uint8_t *d, bus_size_t c)
{
	struct bus_space *bs = t;

	bus_space_read_multi_1(bs_parent_space(bs), h, awusbdrd_reg(o), d, c);
}

static void
awusbdrd_bs_rm_4(awusb_bs_tag t, bus_space_handle_t h, bus_size_t o,
    uint32_t *d, bus_size_t c)
{
	struct bus_space *bs = t;

	bus_space_read_multi_4(bs_parent_space(bs), h, awusbdrd_reg(o), d, c);
}

static void
awusbdrd_bs_wm_1(awusb_bs_tag t, bus_space_handle_t h, bus_size_t o,
    const uint8_t *d, bus_size_t c)
{
	struct bus_space *bs = t;

	if (awusbdrd_filt(o) != 0)
		return;

	bus_space_write_multi_1(bs_parent_space(bs), h, awusbdrd_reg(o), d, c);
}

static void
awusbdrd_bs_wm_4(awusb_bs_tag t, bus_space_handle_t h, bus_size_t o,
    const uint32_t *d, bus_size_t c)
{
	struct bus_space *bs = t;

	if (awusbdrd_filt(o) != 0)
		return;

	bus_space_write_multi_4(bs_parent_space(bs), h, awusbdrd_reg(o), d, c);
}

static void
awusbdrd_intr(void *arg)
{
	struct awusbdrd_softc *sc = arg;
	uint8_t intusb;
	uint16_t inttx, intrx;

	intusb = MUSB2_READ_1(&sc->sc, MUSB2_REG_INTUSB);
	inttx = MUSB2_READ_2(&sc->sc, MUSB2_REG_INTTX);
	intrx = MUSB2_READ_2(&sc->sc, MUSB2_REG_INTRX);
	if (intusb == 0 && inttx == 0 && intrx == 0)
		return;

	if (intusb)
		MUSB2_WRITE_1(&sc->sc, MUSB2_REG_INTUSB, intusb);
	if (inttx)
		MUSB2_WRITE_2(&sc->sc, MUSB2_REG_INTTX, inttx);
	if (intrx)
		MUSB2_WRITE_2(&sc->sc, MUSB2_REG_INTRX, intrx);

	musbotg_interrupt(arg, intrx, inttx, intusb);
}

static int
awusbdrd_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "Allwinner USB DRD");
	return (BUS_PROBE_DEFAULT);
}

static int
awusbdrd_attach(device_t dev)
{
	char usb_mode[24];
	struct awusbdrd_softc *sc;
	uint8_t musb_mode;
	int phy_mode;
	int error;

	sc = device_get_softc(dev);
	sc->flags = ofw_bus_search_compatible(dev, compat_data)->ocd_data;

	error = bus_alloc_resources(dev, awusbdrd_spec, sc->res);
	if (error != 0)
		return (error);

	musb_mode = MUSB2_HOST_MODE;	/* default */
	phy_mode = PHY_USB_MODE_HOST;
	if (OF_getprop(ofw_bus_get_node(dev), "dr_mode",
	    &usb_mode, sizeof(usb_mode)) > 0) {
		usb_mode[sizeof(usb_mode) - 1] = 0;
		if (strcasecmp(usb_mode, "host") == 0) {
			musb_mode = MUSB2_HOST_MODE;
			phy_mode = PHY_USB_MODE_HOST;
		} else if (strcasecmp(usb_mode, "peripheral") == 0) {
			musb_mode = MUSB2_DEVICE_MODE;
			phy_mode = PHY_USB_MODE_DEVICE;
		} else if (strcasecmp(usb_mode, "otg") == 0) {
			/*
			 * XXX phy has PHY_USB_MODE_OTG, but MUSB does not have
			 * it.  It's not clear how to propagate mode changes
			 * from phy layer (that detects them) to MUSB.
			 */
			musb_mode = MUSB2_DEVICE_MODE;
			phy_mode = PHY_USB_MODE_DEVICE;
		} else {
			device_printf(dev, "Invalid FDT dr_mode: %s\n",
			    usb_mode);
		}
	}

	/* AHB gate clock is required */
	error = clk_get_by_ofw_index(dev, 0, 0, &sc->clk);
	if (error != 0)
		goto fail;

	/* AHB reset is only present on some SoCs */
	(void)hwreset_get_by_ofw_idx(dev, 0, 0, &sc->reset);

	/* Enable clocks */
	error = clk_enable(sc->clk);
	if (error != 0) {
		device_printf(dev, "failed to enable clock: %d\n", error);
		goto fail;
	}
	if (sc->reset != NULL) {
		error = hwreset_deassert(sc->reset);
		if (error != 0) {
			device_printf(dev, "failed to de-assert reset: %d\n",
			    error);
			goto fail;
		}
	}

	/* XXX not sure if this is universally needed. */
	(void)phy_get_by_ofw_name(dev, 0, "usb", &sc->phy);
	if (sc->phy != NULL) {
		device_printf(dev, "setting phy mode %d\n", phy_mode);
		if (musb_mode == MUSB2_HOST_MODE) {
			error = phy_enable(sc->phy);
			if (error != 0) {
				device_printf(dev, "Could not enable phy\n");
				goto fail;
			}
		}
		error = phy_usb_set_mode(sc->phy, phy_mode);
		if (error != 0) {
			device_printf(dev, "Could not set phy mode\n");
			goto fail;
		}
	}

	sc->sc.sc_bus.parent = dev;
	sc->sc.sc_bus.devices = sc->sc.sc_devices;
	sc->sc.sc_bus.devices_max = MUSB2_MAX_DEVICES;
	sc->sc.sc_bus.dma_bits = 32;

	error = usb_bus_mem_alloc_all(&sc->sc.sc_bus, USB_GET_DMA_TAG(dev),
	    NULL);
	if (error != 0) {
		error = ENOMEM;
		goto fail;
	}

#if defined(__arm__)
	sc->bs.bs_parent = rman_get_bustag(sc->res[0]);
#elif defined(__aarch64__)
	sc->bs.bs_cookie = rman_get_bustag(sc->res[0]);
#endif

	if ((sc->flags & AWUSB_NO_CONFDATA) == AWUSB_NO_CONFDATA)
		sc->bs.bs_r_1 = awusbdrd_bs_r_1_noconf;
	else
		sc->bs.bs_r_1 = awusbdrd_bs_r_1;
	sc->bs.bs_r_2 = awusbdrd_bs_r_2;
	sc->bs.bs_w_1 = awusbdrd_bs_w_1;
	sc->bs.bs_w_2 = awusbdrd_bs_w_2;
	sc->bs.bs_rm_1 = awusbdrd_bs_rm_1;
	sc->bs.bs_rm_4 = awusbdrd_bs_rm_4;
	sc->bs.bs_wm_1 = awusbdrd_bs_wm_1;
	sc->bs.bs_wm_4 = awusbdrd_bs_wm_4;

	sc->sc.sc_io_tag = &sc->bs;
	sc->sc.sc_io_hdl = rman_get_bushandle(sc->res[0]);
	sc->sc.sc_io_size = rman_get_size(sc->res[0]);

	sc->sc.sc_bus.bdev = device_add_child(dev, "usbus", -1);
	if (sc->sc.sc_bus.bdev == NULL) {
		error = ENXIO;
		goto fail;
	}
	device_set_ivars(sc->sc.sc_bus.bdev, &sc->sc.sc_bus);
	sc->sc.sc_id = 0;
	sc->sc.sc_platform_data = sc;
	sc->sc.sc_mode = musb_mode;
	if (ofw_bus_is_compatible(dev, "allwinner,sun8i-h3-musb")) {
		sc->sc.sc_ep_cfg = musbotg_ep_allwinner_h3;
		sc->sc.sc_ep_max = DRD_EP_MAX_H3;
	} else {
		sc->sc.sc_ep_cfg = musbotg_ep_allwinner;
		sc->sc.sc_ep_max = DRD_EP_MAX;
	}

	error = bus_setup_intr(dev, sc->res[1], INTR_MPSAFE | INTR_TYPE_BIO,
	    NULL, awusbdrd_intr, sc, &sc->sc.sc_intr_hdl);
	if (error != 0)
		goto fail;

	/* Enable PIO mode */
	bus_write_1(sc->res[0], MUSB2_REG_AWIN_VEND0, VEND0_PIO_MODE);

#ifdef __arm__
	/* Map SRAMD area to USB0 (sun4i/sun7i only) */
	switch (allwinner_soc_family()) {
	case ALLWINNERSOC_SUN4I:
	case ALLWINNERSOC_SUN7I:
		a10_map_to_otg();
		break;
	}
#endif

	error = musbotg_init(&sc->sc);
	if (error != 0)
		goto fail;

	error = device_probe_and_attach(sc->sc.sc_bus.bdev);
	if (error != 0)
		goto fail;

	musbotg_vbus_interrupt(&sc->sc, 1);	/* XXX VBUS */

	return (0);

fail:
	if (sc->phy != NULL) {
		if (musb_mode == MUSB2_HOST_MODE)
			(void)phy_disable(sc->phy);
		phy_release(sc->phy);
	}
	if (sc->reset != NULL) {
		hwreset_assert(sc->reset);
		hwreset_release(sc->reset);
	}
	if (sc->clk != NULL)
		clk_release(sc->clk);
	bus_release_resources(dev, awusbdrd_spec, sc->res);
	return (error);
}

static int
awusbdrd_detach(device_t dev)
{
	struct awusbdrd_softc *sc;
	device_t bdev;
	int error;

	sc = device_get_softc(dev);

	if (sc->sc.sc_bus.bdev != NULL) {
		bdev = sc->sc.sc_bus.bdev;
		device_detach(bdev);
		device_delete_child(dev, bdev);
	}

	musbotg_uninit(&sc->sc);
	error = bus_teardown_intr(dev, sc->res[1], sc->sc.sc_intr_hdl);
	if (error != 0)
		return (error);

	usb_bus_mem_free_all(&sc->sc.sc_bus, NULL);

	if (sc->phy != NULL) {
		if (sc->sc.sc_mode == MUSB2_HOST_MODE)
			phy_disable(sc->phy);
		phy_release(sc->phy);
	}
	if (sc->reset != NULL) {
		if (hwreset_assert(sc->reset) != 0)
			device_printf(dev, "failed to assert reset\n");
		hwreset_release(sc->reset);
	}
	if (sc->clk != NULL)
		clk_release(sc->clk);

	bus_release_resources(dev, awusbdrd_spec, sc->res);

	device_delete_children(dev);

	return (0);
}

static device_method_t awusbdrd_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		awusbdrd_probe),
	DEVMETHOD(device_attach,	awusbdrd_attach),
	DEVMETHOD(device_detach,	awusbdrd_detach),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),

	DEVMETHOD_END
};

static driver_t awusbdrd_driver = {
	.name = "musbotg",
	.methods = awusbdrd_methods,
	.size = sizeof(struct awusbdrd_softc),
};

DRIVER_MODULE(musbotg, simplebus, awusbdrd_driver, 0, 0);
MODULE_DEPEND(musbotg, usb, 1, 1, 1);
