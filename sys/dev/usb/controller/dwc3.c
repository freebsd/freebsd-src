/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2019 Emmanuel Vadot <manu@FreeBSD.Org>
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
 * $FreeBSD$
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/condvar.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/gpio.h>
#include <machine/bus.h>

#include <dev/fdt/simplebus.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/ofw_subr.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>

#include <dev/usb/usb_core.h>
#include <dev/usb/usb_busdma.h>
#include <dev/usb/usb_process.h>

#include <dev/usb/usb_controller.h>
#include <dev/usb/usb_bus.h>
#include <dev/usb/controller/xhci.h>
#include <dev/usb/controller/dwc3.h>

#include <dev/extres/clk/clk.h>
#include <dev/extres/phy/phy_usb.h>

#include "generic_xhci.h"

static struct ofw_compat_data compat_data[] = {
	{ "snps,dwc3",	1 },
	{ NULL,		0 }
};

struct snps_dwc3_softc {
	struct xhci_softc	sc;
	device_t		dev;
	char			dr_mode[16];
	struct resource *	mem_res;
	bus_space_tag_t		bst;
	bus_space_handle_t	bsh;
	phandle_t		node;
	phy_t			usb2_phy;
	phy_t			usb3_phy;
};

#define	DWC3_WRITE(_sc, _off, _val)		\
    bus_space_write_4(_sc->bst, _sc->bsh, _off, _val)
#define	DWC3_READ(_sc, _off)		\
    bus_space_read_4(_sc->bst, _sc->bsh, _off)

static int
snps_dwc3_attach_xhci(device_t dev)
{
	struct snps_dwc3_softc *snps_sc = device_get_softc(dev);
	struct xhci_softc *sc = &snps_sc->sc;
	int err = 0, rid = 0;

	sc->sc_io_res = snps_sc->mem_res;
	sc->sc_io_tag = snps_sc->bst;
	sc->sc_io_hdl = snps_sc->bsh;
	sc->sc_io_size = rman_get_size(snps_sc->mem_res);

	sc->sc_irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_SHAREABLE | RF_ACTIVE);
	if (sc->sc_irq_res == NULL) {
		device_printf(dev, "Failed to allocate IRQ\n");
		return (ENXIO);
	}

	sc->sc_bus.bdev = device_add_child(dev, "usbus", -1);
	if (sc->sc_bus.bdev == NULL) {
		device_printf(dev, "Failed to add USB device\n");
		return (ENXIO);
	}

	device_set_ivars(sc->sc_bus.bdev, &sc->sc_bus);

	sprintf(sc->sc_vendor, "Synopsys");
	device_set_desc(sc->sc_bus.bdev, "Synopsys");

	err = bus_setup_intr(dev, sc->sc_irq_res, INTR_TYPE_BIO | INTR_MPSAFE,
	    NULL, (driver_intr_t *)xhci_interrupt, sc, &sc->sc_intr_hdl);
	if (err != 0) {
		device_printf(dev, "Failed to setup IRQ, %d\n", err);
		sc->sc_intr_hdl = NULL;
		return (err);
	}

	err = xhci_init(sc, dev, 1);
	if (err != 0) {
		device_printf(dev, "Failed to init XHCI, with error %d\n", err);
		return (ENXIO);
	}

	err = xhci_start_controller(sc);
	if (err != 0) {
		device_printf(dev, "Failed to start XHCI controller, with error %d\n", err);
		return (ENXIO);
	}

	device_printf(sc->sc_bus.bdev, "trying to attach\n");
	err = device_probe_and_attach(sc->sc_bus.bdev);
	if (err != 0) {
		device_printf(dev, "Failed to initialize USB, with error %d\n", err);
		return (ENXIO);
	}

	return (0);
}

