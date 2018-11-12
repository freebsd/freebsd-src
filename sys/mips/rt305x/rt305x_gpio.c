/*-
 * Copyright (c) 2010-2011, Aleksandr Rybalko <ray@ddteam.net>
 * Copyright (c) 2009, Oleksandr Tymoshenko <gonzo@FreeBSD.org>
 * Copyright (c) 2009, Luiz Otavio O Souza. 
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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
 * GPIO driver for RT305X SoC.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>

#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/gpio.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <mips/rt305x/rt305xreg.h>
#include <mips/rt305x/rt305x_gpio.h>
#include <mips/rt305x/rt305x_gpiovar.h>
#include <mips/rt305x/rt305x_sysctlvar.h>
#include <dev/gpio/gpiobusvar.h>

#include "gpio_if.h"

#ifdef	notyet
#define	DEFAULT_CAPS	(GPIO_PIN_INPUT | GPIO_PIN_OUTPUT | GPIO_PIN_INVIN | \
 		         GPIO_PIN_INVOUT | GPIO_PIN_REPORT )
#else
#define	DEFAULT_CAPS	(GPIO_PIN_INPUT | GPIO_PIN_OUTPUT | GPIO_PIN_INVIN | \
 		         GPIO_PIN_INVOUT )
#endif

/*
 * Helpers
 */
static void rt305x_gpio_pin_configure(struct rt305x_gpio_softc *sc, 
    struct gpio_pin *pin, uint32_t flags);

/*
 * Driver stuff
 */
static int rt305x_gpio_probe(device_t dev);
static int rt305x_gpio_attach(device_t dev);
static int rt305x_gpio_detach(device_t dev);
static int rt305x_gpio_intr(void *arg);

int 	rt305x_get_int_mask  (device_t);
void 	rt305x_set_int_mask  (device_t, uint32_t);
int 	rt305x_get_int_status(device_t);
void 	rt305x_set_int_status(device_t, uint32_t);

/*
 * GPIO interface
 */
static device_t rt305x_gpio_get_bus(device_t);
static int rt305x_gpio_pin_max(device_t dev, int *maxpin);
static int rt305x_gpio_pin_getcaps(device_t dev, uint32_t pin, uint32_t *caps);
static int rt305x_gpio_pin_getflags(device_t dev, uint32_t pin, uint32_t
    *flags);
static int rt305x_gpio_pin_getname(device_t dev, uint32_t pin, char *name);
static int rt305x_gpio_pin_setflags(device_t dev, uint32_t pin, uint32_t flags);
static int rt305x_gpio_pin_set(device_t dev, uint32_t pin, unsigned int value);
static int rt305x_gpio_pin_get(device_t dev, uint32_t pin, unsigned int *val);
static int rt305x_gpio_pin_toggle(device_t dev, uint32_t pin);

static void
rt305x_gpio_pin_configure(struct rt305x_gpio_softc *sc, struct gpio_pin *pin,
    unsigned int flags)
{
	GPIO_LOCK(sc);

	/*
	 * Manage input/output
	 */
	if (flags & (GPIO_PIN_INPUT|GPIO_PIN_OUTPUT)) {
		pin->gp_flags &= ~(GPIO_PIN_INPUT|GPIO_PIN_OUTPUT);
		if (flags & GPIO_PIN_OUTPUT) {
			pin->gp_flags |= GPIO_PIN_OUTPUT;
			GPIO_BIT_SET(sc, pin->gp_pin, DIR);
		}
		else {
			pin->gp_flags |= GPIO_PIN_INPUT;
			GPIO_BIT_CLR(sc, pin->gp_pin, DIR);
		}
	}

	if (flags & GPIO_PIN_INVOUT) {
		pin->gp_flags |= GPIO_PIN_INVOUT;
		GPIO_BIT_SET(sc, pin->gp_pin, POL);
	}
	else {
		pin->gp_flags &= ~GPIO_PIN_INVOUT;
		GPIO_BIT_CLR(sc, pin->gp_pin, POL);
	}

	if (flags & GPIO_PIN_INVIN) {
		pin->gp_flags |= GPIO_PIN_INVIN;
		GPIO_BIT_SET(sc, pin->gp_pin, POL);
	}
	else {
		pin->gp_flags &= ~GPIO_PIN_INVIN;
		GPIO_BIT_CLR(sc, pin->gp_pin, POL);
	}

#ifdef	notyet
	/* Enable interrupt bits for rising/falling transitions */
	if (flags & GPIO_PIN_REPORT) {
		pin->gp_flags |= GPIO_PIN_REPORT;
		GPIO_BIT_SET(sc, pin->gp_pin, RENA);
		GPIO_BIT_SET(sc, pin->gp_pin, FENA);
		device_printf(sc->dev, "Will report interrupt on pin %d\n", 
		    pin->gp_pin);

	}
	else {
		pin->gp_flags &= ~GPIO_PIN_REPORT;
		GPIO_BIT_CLR(sc, pin->gp_pin, RENA);
		GPIO_BIT_CLR(sc, pin->gp_pin, FENA);
	}
#else
	/* Disable generating interrupts for now */
	GPIO_BIT_CLR(sc, pin->gp_pin, RENA);
	GPIO_BIT_CLR(sc, pin->gp_pin, FENA);
#endif

	GPIO_UNLOCK(sc);
}

