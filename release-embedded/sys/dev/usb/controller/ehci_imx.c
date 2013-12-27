/*-
 * Copyright (c) 2010-2012 Semihalf
 * Copyright (c) 2012 The FreeBSD Foundation
 * Copyright (c) 2013 Ian Lepore <ian@freebsd.org>
 * All rights reserved.
 *
 * Portions of this software were developed by Oleksandr Rybalko
 * under sponsorship from the FreeBSD Foundation.
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

/*
 * EHCI driver for Freescale i.MX SoCs which incorporate the USBOH3 controller.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/condvar.h>
#include <sys/rman.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usb_busdma.h>
#include <dev/usb/usb_process.h>
#include <dev/usb/usb_controller.h>
#include <dev/usb/usb_bus.h>
#include <dev/usb/controller/ehci.h>
#include <dev/usb/controller/ehcireg.h>
#include "usbdevs.h"

#include <machine/bus.h>
#include <machine/resource.h>

#include <arm/freescale/imx/imx_machdep.h>

#include "opt_platform.h"

/*
 * Notes on the hardware and related FDT data seen in the wild.
 *
 * There are two sets of registers in the USBOH3 implementation; documentation
 * refers to them as "core" and "non-core" registers.  A set of core register
 * exists for each OTG or EHCI device.  There is a single set of non-core
 * registers per USBOH3, and they control aspects of operation not directly
 * related to the USB specs, such as whether interrupts from each of the core
 * devices are able to generate a SoC wakeup event.
 *
 * In the FreeBSD universe we might be inclined to describe the core and
 * non-core registers by using a pair of resource address/size values (two
 * entries in the reg property for each core).  However, we have to work with
 * existing FDT data (which mostly comes from the linux universe), and the way
 * they've chosen to represent this is with an entry for a "usbmisc" device
 * whose reg property describes the non-core registers. The way we handle FDT
 * data, this means that the resources (memory-mapped register range) for the
 * non-core registers belongs to a device other than the echi devices.
 *
 * At the moment we have no need to access the non-core registers, so all of
 * this amounts to documenting what's known.  The following compat strings have
 * been seen in existing FDT data:
 *   - "fsl,imx25-usbmisc"
 *   - "fsl,imx51-usbmisc";
 *   - "fsl,imx6q-usbmisc";
 *
 * In addition to the single usbmisc device, the existing FDT data defines a
 * separate device for each of the OTG or EHCI cores within the USBOH3.  Each of
 * those devices has a set of core registers described by the reg property.
 *
 * The core registers for each of the four cores in the USBOH3 are divided into
 * two parts: a set of imx-specific registers at an offset of 0 from the
 * beginning of the register range, and the standard USB (EHCI or OTG) registers
 * at an offset of 0x100 from the beginning of the register range.  The FreeBSD
 * way of dealing with this might be to map out two ranges in the reg property,
 * but that's not what the alternate universe has done.  To work with existing
 * FDT data, we acquire the resource that maps all the core registers, then use
 * bus_space_subregion() to create another resource that maps just the standard
 * USB registers, which we provide to the standard USB code in the ehci_softc.
 *
 * The following compat strings have been seen for the OTG and EHCI cores.  The
 * FDT compat table in this driver contains all these strings, but as of this
 * writing, not all of these SoCs have been tested with the driver.  The fact
 * that imx27 is common to all of them gives some hope that the driver will work
 * on all these SoCs.
 *   - "fsl,imx23-usb", "fsl,imx27-usb";
 *   - "fsl,imx25-usb", "fsl,imx27-usb";
 *   - "fsl,imx28-usb", "fsl,imx27-usb";
 *   - "fsl,imx51-usb", "fsl,imx27-usb";
 *   - "fsl,imx53-usb", "fsl,imx27-usb";
 *   - "fsl,imx6q-usb", "fsl,imx27-usb";
 *
 * The FDT data for some SoCs contains the following properties, which we don't
 * currently do anything with:
 *   - fsl,usbmisc = <&usbmisc 0>;
 *   - fsl,usbphy = <&usbphy0>;
 *
 * Some imx SoCs have FDT data related to USB PHY, some don't.  We have separate
 * usbphy drivers where needed; this data is mentioned here just to keep all the
 * imx-FDT-usb-related info in one place.  Here are the usbphy compat strings
 * known to exist:
 *   - "nop-usbphy"
 *   - "usb-nop-xceiv";
 *   - "fsl,imx23-usbphy" 
 *   - "fsl,imx28-usbphy", "fsl,imx23-usbphy";
 *   - "fsl,imx6q-usbphy", "fsl,imx23-usbphy";
 *
 */

static struct ofw_compat_data compat_data[] = {
	{"fsl,imx6q-usb",	1},
	{"fsl,imx53-usb",	1},
	{"fsl,imx51-usb",	1},
	{"fsl,imx28-usb",	1},
	{"fsl,imx27-usb",	1},
	{"fsl,imx25-usb",	1},
	{"fsl,imx23-usb",	1},
	{NULL,		 	0},
};

/*
 * Each EHCI device in the SoC has some SoC-specific per-device registers at an
 * offset of 0, then the standard EHCI registers begin at an offset of 0x100.
 */
#define	IMX_EHCI_REG_OFF	0x100
#define	IMX_EHCI_REG_SIZE	0x100

struct imx_ehci_softc {
	ehci_softc_t	ehci_softc;
	struct resource	*ehci_mem_res;	/* EHCI core regs. */
	struct resource	*ehci_irq_res;	/* EHCI core IRQ. */ 
};

