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
#include <dev/usb2/controller/at91dci.h>

#include <sys/rman.h>

#include <arm/at91/at91_pmcvar.h>
#include <arm/at91/at91rm92reg.h>
#include <arm/at91/at91_pio_rm9200.h>
#include <arm/at91/at91_piovar.h>

#define	MEM_RID	0

/* Pin Definitions - do they belong here or somewhere else ? */

#define	VBUS_MASK	AT91C_PIO_PB24
#define	VBUS_BASE	AT91RM92_PIOB_BASE

#define	PULLUP_MASK	AT91C_PIO_PB22
#define	PULLUP_BASE	AT91RM92_PIOB_BASE

static device_probe_t at91_udp_probe;
static device_attach_t at91_udp_attach;
static device_detach_t at91_udp_detach;
static device_shutdown_t at91_udp_shutdown;

struct at91_udp_softc {
	struct at91dci_softc sc_dci;	/* must be first */
	struct at91_pmc_clock *sc_iclk;
	struct at91_pmc_clock *sc_fclk;
	struct resource *sc_vbus_irq_res;
	void   *sc_vbus_intr_hdl;
};

static void
at91_vbus_interrupt(struct at91_udp_softc *sc)
{
	uint32_t temp;
	uint8_t vbus_val;

	/* XXX temporary clear interrupts here */

	temp = at91_pio_gpio_clear_interrupt(VBUS_BASE);

	/* just forward it */

	vbus_val = at91_pio_gpio_get(VBUS_BASE, VBUS_MASK);
	(sc->sc_dci.sc_bus.methods->vbus_interrupt)
	    (&sc->sc_dci.sc_bus, vbus_val);
	return;
}

static void
at91_udp_clocks_on(void *arg)
{
	struct at91_udp_softc *sc = arg;

	at91_pmc_clock_enable(sc->sc_iclk);
	at91_pmc_clock_enable(sc->sc_fclk);
	return;
}

static void
at91_udp_clocks_off(void *arg)
{
	struct at91_udp_softc *sc = arg;

	at91_pmc_clock_disable(sc->sc_fclk);
	at91_pmc_clock_disable(sc->sc_iclk);
	return;
}

static void
at91_udp_pull_up(void *arg)
{
	at91_pio_gpio_set(PULLUP_BASE, PULLUP_MASK);
	return;
}

static void
at91_udp_pull_down(void *arg)
{
	at91_pio_gpio_clear(PULLUP_BASE, PULLUP_MASK);
	return;
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

	if (sc == NULL) {
		return (ENXIO);
	}
	/* setup AT9100 USB device controller interface softc */

	sc->sc_dci.sc_clocks_on = &at91_udp_clocks_on;
	sc->sc_dci.sc_clocks_off = &at91_udp_clocks_off;
	sc->sc_dci.sc_clocks_arg = sc;
	sc->sc_dci.sc_pull_up = &at91_udp_pull_up;
	sc->sc_dci.sc_pull_down = &at91_udp_pull_down;
	sc->sc_dci.sc_pull_arg = sc;

	/* get all DMA memory */

	if (usb2_bus_mem_alloc_all(&sc->sc_dci.sc_bus,
	    USB_GET_DMA_TAG(dev), NULL)) {
		return (ENOMEM);
	}
	/*
	 * configure VBUS input pin, enable deglitch and enable
	 * interrupt :
	 */
	at91_pio_use_gpio(VBUS_BASE, VBUS_MASK);
	at91_pio_gpio_input(VBUS_BASE, VBUS_MASK);
	at91_pio_gpio_set_deglitch(VBUS_BASE, VBUS_MASK, 1);
	at91_pio_gpio_set_interrupt(VBUS_BASE, VBUS_MASK, 1);

	/*
	 * configure PULLUP output pin :
	 */
	at91_pio_use_gpio(PULLUP_BASE, PULLUP_MASK);
	at91_pio_gpio_output(PULLUP_BASE, PULLUP_MASK, 0);

	at91_udp_pull_down(sc);

	/* wait 10ms for pulldown to stabilise */
	usb2_pause_mtx(NULL, 10);

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
	rid = 1;
	sc->sc_vbus_irq_res =
	    bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid, RF_ACTIVE);
	if (!(sc->sc_vbus_irq_res)) {
		goto error;
	}
	sc->sc_dci.sc_bus.bdev = device_add_child(dev, "usbus", -1);
	if (!(sc->sc_dci.sc_bus.bdev)) {
		goto error;
	}
	device_set_ivars(sc->sc_dci.sc_bus.bdev, &sc->sc_dci.sc_bus);

	err = usb2_config_td_setup(&sc->sc_dci.sc_config_td, sc,
	    &sc->sc_dci.sc_bus.mtx, NULL, 0, 4);
	if (err) {
		device_printf(dev, "could not setup config thread!\n");
		goto error;
	}