static device_t
rt305x_gpio_get_bus(device_t dev)
{
	struct rt305x_gpio_softc *sc;

	sc = device_get_softc(dev);

	return (sc->busdev);
}

static int
rt305x_gpio_pin_max(device_t dev, int *maxpin)
{

	*maxpin = NGPIO - 1;
	return (0);
}

static int
rt305x_gpio_pin_getcaps(device_t dev, uint32_t pin, uint32_t *caps)
{
	struct rt305x_gpio_softc *sc = device_get_softc(dev);
	int i;

	for (i = 0; i < sc->gpio_npins; i++) {
		if (sc->gpio_pins[i].gp_pin == pin)
			break;
	}

	if (i >= sc->gpio_npins)
		return (EINVAL);

	GPIO_LOCK(sc);
	*caps = sc->gpio_pins[i].gp_caps;
	GPIO_UNLOCK(sc);

	return (0);
}

static int
rt305x_gpio_pin_getflags(device_t dev, uint32_t pin, uint32_t *flags)
{
	struct rt305x_gpio_softc *sc = device_get_softc(dev);
	int i;

	for (i = 0; i < sc->gpio_npins; i++) {
		if (sc->gpio_pins[i].gp_pin == pin)
			break;
	}

	if (i >= sc->gpio_npins)
		return (EINVAL);

	GPIO_LOCK(sc);
	*flags = sc->gpio_pins[i].gp_flags;
	GPIO_UNLOCK(sc);

	return (0);
}

static int
rt305x_gpio_pin_getname(device_t dev, uint32_t pin, char *name)
{
	struct rt305x_gpio_softc *sc = device_get_softc(dev);
	int i;

	for (i = 0; i < sc->gpio_npins; i++) {
		if (sc->gpio_pins[i].gp_pin == pin)
			break;
	}

	if (i >= sc->gpio_npins)
		return (EINVAL);

	GPIO_LOCK(sc);
	memcpy(name, sc->gpio_pins[i].gp_name, GPIOMAXNAME);
	GPIO_UNLOCK(sc);

	return (0);
}

static int
rt305x_gpio_pin_setflags(device_t dev, uint32_t pin, uint32_t flags)
{
	int i;
	struct rt305x_gpio_softc *sc = device_get_softc(dev);

	for (i = 0; i < sc->gpio_npins; i++) {
		if (sc->gpio_pins[i].gp_pin == pin)
			break;
	}

	if (i >= sc->gpio_npins)
		return (EINVAL);

	rt305x_gpio_pin_configure(sc, &sc->gpio_pins[i], flags);

	return (0);
}

static int
rt305x_gpio_pin_set(device_t dev, uint32_t pin, unsigned int value)
{
	struct rt305x_gpio_softc *sc = device_get_softc(dev);
	int i;

	for (i = 0; i < sc->gpio_npins; i++) {
		if (sc->gpio_pins[i].gp_pin == pin)
			break;
	}

	if (i >= sc->gpio_npins)
		return (EINVAL);


	GPIO_LOCK(sc);
	if (value) GPIO_BIT_SET(sc, i, DATA);
	else       GPIO_BIT_CLR(sc, i, DATA);
	GPIO_UNLOCK(sc);

	return (0);
}

