/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Emmanuel Vadot <manu@FreeBSD.Org>
 * Copyright (c) 2021-2022 Bjoern A. Zeeb <bz@FreeBSD.ORG>
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
#include "opt_platform.h"
#include "opt_acpi.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/condvar.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/mutex.h>
#ifdef FDT
#include <sys/gpio.h>
#endif

#include <machine/bus.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>

#include <dev/usb/usb_core.h>
#include <dev/usb/usb_busdma.h>
#include <dev/usb/usb_process.h>

#include <dev/usb/usb_controller.h>
#include <dev/usb/usb_bus.h>
#include <dev/usb/controller/generic_xhci.h>
#include <dev/usb/controller/xhci.h>
#include <dev/usb/controller/dwc3/dwc3.h>

#ifdef FDT
#include <dev/fdt/simplebus.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/ofw_subr.h>

#include <dev/clk/clk.h>
#include <dev/phy/phy_usb.h>
#endif

#ifdef DEV_ACPI
#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <dev/acpica/acpivar.h>
#endif

struct snps_dwc3_softc {
	struct xhci_softc	sc;
	device_t		dev;
	struct resource *	mem_res;
	bus_space_tag_t		bst;
	bus_space_handle_t	bsh;
	uint32_t		snpsid;
	uint32_t		snpsversion;
	uint32_t		snpsrevision;
	uint32_t		snpsversion_type;
#ifdef FDT
	clk_t			clk_ref;
	clk_t			clk_suspend;
	clk_t			clk_bus;
#endif
};

#define	DWC3_WRITE(_sc, _off, _val)		\
    bus_space_write_4(_sc->bst, _sc->bsh, _off, _val)
#define	DWC3_READ(_sc, _off)		\
    bus_space_read_4(_sc->bst, _sc->bsh, _off)

#define	IS_DMA_32B	1

