#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*-
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

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>

#include <dev/usb/usb_core.h>
#include <dev/usb/usb_busdma.h>
#include <dev/usb/usb_process.h>
#include <dev/usb/usb_util.h>

#include <dev/usb/usb_controller.h>
#include <dev/usb/usb_bus.h>
#include <dev/usb/controller/at91dci.h>

#include <sys/rman.h>

#include <arm/at91/at91_pmcvar.h>
#include <arm/at91/at91rm92reg.h>
#include <arm/at91/at91_pioreg.h>
#include <arm/at91/at91_piovar.h>

#define	MEM_RID	0

/* Pin Definitions - do they belong here or somewhere else ? -- YES! */

#define	VBUS_MASK	AT91C_PIO_PB24
#define	VBUS_BASE	AT91RM92_PIOB_BASE

#define	PULLUP_MASK	AT91C_PIO_PB22
#define	PULLUP_BASE	AT91RM92_PIOB_BASE

static device_probe_t at91_udp_probe;
static device_attach_t at91_udp_attach;
static device_detach_t at91_udp_detach;

struct at91_udp_softc {
	struct at91dci_softc sc_dci;	/* must be first */
	struct at91_pmc_clock *sc_mclk;
	struct at91_pmc_clock *sc_iclk;
	struct at91_pmc_clock *sc_fclk;
	struct callout sc_vbus;
};

static void
at91_vbus_poll(struct at91_udp_softc *sc)
{
	uint8_t vbus_val;

	vbus_val = at91_pio_gpio_get(VBUS_BASE, VBUS_MASK) != 0;
	at91dci_vbus_interrupt(&sc->sc_dci, vbus_val);

	callout_reset(&sc->sc_vbus, hz, (void *)&at91_vbus_poll, sc);
}

static void
at91_udp_clocks_on(void *arg)
{
	struct at91_udp_softc *sc = arg;

	at91_pmc_clock_enable(sc->sc_mclk);
	at91_pmc_clock_enable(sc->sc_iclk);
	at91_pmc_clock_enable(sc->sc_fclk);
}

static void
at91_udp_clocks_off(void *arg)
{
	struct at91_udp_softc *sc = arg;

	at91_pmc_clock_disable(sc->sc_fclk);
	at91_pmc_clock_disable(sc->sc_iclk);
	at91_pmc_clock_disable(sc->sc_mclk);
}

static void
at91_udp_pull_up(void *arg)
{
	at91_pio_gpio_set(PULLUP_BASE, PULLUP_MASK);
}

static void
at91_udp_pull_down(void *arg)
{
	at91_pio_gpio_clear(PULLUP_BASE, PULLUP_MASK);
}

static int
at91_udp_probe(device_t dev)
{
	device_set_desc(dev, "AT91 integrated AT91_UDP controller");
	return (0);
}

static int
at91_udp_attach(device_t dev)
{
	struct at91_udp_softc *sc = device_get_softc(dev);
	int err;
	int rid;

	/* setup AT9100 USB device controller interface softc */

	sc->sc_dci.sc_clocks_on = &at91_udp_clocks_on;
	sc->sc_dci.sc_clocks_off = &at91_udp_clocks_off;
	sc->sc_dci.sc_clocks_arg = sc;
	sc->sc_dci.sc_pull_up = &at91_udp_pull_up;
	sc->sc_dci.sc_pull_down = &at91_udp_pull_down;
	sc->sc_dci.sc_pull_arg = sc;

	/* initialise some bus fields */
	sc->sc_dci.sc_bus.parent = dev;
	sc->sc_dci.sc_bus.devices = sc->sc_dci.sc_devices;
	sc->sc_dci.sc_bus.devices_max = AT91_MAX_DEVICES;
	sc->sc_dci.sc_bus.dma_bits = 32;

	/* get all DMA memory */
	if (usb_bus_mem_alloc_all(&sc->sc_dci.sc_bus,
	    USB_GET_DMA_TAG(dev), NULL)) {
		return (ENOMEM);
	}
	callout_init_mtx(&sc->sc_vbus, &sc->sc_dci.sc_bus.bus_mtx, 0);

	/*
	 * configure VBUS input pin, enable deglitch and enable
	 * interrupt :
	 */
	at91_pio_use_gpio(VBUS_BASE, VBUS_MASK);
	at91_pio_gpio_input(VBUS_BASE, VBUS_MASK);
	at91_pio_gpio_set_deglitch(VBUS_BASE, VBUS_MASK, 1);
	at91_pio_gpio_set_interrupt(VBUS_BASE, VBUS_MASK, 0);

	/*
	 * configure PULLUP output pin :
	 */
	at91_pio_use_gpio(PULLUP_BASE, PULLUP_MASK);
	at91_pio_gpio_output(PULLUP_BASE, PULLUP_MASK, 0);

	at91_udp_pull_down(sc);

	/* wait 10ms for pulldown to stabilise */
	usb_pause_mtx(NULL, hz / 100);

	sc->sc_mclk = at91_pmc_clock_ref("mck");
	sc->sc_iclk = at91_pmc_clock_ref("udc_clk");
	sc->sc_fclk = at91_pmc_clock_ref("udpck");

	rid = MEM_RID;
	sc->sc_dci.sc_io_res =
	    bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid, RF_ACTIVE);

	if (!(sc->sc_dci.sc_io_res)) {
		err = ENOMEM;
		goto error;
	}
	sc->sc_dci.sc_io_tag = rman_get_bustag(sc->sc_dci.sc_io_res);
	sc->sc_dci.sc_io_hdl = rman_get_bushandle(sc->sc_dci.sc_io_res);
	sc->sc_dci.sc_io_size = rman_get_size(sc->sc_dci.sc_io_res);

	rid = 0;
	sc->sc_dci.sc_irq_res =
	    bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid, RF_ACTIVE);
	if (!(sc->sc_dci.sc_irq_res)) {
		goto error;
	}
	sc->sc_dci.sc_bus.bdev = device_add_child(dev, "usbus", -1);
	if (!(sc->sc_dci.sc_bus.bdev)) {
		goto error;
	}
	device_set_ivars(sc->sc_dci.sc_bus.bdev, &sc->sc_dci.sc_bus);

	err = bus_setup_intr(dev, sc->sc_dci.sc_irq_res, INTR_TYPE_TTY | INTR_MPSAFE,
	    at91dci_filter_interrupt, at91dci_interrupt, sc, &sc->sc_dci.sc_intr_hdl);
	if (err) {
		sc->sc_dci.sc_intr_hdl = NULL;
		goto error;
	}

	err = at91dci_init(&sc->sc_dci);
	if (!err) {
		err = device_probe_and_attach(sc->sc_dci.sc_bus.bdev);
	}
	if (err) {
		goto error;
	} else {
		/* poll VBUS one time */
		USB_BUS_LOCK(&sc->sc_dci.sc_bus);
		at91_vbus_poll(sc);
		USB_BUS_UNLOCK(&sc->sc_dci.sc_bus);
	}
	return (0);