static int
rt305x_gpio_pin_get(device_t dev, uint32_t pin, unsigned int *val)
{
	struct rt305x_gpio_softc *sc = device_get_softc(dev);
	int i;

	for (i = 0; i < sc->gpio_npins; i++) {
		if (sc->gpio_pins[i].gp_pin == pin)
			break;
	}

	if (i >= sc->gpio_npins)
		return (EINVAL);

	GPIO_LOCK(sc);
	*val = GPIO_BIT_GET(sc, i, DATA);
	GPIO_UNLOCK(sc);

	return (0);
}

static int
rt305x_gpio_pin_toggle(device_t dev, uint32_t pin)
{
	int i;
	struct rt305x_gpio_softc *sc = device_get_softc(dev);

	for (i = 0; i < sc->gpio_npins; i++) {
		if (sc->gpio_pins[i].gp_pin == pin)
			break;
	}

	if (i >= sc->gpio_npins)
		return (EINVAL);

	GPIO_LOCK(sc);
	GPIO_BIT_SET(sc, i, TOG);
	GPIO_UNLOCK(sc);

	return (0);
}

static int
rt305x_gpio_intr(void *arg)
{
	struct rt305x_gpio_softc *sc = arg;
#ifdef	notyet
	uint32_t i;
#endif
	uint64_t input, value;
#ifdef	notyet
	uint64_t reset_pin;
	char notify[16];
	char pinname[6];
#endif

	/* Read all reported pins */
	input  = GPIO_READ_ALL(sc, INT);
	/* Clear int status */
	GPIO_WRITE_ALL(sc, INT, input);
	/* Clear report for OUTs */
	input &= ~GPIO_READ_ALL(sc, DIR);
	value = input & GPIO_READ_ALL(sc, DATA);

	if (!input) goto intr_done;

#ifdef	notyet
	/* if reset_gpio and this pin is input */
	if (sc->reset_gpio >= 0  && (input & (1 << sc->reset_gpio))) {
		/* get reset_gpio pin value */
		reset_pin = (value & (1 << sc->reset_gpio))?1:0;
		if ( sc->reset_gpio_last != reset_pin )	{
			/*
			 * if now reset is high, check how long
			 * and do reset if less than 2 seconds
			 */
			if ( reset_pin && 
			    (time_uptime - sc->reset_gpio_ontime) < 2 )
				shutdown_nice(0);

			sc->reset_gpio_last = reset_pin;
			sc->reset_gpio_ontime = time_uptime;
		}
	}

	for ( i = 0; i < NGPIO; i ++ )
	{
		/* Next if output pin */
		if ( !(( input >> i) & 1) ) continue;

		if ( (((value & input) >> i) & 1) != sc->gpio_pins[i].gp_last )
		{
			/* !system=GPIO subsystem=pin7 type=PIN_HIGH period=3 */
			snprintf(notify , sizeof(notify ), "period=%d", 
			    (uint32_t)time_uptime - sc->gpio_pins[i].gp_time);
			snprintf(pinname, sizeof(pinname), "pin%02d", i);
			devctl_notify("GPIO", pinname, 
			    (((value & input) >> i) & 1)?"PIN_HIGH":"PIN_LOW", 
			    notify);
			printf("GPIO[%s] %s %s\n", pinname, 
			    (((value & input) >> i) & 1)?"PIN_HIGH":"PIN_LOW", 
			    notify);
			sc->gpio_pins[i].gp_last = ((value & input) >> i) & 1;
			sc->gpio_pins[i].gp_time = time_uptime;
		}

	}
#endif

intr_done:
	return (FILTER_HANDLED);
}

static int
rt305x_gpio_probe(device_t dev)
{
	device_set_desc(dev, "RT305X GPIO driver");
	return (0);
}