static int
imx_ehci_probe(device_t dev)
{

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data != 0) {
		device_set_desc(dev, "Freescale i.MX integrated USB controller");
		return (BUS_PROBE_DEFAULT);
	}
	return (ENXIO);
}

static int
imx_ehci_detach(device_t dev)
{
	struct imx_ehci_softc *sc;
	ehci_softc_t *esc;

	sc = device_get_softc(dev);

	esc = &sc->ehci_softc;

	if (esc->sc_bus.bdev != NULL)
		device_delete_child(dev, esc->sc_bus.bdev);
	if (esc->sc_flags & EHCI_SCFLG_DONEINIT)
		ehci_detach(esc);
	if (esc->sc_intr_hdl != NULL)
		bus_teardown_intr(dev, esc->sc_irq_res, 
		    esc->sc_intr_hdl);
	if (sc->ehci_irq_res != NULL)
		bus_release_resource(dev, SYS_RES_IRQ, 0, 
		    sc->ehci_irq_res);
	if (sc->ehci_mem_res != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY, 0,
		    sc->ehci_mem_res);

	usb_bus_mem_free_all(&esc->sc_bus, &ehci_iterate_hw_softc);

	/* During module unload there are lots of children leftover */
	device_delete_children(dev);

	return (0);
}

static int
imx_ehci_attach(device_t dev)
{
	struct imx_ehci_softc *sc;
	ehci_softc_t *esc;
	int err, rid;

	sc = device_get_softc(dev);
	esc = &sc->ehci_softc;
	err = 0;

	/* Allocate bus_space resources. */
	rid = 0;
	sc->ehci_mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->ehci_mem_res == NULL) {
		device_printf(dev, "Cannot allocate memory resources\n");
		err = ENXIO;
		goto out;
	}

	rid = 0;
	sc->ehci_irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE);
	if (sc->ehci_irq_res == NULL) {
		device_printf(dev, "Cannot allocate IRQ resources\n");
		err = ENXIO;
		goto out;
	}

	esc->sc_io_tag = rman_get_bustag(sc->ehci_mem_res);
	esc->sc_bus.parent = dev;
	esc->sc_bus.devices = esc->sc_devices;
	esc->sc_bus.devices_max = EHCI_MAX_DEVICES;

	if (usb_bus_mem_alloc_all(&esc->sc_bus, USB_GET_DMA_TAG(dev),
	    &ehci_iterate_hw_softc) != 0) {
		device_printf(dev, "usb_bus_mem_alloc_all() failed\n");
		err = ENOMEM;
		goto out;
	}

	/*
	 * Set handle to USB related registers subregion used by
	 * generic EHCI driver.
	 */
	err = bus_space_subregion(esc->sc_io_tag, 
	    rman_get_bushandle(sc->ehci_mem_res),
	    IMX_EHCI_REG_OFF, IMX_EHCI_REG_SIZE, &esc->sc_io_hdl);
	if (err != 0) {
		device_printf(dev, "bus_space_subregion() failed\n");
		err = ENXIO;
		goto out;
	}

	/* Setup interrupt handler. */
	err = bus_setup_intr(dev, sc->ehci_irq_res, INTR_TYPE_BIO, NULL, 
	    (driver_intr_t *)ehci_interrupt, esc, &esc->sc_intr_hdl);
	if (err != 0) {
		device_printf(dev, "Could not setup IRQ\n");
		goto out;
	}

	/* Turn on clocks. */
	imx_ccm_usb_enable(dev);

	/* Add USB bus device. */
	esc->sc_bus.bdev = device_add_child(dev, "usbus", -1);
	if (esc->sc_bus.bdev == NULL) {
		device_printf(dev, "Could not add USB device\n");
		goto out;
	}
	device_set_ivars(esc->sc_bus.bdev, &esc->sc_bus);

	esc->sc_id_vendor = USB_VENDOR_FREESCALE;
	strlcpy(esc->sc_vendor, "Freescale", sizeof(esc->sc_vendor));

	/* Set flags that affect ehci_init() behavior. */
	esc->sc_flags |= EHCI_SCFLG_DONTRESET | EHCI_SCFLG_NORESTERM;
	err = ehci_init(esc);
	if (err != 0) {
		device_printf(dev, "USB init failed, usb_err_t=%d\n", 
		    err);
		goto out;
	}
	esc->sc_flags |= EHCI_SCFLG_DONEINIT;

	/* Probe the bus. */
	err = device_probe_and_attach(esc->sc_bus.bdev);
	if (err != 0) {
		device_printf(dev,
		    "device_probe_and_attach() failed\n");
		goto out;
	}

	err = 0;

out:

	if (err != 0)
		imx_ehci_detach(dev);

	return (err);
}

static device_method_t ehci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, imx_ehci_probe),
	DEVMETHOD(device_attach, imx_ehci_attach),
	DEVMETHOD(device_detach, imx_ehci_detach),
	DEVMETHOD(device_suspend, bus_generic_suspend),
	DEVMETHOD(device_resume, bus_generic_resume),
	DEVMETHOD(device_shutdown, bus_generic_shutdown),

	/* Bus interface */
	DEVMETHOD(bus_print_child, bus_generic_print_child),

	DEVMETHOD_END
};

static driver_t ehci_driver = {
	"ehci",
	ehci_methods,
	sizeof(struct imx_ehci_softc)
};

static devclass_t ehci_devclass;

DRIVER_MODULE(ehci, simplebus, ehci_driver, ehci_devclass, 0, 0);
MODULE_DEPEND(ehci, usb, 1, 1, 1);