error:
	at91_udp_detach(dev);
	return (ENXIO);
}

static int
at91_udp_detach(device_t dev)
{
	struct at91_udp_softc *sc = device_get_softc(dev);
	int err;

	/* during module unload there are lots of children leftover */
	device_delete_children(dev);

	USB_BUS_LOCK(&sc->sc_dci.sc_bus);
	callout_stop(&sc->sc_vbus);
	USB_BUS_UNLOCK(&sc->sc_dci.sc_bus);

	callout_drain(&sc->sc_vbus);

	/* disable Transceiver */
	AT91_UDP_WRITE_4(&sc->sc_dci, AT91_UDP_TXVC, AT91_UDP_TXVC_DIS);

	/* disable and clear all interrupts */
	AT91_UDP_WRITE_4(&sc->sc_dci, AT91_UDP_IDR, 0xFFFFFFFF);
	AT91_UDP_WRITE_4(&sc->sc_dci, AT91_UDP_ICR, 0xFFFFFFFF);

	if (sc->sc_dci.sc_irq_res && sc->sc_dci.sc_intr_hdl) {
		/*
		 * only call at91_udp_uninit() after at91_udp_init()
		 */
		at91dci_uninit(&sc->sc_dci);

		err = bus_teardown_intr(dev, sc->sc_dci.sc_irq_res,
		    sc->sc_dci.sc_intr_hdl);
		sc->sc_dci.sc_intr_hdl = NULL;
	}
	if (sc->sc_dci.sc_irq_res) {
		bus_release_resource(dev, SYS_RES_IRQ, 0,
		    sc->sc_dci.sc_irq_res);
		sc->sc_dci.sc_irq_res = NULL;
	}
	if (sc->sc_dci.sc_io_res) {
		bus_release_resource(dev, SYS_RES_MEMORY, MEM_RID,
		    sc->sc_dci.sc_io_res);
		sc->sc_dci.sc_io_res = NULL;
	}
	usb_bus_mem_free_all(&sc->sc_dci.sc_bus, NULL);

	/* disable clocks */
	at91_pmc_clock_disable(sc->sc_iclk);
	at91_pmc_clock_disable(sc->sc_fclk);
	at91_pmc_clock_disable(sc->sc_mclk);
	at91_pmc_clock_deref(sc->sc_fclk);
	at91_pmc_clock_deref(sc->sc_iclk);
	at91_pmc_clock_deref(sc->sc_mclk);

	return (0);
}

static device_method_t at91_udp_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, at91_udp_probe),
	DEVMETHOD(device_attach, at91_udp_attach),
	DEVMETHOD(device_detach, at91_udp_detach),
	DEVMETHOD(device_suspend, bus_generic_suspend),
	DEVMETHOD(device_resume, bus_generic_resume),
	DEVMETHOD(device_shutdown, bus_generic_shutdown),

	DEVMETHOD_END
};

static driver_t at91_udp_driver = {
	.name = "at91_udp",
	.methods = at91_udp_methods,
	.size = sizeof(struct at91_udp_softc),
};

static devclass_t at91_udp_devclass;

DRIVER_MODULE(at91_udp, atmelarm, at91_udp_driver, at91_udp_devclass, 0, 0);