static uint64_t
rt305x_gpio_init(device_t dev)
{
	uint64_t avl = ~0ULL;
	uint32_t gmode = rt305x_sysctl_get(SYSCTL_GPIOMODE);
	if (!(gmode & SYSCTL_GPIOMODE_RGMII_GPIO_MODE))
		avl &= ~RGMII_GPIO_MODE_MASK;
	if (!(gmode & SYSCTL_GPIOMODE_SDRAM_GPIO_MODE))
		avl &= ~SDRAM_GPIO_MODE_MASK;
	if (!(gmode & SYSCTL_GPIOMODE_MDIO_GPIO_MODE))
		avl &= ~MDIO_GPIO_MODE_MASK;
	if (!(gmode & SYSCTL_GPIOMODE_JTAG_GPIO_MODE))
		avl &= ~JTAG_GPIO_MODE_MASK;
	if (!(gmode & SYSCTL_GPIOMODE_UARTL_GPIO_MODE))
		avl &= ~UARTL_GPIO_MODE_MASK;
	if (!(gmode & SYSCTL_GPIOMODE_SPI_GPIO_MODE))
		avl &= ~SPI_GPIO_MODE_MASK;
	if (!(gmode & SYSCTL_GPIOMODE_I2C_GPIO_MODE))
		avl &= ~I2C_GPIO_MODE_MASK;
	if ((gmode & SYSCTL_GPIOMODE_UARTF_SHARE_MODE_GPIO) != 
	    SYSCTL_GPIOMODE_UARTF_SHARE_MODE_GPIO)
		avl &= ~I2C_GPIO_MODE_MASK;
/* D-Link DAP-1350 Board have
 * MDIO_GPIO_MODE
 * UARTF_GPIO_MODE
 * SPI_GPIO_MODE
 * I2C_GPIO_MODE
 * So we have 
 * 00000001 10000000 01111111 11111110
*/
	return (avl);

}

#define DAP1350_RESET_GPIO	10

static int
rt305x_gpio_attach(device_t dev)
{
	struct rt305x_gpio_softc *sc = device_get_softc(dev);
	int i;
	uint64_t avlpins = 0;
	sc->reset_gpio = DAP1350_RESET_GPIO;

	KASSERT((device_get_unit(dev) == 0),
	    ("rt305x_gpio_gpio: Only one gpio module supported"));

	mtx_init(&sc->gpio_mtx, device_get_nameunit(dev), NULL, MTX_DEF);

	/* Map control/status registers. */
	sc->gpio_mem_rid = 0;
	sc->gpio_mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &sc->gpio_mem_rid, RF_ACTIVE);

	if (sc->gpio_mem_res == NULL) {
		device_printf(dev, "couldn't map memory\n");
		rt305x_gpio_detach(dev);
		return (ENXIO);
	}

	if ((sc->gpio_irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, 
	    &sc->gpio_irq_rid, RF_SHAREABLE | RF_ACTIVE)) == NULL) {
		device_printf(dev, "unable to allocate IRQ resource\n");
		rt305x_gpio_detach(dev);
		return (ENXIO);
	}

	if ((bus_setup_intr(dev, sc->gpio_irq_res, INTR_TYPE_MISC, 
	    /* rt305x_gpio_filter, */
	    rt305x_gpio_intr, NULL, sc, &sc->gpio_ih))) {
		device_printf(dev,
		    "WARNING: unable to register interrupt handler\n");
		rt305x_gpio_detach(dev);
		return (ENXIO);
	}

	sc->dev = dev;
	avlpins = rt305x_gpio_init(dev);

	/* Configure all pins as input */
	/* disable interrupts for all pins */
	/* TODO */

	sc->gpio_npins = NGPIO;
	resource_int_value(device_get_name(dev), device_get_unit(dev), 
	    "pins", &sc->gpio_npins);

	for (i = 0; i < sc->gpio_npins; i++) {
 		sc->gpio_pins[i].gp_pin = i;
 		sc->gpio_pins[i].gp_caps = DEFAULT_CAPS;
 		sc->gpio_pins[i].gp_flags = 0;
	}

	/* Setup reset pin interrupt */
	if (TUNABLE_INT_FETCH("reset_gpio", &sc->reset_gpio)) {
		device_printf(dev, "\tHinted reset_gpio %d\n", sc->reset_gpio);
	}
#ifdef	notyet
	if (sc->reset_gpio != -1) {
		rt305x_gpio_pin_setflags(dev, sc->reset_gpio, 
		    GPIO_PIN_INPUT|GPIO_PIN_INVOUT|
		    GPIO_PIN_INVOUT|GPIO_PIN_REPORT);
		device_printf(dev, "\tUse reset_gpio %d\n", sc->reset_gpio);
	}
