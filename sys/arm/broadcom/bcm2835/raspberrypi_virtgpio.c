/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Tetsuya Uemura <t_uemura@macome.co.jp>
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

/*
 * This is a driver for bcm2835-virtgpio GPIO controller device found on some
 * Raspberry Pi models (listed below but not limited to). On which, the green
 * LED (ACT) is connected to this controller. With the help of this driver, a
 * node corresponding to the green LED will be created under /dev/led, allowing
 * us to control it.
 *
 * Applicable models (according to the FDTs of those models):
 *   Compute Module 2 (CM2)
 *   3 Model B (not 3B+)
 *   Compute Module 3 (CM3) and possibly 3+ (CM3+)
 *   Compute Module 4 SODIMM (CM4S)
 */

#include "opt_platform.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/gpio.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <dev/gpio/gpiobusvar.h>
#include <dev/ofw/ofw_bus.h>

#include <arm/broadcom/bcm2835/bcm2835_firmware.h>
#include <arm/broadcom/bcm2835/bcm2835_vcbus.h>

#include "gpio_if.h"

#define	RPI_VIRT_GPIO_PINS	2

struct rpi_virt_gpio_softc {
	device_t		busdev;
	device_t		firmware;
	struct mtx		sc_mtx;

	void			*vaddr;	/* Virtual address. */
	vm_paddr_t		paddr;	/* Physical address. */

	struct gpio_pin		gpio_pins[RPI_VIRT_GPIO_PINS];
	uint32_t		state[RPI_VIRT_GPIO_PINS];
};

#define	RPI_VIRT_GPIO_LOCK(_sc)		mtx_lock_spin(&(_sc)->sc_mtx)
#define	RPI_VIRT_GPIO_UNLOCK(_sc)	mtx_unlock_spin(&(_sc)->sc_mtx)

static struct ofw_compat_data compat_data[] = {
	{"brcm,bcm2835-virtgpio",	1},
	{NULL,				0}
};

static device_t
rpi_virt_gpio_get_bus(device_t dev)
{
	struct rpi_virt_gpio_softc *sc;

	sc = device_get_softc(dev);

	return (sc->busdev);
}

static int
rpi_virt_gpio_pin_max(device_t dev, int *maxpin)
{
	*maxpin = RPI_VIRT_GPIO_PINS - 1;

	return (0);
}

static int
rpi_virt_gpio_pin_getcaps(device_t dev, uint32_t pin, uint32_t *caps)
{
	if (pin >= RPI_VIRT_GPIO_PINS)
		return (EINVAL);

	*caps = GPIO_PIN_OUTPUT;

	return (0);
}

static int
rpi_virt_gpio_pin_getflags(device_t dev, uint32_t pin, uint32_t *flags)
{
	if (pin >= RPI_VIRT_GPIO_PINS)
		return (EINVAL);

	*flags = GPIO_PIN_OUTPUT;

	return (0);
}

static int
rpi_virt_gpio_pin_set(device_t dev, uint32_t pin, uint32_t value)
{
	struct rpi_virt_gpio_softc *sc;
	uint32_t *ptr;
	uint16_t on, off;

	if (pin >= RPI_VIRT_GPIO_PINS)
		return (EINVAL);

	sc = device_get_softc(dev);

	RPI_VIRT_GPIO_LOCK(sc);
	on = (uint16_t)(sc->state[pin] >> 16);
	off = (uint16_t)sc->state[pin];

	if (bootverbose)
		device_printf(dev, "on: %hu, off: %hu, now: %d -> %u\n",
		    on, off, on - off, value);

	if ((value > 0 && on - off != 0) || (value == 0 && on - off == 0)) {
		RPI_VIRT_GPIO_UNLOCK(sc);
		return (0);
	}

	if (value > 0)
		++on;
	else
		++off;

	sc->state[pin] = (on << 16 | off);
	ptr = (uint32_t *)sc->vaddr;
	ptr[pin] = sc->state[pin];
	RPI_VIRT_GPIO_UNLOCK(sc);

	return (0);
}

static int
rpi_virt_gpio_pin_get(device_t dev, uint32_t pin, uint32_t *val)
{
	struct rpi_virt_gpio_softc *sc;
	uint32_t *ptr, v;

	if (pin >= RPI_VIRT_GPIO_PINS)
		return (EINVAL);

	sc = device_get_softc(dev);

	ptr = (uint32_t *)sc->vaddr;
	RPI_VIRT_GPIO_LOCK(sc);
	v = ptr[pin];
	RPI_VIRT_GPIO_UNLOCK(sc);
	*val = ((uint16_t)(v >> 16) - (uint16_t)v) == 0 ? 0 : 1;

	return (0);
}

static int
rpi_virt_gpio_pin_toggle(device_t dev, uint32_t pin)
{
	int rv;
	unsigned int val;

	if (pin >= RPI_VIRT_GPIO_PINS)
		return (EINVAL);

	rv = rpi_virt_gpio_pin_get(dev, pin, &val);
	if (rv != 0)
		return (rv);

	rv = rpi_virt_gpio_pin_set(dev, pin, val == 0 ? 1 : 0);

	return (rv);
}

