#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*-
 * Copyright (c) 2015 Stanislav Galabov. All rights reserved.
 * Copyright (c) 2010,2011 Aleksandr Rybalko. All rights reserved.
 * Copyright (c) 2007-2008 Hans Petter Selasky. All rights reserved.
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

#include <sys/stdint.h>
#include <sys/stddef.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/module.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/condvar.h>
#include <sys/sysctl.h>
#include <sys/sx.h>
#include <sys/unistd.h>
#include <sys/callout.h>
#include <sys/malloc.h>
#include <sys/priv.h>
#include <sys/rman.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>

#include <dev/usb/usb_core.h>
#include <dev/usb/usb_busdma.h>
#include <dev/usb/usb_process.h>
#include <dev/usb/usb_util.h>

#include <dev/usb/usb_controller.h>
#include <dev/usb/usb_bus.h>

#include <dev/usb/controller/ohci.h>

#include <mips/rt305x/rt305xreg.h>
#include <mips/rt305x/rt305x_sysctlvar.h>

#define OHCI_HC_DEVSTR	"Ralink integrated USB controller"

static device_probe_t ohci_obio_probe;
static device_attach_t ohci_obio_attach;
static device_detach_t ohci_obio_detach;

static int
ohci_obio_probe(device_t self)
{
	device_set_desc(self, OHCI_HC_DEVSTR);

	return (BUS_PROBE_DEFAULT);
}

static int
ohci_obio_attach(device_t self)
{
	ohci_softc_t *sc = device_get_softc(self);
	uint32_t reg;
	int err;
	int rid;

	/* setup controller interface softc */
	reg = rt305x_sysctl_get(SYSCTL_SYSCFG1);
	reg |= SYSCTL_SYSCFG1_USB0_HOST_MODE;
	rt305x_sysctl_set(SYSCTL_SYSCFG1, reg);

	reg = rt305x_sysctl_get(SYSCTL_CLKCFG1);
	reg |= SYSCTL_CLKCFG1_UPHY0_CLK_EN;
#ifdef MT7620
	reg |= SYSCTL_CLKCFG1_UPHY1_CLK_EN;
#endif
	rt305x_sysctl_set(SYSCTL_CLKCFG1, reg);

	reg = rt305x_sysctl_get(SYSCTL_RSTCTRL);
	reg |= SYSCTL_RSTCTRL_UPHY0 | SYSCTL_RSTCTRL_UPHY1;
	rt305x_sysctl_set(SYSCTL_RSTCTRL, reg);
	reg &= ~(SYSCTL_RSTCTRL_UPHY0 | SYSCTL_RSTCTRL_UPHY1);
	DELAY(100000);
	rt305x_sysctl_set(SYSCTL_RSTCTRL, reg);
	DELAY(100000);

	/* initialise some bus fields */
	sc->sc_bus.parent = self;
	sc->sc_bus.devices = sc->sc_devices;
	sc->sc_bus.devices_max = OHCI_MAX_DEVICES;
	sc->sc_bus.dma_bits = 32;

	/* get all DMA memory */
	if (usb_bus_mem_alloc_all(&sc->sc_bus,
	    USB_GET_DMA_TAG(self), &ohci_iterate_hw_softc)) {
		printf("No mem\n");
		return (ENOMEM);
	}

	rid = 0;
	sc->sc_io_res = bus_alloc_resource_any(self, SYS_RES_MEMORY, &rid,
				RF_ACTIVE);
	if (!sc->sc_io_res) {
		device_printf(self, "Could not map memory\n");
		goto error;
	}
	sc->sc_io_tag = rman_get_bustag(sc->sc_io_res);
	sc->sc_io_hdl = rman_get_bushandle(sc->sc_io_res);
	sc->sc_io_size = rman_get_size(sc->sc_io_res);

	rid = 0;
	sc->sc_irq_res = bus_alloc_resource_any(self, SYS_RES_IRQ, &rid,
		RF_SHAREABLE | RF_ACTIVE);
	if (sc->sc_irq_res == NULL) {
		device_printf(self, "Could not allocate irq\n");
		goto error;
	}

	sc->sc_bus.bdev = device_add_child(self, "usbus", -1);
	if (!(sc->sc_bus.bdev)) {
		device_printf(self, "Could not add USB device\n");
		goto error;
	}
	device_set_ivars(sc->sc_bus.bdev, &sc->sc_bus);
	device_set_desc(sc->sc_bus.bdev, OHCI_HC_DEVSTR);

	sprintf(sc->sc_vendor, "Ralink");

	err = bus_setup_intr(self, sc->sc_irq_res, INTR_TYPE_BIO | INTR_MPSAFE,
		NULL, (driver_intr_t *)ohci_interrupt, sc, &sc->sc_intr_hdl);
	if (err) {
		device_printf(self, "Could not setup irq, %d\n", err);
		sc->sc_intr_hdl = NULL;
		goto error;
	}

	err = ohci_init(sc);
	if (!err) {
		err = device_probe_and_attach(sc->sc_bus.bdev);
	}
	if (err) {
		device_printf(self, "USB init failed err=%d\n", err);
		goto error;
	}
	return (0);

error:
	ohci_obio_detach(self);
	return (ENXIO);
}

static int
ohci_obio_detach(device_t self)
{
	ohci_softc_t *sc = device_get_softc(self);
	device_t bdev;
	int err;

	if (sc->sc_bus.bdev) {
		bdev = sc->sc_bus.bdev;
		device_detach(bdev);
		device_delete_child(self, bdev);
	}
	/* during module unload there are lots of children leftover */
	device_delete_children(self);

	if (sc->sc_irq_res && sc->sc_intr_hdl) {
		/*
		 * only call ohci_detach() after ohci_init()
		 */
		ohci_detach(sc);

		/* Stop OHCI clock */
		rt305x_sysctl_set(SYSCTL_CLKCFG1, 
		  rt305x_sysctl_get(SYSCTL_CLKCFG1) & 
		  ~(SYSCTL_CLKCFG1_UPHY0_CLK_EN
#ifdef MT7620
		    | SYSCTL_CLKCFG1_UPHY1_CLK_EN
#endif
		));

		err = bus_teardown_intr(self, sc->sc_irq_res, sc->sc_intr_hdl);
		if (err)
			device_printf(self, "Could not tear down irq, %d\n",
				err);
		sc->sc_intr_hdl = NULL;
	}
	if (sc->sc_irq_res) {
		bus_release_resource(self, SYS_RES_IRQ, 0,
		    sc->sc_irq_res);
		sc->sc_irq_res = NULL;
	}
	if (sc->sc_io_res) {
		bus_release_resource(self, SYS_RES_MEMORY, 0,
		    sc->sc_io_res);
		sc->sc_io_res = NULL;
	}
	usb_bus_mem_free_all(&sc->sc_bus, &ohci_iterate_hw_softc);

	return (0);
}

static device_method_t ohci_obio_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, ohci_obio_probe),
	DEVMETHOD(device_attach, ohci_obio_attach),
	DEVMETHOD(device_detach, ohci_obio_detach),
	DEVMETHOD(device_suspend, bus_generic_suspend),
	DEVMETHOD(device_resume, bus_generic_resume),
	DEVMETHOD(device_shutdown, bus_generic_shutdown),

	DEVMETHOD_END
};

static driver_t ohci_obio_driver = {
	.name = "ohci",
	.methods = ohci_obio_methods,
	.size = sizeof(ohci_softc_t),
};

static devclass_t ohci_obio_devclass;

DRIVER_MODULE(ohci, obio, ohci_obio_driver, ohci_obio_devclass, 0, 0);