#if (__FreeBSD_version >= 700031)
	err = bus_setup_intr(dev, sc->sc_dci.sc_irq_res, INTR_TYPE_BIO | INTR_MPSAFE,
	    NULL, (void *)at91dci_interrupt, sc, &sc->sc_dci.sc_intr_hdl);
#else
	err = bus_setup_intr(dev, sc->sc_dci.sc_irq_res, INTR_TYPE_BIO | INTR_MPSAFE,
	    (void *)at91dci_interrupt, sc, &sc->sc_dci.sc_intr_hdl);
#endif
	if (err) {
		sc->sc_dci.sc_intr_hdl = NULL;
		goto error;
	}
#if (__FreeBSD_version >= 700031)
	err = bus_setup_intr(dev, sc->sc_vbus_irq_res, INTR_TYPE_BIO | INTR_MPSAFE,
	    NULL, (void *)at91_vbus_interrupt, sc, &sc->sc_vbus_intr_hdl);
#else
	err = bus_setup_intr(dev, sc->sc_vbus_irq_res, INTR_TYPE_BIO | INTR_MPSAFE,
	    (void *)at91_vbus_interrupt, sc, &sc->sc_vbus_intr_hdl);
#endif
	if (err) {
		sc->sc_vbus_intr_hdl = NULL;
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
		at91_vbus_interrupt(sc);
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
	device_t bdev;
	int err;

	if (sc->sc_dci.sc_bus.bdev) {
		bdev = sc->sc_dci.sc_bus.bdev;
		device_detach(bdev);
		device_delete_child(dev, bdev);
	}
	/* during module unload there are lots of children leftover */
	device_delete_all_children(dev);

	/* disable Transceiver */
	AT91_UDP_WRITE_4(&sc->sc_dci, AT91_UDP_TXVC, AT91_UDP_TXVC_DIS);

	/* disable and clear all interrupts */
	AT91_UDP_WRITE_4(&sc->sc_dci, AT91_UDP_IDR, 0xFFFFFFFF);
	AT91_UDP_WRITE_4(&sc->sc_dci, AT91_UDP_ICR, 0xFFFFFFFF);

	/* disable VBUS interrupt */
	at91_pio_gpio_set_interrupt(VBUS_BASE, VBUS_MASK, 0);

	if (sc->sc_vbus_irq_res && sc->sc_vbus_intr_hdl) {
		err = bus_teardown_intr(dev, sc->sc_vbus_irq_res,
		    sc->sc_vbus_intr_hdl);
		sc->sc_vbus_intr_hdl = NULL;
	}
	if (sc->sc_vbus_irq_res) {
		bus_release_resource(dev, SYS_RES_IRQ, 1,
		    sc->sc_vbus_irq_res);
		sc->sc_vbus_irq_res = NULL;
	}
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
	usb2_config_td_unsetup(&sc->sc_dci.sc_config_td);

	usb2_bus_mem_free_all(&sc->sc_dci.sc_bus, NULL);

	/* disable clocks */
	at91_pmc_clock_disable(sc->sc_iclk);
	at91_pmc_clock_disable(sc->sc_fclk);
	at91_pmc_clock_deref(sc->sc_fclk);
	at91_pmc_clock_deref(sc->sc_iclk);

	return (0);
}

static int
at91_udp_shutdown(device_t dev)
{
	struct at91_udp_softc *sc = device_get_softc(dev);
	int err;

	err = bus_generic_shutdown(dev);
	if (err)
		return (err);

	at91dci_uninit(&sc->sc_dci);

	return (0);
}

static device_method_t at91_udp_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, at91_udp_probe),
	DEVMETHOD(device_attach, at91_udp_attach),
	DEVMETHOD(device_detach, at91_udp_detach),
	DEVMETHOD(device_shutdown, at91_udp_shutdown),

	/* Bus interface */
	DEVMETHOD(bus_print_child, bus_generic_print_child),

	{0, 0}
};

static driver_t at91_udp_driver = {
	"at91_udp",
	at91_udp_methods,
	sizeof(struct at91_udp_softc),
};

static devclass_t at91_udp_devclass;

DRIVER_MODULE(at91_udp, atmelarm, at91_udp_driver, at91_udp_devclass, 0, 0);
