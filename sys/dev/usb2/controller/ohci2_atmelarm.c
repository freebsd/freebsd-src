/*-
 * Copyright (c) 2006 M. Warner Losh.  All rights reserved.
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
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <dev/usb2/include/usb2_mfunc.h>
#include <dev/usb2/include/usb2_defs.h>
#include <dev/usb2/include/usb2_standard.h>

#include <dev/usb2/core/usb2_core.h>
#include <dev/usb2/core/usb2_busdma.h>
#include <dev/usb2/core/usb2_process.h>
#include <dev/usb2/core/usb2_config_td.h>
#include <dev/usb2/core/usb2_sw_transfer.h>
#include <dev/usb2/core/usb2_util.h>

#include <dev/usb2/controller/usb2_controller.h>
#include <dev/usb2/controller/usb2_bus.h>
#include <dev/usb2/controller/ohci2.h>

#include <sys/rman.h>

#include <arm/at91/at91_pmcvar.h>

#define	MEM_RID	0

static device_probe_t ohci_atmelarm_probe;
static device_attach_t ohci_atmelarm_attach;
static device_detach_t ohci_atmelarm_detach;

struct at91_ohci_softc {
	struct ohci_softc sc_ohci;	/* must be first */
	struct at91_pmc_clock *iclk;
	struct at91_pmc_clock *fclk;
};

static int
ohci_atmelarm_probe(device_t dev)
{
	device_set_desc(dev, "AT91 integrated OHCI controller");
	return (BUS_PROBE_DEFAULT);
}

static int
ohci_atmelarm_attach(device_t dev)
{
	struct at91_ohci_softc *sc = device_get_softc(dev);
	int err;
	int rid;

	if (sc == NULL) {
		return (ENXIO);
	}
	/* get all DMA memory */

	if (usb2_bus_mem_alloc_all(&sc->sc_ohci.sc_bus,
	    USB_GET_DMA_TAG(dev), &ohci_iterate_hw_softc)) {
		return ENOMEM;
	}
	sc->iclk = at91_pmc_clock_ref("ohci_clk");
	sc->fclk = at91_pmc_clock_ref("uhpck");

	sc->sc_ohci.sc_dev = dev;

	rid = MEM_RID;
	sc->sc_ohci.sc_io_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &rid, RF_ACTIVE);

	if (!(sc->sc_ohci.sc_io_res)) {
		err = ENOMEM;
		goto error;
	}
	sc->sc_ohci.sc_io_tag = rman_get_bustag(sc->sc_ohci.sc_io_res);
	sc->sc_ohci.sc_io_hdl = rman_get_bushandle(sc->sc_ohci.sc_io_res);
	sc->sc_ohci.sc_io_size = rman_get_size(sc->sc_ohci.sc_io_res);

	rid = 0;
	sc->sc_ohci.sc_irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE);
	if (!(sc->sc_ohci.sc_irq_res)) {
		goto error;
	}
	sc->sc_ohci.sc_bus.bdev = device_add_child(dev, "usbus", -1);
	if (!(sc->sc_ohci.sc_bus.bdev)) {
		goto error;
	}
	device_set_ivars(sc->sc_ohci.sc_bus.bdev, &sc->sc_ohci.sc_bus);

	strlcpy(sc->sc_ohci.sc_vendor, "Atmel", sizeof(sc->sc_ohci.sc_vendor));

	err = usb2_config_td_setup(&sc->sc_ohci.sc_config_td, sc,
	    &sc->sc_ohci.sc_bus.bus_mtx, NULL, 0, 4);
	if (err) {
		device_printf(dev, "could not setup config thread!\n");
		goto error;
	}
#if (__FreeBSD_version >= 700031)
	err = bus_setup_intr(dev, sc->sc_ohci.sc_irq_res, INTR_TYPE_BIO | INTR_MPSAFE,
	    NULL, (void *)ohci_interrupt, sc, &sc->sc_ohci.sc_intr_hdl);