#if 0
static void
snsp_dwc3_dump_regs(struct snps_dwc3_softc *sc)
{
	uint32_t reg;

	reg = DWC3_READ(sc, DWC3_GCTL);
	device_printf(sc->dev, "GCTL: %x\n", reg);
	reg = DWC3_READ(sc, DWC3_GUCTL1);
	device_printf(sc->dev, "GUCTL1: %x\n", reg);
	reg = DWC3_READ(sc, DWC3_GUSB2PHYCFG0);
	device_printf(sc->dev, "GUSB2PHYCFG0: %x\n", reg);
	reg = DWC3_READ(sc, DWC3_GUSB3PIPECTL0);
	device_printf(sc->dev, "GUSB3PIPECTL0: %x\n", reg);
	reg = DWC3_READ(sc, DWC3_DCFG);
	device_printf(sc->dev, "DCFG: %x\n", reg);
}
#endif

static void
snps_dwc3_reset(struct snps_dwc3_softc *sc)
{
	uint32_t gctl, phy2, phy3;

	if (sc->usb2_phy)
		phy_enable(sc->usb2_phy);
	if (sc->usb3_phy)
		phy_enable(sc->usb3_phy);

	gctl = DWC3_READ(sc, DWC3_GCTL);
	gctl |= DWC3_GCTL_CORESOFTRESET;
	DWC3_WRITE(sc, DWC3_GCTL, gctl);

	phy2 = DWC3_READ(sc, DWC3_GUSB2PHYCFG0);
	phy2 |= DWC3_GUSB2PHYCFG0_PHYSOFTRST;
	DWC3_WRITE(sc, DWC3_GUSB2PHYCFG0, phy2);

	phy3 = DWC3_READ(sc, DWC3_GUSB3PIPECTL0);
	phy3 |= DWC3_GUSB3PIPECTL0_PHYSOFTRST;
	DWC3_WRITE(sc, DWC3_GUSB3PIPECTL0, phy3);

	DELAY(1000);

	phy2 &= ~DWC3_GUSB2PHYCFG0_PHYSOFTRST;
	DWC3_WRITE(sc, DWC3_GUSB2PHYCFG0, phy2);

	phy3 &= ~DWC3_GUSB3PIPECTL0_PHYSOFTRST;
	DWC3_WRITE(sc, DWC3_GUSB3PIPECTL0, phy3);

	gctl &= ~DWC3_GCTL_CORESOFTRESET;
	DWC3_WRITE(sc, DWC3_GCTL, gctl);

}

static void
snps_dwc3_configure_host(struct snps_dwc3_softc *sc)
{
	uint32_t reg;

	reg = DWC3_READ(sc, DWC3_GCTL);
	reg &= ~DWC3_GCTL_PRTCAPDIR_MASK;
	reg |= DWC3_GCTL_PRTCAPDIR_HOST;
	DWC3_WRITE(sc, DWC3_GCTL, reg);
}

static void
snps_dwc3_configure_phy(struct snps_dwc3_softc *sc)
{
	char *phy_type;
	uint32_t reg;
	int nphy_types;

	phy_type = NULL;
	nphy_types = OF_getprop_alloc(sc->node, "phy_type", (void **)&phy_type);
	if (nphy_types <= 0)
		return;

	reg = DWC3_READ(sc, DWC3_GUSB2PHYCFG0);
	if (strncmp(phy_type, "utmi_wide", 9) == 0) {
		reg &= ~(DWC3_GUSB2PHYCFG0_PHYIF | DWC3_GUSB2PHYCFG0_USBTRDTIM(0xf));
		reg |= DWC3_GUSB2PHYCFG0_PHYIF |
			DWC3_GUSB2PHYCFG0_USBTRDTIM(DWC3_GUSB2PHYCFG0_USBTRDTIM_16BITS);
	} else {
		reg &= ~(DWC3_GUSB2PHYCFG0_PHYIF | DWC3_GUSB2PHYCFG0_USBTRDTIM(0xf));
		reg |= DWC3_GUSB2PHYCFG0_PHYIF |
			DWC3_GUSB2PHYCFG0_USBTRDTIM(DWC3_GUSB2PHYCFG0_USBTRDTIM_8BITS);
	}
	DWC3_WRITE(sc, DWC3_GUSB2PHYCFG0, reg);
}