#else
	if (sc->reset_gpio != -1) {
		rt305x_gpio_pin_setflags(dev, sc->reset_gpio, 
		    GPIO_PIN_INPUT|GPIO_PIN_INVOUT);
		device_printf(dev, "\tUse reset_gpio %d\n", sc->reset_gpio);
	}
#endif
	sc->busdev = gpiobus_attach_bus(dev);
	if (sc->busdev == NULL) {
		rt305x_gpio_detach(dev);
		return (ENXIO);
	}

	return (0);
}

static int
rt305x_gpio_detach(device_t dev)
{
	struct rt305x_gpio_softc *sc = device_get_softc(dev);

	KASSERT(mtx_initialized(&sc->gpio_mtx), ("gpio mutex not initialized"));

	gpiobus_detach_bus(dev);
	if (sc->gpio_ih)
		bus_teardown_intr(dev, sc->gpio_irq_res, sc->gpio_ih);
	if (sc->gpio_irq_res)
		bus_release_resource(dev, SYS_RES_IRQ, sc->gpio_irq_rid,
		    sc->gpio_irq_res);
	if (sc->gpio_mem_res)
		bus_release_resource(dev, SYS_RES_MEMORY, sc->gpio_mem_rid,
		    sc->gpio_mem_res);
	mtx_destroy(&sc->gpio_mtx);

	return(0);
}

#ifdef notyet
static struct resource *
rt305x_gpio_alloc_resource(device_t bus, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
{
	struct obio_softc		*sc = device_get_softc(bus);
	struct resource			*rv;
	struct rman			*rm;

	switch (type) {
	case SYS_RES_GPIO:
		rm = &sc->gpio_rman;
		break;
	default:
		printf("%s: unknown resource type %d\n", __func__, type);
		return (0);
	}

	rv = rman_reserve_resource(rm, start, end, count, flags, child);
	if (rv == 0) {
		printf("%s: could not reserve resource\n", __func__);
		return (0);
	}

	rman_set_rid(rv, *rid);

	return (rv);
}

static int
rt305x_gpio_activate_resource(device_t bus, device_t child, int type, int rid,
    struct resource *r)
{

	return (rman_activate_resource(r));
}

static int
rt305x_gpio_deactivate_resource(device_t bus, device_t child, int type, int rid,
    struct resource *r)
{

	return (rman_deactivate_resource(r));
}

static int
rt305x_gpio_release_resource(device_t dev, device_t child, int type,
    int rid, struct resource *r)
{
	rman_release_resource(r);
	return (0);
}
#endif

static device_method_t rt305x_gpio_methods[] = {
	DEVMETHOD(device_probe,		rt305x_gpio_probe),
	DEVMETHOD(device_attach,	rt305x_gpio_attach),
	DEVMETHOD(device_detach,	rt305x_gpio_detach),

	/* GPIO protocol */
	DEVMETHOD(gpio_get_bus,		rt305x_gpio_get_bus),
	DEVMETHOD(gpio_pin_max,		rt305x_gpio_pin_max),
	DEVMETHOD(gpio_pin_getname,	rt305x_gpio_pin_getname),
	DEVMETHOD(gpio_pin_getflags,	rt305x_gpio_pin_getflags),
	DEVMETHOD(gpio_pin_getcaps,	rt305x_gpio_pin_getcaps),
	DEVMETHOD(gpio_pin_setflags,	rt305x_gpio_pin_setflags),
	DEVMETHOD(gpio_pin_get,		rt305x_gpio_pin_get),
	DEVMETHOD(gpio_pin_set,		rt305x_gpio_pin_set),
	DEVMETHOD(gpio_pin_toggle,	rt305x_gpio_pin_toggle),
	{0, 0},
};

static driver_t rt305x_gpio_driver = {
	"gpio",
	rt305x_gpio_methods,
	sizeof(struct rt305x_gpio_softc),
};
static devclass_t rt305x_gpio_devclass;

DRIVER_MODULE(rt305x_gpio, obio, rt305x_gpio_driver, 
    rt305x_gpio_devclass, 0, 0);