#else
	err = bus_setup_intr(dev, sc->sc_ohci.sc_irq_res, INTR_TYPE_BIO | INTR_MPSAFE,
	    (void *)ohci_interrupt, sc, &sc->sc_ohci.sc_intr_hdl);
#endif
	if (err) {
		sc->sc_ohci.sc_intr_hdl = NULL;
		goto error;
	}
	/*
	 * turn on the clocks from the AT91's point of view.  Keep the unit in reset.
	 */
	at91_pmc_clock_enable(sc->iclk);
	at91_pmc_clock_enable(sc->fclk);
	bus_space_write_4(sc->sc_ohci.sc_io_tag, sc->sc_ohci.sc_io_hdl,
	    OHCI_CONTROL, 0);

	err = ohci_init(&sc->sc_ohci);
	if (!err) {
		err = device_probe_and_attach(sc->sc_ohci.sc_bus.bdev);
	}
	if (err) {
		goto error;
	}
	return (0);

error:
	ohci_atmelarm_detach(dev);
	return (ENXIO);
}

static int
ohci_atmelarm_detach(device_t dev)
{
	struct at91_ohci_softc *sc = device_get_softc(dev);
	device_t bdev;
	int err;

	if (sc->sc_ohci.sc_bus.bdev) {
		bdev = sc->sc_ohci.sc_bus.bdev;
		device_detach(bdev);
		device_delete_child(dev, bdev);
	}
	/* during module unload there are lots of children leftover */
	device_delete_all_children(dev);

	/*
	 * Put the controller into reset, then disable clocks and do
	 * the MI tear down.  We have to disable the clocks/hardware
	 * after we do the rest of the teardown.  We also disable the
	 * clocks in the opposite order we acquire them, but that
	 * doesn't seem to be absolutely necessary.  We free up the
	 * clocks after we disable them, so the system could, in
	 * theory, reuse them.
	 */
	bus_space_write_4(sc->sc_ohci.sc_io_tag, sc->sc_ohci.sc_io_hdl,
	    OHCI_CONTROL, 0);

	at91_pmc_clock_disable(sc->fclk);
	at91_pmc_clock_disable(sc->iclk);
	at91_pmc_clock_deref(sc->fclk);
	at91_pmc_clock_deref(sc->iclk);

	if (sc->sc_ohci.sc_irq_res && sc->sc_ohci.sc_intr_hdl) {
		/*
		 * only call ohci_detach() after ohci_init()
		 */
		ohci_detach(&sc->sc_ohci);

		err = bus_teardown_intr(dev, sc->sc_ohci.sc_irq_res, sc->sc_ohci.sc_intr_hdl);
		sc->sc_ohci.sc_intr_hdl = NULL;
	}
	if (sc->sc_ohci.sc_irq_res) {
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->sc_ohci.sc_irq_res);
		sc->sc_ohci.sc_irq_res = NULL;
	}
	if (sc->sc_ohci.sc_io_res) {
		bus_release_resource(dev, SYS_RES_MEMORY, MEM_RID,
		    sc->sc_ohci.sc_io_res);
		sc->sc_ohci.sc_io_res = NULL;
	}
	usb2_config_td_unsetup(&sc->sc_ohci.sc_config_td);

	usb2_bus_mem_free_all(&sc->sc_ohci.sc_bus, &ohci_iterate_hw_softc);

	return (0);
}

static device_method_t ohci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, ohci_atmelarm_probe),
	DEVMETHOD(device_attach, ohci_atmelarm_attach),
	DEVMETHOD(device_detach, ohci_atmelarm_detach),
	DEVMETHOD(device_shutdown, bus_generic_shutdown),

	/* Bus interface */
	DEVMETHOD(bus_print_child, bus_generic_print_child),

	{0, 0}
};

static driver_t ohci_driver = {
	"ohci",
	ohci_methods,
	sizeof(struct at91_ohci_softc),
};

static devclass_t ohci_devclass;

DRIVER_MODULE(ohci, atmelarm, ohci_driver, ohci_devclass, 0, 0);
MODULE_DEPEND(ohci, usb2_controller, 1, 1, 1);
MODULE_DEPEND(ohci, usb2_core, 1, 1, 1);