static void
snps_dwc3_do_quirks(struct snps_dwc3_softc *sc)
{
	uint32_t reg;

	reg = DWC3_READ(sc, DWC3_GUSB2PHYCFG0);
	if (OF_hasprop(sc->node, "snps,dis-u2-freeclk-exists-quirk"))
		reg &= ~DWC3_GUSB2PHYCFG0_U2_FREECLK_EXISTS;
	else
		reg |= DWC3_GUSB2PHYCFG0_U2_FREECLK_EXISTS;
	if (OF_hasprop(sc->node, "snps,dis_u2_susphy_quirk"))
		reg &= ~DWC3_GUSB2PHYCFG0_SUSPENDUSB20;
	else
		reg |= DWC3_GUSB2PHYCFG0_SUSPENDUSB20;
	if (OF_hasprop(sc->node, "snps,dis_enblslpm_quirk"))
		reg &= ~DWC3_GUSB2PHYCFG0_ENBLSLPM;
	else
		reg |= DWC3_GUSB2PHYCFG0_ENBLSLPM;

	DWC3_WRITE(sc, DWC3_GUSB2PHYCFG0, reg);

	reg = DWC3_READ(sc, DWC3_GUCTL1);
	if (OF_hasprop(sc->node, "snps,dis-tx-ipgap-linecheck-quirk"))
		reg |= DWC3_GUCTL1_TX_IPGAP_LINECHECK_DIS;
	DWC3_WRITE(sc, DWC3_GUCTL1, reg);

	if (OF_hasprop(sc->node, "snps,dis-del-phy-power-chg-quirk")) {
		reg = DWC3_READ(sc, DWC3_GUSB3PIPECTL0);
		reg |= DWC3_GUSB3PIPECTL0_DELAYP1TRANS;
		DWC3_WRITE(sc, DWC3_GUSB3PIPECTL0, reg);
	}
}

static int
snps_dwc3_probe(device_t dev)
{
	struct snps_dwc3_softc *sc;

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	sc = device_get_softc(dev);
	sc->node = ofw_bus_get_node(dev);
	OF_getprop(sc->node, "dr_mode", sc->dr_mode, sizeof(sc->dr_mode));
	if (strcmp(sc->dr_mode, "host") != 0) {
		device_printf(dev, "Only host mode is supported\n");
		return (ENXIO);
	}

	device_set_desc(dev, "Synopsys Designware DWC3");
	return (BUS_PROBE_DEFAULT);
}

static int
snps_dwc3_attach(device_t dev)
{
	struct snps_dwc3_softc *sc;
	int rid = 0;

	sc = device_get_softc(dev);
	sc->dev = dev;

	sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->mem_res == NULL) {
		device_printf(dev, "Failed to map memory\n");
		return (ENXIO);
	}
	sc->bst = rman_get_bustag(sc->mem_res);
	sc->bsh = rman_get_bushandle(sc->mem_res);

	if (bootverbose)
		device_printf(dev, "snps id: %x\n", DWC3_READ(sc, DWC3_GSNPSID));

	/* Get the phys */
	phy_get_by_ofw_name(dev, sc->node, "usb2-phy", &sc->usb2_phy);
	phy_get_by_ofw_name(dev, sc->node, "usb3-phy", &sc->usb3_phy);

	snps_dwc3_reset(sc);
	snps_dwc3_configure_host(sc);
	snps_dwc3_configure_phy(sc);
	snps_dwc3_do_quirks(sc);
#if 0
	snsp_dwc3_dump_regs(sc);
#endif
	snps_dwc3_attach_xhci(dev);

	return (0);
}

static device_method_t snps_dwc3_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		snps_dwc3_probe),
	DEVMETHOD(device_attach,	snps_dwc3_attach),

	DEVMETHOD_END
};

static driver_t snps_dwc3_driver = {
	"xhci",
	snps_dwc3_methods,
	sizeof(struct snps_dwc3_softc)
};

static devclass_t snps_dwc3_devclass;
DRIVER_MODULE(snps_dwc3, simplebus, snps_dwc3_driver, snps_dwc3_devclass, 0, 0);
MODULE_DEPEND(snps_dwc3, xhci, 1, 1, 1);