static void
xhci_interrupt_poll(void *_sc)
{
	struct xhci_softc *sc = _sc;

	USB_BUS_UNLOCK(&sc->sc_bus);
	xhci_interrupt(sc);
	USB_BUS_LOCK(&sc->sc_bus);
	usb_callout_reset(&sc->sc_callout, 1, (void *)&xhci_interrupt_poll, sc);
}

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

	if (xhci_use_polling() == 0) {
		err = bus_setup_intr(dev, sc->sc_irq_res, INTR_TYPE_BIO | INTR_MPSAFE,
		    NULL, (driver_intr_t *)xhci_interrupt, sc, &sc->sc_intr_hdl);
		if (err != 0) {
			device_printf(dev, "Failed to setup IRQ, %d\n", err);
			sc->sc_intr_hdl = NULL;
			return (err);
		}
	}

	err = xhci_init(sc, dev, IS_DMA_32B);
	if (err != 0) {
		device_printf(dev, "Failed to init XHCI, with error %d\n", err);
		return (ENXIO);
	}

	usb_callout_init_mtx(&sc->sc_callout, &sc->sc_bus.bus_mtx, 0);

	if (xhci_use_polling() != 0) {
		device_printf(dev, "Interrupt polling at %dHz\n", hz);
		USB_BUS_LOCK(&sc->sc_bus);
		xhci_interrupt_poll(sc);
		USB_BUS_UNLOCK(&sc->sc_bus);
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

#ifdef DWC3_DEBUG
static void
snsp_dwc3_dump_regs(struct snps_dwc3_softc *sc, const char *msg)
{
	struct xhci_softc *xsc;
	uint32_t reg;

	if (!bootverbose)
		return;

	device_printf(sc->dev, "%s: %s:\n", __func__, msg ? msg : "");

	reg = DWC3_READ(sc, DWC3_GCTL);
	device_printf(sc->dev, "GCTL: %#012x\n", reg);
	reg = DWC3_READ(sc, DWC3_GUCTL);
	device_printf(sc->dev, "GUCTL: %#012x\n", reg);
	reg = DWC3_READ(sc, DWC3_GUCTL1);
	device_printf(sc->dev, "GUCTL1: %#012x\n", reg);
	reg = DWC3_READ(sc, DWC3_GUSB2PHYCFG0);
	device_printf(sc->dev, "GUSB2PHYCFG0: %#012x\n", reg);
	reg = DWC3_READ(sc, DWC3_GUSB3PIPECTL0);
	device_printf(sc->dev, "GUSB3PIPECTL0: %#012x\n", reg);
	reg = DWC3_READ(sc, DWC3_DCFG);
	device_printf(sc->dev, "DCFG: %#012x\n", reg);

	xsc = &sc->sc;
	device_printf(sc->dev, "xhci quirks: %#012x\n", xsc->sc_quirks);
}

static void
snps_dwc3_dump_ctrlparams(struct snps_dwc3_softc *sc)
{
	const bus_size_t offs[] = {
	    DWC3_GHWPARAMS0, DWC3_GHWPARAMS1, DWC3_GHWPARAMS2, DWC3_GHWPARAMS3,
	    DWC3_GHWPARAMS4, DWC3_GHWPARAMS5, DWC3_GHWPARAMS6, DWC3_GHWPARAMS7,
	    DWC3_GHWPARAMS8,
	};
	uint32_t reg;
	int i;

	for (i = 0; i < nitems(offs); i++) {
		reg = DWC3_READ(sc, offs[i]);
		if (bootverbose)
			device_printf(sc->dev, "hwparams[%d]: %#012x\n", i, reg);
	}
}
#endif

static void
snps_dwc3_reset(struct snps_dwc3_softc *sc)
{
	uint32_t gctl, ghwp0, phy2, phy3;

	ghwp0 = DWC3_READ(sc, DWC3_GHWPARAMS0);

	gctl = DWC3_READ(sc, DWC3_GCTL);
	gctl |= DWC3_GCTL_CORESOFTRESET;
	DWC3_WRITE(sc, DWC3_GCTL, gctl);

	phy2 = DWC3_READ(sc, DWC3_GUSB2PHYCFG0);
	phy2 |= DWC3_GUSB2PHYCFG0_PHYSOFTRST;
	if ((ghwp0 & DWC3_GHWPARAMS0_MODE_MASK) ==
	    DWC3_GHWPARAMS0_MODE_DUALROLEDEVICE)
		phy2 &= ~DWC3_GUSB2PHYCFG0_SUSPENDUSB20;
	DWC3_WRITE(sc, DWC3_GUSB2PHYCFG0, phy2);

	phy3 = DWC3_READ(sc, DWC3_GUSB3PIPECTL0);
	phy3 |= DWC3_GUSB3PIPECTL0_PHYSOFTRST;
	if ((ghwp0 & DWC3_GHWPARAMS0_MODE_MASK) ==
	    DWC3_GHWPARAMS0_MODE_DUALROLEDEVICE)
		phy3 &= ~DWC3_GUSB3PIPECTL0_SUSPENDUSB3;
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

	/*
	 * Enable the Host IN Auto Retry feature, making the
	 * host respond with a non-terminating retry ACK.
	 * XXX If we ever support more than host mode this needs a dr_mode check.
	 */
	reg = DWC3_READ(sc, DWC3_GUCTL);
	reg |= DWC3_GUCTL_HOST_AUTO_RETRY;
	DWC3_WRITE(sc, DWC3_GUCTL, reg);
}

#ifdef FDT
static void
snps_dwc3_configure_phy(struct snps_dwc3_softc *sc, phandle_t node)
{
	char *phy_type;
	uint32_t reg;
	int nphy_types;

	phy_type = NULL;
	nphy_types = OF_getprop_alloc(node, "phy_type", (void **)&phy_type);
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
	OF_prop_free(phy_type);
}
#endif

static void
snps_dwc3_do_quirks(struct snps_dwc3_softc *sc)
{
	struct xhci_softc *xsc;
	uint32_t ghwp0, reg;

	ghwp0 = DWC3_READ(sc, DWC3_GHWPARAMS0);
	reg = DWC3_READ(sc, DWC3_GUSB2PHYCFG0);
	if (device_has_property(sc->dev, "snps,dis-u2-freeclk-exists-quirk"))
		reg &= ~DWC3_GUSB2PHYCFG0_U2_FREECLK_EXISTS;
	else
		reg |= DWC3_GUSB2PHYCFG0_U2_FREECLK_EXISTS;
	if (device_has_property(sc->dev, "snps,dis_u2_susphy_quirk"))
		reg &= ~DWC3_GUSB2PHYCFG0_SUSPENDUSB20;
	else if ((ghwp0 & DWC3_GHWPARAMS0_MODE_MASK) ==
	    DWC3_GHWPARAMS0_MODE_DUALROLEDEVICE)
		reg |= DWC3_GUSB2PHYCFG0_SUSPENDUSB20;
	if (device_has_property(sc->dev, "snps,dis_enblslpm_quirk"))
		reg &= ~DWC3_GUSB2PHYCFG0_ENBLSLPM;
	else
		reg |= DWC3_GUSB2PHYCFG0_ENBLSLPM;
	DWC3_WRITE(sc, DWC3_GUSB2PHYCFG0, reg);

	reg = DWC3_READ(sc, DWC3_GUCTL1);
	if (device_has_property(sc->dev, "snps,dis-tx-ipgap-linecheck-quirk"))
		reg |= DWC3_GUCTL1_TX_IPGAP_LINECHECK_DIS;
	DWC3_WRITE(sc, DWC3_GUCTL1, reg);

	reg = DWC3_READ(sc, DWC3_GUSB3PIPECTL0);
	if (device_has_property(sc->dev, "snps,dis-del-phy-power-chg-quirk"))
		reg &= ~DWC3_GUSB3PIPECTL0_DELAYP1TRANS;
	if (device_has_property(sc->dev, "snps,dis_rxdet_inp3_quirk"))
		reg |= DWC3_GUSB3PIPECTL0_DISRXDETINP3;
	if (device_has_property(sc->dev, "snps,dis_u3_susphy_quirk"))
		reg &= ~DWC3_GUSB3PIPECTL0_SUSPENDUSB3;
	else if ((ghwp0 & DWC3_GHWPARAMS0_MODE_MASK) ==
	    DWC3_GHWPARAMS0_MODE_DUALROLEDEVICE)
		reg |= DWC3_GUSB3PIPECTL0_SUSPENDUSB3;
	DWC3_WRITE(sc, DWC3_GUSB3PIPECTL0, reg);

	/* Port Disable does not work on <= 3.00a. Disable PORT_PED. */
	if ((sc->snpsid & 0xffff) <= 0x300a) {
		xsc = &sc->sc;
		xsc->sc_quirks |= XHCI_QUIRK_DISABLE_PORT_PED;
	}
}

static int
snps_dwc3_probe_common(device_t dev)
{
	char dr_mode[16] = { 0 };
	ssize_t s;

	s = device_get_property(dev, "dr_mode", dr_mode, sizeof(dr_mode),
	    DEVICE_PROP_BUFFER);
	if (s == -1) {
		device_printf(dev, "Cannot determine dr_mode\n");
		return (ENXIO);
	}
	if (strcmp(dr_mode, "host") != 0) {
		device_printf(dev,
		    "Found dr_mode '%s' but only 'host' supported. s=%zd\n",
		    dr_mode, s);
		return (ENXIO);
	}

	device_set_desc(dev, "Synopsys Designware DWC3");
	return (BUS_PROBE_DEFAULT);
}

static int
snps_dwc3_common_attach(device_t dev, bool is_fdt)
{
	struct snps_dwc3_softc *sc;
#ifdef FDT
	phandle_t node;
	phy_t usb2_phy, usb3_phy;
	uint32_t reg;
#endif
	int error, rid;

	sc = device_get_softc(dev);
	sc->dev = dev;

	rid = 0;
	sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->mem_res == NULL) {
		device_printf(dev, "Failed to map memory\n");
		return (ENXIO);
	}
	sc->bst = rman_get_bustag(sc->mem_res);
	sc->bsh = rman_get_bushandle(sc->mem_res);

	sc->snpsid = DWC3_READ(sc, DWC3_GSNPSID);
	sc->snpsversion = DWC3_VERSION(sc->snpsid);
	sc->snpsrevision = DWC3_REVISION(sc->snpsid);
	if (sc->snpsversion == DWC3_1_IP_ID ||
	    sc->snpsversion == DWC3_2_IP_ID) {
		sc->snpsrevision = DWC3_READ(sc, DWC3_1_VER_NUMBER);
		sc->snpsversion_type = DWC3_READ(sc, DWC3_1_VER_TYPE);
	}
	if (bootverbose) {
		switch (sc->snpsversion) {
		case DWC3_IP_ID:
			device_printf(sc->dev, "SNPS Version: DWC3 (%x %x)\n",
			    sc->snpsversion, sc->snpsrevision);
			break;
		case DWC3_1_IP_ID:
			device_printf(sc->dev, "SNPS Version: DWC3.1 (%x %x %x)\n",
			    sc->snpsversion, sc->snpsrevision,
			    sc->snpsversion_type);
			break;
		case DWC3_2_IP_ID:
			device_printf(sc->dev, "SNPS Version: DWC3.2 (%x %x %x)\n",
			    sc->snpsversion, sc->snpsrevision,
			    sc->snpsversion_type);
			break;
		}
	}
#ifdef DWC3_DEBUG
	snps_dwc3_dump_ctrlparams(sc);
#endif

#ifdef FDT
	if (!is_fdt)
		goto skip_phys;

	node = ofw_bus_get_node(dev);

	/* Get the clocks if any */
	if (ofw_bus_is_compatible(dev, "rockchip,rk3328-dwc3") == 1 ||
	    ofw_bus_is_compatible(dev, "rockchip,rk3568-dwc3") == 1) {
		if (clk_get_by_ofw_name(dev, node, "ref_clk", &sc->clk_ref) != 0)
			device_printf(dev, "Cannot get ref_clk\n");
		if (clk_get_by_ofw_name(dev, node, "suspend_clk", &sc->clk_suspend) != 0)
			device_printf(dev, "Cannot get suspend_clk\n");
		if (clk_get_by_ofw_name(dev, node, "bus_clk", &sc->clk_bus) != 0)
			device_printf(dev, "Cannot get bus_clk\n");
	}

	if (sc->clk_ref != NULL) {
		if (clk_enable(sc->clk_ref) != 0)
			device_printf(dev, "Cannot enable ref_clk\n");
	}
	if (sc->clk_suspend != NULL) {
		if (clk_enable(sc->clk_suspend) != 0)
			device_printf(dev, "Cannot enable suspend_clk\n");
	}
	if (sc->clk_bus != NULL) {
		if (clk_enable(sc->clk_bus) != 0)
			device_printf(dev, "Cannot enable bus_clk\n");
	}

	/* Get the phys */
	usb2_phy = usb3_phy = NULL;
	error = phy_get_by_ofw_name(dev, node, "usb2-phy", &usb2_phy);
	if (error == 0 && usb2_phy != NULL)
		phy_enable(usb2_phy);
	error = phy_get_by_ofw_name(dev, node, "usb3-phy", &usb3_phy);
	if (error == 0 && usb3_phy != NULL)
		phy_enable(usb3_phy);
	if (sc->snpsversion == DWC3_IP_ID) {
		if (sc->snpsrevision >= 0x290A) {
			uint32_t hwparams3;

			hwparams3 = DWC3_READ(sc, DWC3_GHWPARAMS3);
			if (DWC3_HWPARAMS3_SSPHY(hwparams3) == DWC3_HWPARAMS3_SSPHY_DISABLE) {
				reg = DWC3_READ(sc, DWC3_GUCTL1);
				if (bootverbose)
					device_printf(dev, "Forcing USB2 clock only\n");
				reg |= DWC3_GUCTL1_DEV_FORCE_20_CLK_FOR_30_CLK;
				DWC3_WRITE(sc, DWC3_GUCTL1, reg);
			}
		}
	}
	snps_dwc3_configure_phy(sc, node);
skip_phys:
#endif

	snps_dwc3_reset(sc);
	snps_dwc3_configure_host(sc);
	snps_dwc3_do_quirks(sc);

#ifdef DWC3_DEBUG
	snsp_dwc3_dump_regs(sc, "Pre XHCI init");
#endif
	error = snps_dwc3_attach_xhci(dev);
#ifdef DWC3_DEBUG
	snsp_dwc3_dump_regs(sc, "Post XHCI init");
#endif

#ifdef FDT
	if (error) {
		if (sc->clk_ref != NULL)
			clk_disable(sc->clk_ref);
		if (sc->clk_suspend != NULL)
			clk_disable(sc->clk_suspend);
		if (sc->clk_bus != NULL)
			clk_disable(sc->clk_bus);
	}
#endif
	return (error);
}