static int
rpi_virt_gpio_probe(device_t dev)
{
	device_t firmware;
	phandle_t gpio;
	union msg_gpiovirtbuf cfg;
	int rv;

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	gpio = ofw_bus_get_node(dev);
	if (!OF_hasprop(gpio, "gpio-controller"))
		return (ENXIO);

	/* Check whether the firmware is ready. */
	firmware = device_get_parent(dev);
	rv = bcm2835_firmware_property(firmware,
	    BCM2835_FIRMWARE_TAG_GET_GPIOVIRTBUF, &cfg, sizeof(cfg));
	if (rv != 0)
		return (ENXIO);

	device_set_desc(dev, "Raspberry Pi Virtual GPIO controller");

	return (BUS_PROBE_DEFAULT);
}

static int
rpi_virt_gpio_attach(device_t dev)
{
	struct rpi_virt_gpio_softc *sc;
	union msg_gpiovirtbuf cfg;
	int i, rv;

	sc = device_get_softc(dev);
	sc->firmware = device_get_parent(dev);
	mtx_init(&sc->sc_mtx, "Raspberry Pi virtgpio", NULL, MTX_SPIN);

	/*
	 * According to the Linux source at:
	 * https://github.com/raspberrypi/linux/blob/rpi-6.12.y/drivers/gpio/gpio-bcm-virt.c
	 * it first attempts to set the pre-allocated physical memory address
	 * in the firmware. If it is successfully acquired, access virtgpio via
	 * the virtual memory address mapped to that physical address.
	 *
	 * If the above fails, then as a fallback, attempts to obtain a
	 * physical memory address for accessing virtgpio from the firmware.
	 * And if obtained, link it to a virtual memory address and access
	 * virtgpio via it.
	 *
	 * An OpenWRT virtgpio driver I happened to see at first only
	 * implemented the fallback method. Then I implemented this method on
	 * FreeBSD and tested it with the 20240429 firmware, but it didn't
	 * work.
	 *
	 * At this point, I realised the first method in the source above. So I
	 * implemented this method on FreeBSD and tested it, and it worked. In
	 * my opinion, the second method was used until some time prior to
	 * 20240429, and then the firmware was modified and the first method
	 * was introduced. In my driver, only the first method exists.
	 */

	/* Allocate a physical memory range for accessing virtgpio. */
	sc->vaddr = contigmalloc(
	    PAGE_SIZE,			/* size */
	    M_DEVBUF, M_ZERO,		/* type, flags */
	    0, BCM2838_PERIPH_MAXADDR,	/* low, high */
	    PAGE_SIZE, 0);		/* alignment, boundary */
	if (sc->vaddr == NULL) {
		device_printf(dev, "Failed to allocate memory.\n");
		return ENOMEM;
	}
	sc->paddr = vtophys(sc->vaddr);
	/* Mark it uncacheable. */
	pmap_change_attr((vm_offset_t)sc->vaddr, PAGE_SIZE,
	    VM_MEMATTR_UNCACHEABLE);

	if (bootverbose)
		device_printf(dev,
		    "KVA alloc'd: virtual: %p, phys: %#jx\n",
		    sc->vaddr, (uintmax_t)sc->paddr);

	/* Set this address in firmware. */
	cfg.req.addr = (uint32_t)sc->paddr;
	rv = bcm2835_firmware_property(sc->firmware,
	    BCM2835_FIRMWARE_TAG_SET_GPIOVIRTBUF, &cfg, sizeof(cfg));
	if (bootverbose)
		device_printf(dev, "rv: %d, addr: 0x%x\n", rv, cfg.resp.addr);
	if (rv != 0 || cfg.resp.addr != 0)
		goto fail;

	/* Pins only support output. */
	for (i = 0; i < RPI_VIRT_GPIO_PINS; i++) {
		sc->gpio_pins[i].gp_pin = i;
		sc->gpio_pins[i].gp_caps = sc->gpio_pins[i].gp_flags
		    = GPIO_PIN_OUTPUT;
	}
	sc->busdev = gpiobus_add_bus(dev);
	if (sc->busdev == NULL)
		goto fail;

	bus_attach_children(dev);
	return (0);

fail:
	/* Release resource if necessary. */
	free(sc->vaddr, M_DEVBUF);
	mtx_destroy(&sc->sc_mtx);

	return (ENXIO);
}

static int
rpi_virt_gpio_detach(device_t dev)
{
	return (EBUSY);
}

static device_method_t rpi_virt_gpio_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		rpi_virt_gpio_probe),
	DEVMETHOD(device_attach,	rpi_virt_gpio_attach),
	DEVMETHOD(device_detach,	rpi_virt_gpio_detach),

	/* GPIO protocol */
	DEVMETHOD(gpio_get_bus,		rpi_virt_gpio_get_bus),
	DEVMETHOD(gpio_pin_max,		rpi_virt_gpio_pin_max),
	DEVMETHOD(gpio_pin_getcaps,	rpi_virt_gpio_pin_getcaps),
	DEVMETHOD(gpio_pin_getflags,	rpi_virt_gpio_pin_getflags),
	DEVMETHOD(gpio_pin_set,		rpi_virt_gpio_pin_set),
	DEVMETHOD(gpio_pin_get,		rpi_virt_gpio_pin_get),
	DEVMETHOD(gpio_pin_toggle,	rpi_virt_gpio_pin_toggle),

	DEVMETHOD_END
};

static driver_t rpi_virt_gpio_driver = {
	"gpio",
	rpi_virt_gpio_methods,
	sizeof(struct rpi_virt_gpio_softc),
};

EARLY_DRIVER_MODULE(rpi_virt_gpio, bcm2835_firmware, rpi_virt_gpio_driver,
    0, 0, BUS_PASS_INTERRUPT + BUS_PASS_ORDER_LATE);