#ifdef FDT
static struct ofw_compat_data compat_data[] = {
	{ "snps,dwc3",	1 },
	{ NULL,		0 }
};

static int
snps_dwc3_fdt_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	return (snps_dwc3_probe_common(dev));
}

static int
snps_dwc3_fdt_attach(device_t dev)
{

	return (snps_dwc3_common_attach(dev, true));
}

static device_method_t snps_dwc3_fdt_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		snps_dwc3_fdt_probe),
	DEVMETHOD(device_attach,	snps_dwc3_fdt_attach),

	DEVMETHOD_END
};

DEFINE_CLASS_1(snps_dwc3_fdt, snps_dwc3_fdt_driver, snps_dwc3_fdt_methods,
    sizeof(struct snps_dwc3_softc), generic_xhci_driver);

DRIVER_MODULE(snps_dwc3_fdt, simplebus, snps_dwc3_fdt_driver, 0, 0);
MODULE_DEPEND(snps_dwc3_fdt, xhci, 1, 1, 1);
#endif

#ifdef DEV_ACPI
static char *dwc3_acpi_ids[] = {
	"808622B7",	/* This was an Intel PCI Vendor/Device ID used. */
	"PNP0D10",	/* The generic XHCI PNP ID needing extra probe checks. */
	NULL
};

static int
snps_dwc3_acpi_probe(device_t dev)
{
	char *match;
	int error;

	if (acpi_disabled("snps_dwc3"))
		return (ENXIO);

	error = ACPI_ID_PROBE(device_get_parent(dev), dev, dwc3_acpi_ids, &match);
	if (error > 0)
		return (ENXIO);

	/*
	 * If we found the Generic XHCI PNP ID we can only attach if we have
	 * some other means to identify the device as dwc3.
	 */
	if (strcmp(match, "PNP0D10") == 0) {
		/* This is needed in SolidRun's HoneyComb. */
		if (device_has_property(dev, "snps,dis_rxdet_inp3_quirk"))
			goto is_dwc3;

		return (ENXIO);
	}

is_dwc3:
	return (snps_dwc3_probe_common(dev));
}

static int
snps_dwc3_acpi_attach(device_t dev)
{

	return (snps_dwc3_common_attach(dev, false));
}

static device_method_t snps_dwc3_acpi_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		snps_dwc3_acpi_probe),
	DEVMETHOD(device_attach,	snps_dwc3_acpi_attach),

	DEVMETHOD_END
};

DEFINE_CLASS_1(snps_dwc3_acpi, snps_dwc3_acpi_driver, snps_dwc3_acpi_methods,
    sizeof(struct snps_dwc3_softc), generic_xhci_driver);

DRIVER_MODULE(snps_dwc3_acpi, acpi, snps_dwc3_acpi_driver, 0, 0);
MODULE_DEPEND(snps_dwc3_acpi, usb, 1, 1, 1);
#endif
